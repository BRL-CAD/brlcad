/*
 * bltTable.h --
 *
 *	This module implements a table-based geometry manager
 *	for the BLT toolkit.
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
 *	The table geometry manager was created by George Howlett.
 */

#ifndef _BLT_TABLE_H
#define _BLT_TABLE_H

#include "bltChain.h"
#include "bltHash.h"
#include "bltList.h"

typedef struct {
    Blt_HashTable tableTable;	/* Hash table of table structures keyed by 
				 * the address of the reference Tk window */
} TableInterpData;


typedef struct EditorStruct Editor;
typedef void (EditorDrawProc) _ANSI_ARGS_((Editor *editor));
typedef void (EditorDestroyProc) _ANSI_ARGS_((DestroyData destroyData));

struct EditorStruct {
    int gridLineWidth;
    int buttonHeight;
    int entryPad;
    int minSize;		/* Minimum size to allow any partition */

    EditorDrawProc *drawProc;
    EditorDestroyProc *destroyProc;
};

#define nRows		rowInfo.chainPtr->nLinks
#define nColumns	columnInfo.chainPtr->nLinks

/*
 * Limits --
 *
 * 	Defines the bounding of a size (width or height) in the table.
 * 	It may be related to the partition, entry, or table size.  The
 * 	widget pointers are used to associate sizes with the requested
 * 	size of other widgets.
 */

typedef struct {
    int flags;			/* Flags indicate whether using default
				 * values for limits or not. See flags
				 * below. */
    int max, min;		/* Values for respective limits. */
    int nom;			/* Nominal starting value. */
    Tk_Window wMax, wMin;	/* If non-NULL, represents widgets whose
				 * requested sizes will be set as limits. */
    Tk_Window wNom;		/* If non-NULL represents widget whose
				 * requested size will be the nominal
				 * size. */
} Limits;

#define LIMITS_SET_BIT	1
#define LIMITS_SET_MIN  (LIMITS_SET_BIT<<0)
#define LIMITS_SET_MAX  (LIMITS_SET_BIT<<1)
#define LIMITS_SET_NOM  (LIMITS_SET_BIT<<2)

#define LIMITS_MIN	0	/* Default minimum limit  */
#define LIMITS_MAX	SHRT_MAX/* Default maximum limit */
#define LIMITS_NOM	-1000	/* Default nomimal value.  Indicates if a
				 * partition has received any space yet */

typedef int (LimitsProc) _ANSI_ARGS_((int value, Limits *limitsPtr));

/*
 * Resize --
 *
 *	These flags indicate in what ways each partition in a table
 *	can be resized from its default dimensions.  The normal size of
 *	a row/column is the minimum amount of space needed to hold the
 *	widgets that span it.  The table may then be stretched or
 *	shrunk depending if the container is larger or smaller than
 *	the table. This can occur if 1) the user resizes the toplevel
 *	widget, or 2) the container is in turn packed into a larger
 *	widget and the "fill" option is set.
 *
 * 	  RESIZE_NONE 	  - No resizing from normal size.
 *	  RESIZE_EXPAND   - Do not allow the size to decrease.
 *			    The size may increase however.
 *        RESIZE_SHRINK   - Do not allow the size to increase.
 *			    The size may decrease however.
 *	  RESIZE_BOTH     - Allow the size to increase or
 *			    decrease from the normal size.
 *	  RESIZE_VIRGIN   - Special case of the resize flag.  Used to
 *			    indicate the initial state of the flag.
 *			    Empty rows/columns are treated differently
 *			    if this row/column is set.
 */

#define RESIZE_NONE	0
#define RESIZE_EXPAND	(1<<0)
#define RESIZE_SHRINK	(1<<1)
#define RESIZE_BOTH	(RESIZE_EXPAND | RESIZE_SHRINK)
#define RESIZE_VIRGIN	(1<<2)

/*
 * Control --
 */
#define CONTROL_NORMAL	1.0	/* Consider the widget when
				 * calculating the row heights and
				 * column widths.  */
