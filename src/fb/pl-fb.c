/*                         P L - F B . C
 * BRL-CAD
 *
 * Copyright (c) 1986-2011 United States Government as represented by
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
 *
 */
/** @file pl-fb.c
 *
 * Program to take 3-D UNIX plot data and output on a framebuffer.
 *
 * Function:
 *
 * Reads device-independent plot data from specified input file; for
 * each frame, builds an image file containing raster data then sends
 * the frame output to the output device.
 *
 * Edge-limiting is done here; use "rot" if clipping is desired.
 *
 * Method:
 *
 * Inputs vector data and builds a rasterization descriptor for each
 * visible stroke.  (Strokes are limited to frame boundaries.)  X goes
 * down the page, Y goes from left to right.  To obtain a different
 * orientation, pre-process data with the "rot" filter.  (Quadrant 1
 * graphics devices)
 *
 * The frame image file of SCANS scans is considered artificially
 * divided into BANDS bands, each containing lines_per_band scans.
 * Each band has a linked list of descriptors for not-yet-rasterized
 * strokes that start in the band.
 *
 * Space for descriptors is obtained via "malloc".  When no more space
 * is available, the image file is updated as follows, then "malloc"
 * is tried again ("must" work the second time):
 *
 * Each band in increasing X order becomes "active"; if no descriptors
 * exist for the band it is skipped, otherwise its existing raster
 * data is re-read from the image file into a buffer and each
 * descriptor is processed to rasterize its stroke.  If the stroke
 * terminates in the band its descriptor is freed, otherwise the
 * descriptor is linked into the following band's list.  When the
 * descriptor list for the active band becomes empty (must happen),
 * the band's raster data is flushed back to the image file and the
 * next band becomes active.  This process continues until all vectors
 * have been input and rasterized.
 *
 * Acknowledgment:
 *
 * Based rather heavily on Doug Gwyn's Versatec PLOT rasterizer VPL.C
 * which was based on Mike Muuss's Versatec TIGpack interpreter.
 *
 * Note:
 *
 * UNIX-Plot files are defined to be machine-independent, with
 * "little-endian" (eg, VAX) byte-ordering.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <ctype.h>
#include "bio.h"

#include "bu.h"
#include "fb.h"
#include "plot3.h"

#define COMMA ','

/*
  Raster device model and image terminology as used herein:

  Frames are delimited in plot data by "erase" codes; each frame looks
  like:


  top of frame
  -------------------------------------------------------
  |			.				|
  |scan line ->.........................................|
  ^	|		.				|
  |	|		^				|
  |			|				|
  |Y axis						|
  |plot							|
  ^	|^						|
  |	||						|
  ||	X axis						|
  |-----> plot						|
  ---------------------------------------------------------
  bottom of frame


  Each plot-mode scan line consists of exactly BYTES bytes of plot data.

  Each scan line is composed of bytes of color data, for the Red,
  Green, and Blue for each pixel, times the number of pixels desired
  (512 in LORES, 1024 in HIRES)

*/

/* Device Parameters */

/* the following parameter should be tweaked for fine-tuning */
#define X_CHAR_SIZE (8)		/* pixels per char horizontal */
#define Y_CHAR_SIZE (10)	/* pixels per char vertical */

#define CLEAR 0 /* value for no intensity */


/* Program constants computed from device parameters:	*/
#define BANDS (Nscanlines / lines_per_band)	/* # of "bands" */
#define BANDSLOP (BANDS+2)			/* # bands in array */
#define BYTES (Npixels * sizeof(RGBpixel))	/* max data bytes per scan */
#define XMAX (Npixels - 1)
#define YMAX (Nscanlines - 1)

/* helper macros */
#define GET_STROKE(vp) { \
			while (((vp)=freep) == STROKE_NULL) \
				get_strokes(); \
			freep = (vp)->freep; \
			CK_STROKE(vp); \
			(vp)->freep = STROKE_NULL; }

#define FREE_STROKE(vp) { \
			CK_STROKE(vp); \
			(vp)->freep = freep; \
			freep = (vp); }


/* Data structure definitions:	*/

#ifdef pdp11
typedef char tiny;		/* for very small numbers */
#else
typedef short tiny;		/* for very small numbers */
#endif

#define false 0
#define true 1

RGBpixel cur_color = { 255, 255, 255 };

typedef struct {
    short x;
    short y;
} coords; 			/* Cartesian coordinates */

typedef struct descr {
    uint32_t magic;
    struct descr *next;		/* next in list, or NULL */
    coords pixel;		/* starting scan, nib */
    tiny xsign;			/* 0 or +1 */
    tiny ysign;			/* -1, 0, or +1 */
    int ymajor; 		/* true iff Y is major dir. */
#undef major
#undef minor
    short major;		/* major dir delta (nonneg) */
    short minor;		/* minor dir delta (nonneg) */
    short e;			/* DDA error accumulator */
    short de;			/* increment for `e' */
    RGBpixel col;		/* COLOR of this vector */
    struct descr *freep;	/* next in free list, or NULL */
} stroke; 			/* rasterization descriptor */
#define STROKE_MAGIC 0x12997601	/* Magic number */

#define CK_STROKE(_sp) { \
	if ((_sp)->magic != STROKE_MAGIC) {  \
		fprintf(stderr, "Bad stroke struct, ptr=%p, magic was x%lx, s/b=x%lx, at file %s, line %d\n",  \
			(void *)(_sp), (unsigned long)((_sp)->magic), (unsigned long)STROKE_MAGIC,  \
			__FILE__, __LINE__);  \
		abort();  \
	} }

/* Global data allocations:	*/

/* Space is used strictly to compute delta and deltao2
 * and to compute integer screen coordinates from them.
 * We thus make these variables only floating point.
 */
static struct {
    double left;		/* window edges */
    double bottom;
    double right;
    double top;
} space;
static double delta;		/* larger window dimension */
static double deltao2;		/* delta / 2 */

