
/*
 * bltGrMarker.c --
 *
 *	This module implements markers for the BLT graph widget.
 *
 * Copyright 1993-1998 Lucent Technologies, Inc.
 *
 * Permission to use, copy, modify, and distribute this software and
 * its documentation for any purpose and without fee is hereby
 * granted, provided that the above copyright notice appear in all
 * copies and that both that the copyright notice and warranty
 * disclaimer appear in supporting documentation, and that the names
 * of Lucent Technologies any of their entities not be used in
 * advertising or publicity pertaining to distribution of the software
 * without specific, written prior permission.
 *
 * Lucent Technologies disclaims all warranties with regard to this
 * software, including all implied warranties of merchantability and
 * fitness.  In no event shall Lucent Technologies be liable for any
 * special, indirect or consequential damages or any damages
 * whatsoever resulting from loss of use, data or profits, whether in
 * an action of contract, negligence or other tortuous action, arising
 * out of or in connection with the use or performance of this
 * software.
 */

#include "bltGraph.h"
#include "bltChain.h"
#include "bltGrElem.h"

#define GETBITMAP(b) \
	(((b)->destBitmap == None) ? (b)->srcBitmap : (b)->destBitmap)

#define MAX_OUTLINE_POINTS	12

/* Map graph coordinates to normalized coordinates [0..1] */
#define NORMALIZE(A,x) 	(((x) - (A)->axisRange.min) / (A)->axisRange.range)

#define DEF_MARKER_ANCHOR	"center"
#define DEF_MARKER_BACKGROUND	RGB_WHITE
#define DEF_MARKER_BG_MONO	RGB_WHITE
#define DEF_MARKER_BITMAP	(char *)NULL
#define DEF_MARKER_CAP_STYLE	"butt"
#define DEF_MARKER_COORDS	(char *)NULL
#define DEF_MARKER_DASHES	(char *)NULL
#define DEF_MARKER_DASH_OFFSET	"0"
#define DEF_MARKER_ELEMENT	(char *)NULL
#define DEF_MARKER_FOREGROUND	RGB_BLACK
#define DEF_MARKER_FG_MONO	RGB_BLACK
#define DEF_MARKER_FILL_COLOR	RGB_RED
#define DEF_MARKER_FILL_MONO	RGB_WHITE
#define DEF_MARKER_FONT		STD_FONT
#define DEF_MARKER_GAP_COLOR	RGB_PINK
#define DEF_MARKER_GAP_MONO	RGB_BLACK
#define DEF_MARKER_HEIGHT	"0"
#define DEF_MARKER_HIDE		"no"
#define DEF_MARKER_JOIN_STYLE	"miter"
#define DEF_MARKER_JUSTIFY	"left"
#define DEF_MARKER_LINE_WIDTH	"1"
#define DEF_MARKER_MAP_X	"x"
#define DEF_MARKER_MAP_Y	"y"
#define DEF_MARKER_NAME		(char *)NULL
#define DEF_MARKER_OUTLINE_COLOR RGB_BLACK
#define DEF_MARKER_OUTLINE_MONO	RGB_BLACK
#define DEF_MARKER_PAD		"4"
#define DEF_MARKER_ROTATE	"0.0"
#define DEF_MARKER_SCALE	"1.0"
#define DEF_MARKER_SHADOW_COLOR	(char *)NULL
#define DEF_MARKER_SHADOW_MONO	(char *)NULL
#define DEF_MARKER_STATE	"normal"
#define DEF_MARKER_STIPPLE	(char *)NULL
#define DEF_MARKER_TEXT		(char *)NULL
#define DEF_MARKER_UNDER	"no"
#define DEF_MARKER_WIDTH	"0"
#define DEF_MARKER_WINDOW	(char *)NULL
#define DEF_MARKER_XOR		"no"
#define DEF_MARKER_X_OFFSET	"0"
#define DEF_MARKER_Y_OFFSET	"0"

#define DEF_MARKER_TEXT_TAGS	"Text all"
#define DEF_MARKER_IMAGE_TAGS	"Image all"
#define DEF_MARKER_BITMAP_TAGS	"Bitmap all"
#define DEF_MARKER_WINDOW_TAGS	"Window all"
#define DEF_MARKER_POLYGON_TAGS	"Polygon all"
#define DEF_MARKER_LINE_TAGS	"Line all"

static Tk_OptionParseProc StringToCoordinates;
static Tk_OptionPrintProc CoordinatesToString;
static Tk_CustomOption coordsOption =
{
    StringToCoordinates, CoordinatesToString, (ClientData)0
};
extern Tk_CustomOption bltColorPairOption;
extern Tk_CustomOption bltDashesOption;
extern Tk_CustomOption bltDistanceOption;
extern Tk_CustomOption bltListOption;
extern Tk_CustomOption bltPadOption;
extern Tk_CustomOption bltPositiveDistanceOption;
extern Tk_CustomOption bltShadowOption;
extern Tk_CustomOption bltStateOption;
extern Tk_CustomOption bltXAxisOption;
extern Tk_CustomOption bltYAxisOption;

typedef Marker *(MarkerCreateProc) _ANSI_ARGS_((void));
typedef void (MarkerDrawProc) _ANSI_ARGS_((Marker *markerPtr, 
	Drawable drawable));
typedef void (MarkerFreeProc) _ANSI_ARGS_((Graph *graphPtr, Marker *markerPtr));
typedef int (MarkerConfigProc) _ANSI_ARGS_((Marker *markerPtr));
typedef void (MarkerMapProc) _ANSI_ARGS_((Marker *markerPtr));
typedef void (MarkerPostScriptProc) _ANSI_ARGS_((Marker *markerPtr,
	PsToken psToken));
typedef int (MarkerPointProc) _ANSI_ARGS_((Marker *markerPtr, 
	Point2D *samplePtr));
typedef int (MarkerRegionProc) _ANSI_ARGS_((Marker *markerPtr, 
	Extents2D *extsPtr, int enclosed));

typedef struct {
    Tk_ConfigSpec *configSpecs;	/* Marker configuration specifications */

    MarkerConfigProc *configProc;
    MarkerDrawProc *drawProc;
    MarkerFreeProc *freeProc;
    MarkerMapProc *mapProc;
    MarkerPointProc *pointProc;
    MarkerRegionProc *regionProc;
    MarkerPostScriptProc *postscriptProc;

}  MarkerClass;



/*
 * -------------------------------------------------------------------
 *
 * Marker --
 *
 *	Structure defining the generic marker.  In C++ parlance this
 *	would be the base type from which all markers are derived.
 *
 *	This structure corresponds with the specific types of markers.
 *	Don't change this structure without changing the individual
 *	marker structures of each type below.
 *
 * ------------------------------------------------------------------- 
 */
struct MarkerStruct {
    char *name;			/* Identifier for marker in list */

    Blt_Uid classUid;		/* Type of marker. */

    Graph *graphPtr;		/* Graph widget of marker. */

    unsigned int flags;

    char **tags;

    int hidden;			/* If non-zero, don't display the marker. */

    Blt_HashEntry *hashPtr;

    Blt_ChainLink *linkPtr;

    Point2D *worldPts;		/* Coordinate array to position marker */
    int nWorldPts;		/* Number of points in above array */

    char *elemName;		/* Element associated with marker */

    Axis2D axes;

    int drawUnder;		/* If non-zero, draw the marker
				 * underneath any elements. This can
				 * be a performance penalty because
				 * the graph must be redraw entirely
				 * each time the marker is redrawn. */

    int clipped;		/* Indicates if the marker is totally
				 * clipped by the plotting area. */

    int xOffset, yOffset;	/* Pixel offset from graph position */

    MarkerClass *classPtr;

    int state;
};

/*
 * -------------------------------------------------------------------
 *
 * TextMarker --
 *
 * -------------------------------------------------------------------
 */
typedef struct {
    char *name;			/* Identifier for marker */
    Blt_Uid classUid;		/* Type of marker */
    Graph *graphPtr;		/* The graph this marker belongs to */
    unsigned int flags;
    char **tags;
    int hidden;			/* If non-zero, don't display the
				 * marker. */

    Blt_HashEntry *hashPtr;
    Blt_ChainLink *linkPtr;

    Point2D *worldPts;		/* Position of marker (1 X-Y coordinate) in
				 * world (graph) coordinates. */
    int nWorldPts;		/* Number of points */

    char *elemName;		/* Element associated with marker */
    Axis2D axes;
    int drawUnder;		/* If non-zero, draw the marker
				 * underneath any elements. There can
				 * be a performance because the graph
				 * must be redraw entirely each time
				 * this marker is redrawn. */
    int clipped;		/* Indicates if the marker is totally
				 * clipped by the plotting area. */
    int xOffset, yOffset;	/* pixel offset from anchor */

    MarkerClass *classPtr;

    int state;
    /*
     * Text specific fields and attributes
     */
#ifdef notdef
    char *textVarName;		/* Name of variable (malloc'ed) or
				 * NULL. If non-NULL, graph displays
				 * the contents of this variable. */
#endif
    char *string;		/* Text string to be display.  The string
				 * make contain newlines. */

    Tk_Anchor anchor;		/* Indicates how to translate the given
				 * marker position. */

    Point2D anchorPos;		/* Translated anchor point. */

    int width, height;		/* Dimension of bounding box.  */

    TextStyle style;		/* Text attributes (font, fg, anchor, etc) */

    TextLayout *textPtr;	/* Contains information about the layout
				 * of the text. */
    Point2D outline[5];
    XColor *fillColor;
    GC fillGC;
} TextMarker;


static Tk_ConfigSpec textConfigSpecs[] =
{
    {TK_CONFIG_ANCHOR, "-anchor", "anchor", "Anchor",
	DEF_MARKER_ANCHOR, Tk_Offset(TextMarker, anchor), 0},
    {TK_CONFIG_COLOR, "-background", "background", "MarkerBackground",
	(char *)NULL, Tk_Offset(TextMarker, fillColor),
	TK_CONFIG_NULL_OK},
    {TK_CONFIG_SYNONYM, "-bg", "background", "Background",
	(char *)NULL, 0, 0},
    {TK_CONFIG_CUSTOM, "-bindtags", "bindTags", "BindTags",
	DEF_MARKER_TEXT_TAGS, Tk_Offset(Marker, tags),
	TK_CONFIG_NULL_OK, &bltListOption},
    {TK_CONFIG_CUSTOM, "-coords", "coords", "Coords",
	DEF_MARKER_COORDS, Tk_Offset(Marker, worldPts),
	TK_CONFIG_NULL_OK, &coordsOption},
    {TK_CONFIG_STRING, "-element", "element", "Element",
	DEF_MARKER_ELEMENT, Tk_Offset(Marker, elemName), TK_CONFIG_NULL_OK},
    {TK_CONFIG_SYNONYM, "-fg", "foreground", "Foreground",
	(char *)NULL, 0, 0},
    {TK_CONFIG_SYNONYM, "-fill", "background", (char *)NULL,
	(char *)NULL, 0, 0},
    {TK_CONFIG_FONT, "-font", "font", "Font",
	DEF_MARKER_FONT, Tk_Offset(TextMarker, style.font), 0},
    {TK_CONFIG_COLOR, "-foreground", "foreground", "Foreground",
	DEF_MARKER_FOREGROUND, Tk_Offset(TextMarker, style.color),
	TK_CONFIG_COLOR_ONLY},
    {TK_CONFIG_COLOR, "-foreground", "foreground", "Foreground",
	DEF_MARKER_FG_MONO, Tk_Offset(TextMarker, style.color),
	TK_CONFIG_MONO_ONLY},
    {TK_CONFIG_JUSTIFY, "-justify", "justify", "Justify",
	DEF_MARKER_JUSTIFY, Tk_Offset(TextMarker, style.justify),
	TK_CONFIG_DONT_SET_DEFAULT},
    {TK_CONFIG_BOOLEAN, "-hide", "hide", "Hide",
	DEF_MARKER_HIDE, Tk_Offset(Marker, hidden),
	TK_CONFIG_DONT_SET_DEFAULT},
    {TK_CONFIG_CUSTOM, "-mapx", "mapX", "MapX",
	DEF_MARKER_MAP_X, Tk_Offset(Marker, axes.x), 0, &bltXAxisOption},
    {TK_CONFIG_CUSTOM, "-mapy", "mapY", "MapY",
	DEF_MARKER_MAP_Y, Tk_Offset(Marker, axes.y), 0, &bltYAxisOption},
    {TK_CONFIG_STRING, "-name", (char *)NULL, (char *)NULL,
	DEF_MARKER_NAME, Tk_Offset(Marker, name), TK_CONFIG_NULL_OK},
    {TK_CONFIG_SYNONYM, "-outline", "foreground", (char *)NULL,
	(char *)NULL, 0, 0},
    {TK_CONFIG_CUSTOM, "-padx", "padX", "PadX",
	DEF_MARKER_PAD, Tk_Offset(TextMarker, style.padX),
	TK_CONFIG_DONT_SET_DEFAULT, &bltPadOption},
    {TK_CONFIG_CUSTOM, "-pady", "padY", "PadY",
	DEF_MARKER_PAD, Tk_Offset(TextMarker, style.padY),
	TK_CONFIG_DONT_SET_DEFAULT, &bltPadOption},
    {TK_CONFIG_DOUBLE, "-rotate", "rotate", "Rotate",
	DEF_MARKER_ROTATE, Tk_Offset(TextMarker, style.theta),
	TK_CONFIG_DONT_SET_DEFAULT},
    {TK_CONFIG_CUSTOM, "-shadow", "shadow", "Shadow",
	DEF_MARKER_SHADOW_COLOR, Tk_Offset(TextMarker, style.shadow),
	TK_CONFIG_COLOR_ONLY, &bltShadowOption},
    {TK_CONFIG_CUSTOM, "-shadow", "shadow", "Shadow",
	DEF_MARKER_SHADOW_MONO, Tk_Offset(TextMarker, style.shadow),
	TK_CONFIG_MONO_ONLY, &bltShadowOption},
    {TK_CONFIG_CUSTOM, "-state", "state", "State",
	DEF_MARKER_STATE, Tk_Offset(Marker, state), 
	TK_CONFIG_DONT_SET_DEFAULT, &bltStateOption},
    {TK_CONFIG_STRING, "-text", "text", "Text",
	DEF_MARKER_TEXT, Tk_Offset(TextMarker, string), TK_CONFIG_NULL_OK},
    {TK_CONFIG_BOOLEAN, "-under", "under", "Under",
	DEF_MARKER_UNDER, Tk_Offset(Marker, drawUnder),
	TK_CONFIG_DONT_SET_DEFAULT},
    {TK_CONFIG_PIXELS, "-xoffset", "xOffset", "XOffset",
	DEF_MARKER_X_OFFSET, Tk_Offset(Marker, xOffset),
	TK_CONFIG_DONT_SET_DEFAULT},
    {TK_CONFIG_PIXELS, "-yoffset", "yOffset", "YOffset",
	DEF_MARKER_Y_OFFSET, Tk_Offset(Marker, yOffset),
	TK_CONFIG_DONT_SET_DEFAULT},
    {TK_CONFIG_END, NULL, NULL, NULL, NULL, 0, 0}
};


/*
 * -------------------------------------------------------------------
 *
 * WindowMarker --
 *
 * -------------------------------------------------------------------
 */
typedef struct {
    char *name;			/* Identifier for marker */
    Blt_Uid classUid;		/* Type of marker */
    Graph *graphPtr;		/* Graph marker belongs to */
    unsigned int flags;
    char **tags;
    int hidden;			/* Indicates if the marker is
				 * currently hidden or not. */

    Blt_HashEntry *hashPtr;
    Blt_ChainLink *linkPtr;

    Point2D *worldPts;		/* Position of marker (1 X-Y coordinate) in
				 * world (graph) coordinates. */
    int nWorldPts;		/* Number of points */

    char *elemName;		/* Element associated with marker */
    Axis2D axes;
    int drawUnder;		/* If non-zero, draw the marker
				 * underneath any elements. There can
				 * be a performance because the graph
				 * must be redraw entirely each time
				 * this marker is redrawn. */
    int clipped;		/* Indicates if the marker is totally
				 * clipped by the plotting area. */
    int xOffset, yOffset;	/* Pixel offset from anchor. */

    MarkerClass *classPtr;

    int state;

    /*
     * Window specific attributes
     */
    char *pathName;		/* Name of child widget to be displayed. */
    Tk_Window tkwin;		/* Window to display. */
    int reqWidth, reqHeight;	/* If non-zero, this overrides the size 
				 * requested by the child widget. */

    Tk_Anchor anchor;		/* Indicates how to translate the given
				 * marker position. */

    Point2D anchorPos;		/* Translated anchor point. */
    int width, height;		/* Current size of the child window. */

} WindowMarker;

static Tk_ConfigSpec windowConfigSpecs[] =
{
    {TK_CONFIG_ANCHOR, "-anchor", "anchor", "Anchor",
	DEF_MARKER_ANCHOR, Tk_Offset(WindowMarker, anchor), 0},
    {TK_CONFIG_CUSTOM, "-bindtags", "bindTags", "BindTags",
	DEF_MARKER_WINDOW_TAGS, Tk_Offset(Marker, tags),
	TK_CONFIG_NULL_OK, &bltListOption},
    {TK_CONFIG_CUSTOM, "-coords", "coords", "Coords",
	DEF_MARKER_COORDS, Tk_Offset(WindowMarker, worldPts),
	TK_CONFIG_NULL_OK, &coordsOption},
    {TK_CONFIG_STRING, "-element", "element", "Element",
	DEF_MARKER_ELEMENT, Tk_Offset(Marker, elemName), TK_CONFIG_NULL_OK},
    {TK_CONFIG_CUSTOM, "-height", "height", "Height",
	DEF_MARKER_HEIGHT, Tk_Offset(WindowMarker, reqHeight),
	TK_CONFIG_DONT_SET_DEFAULT, &bltPositiveDistanceOption},
    {TK_CONFIG_BOOLEAN, "-hide", "hide", "Hide",
	DEF_MARKER_HIDE, Tk_Offset(Marker, hidden),
	TK_CONFIG_DONT_SET_DEFAULT},
    {TK_CONFIG_CUSTOM, "-mapx", "mapX", "MapX",
	DEF_MARKER_MAP_X, Tk_Offset(Marker, axes.x), 0, &bltXAxisOption},
    {TK_CONFIG_CUSTOM, "-mapy", "mapY", "MapY",
	DEF_MARKER_MAP_Y, Tk_Offset(Marker, axes.y), 0, &bltYAxisOption},
    {TK_CONFIG_STRING, "-name", (char *)NULL, (char *)NULL,
	DEF_MARKER_NAME, Tk_Offset(Marker, name), TK_CONFIG_NULL_OK},
    {TK_CONFIG_CUSTOM, "-state", "state", "State",
	DEF_MARKER_STATE, Tk_Offset(Marker, state), 
	TK_CONFIG_DONT_SET_DEFAULT, &bltStateOption},
    {TK_CONFIG_BOOLEAN, "-under", "under", "Under",
	DEF_MARKER_UNDER, Tk_Offset(Marker, drawUnder),
	TK_CONFIG_DONT_SET_DEFAULT},
    {TK_CONFIG_CUSTOM, "-width", "width", "Width",
	DEF_MARKER_WIDTH, Tk_Offset(WindowMarker, reqWidth),
	TK_CONFIG_DONT_SET_DEFAULT, &bltPositiveDistanceOption},
    {TK_CONFIG_STRING, "-window", "window", "Window",
	DEF_MARKER_WINDOW, Tk_Offset(WindowMarker, pathName),
	TK_CONFIG_NULL_OK},
    {TK_CONFIG_PIXELS, "-xoffset", "xOffset", "XOffset",
	DEF_MARKER_X_OFFSET, Tk_Offset(Marker, xOffset),
	TK_CONFIG_DONT_SET_DEFAULT},
    {TK_CONFIG_PIXELS, "-yoffset", "yOffset", "YOffset",
	DEF_MARKER_Y_OFFSET, Tk_Offset(Marker, yOffset),
	TK_CONFIG_DONT_SET_DEFAULT},
    {TK_CONFIG_END, NULL, NULL, NULL, NULL, 0, 0}
};

/*
 * -------------------------------------------------------------------
 *
 * BitmapMarker --
 *
 * -------------------------------------------------------------------
 */
typedef struct {
    char *name;			/* Identifier for marker */
    Blt_Uid classUid;		/* Type of marker */
    Graph *graphPtr;		/* Graph marker belongs to */
    unsigned int flags;
    char **tags;
    int hidden;			/* Indicates if the marker is currently
				 * hidden or not. */

    Blt_HashEntry *hashPtr;
    Blt_ChainLink *linkPtr;

    Point2D *worldPts;		/* Position of marker in world (graph)
				 * coordinates. If 2 pairs of X-Y
				 * coordinates are specified, then the
				 * bitmap is resized to fit this area.
				 * Otherwise if 1 pair, then the bitmap
				 * is positioned at the coordinate at its
				 * normal size. */
    int nWorldPts;		/* Number of points */

    char *elemName;		/* Element associated with marker */
    Axis2D axes;
    int drawUnder;		/* If non-zero, draw the marker
				 * underneath any elements. There can
				 * be a performance because the graph
				 * must be redraw entirely each time
				 * this marker is redrawn. */

    int clipped;		/* Indicates if the marker is totally
				 * clipped by the plotting area. */

    int xOffset, yOffset;	/* Pixel offset from origin of bitmap */

    MarkerClass *classPtr;

    int state;

    /* Bitmap specific attributes */
    Pixmap srcBitmap;		/* Original bitmap. May be further
				 * scaled or rotated. */
    double rotate;		/* Requested rotation of the bitmap */
    double theta;		/* Normalized rotation (0..360
				 * degrees) */
    Tk_Anchor anchor;		/* If only one X-Y coordinate is
				 * given, indicates how to translate
				 * the given marker position.  Otherwise,
				 * if there are two X-Y coordinates, then
				 * this value is ignored. */
    Point2D anchorPos;		/* Translated anchor point. */

    XColor *outlineColor;	/* Foreground color */
    XColor *fillColor;		/* Background color */

    GC gc;			/* Private graphic context */
    GC fillGC;			/* Shared graphic context */
    Pixmap destBitmap;		/* Bitmap to be drawn. */
    int destWidth, destHeight;	/* Dimensions of the final bitmap */

    Point2D outline[MAX_OUTLINE_POINTS]; 
				/* Polygon representing the background
				 * of the bitmap. */
    int nOutlinePts;
} BitmapMarker;

