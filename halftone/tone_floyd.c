#ifndef lint
static char rcsid[] = "$Header$";
#endif
#include <stdio.h>
extern int Debug;
extern int Levels;
extern int width;

/*	tone_floyd	floyd-steinberg dispersed error method.
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
 * Revision 1.1  90/04/10  05:23:24  cjohnson
 * Initial revision
 * 
 */
tone_floyd(Pix,X,Y,NX,NY,New)
int	Pix;
int	X, Y, NX, NY;
int	New;
{
	static int *error = 0;
	static int *thisline;
	int diff,value;
	int Dir = NX-X;

	if (!error) {
		register int i;
		error = (int *) malloc(width*sizeof(int));
		thisline = (int *) malloc(width*sizeof(int));
		for (i=0; i<width; i++) {
			error[i] = 0;
			thisline[i] = 0;
		}
	}
	if (New) {
		int *p;
		p = error;
		error = thisline;
		thisline = p;
	}

	Pix += thisline[X];
	thisline[X] = 0;
	value = (Pix*Levels + 127) / 255;
	diff =  Pix - (value * 255 /Levels);
/*	if (Pix > 127) {
		value=Levels;
		diff = Pix - 255;
	} else {
		value = 0;
		diff = Pix;
	}
*/
	if (X+Dir < width && X+Dir >= 0) {
		thisline[X+Dir] += diff*7/16;
		error[X+Dir] += diff/16;
	}
	error[X] += diff*5/16;
	if (X-Dir < width && X-Dir >= 0) {
		error[X-Dir] += diff*3/16;
	}
	return(value);
}
