/*                          M A I N . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2014 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file halftone/main.c
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

#include "vmath.h"
#include "raytrace.h"
#include "fb.h"


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

void cubic_init(int n, int *x, int *y);
void tonescale(unsigned char *map, float Slope, float B, int (*eqptr)() );
int sharpen(unsigned char *buf, int size, int num, FILE *file, unsigned char *Map);
int tone_floyd(int pix, int x, int y, int nx, int ny, int newrow);
int tone_folly(int pix, int x, int y, int nx, int ny, int newrow);
int tone_simple(int pix, int x, int y, int nx, int ny, int newrow);
int tone_classic(int pix, int x, int y, int nx, int ny, int newrow);

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

/*	setup:	process parameters and setup working environment
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
		if (Levels < 1) Levels = 1;
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
	char *ifname = bu_realpath(argv[bu_optind], NULL);
	if (freopen(ifname, "r", stdin) == NULL ) {
	    bu_free(ifname,"ifname alloc from bu_realpath");
	    bu_exit(1, "halftone: cannot open \"%s\" for reading.\n", argv[bu_optind]);
	}
	bu_free(ifname,"ifname alloc from bu_realpath");
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
