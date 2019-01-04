/*
Copyright (c) 2012 Daniel Burke

This software is provided 'as-is', without any express or implied
warranty. In no event will the authors be held liable for any damages
arising from the use of this software.

Permission is granted to anyone to use this software for any purpose,
including commercial applications, and to alter it and redistribute it
freely, subject to the following restrictions:

   1. The origin of this software must not be misrepresented; you must not
   claim that you wrote the original software. If you use this software
   in a product, an acknowledgment in the product documentation would be
   appreciated but is not required.

   2. Altered source versions must be plainly marked as such, and must not be
   misrepresented as being the original software.

   3. This notice may not be removed or altered from any source
   distribution.
*/

#define _XOPEN_SOURCE 700	// for CLOCK_MONOTONIC_RAW
#include <time.h>

#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/extensions/XInput2.h>


#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <GL/glew.h>
#include <GL/glx.h>

#include <dlfcn.h>
#include <fcntl.h>
#include <unistd.h>

#include "log.h"
#include "global.h"

///////////////////////////////////////////////////////////////////////////////
//////// Public Interface to the rest of the program
///////////////////////////////////////////////////////////////////////////////

int win_width  = 0;	/* used for switching from fullscreen back to window */
int win_height = 0;
int fullscreen=0;
int fullscreen_toggle=0;

///////////////////////////////////////////////////////////////////////////////
//////// X11 OpenGL window setup
///////////////////////////////////////////////////////////////////////////////

extern unsigned char _binary_build_lin_icon_head_start[];

Display *display;
Window window;
GLXContext glx_context;
XSetWindowAttributes xwin_attr;
XVisualInfo *xvis;
Cursor cursor_none;
int opcode;		// for XInput

int xAttrList[] = {
	GLX_RGBA,
	GLX_DOUBLEBUFFER,
	GLX_RED_SIZE, 8,
	GLX_GREEN_SIZE, 8,
	GLX_BLUE_SIZE, 8,
	GLX_DEPTH_SIZE, 24,
	None
};

int oldx=0, oldy=0;



static void x11_down(void)
{
	if(glx_context)
	{
		glXMakeCurrent(display, None, NULL);
//		glXDestroyContext(display, glx_context);
	}
	if(fullscreen)
	{
		XUndefineCursor(display, window);
		XDestroyWindow(display, window);
		XUngrabKeyboard(display, CurrentTime);
		vid_width = win_width;
		vid_height = win_height;
	}
	else
	{
		XWindowAttributes attr;
		XGetWindowAttributes(display, window, &attr);
		oldx = attr.x; oldy = attr.y;
		XDestroyWindow(display, window);
		win_width = vid_width;
		win_height = vid_height;
	}
//	XCloseDisplay(display);
}


static void x11_window(void)
{
/*	if(fullscreen)
	{
	XWindowAttributes attr;
	Window root = DefaultRootWindow(display);
	XGetWindowAttributes(display, root, &attr);

	vid_width = attr.width;
	vid_height = attr.height;

	xwin_attr.override_redirect = True;
	window = XCreateWindow(display, RootWindow(display, xvis->screen),
		0, 0, vid_width, vid_height, 0, xvis->depth, InputOutput,
		xvis->visual, CWBorderPixel | CWColormap | CWEventMask
		| CWOverrideRedirect
		, &xwin_attr);


	XMapRaised(display, window);
//		window = DefaultRootWindow(display);
//		XF86VidModeSetViewPort(display, screen, 0, 0);
//		XWarpPointer(display, None, window, 0, 0, 0, 0,
//				vid_width/2, vid_height/2);
	XGrabKeyboard(display, window, True,
		GrabModeAsync, GrabModeAsync, CurrentTime);
//		XGrabPointer(display, window, True,
//				PointerMotionMask | ButtonPressMask | ButtonReleaseMask,
//				GrabModeAsync, GrabModeAsync, window, None, CurrentTime);
//		XDefineCursor(display, window, cursor_none);

	}
	else
*/
	xwin_attr.override_redirect = False;
	window = XCreateWindow(display, RootWindow(display, xvis->screen),
		oldx, oldy, vid_width, vid_height, 0,
		xvis->depth, InputOutput, xvis->visual,
		CWBorderPixel | CWColormap | CWEventMask | CWOverrideRedirect,
		&xwin_attr);
	// Set the icon
	Atom net_wm_icon = XInternAtom(display, "_NET_WM_ICON", False);
	Atom cardinal = XInternAtom(display, "CARDINAL", False);
	int icon_length = 2 + 256 * 256;
	XChangeProperty(display, window, net_wm_icon, cardinal, 32,
		PropModeReplace, _binary_build_lin_icon_head_start, icon_length);

	// I will handle quit messages
	Atom delwm = XInternAtom(display, "WM_DELETE_WINDOW", False);
	XSetWMProtocols(display, window, &delwm, 1);

	// My window shall be named...
	XSetStandardProperties(display, window, "Kittens", "Kitteh", None,
		NULL, 0, NULL);
	XMapRaised(display, window);

	if(fullscreen)
	{
		Atom atoms[2] = { XInternAtom(display,
			"_NET_WM_STATE_FULLSCREEN", False), None };
		XChangeProperty(display, window,
		XInternAtom(display, "_NET_WM_STATE", False), XA_ATOM, 32,
		PropModeReplace, (unsigned char*)atoms, 1);
	}

	// let OpenGL know what's going on
	glXMakeCurrent(display, window, glx_context);
	glViewport(0, 0, vid_width, vid_height);
	if(!glXIsDirect(display, glx_context))
		log_fatal("DRI did not respond to hails.");
}


