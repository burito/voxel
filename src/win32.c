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

#include <windows.h>
#include <Xinput.h>
#include <GL/glew.h>
#include <GL/wglew.h>
#include <stdio.h>

#include "log.h"
///////////////////////////////////////////////////////////////////////////////
//////// Public Interface to the rest of the program
///////////////////////////////////////////////////////////////////////////////

#include "keyboard.h"

int killme=0;
int sys_width  = 1980;	/* dimensions of default screen */
int sys_height = 1200;
float sys_dpi = 1.0;
int vid_width  = 1280;	/* dimensions of our part of the screen */
int vid_height = 720;
int mouse_x = 0;
int mouse_y = 0;
int mickey_x = 0;
int mickey_y = 0;
char mouse[] = {0,0,0,0,0,0,0,0};
#define KEYMAX 512
char keys[KEYMAX];

int fullscreen = 0;
int fullscreen_toggle = 0;

int main_init(int argc, char *argv[]);
void main_loop(void);
void main_end(void);

const int sys_ticksecond = 1000;
long long sys_time(void)
{
	return timeGetTime();
}

void shell_browser(char *url)
{
	ShellExecute(NULL, "open", url, NULL, NULL, SW_SHOWNORMAL);
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
//////// Windows OpenGL window setup
///////////////////////////////////////////////////////////////////////////////

HINSTANCE hInst;
HWND hWnd;
int CmdShow;
HDC hDC;
HGLRC hGLRC;

int win_width  = 0;	/* used for switching from fullscreen back to window */
int win_height = 0;

int window_maximized = 0;
int focus = 1;
int menu = 1;
int sys_bpp = 32;

static void fail(const char * string)
{
	int err;
	char errStr[1000];
	err = GetLastError();
	FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, 0, err, 0,
		errStr, 1000, 0);
	log_fatal("%s: %s", string, errStr);
}

static void sys_input(void)
{
 	DWORD result;
 	XINPUT_STATE state;

	for(int i=0; i<4; i++)
	{
		ZeroMemory( &state, sizeof(XINPUT_STATE) );
		result = XInputGetState( i, &state );
		if( result != ERROR_SUCCESS )
		{
			joy[i].connected = 0;
			continue;
		}
		joy[i].connected = 1;

		joy[i].l.x = state.Gamepad.sThumbLX;
		joy[i].l.y = state.Gamepad.sThumbLY;
		joy[i].r.x = state.Gamepad.sThumbRX;
		joy[i].r.y = state.Gamepad.sThumbRY;
		joy[i].lt = state.Gamepad.bLeftTrigger;
		joy[i].rt = state.Gamepad.bRightTrigger;

		joy[i].button[0] = !!(state.Gamepad.wButtons & XINPUT_GAMEPAD_A);
		joy[i].button[1] = !!(state.Gamepad.wButtons & XINPUT_GAMEPAD_B);
		joy[i].button[2] = !!(state.Gamepad.wButtons & XINPUT_GAMEPAD_X);
		joy[i].button[3] = !!(state.Gamepad.wButtons & XINPUT_GAMEPAD_Y);
		joy[i].button[4] = !!(state.Gamepad.wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER);
		joy[i].button[5] = !!(state.Gamepad.wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER);
		joy[i].button[6] = !!(state.Gamepad.wButtons & XINPUT_GAMEPAD_LEFT_THUMB);
		joy[i].button[7] = !!(state.Gamepad.wButtons & XINPUT_GAMEPAD_RIGHT_THUMB);
		joy[i].button[8] = !!(state.Gamepad.wButtons & XINPUT_GAMEPAD_START);
		joy[i].button[9] = !!(state.Gamepad.wButtons & XINPUT_GAMEPAD_BACK);
		joy[i].button[10] = 0;
		joy[i].button[11] = !!(state.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_UP);
		joy[i].button[12] = !!(state.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_DOWN);
		joy[i].button[13] = !!(state.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_LEFT);
		joy[i].button[14] = !!(state.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_RIGHT);

		XINPUT_VIBRATION vib;
		vib.wLeftMotorSpeed = joy[i].fflarge * 255;
		vib.wRightMotorSpeed = joy[i].ffsmall * 255;
		XInputSetState(i, &vib);
	}
}


int w32_moving = 0;
int w32_delta_x = 0;
int w32_delta_y = 0;

