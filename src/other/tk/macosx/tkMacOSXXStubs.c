/*
 * tkMacOSXXStubs.c --
 *
 *	This file contains most of the X calls called by Tk. Many of these
 *	calls are just stubs and either don't make sense on the Macintosh or
 *	their implamentation just doesn't do anything. Other calls will
 *	eventually be moved into other files.
 *
 * Copyright (c) 1995-1997 Sun Microsystems, Inc.
 * Copyright 2001-2009, Apple Inc.
 * Copyright (c) 2005-2009 Daniel A. Steffen <das@users.sourceforge.net>
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#include "tkMacOSXPrivate.h"
#include "tkMacOSXEvent.h"

#include <IOKit/IOKitLib.h>
#include <IOKit/hidsystem/IOHIDShared.h>

/*
 * Because this file is still under major development Debugger statements are
 * used through out this file. The define TCL_DEBUG will decide whether the
 * debugger statements actually call the debugger or not.
 */

#ifndef TCL_DEBUG
#   define Debugger()
#endif

#define ROOT_ID 10

CGFloat tkMacOSXZeroScreenHeight = 0;
CGFloat tkMacOSXZeroScreenTop = 0;

/*
 * Declarations of static variables used in this file.
 */

static TkDisplay *gMacDisplay = NULL;
				/* Macintosh display. */
static const char *macScreenName = ":0";
				/* Default name of macintosh display. */

/*
 * Forward declarations of procedures used in this file.
 */

static XID		MacXIdAlloc(Display *display);
static int		DefaultErrorHandler(Display *display,
			    XErrorEvent *err_evt);

/*
 * Other declarations
 */

static int		DestroyImage(XImage *image);
static unsigned long	ImageGetPixel(XImage *image, int x, int y);
static int		ImagePutPixel(XImage *image, int x, int y,
			    unsigned long pixel);

/*
 *----------------------------------------------------------------------
 *
 * TkMacOSXDisplayChanged --
 *
 *	Called to set up initial screen info or when an event indicated
 *	display (screen) change.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	May change info regarding the screen.
 *
 *----------------------------------------------------------------------
 */

