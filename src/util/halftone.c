/*                      H A L F T O N E . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2022 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file halftone.c
 *
 * given a bw file, generate a ht file.
 *
 * Usage:
 *	halftone
 *		-s	square size
 *		-n	number of lines
 *		-w	width
 *		-a	Automatic bw file sizing.
 *		-B	Beta for sharpening
 *		-I	number of intensity levels
 *		-M	method
 *			0 Floyd-Steinberg
 *			1 45-degree classical halftone screen
 *			2 Threshold
 *			3 0-degree dispersed halftone screen
 *		-R	Add some noise.
 *		-S	Surpent flag (set back to 0 if method is not Floyd-Steinberg)
 *		-T	tonescale points
 *		-D	debug level
 *
 * Exit:
 *	writes a ht(bw) file.
 *		an ht file is a bw file with a limited set of values
 *		ranging from 0 to -I(n) as integers.
 *
 * Uses:
 *	None.
 *
 * Calls:
 *	sharpen()	- get a line from the file that has been sharpened
 *	tone_simple()	- Threshold halftone.
 *	tone_floyd()	- Floyd-Steinberg halftone.
 *	tone_folly()	- 0 degree halftone screen (from Folly and Van Dam)
 *	tone_classic()	- 45 degree classical halftone screen.
 *	tonescale()	- Generates a tone scale map default is 0, 0 to 255, 255
 *	cubic_init()	- Generates "cubics" for tonescale for a set of points.
 *
 * Method:
 *	Fairly simple.  Most of the algorithms are inspired by
 *		Digital Halftoning by Robert Ulichney
 *
 */

#include "common.h"

#include <stdlib.h>
#include <ctype.h>
#include <math.h>

#include "bio.h"

#include "bu/app.h"
#include "bu/exit.h"
#include "bu/getopt.h"
#include "bu/malloc.h"
#include "vmath.h"
#include "raytrace.h"
#include "dm.h"

#define	DLEVEL	1
#define THRESHOLD	127

long int width=512;	/* width of picture */
long int height=512;	/* height of picture */
double Beta=0.0;	/* Beta for sharpening */

#define	M_FLOYD	0
#define	M_CLASSIC 1
#define	M_THRESH 2
#define	M_FOLLY 3
int Method=M_FLOYD;	/* Method of halftoning */

int Surpent=0;		/* use serpentine scan lines */
int Levels=1;		/* Number of levels */
int Debug=0;
struct bn_unif *RandomFlag=0;	/* Use random numbers ? */

#if 0
void cubic_init(int n, int *x, int *y);
void tonescale(unsigned char *map, float Slope, float B, int (*eqptr)() );
int sharpen(unsigned char *buf, int size, int num, FILE *file, unsigned char *Map);
int tone_floyd(int pix, int x, int y, int nx, int ny, int newrow);
int tone_folly(int pix, int x, int y, int nx, int ny, int newrow);
int tone_simple(int pix, int x, int y, int nx, int ny, int newrow);
int tone_classic(int pix, int x, int y, int nx, int ny, int newrow);
#endif


/**
 * return a sharpened tone mapped buffer.
 *
 * Entry:
 *	buf	- buffer to place bytes.
 *	size	- size of element
 *	num	- number of elements to read.
 *	file	- file to read from.
 *	Map	- tone scale mapping.
 *
 * Exit:
 *	returns 0 on EOF
 *		number of byes read otherwise.
 *
 * Uses:
 *	Debug	- Current debug level.
 *	Beta	- sharpening value.
 *
 * Calls:
 *	None.
 *
 * Method:
 *	if no sharpening just read a line tone scale and return.
 *	if first time called get space for processing and do setup.
 *	if first line then
 *		only use cur and next lines
 *	else if last line then
 *		only use cur and last lines
 *	else
 *		use last cur and next lines
 *	endif
 *
 */
