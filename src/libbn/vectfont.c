/*                      V E C T F O N T . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2011 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @addtogroup plot */
/** @{ */
/** @file libbn/vectfont.c
 *
 *	Terminal Independant Graphics Display Package.
 *		Mike Muuss  July 31, 1978
 *
 *	This routine is used to plot a string of ASCII symbols
 *  on the plot being generated, using a built-in set of fonts
 *  drawn as vector lists.
 *
 *	Internally, the basic font resides in a 10x10 unit square.
 *  Externally, each character can be thought to occupy one square
 *  plotting unit;  the 'scale'
 *  parameter allows this to be changed as desired, although scale
 *  factors less than 10.0 are unlikely to be legible.
 *
 *  The vector font table here was provided courtesy of Dr. Bruce
 *  Henriksen and Dr. Stephen Wolff, US Army Ballistic Research
 *  Laboratory, Summer of 1978.  They had developed it for their
 *  remote Houston Instruments pen plotter package for the
 *  GE Tymeshare system.
 *
 */
/** @} */

#include "common.h"

#include <stdio.h>
#include <math.h>

#include "vmath.h"
#include "plot3.h"
#include "vectfont.h"

#define NUM_SYMBOLS	8

int *tp_cindex[256];	/* index to stroke tokens */

/**
 * @brief
 *  Once-only setup routine
 *  Used by libplot3/symbol.c, so it can't be static.
 */
void
tp_setup(void)
{
    register int *p;	/* pointer to stroke table */
    register int i;

    p = tp_ctable;		/* pointer to stroke list */

    /* Store start addrs of each stroke list */
    for ( i=040-NUM_SYMBOLS; i<128; i++)  {
	tp_cindex[i+128] = tp_cindex[i] = p;
	while ( (*p++) != LAST );
    }
    for ( i=1; i<=NUM_SYMBOLS; i++ )  {
	tp_cindex[i+128] = tp_cindex[i] = tp_cindex[040-NUM_SYMBOLS-1+i];
    }
    for ( i=NUM_SYMBOLS+1; i<040; i++ )  {
	tp_cindex[i+128] = tp_cindex[i] = tp_cindex['?'];
    }
}

/*	tables for markers	*/

