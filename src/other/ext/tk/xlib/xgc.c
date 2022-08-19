/*
 * xgc.c --
 *
 *	This file contains generic routines for manipulating X graphics
 *	contexts.
 *
 * Copyright (c) 1995-1996 Sun Microsystems, Inc.
 * Copyright (c) 2002-2009 Daniel A. Steffen <das@users.sourceforge.net>
 * Copyright 2008-2009, Apple Inc.
 *
 * See the file "license.terms" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#include "tkInt.h"
#include <X11/Xlib.h>
#if defined(MAC_OSX_TK)
#   define Cursor XCursor
#   define Region XRegion
#endif

#undef TkSetRegion

#define MAX_DASH_LIST_SIZE 10
typedef struct {
    XGCValues gc;
    char dash[MAX_DASH_LIST_SIZE];
} XGCValuesWithDash;

/*
 *----------------------------------------------------------------------
 *
 * AllocClipMask --
 *
 *	Static helper proc to allocate new or clear existing TkpClipMask.
 *
 * Results:
 *	Returns ptr to the new/cleared TkpClipMask.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static TkpClipMask *AllocClipMask(GC gc) {
    TkpClipMask *clip_mask = (TkpClipMask*) gc->clip_mask;

    if (clip_mask == NULL) {
	clip_mask = (TkpClipMask *)ckalloc(sizeof(TkpClipMask));
	gc->clip_mask = (Pixmap) clip_mask;
    }
    return clip_mask;
}

/*
 *----------------------------------------------------------------------
 *
 * FreeClipMask --
 *
 *	Static helper proc to free TkpClipMask.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static void FreeClipMask(GC gc) {
    if (gc->clip_mask != None) {
	ckfree((char *)gc->clip_mask);
	gc->clip_mask = None;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * XCreateGC --
 *
 *	Allocate a new GC, and initialize the specified fields.
 *
 * Results:
 *	Returns a newly allocated GC.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

GC
XCreateGC(
    Display *display,
    Drawable d,
    unsigned long mask,
    XGCValues *values)
{
    GC gp;
    (void)d;

    /*
     * In order to have room for a dash list, MAX_DASH_LIST_SIZE extra chars
     * are defined, which is invisible from the outside. The list is assumed
     * to end with a 0-char, so this must be set explicitly during
     * initialization.
     */

    gp = (GC)ckalloc(sizeof(XGCValuesWithDash));
    if (!gp) {
	return NULL;
    }

#define InitField(name,maskbit,default) \
	(gp->name = (mask & (maskbit)) ? values->name : (default))

    InitField(function,		  GCFunction,		GXcopy);
    InitField(plane_mask,	  GCPlaneMask,		(unsigned long)(~0));
    InitField(foreground,	  GCForeground,
	    BlackPixelOfScreen(DefaultScreenOfDisplay(display)));
    InitField(background,	  GCBackground,
	    WhitePixelOfScreen(DefaultScreenOfDisplay(display)));
    InitField(line_width,	  GCLineWidth,		1);
    InitField(line_style,	  GCLineStyle,		LineSolid);
    InitField(cap_style,	  GCCapStyle,		0);
    InitField(join_style,	  GCJoinStyle,		0);
    InitField(fill_style,	  GCFillStyle,		FillSolid);
    InitField(fill_rule,	  GCFillRule,		WindingRule);
    InitField(arc_mode,		  GCArcMode,		ArcPieSlice);
    InitField(tile,		  GCTile,		0);
    InitField(stipple,		  GCStipple,		0);
    InitField(ts_x_origin,	  GCTileStipXOrigin,	0);
    InitField(ts_y_origin,	  GCTileStipYOrigin,	0);
    InitField(font,		  GCFont,		0);
    InitField(subwindow_mode,	  GCSubwindowMode,	ClipByChildren);
    InitField(graphics_exposures, GCGraphicsExposures,	True);
    InitField(clip_x_origin,	  GCClipXOrigin,	0);
    InitField(clip_y_origin,	  GCClipYOrigin,	0);
    InitField(dash_offset,	  GCDashOffset,		0);
    InitField(dashes,		  GCDashList,		4);
    (&(gp->dashes))[1] = 0;

    gp->clip_mask = None;
    if (mask & GCClipMask) {
	TkpClipMask *clip_mask = AllocClipMask(gp);

	clip_mask->type = TKP_CLIP_PIXMAP;
	clip_mask->value.pixmap = values->clip_mask;
    }
    return gp;
}

/*
 *----------------------------------------------------------------------
 *
 * XChangeGC --
 *
 *	Changes the GC components specified by valuemask for the specified GC.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Updates the specified GC.
 *
 *----------------------------------------------------------------------
 */

