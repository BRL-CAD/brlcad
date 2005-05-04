/*                      P I X M O R P H . C
 * BRL-CAD
 *
 * Copyright (C) 1996-2005 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this file; see the file named COPYING for more
 * information.
 */
/** @file pixmorph.c
 *
 *  Utility for morphing two BRL-CAD pix files.
 *  
 *  Author -
 *      Glenn Durfee
 *  
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 *
 *  Description -
 *      Morphs two pix files.  Performs the morph according to the given line
 *      segment correspondence file and two values in [0,1]: the first,
 *      warpfrac, is a value which describes how far each image is warped;
 *      the second, dissolvefrac, specifies how much of a cross-dissolve
 *      is performed on the two resulting warped images to produce an
 *      output.  Typically, the user sets warpfrac = dissolvefrac.
 *      See the man page for more details.
 *      
 *  For details of the morph algorithm, see
 *        T. Beier and S. Neely.  Feature-Based Image Metamorphosis.  In
 *        "SIGGRAPH 1992 Computer Graphics Proceedings (volume 26 number 2)"
 *        (Chicago, July 26-31, 1992).
 */

#include "common.h"

#ifdef HAVE_UNISTD_H
#  include <unistd.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <math.h>

#include "machine.h"
#include "bu.h"
#include "vmath.h"
#include "bn.h"
#include "fb.h"

/* Adapted from an assignment for
 *    15-463 Advanced Computer Graphics
 *    Professor Paul Heckbert
 *    Carnegie Mellon University
 */

/* CBLEND is for interpolating characters.  Useful for dissolve. */
#define CBLEND(a, b, cfrac) (unsigned char)(((256-cfrac)*(int)(a)+\
					     cfrac*(int)(b))>>8)

/* DBLEND is for interpolating doubles.  Useful for warping. */
#define DBLEND(a, b, dfrac) ((1.0-dfrac)*(double)(a)+dfrac*(double)(b))

#define FIRST  0
#define MIDDLE 1
#define LAST   2

#define EPSILON 10e-8

/* Hopefully the distance between two pixels in the image will be less
   than the following value.  If not, cap it at this value in the
   computations.*/
#define MAXLEN 65536

/* gprof tells me that 73% of the time to execute was spent in those two
   pow() function calls.  So, we "memoize" a little bit, and don't call those
   pow()s for weights already calculated (they only depend on length and dist,
   which range from 0 to MAXLEN.)  Truncating them to integers changes things
   *very* little...trust me. */
double weightlookup[MAXLEN];

/* Basically, we throw *everything* that doesn't need to be recalculated
 * into this structure.  Makes things go faster later.
 * "oo" means "one over" -- floating point divides are much more expensive
 *                          than multiplies 
 */

struct ldata {
    double x1, y1, x2, y2;
    double xdelta, ydelta;
    double oolensq, oolen, len;
    double len_pb;
};

struct lineseg {
    struct ldata s[3];
};

/* 
 * cross_dissolve
 *
 * Takes the two given images "wa" and "wb", and cross-dissolves them by
 * "dissolvefrac", placing the resulting image in "morph".
 */

void
cross_dissolve(unsigned char *morph, unsigned char *wa, unsigned char *wb, int dissolvefrac, long int numpixels)
{
    register int i;

    for (i = 0; i < numpixels; i++, morph += 3, wa += 3, wb += 3) {
	morph[RED] = CBLEND(wa[RED], wb[RED], dissolvefrac);
	morph[GRN] = CBLEND(wa[GRN], wb[GRN], dissolvefrac);
	morph[BLU] = CBLEND(wa[BLU], wb[BLU], dissolvefrac);
    }
}


/*
 * warp_image
 *
 * Takes the src image (which is either "FIRST" or "LAST", depending on the
 * value in "which"), and warps it in the appropriate manner (with bilinear
 * interpolation to avoid blockiness).
 */

