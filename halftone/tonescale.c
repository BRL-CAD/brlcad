#ifndef lint
static const char RCSid[] = "$Header$";
#endif

#include "conf.h"

#include <stdio.h>

#include "machine.h"
#include "externs.h"		/* For alloc */

#define	DLEVEL	1
extern int Debug;
typedef struct Cubic {
	double	x,A,B,C,D;
} C;
static struct Cubic	*EqCubics=0;
int eq_cubic();

/*	tonescale	Given a raw pixel value, return a scaled value
 *
 * This is normally used to map the output devices characteristics to
 * the input intinsites.  There can be a diffrent map for each color.
 *
 * Entry:
 *	map	pointer to a 256 byte map
 *	Slope	Slope of a line
 *	B	offset of line.
 *	eqptr	Null or the pointer to a equation for generating a curve
 *
 * Exit:
 *	map	is filled using eqptr 
 *
 * Uses:
 *	EqCubics	x,A,B,C,D of a set of cubics for a curve
 *
 * Calls:
 *	eq_line	given x return y; requires EqLineSlope and EqLineB
 *	eqptr	if not null.
 *
 * Method:
 *	straight-forward.
 *
 * Author:
 *	Christopher T. Johnson	- 90/03/22
 *
 * Change History:
 *	ctj 90/04/04 - change to use a standard cubic line format for the
 *	tone scale.  If eqptr is null then Set EqCubic to evaluate to a line.
 *
 * $Log$
 * Revision 11.3  2000/01/31 16:20:25  jra
 * Eliminated an unused variable
 *
 * Revision 11.2  1996/07/17 17:02:46  jra
 * Minor Mods for IRIX 6.2
 *
 * Revision 11.1  1995/01/04  10:21:56  mike
 * Release_4.4
 *
 * Revision 10.2  94/08/23  17:58:17  gdurf
 * Added include of conf.h, machine.h and externs.h, the last for malloc()
 * 
 * Revision 10.1  1991/10/12  06:53:23  mike
 * Release_4.0
 *
 * Revision 2.2  91/07/19  01:51:34  mike
 * Can't declare something static and extern both
 * 
 * Revision 2.1  90/04/13  01:23:27  cjohnson
 * First Relese.
 * 
 * Revision 1.3  90/04/13  01:13:52  cjohnson
 * Cleanup the comments.
 * 
 * Revision 1.2  90/04/10  03:29:34  cjohnson
 * Bug fixes to allow Tonescale to be used.
 * added debug to print Cubics
 * 
 * Revision 1.1  90/04/09  16:18:47  cjohnson
 * Initial revision
 * 
 */
void
tonescale(map,Slope,B,eqptr)
unsigned char	*map;
float		Slope,
		B;
int 		(*eqptr)();
{
	int i,result;

/*
 * 	Is there a function we should be using?
 * N.B.	This code has not been tested with a funtion being passed in.
 */
	if (!eqptr ) eqptr=eq_cubic;

/*
 *	If there is no defined Cubics then set a straight line.
 */
	if (!EqCubics) {
/*
 *		We need an extra cubic to make eq_cubic processing
 *		easier.
 */
		EqCubics = (struct Cubic *)malloc(2*sizeof(struct Cubic));
		EqCubics[0].x = 0.0;
		EqCubics[0].A = B;
		EqCubics[0].B = Slope;
		EqCubics[1].x = 256.0;
		EqCubics[0].C = EqCubics[0].D =	EqCubics[1].A =
		    EqCubics[1].B = EqCubics[1].C = EqCubics[1].D = 0.0;
	}

	for (i=0;i<256;i++) {
		result=eqptr(i);
		if (result<0) {
			if (Debug >= DLEVEL) {
				fprintf(stderr,"tonescale: y=%d, x=%d\n",
				    result, i);
			}
			result=0;
		} else if (result > 255) {
			if (Debug >= DLEVEL) {
				fprintf(stderr,"tonescale: y=%d, x=%d\n",
				    result, i);
			}
			result=255;
		}

		map[i] = result;
	}
}

