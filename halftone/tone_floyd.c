#ifndef lint
static char rcsid[] = "$Header$";
#endif
#include <stdio.h>
#include "msr.h"
extern int Debug;
extern int Levels;
extern int width;
extern struct msr_unif *RandomFlag;

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
 *	MSR_UNIF_DOUBLE()	Returns a random double between -0.5 and 0.5.
 *
 * Method:
 *	straight-forward.
 *
 * Author:
 *	Christopher T. Johnson	- 90/03/21
 *
 * $Log$
 * Revision 2.2  90/04/13  01:46:20  cjohnson
 * Change include "*.h" to "./*.h"
 * 
 * Revision 2.1  90/04/13  01:23:18  cjohnson
 * First Relese.
 * 
 * Revision 1.4  90/04/13  00:56:25  cjohnson
 * Comment clean up.
 * Change some variables to registers.
 * 
 * Revision 1.3  90/04/12  17:36:57  cjohnson
 * Add Random number processing.
 * 
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
	register int diff,value;
	int Dir = NX-X;
	register double w1,w3,w5,w7;

	if (RandomFlag) {
		register double val;
		val = MSR_UNIF_DOUBLE(RandomFlag)*1.0/16.0; /* slowest */
		w1 = 1.0/16.0 + val;
		w3 = 3.0/16.0 - val;
		val = MSR_UNIF_DOUBLE(RandomFlag)*5.0/16.0; /* slowest */
		w5 = 5.0/16.0 + val;
		w7 = 7.0/16.0 - val;
	} else {
		w1 = 1.0/16.0;
		w3 = 3.0/16.0;
		w5 = 5.0/16.0;
		w7 = 7.0/16.0;
	}

/*
 *	is this the first time through?
 */
	if (!error) {
		register int i;
		error = (int *) malloc(width*sizeof(int));
		thisline = (int *) malloc(width*sizeof(int));
		for (i=0; i<width; i++) {
			error[i] = 0;
			thisline[i] = 0;
		}
	}
/*
 *	if this is a new line then trade error for thisline.
 */
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
		thisline[X+Dir] += diff*w7;	/* slow */
		error[X+Dir] += diff*w1;
	}
	error[X] += diff*w5;			/* slow */
	if (X-Dir < width && X-Dir >= 0) {
		error[X-Dir] += diff*w3;
	}
	return(value);
}
