/*                      P I X E M B E D . C
 * BRL-CAD
 *
 * Copyright (c) 1992-2011 United States Government as represented by
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
/** @file util/pixembed.c
 *
 * Embed a smaller pix file in a larger space, replicating the boundary
 * pixels to fill out the borders, and output as a pix file.
 *
 */

#include "common.h"

#include <stdlib.h>
#include "bio.h"

#include "bu.h"


unsigned char *obuf;

int scanlen;			/* length of infile (and buffer) scanlines */

FILE *buffp;
static char *file_name;

size_t xin = 512;
size_t yin = 512;
size_t xout = 512;
size_t yout = 512;

size_t border_inset = 0;	/* Sometimes border pixels are bad */

void load_buffer(void), write_buffer(void);

static char usage[] = "\
Usage: pixembed [-h] [-b border_inset] \n\
	[-s squareinsize] [-w inwidth] [-n inheight]\n\
	[-S squareoutsize] [-W outwidth] [-N outheight] [in.pix] > out.pix\n";

/*
 * G E T _ A R G S
 */
int
get_args(int argc, char **argv)
{
    int c;

    while ((c = bu_getopt(argc, argv, "b:hs:w:n:S:W:N:")) != -1) {
	switch (c) {
	    case 'b':
		border_inset = atoi(bu_optarg);
		break;
	    case 'h':
		/* high-res */
		xin = yin = 1024;
		break;
	    case 'S':
		/* square size */
		xout = yout = atoi(bu_optarg);
		break;
	    case 's':
		/* square size */
		xin = yin = atoi(bu_optarg);
		break;
	    case 'W':
		xout = atoi(bu_optarg);
		break;
	    case 'w':
		xin = atoi(bu_optarg);
		break;
	    case 'N':
		yout = atoi(bu_optarg);
		break;
	    case 'n':
		yin = atoi(bu_optarg);
		break;

	    default:		/* '?' */
		return 0;
	}
    }

    if (bu_optind >= argc) {
	if (isatty(fileno(stdin)))
	    return 0;
	file_name = "-";
	buffp = stdin;
    } else {
	file_name = argv[bu_optind];
	if ((buffp = fopen(file_name, "r")) == NULL) {
	    (void)fprintf(stderr,
			  "pixembed: cannot open \"%s\" for reading\n",
			  file_name);
	    return 0;
	}
    }

    if (argc > ++bu_optind)
	(void)fprintf(stderr, "pixembed: excess argument(s) ignored\n");

    return 1;		/* OK */
}


/*
 * M A I N
 */
int
main(int argc, char **argv)
{
    size_t ydup;
    size_t i;
    size_t y;

    if (!get_args(argc, argv) || isatty(fileno(stdout))) {
	(void)fputs(usage, stderr);
	bu_exit (1, NULL);
    }

    if (xin <= 0 || yin <= 0 || xout <= 0 || yout <= 0) {
	fprintf(stderr, "pixembed: sizes must be positive\n");
	bu_exit (2, NULL);
    }
    if (xout < xin || yout < yin) {
	fprintf(stderr, "pixembed: output size must exceed input size\n");
	bu_exit (3, NULL);
    }

    if (border_inset >= xin) {
	fprintf(stderr, "pixembed: border inset out of range\n");
	bu_exit (4, NULL);
    }

    /* Allocate storage for one output line */
    scanlen = 3*xout;
    obuf = (unsigned char *)bu_malloc(scanlen, "obuf");

    /* Pre-fetch the first line (after skipping) */
    for (i= -1; i<border_inset; i++) load_buffer();

    /* Write out duplicates at bottom, including real copy of 1st line */
    ydup = (yout - yin) / 2 + border_inset + 1;
    for (y = 0; y < ydup; y++) write_buffer();

    /* Read and write the remaining lines */
    for (; i < yin-border_inset; i++, y++) {
	load_buffer();
	write_buffer();
    }

    /* Write out duplicates at the top, until all done */
    for (; y < yout; y++) write_buffer();

    bu_free(obuf, "obuf");

    return 0;
}


/*
 * L O A D _ B U F F E R
 *
 * Read one input scanline into the middle of the output scanline,
 * and duplicate the border pixels.
 */
void
load_buffer(void)
{
    size_t ret;
    unsigned char r, g, b;
    unsigned char *cp;
    size_t i;
    size_t inbase;

    inbase = (xout - xin) / 2;

    ret = fread(obuf + inbase*3, 3, xin, buffp);
    if (ret != xin) {
	perror("pixembed fread");
	bu_exit (7, NULL);
    }
    r = obuf[(inbase+border_inset)*3+0];
    g = obuf[(inbase+border_inset)*3+1];
    b = obuf[(inbase+border_inset)*3+2];

    cp = obuf;
    for (i=0; i<inbase+border_inset; i++) {
	*cp++ = r;
	*cp++ = g;
	*cp++ = b;
    }

    r = obuf[(xin+inbase-1-border_inset)*3+0];
    g = obuf[(xin+inbase-1-border_inset)*3+1];
    b = obuf[(xin+inbase-1-border_inset)*3+2];
    cp = &obuf[(xin+inbase+0-border_inset)*3+0];
    for (i=xin+inbase-border_inset; i<xout; i++) {
	*cp++ = r;
	*cp++ = g;
	*cp++ = b;
    }

}


/*
 * W R I T E _ B U F F E R
 *
 * Write the buffer to stdout, with error checking.
 */
void
write_buffer(void)
{
    if (fwrite(obuf, 3, xout, stdout) != xout) {
	perror("pixembed stdout fwrite");
	bu_exit (8, NULL);
    }
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
