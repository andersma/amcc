/*
* Copyright 2011 Anders Ma (andersma.net). All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are met:
*
* 1. Redistributions of source code must retain the above copyright
* notice, this list of conditions and the following disclaimer.
*
* 2. Redistributions in binary form must reproduce the above copyright
* notice, this list of conditions and the following disclaimer in the
* documentation and/or other materials provided with the distribution.
*
* 3. The name of the copyright holder may not be used to endorse or promote
* products derived from this software without specific prior written
* permission.
*
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER ``AS IS'' AND ANY
* EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
* WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
* DISCLAIMED. IN NO EVENT SHALL WILLIAM TISÄTER BE LIABLE FOR ANY
* DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
* (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
* LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
* ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
* (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
* SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*
*/

#include <stdlib.h>
#include <unistd.h>
#include <math.h>
#include <string.h>
#include <pthread.h>
#include <errno.h>
#include <sys/types.h>

#include <gtk/gtk.h>
#include <gtk/gtkgl.h>
#include <gdk/gdk.h>
#include <gdk/gdkkeysyms.h>
#include <glib-object.h>
#include <glib.h>
#include <GL/gl.h>

#include <lib3ds/file.h>
#include <lib3ds/mesh.h>
#include <lib3ds/material.h>

#include "amcc.h"
#include "graph.h"
#include "mx.h"
#include "serial.h"
#include "attitude.h"

#define MAX_ANALOGDATA_ENTRY 10

/*
 * static variables
 */

/* 3ds loader */
static char* copter_filename = THREED_MODEL_FILENAME;
static int copter_faces;
static Lib3dsFile *copter_model;
static Lib3dsVector *copter_normals = NULL;
static Lib3dsVector *copter_vertices = NULL;
static Lib3dsRgba *copter_colors = NULL;
/* GTK+ */
static GtkBuilder *theXml;
static graph_t acc_graph;
static graph_t gyro_graph;
/* communication */
static mx_t mx;
static serial_t serial;
/* acc & gyro data*/
static guint accdata_process_index = 0;
static guint accdata_present_index = 0;
static guint gyrodata_process_index = 0;
static guint gyrodata_present_index = 0;
static analog_data_t acc_data[MAX_ANALOGDATA_ENTRY];
static analog_data_t gyro_data[MAX_ANALOGDATA_ENTRY];
/* mutex */
static pthread_mutex_t copter_render_mutex = PTHREAD_MUTEX_INITIALIZER;
/* attitude */
static float yaw_patch = 0;
static attitude_t attitude;

/*
 * parse input packet and update pertinent variables
 */
gint parse_packet(packet_t *p, void *arg)
{
	int i;
	float adc, vol;
	float accx, accy, accz;

	if (p->type == ANALOG_NAME_RESPONSE) {
		// to be continued. @_@
	} else if (p->type == ANALOG_DATA_RESPONSE) {
		// update analog data buffer
		memcpy(&acc_data[accdata_present_index], 
			    &p->raw.analog_data, sizeof(analog_data_t));
		ADD_ONE_WITH_WRAP_AROUND(accdata_present_index, MAX_ANALOGDATA_ENTRY);
		memcpy(&gyro_data[gyrodata_present_index], 
			    &p->raw.analog_data, sizeof(analog_data_t));
		ADD_ONE_WITH_WRAP_AROUND(gyrodata_present_index, MAX_ANALOGDATA_ENTRY);
		// attitude
		attitude.acc_crt_x = p->raw.analog_data.value[ACCX_CHANNEL];
		attitude.acc_crt_y = p->raw.analog_data.value[ACCY_CHANNEL];
		attitude.acc_crt_z = p->raw.analog_data.value[ACCZ_CHANNEL];
		attitude_by_acc(&attitude);
	}

	return 0;
}

gint acc_graph_callback(graph_t *g, guint channel, gfloat *data)
{
	gfloat f;
	gchar buffer[20];

	if (channel > acc_data[accdata_process_index].channel_number) {
		*data = EMPTY_DATA;
	} else {
		f = (float)acc_data[accdata_process_index].value[channel -1];
		*data = (f * 3300) / 4096.0;
		switch (channel) {
		case 1:
			sprintf(buffer, "Acc_X : %d(mv)", (gint)*data);
			break;
		case 2:
			sprintf(buffer, "Acc_Y : %d(mv)", (gint)*data);
			break;
		case 3:
			sprintf(buffer, "Acc_Z : %d(mv)", (gint)*data);
			break;
		}
		graph_set_channel_name(&acc_graph, channel, buffer);
	}

	return 0;
}

