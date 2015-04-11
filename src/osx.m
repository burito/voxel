/*
Copyright (c) 2015 Daniel Burke

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

//#define CVDISPLAYLINK		// or use an NSTimer
//#define MODERN_OPENGL		// or use a GL2 context

#import <Cocoa/Cocoa.h>
#include <OpenGL/GL.h>
#include <sys/time.h>

////////////////////////////////////////////////////////////////////////////////
//////// Public Interface to the rest of the program
////////////////////////////////////////////////////////////////////////////////

#include "keyboard.h"

int killme=0;
int sys_width  = 1980;	/* dimensions of default screen */
int sys_height = 1200;
int vid_width  = 1280;	/* dimensions of our part of the screen */
int vid_height = 720;
int win_width  = 0;	/* used for switching from fullscreen back to window */
int win_height = 0;
int mouse_x = 0;
int mouse_y = 0;
int mickey_x = 0;
int mickey_y = 0;
char mouse[] = {0,0,0,0,0,0,0,0};
// the Logitech drivers are happy to send up to 8 numbered "mouse" buttons
// http://www.logitech.com/pub/techsupport/mouse/mac/lcc3.9.1.b20.zip

#define KEYMAX 512
char keys[KEYMAX];

int fullscreen = 0;
int fullscreen_toggle = 0;

float bsFactor = 1.0;	// the "Backing Scale Factor"

int main_init(int argc, const char *argv[]);
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
	NSURL *MyNSURL = [NSURL URLWithString:[NSString stringWithUTF8String:url]];
	[[NSWorkspace sharedWorkspace] openURL:MyNSURL];
}

////////////////////////////////////////////////////////////////////////////////
//////// Mac OS X OpenGL window setup
////////////////////////////////////////////////////////////////////////////////

static int gargc;
static const char ** gargv;
static NSWindow * window;
static NSApplication * myapp;
static int y_correction = 0;  // to correct mouse position for title bar

static NSOpenGLContext *MyContext;

@interface MyOpenGLView : NSOpenGLView
{
#ifdef CVDISPLAYLINK
	CVDisplayLinkRef displayLink;
#else
	NSTimer * renderTimer;
#endif
}
@end

@implementation MyOpenGLView

-(BOOL)acceptsFirstResponder
{
    return YES;
}

-(void)reshape
{
	vid_width = [self convertRectToBacking:[self bounds]].size.width;
	vid_height = [self convertRectToBacking:[self bounds]].size.height;
	y_correction = bsFactor * [[[self window] contentView] frame].size.height - vid_height;
	if(vid_height == 0) vid_height = 1;
	glViewport(0, y_correction, vid_width, vid_height);
}


- (void)prepareOpenGL
{
	GLint vsync = 0;

	[self setWantsBestResolutionOpenGLSurface:YES];   // enable retina resolutions
	bsFactor = [self.window backingScaleFactor];
	[[self openGLContext] setValues:&vsync forParameter:NSOpenGLCPSwapInterval];
#ifdef CVDISPLAYLINK
	// Use a CVDisplayLink to do the render loop
	CVDisplayLinkCreateWithActiveCGDisplays(&displayLink);
	CVDisplayLinkSetOutputCallback(displayLink, &MyDisplayLinkCallback, (__bridge void *)(self));
	CGLContextObj cglContext = [[self openGLContext] CGLContextObj];
	CGLPixelFormatObj cglPixelFormat = [[self pixelFormat] CGLPixelFormatObj];
	CVDisplayLinkSetCurrentCGDisplayFromOpenGLContext(displayLink, cglContext, cglPixelFormat);
	CVDisplayLinkStart(displayLink);
#else
	// Use an NSTimer to do the render loop
	[self reshape];
	renderTimer = [NSTimer scheduledTimerWithTimeInterval:0.001
						       target:self
						     selector:@selector(timerFired:)
						     userInfo:nil
						      repeats:YES];
	[[NSRunLoop currentRunLoop] addTimer:renderTimer forMode:NSDefaultRunLoopMode];
	[[NSRunLoop currentRunLoop] addTimer:renderTimer forMode:NSEventTrackingRunLoopMode];
	//Ensure timer fires during resize
#endif
	[NSApp activateIgnoringOtherApps:YES];  // to the front!

}