void
TkMacOSXDisplayChanged(
    Display *display)
{
    Screen *screen;
    NSArray *nsScreens;


    if (display == NULL || display->screens == NULL) {
	return;
    }
    screen = display->screens;

    nsScreens = [NSScreen screens];
    if (nsScreens && [nsScreens count]) {
	NSScreen *s = [nsScreens objectAtIndex:0];
	NSRect bounds = [s frame], visible = [s visibleFrame];
	NSRect maxBounds = NSZeroRect;

	tkMacOSXZeroScreenHeight = bounds.size.height;
	tkMacOSXZeroScreenTop = tkMacOSXZeroScreenHeight -
		(visible.origin.y + visible.size.height);

	screen->root_depth = NSBitsPerPixelFromDepth([s depth]);
	screen->width = bounds.size.width;
	screen->height = bounds.size.height;
	screen->mwidth = (bounds.size.width * 254 + 360) / 720;
	screen->mheight = (bounds.size.height * 254 + 360) / 720;

	for (s in nsScreens) {
	    maxBounds = NSUnionRect(maxBounds, [s visibleFrame]);
	}
	*((NSRect *)screen->ext_data) = maxBounds;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TkpOpenDisplay --
 *
 *	Create the Display structure and fill it with device specific
 *	information.
 *
 * Results:
 *	Returns a Display structure on success or NULL on failure.
 *
 * Side effects:
 *	Allocates a new Display structure.
 *
 *----------------------------------------------------------------------
 */

TkDisplay *
TkpOpenDisplay(
    const char *display_name)
{
    Display *display;
    Screen *screen;
    int fd = 0;
    static NSRect maxBounds = {{0, 0}, {0, 0}};
    static char vendor[25] = "";
    NSArray *cgVers;
    NSAutoreleasePool *pool;

    if (gMacDisplay != NULL) {
	if (strcmp(gMacDisplay->display->display_name, display_name) == 0) {
	    return gMacDisplay;
	} else {
	    return NULL;
	}
    }

    display = (Display *) ckalloc(sizeof(Display));
    screen  = (Screen *) ckalloc(sizeof(Screen));
    bzero(display, sizeof(Display));
    bzero(screen, sizeof(Screen));

    display->resource_alloc = MacXIdAlloc;
    display->request	    = 0;
    display->qlen	    = 0;
    display->fd		    = fd;
    display->screens	    = screen;
    display->nscreens	    = 1;
    display->default_screen = 0;
    display->display_name   = (char *) macScreenName;

    pool = [NSAutoreleasePool new];
    cgVers = [[[NSBundle bundleWithIdentifier:@"com.apple.CoreGraphics"]
	    objectForInfoDictionaryKey:@"CFBundleShortVersionString"]
	    componentsSeparatedByString:@"."];
    if ([cgVers count] >= 2) {
	display->proto_major_version = [[cgVers objectAtIndex:1] integerValue];
    }
    if ([cgVers count] >= 3) {
	display->proto_minor_version = [[cgVers objectAtIndex:2] integerValue];
    }
    if (!vendor[0]) {
	snprintf(vendor, sizeof(vendor), "Apple AppKit %s %g",
		([NSGarbageCollector defaultCollector] ? "GC" : "RR"),
		NSAppKitVersionNumber);
    }
    display->vendor = vendor;
    Gestalt(gestaltSystemVersion, (SInt32 *) &display->release);

    /*
     * These screen bits never change
     */
    screen->root	= ROOT_ID;
    screen->display	= display;
    screen->black_pixel = 0x00000000 | PIXEL_MAGIC << 24;
    screen->white_pixel = 0x00FFFFFF | PIXEL_MAGIC << 24;
    screen->ext_data	= (XExtData *) &maxBounds;

    screen->root_visual = (Visual *) ckalloc(sizeof(Visual));
    screen->root_visual->visualid     = 0;
    screen->root_visual->class	      = TrueColor;
    screen->root_visual->red_mask     = 0x00FF0000;
    screen->root_visual->green_mask   = 0x0000FF00;
    screen->root_visual->blue_mask    = 0x000000FF;
    screen->root_visual->bits_per_rgb = 24;
    screen->root_visual->map_entries  = 256;

    /*
     * Initialize screen bits that may change
     */

    TkMacOSXDisplayChanged(display);

    gMacDisplay = (TkDisplay *) ckalloc(sizeof(TkDisplay));

    /*
     * This is the quickest way to make sure that all the *Init flags get
     * properly initialized
     */

    bzero(gMacDisplay, sizeof(TkDisplay));
    gMacDisplay->display = display;
    [pool drain];
    return gMacDisplay;
}

/*
 *----------------------------------------------------------------------
 *
 * TkpCloseDisplay --
 *
 *	Deallocates a display structure created by TkpOpenDisplay.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Frees memory.
 *
 *----------------------------------------------------------------------
 */

void
TkpCloseDisplay(
    TkDisplay *displayPtr)
{
    Display *display = displayPtr->display;

    if (gMacDisplay != displayPtr) {
	Tcl_Panic("TkpCloseDisplay: tried to call TkpCloseDisplay on bad display");
    }

    gMacDisplay = NULL;
    if (display->screens != NULL) {
	if (display->screens->root_visual != NULL) {
	    ckfree((char *) display->screens->root_visual);
	}
	ckfree((char *) display->screens);
    }
    ckfree((char *) display);
}

/*
 *----------------------------------------------------------------------
 *
 * TkClipCleanup --
 *
 *	This procedure is called to cleanup resources associated with claiming
 *	clipboard ownership and for receiving selection get results. This
 *	function is called in tkWindow.c. This has to be called by the display
 *	cleanup function because we still need the access display elements.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Resources are freed - the clipboard may no longer be used.
 *
 *----------------------------------------------------------------------
 */

void
TkClipCleanup(
    TkDisplay *dispPtr)		/* display associated with clipboard */
{
    /*
     * Make sure that the local scrap is transfered to the global scrap if
     * needed.
     */

    [NSApp tkProvidePasteboard:dispPtr];

    if (dispPtr->clipWindow != NULL) {
	Tk_DeleteSelHandler(dispPtr->clipWindow, dispPtr->clipboardAtom,
		dispPtr->applicationAtom);
	Tk_DeleteSelHandler(dispPtr->clipWindow, dispPtr->clipboardAtom,
		dispPtr->windowAtom);

	Tk_DestroyWindow(dispPtr->clipWindow);
	Tcl_Release(dispPtr->clipWindow);
	dispPtr->clipWindow = NULL;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * MacXIdAlloc --
 *
 *	This procedure is invoked by Xlib as the resource allocator for a
 *	display.
 *
 * Results:
 *	The return value is an X resource identifier that isn't currently in
 *	use.
 *
 * Side effects:
 *	The identifier is removed from the stack of free identifiers, if it
 *	was previously on the stack.
 *
 *----------------------------------------------------------------------
 */

static XID
MacXIdAlloc(
    Display *display)		/* Display for which to allocate. */
{
    static long int cur_id = 100;
    /*
     * Some special XIds are reserved
     *   - this is why we start at 100
     */

    return ++cur_id;
}

/*
 *----------------------------------------------------------------------
 *
 * TkpWindowWasRecentlyDeleted --
 *
 *	Tries to determine whether the given window was recently deleted.
 *	Called from the generic code error handler to attempt to deal with
 *	async BadWindow errors under some circumstances.
 *
 * Results:
 *	Always 0, we do not keep this information on the Mac, so we do not
 *	know whether the window was destroyed.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
TkpWindowWasRecentlyDeleted(
    Window win,
    TkDisplay *dispPtr)
{
    return 0;
}

/*
 *----------------------------------------------------------------------
 *
 * DefaultErrorHandler --
 *
 *	This procedure is the default X error handler. Tk uses it's own error
 *	handler so this call should never be called.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	This function will call panic and exit.
 *
 *----------------------------------------------------------------------
 */

static int
DefaultErrorHandler(
    Display* display,
    XErrorEvent* err_evt)
{
    /*
     * This call should never be called. Tk replaces it with its own error
     * handler.
     */

    Tcl_Panic("Warning hit bogus error handler!");
    return 0;
}

char *
XGetAtomName(
    Display * display,
    Atom atom)
{
    display->request++;
    return NULL;
}

int
_XInitImageFuncPtrs(
    XImage *image)
{
    return 0;
}

XErrorHandler
XSetErrorHandler(
    XErrorHandler handler)
{
    return DefaultErrorHandler;
}

Window
XRootWindow(
    Display *display,
    int screen_number)
{
    display->request++;
    return ROOT_ID;
}

int
XGetGeometry(
    Display *display,
    Drawable d,
    Window *root_return,
    int *x_return,
    int *y_return,
    unsigned int *width_return,
    unsigned int *height_return,
    unsigned int *border_width_return,
    unsigned int *depth_return)
{
    TkWindow *winPtr = ((MacDrawable *) d)->winPtr;

    display->request++;
    *root_return = ROOT_ID;
    if (winPtr) {
	*x_return = Tk_X(winPtr);
	*y_return = Tk_Y(winPtr);
	*width_return = Tk_Width(winPtr);
	*height_return = Tk_Height(winPtr);
	*border_width_return = winPtr->changes.border_width;
	*depth_return = Tk_Depth(winPtr);
    } else {
	CGSize size = ((MacDrawable *) d)->size;
	*x_return = 0;
	*y_return =  0;
	*width_return = size.width;
	*height_return = size.height;
	*border_width_return = 0;
	*depth_return = 32;
    }
    return 1;
}

void
XChangeProperty(
    Display* display,
    Window w,
    Atom property,
    Atom type,
    int format,
    int mode,
    _Xconst unsigned char* data,
    int nelements)
{
    Debugger();
}

void
XSelectInput(
    Display* display,
    Window w,
    long event_mask)
{
    Debugger();
}

int
XBell(
    Display* display,
    int percent)
{
    NSBeep();
    return Success;
}

#if 0
void
XSetWMNormalHints(
    Display* display,
    Window w,
    XSizeHints* hints)
{
    /*
     * Do nothing. Shouldn't even be called.
     */
}

XSizeHints *
XAllocSizeHints(void)
{
    /*
     * Always return NULL. Tk code checks to see if NULL is returned & does
     * nothing if it is.
     */

    return NULL;
}
#endif

GContext
XGContextFromGC(
    GC gc)
{
    /*
     * TODO: currently a no-op
     */

    return 0;
}

Status
XSendEvent(
    Display* display,
    Window w,
    Bool propagate,
    long event_mask,
    XEvent* event_send)
{
    Debugger();
    return 0;
}

void
XClearWindow(
    Display* display,
    Window w)
{
}

/*
void
XDrawPoint(
    Display* display,
    Drawable d,
    GC gc,
    int x,
    int y)
{
}

void
XDrawPoints(
    Display* display,
    Drawable d,
    GC gc,
    XPoint* points,
    int npoints,
    int mode)
{
}
*/

int
XWarpPointer(
    Display* display,
    Window src_w,
    Window dest_w,
    int src_x,
    int src_y,
    unsigned int src_width,
    unsigned int src_height,
    int dest_x,
    int dest_y)
{
    return Success;
}

void
XQueryColor(
    Display* display,
    Colormap colormap,
    XColor* def_in_out)
{
    unsigned long p;
    unsigned char r, g, b;
    XColor *d = def_in_out;

    p		= d->pixel;
    r		= (p & 0x00FF0000) >> 16;
    g		= (p & 0x0000FF00) >> 8;
    b		= (p & 0x000000FF);
    d->red	= (r << 8) | r;
    d->green	= (g << 8) | g;
    d->blue	= (b << 8) | b;
    d->flags	= DoRed|DoGreen|DoBlue;
    d->pad	= 0;
}

void
XQueryColors(
    Display* display,
    Colormap colormap,
    XColor* defs_in_out,
    int ncolors)
{
    int i;
    unsigned long p;
    unsigned char r, g, b;
    XColor *d = defs_in_out;

    for (i = 0; i < ncolors; i++, d++) {
	p		= d->pixel;
	r		= (p & 0x00FF0000) >> 16;
	g		= (p & 0x0000FF00) >> 8;
	b		= (p & 0x000000FF);
	d->red		= (r << 8) | r;
	d->green	= (g << 8) | g;
	d->blue		= (b << 8) | b;
	d->flags	= DoRed|DoGreen|DoBlue;
	d->pad		= 0;
    }
}

int
XQueryTree(display, w, root_return, parent_return, children_return,
	nchildren_return)
    Display* display;
    Window w;
    Window* root_return;
    Window* parent_return;
    Window** children_return;
    unsigned int* nchildren_return;
{
    return 0;
}


int
XGetWindowProperty(
    Display *display,
    Window w,
    Atom property,
    long long_offset,
    long long_length,
    Bool delete,
    Atom req_type,
    Atom *actual_type_return,
    int *actual_format_return,
    unsigned long *nitems_return,
    unsigned long *bytes_after_return,
    unsigned char ** prop_return)
{
    display->request++;
    *actual_type_return = None;
    *actual_format_return = *bytes_after_return = 0;
    *nitems_return = 0;
    return 0;
}

void
XRefreshKeyboardMapping(
    XMappingEvent *x)
{
    /* used by tkXEvent.c */
    Debugger();
}

void
XSetIconName(
    Display* display,
    Window w,
    const char *icon_name)
{
    /*
     * This is a no-op, no icon name for Macs.
     */
    display->request++;
}

void
XForceScreenSaver(
    Display* display,
    int mode)
{
    /*
     * This function is just a no-op. It is defined to reset the screen saver.
     * However, there is no real way to do this on a Mac. Let me know if there
     * is!
     */

    display->request++;
}

void
Tk_FreeXId(
    Display *display,
    XID xid)
{
    /* no-op function needed for stubs implementation. */
}

int
XSync(
    Display *display,
    Bool flag)
{
    TkMacOSXFlushWindows();
    display->request++;
    return 0;
}

#if 0
int
XSetClipRectangles(
    Display *d,
    GC gc,
    int clip_x_origin,
    int clip_y_origin,
    XRectangle* rectangles,
    int n,
    int ordering)
{
    TkRegion clipRgn = TkCreateRegion();

    while (n--) {
	XRectangle rect = *rectangles;

	rect.x += clip_x_origin;
	rect.y += clip_y_origin;
	TkUnionRectWithRegion(&rect, clipRgn, clipRgn);
	rectangles++;
    }
    TkSetRegion(d, gc, clipRgn);
    TkDestroyRegion(clipRgn);
    return 1;
}
#endif

/*
 *----------------------------------------------------------------------
 *
 * TkGetServerInfo --
 *
 *	Given a window, this procedure returns information about the window
 *	server for that window. This procedure provides the guts of the "winfo
 *	server" command.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

void
TkGetServerInfo(
    Tcl_Interp *interp,		/* The server information is returned in this
				 * interpreter's result. */
    Tk_Window tkwin)		/* Token for window; this selects a particular
				 * display and server. */
{
    char buffer[5 + TCL_INTEGER_SPACE * 2];
    char buffer2[11 + TCL_INTEGER_SPACE];

    snprintf(buffer, sizeof(buffer), "CG%d.%d ",
	    ProtocolVersion(Tk_Display(tkwin)),
	    ProtocolRevision(Tk_Display(tkwin)));
    snprintf(buffer2, sizeof(buffer2), " Mac OS X %x",
	    VendorRelease(Tk_Display(tkwin)));
    Tcl_AppendResult(interp, buffer, ServerVendor(Tk_Display(tkwin)),
	    buffer2, NULL);
}

#pragma mark XImage handling

/*
 *----------------------------------------------------------------------
 *
 * XCreateImage --
 *
 *	Allocates storage for a new XImage.
 *
 * Results:
 *	Returns a newly allocated XImage.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

XImage *
XCreateImage(
    Display* display,
    Visual* visual,
    unsigned int depth,
    int format,
    int offset,
    char* data,
    unsigned int width,
    unsigned int height,
    int bitmap_pad,
    int bytes_per_line)
{
    XImage *ximage;

    display->request++;
    ximage = (XImage *) ckalloc(sizeof(XImage));

    ximage->height = height;
    ximage->width = width;
    ximage->depth = depth;
    ximage->xoffset = offset;
    ximage->format = format;
    ximage->data = data;

    if (format == ZPixmap) {
	ximage->bits_per_pixel = 32;
	ximage->bitmap_unit = 32;
    } else {
	ximage->bits_per_pixel = 1;
	ximage->bitmap_unit = 8;
    }
    if (bitmap_pad) {
	ximage->bitmap_pad = bitmap_pad;
    } else {
	/* Use 16 byte alignment for best Quartz perfomance */
	ximage->bitmap_pad = 128;
    }
    if (bytes_per_line) {
	ximage->bytes_per_line = bytes_per_line;
    } else {
	ximage->bytes_per_line = ((width * ximage->bits_per_pixel +
		(ximage->bitmap_pad - 1)) >> 3) &
		~((ximage->bitmap_pad >> 3) - 1);
    }
#ifdef WORDS_BIGENDIAN
    ximage->byte_order = MSBFirst;
    ximage->bitmap_bit_order = MSBFirst;
#else
    ximage->byte_order = LSBFirst;
    ximage->bitmap_bit_order = LSBFirst;
#endif
    ximage->red_mask = 0x00FF0000;
    ximage->green_mask = 0x0000FF00;
    ximage->blue_mask = 0x000000FF;
    ximage->obdata = NULL;
    ximage->f.create_image = NULL;
    ximage->f.destroy_image = DestroyImage;
    ximage->f.get_pixel = ImageGetPixel;
    ximage->f.put_pixel = ImagePutPixel;
    ximage->f.sub_image = NULL;
    ximage->f.add_pixel = NULL;

    return ximage;
}

/*
 *----------------------------------------------------------------------
 *
 * XGetImage --
 *
 *	This function copies data from a pixmap or window into an XImage.
 *
 * Results:
 *	Returns a newly allocated image containing the data from the given
 *	rectangle of the given drawable.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

XImage *
XGetImage(
    Display *display,
    Drawable d,
    int x,
    int y,
    unsigned int width,
    unsigned int height,
    unsigned long plane_mask,
    int format)
{
    MacDrawable *macDraw = (MacDrawable *) d;
    XImage *   imagePtr = NULL;
    Pixmap     pixmap = (Pixmap) NULL;
    Tk_Window  win = (Tk_Window) macDraw->winPtr;
    GC	       gc;
    char *     data = NULL;
    int	       depth = 32;
    int	       offset = 0;
    int	       bitmap_pad = 0;
    int	       bytes_per_line = 0;

    if (format == ZPixmap) {
	if (width > 0 && height > 0) {
	    /*
	     * Tk_GetPixmap fails for zero width or height.
	     */

	    pixmap = Tk_GetPixmap(display, d, width, height, depth);
	}
	if (win) {
	    XGCValues values;

	    gc = Tk_GetGC(win, 0, &values);
	} else {
	    gc = XCreateGC(display, pixmap, 0, NULL);
	}
	if (pixmap) {
	    CGContextRef context;

	    XCopyArea(display, d, pixmap, gc, x, y, width, height, 0, 0);
	    context = ((MacDrawable *) pixmap)->context;
	    if (context) {
		data = CGBitmapContextGetData(context);
		bytes_per_line = CGBitmapContextGetBytesPerRow(context);
	    }
	}
	if (data) {
	    imagePtr = XCreateImage(display, NULL, depth, format, offset,
		    data, width, height, bitmap_pad, bytes_per_line);

	    /*
	     * Track Pixmap underlying the XImage in the unused obdata field
	     * so that we can treat XImages coming from XGetImage specially.
	     */

	    imagePtr->obdata = (XPointer) pixmap;
	} else if (pixmap) {
	    Tk_FreePixmap(display, pixmap);
	}
	if (!win) {
	    XFreeGC(display, gc);
	}
    } else {
	TkMacOSXDbgMsg("Invalid image format");
    }
    return imagePtr;
}

