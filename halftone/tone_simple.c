#ifndef lint
static char rcsid[] = "$Header$";
#endif
#include <stdio.h>
#include "msr.h"
extern int Debug;
extern int Levels;
extern struct msr_unif *RandomFlag;
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
 *	MSR_UNIF_DOUBLE	- return a random number between -0.5 and 0.5;
 *
 * Method:
 *	straight-forward.
 *
 * Author:
 *	Christopher T. Johnson	- 90/03/21
 *
 * $Log$
 * Revision 2.2  90/04/13  01:46:29  cjohnson
 * Change include "*.h" to "./*.h"
 * 
 * Revision 2.1  90/04/13  01:23:24  cjohnson
 * First Relese.
 * 
 * Revision 1.5  90/04/11  15:26:23  cjohnson
 * Add random number processing.
 * 
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
		threshold = THRESHOLD + MSR_UNIF_DOUBLE(RandomFlag)*127;
	} else {
		threshold = THRESHOLD;
	}
	return((Pix*Levels + threshold) / 256 );
}
