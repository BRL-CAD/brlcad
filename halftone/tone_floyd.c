/*
 *			T O N E _ F L O Y D . C
 *
 *  Author -
 *	Christopher T. Johnson	- 90/03/21
 *  
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 *  
 *  Distribution Status -
 *	Public Domain, Distribution Unlimitied.
 */
#ifndef lint
static const char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "conf.h"

#include <stdio.h>

#include "machine.h"
#include "externs.h"			/* For malloc */
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
tone_floyd(pix,x,y,nx,ny,new)
int	pix;
int	x, y, nx, ny;
int	new;
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
		register int i;
		error = (int *) malloc(width*sizeof(int));
		thisline = (int *) malloc(width*sizeof(int));
		for (i=0; i<width; i++) {
			error[i] = 0;
			thisline[i] = 0;
		}
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