/*
 *----------------------------------------------------------------------
 *
 * DestroyImage --
 *
 *	Destroys storage associated with an image.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Deallocates the image.
 *
 *----------------------------------------------------------------------
 */

static int
DestroyImage(
    XImage *image)
{
    if (image) {
	if (image->obdata) {
	    Tk_FreePixmap((Display*) gMacDisplay, (Pixmap) image->obdata);
	} else if (image->data) {
	    ckfree(image->data);
	}
	ckfree((char*) image);
    }
    return 0;
}

/*
 *----------------------------------------------------------------------
 *
 * ImageGetPixel --
 *
 *	Get a single pixel from an image.
 *
 * Results:
 *	Returns the 32 bit pixel value.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static unsigned long
ImageGetPixel(
    XImage *image,
    int x,
    int y)
{
    unsigned char r = 0, g = 0, b = 0;

    if (image && image->data) {
	unsigned char *srcPtr = ((unsigned char*) image->data)
		+ (y * image->bytes_per_line)
		+ (((image->xoffset + x) * image->bits_per_pixel) / NBBY);

	switch (image->bits_per_pixel) {
	    case 32: {
		r = (*((unsigned int*) srcPtr) >> 16) & 0xff;
		g = (*((unsigned int*) srcPtr) >>  8) & 0xff;
		b = (*((unsigned int*) srcPtr)      ) & 0xff;
		/*if (image->byte_order == LSBFirst) {
		    r = srcPtr[2]; g = srcPtr[1]; b = srcPtr[0];
		} else {
		    r = srcPtr[1]; g = srcPtr[2]; b = srcPtr[3];
		}*/
		break;
	    }
	    case 16:
		r = (*((unsigned short*) srcPtr) >> 7) & 0xf8;
		g = (*((unsigned short*) srcPtr) >> 2) & 0xf8;
		b = (*((unsigned short*) srcPtr) << 3) & 0xf8;
		break;
	    case 8:
		r = (*srcPtr << 2) & 0xc0;
		g = (*srcPtr << 4) & 0xc0;
		b = (*srcPtr << 6) & 0xc0;
		r |= r >> 2 | r >> 4 | r >> 6;
		g |= g >> 2 | g >> 4 | g >> 6;
		b |= b >> 2 | b >> 4 | b >> 6;
		break;
	    case 4: {
		unsigned char c = (x % 2) ? *srcPtr : (*srcPtr >> 4);
		r = (c & 0x04) ? 0xff : 0;
		g = (c & 0x02) ? 0xff : 0;
		b = (c & 0x01) ? 0xff : 0;
		break;
		}
	    case 1:
		r = g = b = ((*srcPtr) & (0x80 >> (x % 8))) ? 0xff : 0;
		break;
	}
    }
    return (PIXEL_MAGIC << 24) | (r << 16) | (g << 8) | b;
}

