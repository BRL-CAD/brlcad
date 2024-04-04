/*
 * This software is copyrighted as noted below.  It may be freely copied,
 * modified, and redistributed, provided that the copyright notice is
 * preserved on all copies.
 *
 * There is no warranty or other guarantee of fitness for this software,
 * it is provided solely "as is".  Bug reports or fixes may be sent
 * to the author, who may or may not act on them as he desires.
 *
 * You may not include this software in a program or other software product
 * without supplying the source, or without informing the end-user that the
 * source is available for no extra charge.
 *
 * If you modify this software, you should include a notice giving the
 * name of the person performing the modification, the date of modification,
 * and the reason for such modification.
 *
 *  Modified at BRL 16-May-88 by Mike Muuss to avoid Alliant STDC desire
 *  to have all "void" functions so declared.
 */
/*
 * rle_global.c - Global variable initialization for rle routines.
 *
 * Author:	Spencer W. Thomas
 * 		Computer Science Dept.
 * 		University of Utah
 * Date:	Thu Apr 25 1985
 * Copyright (c) 1985,1986 Spencer W. Thomas
 *
 * $Id$
 */

#include <stdio.h>
#include "rle_put.h"
#include "rle.h"


extern int	RunSetup( ARB_ARGS ),
		RunSkipBlankLines( ARB_ARGS ),
		RunSetColor( ARB_ARGS ),
		RunSkipPixels( ARB_ARGS ),
		RunNewScanLine( ARB_ARGS ),
		Runputdata( ARB_ARGS ),
		Runputrun( ARB_ARGS ),
		RunputEof( ARB_ARGS );

extern int	DefaultBlockHook( ARB_ARGS );
extern void	NullputEof( ARB_ARGS );

struct rle_dispatch_tab rle_DTable[] = {
    {
	" OB",
	RunSetup,
	RunSkipBlankLines,
	RunSetColor,
	RunSkipPixels,
	RunNewScanLine,
	Runputdata,
	Runputrun,
	DefaultBlockHook,
	RunputEof
    },
};

static int bg_color[3] = { 0, 0, 0 };

rle_hdr rle_dflt_hdr = {
    RUN_DISPATCH,		/* dispatch value */
    3,				/* 3 colors */
    bg_color,			/* background color */
    0,				/* (alpha) if 1, save alpha channel */
    2,				/* (background) 0->just save pixels, */
				/* 1->overlay, 2->clear to bg first */
    0, 511,			/* (xmin, xmax) X bounds to save */
    0, 511,			/* (ymin, ymax) Y bounds to save */
    0,				/* ncmap (if != 0, save color map) */
    8,				/* cmaplen (log2 of length of color map) */
    NULL,			/* pointer to color map */
    NULL,			/* pointer to comment strings */
    0,				/* output file */
    { 7 },			/* RGB channels only */
    0L,				/* Can't free name and file fields. */
    "Urt",			/* Default "program name". */
    "no file",			/* No file name given. */
    0				/* First image. */
    /* Can't initialize the union */
};

/* ARGSUSED */
void
NullputEof(the_hdr)
rle_hdr * the_hdr;
{
				/* do nothing */
}