#define CONTROL_NONE	0.0	/* Ignore the widget.  The height and
				 * width of the rows/columns spanned
				 * by this widget will not affected by
				 * the size of the widget.
				 */
#define CONTROL_FULL	-1.0	/* Consider only this widget when
				 * determining the column widths
				 * and row heights of the partitions
				 * it spans. */
#define EXCL_PAD 	0
#define INCL_PAD	1

typedef struct TableStruct Table;
typedef struct RowColumnStruct RowColumn;

/*
 * Entry --
 *
 *	An entry holds a widget and describes how the widget should
 *	appear in a range of cells.
 *	 1. padding.
 *	 2. how many rows/columns the entry spans.
 *	 3. size bounds for the widget.
 *
 *	Several entries may start at the same cell in
 *	the table, but a entry can hold only one widget.
 */

typedef struct  {
    Tk_Window tkwin;		/* Widget to be managed. */

    Table *tablePtr;		/* Table managing this widget */

    int borderWidth;		/* The external border width of
				 * the widget. This is needed to check if
				 * Tk_Changes(tkwin)->border_width changes.
				 */

    int manageWhenNeeded;	/* If non-zero, allow joint custody of
				 * the widget.  This is for cases
				 * where the same widget may be shared
				 * between two different tables
				 * (e.g. same graph on two different
				 * notebook pages).  Claim the widget
				 * only when the table is
				 * mapped. Don't destroy the entry if
				 * the table loses custody of the
				 * widget. */

    Limits reqWidth, reqHeight;	/* Bounds for width and height requests
				 * made by the widget. */
    struct PositionInfo {
	RowColumn *rcPtr;	/* Row or column where this entry starts. */

	int span;		/* Number of rows or columns spanned. */
	double control;		/* Weight of widget in the row or column. */

	Blt_ChainLink *linkPtr;	/* Link to widget in the chain of spans */

	Blt_Chain *chainPtr;	/* Pointer to the chain of spans. */
    } row, column;

    Tk_Anchor anchor;		/* Anchor type: indicates how the
				 * widget is positioned if extra space
				 * is available in the entry */

    Blt_Pad padX;		/* Extra padding placed left and right of the
				 * widget. */
    Blt_Pad padY;		/* Extra padding placed above and below the
				 * widget */

    int ipadX, ipadY;		/* Extra padding added to the interior of
				 * the widget (i.e. adds to the requested
				 * size of the widget) */

    int fill;			/* Indicates how the widget should
				 * fill the span of cells it occupies. */

    int x, y;			/* Origin of widget wrt container. */

    Blt_ChainLink *linkPtr;	/* Pointer into list of entries. */

    Blt_HashEntry *hashPtr;	/* Pointer into table of entries. */

} Entry;

/*
 * RowColumn --
 *
 * 	Creates a definable space (row or column) in the table. It may
 * 	have both requested minimum or maximum values which constrain
 * 	the size of it.
 */

struct RowColumnStruct {
    int index;			/* Index of row or column */

    int size;			/* Current size of the partition. This size
				 * is bounded by minSize and maxSize. */

    /*
     * nomSize and size perform similar duties.  I need to keep track
     * of the amount of space allocated to the partition (using size).
     * But at the same time, I need to indicate that space can be
     * parcelled out to this partition.  If a nominal size was set for
     * this partition, I don't want to add space.
     */

    int nomSize;		/* The nominal size (neither expanded
				 * nor shrunk) of the partition based
				 * upon the requested sizes of the
				 * widgets spanning this partition. */

    int minSize, maxSize;	/* Size constraints on the partition */

    int offset;			/* Offset of the partition (in pixels)
				 * from the origin of the container. */

    int minSpan;		/* Minimum spanning widget in
				 * partition. Used for bookkeeping
				 * when growing a span of partitions
				 * */

    double weight;		/* Weight of row or column */

    Entry *control;		/* Pointer to the entry that is
				 * determining the size of this
				 * partition.  This is used to know
				 * when a partition is occupied. */

    int resize;			/* Indicates if the partition should
				 * shrink or expand from its nominal
				 * size. */