int
sharpen(unsigned char *buf, int size, int num, FILE *file, unsigned char *Map)
{
    static unsigned char *last, *cur=0, *next;
    static int linelen;
    size_t result = 0;
    int newvalue = 0;
    int i = 0;
    int value = 0;
    int idx = 0;

/*
 *	if no sharpening going on then just read from the file and exit.
 */
    if (ZERO(Beta)) {
	result = fread(buf, size, num, file);
	if (!result)
	    return result;
	for (i=0; i<size*num; i++) {
	    idx = buf[i];
	    CLAMP(idx, 0, size*num);
	    buf[i] = Map[idx];
	}
	return result;
    }

/*
 *	if we are sharpening we depend on the pixel size being one char.
 */
    if (size != 1) {
	fprintf(stderr, "sharpen: size != 1.\n");
	bu_exit(2, NULL);
    }

/*
 *	if this is the first time we have been called then get some
 *	space to load pixels into and read first and second line of
 *	the file.
 */
    if (!cur) {
	linelen=num;
	last = 0;
	cur  = (unsigned char *)bu_calloc(linelen, 1, "cur");
	result = fread(cur, 1, linelen, file);
	for (i=0; i<linelen;i++) {
	    idx = cur[i];
	    CLAMP(idx, 0, size*num);
	    cur[i] = Map[idx];
	}
	if (!result) {
	    free(cur);
	    return result;	/* nothing there! */
	}
	next = (unsigned char *)bu_calloc(linelen, 1, "next");
	result = fread(next, 1, linelen, file);
	if (!result) {
	    free(cur);
	    free(next);
	    return result;	/* nothing there! */
	} else {
	    for (i=0; i<linelen;i++) {
		idx = cur[i];
		CLAMP(idx, 0, size*num);
		cur[i] = Map[idx];
	    }
	}
    } else {
	unsigned char *tmp;

	if (linelen != num) {
	    fprintf(stderr, "sharpen: buffer size changed!\n");
	    bu_exit(2, NULL);
	}
/*
 *		rotate the buffers.
 */
	tmp=last;
	last=cur;
	cur=next;
	next=tmp;
	result=fread(next, 1, linelen, file);
/*
 *		if at EOF then free up a buffer and set next to NULL
 *		to flag EOF for the next time we are called.
 */
	if (!result) {
	    free(next);
	    next = 0;
	} else {
	    for (i=0; i<linelen;i++) {
		idx = cur[i];
		CLAMP(idx, 0, size*num);
		cur[i] = Map[idx];
	    }
	}
    }
/*
 *	if cur is NULL then we are at EOF.  Memory leak here as
 *	we don't free last.
 */
    if (!cur) return 0;

/*
 * Value is the following Laplacian filter kernel:
 *		0.25
 *	0.25	-1.0	0.25
 *		0.25
 *
 * Thus value is zero in constant value areas and none zero on
 * edges.
 *
 * Page 335 of Digital Halftoning defines
 *	J     [n] = J[n] - Beta*Laplacian_filter[n]*J[n]
 *	 sharp
 *
 * J, J, Laplacian_filter[n] are all in the range 0.0 to 1.0
 *     sharp
 *
 * The following is done in mostly integer mode, there is only one
 * floating point multiply done.
 */

/*
 *	if first line
 */
    if (!last) {
	i=0;
	value=next[i] + cur[i+1] - cur[i]*2;
	newvalue = cur[i] - Beta*value*((int)cur[i])/(255*2);
	buf[i] = (newvalue < 0) ? 0 : (newvalue > 255) ?
	    255: newvalue;
	for (; i < linelen-1; i++) {
	    value = next[i] + cur[i-1] + cur[i+1] - cur[i]*3;
	    newvalue = cur[i] - Beta*value*((int)cur[i])/(255*3);
	    buf[i] = (newvalue < 0) ? 0 : (newvalue > 255) ?
		255: newvalue;
	}
	value=next[i] + cur[i-1] - cur[i]*2;
	newvalue = cur[i] - Beta*value*((int)cur[i])/(255*2);
	buf[i] = (newvalue < 0) ? 0 : (newvalue > 255) ?
	    255: newvalue;

/*
 *		first time through so we will need this buffer space
 *		the next time through.
 */
	last  = (unsigned char *)malloc(linelen);
/*
 *	if last line
 */
    } else if (!next) {
	i=0;
	value=last[i] + cur[i+1] - cur[i]*2;
	newvalue = cur[i] - Beta*value*((int)cur[i])/(255*2);
	buf[i] = (newvalue < 0) ? 0 : (newvalue > 255) ?
	    255: newvalue;
	for (; i < linelen-1; i++) {
	    value = last[i] + cur[i-1] + cur[i+1] - cur[i]*3;
	    newvalue = cur[i] - Beta*value*((int)cur[i])/(255*3);
	    buf[i] = (newvalue < 0) ? 0 : (newvalue > 255) ?
		255: newvalue;
	}
	value=last[i] + cur[i-1] - cur[i]*2;
	newvalue = cur[i] - Beta*value*((int)cur[i])/(255*2);
	buf[i] = (newvalue < 0) ? 0 : (newvalue > 255) ?
	    255: newvalue;
/*
 *	all other lines.
 */
    } else {
	i=0;
	value=last[i] + next[i] + cur[i+1] - cur[i]*3;
	newvalue = cur[i] - Beta*value*((int)cur[i])/(255*3);
	buf[i] = (newvalue < 0) ? 0 : (newvalue > 255) ?
	    255: newvalue;
	for (; i < linelen-1; i++) {
	    value = last[i] + next[i] + cur[i-1] + cur[i+1]
		- cur[i]*4;
	    newvalue = cur[i] - Beta*value*((int)cur[i])/(255*4);
	    buf[i] = (newvalue < 0) ? 0 : (newvalue > 255) ?
		255: newvalue;
	}
	value=last[i] + next[i] + cur[i-1] - cur[i]*3;
	newvalue = cur[i] - Beta*value*((int)cur[i])/(255*3);
	buf[i] = (newvalue < 0) ? 0 : (newvalue > 255) ?
	    255: newvalue;
    }
    return linelen;
}

