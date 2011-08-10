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
#include <string.h>
#include <assert.h>

#include "amcc.h"
#include "mx.h"

static void mx_rx_packet_dispatch(mx_t *m, packet_t *p)
{
	RxHandler *h;
	GSList *l;

	pthread_mutex_lock(&m->rx_dispatch_mutex);
	for (l = m->rx_callback_list; l != NULL; l = g_slist_next (l)) {
		h = (RxHandler*)l->data;
		if (h->type == p->type) {
			h->callback(p, h->arg);
		}
	}
	pthread_mutex_unlock(&m->rx_dispatch_mutex);
}
 
static void* mx_rx_thread(void *data)
{
	guint i;
	gint ret;
	mx_t *m = (mx_t*)data;
	gchar *pointer, *start, *end;

	while (1) {
		if (m->thread_start == FALSE)
			return;
		/* 
		 * scan rx pool
		 */
		pointer = m->rx_pool;
		start = end = NULL;
		pthread_mutex_lock(&m->rx_pool_mutex);
		while (pointer < m->rx_pool + m->rx_pool_index) {
			if (m->thread_start == FALSE) {
				pthread_mutex_unlock(&m->rx_pool_mutex);
				return;
			}
			switch (*pointer) {
			case PACKET_START :
				start = pointer;
				break;
			case PACKET_END :
				end = pointer;
				if (start == NULL) {
					break;
				}
				memcpy(m->rx_buffer[m->rx_present_index].data, start, end - start + 1);
				m->rx_buffer[m->rx_present_index].data_length =  end - start + 1;
				ret = packet_decode(&m->rx_buffer[m->rx_present_index]);
				if (ret == PACKET_SUCCESS) {
					ADD_ONE_WITH_WRAP_AROUND(m->rx_present_index, RX_BUFFER_LENGTH);
				}
				break;
			}
			if (end != NULL) {
				/* re-arrange rx pool */
				memcpy(m->rx_pool, end + 1,
							m->rx_pool_index - (end + 1 - m->rx_pool));
				m->rx_pool_index -= end + 1 - m->rx_pool;
				pointer = m->rx_pool;
				start = end = NULL;
			} else {
				pointer++;
			}
		}
		pthread_mutex_unlock(&m->rx_pool_mutex);
		/* check rx_buffer */
		while (m->rx_process_index != m->rx_present_index) {
			/* pass packet up */
			mx_rx_packet_dispatch(m, &m->rx_buffer[m->rx_process_index]);
			ADD_ONE_WITH_WRAP_AROUND(m->rx_process_index, RX_BUFFER_LENGTH);
		}
	}
}

static void* mx_tx_thread(void *data)
{
	gint ret;
	mx_t *m = (mx_t*)data;

	/* check tx buffer */
	while(1) {
		if (m->thread_start == FALSE)
			return;
		if (m->tx_process_index != m->tx_present_index) {
			pthread_mutex_lock(&m->tx_buffer_mutex);
			ret = packet_encode(&m->tx_buffer[m->tx_process_index]);
			if (ret == PACKET_SUCCESS) {
				/* send packet */
				m->tx_data(m->tx_interface, m->tx_buffer[m->tx_process_index].data,
							m->tx_buffer[m->tx_process_index].data_length);
			}
			ADD_ONE_WITH_WRAP_AROUND(m->tx_process_index, TX_BUFFER_LENGTH);
			pthread_mutex_unlock(&m->tx_buffer_mutex);
		}
	}
}

static void mx_start_threads(mx_t *m)
{
	m->thread_start = TRUE;
	/* start rx/tx thread */
	pthread_create( &m->thread_rx, NULL, mx_rx_thread, (void*)m);
	pthread_create( &m->thread_tx, NULL, mx_tx_thread, (void*)m);
}

static void mx_stop_threads(mx_t *m)
{
	m->thread_start = FALSE;
}
 
/*
 * called by interface layer , eg : serial/ethernet/etc..
 * should be a callback function for interface module
 */
gint mx_rx_data(mx_t *m, gchar *buffer, guint length)
{
	pthread_mutex_lock(&m->rx_pool_mutex);
	if (m->rx_pool_index + length > RX_POOL_LENGTH) {
		if (length < RX_POOL_LENGTH) {
			memcpy(m->rx_pool, buffer, length);
			m->rx_pool_index = length;
		}
		else
			return -1;
	} else {
		memcpy(m->rx_pool + m->rx_pool_index, buffer, length);
		m->rx_pool_index +=  length;
	}
	pthread_mutex_unlock(&m->rx_pool_mutex);

	return 0;
}

gint mx_rx_register(mx_t *m, PACKET_TYPE type, RX_CALLBACK callback, void *arg)
{
	RxHandler *h;

	/* prevent saving same callback more entrys */
	mx_rx_unregister(m, type, callback); 

	h = (RxHandler*)g_malloc(sizeof(RxHandler));
	h->type = type;
	h->callback = callback;
	h->arg = arg;

	pthread_mutex_lock(&m->rx_dispatch_mutex);
	m->rx_callback_list = g_slist_append(m->rx_callback_list, (gpointer*)h);
	pthread_mutex_unlock(&m->rx_dispatch_mutex);

	return 0;
}

gint mx_rx_unregister(mx_t *m, PACKET_TYPE type, RX_CALLBACK callback)
{
	RxHandler *h;
	GSList *l;

	pthread_mutex_lock(&m->rx_dispatch_mutex);
	for (l = m->rx_callback_list; l != NULL; l = g_slist_next (l)) {
		h = (RxHandler*)l->data;
		if ((h->type == type) && (h->callback == callback)) {
			break;
		}
	}
	if (l != NULL) {
		m->rx_callback_list = g_slist_remove(m->rx_callback_list, l->data);
		g_free(h);
	}
	pthread_mutex_unlock(&m->rx_dispatch_mutex);

	return 0;
}

gint mx_tx_packet(mx_t *m, packet_t* p)
{
	/* move packet to buffer*/
	pthread_mutex_lock(&m->tx_buffer_mutex);
	memcpy(&m->tx_buffer[m->tx_present_index], p, sizeof(packet_t));
	ADD_ONE_WITH_WRAP_AROUND(m->tx_present_index, TX_BUFFER_LENGTH);
	pthread_mutex_unlock(&m->tx_buffer_mutex);

	return 0;
}

void mx_init(mx_t *m, TX_DATA tx_data, void *arg)
{
	m->tx_data = tx_data;
	m->tx_interface = arg;

	m->rx_pool_index = 0;
	m->rx_process_index = 0;
	m->rx_present_index = 0;
	m->tx_process_index = 0;
	m->tx_present_index = 0;
	m->rx_callback_list = NULL;
	
	memset(m->rx_pool, 0, RX_POOL_LENGTH);

	pthread_mutex_init(&m->rx_pool_mutex, NULL);
	pthread_mutex_init(&m->tx_buffer_mutex, NULL);
	pthread_mutex_init(&m->rx_dispatch_mutex, NULL);
	
	mx_start_threads(m);
}

void mx_destroy(mx_t *m)
{
	mx_stop_threads(m);
	g_slist_foreach(m->rx_callback_list, (GFunc)g_free, NULL);
	g_slist_free(m->rx_callback_list);
}
