/*                    T O N E _ F L O Y D . C
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
/** @file halftone/tone_floyd.c
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
extern int width;
extern struct bn_unif *RandomFlag;

/*	tone_floyd	floyd-steinberg dispersed error method.
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
 *	returns	0 - Levels
 *
 * Uses:
 *	Debug	- Current debug level
 *	Levels	- Number of intensity levels.
 *	width	- width of bw file.
 *	RandomFlag - Should we toss random numbers?
 *
 * Calls:
 *	BN_UNIF_DOUBLE()	Returns a random double between -0.5 and 0.5.
 *
 * Author:
 *	Christopher T. Johnson	- 90/03/21
 */
int
tone_floyd(int pix, int x, int UNUSED(y), int nx, int UNUSED(ny), int newrow)
{
    static int *error = 0;
    static int *thisline;
    int diff, value;
    int Dir = nx-x;
    double w1, w3, w5, w7;

    if (RandomFlag) {
	double val;
	val = BN_UNIF_DOUBLE(RandomFlag)*1.0/16.0; /* slowest */
	w1 = 1.0/16.0 + val;
	w3 = 3.0/16.0 - val;
	val = BN_UNIF_DOUBLE(RandomFlag)*5.0/16.0; /* slowest */
	w5 = 5.0/16.0 + val;
	w7 = 7.0/16.0 - val;
    } else {
	w1 = 1.0/16.0;
	w3 = 3.0/16.0;
	w5 = 5.0/16.0;
	w7 = 7.0/16.0;
    }

/*
 *	is this the first time through?
 */
    if (!error) {
	error = (int *) bu_calloc(width, sizeof(int), "error");
	thisline = (int *) bu_calloc(width, sizeof(int), "thisline");
    }
/*
 *	if this is a new line then trade error for thisline.
 */
    if (newrow) {
	int *p;
	p = error;
	error = thisline;
	thisline = p;
    }

    pix += thisline[x];
    thisline[x] = 0;

    value = (pix*Levels + 127) / 255;
    diff =  pix - (value * 255 /Levels);

    if (x+Dir < width && x+Dir >= 0) {
	thisline[x+Dir] += diff*w7;	/* slow */
	error[x+Dir] += diff*w1;
    }
    error[x] += diff*w5;			/* slow */
    if (x-Dir < width && x-Dir >= 0) {
	error[x-Dir] += diff*w3;
    }
    return value;
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