    Blt_Pad pad;		/* Pads the partition beyond its nominal
				 * size */

    Limits reqSize;		/* Requested bounds for the size of
				 * the partition. The partition will
				 * not expand or shrink beyond these
				 * limits, regardless of how it was
				 * specified (max widget size).  This
				 * includes any extra padding which
				 * may be specified. */

    int maxSpan;		/* Maximum spanning widget to consider
				 * when growing a span of partitions.
				 * A value of zero indicates that all
				 * spans should be considered. */

    int count;

    Blt_ChainLink *linkPtr;

};

#define DEF_TBL_RESIZE	"both"
#define DEF_TBL_PAD	"0"
#define DEF_TBL_MAXSPAN	"0"


/*
 * This is the default number of elements in the statically
 * pre-allocated column and row arrays.  This number should reflect a
 * useful number of row and columns, which fit most applications.
 */
#define DEF_ARRAY_SIZE	32

typedef Entry *(EntrySearchProc) _ANSI_ARGS_((Table *tablePtr, 
	Tk_Window tkwin));

/*
 * PartitionInfo --
 *
 * 	Manages the rows or columns of the table.  Contains
 *	a chain of partitions (representing the individiual
 *	rows or columns).
 *
 */
typedef struct PartitionInfo {
    char *type;			/* String identifying the type of
				 * partition: "row" or "column". */
    Blt_Chain *chainPtr;
    Blt_List list;		/* Linked list of bins of widgets
				 * keyed by increasing span. */
    Tk_ConfigSpec *configSpecs;
    int reqLength;
    int ePad;			/* Extra padding for row/column
				 * needed to display editor marks */
} PartitionInfo;

/*
 * Table structure
 */
struct TableStruct {
    int flags;			/* See the flags definitions below. */
    Tk_Window tkwin;		/* The container widget into which
				 * other widgets are arranged. */
    Tcl_Interp *interp;		/* Interpreter associated with all
				 * widgets */

    Blt_Chain *chainPtr;	/* Chain of entries in the table. */

    Blt_HashTable entryTable;	/* Table of entries.  Serves as a
				 * directory to look up entries from
				 * widget their names. */
    Blt_Pad padX, padY;

    int propagate;		/* If non-zero, the table will make a
				 * geometry request on behalf of the
				 * container widget. */

    int eTablePad, eEntryPad;

    PartitionInfo columnInfo;
    PartitionInfo rowInfo;	/* Manages row and column partitions */

    Dim2D container;		/* Last known dimenion of the container. */
    Dim2D normal;		/* Normal dimensions of the table */
    Limits reqWidth, reqHeight;	/* Constraints on the table's normal
				 * width and height */
    Editor *editPtr;		/* If non-NULL, indicates that the
				 * table is currently being edited */
    Tcl_IdleProc *arrangeProc;
    EntrySearchProc *findEntryProc;
    Blt_HashEntry *hashPtr;	/* Used to delete the table from its
				 * hashtable. */
    Blt_HashTable *tablePtr;
};

/*
 * Table flags definitions
 */
#define ARRANGE_PENDING (1<<0)	/* A call to ArrangeTable is
				 * pending. This flag allows multiple
				 * layout changes to be requested
				 * before the table is actually
				 * reconfigured. */
#define REQUEST_LAYOUT 	(1<<1)	/* Get the requested sizes of the
				 * widgets before expanding/shrinking
				 * the size of the container.  It's
				 * necessary to recompute the layout
				 * every time a partition or entry is
				 * added, reconfigured, or deleted,
				 * but not when the container is
				 * resized. */
#define NON_PARENT	(1<<2)	/* The table is managing widgets that
				 * arern't children of the container.
				 * This requires that they are
				 * manually moved when the container
				 * is moved (a definite performance
				 * hit). */
/*
 * Forward declarations
 */

extern int Blt_GetTable _ANSI_ARGS_((TableInterpData *dataPtr, 
	Tcl_Interp *interp, char *pathName, Table **tablePtrPtr));

#endif /* _BLT_TABLE_H */
