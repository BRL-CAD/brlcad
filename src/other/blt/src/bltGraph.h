/*
 * bltGraph.h --
 *
 * Copyright 1991-1998 Lucent Technologies, Inc.
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

#ifndef _BLT_GRAPH_H
#define _BLT_GRAPH_H

#include "bltInt.h"
#include "bltHash.h"
#include "bltBind.h"
#include "bltChain.h"
#include "bltPs.h"
#include "bltTile.h"

typedef struct GraphStruct Graph;
typedef struct ElementStruct Element;
typedef struct LegendStruct Legend;

#include "bltGrAxis.h"
#include "bltGrLegd.h"

#define MARKER_UNDER	1	/* Draw markers designated to lie underneath
				 * elements, grids, legend, etc. */
#define MARKER_ABOVE	0	/* Draw markers designated to rest above
				 * elements, grids, legend, etc. */

#define PADX		2	/* Padding between labels/titles */
#define PADY    	2	/* Padding between labels */

#define MINIMUM_MARGIN	20	/* Minimum margin size */


#define BOUND(x, lo, hi)	 \
	(((x) > (hi)) ? (hi) : ((x) < (lo)) ? (lo) : (x))

/*
 * -------------------------------------------------------------------
 *
 * 	Graph component structure definitions
 *
 * -------------------------------------------------------------------
 */
#define PointInGraph(g,x,y) \
	(((x) <= (g)->right) && ((x) >= (g)->left) && \
	 ((y) <= (g)->bottom) && ((y) >= (g)->top))

/*
 * -------------------------------------------------------------------
 *
 * ClassType --
 *
 *	Enumerates the different types of graph elements this program
 *	produces.  An element can be either a line or a bar.
 *
 * -------------------------------------------------------------------
 */
typedef enum {
    CLASS_UNKNOWN,
    CLASS_LINE_ELEMENT,
    CLASS_STRIP_ELEMENT,
    CLASS_BAR_ELEMENT,
    CLASS_BITMAP_MARKER,
    CLASS_IMAGE_MARKER,
    CLASS_LINE_MARKER,
    CLASS_POLYGON_MARKER,
    CLASS_TEXT_MARKER,
    CLASS_WINDOW_MARKER

} ClassType;

/*
 * Mask values used to selectively enable GRAPH or BARCHART entries in
 * the various configuration specs.
 */
#define GRAPH		(TK_CONFIG_USER_BIT << 1)
#define STRIPCHART	(TK_CONFIG_USER_BIT << 2)
#define BARCHART	(TK_CONFIG_USER_BIT << 3)
#define LINE_GRAPHS	(GRAPH | STRIPCHART)
#define ALL_GRAPHS	(GRAPH | BARCHART | STRIPCHART)

#define PEN_DELETE_PENDING	(1<<0)
#define ACTIVE_PEN		(TK_CONFIG_USER_BIT << 6)
#define NORMAL_PEN		(TK_CONFIG_USER_BIT << 7)
#define ALL_PENS		(NORMAL_PEN | ACTIVE_PEN)

/*
 * -------------------------------------------------------------------
 *
 * FreqInfo --
 *
 * -------------------------------------------------------------------
 */
typedef struct {
    int freq;			/* Number of occurrences of x-coordinate */
    Axis2D axes;		/* Indicates which x and y axis are mapped to
				 * the x-value */
    double sum;			/* Sum of the ordinates of each duplicate
				 * abscissa */
    int count;
    double lastY;

} FreqInfo;

/*
 * -------------------------------------------------------------------
 *
 * FreqKey --
 *
 *
 * -------------------------------------------------------------------
 */
typedef struct {
    double value;		/* Duplicated abscissa */
    Axis2D axes;		/* Axis mapping of element */
} FreqKey;

/*
 * BarModes --
 *
 *	Bar elements are displayed according to their x-y coordinates.
 *	If two bars have the same abscissa (x-coordinate), the bar
 *	segments will be drawn according to one of the following
 *	modes:
 */

