#include <assert.h>
#include <pthread.h>
#include "options.h"
#include "scrot.h"
#include "options.h"
#include "structs.h"


void scrot_sel_and_grab_image(void);
void* capture(void*);
void setpos(const XButtonEvent);
void capture_image(const short index);
void free_images();
int x, y, w, h;
int in_capture;
#define MAX_SHOTS 20

int main(int argc, char **argv) {
   init_x_and_imlib(NULL, 0);
   pthread_t cap_thread;
   if(pthread_create(&cap_thread, 0, capture, 0)){
	gib_eprintf("threading failed");
   }
   scrot_sel_and_grab_image();
   return 0;
}

void scrot_sel_and_grab_image(void) {
   static int xfd = 0;
   static int fdsize = 0;
   XEvent ev;
   fd_set fdset;
   int count = 0, done = 0;
   int btn_pressed = 0;
   int rect_x = 0, rect_y = 0, rect_w = 0, rect_h = 0;
   Cursor cursor, cursor2;
//   Window target = None;
   GC gc;
   XGCValues gcval;

   xfd = ConnectionNumber(disp);
   fdsize = xfd + 1;

   cursor = XCreateFontCursor(disp, XC_left_ptr);
   cursor2 = XCreateFontCursor(disp, XC_lr_angle);

   gcval.foreground = XWhitePixel(disp, 0);
   gcval.function = GXxor;
   gcval.background = XBlackPixel(disp, 0);
   gcval.plane_mask = gcval.background ^ gcval.foreground;
   gcval.subwindow_mode = IncludeInferiors;

   gc = XCreateGC(disp, root,
	   GCFunction | GCForeground | GCBackground | GCSubwindowMode,
	   &gcval);

   if ((XGrabPointer
		(disp, root, False,
		 ButtonMotionMask | ButtonPressMask | ButtonReleaseMask, GrabModeAsync,
		 GrabModeAsync, root, cursor, CurrentTime) != GrabSuccess))
	gib_eprintf("couldn't grab pointer:");

   if ((XGrabKeyboard
		(disp, root, False, GrabModeAsync, GrabModeAsync,
		 CurrentTime) != GrabSuccess))
	gib_eprintf("couldn't grab keyboard:");


   KeySym ks; int size=64, stop=2;
   char tmp[size];
   while (1) {
	/* handle events here */
	while (!done && XPending(disp)) {
	   XNextEvent(disp, &ev);
	   switch (ev.type) {
		case MotionNotify:
		   if (btn_pressed) {
			if (rect_w) {
			   /* re-draw the last rect to clear it */
			   XDrawRectangle(disp, root, gc, rect_x, rect_y, rect_w, rect_h);
			} else {
			   /* Change the cursor to show we're selecting a region */
			   XChangeActivePointerGrab(disp,
				   ButtonMotionMask | ButtonReleaseMask,
				   cursor2, CurrentTime);
			}

			rect_x = x;
			rect_y = y;
			rect_w = ev.xmotion.x - rect_x;
			rect_h = ev.xmotion.y - rect_y;

			if (rect_w < 0) {
			   rect_x += rect_w;
			   rect_w = 0 - rect_w;
			}
			if (rect_h < 0) {
			   rect_y += rect_h;
			   rect_h = 0 - rect_h;
			}
			/* draw rectangle */
			XDrawRectangle(disp, root, gc, rect_x, rect_y, rect_w, rect_h);
			XFlush(disp);
		   }
		   break;
		case ButtonPress:
		   btn_pressed = 1;
		   x = ev.xbutton.x;
		   y = ev.xbutton.y;
		   break;
		case ButtonRelease:
		   do{
			XNextEvent(disp, &ev);
			if(ev.type == KeyPress){
			   XLookupString(&ev.xkey, tmp, size, &ks, 0);
			   do{                 // wait till release
				XNextEvent(disp, &ev);
			   }while(ev.type!=KeyRelease);
			   switch(ks){
				case XK_backslash:
				   in_capture = --stop;
				   break;
				case XK_Escape:
				case XK_q:         // abort wo. capturing
				   stop=-1;
				default:;
			   }
			   usleep(1500);
			}
		   }while(stop>0);
		   if(stop<0)
			gib_eprintf("Key was pressed, aborting shot");
		   done = 2;
		   break;
		case KeyRelease:
		   /* ignore */
		   break;
		default:
		   break;
	   }
	}
	if (done)
	   break;

	/* now block some */
	FD_ZERO(&fdset);
	FD_SET(xfd, &fdset);
	errno = 0;
	count = select(fdsize, &fdset, NULL, NULL, NULL);
	if ((count < 0)
		&& ((errno == ENOMEM) || (errno == EINVAL) || (errno == EBADF)))
	   gib_eprintf("Connection to X display lost");
   }
   if (rect_w) {
	XDrawRectangle(disp, root, gc, rect_x, rect_y, rect_w, rect_h);
	XFlush(disp);
   }
   XUngrabPointer(disp, CurrentTime);
   XUngrabKeyboard(disp, CurrentTime);
   XFreeCursor(disp, cursor);
   XFreeGC(disp, gc);
   XSync(disp, True);
}


void* capture(void*v){
   static short index=0;
   scrot_nice_clip();
   char has_turned_on=0;
   while(index<MAX_SHOTS){
	if(in_capture){
	   capture_image(index++);
	   has_turned_on=1;
	   usleep(150000);       // sleep 0.15 sec
	}else if(has_turned_on)break;
	usleep(1000);
   }
   return 0;
}

char file_name[64];
Imlib_Load_Error err;
void capture_image(const short index){
   sprintf(file_name, "%d.png", index);
//   usleep(10000);
   Imlib_Image image = gib_imlib_create_image_from_drawable(root, 0, x, y, w, h, 1);
   usleep(1000);
   imlib_context_set_image(image);
//   usleep(10000);
//   assert(image);
   imlib_image_attach_data_value("quality", NULL, 60/*opt.quality*/, NULL);
   usleep(3000);
   gib_imlib_save_image_with_error_return(image, file_name, &err);
   usleep(3000);
   if (err) gib_eprintf("Saving to file %s failed\n", file_name);
}

/* clip rectangle nicely */
void scrot_nice_clip() {
   if (x < 0) {
	w += x;
	x = 0;
   }
   if (y < 0) {
	h += y;
	y = 0;
   }
   if ((x + w) > scr->width)
	w = scr->width - x;
   if ((y + h) > scr->height)
	h = scr->height - y;
}

Window scrot_get_window(Display * display, Window window, int x, int y) {
   Window source, target;

   int status, x_offset, y_offset;

   source = root;
   target = window;
   if (window == None)
	window = root;
   while (1) {
	status =
	   XTranslateCoordinates(display, source, window, x, y, &x_offset,
		   &y_offset, &target);
	if (status != True)
	   break;
	if (target == None)
	   break;
	source = window;
	window = target;
	x = x_offset;
	y = y_offset;
   }
   if (target == None)
	target = window;
   return (target);
}
