/*                   T O N E _ S I M P L E . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2011 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file halftone/tone_simple.c
 *
 *  Author -
 *	Christopher T. Johnson	- 90/03/21
 *
 */

#include "common.h"

#include <stdio.h>

#include "vmath.h"
#include "raytrace.h"


extern int Debug;
extern int Levels;
extern struct bn_unif *RandomFlag;

#define THRESHOLD	127

/*	tone_simple	thresh hold method
 *
 * Entry:
 *	pix	Pixel value	0-255
 *	x	Current column
 *	y	Current row
 *	nx	Next column
 *	ny	Next row
 *	newrow	New row flag.
 *
 * Exit:
 *	returns	0 or 1
 *
 * Uses:
 *	Debug	- Debug level (currently not used.)
 *	Levels	- Number of intenisty levels.
 *	RandomFlag - Use random threshold flag.
 *
 * Calls:
 *	BN_UNIF_DOUBLE	- return a random number between -0.5 and 0.5;
 *
 * Author:
 *	Christopher T. Johnson	- 90/03/21
 */
int
tone_simple(int pix, int UNUSED(x), int UNUSED(y), int UNUSED(nx), int UNUSED(ny), int UNUSED(newrow))
{
    int threshold;
    if (RandomFlag) {
	threshold = THRESHOLD + BN_UNIF_DOUBLE(RandomFlag)*127;
    } else {
	threshold = THRESHOLD;
    }
    return((pix*Levels + threshold) / 256 );
}

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