typedef enum BarModes {
    MODE_INFRONT,		/* Each successive segment is drawn in
				 * front of the previous. */
    MODE_STACKED,		/* Each successive segment is drawn
				 * stacked above the previous. */
    MODE_ALIGNED,		/* Each successive segment is drawn
				 * aligned to the previous from
				 * right-to-left. */
    MODE_OVERLAP		/* Like "aligned", each successive segment
				 * is drawn from right-to-left. In addition
				 * the segments will overlap each other
				 * by a small amount */
} BarMode;

typedef struct PenStruct Pen;
typedef struct MarkerStruct Marker;

typedef Pen *(PenCreateProc) _ANSI_ARGS_((void));
typedef int (PenConfigureProc) _ANSI_ARGS_((Graph *graphPtr, Pen *penPtr));
typedef void (PenDestroyProc) _ANSI_ARGS_((Graph *graphPtr, Pen *penPtr));

struct PenStruct {
    char *name;			/* Pen style identifier.  If NULL pen
				 * was statically allocated. */
    Blt_Uid classUid;		/* Type of pen */
    char *typeId;		/* String token identifying the type of pen */
    unsigned int flags;		/* Indicates if the pen element is active or
				 * normal */
    int refCount;		/* Reference count for elements using
				 * this pen. */
    Blt_HashEntry *hashPtr;

    Tk_ConfigSpec *configSpecs;	/* Configuration specifications */

    PenConfigureProc *configProc;
    PenDestroyProc *destroyProc;

};

typedef enum {
    PS_MONO_BACKGROUND,
    PS_MONO_FOREGROUND
} MonoAttribute;

/*
 * PostScript --
 *
 * 	Structure contains information specific to the outputting of
 *	PostScript commands to print the graph.
 *
 */
typedef struct  {
    /* User configurable fields */

    int decorations;		/* If non-zero, print graph with
				 * color background and 3D borders */

    int reqWidth, reqHeight;	/* If greater than zero, represents the
				 * requested dimensions of the printed graph */
    int reqPaperWidth;
    int reqPaperHeight;		/* Requested dimensions for the PostScript
				 * page. Can constrain the size of the graph
				 * if the graph (plus padding) is larger than
				 * the size of the page. */
    Blt_Pad padX, padY;		/* Requested padding on the exterior of the
				 * graph. This forms the bounding box for
				 * the page. */
    PsColorMode colorMode;	/* Selects the color mode for PostScript page
				 * (0=monochrome, 1=greyscale, 2=color) */
    char *colorVarName;		/* If non-NULL, is the name of a Tcl array
				 * variable containing X to PostScript color
				 * translations */
    char *fontVarName;		/* If non-NULL, is the name of a Tcl array
				 * variable containing X to PostScript font
				 * translations */
    int landscape;		/* If non-zero, orient the page 90 degrees */
    int center;			/* If non-zero, center the graph on the page */
    int maxpect;		/* If non-zero, indicates to scale the graph
				 * so that it fills the page (maintaining the
				 * aspect ratio of the graph) */
    int addPreview;		/* If non-zero, generate a preview image and
				 * add it to the PostScript output */
    int footer;			/* If non-zero, a footer with the title, date
				 * and user will be added to the PostScript
				 * output outside of the bounding box. */
    int previewFormat;		/* Format of EPS preview:
				 * PS_PREVIEW_WMF, PS_PREVIEW_EPSI, or
				 * PS_PREVIEW_TIFF. */

    /* Computed fields */

    int left, bottom;		/* Bounding box of PostScript plot. */
    int right, top;

    double pageScale;		/* Scale of page. Set if "-maxpect" option
				 * is set, otherwise 1.0. */
} PostScript;

/*
 * -------------------------------------------------------------------
 *
 * Grid
 *
 *	Contains attributes of describing how to draw grids (at major
 *	ticks) in the graph.  Grids may be mapped to either/both x and
 *	y axis.
 *
 * -------------------------------------------------------------------
 */
