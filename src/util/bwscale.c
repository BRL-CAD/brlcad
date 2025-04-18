/*                       B W S C A L E . C
 * BRL-CAD
 *
 * Copyright (c) 1986-2025 United States Government as represented by
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
/** @file util/bwscale.c
 *
 * Scale a black and white picture.
 *
 * To scale up, we use bilinear interpolation.
 * To scale down, we assume "square pixels" and preserve the
 * amount of light energy per unit area.
 *
 * This is a buffered version that can handle files of
 * almost arbitrary size.
 * Note: this buffer code also appears in bwcrop.c
 *
 */

#include "common.h"

#include <stdlib.h>
#include <string.h>
#include "bio.h"

#include "bu/app.h"
#include "bu/getopt.h"
#include "bu/malloc.h"
#include "bu/log.h"
#include "bu/file.h"


#define MAXBUFBYTES BU_PAGE_SIZE*BU_PAGE_SIZE	/* max bytes to malloc in buffer space */

unsigned char *outbuf;
unsigned char *buffer;
ssize_t scanlen;		/* length of infile (and buffer) scanlines */
ssize_t buflines;		/* Number of lines held in buffer */
b_off_t buf_start = -1000;	/* First line in buffer */

ssize_t bufy;				/* y coordinate in buffer */
FILE *buffp;
static char *file_name;
static char hyphen[] = "-";

int rflag = 0;
int inx = 512;
int iny = 512;
int outx = 512;
int outy = 512;


static char usage[] = "\
Usage: bwscale [-r] [-s squareinsize] [-w inwidth] [-n inheight]\n\
	[-S squareoutsize] [-W outwidth] [-N outheight] [in.bw] > out.bw\n";

/****** THIS PROBABLY SHOULD BE ELSEWHERE *******/

/* ceiling and floor functions for positive numbers */
#define CEILING(x)	(((x) > (int)(x)) ? (int)(x)+1 : (int)(x))
#define FLOOR(x)	((int)(x))
#define MIN(x, y)	(((x) > (y)) ? (y) : (x))


/*
 * Load the buffer with scan lines centered around
 * the given y coordinate.
 */
static void
fill_buffer(int y)
{
    size_t ret;

    buf_start = y - buflines/2;
    if (buf_start < 0) buf_start = 0;

    if (bu_fseek(buffp, buf_start * scanlen, 0) < 0) {
	fprintf(stderr, "bwscale: Can't seek to input pixel!\n");
	/* bu_exit (3, NULL); */
    }
    ret = fread(buffer, scanlen, buflines, buffp);
    if (ret != (size_t)buflines)
	perror("fread");
}


/*
 * Nearest Neighbor Interpolate a file of pixels.
 *
 * This version preserves the outside pixels and interps inside only.
 */
void
ninterp(FILE *ofp, int ix, int iy, int ox, int oy)
{
    int i, j;
    double x, y;
    double xstep, ystep;
    unsigned char *op, *lp;
    size_t ret;

    xstep = (double)(ix - 1) / (double)ox - 1.0e-6;
    ystep = (double)(iy - 1) / (double)oy - 1.0e-6;

    /* For each output pixel */
    for (j = 0; j < oy; j++) {
	y = j * ystep;
	/*
	 * Make sure we have this row (and the one after it)
	 * in the buffer
	 */
	bufy = (int)y - buf_start;
	if (bufy < 0 || bufy >= buflines-1) {
	    fill_buffer((int)y);
	    bufy = (int)y - buf_start;
	}

	op = outbuf;

	for (i = 0; i < ox; i++) {
	    x = i * xstep;
	    lp = &buffer[bufy*scanlen+(int)x];
	    *op++ = lp[0];
	}

	ret = fwrite(outbuf, 1, ox, ofp);
	if (ret != (size_t)ox)
	    perror("fwrite");
    }
}


/*
 * Bilinear Interpolate a file of pixels.
 *
 * This version preserves the outside pixels and interps inside only.
 */