static LONG WINAPI wProc(HWND hWndProc, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	int code;
	int bit=0;
	switch(uMsg) {
	case WM_SYSCOMMAND:
//		log_debug("WM_SYSCOMMAND");
		switch(wParam & 0xFFF0) {
		case SC_SIZE:
//			log_debug("SC_SIZE");
			break;
			return 0;
		case SC_MOVE:	// Moving the window locks the app, so implement it manually
			w32_moving = 1;
			POINT p;
			GetCursorPos(&p);
			RECT r;
			GetWindowRect(hWnd,&r);
			w32_delta_x = p.x - r.left;
			w32_delta_y = p.y - r.top;
			SetCapture(hWnd);
			return 0;
		default:
			break;
		}
		break;
		return 0;
//	case WM_EXITSIZEMOVE:
//		log_debug("WM_EXITSIZEMOVE");
//		return 0;

	case WM_SIZING:
	case WM_SIZE:
		vid_width = LOWORD(lParam);
		vid_height = HIWORD(lParam);
		glViewport(0, 0, vid_width, vid_height);
		PostMessage(hWndProc, WM_PAINT, 0, 0);
		return 0;

	case WM_KEYDOWN:
		bit = 1;
	case WM_KEYUP:
		code = (HIWORD(lParam)) & 511;
		if(code < KEYMAX)keys[code]=bit;
		if(code == KEY_F11 && bit == 0)
			fullscreen_toggle = 1;
		return 0;

	case WM_LBUTTONDOWN:
		bit = 1;
	case WM_LBUTTONUP:
		mouse[0]=bit;
		if(w32_moving)
		{
			w32_moving = 0;
			ReleaseCapture();
		}
		return 0;

	case WM_MBUTTONDOWN:
		bit = 1;
	case WM_MBUTTONUP:
		mouse[1]=bit;
		return 0;

	case WM_RBUTTONDOWN:
		bit = 1;
	case WM_RBUTTONUP:
		mouse[2]=bit;
		return 0;

	case WM_XBUTTONDOWN:
		bit = 1;
	case WM_XBUTTONUP:
		switch(GET_XBUTTON_WPARAM(wParam)) {
		case XBUTTON1:
			mouse[3]=bit;
			break;
		case XBUTTON2:
			mouse[4]=bit;
			break;
		default:
			break;
		}
		return 0;

	case WM_MOUSEMOVE:
		mickey_x += mouse_x - LOWORD(lParam);
		mickey_y += mouse_y - HIWORD(lParam);
		mouse_x = LOWORD(lParam);
		mouse_y = HIWORD(lParam);
		if(w32_moving)
		{
			RECT r;
			int win_width, win_height;
			GetWindowRect(hWnd,&r);
			win_height = r.bottom - r.top;
			win_width = r.right - r.left;
			POINT p;
			GetCursorPos(&p);
			MoveWindow(hWnd, p.x - w32_delta_x,
					 p.y - w32_delta_y,
					 win_width, win_height, TRUE);
		}
		else
		{

		}
		break;

	case WM_SETCURSOR:
		switch(LOWORD(lParam))
		{
		case HTCLIENT:
			if(!menu)
				while(ShowCursor(FALSE) >= 0);
			SetCursor(LoadCursor(NULL, IDC_ARROW));
			return 0;
		default:
			while(ShowCursor(TRUE) < 0);
			SetCursor(LoadCursor(NULL, IDC_ARROW));
			break;
		}
		break;

	case 0x02E0:	// WM_DPICHANGED
		sys_dpi =(float)LOWORD(wParam) / 96.0; // dpi-X
//		sys_dpi = HIWORD(wParam);	// dpi-Y
		RECT *r = (RECT*)lParam;
		SetWindowPos(hWnd, HWND_TOP, 0, 0,
			r->left - r->right,
			r->top - r->bottom,
			SWP_NOMOVE);
		return 0;

	case WM_CLOSE:
		killme=1;
		return 0;
	}
	return DefWindowProc(hWndProc, uMsg, wParam, lParam);
}