static Tk_ConfigSpec bitmapConfigSpecs[] =
{
    {TK_CONFIG_ANCHOR, "-anchor", "anchor", "Anchor",
	DEF_MARKER_ANCHOR, Tk_Offset(BitmapMarker, anchor), 0},
    {TK_CONFIG_COLOR, "-background", "background", "Background",
	DEF_MARKER_BACKGROUND, Tk_Offset(BitmapMarker, fillColor),
	TK_CONFIG_COLOR_ONLY | TK_CONFIG_NULL_OK},
    {TK_CONFIG_COLOR, "-background", "background", "Background",
	DEF_MARKER_BG_MONO, Tk_Offset(BitmapMarker, fillColor),
	TK_CONFIG_MONO_ONLY | TK_CONFIG_NULL_OK},
    {TK_CONFIG_SYNONYM, "-bg", "background", (char *)NULL,
	(char *)NULL, 0, 0},
    {TK_CONFIG_CUSTOM, "-bindtags", "bindTags", "BindTags",
	DEF_MARKER_BITMAP_TAGS, Tk_Offset(Marker, tags),
	TK_CONFIG_NULL_OK, &bltListOption},
    {TK_CONFIG_BITMAP, "-bitmap", "bitmap", "Bitmap",
	DEF_MARKER_BITMAP, Tk_Offset(BitmapMarker, srcBitmap), 
	TK_CONFIG_NULL_OK},
    {TK_CONFIG_CUSTOM, "-coords", "coords", "Coords",
	DEF_MARKER_COORDS, Tk_Offset(Marker, worldPts),
	TK_CONFIG_NULL_OK, &coordsOption},
    {TK_CONFIG_STRING, "-element", "element", "Element",
	DEF_MARKER_ELEMENT, Tk_Offset(Marker, elemName), TK_CONFIG_NULL_OK},
    {TK_CONFIG_SYNONYM, "-fg", "foreground", (char *)NULL,
	(char *)NULL, 0, 0},
    {TK_CONFIG_SYNONYM, "-fill", "background", (char *)NULL,
	(char *)NULL, 0, 0},
    {TK_CONFIG_COLOR, "-foreground", "foreground", "Foreground",
	DEF_MARKER_FOREGROUND, Tk_Offset(BitmapMarker, outlineColor),
	TK_CONFIG_COLOR_ONLY | TK_CONFIG_NULL_OK},
    {TK_CONFIG_COLOR, "-foreground", "foreground", "Foreground",
	DEF_MARKER_FG_MONO, Tk_Offset(BitmapMarker, outlineColor),
	TK_CONFIG_MONO_ONLY | TK_CONFIG_NULL_OK},
    {TK_CONFIG_BOOLEAN, "-hide", "hide", "Hide",
	DEF_MARKER_HIDE, Tk_Offset(Marker, hidden),
	TK_CONFIG_DONT_SET_DEFAULT},
    {TK_CONFIG_CUSTOM, "-mapx", "mapX", "MapX",
	DEF_MARKER_MAP_X, Tk_Offset(Marker, axes.x), 0, &bltXAxisOption},
    {TK_CONFIG_CUSTOM, "-mapy", "mapY", "MapY",
	DEF_MARKER_MAP_Y, Tk_Offset(Marker, axes.y), 0, &bltYAxisOption},
    {TK_CONFIG_STRING, "-name", (char *)NULL, (char *)NULL,
	DEF_MARKER_NAME, Tk_Offset(Marker, name), TK_CONFIG_NULL_OK},
    {TK_CONFIG_SYNONYM, "-outline", "foreground", (char *)NULL,
	(char *)NULL, 0, 0},
    {TK_CONFIG_DOUBLE, "-rotate", "rotate", "Rotate",
	DEF_MARKER_ROTATE, Tk_Offset(BitmapMarker, rotate),
	TK_CONFIG_DONT_SET_DEFAULT},
    {TK_CONFIG_CUSTOM, "-state", "state", "State",
	DEF_MARKER_STATE, Tk_Offset(Marker, state), 
	TK_CONFIG_DONT_SET_DEFAULT, &bltStateOption},
    {TK_CONFIG_BOOLEAN, "-under", "under", "Under",
	DEF_MARKER_UNDER, Tk_Offset(Marker, drawUnder),
	TK_CONFIG_DONT_SET_DEFAULT},
    {TK_CONFIG_PIXELS, "-xoffset", "xOffset", "XOffset",
	DEF_MARKER_X_OFFSET, Tk_Offset(Marker, xOffset),
	TK_CONFIG_DONT_SET_DEFAULT},
    {TK_CONFIG_PIXELS, "-yoffset", "yOffset", "YOffset",
	DEF_MARKER_Y_OFFSET, Tk_Offset(Marker, yOffset),
	TK_CONFIG_DONT_SET_DEFAULT},
    {TK_CONFIG_END, NULL, NULL, NULL, NULL, 0, 0}
};


/*
 * -------------------------------------------------------------------
 *
 * ImageMarker --
 *
 * -------------------------------------------------------------------
 */
typedef struct {
    char *name;			/* Identifier for marker */
    Blt_Uid classUid;		/* Type of marker */
    Graph *graphPtr;		/* Graph marker belongs to */
    unsigned int flags;
    char **tags;
    int hidden;			/* Indicates if the marker is
				 * currently hidden or not. */

    Blt_HashEntry *hashPtr;
    Blt_ChainLink *linkPtr;
    Point2D *worldPts;		/* Position of marker in world (graph)
				 * coordinates. If 2 pairs of X-Y
				 * coordinates are specified, then the
				 * image is resized to fit this area.
				 * Otherwise if 1 pair, then the image
				 * is positioned at the coordinate at
				 * its normal size. */
    int nWorldPts;		/* Number of points */

    char *elemName;		/* Element associated with marker */
    Axis2D axes;
    int drawUnder;		/* If non-zero, draw the marker
				 * underneath any elements. There can
				 * be a performance because the graph
				 * must be redraw entirely each time
				 * this marker is redrawn. */
    int clipped;		/* Indicates if the marker is totally
				 * clipped by the plotting area. */
    int xOffset, yOffset;	/* Pixel offset from anchor */

    MarkerClass *classPtr;

    int state;

    /* Image specific attributes */
    char *imageName;		/* Name of image to be displayed. */
    Tk_Image tkImage;		/* Tk image to be displayed. */
    Tk_Anchor anchor;		/* Indicates how to translate the given
				 * marker position. */
    Point2D anchorPos;		/* Translated anchor point. */
    int width, height;		/* Dimensions of the image */
    Tk_Image tmpImage;
    Pixmap pixmap;		/* Pixmap containing the scaled image */
    ColorTable colorTable;	/* Pointer to color table */
    Blt_ColorImage srcImage;
    GC gc;

} ImageMarker;

static Tk_ConfigSpec imageConfigSpecs[] =
{
    {TK_CONFIG_ANCHOR, "-anchor", "anchor", "Anchor",
	DEF_MARKER_ANCHOR, Tk_Offset(ImageMarker, anchor), 0},
    {TK_CONFIG_CUSTOM, "-bindtags", "bindTags", "BindTags",
	DEF_MARKER_IMAGE_TAGS, Tk_Offset(Marker, tags),
	TK_CONFIG_NULL_OK, &bltListOption},
    {TK_CONFIG_CUSTOM, "-coords", "coords", "Coords",
	DEF_MARKER_COORDS, Tk_Offset(Marker, worldPts),
	TK_CONFIG_NULL_OK, &coordsOption},
    {TK_CONFIG_STRING, "-element", "element", "Element",
	DEF_MARKER_ELEMENT, Tk_Offset(Marker, elemName), TK_CONFIG_NULL_OK},
    {TK_CONFIG_BOOLEAN, "-hide", "hide", "Hide",
	DEF_MARKER_HIDE, Tk_Offset(Marker, hidden),
	TK_CONFIG_DONT_SET_DEFAULT},
    {TK_CONFIG_STRING, "-image", "image", "Image",
	(char *)NULL, Tk_Offset(ImageMarker, imageName), TK_CONFIG_NULL_OK},
    {TK_CONFIG_CUSTOM, "-mapx", "mapX", "MapX",
	DEF_MARKER_MAP_X, Tk_Offset(Marker, axes.x), 0, &bltXAxisOption},
    {TK_CONFIG_CUSTOM, "-mapy", "mapY", "MapY",
	DEF_MARKER_MAP_Y, Tk_Offset(Marker, axes.y), 0, &bltYAxisOption},
    {TK_CONFIG_STRING, "-name", (char *)NULL, (char *)NULL,
	DEF_MARKER_NAME, Tk_Offset(Marker, name), TK_CONFIG_NULL_OK},
    {TK_CONFIG_CUSTOM, "-state", "state", "State",
	DEF_MARKER_STATE, Tk_Offset(Marker, state), 
	TK_CONFIG_DONT_SET_DEFAULT, &bltStateOption},
    {TK_CONFIG_BOOLEAN, "-under", "under", "Under",
	DEF_MARKER_UNDER, Tk_Offset(Marker, drawUnder),
	TK_CONFIG_DONT_SET_DEFAULT},
    {TK_CONFIG_PIXELS, "-xoffset", "xOffset", "XOffset",
	DEF_MARKER_X_OFFSET, Tk_Offset(Marker, xOffset),
	TK_CONFIG_DONT_SET_DEFAULT},
    {TK_CONFIG_PIXELS, "-yoffset", "yOffset", "YOffset",
	DEF_MARKER_Y_OFFSET, Tk_Offset(Marker, yOffset),
	TK_CONFIG_DONT_SET_DEFAULT},
    {TK_CONFIG_END, NULL, NULL, NULL, NULL, 0, 0}
};

/*
 * -------------------------------------------------------------------
 *
 * LineMarker --
 *
 * -------------------------------------------------------------------
 */
typedef struct {
    char *name;			/* Identifier for marker */
    Blt_Uid classUid;		/* Type is "linemarker" */
    Graph *graphPtr;		/* Graph marker belongs to */
    unsigned int flags;
    char **tags;
    int hidden;			/* Indicates if the marker is currently
				 * hidden or not. */

    Blt_HashEntry *hashPtr;
    Blt_ChainLink *linkPtr;

    Point2D *worldPts;		/* Position of marker (X-Y coordinates) in
				 * world (graph) coordinates. */
    int nWorldPts;		/* Number of points */

    char *elemName;		/* Element associated with marker */
    Axis2D axes;
    int drawUnder;		/* If non-zero, draw the marker
				 * underneath any elements. There can
				 * be a performance because the graph
				 * must be redraw entirely each time
				 * this marker is redrawn. */
    int clipped;		/* Indicates if the marker is totally
				 * clipped by the plotting area. */
    int xOffset, yOffset;	/* Pixel offset */

    MarkerClass *classPtr;

    int state;

    /* Line specific attributes */
    XColor *fillColor;
    XColor *outlineColor;	/* Foreground and background colors */

    int lineWidth;		/* Line width. */
    int capStyle;		/* Cap style. */
    int joinStyle;		/* Join style.*/
    Blt_Dashes dashes;		/* Dash list values (max 11) */

    GC gc;			/* Private graphic context */

    Segment2D *segments;	/* Malloc'ed array of points.
				 * Represents individual line segments
				 * (2 points per segment) comprising
				 * the mapped line.  The segments may
				 * not necessarily be connected after
				 * clipping. */
    int nSegments;		/* # segments in the above array. */

    int xor;
    int xorState;		/* State of the XOR drawing. Indicates
				 * if the marker is currently drawn. */
} LineMarker;

static Tk_ConfigSpec lineConfigSpecs[] =
{
    {TK_CONFIG_CUSTOM, "-bindtags", "bindTags", "BindTags",
	DEF_MARKER_LINE_TAGS, Tk_Offset(Marker, tags),
	TK_CONFIG_NULL_OK, &bltListOption},
    {TK_CONFIG_CAP_STYLE, "-cap", "cap", "Cap",
	DEF_MARKER_CAP_STYLE, Tk_Offset(LineMarker, capStyle),
	TK_CONFIG_DONT_SET_DEFAULT},
    {TK_CONFIG_CUSTOM, "-coords", "coords", "Coords",
	DEF_MARKER_COORDS, Tk_Offset(Marker, worldPts),
	TK_CONFIG_NULL_OK, &coordsOption},
    {TK_CONFIG_CUSTOM, "-dashes", "dashes", "Dashes",
	DEF_MARKER_DASHES, Tk_Offset(LineMarker, dashes),
	TK_CONFIG_NULL_OK, &bltDashesOption},
    {TK_CONFIG_CUSTOM, "-dashoffset", "dashOffset", "DashOffset",
	DEF_MARKER_DASH_OFFSET, Tk_Offset(LineMarker, dashes.offset),
	TK_CONFIG_DONT_SET_DEFAULT, &bltDistanceOption},
    {TK_CONFIG_STRING, "-element", "element", "Element",
	DEF_MARKER_ELEMENT, Tk_Offset(Marker, elemName), TK_CONFIG_NULL_OK},
    {TK_CONFIG_COLOR, "-fill", "fill", "Fill",
	(char *)NULL, Tk_Offset(LineMarker, fillColor), TK_CONFIG_NULL_OK},
    {TK_CONFIG_JOIN_STYLE, "-join", "join", "Join",
	DEF_MARKER_JOIN_STYLE, Tk_Offset(LineMarker, joinStyle),
	TK_CONFIG_DONT_SET_DEFAULT},
    {TK_CONFIG_CUSTOM, "-linewidth", "lineWidth", "LineWidth",
	DEF_MARKER_LINE_WIDTH, Tk_Offset(LineMarker, lineWidth),
	TK_CONFIG_DONT_SET_DEFAULT, &bltDistanceOption},
    {TK_CONFIG_BOOLEAN, "-hide", "hide", "Hide",
	DEF_MARKER_HIDE, Tk_Offset(Marker, hidden),
	TK_CONFIG_DONT_SET_DEFAULT},
    {TK_CONFIG_CUSTOM, "-mapx", "mapX", "MapX",
	DEF_MARKER_MAP_X, Tk_Offset(Marker, axes.x), 0, &bltXAxisOption},
    {TK_CONFIG_CUSTOM, "-mapy", "mapY", "MapY",
	DEF_MARKER_MAP_Y, Tk_Offset(Marker, axes.y), 0, &bltYAxisOption},
    {TK_CONFIG_STRING, "-name", (char *)NULL, (char *)NULL,
	DEF_MARKER_NAME, Tk_Offset(Marker, name), TK_CONFIG_NULL_OK},
    {TK_CONFIG_COLOR, "-outline", "outline", "Outline",
	DEF_MARKER_OUTLINE_COLOR, Tk_Offset(LineMarker, outlineColor),
	TK_CONFIG_COLOR_ONLY | TK_CONFIG_NULL_OK},
    {TK_CONFIG_COLOR, "-outline", "outline", "Outline",
	DEF_MARKER_OUTLINE_MONO, Tk_Offset(LineMarker, outlineColor),
	TK_CONFIG_MONO_ONLY | TK_CONFIG_NULL_OK},
    {TK_CONFIG_CUSTOM, "-state", "state", "State",
	DEF_MARKER_STATE, Tk_Offset(Marker, state), 
	TK_CONFIG_DONT_SET_DEFAULT, &bltStateOption},
    {TK_CONFIG_BOOLEAN, "-under", "under", "Under",
	DEF_MARKER_UNDER, Tk_Offset(Marker, drawUnder),
	TK_CONFIG_DONT_SET_DEFAULT},
    {TK_CONFIG_PIXELS, "-xoffset", "xOffset", "XOffset",
	DEF_MARKER_X_OFFSET, Tk_Offset(Marker, xOffset),
	TK_CONFIG_DONT_SET_DEFAULT},
    {TK_CONFIG_BOOLEAN, "-xor", "xor", "Xor",
	DEF_MARKER_XOR, Tk_Offset(LineMarker, xor), TK_CONFIG_DONT_SET_DEFAULT},
    {TK_CONFIG_PIXELS, "-yoffset", "yOffset", "YOffset",
	DEF_MARKER_Y_OFFSET, Tk_Offset(Marker, yOffset),
	TK_CONFIG_DONT_SET_DEFAULT},
    {TK_CONFIG_END, NULL, NULL, NULL, NULL, 0, 0}
};

/*
 * -------------------------------------------------------------------
 *
 * PolygonMarker --
 *
 * -------------------------------------------------------------------
 */
typedef struct {
    char *name;			/* Identifier for marker */
    Blt_Uid classUid;		/* Type of marker */
    Graph *graphPtr;		/* Graph marker belongs to */
    unsigned int flags;
    char **tags;
    int hidden;			/* Indicates if the marker is currently
				 * hidden or not. */

    Blt_HashEntry *hashPtr;
    Blt_ChainLink *linkPtr;

    Point2D *worldPts;		/* Position of marker (X-Y coordinates) in
				 * world (graph) coordinates. */
    int nWorldPts;		/* Number of points */

    char *elemName;		/* Element associated with marker */
    Axis2D axes;
    int drawUnder;		/* If non-zero, draw the marker
				 * underneath any elements. There can
				 * be a performance because the graph
				 * must be redraw entirely each time
				 * this marker is redrawn. */
    int clipped;		/* Indicates if the marker is totally
				 * clipped by the plotting area. */
    int xOffset, yOffset;	/* Pixel offset */

    MarkerClass *classPtr;

    int state;

    /* Polygon specific attributes and fields */

    Point2D *screenPts;		/* Array of points representing the
				 * polygon in screen coordinates. It's
				 * not used for drawing, but to
				 * generate the outlinePts and fillPts
				 * arrays that are the coordinates of
				 * the possibly clipped outline and
				 * filled polygon. */

    ColorPair outline;
    ColorPair fill;

    Pixmap stipple;		/* Stipple pattern to fill the polygon. */
    int lineWidth;		/* Width of polygon outline. */
    int capStyle;
    int joinStyle;
    Blt_Dashes dashes;		/* List of dash values.  Indicates how
				 * draw the dashed line.  If no dash
				 * values are provided, or the first value
				 * is zero, then the line is drawn solid. */

    GC outlineGC;		/* Graphics context to draw the outline of 
				 * the polygon. */
    GC fillGC;			/* Graphics context to draw the filled
				 * polygon. */

    Point2D *fillPts;		/* Malloc'ed array of points used to draw
				 * the filled polygon. These points may
				 * form a degenerate polygon after clipping.
				 */

    int nFillPts;		/* # points in the above array. */

    Segment2D *outlinePts;	/* Malloc'ed array of points.
				 * Represents individual line segments
				 * (2 points per segment) comprising
				 * the outline of the polygon.  The
				 * segments may not necessarily be
				 * closed or connected after clipping. */

    int nOutlinePts;		/* # points in the above array. */

    int xor;
    int xorState;		/* State of the XOR drawing. Indicates
				 * if the marker is visible. We have
				 * to drawn it again to erase it. */
} PolygonMarker;

static Tk_ConfigSpec polygonConfigSpecs[] =
{
    {TK_CONFIG_CUSTOM, "-bindtags", "bindTags", "BindTags",
	DEF_MARKER_POLYGON_TAGS, Tk_Offset(Marker, tags),
	TK_CONFIG_NULL_OK, &bltListOption},
    {TK_CONFIG_CAP_STYLE, "-cap", "cap", "Cap",
	DEF_MARKER_CAP_STYLE, Tk_Offset(PolygonMarker, capStyle),
	TK_CONFIG_DONT_SET_DEFAULT},
    {TK_CONFIG_CUSTOM, "-coords", "coords", "Coords",
	DEF_MARKER_COORDS, Tk_Offset(Marker, worldPts),
	TK_CONFIG_NULL_OK, &coordsOption},
    {TK_CONFIG_CUSTOM, "-dashes", "dashes", "Dashes",
	DEF_MARKER_DASHES, Tk_Offset(PolygonMarker, dashes),
	TK_CONFIG_NULL_OK, &bltDashesOption},
    {TK_CONFIG_STRING, "-element", "element", "Element",
	DEF_MARKER_ELEMENT, Tk_Offset(Marker, elemName), TK_CONFIG_NULL_OK},
    {TK_CONFIG_CUSTOM, "-fill", "fill", "Fill",
	DEF_MARKER_FILL_COLOR, Tk_Offset(PolygonMarker, fill),
	TK_CONFIG_COLOR_ONLY | TK_CONFIG_NULL_OK, &bltColorPairOption},
    {TK_CONFIG_CUSTOM, "-fill", "fill", "Fill",
	DEF_MARKER_FILL_MONO, Tk_Offset(PolygonMarker, fill),
	TK_CONFIG_MONO_ONLY | TK_CONFIG_NULL_OK, &bltColorPairOption},
    {TK_CONFIG_JOIN_STYLE, "-join", "join", "Join",
	DEF_MARKER_JOIN_STYLE, Tk_Offset(PolygonMarker, joinStyle),
	TK_CONFIG_DONT_SET_DEFAULT},
    {TK_CONFIG_CUSTOM, "-linewidth", "lineWidth", "LineWidth",
	DEF_MARKER_LINE_WIDTH, Tk_Offset(PolygonMarker, lineWidth),
	TK_CONFIG_DONT_SET_DEFAULT, &bltDistanceOption},
    {TK_CONFIG_BOOLEAN, "-hide", "hide", "Hide",
	DEF_MARKER_HIDE, Tk_Offset(Marker, hidden),
	TK_CONFIG_DONT_SET_DEFAULT},
    {TK_CONFIG_CUSTOM, "-mapx", "mapX", "MapX",
	DEF_MARKER_MAP_X, Tk_Offset(Marker, axes.x), 0, &bltXAxisOption},
    {TK_CONFIG_CUSTOM, "-mapy", "mapY", "MapY",
	DEF_MARKER_MAP_Y, Tk_Offset(Marker, axes.y), 0, &bltYAxisOption},
    {TK_CONFIG_STRING, "-name", (char *)NULL, (char *)NULL,
	DEF_MARKER_NAME, Tk_Offset(Marker, name), TK_CONFIG_NULL_OK},
    {TK_CONFIG_CUSTOM, "-outline", "outline", "Outline",
	DEF_MARKER_OUTLINE_COLOR, Tk_Offset(PolygonMarker, outline),
	TK_CONFIG_COLOR_ONLY | TK_CONFIG_NULL_OK, &bltColorPairOption},
    {TK_CONFIG_CUSTOM, "-outline", "outline", "Outline",
	DEF_MARKER_OUTLINE_MONO, Tk_Offset(PolygonMarker, outline),
	TK_CONFIG_MONO_ONLY | TK_CONFIG_NULL_OK, &bltColorPairOption},
    {TK_CONFIG_CUSTOM, "-state", "state", "State",
	DEF_MARKER_STATE, Tk_Offset(Marker, state), 
	TK_CONFIG_DONT_SET_DEFAULT, &bltStateOption},
    {TK_CONFIG_BITMAP, "-stipple", "stipple", "Stipple",
	DEF_MARKER_STIPPLE, Tk_Offset(PolygonMarker, stipple),
	TK_CONFIG_NULL_OK},
    {TK_CONFIG_BOOLEAN, "-under", "under", "Under",
	DEF_MARKER_UNDER, Tk_Offset(Marker, drawUnder),
	TK_CONFIG_DONT_SET_DEFAULT},
    {TK_CONFIG_PIXELS, "-xoffset", "xOffset", "XOffset",
	DEF_MARKER_X_OFFSET, Tk_Offset(Marker, xOffset),
	TK_CONFIG_DONT_SET_DEFAULT},
    {TK_CONFIG_BOOLEAN, "-xor", "xor", "Xor",
	DEF_MARKER_XOR, Tk_Offset(PolygonMarker, xor), 
	TK_CONFIG_DONT_SET_DEFAULT},
    {TK_CONFIG_PIXELS, "-yoffset", "yOffset", "YOffset",
	DEF_MARKER_Y_OFFSET, Tk_Offset(Marker, yOffset),
	TK_CONFIG_DONT_SET_DEFAULT},
    {TK_CONFIG_END, NULL, NULL, NULL, NULL, 0, 0}
};

