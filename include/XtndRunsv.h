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
 *  Modified at BRL 10-Sept-88 by Mike Muuss to be portable to machines
 *  such as the Cray where sizeof(short) != 2.
 *  All use of the symbol LITTLE_ENDIAN has been removed.
 */
/*
 * Runsv.h - Definitions for Run Length Encoding.
 *
 * Author:	Spencer W. Thomas
 * 		Computer Science Dept.
 * 		University of Utah
 * Date:	Mon Aug  9 1982
 * Copyright (c) 1982 Spencer W. Thomas
 *
 * $Header$
 */

#ifndef XTNDRUNSV
#define XTNDRUNSV

/*
 * Opcode definitions
 */

#define     LONG                0x40
#define	    RSkipLinesOp	1
#define	    RSetColorOp		2
#define	    RSkipPixelsOp	3
#define	    RByteDataOp		5
#define	    RRunDataOp		6
#define	    REOFOp		7

#define     H_CLEARFIRST        0x1	/* clear framebuffer flag */
#define	    H_NO_BACKGROUND	0x2	/* if set, no bg color supplied */
#define	    H_ALPHA		0x4   /* if set, alpha channel (-1) present */
#define	    H_COMMENT		0x8	/* if set, comments present */

struct XtndRsetup
{
    char    hc_xpos[2];
    char    hc_ypos[2];
    char    hc_xlen[2];
    char    hc_ylen[2];
    char    h_flags,
	    h_ncolors,
	    h_pixelbits,
	    h_ncmap,
	    h_cmaplen;
};
#define	    SETUPSIZE	((4*2)+5)

/* "Old" RLE format magic numbers */
#define	    RMAGIC	('R' << 8)	/* top half of magic number */
#define	    WMAGIC	('W' << 8)	/* black&white rle image */

#define	    XtndRMAGIC	((short)0xcc52)	/* RLE file magic number */

#endif /* XTNDRUNSV */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
