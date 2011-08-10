#
#Process this file with automake to produle Makefile.in
#
bin_PROGRAMS=amcc
amcc_SOURCES=amcc.c graph.c serial.c mx.c packet.c attitude.c
amcc_CFLAGS=@AMCC_CFLAGS@  -I/usr/lib/gtkglext-1.0/include -I/usr/include/gtkglext-1.0 -I/usr/include/GL
amcc_LDADD=@AMCC_LIBS@ -lxml2 -lGLU -lgtkglext-x11-1.0 -lGL -l3ds