typedef struct {
    GC gc;			/* Graphics context for the grid. */
    Axis2D axes;
    int hidden;			/* If non-zero, grid isn't displayed. */
    int minorGrid;		/* If non-zero, draw grid line for minor
				 * axis ticks too */
    Blt_Dashes dashes;		/* Dashstyle of the grid. This represents
				 * an array of alternatingly drawn pixel
				 * values. */
    int lineWidth;		/* Width of the grid lines */
    XColor *colorPtr;		/* Color of the grid lines */

    struct GridSegments {
	Segment2D *segments;	/* Array of line segments representing the
				 * x or y grid lines */
	int nSegments;		/* # of axis segments. */
    } x, y;	

} Grid;

/*
 * -------------------------------------------------------------------
 *
 * Crosshairs
 *
 *	Contains the line segments positions and graphics context used
 *	to simulate crosshairs (by XOR-ing) on the graph.
 *
 * -------------------------------------------------------------------
 */
typedef struct CrosshairsStruct Crosshairs;

typedef struct {
    short int width, height;	/* Extents of the margin */

    short int axesOffset;
    short int axesTitleLength;	/* Width of the widest title to be shown. 
				 * Multiple titles are displayed in 
				 * another margin. This is the minimum
				 * space requirement. */
    unsigned int nAxes;		/* Number of axes to be displayed */
    Blt_Chain *axes;		/* Extra axes associated with this margin */

    char *varName;		/* If non-NULL, name of variable to be
				 * updated when the margin size changes */

    int reqSize;		/* Requested size of margin */
    int site;			/* Indicates where margin is located: 
				 * left/right/top/bottom. */
} Margin;

#define MARGIN_NONE	-1
#define MARGIN_BOTTOM	0	
#define MARGIN_LEFT	1 
#define MARGIN_TOP	2			
#define MARGIN_RIGHT	3

#define rightMargin	margins[MARGIN_RIGHT]
#define leftMargin	margins[MARGIN_LEFT]
#define topMargin	margins[MARGIN_TOP]
#define bottomMargin	margins[MARGIN_BOTTOM]

/*
 * -------------------------------------------------------------------
 *
 * Graph --
 *
 *	Top level structure containing everything pertaining to
 *	the graph.
 *
 * -------------------------------------------------------------------
 */
struct GraphStruct {
    unsigned int flags;		/* Flags;  see below for definitions. */
    Tcl_Interp *interp;		/* Interpreter associated with graph */
    Tk_Window tkwin;		/* Window that embodies the graph.  NULL
				 * means that the window has been
				 * destroyed but the data structures
				 * haven't yet been cleaned up. */
    Display *display;		/* Display containing widget; needed,
				 * among other things, to release
				 * resources after tkwin has already gone
				 * away. */
    Tcl_Command cmdToken;	/* Token for graph's widget command. */

    char *data;			/* This value isn't used in C code.
				 * It may be used in Tcl bindings to
				 * associate extra data. */

    Tk_Cursor cursor;

    int inset;			/* Sum of focus highlight and 3-D
				 * border.  Indicates how far to 
				 * offset the graph from outside 
				 * edge of the window. */

    int borderWidth;		/* Width of the exterior border */
    int relief;			/* Relief of the exterior border */
    Tk_3DBorder border;		/* 3-D border used to delineate the plot
				 * surface and outer edge of window */
    
    int highlightWidth;		/* Width in pixels of highlight to draw
				 * around widget when it has the focus.
				 * <= 0 means don't draw a highlight. */
    XColor *highlightBgColor;	/* Color for drawing traversal highlight
				 * area when highlight is off. */
    XColor *highlightColor;	/* Color for drawing traversal highlight. */

    char *title;
    short int titleX, titleY;
    TextStyle titleTextStyle;	/* Graph title */
    
    char *takeFocus;
    
    int reqWidth, reqHeight;	/* Requested size of graph window */
    int width, height;		/* Size of graph window or PostScript
				 * page */
    
    Blt_HashTable penTable;	/* Table of pens */
    
    struct Component {
	Blt_HashTable table;	/* Hash table of ids. */
	Blt_Chain *displayList;	/* Display list. */
	Blt_HashTable tagTable;	/* Table of bind tags. */
    } elements, markers, axes;
    
    Blt_Uid classUid;		/* Default element type */

    Blt_BindTable bindTable;
    int nextMarkerId;		/* Tracks next marker identifier available */
    
