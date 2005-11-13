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
 * svfb.h - Definitions and a few global variables for svfb.
 *
 * Author:	Spencer W. Thomas
 * 		Computer Science Dept.
 * 		University of Utah
 * Date:	Mon Aug  9 1982
 * Copyright (c) 1982 Spencer W. Thomas
 *
 * $Header$
 */

/* ****************************************************************
 * Dispatch table for different output types.
 */
typedef int sv_fn();
struct sv_dispatch_tab {
    char   *magic;			/* magic type flags */
    sv_fn  *setup,			/* startup function */
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

extern struct sv_dispatch_tab sv_DTable[];

/*
 * These definitions presume the existence of a variable called
 * "fileptr", declared "long * fileptr".  *fileptr should be
 * initialized to 0 before calling Setup().
 * A pointer "globals" declared "struct sv_globals * globals" is also
 * presumed to exist.
 */
#define	    sv_magic		(sv_DTable[(int)globals->sv_dispatch].magic)
#define	    Setup()		(*sv_DTable[(int)globals->sv_dispatch].setup)(globals)
#define	    SkipBlankLines(n)	(*sv_DTable[(int)globals->sv_dispatch].skipBlankLines)(n, globals)
#define	    SetColor(c)		(*sv_DTable[(int)globals->sv_dispatch].setColor)(c, globals)
#define	    SkipPixels(n, l, r)	(*sv_DTable[(int)globals->sv_dispatch].skipPixels)(n,l,r, globals)
#define	    NewScanLine(flag)	(*sv_DTable[(int)globals->sv_dispatch].newScanLine)(flag, globals)
#define	    putdata(buf, len)	(*sv_DTable[(int)globals->sv_dispatch].putdat)(buf, len, globals)
#define	    putrun(val, len, f)	(*sv_DTable[(int)globals->sv_dispatch].putrn)(val,len,f, globals)
#define	    BlockHook()		(*sv_DTable[(int)globals->sv_dispatch].blockHook)(globals)
#define	    PutEof()		(*sv_DTable[(int)globals->sv_dispatch].putEof)(globals)

/*
 * States for run detection
 */
#define	DATA	0
#define	RUN2	1
#define RUN3	2
#define	RUN4	3
#define	INRUN	-1

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
