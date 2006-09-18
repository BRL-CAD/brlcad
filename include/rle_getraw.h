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
/** @addtogroup utahrle */
/*@{*/
/* 
 * rle_getraw.h - Definitions for rle_getraw
 * 
 * Author:	Spencer W. Thomas
 * 		Computer Science Dept.
 * 		University of Utah
 * Date:	Mon Jul  7 1986
 * Copyright (c) 1986, Spencer W. Thomas
 */

#ifndef RLEGETRAW
#define RLEGETRAW

#include <XtndRunsv.h>

/*****************************************************************
 * TAG( rle_op )
 *
 * Struct representing one rle opcode.
 */

typedef struct rle_op rle_op;

struct rle_op {
    int opcode;			/* one of RByteDataOp or RRunDataOp */
    int xloc;			/* X location this op starts at */
    int length;			/* length of run or data */
    union {
	rle_pixel * pixels;	/* for ByteData */
	int run_val;		/* for RunData */
    } u;
};

#endif

/*@}*/
/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