/*
 *----------------------------------------------------------------------
 *
 * ImagePutPixel --
 *
 *	Set a single pixel in an image.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static int
ImagePutPixel(
    XImage *image,
    int x,
    int y,
    unsigned long pixel)
{
    if (image && image->data) {
	unsigned char r = ((pixel & image->red_mask)   >> 16) & 0xff;
	unsigned char g = ((pixel & image->green_mask) >>  8) & 0xff;
	unsigned char b = ((pixel & image->blue_mask)       ) & 0xff;
	unsigned char *dstPtr = ((unsigned char*) image->data)
		+ (y * image->bytes_per_line)
		+ (((image->xoffset + x) * image->bits_per_pixel) / NBBY);

	switch (image->bits_per_pixel) {
	    case 32:
		*((unsigned int*) dstPtr) = (0xff << 24) | (r << 16) |
			(g << 8) | b;
		/*if (image->byte_order == LSBFirst) {
		    dstPtr[3] = 0xff; dstPtr[2] = r; dstPtr[1] = g; dstPtr[0] = b;
		} else {
		    dstPtr[0] = 0xff; dstPtr[1] = r; dstPtr[2] = g; dstPtr[3] = b;
		}*/
		break;
	    case 16:
		*((unsigned short*) dstPtr) = ((r & 0xf8) << 7) |
			((g & 0xf8) << 2) | ((b & 0xf8) >> 3);
		break;
	    case 8:
		*dstPtr = ((r & 0xc0) >> 2) | ((g & 0xc0) >> 4) |
			((b & 0xc0) >> 6);
		break;
	    case 4: {
		unsigned char c = ((r & 0x80) >> 5) | ((g & 0x80) >> 6) |
			((b & 0x80) >> 7);
		*dstPtr = (x % 2) ? ((*dstPtr & 0xf0) | (c & 0x0f)) :
			((*dstPtr & 0x0f) | ((c << 4) & 0xf0));
		break;
		}
	    case 1:
		*dstPtr = ((r|g|b) & 0x80) ? (*dstPtr | (0x80 >> (x % 8))) :
			(*dstPtr & ~(0x80 >> (x % 8)));
		break;
	}
    }
    return 0;
}

