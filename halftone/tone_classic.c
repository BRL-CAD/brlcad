#ifndef lint
static char rcsid[] = "$Header$";
#endif
#include <stdio.h>
extern int Debug;

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
 * Revision 1.1  90/04/09  16:16:37  cjohnson
 * Initial revision
 * 
 */
tone_classic(Pix,X,Y,NX,NY,New)
int	Pix;
int	X, Y, NX, NY;
int	New;
{
	return (Pix >= 14*ordered[( X + 3 ) % 6 ][ Y % 6] );
}
