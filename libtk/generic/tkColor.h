/*
 * tkColor.h --
 *
 *	Declarations of data types and functions used by the
 *	Tk color module.
 *
 * Copyright (c) 1996 by Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * SCCS: @(#) tkColor.h 1.1 96/10/22 16:53:09
 */

#ifndef _TKCOLOR
#define _TKCOLOR

#include <tkInt.h>

/*
 * One of the following data structures is used to keep track of
 * each color that the color module has allocated from the X display
 * server.
 */

#define COLOR_MAGIC ((unsigned int) 0x46140277)

typedef struct TkColor {
    XColor color;		/* Information about this color. */
    unsigned int magic;		/* Used for quick integrity check on this
				 * structure.   Must always have the
				 * value COLOR_MAGIC. */
    GC gc;			/* Simple gc with this color as foreground
				 * color and all other fields defaulted.
				 * May be None. */
    Screen *screen;		/* Screen where this color is valid.  Used
				 * to delete it, and to find its display. */
    Colormap colormap;		/* Colormap from which this entry was
				 * allocated. */
    Visual *visual;             /* Visual associated with colormap. */
    int refCount;		/* Number of uses of this structure. */
    Tcl_HashTable *tablePtr;	/* Hash table that indexes this structure
				 * (needed when deleting structure). */
    Tcl_HashEntry *hashPtr;	/* Pointer to hash table entry for this
				 * structure. (for use in deleting entry). */
} TkColor;

/*
 * Common APIs exported from all platform-specific implementations.
 */

#ifndef TkpFreeColor
EXTERN void		TkpFreeColor _ANSI_ARGS_((TkColor *tkColPtr));
#endif
EXTERN TkColor *	TkpGetColor _ANSI_ARGS_((Tk_Window tkwin,
			    Tk_Uid name));
EXTERN TkColor *	TkpGetColorByValue _ANSI_ARGS_((Tk_Window tkwin,
			    XColor *colorPtr));	

#endif /* _TKCOLOR */