/*
 * Clustered-Dot ordered dither at 45 degrees.
 *	Page 86 of Digital Halftoning.
 */
static unsigned char	classic_ordered[6][6] = {
    {5, 4, 3, 14, 15, 16},
    {6, 1, 2, 13, 18, 17},
    {9, 7, 8, 10, 12, 11},
    {14, 15, 16, 5, 4, 3},
    {13, 18, 17, 6, 1, 2},
    {10, 12, 11, 9, 7, 8}};

/**
 * tone_classic - classic diagonal clustered halftones.
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
 * Author:
 *	Christopher T. Johnson	- 90/03/21
 */
int
tone_classic(int pix, int x, int y, int UNUSED(nx), int UNUSED(ny), int UNUSED(newrow))
{
    int threshold = 14*classic_ordered[( x + 3) % 6][ y % 6];
    if (RandomFlag) {
	threshold += BN_UNIF_DOUBLE(RandomFlag)*63;
    }
    return ((pix*Levels + threshold)/255);
}


/*	tone_floyd	floyd-steinberg dispersed error method.
 *
 * Entry:
 *	pix	Pixel value	0-255
 *	x	Current column
 *	y	Current row
 *	nx	Next column
 *	ny	Next row
 *	newrow	New row flag.
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
 *	BN_UNIF_DOUBLE()	Returns a random double between -0.5 and 0.5.
 *
 * Author:
 *	Christopher T. Johnson	- 90/03/21
 */
int
tone_floyd(int pix, int x, int UNUSED(y), int nx, int UNUSED(ny), int newrow)
{
    static int *error = 0;
    static int *thisline;
    int diff, value;
    int Dir = nx-x;
    double w1, w3, w5, w7;

    if (RandomFlag) {
	double val;
	val = BN_UNIF_DOUBLE(RandomFlag)*1.0/16.0; /* slowest */
	w1 = 1.0/16.0 + val;
	w3 = 3.0/16.0 - val;
	val = BN_UNIF_DOUBLE(RandomFlag)*5.0/16.0; /* slowest */
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
	error = (int *) bu_calloc(width, sizeof(int), "error");
	thisline = (int *) bu_calloc(width, sizeof(int), "thisline");
    }
/*
 *	if this is a new line then trade error for thisline.
 */
    if (newrow) {
	int *p;
	p = error;
	error = thisline;
	thisline = p;
    }

    pix += thisline[x];
    thisline[x] = 0;

    value = (pix*Levels + 127) / 255;
    diff =  pix - (value * 255 /Levels);

    if (x+Dir < width && x+Dir >= 0) {
	thisline[x+Dir] += diff*w7;	/* slow */
	error[x+Dir] += diff*w1;
    }
    error[x] += diff*w5;			/* slow */
    if (x-Dir < width && x-Dir >= 0) {
	error[x-Dir] += diff*w3;
    }
    return value;
}

/*
 * Dispersed-Dot ordered Dither at 0 degrees (n=4)
 * 	From page 135 of Digital Halftoning.
 */
static unsigned char	folly_ordered[4][4] = {
    {2, 16, 3, 13},
    {12, 8, 9, 5},
    {4, 14, 1, 15},
    {10, 6, 11, 7}};

