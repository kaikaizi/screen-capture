LDFLAGS=-L/usr/X11R6/lib -lX11 -lgiblib -lImlib2 -lpthread
CFLAGS=-Wall -I/usr/X11R6/include -I.

main:main.c imlib.o options.o
clean:
	$(RM) *.png