gint gyro_graph_callback(graph_t *g, guint channel, gfloat *data)
{
	gfloat f;
	gchar buffer[20];

	if (channel > gyro_data[gyrodata_process_index].channel_number) {
		*data = EMPTY_DATA;
	} else {
		f = (float)gyro_data[gyrodata_process_index].value[channel + 3 -1]; // 3, 4, 5 for gyros
		*data = (f * 3300) / 4096.0;
		switch (channel) {
		case 1:
			sprintf(buffer, "Gyro_X : %d(mv)", (gint)*data);
			break;
		case 2:
			sprintf(buffer, "Gyro_Y : %d(mv)", (gint)*data);
			break;
		case 3:
			sprintf(buffer, "Gyro_Z : %d(mv)", (gint)*data);
			break;
		}
		graph_set_channel_name(&gyro_graph, channel, buffer);
	}

	return 0;
}

static void usage ()
{
	fprintf(stderr, "Usage: amcc [option]\n");
	fprintf(stderr, "\t -d      serial device (eg: /dev/ttyS0)\n");
	fprintf(stderr, "\t -f      serial speed (eg: 57600)\n");
	fprintf(stderr, "\t -m      3D model filename (eg: ./copter.3ds)\n");
	fprintf(stderr, "\t -h      this usage info\n");
}

/*
 * set channel graph scale
 */
void set_scale (GtkButton *button, gpointer data)
{
	graph_t *g = (graph_t*)data;
	GtkWidget *wmin, *wmax;
	gint vmin, vmax;

	wmin = GTK_WIDGET (gtk_builder_get_object (theXml, "min"));
	wmax = GTK_WIDGET (gtk_builder_get_object (theXml, "max"));
	vmin = atoi(gtk_entry_get_text (GTK_ENTRY (wmin)));
	vmax = atoi(gtk_entry_get_text (GTK_ENTRY (wmax)));

	graph_set_data(g, (gfloat)vmin, (gfloat)vmax);
}

/*
 * start/stop channel graph/3D copter drawing
 */
void on_start_activate (GtkWidget* widget, gpointer data)
{
	const gchar *label;
	
	label = gtk_menu_item_get_label ((GtkMenuItem*) widget);
	if (label[2] == 'o') {  
		mx_rx_unregister(&mx, ANALOG_DATA_RESPONSE, parse_packet);
		gtk_menu_item_set_label ((GtkMenuItem*) widget, "Start");
		// sensors caliberation
		attitude.acc_nml_x = acc_data[accdata_present_index].value[ACCX_CHANNEL];
		attitude.acc_nml_y = acc_data[accdata_present_index].value[ACCY_CHANNEL];
		attitude.acc_nml_z = acc_data[accdata_present_index].value[ACCZ_CHANNEL];

	} else {
		mx_rx_register(&mx, ANALOG_DATA_RESPONSE, parse_packet, NULL);
		gtk_menu_item_set_label ((GtkMenuItem*) widget, "Stop");
	}
}

/*
 * rotate copter PIE/4
 */
void on_copter_rotate90 (GtkWidget* widget, gpointer data)
{
	yaw_patch += 90.0f;
}

/*
 * show/hide voltage graph
 */
void on_view_voltage_graph (GtkWidget* widget, gpointer data)
{
	const gchar *label;
	
	label = gtk_menu_item_get_label ((GtkMenuItem*) widget);
	/*
	 *hbox1 is the father of vobx2(acc_graph) & vbox3(gyro_graph) 
	*/
	if (label[0] == '-') {  
		gtk_widget_hide(GTK_WIDGET (gtk_builder_get_object (theXml, "hbox1")));
		gtk_menu_item_set_label ((GtkMenuItem*) widget, "+ Acc/Gyro Graph");
	} else {
		gtk_widget_show(GTK_WIDGET (gtk_builder_get_object (theXml, "hbox1")));
		gtk_menu_item_set_label ((GtkMenuItem*) widget, "- Acc/Gyro Graph");
	}
}

/*
 * show about dialog
 */
