/*                          L O W P . C
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
/** @file util/lowp.c
 *
 * Read 3 .pix files, and do a 3D low-pass filter on them.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <string.h>
#include "bio.h"

#include "bu/log.h"
#include "bu/file.h"
#include "bu/malloc.h"


#define MAX_LINE 10000		/* Max pixels/line */
static int scanbytes;			/* # of bytes of scanline */

unsigned char *in1, *in2, *in3;

/* Output line */
unsigned char out1[MAX_LINE*3] = {0};

static int nlines = 512;		/* Number of input lines */
static int pix_line;		/* Number of pixels/line */
int readval = 0;

char usage[] = "Usage: lowp file1.pix file2.pix file3.pix [width] > out.pix\n";

int infd1, infd2, infd3;		/* input fd's */

static void
check_readval(unsigned char *in, int infd)
{
    readval = read(infd, in, scanbytes);
    if (readval != scanbytes) {
	if (readval < 0) {
	    perror("lowp READ ERROR");
	}
	bu_exit (0, NULL);
    }
}
int
main(int argc, char **argv)
{
    int x, y;
    ssize_t ret;
    char *ifname;

    if (argc < 2) {
	fprintf(stderr, "%s", usage);
	bu_exit (1, NULL);
    }

    ifname = bu_realpath(argv[1], NULL);
    if ((infd1 = open(ifname, 0)) < 0) {
	perror(ifname);
	bu_free(ifname, "ifname alloc from bu_realpath");
	bu_exit (3, NULL);
    }
    bu_free(ifname, "ifname alloc from bu_realpath");

    ifname = bu_realpath(argv[2], NULL);
    if ((infd2 = open(ifname, 0)) < 0) {
	perror(ifname);
	bu_free(ifname, "ifname alloc from bu_realpath");
	bu_exit (3, NULL);
    }
    bu_free(ifname, "ifname alloc from bu_realpath");

    ifname = bu_realpath(argv[3], NULL);
    if ((infd3 = open(ifname, 0)) < 0) {
	perror(ifname);
	bu_free(ifname, "ifname alloc from bu_realpath");
	bu_exit (3, NULL);
    }
    bu_free(ifname, "ifname alloc from bu_realpath");

    if (argc == 5) {
	nlines = atoi(argv[4]);
    }

    pix_line = nlines;	/* Square pictures */
    scanbytes = nlines * pix_line * 3;
    in1 = (unsigned char *) bu_malloc(scanbytes, "lowp in1");
    in2 = (unsigned char *) bu_malloc(scanbytes, "lowp in2");
    in3 = (unsigned char *) bu_malloc(scanbytes, "lowp in3");

    check_readval(in1, infd1);
    check_readval(in2, infd2);
    check_readval(in3, infd3);

    /* First and last are black */
    memset(out1, 0, pix_line*3);
    ret = write(1, out1, pix_line*3);
    if (ret < 0)
	perror("write");

    for (y=1; y < nlines-2; y++) {
	static unsigned char *op;

	op = out1+3;

	/* do (width-2)*3 times, borders are black */
	for (x=3; x < (pix_line-2)*3; x++) {
	    int i;
	    unsigned char *a, *b, *c;

	    i = (y*pix_line)*3+x;
	    a = in1+i;
	    b = in2+i;
	    c = in3+i;
	    i = pix_line*3;
	    *op++ = ((int)
		     a[-i-3]    + a[-i  ]*3  + a[-i+3]    +
		     a[  -3]*3  + a[   0]*5  + a[   3]*3  +
		     a[ i-3]    + a[ i  ]*3  + a[ i+3]    +

		     b[-i-3]*3  + b[-i  ]*5  + b[-i+3]*3  +
		     b[  -3]*5  + b[   0]*10 + b[   3]*5  +
		     b[ i-3]*3  + b[ i  ]*5  + b[ i+3]*3  +

		     c[-i-3]    + c[-i  ]*3  + c[-i+3]    +
		     c[  -3]*3  + c[   0]*5  + c[   3]*3  +
		     c[ i-3]    + c[ i  ]*3  + c[ i+3]
		) / 84;
	}
	ret = write(1, out1, pix_line*3);
	if (ret < 0)
	    perror("write");
    }

    /* First and last are black */
    memset(out1, 0, pix_line*3);
    ret = write(1, out1, pix_line*3);
    if (ret < 0)
	perror("write");

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
