/*                      P I X S C A L E . C
 * BRL-CAD
 *
 * Copyright (c) 1986-2026 United States Government as represented by
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
/** @file util/pixscale.c
 *
 * Scale an RGB pix file.
 *
 * To scale up, we use bilinear interpolation.
 * To scale down, we assume "square pixels" and preserve the
 * amount of light energy per unit area.
 *
 * This is a buffered version that can handle files of
 * almost arbitrary size.
 *
 * Note: This is a simple extension to bwcrop.  Improvements made
 * there should be incorporated here.
 *
 */

#include "common.h"

#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include "bio.h"

#include "bu/app.h"
#include "bu/getopt.h"
#include "bu/opt.h"
#include "bu/malloc.h"
#include "bu/log.h"
#include "bu/file.h"


#define MAXBUFBYTES 3*1024*1024	/* max bytes to malloc in buffer space */
#define INTERPOLATION_SCANLINES 2

unsigned char *outbuf;
unsigned char *buffer;
ssize_t scanlen;		/* length of infile (and buffer) scanlines */
ssize_t buflines;		/* Number of lines held in buffer */

static b_off_t buf_start = -1000;	/* First line in buffer */

static ssize_t bufloaded;	/* Number of valid lines held in buffer */

static b_off_t next_scanline;	/* Next unread input scanline */

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
Usage: pixscale [-r] [-s squareinsize] [-w inwidth] [-n inheight]\n\
	[-S squareoutsize] [-W outwidth] [-N outheight] [in.pix] > out.pix\n";

/****** THIS PROBABLY SHOULD BE ELSEWHERE *******/

/* ceiling and floor functions for positive numbers */
#define CEILING(x)	(((x) > (int)(x)) ? (int)(x)+1 : (int)(x))
#define FLOOR(x)	((int)(x))
#define MIN(x, y)	(((x) > (y)) ? (y) : (x))


static size_t
read_scanlines(unsigned char *destination, size_t count)
{
    size_t ret = fread(destination, (size_t)scanlen, count, buffp);

    if (ret < count && ferror(buffp))
	bu_exit(3, "pixscale: error reading input: %s\n", strerror(errno));

    return ret;
}


/* Refill without seeking so files and standard input behave alike. */
void
fill_buffer(int y)
{
    size_t keep = 0;

    if ((b_off_t)y < buf_start)
	bu_exit(3, "pixscale: cannot read input scanlines out of order\n");

    if (bufloaded > 0 && (b_off_t)y < buf_start + bufloaded) {
	keep = (size_t)(buf_start + bufloaded - y);
	memmove(buffer, buffer + ((b_off_t)y - buf_start) * scanlen, keep * (size_t)scanlen);
    } else {
	while (next_scanline < (b_off_t)y) {
	    size_t skip = (size_t)((b_off_t)y - next_scanline);
	    size_t chunk = (skip < (size_t)buflines) ? skip : (size_t)buflines;
	    size_t ret = read_scanlines(buffer, chunk);

	    next_scanline += (b_off_t)ret;
	    if (ret != chunk)
		bu_exit(3, "pixscale: input ends before scanline %d\n", y);
	}
    }

    buf_start = y;
    bufloaded = (ssize_t)(keep + read_scanlines(buffer + keep * (size_t)scanlen,
	(size_t)buflines - keep));
    next_scanline += (b_off_t)(bufloaded - (ssize_t)keep);
}