void win_pixelformat(void)
{
	unsigned char bpp = 32;
	PIXELFORMATDESCRIPTOR pfd = {
		sizeof(PIXELFORMATDESCRIPTOR),
		1,				// version number
		PFD_DRAW_TO_WINDOW |
		PFD_SUPPORT_OPENGL |
		PFD_DOUBLEBUFFER |
		PFD_STEREO_DONTCARE,
		PFD_TYPE_RGBA,
		bpp,
		0, 0, 0, 0, 0, 0,		// color bits ignored
		0,				// no alpha buffer
		0,				// shift bit ignored
		0,				// no accumulation buffer
		0, 0, 0, 0,			// accum bits ignored
		bpp,				// 32-bit z-buffer
		0,				// no stencil buffer
		0,				// no auxiliary buffer
		PFD_MAIN_PLANE,			// main layer
		0,				// reserved
		0, 0, 0				// layer masks ignored
	};

	int pf = ChoosePixelFormat(hDC, &pfd);
	if(!pf)
	{
		fail("ChoosePixelFormat() failed");
		return;
	}

	if(!SetPixelFormat(hDC, pf, &pfd))
	{
		fail("SetPixelFormat() failed");
		return;
	}

	if(!DescribePixelFormat(hDC, pf, sizeof(PIXELFORMATDESCRIPTOR), &pfd))
	{
		fail("DescribePixelFormat() failed");
		return;
	}
}

static void win_toggle(void)
{
	if(wglGetCurrentContext())
		wglMakeCurrent(NULL, NULL);

	DestroyWindow(hWnd);

	if(!fullscreen)
	{
		win_width = vid_width;
		win_height = vid_height;
		vid_width = GetSystemMetrics(SM_CXSCREEN);
		vid_height = GetSystemMetrics(SM_CYSCREEN);

		hWnd = CreateWindowEx(0, "Kittens", "Kittens",
		WS_POPUP, //|WS_CLIPSIBLINGS|WS_CLIPCHILDREN,
		0,0,vid_width,vid_height,NULL,NULL,hInst,NULL);
		ShowCursor(FALSE);
	}
	else
	{
		vid_width = win_width;
		vid_height = win_height;
		hWnd = CreateWindowEx(0, "Kittens", "Kittens",
		WS_TILEDWINDOW, //|WS_CLIPSIBLINGS|WS_CLIPCHILDREN,
		50,50,vid_width,vid_height,NULL,NULL,hInst,NULL);
		ShowCursor(TRUE);
	}

	fullscreen_toggle = 0;
	fullscreen = !fullscreen;

	hDC = GetDC(hWnd);
	win_pixelformat();
	wglMakeCurrent(hDC, hGLRC);
	ReleaseDC(hWnd, hDC);

	glViewport(0,0,vid_width, vid_height);
	ShowWindow(hWnd, CmdShow);
	UpdateWindow(hWnd);
}


