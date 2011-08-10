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

#ifndef SERIAL_H_
#define SERIAL_H_

#include <pthread.h>
#include <glib/gtypes.h>
#include "mx.h"


/*
 * macro 
 */

#define MAX_SERIAL_NAME_LENGTH 15

/*
 * data structure 
 */

struct serial_struct;
typedef struct serial_struct serial_t;
typedef gboolean (*RX_HANDLER)(mx_t *mx, gchar *buffer, guint length);

struct serial_struct {
	gint fd;
	gchar name[MAX_SERIAL_NAME_LENGTH];
	guint baudrate;
	pthread_t thread_rx;
	RX_HANDLER rx_handler;
	gboolean active;
	mx_t *mx;
	/* ALWAYS USE 8N1 MODE, NO HW FLOW CONTROL */
};

/*
 * functions
 */

extern void serial_init(serial_t *s, RX_HANDLER rx_handler, mx_t *mx);
extern gint serial_open(serial_t *s, gchar *name, guint baudrate);
extern gint serial_tx_data(void *p, gchar *buffer, guint length);
extern gint serial_close(serial_t *s);

#endif