static void
buffer_scanlines(int y, size_t count)
{
    b_off_t offset = (b_off_t)y - buf_start;

    if (offset < 0 || offset + (b_off_t)count > bufloaded) {
	fill_buffer(y);
	offset = (b_off_t)y - buf_start;
    }

    if (offset < 0 || offset + (b_off_t)count > bufloaded)
	bu_exit(3, "pixscale: input ends before scanline %d\n", y + (int)count - 1);

    bufy = (ssize_t)offset;
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
	buffer_scanlines((int)y, 1);

	op = outbuf;

	for (i = 0; i < ox; i++) {
	    x = i * xstep;
	    lp = &buffer[bufy*scanlen+(int)x*3];
	    *op++ = lp[0];
	    *op++ = lp[1];
	    *op++ = lp[2];
	}

	ret = fwrite(outbuf, 3, ox, ofp);
	if (ret < (size_t)ox)
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
	buffer_scanlines((int)y, INTERPOLATION_SCANLINES);

	op = outbuf;

	for (i = 0; i < ox; i++) {
	    x = i * xstep;
	    dx = x - (int)x;
	    dy = y - (int)y;

	    /* Note: (1-a)*foo + a*bar = foo + a*(bar-foo) */

	    lp = &buffer[bufy*scanlen+(int)x*3];
	    up = &buffer[(bufy+1)*scanlen+(int)x*3];

	    /* Red */
	    mid1 = lp[0] + dx * ((double)lp[3] - (double)lp[0]);
	    mid2 = up[0] + dx * ((double)up[3] - (double)up[0]);
	    *op++ = mid1 + dy * (mid2 - mid1);
	    lp++; up++;

	    /* Green */
	    mid1 = lp[0] + dx * ((double)lp[3] - (double)lp[0]);
	    mid2 = up[0] + dx * ((double)up[3] - (double)up[0]);
	    *op++ = mid1 + dy * (mid2 - mid1);
	    lp++; up++;

	    /* Blue */
	    mid1 = lp[0] + dx * ((double)lp[3] - (double)lp[0]);
	    mid2 = up[0] + dx * ((double)up[3] - (double)up[0]);
	    *op++ = mid1 + dy * (mid2 - mid1);
	}

	ret = fwrite(outbuf, 3, ox, ofp);
	if (ret < (size_t)ox)
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
    double sumr, sumg, sumb;
    unsigned char *op;
    size_t ret;

    if (ix == ox)
	pxlen = 1.0;
    else
	pxlen = (double)ix / (double)ox;
    if (iy == oy)
	pylen = 1.0;
    else
	pylen = (double)iy / (double)oy;
    if ((pxlen < 1.0 && pylen > 1.0) || (pxlen > 1.0 && pylen < 1.0)) {
	bu_log("pixscale: can't stretch one way and compress another!\n");
	return -1;
    }
    if (pxlen < 1.0 || pylen < 1.0) {
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
	ystart = j * pylen;
	yend = ystart + pylen;
	op = outbuf;
	for (i = 0; i < ox; i++) {
	    xstart = i * pxlen;
	    xend = xstart + pxlen;
	    sumr = sumg = sumb = 0.0;
	    /*
	     * For each pixel of the original falling
	     * inside this new pixel.
	     */
	    for (l = FLOOR(ystart); l < CEILING(yend); l++) {

		/* Make sure we have this row in the buffer */
		buffer_scanlines(l, 1);

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
		    sumr += buffer[bufy * scanlen + 3*k] * xdist * ydist;
		    sumg += buffer[bufy * scanlen + 3*k+1] * xdist * ydist;
		    sumb += buffer[bufy * scanlen + 3*k+2] * xdist * ydist;
		}
	    }
	    *op++ = (int)(sumr / (pxlen * pylen));
	    *op++ = (int)(sumg / (pxlen * pylen));
	    *op++ = (int)(sumb / (pxlen * pylen));
	}
	ret = fwrite(outbuf, 3, ox, ofp);
	if (ret < (size_t)ox)
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
     * Do a max of page size.  We really should see how big the input
     * file is to decide if we should buffer less than our max.
     */
    if (max > BU_PAGE_SIZE)
	max = BU_PAGE_SIZE;

    if (iny > 1 && max < INTERPOLATION_SCANLINES)
	max = INTERPOLATION_SCANLINES;
    else if (max < 1)
	max = 1;

    if (max < iny)
	buflines = max;
    else
	buflines = iny;

    buf_start = 0;
    bufloaded = 0;
    next_scanline = 0;
    buffer = (unsigned char *)bu_malloc(buflines * scanlen, "buffer");
}


