/*
 *			T O N E _ S I M P L E . C
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
#include "vmath.h"
#include "raytrace.h"
#include "msr.h"

extern int Debug;
extern int Levels;
extern struct msr_unif *RandomFlag;

#define THRESHOLD	127

/*	tone_simple	thresh hold method
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
 *	returns	0 or 1
 *
 * Uses:
 *	Debug	- Debug level (currently not used.)
 *	Levels	- Number of intenisty levels.
 *	RandomFlag - Use random threshold flag.
 *
 * Calls:
 *	MSR_UNIF_DOUBLE	- return a random number between -0.5 and 0.5;
 *
 * Author:
 *	Christopher T. Johnson	- 90/03/21
 */
int
tone_simple(pix,x,y,nx,ny,new)
int	pix;
int	x, y, nx, ny;
int	new;
{
	register int threshold;
	if (RandomFlag) {
		threshold = THRESHOLD + MSR_UNIF_DOUBLE(RandomFlag)*127;
	} else {
		threshold = THRESHOLD;
	}
	return((pix*Levels + threshold) / 256 );
}
