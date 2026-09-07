/*                      D P I X - P I X . C
 * BRL-CAD
 *
 * Copyright (c) 1990-2026 United States Government as represented by
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
#include "bu/app.h"
#include "bu/cv.h"
#include "bu/file.h"
#include "bu/log.h"
#include "bu/malloc.h"
#include "bu/exit.h"


#define NUM (1024 * 16)	/* Note the powers of 2 -- v. efficient */


static ssize_t
read_dpix(int fd, unsigned char *encoded, double *decoded)
{
    const size_t encoded_size = NUM * SIZEOF_NETWORK_DOUBLE;
    long int bytes_read;
    size_t count;

    bytes_read = bu_mread(fd, encoded, (long int)encoded_size);
    if (bytes_read <= 0)
	return (ssize_t)bytes_read;
    if ((size_t)bytes_read % SIZEOF_NETWORK_DOUBLE != 0) {
	bu_log("dpix-pix: incomplete double at end of input\n");
	return -1;
    }

    count = (size_t)bytes_read / SIZEOF_NETWORK_DOUBLE;
    bu_cv_ntohd((unsigned char *)decoded, encoded, count);

    return (ssize_t)count;
}


int
main(int argc, char **argv)
{
    double doub[NUM];
    unsigned char encoded[NUM * SIZEOF_NETWORK_DOUBLE];
    unsigned char cha[NUM];

    size_t count;			/* count of items */
    ssize_t got;			/* count of items */
    int fd;			/* UNIX file descriptor */
    double *dp;			/* ptr to d */
    double *ep;
    double m;			/* slope */
    double b;			/* intercept */
    char *ifname;

    double min, max;		/* high usage items */

    bu_setprogname(argv[0]);

    if (argc < 2) {
	bu_exit(1, "Usage: dpix-pix file.dpix > file.pix\n");
    }

    setmode(fileno(stdin), O_BINARY);
    setmode(fileno(stdout), O_BINARY);

    ifname = bu_file_realpath(argv[1], NULL);
    if ((fd = open(ifname, O_RDONLY|O_BINARY)) < 0) {
	perror(ifname);
	bu_free(ifname, "ifname alloc from bu_file_realpath");
	exit(1);
    }
    bu_free(ifname, "ifname alloc from bu_file_realpath");

    if (isatty(fileno(stdout))) {
	bu_exit(2, "dpix-pix:  binary output directed to terminal, aborting\n");
    }

    min = INFINITY;
    max = -INFINITY;

    while (1) {
	got = read_dpix(fd, encoded, doub);
	if (got <= 0) {
	    if (got < 0)
		bu_exit(1, "dpix-pix: error reading DPIX input\n");
	    break;
	}
	count = (size_t)got;
	ep = &doub[count];
	for (dp = &doub[0]; dp < ep;) {
	    double val = *dp++;

	    V_MIN(min, val);
	    V_MAX(max, val);
	}
    }

    bu_lseek(fd, 0, 0);		/* rewind(fp); */

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

	got = read_dpix(fd, encoded, doub);
	if (got <=  0) {
	    if (got < 0)
		bu_exit(1, "dpix-pix: error reading DPIX input\n");
	    break;
	}
	count = (size_t)got;
	ep = &doub[count];
	cp = (char *)&cha[0];
	for (dp = &doub[0]; dp < ep;) {
	    *cp++ = mm * (*dp++) + bb;
	}

	got = write(fileno(stdout), (char *)&cha[0], count*sizeof(cha[0]));
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