int tp_ctable[] = {

/*	+	*/
    drk(0, 5),
    brt(8, 5),
    drk(4, 8),
    brt(4, 2),
    LAST,

/*	x	*/
    drk(0, 2),
    brt(8, 8),
    drk(0, 8),
    brt(8, 2),
    LAST,

/*	triangle	*/
    drk(0, 2),
    brt(4, 8),
    brt(8, 2),
    brt(0, 2),
    LAST,

/*	square	*/
    drk(0, 2),
    brt(0, 8),
    brt(8, 8),
    brt(8, 2),
    brt(0, 2),
    LAST,

/*	hourglass	*/
    drk(0, 2),
    brt(8, 8),
    brt(0, 8),
    brt(8, 2),
    brt(0, 2),
    LAST,

/*	plus-minus	*/
    drk(5, 7),
    brt(5, 2),
    drk(2, 2),
    brt(8, 2),
    drk(2, 5),
    brt(8, 5),
    LAST,

/*	centerline symbol	*/
    drk(8, 4),
    brt(7, 6),
    brt(4, 7),
    brt(1, 6),
    brt(0, 4),
    brt(1, 2),
    brt(4, 1),
    brt(7, 2),
    brt(8, 4),
    drk(1, 1),
    brt(7, 7),
    LAST,

/*	degree symbol	*/
    drk(1, 9),
    brt(2, 9),
    brt(3, 8),
    brt(3, 7),
    brt(2, 6),
    brt(1, 6),
    brt(0, 7),
    brt(0, 8),
    brt(1, 9),
    LAST,

/*	table for ascii 040, ' '	*/
    LAST,

/*	table for !	*/
    drk(3, 0),
    brt(5, 2),
    brt(5, 0),
    brt(3, 2),
    brt(3, 0),
    drk(4, 4),
    brt(3, 10),
    brt(5, 10),
    brt(4, 4),
    brt(4, 10),
    LAST,

/*	table for "	*/
    drk(1, 10),
    brt(3, 10),
    brt(2, 7),
    brt(1, 10 ),
    drk(5, 10),
    brt(7, 10),
    brt(6, 7),
    brt(5, 10),
    LAST,


/*	table for #	*/
    drk(1, 0),
    brt(3, 9),
    drk(6, 9),
    brt(4, 0),
    drk(6, 3),
    brt(0, 3),
    drk(1, 6),
    brt(7, 6),
    LAST,

/*	table for $	*/
    drk(1, 2),
    brt(1, 1),
    brt(7, 1),
    brt(7, 5),
    brt(1, 5),
    brt(1, 9),
    brt(7, 9),
    brt(7, 8),
    drk(4, 10),
    brt(4, 0),
    LAST,

/*	table for %	*/
    drk(3, 10),
    brt(3, 7),
    brt(0, 7),
    brt(0, 10),
    brt(8, 10),
    brt(0, 0),
    drk(8, 0),
    brt(5, 0),
    brt(5, 3),
    brt(8, 3),
    brt(8, 0),
    LAST,

/*	table for &	*/
    drk(7, 3),
    brt(4, 0),
    brt(1, 0),
    brt(0, 3),
    brt(5, 8),
    brt(4, 10),
    brt(3, 10),
    brt(1, 8),
    brt(8, 0),
    LAST,

/*	table for '	*/
    drk(4, 6),
    brt(5, 10),
    brt(6, 10),
    brt(4, 6),
    LAST,

/*	table for (	*/
    drk(5, 0 ),
    brt(3, 1 ),
    brt(2, 4 ),
    brt(2, 6 ),
    brt(3, 9 ),
    brt(5, 10 ),
    LAST,

/*	table for )	*/
    drk(3, 0 ),
    brt(5, 1 ),
    brt(6, 4 ),
    brt(6, 6 ),
    brt(5, 9 ),
    brt(3, 10 ),
    LAST,

/*	table for *	*/
    drk(4, 2 ),
    brt(4, 8 ),
    drk(6, 7 ),
    brt(2, 3 ),
    drk(6, 3 ),
    brt(2, 7 ),
    drk(1, 5 ),
    brt(7, 5 ),
    LAST,

/*	table for +	*/
    drk(1, 5 ),
    brt(7, 5 ),
    drk(4, 8 ),
    brt(4, 2 ),
    LAST,

/*	table for, 	*/
    drk(5, 0 ),
    brt(3, 2 ),
    brt(3, 0 ),
    brt(5, 2 ),
    brt(5, 0 ),
    bneg(2, 2 ),
    brt(4, 0 ),
    LAST,

/*	table for -	*/
    drk(1, 5 ),
    brt(7, 5 ),
    LAST,

/*	table for .	*/
    drk(5, 0 ),
    brt(3, 2 ),
    brt(3, 0 ),
    brt(5, 2 ),
    brt(5, 0 ),
    LAST,

/*	table for /	*/
    brt(8, 10 ),
    LAST,

/*	table for 0	*/
    drk(8, 10),
    brt(0, 0),
    brt(0, 10),
    brt(8, 10),
    brt(8, 0),
    brt(0, 0),
    LAST,

/*	table for 1	*/
    drk(4, 0 ),
    brt(4, 10 ),
    brt(2, 8 ),
    LAST,

/*	table for 2	*/
    drk(0, 6 ),
    brt(0, 8 ),
    brt(3, 10 ),
    brt(5, 10 ),
    brt(8, 8 ),
    brt(8, 7 ),
    brt(0, 2 ),
    brt(0, 0 ),
    brt(8, 0 ),
    LAST,

/*	table for 3	*/
    drk(0, 10 ),
    brt(8, 10 ),
    brt(8, 5 ),
    brt(0, 5 ),
    brt(8, 5 ),
    brt(8, 0 ),
    brt(0, 0 ),
    LAST,

/*	table for 4	*/
    drk(0, 10 ),
    brt(0, 5 ),
    brt(8, 5 ),
    drk(8, 10 ),
    brt(8, 0 ),
    LAST,

/*	table for 5	*/
    drk(8, 10 ),
    brt(0, 10 ),
    brt(0, 5 ),
    brt(8, 5 ),
    brt(8, 0 ),
    brt(0, 0 ),
    LAST,

/*	table for 6	*/
    drk(0, 10 ),
    brt(0, 0 ),
    brt(8, 0 ),
    brt(8, 5 ),
    brt(0, 5 ),
    LAST,

/*	table for 7	*/
    drk(0, 10 ),
    brt(8, 10 ),
    brt(6, 0 ),
    LAST,

/*	table for 8	*/
    drk(0, 5 ),
    brt(0, 0 ),
    brt(8, 0 ),
    brt(8, 5 ),
    brt(0, 5 ),
    brt(0, 10 ),
    brt(8, 10 ),
    brt(8, 5 ),
    LAST,

/*	table for 9	*/
    drk(8, 5 ),
    brt(0, 5 ),
    brt(0, 10 ),
    brt(8, 10 ),
    brt(8, 0 ),
    LAST,

/*	table for :	*/
    drk(5, 6 ),
    brt(3, 8 ),
    brt(3, 6 ),
    brt(5, 8 ),
    brt(5, 6 ),
    drk(5, 0 ),
    brt(3, 2 ),
    brt(3, 0 ),
    brt(5, 2 ),
    brt(5, 0 ),
    LAST,

/*	table for ;	*/
    drk(5, 6 ),
    brt(3, 8 ),
    brt(3, 6 ),
    brt(5, 8 ),
    brt(5, 6 ),
    drk(5, 0 ),
    brt(3, 2 ),
    brt(3, 0 ),
    brt(5, 2 ),
    brt(5, 0 ),
    bneg(2, 2 ),
    brt(4, 0 ),
    LAST,

/*	table for <	*/
    drk(8, 8 ),
    brt(0, 5 ),
    brt(8, 2 ),
    LAST,

/*	table for =	*/
    drk(0, 7 ),
    brt(8, 7 ),
    drk(0, 3 ),
    brt(8, 3 ),
    LAST,

/*	table for >	*/
    drk(0, 8 ),
    brt(8, 5 ),
    brt(0, 2 ),
    LAST,

/*	table for ?	*/
    drk(3, 0 ),
    brt(5, 2 ),
    brt(5, 0 ),
    brt(3, 2 ),
    brt(3, 0 ),
    drk(1, 7 ),
    brt(1, 9 ),
    brt(3, 10 ),
    brt(5, 10 ),
    brt(7, 9 ),
    brt(7, 7 ),
    brt(4, 5 ),
    brt(4, 3 ),
    LAST,

/*	table for @	*/
    drk(0, 8 ),
    brt(2, 10 ),
    brt(6, 10 ),
    brt(8, 8 ),
    brt(8, 2 ),
    brt(6, 0 ),
    brt(2, 0 ),
    brt(1, 1 ),
    brt(1, 4 ),
    brt(2, 5 ),
    brt(4, 5 ),
    brt(5, 4 ),
    brt(5, 0 ),
    LAST,

/*	table for A	*/
    brt(0, 8 ),
    brt(2, 10 ),
    brt(6, 10 ),
    brt(8, 8 ),
    brt(8, 0 ),
    drk(0, 5 ),
    brt(8, 5 ),
    LAST,

/*	table for B	*/
    brt(0, 10 ),
    brt(5, 10 ),
    brt(8, 9 ),
    brt(8, 6 ),
    brt(5, 5 ),
    brt(0, 5 ),
    brt(5, 5 ),
    brt(8, 4 ),
    brt(8, 1 ),
    brt(5, 0 ),
    brt(0, 0 ),
    LAST,

/*	table for C	*/
    drk(8, 2 ),
    brt(6, 0 ),
    brt(2, 0 ),
    brt(0, 2 ),
    brt(0, 8 ),
    brt(2, 10 ),
    brt(6, 10 ),
    brt(8, 8 ),
    LAST,

/*	table for D	*/
    brt(0, 10 ),
    brt(5, 10 ),
    brt(8, 8 ),
    brt(8, 2 ),
    brt(5, 0 ),
    brt(0, 0 ),
    LAST,

/*	table for E	*/
    drk(8, 0 ),
    brt(0, 0 ),
    brt(0, 10 ),
    brt(8, 10 ),
    drk(0, 5 ),
    brt(5, 5 ),
    LAST,

/*	table for F	*/
    brt(0, 10 ),
    brt(8, 10 ),
    drk(0, 5 ),
    brt(5, 5 ),
    LAST,

/*	table for G	*/
    drk(5, 5 ),
    brt(8, 5 ),
    brt(8, 2 ),
    brt(6, 0 ),
    brt(2, 0 ),
    brt(0, 2 ),
    brt(0, 8 ),
    brt(2, 10 ),
    brt(6, 10 ),
    brt(8, 8 ),
    LAST,

/*	table for H	*/
    brt(0, 10 ),
    drk(8, 10 ),
    brt(8, 0 ),
    drk(0, 6 ),
    brt(8, 6 ),
    LAST,

/*	table for I	*/
    drk(4, 0 ),
    brt(6, 0 ),
    drk(5, 0 ),
    brt(5, 10 ),
    brt(4, 10 ),
    brt(6, 10 ),
    LAST,

/*	table for J	*/
    drk(0, 2 ),
    brt(2, 0 ),
    brt(5, 0 ),
    brt(7, 2 ),
    brt(7, 10 ),
    brt(6, 10 ),
    brt(8, 10 ),
    LAST,

/*	table for K	*/
    brt(0, 10 ),
    drk(0, 5 ),
    brt(8, 10 ),
    drk(3, 7 ),
    brt(8, 0 ),
    LAST,

/*	table for L	*/
    drk(8, 0 ),
    brt(0, 0 ),
    brt(0, 10 ),
    LAST,

/*	table for M	*/
    brt(0, 10 ),
    brt(4, 5 ),
    brt(8, 10 ),
    brt(8, 10 ),
    brt(8, 0 ),
    LAST,

/*	table for N	*/
    brt(0, 10 ),
    brt(8, 0 ),
    brt(8, 10 ),
    LAST,

/*	table for O	*/
    drk(0, 2 ),
    brt(0, 8 ),
    brt(2, 10 ),
    brt(6, 10 ),
    brt(8, 8 ),
    brt(8, 2 ),
    brt(6, 0 ),
    brt(2, 0 ),
    brt(0, 2 ),
    LAST,

/*	table for P	*/
    brt(0, 10 ),
    brt(6, 10 ),
    brt(8, 9 ),
    brt(8, 6 ),
    brt(6, 5 ),
    brt(0, 5 ),
    LAST,

/*	table for Q	*/
    drk(0, 2 ),
    brt(0, 8 ),
    brt(2, 10 ),
    brt(6, 10 ),
    brt(8, 8 ),
    brt(8, 2 ),
    brt(6, 0 ),
    brt(2, 0 ),
    brt(0, 2 ),
    drk(5, 3 ),
    brt(8, 0 ),
    LAST,

/*	table for R	*/
    brt(0, 10 ),
    brt(6, 10 ),
    brt(8, 8 ),
    brt(8, 6 ),
    brt(6, 5 ),
    brt(0, 5 ),
    drk(5, 5 ),
    brt(8, 0 ),
    LAST,

/*	table for S	*/
    drk(0, 1 ),
    brt(1, 0 ),
    brt(6, 0 ),
    brt(8, 2 ),
    brt(8, 4 ),
    brt(6, 6 ),
    brt(2, 6 ),
    brt(0, 7 ),
    brt(0, 9 ),
    brt(1, 10 ),
    brt(7, 10 ),
    brt(8, 9 ),
    LAST,

/*	table for T	*/
    drk(4, 0 ),
    brt(4, 10 ),
    drk(0, 10 ),
    brt(8, 10 ),
    LAST,

/*	table for U	*/
    drk(0, 10 ),
    brt(0, 2 ),
    brt(2, 0 ),
    brt(6, 0 ),
    brt(8, 2 ),
    brt(8, 10 ),
    LAST,

/*	table for V	*/
    drk(0, 10 ),
    brt(4, 0 ),
    brt(8, 10 ),
    LAST,

/*	table for W	*/
    drk(0, 10 ),
    brt(1, 0 ),
    brt(4, 4 ),
    brt(7, 0 ),
    brt(8, 10 ),
    LAST,

/*	table for X	*/
    brt(8, 10 ),
    drk(0, 10 ),
    brt(8, 0 ),
    LAST,

/*	table for Y	*/
    drk(0, 10 ),
    brt(4, 4 ),
    brt(8, 10 ),
    drk(4, 4 ),
    brt(4, 0 ),
    LAST,

/*	table for Z	*/
    drk(0, 10 ),
    brt(8, 10 ),
    brt(0, 0 ),
    brt(8, 0 ),
    LAST,

/*	table for [	*/
    drk(6, 0 ),
    brt(4, 0 ),
    brt(4, 10 ),
    brt(6, 10 ),
    LAST,

/*	table for \	*/
    drk(0, 10 ),
    brt(8, 0 ),
    LAST,

/*	table for ]	*/
    drk(2, 0 ),
    brt(4, 0 ),
    brt(4, 10 ),
    brt(2, 10 ),
    LAST,

/*	table for ^	*/
    drk(4, 0 ),
    brt(4, 10 ),
    drk(2, 8 ),
    brt(4, 10 ),
    brt(6, 8 ),
    LAST,

/*	table for _	*/
    dneg(0, 1),
    bneg(11, 1),
    LAST,

/*	table for ascii 96: accent	*/
    drk(3, 10),
    brt(5, 6),
    brt(4, 10),
    brt(3, 10),
    LAST,

/*	table for a	*/
    drk(0, 5),
    brt(1, 6),
    brt(6, 6),
    brt(7, 5),
    brt(7, 1),
    brt(8, 0),
    drk(7, 1),
    brt(6, 0),
    brt(1, 0),
    brt(0, 1),
    brt(0, 2),
    brt(1, 3),
    brt(6, 3),
    brt(7, 2),
    LAST,

/*	table for b	*/
    brt(0, 10),
    drk(8, 3),
    brt(7, 5),
    brt(4, 6),
    brt(1, 5),
    brt(0, 3),
    brt(1, 1),
    brt(4, 0),
    brt(7, 1),
    brt(8, 3),
    LAST,

/*	table for c	*/
    drk(8, 5),
    brt(7, 6),
    brt(2, 6),
    brt(0, 4),
    brt(0, 4),
    brt(0, 2),
    brt(2, 0),
    brt(7, 0),
    brt(8, 1),
    LAST,

/*	table for d	*/
    drk(8, 0),
    brt(8, 10),
    drk(8, 3),
    brt(7, 5),
    brt(4, 6),
    brt(1, 5),
    brt(0, 3),
    brt(1, 1),
    brt(4, 0),
    brt(7, 1),
    brt(8, 3),
    LAST,

/*	table for e	*/
    drk(0, 4),
    brt(1, 3),
    brt(7, 3),
    brt(8, 4),
    brt(8, 5),
    brt(7, 6),
    brt(1, 6),
    brt(0, 5),
    brt(0, 1),
    brt(1, 0),
    brt(7, 0),
    brt(8, 1),
    LAST,

/*	table for f	*/
    drk(2, 0),
    brt(2, 9),
    brt(3, 10),
    brt(5, 10),
    brt(6, 9),
    drk(1, 5),
    brt(4, 5),
    LAST,

/*	table for g	*/
    drk(8, 6),
    drk(8, 3),
    brt(7, 5),
    brt(4, 6),
    brt(1, 5),
    brt(0, 3),
    brt(1, 1),
    brt(4, 0),
    brt(7, 1),
    brt(8, 3),
    bneg(8, 2),
    bneg(7, 3),
    bneg(1, 3),
    bneg(0, 2),
    LAST,

/*	table for h	*/
    brt(0, 10),
    drk(0, 4),
    brt(2, 6),
    brt(6, 6),
    brt(8, 4),
    brt(8, 0),
    LAST,

/*	table for i	*/
    drk(4, 0),
    brt(4, 6),
    brt(3, 6),
    drk(4, 9),
    brt(4, 8),
    drk(3, 0),
    brt(5, 0),
    LAST,

/*	table for j	*/
    drk(5, 6),
    brt(6, 6),
    bneg(6, 2),
    bneg(5, 3),
    bneg(3, 3),
    bneg(2, 2),
    LAST,

/*	table for k	*/
    brt(2, 0),
    brt(2, 10),
    brt(0, 10),
    drk(2, 4),
    brt(4, 4),
    brt(8, 6),
    drk(4, 4),
    brt(8, 0),
    LAST,

/*	table for l	*/
    drk(3, 10),
    brt(4, 10),
    brt(4, 2),
    brt(5, 0),
    LAST,

/*	table for m	*/
    brt(0, 6),
    drk(0, 5),
    brt(1, 6),
    brt(3, 6),
    brt(4, 5),
    brt(4, 0),
    drk(4, 5),
    brt(5, 6),
    brt(7, 6),
    brt(8, 5),
    brt(8, 0),
    LAST,

/*	table for n	*/
    brt(0, 6),
    drk(0, 4),
    brt(2, 6),
    brt(6, 6),
    brt(8, 4),
    brt(8, 0),
    LAST,

/*	table for o	*/
    drk(8, 3),
    brt(7, 5),
    brt(4, 6),
    brt(1, 5),
    brt(0, 3),
    brt(1, 1),
    brt(4, 0),
    brt(7, 1),
    brt(8, 3),
    LAST,

/*	table for p	*/
    drk(0, 6),
    bneg(0, 3),
    drk(8, 3),
    brt(7, 5),
    brt(4, 6),
    brt(1, 5),
    brt(0, 3),
    brt(1, 1),
    brt(4, 0),
    brt(7, 1),
    brt(8, 3),
    LAST,

/*	table for q	*/
    drk(8, 6),
    drk(8, 3),
    brt(7, 5),
    brt(4, 6),
    brt(1, 5),
    brt(0, 3),
    brt(1, 1),
    brt(4, 0),
    brt(7, 1),
    brt(8, 3),
    bneg(8, 3),
    bneg(9, 3),
    LAST,

/*	table for r	*/
    brt(1, 0),
    brt(1, 6),
    brt(0, 6),
    drk(1, 4),
    brt(3, 6),
    brt(6, 6),
    brt(8, 4),
    LAST,

/*	table for s	*/
    drk(0, 1),
    brt(1, 0),
    brt(7, 0),
    brt(8, 1),
    brt(7, 2),
    brt(1, 4),
    brt(0, 5),
    brt(1, 6),
    brt(7, 6),
    brt(8, 5),
    LAST,

/*	table for t	*/
    drk(7, 1),
    brt(6, 0),
    brt(4, 0),
    brt(3, 1),
    brt(3, 10),
    brt(2, 10),
    drk(1, 5),
    brt(5, 5),
    LAST,

/*	table for u	*/
    drk(0, 6),
    brt(1, 6),
    brt(1, 1),
    brt(2, 0),
    brt(6, 0),
    brt(7, 1),
    brt(7, 6),
    drk(7, 1),
    brt(8, 0),
    LAST,

/*	table for v	*/
    drk(0, 6),
    brt(4, 0),
    brt(8, 6),
    LAST,

/*	table for w	*/
    drk(0, 6),
    brt(0, 5),
    brt(2, 0),
    brt(4, 5),
    brt(6, 0),
    brt(8, 5),
    brt(8, 6),
    LAST,

/*	table for x	*/
    brt(8, 6),
    drk(0, 6),
    brt(8, 0),
    LAST,

/*	table for y	*/
    drk(0, 6),
    brt(0, 1),
    brt(1, 0),
    brt(7, 0),
    brt(8, 1),
    drk(8, 6),
    bneg(8, 2),
    bneg(7, 3),
    bneg(1, 3),
    bneg(0, 2),
    LAST,

/*	table for z	*/
    drk(0, 6),
    brt(8, 6),
    brt(0, 0),
    brt(8, 0),
    LAST,

/*	table for ascii 123, left brace	*/
    drk(6, 10),
    brt(5, 10),
    brt(4, 9),
    brt(4, 6),
    brt(3, 5),
    brt(4, 4),
    brt(4, 1),
    brt(5, 0),
    brt(6, 0),
    LAST,

/*	table for ascii 124, vertical bar	*/
    drk(4, 4),
    brt(4, 0),
    brt(5, 0),
    brt(5, 4),
    brt(4, 4),
    drk(4, 6),
    brt(4, 10),
    brt(5, 10),
    brt(5, 6),
    brt(4, 6),
    LAST,

/*	table for ascii 125, right brace	*/
    drk(2, 0),
    brt(3, 0),
    brt(4, 1),
    brt(4, 4),
    brt(5, 5),
    brt(4, 6),
    brt(4, 9),
    brt(3, 10),
    brt(2, 10),
    LAST,

/*	table for ascii 126, tilde	*/
    drk(0, 5),
    brt(1, 6),
    brt(3, 6),
    brt(5, 4),
    brt(7, 4),
    brt(8, 5),
    LAST,

/*	table for ascii 127, rubout	*/
    drk(0, 2),
    brt(0, 8),
    brt(8, 8),
    brt(8, 2),
    brt(0, 2),
    LAST
};

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