int
XChangeGC(
    Display *d,
    GC gc,
    unsigned long mask,
    XGCValues *values)
{
#define ModifyField(name,maskbit) \
	if (mask & (maskbit)) { gc->name = values->name; }

    ModifyField(function, GCFunction);
    ModifyField(plane_mask, GCPlaneMask);
    ModifyField(foreground, GCForeground);
    ModifyField(background, GCBackground);
    ModifyField(line_width, GCLineWidth);
    ModifyField(line_style, GCLineStyle);
    ModifyField(cap_style, GCCapStyle);
    ModifyField(join_style, GCJoinStyle);
    ModifyField(fill_style, GCFillStyle);
    ModifyField(fill_rule, GCFillRule);
    ModifyField(arc_mode, GCArcMode);
    ModifyField(tile, GCTile);
    ModifyField(stipple, GCStipple);
    ModifyField(ts_x_origin, GCTileStipXOrigin);
    ModifyField(ts_y_origin, GCTileStipYOrigin);
    ModifyField(font, GCFont);
    ModifyField(subwindow_mode, GCSubwindowMode);
    ModifyField(graphics_exposures, GCGraphicsExposures);
    ModifyField(clip_x_origin, GCClipXOrigin);
    ModifyField(clip_y_origin, GCClipYOrigin);
    ModifyField(dash_offset, GCDashOffset);
    if (mask & GCClipMask) {
	XSetClipMask(d, gc, values->clip_mask);
    }
    if (mask & GCDashList) {
	gc->dashes = values->dashes;
	(&(gc->dashes))[1] = 0;
    }
    return Success;
}

/*
 *----------------------------------------------------------------------
 *
 * XFreeGC --
 *
 *	Deallocates the specified graphics context.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int XFreeGC(
    Display *d,
    GC gc)
{
    (void)d;

    if (gc != NULL) {
	FreeClipMask(gc);
	ckfree(gc);
    }
    return Success;
}

/*
 *----------------------------------------------------------------------
 *
 * XSetForeground, etc. --
 *
 *	The following functions are simply accessor functions for the GC
 *	slots.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Each function sets some slot in the GC.
 *
 *----------------------------------------------------------------------
 */

int
XSetForeground(
    Display *display,
    GC gc,
    unsigned long foreground)
{
    (void)display;

    gc->foreground = foreground;
    return Success;
}

int
XSetBackground(
    Display *display,
    GC gc,
    unsigned long background)
{
    (void)display;

    gc->background = background;
    return Success;
}

int
XSetDashes(
    Display *display,
    GC gc,
    int dash_offset,
    _Xconst char *dash_list,
    int n)
{
    char *p = &(gc->dashes);
    (void)display;

#ifdef TkWinDeleteBrush
    TkWinDeleteBrush(gc->fgBrush);
    TkWinDeletePen(gc->fgPen);
    TkWinDeleteBrush(gc->bgBrush);
    TkWinDeletePen(gc->fgExtPen);
#endif
    gc->dash_offset = dash_offset;
    if (n > MAX_DASH_LIST_SIZE) n = MAX_DASH_LIST_SIZE;
    while (n-- > 0) {
	*p++ = *dash_list++;
    }
    *p = 0;
    return Success;
}

int
XSetFunction(
    Display *display,
    GC gc,
    int function)
{
    (void)display;

    gc->function = function;
    return Success;
}

int
XSetFillRule(
    Display *display,
    GC gc,
    int fill_rule)
{
    (void)display;

    gc->fill_rule = fill_rule;
    return Success;
}

int
XSetFillStyle(
    Display *display,
    GC gc,
    int fill_style)
{
    (void)display;

    gc->fill_style = fill_style;
    return Success;
}

int
XSetTSOrigin(
    Display *display,
    GC gc,
    int x, int y)
{
    (void)display;

    gc->ts_x_origin = x;
    gc->ts_y_origin = y;
    return Success;
}

int
XSetFont(
    Display *display,
    GC gc,
    Font font)
{
    (void)display;

    gc->font = font;
    return Success;
}

int
XSetArcMode(
    Display *display,
    GC gc,
    int arc_mode)
{
    (void)display;

    gc->arc_mode = arc_mode;
    return Success;
}

int
XSetStipple(
    Display *display,
    GC gc,
    Pixmap stipple)
{
    (void)display;

    gc->stipple = stipple;
    return Success;
}

int
XSetLineAttributes(
    Display *display,
    GC gc,
    unsigned int line_width,
    int line_style,
    int cap_style,
    int join_style)
{
    (void)display;

    gc->line_width = line_width;
    gc->line_style = line_style;
    gc->cap_style = cap_style;
    gc->join_style = join_style;
    return Success;
}

