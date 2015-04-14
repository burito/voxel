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
#import <IOKit/pwr_mgt/IOPMLib.h>	// to disable sleep
#import <IOKit/hid/IOHIDLib.h>
#include <IOKit/hid/IOHIDKeys.h>
#include <ForceFeedback/ForceFeedback.h>

#include <OpenGL/GL.h>
#include <sys/time.h>

///////////////////////////////////////////////////////////////////////////////
//////// Public Interface to the rest of the program
///////////////////////////////////////////////////////////////////////////////

#include "keyboard.h"

int killme=0;
int sys_width  = 1980;	/* dimensions of default screen */
int sys_height = 1200;
int vid_width  = 1280;	/* dimensions of our part of the screen */
int vid_height = 720;
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
//////// Mac OS X OpenGL window setup
///////////////////////////////////////////////////////////////////////////////

typedef struct osx_joystick
{
	IOHIDDeviceRef device;
	io_service_t iost;
	FFDeviceObjectReference ff;
	FFEFFECT *effect;
	FFCUSTOMFORCE *customforce;
	FFEffectObjectReference effectRef;
} osx_joystick;


osx_joystick osx_joy[4];

void ff_set(void);

static int gargc;
static const char ** gargv;
static NSWindow * window;
static NSApplication * myapp;
static int y_correction = 0;  // to correct mouse position for title bar

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
	ff_set();

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
	ff_set();

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
	IOHIDManagerRef hidManager;
	IOPMAssertionID assertNoSleep;
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

	IOReturn success = IOPMAssertionCreateWithName(kIOPMAssertionTypeNoDisplaySleep, kIOPMAssertionLevelOn, CFSTR("Game Window"), &assertNoSleep);
	if (success != kIOReturnSuccess)
	{
		NSLog(@"Requesting NoSleep failed");
	}
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
	memset(joy, 0, sizeof(joystick)*4);
	memset(osx_joy, 0, sizeof(osx_joystick)*4);

	[self setupGamepad];

	main_init(gargc, gargv);

//	[window toggleFullScreen:(self)];
}


- (void)applicationWillTerminate:(NSNotification *)aNotification
{
	main_end();
	IOPMAssertionRelease(assertNoSleep);
}


void ff_init(osx_joystick* j)
{
	FFCreateDevice(j->iost, &j->ff);
	if (j->ff == 0) return;

	FFCAPABILITIES capabs;
	FFDeviceGetForceFeedbackCapabilities(j->ff, &capabs);

	if(capabs.numFfAxes != 2) return;

	j->effect = malloc(sizeof(FFEFFECT));
	j->customforce = malloc(sizeof(FFCUSTOMFORCE));
	LONG *c = malloc(2 * sizeof(LONG));
	DWORD *a = malloc(2 * sizeof(DWORD));
	LONG *d = malloc(2 * sizeof(LONG));

	c[0] = 0;
	c[1] = 0;
	a[0] = capabs.ffAxes[0];
	a[1] = capabs.ffAxes[1];
	d[0] = 0;
	d[1] = 0;

	j->customforce->cChannels = 2;
	j->customforce->cSamples = 2;
	j->customforce->rglForceData = c;
	j->customforce->dwSamplePeriod = 100*1000;

	j->effect->cAxes = capabs.numFfAxes;
	j->effect->rglDirection = d;
	j->effect->rgdwAxes = a;
	j->effect->dwSamplePeriod = 0;
	j->effect->dwGain = 10000;
	j->effect->dwFlags = FFEFF_OBJECTOFFSETS | FFEFF_SPHERICAL;
	j->effect->dwSize = sizeof(FFEFFECT);
	j->effect->dwDuration = FF_INFINITE;
	j->effect->dwSamplePeriod = 100*1000;
	j->effect->cbTypeSpecificParams = sizeof(FFCUSTOMFORCE);
	j->effect->lpvTypeSpecificParams = j->customforce;
	j->effect->lpEnvelope = NULL;
	FFDeviceCreateEffect(j->ff, kFFEffectType_CustomForce_ID, j->effect, &j->effectRef);
}

void ff_shutdown(osx_joystick *j)
{
	if (j->effectRef == NULL) return;
	FFDeviceReleaseEffect(j->ff, j->effectRef);
	j->effectRef = NULL;
	free(j->customforce->rglForceData);
	free(j->effect->rgdwAxes);
	free(j->effect->rglDirection);
	free(j->customforce);
	free(j->effect);
	FFReleaseDevice(j->ff);
}
void ff_set(void)
{
	for(int i=0; i<4; i++)
	{
		if(osx_joy[i].effectRef == NULL) continue;
		osx_joy[i].customforce->rglForceData[0] = (joy[i].fflarge * 10000) / 255;
		osx_joy[i].customforce->rglForceData[1] = (joy[i].ffsmall * 10000) / 255;
		FFEffectSetParameters(osx_joy[i].effectRef, osx_joy[i].effect, FFEP_TYPESPECIFICPARAMS);
		FFEffectStart(osx_joy[i].effectRef, 1, 0);
	}
}

void gamepadWasAdded(void* inContext, IOReturn inResult, void* inSender, IOHIDDeviceRef device)
{
	for(int i=0; i<4; i++)
	if(osx_joy[i].device == 0)
	{
		osx_joy[i].device = device;
		osx_joy[i].iost = IOHIDDeviceGetService(device);
		ff_init(&osx_joy[i]);
		joy[i].connected = 1;
		return;
	}
	printf("More than 4 joysticks plugged in\n");
}