int ignore_mouse = 0;
static void x11_init(void)
{
	memset(keys, 0, KEYMAX);
	memset(mouse, 0, 3);

	display = XOpenDisplay(0);
	Screen *screen = DefaultScreenOfDisplay(display);
	sys_width = XWidthOfScreen(screen);
	sys_height = XHeightOfScreen(screen);

	int screen_id = DefaultScreen(display);
	xvis = glXChooseVisual(display, screen_id, xAttrList);
	if(!xvis)
	{
		log_fatal("glXChooseVisual() failed");
		exit(1);
	}
	glx_context = glXCreateContext(display, xvis, 0, GL_TRUE);

	memset(&xwin_attr, 0, sizeof(XSetWindowAttributes));

	xwin_attr.colormap = XCreateColormap(display,
	RootWindow(display, xvis->screen), xvis->visual, AllocNone);

	xwin_attr.event_mask = ExposureMask | StructureNotifyMask |
	ButtonPressMask | ButtonReleaseMask | PointerMotionMask |
	KeyPressMask | KeyReleaseMask;

	x11_window();

	static char buf[] = { 0,0,0,0,0,0,0,0 };
	XColor black = { .red=0, .green=0, .blue=0 };
	Pixmap bitmap = XCreateBitmapFromData(display, window, buf, 2, 2);
	cursor_none = XCreatePixmapCursor(display, bitmap, bitmap,
		&black, &black, 0, 0);

	int event, error;
	if(!XQueryExtension(display,"XInputExtension", &opcode, &event, &error))
	{
		log_warning("X Input extension not available");
		return;
	}
#ifdef ASD
	/* Which version of XI2? We support 2.0 */
	int major = 2, minor = 0;
	if (XIQueryVersion(display, &major, &minor) == BadRequest)
	{
		log_warning("XI2 not available - You have %d.%d", major, minor);
		return;
	}

	XIEventMask eventmask;
	unsigned char mask[3] = { 0,0,0 }; /* the actual mask */

	eventmask.deviceid = XIAllDevices;
	eventmask.mask_len = sizeof(mask); /* always in bytes */
	eventmask.mask = mask;
	/* now set the mask */
	//	XISetMask(mask, XI_ButtonPress);
	//	XISetMask(mask, XI_ButtonRelease);
	XISetMask(mask, XI_RawMotion);
	//	XISetMask(mask, XI_KeyPress);

	//	mask = mask | KeyReleaseMask | KeyPressMask;
	/* select on the window */
	Window root = DefaultRootWindow(display);
	XISelectEvents(display, root, &eventmask, 1);
	//	XSetDeviceMode(display, mouse, Relative);
#endif
}
/*
static void print_rawmotion(XIRawEvent *event)
{
	int i;
	double *raw_valuator = event->raw_values,
		*valuator = event->valuators.values;

	for (i = 0; i < event->valuators.mask_len * 8; i++)
	{
		if (XIMaskIsSet(event->valuators.mask, i))
		{
			switch(i) {
			case 0:
				mickey_x += *valuator;
				break;
			case 1:
				mickey_y += *valuator;
				break;
			default:
				break;
			}
			valuator++;
			raw_valuator++;
		}
	}
}
*/

