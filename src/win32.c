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
//#include <Xinput.h>

#include <GL/glew.h>

#include <stdio.h>

#include "keyboard.h"



int killme=0;
int sys_width  = 1980;	/* dimensions of default screen */
int sys_height = 1200;
int vid_width  = 1280;	/* dimensions of our part of the screen */
int vid_height = 720;
int win_width  = 0;		/* used for switching from fullscreen back to window */
int win_height = 0;
int mouse_x = 0;
int mouse_y = 0;
int mickey_x = 0;
int mickey_y = 0;
char mouse[] = {0,0,0};
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


HINSTANCE hInst;
HWND hWnd;
int CmdShow;
HDC hDC;
HGLRC hGLRC;

int window_maximized = 0;
int focus = 1;
int menu = 1;
int sys_bpp = 24;

static void fail(const char * string)
{
    int err;
    char errStr[1000];
    err = GetLastError();
    FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, 0, err, 0,
            errStr, 1000, 0);
    printf("%s: %s", string, errStr);
}


#ifdef XBOX360PAD

#define THUMB XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE
#define TRIGGER XINPUT_GAMEPAD_TRIGGER_THRESHOLD
#define CAP 32767
static void sys_input(void)
{

    float x, l, r, mag, normx;
    int i;
    DWORD result;
    XINPUT_STATE state;

    memset(&pin1, 0, sizeof(playerstate));

    for(i=0; i<1; i++)
    {
        ZeroMemory( &state, sizeof(XINPUT_STATE) );
        result = XInputGetState( i, &state );
        if( result != ERROR_SUCCESS ) continue;

        x = state.Gamepad.sThumbLX;
        if( x > 0.0 )
        {
            if( x < THUMB ) x = 0.0;
            else
            {
                if( x > CAP ) x = CAP;
                x -= THUMB;
            }
        }
        else
        {
            if( x > -THUMB ) x = 0.0;
            else
            {
                if( x < -CAP ) x = -CAP;
                x += THUMB;
            }
        }
        x = x / (CAP - THUMB);

//		x = x * x * x;

        l = state.Gamepad.bLeftTrigger;
        if(l > 255) l = 255;
        l -= TRIGGER;
        if( l < 0.0 ) l = 0.0;
        l = l / (255 - TRIGGER);

        r = state.Gamepad.bRightTrigger;
        if(r > 255) r = 255;
        r -= TRIGGER;
        if( r < 0.0 ) r = 0.0;
        r = r / (255 - TRIGGER);

        pin1.steering = x;
        pin1.throttle = r;
        pin1.brake = l;

        if( state.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_LEFT )
            pin1.steering = -1.0;
        if( state.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_RIGHT )
            pin1.steering = 1.0;
        if( state.Gamepad.wButtons & XINPUT_GAMEPAD_A )
            pin1.throttle = 1.0;
        if( state.Gamepad.wButtons & XINPUT_GAMEPAD_B )
            pin1.brake = 1.0;
    }

    if(keys[KEY_UP]) pin1.throttle = 1.0;
    if(keys[KEY_DOWN]) pin1.brake = 1.0;
    if(keys[KEY_LEFT] && !keys[KEY_RIGHT])pin1.steering = -1.0;
    if(keys[KEY_RIGHT] && !keys[KEY_LEFT])pin1.steering = 1.0;

    if(keys[27])killme = 1;

}
#endif

static LONG WINAPI wProc(HWND hWndProc, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    // PAINTSTRUCT: temporary variable for painting to the window
    static PAINTSTRUCT ps;
    int code;

    switch(uMsg)
    {
        case WM_PAINT:
            if(GetUpdateRect(hWndProc, NULL, 0))
            {
                BeginPaint(hWndProc, &ps);
//				DrawLoop(0);
                EndPaint(hWndProc, &ps);
            }
            return 0;

        case WM_SYSCOMMAND:
            switch(wParam & 0xFFF0)
            {
                case SC_MAXIMIZE:
                    window_maximized = 1;
                    break;
                case SC_RESTORE:
                    window_maximized = 0;
                    break;
                default:
                    break;
            }
            break;

        case WM_SIZING:
        case WM_SIZE:
            vid_width = LOWORD(lParam);
            vid_height = HIWORD(lParam);
            glViewport(0, 0, vid_width, vid_height);
            PostMessage(hWndProc, WM_PAINT, 0, 0);
            return 0;

        case WM_KEYDOWN:
            code = (HIWORD(lParam)) & 511;
            if(code < KEYMAX)
                keys[code]=1;
            return 0;
        case WM_KEYUP:
            code = HIWORD(lParam) & 511;
            if(code < KEYMAX)
                keys[code]=0;
            if(code == KEY_F11)
                fullscreen_toggle = 1;
            return 0;

        case WM_LBUTTONDOWN:
            mouse[0]=1;
            break;
        case WM_LBUTTONUP:
            mouse[0]=0;
            break;
        case WM_MBUTTONDOWN:
            mouse[1]=1;
            break;
        case WM_MBUTTONUP:
            mouse[1]=0;
            break;
        case WM_RBUTTONDOWN:
            mouse[2]=1;
            break;
        case WM_RBUTTONUP:
            mouse[2]=0;
            break;

        case WM_MOUSEMOVE:
            mickey_x += mouse_x - LOWORD(lParam);
            mickey_y += mouse_y - HIWORD(lParam);
            mouse_x = LOWORD(lParam);
            mouse_y = HIWORD(lParam);
//			msg("Mouse %d, %d\n", mickey_x, mickey_y);
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

        case WM_CLOSE:
            killme=1;
            return 0;

    }

    return DefWindowProc(hWndProc, uMsg, wParam, lParam);
}

