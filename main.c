#include "options.h"
#include "scrot.h"
#include "options.h"
#include "structs.h"

int
main(int argc,
     char **argv)
{
  Imlib_Image image;
  Imlib_Load_Error err;

  init_parse_options(argc, argv);
  init_x_and_imlib(NULL, 0);
  char* file_name = "%Y-%m-%d-%H%M%S_$wx$h_scrot.png";

  image = scrot_sel_and_grab_image();
  if (!image)
    gib_eprintf("no image grabbed");

  imlib_context_set_image(image);
  imlib_image_attach_data_value("quality", NULL, 60/*opt.quality*/, NULL);

  gib_imlib_save_image_with_error_return(image, file_name, &err);
  if (err)
    gib_eprintf("Saving to file %s failed\n", file_name);
  gib_imlib_free_image_and_decache(image);
  return 0;
}

Imlib_Image
scrot_sel_and_grab_image(void)
{    // HERE:
  static int xfd = 0;
  static int fdsize = 0;
  XEvent ev;
  fd_set fdset;
  int count = 0, done = 0;
  int rx = 0, ry = 0, rw = 0, rh = 0, btn_pressed = 0;
  int rect_x = 0, rect_y = 0, rect_w = 0, rect_h = 0;
  Cursor cursor, cursor2;
  Window target = None;
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

  gc =
    XCreateGC(disp, root,
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
	   /* TODO: handle start/end key press */
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

            rect_x = rx;
            rect_y = ry;
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
          rx = ev.xbutton.x;
          ry = ev.xbutton.y;
	    target = scrot_get_window(disp, ev.xbutton.subwindow,
		    ev.xbutton.x, ev.xbutton.y);
          if (target == None)
            target = root;
          break;
        case ButtonRelease:
          done = 1;
          break;
	  case KeyPress:    // TODO: When pressing '`' for 1st time, start recording;
	    // for 2nd time, stop recording.
	    do{
		 XNextEvent(disp, &ev);
		 XLookupString(&ev.xkey, tmp, size, &ks, 0);
		 switch(ks){
		    case XK_asciitilde:
			 --stop;
			 break;
		    case XK_Escape:
		    case XK_q:
			 stop=0;
			 break;
		    default:;
		 }
		 printf("%d:\n", stop);
	    }while(stop);
          fprintf(stderr, "Key was pressed, aborting shot\n");
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


  if (rect_w > 5) {
     /* if a rect has been drawn, it's an area selection */
     rw = ev.xbutton.x - rx;
     rh = ev.xbutton.y - ry;

     if (rw < 0) {
	  rx += rw;
	  rw = 0 - rw;
     }
     if (rh < 0) {
	  ry += rh;
	  rh = 0 - rh;
     }
  } else return 0;
  scrot_nice_clip(&rx, &ry, &rw, &rh);
  return gib_imlib_create_image_from_drawable(root, 0, rx, ry, rw, rh, 1);
}

/* clip rectangle nicely */
void
scrot_nice_clip(int *rx,
		int *ry,
		int *rw,
		int *rh)
{
  if (*rx < 0) {
    *rw += *rx;
    *rx = 0;
  }
  if (*ry < 0) {
    *rh += *ry;
    *ry = 0;
  }
  if ((*rx + *rw) > scr->width)
    *rw = scr->width - *rx;
  if ((*ry + *rh) > scr->height)
    *rh = scr->height - *ry;
}

/* get geometry of window and use that */
int
scrot_get_geometry(Window target,
		   int *rx,
		   int *ry,
		   int *rw,
		   int *rh)
{
  Window child;
  XWindowAttributes attr;
  int stat;

  /* get windowmanager frame of window */
  if (target != root) {
    unsigned d; int x;
    int status;

    status = XGetGeometry(disp, target, &root, &x, &x, &d, &d, &d, &d);
    if (status != 0) {
      Window rt, *children, parent;

      for (;;) {
	/* Find window manager frame. */
	status = XQueryTree(disp, target, &rt, &parent, &children, &d);
	if (status && (children != None))
	  XFree((char *) children);
	if (!status || (parent == None) || (parent == rt))
	  break;
	target = parent;
      }
      XRaiseWindow(disp, target);
    }
  }
  stat = XGetWindowAttributes(disp, target, &attr);
  if ((stat == False) || (attr.map_state != IsViewable))
    return 0;
  *rw = attr.width;
  *rh = attr.height;
  XTranslateCoordinates(disp, target, root, 0, 0, rx, ry, &child);
  return 1;
}

Window
scrot_get_window(Display * display,
                 Window window,
                 int x,
                 int y)
{
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
