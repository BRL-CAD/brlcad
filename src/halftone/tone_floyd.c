/*                    T O N E _ F L O Y D . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2006 United States Government as represented by
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
/** @file tone_floyd.c
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
extern int width;
extern struct msr_unif *RandomFlag;

/*	tone_floyd	floyd-steinberg dispersed error method.
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
 *	returns	0 - Levels
 *
 * Uses:
 *	Debug	- Current debug level
 *	Levels	- Number of intensity levels.
 *	width	- width of bw file.
 *	RandomFlag - Should we toss random numbers?
 *
 * Calls:
 *	MSR_UNIF_DOUBLE()	Returns a random double between -0.5 and 0.5.
 *
 * Author:
 *	Christopher T. Johnson	- 90/03/21
 */
int
tone_floyd(int pix, int x, int y, int nx, int ny, int new)
{
	static int *error = 0;
	static int *thisline;
	register int diff,value;
	int Dir = nx-x;
	register double w1,w3,w5,w7;

	if (RandomFlag) {
		register double val;
		val = MSR_UNIF_DOUBLE(RandomFlag)*1.0/16.0; /* slowest */
		w1 = 1.0/16.0 + val;
		w3 = 3.0/16.0 - val;
		val = MSR_UNIF_DOUBLE(RandomFlag)*5.0/16.0; /* slowest */
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
	if (new) {
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
	return(value);
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
