/*
 * This software is copyrighted as noted below.  It may be freely copied,
 * modified, and redistributed, provided that the copyright notice is 
 * preserved on all copies.
 * 
 * There is no warranty or other guarantee of fitness for this software,
 * it is provided solely "as is".  Bug reports or fixes may be sent
 * to the author, who may or may not act on them as he desires.
 *
 * You may not include this software in a program or other software product
 * without supplying the source, or without informing the end-user that the 
 * source is available for no extra charge.
 *
 * If you modify this software, you should include a notice giving the
 * name of the person performing the modification, the date of modification,
 * and the reason for such modification.
 */
/*
 * colorquant.c
 *
 * Perform variance-based color quantization on a "full color" image.
 * Author:	Craig Kolb
 *		Department of Mathematics
 *		Yale University
 *		kolb@yale.edu
 * Date:	Tue Aug 22 1989
 * Copyright (C) 1989 Craig E. Kolb
 * $Id$
 *
 * $Log$
 * Revision 3.0.1.4  1992/04/30  14:06:48  spencer
 * patch3: lint fixes.
 *
 * Revision 3.0.1.3  1991/02/06  16:46:47  spencer
 * patch3: Fix round-off error in variance calculation that sometimes
 * patch3: caused multiple boxes with the same center to be cut.
 * patch3: Change 'fast' argument to 'flags' argument.  Currently:
 * patch3: 	CQ_FAST: same as old fast argument
 * patch3: 	CQ_QUANTIZE: data is not prequantized to bits
 * patch3: 	CQ_NO_RGBMAP: don't build rgbmap.
 *
 * Revision 3.0.1.2  90/11/29  15:18:04  spencer
 * Remove a typo.
 * 
 * 
 * Revision 3.0.1.1  90/11/19  16:59:48  spencer
 * Use inv_cmap instead of find_colors.
 * Changes to process multiple files into one colormap (accum_hist arg).
 * Delete 'otherimages' argument -- unnecessary with faster inv_cmap code.
 * 
 * 
 * Revision 3.0  90/08/03  15:20:11  spencer
 * Establish version 3.0 base.
 * 
 * Revision 1.6  90/07/29  08:06:06  spencer
 * If HUGE isn't defined, make it HUGE_VAL (for ansi).
 * 
 * Revision 1.5  90/07/26  17:25:48  rgb
 * Added a parameter to colorquant for rgbmap construction.
 * 
 * Revision 1.4  90/07/13  14:53:31  spencer
 * Get rid of ARB_ARG sh*t.
 * Change a couple of vars to double so that HUGE won't cause problems.
 * 
 * Revision 1.3  90/06/28  21:42:56  spencer
 * Declare internal functions properly.
 * Delete unused global variable.
 * 
 * Revision 1.2  90/06/28  13:18:56  spencer
 * Make internal functions static.
 * Build entire RGB cube, not just those colors used by this image.  This
 * is so dithering will work.
 * 
 * Revision 1.1  90/06/18  20:45:17  spencer
 * Initial revision
 * 
 * Revision 1.3  89/12/03  18:27:16  craig
 * Removed bogus integer casts in distance calculation in makenearest().
 * 
 * Revision 1.2  89/12/03  18:13:12  craig
 * FindCutpoint now returns FALSE if the given box cannot be cut.  This
 * to avoid overflow problems in CutBox.
 * "whichbox" in GreatestVariance() is now initialized to 0.
 * 
 */
static char rcsid[] = "$Header$";

#include <math.h>
#include <stdio.h>
#include "rle_config.h"
#include "colorquant.h"

/* Ansi uses HUGE_VAL. */
#ifndef HUGE
#ifdef HUGE_VAL
#define HUGE HUGE_VAL
#else
#define HUGE MAXFLOAT
#endif
#endif

static void QuantHistogram();
static void BoxStats();
static void UpdateFrequencies();
static void ComputeRGBMap();
static void SetRGBmap();
#ifdef slow_color_map
static void find_colors();
static int  getneighbors();
static int  makenearest();
#else
extern void inv_cmap();
#endif
static int  CutBoxes();
static int  CutBox();
static int  GreatestVariance();
static int  FindCutpoint();