#ifdef CVDISPLAYLINK
// This is the CVDisplayLink callback
static CVReturn MyDisplayLinkCallback(CVDisplayLinkRef displayLink, const CVTimeStamp* now, const CVTimeStamp* outputTime, CVOptionFlags flagsIn, CVOptionFlags* flagsOut, void* displayLinkContext)
{
	MyOpenGLView *glview= (__bridge MyOpenGLView*) displayLinkContext;
	NSOpenGLContext *glcontext = [glview openGLContext];
	CGLContextObj context = [glcontext CGLContextObj];

	CGLLockContext(context);
	[glcontext makeCurrentContext];
	main_loop();
	[glcontext flushBuffer];
	CGLUnlockContext(context);

	mickey_x = mickey_y = 0;
	if(killme)
	{
		[NSApp performSelector:@selector(terminate:) withObject:nil afterDelay:0.0];
	}

	if(fullscreen_toggle)
	{
		fullscreen_toggle = 0;
		[window toggleFullScreen:(window)];
	}

	return kCVReturnSuccess;
}

- (void)dealloc
{
	CVDisplayLinkRelease(displayLink);
	[super dealloc];
}

#else
-(void)drawRect:(NSRect)dirtyRect
{
	[[self openGLContext] makeCurrentContext];
	main_loop();
	[[self openGLContext] flushBuffer];

	mickey_x = mickey_y = 0;

	if(killme)
	{
		[NSApp performSelector:@selector(terminate:) withObject:nil afterDelay:0.0];
	}

	if(fullscreen_toggle)
	{
		fullscreen_toggle = 0;
		[window toggleFullScreen:(window)];
	}
}

- (void)timerFired:(NSTimer*)timer
{
	[self setNeedsDisplay:YES];
}
#endif

@end

@interface AppDelegate : NSObject <NSApplicationDelegate>
{
	NSView * view;
}
@end

@implementation AppDelegate
- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication *)app
{
	return YES;
}

-(id)init
{
	NSRect contentSize = NSMakeRect(100.0, 400.0, 640.0, 360.0);
	NSUInteger windowStyleMask = NSTitledWindowMask | NSResizableWindowMask | NSClosableWindowMask | NSMiniaturizableWindowMask;
	window = [[NSWindow alloc] initWithContentRect:contentSize styleMask:windowStyleMask backing:NSBackingStoreBuffered defer:YES];
	window.backgroundColor = [NSColor whiteColor];
	window.title = @"Kittens";

	[window setCollectionBehavior:(NSWindowCollectionBehaviorFullScreenPrimary)];

	NSOpenGLPixelFormatAttribute pixelFormatAttributes[] =
	{
#ifdef MODERN_OPENGL
		NSOpenGLPFAOpenGLProfile, NSOpenGLProfileVersion3_2Core,
#else
		NSOpenGLPFAOpenGLProfile, NSOpenGLProfileVersionLegacy,
#endif
		NSOpenGLPFAColorSize    , 24,
		NSOpenGLPFAAlphaSize    , 8,
		NSOpenGLPFADepthSize    , 24,
		NSOpenGLPFADoubleBuffer ,
		NSOpenGLPFAAccelerated  ,
		NSOpenGLPFANoRecovery   ,
		0
	};
	NSOpenGLPixelFormat *pixelFormat = [[NSOpenGLPixelFormat alloc] initWithAttributes:pixelFormatAttributes];

	view = [[MyOpenGLView alloc] initWithFrame:[[window contentView] bounds] pixelFormat:pixelFormat];
	return [super init];
}

- (void)applicationWillFinishLaunching:(NSNotification *)aNotification
{
	// Create the menu that goes on the Apple Bar
	NSMenu * mainMenu = [[NSMenu alloc] initWithTitle:@"MainMenu"];
	NSMenuItem * menuTitle;
	NSMenu * aMenu;

	menuTitle = [mainMenu addItemWithTitle:@"Apple" action:NULL keyEquivalent:@""];
	aMenu = [[NSMenu alloc] initWithTitle:@"Apple"];
	[NSApp performSelector:@selector(setAppleMenu:) withObject:aMenu];

	// generate contents of menu
	NSMenuItem * menuItem;
	NSString * applicationName = @"voxel";
	menuItem = [aMenu addItemWithTitle:[NSString stringWithFormat:@"%@ %@", NSLocalizedString(@"About", nil), applicationName]
				    action:@selector(orderFrontStandardAboutPanel:)
			     keyEquivalent:@""];
	[menuItem setTarget:NSApp];
	[aMenu addItem:[NSMenuItem separatorItem]];

	menuItem = [aMenu addItemWithTitle:NSLocalizedString(@"Fullscreen", nil)
				    action:@selector(toggleFullScreen:)
			     keyEquivalent:@"f"];
	[menuItem setKeyEquivalentModifierMask:NSCommandKeyMask | NSControlKeyMask];
	menuItem.target = nil;

	[aMenu addItem:[NSMenuItem separatorItem]];

	menuItem = [aMenu addItemWithTitle:[NSString stringWithFormat:@"%@ %@", NSLocalizedString(@"Quit", nil), applicationName]
				    action:@selector(terminate:)
			     keyEquivalent:@"q"];
	[menuItem setTarget:NSApp];

	// attach generated menu to menuitem
	[mainMenu setSubmenu:aMenu forItem:menuTitle];
	[NSApp setMainMenu:mainMenu];

	// Because this is where you do it?
	[window setContentView:view];
}