void on_about (GtkWidget* widget, gpointer data)
{
	GtkDialog *about;

	about = GTK_DIALOG (gtk_builder_get_object (theXml, "aboutdialog"));
	gtk_dialog_run(about);
	gtk_widget_hide(GTK_WIDGET(about));
}

/*
 * The "realize" signal handler for copter. 
 * One-time OpenGL initialization should be performed here.
 */
void on_copter_realize (GtkWidget *copter, gpointer data)
{
	GdkGLContext *glContext;
	GdkGLDrawable *glDrawable;
	Lib3dsMesh * mesh;
	int i, cface; 
	int tfaces = 0;

	// Don't continue if we get fed a dud window type.
	if (copter->window == NULL) {
		return;
	}
	
	copter_model = lib3ds_file_load(copter_filename);
	// If loading the model failed, we throw an exception
	if (!copter_model) {
		fprintf(stderr, "open %s error\n", THREED_MODEL_FILENAME);
		return;
	}

	// Loop through every mesh
	for(mesh = copter_model->meshes; mesh != NULL; mesh = mesh->next) {
		// Add the number of faces this mesh has to the total faces
		copter_faces += mesh->faces;
	}
	copter_normals = (Lib3dsVector*)malloc(sizeof(Lib3dsVector) * copter_faces * 3);
	copter_vertices = (Lib3dsVector*)malloc(sizeof(Lib3dsVector) * copter_faces * 3);
	copter_colors = (Lib3dsRgba*)malloc(sizeof(Lib3dsRgba) * copter_faces);
	if (!copter_normals || !copter_vertices || !copter_colors)
		return;	
	// Loop through all the meshes
	for(mesh = copter_model->meshes; mesh != NULL; mesh = mesh->next) {
		lib3ds_mesh_calculate_normals(mesh, (copter_normals + 3 * tfaces));
		// Loop through every face
		for(cface = 0; cface < mesh->faces; cface++) {
			Lib3dsFace * face = &mesh->faceL[cface];
			Lib3dsMaterial *mat = NULL;
			mat = lib3ds_file_material_by_name(copter_model, face->material);
			if (mat)
				memcpy(copter_colors + tfaces, mat->diffuse, sizeof(Lib3dsRgba));
			for(i = 0;i < 3;i++) {
				memcpy(copter_vertices + tfaces * 3 + i, mesh->pointL[face->points[i]].pos, sizeof(Lib3dsVector));
			}
			tfaces++;
		}
	}
	lib3ds_file_free(copter_model);

	glContext = gtk_widget_get_gl_context (copter);
	glDrawable = gtk_widget_get_gl_drawable (copter);

	// Signal to gdk the start OpenGL operations.
	if (!gdk_gl_drawable_gl_begin (glDrawable, glContext)) {
		return;
	}

	/* Your one-time OpenGL initialization code goes here */
	glEnable(GL_TEXTURE_2D);		       // Enables Depth Testing
	glEnable(GL_DEPTH_TEST);		       // Enables Depth Testing
	glShadeModel(GL_SMOOTH);                        // Enables Smooth Color Shading

	glViewport (0, 0, copter->allocation.width, copter->allocation.height);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();				// Reset The Projection Matrix
	gluPerspective(45.0f,(GLfloat)copter->allocation.width/(GLfloat)copter->allocation.height,
			    0.1f,100.0f);	// Calculate The Aspect Ratio Of The Window
	glMatrixMode(GL_MODELVIEW);

	glEnable(GL_LIGHT0);
	glEnable(GL_LIGHTING);
	glEnable(GL_COLOR_MATERIAL);

	// Signal to gdk we're done with OpenGL operations.
	gdk_gl_drawable_gl_end (glDrawable);

	gdk_window_invalidate_rect (gtk_widget_get_parent_window (copter), &copter->allocation, TRUE);

}


/*
 * The "configure_event" signal handler for copter. 
 * When the area is reconfigured (such as a resize)
 * this gets called - so we change the OpenGL viewport accordingly,
 */
