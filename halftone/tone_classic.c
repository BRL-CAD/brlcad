#ifndef lint
static char rcsid[] = "$Header$";
#endif
#include <stdio.h>
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
 * Calls:
 *	Random(0) - To get a random number.
 *
 * Method:
 *	straight-forward.
 *
 * Author:
 *	Christopher T. Johnson	- 90/03/21
 *
 * $Log$
 * Revision 2.2  90/04/13  01:46:14  cjohnson
 * Change include "*.h" to "./*.h"
 * 
 * Revision 2.1  90/04/13  01:23:15  cjohnson
 * First Relese.
 * 
 * Revision 1.6  90/04/13  00:51:47  cjohnson
 * Comment clean up.
 * 
 * Revision 1.5  90/04/12  17:36:46  cjohnson
 * Add Random number processing.
 * 
 * Revision 1.4  90/04/10  16:45:49  cjohnson
 * Fix Intensity methods 
 * 
 * Revision 1.3  90/04/10  03:31:25  cjohnson
 * Add intensity levels.
 * 
 * Revision 1.2  90/04/10  01:02:29  cjohnson
 * Fix order indices.  Works now.
 * 
 * Revision 1.1  90/04/09  16:16:37  cjohnson
 * Initial revision
 * 
 */
tone_classic(Pix,X,Y,NX,NY,New)
int	Pix;
int	X, Y, NX, NY;
int	New;
{
	register int threshold = 14*ordered[( X + 3) % 6][ Y % 6];
	if (RandomFlag) {
		threshold += MSR_UNIF_DOUBLE(RandomFlag)*63;
	}
	return ((Pix*Levels + threshold)/255);
}