int
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
		if (!bu_opt_scan_int_range(bu_optarg, &outx, 1, INT_MAX, "output size"))
		    return 0;
		outy = outx;
		break;
	    case 's':
		/* square size */
		if (!bu_opt_scan_int_range(bu_optarg, &inx, 1, INT_MAX, "input size"))
		    return 0;
		iny = inx;
		break;
	    case 'W':
		if (!bu_opt_scan_int_range(bu_optarg, &outx, 1, INT_MAX, "output width"))
		    return 0;
		break;
	    case 'w':
		if (!bu_opt_scan_int_range(bu_optarg, &inx, 1, INT_MAX, "input width"))
		    return 0;
		break;
	    case 'N':
		if (!bu_opt_scan_int_range(bu_optarg, &outy, 1, INT_MAX, "output height"))
		    return 0;
		break;
	    case 'n':
		if (!bu_opt_scan_int_range(bu_optarg, &iny, 1, INT_MAX, "input height"))
		    return 0;
		break;

	    default:		/* 'h' , '?' */
		return 0;
	}
    }

    /* XXX - backward compatibility hack */
    if (bu_optind+5 == argc) {
	file_name = argv[bu_optind++];
	if ((buffp = fopen(file_name, "rb")) == NULL) {
	    bu_log("pixscale: cannot open \"%s\" for reading\n", file_name);
	    return 0;
	}
	if (!bu_opt_scan_int_range(argv[bu_optind++], &inx, 1, INT_MAX, "input width"))
	    return 0;
	if (!bu_opt_scan_int_range(argv[bu_optind++], &iny, 1, INT_MAX, "input height"))
	    return 0;
	if (!bu_opt_scan_int_range(argv[bu_optind++], &outx, 1, INT_MAX, "output width"))
	    return 0;
	if (!bu_opt_scan_int_range(argv[bu_optind++], &outy, 1, INT_MAX, "output height"))
	    return 0;
	return 1;
    }
    if (bu_optind >= argc) {
	if (isatty(fileno(stdin)))
	    return 0;
	file_name = hyphen;
	buffp = stdin;
    } else {
	file_name = argv[bu_optind];
	bu_optind++;
	if (argc > bu_optind) {
	    bu_log("pixscale: excess argument(s) not supported\n");
	    return 0;
	}
	if ((buffp = fopen(file_name, "rb")) == NULL) {
	    bu_log("pixscale: cannot open \"%s\" for reading\n", file_name);
	    return 0;
	}
    }

    if (argc > bu_optind) {
	bu_log("pixscale: excess argument(s) not supported\n");
	return 0;
    }

    return 1;		/* OK */
}


int
main(int argc, char **argv)
{
    int i;

    bu_setprogname(argv[0]);

    setmode(fileno(stdin), O_BINARY);
    setmode(fileno(stdout), O_BINARY);

    if (argc == 1 && isatty(fileno(stdin)) && isatty(fileno(stdout)))
	bu_exit(1, "%s", usage);
    if (!get_args(argc, argv) || isatty(fileno(stdout)))
	bu_exit(1, "%s", usage);

    if (inx <= 0 || iny <= 0 || outx <= 0 || outy <= 0) {
	bu_exit(2, "pixscale: bad size\n");
    }

    /* See how many lines we can buffer */
    scanlen = 3 * inx;
    init_buffer();
    if (inx < outx) i = outx * 3;
    else i = inx * 3;

    outbuf = (unsigned char *)bu_malloc(i, "outbuf");

    /* Here we go */
    scale(stdout, inx, iny, outx, outy);
    bu_free(outbuf, "outbuf");
    bu_free(buffer, "buffer");
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