    Blt_Chain *axisChain[4];	/* Chain of axes for each of the
				 * margins.  They're separate from the
				 * margin structures to make it easier
				 * to invert the X-Y axes by simply
				 * switching chain pointers.  
				 */
    Margin margins[4];
    
    PostScript *postscript;	/* PostScript options: see bltGrPS.c */
    Legend *legend;		/* Legend information: see bltGrLegd.c */
    Crosshairs *crosshairs;	/* Crosshairs information: see bltGrHairs.c */
    Grid *gridPtr;		/* Grid attribute information */

    int halo;			/* Maximum distance allowed between points
				 * when searching for a point */
    int inverted;		/* If non-zero, indicates the x and y axis
				 * positions should be inverted. */
    Blt_Tile tile;
    GC drawGC;			/* Used for drawing on the margins. This
				 * includes the axis lines */
    GC fillGC;			/* Used to fill the background of the
				 * margins. The fill is governed by
				 * the background color or the tiled
				 * pixmap. */
    int plotBorderWidth;	/* Width of interior 3-D border. */
    int plotRelief;		/* 3-d effect: TK_RELIEF_RAISED etc. */
    XColor *plotBg;		/* Color of plotting surface */

    GC plotFillGC;		/* Used to fill the plotting area with a
				 * solid background color. The fill color
				 * is stored in "plotBg". */

    /* If non-zero, force plot to conform to aspect ratio W/H */
    double aspect;

    short int left, right;	/* Coordinates of plot bbox */
    short int top, bottom;	

    Blt_Pad padX;		/* Vertical padding for plotarea */
    int vRange, vOffset;	/* Vertical axis range and offset from the
				 * left side of the graph window. Used to
				 * transform coordinates to vertical
				 * axes. */
    Blt_Pad padY;		/* Horizontal padding for plotarea */
    int hRange, hOffset;	/* Horizontal axis range and offset from
				 * the top of the graph window. Used to
				 * transform horizontal axes */
    double vScale, hScale;

    int doubleBuffer;		/* If non-zero, draw the graph into a pixmap
				 * first to reduce flashing. */
    int backingStore;		/* If non-zero, cache elements by drawing
				 * them into a pixmap */
    Pixmap backPixmap;		/* Pixmap used to cache elements
				 * displayed.  If *backingStore* is
				 * non-zero, each element is drawn
				 * into this pixmap before it is
				 * copied onto the screen.  The pixmap
				 * then acts as a cache (only the
				 * pixmap is redisplayed if the none
				 * of elements have changed). This is
				 * done so that markers can be redrawn
				 * quickly over elements without
				 * redrawing each element. */
    int backWidth, backHeight;	/* Size of element backing store pixmap. */

    /*
     * barchart specific information
     */
    double baseline;		/* Baseline from bar chart.  */
    double barWidth;		/* Default width of each bar in graph units.
				 * The default width is 1.0 units. */
    BarMode mode;		/* Mode describing how to display bars
				 * with the same x-coordinates. Mode can
				 * be "stack", "align", or "normal" */
    FreqInfo *freqArr;		/* Contains information about duplicate
				 * x-values in bar elements (malloc-ed).
				 * This information can also be accessed
				 * by the frequency hash table */
    Blt_HashTable freqTable;	/* */
    int nStacks;		/* Number of entries in frequency array.
				 * If zero, indicates nothing special needs
				 * to be done for "stack" or "align" modes */
    char *dataCmd;		/* New data callback? */

};