static MarkerCreateProc CreateBitmapMarker, CreateLineMarker, CreateImageMarker,
	CreatePolygonMarker, CreateTextMarker, CreateWindowMarker;

static MarkerDrawProc DrawBitmapMarker, DrawLineMarker, DrawImageMarker,
	DrawPolygonMarker, DrawTextMarker, DrawWindowMarker;

static MarkerFreeProc FreeBitmapMarker, FreeLineMarker, FreeImageMarker, 
	FreePolygonMarker, FreeTextMarker, FreeWindowMarker;

static MarkerConfigProc ConfigureBitmapMarker, ConfigureLineMarker, 
	ConfigureImageMarker, ConfigurePolygonMarker, ConfigureTextMarker, 
	ConfigureWindowMarker;

static MarkerMapProc MapBitmapMarker, MapLineMarker, MapImageMarker, 
	MapPolygonMarker, MapTextMarker, MapWindowMarker;

static MarkerPostScriptProc BitmapMarkerToPostScript, LineMarkerToPostScript, 
	ImageMarkerToPostScript, PolygonMarkerToPostScript, 
	TextMarkerToPostScript, WindowMarkerToPostScript;

static MarkerPointProc PointInBitmapMarker, PointInLineMarker, 
	PointInImageMarker, PointInPolygonMarker, PointInTextMarker, 
	PointInWindowMarker;

static MarkerRegionProc RegionInBitmapMarker, RegionInLineMarker, 
	RegionInImageMarker, RegionInPolygonMarker, RegionInTextMarker, 
	RegionInWindowMarker;

static Tk_ImageChangedProc ImageChangedProc;

static MarkerClass bitmapMarkerClass = {
    bitmapConfigSpecs,
    ConfigureBitmapMarker,
    DrawBitmapMarker,
    FreeBitmapMarker,
    MapBitmapMarker,
    PointInBitmapMarker,
    RegionInBitmapMarker,
    BitmapMarkerToPostScript,
};

static MarkerClass imageMarkerClass = {
    imageConfigSpecs,
    ConfigureImageMarker,
    DrawImageMarker,
    FreeImageMarker,
    MapImageMarker,
    PointInImageMarker,
    RegionInImageMarker,
    ImageMarkerToPostScript,
};

static MarkerClass lineMarkerClass = {
    lineConfigSpecs,
    ConfigureLineMarker,
    DrawLineMarker,
    FreeLineMarker,
    MapLineMarker,
    PointInLineMarker,
    RegionInLineMarker,
    LineMarkerToPostScript,
};

static MarkerClass polygonMarkerClass = {
    polygonConfigSpecs,
    ConfigurePolygonMarker,
    DrawPolygonMarker,
    FreePolygonMarker,
    MapPolygonMarker,
    PointInPolygonMarker,
    RegionInPolygonMarker,
    PolygonMarkerToPostScript,
};

static MarkerClass textMarkerClass = {
    textConfigSpecs,
    ConfigureTextMarker,
    DrawTextMarker,
    FreeTextMarker,
    MapTextMarker,
    PointInTextMarker,
    RegionInTextMarker,
    TextMarkerToPostScript,
};

static MarkerClass windowMarkerClass = {
    windowConfigSpecs,
    ConfigureWindowMarker,
    DrawWindowMarker,
    FreeWindowMarker,
    MapWindowMarker,
    PointInWindowMarker,
    RegionInWindowMarker,
    WindowMarkerToPostScript,
};

#ifdef notdef
static MarkerClass rectangleMarkerClass = {
    rectangleConfigSpecs,
    ConfigureRectangleMarker,
    DrawRectangleMarker,
    FreeRectangleMarker,
    MapRectangleMarker,
    PointInRectangleMarker,
    RegionInRectangleMarker,
    RectangleMarkerToPostScript,
};

static MarkerClass ovalMarkerClass = {
    ovalConfigSpecs,
    ConfigureOvalMarker,
    DrawOvalMarker,
    FreeOvalMarker,
    MapOvalMarker,
    PointInOvalMarker,
    RegionInOvalMarker,
    OvalMarkerToPostScript,
};
#endif

/*
 * ----------------------------------------------------------------------
 *
 * BoxesDontOverlap --
 *
 *	Tests if the bounding box of a marker overlaps the plotting
 *	area in any way.  If so, the marker will be drawn.  Just do a
 *	min/max test on the extents of both boxes.
 *
 *	Note: It's assumed that the extents of the bounding box lie 
 *	      within the area.  So for a 10x10 rectangle, bottom and
 *	      left would be 9.
 *
 * Results:
 *	Returns 0 is the marker is visible in the plotting area, and
 *	1 otherwise (marker is clipped).
 *
 * ----------------------------------------------------------------------
 */
static int
BoxesDontOverlap(graphPtr, extsPtr)
    Graph *graphPtr;
    Extents2D *extsPtr;
{
    assert(extsPtr->right >= extsPtr->left);
    assert(extsPtr->bottom >= extsPtr->top);
    assert(graphPtr->right >= graphPtr->left);
    assert(graphPtr->bottom >= graphPtr->top);

    return (((double)graphPtr->right < extsPtr->left) ||
	    ((double)graphPtr->bottom < extsPtr->top) ||
	    (extsPtr->right < (double)graphPtr->left) ||
	    (extsPtr->bottom < (double)graphPtr->top));
}


/*
 * ----------------------------------------------------------------------
 *
 * GetCoordinate --
 *
 * 	Convert the expression string into a floating point value. The
 *	only reason we use this routine instead of Blt_ExprDouble is to
 *	handle "elastic" bounds.  That is, convert the strings "-Inf",
 *	"Inf" into -(DBL_MAX) and DBL_MAX respectively.
 *
 * Results:
 *	The return value is a standard Tcl result.  The value of the
 * 	expression is passed back via valuePtr.
 *
 * ----------------------------------------------------------------------
 */
static int
GetCoordinate(interp, expr, valuePtr)
    Tcl_Interp *interp;		/* Interpreter to send results back to */
    char *expr;			/* Numeric expression string to parse */
    double *valuePtr;		/* Real-valued result of expression */
{
    char c;

    c = expr[0];
    if ((c == 'I') && (strcmp(expr, "Inf") == 0)) {
	*valuePtr = DBL_MAX;	/* Elastic upper bound */
    } else if ((c == '-') && (expr[1] == 'I') && (strcmp(expr, "-Inf") == 0)) {
	*valuePtr = -DBL_MAX;	/* Elastic lower bound */
    } else if ((c == '+') && (expr[1] == 'I') && (strcmp(expr, "+Inf") == 0)) {
	*valuePtr = DBL_MAX;	/* Elastic upper bound */
    } else if (Tcl_ExprDouble(interp, expr, valuePtr) != TCL_OK) {
	return TCL_ERROR;
    }
    return TCL_OK;
}


/*
 * ----------------------------------------------------------------------
 *
 * PrintCoordinate --
 *
 * 	Convert the floating point value into its string
 * 	representation.  The only reason this routine is used in
 * 	instead of sprintf, is to handle the "elastic" bounds.  That
 * 	is, convert the values DBL_MAX and -(DBL_MAX) into "+Inf" and
 * 	"-Inf" respectively.
 *
 * Results:
 *	The return value is a standard Tcl result.  The string of the
 * 	expression is passed back via string.
 *
 * ---------------------------------------------------------------------- */
static char *
PrintCoordinate(interp, x)
    Tcl_Interp *interp;
    double x;			/* Numeric value */
{
    if (x == DBL_MAX) {
	return "+Inf";
    } else if (x == -DBL_MAX) {
	return "-Inf";
    } else {
	static char string[TCL_DOUBLE_SPACE + 1];

	Tcl_PrintDouble(interp, (double)x, string);
	return string;
    }
}

/*
 * ----------------------------------------------------------------------
 *
 * ParseCoordinates --
 *
 *	The Tcl coordinate list is converted to their floating point
 *	values. It will then replace the current marker coordinates.
 *
 *	Since different marker types require different number of
 *	coordinates this must be checked here.
 *
 * Results:
 *	The return value is a standard Tcl result.
 *
 * Side effects:
 *	If the marker coordinates are reset, the graph is eventually
 *	redrawn with at the new marker coordinates.
 *
 * ----------------------------------------------------------------------
 */
static int
ParseCoordinates(interp, markerPtr, nExprs, exprArr)
    Tcl_Interp *interp;
    Marker *markerPtr;
    int nExprs;
    char **exprArr;
{
    int nWorldPts;
    int minArgs, maxArgs;
    Point2D *worldPts;
    register int i;
    register Point2D *pointPtr;
    double x, y;

    if (nExprs == 0) {
	return TCL_OK;
    }
    if (nExprs & 1) {
	Tcl_AppendResult(interp, "odd number of marker coordinates specified",
	    (char *)NULL);
	return TCL_ERROR;
    }
    if (markerPtr->classUid == bltLineMarkerUid) {
	minArgs = 4, maxArgs = 0;
    } else if (markerPtr->classUid == bltPolygonMarkerUid) {
	minArgs = 6, maxArgs = 0;
    } else if ((markerPtr->classUid == bltWindowMarkerUid) ||
	       (markerPtr->classUid == bltTextMarkerUid)) {
	minArgs = 2, maxArgs = 2;
    } else if ((markerPtr->classUid == bltImageMarkerUid) ||
	       (markerPtr->classUid == bltBitmapMarkerUid)) {
	minArgs = 2, maxArgs = 4;
    } else {
	Tcl_AppendResult(interp, "unknown marker type", (char *)NULL);
	return TCL_ERROR;
    }

    if (nExprs < minArgs) {
	Tcl_AppendResult(interp, "too few marker coordinates specified",
	    (char *)NULL);
	return TCL_ERROR;
    }
    if ((maxArgs > 0) && (nExprs > maxArgs)) {
	Tcl_AppendResult(interp, "too many marker coordinates specified",
	    (char *)NULL);
	return TCL_ERROR;
    }
    nWorldPts = nExprs / 2;
    worldPts = Blt_Malloc(nWorldPts * sizeof(Point2D));
    if (worldPts == NULL) {
	Tcl_AppendResult(interp, "can't allocate new coordinate array",
	    (char *)NULL);
	return TCL_ERROR;
    }

    /* Don't free the old coordinate array until we've parsed the new
     * coordinates without errors.  */
    pointPtr = worldPts;
    for (i = 0; i < nExprs; i += 2) {
	if ((GetCoordinate(interp, exprArr[i], &x) != TCL_OK) ||
	    (GetCoordinate(interp, exprArr[i + 1], &y) != TCL_OK)) {
	    Blt_Free(worldPts);
	    return TCL_ERROR;
	}
	pointPtr->x = x, pointPtr->y = y;
	pointPtr++;
    }
    if (markerPtr->worldPts != NULL) {
	Blt_Free(markerPtr->worldPts);
    }
    markerPtr->worldPts = worldPts;
    markerPtr->nWorldPts = nWorldPts;
    markerPtr->flags |= MAP_ITEM;
    return TCL_OK;
}

/*
 * ----------------------------------------------------------------------
 *
 * StringToCoordinates --
 *
 *	Given a Tcl list of numeric expression representing the
 *	element values, convert into an array of floating point
 *	values. In addition, the minimum and maximum values are saved.
 *	Since elastic values are allow (values which translate to the
 *	min/max of the graph), we must try to get the non-elastic
 *	minimum and maximum.
 *
 * Results:
 *	The return value is a standard Tcl result.  The vector is
 *	passed back via the vecPtr.
 *
 * ---------------------------------------------------------------------- 
 */
/*ARGSUSED*/
static int
StringToCoordinates(clientData, interp, tkwin, string, widgRec, offset)
    ClientData clientData;	/* Not used. */
    Tcl_Interp *interp;		/* Interpreter to send results back to */
    Tk_Window tkwin;		/* Not used. */
    char *string;		/* Tcl list of numeric expressions */
    char *widgRec;		/* Marker record */
    int offset;			/* Not used. */
{
    Marker *markerPtr = (Marker *)widgRec;
    int nExprs;
    char **exprArr;
    int result;

    nExprs = 0;
    if ((string != NULL) &&
	(Tcl_SplitList(interp, string, &nExprs, &exprArr) != TCL_OK)) {
	return TCL_ERROR;
    }
    if (nExprs == 0) {
	if (markerPtr->worldPts != NULL) {
	    Blt_Free(markerPtr->worldPts);
	    markerPtr->worldPts = NULL;
	}
	markerPtr->nWorldPts = 0;
	return TCL_OK;
    }
    result = ParseCoordinates(interp, markerPtr, nExprs, exprArr);
    Blt_Free(exprArr);
    return result;
}

/*
 * ----------------------------------------------------------------------
 *
 * CoordinatesToString --
 *
 *	Convert the vector of floating point values into a Tcl list.
 *
 * Results:
 *	The string representation of the vector is returned.
 *
 * ----------------------------------------------------------------------
 */
/*ARGSUSED*/
static char *
CoordinatesToString(clientData, tkwin, widgRec, offset, freeProcPtr)
    ClientData clientData;	/* Not used. */
    Tk_Window tkwin;		/* Not used. */
    char *widgRec;		/* Marker record */
    int offset;			/* Not used. */
    Tcl_FreeProc **freeProcPtr;	/* Memory deallocation scheme to use */
{
    Marker *markerPtr = (Marker *)widgRec;
    Tcl_Interp *interp;
    Tcl_DString dString;
    char *result;
    register int i;
    register Point2D *p;

    if (markerPtr->nWorldPts < 1) {
	return "";
    }
    interp = markerPtr->graphPtr->interp;

    Tcl_DStringInit(&dString);
    p = markerPtr->worldPts;
    for (i = 0; i < markerPtr->nWorldPts; i++) {
	Tcl_DStringAppendElement(&dString, PrintCoordinate(interp, p->x));
	Tcl_DStringAppendElement(&dString, PrintCoordinate(interp, p->y));
	p++;
    }
    result = Tcl_DStringValue(&dString);

    /*
     * If memory wasn't allocated for the dynamic string, do it here (it's
     * currently on the stack), so that Tcl can free it normally.
     */
    if (result == dString.staticSpace) {
	result = Blt_Strdup(result);
    }
    *freeProcPtr = (Tcl_FreeProc *)Blt_Free;
    return result;
}

/*
 * ----------------------------------------------------------------------
 *
 * HMap --
 *
 *	Map the given graph coordinate value to its axis, returning a
 *	window position.
 *
 * Results:
 *	Returns a floating point number representing the window
 *	coordinate position on the given axis.
 *
 * ---------------------------------------------------------------------- 
 */
static double
HMap(graphPtr, axisPtr, x)
    Graph *graphPtr;
    Axis *axisPtr;
    double x;
{
    register double norm;

    if (x == DBL_MAX) {
	norm = 1.0;
    } else if (x == -DBL_MAX) {
	norm = 0.0;
    } else {
	if (axisPtr->logScale) {
	    if (x > 0.0) {
		x = log10(x);
	    } else if (x < 0.0) {
		x = 0.0;
	    }
	}
	norm = NORMALIZE(axisPtr, x);
    }
    if (axisPtr->descending) {
	norm = 1.0 - norm;
    }
    /* Horizontal transformation */
    return ((norm * graphPtr->hRange) + graphPtr->hOffset);
}

/*
 * ----------------------------------------------------------------------
 *
 * VMap --
 *
 *	Map the given graph coordinate value to its axis, returning a
 *	window position.
 *
 * Results:
 *	Returns a double precision number representing the window
 *	coordinate position on the given axis.
 *
 * ----------------------------------------------------------------------
 */
static double
VMap(graphPtr, axisPtr, y)
    Graph *graphPtr;
    Axis *axisPtr;
    double y;
{
    register double norm;

    if (y == DBL_MAX) {
	norm = 1.0;
    } else if (y == -DBL_MAX) {
	norm = 0.0;
    } else {
	if (axisPtr->logScale) {
	    if (y > 0.0) {
		y = log10(y);
	    } else if (y < 0.0) {
		y = 0.0;
	    }
	}
	norm = NORMALIZE(axisPtr, y);
    }
    if (axisPtr->descending) {
	norm = 1.0 - norm;
    }
    /* Vertical transformation */
    return (((1.0 - norm) * graphPtr->vRange) + graphPtr->vOffset);
}

/*
 * ----------------------------------------------------------------------
 *
 * MapPoint --
 *
 *	Maps the given graph x,y coordinate values to a window position.
 *
 * Results:
 *	Returns a XPoint structure containing the window coordinates
 *	of the given graph x,y coordinate.
 *
 * ----------------------------------------------------------------------
 */
static Point2D
MapPoint(graphPtr, pointPtr, axesPtr)
    Graph *graphPtr;
    Point2D *pointPtr;		/* Graph X-Y coordinate. */
    Axis2D *axesPtr;		/* Specifies which axes to use */
{
    Point2D result;

    if (graphPtr->inverted) {
	result.x = HMap(graphPtr, axesPtr->y, pointPtr->y);
	result.y = VMap(graphPtr, axesPtr->x, pointPtr->x);
    } else {
	result.x = HMap(graphPtr, axesPtr->x, pointPtr->x);
	result.y = VMap(graphPtr, axesPtr->y, pointPtr->y);
    }
    return result;		/* Result is screen coordinate. */
}

static Marker *
CreateMarker(graphPtr, name, classUid)
    Graph *graphPtr;
    char *name;
    Blt_Uid classUid;    
{    
    Marker *markerPtr;

    /* Create the new marker based upon the given type */
    if (classUid == bltBitmapMarkerUid) {
	markerPtr = CreateBitmapMarker(); /* bitmap */
    } else if (classUid == bltLineMarkerUid) {
	markerPtr = CreateLineMarker(); /* line */
    } else if (classUid == bltImageMarkerUid) {
	markerPtr = CreateImageMarker(); /* image */
    } else if (classUid == bltTextMarkerUid) {
	markerPtr = CreateTextMarker(); /* text */
    } else if (classUid == bltPolygonMarkerUid) {
	markerPtr = CreatePolygonMarker(); /* polygon */
    } else if (classUid == bltWindowMarkerUid) {
	markerPtr = CreateWindowMarker(); /* window */
    } else {
	return NULL;
    }
    assert(markerPtr);
    markerPtr->graphPtr = graphPtr;
    markerPtr->hidden = markerPtr->drawUnder = FALSE;
    markerPtr->flags |= MAP_ITEM;
    markerPtr->name = Blt_Strdup(name);
    markerPtr->classUid = classUid;
    return markerPtr;
}

static void
DestroyMarker(markerPtr)
    Marker *markerPtr;
{
    Graph *graphPtr = markerPtr->graphPtr;

    if (markerPtr->drawUnder) {
	graphPtr->flags |= REDRAW_BACKING_STORE;
    }
    /* Free the resources allocated for the particular type of marker */
    (*markerPtr->classPtr->freeProc) (graphPtr, markerPtr);
    if (markerPtr->worldPts != NULL) {
	Blt_Free(markerPtr->worldPts);
    }
    Blt_DeleteBindings(graphPtr->bindTable, markerPtr);
    Tk_FreeOptions(markerPtr->classPtr->configSpecs, (char *)markerPtr,
	graphPtr->display, 0);
    if (markerPtr->hashPtr != NULL) {
	Blt_DeleteHashEntry(&graphPtr->markers.table, markerPtr->hashPtr);
    }
    if (markerPtr->linkPtr != NULL) {
	Blt_ChainDeleteLink(graphPtr->markers.displayList, markerPtr->linkPtr);
    }
    if (markerPtr->name != NULL) {
	Blt_Free(markerPtr->name);
    }
    if (markerPtr->elemName != NULL) {
	Blt_Free(markerPtr->elemName);
    }
    if (markerPtr->tags != NULL) {
	Blt_Free(markerPtr->tags);
    }
    Blt_Free(markerPtr);
}

/*
 * ----------------------------------------------------------------------
 *
 * ConfigureBitmapMarker --
 *
 *	This procedure is called to process an argv/argc list, plus
 *	the Tk option database, in order to configure (or reconfigure)
 *	a bitmap marker.
 *
 * Results:
 *	A standard Tcl result.  If TCL_ERROR is returned, then
 *	interp->result contains an error message.
 *
 * Side effects:
 *	Configuration information, such as bitmap pixmap, colors,
 *	rotation, etc. get set for markerPtr; old resources get freed,
 *	if there were any.  The marker is eventually redisplayed.
 *
 * ----------------------------------------------------------------------
 */