void
warp_image(unsigned char *dest, unsigned char *src, 
	   struct lineseg *lines, int which, 
	   long int width, long int height, 
	   long int numlines, 
	   double a, double b, double p)
{
    register long int i, j, k, width3;
    struct lineseg *tlines;

    width3 = width*3;
    for (i = 0; i < height; i++) {
	fprintf(stderr, "line %ld   \r", height-i);
	fflush(stderr);
	for (j = 0; j < width; j++, dest += 3) {
	    double dsum_x, dsum_y, weightsum, x_x, x_y, new_x, new_y,
	           frac_x, frac_y, newcolor;
	    int fin_x, fin_y, findex;
	    
	    x_x = (double)j;
	    x_y = (double)i;

	    weightsum = dsum_x = dsum_y = 0.0;
	    for (k = 0, tlines = lines; k < numlines; k++, tlines++) {
		register double x_minus_p_x, x_minus_p_y, u, v, x, y, weight,
 		                dist, tmpx, tmpy;
		register long int l2;

		/* This is a fairly straightforward implementation of the
		   algorithm in Beier and Neely's paper.
		   We work only with vector components here... note that
		   Perpindicular((a,b)) = (b, -a). */
		
		x_minus_p_x = x_x - tlines->s[MIDDLE].x1;
		x_minus_p_y = x_y - tlines->s[MIDDLE].y1;

		u = x_minus_p_x * tlines->s[MIDDLE].xdelta +
		    x_minus_p_y * tlines->s[MIDDLE].ydelta;
		u *= tlines->s[MIDDLE].oolensq;

		v = x_minus_p_x * tlines->s[MIDDLE].ydelta -
		    x_minus_p_y * tlines->s[MIDDLE].xdelta;
		v *= tlines->s[MIDDLE].oolen;

		if (u < 0) {
		    tmpx = tlines->s[MIDDLE].x1 - x_x;
		    tmpy = tlines->s[MIDDLE].y1 - x_y;
		    dist = sqrt(tmpx*tmpx+tmpy*tmpy);
		} else if (u > 1) {
		    tmpx = tlines->s[MIDDLE].x2 - x_x;
		    tmpy = tlines->s[MIDDLE].y2 - x_y;
		    dist = sqrt(tmpx*tmpx+tmpy*tmpy);
		} else
		    dist = fabs(v);

		l2 = (long int)dist;
		if (l2 > MAXLEN-1) l2 = MAXLEN-1;

		if (weightlookup[l2] > -0.5)
		    weight = weightlookup[l2];
		else
		    weight = weightlookup[l2] = pow(1.0 / (a + dist), b);

		weight *= tlines->s[MIDDLE].len_pb;

		x = tlines->s[which].x1 + u*tlines->s[which].xdelta +
		    v*tlines->s[which].oolen*tlines->s[which].ydelta - x_x;
		y = tlines->s[which].y1 + u*tlines->s[which].ydelta -
		    v*tlines->s[which].oolen*tlines->s[which].xdelta - x_y;

		dsum_x += x * weight;
		dsum_y += y * weight;
		weightsum += weight;
	    }

	    if (weightsum < EPSILON) {
		new_x = x_x;
		new_y = x_y;
	    } else {
		new_x = x_x + dsum_x / weightsum;
		new_y = x_y + dsum_y / weightsum;
	    }

	    if (new_x < 0.0) new_x = 0.0;
	    if (new_x > (double)width-1.01) new_x = (double)width-1.01;
	    if (new_y < 0.0) new_y = 0.0;
	    if (new_y > (double)height-1.01) new_y = (double)height-1.01;

	    fin_x = (int)(new_x+.5);
	    fin_y = (int)(new_y+.5);

	    /* The following should be unnecessary... */

	    if (fin_x < 0) fin_x = 0;
	    if (fin_y < 0) fin_y = 0;
	    if (fin_x > width-2) fin_x = width-2;
	    if (fin_y > height-2) fin_y = height-2;

	    /* End of unnecessary stuff. */

	    findex = 3*(fin_x + fin_y*width);

	    frac_x = new_x - (double)fin_x;
	    frac_y = new_y - (double)fin_y;

#define ICLAMP(d, a, b) (d < a ? (int)a : d > b ? (int)b : (int)d)

	    /* Bilinear interpolation.
	       It's the somewhat more expensive than it needs to be.
	       I'm going for clarity, here. */
	    
	    newcolor = ((1-frac_x)*(1-frac_y)*(double)src[findex+RED] +
			(1-frac_y)*frac_x*(double)src[findex+3+RED] +
			frac_y*frac_x*(double)src[findex+width3+3+RED] +
			frac_y*(1-frac_x)*(double)src[findex+width3+RED]);
	    dest[RED] = ICLAMP(newcolor, 0, 255);
	    
	    newcolor = ((1-frac_x)*(1-frac_y)*(double)src[findex+GRN] +
			     (1-frac_y)*frac_x*(double)src[findex+3+GRN] +
			     frac_y*frac_x*(double)src[findex+width3+3+GRN] +
			     frac_y*(1-frac_x)*(double)src[findex+width3+GRN]);
	    dest[GRN] = ICLAMP(newcolor, 0, 255);
	    
	    newcolor = ((1-frac_x)*(1-frac_y)*(double)src[findex+BLU] +
			     (1-frac_y)*frac_x*(double)src[findex+3+BLU] +
			     frac_y*frac_x*(double)src[findex+width3+3+BLU] +
			     frac_y*(1-frac_x)*(double)src[findex+width3+BLU]);
	    dest[BLU] = ICLAMP(newcolor, 0, 255);
	}
    }
}


