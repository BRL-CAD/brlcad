
/*
 * bltGrHairs.c --
 *
 *	This module implements crosshairs for the BLT graph widget.
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
 *
 * Graph widget created by Sani Nassif and George Howlett.
*/

#include "bltGraph.h"

extern Tk_CustomOption bltPointOption;
extern Tk_CustomOption bltDistanceOption;
extern Tk_CustomOption bltDashesOption;

/*
 * -------------------------------------------------------------------
 *
 * Crosshairs
 *
 *	Contains the line segments positions and graphics context used
 *	to simulate crosshairs (by XORing) on the graph.
 *
 * -------------------------------------------------------------------
 */

struct CrosshairsStruct {

    XPoint hotSpot;		/* Hot spot for crosshairs */
    int visible;		/* Internal state of crosshairs. If non-zero,
				 * crosshairs are displayed. */
    int hidden;			/* If non-zero, crosshairs are not displayed.
				 * This is not necessarily consistent with the
				 * internal state variable.  This is true when
				 * the hot spot is off the graph.  */
    Blt_Dashes dashes;		/* Dashstyle of the crosshairs. This represents
				 * an array of alternatingly drawn pixel
				 * values. If NULL, the hairs are drawn as a
				 * solid line */
    int lineWidth;		/* Width of the simulated crosshair lines */
    XSegment segArr[2];		/* Positions of line segments representing the
				 * simulated crosshairs. */
    XColor *colorPtr;		/* Foreground color of crosshairs */
    GC gc;			/* Graphics context for crosshairs. Set to
				 * GXxor to not require redraws of graph */
};

#define DEF_HAIRS_DASHES	(char *)NULL
#define DEF_HAIRS_FOREGROUND	RGB_BLACK
#define DEF_HAIRS_FG_MONO	RGB_BLACK
#define DEF_HAIRS_LINE_WIDTH	"0"
#define DEF_HAIRS_HIDE		"yes"
#define DEF_HAIRS_POSITION	(char *)NULL

static Tk_ConfigSpec configSpecs[] =
{
    {TK_CONFIG_COLOR, "-color", "color", "Color",
	DEF_HAIRS_FOREGROUND, Tk_Offset(Crosshairs, colorPtr), TK_CONFIG_COLOR_ONLY},
    {TK_CONFIG_COLOR, "-color", "color", "Color",
	DEF_HAIRS_FG_MONO, Tk_Offset(Crosshairs, colorPtr), TK_CONFIG_MONO_ONLY},
    {TK_CONFIG_CUSTOM, "-dashes", "dashes", "Dashes",
	DEF_HAIRS_DASHES, Tk_Offset(Crosshairs, dashes),
	TK_CONFIG_NULL_OK, &bltDashesOption},
    {TK_CONFIG_BOOLEAN, "-hide", "hide", "Hide",
	DEF_HAIRS_HIDE, Tk_Offset(Crosshairs, hidden),
	TK_CONFIG_DONT_SET_DEFAULT},
    {TK_CONFIG_CUSTOM, "-linewidth", "lineWidth", "Linewidth",
	DEF_HAIRS_LINE_WIDTH, Tk_Offset(Crosshairs, lineWidth),
	TK_CONFIG_DONT_SET_DEFAULT, &bltDistanceOption},
    {TK_CONFIG_CUSTOM, "-position", "position", "Position",
	DEF_HAIRS_POSITION, Tk_Offset(Crosshairs, hotSpot),
	0, &bltPointOption},
    {TK_CONFIG_END, NULL, NULL, NULL, NULL, 0, 0}
};

/*
 *----------------------------------------------------------------------
 *
 * TurnOffHairs --
 *
 *	XOR's the existing line segments (representing the crosshairs),
 *	thereby erasing them.  The internal state of the crosshairs is
 *	tracked.
 *
 * Results:
 *	None
 *
 * Side Effects:
 *	Crosshairs are erased.
 *
 *----------------------------------------------------------------------
 */
