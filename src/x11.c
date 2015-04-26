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


#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/extensions/XInput2.h>

#include <linux/input.h>

#include <sys/time.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <GL/glew.h>
#include <GL/glx.h>

#include <dlfcn.h>
#include <fcntl.h>
#include <unistd.h>

///////////////////////////////////////////////////////////////////////////////
//////// Public Interface to the rest of the program
///////////////////////////////////////////////////////////////////////////////

//#include "keyboard.h"

int killme = 0;
int sys_width  = 1980;	/* dimensions of default screen */
int sys_height = 1200;
int sys_dpi = 1.0;
int vid_width  = 1280;	/* dimensions of our part of the screen */
int vid_height = 720;
int win_width  = 0;		/* used for switching from fullscreen back to window */
int win_height = 0;
int mouse_x;
int mouse_y;
int mickey_x;
int mickey_y;
char mouse[] = {0,0,0};
#define KEYMAX 128
char keys[KEYMAX];

int fullscreen=0;
int fullscreen_toggle=0;

int main_init(int argc, char *argv[]);
void main_loop(void);
void main_end(void);

const int sys_ticksecond = 1000000;
long long sys_time(void)
{
	struct timeval tv;
	tv.tv_usec = 0;	// tv.tv_sec = 0;
	gettimeofday(&tv, NULL);
	return tv.tv_usec + tv.tv_sec * sys_ticksecond;
}

void shell_browser(char *url)
{
	int c=1000;
	char buf[c];
	memset(buf, 0, sizeof(char)*c);
	snprintf(buf, c, "sensible-browser %s &", url);
	system(buf);
}

struct fvec2
{
	float x, y;
};

typedef struct joystick
{
	int connected;
	struct fvec2 l, r;
	float lt, rt;
	int button[15];
	int fflarge, ffsmall;
} joystick;

joystick joy[4];

///////////////////////////////////////////////////////////////////////////////
//////// X11 OpenGL window setup
///////////////////////////////////////////////////////////////////////////////


const unsigned char icon_buffer[] = { 0,1,0,0,0,0,0,0, 0,1,0,0,0,0,0,0,
#include <icon.h>
};

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

// https://www.kernel.org/doc/Documentation/input/input.txt

struct lin_joystick
{
	int fd;
	int id;
	int ffid;
};

struct lin_joystick lin_joy[4]; 


static int dev_open(int n)
{
	char filename[32];
	snprintf(filename, sizeof(filename), "/dev/input/event%d", n);
	return open(filename, O_RDWR);
}

static void dev_all(void)
{
	struct ff_effect ff;
	memset(&ff, 0, sizeof(ff));
	ff.type = FF_RUMBLE;
	ff.id = -1;
	for(int i=0; i<32; i++)
	{
		int fd = dev_open(i);
		if(fd < 0)continue;
		int j;

		for(j=0; j<4; j++)
		{
			if(joy[j].connected)
			if(lin_joy[j].id == i)break;
		}
		if(j < 4)	// we already have it
		{
			close(fd);
//			printf("already have it\n");
			continue;
		}
		for(j=0; j<4; j++)
		{
			if(joy[j].connected == 0)
			{
				struct input_id id;
				ioctl(fd, EVIOCGID, &id);
				printf("vend:%x, prod:%x, ver:%d\n",
					id.vendor, id.product, id.version);
				char buf[32];
				ioctl(fd, EVIOCGNAME(sizeof(buf)), buf);
				printf("name = %s\n", buf);

				lin_joy[j].fd = fd;
				lin_joy[j].id = i;
				joy[j].connected = 1;
				lin_joy[j].ffid = ioctl(fd, EVIOCSFF, &ff);
				break;
			}
		}
		if(j == 4)
		{
			close(fd);
			printf("too many joysticks\n");
			return;
		}
	}
}