/*
 * lines_read
 *
 * Fills up the lines array given by "lines" with the "numlines" many
 * line segment pairs from the given file.  Since the lines are in unit
 * square space, we multiply and stuff to get them to our space.
 * Furthermore, we calculate all of the useful information, including
 * interpolation, length, etc...
 */
 

int
lines_read(FILE *fp, long int numlines, 
	   struct lineseg *lines, 
	   long int width, long int height, 
	   double warpfrac, double pb)
{
    register long int i, j;
    double x1, y1, x2, y2, x3, y3, x4, y4;

    for (i = 0; i < numlines; i++, lines++) {
	if (fscanf(fp, "%lf %lf %lf %lf %lf %lf %lf %lf ",
		   &x1, &y1, &x2, &y2, &x3, &y3, &x4, &y4) < 4) {
	    fprintf(stderr, "pixmorph: lines_read: failure\n");
	    exit(1);
	}

	if ((fabs(x1-x2) < EPSILON && fabs(y1-y2) < EPSILON) ||
	    (fabs(x3-x4) < EPSILON && fabs(y3-y4) < EPSILON)) {
	    fprintf(stderr, "pixmorph: warning: zero-length line segment\n");
	    --numlines;
	    continue;
	}

	lines->s[FIRST].x1 = (double)width*x1;
	lines->s[FIRST].y1 = (double)height*y1;
	lines->s[FIRST].x2 = (double)width*x2;
	lines->s[FIRST].y2 = (double)height*y2;
	lines->s[LAST].x1 = (double)width*x3;	
	lines->s[LAST].y1 = (double)height*y3;
	lines->s[LAST].x2 = (double)width*x4;
	lines->s[LAST].y2 = (double)height*y4;

	/* Now, the other useful information. */

	lines->s[MIDDLE].x1 = DBLEND(lines->s[FIRST].x1,
				     lines->s[LAST].x1, warpfrac);
	lines->s[MIDDLE].y1 = DBLEND(lines->s[FIRST].y1,
				     lines->s[LAST].y1, warpfrac);
	lines->s[MIDDLE].x2 = DBLEND(lines->s[FIRST].x2,
				     lines->s[LAST].x2, warpfrac);
	lines->s[MIDDLE].y2 = DBLEND(lines->s[FIRST].y2,
				     lines->s[LAST].y2, warpfrac);

	for (j = 0; j < 3; j++) {
	    lines->s[j].xdelta = lines->s[j].x2 - lines->s[j].x1;
	    lines->s[j].ydelta = lines->s[j].y2 - lines->s[j].y1;
	    lines->s[j].oolensq = 1.0/(lines->s[j].xdelta*lines->s[j].xdelta +
				       lines->s[j].ydelta*lines->s[j].ydelta);
	    lines->s[j].oolen = sqrt(lines->s[j].oolensq);
	    lines->s[j].len = 1.0/lines->s[j].oolen;
	    lines->s[j].len_pb = pow(lines->s[j].len, pb);
	}
    }

    return numlines;
}

