#ifndef lint
static char rcsid[] = "$Header$";
#endif
#include <stdio.h>
#include "rndnum.h"
extern int Debug;
extern int Levels;
extern int RandomFlag;
#define THRESHOLD	127
/*	tone_simple	thresh hold method
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
 *	returns	0 or 1
 *
 * Uses:
 *	Debug	- Debug level (currently not used.)
 *	Levels	- Number of intenisty levels.
 *	RandomFlag - Use random threshold flag.
 *
 * Calls:
 *	Random	- return a random number between -0.5 and 0.5;
 *
 * Method:
 *	straight-forward.
 *
 * Author:
 *	Christopher T. Johnson	- 90/03/21
 *
 * $Log$
 * Revision 1.4  90/04/10  16:46:38  cjohnson
 * Fix Intensity methods 
 * 
 * Revision 1.3  90/04/10  01:32:02  cjohnson
 * Add Intensity Levels
 * 
 * Revision 1.2  90/04/09  17:05:37  cjohnson
 * define Pix to be of type integer.
 * 
 * Revision 1.1  90/04/09  16:16:37  cjohnson
 * Initial revision
 * 
 */
tone_simple(Pix,X,Y,NX,NY,New)
int	Pix;
int	X, Y, NX, NY;
int	New;
{
	register int threshold;
	if (RandomFlag) {
		threshold = THRESHOLD + Random(0)*127;
	} else {
		threshold = THRESHOLD;
	}
	return((Pix*Levels + threshold) / 256 );
}