/*
 * Bit flags definitions:
 *
 * 	All kinds of state information kept here.  All these
 *	things happen when the window is available to draw into
 *	(DisplayGraph). Need the window width and height before
 *	we can calculate graph layout (i.e. the screen coordinates
 *	of the axes, elements, titles, etc). But we want to do this
 *	only when we have to, not every time the graph is redrawn.
 *
 *	Same goes for maintaining a pixmap to double buffer graph
 *	elements.  Need to mark when the pixmap needs to updated.
 *
 *
 *	MAP_ITEM		Indicates that the element/marker/axis
 *				configuration has changed such that
 *				its layout of the item (i.e. its
 *				position in the graph window) needs
 *				to be recalculated.
 *
 *	MAP_ALL			Indicates that the layout of the axes and 
 *				all elements and markers and the graph need 
 *				to be recalculated. Otherwise, the layout
 *				of only those markers and elements that
 *				have changed will be reset. 
 *
 *	GET_AXIS_GEOMETRY	Indicates that the size of the axes needs 
 *				to be recalculated. 
 *
 *	RESET_AXES		Flag to call to Blt_ResetAxes routine.  
 *				This routine recalculates the scale offset
 *				(used for mapping coordinates) of each axis.
 *				If an axis limit has changed, then it sets 
 *				flags to re-layout and redraw the entire 
 *				graph.  This needs to happend before the axis
 *				can compute transformations between graph and 
 *				screen coordinates. 
 *
 *	LAYOUT_NEEDED		
 *
 *	REDRAW_BACKING_STORE	If set, redraw all elements into the pixmap 
 *				used for buffering elements. 
 *
 *	REDRAW_PENDING		Non-zero means a DoWhenIdle handler has 
 *				already been queued to redraw this window. 
 *
 *	DRAW_LEGEND		Non-zero means redraw the legend. If this is 
 *				the only DRAW_* flag, the legend display 
 *				routine is called instead of the graph 
 *				display routine. 
 *
 *	DRAW_MARGINS		Indicates that the margins bordering 
 *				the plotting area need to be redrawn. 
 *				The possible reasons are:
 *
 *				1) an axis configuration changed
 *				2) an axis limit changed
 *				3) titles have changed
 *				4) window was resized. 
 *
 *	GRAPH_FOCUS	
 */

#define	MAP_ITEM		(1<<0) /* 0x0001 */
#define	MAP_ALL			(1<<1) /* 0x0002 */
#define	GET_AXIS_GEOMETRY	(1<<2) /* 0x0004 */
#define RESET_AXES		(1<<3) /* 0x0008 */
#define LAYOUT_NEEDED		(1<<4) /* 0x0010 */

#define REDRAW_PENDING		(1<<8) /* 0x0100 */
#define DRAW_LEGEND		(1<<9) /* 0x0200 */
#define DRAW_MARGINS		(1<<10)/* 0x0400 */
#define	REDRAW_BACKING_STORE	(1<<11)/* 0x0800 */

#define GRAPH_FOCUS		(1<<12)/* 0x1000 */
#define DATA_CHANGED		(1<<13)/* 0x2000 */

#define	MAP_WORLD		(MAP_ALL|RESET_AXES|GET_AXIS_GEOMETRY)
#define REDRAW_WORLD		(DRAW_MARGINS | DRAW_LEGEND)
#define RESET_WORLD		(REDRAW_WORLD | MAP_WORLD)

/*
 * ---------------------- Forward declarations ------------------------
 */

extern int Blt_CreatePostScript _ANSI_ARGS_((Graph *graphPtr));
extern int Blt_CreateCrosshairs _ANSI_ARGS_((Graph *graphPtr));
extern int Blt_CreateGrid _ANSI_ARGS_((Graph *graphPtr));
extern double Blt_InvHMap _ANSI_ARGS_((Graph *graphPtr, Axis *axisPtr, 
	double x));
extern double Blt_InvVMap _ANSI_ARGS_((Graph *graphPtr, Axis *axisPtr, 
	double x));
extern double Blt_HMap _ANSI_ARGS_((Graph *graphPtr, Axis *axisPtr, double x));
extern double Blt_VMap _ANSI_ARGS_((Graph *graphPtr, Axis *axisPtr, double y));
extern Point2D Blt_InvMap2D _ANSI_ARGS_((Graph *graphPtr, double x,
	double y, Axis2D *pairPtr));
extern Point2D Blt_Map2D _ANSI_ARGS_((Graph *graphPtr, double x,
	double y, Axis2D *pairPtr));