/*	tone_folly	4x4 square ordered dither dispersed (folly and van dam)
 *
 * Entry:
 *	pix	Pixel value	0-255
 *	x	Current column
 *	y	Current row
 *	nx	Next column
 *	ny	Next row
 *	newrow	New row flag.
 *
 * Exit:
 *	returns	0 to Levels
 *
 * Uses:
 *	Debug	- Current debug level.
 *	Levels	- Number of intensity levels.
 *	RandomFlag - should we toss some random numbers?
 *
 * Calls:
 *	BN_UNIF_DOUBLE() - to get random numbers from -0.5 to 0.5
 *
 *  Author -
 *	Christopher T. Johnson	- 90/03/21
 */
int
tone_folly(int pix, int x, int y, int UNUSED(nx), int UNUSED(ny), int UNUSED(newrow))
{
    int threshold = 16*folly_ordered[ x % 4][ y % 4];

    if (RandomFlag) {
	threshold += BN_UNIF_DOUBLE(RandomFlag)*63;
    }
    return ((pix*Levels + threshold)/255);
}

/**
 * tonescale
 *
 * Given a raw pixel value, return a scaled value.
 *
 * This is normally used to map the output devices characteristics to
 * the input intensities.  There can be a different map for each color.
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
 *	EqCubics	x, A, B, C, D of a set of cubics for a curve
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
 */
typedef struct Cubic {
    double	x, A, B, C, D;
} C;
static struct Cubic	*EqCubics=0;
int eq_cubic(int x);