/*
 *----------------------------------------------------------------------
 *
 * XChangeWindowAttributes, XSetWindowBackground,
 * XSetWindowBackgroundPixmap, XSetWindowBorder, XSetWindowBorderPixmap,
 * XSetWindowBorderWidth, XSetWindowColormap
 *
 *	These functions are all no-ops. They all have equivalent Tk calls that
 *	should always be used instead.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

void
XChangeWindowAttributes(
    Display *display,
    Window w,
    unsigned long value_mask,
    XSetWindowAttributes *attributes)
{
}

void
XSetWindowBackground(
    Display *display,
    Window window,
    unsigned long value)
{
}

void
XSetWindowBackgroundPixmap(
    Display *display,
    Window w,
    Pixmap background_pixmap)
{
}

void
XSetWindowBorder(
    Display *display,
    Window w,
    unsigned long border_pixel)
{
}

void
XSetWindowBorderPixmap(
    Display *display,
    Window w,
    Pixmap border_pixmap)
{
}

void
XSetWindowBorderWidth(
    Display *display,
    Window w,
    unsigned int width)
{
}

void
XSetWindowColormap(
    Display *display,
    Window w,
    Colormap colormap)
{
    Debugger();
}

Status
XStringListToTextProperty(
    char **list,
    int count,
    XTextProperty *text_prop_return)
{
    Debugger();
    return (Status) 0;
}

void
XSetWMClientMachine(
    Display *display,
    Window w,
    XTextProperty *text_prop)
{
    Debugger();
}

XIC
XCreateIC(void)
{
    Debugger();
    return (XIC) 0;
}

/*
 *----------------------------------------------------------------------
 *
 * TkGetDefaultScreenName --
 *
 *	Returns the name of the screen that Tk should use during
 *	initialization.
 *
 * Results:
 *	Returns a statically allocated string.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

const char *
TkGetDefaultScreenName(
    Tcl_Interp *interp,		/* Not used. */
    const char *screenName)		/* If NULL, use default string. */
{
#if 0
    if ((screenName == NULL) || (screenName[0] == '\0')) {
	screenName = macScreenName;
    }
    return screenName;
#endif
    return macScreenName;
}