/* ARGSUSED */
static int
ConfigureBitmapMarker(markerPtr)
    Marker *markerPtr;
{
    Graph *graphPtr = markerPtr->graphPtr;
    BitmapMarker *bmPtr = (BitmapMarker *)markerPtr;
    GC newGC;
    XGCValues gcValues;
    unsigned long gcMask;

    if (bmPtr->srcBitmap == None) {
	return TCL_OK;
    }
    bmPtr->theta = FMOD(bmPtr->rotate, 360.0);
    if (bmPtr->theta < 0.0) {
	bmPtr->theta += 360.0;
    }
    gcMask = 0;
    if (bmPtr->outlineColor != NULL) {
	gcMask |= GCForeground;
	gcValues.foreground = bmPtr->outlineColor->pixel;
    }
    if (bmPtr->fillColor != NULL) {
	gcValues.background = bmPtr->fillColor->pixel;
	gcMask |= GCBackground;
    } else {
	gcValues.clip_mask = bmPtr->srcBitmap;
	gcMask |= GCClipMask;
    }

    /* Note that while this is a "shared" GC, we're going to change
     * the clip origin right before the bitmap is drawn anyways.  This
     * assumes that any drawing code using this GC (with GCClipMask
     * set) is going to want to set the clip origin anyways.  */
    newGC = Tk_GetGC(graphPtr->tkwin, gcMask, &gcValues);
    if (bmPtr->gc != NULL) {
	Tk_FreeGC(graphPtr->display, bmPtr->gc);
    }
    bmPtr->gc = newGC;

    /* Create background GC color */

    if (bmPtr->fillColor != NULL) {
	gcValues.foreground = bmPtr->fillColor->pixel;
	newGC = Tk_GetGC(graphPtr->tkwin, gcMask, &gcValues);
	if (bmPtr->fillGC != NULL) {
	    Tk_FreeGC(graphPtr->display, bmPtr->fillGC);
	}
	bmPtr->fillGC = newGC;
    }
    bmPtr->flags |= MAP_ITEM;
    if (bmPtr->drawUnder) {
	graphPtr->flags |= REDRAW_BACKING_STORE;
    }
    Blt_EventuallyRedrawGraph(graphPtr);
    return TCL_OK;
}

/*
 * ----------------------------------------------------------------------
 *
 * MapBitmapMarker --
 *
 * 	This procedure gets called each time the layout of the graph
 *	changes.  The x, y window coordinates of the bitmap marker are
 *	saved in the marker structure.
 *
 *	Additionly, if no background color was specified, the
 *	GCTileStipXOrigin and GCTileStipYOrigin attributes are set in
 *	the private GC.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Window coordinates are saved and if no background color was
 * 	set, the GC stipple origins are changed to calculated window
 *	coordinates.
 *
 * ----------------------------------------------------------------------
 */
static void
MapBitmapMarker(markerPtr)
    Marker *markerPtr;
{
    BitmapMarker *bmPtr = (BitmapMarker *)markerPtr;
    Extents2D exts;
    Graph *graphPtr = markerPtr->graphPtr;
    Point2D anchorPos;
    Point2D corner1, corner2;
    int destWidth, destHeight;
    int srcWidth, srcHeight;
    register int i;

    if (bmPtr->srcBitmap == None) {
	return;
    }
    if (bmPtr->destBitmap != None) {
	Tk_FreePixmap(graphPtr->display, bmPtr->destBitmap);
	bmPtr->destBitmap = None;
    }
    /* 
     * Collect the coordinates.  The number of coordinates will determine
     * the calculations to be made.
     * 
     *	   x1 y1	A single pair of X-Y coordinates.  They represent
     *			the anchor position of the bitmap.  
     *
     *	x1 y1 x2 y2	Two pairs of X-Y coordinates.  They represent
     *			two opposite corners of a bounding rectangle. The
     *			bitmap is possibly rotated and scaled to fit into
     *			this box.
     *
     */   
    Tk_SizeOfBitmap(graphPtr->display, bmPtr->srcBitmap, &srcWidth, 
		    &srcHeight);
    corner1 = MapPoint(graphPtr, bmPtr->worldPts, &bmPtr->axes);
    if (bmPtr->nWorldPts > 1) {
	double hold;

	corner2 = MapPoint(graphPtr, bmPtr->worldPts + 1, &bmPtr->axes);
	/* Flip the corners if necessary */
	if (corner1.x > corner2.x) {
	    hold = corner1.x, corner1.x = corner2.x, corner2.x = hold;
	}
	if (corner1.y > corner2.y) {
	    hold = corner1.y, corner1.y = corner2.y, corner2.y = hold;
	}
    } else {
	corner2.x = corner1.x + srcWidth - 1;
	corner2.y = corner1.y + srcHeight - 1;
    }
    destWidth = (int)(corner2.x - corner1.x) + 1;
    destHeight = (int)(corner2.y - corner1.y) + 1;

    if (bmPtr->nWorldPts == 1) {
	anchorPos = Blt_TranslatePoint(&corner1, destWidth, destHeight, 
		bmPtr->anchor);
    } else {
	anchorPos = corner1;
    }
    anchorPos.x += bmPtr->xOffset;
    anchorPos.y += bmPtr->yOffset;

    /* Check if the bitmap sits at least partially in the plot area. */
    exts.left = anchorPos.x;
    exts.top = anchorPos.y;
    exts.right = anchorPos.x + destWidth - 1;
    exts.bottom = anchorPos.y + destHeight - 1;

    bmPtr->clipped = BoxesDontOverlap(graphPtr, &exts);
    if (bmPtr->clipped) {
	return;			/* Bitmap is offscreen. Don't generate
				 * rotated or scaled bitmaps. */
    }

    /*  
     * Scale the bitmap if necessary. It's a little tricky because we
     * only want to scale what's visible on the screen, not the entire
     * bitmap.  
     */
    if ((bmPtr->theta != 0.0) || (destWidth != srcWidth) || 
	(destHeight != srcHeight)) {
	int regionWidth, regionHeight;
	Region2D region;	/* Indicates the portion of the scaled
				 * bitmap that we want to display. */
	double left, right, top, bottom;

	/*
	 * Determine the region of the bitmap visible in the plot area.
	 */
	left = MAX(graphPtr->left, exts.left);
	right = MIN(graphPtr->right, exts.right);
	top = MAX(graphPtr->top, exts.top);
	bottom = MIN(graphPtr->bottom, exts.bottom);

	region.left = region.top = 0;
	if (graphPtr->left > exts.left) {
	    region.left = (int)(graphPtr->left - exts.left);
	}
	if (graphPtr->top > exts.top) {
	    region.top = (int)(graphPtr->top - exts.top);
	}	    
	regionWidth = (int)(right - left) + 1;
	regionHeight = (int)(bottom - top) + 1;
	region.right = region.left + (int)(right - left);
	region.bottom = region.top + (int)(bottom - top);
	
	anchorPos.x = left;
	anchorPos.y = top;
	bmPtr->destBitmap = Blt_ScaleRotateBitmapRegion(graphPtr->tkwin, 
		bmPtr->srcBitmap, srcWidth, srcHeight, 
		region.left, region.top, regionWidth, regionHeight, 
		destWidth, destHeight, bmPtr->theta);
	bmPtr->destWidth = regionWidth;
	bmPtr->destHeight = regionHeight;
    } else {
	bmPtr->destWidth = srcWidth;
	bmPtr->destHeight = srcHeight;
	bmPtr->destBitmap = None;
    }
    bmPtr->anchorPos = anchorPos;
    {
	double xScale, yScale;
	double tx, ty;
	double rotWidth, rotHeight;
	Point2D polygon[5];
	int n;

	/* 
	 * Compute a polygon to represent the background area of the bitmap.  
	 * This is needed for backgrounds of arbitrarily rotated bitmaps.  
	 * We also use it to print a background in PostScript. 
	 */
	Blt_GetBoundingBox(srcWidth, srcHeight, bmPtr->theta, &rotWidth, 
			   &rotHeight, polygon);
	xScale = (double)destWidth / rotWidth;
	yScale = (double)destHeight / rotHeight;
	
	/* 
	 * Adjust each point of the polygon. Both scale it to the new size
	 * and translate it to the actual screen position of the bitmap.
	 */
	tx = exts.left + destWidth * 0.5;
	ty = exts.top + destHeight * 0.5;
	for (i = 0; i < 4; i++) {
	    polygon[i].x = (polygon[i].x * xScale) + tx;
	    polygon[i].y = (polygon[i].y * yScale) + ty;
	}
	Blt_GraphExtents(graphPtr, &exts);
	n = Blt_PolyRectClip(&exts, polygon, 4, bmPtr->outline); 
	assert(n <= MAX_OUTLINE_POINTS);
	if (n < 3) { 
	    memcpy(&bmPtr->outline, polygon, sizeof(Point2D) * 4);
	    bmPtr->nOutlinePts = 4;
	} else {
	    bmPtr->nOutlinePts = n;
	}
    }
}

/*
 * ----------------------------------------------------------------------
 *
 * PointInBitmapMarker --
 *
 *	Indicates if the given point is over the bitmap marker.  The
 *	area of the bitmap is the rectangle.
 *
 * Results:
 *	Returns 1 is the point is over the bitmap marker, 0 otherwise.
 *
 * ----------------------------------------------------------------------
 */
static int
PointInBitmapMarker(markerPtr, samplePtr)
    Marker *markerPtr;
    Point2D *samplePtr;
{
    BitmapMarker *bmPtr = (BitmapMarker *)markerPtr;

    if (bmPtr->srcBitmap == None) {
	return 0;
    }
    if (bmPtr->theta != 0.0) {
	Point2D points[MAX_OUTLINE_POINTS];
	register int i;

	/*  
	 * Generate the bounding polygon (isolateral) for the bitmap
	 * and see if the point is inside of it.  
	 */
	for (i = 0; i < bmPtr->nOutlinePts; i++) {
	    points[i].x = bmPtr->outline[i].x + bmPtr->anchorPos.x;
	    points[i].y = bmPtr->outline[i].y + bmPtr->anchorPos.y;
	}
	return Blt_PointInPolygon(samplePtr, points, bmPtr->nOutlinePts);
    }
    return ((samplePtr->x >= bmPtr->anchorPos.x) && 
	    (samplePtr->x < (bmPtr->anchorPos.x + bmPtr->destWidth)) &&
	    (samplePtr->y >= bmPtr->anchorPos.y) && 
	    (samplePtr->y < (bmPtr->anchorPos.y + bmPtr->destHeight)));
}


/*
 * ----------------------------------------------------------------------
 *
 * RegionInBitmapMarker --
 *
 * ----------------------------------------------------------------------
 */
static int
RegionInBitmapMarker(markerPtr, extsPtr, enclosed)
    Marker *markerPtr;
    Extents2D *extsPtr;
    int enclosed;
{
    BitmapMarker *bmPtr = (BitmapMarker *)markerPtr;

    if (bmPtr->nWorldPts < 1) {
	return FALSE;
    }
    if (bmPtr->theta != 0.0) {
	Point2D points[MAX_OUTLINE_POINTS];
	register int i;
	
	/*  
	 * Generate the bounding polygon (isolateral) for the bitmap
	 * and see if the point is inside of it.  
	 */
	for (i = 0; i < bmPtr->nOutlinePts; i++) {
	    points[i].x = bmPtr->outline[i].x + bmPtr->anchorPos.x;
	    points[i].y = bmPtr->outline[i].y + bmPtr->anchorPos.y;
	}
	return Blt_RegionInPolygon(extsPtr, points, bmPtr->nOutlinePts, 
		   enclosed);
    }
    if (enclosed) {
	return ((bmPtr->anchorPos.x >= extsPtr->left) &&
		(bmPtr->anchorPos.y >= extsPtr->top) && 
		((bmPtr->anchorPos.x + bmPtr->destWidth) <= extsPtr->right) &&
		((bmPtr->anchorPos.y + bmPtr->destHeight) <= extsPtr->bottom));
    }
    return !((bmPtr->anchorPos.x >= extsPtr->right) ||
	     (bmPtr->anchorPos.y >= extsPtr->bottom) ||
	     ((bmPtr->anchorPos.x + bmPtr->destWidth) <= extsPtr->left) ||
	     ((bmPtr->anchorPos.y + bmPtr->destHeight) <= extsPtr->top));
}

/*
 * ----------------------------------------------------------------------
 *
 * DrawBitmapMarker --
 *
 *	Draws the bitmap marker that have a transparent of filled
 *	background.  
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	GC stipple origins are changed to current window coordinates.
 *	Commands are output to X to draw the marker in its current
 *	mode.
 *
 * ----------------------------------------------------------------------
 */
static void
DrawBitmapMarker(markerPtr, drawable)
    Marker *markerPtr;
    Drawable drawable;		/* Pixmap or window to draw into */
{
    Graph *graphPtr = markerPtr->graphPtr;
    BitmapMarker *bmPtr = (BitmapMarker *)markerPtr;
    double theta;
    Pixmap bitmap;

    bitmap = GETBITMAP(bmPtr);
    if ((bitmap == None) || (bmPtr->destWidth < 1) || (bmPtr->destHeight < 1)) {
	return;
    }
    theta = FMOD(bmPtr->theta, (double)90.0);
    if ((bmPtr->fillColor == NULL) || (theta != 0.0)) {

	/* 
	 * If the bitmap is rotated and a filled background is
	 * required, then a filled polygon is drawn before the
	 * bitmap. 
	 */

	if (bmPtr->fillColor != NULL) {
	    int i;
	    XPoint polygon[MAX_OUTLINE_POINTS];

	    for (i = 0; i < bmPtr->nOutlinePts; i++) {
		polygon[i].x = (short int)bmPtr->outline[i].x;
		polygon[i].y = (short int)bmPtr->outline[i].y;
	    }
	    XFillPolygon(graphPtr->display, drawable, bmPtr->fillGC,
		 polygon, bmPtr->nOutlinePts, Convex, CoordModeOrigin);
	}
	XSetClipMask(graphPtr->display, bmPtr->gc, bitmap);
	XSetClipOrigin(graphPtr->display, bmPtr->gc, (int)bmPtr->anchorPos.x, 
	       (int)bmPtr->anchorPos.y);
    } else {
	XSetClipMask(graphPtr->display, bmPtr->gc, None);
	XSetClipOrigin(graphPtr->display, bmPtr->gc, 0, 0);
    }
    XCopyPlane(graphPtr->display, bitmap, drawable, bmPtr->gc, 0, 0,
	bmPtr->destWidth, bmPtr->destHeight, (int)bmPtr->anchorPos.x, 
	(int)bmPtr->anchorPos.y, 1);
}

/*
 * ----------------------------------------------------------------------
 *
 * BitmapMarkerToPostScript --
 *
 *	Generates PostScript to print a bitmap marker.
 *
 * Results:
 *	None.
 *
 * ----------------------------------------------------------------------
 */
static void
BitmapMarkerToPostScript(markerPtr, psToken)
    Marker *markerPtr;		/* Marker to be printed */
    PsToken psToken;
{
    Graph *graphPtr = markerPtr->graphPtr;
    BitmapMarker *bmPtr = (BitmapMarker *)markerPtr;
    Pixmap bitmap;

    bitmap = GETBITMAP(bmPtr);
    if (bitmap == None) {
	return;
    }
    if (bmPtr->fillColor != NULL) {
	Blt_BackgroundToPostScript(psToken, bmPtr->fillColor);
	Blt_PolygonToPostScript(psToken, bmPtr->outline, 4);
    }
    Blt_ForegroundToPostScript(psToken, bmPtr->outlineColor);

    Blt_FormatToPostScript(psToken,
	"  gsave\n    %g %g translate\n    %d %d scale\n", 
	   bmPtr->anchorPos.x, bmPtr->anchorPos.y + bmPtr->destHeight, 
	   bmPtr->destWidth, -bmPtr->destHeight);
    Blt_FormatToPostScript(psToken, "    %d %d true [%d 0 0 %d 0 %d] {",
	bmPtr->destWidth, bmPtr->destHeight, bmPtr->destWidth, 
	-bmPtr->destHeight, bmPtr->destHeight);
    Blt_BitmapDataToPostScript(psToken, graphPtr->display, bitmap,
	bmPtr->destWidth, bmPtr->destHeight);
    Blt_AppendToPostScript(psToken, "    } imagemask\n",
	"grestore\n", (char *)NULL);
}

/*
 * ----------------------------------------------------------------------
 *
 * FreeBitmapMarker --
 *
 *	Releases the memory and attributes of the bitmap marker.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Bitmap attributes (GCs, colors, bitmap, etc) get destroyed.
 *	Memory is released, X resources are freed, and the graph is
 *	redrawn.
 *
 * ----------------------------------------------------------------------
 */
static void
FreeBitmapMarker(graphPtr, markerPtr)
    Graph *graphPtr;
    Marker *markerPtr;
{
    BitmapMarker *bmPtr = (BitmapMarker *)markerPtr;

    if (bmPtr->gc != NULL) {
	Tk_FreeGC(graphPtr->display, bmPtr->gc);
    }
    if (bmPtr->fillGC != NULL) {
	Tk_FreeGC(graphPtr->display, bmPtr->fillGC);
    }
    if (bmPtr->destBitmap != None) {
	Tk_FreePixmap(graphPtr->display, bmPtr->destBitmap);
    }
}

/*
 * ----------------------------------------------------------------------
 *
 * CreateBitmapMarker --
 *
 *	Allocate memory and initialize methods for the new bitmap marker.
 *
 * Results:
 *	The pointer to the newly allocated marker structure is returned.
 *
 * Side effects:
 *	Memory is allocated for the bitmap marker structure.
 *
 * ----------------------------------------------------------------------
 */
static Marker *
CreateBitmapMarker()
{
    BitmapMarker *bmPtr;

    bmPtr = Blt_Calloc(1, sizeof(BitmapMarker));
    if (bmPtr != NULL) {
	bmPtr->classPtr = &bitmapMarkerClass;
    }
    return (Marker *)bmPtr;
}

/*
 *----------------------------------------------------------------------
 *
 * ImageChangedProc
 *
 *
 * Results:
 *	None.
 *
 *----------------------------------------------------------------------
 */
/* ARGSUSED */
static void
ImageChangedProc(clientData, x, y, width, height, imageWidth, imageHeight)
    ClientData clientData;
    int x, y, width, height;	/* Not used. */
    int imageWidth, imageHeight; /* Not used. */
{
    ImageMarker *imPtr = clientData;
    Tk_PhotoHandle photo;
    
    photo = Blt_FindPhoto(imPtr->graphPtr->interp, imPtr->imageName);
    if (photo != NULL) {
	if (imPtr->srcImage != NULL) {
	    Blt_FreeColorImage(imPtr->srcImage);
	}
	/* Convert the latest incarnation of the photo image back to a
	 * color image that we can scale. */
	imPtr->srcImage = Blt_PhotoToColorImage(photo);
    }
    imPtr->graphPtr->flags |= REDRAW_BACKING_STORE;
    imPtr->flags |= MAP_ITEM;
    Blt_EventuallyRedrawGraph(imPtr->graphPtr);
}

/*
 * ----------------------------------------------------------------------
 *
 * ConfigureImageMarker --
 *
 *	This procedure is called to process an argv/argc list, plus
 *	the Tk option database, in order to configure (or reconfigure)
 *	a image marker.
 *
 * Results:
 *	A standard Tcl result.  If TCL_ERROR is returned, then
 *	interp->result contains an error message.
 *
 * Side effects:
 *	Configuration information, such as image pixmap, colors,
 *	rotation, etc. get set for markerPtr; old resources get freed,
 *	if there were any.  The marker is eventually redisplayed.
 *
 * ----------------------------------------------------------------------
 */
static int
ConfigureImageMarker(markerPtr)
    Marker *markerPtr;
{
    ImageMarker *imPtr = (ImageMarker *)markerPtr;
    Graph *graphPtr = markerPtr->graphPtr;

    if (Blt_ConfigModified(markerPtr->classPtr->configSpecs, "-image", 
			   (char *)NULL)) {
	Tcl_Interp *interp = graphPtr->interp;

	if (imPtr->tkImage != NULL) {
	    Tk_FreeImage(imPtr->tkImage);
	    imPtr->tkImage = NULL;
	}
	if (imPtr->imageName[0] != '\0') {
	    GC newGC;
	    Tk_PhotoHandle photo;

	    imPtr->tkImage = Tk_GetImage(interp, graphPtr->tkwin,
		imPtr->imageName, ImageChangedProc, imPtr);
	    if (imPtr->tkImage == NULL) {
		Blt_Free(imPtr->imageName);
		imPtr->imageName = NULL;
		return TCL_ERROR;
	    }
	    photo = Blt_FindPhoto(interp, imPtr->imageName);
	    if (photo != NULL) {
		if (imPtr->srcImage != NULL) {
		    Blt_FreeColorImage(imPtr->srcImage);
		}
		/* Convert the photo into a color image */
		imPtr->srcImage = Blt_PhotoToColorImage(photo);
	    }
	    newGC = Tk_GetGC(graphPtr->tkwin, 0L, (XGCValues *)NULL);
	    if (imPtr->gc != NULL) {
		Tk_FreeGC(graphPtr->display, imPtr->gc);
	    }
	    imPtr->gc = newGC;
	}
    }
    imPtr->flags |= MAP_ITEM;
    if (imPtr->drawUnder) {
	graphPtr->flags |= REDRAW_BACKING_STORE;
    }
    Blt_EventuallyRedrawGraph(graphPtr);
    return TCL_OK;
}

/*
 * ----------------------------------------------------------------------
 *
 * MapImageMarker --
 *
 * 	This procedure gets called each time the layout of the graph
 *	changes.  The x, y window coordinates of the image marker are
 *	saved in the marker structure.
 *
 *	Additionly, if no background color was specified, the
 *	GCTileStipXOrigin and GCTileStipYOrigin attributes are set in
 *	the private GC.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Window coordinates are saved and if no background color was *
 *	set, the GC stipple origins are changed to calculated window
 *	coordinates.
 *
 * ----------------------------------------------------------------------
 */
