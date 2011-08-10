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

#ifndef GRAPH_H_
#define GRAPH_H_

#include <glib/gtypes.h>

/*
 * macro 
 */

#undef DRAW_CURVE 
#define RANDOM_DATA_DEMO 0
#define EMPTY_DATA -1.0f
#define DEFAULT_FONT_SIZE 10.0f
#define NUM_POINTS 62
#define GRAPH_MIN_HEIGHT 40;
#define FRAME_WIDTH 8

#define DEFAULT_SPEED 1000
#define MIN_MONITOR_SPEED 100

#define DEFAULT_CHANNEL_NUMBER 1
#define MAX_CHANNEL_NUMBER 10

#define DEFAULT_DATA_MIN 0.0f
#define DEFAULT_DATA_MAX 100.0f

/*
 * data structure 
 */

struct graph_struct;
typedef struct graph_struct graph_t;
typedef gint (*GRAPH_CALLBACK)(graph_t *gph, guint channel, gfloat *data);

struct graph_struct {
	double fontsize;
	double rmargin;
	double indent;

	guint n;
	gint type;
	guint speed;
	guint channel;
	guint draw_width, draw_height;
	guint render_counter;
	guint frames_per_unit;
	guint graph_dely;
	guint real_draw_height;
	double graph_delx;
	guint graph_buffer_offset;
	GArray *colors;
	GArray *buttons;
	GArray *labels;
/*
	std::vector<GdkColor> colors;
	std::vector<float> data_block;
*/
	gfloat data[MAX_CHANNEL_NUMBER][NUM_POINTS];
	gfloat min;
	gfloat max;
	GtkWidget *main_widget;
	GtkWidget *disp;
	GtkWidget *box;

	GdkGC *gc;
	GdkDrawable *background;

	guint timer_index;

	gboolean draw;

	GtkWidget *mem_color_picker;
	GtkWidget *swap_color_picker;
	GRAPH_CALLBACK callback;
};

/*
 * core functions
 */

extern void graph_init(graph_t *gph, GtkWidget *window, guint channel, 
				guint speed, GRAPH_CALLBACK callback);
extern void graph_force_update(gpointer user_data);
extern GtkWidget* graph_get_widget(graph_t *gph);
/*
 * polling mode functions
 */
extern void graph_data_clear(gpointer user_data);
extern void graph_polling_start(graph_t *gph);
extern void graph_polling_stop(graph_t *gph);
extern void graph_get_polling_speed(graph_t *gph, guint *speed);
extern gint graph_set_polling_speed(graph_t *gph, guint speed);
/*
 * channel property functions
 */
extern gint graph_set_data(graph_t *gph , gfloat min, gfloat max);
extern gint graph_set_channel_name(graph_t *gph, guint channel, gchar *name);
extern gint graph_set_channel_color(graph_t *gph, guint channel, gchar* color);

#endif /* GRAPH_H_ */
