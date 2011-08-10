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

#include <stdio.h>
#include <math.h>
#include <sys/stat.h>
#include <unistd.h>
#include <signal.h>
#include <dirent.h>
#include <string.h>
#include <time.h>

#include <gtk/gtk.h>
#include <gdk/gdkx.h>

#include "amcc.h"
#include "graph.h"

/*
 * static variables
 */

static guchar *color_depository[] = {
	"#FF0000",
	"#00FF00",
	"#0000FF",
	"#FFFF00",
	"#FF00FF",
	"#00FFFF",
	"#FFFFFF",
	"#000000",
};

/*
 * static function declarations
 */

static gfloat graph_get_data(graph_t*);
static void graph_rotate_data(graph_t*);
static gint graph_convert_data(graph_t*, gfloat[][NUM_POINTS]);
static unsigned graph_num_bars(graph_t*);
static void graph_clear_background(graph_t*);
static void graph_draw_background(graph_t *gph);
static gboolean graph_is_polling_start (graph_t *gph);
static gboolean graph_update (gpointer user_data);
static void graph_draw(graph_t *gph);
static gboolean graph_configure(GtkWidget*, GdkEventConfigure*, gpointer);
static gboolean graph_expose(GtkWidget*, GdkEventExpose*, gpointer);
static void graph_generate_test_data(graph_t*);
static void graph_destroy (GtkWidget*, gpointer);


static void channel_color_changed (GtkColorButton *button, 
					GtkWidget *data_ptr) 
{
	int i; 
	const gchar *title;
	GdkColor *color, *remove; 
	graph_t *gph = (graph_t*)data_ptr;

	title = gtk_color_button_get_title (button);
	i = atoi(title + strlen("Channel"));
	color = (GdkColor*)g_malloc(sizeof(GdkColor));
	gtk_color_button_get_color (button, color);
	g_array_insert_val(gph->colors, i - 1, color);
	remove = g_array_index(gph->colors, GdkColor*, i);
 	g_array_remove_index(gph->colors, i);
	g_free(remove);
 }

/* Updates the load graph when the timeout expires */
static gfloat graph_get_data(graph_t *gph)
{
	GRand *rand;	
	guint r;	

	rand = g_rand_new ();	
	r = g_rand_int_range(rand, 30, 50);		
	g_rand_free(rand);	

	return (gfloat)r / 100.0 ;	
}

static void graph_rotate_data(graph_t *gph)
{
	guint i, j;

	for (i = 0; i < gph->channel; i++) {
		for (j = 0; j < NUM_POINTS - 1; j++) {
			gph->data[i][NUM_POINTS - 1 - j] = gph->data[i][NUM_POINTS - 1 - j - 1];
		}
	}
}

static gint graph_convert_data(graph_t *gph , gfloat data[][NUM_POINTS])
{
	guint i, j;

	for (i = 0; i < MAX_CHANNEL_NUMBER; i++) {
		for (j = 0; j < NUM_POINTS; j++) {
			if (gph->data[i][j] == EMPTY_DATA) {
				data[i][j] = EMPTY_DATA;
			} else {
				data[i][j] = (gph->data[i][j] - gph->min) / (gph->max - gph->min);
			}
		}
	}

	return 0;
}

static unsigned graph_num_bars(graph_t *gph)
{
	unsigned n;

	switch((int)(gph->draw_height / (gph->fontsize + 14))) {
	case 0:
	case 1:
		n = 1;
		break;
	case 2:
	case 3:
		n = 2;
		break;
	case 4:
	case 5:
		n = 4;
		break;
	case 6:
	case 7:
		n = 6;
	case 8:
	case 9:
		n = 8;
	default:
		n = 10;
	}

	return n;
}

static void graph_clear_background(graph_t *gph)
{
	if (gph->background) {
		g_object_unref(gph->background);
		gph->background = NULL;
	}
}

