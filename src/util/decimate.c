/*                      D E C I M A T E . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2022 United States Government as represented by
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
/** @file util/decimate.c
 *
 * Program to take M by N array of tuples, and reduce it down to
 * being an I by J array of tuples, by discarding unnecessary stuff.
 * The borders are padded with zeros.
 *
 * Especially useful for looking at image files which are larger than
 * your biggest framebuffer, quickly.
 *
 * Specify nbytes/pixel as:
 * 1 .bw files
 * 3 .pix files
 *
 */

#include "common.h"

#include "bio.h" /* for setmode */

#include <stdlib.h>
#include <limits.h> /* for INT_MAX */

#include "bu/app.h"
#include "bu/log.h"
#include "bu/malloc.h"

static char usage[] = "\
Usage: decimate nbytes/pixel width height [outwidth outheight]\n\
";

size_t nbytes;
size_t iwidth;
size_t iheight;
size_t owidth = 512;
size_t oheight = 512;

unsigned char *iline;
unsigned char *oline;

size_t discard;
size_t wpad;

int
main(int argc, char **argv)
{
    size_t i = 0;
    size_t j = 0;
    size_t nh = 0;
    size_t nw = 0;
    size_t dh = 0;
    size_t dw = 0;
    size_t todo = 0;
    int failure = 0;

    bu_setprogname(argv[0]);

    setmode(fileno(stdin), O_BINARY);
    setmode(fileno(stdout), O_BINARY);

    if (argc < 4) {
	bu_exit (1, "%s", usage);
    }

    nbytes = atoi(argv[1]);
    iwidth = atoi(argv[2]);
    iheight = atoi(argv[3]);

    if (argc >= 6) {
	owidth = atoi(argv[4]);
	oheight = atoi(argv[5]);
    }

    if (nbytes <= 0 || nbytes > INT_MAX) {
	failure = 1;
	bu_log("decimate: bad nbytes/pixel: %ld\n", (long int)nbytes);
    }
    if (iwidth <= 0 || iwidth > INT_MAX || iheight <= 0 || iheight > INT_MAX) {
	failure = 1;
	bu_log("decimate: bad size of input range: %ldx%ld\n", (long int)iwidth, (long int)iheight);
    }
    if (owidth <= 0 || owidth > INT_MAX || oheight <= 0 || oheight > INT_MAX) {
	failure = 1;
	bu_log("decimate: bad size of output range: %ldx%ld\n", (long int)owidth, (long int)oheight);
    }
    if (failure) {
	bu_exit(EXIT_FAILURE, "%s", usage);
    }

    /* Determine how many samples/lines to discard after each one saved,
     * and how much padding to add to the end of each line.
     */
    nh = (iheight + oheight-1) / oheight;
    nw = (iwidth + owidth-1) / owidth;

    dh = nh - 1;
    dw = nw - 1;
    discard = dh;
    if (dw > discard) discard = dw;

    wpad = owidth - (iwidth / (discard+1));

    iline = (unsigned char *)bu_calloc((iwidth+1), nbytes, "iline");
    oline = (unsigned char *)bu_calloc((owidth+1),  nbytes, "oline");

    todo = iwidth / (discard+1) * (discard+1);
    if (owidth < todo) todo = owidth;
    if (todo > iwidth/(discard+1)) todo = iwidth/(discard+1);

    while (!feof(stdin)) {
	size_t ret;
	unsigned char *ip, *op;

	/* Scrunch down first scanline of input data */
	ret = fread(iline, nbytes, iwidth, stdin);
	if (ret != iwidth)
	    break;
	ip = iline;
	op = oline;
	for (i=0; i < todo; i++) {
	    for (j=0; j < nbytes; j++) {
		*op++ = *ip++;
	    }
	    ip += discard * nbytes;
	}
	if (fwrite(oline, nbytes, owidth, stdout) != owidth) {
	    perror("fwrite");
	    goto out;
	}

	/* Discard extra scanlines of input data */
	for (i=0; i < discard; i++) {
	    if (fread(iline, nbytes, iwidth, stdin) != iwidth) {
		goto out;
	    }
	}
    }

out:
    bu_free(iline, "iline");
    bu_free(oline, "oline");

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