/*
 * lines_headerinfo
 *
 * Gets the useful header information from the given lines file.
 */

void
lines_headerinfo(FILE *fp, double *ap, double *bp, double *pp, long int *np)
{
    if (fscanf(fp, "%lf %lf %lf %ld ", ap, bp, pp, np) < 4) {
	fprintf(stderr, "pixmorph: cannot read header info in lines file\n");
	exit(1);
    }
}

int
get_args(int argc, char **argv, char **picAnamep, char **picBnamep, char **linesfilenamep, 
	 double *warpfracp, int *dissolvefracp, long int *autosizep, 
	 long int *widthp, long int *heightp)
{
    register long int c;

    *autosizep = 1;
    *widthp = *heightp = 0;

    while ((c = getopt(argc, argv, "w:n:")) != EOF) {
	switch (c) {
	case 'w':
	    *widthp = atol(optarg);
	    *autosizep = 0;
	    break;
	case 'n':
	    *heightp = atol(optarg);
	    *autosizep = 0;
	    break;
	default:
	    return 0;
	}
    }

    if (argc != optind+5)
	return 0;

    *picAnamep = argv[optind];
    *picBnamep = argv[optind+1];
    *linesfilenamep = argv[optind+2];
    *warpfracp = atof(argv[optind+3]);
    *dissolvefracp = (int)(255.0*atof(argv[optind+4])+0.5);

    return 1;
}

int
pix_readpixels(FILE *fp, long int numpix, unsigned char *pixarray)
{
    return fread(pixarray, 3, (size_t)numpix, fp);
}

int
pix_writepixels(long int numpix, unsigned char *pixarray)
{
    return fwrite(pixarray, 3, (size_t)numpix, stdout);
}
    
