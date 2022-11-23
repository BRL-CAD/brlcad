/*
 * tkMacOSXInit.c --
 *
 *	This file contains Mac OS X -specific interpreter initialization
 *	functions.
 *
 * Copyright (c) 1995-1997 Sun Microsystems, Inc.
 * Copyright (c) 2001-2009, Apple Inc.
 * Copyright (c) 2005-2009 Daniel A. Steffen <das@users.sourceforge.net>
 * Copyright (c) 2017 Marc Culler
 *
 * See the file "license.terms" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#include "tkMacOSXPrivate.h"
#include <dlfcn.h>
#include <objc/objc-auto.h>
#include <sys/stat.h>
#include <sys/utsname.h>

static char tkLibPath[PATH_MAX + 1] = "";

/*
 * If the App is in an App package, then we want to add the Scripts directory
 * to the auto_path.
 */

static char scriptPath[PATH_MAX + 1] = "";

/*
 * Forward declarations...
 */

static int		TkMacOSXGetAppPathCmd(ClientData cd, Tcl_Interp *ip,
			    int objc, Tcl_Obj *const objv[]);

#pragma mark TKApplication(TKInit)

@implementation TKApplication
@synthesize poolLock = _poolLock;
@synthesize macOSVersion = _macOSVersion;
@synthesize isDrawing = _isDrawing;
@synthesize needsToDraw = _needsToDraw;
@end

/*
 * #define this to see a message on stderr whenever _resetAutoreleasePool is
 * called while the pool is locked.
 */
#undef DEBUG_LOCK

@implementation TKApplication(TKInit)
- (void) _resetAutoreleasePool
{
    if ([self poolLock] == 0) {
	[_mainPool drain];
	_mainPool = [NSAutoreleasePool new];
    } else {
#ifdef DEBUG_LOCK
	fprintf(stderr, "Pool is locked with count %d!!!!\n", [self poolLock]);
#endif
    }
}
- (void) _lockAutoreleasePool
{
    [self setPoolLock:[self poolLock] + 1];
}
- (void) _unlockAutoreleasePool
{
    [self setPoolLock:[self poolLock] - 1];
}
#ifdef TK_MAC_DEBUG_NOTIFICATIONS
- (void) _postedNotification: (NSNotification *) notification
{
    TKLog(@"-[%@(%p) %s] %@", [self class], self, _cmd, notification);
}
#endif

- (void) _setupApplicationNotifications
{
    NSNotificationCenter *nc = [NSNotificationCenter defaultCenter];
#define observe(n, s) \
	[nc addObserver:self selector:@selector(s) name:(n) object:nil]
    observe(NSApplicationDidBecomeActiveNotification, applicationActivate:);
    observe(NSApplicationWillResignActiveNotification, applicationDeactivate:);
    observe(NSApplicationDidUnhideNotification, applicationShowHide:);
    observe(NSApplicationDidHideNotification, applicationShowHide:);
    observe(NSApplicationDidChangeScreenParametersNotification, displayChanged:);
    observe(NSTextInputContextKeyboardSelectionDidChangeNotification, keyboardChanged:);
#undef observe
}

-(void)applicationWillFinishLaunching:(NSNotification *)aNotification
{
    (void)aNotification;

    /*
     * Initialize notifications.
     */
#ifdef TK_MAC_DEBUG_NOTIFICATIONS
    [[NSNotificationCenter defaultCenter] addObserver:self
	    selector:@selector(_postedNotification:) name:nil object:nil];
#endif
    [self _setupWindowNotifications];
    [self _setupApplicationNotifications];

    if ([NSApp macOSVersion] >= 110000) {

   /*
    * Initialize Apple Event processing. Apple's docs (see
    * https://developer.apple.com/documentation/appkit/nsapplication)
    * recommend doing this here, although historically we have
    * done this in applicationWillFinishLaunching. In response to
    * bug 7bb246b072.
    */

    TkMacOSXInitAppleEvents(_eventInterp);

    }
}

