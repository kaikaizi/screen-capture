LDFLAGS=-L/usr/X11R6/lib -lX11
CFLAGS=-Wall -I/usr/X11R6/include -O3

screen_capture:screen_capture.c imlib.o
clean:
	$(RM) screen_capture
