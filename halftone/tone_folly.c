#ifndef lint
static char rcsid[] = "$Header$";
#endif
#include <stdio.h>
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
 *	Pix	Pixel value	0-255
 *	X	Current column
 *	Y	Current row
 *	NX	Next column
 *	NY	Next row
 *	New	New row flag.
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
 * Method:
 *	straight-forward.
 *
 * Author:
 *	Christopher T. Johnson	- 90/03/21
 *
 * $Log$
 * Revision 2.2  90/04/13  01:46:26  cjohnson
 * Change include "*.h" to "./*.h"
 * 
 * Revision 2.1  90/04/13  01:23:21  cjohnson
 * First Relese.
 * 
 * Revision 1.4  90/04/13  01:03:32  cjohnson
 * Add random numbers.
 * Clean comments.
 * 
 * Revision 1.3  90/04/10  16:46:35  cjohnson
 * Fix Intensity methods 
 * 
 * Revision 1.2  90/04/10  03:30:45  cjohnson
 * add intensity levels.
 * 
 * Revision 1.1  90/04/10  01:01:51  cjohnson
 * Initial revision
 * 
 */
tone_folly(Pix,X,Y,NX,NY,New)
int	Pix;
int	X, Y, NX, NY;
int	New;
{
	register int threshold = 16*ordered[ X % 4][ Y % 4];

	if (RandomFlag) {
		threshold += MSR_UNIF_DOUBLE(RandomFlag)*63;
	}
	return ((Pix*Levels + threshold)/255);
}