/* 
 * The colorquant() routine has been tested on an Iris 4D70 workstation,
 * a Sun 3/60 workstation, and (to some extent), a Macintosh.
 * 
 * Calls to bzero() may have to be replaced with the appropriate thing on
 * your system.  (bzero(ptr, len) writes 'len' 0-bytes starting at the location
 * pointed to by ptr.)
 * 
 * Although I've tried to avoid integer overflow problems where ever possible,
 * it's likely I've missed a spot where an 'int' should really be a 'long'.
 * (On the machine this was developed on, an int == long == 32 bits.)
 * 
 * Note that it's quite easy to optimize this code for a given value for
 * 'bits'.  In addition, if you assume bits is small and
 * that the total number of pixels is relatively small, there are several
 * places that integer arithmetic may be substituted for floating-point.
 * One such place is the loop in BoxStats -- mean and var need not necessary
 * be floats.
 * 
 * As things stand, the maximum number of colors to which an image may
 * be quantized is 256.  This limit may be overcome by changing rgbmap and
 * colormap from pointers to characters to pointers to something larger.
 */

/*
 * Maximum number of colormap entries.  To make larger than 2^8, the rgbmap
 * type will have to be changed from unsigned chars to something larger.
 */
#define MAXCOLORS		256
/*
 * Value corrersponding to full intensity in colormap.  The values placed
 * in the colormap are scaled to be between zero and this number.  Note
 * that anything larger than 255 is going to lead to problems, as the
 * colormap is declared as an unsigned char.
 */
#define FULLINTENSITY		255
#define MAX(x,y)	((x) > (y) ? (x) : (y))

/*
 * Readability constants.
 */
#define REDI		0	
#define GREENI		1
#define BLUEI		2	
#define TRUE		1
#define FALSE		0

typedef struct {
	double		weightedvar;		/* weighted variance */
	float		mean[3];		/* centroid */
	unsigned long 	weight,			/* # of pixels in box */
			freq[3][MAXCOLORS];	/* Projected frequencies */
	int 		low[3], high[3];	/* Box extent */
} Box;

static unsigned long	*Histogram,		/* image histogram */
			NPixels,		/* # of pixels in an image*/
			SumPixels;		/* total # of pixels */
static unsigned int	Bits,			/* # significant input bits */
			ColormaxI;		/* # of colors, 2^Bits */
static Box		*Boxes;			/* Array of color boxes. */

/*
 * Perform variance-based color quantization on a 24-bit image.
 *
 * Input consists of:
 *	red, green, blue	Arrays of red, green and blue pixel
 *				intensities stored as unsigned characters.
 *				The color of the ith pixel is given
 *				by red[i] green[i] and blue[i].  0 indicates
 *				zero intensity, 255 full intensity.
 *	pixels			The length of the red, green and blue arrays
 *				in bytes, stored as an unsigned long int.
 *	colormap		Points to the colormap.  The colormap
 *				consists of red, green and blue arrays.
 *				The red/green/blue values of the ith
 *				colormap entry are given respectively by
 *				colormap[0][i], colormap[1][i] and
 *				colormap[2][i].  Each entry is an unsigned char.
 *	colors			The number of colormap entries, stored
 *				as an integer.	
 *	bits			The number of significant bits in each entry
 *				of the red, greena and blue arrays. An integer.
 *	rgbmap			An array of unsigned chars of size (2^bits)^3.
 *				This array is used to map from pixels to
 *				colormap entries.  The 'prequantized' red,
 *				green and blue components of a pixel
 *				are used as an index into rgbmap to retrieve
 *				the index which should be used into the colormap
 *				to represent the pixel.  In short:
 *				index = rgbmap[(((r << bits) | g) << bits) | b];
 * 	flags			A set of bit-flags:
 * 		CQ_FAST		If set, the rgbmap will be constructed
 *				quickly.  If not set, the rgbmap will
 *				be built more slowly, but more
 *				accurately.  In most cases, fast
 *				should be set, as the error introduced
 *				by the approximation is usually small.
 * 		CQ_QUANTIZE:	If set, the data in red, green, and
 * 				blue is not pre-quantized, and will be
 * 				quantized "on the fly".  This slows
 * 				the routine slightly.  If not set, the
 * 				data has been prequantized to 'bits'
 * 				significant bits.
 * 		
 *	accum_hist		If non-zero the histogram will accumulate and 
 *				reflect pixels from multiple images.
 *				when 1, the histogram will be initialized and
 *				summed, but not thrown away OR processed. when 
 *				2 the image RGB will be added to it.  When 3 
 *				Boxes are cut and a colormap and rgbmap
 *				are be returned, Histogram is freed too.
 *				When zero, all code is executed as per normal.
 *
 * colorquant returns the number of colors to which the image was
 * quantized.
 */