struct relvect {
    short x, y;			/* x, y values (255, 255 is end) */
};


#define END -1, -1		/* end of stroke description */
#define NIL 0, 0

/*
 * These character sets are taken from the Motorola MC6575 Pattern Generator,
 * page 5-119 of 'The Complete Motorola Microcomputer Data Library'
 */
static struct vectorchar {
    char ascii;		/* ASCII character emulated */
    struct relvect r[10];	/* maximum # of vectors 1 char */
} charset[] = {
    /*ASCII <1> <2> <3> <4> <5> <6> <7> <8> <9> <10> */
    {'0', {{5, 0},   {1, 0},   {0, 1},   {0, 7},   {6, 1},   {6, 7},   {5, 8},   {1, 8},   {END},   {NIL}}},
    {'1', {{1, 2},   {3, 0},   {3, 8},   {5, 8},   {1, 8},   {END},   {NIL},   {NIL},   {NIL},   {NIL}}},
    {'2', {{0, 1},   {1, 0},   {5, 0},   {6, 1},   {6, 2},   {4, 4},   {2, 4},   {0, 6},   {0, 8},   {6, 8}}},
    {'3', {{1, 0},   {5, 0},   {6, 1},   {6, 4},   {2, 4},   {6, 4},   {6, 7},   {5, 8},   {1, 8},   {END}}},
    {'4', {{5, 8},   {5, 0},   {0, 5},   {0, 6},   {6, 6},   {END},   {NIL},   {NIL},   {NIL},   {NIL}}},
    {'5', {{6, 0},   {0, 0},   {0, 3},   {4, 3},   {6, 5},   {6, 6},   {4, 8},   {1, 8},   {0, 7},   {END}}},
    {'6', {{5, 0},   {2, 0},   {0, 2},   {0, 7},   {1, 8},   {5, 8},   {6, 7},   {6, 5},   {5, 4},   {1, 4}}},
    {'7', {{0, 1},   {0, 0},   {6, 0},   {6, 1},   {2, 5},   {2, 8},   {END},   {NIL},   {NIL},   {NIL}}},
    {'8', {{1, 0},   {5, 0},   {6, 1},   {6, 7},   {5, 8},   {1, 8},   {0, 7},   {0, 1},   {0, 4},   {6, 4}}},
    {'9', {{1, 8},   {4, 8},   {6, 6},   {6, 1},   {5, 0},   {1, 0},   {0, 1},   {0, 3},   {1, 4},   {6, 4}}},
    {'A', {{0, 8},   {0, 2},   {2, 0},   {4, 0},   {6, 2},   {6, 8},   {6, 5},   {0, 5},   {END},   {NIL}}},
    {'B', {{6, 5},   {6, 7},   {5, 8},   {0, 8},   {0, 0},   {5, 0},   {6, 1},   {6, 3},   {5, 4},   {0, 4}}},
    {'C', {{6, 1},   {5, 0},   {2, 0},   {0, 2},   {0, 6},   {2, 8},   {5, 8},   {6, 7},   {END},   {NIL}}},
    {'D', {{0, 0},   {4, 0},   {6, 2},   {6, 6},   {4, 8},   {0, 8},   {0, 0},   {END},   {NIL},   {NIL}}},
    {'E', {{6, 0},   {0, 0},   {0, 4},   {3, 4},   {0, 4},   {0, 8},   {6, 8},   {END},   {NIL},   {NIL}}},
    {'F', {{6, 0},   {0, 0},   {0, 4},   {3, 4},   {0, 4},   {0, 8},   {END},   {NIL},   {NIL},   {NIL}}},
    {'G', {{6, 1},   {5, 0},   {2, 0},   {0, 2},   {0, 6},   {2, 8},   {5, 8},   {6, 7},   {6, 5},   {3, 5}}},
    {'H', {{0, 0},   {0, 8},   {0, 4},   {6, 4},   {6, 0},   {6, 8},   {END},   {NIL},   {NIL},   {NIL}}},
    {'I', {{1, 0},   {5, 0},   {3, 0},   {3, 8},   {1, 8},   {5, 8},   {END},   {NIL},   {NIL},   {NIL}}},
    {'J', {{2, 0},   {6, 0},   {4, 0},   {4, 7},   {3, 8},   {1, 8},   {0, 7},   {END},   {NIL},   {NIL}}},
    {'K', {{0, 0},   {0, 8},   {0, 6},   {6, 0},   {2, 4},   {6, 8},   {END},   {NIL},   {NIL},   {NIL}}},
    {'L', {{0, 0},   {0, 8},   {6, 8},   {END},   {NIL},   {NIL},   {NIL},   {NIL},   {NIL},   {NIL}}},
    {'M', {{0, 8},   {0, 0},   {3, 3},   {3, 4},   {3, 3},   {6, 0},   {6, 8},   {END},   {NIL},   {NIL}}},
    {'N', {{0, 8},   {0, 0},   {6, 6},   {6, 8},   {6, 0},   {END},   {NIL},   {NIL},   {NIL},   {NIL}}},
    {'O', {{0, 6},   {0, 2},   {2, 0},   {4, 0},   {6, 2},   {6, 6},   {4, 8},   {2, 8},   {0, 6},   {END}}},
    {'P', {{0, 8},   {0, 0},   {5, 0},   {6, 1},   {6, 3},   {5, 4},   {0, 4},   {END},   {NIL},   {NIL}}},
    {'Q', {{6, 6},   {6, 2},   {4, 0},   {2, 0},   {0, 2},   {0, 6},   {2, 8},   {4, 8},   {4, 6},   {6, 8}}},
    {'R', {{0, 8},   {0, 0},   {5, 0},   {6, 1},   {6, 3},   {5, 4},   {0, 4},   {2, 4},   {6, 8},   {END}}},
    {'S', {{6, 1},   {5, 0},   {1, 0},   {0, 1},   {0, 4},   {5, 4},   {6, 5},   {6, 7},   {5, 8},   {0, 8}}},
    {'T', {{0, 0},   {6, 0},   {3, 0},   {3, 8},   {END},   {NIL},   {NIL},   {NIL},   {NIL},   {NIL}}},
    {'U', {{0, 0},   {0, 7},   {1, 8},   {5, 8},   {6, 7},   {6, 0},   {END},   {NIL},   {NIL},   {NIL}}},
    {'V', {{0, 0},   {0, 2},   {3, 8},   {6, 2},   {6, 0},   {END},   {NIL},   {NIL},   {NIL},   {NIL}}},
    {'W', {{0, 0},   {0, 8},   {3, 5},   {3, 4},   {3, 5},   {6, 8},   {6, 0},   {END},   {NIL},   {NIL}}},
    {'X', {{0, 0},   {6, 8},   {3, 4},   {0, 8},   {6, 0},   {END},   {NIL},   {NIL},   {NIL},   {NIL}}},
    {'Y', {{0, 0},   {0, 1},   {3, 4},   {3, 8},   {3, 4},   {6, 1},   {6, 0},   {END},   {NIL},   {NIL}}},
    {'Z', {{0, 0},   {6, 0},   {6, 1},   {0, 7},   {0, 8},   {6, 8},   {END},   {NIL},   {NIL},   {NIL}}},
    {'+', {{0, 4},   {6, 4},   {3, 4},   {3, 1},   {3, 7},   {END},   {NIL},   {NIL},   {NIL},   {NIL}}},
    {'-', {{0, 4},   {6, 4},   {END},   {NIL},   {NIL},   {NIL},   {NIL},   {NIL},   {NIL},   {NIL}}},
    {'/', {{0, 7},   {6, 1},   {END},   {NIL},   {NIL},   {NIL},   {NIL},   {NIL},   {NIL},   {NIL}}},
    {'(', {{4, 0},   {2, 2},   {2, 6},   {4, 8},   {END},   {NIL},   {NIL},   {NIL},   {NIL},   {NIL}}},
    {')', {{2, 0},   {4, 2},   {4, 6},   {2, 8},   {END},   {NIL},   {NIL},   {NIL},   {NIL},   {NIL}}},
    {'<', {{4, 0},   {0, 4},   {4, 8},   {END},   {NIL},   {NIL},   {NIL},   {NIL},   {NIL},   {NIL}}},
    {'>', {{2, 0},   {6, 4},   {2, 8},   {END},   {NIL},   {NIL},   {NIL},   {NIL},   {NIL},   {NIL}}},

    /* Some needed chars, hastily drawn -MJM */
    {'.', {{4, 7},   {3, 7},   {3, 6},   {4, 6},   {4, 7},   {END},   {NIL},   {NIL},   {NIL},   {NIL}}},
    {COMMA, {{4, 6},   {3, 6},   {3, 5},   {4, 5},   {4, 8},   {END},   {NIL},   {NIL},   {NIL},   {NIL}}},

    {'\0', {{NIL}, {NIL}, {NIL}, {NIL}, {NIL}, {NIL}, {NIL}, {NIL}, {NIL}, {NIL}}}
};


