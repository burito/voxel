//
//  main.m
//  voxel
//
//  Created by Daniel Burke on 15/03/2015.
//  Copyright (c) 2015 Daniel Burke. All rights reserved.
//

#import <Cocoa/Cocoa.h>
#include <OpenGL/GL.h>



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

const int sys_ticksecond = 1000;
long long sys_timevalue = 0;
long long sys_time(void)
{
    return sys_timevalue;
}

void shell_browser(char *url)
{
//    NSURL *MyNSURL = [NSURL fileURLWithPath:[NSString initWithUTF8String:url]];
    NSURL *MyNSURL = [NSURL URLWithString:[NSString stringWithUTF8String:url]];
    [[NSWorkspace sharedWorkspace] openURL:MyNSURL];
}

int gargc;
const char ** gargv;
NSWindow * window;
int y_correction = 0;  // to correct mouse position for title bar

void osx_init(void)
{
}

NSOpenGLContext *MyContext;

@interface MyOpenGLView : NSOpenGLView
{
    CVDisplayLinkRef displayLink;
}
//-(void) drawRect:(NSRect)dirtyRect;
@end

@implementation MyOpenGLView

- (BOOL)acceptsFirstResponder
{
    return YES;
}

-(void)keyDown:(NSEvent *)event
{
	printf("keydown:%s\n", [event charactersIgnoringModifiers]);

	[super keyDown:event];
}

-(void)mouseDown:(NSEvent *)event
{
    mouse_x = event.locationInWindow.x * bsFactor;
    mouse_y = -(event.locationInWindow.y + y_correction) * bsFactor;
    mouse[0] = 1;
}
-(void)mouseUp:(NSEvent *)event
{
    mouse[0] = 0;
}

-(void)otherMouseDown:(NSEvent *)event
{
    int a = [event buttonNumber];
    switch(a) {
    case 2: mouse[2] = 1; break;
    case 3: mouse[3] = 1; break;
    case 4: mouse[4] = 1; break;
    case 5: mouse[5] = 1; break;
    case 6: mouse[6] = 1; break;
    case 7: mouse[7] = 1; break;
    default:
    printf("Mouse button %d\n", a);
        break;
    }
}

-(void)otherMouseUp:(NSEvent *)event
{
    int a = [event buttonNumber];
    switch(a) {
    case 2: mouse[2] = 0; break;
    case 3: mouse[3] = 0; break;
    case 4: mouse[4] = 0; break;
    case 5: mouse[5] = 0; break;
    case 6: mouse[6] = 0; break;
    case 7: mouse[7] = 0; break;
    default: break;
    }
}


-(void)rightMouseDown:(NSEvent *)event
{
    mouse[1] = 1;
}
-(void)rightMouseUp:(NSEvent *)event
{
    mouse[1] = 0;
}


-(void)mouseMoved:(NSEvent *)event
{
    mouse_x = event.locationInWindow.x * bsFactor;
    mouse_y = -(event.locationInWindow.y + y_correction) * bsFactor;
    mickey_x -= event.deltaX * bsFactor;
    mickey_y -= event.deltaY * bsFactor;
    printf("mmouse = %d, %d\n", mouse_x, mouse_y);
}

-(void)mouseDragged:(NSEvent *)event
{
    mouse_x = event.locationInWindow.x * bsFactor;
    mouse_y = -(event.locationInWindow.y + y_correction) * bsFactor;
    mickey_x -= event.deltaX * bsFactor;
    mickey_y -= event.deltaY * bsFactor;
    printf("dmouse = %d, %d\n", mouse_x, mouse_y);

}



-(void)reshape
{
//    vid_width = [[self superview] bounds].size.width;
//    vid_height = [[self superview] bounds].size.height;
#ifdef RETINA_TEST
    vid_width = [[self window] frame].size.width;
    vid_height = [[self window] frame].size.height;
    y_correction = [[[self window] contentView] frame].size.height - vid_height;

    if(vid_height == 0) vid_height = 1;
    glViewport(0, y_correction * bsFactor, vid_width*bsFactor, vid_height*bsFactor);
// glScissor knows the real resolution??!?!?
#else
    vid_width = [self convertRectToBacking:[self bounds]].size.width;
    vid_height = [self convertRectToBacking:[self bounds]].size.height;
    y_correction = [[[self window] contentView] frame].size.height - vid_height;
    if(vid_height == 0) vid_height = 1;
    glViewport(0, y_correction * bsFactor, vid_width, vid_height);
#endif
}

- (void)prepareOpenGL
{
    GLint vsync = 1;
//    MyContext = [NSOpenGLContext currentContext];
//    [context makeCurrentContext];

    [self setWantsBestResolutionOpenGLSurface:YES];   // enable retina resolutions
    bsFactor = [self.window backingScaleFactor];

    [[self openGLContext] setValues:&vsync forParameter:NSOpenGLCPSwapInterval];
    CVDisplayLinkCreateWithActiveCGDisplays(&displayLink);
    CVDisplayLinkSetOutputCallback(displayLink, &MyDisplayLinkCallback, (__bridge void *)(self));
    CGLContextObj cglContext = [[self openGLContext] CGLContextObj];
    CGLPixelFormatObj cglPixelFormat = [[self pixelFormat] CGLPixelFormatObj];
    CVDisplayLinkSetCurrentCGDisplayFromOpenGLContext(displayLink, cglContext, cglPixelFormat);
    CVDisplayLinkStart(displayLink);
    [self reshape];
}