#define INIT_HIST 1
#define USE_HIST 2
#define PROCESS_HIST 3
int
colorquant(red,green,blue,pixels,colormap,colors,bits,rgbmap,flags,accum_hist)
unsigned char *red, *green, *blue;
unsigned long pixels;
unsigned char *colormap[3];
int colors, bits;
unsigned char *rgbmap;
int flags, accum_hist;
{
    int	i,			/* Counter */
    OutColors,			/* # of entries computed */
    Colormax;			/* quantized full-intensity */ 
    float	Cfactor;	/* Conversion factor */

    if (accum_hist < 0 || accum_hist > PROCESS_HIST)
	fprintf(stderr, "colorquant: bad value for accum_hist\n");

    ColormaxI = 1 << bits;	/* 2 ^ Bits */
    Colormax = ColormaxI - 1;
    Bits = bits;
    NPixels = pixels;
    Cfactor = (float)FULLINTENSITY / Colormax;

    if (! accum_hist || accum_hist == INIT_HIST) {
	Histogram = (unsigned long *)calloc(ColormaxI*ColormaxI*ColormaxI, 
					    sizeof(long));
	Boxes = (Box *)malloc(colors * sizeof(Box));
	/*
	 * Zero-out the projected frequency arrays of the largest box.
	 */
	bzero(Boxes->freq[0], ColormaxI * sizeof(unsigned long));
	bzero(Boxes->freq[1], ColormaxI * sizeof(unsigned long));
	bzero(Boxes->freq[2], ColormaxI * sizeof(unsigned long));
	SumPixels = 0;
    }

    SumPixels += NPixels;

    if ( accum_hist != PROCESS_HIST ) 
	QuantHistogram(red, green, blue, &Boxes[0], flags&CQ_QUANTIZE);

    if ( !accum_hist || accum_hist == PROCESS_HIST) {
	OutColors = CutBoxes(Boxes, colors);

	/*
	 * We now know the set of representative colors.  We now
	 * must fill in the colormap and convert the representatives
	 * from their 'prequantized' range to 0-FULLINTENSITY.
	 */
	for (i = 0; i < OutColors; i++) {
	    colormap[0][i] =
		(unsigned char)(Boxes[i].mean[REDI] * Cfactor + 0.5);
	    colormap[1][i] =
		(unsigned char)(Boxes[i].mean[GREENI] * Cfactor + 0.5);
	    colormap[2][i] =
		(unsigned char)(Boxes[i].mean[BLUEI] * Cfactor + 0.5);
	}

	if ( !(flags & CQ_NO_RGBMAP) )
	    ComputeRGBMap(Boxes, OutColors, rgbmap, bits, colormap,
			  flags&CQ_FAST);

	free((char *)Histogram);
	free((char *)Boxes);
	return OutColors;	/* Return # of colormap entries */
    }
    return 0;
}

/*
 * Compute the histogram of the image as well as the projected frequency
 * arrays for the first world-encompassing box.
 */
static void
QuantHistogram(r, g, b, box, quantize)
register unsigned char *r, *g, *b;
Box *box;
int quantize;
{
	register unsigned long *rf, *gf, *bf;
	unsigned long i;

	rf = box->freq[0];
	gf = box->freq[1];
	bf = box->freq[2];

	/*
	 * We compute both the histogram and the proj. frequencies of
	 * the first box at the same time to save a pass through the
	 * entire image. 
	 */
	
	if ( !quantize )
	    for (i = 0; i < NPixels; i++) {
		rf[*r]++;
		gf[*g]++;
		bf[*b]++;
		Histogram[((((*r++)<<Bits)|(*g++))<<Bits)|(*b++)]++;
	    }
	else
	{
	    unsigned char rr, gg, bb;
	    for (i = 0; i < NPixels; i++) {
		rr = (*r++)>>(8-Bits);
		gg = (*g++)>>(8-Bits);
		bb = (*b++)>>(8-Bits);
		rf[rr]++;
		gf[gg]++;
		bf[bb]++;
		Histogram[(((rr<<Bits)|gg)<<Bits)|bb]++;
	    }
	}
	    
}

/*
 * Interatively cut the boxes.
 */