char *winClassName = "Kittens";
static void win_init(void)
{
	memset(keys, 0, KEYMAX);
	memset(joy, 0, sizeof(joystick)*4);

	HMODULE l = LoadLibrary("shcore.dll");
	if(NULL != l)
	{
		void (*SetProcessDpiAwareness)() =
			(void(*)())GetProcAddress(l, "SetProcessDpiAwareness");

		void (*GetDpiForMonitor)() =
			(void(*)())GetProcAddress(l, "GetDpiForMonitor");

		SetProcessDpiAwareness(2);// 2 = Process_Per_Monitor_DPI_Aware
		int dpi_x=0, dpi_y=0;
		POINT p = {1, 1};
		HMONITOR hmon = MonitorFromPoint(p, MONITOR_DEFAULTTOPRIMARY);
		GetDpiForMonitor(hmon, 0, &dpi_x, &dpi_y);
		FreeLibrary(l);
		sys_dpi = (float)dpi_x / 96.0;
	}

	WNDCLASSEX wc;
	wc.cbSize	= sizeof(WNDCLASSEX);
	wc.style	= CS_OWNDC;
	wc.lpfnWndProc	= (WNDPROC)wProc;
	wc.cbClsExtra	= 0;
	wc.cbWndExtra	= 0;
	wc.hInstance	= hInst;
	wc.hIcon	= LoadIcon(hInst, MAKEINTRESOURCE(0));
	wc.hIconSm	= NULL; //LoadIcon(hInst, IDI_APPLICAITON);
	wc.hCursor	= LoadCursor(NULL, IDC_ARROW);
	wc.hbrBackground= NULL;
	wc.lpszMenuName	= NULL;
	wc.lpszClassName= winClassName;

	if(!RegisterClassEx(&wc))
	{
		log_fatal("RegisterClassEx() failed\n");
		return;
	}

	hWnd = CreateWindowEx(0, "Kittens", "Kittens",
		WS_TILEDWINDOW, //|WS_CLIPSIBLINGS|WS_CLIPCHILDREN,
		50,50,vid_width,vid_height,NULL,NULL,hInst,NULL);

	hDC = GetDC(hWnd);
	if(!hWnd)
	{
		log_fatal("CreateWindowEx() failed\n");
		return;
	}
	ShowWindow(hWnd, CmdShow);
	UpdateWindow(hWnd);

	win_pixelformat();

	HGLRC tmpGLRC;

	tmpGLRC = wglCreateContext(hDC);
	if(!tmpGLRC)
	{
		fail("wglCreateContext() failed");
		return;
	}
	if(!wglMakeCurrent(hDC, tmpGLRC))
	{
		fail("wglMakeCurrent() failed");
		return;
	}
	glewInit();
	GLint glattrib[] = {
		WGL_CONTEXT_MAJOR_VERSION_ARB, 4,
		WGL_CONTEXT_MINOR_VERSION_ARB, 1,
//		WGL_CONTEXT_PROFILE_MASK_ARB, WGL_CONTEXT_CORE_PROFILE_BIT_ARB,
		WGL_CONTEXT_PROFILE_MASK_ARB, WGL_CONTEXT_COMPATIBILITY_PROFILE_BIT_ARB,
//		WGL_CONTEXT_FLAGS_ARB, WGL_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB,
		WGL_CONTEXT_FLAGS_ARB, WGL_CONTEXT_DEBUG_BIT_ARB,
		0 };

	if(1 == wglewIsSupported("WGL_ARB_create_context"))
	{
		hGLRC = wglCreateContextAttribsARB(hDC, 0, glattrib);
		wglMakeCurrent(NULL, NULL);
		wglDeleteContext(tmpGLRC);
		wglMakeCurrent(hDC, hGLRC);
	}
	else hGLRC = tmpGLRC;
//	ReleaseDC(hWnd, hDC);

}

static void win_end(void)
{
	if(fullscreen)ShowCursor(TRUE);
	wglMakeCurrent(NULL, NULL);
	wglDeleteContext(hGLRC);
	DestroyWindow(hWnd);
	UnregisterClass(winClassName, hInst);
}

static void handle_events(void)
{
	MSG mesg;
	mickey_x = mickey_y = 0;
	sys_input();

	while(PeekMessage(&mesg, NULL, 0, 0, PM_REMOVE))
	{
		if(mesg.message == WM_QUIT)
			killme=TRUE;
		if(mesg.hwnd == hWnd)
		{
			wProc(mesg.hwnd,mesg.message,mesg.wParam,mesg.lParam);
		}
		else
		{
			TranslateMessage(&mesg);
			DispatchMessage(&mesg);
		}
	}

	if(fullscreen_toggle)
	{
		win_toggle();
	}
}


int APIENTRY WinMain(HINSTANCE hCurrentInst, HINSTANCE hPrev,
	    LPSTR lpszCmdLine, int nCmdShow)
{
	hInst = hCurrentInst;
	CmdShow = nCmdShow;

	/* Convert win32 style arguments to standard format */
#define ARGC_MAX 10
	int last=0;
	int escape=0;
	int argc=1;
	char *argv[ARGC_MAX];
	argv[0] = NULL;
	for(int i=0; i<1000; i++)
	{
		if(ARGC_MAX <= argc)break;
		if(lpszCmdLine[i] == '"')escape = !escape;
		if(lpszCmdLine[i]==0 || (!escape && lpszCmdLine[i] == ' '))
		{
			int size = i - last;
			char *arg = malloc(size+1);
			memcpy(arg, lpszCmdLine+last, size);
			arg[size]=0;
			argv[argc]=arg;
			argc++;
			while(lpszCmdLine[i]==' ')i++;
			last = i;
			if(lpszCmdLine[i]==0)break;
		}
	}
#undef ARGC_MAX

	win_init();
	int ret = main_init(argc, argv);
	for(int i=0; i<argc; i++)free(argv[i]); /* delete args */
	if(ret)
	{
		win_end();
		return ret;
	}

	while(!killme)
	{
		handle_events();
		main_loop();
		SwapBuffers(hDC);
	}

	main_end();
	win_end();
	return 0;
}