extern Graph *Blt_GetGraphFromWindowData _ANSI_ARGS_((Tk_Window tkwin));
extern void Blt_AdjustAxisPointers _ANSI_ARGS_((Graph *graphPtr));
extern int Blt_LineRectClip _ANSI_ARGS_((Extents2D *extsPtr, Point2D *p,
	Point2D *q));
extern int Blt_PolyRectClip _ANSI_ARGS_((Extents2D *extsPtr, Point2D *inputPts,
	int nInputPts, Point2D *outputPts));

extern void Blt_ComputeStacks _ANSI_ARGS_((Graph *graphPtr));
extern void Blt_ConfigureCrosshairs _ANSI_ARGS_((Graph *graphPtr));
extern void Blt_DestroyAxes _ANSI_ARGS_((Graph *graphPtr));
extern void Blt_DestroyCrosshairs _ANSI_ARGS_((Graph *graphPtr));
extern void Blt_DestroyGrid _ANSI_ARGS_((Graph *graphPtr));
extern void Blt_DestroyElements _ANSI_ARGS_((Graph *graphPtr));
extern void Blt_DestroyMarkers _ANSI_ARGS_((Graph *graphPtr));
extern void Blt_DestroyPostScript _ANSI_ARGS_((Graph *graphPtr));
extern void Blt_DrawAxes _ANSI_ARGS_((Graph *graphPtr, Drawable drawable));
extern void Blt_DrawAxisLimits _ANSI_ARGS_((Graph *graphPtr,
	Drawable drawable));
extern void Blt_DrawElements _ANSI_ARGS_((Graph *graphPtr, Drawable drawable));
extern void Blt_DrawActiveElements _ANSI_ARGS_((Graph *graphPtr,
	Drawable drawable));
extern void Blt_DrawGraph _ANSI_ARGS_((Graph *graphPtr, Drawable drawable,
	int backingStore));
extern void Blt_DrawGrid _ANSI_ARGS_((Graph *graphPtr, Drawable drawable));
extern void Blt_DrawMarkers _ANSI_ARGS_((Graph *graphPtr, Drawable drawable,
	int under));
extern void Blt_Draw2DSegments _ANSI_ARGS_((Display *display, 
	Drawable drawable, GC gc, Segment2D *segments, int nSegments));
extern int Blt_GetCoordinate _ANSI_ARGS_((Tcl_Interp *interp,
	char *expr, double *valuePtr));
extern void Blt_InitFreqTable _ANSI_ARGS_((Graph *graphPtr));
extern void Blt_LayoutGraph _ANSI_ARGS_((Graph *graphPtr));
extern void Blt_LayoutMargins _ANSI_ARGS_((Graph *graphPtr));
extern void Blt_EventuallyRedrawGraph _ANSI_ARGS_((Graph *graphPtr));
extern void Blt_ResetAxes _ANSI_ARGS_((Graph *graphPtr));
extern void Blt_ResetStacks _ANSI_ARGS_((Graph *graphPtr));
extern void Blt_GraphExtents _ANSI_ARGS_((Graph *graphPtr, Extents2D *extsPtr));
extern void Blt_DisableCrosshairs _ANSI_ARGS_((Graph *graphPtr));
extern void Blt_EnableCrosshairs _ANSI_ARGS_((Graph *graphPtr));
extern void Blt_MapAxes _ANSI_ARGS_((Graph *graphPtr));
extern void Blt_MapElements _ANSI_ARGS_((Graph *graphPtr));
extern void Blt_MapGraph _ANSI_ARGS_((Graph *graphPtr));
extern void Blt_MapMarkers _ANSI_ARGS_((Graph *graphPtr));
extern void Blt_MapGrid _ANSI_ARGS_((Graph *graphPtr));
extern void Blt_UpdateCrosshairs _ANSI_ARGS_((Graph *graphPtr));
extern void Blt_DestroyPens _ANSI_ARGS_((Graph *graphPtr));
extern int Blt_GetPen _ANSI_ARGS_((Graph *graphPtr, char *name, 
	Blt_Uid classUid, Pen **penPtrPtr));
extern Pen *Blt_BarPen _ANSI_ARGS_((char *penName));
extern Pen *Blt_LinePen _ANSI_ARGS_((char *penName));
extern Pen *Blt_CreatePen _ANSI_ARGS_((Graph *graphPtr, char *penName,
	Blt_Uid classUid, int nOpts, char **options));