static void
MapImageMarker(markerPtr)
    Marker *markerPtr;
{
    Extents2D exts;
    Graph *graphPtr;
    ImageMarker *imPtr;
    Point2D anchorPos;
    Point2D corner1, corner2;
    int scaledWidth, scaledHeight;
    int srcWidth, srcHeight;

    imPtr = (ImageMarker *)markerPtr;
    if (imPtr->tkImage == NULL) {
	return;
    }
    graphPtr = imPtr->graphPtr;
    corner1 = MapPoint(graphPtr, imPtr->worldPts, &imPtr->axes);
    if (imPtr->srcImage == NULL) {
	/* 
	 * Don't scale or rotate non-photo images.
	 */
	Tk_SizeOfImage(imPtr->tkImage, &srcWidth, &srcHeight);
	imPtr->width = srcWidth;
	imPtr->height = srcHeight;
	imPtr->anchorPos.x = corner1.x + imPtr->xOffset;
	imPtr->anchorPos.y = corner1.y + imPtr->yOffset;
	exts.left = imPtr->anchorPos.x;
	exts.top = imPtr->anchorPos.y;
	exts.right = exts.left + srcWidth - 1;
	exts.bottom = exts.top + srcHeight - 1;
	imPtr->clipped = BoxesDontOverlap(graphPtr, &exts);
	return;
    }
	
    imPtr->width = srcWidth = Blt_ColorImageWidth(imPtr->srcImage);
    imPtr->height = srcHeight = Blt_ColorImageHeight(imPtr->srcImage);
    if ((srcWidth == 0) && (srcHeight == 0)) {
	imPtr->clipped = TRUE;
	return;			/* Empty image. */
    }
    if (imPtr->nWorldPts > 1) {
	double hold;

	corner2 = MapPoint(graphPtr, imPtr->worldPts + 1, &imPtr->axes);
	/* Flip the corners if necessary */
	if (corner1.x > corner2.x) {
	    hold = corner1.x, corner1.x = corner2.x, corner2.x = hold;
	}
	if (corner1.y > corner2.y) {
	    hold = corner1.y, corner1.y = corner2.y, corner2.y = hold;
	}
    } else {
	corner2.x = corner1.x + srcWidth - 1;
	corner2.y = corner1.y + srcHeight - 1;
    }
    scaledWidth = (int)(corner2.x - corner1.x) + 1;
    scaledHeight = (int)(corner2.y - corner1.y) + 1;

    if (imPtr->nWorldPts == 1) {
	anchorPos = Blt_TranslatePoint(&corner1, scaledWidth, scaledHeight, 
		imPtr->anchor);
    } else {
	anchorPos = corner1;
    }
    anchorPos.x += imPtr->xOffset;
    anchorPos.y += imPtr->yOffset;

    /* Check if the image sits at least partially in the plot area. */
    exts.left = anchorPos.x;
    exts.top = anchorPos.y;
    exts.right = anchorPos.x + scaledWidth - 1;
    exts.bottom = anchorPos.y + scaledHeight - 1;

    imPtr->clipped = BoxesDontOverlap(graphPtr, &exts);
    if (imPtr->clipped) {
	return;			/* Image is offscreen. Don't generate
				 * rotated or scaled images. */
    }
    if ((scaledWidth != srcWidth) || (scaledHeight != srcHeight)) {
	Tk_PhotoHandle photo;
	Blt_ColorImage destImage;
	int x, y, width, height;
	int left, right, top, bottom;

	/* Determine the region of the subimage inside of the
	 * destination image. */
 	left = MAX((int)exts.left, graphPtr->left);
	top = MAX((int)exts.top, graphPtr->top);
	right = MIN((int)exts.right, graphPtr->right);
	bottom = MIN((int)exts.bottom, graphPtr->bottom);

	/* Reset image location and coordinates to that of the region */
	anchorPos.x = left;
	anchorPos.y = top;

	x = y = 0;
	if (graphPtr->left > (int)exts.left) {
	    x = graphPtr->left - (int)exts.left;
	} 
	if (graphPtr->top > (int)exts.top) {
	    y = graphPtr->top - (int)exts.top;
	} 
	width = (int)(right - left + 1);
	height = (int)(bottom - top + 1);

	destImage = Blt_ResizeColorSubimage(imPtr->srcImage, x, y, width, 
		height, scaledWidth, scaledHeight);
#ifdef notyet
	/* Now convert the color image into a pixmap */
	if (imPtr->pixmap != None) {
	    Blt_FreeColorTable(imPtr->colorTable);
	    Tk_FreePixmap(Tk_Display(graphPtr->tkwin), imPtr->pixmap);
	    imPtr->colorTable = NULL;
	}
	imPtr->pixmap = Blt_ColorImageToPixmap(graphPtr->interp,
	    graphPtr->tkwin, destImage, &imPtr->colorTable);
#else
	imPtr->pixmap = None;
	if (imPtr->tmpImage == NULL) {
	    imPtr->tmpImage = Blt_CreateTemporaryImage(graphPtr->interp, 
		     graphPtr->tkwin, imPtr);
	    if (imPtr->tmpImage == NULL) {
		return;
	    }
	}	    
	/* Put the scaled colorimage into the photo. */
	photo = Blt_FindPhoto(graphPtr->interp, 
			      Blt_NameOfImage(imPtr->tmpImage));
	Blt_ColorImageToPhoto(destImage, photo);
#endif
	Blt_FreeColorImage(destImage);
	imPtr->width = width;
	imPtr->height = height;
    }
    imPtr->anchorPos = anchorPos;
}

/*
 * ----------------------------------------------------------------------
 *
 * PointInWindowMarker --
 *
 *	Indicates if the given point is over the window marker.  The
 *	area of the window is the rectangle.
 *
 * Results:
 *	Returns 1 is the point is over the window marker, 0 otherwise.
 *
 * ----------------------------------------------------------------------
 */
static int
PointInImageMarker(markerPtr, samplePtr)
    Marker *markerPtr;
    Point2D *samplePtr;
{
    ImageMarker *imPtr = (ImageMarker *)markerPtr;

    return ((samplePtr->x >= imPtr->anchorPos.x) && 
	    (samplePtr->x < (imPtr->anchorPos.x + imPtr->width)) &&
	    (samplePtr->y >= imPtr->anchorPos.y) && 
	    (samplePtr->y < (imPtr->anchorPos.y + imPtr->height)));
}

/*
 * ----------------------------------------------------------------------
 *
 * RegionInImageMarker --
 *
 * ----------------------------------------------------------------------
 */
static int
RegionInImageMarker(markerPtr, extsPtr, enclosed)
    Marker *markerPtr;
    Extents2D *extsPtr;
    int enclosed;
{
    ImageMarker *imPtr = (ImageMarker *)markerPtr;

    if (imPtr->nWorldPts < 1) {
	return FALSE;
    }
    if (enclosed) {
	return ((imPtr->anchorPos.x >= extsPtr->left) &&
		(imPtr->anchorPos.y >= extsPtr->top) && 
		((imPtr->anchorPos.x + imPtr->width) <= extsPtr->right) &&
		((imPtr->anchorPos.y + imPtr->height) <= extsPtr->bottom));
    } 
    return !((imPtr->anchorPos.x >= extsPtr->right) ||
	     (imPtr->anchorPos.y >= extsPtr->bottom) ||
	     ((imPtr->anchorPos.x + imPtr->width) <= extsPtr->left) ||
	     ((imPtr->anchorPos.y + imPtr->height) <= extsPtr->top));
}

/*
 * ----------------------------------------------------------------------
 *
 * DrawImageMarker --
 *
 *	This procedure is invoked to draw a image marker.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	GC stipple origins are changed to current window coordinates.
 *	Commands are output to X to draw the marker in its current mode.
 *
 * ----------------------------------------------------------------------
 */
static void
DrawImageMarker(markerPtr, drawable)
    Marker *markerPtr;
    Drawable drawable;		/* Pixmap or window to draw into */
{
    ImageMarker *imPtr = (ImageMarker *)markerPtr;
    int width, height;

    if ((imPtr->tkImage == NULL) || (Tk_ImageIsDeleted(imPtr->tkImage))) {
	return;
    }
    if (imPtr->pixmap == None) {
	Pixmap pixmap;
	Tk_Image tkImage;

	tkImage = (imPtr->tmpImage != NULL) ? imPtr->tmpImage : imPtr->tkImage;
	Tk_SizeOfImage(tkImage, &width, &height);
	/* pixmap = Tk_ImageGetPhotoPixmap(tkImage); */
	pixmap = None;
	if (pixmap == None) {	/* May not be a "photo" image. */
	    Tk_RedrawImage(tkImage, 0, 0, width, height, drawable,
			   (int)imPtr->anchorPos.x, (int)imPtr->anchorPos.y);
	} else {
	    XCopyArea(imPtr->graphPtr->display, pixmap, drawable,
		      imPtr->gc, 0, 0, width, height, (int)imPtr->anchorPos.x, 
			(int)imPtr->anchorPos.y);
	}
    } else {
	XCopyArea(imPtr->graphPtr->display, imPtr->pixmap, drawable,
	    imPtr->gc, 0, 0, imPtr->width, imPtr->height,
	    (int)imPtr->anchorPos.x, (int)imPtr->anchorPos.y);
    }
}

/*
 * ----------------------------------------------------------------------
 *
 * ImageMarkerToPostScript --
 *
 *	This procedure is invoked to print a image marker.
 *
 * Results:
 *	None.
 *
 * ----------------------------------------------------------------------
 */
static void
ImageMarkerToPostScript(markerPtr, psToken)
    Marker *markerPtr;		/* Marker to be printed */
    PsToken psToken;
{
    ImageMarker *imPtr = (ImageMarker *)markerPtr;
    char *imageName;
    Tk_PhotoHandle photo;

    if ((imPtr->tkImage == NULL) || (Tk_ImageIsDeleted(imPtr->tkImage))) {
	return;			/* Image doesn't exist anymore */
    }
    imageName = (imPtr->tmpImage == NULL) 
	? Blt_NameOfImage(imPtr->tkImage) : Blt_NameOfImage(imPtr->tmpImage);
    photo = Blt_FindPhoto(markerPtr->graphPtr->interp, imageName);
    if (photo == NULL) {
	return;			/* Image isn't a photo image */
    }
    Blt_PhotoToPostScript(psToken, photo, imPtr->anchorPos.x, 
			  imPtr->anchorPos.y);
}

/*
 * ----------------------------------------------------------------------
 *
 * FreeImageMarker --
 *
 *	Destroys the structure containing the attributes of the image
 * 	marker.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Image attributes (GCs, colors, image, etc) get destroyed.
 *	Memory is released, X resources are freed, and the graph is
 *	redrawn.
 *
 * ----------------------------------------------------------------------
 */
static void
FreeImageMarker(graphPtr, markerPtr)
    Graph *graphPtr;
    Marker *markerPtr;
{
    ImageMarker *imPtr = (ImageMarker *)markerPtr;

    if (imPtr->pixmap != None) {
	Tk_FreePixmap(graphPtr->display, imPtr->pixmap);
    }
    if (imPtr->tkImage != NULL) {
	Tk_FreeImage(imPtr->tkImage);
    }
    if (imPtr->tmpImage != NULL) {
	Blt_DestroyTemporaryImage(graphPtr->interp, imPtr->tmpImage);
    }	
    if (imPtr->srcImage != NULL) {
	Blt_FreeColorImage(imPtr->srcImage);
    }
    if (imPtr->gc != NULL) {
	Tk_FreeGC(graphPtr->display, imPtr->gc);
    }
}

/*
 * ----------------------------------------------------------------------
 *
 * CreateImageMarker --
 *
 *	Allocate memory and initialize methods for the new image marker.
 *
 * Results:
 *	The pointer to the newly allocated marker structure is returned.
 *
 * Side effects:
 *	Memory is allocated for the image marker structure.
 *
 * ----------------------------------------------------------------------
 */
static Marker *
CreateImageMarker()
{
    ImageMarker *imPtr;

    imPtr = Blt_Calloc(1, sizeof(ImageMarker));
    if (imPtr != NULL) {
	imPtr->classPtr = &imageMarkerClass;
    }
    return (Marker *)imPtr;
}

/*
 * ----------------------------------------------------------------------
 *
 * ConfigureTextMarker --
 *
 *	This procedure is called to process an argv/argc list, plus
 *	the Tk option database, in order to configure (or
 *	reconfigure) a text marker.
 *
 * Results:
 *	A standard Tcl result.  If TCL_ERROR is returned, then
 *	interp->result contains an error message.
 *
 * Side effects:
 *	Configuration information, such as text string, colors, font,
 *	etc. get set for markerPtr;  old resources get freed, if there
 *	were any.  The marker is eventually redisplayed.
 *
 * ----------------------------------------------------------------------
 */
static int
ConfigureTextMarker(markerPtr)
    Marker *markerPtr;
{
    Graph *graphPtr = markerPtr->graphPtr;
    TextMarker *tmPtr = (TextMarker *)markerPtr;
    GC newGC;
    XGCValues gcValues;
    unsigned long gcMask;

    tmPtr->style.theta = FMOD(tmPtr->style.theta, 360.0);
    if (tmPtr->style.theta < 0.0) {
	tmPtr->style.theta += 360.0;
    }
    newGC = NULL;
    if (tmPtr->fillColor != NULL) {
	gcMask = GCForeground;
	gcValues.foreground = tmPtr->fillColor->pixel;
	newGC = Tk_GetGC(graphPtr->tkwin, gcMask, &gcValues);
    }
    if (tmPtr->fillGC != NULL) {
	Tk_FreeGC(graphPtr->display, tmPtr->fillGC);
    }
    tmPtr->fillGC = newGC;
    Blt_ResetTextStyle(graphPtr->tkwin, &tmPtr->style);

    if (Blt_ConfigModified(tmPtr->classPtr->configSpecs, "-text", 
	(char *)NULL)) {
	if (tmPtr->textPtr != NULL) {
	    Blt_Free(tmPtr->textPtr);
	    tmPtr->textPtr = NULL;
	}
	tmPtr->width = tmPtr->height = 0;
	if (tmPtr->string != NULL) {
	    register int i;
	    double rotWidth, rotHeight;

	    tmPtr->textPtr = Blt_GetTextLayout(tmPtr->string, &tmPtr->style);
	    Blt_GetBoundingBox(tmPtr->textPtr->width, tmPtr->textPtr->height, 
	       tmPtr->style.theta, &rotWidth, &rotHeight, tmPtr->outline);
	    tmPtr->width = ROUND(rotWidth);
	    tmPtr->height = ROUND(rotHeight);
	    for (i = 0; i < 4; i++) {
		tmPtr->outline[i].x += ROUND(rotWidth * 0.5);
		tmPtr->outline[i].y += ROUND(rotHeight * 0.5);
	    }
	    tmPtr->outline[4].x = tmPtr->outline[0].x;
	    tmPtr->outline[4].y = tmPtr->outline[0].y;
	}
    }
    tmPtr->flags |= MAP_ITEM;
    if (tmPtr->drawUnder) {
	graphPtr->flags |= REDRAW_BACKING_STORE;
    }
    Blt_EventuallyRedrawGraph(graphPtr);
    return TCL_OK;
}

/*
 * ----------------------------------------------------------------------
 *
 * MapTextMarker --
 *
 *	Calculate the layout position for a text marker.  Positional
 *	information is saved in the marker.  If the text is rotated,
 *	a bitmap containing the text is created.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	If no background color has been specified, the GC stipple
 *	origins are changed to current window coordinates. For both
 *	rotated and non-rotated text, if any old bitmap is leftover,
 *	it is freed.
 *
 * ----------------------------------------------------------------------
 */
static void
MapTextMarker(markerPtr)
    Marker *markerPtr;
{
    Graph *graphPtr = markerPtr->graphPtr;
    TextMarker *tmPtr = (TextMarker *)markerPtr;
    Extents2D exts;
    Point2D anchorPos;

    if (tmPtr->string == NULL) {
	return;
    }
    anchorPos = MapPoint(graphPtr, tmPtr->worldPts, &tmPtr->axes);
    anchorPos = Blt_TranslatePoint(&anchorPos, tmPtr->width, tmPtr->height, 
	tmPtr->anchor);
    anchorPos.x += tmPtr->xOffset;
    anchorPos.y += tmPtr->yOffset;
    /*
     * Determine the bounding box of the text and test to see if it
     * is at least partially contained within the plotting area.
     */
    exts.left = anchorPos.x;
    exts.top = anchorPos.y;
    exts.right = anchorPos.x + tmPtr->width - 1;
    exts.bottom = anchorPos.y + tmPtr->height - 1;
    tmPtr->clipped = BoxesDontOverlap(graphPtr, &exts);
    tmPtr->anchorPos = anchorPos;

}

static int
PointInTextMarker(markerPtr, samplePtr)
    Marker *markerPtr;
    Point2D *samplePtr;
{
    TextMarker *tmPtr = (TextMarker *)markerPtr;

    if (tmPtr->string == NULL) {
	return 0;
    }
    if (tmPtr->style.theta != 0.0) {
	Point2D points[5];
	register int i;

	/* 
	 * Figure out the bounding polygon (isolateral) for the text
	 * and see if the point is inside of it.
	 */

	for (i = 0; i < 5; i++) {
	    points[i].x = tmPtr->outline[i].x + tmPtr->anchorPos.x;
	    points[i].y = tmPtr->outline[i].y + tmPtr->anchorPos.y;
	}
	return Blt_PointInPolygon(samplePtr, points, 5);
    } 
    return ((samplePtr->x >= tmPtr->anchorPos.x) && 
	    (samplePtr->x < (tmPtr->anchorPos.x + tmPtr->width)) &&
	    (samplePtr->y >= tmPtr->anchorPos.y) && 
	    (samplePtr->y < (tmPtr->anchorPos.y + tmPtr->height)));
}

/*
 * ----------------------------------------------------------------------
 *
 * RegionInTextMarker --
 *
 * ----------------------------------------------------------------------
 */
static int
RegionInTextMarker(markerPtr, extsPtr, enclosed)
    Marker *markerPtr;
    Extents2D *extsPtr;
    int enclosed;
{
    TextMarker *tmPtr = (TextMarker *)markerPtr;

    if (tmPtr->nWorldPts < 1) {
	return FALSE;
    }
    if (tmPtr->style.theta != 0.0) {
	Point2D points[5];
	register int i;
	
	/*  
	 * Generate the bounding polygon (isolateral) for the bitmap
	 * and see if the point is inside of it.  
	 */
	for (i = 0; i < 4; i++) {
	    points[i].x = tmPtr->outline[i].x + tmPtr->anchorPos.x;
	    points[i].y = tmPtr->outline[i].y + tmPtr->anchorPos.y;
	}
	return Blt_RegionInPolygon(extsPtr, points, 4, enclosed);
    } 
    if (enclosed) {
	return ((tmPtr->anchorPos.x >= extsPtr->left) &&
		(tmPtr->anchorPos.y >= extsPtr->top) && 
		((tmPtr->anchorPos.x + tmPtr->width) <= extsPtr->right) &&
		((tmPtr->anchorPos.y + tmPtr->height) <= extsPtr->bottom));
    }
    return !((tmPtr->anchorPos.x >= extsPtr->right) ||
	     (tmPtr->anchorPos.y >= extsPtr->bottom) ||
	     ((tmPtr->anchorPos.x + tmPtr->width) <= extsPtr->left) ||
	     ((tmPtr->anchorPos.y + tmPtr->height) <= extsPtr->top));
}

/*
 * ----------------------------------------------------------------------
 *
 * DrawTextMarker --
 *
 *	Draws the text marker on the graph.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Commands are output to X to draw the marker in its current
 *	mode.
 *
 * ----------------------------------------------------------------------
 */
static void
DrawTextMarker(markerPtr, drawable)
    Marker *markerPtr;
    Drawable drawable;		/* Pixmap or window to draw into */
{
    TextMarker *tmPtr = (TextMarker *)markerPtr;
    Graph *graphPtr = markerPtr->graphPtr;

    if (tmPtr->string == NULL) {
	return;
    }
    if (tmPtr->fillGC != NULL) {
	XPoint pointArr[4];
	register int i;

	/*
	 * Simulate the rotated background of the bitmap by
	 * filling a bounding polygon with the background color.
	 */
	for (i = 0; i < 4; i++) {
	    pointArr[i].x = (short int)
		(tmPtr->outline[i].x + tmPtr->anchorPos.x);
	    pointArr[i].y = (short int)
		(tmPtr->outline[i].y + tmPtr->anchorPos.y);
	}
	XFillPolygon(graphPtr->display, drawable, tmPtr->fillGC, pointArr, 4,
	    Convex, CoordModeOrigin);
    }
    if (tmPtr->style.color != NULL) {
	Blt_DrawTextLayout(graphPtr->tkwin, drawable, tmPtr->textPtr,
	    &tmPtr->style, (int)tmPtr->anchorPos.x, (int)tmPtr->anchorPos.y);
    }
}

/*
 * ----------------------------------------------------------------------
 *
 * TextMarkerToPostScript --
 *
 *	Outputs PostScript commands to draw a text marker at a given
 *	x,y coordinate, rotation, anchor, and font.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	PostScript font and color settings are changed.
 *
 * ----------------------------------------------------------------------
 */
static void
TextMarkerToPostScript(markerPtr, psToken)
    Marker *markerPtr;
    PsToken psToken;
{
    TextMarker *tmPtr = (TextMarker *)markerPtr;

    if (tmPtr->string == NULL) {
	return;
    }
    if (tmPtr->fillGC != NULL) {
	Point2D polygon[4];
	register int i;

	/*
	 * Simulate the rotated background of the bitmap by
	 * filling a bounding polygon with the background color.
	 */
	for (i = 0; i < 4; i++) {
	    polygon[i].x = tmPtr->outline[i].x + tmPtr->anchorPos.x;
	    polygon[i].y = tmPtr->outline[i].y + tmPtr->anchorPos.y;
	}
	Blt_BackgroundToPostScript(psToken, tmPtr->fillColor);
	Blt_PolygonToPostScript(psToken, polygon, 4);
    }
    Blt_TextToPostScript(psToken, tmPtr->string, &tmPtr->style, 
		 tmPtr->anchorPos.x, tmPtr->anchorPos.y);
}

/*
 * ----------------------------------------------------------------------
 *
 * FreeTextMarker --
 *
 *	Destroys the structure containing the attributes of the text
 * 	marker.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Text attributes (GCs, colors, stipple, font, etc) get destroyed.
 *	Memory is released, X resources are freed, and the graph is
 *	redrawn.
 *
 * ----------------------------------------------------------------------
 */
static void
FreeTextMarker(graphPtr, markerPtr)
    Graph *graphPtr;
    Marker *markerPtr;
{
    TextMarker *tmPtr = (TextMarker *)markerPtr;

    Blt_FreeTextStyle(graphPtr->display, &tmPtr->style);
    if (tmPtr->textPtr != NULL) {
	Blt_Free(tmPtr->textPtr);
    }
}

/*
 * ----------------------------------------------------------------------
 *
 * CreateTextMarker --
 *
 *	Allocate memory and initialize methods for the new text marker.
 *
 * Results:
 *	The pointer to the newly allocated marker structure is returned.
 *
 * Side effects:
 *	Memory is allocated for the text marker structure.
 *
 * ----------------------------------------------------------------------
 */
static Marker *
CreateTextMarker()
{
    TextMarker *tmPtr;

    tmPtr = Blt_Calloc(1, sizeof(TextMarker));
    assert(tmPtr);

    tmPtr->classPtr = &textMarkerClass;
    Blt_InitTextStyle(&tmPtr->style);
    tmPtr->style.anchor = TK_ANCHOR_NW;
    tmPtr->style.padLeft = tmPtr->style.padRight = 4;
    tmPtr->style.padTop = tmPtr->style.padBottom = 4;

    return (Marker *)tmPtr;
}