static int Nscanlines = 512;
static int Npixels = 512;
static char *framebuffer = NULL;
static char *filename = NULL;

struct band {
    stroke *first;
    stroke *last;
};
static struct band *band;		/* array of descriptor lists */
static struct band *bandEnd;

static unsigned char *buffer;		/* ptr to active band buffer */
static long buffersize;			/* active band buffer bytes */
static short ystart = 0;		/* active band starting scan */
static int debug  = 0;
static int over = 0;			/* !0 to overlay on existing image */
static int immediate = 0;		/* !0 to plot immediately */
static int single_banded = 0;		/* !0 if one fullscreen band */
static short lines_per_band = 16;	/* scan lines per band */
static short line_thickness = 1;

/* signals to be caught */
static int sigs[] = {
#ifdef SIGHUP
    SIGHUP,
#endif
#ifdef SIGINT
    SIGINT,
#endif
#ifdef SIGQUIT
    SIGQUIT,
#endif
#ifdef SIGTERM
    SIGTERM,
#endif
    0
};


static FILE *pfin;		/* input file FIO block ptr */
FBIO *fbp;			/* Current framebuffer */


/*
 * Stroke descriptor management.
 * We malloc these in large blocks and keep our own free list.
 * Last I looked it took 32 bytes per stroke, so every 32 strokes
 * is 1KB, or 32K strokes / MByte.
 */
#define STROKE_NULL ((stroke *)0)

static struct descr *freep = STROKE_NULL;	/* head of free stroke list */


/*
 * Dequeue - remove descriptor from band list (do not free space)
 *
 * Returns addr of descriptor, or NULL if none left.
 */
static stroke *
Dequeue(struct band *bp, stroke **hp)
    /* *hp -> first descr in list */
{
    stroke *vp;		/* -> descriptor */

    if ((vp = *hp) != NULL)
	*hp = vp->next; 	/* -> next element in list */

    if (vp == NULL)
	bp->last = NULL;

    return vp;			/* may be NULL */
}


/*
 * Requeue - enqueue descriptor at END of band list
 */
static void
Requeue(struct band *bp, stroke *vp)
{
    CK_STROKE(vp);
    vp->next = NULL;
    if (bp->last)
	bp->last->next = vp;
    else
	bp->first = vp;

    bp->last = vp;
}


/*
 * Raster - rasterize stroke.
 *
 * If immediate mode, draw the individual pixel on the frame buffer.
 * If banded buffered mode, draw the portion in this band.  If it
 * overflows into next band, requeue; else free the descriptor.
 *
 * Method:
 * Modified Bresenham algorithm.  Guaranteed to mark a dot for
 * a zero-length stroke.  Please do not try to "improve" this code
 * as it is extremely hard to get all aspects just right.
 */