gboolean on_copter_configure_event (GtkWidget *copter, GdkEventConfigure *event, gpointer data)
{
	GdkGLContext *glContext;
	GdkGLDrawable *glDrawable;


	// Don't continue if we get fed a dud window type.
	if (copter->window == NULL) {
		return FALSE;
	}

	/*
	 * Wait until the rendering process is complete before issuing a context change,
	 * then lock it - to prevent possible driver issues on long rendering processes.
	 * You can add a timeout here or pthread_mutex_trylock according to taste.
	 */
	pthread_mutex_lock (&copter_render_mutex);


	glContext = gtk_widget_get_gl_context (copter);
	glDrawable = gtk_widget_get_gl_drawable (copter);

	// Signal to gdk the start OpenGL operations.
	if (!gdk_gl_drawable_gl_begin (glDrawable, glContext)) {
		pthread_mutex_unlock (&copter_render_mutex);
		return FALSE;
	}

	glViewport (0, 0, copter->allocation.width, copter->allocation.height);

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();

	gluPerspective(45.0f,(GLfloat)copter->allocation.width/(GLfloat)copter->allocation.height,
			    0.1f,100.0f);	// Calculate The Aspect Ratio Of The Window
	glMatrixMode(GL_MODELVIEW);

	// Signal to gdk we're done with OpenGL operations.
	gdk_gl_drawable_gl_end (glDrawable);

	// Unlock the rendering mutex before sending an immediate redraw request.
	pthread_mutex_unlock (&copter_render_mutex);
	gdk_window_invalidate_rect (gtk_widget_get_parent_window (copter), &copter->allocation, TRUE);
	gdk_window_process_updates (gtk_widget_get_parent_window (copter), TRUE);

	return TRUE;
}


/*
 * The "expose_event" signal handler for drawingarea1. All OpenGL re-drawing should be done here.
 * This is called every time the expose/draw event is signalled.
 *
 */
gboolean on_copter_expose_event (GtkWidget *copter, GdkEventExpose *event, gpointer data)
{
	GdkGLContext *glContext;
	GdkGLDrawable *glDrawable;

	int i = 0;
	Lib3dsRgba *color = NULL;	

	// Don't continue if we get fed a dud window type.
	if (copter->window == NULL) {
		return FALSE;
	}

	/*
	 * Lock rendering mutex if it's not busy - otherwise exit.
	 * This prevents the renderer being spammed by drawing requests 
	 * if the rendering takes longer than the feed/timer process.
	 */
	if (pthread_mutex_trylock (&copter_render_mutex) == EBUSY)
		return FALSE;

	glContext = gtk_widget_get_gl_context (copter);
	glDrawable = gtk_widget_get_gl_drawable (copter);

	// Signal to gdk the start OpenGL operations.
	if (!gdk_gl_drawable_gl_begin (glDrawable, glContext)) {
		pthread_mutex_unlock (&copter_render_mutex);
		return FALSE;
	}

	glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glPushMatrix ();
	glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);	// Clear The Screen And The Depth Buffer

	glLoadIdentity();				// make sure we're no longer rotated.
	glTranslatef(0.0f,0.0f,-20.0f);		// Move Right 3 Units, and back into the screen 7
	// scaling matrix
	glScalef(0.1f, 0.1f, 0.1f);

	glRotatef(yaw_angle + yaw_patch, 0.0f,1.0f,0.0f); // Rotate The Cube On Y
	glRotatef(pitch_angle, 1.0f,0.0f,0.0f);		// Rotate The Cube On X
	glRotatef(roll_angle, 0.0f,0.0f,1.0f);		// Rotate The Cube On Z

	while(i < copter_faces) {
		glBegin(GL_TRIANGLES);
		color = copter_colors + i;
		glColor4fv(*color);
		glNormal3fv(*(copter_normals + 3 * i));
#if 0
		glVertex3fv(copter_vertices[i * 3]);
		glVertex3fv(copter_vertices[i * 3 + 1]);
		glVertex3fv(copter_vertices[i * 3 + 2]);
#else
		// drawing face, be sure to exchange Y and Z cordinates
		glVertex3f(copter_vertices[i * 3][0], copter_vertices[i * 3][2], copter_vertices[i * 3][1]);
		glNormal3fv(*(copter_normals + 3 * i + 1));
		glVertex3f(copter_vertices[i * 3 + 1][0], copter_vertices[i * 3 + 1][2], copter_vertices[i * 3 + 1][1]);
		glNormal3fv(*(copter_normals + 3 * i + 2));
		glVertex3f(copter_vertices[i * 3 + 2][0], copter_vertices[i * 3 + 2][2], copter_vertices[i * 3 + 2][1]);
		glEnd();
#endif
		i++;
	}

	glPopMatrix ();

	/*
	 * Swap rendering buffers out (an auto glFlush is issued after) if we previously managed to initialise
	 * it as a double-buffered context, otherwise issue a standard glFlush call to signal we've finished
	 * OpenGL rendering.
	 */
	if (gdk_gl_drawable_is_double_buffered (glDrawable))
		gdk_gl_drawable_swap_buffers (glDrawable);
	else
		glFlush ();

	// Signal to gdk we're done with OpenGL operations.
	gdk_gl_drawable_gl_end (glDrawable);

	// Release the rendering mutex.
	pthread_mutex_unlock (&copter_render_mutex);

	return TRUE;
}

