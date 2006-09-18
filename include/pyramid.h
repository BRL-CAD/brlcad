/** @addotgroup deprecated */
/*@{*/
/** @file pyramid.h
 * @deprecated 
 * pyramid.h - Types, constants, globals, routine decls for pyramids
 * 
 * Author:	Rod Bogart
 * 		Computer Science Dept.
 * 		University of Utah
 * Date:	Thu Mar 12 1987
 * Copyright (c) 1987 Rod Bogart
 * 
 */
#define and &&
#define or ||
#define not !

#define MASKSIZE 5
#define MASKBELOW 2			/* MASKBELOW = (MASKSIZE-1) / 2; */
#define MASKABOVE 2			/* MASKABOVE = MASKSIZE / 2; */

typedef struct
{
    /* pointers to the corners of the arrays for each level */
    rle_pixel ** corners;

    int *xlen, *ylen;			/* sizes of each level */

    int nchan;				/* total channels per level */

    int levels;				/* number of levels in this pyramid */
} pyramid;

float * gauss_mask();
/*@}*/