static void
Raster(stroke *vp, struct band *np)
    /* -> rasterization descr */
    /* *np -> next band 1st descr */
{
    short dy;		/* raster within active band */

    CK_STROKE(vp);

    /*
     * Draw the portion within this band.
     */
    for (dy = vp->pixel.y - ystart; dy < lines_per_band;) {

	/* set the appropriate pixel in the buffer */
	if (immediate) {
	    fb_write(fbp, vp->pixel.x, dy, vp->col, 1);
	} else {
	    unsigned char *pp;

	    pp = (unsigned char *)&buffer[((dy*Npixels) + vp->pixel.x)*sizeof(RGBpixel)];
	    COPYRGB(pp, vp->col);
	}

	if (vp->major-- == 0) {
	    /* done! */
	    FREE_STROKE(vp);	/* return to "malloc" */
	    return;
	}

	if (vp->e < 0) {
	    /* advance major & minor */
	    dy += vp->ysign;
	    vp->pixel.x += vp->xsign;
	    vp->e += vp->de;
	} else {
	    /* advance major only */
	    if (vp->ymajor)	/* Y is major dir */
		++dy;
	    else			/* X is major dir */
		vp->pixel.x += vp->xsign;
	    vp->e -= vp->minor;
	}
    }

    /* overflow into next band; re-queue */
    vp->pixel.y = ystart + lines_per_band;
    Requeue(np, vp);       /* DDA parameters already set */
}


/*
 * OutBuild - rasterize all strokes into raster frame image
 */
static int
OutBuild(void)				/* returns true if successful */
{
    struct band *hp;	/* *hp -> head of descr list */
    struct band *np;	/* `hp' for next band */
    stroke *vp;		/* -> rasterization descr */

    if (single_banded) {
	if (debug) fprintf(stderr, "OutBuild:  band y=%d\n", ystart);
	if (fb_write(fbp, 0, ystart, buffer, buffersize/sizeof(RGBpixel)) <= 0)
	    return false;	/* can't write image file */
	if (over) {
	    /* Read back the composite image */
	    if (fb_read(fbp, 0, ystart, buffer, buffersize/sizeof(RGBpixel)) <= 0)
		fprintf(stderr, "pl-fb:  band read error\n");
	}
	return true;
    }

    for (hp = &band[0]; hp < bandEnd; ++hp)
	if (hp->first != NULL)
	    break;

    if (hp == bandEnd)
	return true;		/* nothing to do */

    for (hp = &band[0], np = &band[1], ystart = 0;
	 hp < bandEnd;
	 hp = np++, ystart += lines_per_band
	) {
	if (debug) fprintf(stderr, "OutBuild:  band y=%d\n", ystart);
	if (over) {
	    /* Read in current band */
	    if (fb_read(fbp, 0, ystart, buffer, buffersize/sizeof(RGBpixel)) <= 0)
		fprintf(stderr, "pl-fb:  band read error\n");
	} else {
	    /* clear pixels in the band */
	    memset((char *)buffer, 0, buffersize);
	}

	while ((vp = Dequeue(hp, &hp->first)) != NULL)
	    Raster(vp, np);      /* rasterize stroke */

	/* Raster() either re-queued the descriptor onto the
	   next band list or else it freed the descriptor */

	if (debug) fprintf(stderr, "OutBuild:  fbwrite y=%d\n", ystart);
	if (fb_write(fbp, 0, ystart, buffer, buffersize/sizeof(RGBpixel)) <= 0)
	    return false;	/* can't write image file */
    }

    return true;			/* success */
}


/* allocate new strokes to the free list */
static void
get_strokes(void)
{
    stroke *sp;
    char *cp;
    size_t bytes;

    /* ~32 strokes/KB */
    bytes = 640 * sizeof(stroke);
    if ((cp = malloc(bytes)) == (char *)0) {
	/* Attempt to draw/free some vectors and try again */
	OutBuild();
	if ((cp = malloc(bytes)) == (char *)0) {
	    fprintf(stderr, "pl-fb: malloc failed!\n");
	    bu_exit(2, NULL);
	}
    }
    /* link them all into the free list */
    sp = (stroke *)cp;
    while (bytes >= sizeof(stroke)) {
	sp->freep = freep;
	sp->magic = STROKE_MAGIC;
	freep = sp++;
	bytes -= sizeof(stroke);
    }
}


/*
 * S X T 1 6
 *
 * Take a value which is currently considered "unsigned" with 16 bits
 * of significance, and sign-extend it in a reasonably portable way.
 * We assume the machine is twos-compliment.
 */
long
sxt16(long int v)
{
    long w;
    if (v <= 0x7FFF) return v;
    w = -1;
    w &= ~0x7FFF;
    return w | v;
}


int
get_args(int argc, char **argv)
{
    int c;

    while ((c = bu_getopt(argc, argv, "hdoOit:F:s:S:w:W:n:N:")) != -1) {
	switch (c) {
	    case 't':
		line_thickness = atoi(bu_optarg);
		if (line_thickness <= 0)
		    line_thickness = 1;
		break;
	    case 'i':
		immediate = 1;
		break;
	    case 'd':
		/* -d given multiple times increases debug level */
		debug++;
		break;
	    case 'O':
	    case 'o':
		over = 1;
		break;
	    case 'F':
		framebuffer = bu_optarg;
		break;
	    case 'h':
		Nscanlines = Npixels = 1024;
		break;
	    case 'S':
	    case 's':
		Nscanlines = Npixels = atoi(bu_optarg);
		break;
	    case 'W':
	    case 'w':
		Npixels = atoi(bu_optarg);
		break;
	    case 'N':
	    case 'n':
		Nscanlines = atoi(bu_optarg);
		break;
	    default:		/* '?' */
		return 0;
	}
    }

    if (bu_optind >= argc) {
	/* no file name given, use stdin */
	if (isatty(fileno(stdin)))
	    return 0;
	filename = "-";
	pfin = stdin;
    } else {
	/* open file */
	filename = argv[bu_optind];
	if ((pfin = fopen(filename, "rb")) == NULL) {
	    fprintf(stderr,
		    "pl-fb: Can't open file \"%s\"\n", filename);
	    return 0;
	}
    }

    if (argc > ++bu_optind)
	(void)fprintf(stderr, "pl-fb: excess argument(s) ignored\n");

    return 1;		/* OK */
}


