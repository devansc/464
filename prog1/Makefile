# Example makefile for CPE464 program 1
#
#  Remember to add /opt/csw/lib to your path in order to execute your program
#  under Solaris.  Putting something like:
#     [ -e "/opt/csw/lib" ] && export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/opt/csw/lib
#  in your ~/.mybashrc file should do the trick

CC = gcc
#CFLAGS = -g -Wall -Werror
CGLAGS = 
OS = $(shell uname -s)
PROC = $(shell uname -p)
LIB=/usr/lib/i386-linux-gnu/

ifeq ("$(OS)", "SunOS")
	OSLIB=-L/opt/csw/lib -lsocket -lnsl
	OSINC=-I/opt/csw/include
	OSDEF=-DSOLARIS
else
ifeq ("$(OS)", "Darwin")
	OSLIB=
	OSINC=
	OSDEF=-DDARWIN
else
	OSLIB=
	OSINC=
	OSDEF=-DLINUX
endif
endif

all:  trace

# On some OSes you must have the libraries (e.g. -lpcap...) after the code.. others not
trace: trace.c
	$(CC) $(CFLAGS) $(OSDEF) -o $@ trace.c checksum.c -lpcap $(OSINC) $(OSLIB)

clean:
	rm -f trace