static void graph_draw_background(graph_t *gph)
{
	double dash[2] = { 1.0, 2.0 };
	cairo_t *cr;
	gint scale;
	guint i;
	unsigned num_bars;
	char *caption;
	cairo_text_extents_t extents;

	num_bars = graph_num_bars(gph);
	gph->graph_dely = (gph->draw_height - 15) / num_bars; /* round to int to avoid AA blur */
	gph->real_draw_height = gph->graph_dely * num_bars;
	gph->graph_delx = (gph->draw_width - 2.0 - gph->rmargin - gph->indent) / (NUM_POINTS - 3);
	gph->graph_buffer_offset = (int) (1.5 * gph->graph_delx) + FRAME_WIDTH ;


	gph->background = gdk_pixmap_new (GDK_DRAWABLE (gph->disp->window),
					gph->disp->allocation.width,
					gph->disp->allocation.height,
					-1);
	cr = gdk_cairo_create (gph->background);

	/* set the background colour */
	GtkStyle *style = gtk_widget_get_style (gph->main_widget);
	gdk_cairo_set_source_color (cr, &style->bg[GTK_STATE_NORMAL]);
	cairo_paint (cr);
	/* draw frame */
	cairo_translate (cr, FRAME_WIDTH, FRAME_WIDTH);
	/* Draw background rectangle */
	cairo_set_source_rgb (cr, 1.0, 1.0, 1.0);
	cairo_rectangle (cr, gph->rmargin + gph->indent, 0,
			 gph->draw_width - gph->rmargin - gph->indent, gph->real_draw_height);
	cairo_fill(cr);
	cairo_set_line_width (cr, 1.0);
	cairo_set_dash (cr, dash, 2, 0);
	cairo_set_line_cap (cr, CAIRO_LINE_CAP_ROUND);
	cairo_set_line_join (cr, CAIRO_LINE_JOIN_ROUND);
	cairo_set_font_size (cr, gph->fontsize);

	for (i = 0; i <= num_bars; ++i) {
		double y;

		if (i == 0)
			y = 0.5 + gph->fontsize / 2.0;
		else if (i == num_bars)
			y = i * gph->graph_dely + 0.5;
		else
			y = i * gph->graph_dely + gph->fontsize / 2.0;

		gdk_cairo_set_source_color (cr, &style->fg[GTK_STATE_NORMAL]);

		scale = gph->max - i * (gint)((gph->max - gph->min) / num_bars);
		caption = g_strdup_printf("%d", scale);

		cairo_text_extents (cr, caption, &extents);
		cairo_move_to (cr, gph->indent - extents.width + 20, y);
		cairo_show_text (cr, caption);
		g_free (caption);

		cairo_set_source_rgba (cr, 0, 0, 0, 0.75);
		cairo_move_to (cr, gph->rmargin + gph->indent, i * gph->graph_dely + 0.5);
		cairo_line_to (cr, gph->draw_width - 0.5, i * gph->graph_dely + 0.5);
	}
	cairo_stroke (cr);
	cairo_set_dash (cr, dash, 2, 0);

	const unsigned total_points = (NUM_POINTS - 2);

	for (i = 0; i < 7; i++) {
		double x = (i) * (gph->draw_width - gph->rmargin - gph->indent) / 6;
		cairo_set_source_rgba (cr, 0, 0, 0, 0.75);
		cairo_move_to (cr, (ceil(x) + 0.5) + gph->rmargin + gph->indent, 0.5);
		cairo_line_to (cr, (ceil(x) + 0.5) + gph->rmargin + gph->indent, gph->real_draw_height);
		cairo_stroke(cr);
		unsigned points = total_points - i * total_points / 6;
		const char* format;
		if (i == 0)
			format = dngettext("graph", "%u point", "%u points", points);
		else
			format = "%u";
		caption = g_strdup_printf(format, points);
		cairo_text_extents (cr, caption, &extents);
		cairo_move_to (cr, ((ceil(x) + 0.5) + gph->rmargin + gph->indent) - (extents.width/2), gph->draw_height);
		gdk_cairo_set_source_color (cr, &style->fg[GTK_STATE_NORMAL]);
		cairo_show_text (cr, caption);
		g_free (caption);
	}

	cairo_stroke (cr);
	cairo_destroy (cr);
}

static void graph_draw(graph_t *gph)
{
	gtk_widget_queue_draw(gph->disp);
}