void
tonescale(unsigned char *map, float Slope, float B, int (*eqptr) (/* ??? */))
{
    int i, result;

/*
 * 	Is there a function we should be using?
 * N.B.	This code has not been tested with a function being passed in.
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
	EqCubics = (struct Cubic *)bu_calloc(2, sizeof(struct Cubic), "EqCubics");
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
		fprintf(stderr, "tonescale: y=%d, x=%d\n",
			result, i);
	    }
	    result=0;
	} else if (result > 255) {
	    if (Debug >= DLEVEL) {
		fprintf(stderr, "tonescale: y=%d, x=%d\n",
			result, i);
	    }
	    result=255;
	}

	map[i] = result;
    }
}

/* eq_cubic	default tone scale algorithm
 *
 * implement
 *	y = A+B*(X-x)+C*(X-x)^2+D*(X-x)^3
 * as a default tonescale algorithm;
 *
 * Entry:
 *	x	x value for equation.
 *
 * Exit:
 *	returns	y in the range 0-255  requires clipping.
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
eq_cubic(int x)
{
    int y;
    struct Cubic *p = EqCubics;

    if (!p) {
	fprintf(stderr, "eq_cubic called with no cubics!\n");
	return x;
    }
    while (x >= (p+1)->x) p++;

    y = ((p->D * (x - p->x) + p->C) * (x - p->x) + p->B)
	* (x - p->x) + p->A;

    CLAMP(y, 0, 255);
    return y;
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
cubic_init(int n, int *x, int *y)
{
    int i;
    double *h, *alpha, *mi, *z, *l;

    h = (double *) malloc(n*sizeof(double));
    alpha = (double *) malloc(n*sizeof(double));
    mi = (double *) malloc(n*sizeof(double));
    z = (double *) malloc(n*sizeof(double));
    l = (double *) malloc(n*sizeof(double));

    EqCubics = (struct Cubic *)bu_calloc(n, sizeof(struct Cubic), "EqCubics");

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
	for (i=0;i<n;i++) {
	    fprintf(stderr, "x=%g, A=%g, B=%g, C=%g, D=%g\n",
		    EqCubics[i].x, EqCubics[i].A, EqCubics[i].B,
		    EqCubics[i].C, EqCubics[i].D);
	}
    }
}

/*	tone_simple	thresh hold method
 *
 * Entry:
 *	pix	Pixel value	0-255
 *	x	Current column
 *	y	Current row
 *	nx	Next column
 *	ny	Next row
 *	newrow	New row flag.
 *
 * Exit:
 *	returns	0 or 1
 *
 * Uses:
 *	Debug	- Debug level (currently not used.)
 *	Levels	- Number of intensity levels.
 *	RandomFlag - Use random threshold flag.
 *
 * Calls:
 *	BN_UNIF_DOUBLE	- return a random number between -0.5 and 0.5;
 *
 * Author:
 *	Christopher T. Johnson	- 90/03/21
 */
int
tone_simple(int pix, int UNUSED(x), int UNUSED(y), int UNUSED(nx), int UNUSED(ny), int UNUSED(newrow))
{
    int threshold;
    if (RandomFlag) {
	threshold = THRESHOLD + BN_UNIF_DOUBLE(RandomFlag)*127;
    } else {
	threshold = THRESHOLD;
    }
    return ((pix*Levels + threshold) / 256 );
}


static const char usage[] = "\
Usage: halftone [-R -S -a] [-D Debug_Level]\n\
	[-s squarefilesize] [-w file_width] [-n file_height]\n\
	[-B contrast] [-I #_of_intensity_levels] [-T x y ... (tone curve)]\n\
	[-M Method] [file.bw]\n\
	where Method is chosen from:\n\
	    0  Floyd-Steinberg\n\
	    1  45-Degree Classic Screen\n\
	    2  Thresholding\n\
	    3  0-Degree Dispersed Screen\n\
       (stdin used with '<' construct if file.bw not supplied)\n";

/*
 * process parameters and setup working environment
 *
 * Entry:
 *	argc	- number of arguments.
 *	argv	- the arguments.
 *
 * Exit:
 *	parameters have been set.
 *	if there is a fatal error, exit non-zero
 *
 * Uses:
 *	width	- width of picture
 *	height	- height of picture
 *	Beta	- sharpening value
 *	surpent	- to surpenten rasters?
 *	Levels	- number of intensity levels
 *	Debug	- debug level
 *	RandomFlag - Add noise to processes.
 *
 * Calls:
 *	cubic_init - setup for tonescale.
 *
 * Method:
 *	straight-forward.
 */
void
setup(int argc, char **argv)
{
    int c;
    int i, j;
    int *Xlist, *Ylist;
    int	autosize = 0;

    while ((c = bu_getopt(argc, argv, "D:s:an:w:B:M:RSI:T:h?")) != -1) {
	switch (c) {
	    case 's':
		width = height = atol(bu_optarg);
		break;
	    case 'n':
		height = atol(bu_optarg);
		break;
	    case 'w':
		width = atol(bu_optarg);
		break;
	    case 'a':
		autosize = 1;
		break;
	    case 'B':
		Beta = atof(bu_optarg);
		break;
	    case 'M':
		Method = atoi(bu_optarg);
		if (Method <0 || Method >3) {
			fprintf(stderr,"halftone: Method must be 0,1,2,3; set to default (0)\n");
			Method = 0;
		}
		break;
	    case 'R':
		RandomFlag = bn_unif_init(1, 0);
		break;
	    case 'S':
		Surpent = 1;
		break;
	    case 'I':
		Levels = atoi(bu_optarg)-1;
		V_MAX(Levels, 1);
		break;
/*
 * Tone scale processing is a little strange.  The -T option is followed
 * by a list of points.  The points are collected and one point is added
 * at 1024, 1024 to let tonescale be stupid.  Cubic_init takes the list
 * of points and generates "cubics" for tonescale to use in generating
 * curve to use for the tone map.  If tonescale is called with no cubics
 * defined tonescale will generate a straight-line (generally from 0, 0 to
 * 255, 255).
 */
	    case 'T':
		--bu_optind;
		for (i=bu_optind; i < argc && (isdigit((int)*argv[i]) ||
					       (*argv[i] == '-' && isdigit((int)(*(argv[i]+1))))); i++);
		if ((c=i-bu_optind) % 2) {
		    fprintf(stderr, "Missing Y coordinate for tone map.\n");
		    bu_exit(1, NULL);
		}
		Xlist = (int *) bu_malloc((c+2)*sizeof(int), "Xlist");
		Ylist = (int *) bu_malloc((c+2)*sizeof(int), "Ylist");

		for (j=0;bu_optind < i; ) {
		    Xlist[j] = atoi(argv[bu_optind++]);
		    Ylist[j] = atoi(argv[bu_optind++]);
		    j++;
		}
		Xlist[j] = 1024;
		Ylist[j] = 1024;
		if (Debug>6) fprintf(stderr, "Number points=%d\n", j+1);
		(void) cubic_init(j+1, Xlist, Ylist);
		bu_free(Xlist, "Xlist");
		bu_free(Ylist, "Ylist");
		break;
/*
 * Debug is not well used at this point a value of 9 will get all
 * debug statements.  Debug is a level indicator NOT a bit flag.
 */
	    case 'D':
		Debug = atoi(bu_optarg);
		break;
	    default:
		bu_exit(1, usage);
	}
    }
/*
 *	if there are no extra arguments and stdin is a tty then
 *	the user has given us no input file.  Spit a usage message
 * 	at them and exit.
 */
    if (bu_optind >= argc) {
	if ( isatty(fileno(stdin)) ) {
	    bu_exit(1, usage);
	}
	if (autosize) {
	    (void) fprintf(stderr, "%s", usage);
	    bu_exit(1, "Automatic sizing can not be used with pipes.\n");
	}
    } else {
	char *ifname = bu_file_realpath(argv[bu_optind], NULL);
	if (freopen(ifname, "rb", stdin) == NULL ) {
	    bu_free(ifname,"ifname alloc from bu_file_realpath");
	    bu_exit(1, "halftone: cannot open \"%s\" for reading.\n", argv[bu_optind]);
	}
	bu_free(ifname,"ifname alloc from bu_file_realpath");
	if (autosize) {
	    if ( !fb_common_file_size((size_t *)&width, (size_t *)&height, argv[bu_optind], 1)) {
		(void) fprintf(stderr, "halftone: unable to autosize.\n");
	    }
	}
    }

    if ( argc > ++bu_optind) {
	(void) fprintf(stderr, "halftone: excess argument(s) ignored.\n");
    }
}

int
main(int argc, char **argv)
{
    int pixel, x, y, i;
    unsigned char *Line, *Out;
    int NewFlag = 1;
    int Scale;
    unsigned char Map[256];
    size_t ret;

    bu_setprogname(argv[0]);

    setmode(fileno(stdin), O_BINARY);
    setmode(fileno(stdout), O_BINARY);

    /*
     *	parameter processing.
     */
    setup(argc, argv);
    /*
     *	Get a tone map.  Map is the result.  1.0 is slope, 0.0 is
     *	the Y intercept (y=mx+b). 0 is the address of a function to
     *	do a x to y mapping, 0 means use the default function.
     */
    (void) tonescale(Map, 1.0, 0.0, 0);

    /*
     * Currently the halftone file is scaled from 0 to 255 on output to
     * ease display via bw-fb.  In the future there might be flag to
     * set Scale to 1 to get a unscaled output.
     */
    Scale = 255/Levels;

    if (Debug) {
	fprintf(stderr, "Debug = %d, Scale = %d\n", Debug, Scale);
    }

    if (Debug>2) {
	for (i=0;i<256;i++) fprintf(stderr, "%d ", Map[i]);
	fprintf(stderr, "\n");
    }

    Line = (unsigned char *) bu_malloc(width, "Line");
    Out = (unsigned char *) bu_malloc(width, "Out");
    /*
     * should be a test here to make sure we got the memory requested.
     */

    /*
     *	Currently only the Floyd-Steinberg method uses the surpent flag
     *	so we make things easy with in the 'y' loop by resetting surpent
     *	for all other methods to "No Surpent".
     */
    if (Method != M_FLOYD) Surpent = 0;

    for (y=0; y<height; y++) {
	int NextX;
	/*
	 * 		A few of the methods benefit by knowing when a new line is
	 *		started.
	 */
	NewFlag = 1;
	(void) sharpen(Line, 1, width, stdin, Map);
	/*
	 *		Only M_FLOYD will have Surpent != 0.
	 */
	if (Surpent && y % 2) {
	    for (x=width-1; x>=0; x--) {
		pixel = Line[x];
		Out[x] = Scale*tone_floyd(pixel, x, y, x-1,
			y+1, NewFlag);
		NewFlag = 0;
	    }
	} else {
	    for (x=0; x<width; x++) {
		NextX = x+1;
		pixel = Line[x];
		switch (Method) {
		    case M_FOLLY:
			Out[x] = Scale*tone_folly(pixel, x, y,
				NextX, y+1, NewFlag);
			break;
		    case M_FLOYD:
			Out[x] = Scale*tone_floyd(pixel, x, y,
				NextX, y+1, NewFlag);
			break;
		    case M_THRESH:
			Out[x]=Scale*tone_simple(pixel, x, y,
				NextX, y+1, NewFlag);
			break;
		    case M_CLASSIC:
			Out[x]=Scale*tone_classic(pixel, x, y,
				NextX, y+1, NewFlag);
			break;
		}
		NewFlag=0;
	    }
	}
	ret = fwrite(Out, 1, width, stdout);
	if ( ret < (size_t)width)
	    perror("fwrite");
    }
    bu_free(Line, "Line");
    bu_free(Out, "Out");
    return 0;
}

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
