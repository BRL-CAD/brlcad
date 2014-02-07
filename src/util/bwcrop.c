/*                        B W C R O P . C
 * BRL-CAD
 *
 * Copyright (c) 1986-2014 United States Government as represented by
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
/** @file util/bwcrop.c
 *
 * Crop Black and White files.
 *
 * Given four "corner points" in the input file, we produce an output
 * file of the requested size consisting of the nearest pixels.  No
 * filtering/interpolating is done.
 *
 * This can handle arbitrarily large files.  We keep a buffer of scan
 * lines centered around the last point that fell outside of the
 * buffer.  Note: this buffer code is shared with bwscale.c
 *
 */

#include "common.h"

#include <stdlib.h>
#include <stdio.h>

#include "bu.h"


#define round(x) ((int)(x+0.5))
#define MAXBUFBYTES 4096*4096	/* max bytes to malloc in buffer space */

unsigned char *buffer;
ssize_t scanlen;			/* length of infile scanlines */
ssize_t buflines;		/* Number of lines held in buffer */
off_t buf_start = -1000;	/* First line in buffer */

unsigned long xnum, ynum;	/* Number of pixels in new file */
float ulx, uly, urx, ury, lrx, lry, llx, lly;	/* Corners of original file */

FILE *ifp, *ofp;

static const char usage[] = "\
Usage: bwcrop in.bw out.bw (I prompt!)\n\
   or  bwcrop in.bw out.bw inwidth outwidth outheight\n\
	ulx uly urx ury lrx lry llx lly\n";

/*
 * Determine max number of lines to buffer.
 * and malloc space for it.
 * XXX - CHECK FILE SIZE
 */
void
init_buffer(int len)
{
    int max;

    /* See how many we could buffer */
    max = MAXBUFBYTES / len;

    /*
     * Do a max of 4096.  We really should see how big
     * the input file is to decide if we should buffer
     * less than our max.
     */
    if (max > 4096) max = 4096;

    buflines = max;
    buffer = (unsigned char *)bu_malloc(buflines * len, "buffer");
}


/*
 * Load the buffer with scan lines centered around
 * the given y coordinate.
 */
void
fill_buffer(int y)
{
    size_t ret;

    buf_start = y - buflines/2;
    if (buf_start < 0) buf_start = 0;

    bu_fseek(ifp, buf_start * scanlen, 0);
    ret = fread(buffer, scanlen, buflines, ifp);
    if (ret == 0)
	perror("fread");
}