static gboolean graph_configure(GtkWidget *widget,
		      GdkEventConfigure *event,
		      gpointer data_ptr)
{
	graph_t *gph = (graph_t*)data_ptr;

	gph->draw_width = widget->allocation.width - 2 * FRAME_WIDTH;
	gph->draw_height = widget->allocation.height - 2 * FRAME_WIDTH;

	graph_clear_background(gph);
	if (gph->gc == NULL) {
		gph->gc = gdk_gc_new (GDK_DRAWABLE (widget->window));
	}
	graph_draw(gph);

	return TRUE;
}
static gboolean graph_expose(GtkWidget *widget,
		   GdkEventExpose *event,
		   gpointer data_ptr)
{
	guint i, j;
	gdouble sample_width, x_offset;
	GdkColor *color;

	graph_t *gph = (graph_t*)data_ptr;
	if (gph->background == NULL) {
		graph_draw_background(gph);
	}
	gdk_draw_drawable (gph->disp->window,
				gph->gc,
				gph->background,
				0, 0, 0, 0,
				gph->disp->allocation.width,
				gph->disp->allocation.height);
#ifdef DRAW_CURVE
	/* Number of pixels wide for one graph point */
	sample_width = (float)(gph->draw_width - gph->rmargin - gph->indent) / (float)NUM_POINTS;
	/* General offset */
	x_offset = gph->draw_width - gph->rmargin + (sample_width*2);
	/* Subframe offset */
	x_offset += gph->rmargin - ((sample_width / gph->frames_per_unit) * gph->render_counter);
#else
	x_offset = gph->draw_width + 6.0;
#endif
	/* draw the graph */
	cairo_t* cr;

	cr = gdk_cairo_create (gph->disp->window);
	cairo_set_line_width (cr, 1.5);
	cairo_set_line_cap (cr, CAIRO_LINE_CAP_ROUND);
	cairo_set_line_join (cr, CAIRO_LINE_JOIN_ROUND);
	cairo_rectangle (cr, gph->rmargin + gph->indent + FRAME_WIDTH + 1, FRAME_WIDTH - 1,
			 gph->draw_width - gph->rmargin - gph->indent - 1, gph->real_draw_height + FRAME_WIDTH - 1);
	cairo_clip(cr);

	gfloat data[MAX_CHANNEL_NUMBER][NUM_POINTS];

	graph_convert_data(gph, data);
	for (j = 0; j < gph->channel; j++) {
		cairo_move_to (cr, x_offset, (1.0f - data[j][0]) * gph->real_draw_height);
		color = g_array_index(gph->colors, GdkColor*, j);
		gdk_cairo_set_source_color (cr, color);
		for (i = 1; i < NUM_POINTS; i++) {
			if (data[j][i] == EMPTY_DATA)
				continue;
#ifdef DRAW_CURVE
			cairo_curve_to (cr, 
				       x_offset - ((i - 0.5f) * gph->graph_delx),
				       (1.0f - data[j][i-1]) * gph->real_draw_height + 3.5f,
				       x_offset - ((i - 0.5f) * gph->graph_delx),
				       (1.0f - data[j][i]) * gph->real_draw_height + 3.5f,
				       x_offset - (i * gph->graph_delx),
				       (1.0f - data[j][i]) * gph->real_draw_height + 3.5f);
#else
			cairo_line_to (cr, x_offset - (i * gph->graph_delx),
				       (1.0f - data[j][i]) * gph->real_draw_height + 3.5f);
#endif
		}
		cairo_stroke (cr);
	}
	cairo_destroy (cr);

	return TRUE;
}

static void graph_destroy (GtkWidget *widget, gpointer data_ptr)
{
	graph_t *gph = (graph_t*)data_ptr;

	graph_polling_stop(gph);
	g_array_free(gph->labels, FALSE);
	g_array_free(gph->buttons, FALSE);
	g_array_free(gph->colors, TRUE);
}

static gboolean graph_is_polling_start (graph_t *gph)
{
	return gph->draw;
}

void graph_polling_start (graph_t *gph)
{
	if(!gph->timer_index) {
		//graph_update(g);
		gph->timer_index = g_timeout_add (gph->speed / gph->frames_per_unit,
							graph_update,
							gph);
	}

	gph->draw = TRUE;
}

void graph_polling_stop(graph_t *gph)
{
	if (graph_is_polling_start(gph)) {
		gph->draw = FALSE;
		g_source_remove (gph->timer_index);
		gph->timer_index = 0;
	}
}


