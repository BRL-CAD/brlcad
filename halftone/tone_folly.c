#ifndef lint
static char rcsid[] = "$Header$";
#endif
#include <stdio.h>
extern int Debug;
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
 * 
 */
tone_folly(Pix,X,Y,NX,NY,New)
int	Pix;
int	X, Y, NX, NY;
int	New;
{
	return (Pix >= 16*ordered[ X % 4][ Y % 4] );
}