static char usage[] = "\
Usage: pl-fb [-h -d -o -i] [-t thickness] [-F framebuffer]\n\
	[-S squaresize] [-W width] [-N height] [file.plot]\n";


/*
  InitDesc - initialize stroke descriptor lists
*/
static void
InitDesc(void)
{
    struct band *bp;	/* *bp -> start of descr list */

    for (bp = &band[0]; bp < &band[BANDSLOP]; ++bp) {
	bp->first = NULL;		/* nothing in band yet */
	bp->last  = NULL;
    }
}


/*
  FreeUp - deallocate descriptors
*/
static void
FreeUp(void)
{
    struct band *bp;
    stroke *vp;		/* -> rasterization descr */

    for (bp = &band[0]; bp < bandEnd; ++bp)
	while ((vp = Dequeue(bp, &bp->first)) != NULL)
	    FREE_STROKE(vp);	/* free storage */
}


/*
  Foo - clean up before return from rasterizer
*/
static int
Foo(int code)				/* returns status code */
{
    if (debug) fprintf(stderr, "Foo(%d)\n", code);
    fb_close(fbp);		/* release framebuffer */

    FreeUp();			/* deallocate descriptors */

    return code;
}


/*
 * P R E P _ D D A
 *
 * Set up multi-band DDA parameters for stroke
 */
static void
prep_dda(stroke *vp, coords *pt1, coords *pt2)
{
    CK_STROKE(vp);
    vp->pixel = *pt1;		/* initial pixel */
    vp->major = pt2->y - vp->pixel.y;	/* always nonnegative */
    vp->ysign = vp->major ? 1 : 0;
    vp->minor = pt2->x - vp->pixel.x;
    COPYRGB(vp->col, cur_color);
    if ((vp->xsign = vp->minor ? (vp->minor > 0 ? 1 : -1) : 0) < 0)
	vp->minor = -vp->minor;

    /* if Y is not really major, correct the assignments */
    if (!(vp->ymajor = vp->minor <= vp->major)) {
	short temp;	/* temporary for swap */

	temp = vp->minor;
	vp->minor = vp->major;
	vp->major = temp;
    }

    vp->e = vp->major / 2 - vp->minor;	/* initial DDA error */
    vp->de = vp->major - vp->minor;
}


/*
 * BuildStr - set up DDA parameters and queue stroke
 *
 * Given two end points of a line, allocate and intialize a stroke
 * descriptor for it.  If we are drawing "thick" lines we generate
 * several extra stroke descriptors as well.  In immediate or memory
 * buffered mode we rasterize it and free it right away.  In "regular"
 * banded buffered mode, we link the descriptor(s) into its starting
 * point band(s).
 */
static int
BuildStr(coords *pt1, coords *pt2)		/* returns true or dies */
    /* endpoints */
{
    stroke *vp;		/* -> rasterization descr */
    int thick;

    /* arrange for pt1 to have the smaller Y-coordinate: */
    if (pt1->y > pt2->y) {
	coords *temp;	/* temporary for swap */

	temp = pt1;		/* swap pointers */
	pt1 = pt2;
	pt2 = temp;
    }

    GET_STROKE(vp);			/* alloc a descriptor */
    prep_dda(vp, pt1, pt2);	/* prep it */

    /* Thicken by advancing alternating pixels in minor direction */
    thick = line_thickness - 1;	/* number of "extra" pixels */
    if (thick >= vp->major && vp->major > 0) thick = vp->major-1;
    for (; thick >= 0; thick--) {
	stroke *v2;

	if (thick == 0) {
	    /* last pass, use vp */
	    v2 = vp;
	} else {
	    /* make a new one */
	    GET_STROKE(v2);
	    *v2 = *vp;
	}

	/* Advance minor only */
	if (vp->ymajor)
	    v2->pixel.x += (vp->xsign!=0 ? vp->xsign : 1) *
		((thick&1)==0 ? (thick+1)/2 : (thick+1)/-2);
	else
	    v2->pixel.y += (vp->ysign!=0 ? vp->ysign : 1) *
		((thick&1)==0 ? (thick+1)/2 : (thick+1)/-2);

	if (immediate || single_banded) {
	    ystart = 0;
	    Raster(v2, (struct band *)0);
	}  else
	    /* link descriptor into band corresponding to starting scan */
	    Requeue(&band[v2->pixel.y / lines_per_band], v2);
    }
    return true;
}


static int
GetCoords(coords *coop)
    /* -> input coordinates */
{
    unsigned char buf[4];
    double x, y;

    /* read coordinates */
    if (fread((char *)buf, (int)sizeof (buf), 1, pfin) != 1)
	return false;


    x = sxt16((long)(buf[1]<<8) | buf[0]);
    y = sxt16((long)(buf[3]<<8) | buf[2]);
    if (debug) fprintf(stderr, "Coord: (%g, %g) ", x, y);

    /* limit left, bottom */
    if ((x -= space.left) < 0)
	x = 0;
    if ((y -= space.bottom) < 0)
	y = 0;

    /* convert to device pixels */
    coop->x = (short)(x * Npixels / (double)delta + 0.5);
    coop->y = (short)(y * Nscanlines / (double)delta + 0.5);

    /* limit right, top */
    if (coop->x > XMAX)
	coop->x = XMAX;
    if (coop->y > YMAX)
	coop->y = YMAX;

    if (debug)
	fprintf(stderr, "Pixel: (%d, %d)\n", coop->x, coop->y);

    return true;
}


/*
  GetCoords - input x, y coordinates and scale into pixels
*/
int Get3Coords(coords *coop)
{
    char trash[2];
    int ret;

    ret = GetCoords(coop);
    if (fread(trash, sizeof(trash), 1, pfin) != 1)
	return false;

    return ret;
}


