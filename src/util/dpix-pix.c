/*                      D P I X - P I X . C
 * BRL-CAD
 *
 * Copyright (c) 1990-2014 United States Government as represented by
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
/** @file util/dpix-pix.c
 *
 * Convert double precision images in .dpix form to a .pix file.  By
 * default, will determine min/max values to drive exposure
 * calculations, and perform linear interpolation on the way to 1-byte
 * values.
 *
 * Reads the binary input file, finds the minimum and maximum values
 * read, and linearly interpolates these values between 0 and 255.
 *
 */

#include "common.h"

#include <stdlib.h>
#include "bio.h"

#include "vmath.h"
#include "bu.h"


#define NUM (1024 * 16)	/* Note the powers of 2 -- v. efficient */


int
main(int argc, char **argv)
{
    double doub[NUM];
    unsigned char cha[NUM];

    size_t count;			/* count of items */
    ssize_t got;			/* count of bytes */
    int fd;			/* UNIX file descriptor */
    double *dp;			/* ptr to d */
    double *ep;
    double m;			/* slope */
    double b;			/* intercept */
    char *ifname;

    double min, max;		/* high usage items */

    if (argc < 2) {
	bu_exit(1, "Usage: dpix-pix file.dpix > file.pix\n");
    }

    ifname = bu_realpath(argv[1], NULL);
    if ((fd = open(ifname, 0)) < 0) {
	perror(ifname);
	bu_free(ifname, "ifname alloc from bu_realpath");
	exit(1);
    }
    bu_free(ifname, "ifname alloc from bu_realpath");

    if (isatty(fileno(stdout))) {
	bu_exit(2, "dpix-pix:  binary output directed to terminal, aborting\n");
    }

    min = INFINITY;
    max = -INFINITY;

    while (1) {
	got = read(fd, (char *)&doub[0], NUM*sizeof(doub[0]));
	if (got <= 0) {
	    if (got < 0) {
		perror("dpix-pix READ ERROR");
	    }
	    break;
	}
	count = got / sizeof(doub[0]);
	ep = &doub[count];
	for (dp = &doub[0]; dp < ep;) {
	    double val = *dp++;

	    V_MIN(min, val);
	    V_MAX(max, val);
	}
    }

    lseek(fd, 0, 0);		/* rewind(fp); */

    /* This section uses the maximum and the minimum values found to
     * compute the m and the b of the line as specified by the
     * equation y = mx + b.
     */
    fprintf(stderr, "min=%f, max=%f\n", min, max);
    if (max < min) {
	bu_exit(1, "MINMAX: max less than min!\n");
    }

    m = (255 - 0)/(max - min);
    b = (-255 * min)/(max - min);

    while (1) {
	char *cp;		/* ptr to c */
	double mm;		/* slope */
	double bb;		/* intercept */

	mm = m;
	bb = b;

	got = read(fd, (char *)&doub[0], NUM*sizeof(doub[0]));
	if (got <=  0) {
	    if (got < 0) {
		perror("dpix-pix READ ERROR");
	    }
	    break;
	}
	count = got / sizeof(doub[0]);
	ep = &doub[count];
	cp = (char *)&cha[0];
	for (dp = &doub[0]; dp < ep;) {
	    *cp++ = mm * (*dp++) + bb;
	}

	/* fd 1 is stdout */
	got = write(1, (char *)&cha[0], count*sizeof(cha[0]));
	if (got < 0 || (size_t)got != count*sizeof(cha[0])) {
	    perror("write");
	    exit(2);
	}
    }

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
