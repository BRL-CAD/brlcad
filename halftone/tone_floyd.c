#ifndef lint
static char rcsid[] = "$Header$";
#endif
#include <stdio.h>
#include "rndnum.h"
extern int Debug;
extern int Levels;
extern int width;
extern int RandomFlag;

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
 *	returns	0 - Levels
 *
 * Uses:
 *	Debug	- Current debug level
 *	Levels	- Number of intensity levels.
 *	width	- width of bw file.
 *	RandomFlag - Should we toss random numbers?
 *
 * Calls:
 *	Random()	Returns a random double between -0.5 and 0.5.
 *
 * Method:
 *	straight-forward.
 *
 * Author:
 *	Christopher T. Johnson	- 90/03/21
 *
 * $Log$
 * Revision 1.2  90/04/10  16:46:25  cjohnson
 * Fix Intensity methods 
 * 
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
	double w1,w3,w5,w7,val;

	if (RandomFlag) {
		val = Random(0)*1.0/16.0;
		w1 = 1.0/16.0 + val;
		w3 = 3.0/16.0 - val;
		val = Random(0)*5.0/16.0;
		w5 = 5.0/16.0 + val;
		w7 = 7.0/16.0 - val;
	} else {
		w1 = 1.0/16.0;
		w3 = 3.0/16.0;
		w5 = 5.0/16.0;
		w7 = 7.0/16.0;
	}

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

	if (X+Dir < width && X+Dir >= 0) {
		thisline[X+Dir] += diff*w7;
		error[X+Dir] += diff*w1;
	}
	error[X] += diff*w5;
	if (X-Dir < width && X-Dir >= 0) {
		error[X-Dir] += diff*w3;
	}
	return(value);
}
