/*
 * bltGrAxis.h --
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

#ifndef _BLT_GR_AXIS_H
#define _BLT_GR_AXIS_H

#include "bltList.h"

/*
 * -------------------------------------------------------------------
 *
 * AxisRange --
 *
 *	Designates a range of values by a minimum and maximum limit.
 *
 * -------------------------------------------------------------------
 */
typedef struct {
    double min, max, range, scale;
} AxisRange;

/*
 * ----------------------------------------------------------------------
 *
 * TickLabel --
 *
 * 	Structure containing the X-Y screen coordinates of the tick
 * 	label (anchored at its center).
 *
 * ----------------------------------------------------------------------
 */
typedef struct {
    Point2D anchorPos;
    int width, height;
    char string[1];
} TickLabel;

/*
 * ----------------------------------------------------------------------
 *
 * Ticks --
 *
 * 	Structure containing information where the ticks (major or
 *	minor) will be displayed on the graph.
 *
 * ----------------------------------------------------------------------
 */
typedef struct {
    int nTicks;			/* # of ticks on axis */
    double values[1];		/* Array of tick values (malloc-ed). */
} Ticks;

/*
 * ----------------------------------------------------------------------
 *
 * TickSweep --
 *
 * 	Structure containing information where the ticks (major or
 *	minor) will be displayed on the graph.
 *
 * ----------------------------------------------------------------------
 */
typedef struct {
    double initial;		/* Initial value */
    double step;		/* Size of interval */
    int nSteps;			/* Number of intervals. */
} TickSweep;

/*
 * ----------------------------------------------------------------------
 *
 * Axis --
 *
 * 	Structure contains options controlling how the axis will be
 * 	displayed.
 *
 * ----------------------------------------------------------------------
 */
