/*                          M A I N . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2007 United States Government as represented by
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
/** @file main.c
 *
 * given a bw file, generate a ht file.
 *
 * Usage:
 *	halftone
 *		-s	square size
 *		-n	number of lines
 *		-w	width
 *		-h	same as -s 1024
 *		-a	Automatic bw file sizing.
 *		-B	Beta for sharpining
 *		-I	number of intensity levels
 *		-M	method
 *			0 Floyd-Steinburg
 *			1 45 degree classical halftone screen
 *			2 Threshold
 *			3 0 degree dispersed halftone screen.
 *		-R	Add some noise.
 *		-S	Surpent flag.
 *		-T	tonescale points
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
 *	tone_floyd()	- Floyd-Steinburg halftone.
 *	tone_folly()	- 0 degree halftone screen (from Folly and Van Dam)
 *	tone_classic()	- 45 degree classical halftone screen.
 *	tonescale()	- Generates a tone scale map default is 0,0 to 255,255
 *	cubic_init()	- Generates "cubics" for tonescale for a set of points.
 *
 * Method:
 *	Fairly simple.  Most of the algorthems are inspired by
 *		Digital Halftoning by Robert Ulichney
 *
 * Author:
 *	Christopher T. Johnson	- 90/03/21
 *
 */
#ifndef lint
static const char RCSid[] = "$Header$";
#endif

#include "common.h"

#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <math.h>
#ifdef HAVE_UNISTD_H
#  include <unistd.h>
#endif

#include "machine.h"
#include "vmath.h"
#include "raytrace.h"
#include "msr.h"


long int width=512;		/* width of pixture */
long int height=512;		/* height of pixture */
double Beta=0.0;	/* Beta for sharpening */

#define	M_FLOYD	0
#define	M_CLASSIC 1
#define	M_THRESH 2
#define	M_FOLLY 3
int Method=M_FLOYD;	/* Method of halftoning */

int Surpent=0;		/* use serpentine scan lines */
int Levels=1;		/* Number of levels-1 */
int Debug=0;
struct msr_unif *RandomFlag=0;	/* Use random numbers ? */

void cubic_init(int n, int *x, int *y);
void tonescale(unsigned char *map, float Slope, float B, int (*eqptr)() );
int sharpen(unsigned char *buf, int size, int num, FILE *file, unsigned char *Map);
int tone_floyd(int pix, int x, int y, int nx, int ny, int new);
int tone_folly(int pix, int x, int y, int nx, int ny, int new);
int tone_simple(int pix, int x, int y, int nx, int ny, int new);
int tone_classic(int pix, int x, int y, int nx, int ny, int new);

static char usage[] = "\
Usage: halftone [ -h -R -S -a] [-D Debug Level]\n\
	[-s squarefilesize] [-w file_width] [-n file_height]\n\
	[-B contrast] [-I intensity_levels] [-T x y ... tone_curve]\n\
	[-M Method] [file.bw]\n\
	Floyd-Steinberg=0	45 Degree Classic Screen=1\n\
	Thresholding=2		0 Degree Dispersed Screen=3\n";