static void x11_input(void)
{
	struct timeval tv;
	struct ff_effect ff;
	struct input_event event, play;
	fd_set set;
	memset(&ff, 0, sizeof(ff));
	ff.type = FF_RUMBLE;
	ff.replay.length = 1000;
	tv.tv_sec = 0;
	tv.tv_usec = 0;

	dev_all();

	for(int i=0; i<4; i++)
	{
		if(!joy[i].connected)continue;
		FD_ZERO(&set);
		FD_SET(lin_joy[i].fd, &set);
		int ret = select(lin_joy[i].fd+1, &set, NULL, NULL, &tv);
		if(-1 == ret)
		{
			printf("select error\n");
			continue;
		}
		while(ret > 0)
		{
			int ret2 = read(lin_joy[i].fd, &event, sizeof(event));
			if(-1 == ret2)
			{
			//	printf("Joystick disconnected\n");
				joy[i].connected = 0;
				close(lin_joy[i].fd);
				break;
			}
			if(0 == ret2)
			{
				printf("EOF\n");
			}
			switch(event.type) {
			case EV_ABS:	// axis
				switch(event.code) {
				case 0:	// LX
					joy[i].l.x = event.value;break;
				case 1:	// LY
					joy[i].l.y = event.value;break;
				case 2:	// LT
					joy[i].lt = event.value;break;
				case 3:	// RX
					joy[i].r.x = event.value;break;
				case 4:	// RY
					joy[i].r.y = event.value;break;
				case 5:	// RT
					joy[i].rt = event.value;break;
				case 16:	// Dpad X on old Wired Pad
					switch(event.value) {
					case -1: // left
						joy[i].button[13] = 1;
						joy[i].button[14] = 0;
						break;
					case 0: // center 
						joy[i].button[13] = 0;
						joy[i].button[14] = 0;
						break;
					case 1: // right
						joy[i].button[13] = 0;
						joy[i].button[14] = 1;
						break;
					default: // haven't seen this
						break;
					}
					break;
				case 17:	// Dpad Y on old Wired Pad
					switch(event.value) {
					case -1: // up 
						joy[i].button[11] = 1;
						joy[i].button[12] = 0;
						break;
					case 0: // center 
						joy[i].button[11] = 0;
						joy[i].button[12] = 0;
						break;
					case 1: // down
						joy[i].button[11] = 0;
						joy[i].button[12] = 1;
						break;
					default: // haven't seen this
						break;
					}
					break;
				default:
					printf("axis e: %d, v:%d\n", event.code, event.value);
				}
				break;

			case EV_KEY:	// button
				switch(event.code) {
				case 304:	// A
					joy[i].button[0] = event.value; break;
				case 305:	// B
					joy[i].button[1] = event.value; break;
				case 307:	// X
					joy[i].button[2] = event.value; break;
				case 308:	// Y
					joy[i].button[3] = event.value; break;
				case 310:	// LB
					joy[i].button[4] = event.value; break;
				case 311:	// RB
					joy[i].button[5] = event.value; break;
				case 317:	// LJ
					joy[i].button[6] = event.value; break;
				case 318:	// RJ
					joy[i].button[7] = event.value; break;
				case 315:	// Start
					joy[i].button[8] = event.value; break;
				case 314:	// Select
					joy[i].button[9] = event.value; break;
				case 316:	// Logo
					joy[i].button[10] = event.value; break;
				case 706:	// Dpad Up on New Wireless pad
					joy[i].button[11] = event.value; break;
				case 707:	// Dpad Down on New Wireless pad
					joy[i].button[12] = event.value; break;
				case 704:	// Dpad Left on New Wireless pad
					joy[i].button[13] = event.value; break;
				case 705:	// Dpad Right on New Wireless pad
					joy[i].button[14] = event.value; break;
				default:
					printf("butt e: %d, v:%d\n", event.code, event.value);
				}
				break;
			default:
				break;
			}

			ret = select(lin_joy[i].fd+1, &set, NULL, NULL, &tv);
		}

		// upload the Force Feedback settings
		ff.id = lin_joy[i].ffid;
		ff.u.rumble.strong_magnitude = joy[i].fflarge * 255;
		ff.u.rumble.weak_magnitude = joy[i].ffsmall * 255;
		ioctl(lin_joy[i].fd, EVIOCSFF, &ff);
		play.type = EV_FF;
		play.code = ff.id;
		play.value = 1;
		write(lin_joy[i].fd, &play, sizeof(play));
	}
}


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
		PropModeReplace, icon_buffer, icon_length);

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
		printf("DRI did not respond to hails.\n");
}


int ignore_mouse = 0;
static void x11_init(void)
{
	memset(keys, 0, KEYMAX);
	memset(mouse, 0, 3);
	memset(joy, 0, sizeof(joy));
	memset(lin_joy, 0, sizeof(lin_joy));

	display = XOpenDisplay(0);
	Screen *screen = DefaultScreenOfDisplay(display);
	sys_width = XWidthOfScreen(screen);
	sys_height = XHeightOfScreen(screen);

	int screen_id = DefaultScreen(display);
	xvis = glXChooseVisual(display, screen_id, xAttrList);
	if(!xvis)
	{
		printf("glXChooseVisual() failed.\n");
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
		printf("X Input extension not available.\n");
		return;
	}
#ifdef ASD
	/* Which version of XI2? We support 2.0 */
	int major = 2, minor = 0;
	if (XIQueryVersion(display, &major, &minor) == BadRequest)
	{
		printf("XI2 not available. You have %d.%d\n", major, minor);
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

	x11_input();

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
					printf("%d -- %f --\n",pos, value);
					switch(i){
					case 0:
						if(value > 1.0)
						{
							x=value;
							pos++;
							printf("%#+f\n",x);
						}
						break;
					case 1:
						if(value > 1.0)
						{
							y=value;
							pos++;
							printf("\t\t%f\n",y);
						}
						break;
					default:
						printf("%d -- %f --\n",pos, value);
						break;
					}
				}

				printf("%f\t%f\t--\n",x, y);
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
//			printf("mouse[] %d, @ %d\n", foo, (int)event.xbutton.time);
			break;
		case ButtonRelease:
			switch(event.xbutton.button) {
			case 1:	mouse[0]=0; break;
			case 2:	mouse[1]=0; break;
			case 3:	mouse[2]=0; break;
			}
			break;

		case MotionNotify:
//			printf("x=%d, y=%d\n", event.xmotion.x_root,
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
//			printf("keyd[%d] @ %d\n", event.xkey.keycode, (int)event.xkey.time);
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


