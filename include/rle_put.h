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
 */
/* 
 * rle_put.h - Definitions and a few global variables for rle_putrow/putraw.
 * 
 * Author:	Spencer W. Thomas
 * 		Computer Science Dept.
 * 		University of Utah
 * Date:	Mon Aug  9 1982
 * Copyright (c) 1982 Spencer W. Thomas
 * 
 * $Id$
 */

#ifndef RLE_PUT_H
#define RLE_PUT_H

/* ****************************************************************
 * Dispatch table for different output types.
 */
#ifdef c_plusplus
#define ARB_ARGS ...
#else
#define ARB_ARGS
#endif

typedef int rle_fn( ARB_ARGS );

struct rle_dispatch_tab {
    const char   *magic;	/* magic type flags */
    rle_fn *setup,			/* startup function */
	   *skipBlankLines,
	   *setColor,
	   *skipPixels,
	   *newScanLine,
	   *putdat,			/* put a set of differing pixels */
	   *putrn,			/* put a run all the same */
	   *blockHook,			/* hook called at start of new */
					/* output block */
	   *putEof;		/* write EOF marker (if possible) */
};

extern struct rle_dispatch_tab rle_DTable[];

/* 
 * These definitions presume the existence of a variable called
 * "fileptr", declared "long * fileptr".  *fileptr should be
 * initialized to 0 before calling Setup().
 * A pointer "the_hdr" declared "rle_hdr * the_hdr" is also
 * presumed to exist.
 */
#define	    rle_magic		(rle_DTable[(int)the_hdr->dispatch].magic)
#define	    Setup()		(*rle_DTable[(int)the_hdr->dispatch].setup)(the_hdr)
#define	    SkipBlankLines(n)	(*rle_DTable[(int)the_hdr->dispatch].skipBlankLines)(n, the_hdr)
#define	    SetColor(c)		(*rle_DTable[(int)the_hdr->dispatch].setColor)(c, the_hdr)
#define	    SkipPixels(n, l, r)	(*rle_DTable[(int)the_hdr->dispatch].skipPixels)(n,l,r, the_hdr)
#define	    NewScanLine(flag)	(*rle_DTable[(int)the_hdr->dispatch].newScanLine)(flag, the_hdr)
#define	    putdata(buf, len)	(*rle_DTable[(int)the_hdr->dispatch].putdat)(buf, len, the_hdr)
#define	    putrun(val, len, f)	(*rle_DTable[(int)the_hdr->dispatch].putrn)(val,len,f, the_hdr)
#define	    BlockHook()		(*rle_DTable[(int)the_hdr->dispatch].blockHook)(the_hdr)
#define	    PutEof()		(*rle_DTable[(int)the_hdr->dispatch].putEof)(the_hdr)

/* 
 * States for run detection
 */
#define	DATA	0
#define	RUN2	1
#define RUN3	2
#define	RUN4	3
#define	INRUN	-1

#endif

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