/*
 *----------------------------------------------------------------------
 *
 * Tk_GetUserInactiveTime --
 *
 *	Return the number of milliseconds the user was inactive.
 *
 * Results:
 *	The number of milliseconds the user has been inactive, or -1 if
 *	querying the inactive time is not supported.
 *
 * Side effects:
 *	None.
 *----------------------------------------------------------------------
 */

long
Tk_GetUserInactiveTime(
    Display *dpy)
{
    io_registry_entry_t regEntry;
    CFMutableDictionaryRef props = NULL;
    CFTypeRef timeObj;
    long ret = -1l;
    uint64_t time;
    IOReturn result;

    regEntry = IOServiceGetMatchingService(kIOMasterPortDefault,
	    IOServiceMatching("IOHIDSystem"));

    if (regEntry == 0) {
	return -1l;
    }

    result = IORegistryEntryCreateCFProperties(regEntry, &props,
	    kCFAllocatorDefault, 0);
    IOObjectRelease(regEntry);

    if (result != KERN_SUCCESS || props == NULL) {
	return -1l;
    }

    timeObj = CFDictionaryGetValue(props, CFSTR("HIDIdleTime"));

    if (timeObj) {
	CFTypeID type = CFGetTypeID(timeObj);

	if (type == CFDataGetTypeID()) { /* Jaguar */
	    CFDataGetBytes((CFDataRef) timeObj,
		    CFRangeMake(0, sizeof(time)), (UInt8 *) &time);
	    /* Convert nanoseconds to milliseconds. */
	    /* ret /= kMillisecondScale; */
	    ret = (long) (time/kMillisecondScale);
	} else if (type == CFNumberGetTypeID()) { /* Panther+ */
	    CFNumberGetValue((CFNumberRef)timeObj,
		    kCFNumberSInt64Type, &time);
	    /* Convert nanoseconds to milliseconds. */
	    /* ret /= kMillisecondScale; */
	    ret = (long) (time/kMillisecondScale);
	} else {
	    ret = -1l;
	}
    }
    /* Cleanup */
    CFRelease(props);

    return ret;
}

