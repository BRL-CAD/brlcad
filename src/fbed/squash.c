/*                        S Q U A S H . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2007 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this file; see the file named COPYING for more
 * information.
 */
/** @file squash.c
	Author:		Gary S. Moss
			U. S. Army Ballistic Research Laboratory
			Aberdeen Proving Ground
			Maryland 21005-5066
			(301)278-6651 or DSN 298-6651
*/
#ifndef lint
static const char RCSid[] = "@(#)$Header$ (BRL)";
#endif
/*
 * squash.c - Filter super-sampled image for one scan line
 */
#include <stdio.h>
#include "./std.h"
#define DEBUG_SQUASH	false
/* Cone filtering weights.
 * #define CNTR_WT 0.23971778
 * #define MID_WT  0.11985889
 * #define CRNR_WT 0.07021166
 */

/* Gaussian filtering weights. */
#define CNTR_WT 0.3011592441
#define MID_WT 0.1238102667
#define CRNR_WT 0.0508999223

/*	Squash takes three super-sampled "bit arrays", and returns an array
	of intensities at half the resolution.  N is the size of the bit
	arrays.  The "bit arrays" are actually int arrays whose values are
	assumed to be only 0 or 1.
 */
void
squash(register int *buf0, register int *buf1, register int *buf2, register float *ret_buf, register int n)
{	register int     j;
#if DEBUG_SQUASH
	fb_log( "squash: buf0=0x%x buf1=0x%x buf2=0x%x ret_buf=0x%x n=%d\n",
		buf0, buf1, buf2, ret_buf, n );
#endif
	for( j = 1; j < n - 1; j++ )
		{
		ret_buf[j] =
			(
			buf2[j - 1] * CRNR_WT +
			buf2[j] * MID_WT +
			buf2[j + 1] * CRNR_WT +
			buf1[j - 1] * MID_WT +
			buf1[j] * CNTR_WT +
			buf1[j + 1] * MID_WT +
			buf0[j - 1] * CRNR_WT +
			buf0[j] * MID_WT +
			buf0[j + 1] * CRNR_WT
			);
		}
	return;
	}

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