/* IEEE coordinates */
int Get3DCoords(coords *coop)
{
    static unsigned char in[3*8];
    static double out[2];
    double x, y;

    /* read coordinates */
    if (fread(in, sizeof(in), 1, pfin) != 1)
	return false;
    ntohd((unsigned char *)out, in, 2);
    x = out[0];
    y = out[1];

    /* limit left, bottom */
    if ((x -= space.left) < 0)
	x = 0;
    if ((y -= space.bottom) < 0)
	y = 0;

    /* convert to device pixels */
    coop->x = (short)(x * Npixels / (double)delta + 0.5);
    coop->y = (short)(y * Nscanlines / (double)delta + 0.5);

    /* limit right, top */
    if (coop->x > XMAX)
	coop->x = XMAX;
    if (coop->y > YMAX)
	coop->y = YMAX;

    if (debug) {
	fprintf(stderr, "Coord3: (%g, %g) ", out[0], out[1]);
	fprintf(stderr, "Pixel3: (%d, %d)\n", coop->x, coop->y);
    }
    return true;
}


int
GetDCoords(coords *coop)
    /* -> input coordinates */
{
    static unsigned char in[2*8];
    static double out[2];
    double x, y;

    /* read coordinates */
    if (fread(in, sizeof(in), 1, pfin) != 1)
	return false;
    ntohd((unsigned char *)out, in, 2);
    x = out[0];
    y = out[1];

    /* limit left, bottom */
    if ((x -= space.left) < 0)
	x = 0;
    if ((y -= space.bottom) < 0)
	y = 0;

    /* convert to device pixels */
    coop->x = (short)(x * Npixels / (double)delta + 0.5);
    coop->y = (short)(y * Nscanlines / (double)delta + 0.5);

    /* limit right, top */
    if (coop->x > XMAX)
	coop->x = XMAX;
    if (coop->y > YMAX)
	coop->y = YMAX;

    if (debug) {
	fprintf(stderr, "Coord2: (%g, %g) ", out[0], out[1]);
	fprintf(stderr, "Pixel2: (%d, %d)\n", coop->x, coop->y);
    }
    return true;
}


/*
 * E D G E L I M I T
 *
 * Limit generated positions to edges of screen
 */
void
edgelimit(coords *ppos)
{
    if (ppos->x >= Npixels)
	ppos->x = Npixels -1;

    if (ppos->y >= Nscanlines)
	ppos->y = Nscanlines -1;
}


/*
 * PutVectorChar - Put vectors corresponding to this character out
 * into plotting space
 * Update position to reflect character width.
 */
void
put_vector_char(char c, coords *pos)
{
    static coords start, end;
    struct vectorchar *vc;
    struct relvect *rv;

    if (!isascii(c))
	c = '?';
    if (islower(c))
	c = toupper(c);

    for (vc = &charset[0]; vc->ascii; vc++)
	if (vc->ascii == c)
	    break;

    if (!vc->ascii) {
	/* Character not found in table -- space over 1/2 char */
	pos->x += X_CHAR_SIZE/2;
	return;
    }

    /* have the correct character entry - start plotting */
    start.x = vc->r[0].x + pos->x;
    start.y = Y_CHAR_SIZE - vc->r[0].y + pos->y;

    for (rv = &vc->r[1]; (rv < &vc->r[10]) && rv->x >= 0; rv++) {
	end.x = rv->x + pos->x;
	end.y = Y_CHAR_SIZE - rv->y + pos->y;
	edgelimit(&start);
	edgelimit(&end);
	BuildStr(&start, &end);	/* pixels */
	start = end;
    }
    pos->x += X_CHAR_SIZE;
}