static CVReturn MyDisplayLinkCallback(CVDisplayLinkRef displayLink, const CVTimeStamp* now, const CVTimeStamp* outputTime, CVOptionFlags flagsIn, CVOptionFlags* flagsOut, void* displayLinkContext)
{
    MyOpenGLView *glview = (__bridge MyOpenGLView*) displayLinkContext;
    [[glview openGLContext] makeCurrentContext];
    main_loop();

    mickey_x = mickey_y = 0;
    if(killme)
    {
//        [glview terminate];
//        [NSApp terminate:nil];
        [NSApp performSelector:@selector(terminate:) withObject:nil afterDelay:0.001];
    }

    if(fullscreen_toggle)
    {
//        [glview toggleFullScreen];
        fullscreen_toggle = 0;

        [window toggleFullScreen:(window)];
    }


    [[glview openGLContext] flushBuffer];
    return kCVReturnSuccess;
}

- (void)dealloc
{
    CVDisplayLinkRelease(displayLink);
    [super dealloc];
}
/*
- (void) drawRect:(NSRect)dirtyRect
{
    [[self openGLContext] makeCurrentContext];
    glClearColor(0, 0, 0, 0);
    glClear(GL_COLOR_BUFFER_BIT);
    glColor3f(1.0f, 0.85f, 0.35f);
    glBegin(GL_TRIANGLES);
    {
        glVertex3f(  0.0,  0.6, 0.0);
        glVertex3f( -0.2, -0.3, 0.0);
        glVertex3f(  0.2, -0.3 ,0.0);
    }
    glEnd();
    [[self openGLContext] flushBuffer];
    
//    glFlush(); 
}*/
@end

@interface AppDelegate : NSObject <NSApplicationDelegate>
{
    NSView * view;
}
@end

@implementation AppDelegate
- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication *)theApplication {
    return YES;
}


-(void) add_menu
{
    NSMenu * mainMenu = [[NSMenu alloc] initWithTitle:@"MainMenu"];
    NSMenuItem * menuItem;
    NSMenu * submenu;
    
    menuItem = [mainMenu addItemWithTitle:@"Apple" action:NULL keyEquivalent:@""];
    submenu = [[NSMenu alloc] initWithTitle:@"Apple"];
    [NSApp performSelector:@selector(setAppleMenu:) withObject:submenu];
    [self populateApplicationMenu:submenu];
    [mainMenu setSubmenu:submenu forItem:menuItem];
    [NSApp setMainMenu:mainMenu];
}

-(void) populateApplicationMenu:(NSMenu *)aMenu
{
    NSString * applicationName = @"voxel";
    NSMenuItem * menuItem;
    
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
}

-(id)init
{
    
    if(self = [super init]) {
        NSRect contentSize = NSMakeRect(100.0, 400.0, 640.0, 360.0);
        NSUInteger windowStyleMask = NSTitledWindowMask | NSResizableWindowMask | NSClosableWindowMask | NSMiniaturizableWindowMask;
        window = [[NSWindow alloc] initWithContentRect:contentSize styleMask:windowStyleMask backing:NSBackingStoreBuffered defer:YES];
        window.backgroundColor = [NSColor whiteColor];
        window.title = @"Kittens";
        
        [window setCollectionBehavior:(NSWindowCollectionBehaviorFullScreenPrimary)];
        
        NSOpenGLPixelFormatAttribute pixelFormatAttributes[] =
        {
//            NSOpenGLPFAOpenGLProfile, NSOpenGLProfileVersion3_2Core,
            NSOpenGLPFAOpenGLProfile, NSOpenGLProfileVersionLegacy,
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
    }
    return self;
}

- (void)applicationWillFinishLaunching:(NSNotification *)aNotification
{
    [self add_menu];
    [window setContentView:view];

}

- (void)applicationDidFinishLaunching:(NSNotification *)aNotification
{
    [window makeKeyAndOrderFront:self];
    [window setAcceptsMouseMovedEvents:YES];

    memset(keys, 0, KEYMAX);
    main_init(gargc, gargv);

//    [window toggleFullScreen:(self)];
}


- (void)applicationWillTerminate:(NSNotification *)aNotification
{
    main_end();
}

@end



int main(int argc, const char * argv[])
{
    gargc = argc;
    gargv = argv;
    NSApplication * app = [NSApplication sharedApplication];
    AppDelegate * appd = [[AppDelegate alloc] init];
    [app setDelegate:appd];
    [app run];
    [app setDelegate:nil];
    return 0;
}