int
main(int argc, char **argv)
{
    float x1, y1, x2, y2, x, y;
    size_t row, col;
    ssize_t yindex;
    char value;
    size_t ret;

    int atoival;

    if (argc < 3) {
	bu_exit(1, "%s", usage);
    }
    if ((ifp = fopen(argv[1], "r")) == NULL) {
	bu_exit(2, "bwcrop: can't open %s for reading\n", argv[1]);
    }
    if ((ofp = fopen(argv[2], "w")) == NULL) {
	bu_exit(3, "bwcrop: can't open %s for writing\n", argv[2]);
    }

    if (argc == 14) {
	if (argv[3])
	    scanlen = atoi(argv[3]);
	else
	    return 1;

	if (argv[4]) {
	    atoival = atoi(argv[4]);
	    if (atoival < 0)
		atoival = 0;
	    if (atoival > INT_MAX-1)
		atoival = INT_MAX-1;
	    xnum = atoival;
	} else {
	    return 1;
	}

	if (argv[5]) {
	    atoival = atoi(argv[5]);
	    if (atoival < 0)
		atoival = 0;
	    if (atoival > INT_MAX-1)
		atoival = INT_MAX-1;
	    ynum = atoival;
	} else {
	    return 1;
	}

	if (argv[6])
	    ulx = atoi(argv[6]);
	else
	    return 1;

	if (argv[7])
	    uly = atoi(argv[7]);
	else
	    return 1;

	if (argv[8])
	    urx = atoi(argv[8]);
	else
	    return 1;

	if (argv[9])
	    ury = atoi(argv[9]);
	else
	    return 1;

	if (argv[10])
	    lrx = atoi(argv[10]);
	else
	    return 1;

	if (argv[11])
	    lry = atoi(argv[11]);
	else
	    return 1;

	if (argv[12])
	    llx = atoi(argv[12]);
	else
	    return 1;

	if (argv[13])
	    lly = atoi(argv[13]);
	else
	    return 1;
    } else {
	double xval, yval;
	unsigned long len;
	/* Get info */
	printf("Scanline length in input file: ");
	ret = scanf("%lu", &len);
	if (ret != 1)
	    perror("scanf");
	scanlen = len;
	if (scanlen <= 0) {
	    bu_exit(4, "bwcrop: scanlen = %zu, don't be ridiculous\n", scanlen);
	}
	printf("Line Length and Number of scan lines (in new file)?: ");
	ret = scanf("%f%f", &xval, &yval);
	if (ret != 2) {
	    perror("scanf");
	}

	/* sanitize */
	if (xval < 1)
	    xval = 1;
	if (xval > INT_MAX-1)
	    xval = INT_MAX-1;
	xnum = xval;

	/* sanitize */
	if (yval < 1)
	    yval = 1;
	if (yval > INT_MAX-1)
	    yval = INT_MAX-1;
	ynum = yval;

	printf("Upper left corner in input file (x, y)?: ");
	ret = scanf("%f%f", &ulx, &uly);
	if (ret != 2)
	    perror("scanf");

	printf("Upper right corner (x, y)?: ");
	ret = scanf("%f%f", &urx, &ury);
	if (ret != 2)
	    perror("scanf");

	printf("Lower right (x, y)?: ");
	ret = scanf("%f%f", &lrx, &lry);
	if (ret != 2)
	    perror("scanf");

	printf("Lower left (x, y)?: ");
	ret = scanf("%f%f", &llx, &lly);
	if (ret != 2)
	    perror("scanf");
    }

    /* See how many lines we can buffer */
    init_buffer(scanlen);

    /* Check for silly buffer syndrome */
    if ((ssize_t)abs((int)(ury - uly)) > buflines/2 || (ssize_t)abs((int)(lry - lly)) > buflines/2) {
	fprintf(stderr, "bwcrop: Warning: You are skewing enough in the y direction\n");
	fprintf(stderr, "bwcrop: relative to my buffer size that I will exhibit silly\n");
	fprintf(stderr, "bwcrop: buffer syndrome (two replacements per scanline).\n");
	fprintf(stderr, "bwcrop: Recompile me or use a smarter algorithm.\n");
    }

    /* Move all points */
    for (row = 0; row < ynum; row++) {
	/* calculate left point of row */
	x1 = ((ulx-llx)/(fastf_t)(ynum-1)) * (fastf_t)row + llx;
	y1 = ((uly-lly)/(fastf_t)(ynum-1)) * (fastf_t)row + lly;
	/* calculate right point of row */
	x2 = ((urx-lrx)/(fastf_t)(ynum-1)) * (fastf_t)row + lrx;
	y2 = ((ury-lry)/(fastf_t)(ynum-1)) * (fastf_t)row + lry;

	for (col = 0; col < xnum; col++) {
	    /* calculate point along row */
	    x = ((x2-x1)/(fastf_t)(xnum-1)) * (fastf_t)col + x1;
	    y = ((y2-y1)/(fastf_t)(xnum-1)) * (fastf_t)col + y1;

	    /* Make sure we are in the buffer */
	    yindex = round(y) - buf_start;
	    if (yindex >= buflines) {
		fill_buffer(round(y));
		yindex = round(y) - buf_start;
	    }

	    value = buffer[ yindex * scanlen + round(x) ];
	    ret = fwrite(&value, sizeof(value), 1, ofp);
	    if (ret == 0)
		perror("fwrite");
	}
    }

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
