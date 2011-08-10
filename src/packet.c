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

#include "amcc.h"
#include "mx.h"
#include "packet.h"

#define DEBUG_PACKET 0

static void packet_checksum_add(packet_t *p)
{
	unsigned int i;
	unsigned int checksum = 0;

	for(i = 0; i < p->data_length; i++) {
		checksum += p->data[i];
	}
	checksum %= 4096;
	p->data[i++] = '=' + checksum / 64;
	p->data[i++] = '=' + checksum % 64;
	p->data[i++] = PACKET_END;
	p->data_length += 3;
}

static int packet_checksum_verify(packet_t *p)
{
	unsigned char a, b;
	unsigned int i;
	unsigned int checksum = 0;

	for(i = 0; i < p->data_length - 3; i++) { // 3 => START_TAG(1B) + checksum(2B)
		checksum += p->data[i];
	}
	checksum %= 4096;
	a = '=' + checksum / 64;
	b = '=' + checksum % 64;

	if ((a == p->data[i++]) && (b == p->data[i++])) {
		return PACKET_SUCCESS;
	}
	return PACKET_FAIL;
}

int packet_encode(packet_t *p)
{
	/*
	 * encode packet according 'raw'
	 * make packet data buffer containing all data to be send
	*/
	unsigned char a, b, c;
	unsigned char i, j, k;
	char buffer[MAX_PACKET_DATA_LENGTH];

	p->data[0] = PACKET_START;
	p->data[1] = p->type;
	p->data_length = 2;

	switch (p->type) {
	case ANALOG_DATA_RESPONSE:
		p->data[2] = p->raw.analog_data.channel_number;
		p->data_length ++;
		for (i = 0; i < p->raw.analog_data.channel_number; i++) {
			if (i == MAX_CHANNEL) { /* prevent data error */
				p->raw.analog_data.channel_number = MAX_CHANNEL;
				break;
			}
			memcpy(&p->data[3 + i * 2], &p->raw.analog_data.value[i], 2);
			p->data_length += 2;
		}
	}

	i = 2;
	j = 0;
	k = p->data_length - i;

	while (k) {
		if (k) {
			a = p->data[i++];
			k--;
		} else {
			a = 0;
		}
		if (k) {
			b = p->data[i++];
			k--;
		} else {
			b = 0;
		}
		if (k) {
			c = p->data[i++];
			k--;
		} else {
			c = 0;
		}
		buffer[j++] = '=' + (a >> 2);
		buffer[j++] = '=' + (((a & 0x03) << 4) | ((b & 0xf0) >> 4));
		buffer[j++] = '=' + (((b & 0x0f) << 2) | ((c & 0xc0) >> 6));
		buffer[j++] = '=' + ( c & 0x3f);
	}
	memcpy(&p->data[2], buffer, j);
	p->data_length = j + 2; /* 2 (START/TYPE) */
	packet_checksum_add(p);

#if DEBUG_PACKET
	g_print("[E] ");
	for (i = 0; i < p->data_length; i++) {
		g_print("%c", p->data[i]);
	}
	g_print("\n");
#endif

	return PACKET_SUCCESS;
}

int packet_decode(packet_t *p)
{
	unsigned char i;
	unsigned char a, b, c, d;
	unsigned char x, y, z;
	unsigned char in, out, max;
	unsigned char buffer[MAX_PACKET_DATA_LENGTH];
	int ret;

	if(PACKET_SUCCESS != packet_checksum_verify(p)) {
		return PACKET_FAIL;
	}

	if (p->data_length < 2) {
		return PACKET_FAIL;
	}
#if DEBUG_PACKET
	g_print("[D] ");
	for (i = 0; i < p->data_length; i++) {
		g_print("%c", p->data[i]);
	}
#endif
	p->type = p->data[1]; /* begin with PACKET_START */

	in = 2;
	out = 0;
	max = p->data_length;

	while (p->data_length)
	{
		a = p->data[in++] - '=';
		b = p->data[in++] - '=';
		c = p->data[in++] - '=';
		d = p->data[in++] - '=';

		/* CRC space check*/
		if(in > max - 3)
			break;

		x = (a << 2) | (b >> 4);
		y = ((b & 0x0f) << 4) | (c >> 2);
		z = ((c & 0x03) << 6) | d;

		if(p->data_length--) {
			buffer[out++] = x; 
		} else { 
			break;
		}
		if(p->data_length--) {
			buffer[out++] = y; 
		} else {
			break;
		}
		if(p->data_length--) {
			buffer[out++] = z; 
		} else {
			break;
		}
	}
	memcpy(p->data, buffer, out);
	p->data_length = out;

	switch (p->type) {
	case ANALOG_DATA_RESPONSE:
		p->raw.analog_data.channel_number = *(unsigned char*)&p->data[0];
		for (i = 0; i < p->raw.analog_data.channel_number; i++) {
			if (i == MAX_CHANNEL) { /* prevent data error */
				p->raw.analog_data.channel_number = MAX_CHANNEL;
				break;
			}
			p->raw.analog_data.value[i] = *(unsigned short*)&p->data[1 + i * 2];
		}
#if DEBUG_PACKET
	g_print("\t");
	for (i = 0; i < p->data_length; i++) {
		g_print("%d,", p->data[i]);
	}
	g_print("\n");
#endif
		break;
	default:
		return PACKET_FAIL;
	}
	return PACKET_SUCCESS;
}