/*
 * The timer-based process that operates on some variables then invalidates copter's region to trigger
 * an expose_event, thus invoking function "on_copter_expose_event" through the window manager.
 *
 */
static gboolean render_timer_event (gpointer theUser_data)
{
	GdkWindow *parent;
	GtkWidget *theWidget = GTK_WIDGET (theUser_data);


	/*
	 * Only send a redraw request if the render process is not busy.  
	 * This prevents long rendering processes from piling up.
	 */
	if (pthread_mutex_trylock (&copter_render_mutex) != EBUSY)
	{
		// Unlock the mutex before we issue the redraw.
		pthread_mutex_unlock (&copter_render_mutex);
		// Invalidate 3D copter's region to signal the window handler there needs to be an update in that area,
		if (parent = gtk_widget_get_parent_window (theWidget))
			gdk_window_invalidate_rect (parent, &theWidget->allocation, TRUE);
		/*
		 * Force an immediate update - we can omit this and leave the window manager to call the redraw when the
		 * main loop is idle.  However, we get smoother, more consistent updates this way.
		 */
		if (parent = gtk_widget_get_parent_window (theWidget))
			gdk_window_process_updates (parent, TRUE);
	}

	return TRUE;
}

static gboolean update_accs_graph(gpointer data)
{
	graph_t *g = (graph_t*)data;

	if (accdata_present_index != accdata_process_index) {
		graph_force_update(g);
		ADD_ONE_WITH_WRAP_AROUND(accdata_process_index, MAX_ANALOGDATA_ENTRY);
	}
	return TRUE;
}

static gboolean update_gyros_graph(gpointer data)
{
	graph_t *g = (graph_t*)data;

	if (gyrodata_present_index != gyrodata_process_index) {
		graph_force_update(g);
		ADD_ONE_WITH_WRAP_AROUND(gyrodata_process_index, MAX_ANALOGDATA_ENTRY);
	}
	return TRUE;
}

/*
 * destroy (program quits)
 */
void destroy (GtkWidget *widget, gpointer data)
{
	gtk_main_quit ();
	serial_close(&serial);
	mx_destroy(&mx);
	if (copter_normals)
		free((void*)copter_normals);
	if (copter_vertices)
		free((void*)copter_vertices);
	if (copter_colors)
		free((void*)copter_colors);
}

/*
 * main
 */