extern int Blt_InitLinePens _ANSI_ARGS_((Graph *graphPtr));
extern int Blt_InitBarPens _ANSI_ARGS_((Graph *graphPtr));
extern void Blt_FreePen _ANSI_ARGS_((Graph *graphPtr, Pen *penPtr));

extern int Blt_VirtualAxisOp _ANSI_ARGS_((Graph *graphPtr, Tcl_Interp *interp,
	int argc, char **argv));
extern int Blt_AxisOp _ANSI_ARGS_((Graph *graphPtr, int margin, int argc, 
	char **argv));
extern int Blt_ElementOp _ANSI_ARGS_((Graph *graphPtr, Tcl_Interp *interp,
	int argc, char **argv, Blt_Uid classUid));
extern int Blt_GridOp _ANSI_ARGS_((Graph *graphPtr, Tcl_Interp *interp,
	int argc, char **argv));
extern int Blt_CrosshairsOp _ANSI_ARGS_((Graph *graphPtr, Tcl_Interp *interp,
	int argc, char **argv));
extern int Blt_MarkerOp _ANSI_ARGS_((Graph *graphPtr, Tcl_Interp *interp,
	int argc, char **argv));
extern int Blt_PenOp _ANSI_ARGS_((Graph *graphPtr, Tcl_Interp *interp,
	int argc, char **argv));
extern int Blt_PointInPolygon _ANSI_ARGS_((Point2D *samplePtr, 
	Point2D *screenPts, int nScreenPts));
extern int Blt_RegionInPolygon _ANSI_ARGS_((Extents2D *extsPtr, Point2D *points,
	int nPoints, int enclosed));
extern int Blt_PointInSegments _ANSI_ARGS_((Point2D *samplePtr, 
	Segment2D *segments, int nSegments, double halo));
extern int Blt_PostScriptOp _ANSI_ARGS_((Graph *graphPtr, Tcl_Interp *interp,
	int argc, char **argv));
extern int Blt_GraphUpdateNeeded _ANSI_ARGS_((Graph *graphPtr));
extern int Blt_DefaultAxes _ANSI_ARGS_((Graph *graphPtr));
extern Axis *Blt_GetFirstAxis _ANSI_ARGS_((Blt_Chain *chainPtr));
extern void Blt_UpdateAxisBackgrounds _ANSI_ARGS_((Graph *graphPtr));
extern void Blt_GetAxisSegments _ANSI_ARGS_((Graph *graphPtr, Axis *axisPtr,
	Segment2D **segPtrPtr, int *nSegmentsPtr));
extern Marker *Blt_NearestMarker _ANSI_ARGS_((Graph *graphPtr, int x, int y,
	int under));
extern Axis *Blt_NearestAxis _ANSI_ARGS_((Graph *graphPtr, int x, int y));


typedef ClientData (MakeTagProc) _ANSI_ARGS_((Graph *graphPtr, char *tagName));
extern MakeTagProc Blt_MakeElementTag;
extern MakeTagProc Blt_MakeMarkerTag;
extern MakeTagProc Blt_MakeAxisTag;

extern Blt_BindTagProc Blt_GraphTags;
extern Blt_BindTagProc Blt_AxisTags;

extern int Blt_GraphType _ANSI_ARGS_((Graph *graphPtr));

/* ---------------------- Global declarations ------------------------ */

extern Blt_Uid bltBarElementUid;
extern Blt_Uid bltLineElementUid;
extern Blt_Uid bltStripElementUid;
extern Blt_Uid bltLineMarkerUid;
extern Blt_Uid bltBitmapMarkerUid;
extern Blt_Uid bltImageMarkerUid;
extern Blt_Uid bltTextMarkerUid;
extern Blt_Uid bltPolygonMarkerUid;
extern Blt_Uid bltWindowMarkerUid;
extern Blt_Uid bltXAxisUid;
extern Blt_Uid bltYAxisUid;

#endif /* _BLT_GRAPH_H */
