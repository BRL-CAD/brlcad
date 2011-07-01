/*                     P I X E L S W A P . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2011 United States Government as represented by
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
/** @file util/pixelswap.c
 *
 * interchange pixel values in an image
 *
 */

#include "common.h"

#include <stdlib.h>
#include <string.h>
#include "bio.h"

#include "bu.h"
#include "fb.h"


char *options = "hd:";
char *progname = "(noname)";

int depth = 3;
unsigned char ibuf[32767 * 3];
unsigned char obuf[32767 * 3];
#define RGBCMP(a, b) ((a) == (b)[0] && \
		      (a)[1] == (b)[1] && \
		      (a)[2] == (b)[2])

/*
 * U S A G E --- tell user how to invoke this program, then exit
 */
void usage(char *s)
{
    if (s) (void)fputs(s, stderr);

    (void) fprintf(stderr, "Usage: %s [ -%s ] r g b R G B [ < infile > outfile]\n",
		   progname, options);
    bu_exit (1, NULL);
}


/*
 * P A R S E _ A R G S --- Parse through command line flags
 */
int parse_args(int ac, char **av)
{
    int c;

    if (! (progname=strrchr(*av, '/')))
	progname = *av;
    else
	++progname;

    /* Turn off bu_getopt's error messages */
    bu_opterr = 0;

    /* get all the option flags from the command line */
    while ((c=bu_getopt(ac, av, options)) != -1)
	switch (c) {
	    case 'd'	: if ((c=atoi(bu_optarg)) > 0)
		depth = c;
	    else
		fprintf(stderr, "bad # of bytes per pixel (%d)\n", c);
		break;
	    case '?'	:
	    case 'h'	:
	    default		: usage("Bad or help flag specified\n"); break;
	}

    return bu_optind;
}
/*
 * M A I N
 *
 * Call parse_args to handle command line arguments first, then
 * process input.
 */
int main(int ac, char **av)
{
    int i, pixels;
    unsigned char r, g, b, R, G, B;
    size_t ret;

    if ((i=parse_args(ac, av))+6 > ac)
	usage("missing pixel value(s)\n");

    if (isatty(fileno(stdout)) || isatty(fileno(stdin)))
	usage("Redirect standard output\n");

    /* get pixel values */
    r = atoi(av[i++]);
    g = atoi(av[i++]);
    b = atoi(av[i++]);

    R = atoi(av[i++]);
    G = atoi(av[i++]);
    B = atoi(av[i]);

    /* process stdin */
    while ((pixels = fread(ibuf, 3, sizeof(ibuf)/3, stdin)) > 0) {
	for (i = 0; i < pixels; i++) {
	    if (ibuf[i*3] == r &&
		ibuf[i*3+1] == g &&
		ibuf[i*3+2] == b) {
		obuf[i*3] = R;
		obuf[i*3+1] = G;
		obuf[i*3+2] = B;
	    } else if (ibuf[i*3] == R &&
		       ibuf[i*3+1] == G &&
		       ibuf[i*3+2] == B) {
		obuf[i*3] = r;
		obuf[i*3+1] = g;
		obuf[i*3+2] = b;
	    } else {
		obuf[i*3] = ibuf[i*3];
		obuf[i*3+1] = ibuf[i*3+1];
		obuf[i*3+2] = ibuf[i*3+2];
	    }
	}

	ret = fwrite(obuf, 3, pixels, stdout);
	if (ret != (size_t)pixels)
	    perror("fwrite");
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