/* eq_cubic	default tone scale alorithm
 *
 * impliment
 *	y = A+B*(X-x)+C*(X-x)^2+D*(X-x)^3
 * as a default tonescale algorithm;
 *
 * Entry:
 *	x	x value for equation.
 *
 * Exit:
 *	returns	y in the range 0-255  reqires clipping.
 *
 * Calls:
 *	none.
 *
 * Uses:
 *	EqCubic		list of Cubic equations.
 *
 * Method:
 *	straight-forward.
 *
 * Author:
 *	Christopher T. Johnson - 90/03/22
 */
int
eq_cubic(x)
int x;
{
	int y;
	struct Cubic *p = EqCubics;

	if (!p) {
		fprintf(stderr,"eq_cubic called with no cubics!\n");
		return(x);
	}
	while (x >= (p+1)->x) p++;
	
	y = ((p->D * (x - p->x) + p->C) * (x - p->x) + p->B)
	    * (x - p->x) + p->A;

	if (y<0) y = 0;
	if (y>255) y = 255;
	return(y);
}

/* cubic_init	initialize a cubic list given a set of points.
 *
 * Entry:
 *	n	number of points
 *	x	list of x points.
 *	y	list of y points.
 *
 * Exit:
 *	EqCubic	is set to a list of cubics
 *
 * Calls:
 *	none.
 *
 * Uses:
 *	none.
 *
 * Method:
 *	Cubic Spline Interpolation
 *	Taken from page 107 of:
 *		Numerical Analysis 2nd Edition by
 *		Richard L. Burden, J. Douglas Faires and
 *		    Albert C. Reynalds.
 *
 * I.e.  I don't have a clue to what is going on...... :-(
 *	
 */
void
cubic_init(n,x,y)
int n;
int *x,*y;
{
	int i;
	double *h,*alpha,*mi,*z,*l;

	h = (double *) malloc(n*sizeof(double));
	alpha = (double *) malloc(n*sizeof(double));
	mi = (double *) malloc(n*sizeof(double));
	z = (double *) malloc(n*sizeof(double));
	l = (double *) malloc(n*sizeof(double));

	EqCubics = (struct Cubic *) malloc(n*sizeof(struct Cubic));

	for (i=0; i<n-1; i++) {
		h[i] = x[i+1] - x[i];
		EqCubics[i].x = x[i];
		EqCubics[i].A = y[i];
	}

	EqCubics[i].x = x[i];
	EqCubics[i].A = y[i];

	for (i=1; i<n-1; i++) {
		alpha[i] = 3.0*(y[i+1]*h[i-1] - y[i]*(x[i+1]-x[i-1]) +
		    y[i-1]*h[i]) / (h[i-1]*h[i]);
	}

	z[0] = 0;
	mi[0] = 0;

	for (i=1; i<n-1; i++) {
		l[i] = 2.0*(x[i+1] - x[i-1]) - h[i-1]*mi[i-1];
		mi[i] = h[i]/l[i];
		z[i] = (alpha[i]-h[i-1]*z[i-1]) / l[i];
	}

	l[i] = 1.0;
	z[i] = 0.0;
	EqCubics[i].C = z[i];

	for (i=n-2; i>=0; i--) {
		EqCubics[i].C = z[i] - mi[i]*EqCubics[i+1].C;
		EqCubics[i].B = (y[i+1] - y[i])/h[i] -
		    h[i]*(EqCubics[i+1].C + 2.0*EqCubics[i].C)/3.0;
		EqCubics[i].D = (EqCubics[i+1].C - EqCubics[i].C)/(3.0*h[i]);
	}

	free(h);
	free(alpha);
	free(mi);
	free(z);
	free(l);
	if (Debug>1) {
		for(i=0;i<n;i++) {
			fprintf(stderr,"x=%g, A=%g, B=%g, C=%g, D=%g\n",
			EqCubics[i].x,EqCubics[i].A,EqCubics[i].B,
			EqCubics[i].C,EqCubics[i].D);
		}
	}
}