int
XSetClipOrigin(
    Display *display,
    GC gc,
    int clip_x_origin,
    int clip_y_origin)
{
    (void)display;

    gc->clip_x_origin = clip_x_origin;
    gc->clip_y_origin = clip_y_origin;
    return Success;
}

/*
 *----------------------------------------------------------------------
 *
 * TkSetRegion, XSetClipMask --
 *
 *	Sets the clipping region/pixmap for a GC.
 *
 *	Note that unlike the Xlib equivalent, it is not safe to delete the
 *	region after setting it into the GC (except on Mac OS X). The only
 *	uses of TkSetRegion are currently in DisplayFrame and in
 *	ImgPhotoDisplay, which use the GC immediately.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Allocates or deallocates a TkpClipMask.
 *
 *----------------------------------------------------------------------
 */

int
TkSetRegion(
    Display *display,
    GC gc,
    TkRegion r)
{
    (void)display;

    if (r == NULL) {
	Tcl_Panic("must not pass NULL to TkSetRegion for compatibility with X11; use XSetClipMask instead");
    } else {
	TkpClipMask *clip_mask = AllocClipMask(gc);

	clip_mask->type = TKP_CLIP_REGION;
	clip_mask->value.region = r;
    }
    return Success;
}

int
XSetClipMask(
    Display *display,
    GC gc,
    Pixmap pixmap)
{
    (void)display;

    if (pixmap == None) {
	FreeClipMask(gc);
    } else {
	TkpClipMask *clip_mask = AllocClipMask(gc);

	clip_mask->type = TKP_CLIP_PIXMAP;
	clip_mask->value.pixmap = pixmap;
    }
    return Success;
}

/*
 * Some additional dummy functions (hopefully implemented soon).
 */

#if 0
Cursor
XCreateFontCursor(
    Display *display,
    unsigned int shape)
{
    return (Cursor) 0;
}

void
XDrawImageString(
    Display *display,
    Drawable d,
    GC gc,
    int x,
    int y,
    _Xconst char *string,
    int length)
{
}
#endif

int
XDrawPoint(
    Display *display,
    Drawable d,
    GC gc,
    int x,
    int y)
{
    return XDrawLine(display, d, gc, x, y, x, y);
}

int
XDrawPoints(
    Display *display,
    Drawable d,
    GC gc,
    XPoint *points,
    int npoints,
    int mode)
{
    int res = Success;
    (void)mode;

    while (npoints-- > 0) {
	res = XDrawLine(display, d, gc,
		points[0].x, points[0].y, points[0].x, points[0].y);
	if (res != Success) break;
	++points;
    }
    return res;
}

#if !defined(MAC_OSX_TK)
int
XDrawSegments(
    Display *display,
    Drawable d,
    GC gc,
    XSegment *segments,
    int nsegments)
{
    (void)display;
    (void)d;
    (void)gc;
    (void)segments;
    (void)nsegments;

    return BadDrawable;
}
#endif

#if 0
char *
XFetchBuffer(
    Display *display,
    int *nbytes_return,
    int buffer)
{
    (void)display;
    (void)nbytes_return;
    (void)buffer;

    return (char *) 0;
}

Status
XFetchName(
    Display *display,
    Window w,
    char **window_name_return)
{
    (void)display;
    (void)w;
    (void)window_name_return;

    return Success;
}

Atom *
XListProperties(
    Display* display,
    Window w,
    int *num_prop_return)
{
    (void)display;
    (void)w;
    (void)num_prop_return;

    return (Atom *) 0;
}

int
XMapRaised(
    Display *display,
    Window w)
{
    (void)display;
    (void)w;

    return Success;
}

int
XQueryTextExtents(
    Display *display,
    XID font_ID,
    _Xconst char *string,
    int nchars,
    int *direction_return,
    int *font_ascent_return,
    int *font_descent_return,
    XCharStruct *overall_return)
{
    (void)display;
    (void)font_ID;
    (void)string;
    (void)nchars;
    (void)direction_return;
    (void)font_ascent_return;
    (void)font_descent_return;
    (void)overall_return;

    return Success;
}

int
XReparentWindow(
    Display *display,
    Window w,
    Window parent,
    int x,
    int y)
{
    (void)display;
    (void)w;
    (void)parent;
    (void)x;
    (void)y;

    return BadWindow;
}

int
XUndefineCursor(
    Display *display,
    Window w)
{
    (void)display;
    (void)w;

    return Success;
}

XVaNestedList
XVaCreateNestedList(
    int unused, ...)
{
    (void)unused;
    return NULL;
}

char *
XSetICValues(
    XIC xic, ...)
{
    (void)xic;
    return NULL;
}

char *
XGetICValues(
    XIC xic, ...)
{
    (void)xic;
    return NULL;
}