/*	setup	process parameters and setup working enviroment
 *
 * Entry:
 *	argc	- number of arguments.
 *	argv	- the arguments.
 *
 * Exit:
 *	parameters have been set.
 *	if there is a fatal error exit(!0)
 *
 * Uses:
 *	width	- width of pixture
 *	height	- height of pixture
 *	Beta	- sharpening value.
 *	surpent	- to surpenten rasters?
 *	Levels	- number of intensity levels.
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
	int i,j;
	int *Xlist, *Ylist;
	int	autosize = 0;

	while ((c = getopt(argc, argv, "D:hsa:n:w:B:M:RSI:T:")) != EOF) {
		switch(c) {
		case 's':
			width = height = atol(optarg);
		break;
		case 'n':
			height = atol(optarg);
		break;
		case 'w':
			width = atol(optarg);
		break;
		case 'h':
			width = height = 1024;
		break;
		case 'a':
			autosize = 1;
		break;
		case 'B':
			Beta = atof(optarg);
		break;
		case 'M':
			Method = atoi(optarg);
		break;
		case 'R':
			RandomFlag = msr_unif_init(1,0);
		break;
		case 'S':
			Surpent = 1;
		break;
		case 'I':
			Levels = atoi(optarg)-1;
			if (Levels < 1) Levels = 1;
		break;
/*
 * Tone scale processing is a little strange.  The -T option is followed
 * be a list of points.  The points are collected and one point is added
 * at 1024,1024 to let tonescale be stupid.  Cubic_init takes the list
 * of points and generates "cubics" for tonescale to use in generating
 * curve to use for the tone map.  If tonescale is called with no cubics
 * defined tonescale will generate a straight-line (generaly from 0,0 to
 * 255,255).
 */
		case 'T':
			--optind;
			for(i=optind; i < argc && (isdigit(*argv[i]) ||
			    (*argv[i] == '-' && isdigit(*(argv[i]+1)))) ; i++);
			if ((c=i-optind) % 2) {
				fprintf(stderr,"Missing Y coordent for tone map.\n");
				exit(1);
			}
			Xlist = (int *) bu_malloc((c+2)*sizeof(int), "Xlist");
			Ylist = (int *) bu_malloc((c+2)*sizeof(int), "Ylist");

			for (j=0;optind < i; ) {
				Xlist[j] = atoi(argv[optind++]);
				Ylist[j] = atoi(argv[optind++]);
				j++;
			}
			Xlist[j] = 1024;
			Ylist[j] = 1024;
			if (Debug>6) fprintf(stderr,"Number points=%d\n",j+1);
			(void) cubic_init(j+1,Xlist,Ylist);
			bu_free(Xlist, "Xlist");
			bu_free(Ylist, "Ylist");
		break;
/*
 * Debug is not well used at this point a value of 9 will get all
 * debug statements.  Debug is a level indicator NOT a bit flag.
 */
		case 'D':
			Debug = atoi(optarg);
		break;
		case '?':
			fprintf(stderr,usage);
			exit(1);
		break;
		}
	}
/*
 *	if there are no extra arguments and stdin is a tty then
 *	the user has given us no input file.  Spit a usage message
 * 	at them and exit.
 */
	if (optind >= argc) {
		if ( isatty(fileno(stdin)) ) {
			(void) fprintf(stderr,usage);
			exit(1);
		}
		if (autosize) {
			(void) fprintf(stderr, usage);
			(void) fprintf(stderr, "Automatic sizing can not be used with pipes.\n");
			exit(1);
		}
	} else {
		if (freopen(argv[optind],"r",stdin) == NULL ) {
			(void) fprintf( stderr,
			    "halftone: cannot open \"%s\" for reading.\n",
			    argv[optind]);
			exit(1);
		}
		if (autosize) {
			if ( !fb_common_file_size((unsigned long int *)&width, (unsigned long int *)&height, argv[optind], 1)) {
				(void) fprintf(stderr,"halftone: unable to autosize.\n");
			}
		}
	}

	if ( argc > ++optind) {
		(void) fprintf(stderr,"halftone: excess argument(s) ignored.\n");
	}
}

int
main(int argc, char **argv)
{
	int pixel,x,y,i;
	unsigned char *Line, *Out;
	int NewFlag = 1;
	int Scale;
	unsigned char Map[256];
/*
 *	parameter processing.
 */
	setup(argc,argv);
/*
 *	Get a tone map.  Map is the result.  1.0 is slope, 0.0 is
 *	the Y intercept (y=mx+b). 0 is the address of a function to
 *	do a x to y mapping, 0 means use the default function.
 */
	(void) tonescale(Map,1.0,0.0,0);

/*
 * Currently the halftone file is scaled from 0 to 255 on output to
 * ease display via bw-fb.  In the future there might be flag to
 * set Scale to 1 to get a unscaled output.
 */
	Scale = 255/Levels;

	if (Debug) {
		fprintf(stderr,"Debug = %d, Scale = %d\n",Debug, Scale);
	}

	if (Debug>2) {
		for(i=0;i<256;i++) fprintf(stderr,"%d ",Map[i]);
		fprintf(stderr,"\n");
	}

	Line = (unsigned char *) bu_malloc(width, "Line");
	Out = (unsigned char *) bu_malloc(width, "Out");
/*
 * should be a test here to make sure we got the memory requested.
 */

/*
 *	Currently only the Floyd-Steinburg method uses the surpent flag
 *	so we make things easy with in the 'y' loop by reseting surpent
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
		(void) sharpen(Line,1,width,stdin,Map);
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
		fwrite(Out,1,width,stdout);
	}
	bu_free(Line, "Line");
	bu_free(Out, "Out");
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
