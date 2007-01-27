/*                    T O N E _ F O L L Y . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2007 United States Government as represented by
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
/** @file tone_folly.c
 *
 *  Author -
 *	Christopher T. Johnson	- 90/03/21
 *
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 *
 */
#ifndef lint
static const char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "common.h"


#include <stdio.h>

#include "machine.h"
#include "vmath.h"
#include "raytrace.h"
#include "msr.h"

extern int Debug;
extern int Levels;
extern struct msr_unif *RandomFlag;

/*
 * Dispersed-Dot ordered Dither at 0 degrees (n=4)
 * 	From page 135 of Digital Halftoning.
 */
static unsigned char	ordered[4][4] = {
	{2,16,3,13},
	{12,8,9,5},
	{4,14,1,15},
	{10,6,11,7}};

/*	tone_folly	4x4 square ordered dither dispersed (folly and van dam)
 *
 * Entry:
 *	pix	Pixel value	0-255
 *	x	Current column
 *	y	Current row
 *	nx	Next column
 *	ny	Next row
 *	new	New row flag.
 *
 * Exit:
 *	returns	0 to Levels
 *
 * Uses:
 *	Debug	- Current debug level.
 *	Levels	- Number of intensity levels.
 *	RandomFlag - should we toss some random numbers?
 *
 * Calls:
 *	MSR_UNIF_DOUBLE() - to get random numbers from -0.5 to 0.5
 *
 * Author:
 *	Christopher T. Johnson	- 90/03/21
 */
int
tone_folly(int pix, int x, int y, int nx, int ny, int new)
{
	register int threshold = 16*ordered[ x % 4][ y % 4];

	if (RandomFlag) {
		threshold += MSR_UNIF_DOUBLE(RandomFlag)*63;
	}
	return ((pix*Levels + threshold)/255);
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