void win_pixelformat(void)
{
    unsigned char bpp = 24;
    PIXELFORMATDESCRIPTOR pfd = {
        sizeof(PIXELFORMATDESCRIPTOR),
        1,						// version number
        PFD_DRAW_TO_WINDOW |
        PFD_SUPPORT_OPENGL |
        PFD_DOUBLEBUFFER |
        PFD_STEREO_DONTCARE,
        PFD_TYPE_RGBA,
        bpp,
        0, 0, 0, 0, 0, 0,		// color bits ignored
        0,						// no alpha buffer
        0,						// shift bit ignored
        0,						// no accumulation buffer
        0, 0, 0, 0,				// accum bits ignored
        bpp,		// 32-bit z-buffer
        0,						// no stencil buffer
        0,						// no auxiliary buffer
        PFD_MAIN_PLANE,			// main layer
        0,						// reserved
        0, 0, 0					// layer masks ignored
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

    WNDCLASSEX wc;
    wc.cbSize			= sizeof(WNDCLASSEX);
    wc.style			= CS_OWNDC;
    wc.lpfnWndProc		= (WNDPROC)wProc;
    wc.cbClsExtra		= 0;
    wc.cbWndExtra		= 0;
    wc.hInstance		= hInst;
    wc.hIcon			= LoadIcon(hInst, MAKEINTRESOURCE(0));
    wc.hIconSm			= NULL; //LoadIcon(hInst, IDI_APPLICAITON);
    wc.hCursor			= LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground	= NULL;
    wc.lpszMenuName		= NULL;
    wc.lpszClassName	= winClassName;

    if(!RegisterClassEx(&wc))
    {
        printf("RegisterClassEx() failed\n");
        return;
    }

    hWnd = CreateWindowEx(0, "Kittens", "Kittens",
        WS_TILEDWINDOW, //|WS_CLIPSIBLINGS|WS_CLIPCHILDREN,
        50,50,vid_width,vid_height,NULL,NULL,hInst,NULL);

    hDC = GetDC(hWnd);
    if(!hWnd)
    {
        printf("CreateWindowEx() failed\n");
        return;
    }
    ShowWindow(hWnd, CmdShow);
    UpdateWindow(hWnd);

    win_pixelformat();

    hGLRC = wglCreateContext(hDC);
    if(!hGLRC)
    {
        fail("wglCreateContext() failed");
        return;
    }
    if(!wglMakeCurrent(hDC, hGLRC))
    {
        fail("wglMakeCurrent() failed");
        return;
    }

    ReleaseDC(hWnd, hDC);
}

static void win_end(void)
{
    if(fullscreen)ShowCursor(TRUE);
    wglMakeCurrent(NULL, NULL);
    wglDeleteContext(hGLRC);
    DestroyWindow(hWnd);
    UnregisterClass(winClassName, hInst);
}

void handle_events(void)
{
    MSG mesg;
    mickey_x = mickey_y = 0;
    switch(WaitForInputIdle(GetCurrentProcess() , 10))
    {
        case 0:
            break;
        case WAIT_TIMEOUT:
//				msg("Wait timed out");
            break;
        case 0xFFFFFFFF:
            ret = GetLastError();
//				if(lastError)
//					msg("Wait Error:%s", strerror(lastError));
            break;
        default:
//				msg("Wait unexpected retval");
            break;
    }

    while(PeekMessage(&mesg, NULL, 0, 0, PM_REMOVE))
    {
        if(mesg.message == WM_QUIT)
            killme=TRUE;
        if(mesg.hwnd == hWnd)
        {
            wProc(mesg.hwnd, mesg.message, mesg.wParam, mesg.lParam );
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
    glewInit();
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