static int
CutBoxes(boxes, colors) 
Box	*boxes;
int	colors;
{
	int curbox;

	boxes[0].low[REDI] = boxes[0].low[GREENI] = boxes[0].low[BLUEI] = 0;
	boxes[0].high[REDI] = boxes[0].high[GREENI] =
			      boxes[0].high[BLUEI] = ColormaxI;
	boxes[0].weight = SumPixels;

	BoxStats(&boxes[0]);

	for (curbox = 1; curbox < colors; curbox++) {
		if (CutBox(&boxes[GreatestVariance(boxes, curbox)],
			   &boxes[curbox]) == FALSE)
				break;
	}

	return curbox;
}

/*
 * Return the number of the box in 'boxes' with the greatest variance.
 * Restrict the search to those boxes with indices between 0 and n-1.
 */
static int
GreatestVariance(boxes, n)
Box *boxes;
int n;
{
	register int i, whichbox = 0;
	float max;

	max = -1;
	for (i = 0; i < n; i++) {
		if (boxes[i].weightedvar > max) {
			max = boxes[i].weightedvar;
			whichbox = i;
		}
	}
	return whichbox;
}

/*
 * Compute mean and weighted variance of the given box.
 */
static void
BoxStats(box)
register Box *box;
{
	register int i, color;
	unsigned long *freq;
	double mean, var;

	if(box->weight == 0) {
		box->weightedvar = 0;
		return;
	}

	box->weightedvar = 0.;
	for (color = 0; color < 3; color++) {
		var = mean = 0;
		i = box->low[color];
		freq = &box->freq[color][i];
		for (; i < box->high[color]; i++, freq++) {
			mean += i * *freq;
			var += i*i* *freq;
		}
		box->mean[color] = mean / (double)box->weight;
		box->weightedvar += var - box->mean[color]*box->mean[color]*
					(double)box->weight;
	}
	box->weightedvar /= SumPixels;
}

/*
 * Cut the given box.  Returns TRUE if the box could be cut, FALSE otherwise.
 */
static int
CutBox(box, newbox)
Box *box, *newbox;
{
	int i;
	double totalvar[3];
	Box newboxes[3][2];

	if (box->weightedvar == 0. || box->weight == 0)
		/*
		 * Can't cut this box.
		 */
		return FALSE;

	/*
	 * Find 'optimal' cutpoint along each of the red, green and blue
	 * axes.  Sum the variances of the two boxes which would result
	 * by making each cut and store the resultant boxes for 
	 * (possible) later use.
	 */
	for (i = 0; i < 3; i++) {
		if (FindCutpoint(box, i, &newboxes[i][0], &newboxes[i][1]))
			totalvar[i] = newboxes[i][0].weightedvar +
				newboxes[i][1].weightedvar;
		else
			totalvar[i] = HUGE;
	}

	/*
	 * Find which of the three cuts minimized the total variance
	 * and make that the 'real' cut.
	 */
	if (totalvar[REDI] <= totalvar[GREENI] &&
	    totalvar[REDI] <= totalvar[BLUEI]) {
		*box = newboxes[REDI][0];
		*newbox = newboxes[REDI][1];
	} else if (totalvar[GREENI] <= totalvar[REDI] &&
		 totalvar[GREENI] <= totalvar[BLUEI]) {
		*box = newboxes[GREENI][0];
		*newbox = newboxes[GREENI][1];
	} else {
		*box = newboxes[BLUEI][0];
		*newbox = newboxes[BLUEI][1];
	}

	return TRUE;
}

/*
 * Compute the 'optimal' cutpoint for the given box along the axis
 * indcated by 'color'.  Store the boxes which result from the cut
 * in newbox1 and newbox2.
 */
