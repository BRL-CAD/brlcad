/*
 *			T O N E _ C L A S S I C
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif



#include <stdio.h>

#include "machine.h"
#include "vmath.h"
#include "raytrace.h"
#include "msr.h"

extern int Debug;
extern int Levels;
extern struct msr_unif *RandomFlag;
/*
 * Clustered-Dot ordered dither at 45 degrees.
 *	Page 86 of Digital Halftoning.
 */
static unsigned char	ordered[6][6] = {
	{5,4,3,14,15,16},
	{6,1,2,13,18,17},
	{9,7,8,10,12,11},
	{14,15,16,5,4,3},
	{13,18,17,6,1,2},
	{10,12,11,9,7,8}};

/*	tone_classic	classic diaginal clustered halftones.
 *
 * Entry:
 *	Pix	Pixel value	0-255
 * The following are not used but are here for consistency with
 * other halftoning methods.
 *	X	Current column
 *	Y	Current row
 *	NX	Next column
 *	NY	Next row
 *	New	New row flag.
 *
 * Exit:
 *	returns	0-Levels
 *
 * Uses:
 *	Debug	- Global Debug value
 *	Levels	- Number of Intensity Levels
 *	RandomFlag - Show we toss random numbers?
 *
 * Author:
 *	Christopher T. Johnson	- 90/03/21
 */
int
tone_classic(int pix, int x, int y, int nx, int ny, int new)
{
	register int threshold = 14*ordered[( x + 3) % 6][ y % 6];
	if (RandomFlag) {
		threshold += MSR_UNIF_DOUBLE(RandomFlag)*63;
	}
	return ((pix*Levels + threshold)/255);
}