int
main(int argc, char **argv)
{
    char *picAname, *picBname, *linesfilename;
    FILE *picA, *picB, *linesfile;
    long int pa_width = 0, pa_height = 0;
    int dissolvefrac;
    double warpfrac;
    unsigned char *pa, *pb, *wa, *wb, *morph;
    double a, b, p;
    long int numlines;
    struct lineseg *lines;
    register long int i;
    int autosize;
#if 0
    npsw = bu_avail_cpus();
    if (npsw > DEFAULT_PSW) npsw = DEFAULT_PSW;
#endif

    autosize = 1;
    pa_width = pa_height = 0;
    if (get_args(argc, argv, &picAname, &picBname, &linesfilename,
          &warpfrac, &dissolvefrac, &autosize, &pa_width, &pa_height) == 0
	|| isatty(fileno(stdout))) {
	fprintf(stderr,
		"usage: pixmorph [-w width] [-n height] picA.pix picB.pix linesfile warpfrac dissolvefrac > out.pix\n");
	return 1;
    }

    picA = fopen(picAname, "r");
    if (picA == NULL) {
	fprintf(stderr, "pixmorph: cannot open %s\n", picAname);
	return 1;
    }
    picB = fopen(picBname, "r");
    if (picB == NULL) {
	fprintf(stderr, "pixmorph: cannot open %s\n", picBname);
	return 1;
    }
    linesfile = fopen(linesfilename, "r");
    if (linesfile == NULL) {
	fprintf(stderr, "pixmorph: cannot open %s\n", linesfilename);
	return 1;
    }

    if (warpfrac < 0.0 || warpfrac > 1.0) {
	fprintf(stderr, "pixmorph: warpfrac must be between 0 and 1\n");
	return 1;
    }

    if (dissolvefrac < 0 || dissolvefrac > 255) {
	fprintf(stderr, "pixmorph: dissolvefrac must be between 0 and 1\n");
	return 1;
    }

    if (autosize) {
	if (bn_common_file_size(&pa_width, &pa_height, argv[1], 3) == 0) {
	    fprintf(stderr, "pixmorph: unable to autosize\n");
	    return 1;
	}
    } else {
	struct stat sb;

	if (stat(picAname, &sb) < 0) {
	    perror("pixmorph: unable to stat file:");
	    return 1;
	}
	
	if (pa_width > 0) {
	    pa_height = sb.st_size/(3*pa_width);
	    fprintf(stderr, "width = %ld, size = %ld, so height = %ld\n",
		   pa_width, (long)sb.st_size, pa_height);
	} else if (pa_height > 0) pa_width = sb.st_size/(3*pa_height);

	if (pa_width <= 0 || pa_height <= 0) {
	    fprintf(stderr, "pixmorph: Bogus image dimensions: %ld %ld\n",
		    pa_width, pa_height);
	    return 1;
	}
    }

    /* Allocate memory for our bag o' pixels. */
    
    pa = (unsigned char *)malloc(pa_width*pa_height*3);
    pb = (unsigned char *)malloc(pa_width*pa_height*3);
    wa = (unsigned char *)malloc(pa_width*pa_height*3);
    wb = (unsigned char *)malloc(pa_width*pa_height*3);
    morph = (unsigned char *)malloc(pa_width*pa_height*3);
    
    if (pa == NULL || pb == NULL || wa == NULL ||  wb == NULL ||
	morph == NULL) {
	fprintf(stderr, "pixmorph: memory allocation failure\n");
	return 1;
    }

    /* The following is our memoizing table for weight calculation. */
       
    for (i = 0; i < MAXLEN; i++)
	weightlookup[i] = -1.0;

    fprintf(stderr, "pixmorph: Reading images and lines file.\n");
    
    if (pix_readpixels(picA, pa_width*pa_height, pa) < pa_width*pa_height) {
	fprintf(stderr, "Error reading %ld pixels from %s\n",
		pa_width*pa_height, picAname);
	return 1;
    }
    if (pix_readpixels(picB, pa_width*pa_height, pb) < pa_width*pa_height) {
	fprintf(stderr, "Error reading %ld pixels from %s\n",
		pa_width*pa_height,  picBname);
	return 1;
    }
    fclose(picA);
    fclose(picB);

    /* Process the lines file. */
    
    lines_headerinfo(linesfile, &a, &b, &p, &numlines);
    lines = (struct lineseg *)malloc(numlines * sizeof(struct lineseg));
    numlines = lines_read(linesfile, numlines, lines,
			  pa_width, pa_height, warpfrac, p*b);
    fprintf(stderr, "pixmorph: %ld line segments read\n", numlines);

    /* Warp the images */
    
    fprintf(stderr,
	    "pixmorph: Warping first image into first intermediate image.\n");
    warp_image(wa, pa, lines, FIRST, pa_width, pa_height, numlines, a, b, p);
    fprintf(stderr,
           "pixmorph: Warping second image into second intermediate image.\n");
    warp_image(wb, pb, lines, LAST, pa_width, pa_height, numlines, a, b, p);

    /* Do the dissolve */
    
    fprintf(stderr,
	    "pixmorph: Performing cross-dissolve between first and second\n");
    fprintf(stderr, "pixmorph: intermediate images.\n");
    cross_dissolve(morph, wa, wb, dissolvefrac, pa_width*pa_height);

    /* All done.  Write everything out to a file and quit. */

    pix_writepixels(pa_width*pa_height, morph);

    fprintf(stderr, "pixmorph: Morph complete.\n");

    /* System takes care of memory deallocation. */

    return 0;
}

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