static void
TurnOffHairs(tkwin, chPtr)
    Tk_Window tkwin;
    Crosshairs *chPtr;
{
    if (Tk_IsMapped(tkwin) && (chPtr->visible)) {
	XDrawSegments(Tk_Display(tkwin), Tk_WindowId(tkwin), chPtr->gc,
	    chPtr->segArr, 2);
	chPtr->visible = FALSE;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TurnOnHairs --
 *
 *	Draws (by XORing) new line segments, creating the effect of
 *	crosshairs. The internal state of the crosshairs is tracked.
 *
 * Results:
 *	None
 *
 * Side Effects:
 *	Crosshairs are displayed.
 *
 *----------------------------------------------------------------------
 */
static void
TurnOnHairs(graphPtr, chPtr)
    Graph *graphPtr;
    Crosshairs *chPtr;
{
    if (Tk_IsMapped(graphPtr->tkwin) && (!chPtr->visible)) {
	if (!PointInGraph(graphPtr, chPtr->hotSpot.x, chPtr->hotSpot.y)) {
	    return;		/* Coordinates are off the graph */
	}
	XDrawSegments(graphPtr->display, Tk_WindowId(graphPtr->tkwin),
	    chPtr->gc, chPtr->segArr, 2);
	chPtr->visible = TRUE;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * ConfigureCrosshairs --
 *
 *	Configures attributes of the crosshairs such as line width,
 *	dashes, and position.  The crosshairs are first turned off
 *	before any of the attributes changes.
 *
 * Results:
 *	None
 *
 * Side Effects:
 *	Crosshair GC is allocated.
 *
 *----------------------------------------------------------------------
 */
void
Blt_ConfigureCrosshairs(graphPtr)
    Graph *graphPtr;
{
    XGCValues gcValues;
    unsigned long gcMask;
    GC newGC;
    long colorValue;
    Crosshairs *chPtr = graphPtr->crosshairs;

    /*
     * Turn off the crosshairs temporarily. This is in case the new
     * configuration changes the size, style, or position of the lines.
     */
    TurnOffHairs(graphPtr->tkwin, chPtr);

    gcValues.function = GXxor;

    if (graphPtr->plotBg == NULL) {
	/* The graph's color option may not have been set yet */
	colorValue = WhitePixelOfScreen(Tk_Screen(graphPtr->tkwin));
    } else {
	colorValue = graphPtr->plotBg->pixel;
    }
    gcValues.background = colorValue;
    gcValues.foreground = (colorValue ^ chPtr->colorPtr->pixel);

    gcValues.line_width = LineWidth(chPtr->lineWidth);
    gcMask = (GCForeground | GCBackground | GCFunction | GCLineWidth);
    if (LineIsDashed(chPtr->dashes)) {
	gcValues.line_style = LineOnOffDash;
	gcMask |= GCLineStyle;
    }
    newGC = Blt_GetPrivateGC(graphPtr->tkwin, gcMask, &gcValues);
    if (LineIsDashed(chPtr->dashes)) {
	Blt_SetDashes(graphPtr->display, newGC, &(chPtr->dashes));
    }
    if (chPtr->gc != NULL) {
	Blt_FreePrivateGC(graphPtr->display, chPtr->gc);
    }
    chPtr->gc = newGC;

    /*
     * Are the new coordinates on the graph?
     */
    chPtr->segArr[0].x2 = chPtr->segArr[0].x1 = chPtr->hotSpot.x;
    chPtr->segArr[0].y1 = graphPtr->bottom;
    chPtr->segArr[0].y2 = graphPtr->top;
    chPtr->segArr[1].y2 = chPtr->segArr[1].y1 = chPtr->hotSpot.y;
    chPtr->segArr[1].x1 = graphPtr->left;
    chPtr->segArr[1].x2 = graphPtr->right;

    if (!chPtr->hidden) {
	TurnOnHairs(graphPtr, chPtr);
    }
}

void
Blt_EnableCrosshairs(graphPtr)
    Graph *graphPtr;
{
    if (!graphPtr->crosshairs->hidden) {
	TurnOnHairs(graphPtr, graphPtr->crosshairs);
    }
}

void
Blt_DisableCrosshairs(graphPtr)
    Graph *graphPtr;
{
    if (!graphPtr->crosshairs->hidden) {
	TurnOffHairs(graphPtr->tkwin, graphPtr->crosshairs);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * UpdateCrosshairs --
 *
 *	Update the length of the hairs (not the hot spot).
 *
 * Results:
 *	None.
 *
 *----------------------------------------------------------------------
 */
void
Blt_UpdateCrosshairs(graphPtr)
    Graph *graphPtr;
{
    Crosshairs *chPtr = graphPtr->crosshairs;

    chPtr->segArr[0].y1 = graphPtr->bottom;
    chPtr->segArr[0].y2 = graphPtr->top;
    chPtr->segArr[1].x1 = graphPtr->left;
    chPtr->segArr[1].x2 = graphPtr->right;
}

/*
 *----------------------------------------------------------------------
 *
 * Blt_DestroyCrosshairs --
 *
 * Results:
 *	None
 *
 * Side Effects:
 *	Crosshair GC is allocated.
 *
 *----------------------------------------------------------------------
 */
void
Blt_DestroyCrosshairs(graphPtr)
    Graph *graphPtr;
{
    Crosshairs *chPtr = graphPtr->crosshairs;

    Tk_FreeOptions(configSpecs, (char *)chPtr, graphPtr->display, 0);
    if (chPtr->gc != NULL) {
	Blt_FreePrivateGC(graphPtr->display, chPtr->gc);
    }
    Blt_Free(chPtr);
}

/*
 *----------------------------------------------------------------------
 *
 * Blt_CreateCrosshairs --
 *
 *	Creates and initializes a new crosshair structure.
 *
 * Results:
 *	Returns TCL_ERROR if the crosshair structure can't be created,
 *	otherwise TCL_OK.
 *
 * Side Effects:
 *	Crosshair GC is allocated.
 *
 *----------------------------------------------------------------------
 */
int
Blt_CreateCrosshairs(graphPtr)
    Graph *graphPtr;
{
    Crosshairs *chPtr;

    chPtr = Blt_Calloc(1, sizeof(Crosshairs));
    assert(chPtr);
    chPtr->hidden = TRUE;
    chPtr->hotSpot.x = chPtr->hotSpot.y = -1;
    graphPtr->crosshairs = chPtr;

    if (Blt_ConfigureWidgetComponent(graphPtr->interp, graphPtr->tkwin,
	    "crosshairs", "Crosshairs", configSpecs, 0, (char **)NULL,
	    (char *)chPtr, 0) != TCL_OK) {
	return TCL_ERROR;
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * CgetOp --
 *
 *	Queries configuration attributes of the crosshairs such as
 *	line width, dashes, and position.
 *
 * Results:
 *	A standard Tcl result.
 *
 *----------------------------------------------------------------------
 */
/* ARGSUSED */
static int
CgetOp(graphPtr, interp, argc, argv)
    Graph *graphPtr;
    Tcl_Interp *interp;
    int argc;			/* Not used. */
    char **argv;
{
    Crosshairs *chPtr = graphPtr->crosshairs;

    return Tk_ConfigureValue(interp, graphPtr->tkwin, configSpecs,
	    (char *)chPtr, argv[3], 0);
}

/*
 *----------------------------------------------------------------------
 *
 * ConfigureOp --
 *
 *	Queries or resets configuration attributes of the crosshairs
 * 	such as line width, dashes, and position.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side Effects:
 *	Crosshairs are reset.
 *
 *----------------------------------------------------------------------
 */
static int
ConfigureOp(graphPtr, interp, argc, argv)
    Graph *graphPtr;
    Tcl_Interp *interp;
    int argc;
    char **argv;
{
    Crosshairs *chPtr = graphPtr->crosshairs;

    if (argc == 3) {
	return Tk_ConfigureInfo(interp, graphPtr->tkwin, configSpecs,
		(char *)chPtr, (char *)NULL, 0);
    } else if (argc == 4) {
	return Tk_ConfigureInfo(interp, graphPtr->tkwin, configSpecs,
		(char *)chPtr, argv[3], 0);
    }
    if (Tk_ConfigureWidget(interp, graphPtr->tkwin, configSpecs, argc - 3,
	    argv + 3, (char *)chPtr, TK_CONFIG_ARGV_ONLY) != TCL_OK) {
	return TCL_ERROR;
    }
    Blt_ConfigureCrosshairs(graphPtr);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * OnOp --
 *
 *	Maps the crosshairs.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side Effects:
 *	Crosshairs are reset if necessary.
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
OnOp(graphPtr, interp, argc, argv)
    Graph *graphPtr;
    Tcl_Interp *interp;
    int argc;
    char **argv;
{
    Crosshairs *chPtr = graphPtr->crosshairs;

    if (chPtr->hidden) {
	TurnOnHairs(graphPtr, chPtr);
	chPtr->hidden = FALSE;
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * OffOp --
 *
 *	Unmaps the crosshairs.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side Effects:
 *	Crosshairs are reset if necessary.
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
OffOp(graphPtr, interp, argc, argv)
    Graph *graphPtr;
    Tcl_Interp *interp;
    int argc;
    char **argv;
{
    Crosshairs *chPtr = graphPtr->crosshairs;

    if (!chPtr->hidden) {
	TurnOffHairs(graphPtr->tkwin, chPtr);
	chPtr->hidden = TRUE;
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * ToggleOp --
 *
 *	Toggles the state of the crosshairs.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side Effects:
 *	Crosshairs are reset.
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
ToggleOp(graphPtr, interp, argc, argv)
    Graph *graphPtr;
    Tcl_Interp *interp;
    int argc;
    char **argv;
{
    Crosshairs *chPtr = graphPtr->crosshairs;

    chPtr->hidden = (chPtr->hidden == 0);
    if (chPtr->hidden) {
	TurnOffHairs(graphPtr->tkwin, chPtr);
    } else {
	TurnOnHairs(graphPtr, chPtr);
    }
    return TCL_OK;
}


static Blt_OpSpec xhairOps[] =
{
    {"cget", 2, (Blt_Op)CgetOp, 4, 4, "option",},
    {"configure", 2, (Blt_Op)ConfigureOp, 3, 0, "?options...?",},
    {"off", 2, (Blt_Op)OffOp, 3, 3, "",},
    {"on", 2, (Blt_Op)OnOp, 3, 3, "",},
    {"toggle", 1, (Blt_Op)ToggleOp, 3, 3, "",},
};
static int nXhairOps = sizeof(xhairOps) / sizeof(Blt_OpSpec);

/*
 *----------------------------------------------------------------------
 *
 * Blt_CrosshairsOp --
 *
 *	User routine to configure crosshair simulation.  Crosshairs
 *	are simulated by drawing line segments parallel to both axes
 *	using the XOR drawing function. The allows the lines to be
 *	erased (by drawing them again) without redrawing the entire
 *	graph.  Care must be taken to erase crosshairs before redrawing
 *	the graph and redraw them after the graph is redraw.
 *
 * Results:
 *	The return value is a standard Tcl result.
 *
 * Side Effects:
 *	Crosshairs may be drawn in the plotting area.
 *
 *----------------------------------------------------------------------
 */
int
Blt_CrosshairsOp(graphPtr, interp, argc, argv)
    Graph *graphPtr;
    Tcl_Interp *interp;
    int argc;
    char **argv;
{
    Blt_Op proc;

    proc = Blt_GetOp(interp, nXhairOps, xhairOps, BLT_OP_ARG2, argc, argv, 0);
    if (proc == NULL) {
	return TCL_ERROR;
    }
    return (*proc) (graphPtr, interp, argc, argv);
}