int main (int argc, char *argv[])
{

	GdkGLConfig *glConfig;
	GtkWidget *mainWindow;
	GtkWidget *copterDrawingArea;

	extern int optind;
	extern int optopt;
	extern int opterr;
	extern int optreset;

	char *optstr="d:m:s:h";
	char *sdev = NULL;
	int sspeed = -1;
	int opt = 0;

	/*
	 * Init argument control
	 */
	opt = getopt( argc, argv, optstr);
	while( opt != -1 ) {
		switch( opt ) {
		case 'd':
			sdev = optarg;
			break;
		case 'm':
			copter_filename = optarg;
		case 's':
			sspeed = atoi(optarg);
			break;
		case 'h':
			usage();
			return 0;
		default:
			break;
		}
		opt = getopt(argc, argv, optstr);
	}

	if (!g_thread_supported ()) { 
		g_thread_init (NULL); 
	}
	gdk_threads_init ();
	gdk_threads_enter ();

	/*
	 * Init GTK+ and GtkGLExt.
	 */
	gtk_init (&argc, &argv);
	gtk_gl_init (&argc, &argv);

	/*
	 * Configure a OpenGL-capable context.
	 */
	// Try to make it double-buffered.
	glConfig = gdk_gl_config_new_by_mode (GDK_GL_MODE_RGB | GDK_GL_MODE_DEPTH | GDK_GL_MODE_ALPHA | GDK_GL_MODE_DOUBLE);
	if (glConfig == NULL)
	{
		g_print ("Cannot configure a double-buffered context.\n");
		g_print ("Will try a single-buffered context.\n");

		// If we can't configure a double-buffered context, try for single-buffered.
		glConfig = gdk_gl_config_new_by_mode (GDK_GL_MODE_RGB | GDK_GL_MODE_DEPTH | GDK_GL_MODE_ALPHA);
		if (glConfig == NULL)
		{
			g_critical ("Aargh!  Cannot configure any type of OpenGL-capable context.  Exiting.\n");
			return -1;
		}
	}
	/*
	 * Load the GTK interface.
	 */
	theXml = gtk_builder_new ();
	gtk_builder_add_from_file (theXml, "amcc.glade", NULL);
	if (theXml == NULL)
	{
		g_critical ("Failed to load an initialise the GTK file.\n");
		return -1;
	}

	/*
	 * Get the top-level window reference from loaded Glade file.
	 */
	mainWindow = GTK_WIDGET (gtk_builder_get_object (theXml, "windowMain"));

	// Set unassigned widgets to get handled automatically by the window manager.
	gtk_container_set_reallocate_redraws (GTK_CONTAINER (mainWindow), TRUE);
	/*
	 * Get the drawing area's reference from the loaded Glade file which we are going to use for OpenGL rendering.
	 */
	copterDrawingArea = GTK_WIDGET (gtk_builder_get_object (theXml, "copter"));

	// Add OpenGL-capability to the drawing area.
	gtk_widget_set_gl_capability (copterDrawingArea, glConfig, NULL, TRUE, GDK_GL_RGBA_TYPE);

	// Initialise the render mutex.
	pthread_mutex_init (&copter_render_mutex, NULL);

	/*
	 * Get the window manager to connect any assigned signals in the loaded Glade file to our coded functions.
	 */
	gtk_builder_connect_signals (theXml, NULL);

	/*
	 * Init channel graph
	 */
	graph_init(&acc_graph, mainWindow, 3, 0, acc_graph_callback);
	graph_set_channel_name(&acc_graph, 1, "Acc_X");
	graph_set_channel_color(&acc_graph, 1, "#FF0000");
	graph_set_channel_name(&acc_graph, 2, "Acc_Y");
	graph_set_channel_color(&acc_graph, 2, "#00FF00");
	graph_set_channel_name(&acc_graph, 3, "Acc_Z");
	graph_set_channel_color(&acc_graph, 3, "#0000FF");
	gtk_box_pack_start (GTK_BOX(GTK_WIDGET (gtk_builder_get_object (theXml, "vbox2"))), 
			    graph_get_widget(&acc_graph),
			    TRUE, TRUE, 0);	graph_set_data(&acc_graph, 0, 3300);

	graph_init(&gyro_graph, mainWindow, 3, 0, gyro_graph_callback);
	graph_set_channel_name(&gyro_graph, 1, "Gyro_X");
	graph_set_channel_color(&gyro_graph, 1, "#FF0000");
	graph_set_channel_name(&gyro_graph, 2, "Gyro_Y");
	graph_set_channel_color(&gyro_graph, 2, "#00FF00");
	graph_set_channel_name(&gyro_graph, 3, "Gyro_Z");
	graph_set_channel_color(&gyro_graph, 3, "#0000FF");
	gtk_box_pack_start (GTK_BOX(GTK_WIDGET (gtk_builder_get_object (theXml, "vbox3"))), 
			    graph_get_widget(&gyro_graph),
			    TRUE, TRUE, 0);	graph_set_data(&gyro_graph, 0, 3300);

	mx_init(&mx, serial_tx_data, (void*)&serial);
	serial_init(&serial, mx_rx_data, &mx);
	if (!sdev)
		sdev = DEFAULT_SERIAL_DEV; 
	if (sspeed == -1)
		sspeed = 57600;
	serial_open(&serial, sdev, sspeed);

	/*
	 * Show main window.
	 */
	gtk_widget_show (mainWindow);
	// attitude init;
	attitude_init(&attitude);
	// Start the render timer.
	g_timeout_add (1000 / 10, render_timer_event, copterDrawingArea);
	g_timeout_add (1000 / 10, update_accs_graph, &acc_graph);
	g_timeout_add (1000 / 10, update_gyros_graph, &gyro_graph);

	// Run the window manager loop.
	gtk_main ();

	gdk_threads_leave ();

	return 0;
}