static int
FindCutpoint(box, color, newbox1, newbox2)
Box *box, *newbox1, *newbox2;
int color;
{
	float u, v, max;
	int i, maxindex, minindex, cutpoint;
	unsigned long optweight, curweight;

	if (box->low[color] + 1 == box->high[color])
		return FALSE;	/* Cannot be cut. */
	minindex = (int)((box->low[color] + box->mean[color]) * 0.5);
	maxindex = (int)((box->mean[color] + box->high[color]) * 0.5);

	cutpoint = minindex;
	optweight = box->weight;

	curweight = 0;
	for (i = box->low[color] ; i < minindex ; i++)
		curweight += box->freq[color][i];
	u = 0.;
	max = -1;
	for (i = minindex; i <= maxindex ; i++) {
		curweight += box->freq[color][i];
		if (curweight == box->weight)
			break;
		u += (float)(i * box->freq[color][i]) /
					(float)box->weight;
		v = ((float)curweight / (float)(box->weight-curweight)) *
				(box->mean[color]-u)*(box->mean[color]-u);
		if (v > max) {
			max = v;
			cutpoint = i;
			optweight = curweight;
		}
	}
	cutpoint++;
	*newbox1 = *newbox2 = *box;
	newbox1->weight = optweight;
	newbox2->weight -= optweight;
	newbox1->high[color] = cutpoint;
	newbox2->low[color] = cutpoint;
	UpdateFrequencies(newbox1, newbox2);
	BoxStats(newbox1);
	BoxStats(newbox2);

	return TRUE;	/* Found cutpoint. */
}

/*
 * Update projected frequency arrays for two boxes which used to be
 * a single box.
 */
static void
UpdateFrequencies(box1, box2)
register Box *box1, *box2;
{
	register unsigned long myfreq, *h;
	register int b, g, r;
	int roff;

	bzero(box1->freq[0], ColormaxI * sizeof(unsigned long));
	bzero(box1->freq[1], ColormaxI * sizeof(unsigned long));
	bzero(box1->freq[2], ColormaxI * sizeof(unsigned long)); 

	for (r = box1->low[0]; r < box1->high[0]; r++) {
		roff = r << Bits;
		for (g = box1->low[1];g < box1->high[1]; g++) {
			b = box1->low[2];
			h = Histogram + (((roff | g) << Bits) | b);
			for (; b < box1->high[2]; b++) {
				if ((myfreq = *h++) == 0)
					continue;
				box1->freq[0][r] += myfreq;
				box1->freq[1][g] += myfreq;
				box1->freq[2][b] += myfreq;
				box2->freq[0][r] -= myfreq;
				box2->freq[1][g] -= myfreq;
				box2->freq[2][b] -= myfreq;
			}
		}
	}
}

/*
 * Compute RGB to colormap index map.
 */
static void
ComputeRGBMap(boxes, colors, rgbmap, bits, colormap, fast)
Box *boxes;
int colors;
unsigned char *rgbmap, *colormap[3];
int bits, fast;
{
	register int i;

	if (fast) {
		/*
		 * The centroid of each box serves as the representative
		 * for each color in the box.
		 */
		for (i = 0; i < colors; i++)
			SetRGBmap(i, &boxes[i], rgbmap, bits);
	} else
		/*
		 * Find the 'nearest' representative for each
		 * pixel.
		 */
#ifdef slow_color_map
		find_colors(boxes, colors, rgbmap, bits, colormap, 1);
#else
		inv_cmap(colors, colormap, bits, Histogram, rgbmap);
#endif
}

/*
 * Make the centroid of "boxnum" serve as the representative for
 * each color in the box.
 */
static void
SetRGBmap(boxnum, box, rgbmap, bits)
int boxnum;
Box *box;
unsigned char *rgbmap;
int bits;
{
	register int r, g, b;
	
	for (r = box->low[REDI]; r < box->high[REDI]; r++) {
		for (g = box->low[GREENI]; g < box->high[GREENI]; g++) {
			for (b = box->low[BLUEI]; b < box->high[BLUEI]; b++) {
				rgbmap[(((r<<bits)|g)<<bits)|b]=(char)boxnum;
			}
		}
	}
}

#ifdef slow_color_map
/*
 * Form colormap and NearestColor arrays.
 */
static void
find_colors(boxes, colors, rgbmap, bits, colormap, otherimages)
int colors;
Box *boxes;
unsigned char *rgbmap;
int bits;
unsigned char *colormap[3];
int otherimages;
{
	register int i;
	int num, *neighbors;

	neighbors = (int *)malloc(colors * sizeof(int));

	/*
	 * Form map of representative (nearest) colors.
	 */
	for (i = 0; i < colors; i++) {
		/*
		 * Create list of candidate neighbors and
		 * find closest representative for each
		 * color in the box.
		 */
		num = getneighbors(boxes, i, neighbors, colors, colormap);
		makenearest(boxes, i, num, neighbors, rgbmap, bits, colormap, 
			    otherimages);
	}
	free((char *)neighbors);
}

