#include "scrot.h"
#define _GNU_SOURCE

typedef struct{
   int x,y,w,h;
}rect_t;
char *home="/home/kkz", perl_script[64];
Screen* scr;

void setpos(const XButtonEvent*, rect_t*);
inline void block(fd_set* fdset, int* xfd, int* fdsize);
//inline void change_cursor(const Cursor* to, const Cursor* from);
inline void send_kbd_event(XEvent*);

void scrot_sel_and_grab_image(void) {
   static int xfd = 0, fdsize = 0;
   XEvent ev;
   fd_set fdset;
   int done = 0;
   int btn_pressed = 0, rect_x = 0, rect_y = 0, rect_w = 0, rect_h = 0;
   rect_t rect_region={.x=0, .y=0, .w=0, .h=0};
   Cursor cursor, cursor2;
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

   if ((XGrabPointer(disp, root, False,
		 ButtonMotionMask | ButtonPressMask | ButtonReleaseMask, GrabModeAsync,
		 GrabModeAsync, root, cursor, CurrentTime) != GrabSuccess))
	fprintf(stderr, "couldn't grab pointer:");

   if ((XGrabKeyboard(disp, root, False, GrabModeAsync, GrabModeAsync,
		 CurrentTime) != GrabSuccess))
	fprintf(stderr, "couldn't grab keyboard:");

   KeySym ks; int size=32, stop=2;
   char tmp[size];
   pid_t pid_capture=0;
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

			rect_x = rect_region.x;
			rect_y = rect_region.y;
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
		case ButtonPress:       // same for left/right buttons
		   btn_pressed = 1;
		   rect_region.x = ev.xbutton.x;
		   rect_region.y = ev.xbutton.y;
		   break;
		case ButtonRelease:
		   if(ev.xbutton.button==Button3)
			setpos(0, &rect_region);   // if right-button, select full-screen
		   else if(ev.xbutton.button==Button1)
			setpos(&ev.xbutton, &rect_region);       // on left, select region
		   else break;                   // on others, ignore.

		   if(rect_region.w<30 || rect_region.h<30){
			done=2; break;
		   }
		   XUngrabPointer(disp, CurrentTime);     //NOTE: free mouse
		   while(!XNextEvent(disp, &ev)){
			if(ev.type == KeyPress){
			   XLookupString(&ev.xkey, tmp, size, &ks, 0);
			   if(ks==XK_q || ks==XK_Escape){        //trap only these 2 keys
				if(ks==XK_q && --stop){
				   if((pid_capture=vfork())==-1){
					perror("fork failed");
					return;
					// change cursor shape
				   }else if(pid_capture==0){
					char top[8], bottom[8], left[8], right[8], perl5lib[32];
					sprintf(perl5lib, "%s/.perl5/lib/perl5", home);
					sprintf(top,"%d",rect_region.y);
					sprintf(bottom,"%d",rect_region.y+rect_region.h);
					sprintf(left,"%d",rect_region.x);
					sprintf(right,"%d",rect_region.x+rect_region.w);
					char* args[]= {"perl", perl_script,
					   left, right, top, bottom, 0};
					setenv("PERL5LIB", perl5lib, 1);
					execvp(args[0], args);
//					change_cursor(&cursor, &cursor);
				   }
				   continue;
				}
				if(pid_capture)kill(pid_capture, SIGINT);
				break;
			   }
			}else send_kbd_event(&ev);
		   }
		   done = 2; break;
		default: send_kbd_event(&ev);
	   }
	}
	if(done)
	   break;
	block(&fdset, &xfd, &fdsize);
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

void send_kbd_event(XEvent *ev){
   XUngrabKeyboard(disp, CurrentTime);
   XSendEvent(disp, root, True, 0, ev);
   if ((XGrabKeyboard(disp, root, False, GrabModeAsync, GrabModeAsync,
		   CurrentTime) != GrabSuccess))
	fprintf(stderr, "couldn't grab keyboard:");
}

/*void change_cursor(const Cursor* to, const Cursor* from){
   if(XGrabPointer(disp, root, True, ButtonMotionMask,
		GrabModeAsync, GrabModeAsync, root, *from,
		CurrentTime) != GrabSuccess)
	fprintf(stderr, "couldn't grab pointer:");
   XChangeActivePointerGrab(disp, ButtonMotionMask, *to, CurrentTime);
   XUngrabPointer(disp, CurrentTime);     //NOTE: free mouse
}*/

void block(fd_set* fdset, int* xfd, int* fdsize){
   /* now block some */
   FD_ZERO(fdset);
   FD_SET(*xfd, fdset);
   errno = 0;
   int count = select(*fdsize, fdset, NULL, NULL, NULL);
   if ((count < 0)
	   && ((errno == ENOMEM) || (errno == EINVAL) || (errno == EBADF)))
	fprintf(stderr, "Connection to X display lost");
}

void setpos(const XButtonEvent* but, rect_t* region){
   /* if a rect has been drawn, it's an area selection */
   if(!but){
	region->w=scr->width; region->h=scr->height;
	region->x=region->y=0;
   }else{
	region->w = but->x - region->x-1;
	region->h = but->y - region->y-1;
	if (region->w < 0) {
	   region->x += region->w;
	   region->w = -region->w;
	}
	if (region->h < 0) {
	   region->y += region->h;
	   region->h = 0 - region->h;
	}
	/* crop out rectangle */
	++region->x; ++region->y;
   }
}

int main(int argc, char **argv) {
   sprintf(perl_script, "%s/bin/bkup/screen_cap.pl", home);
   if(access(perl_script, F_OK)){
	fprintf(stderr, "Cannot find script %s\n", perl_script);
	return 1;
   }
   init_x_and_imlib(NULL);
   scr=ScreenOfDisplay(disp, DefaultScreen(disp));
   sleep(1);
   scrot_sel_and_grab_image();
   return 0;
}