void
binterp(FILE *ofp, int ix, int iy, int ox, int oy)
{
    int i, j;
    double x, y, dx, dy, mid1, mid2;
    double xstep, ystep;
    unsigned char *op, *up, *lp;

    xstep = (double)(ix - 1) / (double)ox - 1.0e-6;
    ystep = (double)(iy - 1) / (double)oy - 1.0e-6;

    /* For each output pixel */
    for (j = 0; j < oy; j++) {
	size_t ret;
	y = j * ystep;
	/*
	 * Make sure we have this row (and the one after it)
	 * in the buffer
	 */
	bufy = (int)y - buf_start;
	if (bufy < 0 || bufy >= buflines-1) {
	    fill_buffer((int)y);
	    bufy = (int)y - buf_start;
	}

	op = outbuf;

	for (i = 0; i < ox; i++) {
	    x = i * xstep;
	    dx = x - (int)x;
	    dy = y - (int)y;

	    /* Note: (1-a)*foo + a*bar = foo + a*(bar-foo) */

	    lp = &buffer[bufy*scanlen+(int)x];
	    up = &buffer[(bufy+1)*scanlen+(int)x];

	    mid1 = lp[0] + dx * ((double)lp[1] - (double)lp[0]);
	    mid2 = up[0] + dx * ((double)up[1] - (double)up[0]);

	    *op++ = mid1 + dy * (mid2 - mid1);
	}

	ret = fwrite(outbuf, 1, ox, ofp);
	if (ret != (size_t)ox)
	    perror("fwrite");
    }
}


/*
 * Scale a file of pixels to a different size.
 *
 * To scale down we make a square pixel assumption.
 * We will preserve the amount of light energy per unit area.
 * To scale up we use bilinear interpolation.
 */
int
scale(FILE *ofp, int ix, int iy, int ox, int oy)
{
    int i, j, k, l;
    double pxlen, pylen;			/* # old pixels per new pixel */
    double xstart, xend, ystart, yend;	/* edges of new pixel in old coordinates */
    double xdist, ydist;			/* length of new pixel sides in old coord */
    double sum;
    unsigned char *op;

    pxlen = (double)ix / (double)ox;
    pylen = (double)iy / (double)oy;
    if ((pxlen < 1.0 && pylen > 1.0) || (pxlen > 1.0 && pylen < 1.0)) {
	fprintf(stderr, "bwscale: can't stretch one way and compress another!\n");
	return -1;
    }
    if (pxlen < 1.0 || pylen < 1.0) {
	/* scale up */
	if (rflag) {
	    /* nearest neighbor interpolate */
	    ninterp(ofp, ix, iy, ox, oy);
	} else {
	    /* bilinear interpolate */
	    binterp(ofp, ix, iy, ox, oy);
	}
	return 0;
    }

    /* for each output pixel */
    for (j = 0; j < oy; j++) {
	size_t ret;

	ystart = j * pylen;
	yend = ystart + pylen;
	op = outbuf;
	for (i = 0; i < ox; i++) {
	    xstart = i * pxlen;
	    xend = xstart + pxlen;
	    sum = 0.0;
	    /*
	     * For each pixel of the original falling
	     * inside this new pixel.
	     */
	    for (l = FLOOR(ystart); l < CEILING(yend); l++) {

		/* Make sure we have this row in the buffer */
		bufy = l - buf_start;
		if (bufy < 0 || bufy >= buflines) {
		    fill_buffer(l);
		    bufy = l - buf_start;
		}

		/* Compute height of this row */
		if ((double)l < ystart)
		    ydist = CEILING(ystart) - ystart;
		else
		    ydist = MIN(1.0, yend - (double)l);

		for (k = FLOOR(xstart); k < CEILING(xend); k++) {
		    /* Compute width of column */
		    if ((double)k < xstart)
			xdist = CEILING(xstart) - xstart;
		    else
			xdist = MIN(1.0, xend - (double)k);

		    /* Add this pixels contribution */
		    /* sum += old[l][k] * xdist * ydist; */
		    sum += buffer[bufy * scanlen + k] * xdist * ydist;
		}
	    }
	    *op++ = (int)(sum / (pxlen * pylen));
	    if (op > (outbuf+scanlen))
		bu_bomb("unexpected buffer overrun");
	}
	ret = fwrite(outbuf, 1, ox, ofp);
	if (ret != (size_t)ox)
	    perror("fwrite");
    }
    return 1;
}