static void handle_events(void)
{
	int deltax;
	int deltay;
	XEvent event;
	XIDeviceEvent *e;
	double value;

	mickey_x = mickey_y = 0;

	float x=0, y=0;
	int pos=0;
	while(XPending(display) > 0)
	{
		XNextEvent(display, &event);
		if(XGetEventData(display, &event.xcookie))
		{
			XGenericEventCookie *c = &event.xcookie;
			e = c->data;
			if(c->extension == opcode && c->type == GenericEvent)
			switch(c->evtype) {
//			case XI_RawMotion:
//				print_rawmotion(e);
//				break;
		//	case XI_ButtonPress:
			case XI_Motion:
		//	case XI_KeyPress:
				pos = 0;
				for(int i=0; i < e->valuators.mask_len * 8; i++)
				if(XIMaskIsSet(e->valuators.mask, i))
				{
					value = e->valuators.values[pos];
					log_debug("%d -- %f --",pos, value);
					switch(i){
					case 0:
						if(value > 1.0)
						{
							x=value;
							pos++;
							log_debug("%#+f",x);
						}
						break;
					case 1:
						if(value > 1.0)
						{
							y=value;
							pos++;
							log_debug("\t\t%f",y);
						}
						break;
					default:
						log_debug("%d -- %f --",pos, value);
						break;
					}
				}

				log_debug("%f\t%f\t--",x, y);
				break;
			default:
				break;
			}
			XFreeEventData(display, &event.xcookie);
		}
		else
		switch(event.type) {

		case ConfigureNotify:
			vid_width = event.xconfigure.width;
			vid_height = event.xconfigure.height;
			glViewport(0, 0, vid_width, vid_height);
			break;

		case ClientMessage:
			killme=1;
			break;

/* Mouse */
		case ButtonPress:
			switch(event.xbutton.button) {
			case 1:	mouse[0]=1; break;
			case 2:	mouse[1]=1; break;
			case 3:	mouse[2]=1; break;
			}
//			log_debug("mouse[] %d, @ %d", foo, (int)event.xbutton.time);
			break;
		case ButtonRelease:
			switch(event.xbutton.button) {
			case 1:	mouse[0]=0; break;
			case 2:	mouse[1]=0; break;
			case 3:	mouse[2]=0; break;
			}
			break;

		case MotionNotify:
//			log_debug("x=%d, y=%d", event.xmotion.x_root,
//				 event.xmotion.y_root);
			if(ignore_mouse)
			if(event.xmotion.x == vid_width/2)
			{
				ignore_mouse = 0;
				break;
			}
			deltax = mouse_x - event.xmotion.x;
			mouse_x = event.xmotion.x;
			mickey_x += deltax;
			deltay = mouse_y - event.xmotion.y;
			mouse_y = event.xmotion.y;
			mickey_y += deltay;
			break;

/* keyboard */
		case KeyPress:
//			log_debug("keyd[%d] @ %d", event.xkey.keycode, (int)event.xkey.time);
			if(event.xkey.keycode < KEYMAX)
				keys[event.xkey.keycode] = 1;
			break;
		case KeyRelease:
			if(event.xkey.keycode < KEYMAX)
				keys[event.xkey.keycode] = 0;
			if(event.xkey.keycode == KEY_F11)
				fullscreen_toggle=1;
			break;
		}
	}
	if(fullscreen)
	{
//		Window root = XRootWindow(display, 0);
//		XWarpPointer(display, None, window, 0, 0, 0, 0,
//				vid_width/2, vid_height/2);
//		ignore_mouse = 1;
//		XFlush(display);
//		XNextEvent(display, &event);
/*		event.xmotion.x = vid_width/2 - mouse_x;
	event.xmotion.y = vid_height/2 - mouse_y;
	event.xmotion.send_event = True;
	event.xmotion.time = CurrentTime;
	event.xmotion.window = window;
	event.xmotion.is_hint = NotifyNormal;
//		event.xmotion.root = window;
	event.type = MotionNotify;
	XSendEvent(display, PointerWindow, True, PointerMotionMask, &event);
*/	}

	if(fullscreen_toggle)
	{
		x11_down();
		fullscreen_toggle = 0;
		fullscreen = !fullscreen;
		x11_window();
	}
}


static void x11_end(void)
{
	x11_down();
	if(glx_context)
	{
		glXMakeCurrent(display, None, NULL);
		glXDestroyContext(display, glx_context);
	}
	XCloseDisplay(display);
}


int main(int argc, char* argv[])
{
	log_init();
	log_info("Platform    : Xlib");

	x11_init();
	glewInit();	// belongs after GL context creation
	int ret = main_init(argc, argv);
	if(ret)
	{
		x11_end();
		return ret;
	}

	while(!killme)
	{
		handle_events();
		main_loop();
		glXSwapBuffers(display, window);
	}

	main_end();
	x11_end();
	return 0;
}


