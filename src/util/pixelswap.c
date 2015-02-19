/*                     P I X E L S W A P . C
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
/** @file util/pixelswap.c
 *
 * interchange pixel values in an image
 *
 */

#include "common.h"

#include <stdlib.h>
#include <string.h>

#include "bu/getopt.h"
#include "bu/log.h"
#include "fb.h"


char options[] = "d:h?";
char noname[] = "(noname)";
char *progname = noname;

int depth = 3;
unsigned char ibuf[32767 * 3];
unsigned char obuf[32767 * 3];
#define RGBCMP(a, b) ((a) == (b)[0] && \
		      (a)[1] == (b)[1] && \
		      (a)[2] == (b)[2])


void
usage(const char *s)
{
    if (s) (void)fputs(s, stderr);

    (void) fprintf(stderr, "Usage: %s [ -d  pixeldepth ] r g b R G B < infile > outfile\n",
		   progname);
    bu_exit (1, NULL);
}


int
parse_args(int ac, char **av)
{
    int c;

    if (! (progname=strrchr(*av, '/')))
	progname = *av;
    else
	++progname;

    /* get all the option flags from the command line */
    while ((c=bu_getopt(ac, av, options)) != -1) {
	switch (c) {
	    case 'd'	: if ((c=atoi(bu_optarg)) > 0)
		depth = c;
	    else
		fprintf(stderr, "bad # of bytes per pixel (%d)\n", c);
		break;
	    default	: usage(""); break;
	}
    }

    return bu_optind;
}
/*
 * Call parse_args to handle command line arguments first, then
 * process input.
 */
int main(int ac, char **av)
{
    int i, pixels;
    unsigned char r, g, b, R, G, B;
    size_t ret;

    i=parse_args(ac, av);
/* if ac == 1, there is only 1 argument; i.e., run-with-no-arguments
 */
    if (ac == 1) usage("");

    if (i+6 > ac)
	usage("missing pixel value(s)\n");

    if (isatty(fileno(stdout)) || isatty(fileno(stdin)))
	usage("Redirect standard input and output\n");

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
