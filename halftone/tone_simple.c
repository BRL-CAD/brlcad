#ifndef lint
static char rcsid[] = "$Header$";
#endif
#include <stdio.h>
extern int Debug;
extern int Levels;
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
 *	None.
 *
 * Calls:
 *	None.
 *
 * Method:
 *	straight-forward.
 *
 * Author:
 *	Christopher T. Johnson	- 90/03/21
 *
 * $Log$
 * Revision 1.2  90/04/09  17:05:37  cjohnson
 * define Pix to be of type integer.
 * 
 * Revision 1.1  90/04/09  16:16:37  cjohnson
 * Initial revision
 * 
 */
double
tone_simple(Pix,X,Y,NX,NY,New)
int	Pix;
int	X, Y, NX, NY;
int	New;
{
	return(1.0/Levels * (Pix*Levels / 255) );
}