static void ChildEventProc _ANSI_ARGS_((ClientData clientData,
	XEvent *eventPtr));
static void ChildGeometryProc _ANSI_ARGS_((ClientData clientData,
	Tk_Window tkwin));

static void ChildCustodyProc _ANSI_ARGS_((ClientData clientData,
	Tk_Window tkwin));

static Tk_GeomMgr winMarkerMgrInfo =
{
    "graph",			/* Name of geometry manager used by winfo */
    ChildGeometryProc,		/* Procedure to for new geometry requests */
    ChildCustodyProc,		/* Procedure when window is taken away */
};

/*
 * ----------------------------------------------------------------------
 *
 * ConfigureWindowMarker --
 *
 *	This procedure is called to process an argv/argc list, plus
 *	the Tk option database, in order to configure (or reconfigure)
 *	a window marker.
 *
 * Results:
 *	A standard Tcl result.  If TCL_ERROR is returned, then
 *	interp->result contains an error message.
 *
 * Side effects:
 *	Configuration information, such as window pathname, placement,
 *	etc. get set for markerPtr; old resources get freed, if there
 *	were any.  The marker is eventually redisplayed.
 *
 * ----------------------------------------------------------------------
 */
static int
ConfigureWindowMarker(markerPtr)
    Marker *markerPtr;
{
    Graph *graphPtr = markerPtr->graphPtr;
    WindowMarker *wmPtr = (WindowMarker *)markerPtr;
    Tk_Window tkwin;

    if (wmPtr->pathName == NULL) {
	return TCL_OK;
    }
    tkwin = Tk_NameToWindow(graphPtr->interp, wmPtr->pathName, 
	    graphPtr->tkwin);
    if (tkwin == NULL) {
	return TCL_ERROR;
    }
    if (Tk_Parent(tkwin) != graphPtr->tkwin) {
	Tcl_AppendResult(graphPtr->interp, "\"", wmPtr->pathName,
	    "\" is not a child of \"", Tk_PathName(graphPtr->tkwin), "\"",
	    (char *)NULL);
	return TCL_ERROR;
    }
    if (tkwin != wmPtr->tkwin) {
	if (wmPtr->tkwin != NULL) {
	    Tk_DeleteEventHandler(wmPtr->tkwin, StructureNotifyMask,
		ChildEventProc, wmPtr);
	    Tk_ManageGeometry(wmPtr->tkwin, (Tk_GeomMgr *) 0, (ClientData)0);
	    Tk_UnmapWindow(wmPtr->tkwin);
	}
	Tk_CreateEventHandler(tkwin, StructureNotifyMask, ChildEventProc, 
		wmPtr);
	Tk_ManageGeometry(tkwin, &winMarkerMgrInfo, wmPtr);
    }
    wmPtr->tkwin = tkwin;

    wmPtr->flags |= MAP_ITEM;
    if (wmPtr->drawUnder) {
	graphPtr->flags |= REDRAW_BACKING_STORE;
    }
    Blt_EventuallyRedrawGraph(graphPtr);
    return TCL_OK;
}

/*
 * ----------------------------------------------------------------------
 *
 * MapWindowMarker --
 *
 *	Calculate the layout position for a window marker.  Positional
 *	information is saved in the marker.
 *
 * Results:
 *	None.
 *
 * ----------------------------------------------------------------------
 */
static void
MapWindowMarker(markerPtr)
    Marker *markerPtr;
{
    WindowMarker *wmPtr = (WindowMarker *)markerPtr;
    Graph *graphPtr = markerPtr->graphPtr;
    Extents2D exts;
    int width, height;

    if (wmPtr->tkwin == (Tk_Window)NULL) {
	return;
    }
    wmPtr->anchorPos = MapPoint(graphPtr, wmPtr->worldPts, &wmPtr->axes);

    width = Tk_ReqWidth(wmPtr->tkwin);
    height = Tk_ReqHeight(wmPtr->tkwin);
    if (wmPtr->reqWidth > 0) {
	width = wmPtr->reqWidth;
    }
    if (wmPtr->reqHeight > 0) {
	height = wmPtr->reqHeight;
    }
    wmPtr->anchorPos = Blt_TranslatePoint(&wmPtr->anchorPos, width, height, 
	wmPtr->anchor);
    wmPtr->anchorPos.x += wmPtr->xOffset;
    wmPtr->anchorPos.y += wmPtr->yOffset;
    wmPtr->width = width;
    wmPtr->height = height;

    /*
     * Determine the bounding box of the window and test to see if it
     * is at least partially contained within the plotting area.
     */
    exts.left = wmPtr->anchorPos.x;
    exts.top = wmPtr->anchorPos.y;
    exts.right = wmPtr->anchorPos.x + wmPtr->width - 1;
    exts.bottom = wmPtr->anchorPos.y + wmPtr->height - 1;
    wmPtr->clipped = BoxesDontOverlap(graphPtr, &exts);
}

/*
 * ----------------------------------------------------------------------
 *
 * PointInWindowMarker --
 *
 * ----------------------------------------------------------------------
 */
static int
PointInWindowMarker(markerPtr, samplePtr)
    Marker *markerPtr;
    Point2D *samplePtr;
{
    WindowMarker *wmPtr = (WindowMarker *)markerPtr;

    return ((samplePtr->x >= wmPtr->anchorPos.x) && 
	    (samplePtr->x < (wmPtr->anchorPos.x + wmPtr->width)) &&
	    (samplePtr->y >= wmPtr->anchorPos.y) && 
	    (samplePtr->y < (wmPtr->anchorPos.y + wmPtr->height)));
}

/*
 * ----------------------------------------------------------------------
 *
 * RegionInWindowMarker --
 *
 * ----------------------------------------------------------------------
 */
static int
RegionInWindowMarker(markerPtr, extsPtr, enclosed)
    Marker *markerPtr;
    Extents2D *extsPtr;
    int enclosed;
{
    WindowMarker *wmPtr = (WindowMarker *)markerPtr;

    if (wmPtr->nWorldPts < 1) {
	return FALSE;
    }
    if (enclosed) {
	return ((wmPtr->anchorPos.x >= extsPtr->left) &&
		(wmPtr->anchorPos.y >= extsPtr->top) && 
		((wmPtr->anchorPos.x + wmPtr->width) <= extsPtr->right) &&
		((wmPtr->anchorPos.y + wmPtr->height) <= extsPtr->bottom));
    }
    return !((wmPtr->anchorPos.x >= extsPtr->right) ||
	     (wmPtr->anchorPos.y >= extsPtr->bottom) ||
	     ((wmPtr->anchorPos.x + wmPtr->width) <= extsPtr->left) ||
	     ((wmPtr->anchorPos.y + wmPtr->height) <= extsPtr->top));
}

/*
 * ----------------------------------------------------------------------
 *
 * DrawWindowMarker --
 *
 * ----------------------------------------------------------------------
 */
/*ARGSUSED*/
static void
DrawWindowMarker(markerPtr, drawable)
    Marker *markerPtr;
    Drawable drawable;		/* Pixmap or window to draw into */
{
    WindowMarker *wmPtr = (WindowMarker *)markerPtr;

    if (wmPtr->tkwin == NULL) {
	return;
    }
    if ((wmPtr->height != Tk_Height(wmPtr->tkwin)) ||
	(wmPtr->width != Tk_Width(wmPtr->tkwin)) ||
	((int)wmPtr->anchorPos.x != Tk_X(wmPtr->tkwin)) ||
	((int)wmPtr->anchorPos.y != Tk_Y(wmPtr->tkwin))) {
	Tk_MoveResizeWindow(wmPtr->tkwin, (int)wmPtr->anchorPos.x, 
	    (int)wmPtr->anchorPos.y, wmPtr->width, wmPtr->height);
    }
    if (!Tk_IsMapped(wmPtr->tkwin)) {
	Tk_MapWindow(wmPtr->tkwin);
    }
}

/*
 * ----------------------------------------------------------------------
 *
 * WindowMarkerToPostScript --
 *
 * ----------------------------------------------------------------------
 */
static void
WindowMarkerToPostScript(markerPtr, psToken)
    Marker *markerPtr;
    PsToken psToken;
{
    WindowMarker *wmPtr = (WindowMarker *)markerPtr;

    if (wmPtr->tkwin == NULL) {
	return;
    }
    if (Tk_IsMapped(wmPtr->tkwin)) {
	Blt_WindowToPostScript(psToken, wmPtr->tkwin, wmPtr->anchorPos.x, 
	       wmPtr->anchorPos.y);
    }
}

/*
 * ----------------------------------------------------------------------
 *
 * FreeWindowMarker --
 *
 *	Destroys the structure containing the attributes of the window
 *      marker.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Window is destroyed and removed from the screen.
 *
 * ----------------------------------------------------------------------
 */
/*ARGSUSED*/
static void
FreeWindowMarker(graphPtr, markerPtr)
    Graph *graphPtr;
    Marker *markerPtr;
{
    WindowMarker *wmPtr = (WindowMarker *)markerPtr;

    if (wmPtr->tkwin != NULL) {
	Tk_DeleteEventHandler(wmPtr->tkwin, StructureNotifyMask,
	    ChildEventProc, wmPtr);
	Tk_ManageGeometry(wmPtr->tkwin, (Tk_GeomMgr *) 0, (ClientData)0);
	Tk_DestroyWindow(wmPtr->tkwin);
    }
}

/*
 * ----------------------------------------------------------------------
 *
 * CreateWindowMarker --
 *
 *	Allocate memory and initialize methods for the new window marker.
 *
 * Results:
 *	The pointer to the newly allocated marker structure is returned.
 *
 * Side effects:
 *	Memory is allocated for the window marker structure.
 *
 * ----------------------------------------------------------------------
 */
static Marker *
CreateWindowMarker()
{
    WindowMarker *wmPtr;

    wmPtr = Blt_Calloc(1, sizeof(WindowMarker));
    if (wmPtr != NULL) {
	wmPtr->classPtr = &windowMarkerClass;
    }
    return (Marker *)wmPtr;
}

/*
 * ----------------------------------------------------------------------
 *
 * ChildEventProc --
 *
 *	This procedure is invoked whenever StructureNotify events
 *	occur for a window that's managed as part of a graph window
 *	marker. This procedure's only purpose is to clean up when
 *	windows are deleted.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The window is disassociated from the window item when it is
 *	deleted.
 *
 * ----------------------------------------------------------------------
 */
static void
ChildEventProc(clientData, eventPtr)
    ClientData clientData;	/* Pointer to record describing window item. */
    XEvent *eventPtr;		/* Describes what just happened. */
{
    WindowMarker *wmPtr = clientData;

    if (eventPtr->type == DestroyNotify) {
	wmPtr->tkwin = NULL;
    }
}

/*
 * ----------------------------------------------------------------------
 *
 * ChildGeometryProc --
 *
 *	This procedure is invoked whenever a window that's associated
 *	with a window item changes its requested dimensions.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The size and location on the window of the window may change,
 *	depending on the options specified for the window item.
 *
 * ----------------------------------------------------------------------
 */
/* ARGSUSED */
static void
ChildGeometryProc(clientData, tkwin)
    ClientData clientData;	/* Pointer to record for window item. */
    Tk_Window tkwin;		/* Window that changed its desired size. */
{
    WindowMarker *wmPtr = clientData;

    if (wmPtr->reqWidth == 0) {
	wmPtr->width = Tk_ReqWidth(tkwin);
    }
    if (wmPtr->reqHeight == 0) {
	wmPtr->height = Tk_ReqHeight(tkwin);
    }
}

/*
 * ----------------------------------------------------------------------
 *
 * ChildCustodyProc --
 *
 *	This procedure is invoked when an embedded window has been
 *	stolen by another geometry manager.  The information and
 *	memory associated with the widget is released.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Arranges for the graph to be redrawn without the embedded
 *	widget at the next idle point.
 *
 * ----------------------------------------------------------------------
 */
 /* ARGSUSED */
static void
ChildCustodyProc(clientData, tkwin)
    ClientData clientData;	/* Window marker to be destroyed. */
    Tk_Window tkwin;		/* Not used. */
{
    Marker *markerPtr = clientData;
    Graph *graphPtr;

    graphPtr = markerPtr->graphPtr;
    DestroyMarker(markerPtr);
    /*
     * Not really needed. We should get an Expose event when the
     * child window is unmapped.
     */
    Blt_EventuallyRedrawGraph(graphPtr);
}

/*
 * ----------------------------------------------------------------------
 *
 * MapLineMarker --
 *
 *	Calculate the layout position for a line marker.  Positional
 *	information is saved in the marker.  The line positions are
 *	stored in an array of points (malloc'ed).
 *
 * Results:
 *	None.
 *
 * ----------------------------------------------------------------------
 */
static void
MapLineMarker(markerPtr)
    Marker *markerPtr;
{
    Graph *graphPtr = markerPtr->graphPtr;
    LineMarker *lmPtr = (LineMarker *)markerPtr;
    Point2D *srcPtr, *endPtr;
    Segment2D *segments, *segPtr;
    Point2D p, q, next;
    Extents2D exts;

    lmPtr->nSegments = 0;
    if (lmPtr->segments != NULL) {
	Blt_Free(lmPtr->segments);
    }
    if (lmPtr->nWorldPts < 2) {
	return;			/* Too few points */
    }
    Blt_GraphExtents(graphPtr, &exts);

    /* 
     * Allow twice the number of world coordinates. The line will
     * represented as series of line segments, not one continous
     * polyline.  This is because clipping against the plot area may
     * chop the line into several disconnected segments.
     */
    segments = Blt_Malloc(lmPtr->nWorldPts * sizeof(Segment2D));
    srcPtr = lmPtr->worldPts;
    p = MapPoint(graphPtr, srcPtr, &lmPtr->axes);
    p.x += lmPtr->xOffset;
    p.y += lmPtr->yOffset;

    segPtr = segments;
    for (srcPtr++, endPtr = lmPtr->worldPts + lmPtr->nWorldPts; 
	 srcPtr < endPtr; srcPtr++) {
	next = MapPoint(graphPtr, srcPtr, &lmPtr->axes);
	next.x += lmPtr->xOffset;
	next.y += lmPtr->yOffset;
	q = next;
	if (Blt_LineRectClip(&exts, &p, &q)) {
	    segPtr->p = p;
	    segPtr->q = q;
	    segPtr++;
	}
	p = next;
    }
    lmPtr->nSegments = segPtr - segments;
    lmPtr->segments = segments;
    lmPtr->clipped = (lmPtr->nSegments == 0);
}

static int
PointInLineMarker(markerPtr, samplePtr)
    Marker *markerPtr;
    Point2D *samplePtr;
{
    LineMarker *lmPtr = (LineMarker *)markerPtr;

    return Blt_PointInSegments(samplePtr, lmPtr->segments, lmPtr->nSegments, 
	   (double)lmPtr->graphPtr->halo);
}

/*
 * ----------------------------------------------------------------------
 *
 * RegionInLineMarker --
 *
 * ----------------------------------------------------------------------
 */
static int
RegionInLineMarker(markerPtr, extsPtr, enclosed)
    Marker *markerPtr;
    Extents2D *extsPtr;
    int enclosed;
{
    LineMarker *lmPtr = (LineMarker *)markerPtr;

    if (lmPtr->nWorldPts < 2) {
	return FALSE;
    }
    if (enclosed) {
	Point2D p;
	Point2D *pointPtr, *endPtr;

	for (pointPtr = lmPtr->worldPts, 
		 endPtr = lmPtr->worldPts + lmPtr->nWorldPts;
	     pointPtr < endPtr; pointPtr++) {
	    p = MapPoint(lmPtr->graphPtr, pointPtr, &lmPtr->axes);
	    if ((p.x < extsPtr->left) && (p.x > extsPtr->right) &&
		(p.y < extsPtr->top) && (p.y > extsPtr->bottom)) {
		return FALSE;
	    }
	}
	return TRUE;		/* All points inside bounding box. */
    } else {
	Point2D p, q;
	int count;
	Point2D *pointPtr, *endPtr;

	count = 0;
	for (pointPtr = lmPtr->worldPts, 
		 endPtr = lmPtr->worldPts + (lmPtr->nWorldPts - 1);
	     pointPtr < endPtr; pointPtr++) {
	    p = MapPoint(lmPtr->graphPtr, pointPtr, &lmPtr->axes);
	    q = MapPoint(lmPtr->graphPtr, pointPtr + 1, &lmPtr->axes);
	    if (Blt_LineRectClip(extsPtr, &p, &q)) {
		count++;
	    }
	}
	return (count > 0);	/* At least 1 segment passes through region. */
    }
}

/*
 * ----------------------------------------------------------------------
 *
 * DrawLineMarker --
 *
 * ----------------------------------------------------------------------
 */
static void
DrawLineMarker(markerPtr, drawable)
    Marker *markerPtr;
    Drawable drawable;		/* Pixmap or window to draw into */
{
    LineMarker *lmPtr = (LineMarker *)markerPtr;

    if (lmPtr->nSegments > 0) {
	Graph *graphPtr = markerPtr->graphPtr;

	Blt_Draw2DSegments(graphPtr->display, drawable, lmPtr->gc, 
		lmPtr->segments, lmPtr->nSegments);
	if (lmPtr->xor) {	/* Toggle the drawing state */
	    lmPtr->xorState = (lmPtr->xorState == 0);
	}
    }
}

/*
 * ----------------------------------------------------------------------
 *
 * ConfigureLineMarker --
 *
 *	This procedure is called to process an argv/argc list, plus
 *	the Tk option database, in order to configure (or reconfigure)
 *	a line marker.
 *
 * Results:
 *	A standard Tcl result.  If TCL_ERROR is returned, then
 *	interp->result contains an error message.
 *
 * Side effects:
 *	Configuration information, such as line width, colors, dashes,
 *	etc. get set for markerPtr; old resources get freed, if there
 *	were any.  The marker is eventually redisplayed.
 *
 * ----------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
ConfigureLineMarker(markerPtr)
    Marker *markerPtr;
{
    Graph *graphPtr = markerPtr->graphPtr;
    LineMarker *lmPtr = (LineMarker *)markerPtr;
    GC newGC;
    XGCValues gcValues;
    unsigned long gcMask;
    Drawable drawable;

    drawable = Tk_WindowId(graphPtr->tkwin);
    gcMask = (GCLineWidth | GCLineStyle | GCCapStyle | GCJoinStyle);
    if (lmPtr->outlineColor != NULL) {
	gcMask |= GCForeground;
	gcValues.foreground = lmPtr->outlineColor->pixel;
    }
    if (lmPtr->fillColor != NULL) {
	gcMask |= GCBackground;
	gcValues.background = lmPtr->fillColor->pixel;
    }
    gcValues.cap_style = lmPtr->capStyle;
    gcValues.join_style = lmPtr->joinStyle;
    gcValues.line_width = LineWidth(lmPtr->lineWidth);
    gcValues.line_style = LineSolid;
    if (LineIsDashed(lmPtr->dashes)) {
	gcValues.line_style = 
	    (gcMask & GCBackground) ? LineDoubleDash : LineOnOffDash;
    }
    if (lmPtr->xor) {
	unsigned long pixel;
	gcValues.function = GXxor;

	gcMask |= GCFunction;
	if (graphPtr->plotBg == NULL) {
	    pixel = WhitePixelOfScreen(Tk_Screen(graphPtr->tkwin));
	} else {
	    pixel = graphPtr->plotBg->pixel;
	}
	if (gcMask & GCBackground) {
	    gcValues.background ^= pixel;
	}
	gcValues.foreground ^= pixel;
	if (drawable != None) {
	    DrawLineMarker(markerPtr, drawable);
	}
    }
    newGC = Blt_GetPrivateGC(graphPtr->tkwin, gcMask, &gcValues);
    if (lmPtr->gc != NULL) {
	Blt_FreePrivateGC(graphPtr->display, lmPtr->gc);
    }
    if (LineIsDashed(lmPtr->dashes)) {
	Blt_SetDashes(graphPtr->display, newGC, &lmPtr->dashes);
    }
    lmPtr->gc = newGC;
    if (lmPtr->xor) {
	if (drawable != None) {
	    MapLineMarker(markerPtr);
	    DrawLineMarker(markerPtr, drawable);
	}
	return TCL_OK;
    }
    lmPtr->flags |= MAP_ITEM;
    if (lmPtr->drawUnder) {
	graphPtr->flags |= REDRAW_BACKING_STORE;
    }
    Blt_EventuallyRedrawGraph(graphPtr);
    return TCL_OK;
}

/*
 * ----------------------------------------------------------------------
 *
 * LineMarkerToPostScript --
 *
 *	Prints postscript commands to display the connect line.
 *	Dashed lines need to be handled specially, especially if a
 *	background color is designated.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	PostScript output commands are saved in the interpreter
 *	(infoPtr->interp) result field.
 *
 * ----------------------------------------------------------------------
 */
static void
LineMarkerToPostScript(markerPtr, psToken)
    Marker *markerPtr;
    PsToken psToken;
{
    LineMarker *lmPtr = (LineMarker *)markerPtr;

    if (lmPtr->nSegments > 0) {
	Blt_LineAttributesToPostScript(psToken, lmPtr->outlineColor,
	    lmPtr->lineWidth, &lmPtr->dashes, lmPtr->capStyle,
	    lmPtr->joinStyle);
	if ((LineIsDashed(lmPtr->dashes)) && (lmPtr->fillColor != NULL)) {
	    Blt_AppendToPostScript(psToken, "/DashesProc {\n  gsave\n    ",
		(char *)NULL);
	    Blt_BackgroundToPostScript(psToken, lmPtr->fillColor);
	    Blt_AppendToPostScript(psToken, "    ", (char *)NULL);
	    Blt_LineDashesToPostScript(psToken, (Blt_Dashes *)NULL);
	    Blt_AppendToPostScript(psToken,
		"stroke\n",
		"  grestore\n",
		"} def\n", (char *)NULL);
	} else {
	    Blt_AppendToPostScript(psToken, "/DashesProc {} def\n",
		(char *)NULL);
	}
	Blt_2DSegmentsToPostScript(psToken, lmPtr->segments, lmPtr->nSegments);
    }
}

/*
 * ----------------------------------------------------------------------
 *
 * FreeLineMarker --
 *
 *	Destroys the structure and attributes of a line marker.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Line attributes (GCs, colors, stipple, etc) get released.
 *	Memory is deallocated, X resources are freed.
 *
 * ----------------------------------------------------------------------
 */
