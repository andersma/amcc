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

#ifndef MX_H_
#define MX_H_

#include <pthread.h>
#include <glib.h>
#include "packet.h"

/*
 * macro 
 */

#define RX_POOL_LENGTH (MAX_PACKET_DATA_LENGTH * 5)
#define RX_BUFFER_LENGTH 10
#define TX_BUFFER_LENGTH 10

/*
 * data structure 
 */

struct mx_struct;
typedef struct mx_struct mx_t;
typedef gint (*RX_CALLBACK)(packet_t *p, void *arg);
typedef gint (*TX_DATA)(void *tx_interface, gchar *buffer, guint length);

typedef struct _RxHandler {
	PACKET_TYPE type;
	RX_CALLBACK callback;
	void *arg;
} RxHandler;

struct mx_struct {
	pthread_t thread_rx;
	gchar rx_pool[RX_POOL_LENGTH];
	guint rx_pool_index;
	packet_t rx_buffer[RX_BUFFER_LENGTH];
	guint rx_process_index; /* zero based */	
	guint rx_present_index; /* zero based */	
	pthread_mutex_t rx_pool_mutex;
	GSList *rx_callback_list;

	pthread_t thread_tx;
	packet_t tx_buffer[TX_BUFFER_LENGTH];
	guint tx_process_index; /* zero based */	
	guint tx_present_index; /* zero based */		
	pthread_mutex_t tx_buffer_mutex;
	void *tx_interface;
	TX_DATA tx_data;

	pthread_mutex_t rx_dispatch_mutex;
	gboolean thread_start;
};

/*
 * functions
 */

extern void mx_init(mx_t *m, TX_DATA tx_data, void *arg);
extern void mx_destroy(mx_t *m);
extern gint mx_rx_data(mx_t *m, gchar *buffer, guint length);
extern gint mx_rx_register(mx_t *m, PACKET_TYPE type, RX_CALLBACK callback, void *arg);
extern gint mx_rx_unregister(mx_t *m, PACKET_TYPE type, RX_CALLBACK callback);
extern gint mx_tx_packet(mx_t *m, packet_t *p);

#endif