typedef struct {
    char *name;			/* Identifier to refer the element.
				 * Used in the "insert", "delete", or
				 * "show", commands. */

    Blt_Uid classUid;		/* Type of axis. */

    Graph *graphPtr;		/* Graph widget of element*/

    unsigned int flags;		/* Set bit field definitions below */

    /*
     * AXIS_DRAWN		Axis is designated as a logical axis
     * AXIS_DIRTY
     *
     * AXIS_CONFIG_MAJOR	User specified major ticks.
     * AXIS_CONFIG_MINOR	User specified minor ticks.
     */

    char **tags;

    char *detail;

    int deletePending;		/* Indicates that the axis was
				 * scheduled for deletion. The actual
				 * deletion may be deferred until the
				 * axis is no longer in use.  */

    int refCount;		/* Number of elements referencing this
				 * axis. */

    Blt_HashEntry *hashPtr;	/* Points to axis entry in hash
				 * table. Used to quickly remove axis
				 * entries. */

    int logScale;		/* If non-zero, scale the axis values
				 * logarithmically. */

    int hidden;			/* If non-zero, don't display the
				 * axis title, ticks, or line. */

    int showTicks;		/* If non-zero, display tick marks and
				 * labels. */

    int descending;		/* If non-zero, display the range of
				 * values on the axis in descending
				 * order, from high to low. */

    int looseMin, looseMax;	/* If non-zero, axis range extends to
				 * the outer major ticks, otherwise at
				 * the limits of the data values. This
				 * is overriddened by setting the -min
				 * and -max options.  */

    char *title;		/* Title of the axis. */

    TextStyle titleTextStyle;	/* Text attributes (color, font,
				 * rotation, etc.)  of the axis
				 * title. */

    int titleAlternate;		/* Indicates whether to position the
				 * title above/left of the axis. */

    Point2D titlePos;		/* Position of the title */

    unsigned short int titleWidth, titleHeight;	

    int lineWidth;		/* Width of lines representing axis
				 * (including ticks).  If zero, then
				 * no axis lines or ticks are
				 * drawn. */

    char **limitsFormats;	/* One or two strings of sprintf-like
				 * formats describing how to display
				 * virtual axis limits. If NULL,
				 * display no limits. */
    int nFormats;

    TextStyle limitsTextStyle;	/* Text attributes (color, font,
				 * rotation, etc.)  of the limits. */

    double windowSize;		/* Size of a sliding window of values
				 * used to scale the axis automatically
				 * as new data values are added. The axis
				 * will always display the latest values
				 * in this range. */

    double shiftBy;		/* Shift maximum by this interval. */

    int tickLength;		/* Length of major ticks in pixels */

    TextStyle tickTextStyle;	/* Text attributes (color, font, rotation, 
				 * etc.) for labels at each major tick. */

    char *formatCmd;		/* Specifies a Tcl command, to be invoked
				 * by the axis whenever it has to generate 
				 * tick labels. */

    char *scrollCmdPrefix;
    int scrollUnits;

    double min, max;		/* The actual axis range. */

    double reqMin, reqMax;	/* Requested axis bounds. Consult the
				 * axisPtr->flags field for
				 * AXIS_CONFIG_MIN and AXIS_CONFIG_MAX
				 * to see if the requested bound have
				 * been set.  They override the
				 * computed range of the axis
				 * (determined by auto-scaling). */

    double scrollMin, scrollMax;/* Defines the scrolling reqion of the axis.
				 * Normally the region is determined from 
				 * the data limits. If specified, these 
				 * values override the data-range. */

    AxisRange valueRange;	/* Range of data values of elements mapped 
				 * to this axis. This is used to auto-scale 
				 * the axis in "tight" mode. */

    AxisRange axisRange;	/* Smallest and largest major tick values
				 * for the axis.  The tick values lie outside
				 * the range of data values.  This is used to
				 * auto-scale the axis in "loose" mode. */

    double prevMin, prevMax;

    double reqStep;		/* If > 0.0, overrides the computed major 
				 * tick interval.  Otherwise a stepsize 
				 * is automatically calculated, based 
				 * upon the range of elements mapped to the 
				 * axis. The default value is 0.0. */

    double tickZoom;		/* If > 0.0, overrides the computed major 
				 * tick interval.  Otherwise a stepsize 
				 * is automatically calculated, based 
				 * upon the range of elements mapped to the 
				 * axis. The default value is 0.0. */


    GC tickGC;			/* Graphics context for axis and tick labels */

    Ticks *t1Ptr;		/* Array of major tick positions. May be
				 * set by the user or generated from the 
				 * major sweep below. */

    Ticks *t2Ptr;		/* Array of minor tick positions. May be
				 * set by the user or generated from the
				 * minor sweep below. */

    TickSweep minorSweep, majorSweep;

    int reqNumMinorTicks;	/* If non-zero, represents the
				 * requested the number of minor ticks
				 * to be uniformally displayed along
				 * each major tick. */


    int labelOffset;		/* If non-zero, indicates that the tick
				 * label should be offset to sit in the
				 * middle of the next interval. */

    /* The following fields are specific to logical axes */

    Blt_ChainLink *linkPtr;	/* Axis link in margin list. */
    Blt_Chain *chainPtr;

    short int width, height;	/* Extents of axis */

    Segment2D *segments;	/* Array of line segments representing
				 * the major and minor ticks, but also
				 * the axis line itself. The segment
				 * coordinates are relative to the
				 * axis. */

    int nSegments;		/* Number of segments in the above array. */

    Blt_Chain *tickLabels;	/* Contains major tick label strings 
				 * and their offsets along the axis. */
    Region2D region;

    Tk_3DBorder border;
    int borderWidth;
    int relief;
} Axis;

#define AXIS_CONFIG_MAJOR (1<<4) /* User specified major tick intervals. */
#define AXIS_CONFIG_MINOR (1<<5) /* User specified minor tick intervals. */
#define AXIS_ONSCREEN	  (1<<6) /* Axis is displayed on the screen via
				  * the "use" operation */
#define AXIS_DIRTY	  (1<<7)
#define AXIS_ALLOW_NULL   (1<<12)

/*
 * -------------------------------------------------------------------
 *
 * Axis2D --
 *
 *	The pair of axes mapping a point onto the graph.
 *
 * -------------------------------------------------------------------
 */
typedef struct {
    Axis *x, *y;
} Axis2D;

#endif /* _BLT_GR_AXIS_H */
