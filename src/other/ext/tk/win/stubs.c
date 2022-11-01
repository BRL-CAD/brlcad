#include "tkInt.h"

/*
 * Undocumented Xlib internal function
 */

int
_XInitImageFuncPtrs(
    XImage *image)
{
    (void)image;

    return Success;
}

/*
 * From Xutil.h
 */

void
XSetWMClientMachine(
    Display *display,
    Window w,
    XTextProperty *text_prop)
{
	(void)display;
	(void)w;
	(void)text_prop;
}

Status
XStringListToTextProperty(
    char **list,
    int count,
    XTextProperty *text_prop_return)
{
    (void)list;
    (void)count;
    (void)text_prop_return;

    return Success;
}

/*
 * From Xlib.h
 */

int
XChangeProperty(
    Display *display,
    Window w,
    Atom property,
    Atom type,
    int format,
    int mode,
    _Xconst unsigned char *data,
    int nelements)
{
    (void)display;
    (void)w;
    (void)property;
    (void)type;
    (void)format;
    (void)mode;
    (void)data;
    (void)nelements;

    return Success;
}

XIC
XCreateIC(XIM xim, ...)
{
    (void)xim;
    return NULL;
}

int
XDeleteProperty(
    Display *display,
    Window w,
    Atom property)
{
    (void)display;
    (void)w;
    (void)property;

    return Success;
}

Bool
XFilterEvent(
    XEvent *event,
    Window window)
{
    (void)event;
    (void)window;

    return 0;
}

int
XForceScreenSaver(
    Display *display,
    int mode)
{
    (void)display;
    (void)mode;

    return Success;
}

int
XFreeCursor(
    Display *display,
    Cursor cursor)
{
    (void)display;
    (void)cursor;

    return Success;
}

GContext
XGContextFromGC(
    GC gc)
{
    (void)gc;

    return (GContext) NULL;
}

char *
XGetAtomName(
    Display *display,
    Atom atom)
{
    (void)display;
    (void)atom;

    return NULL;
}

int
XGetWindowAttributes(
    Display *display,
    Window w,
    XWindowAttributes *window_attributes_return)
{
    (void)display;
    (void)w;
    (void)window_attributes_return;

    return Success;
}

Status
XGetWMColormapWindows(
    Display *display,
    Window w,
    Window **windows_return,
    int *count_return)
{
    (void)display;
    (void)w;
    (void)windows_return;
    (void)count_return;

    return Success;
}

int
XIconifyWindow(
    Display *display,
    Window w,
    int screen_number)
{
    (void)display;
    (void)w;
    (void)screen_number;

    return Success;
}

XHostAddress *
XListHosts(
    Display *display,
    int *nhosts_return,
    Bool *state_return)
{
    (void)display;
    (void)nhosts_return;
    (void)state_return;

    return NULL;
}

int
XLookupColor(
    Display *display,
    Colormap colormap,
    _Xconst char *color_name,
    XColor *exact_def_return,
    XColor *screen_def_return)
{
    (void)display;
    (void)colormap;
    (void)color_name;
    (void)exact_def_return;
    (void)screen_def_return;

    return Success;
}

int
XNextEvent(
    Display *display,
    XEvent *event_return)
{
    (void)display;
    (void)event_return;

    return Success;
}

int
XPutBackEvent(
    Display *display,
    XEvent *event)
{
    (void)display;
    (void)event;

    return Success;
}

int
XQueryColors(
    Display *display,
    Colormap colormap,
    XColor *defs_in_out,
    int ncolors)
{
    (void)display;
    (void)colormap;
    (void)defs_in_out;
    (void)ncolors;

    return Success;
}

int
XQueryTree(
    Display *display,
    Window w,
    Window *root_return,
    Window *parent_return,
    Window **children_return,
    unsigned int *nchildren_return)
{
    (void)display;
    (void)w;
    (void)root_return;
    (void)parent_return;
    (void)children_return;
    (void)nchildren_return;

    return Success;
}

int
XRefreshKeyboardMapping(
    XMappingEvent *event_map)
{
    (void)event_map;

    return Success;
}

Window
XRootWindow(
    Display *display,
    int screen_number)
{
    (void)display;
    (void)screen_number;

    return (Window) NULL;
}

int
XSelectInput(
    Display *display,
    Window w,
    long event_mask)
{
    (void)display;
    (void)w;
    (void)event_mask;

    return Success;
}

int
XSendEvent(
    Display *display,
    Window w,
    Bool propagate,
    long event_mask,
    XEvent *event_send)
{
    (void)display;
    (void)w;
    (void)propagate;
    (void)event_mask;
    (void)event_send;

    return Success;
}