static void
FreeLineMarker(graphPtr, markerPtr)
    Graph *graphPtr;
    Marker *markerPtr;
{
    LineMarker *lmPtr = (LineMarker *)markerPtr;

    if (lmPtr->gc != NULL) {
	Blt_FreePrivateGC(graphPtr->display, lmPtr->gc);
    }
    if (lmPtr->segments != NULL) {
	Blt_Free(lmPtr->segments);
    }
}


/*
 * ----------------------------------------------------------------------
 *
 * CreateLineMarker --
 *
 *	Allocate memory and initialize methods for a new line marker.
 *
 * Results:
 *	The pointer to the newly allocated marker structure is returned.
 *
 * Side effects:
 *	Memory is allocated for the line marker structure.
 *
 * ----------------------------------------------------------------------
 */
static Marker *
CreateLineMarker()
{
    LineMarker *lmPtr;

    lmPtr = Blt_Calloc(1, sizeof(LineMarker));
    if (lmPtr != NULL) {
	lmPtr->classPtr = &lineMarkerClass;
	lmPtr->xor = FALSE;
	lmPtr->capStyle = CapButt;
	lmPtr->joinStyle = JoinMiter;
    }
    return (Marker *)lmPtr;
}

/*
 * ----------------------------------------------------------------------
 *
 * MapPolygonMarker --
 *
 *	Calculate the layout position for a polygon marker.  Positional
 *	information is saved in the polygon in an array of points
 *	(malloc'ed).
 *
 * Results:
 *	None.
 *
 * ----------------------------------------------------------------------
 */
static void
MapPolygonMarker(markerPtr)
    Marker *markerPtr;
{
    Graph *graphPtr = markerPtr->graphPtr;
    PolygonMarker *pmPtr = (PolygonMarker *)markerPtr;
    Point2D *srcPtr, *destPtr, *endPtr;
    Point2D *screenPts;
    Extents2D exts;
    int nScreenPts;

    if (pmPtr->outlinePts != NULL) {
	Blt_Free(pmPtr->outlinePts);
	pmPtr->outlinePts = NULL;
	pmPtr->nOutlinePts = 0;
    }
    if (pmPtr->fillPts != NULL) {
	Blt_Free(pmPtr->fillPts);
	pmPtr->fillPts = NULL;
	pmPtr->nFillPts = 0;
    }
    if (pmPtr->screenPts != NULL) {
	Blt_Free(pmPtr->screenPts);
	pmPtr->screenPts = NULL;
    }
    if (pmPtr->nWorldPts < 3) {
	return;			/* Too few points */
    }

    /* 
     * Allocate and fill a temporary array to hold the screen
     * coordinates of the polygon. 
     */
    nScreenPts = pmPtr->nWorldPts + 1;
    screenPts = Blt_Malloc((nScreenPts + 1) * sizeof(Point2D));
    endPtr = pmPtr->worldPts + pmPtr->nWorldPts;
    destPtr = screenPts;
    for (srcPtr = pmPtr->worldPts; srcPtr < endPtr; srcPtr++) {
	*destPtr = MapPoint(graphPtr, srcPtr, &pmPtr->axes);
	destPtr->x += pmPtr->xOffset;
	destPtr->y += pmPtr->yOffset;
	destPtr++;
    }
    *destPtr = screenPts[0];

    Blt_GraphExtents(graphPtr, &exts);
    pmPtr->clipped = TRUE;
    if (pmPtr->fill.fgColor != NULL) { /* Polygon fill required. */
	Point2D *fillPts;
	int n;

	fillPts = Blt_Malloc(sizeof(Point2D) * nScreenPts * 3);
	assert(fillPts);
	n = Blt_PolyRectClip(&exts, screenPts, pmPtr->nWorldPts, fillPts);
	if (n < 3) { 
	    Blt_Free(fillPts);
	} else {
	    pmPtr->nFillPts = n;
	    pmPtr->fillPts = fillPts;
	    pmPtr->clipped = FALSE;
	}
    }
    if ((pmPtr->outline.fgColor != NULL) && (pmPtr->lineWidth > 0)) { 
	Segment2D *outlinePts;
	register Segment2D *segPtr;
	/* 
	 * Generate line segments representing the polygon outline.
	 * The resulting outline may or may not be closed from
	 * viewport clipping.  
	 */
	outlinePts = Blt_Malloc(nScreenPts * sizeof(Segment2D));
	if (outlinePts == NULL) {
	    return;		/* Can't allocate point array */
	}
	/* 
	 * Note that this assumes that the point array contains an
	 * extra point that closes the polygon. 
	 */
	segPtr = outlinePts;
	for (srcPtr = screenPts, endPtr = screenPts + (nScreenPts - 1);
	     srcPtr < endPtr; srcPtr++) {
	    segPtr->p = srcPtr[0];
	    segPtr->q = srcPtr[1];
	    if (Blt_LineRectClip(&exts, &segPtr->p, &segPtr->q)) {
		segPtr++;
	    }
	}
	pmPtr->nOutlinePts = segPtr - outlinePts;
	pmPtr->outlinePts = outlinePts;
	if (pmPtr->nOutlinePts > 0) {
	    pmPtr->clipped = FALSE;
	}
    }
    pmPtr->screenPts = screenPts;
}

static int
PointInPolygonMarker(markerPtr, samplePtr)
    Marker *markerPtr;
    Point2D *samplePtr;
{
    PolygonMarker *pmPtr = (PolygonMarker *)markerPtr;

    if ((pmPtr->nWorldPts >= 3) && (pmPtr->screenPts != NULL)) {
	return Blt_PointInPolygon(samplePtr, pmPtr->screenPts, 
		  pmPtr->nWorldPts + 1);
    }
    return FALSE;
}

/*
 * ----------------------------------------------------------------------
 *
 * RegionInPolygonMarker --
 *
 * ----------------------------------------------------------------------
 */
static int
RegionInPolygonMarker(markerPtr, extsPtr, enclosed)
    Marker *markerPtr;
    Extents2D *extsPtr;
    int enclosed;
{
    PolygonMarker *pmPtr = (PolygonMarker *)markerPtr;
    
    if ((pmPtr->nWorldPts >= 3) && (pmPtr->screenPts != NULL)) {
	return Blt_RegionInPolygon(extsPtr, pmPtr->screenPts, pmPtr->nWorldPts,
	       enclosed);
    }
    return FALSE;
}

static void
DrawPolygonMarker(markerPtr, drawable)
    Marker *markerPtr;
    Drawable drawable;		/* Pixmap or window to draw into */
{
    Graph *graphPtr = markerPtr->graphPtr;
    PolygonMarker *pmPtr = (PolygonMarker *)markerPtr;

    /* Draw polygon fill region */
    if ((pmPtr->nFillPts > 0) && (pmPtr->fill.fgColor != NULL)) {
	XPoint *destPtr, *pointArr;
	Point2D *srcPtr, *endPtr;
	
	pointArr = Blt_Malloc(pmPtr->nFillPts * sizeof(XPoint));
	if (pointArr == NULL) {
	    return;
	}
	destPtr = pointArr;
	for (srcPtr = pmPtr->fillPts, 
		 endPtr = pmPtr->fillPts + pmPtr->nFillPts; srcPtr < endPtr; 
	     srcPtr++) {
	    destPtr->x = (short int)srcPtr->x;
	    destPtr->y = (short int)srcPtr->y;
	    destPtr++;
	}
	XFillPolygon(graphPtr->display, drawable, pmPtr->fillGC,
	    pointArr, pmPtr->nFillPts, Complex, CoordModeOrigin);
	Blt_Free(pointArr);
    }
    /* and then the outline */
    if ((pmPtr->nOutlinePts > 0) && (pmPtr->lineWidth > 0) && 
	(pmPtr->outline.fgColor != NULL)) {
	Blt_Draw2DSegments(graphPtr->display, drawable, pmPtr->outlineGC,
	    pmPtr->outlinePts, pmPtr->nOutlinePts);
    }
}


static void
PolygonMarkerToPostScript(markerPtr, psToken)
    Marker *markerPtr;
    PsToken psToken;
{
    Graph *graphPtr = markerPtr->graphPtr;
    PolygonMarker *pmPtr = (PolygonMarker *)markerPtr;

    if (pmPtr->fill.fgColor != NULL) {

	/*
	 * Options:  fg bg
	 *			Draw outline only.
	 *	     x          Draw solid or stipple.
	 *	     x  x       Draw solid or stipple.
	 */

	/* Create a path to use for both the polygon and its outline. */
	Blt_PathToPostScript(psToken, pmPtr->fillPts, pmPtr->nFillPts);
	Blt_AppendToPostScript(psToken, "closepath\n", (char *)NULL);

	/* If the background fill color was specified, draw the
	 * polygon in a solid fashion with that color.  */
	if (pmPtr->fill.bgColor != NULL) {
	    Blt_BackgroundToPostScript(psToken, pmPtr->fill.bgColor);
	    Blt_AppendToPostScript(psToken, "Fill\n", (char *)NULL);
	}
	Blt_ForegroundToPostScript(psToken, pmPtr->fill.fgColor);
	if (pmPtr->stipple != None) {
	    /* Draw the stipple in the foreground color. */
	    Blt_StippleToPostScript(psToken, graphPtr->display, 
				    pmPtr->stipple);
	} else {
	    Blt_AppendToPostScript(psToken, "Fill\n", (char *)NULL);
	}
    }

    /* Draw the outline in the foreground color.  */
    if ((pmPtr->lineWidth > 0) && (pmPtr->outline.fgColor != NULL)) {

	/*  Set up the line attributes.  */
	Blt_LineAttributesToPostScript(psToken, pmPtr->outline.fgColor,
	    pmPtr->lineWidth, &pmPtr->dashes, pmPtr->capStyle,
	    pmPtr->joinStyle);

	/*  
	 * Define on-the-fly a PostScript macro "DashesProc" that
	 * will be executed for each call to the Polygon drawing
	 * routine.  If the line isn't dashed, simply make this an
	 * empty definition.  
	 */
	if ((pmPtr->outline.bgColor != NULL) && (LineIsDashed(pmPtr->dashes))) {
	    Blt_AppendToPostScript(psToken,
		"/DashesProc {\n",
		"gsave\n    ", (char *)NULL);
	    Blt_BackgroundToPostScript(psToken, pmPtr->outline.bgColor);
	    Blt_AppendToPostScript(psToken, "    ", (char *)NULL);
	    Blt_LineDashesToPostScript(psToken, (Blt_Dashes *)NULL);
	    Blt_AppendToPostScript(psToken,
		"stroke\n",
		"  grestore\n",
		"} def\n", (char *)NULL);
	} else {
	    Blt_AppendToPostScript(psToken, "/DashesProc {} def\n",
		(char *)NULL);
	}
	Blt_2DSegmentsToPostScript(psToken, pmPtr->outlinePts, 
		   pmPtr->nOutlinePts);
    }
}

/*
 * ----------------------------------------------------------------------
 *
 * ConfigurePolygonMarker --
 *
 *	This procedure is called to process an argv/argc list, plus
 *	the Tk option database, in order to configure (or reconfigure)
 *	a polygon marker.
 *
 * Results:
 *	A standard Tcl result.  If TCL_ERROR is returned, then
 *	interp->result contains an error message.
 *
 * Side effects:
 *	Configuration information, such as polygon color, dashes,
 *	fillstyle, etc. get set for markerPtr; old resources get
 *	freed, if there were any.  The marker is eventually
 *	redisplayed.
 *
 * ---------------------------------------------------------------------- 
 */
/*ARGSUSED*/
static int
ConfigurePolygonMarker(markerPtr)
    Marker *markerPtr;
{
    Graph *graphPtr = markerPtr->graphPtr;
    PolygonMarker *pmPtr = (PolygonMarker *)markerPtr;
    GC newGC;
    XGCValues gcValues;
    unsigned long gcMask;
    Drawable drawable;

    drawable = Tk_WindowId(graphPtr->tkwin);
    gcMask = (GCLineWidth | GCLineStyle);
    if (pmPtr->outline.fgColor != NULL) {
	gcMask |= GCForeground;
	gcValues.foreground = pmPtr->outline.fgColor->pixel;
    }
    if (pmPtr->outline.bgColor != NULL) {
	gcMask |= GCBackground;
	gcValues.background = pmPtr->outline.bgColor->pixel;
    }
    gcMask |= (GCCapStyle | GCJoinStyle);
    gcValues.cap_style = pmPtr->capStyle;
    gcValues.join_style = pmPtr->joinStyle;
    gcValues.line_style = LineSolid;
    gcValues.dash_offset = 0;
    gcValues.line_width = LineWidth(pmPtr->lineWidth);
    if (LineIsDashed(pmPtr->dashes)) {
	gcValues.line_style = (pmPtr->outline.bgColor == NULL)
	    ? LineOnOffDash : LineDoubleDash;
    }
    if (pmPtr->xor) {
	unsigned long pixel;
	gcValues.function = GXxor;

	gcMask |= GCFunction;
	if (graphPtr->plotBg == NULL) {
	    /* The graph's color option may not have been set yet */
	    pixel = WhitePixelOfScreen(Tk_Screen(graphPtr->tkwin));
	} else {
	    pixel = graphPtr->plotBg->pixel;
	}
	if (gcMask & GCBackground) {
	    gcValues.background ^= pixel;
	}
	gcValues.foreground ^= pixel;
	if (drawable != None) {
	    DrawPolygonMarker(markerPtr, drawable);
	}
    }
    newGC = Blt_GetPrivateGC(graphPtr->tkwin, gcMask, &gcValues);
    if (LineIsDashed(pmPtr->dashes)) {
	Blt_SetDashes(graphPtr->display, newGC, &pmPtr->dashes);
    }
    if (pmPtr->outlineGC != NULL) {
	Blt_FreePrivateGC(graphPtr->display, pmPtr->outlineGC);
    }
    pmPtr->outlineGC = newGC;

    gcMask = 0;
    if (pmPtr->fill.fgColor != NULL) {
	gcMask |= GCForeground;
	gcValues.foreground = pmPtr->fill.fgColor->pixel;
    }
    if (pmPtr->fill.bgColor != NULL) {
	gcMask |= GCBackground;
	gcValues.background = pmPtr->fill.bgColor->pixel;
    }
    if (pmPtr->stipple != None) {
	gcValues.stipple = pmPtr->stipple;
	gcValues.fill_style = (pmPtr->fill.bgColor != NULL)
	    ? FillOpaqueStippled : FillStippled;
	gcMask |= (GCStipple | GCFillStyle);
    }
    newGC = Tk_GetGC(graphPtr->tkwin, gcMask, &gcValues);
    if (pmPtr->fillGC != NULL) {
	Tk_FreeGC(graphPtr->display, pmPtr->fillGC);
    }
    pmPtr->fillGC = newGC;

    if ((gcMask == 0) && !(graphPtr->flags & RESET_AXES) && (pmPtr->xor)) {
	if (drawable != None) {
	    MapPolygonMarker(markerPtr);
	    DrawPolygonMarker(markerPtr, drawable);
	}
	return TCL_OK;
    }
    pmPtr->flags |= MAP_ITEM;
    if (pmPtr->drawUnder) {
	graphPtr->flags |= REDRAW_BACKING_STORE;
    }
    Blt_EventuallyRedrawGraph(graphPtr);
    return TCL_OK;
}

/*
 * ----------------------------------------------------------------------
 *
 * FreePolygonMarker --
 *
 *	Release memory and resources allocated for the polygon element.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Everything associated with the polygon element is freed up.
 *
 * ----------------------------------------------------------------------
 */
static void
FreePolygonMarker(graphPtr, markerPtr)
    Graph *graphPtr;
    Marker *markerPtr;
{
    PolygonMarker *pmPtr = (PolygonMarker *)markerPtr;

    if (pmPtr->fillGC != NULL) {
	Tk_FreeGC(graphPtr->display, pmPtr->fillGC);
    }
    if (pmPtr->outlineGC != NULL) {
	Blt_FreePrivateGC(graphPtr->display, pmPtr->outlineGC);
    }
    if (pmPtr->fillPts != NULL) {
	Blt_Free(pmPtr->fillPts);
    }
    if (pmPtr->outlinePts != NULL) {
	Blt_Free(pmPtr->outlinePts);
    }
    if (pmPtr->screenPts != NULL) {
	Blt_Free(pmPtr->screenPts);
    }
    Blt_FreeColorPair(&pmPtr->outline);
    Blt_FreeColorPair(&pmPtr->fill);
}

/*
 * ----------------------------------------------------------------------
 *
 * CreatePolygonMarker --
 *
 *	Allocate memory and initialize methods for the new polygon
 *	marker.
 *
 * Results:
 *	The pointer to the newly allocated marker structure is
 *	returned.
 *
 * Side effects:
 *	Memory is allocated for the polygon marker structure.
 *
 * ---------------------------------------------------------------------- 
 */
static Marker *
CreatePolygonMarker()
{
    PolygonMarker *pmPtr;

    pmPtr = Blt_Calloc(1, sizeof(PolygonMarker));
    if (pmPtr != NULL) {
	pmPtr->classPtr = &polygonMarkerClass;
	pmPtr->capStyle = CapButt;
	pmPtr->joinStyle = JoinMiter;

    }
    return (Marker *)pmPtr;
}

static int
NameToMarker(graphPtr, name, markerPtrPtr)
    Graph *graphPtr;
    char *name;
    Marker **markerPtrPtr;
{
    Blt_HashEntry *hPtr;
    
    hPtr = Blt_FindHashEntry(&graphPtr->markers.table, name);
    if (hPtr != NULL) {
	*markerPtrPtr = (Marker *)Blt_GetHashValue(hPtr);
	return TCL_OK;
    }
    Tcl_AppendResult(graphPtr->interp, "can't find marker \"", name, 
	     "\" in \"", Tk_PathName(graphPtr->tkwin), (char *)NULL);
    return TCL_ERROR;
}


static int
RenameMarker(graphPtr, markerPtr, oldName, newName)
    Graph *graphPtr;
    Marker *markerPtr;
    char *oldName, *newName;
{
    int isNew;
    Blt_HashEntry *hPtr;

    /* Rename the marker only if no marker already exists by that name */
    hPtr = Blt_CreateHashEntry(&graphPtr->markers.table, newName, &isNew);
    if (!isNew) {
	Tcl_AppendResult(graphPtr->interp, "can't rename marker: \"", newName,
	    "\" already exists", (char *)NULL);
	return TCL_ERROR;
    }
    markerPtr->name = Blt_Strdup(newName);
    markerPtr->hashPtr = hPtr;
    Blt_SetHashValue(hPtr, (char *)markerPtr);

    /* Delete the old hash entry */
    hPtr = Blt_FindHashEntry(&graphPtr->markers.table, oldName);
    Blt_DeleteHashEntry(&graphPtr->markers.table, hPtr);
    if (oldName != NULL) {
	Blt_Free(oldName);
    }
    return TCL_OK;
}

/*
 * ----------------------------------------------------------------------
 *
 * NamesOp --
 *
 *	Returns a list of marker identifiers in interp->result;
 *
 * Results:
 *	The return value is a standard Tcl result.
 *
 * ----------------------------------------------------------------------
 */
static int
NamesOp(graphPtr, interp, argc, argv)
    Graph *graphPtr;
    Tcl_Interp *interp;
    int argc;
    char **argv;
{
    Marker *markerPtr;
    Blt_ChainLink *linkPtr;
    register int i;

    Tcl_ResetResult(interp);
    for (linkPtr = Blt_ChainFirstLink(graphPtr->markers.displayList);
	linkPtr != NULL; linkPtr = Blt_ChainNextLink(linkPtr)) {
	markerPtr = Blt_ChainGetValue(linkPtr);
	if (argc == 3) {
	    Tcl_AppendElement(interp, markerPtr->name);
	    continue;
	}
	for (i = 3; i < argc; i++) {
	    if (Tcl_StringMatch(markerPtr->name, argv[i])) {
		Tcl_AppendElement(interp, markerPtr->name);
		break;
	    }
	}
    }
    return TCL_OK;
}

ClientData
Blt_MakeMarkerTag(graphPtr, tagName)
    Graph *graphPtr;
    char *tagName;
{
    Blt_HashEntry *hPtr;
    int isNew;

    hPtr = Blt_CreateHashEntry(&graphPtr->markers.tagTable, tagName, &isNew);
    assert(hPtr);
    return Blt_GetHashKey(&graphPtr->markers.tagTable, hPtr);
}

/*
 *----------------------------------------------------------------------
 *
 * BindOp --
 *
 *	.g element bind elemName sequence command
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
BindOp(graphPtr, interp, argc, argv)
    Graph *graphPtr;
    Tcl_Interp *interp;
    int argc;
    char **argv;
{
    if (argc == 3) {
	Blt_HashEntry *hPtr;
	Blt_HashSearch cursor;
	char *tag;

	for (hPtr = Blt_FirstHashEntry(&graphPtr->markers.tagTable, &cursor);
	    hPtr != NULL; hPtr = Blt_NextHashEntry(&cursor)) {
	    tag = Blt_GetHashKey(&graphPtr->markers.tagTable, hPtr);
	    Tcl_AppendElement(interp, tag);
	}
	return TCL_OK;
    }
    return Blt_ConfigureBindings(interp, graphPtr->bindTable,
	Blt_MakeMarkerTag(graphPtr, argv[3]), argc - 4, argv + 4);
}

/*
 * ----------------------------------------------------------------------
 *
 * CgetOp --
 *
 * ----------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
CgetOp(graphPtr, interp, argc, argv)
    Graph *graphPtr;
    Tcl_Interp *interp;
    int argc;
    char **argv;
{
    Marker *markerPtr;

    if (NameToMarker(graphPtr, argv[3], &markerPtr) != TCL_OK) {
	return TCL_ERROR;
    }
    if (Tk_ConfigureValue(interp, graphPtr->tkwin, 
	markerPtr->classPtr->configSpecs, (char *)markerPtr, argv[4], 0) 
	!= TCL_OK) {
	return TCL_ERROR;
    }
    return TCL_OK;
}

/*
 * ----------------------------------------------------------------------
 *
 * ConfigureOp --
 *
 * Results:
 *	The return value is a standard Tcl result.
 *
 * Side Effects:
 *
 * ----------------------------------------------------------------------
 */
