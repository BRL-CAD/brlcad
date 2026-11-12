/*                     P I X E L S W A P . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2026 United States Government as represented by
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

#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include "bio.h"

#include "bu/app.h"
#include "bu/getopt.h"
#include "util_arg.h"
#include "bu/exit.h"
#include "dm.h"


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
	    case 'd':
		if (!util_arg_int_range(bu_optarg, &depth, 1, INT_MAX, "pixel depth"))
		    usage("");

		break;

	    default:
		usage(""); break;
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
    int i;
    size_t bytes, pixel;
    unsigned char r, g, b, R, G, B;

    bu_setprogname(av[0]);

    setmode(fileno(stdin), O_BINARY);
    setmode(fileno(stdout), O_BINARY);

    i = parse_args(ac, av);
    /* if ac == 1, there is only 1 argument; i.e., run-with-no-arguments */
    if (ac == 1) usage("");

    if (i+6 > ac)
	usage("missing pixel value(s)\n");
    if (i+6 < ac)
	usage("excess pixel value(s)\n");

    if (isatty(fileno(stdout)) || isatty(fileno(stdin)))
	usage("Redirect standard input and output\n");

    /* get pixel values */
    if (!util_arg_uchar(av[i++], &r, "input red value") ||
	!util_arg_uchar(av[i++], &g, "input green value") ||
	!util_arg_uchar(av[i++], &b, "input blue value") ||
	!util_arg_uchar(av[i++], &R, "output red value") ||
	!util_arg_uchar(av[i++], &G, "output green value") ||
	!util_arg_uchar(av[i], &B, "output blue value")) {
	return 1;
    }

    /* process stdin */
    while ((bytes = fread(ibuf, 1, sizeof(ibuf), stdin)) > 0) {
	if (bytes % 3) {
	    fprintf(stderr, "%s: input ends with an incomplete pixel\n", progname);
	    return 1;
	}
	for (pixel = 0; pixel < bytes / 3; pixel++) {
	    if (ibuf[pixel*3] == r &&
		ibuf[pixel*3+1] == g &&
		ibuf[pixel*3+2] == b) {
		obuf[pixel*3] = R;
		obuf[pixel*3+1] = G;
		obuf[pixel*3+2] = B;
	    } else if (ibuf[pixel*3] == R &&
		       ibuf[pixel*3+1] == G &&
		       ibuf[pixel*3+2] == B) {
		obuf[pixel*3] = r;
		obuf[pixel*3+1] = g;
		obuf[pixel*3+2] = b;
	    } else {
		obuf[pixel*3] = ibuf[pixel*3];
		obuf[pixel*3+1] = ibuf[pixel*3+1];
		obuf[pixel*3+2] = ibuf[pixel*3+2];
	    }
	}

	if (fwrite(obuf, 1, bytes, stdout) != bytes) {
	    perror("pixelswap write");
	    return 1;
	}
    }
    if (ferror(stdin)) {
	perror("pixelswap read");
	return 1;
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