/*
 * Determine max number of lines to buffer.
 * and malloc space for it.
 * XXX - CHECK FILE SIZE
 */
void
init_buffer(void)
{
    ssize_t max;

    /* See how many we could buffer */
    max = MAXBUFBYTES / scanlen;

    /*
     * TODO: We really should see how big the input file is to decide
     * if we should buffer less than our max.
     */
    if (max > BU_PAGE_SIZE)
	max = BU_PAGE_SIZE;

    if (max < iny)
	buflines = max;
    else
	buflines = iny;

    buf_start = (-buflines);
    buffer = (unsigned char *)bu_calloc(buflines, scanlen, "buffer");
}

static int
get_args(int argc, char **argv)
{
    int c;

    while ((c = bu_getopt(argc, argv, "rs:w:n:S:W:N:h?")) != -1) {
	switch (c) {
	    case 'r':
		/* pixel replication */
		rflag = 1;
		break;
	    case 'S':
		/* square size */
		outx = outy = atoi(bu_optarg);
		break;
	    case 's':
		/* square size */
		inx = iny = atoi(bu_optarg);
		break;
	    case 'W':
		outx = atoi(bu_optarg);
		break;
	    case 'w':
		inx = atoi(bu_optarg);
		break;
	    case 'N':
		outy = atoi(bu_optarg);
		break;
	    case 'n':
		iny = atoi(bu_optarg);
		break;

	    default:		/* 'h' , '?' */
		return 0;
	}
    }

    /* XXX - backward compatibility hack */
    if (bu_optind+5 == argc) {
	file_name = argv[bu_optind++];
	if ((buffp = fopen(file_name, "rb")) == NULL) {
	    bu_log("bwscale: cannot open \"%s\" for reading\n",file_name);
	    return 0;
	}
	inx = atoi(argv[bu_optind++]);
	iny = atoi(argv[bu_optind++]);
	outx = atoi(argv[bu_optind++]);
	outy = atoi(argv[bu_optind++]);
	return 1;
    }
    if ((bu_optind >= argc) ||
	(argv[bu_optind][0] == '-' && argv[bu_optind][1] == '\n')) {

	/* input presumably from standard input */
	if (isatty(fileno(stdin))) {
	    return 0;
	}
	file_name = hyphen;
	buffp = stdin;
    } else {
	file_name = argv[bu_optind];
	if ((buffp = fopen(file_name, "rb")) == NULL) {
	    bu_log("bwscale: cannot open \"%s\" for reading\n", file_name);
	    return 0;
	}
    }

    if (argc > ++bu_optind)
	fprintf(stderr, "bwscale: excess argument(s) ignored\n");

    return 1;		/* OK */
}

int
main(int argc, char **argv)
{
    int i;

    bu_setprogname(argv[0]);

    setmode(fileno(stdin), O_BINARY);
    setmode(fileno(stdout), O_BINARY);

    if (!get_args(argc, argv) || isatty(fileno(stdout)))
	bu_exit(1, "%s", usage);

    if (inx <= 0 || iny <= 0 || outx <= 0 || outy <= 0) {
	bu_exit(2, "bwscale: bad size\n");
    }

    /* See how many lines we can buffer */
    scanlen = inx;
    init_buffer();

    i = (inx < outx) ? outx : inx;
    outbuf = (unsigned char *)bu_malloc(i, "outbuf");

    /* Here we go */
    scale(stdout, inx, iny, outx, outy);

    bu_free(outbuf, (const char *)buffer);
    bu_free(buffer, (const char *)buffer);
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