static int
ConfigureOp(graphPtr, interp, argc, argv)
    Graph *graphPtr;
    Tcl_Interp *interp;
    int argc;
    char **argv;
{
    Marker *markerPtr;
    int flags = TK_CONFIG_ARGV_ONLY;
    char *oldName;
    int nNames, nOpts;
    char **options;
    register int i;
    int under;

    /* Figure out where the option value pairs begin */
    argc -= 3;
    argv += 3;
    for (i = 0; i < argc; i++) {
	if (argv[i][0] == '-') {
	    break;
	}
	if (NameToMarker(graphPtr, argv[i], &markerPtr) != TCL_OK) {
	    return TCL_ERROR;
	}
    }
    nNames = i;			/* Number of element names specified */
    nOpts = argc - i;		/* Number of options specified */
    options = argv + nNames;	/* Start of options in argv  */

    for (i = 0; i < nNames; i++) {
	NameToMarker(graphPtr, argv[i], &markerPtr);
	if (nOpts == 0) {
	    return Tk_ConfigureInfo(interp, graphPtr->tkwin, 
		markerPtr->classPtr->configSpecs, (char *)markerPtr, 
		(char *)NULL, flags);
	} else if (nOpts == 1) {
	    return Tk_ConfigureInfo(interp, graphPtr->tkwin,
		markerPtr->classPtr->configSpecs, (char *)markerPtr, 
		options[0], flags);
	}
	/* Save the old marker. */
	oldName = markerPtr->name;
	under = markerPtr->drawUnder;
	if (Tk_ConfigureWidget(interp, graphPtr->tkwin, 
		markerPtr->classPtr->configSpecs, nOpts, options, 
		(char *)markerPtr, flags) != TCL_OK) {
	    return TCL_ERROR;
	}
	if (oldName != markerPtr->name) {
	    if (RenameMarker(graphPtr, markerPtr, oldName, markerPtr->name) 
		!= TCL_OK) {
		markerPtr->name = oldName;
		return TCL_ERROR;
	    }
	}
	if ((*markerPtr->classPtr->configProc) (markerPtr) != TCL_OK) {
	    return TCL_ERROR;
	}
	if (markerPtr->drawUnder != under) {
	    graphPtr->flags |= REDRAW_BACKING_STORE;
	}
    }
    return TCL_OK;
}

/*
 * ----------------------------------------------------------------------
 *
 * CreateOp --
 *
 *	This procedure creates and initializes a new marker.
 *
 * Results:
 *	The return value is a pointer to a structure describing
 *	the new element.  If an error occurred, then the return
 *	value is NULL and an error message is left in interp->result.
 *
 * Side effects:
 *	Memory is allocated, etc.
 *
 * ----------------------------------------------------------------------
 */
static int
CreateOp(graphPtr, interp, argc, argv)
    Graph *graphPtr;
    Tcl_Interp *interp;
    int argc;
    char **argv;
{
    Marker *markerPtr;
    Blt_HashEntry *hPtr;
    int isNew;
    Blt_Uid classUid;
    register int i;
    char *name;
    char string[200];
    unsigned int length;
    char c;

    c = argv[3][0];
    /* Create the new marker based upon the given type */
    if ((c == 't') && (strcmp(argv[3], "text") == 0)) {
	classUid = bltTextMarkerUid;
    } else if ((c == 'l') && (strcmp(argv[3], "line") == 0)) {
	classUid = bltLineMarkerUid;
    } else if ((c == 'p') && (strcmp(argv[3], "polygon") == 0)) {
	classUid = bltPolygonMarkerUid;
    } else if ((c == 'i') && (strcmp(argv[3], "image") == 0)) {
	classUid = bltImageMarkerUid;
    } else if ((c == 'b') && (strcmp(argv[3], "bitmap") == 0)) {
	classUid = bltBitmapMarkerUid;
    } else if ((c == 'w') && (strcmp(argv[3], "window") == 0)) {
	classUid = bltWindowMarkerUid;
    } else {
	Tcl_AppendResult(interp, "unknown marker type \"", argv[3],
    "\": should be \"text\", \"line\", \"polygon\", \"bitmap\", \"image\", or \
\"window\"", (char *)NULL);
	return TCL_ERROR;
    }
    /* Scan for "-name" option. We need it for the component name */
    name = NULL;
    for (i = 4; i < argc; i += 2) {
	length = strlen(argv[i]);
	if ((length > 1) && (strncmp(argv[i], "-name", length) == 0)) {
	    name = argv[i + 1];
	    break;
	}
    }
    /* If no name was given for the marker, make up one. */
    if (name == NULL) {
	sprintf(string, "marker%d", graphPtr->nextMarkerId++);
	name = string;
    } else if (name[0] == '-') {
	Tcl_AppendResult(interp, "name of marker \"", name, 
		"\" can't start with a '-'", (char *)NULL);
	return TCL_ERROR;
    }
    markerPtr = CreateMarker(graphPtr, name, classUid);
    if (Blt_ConfigureWidgetComponent(interp, graphPtr->tkwin, name, 
	     markerPtr->classUid, markerPtr->classPtr->configSpecs,
	    argc - 4, argv + 4, (char *)markerPtr, 0) != TCL_OK) {
	DestroyMarker(markerPtr);
	return TCL_ERROR;
    }
    if ((*markerPtr->classPtr->configProc) (markerPtr) != TCL_OK) {
	DestroyMarker(markerPtr);
	return TCL_ERROR;
    }
    hPtr = Blt_CreateHashEntry(&graphPtr->markers.table, name, &isNew);
    if (!isNew) {
	Marker *oldMarkerPtr;
	/*
	 * Marker by the same name already exists.  Delete the old
	 * marker and it's list entry.  But save the hash entry.
	 */
	oldMarkerPtr = (Marker *)Blt_GetHashValue(hPtr);
	oldMarkerPtr->hashPtr = NULL;
	DestroyMarker(oldMarkerPtr);
    }
    Blt_SetHashValue(hPtr, markerPtr);
    markerPtr->hashPtr = hPtr;
    markerPtr->linkPtr = 
	Blt_ChainAppend(graphPtr->markers.displayList, markerPtr);
    if (markerPtr->drawUnder) {
	graphPtr->flags |= REDRAW_BACKING_STORE;
    }
    Blt_EventuallyRedrawGraph(graphPtr);
    Tcl_SetResult(interp, name, TCL_VOLATILE);
    return TCL_OK;
}

/*
 * ----------------------------------------------------------------------
 *
 * DeleteOp --
 *
 *	Deletes the marker given by markerId.
 *
 * Results:
 *	The return value is a standard Tcl result.
 *
 * Side Effects:
 *	Graph will be redrawn to reflect the new display list.
 *
 * ----------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
DeleteOp(graphPtr, interp, argc, argv)
    Graph *graphPtr;
    Tcl_Interp *interp;		/* Not used. */
    int argc;
    char **argv;
{
    Marker *markerPtr;
    register int i;

    for (i = 3; i < argc; i++) {
	if (NameToMarker(graphPtr, argv[i], &markerPtr) == TCL_OK) {
	    DestroyMarker(markerPtr);
	}
    }
    Tcl_ResetResult(interp);
    Blt_EventuallyRedrawGraph(graphPtr);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * GetOp --
 *
 * 	Find the legend entry from the given argument.  The argument
 *	can be either a screen position "@x,y" or the name of an
 *	element.
 *
 *	I don't know how useful it is to test with the name of an
 *	element.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side Effects:
 *	Graph will be redrawn to reflect the new legend attributes.
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
GetOp(graphPtr, interp, argc, argv)
    Graph *graphPtr;
    Tcl_Interp *interp;
    int argc;			/* Not used. */
    char *argv[];
{
    register Marker *markerPtr;

    if ((argv[3][0] == 'c') && (strcmp(argv[3], "current") == 0)) {
	markerPtr = (Marker *)Blt_GetCurrentItem(graphPtr->bindTable);
	/* Report only on markers. */
	if (markerPtr == NULL) {
	    return TCL_OK;
	}
	if ((markerPtr->classUid == bltBitmapMarkerUid) ||
	    (markerPtr->classUid == bltLineMarkerUid) ||
	    (markerPtr->classUid == bltWindowMarkerUid) ||
	    (markerPtr->classUid == bltPolygonMarkerUid) ||
	    (markerPtr->classUid == bltTextMarkerUid) ||
	    (markerPtr->classUid == bltImageMarkerUid)) {
	    Tcl_SetResult(interp, markerPtr->name, TCL_VOLATILE);
	}
    }
    return TCL_OK;
}

/*
 * ----------------------------------------------------------------------
 *
 * RelinkOp --
 *
 *	Reorders the marker (given by the first name) before/after
 *	the another marker (given by the second name) in the
 *	marker display list.  If no second name is given, the
 *	marker is placed at the beginning/end of the list.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side Effects:
 *	Graph will be redrawn to reflect the new display list.
 *
 * ----------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
RelinkOp(graphPtr, interp, argc, argv)
    Graph *graphPtr;
    Tcl_Interp *interp;		/* Not used. */
    int argc;
    char **argv;
{
    Blt_ChainLink *linkPtr, *placePtr;
    Marker *markerPtr;

    /* Find the marker to be raised or lowered. */
    if (NameToMarker(graphPtr, argv[3], &markerPtr) != TCL_OK) {
	return TCL_ERROR;
    }
    /* Right now it's assumed that all markers are always in the
       display list. */
    linkPtr = markerPtr->linkPtr;
    Blt_ChainUnlinkLink(graphPtr->markers.displayList, markerPtr->linkPtr);

    placePtr = NULL;
    if (argc == 5) {
	if (NameToMarker(graphPtr, argv[4], &markerPtr) != TCL_OK) {
	    return TCL_ERROR;
	}
	placePtr = markerPtr->linkPtr;
    }

    /* Link the marker at its new position. */
    if (argv[2][0] == 'a') {
	Blt_ChainLinkAfter(graphPtr->markers.displayList, linkPtr, placePtr);
    } else {
	Blt_ChainLinkBefore(graphPtr->markers.displayList, linkPtr, placePtr);
    }
    if (markerPtr->drawUnder) {
	graphPtr->flags |= REDRAW_BACKING_STORE;
    }
    Blt_EventuallyRedrawGraph(graphPtr);
    return TCL_OK;
}


/*
 * ----------------------------------------------------------------------
 *
 * FindOp --
 *
 *	Returns if marker by a given ID currently exists.
 *
 * Results:
 *	A standard Tcl result.
 *
 * ----------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
FindOp(graphPtr, interp, argc, argv)
    Graph *graphPtr;
    Tcl_Interp *interp;
    int argc;
    char **argv;
{
    Blt_ChainLink *linkPtr;
    Extents2D exts;
    Marker *markerPtr;
    int mode;
    int left, right, top, bottom;
    int enclosed;

#define FIND_ENCLOSED	 (1<<0)
#define FIND_OVERLAPPING (1<<1)
    if (strcmp(argv[3], "enclosed") == 0) {
	mode = FIND_ENCLOSED;
    } else if (strcmp(argv[3], "overlapping") == 0) {
	mode = FIND_OVERLAPPING;
    } else {
	Tcl_AppendResult(interp, "bad search type \"", argv[3], 
		": should be \"enclosed\", or \"overlapping\"", (char *)NULL);
	return TCL_ERROR;
    }

    if ((Tcl_GetInt(interp, argv[4], &left) != TCL_OK) ||
	(Tcl_GetInt(interp, argv[5], &top) != TCL_OK) ||
	(Tcl_GetInt(interp, argv[6], &right) != TCL_OK) ||
	(Tcl_GetInt(interp, argv[7], &bottom) != TCL_OK)) {
	return TCL_ERROR;
    }
    if (left < right) {
	exts.left = (double)left;
	exts.right = (double)right;
    } else {
	exts.left = (double)right;
	exts.right = (double)left;
    }
    if (top < bottom) {
	exts.top = (double)top;
	exts.bottom = (double)bottom;
    } else {
	exts.top = (double)bottom;
	exts.bottom = (double)top;
    }
    enclosed = (mode == FIND_ENCLOSED);
    for (linkPtr = Blt_ChainFirstLink(graphPtr->markers.displayList);
	 linkPtr != NULL; linkPtr = Blt_ChainNextLink(linkPtr)) {
	markerPtr = Blt_ChainGetValue(linkPtr);
	if (markerPtr->hidden) {
	    continue;
	}
	if (markerPtr->elemName != NULL) {
	    Blt_HashEntry *hPtr;
	    
	    hPtr = Blt_FindHashEntry(&graphPtr->elements.table, 
				     markerPtr->elemName);
	    if (hPtr != NULL) {
		Element *elemPtr;
		
		elemPtr = (Element *)Blt_GetHashValue(hPtr);
		if (elemPtr->hidden) {
		    continue;
		}
	    }
	}
	if ((*markerPtr->classPtr->regionProc)(markerPtr, &exts, enclosed)) {
	    Tcl_SetResult(interp, markerPtr->name, TCL_VOLATILE);
	    return TCL_OK;
	}
    }
    Tcl_SetResult(interp, "", TCL_VOLATILE);
    return TCL_OK;
}

/*
 * ----------------------------------------------------------------------
 *
 * ExistsOp --
 *
 *	Returns if marker by a given ID currently exists.
 *
 * Results:
 *	A standard Tcl result.
 *
 * ----------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
ExistsOp(graphPtr, interp, argc, argv)
    Graph *graphPtr;
    Tcl_Interp *interp;
    int argc;
    char **argv;
{
    Blt_HashEntry *hPtr;

    hPtr = Blt_FindHashEntry(&graphPtr->markers.table, argv[3]);
    Blt_SetBooleanResult(interp, (hPtr != NULL));
    return TCL_OK;
}

/*
 * ----------------------------------------------------------------------
 *
 * TypeOp --
 *
 *	Returns a symbolic name for the type of the marker whose ID is
 *	given.
 *
 * Results:
 *	A standard Tcl result. interp->result will contain the symbolic
 *	type of the marker.
 *
 * ----------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
TypeOp(graphPtr, interp, argc, argv)
    Graph *graphPtr;
    Tcl_Interp *interp;
    int argc;
    char **argv;
{
    Marker *markerPtr;

    if (NameToMarker(graphPtr, argv[3], &markerPtr) != TCL_OK) {
	return TCL_ERROR;
    }
    Tcl_SetResult(interp, markerPtr->classUid, TCL_STATIC);
    return TCL_OK;
}

/* Public routines */

/*
 * ----------------------------------------------------------------------
 *
 * Blt_MarkerOp --
 *
 *	This procedure is invoked to process the Tcl command
 *	that corresponds to a widget managed by this module.
 *	See the user documentation for details on what it does.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 * ----------------------------------------------------------------------
 */

static Blt_OpSpec markerOps[] =
{
    {"after", 1, (Blt_Op)RelinkOp, 4, 5, "marker ?afterMarker?",},
    {"before", 2, (Blt_Op)RelinkOp, 4, 5, "marker ?beforeMarker?",},
    {"bind", 2, (Blt_Op)BindOp, 3, 6, "marker sequence command",},
    {"cget", 2, (Blt_Op)CgetOp, 5, 5, "marker option",},
    {"configure", 2, (Blt_Op)ConfigureOp, 4, 0,
	"marker ?marker?... ?option value?...",},
    {"create", 2, (Blt_Op)CreateOp, 4, 0,
	"type ?option value?...",},
    {"delete", 1, (Blt_Op)DeleteOp, 3, 0, "?marker?...",},
    {"exists", 1, (Blt_Op)ExistsOp, 4, 4, "marker",},
    {"find", 1, (Blt_Op)FindOp, 8, 8, "enclosed|overlapping x1 y1 x2 y2",},
    {"get", 1, (Blt_Op)GetOp, 4, 4, "name",},
    {"names", 1, (Blt_Op)NamesOp, 3, 0, "?pattern?...",},
    {"type", 1, (Blt_Op)TypeOp, 4, 4, "marker",},
};
static int nMarkerOps = sizeof(markerOps) / sizeof(Blt_OpSpec);

/*ARGSUSED*/
int
Blt_MarkerOp(graphPtr, interp, argc, argv)
    Graph *graphPtr;
    Tcl_Interp *interp;		/* Not used. */
    int argc;
    char **argv;
{
    Blt_Op proc;
    int result;

    proc = Blt_GetOp(interp, nMarkerOps, markerOps, BLT_OP_ARG2, argc, argv,0);
    if (proc == NULL) {
	return TCL_ERROR;
    }
    result = (*proc) (graphPtr, interp, argc, argv);
    return result;
}

/*
 * -------------------------------------------------------------------------
 *
 * Blt_MarkersToPostScript --
 *
 * -------------------------------------------------------------------------
 */
void
Blt_MarkersToPostScript(graphPtr, psToken, under)
    Graph *graphPtr;
    PsToken psToken;
    int under;
{
    Blt_ChainLink *linkPtr;
    register Marker *markerPtr;

    for (linkPtr = Blt_ChainFirstLink(graphPtr->markers.displayList);
	linkPtr != NULL; linkPtr = Blt_ChainNextLink(linkPtr)) {
	markerPtr = Blt_ChainGetValue(linkPtr);
	if ((markerPtr->classPtr->postscriptProc == NULL) || 
	    (markerPtr->nWorldPts == 0)) {
	    continue;
	}
	if (markerPtr->drawUnder != under) {
	    continue;
	}
	if (markerPtr->hidden) {
	    continue;
	}
	if (markerPtr->elemName != NULL) {
	    Blt_HashEntry *hPtr;

	    hPtr = Blt_FindHashEntry(&graphPtr->elements.table, 
			     markerPtr->elemName);
	    if (hPtr != NULL) {
		Element *elemPtr;

		elemPtr = (Element *)Blt_GetHashValue(hPtr);
		if (elemPtr->hidden) {
		    continue;
		}
	    }
	}
	Blt_AppendToPostScript(psToken, "\n% Marker \"", markerPtr->name,
	    "\" is a ", markerPtr->classUid, " marker\n", (char *)NULL);
	(*markerPtr->classPtr->postscriptProc) (markerPtr, psToken);
    }
}

/*
 * -------------------------------------------------------------------------
 *
 * Blt_DrawMarkers --
 *
 *	Calls the individual drawing routines (based on marker type)
 *	for each marker in the display list.
 *
 *	A marker will not be drawn if
 *
 *	1) An element linked to the marker (indicated by elemName) 
 *	   is currently hidden.
 *
 *	2) No coordinates have been specified for the marker.
 *
 *	3) The marker is requesting to be drawn at a different level
 *	   (above/below the elements) from the current mode.
 *
 *	4) The marker is configured as hidden (-hide option).
 *
 *	5) The marker isn't visible in the current viewport
 *	   (i.e. clipped).
 *
 * Results:
 *	None
 *
 * Side Effects:
 *	Markers are drawn into the drawable (pixmap) which will eventually
 *	be displayed in the graph window.
 *
 * -------------------------------------------------------------------------
 */
void
Blt_DrawMarkers(graphPtr, drawable, under)
    Graph *graphPtr;
    Drawable drawable;		/* Pixmap or window to draw into */
    int under;
{
    Blt_ChainLink *linkPtr;
    Marker *markerPtr;

    for (linkPtr = Blt_ChainFirstLink(graphPtr->markers.displayList);
	linkPtr != NULL; linkPtr = Blt_ChainNextLink(linkPtr)) {
	markerPtr = Blt_ChainGetValue(linkPtr);

	if ((markerPtr->nWorldPts == 0) || 
	    (markerPtr->drawUnder != under) ||
	    (markerPtr->hidden) || 
	    (markerPtr->clipped)) {
	    continue;
	}
	if (markerPtr->elemName != NULL) {
	    Blt_HashEntry *hPtr;

	    /* Look up the named element and see if it's hidden */
	    hPtr = Blt_FindHashEntry(&graphPtr->elements.table, 
				     markerPtr->elemName);
	    if (hPtr != NULL) {
		Element *elemPtr;

		elemPtr = (Element *)Blt_GetHashValue(hPtr);
		if (elemPtr->hidden) {
		    continue;
		}
	    }
	}

	(*markerPtr->classPtr->drawProc) (markerPtr, drawable);
    }
}

void
Blt_MapMarkers(graphPtr)
    Graph *graphPtr;
{
    Blt_ChainLink *linkPtr;
    Marker *markerPtr;

    for (linkPtr = Blt_ChainFirstLink(graphPtr->markers.displayList);
	linkPtr != NULL; linkPtr = Blt_ChainNextLink(linkPtr)) {
	markerPtr = Blt_ChainGetValue(linkPtr);
	if ((markerPtr->nWorldPts == 0) || (markerPtr->hidden)) {
	    continue;
	}
	if ((graphPtr->flags & MAP_ALL) || (markerPtr->flags & MAP_ITEM)) {
	    (*markerPtr->classPtr->mapProc) (markerPtr);
	    markerPtr->flags &= ~MAP_ITEM;
	}
    }
}


void
Blt_DestroyMarkers(graphPtr)
    Graph *graphPtr;
{
    Blt_HashEntry *hPtr;
    Blt_HashSearch cursor;
    Marker *markerPtr;

    for (hPtr = Blt_FirstHashEntry(&graphPtr->markers.table, &cursor);
	hPtr != NULL; hPtr = Blt_NextHashEntry(&cursor)) {
	markerPtr = (Marker *)Blt_GetHashValue(hPtr);
	/*
	 * Dereferencing the pointer to the hash table prevents the
	 * hash table entry from being automatically deleted.
	 */
	markerPtr->hashPtr = NULL;
	DestroyMarker(markerPtr);
    }
    Blt_DeleteHashTable(&graphPtr->markers.table);
    Blt_DeleteHashTable(&graphPtr->markers.tagTable);
    Blt_ChainDestroy(graphPtr->markers.displayList);
}

Marker *
Blt_NearestMarker(graphPtr, x, y, under)
    Graph *graphPtr;
    int x, y;			/* Screen coordinates */
    int under;
{
    Blt_ChainLink *linkPtr;
    Marker *markerPtr;
    Point2D point;

    point.x = (double)x;
    point.y = (double)y;
    for (linkPtr = Blt_ChainLastLink(graphPtr->markers.displayList);
	linkPtr != NULL; linkPtr = Blt_ChainPrevLink(linkPtr)) {
	markerPtr = Blt_ChainGetValue(linkPtr);
	/* 
	 * Don't consider markers that are pending to be mapped. Even
	 * if the marker has already been mapped, the coordinates
	 * could be invalid now.  Better to pick no marker than the
	 * wrong marker.
	 */
	if ((markerPtr->drawUnder == under) && (markerPtr->nWorldPts > 0) && 
	    ((markerPtr->flags & MAP_ITEM) == 0) && 
	    (!markerPtr->hidden) && (markerPtr->state == STATE_NORMAL)) {
	    if ((*markerPtr->classPtr->pointProc) (markerPtr, &point)) {
		return markerPtr;
	    }
	}
    }
    return NULL;
}
