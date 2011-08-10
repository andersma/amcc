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
#include <unistd.h> 
#include <fcntl.h>  
#include <errno.h>  
#include <termios.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/select.h>
#include <termios.h>

#include "amcc.h"
#include "mx.h"
#include "serial.h"

static void* serial_rx_thread(void *data)
{
	gint n;
	gint max_fd;
	gint length;
	gchar buffer[200];

	fd_set input;
	struct timeval timeout;

	serial_t *s = (serial_t*)data;
	max_fd = s->fd + 1;
	
	while (s->active) {
		FD_ZERO(&input);
		FD_SET(s->fd, &input);

		timeout.tv_sec  = 1;
		timeout.tv_usec = 0;
		/* Do the select */
		n = select(max_fd, &input, NULL, NULL, &timeout);
		/* See if there was an error */
		if (n < 0) {
			g_print("select failed");
			serial_close(s);
			return;
		} else if (n == 0) {
			continue;
		} else {
			/* We have input */
			if (FD_ISSET(s->fd, &input)) {
				length = read(s->fd, buffer, 200);
#if 0
				guint i;
				g_print("[serial_t] ");
				for (i = 0; i < length; i++) {
					g_print("%c", buffer[i]);
				}
				g_print("\n");
				
#endif
				s->rx_handler(s->mx, buffer, length);
			}
		}
	}
}

void serial_init(serial_t *s, RX_HANDLER rx_handler, mx_t *mx)
{
	s->mx = mx;
	s->rx_handler = rx_handler;
}

gint serial_open(serial_t *s, gchar *name, guint baudrate)
{
	struct termios options;

	strncpy(s->name, name, MAX_SERIAL_NAME_LENGTH);
	s->baudrate = baudrate;
	/* open serial port */
	s->fd = open(name, O_RDWR | O_NOCTTY | O_NDELAY);
	if (s->fd == -1) {
		fprintf(stderr, "Unable to open %s\n", name);
		return -1;
  	} else {
		fcntl(s->fd, F_SETFL, FNDELAY); /* non-block mode */
	}
	/* set serial port parameters */
	tcgetattr(s->fd, &options);
	switch (baudrate) {
	case 115200:
	   s->baudrate = B115200;
	   break;
	case 57600:
	   s->baudrate = B57600;
	   break;
	case 38400:
	   s->baudrate = B38400;
	   break;
	case 19200:
	   s->baudrate  = B19200;
	   break;
	case 9600:
	   s->baudrate  = B9600;
	   break;
	case 4800:
	   s->baudrate  = B4800;
	   break;
	}
	cfsetispeed(&options, s->baudrate);
	options.c_cflag |= (CLOCAL | CREAD);
	/* ALWAYS use 8N1 */
	options.c_cflag &= ~PARENB;
	options.c_cflag &= ~CSTOPB;
	options.c_cflag &= ~CSIZE;
	options.c_cflag |= CS8;
	options.c_lflag  &= ~(ICANON | ECHO | ECHOE | ISIG);  /*Input*/
	options.c_oflag  &= ~OPOST;   /*Output*/
	tcsetattr(s->fd, TCSANOW, &options);	
	/* start rx thread */
	s->active = TRUE;
	pthread_create( &s->thread_rx, NULL, serial_rx_thread, (void*)s);	

	return 0;
}

gint serial_close(serial_t *s)
{
	if (s->fd != -1) {
		close(s->fd);
		s->active = FALSE;
		pthread_join(s->thread_rx, NULL);
	}
	return 0;
}

gint serial_tx_data(void *p, gchar *buffer, guint length)
{
	serial_t *s = (serial_t*)p;
	gint len;
	
	if (s->active) {
		len = write(s->fd, buffer, length);
		return len;
	}
}
