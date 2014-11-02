LDFLAGS=-L/usr/X11R6/lib -lX11 -lgiblib -lImlib2
CFLAGS=-g -O3 -Wall -I/usr/X11R6/include -I.

main:main.c imlib.o options.o