/*
  DoFile - process UNIX plot file

  This routine reads UNIX plot records from the specified file
  and controls the entry of the strokes into the descriptor lists.
  Strokes are limited (not clipped) to fit the frame.

  Upon end of file, erase, or flush, plot data is copied to the device.
  Returns status code:
  < 0	=> catastrophe
  = 0	=> complete success
  > 0	=> line limit hit
*/
static int
DoFile(void)	/* returns vpl status code */
{
    int plotted;	/* false => empty frame image */
    int c;		/* input character */
    static coords newpos; 	/* current input coordinates */
    static coords virpos; 	/* virtual pen position */
    static unsigned char buf3[6*2];
    static unsigned char buf2[4*2];
    static int firsterase = true;

    /* process each frame into a raster image file */

    for (;;) {
	/* for each frame */
	InitDesc();		/* empty descriptor lists */

	virpos.x = virpos.y = 0;
	plotted = false;

	for (;;) {
	    /* read until EOF*/
	    c = getc(pfin);
	    if (debug > 1) fprintf(stderr, "%c\n", c);
	    switch (c) {
		/* record type */
		case EOF:
		    if (debug) fprintf(stderr, "EOF\n");

		    if (plotted) {
			/* flush strokes */
			if (debug) fprintf(stderr, "flushing\n");
			if (!OutBuild())
			    return Foo(-6);
		    }
		    return Foo(0);/* success */

		case 'e':	/* erase */
		    if (debug) fprintf(stderr, "Erase\n");

		    if (plotted) {
			/* flush strokes */
			if (debug) fprintf(stderr, "flushing\n");
			if (!OutBuild())
			    return Foo(-6);
		    }
		    if (!firsterase) {
			if (immediate)
			    fb_clear(fbp, RGBPIXEL_NULL);
			over = 0;
		    }
		    firsterase = false;
		    break;	/* next frame */

		case 'F':	/* flush */
		    if (debug) fprintf(stderr, "Flush\n");

		    if (plotted) {
			/* flush strokes */
			if (debug) fprintf(stderr, "flushing\n");
			if (!OutBuild())
			    return Foo(-6);
			if (!immediate)
			    over = 1;
		    }
		    firsterase = false;
		    break;	/* next frame */

		case 'f':	/* linemod */
		    if (debug)
			fprintf(stderr, "linemod\n");
		    /* ignore for time being */
		    while ((c = getc(pfin)) != EOF
			   && c != '\n'
			)
			;	/* eat string */
		    continue;

		case 'L':
		case 'M':
		    if (!Get3Coords(&newpos))
			return Foo(-8);
		    virpos = newpos;
		    if (c == 'M') {
			if (debug)
			    fprintf(stderr, "Move3\n");
			continue;
		    }
		    if (debug)
			fprintf(stderr, "Line3\n");

		case 'N':	/* continue3 */
		case 'P':	/* point3 */
		    if (!Get3Coords(&newpos))
			return Foo(-9);
		    if (c == 'P') {
			if (debug)
			    fprintf(stderr, "point3\n");
			virpos = newpos;
		    } else
			if (debug)
			    fprintf(stderr, "cont3\n");

		    if (!BuildStr(&virpos, &newpos))
			return Foo(-10);
		    plotted = true;
		    virpos = newpos;
		    continue;

		case 'l':	/* line */
		case 'm':	/* move */
		    if (!GetCoords(&newpos))
			return Foo(-8);
		    virpos = newpos;
		    if (c == 'm') {
			if (debug)
			    fprintf(stderr, "move\n");
			continue;
		    }
		    /* line: fall through */
		    if (debug)
			fprintf(stderr, "line\n");

		case 'n':	/* cont */
		case 'p':	/* point */
		    if (!GetCoords(&newpos))
			return Foo(-9);
		    if (c == 'p') {
			if (debug)
			    fprintf(stderr, "point\n");
			virpos = newpos;
		    } else
			if (debug)
			    fprintf(stderr, "cont\n");

		    if (!BuildStr(&virpos, &newpos))
			return Foo(-10);
		    plotted = true;
		    virpos = newpos;
		    continue;

		    /* IEEE */
		case 'V':
		case 'O':
		    if (!Get3DCoords(&newpos))
			return Foo(-8);
		    virpos = newpos;
		    if (c == 'O') {
			if (debug)
			    fprintf(stderr, "dMove3\n");
			continue;
		    }
		    if (debug)
			fprintf(stderr, "dLine3\n");

		case 'Q':	/* continue3 */
		case 'X':	/* point3 */
		    if (!Get3DCoords(&newpos))
			return Foo(-9);
		    if (c == 'X') {
			if (debug)
			    fprintf(stderr, "dpoint3\n");
			virpos = newpos;
		    } else
			if (debug)
			    fprintf(stderr, "dcont3\n");

		    if (!BuildStr(&virpos, &newpos))
			return Foo(-10);
		    plotted = true;
		    virpos = newpos;
		    continue;

		case 'v':	/* line */
		case 'o':	/* move */
		    if (!GetDCoords(&newpos))
			return Foo(-8);
		    virpos = newpos;
		    if (c == 'o') {
			if (debug)
			    fprintf(stderr, "dmove\n");
			continue;
		    }
		    /* line: fall through */
		    if (debug)
			fprintf(stderr, "dline\n");

		case 'q':	/* cont */
		case 'x':	/* point */
		    if (!GetDCoords(&newpos))
			return Foo(-9);
		    if (c == 'x') {
			if (debug)
			    fprintf(stderr, "dpoint\n");
			virpos = newpos;
		    } else
			if (debug)
			    fprintf(stderr, "dcont\n");

		    if (!BuildStr(&virpos, &newpos))
			return Foo(-10);
		    plotted = true;
		    virpos = newpos;
		    continue;

		case 'W': {
		    unsigned char in[6*8];
		    double out[6];
		    if (debug)
			fprintf(stderr, "dspace3\n");
		    if (fread(in, sizeof(in), 1, pfin) != 1)
			return Foo(-11);
		    ntohd((unsigned char *)out, in, 5);
		    /* Only need X and Y, ignore Z */
		    space.left  = out[0]; /* x1 */
		    space.bottom= out[1]; /* y1 */
		    /* z1 */
		    space.right = out[3]; /* x2 */
		    space.top   = out[4]; /* y2 */
		    /* z2 */
		    goto spacend;
		}

		case 'w': {
		    /* space */
		    unsigned char in[4*8];
		    double out[4];
		    if (debug)
			fprintf(stderr, "dspace\n");
		    if (fread(in, sizeof(in), 1, pfin) != 1)
			return Foo(-11);
		    ntohd((unsigned char *)out, in, 4);
		    space.left  = out[0]; /* x1 */
		    space.bottom= out[1]; /* y1 */
		    space.right = out[2]; /* x2 */
		    space.top   = out[3]; /* y2 */
		    goto spacend;
		}

		case 'S': {
		    if (debug)
			fprintf(stderr, "space3\n");
		    if (fread((char *)buf3,
			      (int)sizeof buf3, 1, pfin)
			!= 1
			)
			return Foo(-11);
		    /* Only need X and Y, ignore Z */
		    space.left  = sxt16((long)(buf3[1]<<8) | buf3[0]); /* x1 */
		    space.bottom= sxt16((long)(buf3[3]<<8) | buf3[2]); /* y1 */
		    /* z1 */
		    space.right = sxt16((long)(buf3[7]<<8) | buf3[6]); /* x2 */
		    space.top   = sxt16((long)(buf3[9]<<8) | buf3[8]); /* y2 */
		    /* z2 */
		    goto spacend;
		}

		case 's':	/* space */
		    if (debug)
			fprintf(stderr, "space\n"); {
			if (fread((char *)buf2,
				  (int)sizeof buf2, 1, pfin
				) != 1
			    )
			    return Foo(-11);
			space.left  = sxt16((long)(buf2[1]<<8) | buf2[0]); /* x1 */
			space.bottom= sxt16((long)(buf2[3]<<8) | buf2[2]); /* y1 */
			space.right = sxt16((long)(buf2[5]<<8) | buf2[4]); /* x2 */
			space.top   = sxt16((long)(buf2[7]<<8) | buf2[6]); /* y2 */
		    }

	    spacend:
		    delta = space.right - space.left;
		    deltao2 = space.top - space.bottom;
		    if (deltao2 > delta)
			delta = deltao2;
		    if (delta <= 0) {
			fprintf(stderr, "pl-fb: delta = %g, bad space()\n", delta);
			return Foo(-42);
		    }
		    deltao2 = delta / 2.0;
		    if (debug)
			fprintf(stderr, "Space: X=(%g, %g) Y=(%g, %g) delta=%g\n",
				space.left, space.right,
				space.bottom, space.top,
				delta);
		    continue;

		case 'C':	/* color */
		    if (fread(cur_color, 1, 3, pfin) != 3)
			return Foo(-11);
		    if (debug)
			fprintf(stderr, "Color is R%d G%d B%d\n",
				cur_color[RED],
				cur_color[GRN],
				cur_color[BLU]);
		    continue;

		case 't':	/* label */
		    if (debug)
			fprintf(stderr, "label: ");

		    newpos = virpos;
		    while ((c = getc(pfin)) != EOF && c != '\n'
			) {
			/* vectorize the characters */
			put_vector_char(c, &newpos);

			if (debug)
			    putc(c, stderr);
		    }

		    plotted = true;
		    virpos = newpos;
		    continue;

		    /* discard the deadwood */
		case 'c': {
		    char buf[3*2];
		    if (fread(buf, sizeof(buf), 1, pfin) != 1)
			return Foo(-11);
		    if (debug)
			fprintf(stderr, "circle ignored\n");
		    continue;
		}
		case 'i': {
		    char buf[3*8];
		    if (fread(buf, sizeof(buf), 1, pfin) != 1)
			return Foo(-11);
		    if (debug)
			fprintf(stderr, "d_circle ignored\n");
		    continue;
		}
		case 'a': {
		    char buf[6*2];
		    if (fread(buf, sizeof(buf), 1, pfin) != 1)
			return Foo(-11);
		    if (debug)
			fprintf(stderr, "arc ignored\n");
		    continue;
		}
		case 'r': {
		    char buf[6*8];
		    if (fread(buf, sizeof(buf), 1, pfin) != 1)
			return Foo(-11);
		    if (debug)
			fprintf(stderr, "d_arc ignored\n");
		    continue;
		}

		default:
		    fprintf(stderr, "bad command '%c' (0x%02x)\n", c, c);

		    return Foo(-12);	/* bad input */
	    }
	    break;
	}		/* next input record */
    }			/* next frame */
}