void
XSetICFocus(
    XIC xic)
{
    (void)xic;
}

Window
XCreateWindow(
    Display *display,
	Window parent,
	int x,
	int y,
    unsigned int width,
	unsigned int height,
    unsigned int border_width,
	int depth,
	unsigned int clazz,
    Visual *visual,
	unsigned long value_mask,
    XSetWindowAttributes *attributes)
{
    (void)display;
    (void)parent;
    (void)x;
    (void)y;
    (void)width;
    (void)height;
    (void)border_width;
    (void)depth;
    (void)clazz;
    (void)visual;
    (void)value_mask;
    (void)attributes;

	return 0;
}

int
XPointInRegion(
    Region rgn,
	int x,
	int y)
{
    (void)rgn;
    (void)x;
    (void)y;

	return 0;
}

int
XUnionRegion(
    Region srca,
	Region srcb,
	Region dr_return)
{
    (void)srca;
    (void)srcb;
    (void)dr_return;

	return 0;
}

Region
XPolygonRegion(
    XPoint *pts,
	int n,
	int rule)
{
    (void)pts;
    (void)n;
    (void)rule;

    return 0;
}
#endif

void
XDestroyIC(
    XIC ic)
{
    (void)ic;
}

Cursor
XCreatePixmapCursor(
    Display *display,
    Pixmap source,
    Pixmap mask,
    XColor *foreground_color,
    XColor *background_color,
    unsigned int x,
    unsigned int y)
{
    (void)display;
    (void)source;
    (void)mask;
    (void)foreground_color;
    (void)background_color;
    (void)x;
    (void)y;

    return (Cursor) NULL;
}

Cursor
XCreateGlyphCursor(
    Display *display,
    Font source_font,
    Font mask_font,
    unsigned int source_char,
    unsigned int mask_char,
    XColor _Xconst *foreground_color,
    XColor _Xconst *background_color)
{
    (void)display;
    (void)source_font;
    (void)mask_font;
    (void)source_char;
    (void)mask_char;
    (void)foreground_color;
    (void)background_color;

    return (Cursor) NULL;
}

#if 0
XFontSet
XCreateFontSet(
    Display *display		/* display */,
    _Xconst char *base_font_name_list	/* base_font_name_list */,
    char ***missing_charset_list		/* missing_charset_list */,
    int *missing_charset_count		/* missing_charset_count */,
    char **def_string		/* def_string */
) {
    (void)display;
    (void)base_font_name_list;
    (void)missing_charset_list;
    (void)missing_charset_count;
    (void)def_string;

    return (XFontSet)0;
}

void
XFreeFontSet(
    Display *display,		/* display */
    XFontSet fontset		/* font_set */
) {
    (void)display;
    (void)fontset;
}

void
XFreeStringList(
    char **list		/* list */
) {
    (void)list;
}

Status
XCloseIM(
    XIM im /* im */
) {
    (void)im;

    return Success;
}

Bool
XRegisterIMInstantiateCallback(
    Display *dpy			/* dpy */,
    struct _XrmHashBucketRec *rdb	/* rdb */,
    char *res_name			/* res_name */,
    char *res_class			/* res_class */,
    XIDProc callback			/* callback */,
    XPointer client_data			/* client_data */
) {
    (void)dpy;
    (void)rdb;
    (void)res_name;
    (void)res_class;
    (void)callback;
    (void)client_data;

    return False;
}

Bool
XUnregisterIMInstantiateCallback(
    Display *dpy			/* dpy */,
    struct _XrmHashBucketRec *rdb	/* rdb */,
    char *res_name			/* res_name */,
    char *res_class			/* res_class */,
    XIDProc callback			/* callback */,
    XPointer client_data			/* client_data */
) {
    (void)dpy;
    (void)rdb;
    (void)res_name;
    (void)res_class;
    (void)callback;
    (void)client_data;

    return False;
}

char *
XSetLocaleModifiers(
    const char *modifier_list		/* modifier_list */
) {
    (void)modifier_list;

    return NULL;
}

XIM XOpenIM(
    Display *dpy			/* dpy */,
    struct _XrmHashBucketRec *rdb	/* rdb */,
    char *res_name			/* res_name */,
    char *res_class			/* res_class */
) {
    (void)dpy;
    (void)rdb;
    (void)res_name;
    (void)res_class;

    return NULL;
}

char *
XGetIMValues(
    XIM im /* im */, ...
) {
    (void)im;

    return NULL;
}

char *
XSetIMValues(
    XIM im /* im */, ...
) {
    (void)im;

    return NULL;
}
#endif

/*
 * Local Variables:
 * mode: c
 * c-basic-offset: 4
 * fill-column: 78
 * End:
 */