-(void)applicationDidFinishLaunching:(NSNotification *)notification
{
    (void)notification;

   if ([NSApp macOSVersion] < 110000) {

   /*
    * Initialize Apple Event processing on macOS versions
    * older than Big Sur (11).
    */

    TkMacOSXInitAppleEvents(_eventInterp);

    }


    /*
     * Initialize the graphics context.
     */
    TkMacOSXUseAntialiasedText(_eventInterp, -1);
    TkMacOSXInitCGDrawing(_eventInterp, TRUE, 0);

    /*
     * Construct the menu bar.
     */

    _defaultMainMenu = nil;
    [self _setupMenus];

    /*
     * It is not safe to force activation of the NSApp until this method is
     * called. Activating too early can cause the menu bar to be unresponsive.
     * The call to activateIgnoringOtherApps was moved here to avoid this.
     * However, with the release of macOS 10.15 (Catalina) that was no longer
     * sufficient.  (See ticket bf93d098d7.)  The call to setActivationPolicy
     * needed to be moved into this function as well.
     */

    [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
    [NSApp activateIgnoringOtherApps: YES];

    /*
     * Process events to ensure that the root window is fully initialized. See
     * ticket 56a1823c73.
     */

    [NSApp _lockAutoreleasePool];
    while (Tcl_DoOneEvent(TCL_WINDOW_EVENTS|TCL_DONT_WAIT)) {}
    [NSApp _unlockAutoreleasePool];
}

- (void) _setup: (Tcl_Interp *) interp
{
    /*
     * Remember our interpreter.
     */
    _eventInterp = interp;

    /*
     * Install the global autoreleasePool.
     */
    _mainPool = [NSAutoreleasePool new];
    [NSApp setPoolLock:0];

    /*
     * Record the OS version we are running on.
     */

    int minorVersion, majorVersion;

#if MAC_OS_X_VERSION_MAX_ALLOWED < 101000
    Gestalt(gestaltSystemVersionMinor, (SInt32*)&minorVersion);
    majorVersion = 10;
#else
    NSOperatingSystemVersion systemVersion;
    systemVersion = [[NSProcessInfo processInfo] operatingSystemVersion];
    majorVersion = systemVersion.majorVersion;
    minorVersion = systemVersion.minorVersion;
#endif

    if (majorVersion == 10 && minorVersion == 16) {

	/*
	 * If a program compiled with a macOS 10.XX SDK is run on macOS 11.0 or
	 * later then it will report majorVersion 10 and minorVersion 16, no
	 * matter what the actual OS version of the host may be. And of course
	 * Apple never released macOS 10.16. To work around this we guess the
	 * OS version from the kernel release number, as reported by uname.
	 */  

	struct utsname name;
	char *endptr;
	if (uname(&name) == 0) {
	    majorVersion = strtol(name.release, &endptr, 10) - 9;
	    minorVersion = 0;
	}
    }
    [NSApp setMacOSVersion: 10000*majorVersion + 100*minorVersion];

    /*
     * We are not drawing right now.
     */

    [NSApp setIsDrawing:NO];

    /*
     * Be our own delegate.
     */

    [self setDelegate:self];

    /*
     * If no icon has been set from an Info.plist file, use the Wish icon from
     * the Tk framework.
     */

    NSString *iconFile = [[NSBundle mainBundle] objectForInfoDictionaryKey:
						    @"CFBundleIconFile"];
    if (!iconFile) {
	NSString *path = [NSApp tkFrameworkImagePath:@"Tk.icns"];
	if (path) {
	    NSImage *image = [[NSImage alloc] initWithContentsOfFile:path];
	    if (image) {
		[image setName:@"NSApplicationIcon"];
		[NSApp setApplicationIconImage:image];
		[image release];
	    }
	}
    }
}

- (NSString *) tkFrameworkImagePath: (NSString *) image
{
    NSString *path = nil;
    NSAutoreleasePool *pool = [NSAutoreleasePool new];
    if (tkLibPath[0] != '\0') {
	path = [[NSBundle bundleWithPath:[[NSString stringWithUTF8String:
		tkLibPath] stringByAppendingString:@"/../.."]]
		pathForImageResource:image];
    }
    if (!path) {
	const char *tk_library = Tcl_GetVar2(_eventInterp, "tk_library", NULL,
		TCL_GLOBAL_ONLY);

	if (tk_library) {
	    NSFileManager *fm = [NSFileManager defaultManager];

	    path = [[NSString stringWithUTF8String:tk_library]
		    stringByAppendingFormat:@"/%@", image];
	    if (![fm isReadableFileAtPath:path]) {
		path = [[NSString stringWithUTF8String:tk_library]
			stringByAppendingFormat:@"/../macosx/%@", image];
		if (![fm isReadableFileAtPath:path]) {
		    path = nil;
		}
	    }
	}
    }
#ifdef TK_MAC_DEBUG
    if (!path && getenv("TK_SRCROOT")) {
	path = [[NSString stringWithUTF8String:getenv("TK_SRCROOT")]
		stringByAppendingFormat:@"/macosx/%@", image];
	if (![[NSFileManager defaultManager] isReadableFileAtPath:path]) {
	    path = nil;
	}
    }
#endif
    [path retain];
    [pool drain];
    return path;
}
@end

#pragma mark -

/*
 *----------------------------------------------------------------------
 *
 * TkpInit --
 *
 *	Performs Mac-specific interpreter initialization related to the
 *	tk_library variable.
 *
 * Results:
 *	Returns a standard Tcl result. Leaves an error message or result in
 *	the interp's result.
 *
 * Side effects:
 *	Sets "tk_library" Tcl variable, runs "tk.tcl" script.
 *
 *----------------------------------------------------------------------
 */

/*
 * Helper function which closes the shared NSFontPanel and NSColorPanel.
 */

static void closePanels(
    void)
{
    if ([NSFontPanel sharedFontPanelExists]) {
	[[NSFontPanel sharedFontPanel] orderOut:nil];
    }
    if ([NSColorPanel sharedColorPanelExists]) {
        [[NSColorPanel sharedColorPanel] orderOut:nil];
    }
}

/*
 * This custom exit procedure is called by Tcl_Exit in place of the exit
 * function from the C runtime.  It calls the terminate method of the
 * NSApplication class (superTerminate for a TKApplication).  The purpose of
 * doing this is to ensure that the NSFontPanel and the NSColorPanel are closed
 * before the process exits, and that the application state is recorded
 * correctly for all termination scenarios.
 *
 * TkpWantsExitProc tells Tcl_AppInit whether to install our custom exit proc,
 * which terminates the process by calling [NSApplication terminate].  This
 * does not work correctly if the process is part of an exec pipeline, so it is
 * only done if the process was launched by the launcher or if both stdin and
 * stdout are ttys.  To disable using the custom exit proc altogether, undefine
 * USE_CUSTOM_EXIT_PROC.
 */

#if defined(USE_CUSTOM_EXIT_PROC)
static Bool doCleanupFromExit = NO;

int TkpWantsExitProc(void) {
    return doCleanupFromExit == YES;
}

TCL_NORETURN void TkpExitProc(
    void *clientdata)
{
    Bool doCleanup = doCleanupFromExit;
    if (doCleanupFromExit) {
	doCleanupFromExit = NO; /* prevent possible recursive call. */
	closePanels();
    }

    /*
     * Tcl_Exit does not call Tcl_Finalize if there is an exit proc installed.
     */

    Tcl_Finalize();
    if (doCleanup == YES) {
	[(TKApplication *)NSApp superTerminate:nil]; /* Should not return. */
    }
    exit((long)clientdata); /* Convince the compiler that we don't return. */
}
#endif

/*
 * This signal handler is installed for the SIGINT, SIGHUP and SIGTERM signals
 * so that normal finalization occurs when a Tk app is killed by one of these
 * signals (e.g when ^C is pressed while running Wish in the shell).  It calls
 * Tcl_Exit instead of the C runtime exit function called by the default handler.
 * This is consistent with the Tcl_Exit manual page, which says that Tcl_Exit
 * should always be called instead of exit.  When Tk is killed by a signal we
 * return exit status 1.
 */

static void TkMacOSXSignalHandler(TCL_UNUSED(int)) {

    Tcl_Exit(1);
}

int
TkpInit(
    Tcl_Interp *interp)
{
    static int initialized = 0;

    /*
     * TkpInit can be called multiple times with different interpreters. But
     * The application initialization should only be done onece.
     */

    if (!initialized) {
	struct stat st;
	Bool shouldOpenConsole = NO;
        Bool stdinIsNullish = (!isatty(0) &&
	    (fstat(0, &st) || (S_ISCHR(st.st_mode) && st.st_blocks == 0)));

	/*
	 * Initialize/check OS version variable for runtime checks.
	 */

#if MAC_OS_X_VERSION_MIN_REQUIRED < 1060
#   error Mac OS X 10.6 required
#endif

	initialized = 1;

#ifdef TK_FRAMEWORK

	/*
	 * When Tk is in a framework, force tcl_findLibrary to look in the
	 * framework scripts directory.
	 * FIXME: Should we come up with a more generic way of doing this?
	 */

	if (Tcl_MacOSXOpenVersionedBundleResources(interp,
		"com.tcltk.tklibrary", TK_FRAMEWORK_VERSION, 0, PATH_MAX,
		tkLibPath) != TCL_OK) {
            # if 0 /* This is not really an error.  Wish still runs fine. */
	    TkMacOSXDbgMsg("Tcl_MacOSXOpenVersionedBundleResources failed");
	    # endif
	}
#endif

	/*
	 * Instantiate our NSApplication object. This needs to be done before
	 * we check whether to open a console window.
	 */

	NSAutoreleasePool *pool = [NSAutoreleasePool new];
	[[NSUserDefaults standardUserDefaults] registerDefaults:
		[NSDictionary dictionaryWithObjectsAndKeys:
				  [NSNumber numberWithBool:YES],
			      @"_NSCanWrapButtonTitles",
				   [NSNumber numberWithInt:-1],
			      @"NSStringDrawingTypesetterBehavior",
			      nil]];
	[TKApplication sharedApplication];
	[pool drain];

        /*
         * WARNING: The finishLaunching method runs asynchronously. This
         * creates a race between the initialization of the NSApplication and
         * the initialization of Tk.  If Tk wins the race bad things happen
         * with the root window (see below).  If the NSApplication wins then an
         * AppleEvent created during launch, e.g. by dropping a file icon on
         * the application icon, will be delivered before the procedure meant
         * to to handle the AppleEvent has been defined.  This is handled in
         * tkMacOSXHLEvents.c by scheduling a timer event to handle the
         * ApplEvent later, after the required procedure has been defined.
         */

	[NSApp _setup:interp];
	[NSApp finishLaunching];

        /*
         * Create a Tk event source based on the Appkit event queue.
         */

	Tk_MacOSXSetupTkNotifier();

	/*
	 * If Tk initialization wins the race, the root window is mapped before
         * the NSApplication is initialized.  This can cause bad things to
         * happen.  The root window can open off screen with no way to make it
         * appear on screen until the app icon is clicked.  This will happen if
         * a Tk application opens a modal window in its startup script (see
         * ticket 56a1823c73).  In other cases, an empty root window can open
         * on screen and remain visible for a noticeable amount of time while
         * the Tk initialization finishes (see ticket d1989fb7cf).  The call
         * below forces Tk to block until the Appkit event queue has been
         * created.  This seems to be sufficient to ensure that the
         * NSApplication initialization wins the race, avoiding these bad
         * window behaviors.
	 */

	Tcl_DoOneEvent(TCL_WINDOW_EVENTS | TCL_DONT_WAIT);

	/*
	 * Decide whether to open a console window.  If the TK_CONSOLE
	 * environment variable is not defined we only show the console if
	 * stdin is not a tty and there is no startup script.
	 */

	if (getenv("TK_CONSOLE")) {
	    shouldOpenConsole = YES;
	} else if (stdinIsNullish && Tcl_GetStartupScript(NULL) == NULL) {
	    const char *intvar = Tcl_GetVar2(interp, "tcl_interactive",
					     NULL, TCL_GLOBAL_ONLY);
	    if (intvar == NULL) {
		Tcl_SetVar2(interp, "tcl_interactive", NULL, "1",
			    TCL_GLOBAL_ONLY);
	    }

#if defined(USE_CUSTOM_EXIT_PROC)
	    doCleanupFromExit = YES;
#endif

	    shouldOpenConsole = YES;
	}
	if (shouldOpenConsole) {
	    Tk_InitConsoleChannels(interp);
	    Tcl_RegisterChannel(interp, Tcl_GetStdChannel(TCL_STDIN));
	    Tcl_RegisterChannel(interp, Tcl_GetStdChannel(TCL_STDOUT));
	    Tcl_RegisterChannel(interp, Tcl_GetStdChannel(TCL_STDERR));
	    if (Tk_CreateConsoleWindow(interp) == TCL_ERROR) {
		return TCL_ERROR;
	    }
	} else if (stdinIsNullish) {

	    /*
	     * When launched as a macOS application with no console,
	     * redirect stderr and stdout to /dev/null. This avoids waiting
	     * forever for those files to become writable if the underlying
	     * Tcl program tries to write to them with a puts command.
	     */

	    FILE *null = fopen("/dev/null", "w");
	    dup2(fileno(null), STDOUT_FILENO);
	    dup2(fileno(null), STDERR_FILENO);
#if defined(USE_CUSTOM_EXIT_PROC)
	    doCleanupFromExit = YES;
#endif
	}

	/*
	 * FIXME: Close stdin & stdout for remote debugging if XCNOSTDIN is
	 * set.  Otherwise we will fight with gdb for stdin & stdout
	 */

	if (getenv("XCNOSTDIN") != NULL) {
	    close(0);
	    close(1);
	}

	/*
	 * Initialize the NSServices object here. Apple's docs say to do this
	 * in applicationDidFinishLaunching, but the Tcl interpreter is not
	 * initialized until this function call.
	 */

	TkMacOSXServices_Init(interp);

	/*
	 * The root window has been created and mapped, but XMapWindow deferred its
	 * call to makeKeyAndOrderFront because the first call to XMapWindow
	 * occurs too early in the initialization process for that.  Process idle
	 * tasks now, so the root window is configured, then order it front.
	 */

	while(Tcl_DoOneEvent(TCL_IDLE_EVENTS)) {};
	for (NSWindow *window in [NSApp windows]) {
	    TkWindow *winPtr = TkMacOSXGetTkWindow(window);
	    if (winPtr && Tk_IsMapped(winPtr)) {
		[window makeKeyAndOrderFront:NSApp];
		break;
	    }
	}

# if defined(USE_CUSTOM_EXIT_PROC)

	if ((isatty(0) && isatty(1))) {
	    doCleanupFromExit = YES;
	}

# endif

	/*
	 * Install a signal handler for SIGINT, SIGHUP and SIGTERM which uses
	 * Tcl_Exit instead of exit so that normal cleanup takes place if a TK
	 * application is killed with one of these signals.
	 */

	signal(SIGINT, TkMacOSXSignalHandler);
	signal(SIGHUP, TkMacOSXSignalHandler);
	signal(SIGTERM, TkMacOSXSignalHandler);
    }

    /*
     * Initialization steps that are needed for all interpreters.
     */

    if (tkLibPath[0] != '\0') {
	Tcl_SetVar2(interp, "tk_library", NULL, tkLibPath, TCL_GLOBAL_ONLY);
    }

    if (scriptPath[0] != '\0') {
	Tcl_SetVar2(interp, "auto_path", NULL, scriptPath,
		TCL_GLOBAL_ONLY|TCL_LIST_ELEMENT|TCL_APPEND_VALUE);
    }

    Tcl_CreateObjCommand(interp, "::tk::mac::standardAboutPanel",
	    TkMacOSXStandardAboutPanelObjCmd, NULL, NULL);
    Tcl_CreateObjCommand(interp, "::tk::mac::iconBitmap",
	    TkMacOSXIconBitmapObjCmd, NULL, NULL);
    Tcl_CreateObjCommand(interp, "::tk::mac::GetAppPath",
	    TkMacOSXGetAppPathCmd, NULL, NULL);

    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TkpGetAppName --
 *
 *	Retrieves the name of the current application from a platform specific
 *	location. For Unix, the application name is the tail of the path
 *	contained in the tcl variable argv0.
 *
 * Results:
 *	Returns the application name in the given Tcl_DString.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

void
TkpGetAppName(
    Tcl_Interp *interp,
    Tcl_DString *namePtr)	/* A previously initialized Tcl_DString. */
{
    const char *p, *name;

    name = Tcl_GetVar2(interp, "argv0", NULL, TCL_GLOBAL_ONLY);
    if ((name == NULL) || (*name == 0)) {
	name = "tk";
    } else {
	p = strrchr(name, '/');
	if (p != NULL) {
	    name = p+1;
	}
    }
    Tcl_DStringAppend(namePtr, name, -1);
}

/*
 *----------------------------------------------------------------------
 *
 * TkMacOSXGetAppPathCmd --
 *
 *	Returns the path of the Wish application bundle.
 *
 * Results:
 *	Returns the application path.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static int
TkMacOSXGetAppPathCmd(
    TCL_UNUSED(void *),
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *const objv[])
{
    if (objc != 1) {
	Tcl_WrongNumArgs(interp, 1, objv, NULL);
	return TCL_ERROR;
    }

    /*
     * Get the application path URL and convert it to a string path reference.
     */

    CFURLRef mainBundleURL = CFBundleCopyBundleURL(CFBundleGetMainBundle());
    CFStringRef appPath =
	    CFURLCopyFileSystemPath(mainBundleURL, kCFURLPOSIXPathStyle);

    /*
     * Convert (and copy) the string reference into a Tcl result.
     */

    Tcl_SetObjResult(interp, Tcl_NewStringObj(
	    CFStringGetCStringPtr(appPath, CFStringGetSystemEncoding()), -1));

    CFRelease(mainBundleURL);
    CFRelease(appPath);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TkpDisplayWarning --
 *
 *	This routines is called from Tk_Main to display warning messages that
 *	occur during startup.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Generates messages on stdout.
 *
 *----------------------------------------------------------------------
 */

void
TkpDisplayWarning(
    const char *msg,		/* Message to be displayed. */
    const char *title)		/* Title of warning. */
{
    Tcl_Channel errChannel = Tcl_GetStdChannel(TCL_STDERR);

    if (errChannel) {
	Tcl_WriteChars(errChannel, title, -1);
	Tcl_WriteChars(errChannel, ": ", 2);
	Tcl_WriteChars(errChannel, msg, -1);
	Tcl_WriteChars(errChannel, "\n", 1);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TkMacOSXDefaultStartupScript --
 *
 *	On MacOS X, we look for a file in the Resources/Scripts directory
 *	called AppMain.tcl and if found, we set argv[1] to that, so that the
 *	rest of the code will find it, and add the Scripts folder to the
 *	auto_path. If we don't find the startup script, we just bag it,
 *	assuming the user is starting up some other way.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Tcl_SetStartupScript() called when AppMain.tcl found.
 *
 *----------------------------------------------------------------------
 */

MODULE_SCOPE void
TkMacOSXDefaultStartupScript(void)
{
    NSAutoreleasePool *pool = [NSAutoreleasePool new];
    CFBundleRef bundleRef = CFBundleGetMainBundle();

    if (bundleRef != NULL) {
	CFURLRef appMainURL = CFBundleCopyResourceURL(bundleRef,
		CFSTR("AppMain"), CFSTR("tcl"), CFSTR("Scripts"));

	if (appMainURL != NULL) {
	    CFURLRef scriptFldrURL;
	    char startupScript[PATH_MAX + 1];

	    if (CFURLGetFileSystemRepresentation(appMainURL, true,
		    (unsigned char *) startupScript, PATH_MAX)) {
		Tcl_SetStartupScript(Tcl_NewStringObj(startupScript,-1), NULL);
		scriptFldrURL = CFURLCreateCopyDeletingLastPathComponent(NULL,
			appMainURL);
		if (scriptFldrURL != NULL) {
		    CFURLGetFileSystemRepresentation(scriptFldrURL, true,
			    (unsigned char *) scriptPath, PATH_MAX);
		    CFRelease(scriptFldrURL);
		}
	    }
	    CFRelease(appMainURL);
	}
    }
    [pool drain];
}

/*
 *----------------------------------------------------------------------
 *
 * TkMacOSXGetNamedSymbol --
 *
 *	Dynamically acquire address of a named symbol from a loaded dynamic
 *	library, so that we can use API that may not be available on all OS
 *	versions.
 *
 * Results:
 *	Address of given symbol or NULL if unavailable.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

MODULE_SCOPE void*
TkMacOSXGetNamedSymbol(
    TCL_UNUSED(const char *),
    const char *symbol)
{
    void *addr = dlsym(RTLD_NEXT, symbol);

    if (!addr) {
	(void) dlerror(); /* Clear dlfcn error state */
    }
    return addr;
}

/*
 * Local Variables:
 * mode: objc
 * c-basic-offset: 4
 * fill-column: 79
 * coding: utf-8
 * End:
 */