/*
  Catch - invoked on interrupt
*/
static void
Catch(int sig)
    /* signal number */
{
    int pid;		/* this process's ID */
    int i;

    for (i = 0; sigs[i]; ++i)
	(void)signal(sigs[i], SIG_IGN);

    (void)Foo(-13);		/* clean up */

    (void)signal(sig, SIG_DFL);

    pid = bu_process_id();

    if (pid > 1) {
#ifdef HAVE_KILL
	/* (re)signal process */
	(void)kill(pid, sig);
#endif
    }
}


/*
  SetSigs - set up signal catchers
*/
static void
SetSigs(void)
{
    int i;

    for (i = 0; sigs[i]; ++i) {
	if (signal(sigs[i], SIG_IGN) != SIG_IGN)
	    (void)signal(sigs[i], Catch);
    }
}


/*
 * M A I N
 *
 * Parse arguments, valid ones are:
 * name of file to plot (instead of STDIN)
 * -d for debugging statements
 *
 * Default (no arguments) action is to plot STDIN on current FB.
 */
int
main(int argc, char **argv)
{
    Nscanlines = Npixels = 512;

    if (!get_args(argc, argv)) {
	(void)fputs(usage, stderr);
	bu_exit(1, NULL);
    }

    /* Open frame buffer, adapt to slightly smaller ones */
    if ((fbp = fb_open(framebuffer, Npixels, Nscanlines)) == FBIO_NULL) {
	fprintf(stderr, "pl-fb: fb_open failed\n");
	bu_exit(1, NULL);
    }
    Npixels = fb_getwidth(fbp);
    Nscanlines = fb_getheight(fbp);
    if (immediate) {
	lines_per_band = Nscanlines;
	if (!over)
	    fb_clear(fbp, RGBPIXEL_NULL);
    } else if (Nscanlines <= 512) {
	/* make one full size band */
	lines_per_band = Nscanlines;
	single_banded = 1;
    }

    /*
     * Handle image-size specific initializations
     */
    if ((Nscanlines % lines_per_band) != 0) {
	/* round it down - only necessary if buffered? */
	Nscanlines = (Nscanlines / lines_per_band) * lines_per_band;
    }
    space.left = space.right = 0;
    space.right = Npixels;
    space.top = Nscanlines;
    delta = Nscanlines;
    deltao2 = Nscanlines/2;

    buffersize = lines_per_band*Npixels*sizeof(RGBpixel);
    if ((buffer = (unsigned char *)malloc(buffersize)) == RGBPIXEL_NULL) {
	fprintf(stderr, "pl-fb:  malloc error\n");
	bu_exit(1, NULL);
    }
    /* Extra band protects against requeueing off the top */
    band = (struct band *)malloc((BANDSLOP)*sizeof(struct band));
    if (band == (struct band *)0) {
	fprintf(stderr, "pl-fb: malloc error2\n");
	bu_exit(1, NULL);
    }
    memset((char *)band, 0, (BANDSLOP)*sizeof(struct band));
    bandEnd = &band[BANDS];
    if (single_banded && over) {
	/* Read in initial screen */
	if (fb_read(fbp, 0, 0, buffer, buffersize/sizeof(RGBpixel)) <= 0)
	    fprintf(stderr, "pl-fb: band read error\n");
    }
    if (debug)
	fprintf(stderr, "pl-fb output of %s\n", filename);

    SetSigs();			/* set signal catchers */

    (void)DoFile();	/* plot it */

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