void graph_get_polling_speed(graph_t *gph, guint *speed)
{
	*speed = gph->speed;
}

gint graph_set_polling_speed(graph_t *gph, guint speed)
{
	if (speed < MIN_MONITOR_SPEED)
		return -1;
	gph->speed = speed;
	if (graph_is_polling_start(gph)) {
		graph_polling_stop(gph);
		graph_polling_start(gph);
	}
	return 0;
}

gint graph_set_data(graph_t *gph , gfloat min, gfloat max)
{
	if (min >= max)
		return -1;

	gph->min = min;
	gph->max = max;
	graph_clear_background(gph);

	return 0;
}

gint graph_set_channel_name(graph_t *gph, guint channel, gchar *name)
{
	GdkColor *c;
	GtkWidget *button, *label;

	if (name != NULL) {
		label = g_array_index(gph->labels, GtkWidget*, channel - 1);
		gtk_label_set_text (GTK_LABEL(label), name);
	}

	return 0;
}

gint graph_set_channel_color(graph_t *gph, guint channel, gchar* color)
{
	GdkColor *c;
	GtkWidget *button, *label;

	if (color != NULL) {
		c = (GdkColor*)g_malloc(sizeof(GdkColor));
		if (TRUE != gdk_color_parse (color, c)) {
			g_free(c);
			return -1;
		}
		button = g_array_index(gph->buttons, GtkWidget*, channel - 1);
		gtk_color_button_set_color (GTK_COLOR_BUTTON(button), c);
		// update color array
		g_array_insert_val(gph->colors, channel - 1, c);
		c = g_array_index(gph->colors, GdkColor*, channel);
	 	g_array_remove_index(gph->colors, channel);
	 	g_free(c);
	}

	return 0;
}

static gboolean graph_update (gpointer user_data)
{
	guint i;
	graph_t *gph = (graph_t*)user_data;

	if (gph->render_counter == gph->frames_per_unit - 1) {
		graph_rotate_data(gph);
		for (i = 0; i < gph->channel; i++) {
			if (gph->callback == NULL) {
				gph->data[i][0] = EMPTY_DATA;
			} else {
				gph->callback(gph, i + 1, &gph->data[i][0]);
			}
		}
	}

	if (gph->draw)
		graph_draw(gph);
	gph->render_counter++;
	if (gph->render_counter >= gph->frames_per_unit)
		gph->render_counter = 0;

	return TRUE;
}

void graph_force_update (gpointer user_data)
{
	guint i;
	graph_t *gph = (graph_t*)user_data;

	gph->render_counter = gph->frames_per_unit - 1;
	if (gph->render_counter == gph->frames_per_unit - 1) {
		graph_rotate_data(gph);
		for (i = 0; i < gph->channel; i++) {
			if (gph->callback == NULL) {
				gph->data[i][0] = EMPTY_DATA;
			} else {
				gph->callback(gph, i + 1, &gph->data[i][0]);
			}
		}
	}

	graph_draw(gph);
	gph->render_counter++;
	if (gph->render_counter >= gph->frames_per_unit)
		gph->render_counter = 0;

	return;
}

void graph_data_clear (gpointer user_data)
{
	graph_t *gph = (graph_t*)user_data;
	guint i, j;

	for (j = 0; j < gph->channel; j++) {
		for (i = 0; i < NUM_POINTS; i++) {
			gph->data[j][i] = EMPTY_DATA;
		}
	}
}

GtkWidget* graph_get_widget(graph_t *gph)
{
	return gph->box;
}

