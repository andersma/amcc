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

#ifndef PACKET_H_
#define PACKET_H_

#define MAX_PACKET_DATA_LENGTH 200

struct packet_struct;
typedef struct packet_struct packet_t;

#define PACKET_SUCCESS 0
#define PACKET_FAIL 1

#define PACKET_START '('
#define PACKET_END ')'

#define MAX_CHANNEL 20

#define ACCX_CHANNEL 0
#define ACCY_CHANNEL 1
#define ACCZ_CHANNEL 2
#define GYROX_CHANNEL 3
#define GYROY_CHANNEL 4
#define GYROZ_CHANNEL 5


typedef enum _PACKET_SUB_TYPE {
	ANALOG_NAME_REQUEST = 'A',
	ANALOG_NAME_RESPONSE,
	ANALOG_DATA_REQUEST,
	ANALOG_DATA_RESPONSE,
	DEVICE_INFO_REQUEST,
	DEVICE_INFO_RESPONSE,
	DEVICE_MOTOR_CONTROL,
	DEVICE_PARAM_REQUEST,
	DEVICE_PARAM_RESPONSE,
	DEVICE_PARAM_SAVE,
} PACKET_TYPE;

/* Packet format 
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
| START | TYPE | DATA | CRC | END |
|  (1)  | (1)  | (4*n)| (2) | (1) |
+-+-+-+-++-+-+-+-+-+-+-+-+-+-+-+-+-
*/

/*
 * Be careful to modify following data structure,
 * cause they will let packet encode/decode error
 *
 * Please don't COPY following to/from packet data buffer, for
 * alignment issue.
 */

typedef struct _analog_name_struct {
	unsigned char channel;
	char *name;
} analog_name_t;

typedef struct _analog_data_struct {
	unsigned char channel_number;
	short value[MAX_CHANNEL];
} analog_data_t;

typedef struct dev_info_struct {
	unsigned char board;
	unsigned char firmware;
} dev_info_t;

struct packet_struct {
	PACKET_TYPE type;
	union {
		analog_name_t analog_name;
		analog_data_t analog_data;
		dev_info_t device_info;
	} raw;
	unsigned char data[MAX_PACKET_DATA_LENGTH];
	unsigned int data_length;
};

extern int packet_encode(packet_t *p);
extern int packet_decode(packet_t *p);

#endif