/*
 *----------------------------------------------------------------------
 *
 * Tk_ResetUserInactiveTime --
 *
 *	Reset the user inactivity timer
 *
 * Results:
 *	none
 *
 * Side effects:
 *	The user inactivity timer of the underlaying windowing system is reset
 *	to zero.
 *
 *----------------------------------------------------------------------
 */

void
Tk_ResetUserInactiveTime(
    Display *dpy)
{
    IOGPoint loc;
    kern_return_t kr;
    NXEvent nullEvent = {NX_NULLEVENT, {0, 0}, 0, -1, 0};
    enum { kNULLEventPostThrottle = 10 };
    static io_connect_t io_connection = MACH_PORT_NULL;

    if (io_connection == MACH_PORT_NULL) {
	io_service_t service = IOServiceGetMatchingService(
		kIOMasterPortDefault, IOServiceMatching(kIOHIDSystemClass));

	if (service == MACH_PORT_NULL) {
	    return;
	}
	kr = IOServiceOpen(service, mach_task_self(), kIOHIDParamConnectType,
		&io_connection);
	IOObjectRelease(service);
	if (kr != KERN_SUCCESS) {
	    return;
	}
    }
    kr = IOHIDPostEvent(io_connection, NX_NULLEVENT, loc, &nullEvent.data,
	    FALSE, 0, FALSE);
}

/*
 * Local Variables:
 * mode: objc
 * c-basic-offset: 4
 * fill-column: 79
 * coding: utf-8
 * End:
 */