void graph_init(graph_t *gph, GtkWidget *window,
			guint channel, guint speed,
			GRAPH_CALLBACK callback)
{
	guint i, j;
	guchar *index;
	GdkColor *color;
	GtkWidget *button, *hbox;
	GtkWidget *label, *table, *channel_label, *spacer;

	gph->main_widget = window;
	gph->channel = (channel > MAX_CHANNEL_NUMBER) ?
				    DEFAULT_CHANNEL_NUMBER : channel;
	gph->speed = (speed > MIN_MONITOR_SPEED) ?
				    speed : DEFAULT_SPEED;
	gph->callback = callback;
	gph->min = DEFAULT_DATA_MIN;
	gph->max = DEFAULT_DATA_MAX;
	gph->frames_per_unit = 10;  // this will be changed but needs initialising
	gph->fontsize = DEFAULT_FONT_SIZE;
	gph->rmargin = 3.5 * gph->fontsize;
	gph->indent = 24.0;
	gph->gc = NULL;
	gph->background = NULL;
	gph->render_counter = (gph->frames_per_unit - 1);
	gph->timer_index = 0;
	gph->draw = FALSE;
	gph->box = gtk_vbox_new (FALSE, 5);

	gph->colors = g_array_new(FALSE, TRUE, sizeof(GdkColor*));
	for (i = 0; i < channel; i++) {
		color = (GdkColor*)g_malloc(sizeof(GdkColor));
		index = color_depository[i % (sizeof(color_depository) / sizeof(guchar*))];
		gdk_color_parse (index, color); 		
		g_array_append_val(gph->colors, color);
	}
	for (j = 0; j < gph->channel; j++) {
		for (i = 0; i < NUM_POINTS; i++) {
			gph->data[j][i] = EMPTY_DATA;
		}
	}

	gph->disp = gtk_drawing_area_new ();
	gtk_box_pack_start (GTK_BOX(gph->box), gph->disp, TRUE, TRUE, 0);

	hbox = gtk_hbox_new (FALSE, 0);
	spacer = gtk_label_new ("");
	gtk_widget_set_size_request (GTK_WIDGET(spacer), (gph->rmargin + gph->indent), -1);
	gtk_box_pack_start (GTK_BOX (hbox), spacer, 
			    FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (gph->box), hbox, 
			    FALSE, FALSE, 0);
	GtkWidget* cpu_table = gtk_table_new (2, 2, TRUE);
	gtk_table_set_row_spacings (GTK_TABLE(cpu_table), 6);
	gtk_table_set_col_spacings (GTK_TABLE(cpu_table), 6);
	gtk_box_pack_start (GTK_BOX(hbox), cpu_table, FALSE, FALSE, 0);

	gph->buttons = g_array_new(FALSE, TRUE, sizeof(GtkWidget*));
	gph->labels = g_array_new(FALSE, TRUE, sizeof(GtkWidget*));

	for (i = 0; i < gph->channel; i++) {
		GtkWidget *temp_hbox;
		gchar *text;
		
		temp_hbox = gtk_hbox_new (FALSE, 0);
		gtk_table_attach (GTK_TABLE (cpu_table), temp_hbox,
				 i % 6, i % 6 + 1,
				 i / 6, i / 6 + 1,
				 (GtkAttachOptions) (GTK_EXPAND | GTK_FILL), 
				 (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
				 0, 0);

		color = g_array_index(gph->colors, GdkColor*, i); 
		button = gtk_color_button_new_with_color (color); 
		g_array_append_val(gph->buttons, button);
		gtk_box_pack_start (GTK_BOX (temp_hbox), button, FALSE, TRUE, 0);
		gtk_widget_set_size_request (GTK_WIDGET (button), 32, -1);
		text = g_strdup_printf ("Channel%d", i + 1);
		label = gtk_label_new (text);
		g_array_append_val(gph->labels, label);
		gtk_box_pack_start (GTK_BOX (temp_hbox), label, FALSE, FALSE, 6);
		gtk_color_button_set_title (GTK_COLOR_BUTTON(button), text);
		g_free (text);

		channel_label = gtk_label_new (NULL);
		gtk_misc_set_alignment (GTK_MISC (channel_label), 0.0, 0.5);
		gtk_box_pack_start (GTK_BOX (temp_hbox), channel_label, TRUE, TRUE, 0);
		
		g_signal_connect (G_OBJECT (button), "color_set",
				    G_CALLBACK (channel_color_changed), (gpointer) gph); 
	}

	g_signal_connect (G_OBJECT(gph->disp), "expose_event",
			  G_CALLBACK (graph_expose), gph);
	g_signal_connect (G_OBJECT(gph->disp), "configure_event",
			  G_CALLBACK (graph_configure), gph);

	g_signal_connect (G_OBJECT(gph->disp), "destroy",
			  G_CALLBACK (graph_destroy), gph);

	gtk_widget_set_events (gph->disp, GDK_EXPOSURE_MASK);

	gtk_widget_show_all (gph->box);

}