- (void)applicationDidFinishLaunching:(NSNotification *)aNotification
{
	[window makeKeyAndOrderFront:self];
	[window setAcceptsMouseMovedEvents:YES];
	[window makeFirstResponder:window];

	memset(keys, 0, KEYMAX);
	main_init(gargc, gargv);

//	[window toggleFullScreen:(self)];
}


- (void)applicationWillTerminate:(NSNotification *)aNotification
{
    main_end();
}

@end

@interface MyApp : NSApplication
{
}
@end


static void mouse_move(NSEvent * theEvent)
{
    mouse_x = theEvent.locationInWindow.x * bsFactor;
    mouse_y = vid_height-(theEvent.locationInWindow.y + y_correction) * bsFactor;
    mickey_x -= theEvent.deltaX * bsFactor;
    mickey_y -= theEvent.deltaY * bsFactor;
}

@implementation MyApp

-(void)sendEvent:(NSEvent *)theEvent
{
// https://developer.apple.com/library/mac/documentation/Cocoa/Reference/ApplicationKit/Classes/NSEvent_Class/#//apple_ref/c/tdef/NSEventType
    
    switch(theEvent.type) {
        case NSLeftMouseDown:
            mouse[0] = 1;
            mouse_move(theEvent);
            break;
        case NSLeftMouseUp:
            mouse[0] = 0;
            mouse_move(theEvent);
            break;
        case NSRightMouseDown:
            mouse[1] = 1;
            mouse_move(theEvent);
            break;
        case NSRightMouseUp:
            mouse[1] = 0;
            mouse_move(theEvent);
            break;
        case NSOtherMouseDown:
            switch(theEvent.buttonNumber) {
                case 2: mouse[2] = 1; break;
                case 3: mouse[3] = 1; break;
                case 4: mouse[4] = 1; break;
                case 5: mouse[5] = 1; break;
                case 6: mouse[6] = 1; break;
                case 7: mouse[7] = 1; break;
                default: printf("Unexpected Mouse Button %d\n", (int)theEvent.buttonNumber); break;
            }
            mouse_move(theEvent);
            break;
        case NSOtherMouseUp:
            switch(theEvent.buttonNumber) {
                case 2: mouse[2] = 0; break;
                case 3: mouse[3] = 0; break;
                case 4: mouse[4] = 0; break;
                case 5: mouse[5] = 0; break;
                case 6: mouse[6] = 0; break;
                case 7: mouse[7] = 0; break;
                default: break;
            }
            mouse_move(theEvent);
            break;
           
        case NSMouseMoved:
        case NSLeftMouseDragged:
        case NSRightMouseDragged:
        case NSOtherMouseDragged:
            mouse_move(theEvent);
            break;
            
        case NSKeyDown:
            keys[theEvent.keyCode] = 1;
            
            
            break;
        case NSKeyUp:
            keys[theEvent.keyCode] = 0;
            break;
        case NSFlagsChanged:
            for(int i = 0; i<32; i++)
            {
                int x = 1 << i;
                int bit = !!(theEvent.modifierFlags & x);
                switch(i) {
                    case   0: keys[KEY_LCONTROL] = bit; break;
                    case   1: keys[KEY_LSHIFT] = bit; break;
                    case   2: keys[KEY_RSHIFT] = bit; break;
                    case   3: keys[KEY_LLOGO] = bit; break;
                    case   4: keys[KEY_RLOGO] = bit; break;
                    case   5: keys[KEY_LALT] = bit; break;
                    case   6: keys[KEY_RALT] = bit; break;
                    case   8: break; // Always on?
                    case  13: keys[KEY_RCONTROL] = bit; break;
                    case  16: keys[KEY_CAPSLOCK] = bit; break;
                    case  17: break; // AllShift
                    case  18: break; // AllCtrl
                    case  19: break; // AllAlt
                    case  20: break; // AllLogo
                    case  23: keys[KEY_FN] = bit; break;
                    default: break;
                }
            }
            break;
            
        case NSScrollWheel:
            break;
        
        case NSMouseEntered:
        case NSMouseExited:
            break;
            
        default:
            break;
    }
    [super sendEvent:theEvent];

}

@end


int main(int argc, const char * argv[])
{
    gargc = argc;
    gargv = argv;
    myapp = [MyApp sharedApplication];
    AppDelegate * appd = [[AppDelegate alloc] init];
    [myapp setDelegate:appd];
    [myapp run];
    [myapp setDelegate:nil];
    return 0;
}