/*
 * In order to minimize our search for 'best representative', we form the
 * 'neighbors' array.  This array contains the number of the boxes whose
 * centroids *might* be used as a representative for some color in the
 * current box.  We need only consider those boxes whose centroids are closer
 * to one or more of the current box's corners than is the centroid of the
 * current box. 'Closeness' is measured by Euclidean distance.
 */
static
getneighbors(boxes, num, neighbors, colors, colormap)
Box *boxes;
int num, colors, *neighbors;
unsigned char *colormap[3];
{
	register int i, j;
	Box *bp;
	float dist, LowR, LowG, LowB, HighR, HighG, HighB, ldiff, hdiff;

	bp = &boxes[num];

	ldiff = bp->low[REDI] - bp->mean[REDI];
	ldiff *= ldiff;
	hdiff = bp->high[REDI] - bp->mean[REDI];
	hdiff *= hdiff;
	dist = MAX(ldiff, hdiff);

	ldiff = bp->low[GREENI] - bp->mean[GREENI];
	ldiff *= ldiff;
	hdiff = bp->high[GREENI] - bp->mean[GREENI];
	hdiff *= hdiff;
	dist += MAX(ldiff, hdiff);

	ldiff = bp->low[BLUEI] - bp->mean[BLUEI];
	ldiff *= ldiff;
	hdiff = bp->high[BLUEI] - bp->mean[BLUEI];
	hdiff *= hdiff;
	dist += MAX(ldiff, hdiff);

#ifdef IRIS
	dist = fsqrt(dist);
#else
	dist = (float)sqrt((double)dist);
#endif

	/*
	 * Loop over all colors in the colormap, the ith entry of which
	 * corresponds to the ith box.
	 *
	 * If the centroid of a box is as close to any corner of the
	 * current box as is the centroid of the current box, add that
	 * box to the list of "neighbors" of the current box.
	 */
	HighR = (float)bp->high[REDI] + dist;
	HighG = (float)bp->high[GREENI] + dist;
	HighB = (float)bp->high[BLUEI] + dist;
	LowR = (float)bp->low[REDI] - dist;
	LowG = (float)bp->low[GREENI] - dist;
	LowB = (float)bp->low[BLUEI] - dist;
	for (i = j = 0, bp = boxes; i < colors; i++, bp++) {
		if (LowR <= bp->mean[REDI] && HighR >= bp->mean[REDI] &&
		    LowG <= bp->mean[GREENI] && HighG >= bp->mean[GREENI] &&
		    LowB <= bp->mean[BLUEI] && HighB >= bp->mean[BLUEI])
			neighbors[j++] = i;
	}

	return j;	/* Return the number of neighbors found. */
}

/*
 * Assign representative colors to every pixel in a given box through
 * the construction of the NearestColor array.  For each color in the
 * given box, we look at the list of neighbors passed to find the
 * one whose centroid is closest to the current color.
 */
static
makenearest(boxes, boxnum, nneighbors, neighbors, rgbmap, bits, colormap, 
	    otherimages)
Box *boxes;
int boxnum;
int nneighbors, *neighbors, bits;
unsigned char *rgbmap, *colormap[3];
int otherimages;
{
	register int n, b, g, r;
	double rdist, gdist, bdist, dist, mindist;
	int which, *np;
	Box *box;

	box = &boxes[boxnum];

	for (r = box->low[REDI]; r < box->high[REDI]; r++) {
		for (g = box->low[GREENI]; g < box->high[GREENI]; g++) {
			for (b = box->low[BLUEI]; b < box->high[BLUEI]; b++) {
/*
 * The following "if" is to avoid doing extra work when the RGBmap is
 * not going to be used for other images.
 */
			        if ((!otherimages) && 
				    (Histogram[(((r<<bits)|g)<<bits)|b] == 0))
					continue;

				mindist = HUGE;
				/*
				 * Find the colormap entry which is
				 * closest to the current color.
				 */
				np = neighbors;
				for (n = 0; n < nneighbors; n++, np++) {
					rdist = r-boxes[*np].mean[REDI];
					gdist = g-boxes[*np].mean[GREENI];
					bdist = b-boxes[*np].mean[BLUEI];
					dist = rdist*rdist + gdist*gdist + bdist*bdist;
					if (dist < mindist) {
						mindist = dist;
						which = *np; 
					}
				}
				/*
				 * The colormap entry closest to this
				 * color is used as a representative.
				 */
				rgbmap[(((r<<bits)|g)<<bits)|b] = which;
			}
		}
	}
}
#endif /* slow_color_map */
