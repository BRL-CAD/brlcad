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
 *  Modified at BRL 16-May-88 by Mike Muuss to avoid Alliant STDC problem
 *  with <math.h> having defines for "exp" conflicting with local vars.
 */
/*
 * float_to_exp.c - Convert floating point values to exponent bytes
 *
 * Author:	John W. Peterson
 * 		Computer Science Dept.
 * 		University of Utah
 * Date:	Thu Oct 29 1987
 * Copyright (c) 1987, University of Utah
 */

#include <stdio.h>
#include "rle.h"
#include <math.h>

/*****************************************************************
 * TAG( float_to_exp )
 *
 * Takes an array of count floating point numbers, and makes an array
 * of count+1 pixels out of it.
 */
void
float_to_exp( count, floats, pixels )
int count;
float * floats;
rle_pixel * pixels;
{
    register int i;
    int expon, max_exp = -2000;
    float * fptr = floats;
    double f_exp;

    /* Find largest exponent */
    /* Use "Block normalization":
     * ExpScan[x] is largest exponent of the three
     * color components.  Red/Grn/BluScan[1..3] are the
     * normalized color components.
     */

    for (i = 0; i < count; i++)
    {
	frexp( *fptr++, &expon );
	max_exp = (expon > max_exp) ? expon : max_exp;
    }

    /* Don't over/underflow */
    if (max_exp > 128) max_exp = 128;
    else
	if (max_exp < -127) max_exp = -127;

    f_exp = ldexp( 256.0, -max_exp );

    fptr = floats;
    for( i = 0; i < count; i++ )  /* Extra casts for broken HP compiler */
        *pixels++ = (rle_pixel) ((int)(*fptr++ * f_exp));

    /* Excess 127 exponent */
    *pixels = (rle_pixel) (max_exp + 127);
}