int
XSetCommand(
    Display *display,
    Window w,
    char **argv,
    int argc)
{
    (void)display;
    (void)w;
    (void)argv;
    (void)argc;

    return Success;
}

XErrorHandler
XSetErrorHandler(
    XErrorHandler handler)
{
    (void)handler;

    return NULL;
}

int
XSetIconName(
    Display *display,
    Window w,
    _Xconst char *icon_name)
{
    (void)display;
    (void)w;
    (void)icon_name;

    return Success;
}

int
XSetWindowBackground(
    Display *display,
    Window w,
    unsigned long background_pixel)
{
    (void)display;
    (void)w;
    (void)background_pixel;

    return Success;
}

int
XSetWindowBackgroundPixmap(
    Display *display,
    Window w,
    Pixmap background_pixmap)
{
    (void)display;
    (void)w;
    (void)background_pixmap;

    return Success;
}

int
XSetWindowBorder(
    Display *display,
    Window w,
    unsigned long border_pixel)
{
    (void)display;
    (void)w;
    (void)border_pixel;

    return Success;
}

int
XSetWindowBorderPixmap(
    Display *display,
    Window w,
    Pixmap border_pixmap)
{
    (void)display;
    (void)w;
    (void)border_pixmap;

    return Success;
}

int
XSetWindowBorderWidth(
    Display *display,
    Window w,
    unsigned int width)
{
    (void)display;
    (void)w;
    (void)width;

    return Success;
}

int
XSetWindowColormap(
    Display *display,
    Window w,
    Colormap colormap)
{
    (void)display;
    (void)w;
    (void)colormap;

    return Success;
}

Bool
XTranslateCoordinates(
    Display *display,
    Window src_w,
    Window dest_w,
    int src_x,
    int src_y,
    int *dest_x_return,
    int *dest_y_return,
    Window *child_return)
{
    (void)display;
    (void)src_w;
    (void)dest_w;
    (void)src_x;
    (void)src_y;
    (void)dest_x_return;
    (void)dest_y_return;
    (void)child_return;

    return 0;
}

int
XWindowEvent(
    Display *display,
    Window w,
    long event_mask,
    XEvent *event_return)
{
    (void)display;
    (void)w;
    (void)event_mask;
    (void)event_return;

    return Success;
}

int
XWithdrawWindow(
    Display *display,
    Window w,
    int screen_number)
{
    (void)display;
    (void)w;
    (void)screen_number;

    return Success;
}

int
XmbLookupString(
    XIC ic,
    XKeyPressedEvent *event,
    char *buffer_return,
    int bytes_buffer,
    KeySym *keysym_return,
    Status *status_return)
{
    (void)ic;
    (void)event;
    (void)buffer_return;
    (void)bytes_buffer;
    (void)keysym_return;
    (void)status_return;

    return Success;
}

int
XGetWindowProperty(
    Display *display,
    Window w,
    Atom property,
    long long_offset,
    long long_length,
    Bool del,
    Atom req_type,
    Atom *actual_type_return,
    int *actual_format_return,
    unsigned long *nitems_return,
    unsigned long *bytes_after_return,
    unsigned char **prop_return)
{
    (void)display;
    (void)w;
    (void)property;
    (void)long_offset;
    (void)long_length;
    (void)del;
    (void)req_type;

    *actual_type_return = None;
    *actual_format_return = 0;
    *nitems_return = 0;
    *bytes_after_return = 0;
    *prop_return = NULL;
    return BadValue;
}

/*
 * The following functions were implemented as macros under Windows.
 */

int
XFlush(
    Display *display)
{
    (void)display;

    return 0;
}

int
XGrabServer(
    Display *display)
{
    (void)display;

    return 0;
}

int
XUngrabServer(
    Display *display)
{
    (void)display;

    return 0;
}

int
XFree(
    void *data)
{
	if ((data) != NULL) {
		ckfree(data);
	}
    return 0;
}

int
XNoOp(
    Display *display)
{
    display->request++;
    return 0;
}

XAfterFunction
XSynchronize(
    Display *display,
    Bool onoff)
{
    (void)onoff;

    display->request++;
    return NULL;
}

int
XSync(
    Display *display,
    Bool discard)
{
    (void)discard;

    display->request++;
    return 0;
}

VisualID
XVisualIDFromVisual(
    Visual *visual)
{
    return visual->visualid;
}

int
XOffsetRegion(
    Region rgn,
	int dx,
	int dy)
{
    (void)rgn;
    (void)dx;
    (void)dy;

	return 0;
}
