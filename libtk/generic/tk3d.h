/*
 * tk3d.h --
 *
 *	Declarations of types and functions shared by the 3d border
 *	module.
 *
 * Copyright (c) 1996 by Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * SCCS: @(#) tk3d.h 1.1 96/11/04 13:52:59
 */

#ifndef _TK3D
#define _TK3D

#include <tkInt.h>

/*
 * One of the following data structures is allocated for
 * each 3-D border currently in use.  Structures of this
 * type are indexed by borderTable, so that a single
 * structure can be shared for several uses.
 */

typedef struct {
    Screen *screen;		/* Screen on which the border will be used. */
    Visual *visual;		/* Visual for all windows and pixmaps using
				 * the border. */
    int depth;			/* Number of bits per pixel of drawables where
				 * the border will be used. */
    Colormap colormap;		/* Colormap out of which pixels are
				 * allocated. */
    int refCount;		/* Number of different users of
				 * this border.  */
    XColor *bgColorPtr;		/* Background color (intensity
				 * between lightColorPtr and
				 * darkColorPtr). */
    XColor *darkColorPtr;	/* Color for darker areas (must free when
				 * deleting structure). NULL means shadows
				 * haven't been allocated yet.*/
    XColor *lightColorPtr;	/* Color used for lighter areas of border
				 * (must free this when deleting structure).
				 * NULL means shadows haven't been allocated
				 * yet. */
    Pixmap shadow;		/* Stipple pattern to use for drawing
				 * shadows areas.  Used for displays with
				 * <= 64 colors or where colormap has filled
				 * up. */
    GC bgGC;			/* Used (if necessary) to draw areas in
				 * the background color. */
    GC darkGC;			/* Used to draw darker parts of the
				 * border. None means the shadow colors
				 * haven't been allocated yet.*/
    GC lightGC;			/* Used to draw lighter parts of
				 * the border. None means the shadow colors
				 * haven't been allocated yet. */
    Tcl_HashEntry *hashPtr;	/* Entry in borderTable (needed in
				 * order to delete structure). */
} TkBorder;


/*
 * Maximum intensity for a color:
 */

#define MAX_INTENSITY 65535

/*
 * Declarations for platform specific interfaces used by this module.
 */

EXTERN TkBorder *	TkpGetBorder _ANSI_ARGS_((void));
EXTERN void		TkpGetShadows _ANSI_ARGS_((TkBorder *borderPtr,
			    Tk_Window tkwin));
EXTERN void		TkpFreeBorder _ANSI_ARGS_((TkBorder *borderPtr));

#endif /* _TK3D */
