/*                   P I X I N T E R P 2 X . C
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
 */
/** @file util/pixinterp2x.c
 *
 * Read a .pix file of a given resolution, and produce one with
 * twice as many pixels by interpolating between the pixels.
 *
 */

#include "common.h"

#include <stdlib.h>
#include "bio.h"

#include "bu.h"


FILE *infp;

size_t file_width = 512;
size_t file_height = 512;

size_t inbytes;			/* # bytes of one input scanline */
size_t outbytes;		/* # bytes of one output scanline */
size_t outsize;			/* size of output buffer */
unsigned char *outbuf;		/* ptr to output image buffer */
void widen_line(unsigned char *cp, int y);

void interp_lines(int out, int i1, int i2);

char usage[] = "\
Usage: pixinterp2x [-h] [-s squarefilesize]\n\
	[-w file_width] [-n file_height] [file.pix] > outfile.pix\n";

/*
 * G E T _ A R G S
 */
static int
get_args(int argc, char **argv)
{
    int c;

    while ((c = bu_getopt(argc, argv, "hs:w:n:")) != -1) {
	switch (c) {
	    case 'h':
		/* high-res */
		file_height = file_width = 1024;
		break;
	    case 's':
		/* square file size */
		file_height = file_width = atoi(bu_optarg);
		break;
	    case 'w':
		file_width = atoi(bu_optarg);
		break;
	    case 'n':
		file_height = atoi(bu_optarg);
		break;
	    case '?':
		return 0;
	}
    }
    if (argv[bu_optind] != NULL) {
	if ((infp = fopen(argv[bu_optind], "r")) == NULL) {
	    perror(argv[bu_optind]);
	    return 0;
	}
	bu_optind++;
    }
    if (argc > ++bu_optind)
	(void) fprintf(stderr, "Excess arguments ignored\n");

    if (isatty(fileno(infp)) || isatty(fileno(stdout)))
	return 0;
    return 1;
}


/*
 * M A I N
 */
int
main(int argc, char **argv)
{
    size_t iny, outy;
    unsigned char *inbuf;
    size_t ret;

    infp = stdin;
    if (!get_args(argc, argv)) {
	(void)fputs(usage, stderr);
	bu_exit (1, NULL);
    }

    inbytes = file_width * 3;	/* bytes/ input line */
    inbuf = (unsigned char *)malloc(inbytes);

    outbytes = inbytes * 2;		/* bytes/ output line */
    outsize = file_width * file_height * 4 * 3;
    if ((outbuf = (unsigned char *)malloc(outsize)) == (unsigned char *)0) {
	fprintf(stderr, "pixinterp2x:  unable to malloc buffer\n");
	bu_exit (1, NULL);
    }

    outy = -2;
    for (iny = 0; iny < file_height; iny++) {
	ret = fread((char *)inbuf, 1, inbytes, infp);
	if (ret != inbytes) {
	    fprintf(stderr, "pixinterp2x fread error\n");
	    break;
	}

	outy += 2;
	/* outy is line we will write on */
	widen_line(inbuf, outy);
	if (outy == 0)
	    widen_line(inbuf, ++outy);
	else
	    interp_lines(outy-1, outy, outy-2);
    }
    ret = write(1, (char *)outbuf, outsize);
    if (ret != outsize) {
	perror("pixinterp2x write");
	bu_exit (1, NULL);
    }

    return 0;
}


void
widen_line(unsigned char *cp, int y)
{
    unsigned char *op;
    size_t i;

    op = (unsigned char *)outbuf + (y * outbytes);
    /* Replicate first pixel */
    *op++ = cp[0];
    *op++ = cp[1];
    *op++ = cp[2];
    /* Prep process by copying first pixel */
    *op++ = *cp++;
    *op++ = *cp++;
    *op++ = *cp++;
    for (i=0; i<inbytes; i+=3) {
	/* Average previous pixel with current pixel */
	*op++ = ((int)cp[-3+0] + (int)cp[0])>>1;
	*op++ = ((int)cp[-3+1] + (int)cp[1])>>1;
	*op++ = ((int)cp[-3+2] + (int)cp[2])>>1;
	/* Copy pixel */
	*op++ = *cp++;
	*op++ = *cp++;
	*op++ = *cp++;
    }
}


void
interp_lines(int out, int i1, int i2)
{
    unsigned char *a, *b;	/* inputs */
    unsigned char *op;
    size_t i;

    a = (unsigned char *)outbuf + (i1 * outbytes);
    b = (unsigned char *)outbuf + (i2 * outbytes);
    op = (unsigned char *)outbuf + (out * outbytes);

    for (i=0; i<outbytes; i+=3) {
	*op++ = ((int)*a++ + (int)*b++)>>1;
	*op++ = ((int)*a++ + (int)*b++)>>1;
	*op++ = ((int)*a++ + (int)*b++)>>1;
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