void gamepadWasRemoved(void* inContext, IOReturn inResult, void* inSender, IOHIDDeviceRef device)
{
	for(int i=0; i<4; i++)
	if(osx_joy[i].device == device)
	{
		ff_shutdown(&osx_joy[i]);
		osx_joy[i].device = 0;
		joy[i].connected = 0;
		return;
	}
	printf("Unexpected Joystick unplugged\n");
}

// https://developer.apple.com/library/mac/documentation/DeviceDrivers/Conceptual/HID/new_api_10_5/tn2187.html#//apple_ref/doc/uid/TP40000970-CH214-SW2
void gamepadAction(void* inContext, IOReturn inResult,
		   void* inSender, IOHIDValueRef valueRef) {

	IOHIDElementRef element = IOHIDValueGetElement(valueRef);
	Boolean isElement = CFGetTypeID(element) == IOHIDElementGetTypeID();
	if (!isElement)
		return;
//	IOHIDElementCookie cookie = IOHIDElementGetCookie(element);
//	IOHIDElementType type = IOHIDElementGetType(element);
//	CFStringRef name = IOHIDElementGetName(element);
	int page = IOHIDElementGetUsagePage(element);
	int usage = IOHIDElementGetUsage(element);

	long value = IOHIDValueGetIntegerValue(valueRef);

	int i = -1;
	for(int j=0; j<4; j++)
	if(osx_joy[j].device == inSender)
	{
		i = j;
		break;
	}
	if(i == -1)
	{
		printf("Unexpected joystick event = %p\n", inSender);
		return;
	}
	// page == 1 = axis
	// page == 9 = button
	switch(usage) {
	case 1:	// A
	case 2:	// B
	case 3:	// X
	case 4:	// Y
	case 5:	// LB
	case 6: // RB
	case 7: // LJ
	case 8: // RJ
	case 9: // Start
	case 10: // Select
	case 11: // Logo
	case 12: // Dpad Up
	case 13: // Dpad Down
	case 14: // Dpad Left
	case 15: // Dpad Right
		joy[i].button[usage-1] = value;
		break;
	case 48: // L-X
		joy[i].l.x = value;
		break;
	case 49: // L-Y
		joy[i].l.y = value;
		break;
	case 51: // R-X
		joy[i].r.x = value;
		break;
	case 52: // R-Y
		joy[i].r.y = value;
		break;
	case 50: // LT
		joy[i].lt = value;
//		joy[i].fflarge = value;
		break;
	case 53: // RT
		joy[i].rt = value;
//		joy[i].ffsmall = value;
		break;
	default:
		printf("usage = %d, page = %d, value = %d\n", usage, page, (int)value);
		break;
	}
}

-(void) setupGamepad {
	hidManager = IOHIDManagerCreate( kCFAllocatorDefault, kIOHIDOptionsTypeNone);
	NSMutableDictionary* criterion = [[NSMutableDictionary alloc] init];
	[criterion setObject: [NSNumber numberWithInt: kHIDPage_GenericDesktop] forKey: (NSString*)CFSTR(kIOHIDDeviceUsagePageKey)];
	[criterion setObject: [NSNumber numberWithInt: kHIDUsage_GD_GamePad] forKey: (NSString*)CFSTR(kIOHIDDeviceUsageKey)];
	IOHIDManagerSetDeviceMatching(hidManager, (__bridge CFDictionaryRef)criterion);
	IOHIDManagerRegisterDeviceMatchingCallback(hidManager, gamepadWasAdded, (void*)self);
	IOHIDManagerRegisterDeviceRemovalCallback(hidManager, gamepadWasRemoved, (void*)self);
	IOHIDManagerScheduleWithRunLoop(hidManager, CFRunLoopGetCurrent(), kCFRunLoopDefaultMode);
	IOHIDManagerOpen(hidManager, kIOHIDOptionsTypeNone);
	IOHIDManagerRegisterInputValueCallback(hidManager, gamepadAction, (void*)self);
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
	int bit=0;
	switch(theEvent.type) {
	case NSLeftMouseDown:
		bit = 1;
	case NSLeftMouseUp:
		mouse[0] = bit;
		mouse_move(theEvent);
		break;
	case NSRightMouseDown:
		bit = 1;
	case NSRightMouseUp:
		mouse[1] = bit;
		mouse_move(theEvent);
		break;
	case NSOtherMouseDown:
		bit = 1;
	case NSOtherMouseUp:
		switch(theEvent.buttonNumber) {
		case 2: mouse[2] = bit; break;
		case 3: mouse[3] = bit; break;
		case 4: mouse[4] = bit; break;
		case 5: mouse[5] = bit; break;
		case 6: mouse[6] = bit; break;
		case 7: mouse[7] = bit; break;
		default: printf("Unexpected Mouse Button %d\n", (int)theEvent.buttonNumber); break;
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
		bit = 1;
	case NSKeyUp:
		keys[theEvent.keyCode] = bit;
		break;
	case NSFlagsChanged:
		for(int i = 0; i<24; i++)
		{
			bit = !!(theEvent.modifierFlags & (1 << i));
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
		mouse[0] = 0;
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
