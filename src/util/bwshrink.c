/*                      B W S H R I N K . C
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
/** @file util/bwshrink.c
 *
 * scale down a picture by a uniform factor.
 *
 * Options
 * h help
 *
 */

#include "common.h"

#include <stdlib.h>
#include <string.h>
#include "bio.h"

#include "bu.h"


/* declarations to support use of bu_getopt() system call */
char *options = "uhs:w:n:f:";

char *progname = "(noname)";
char *filename = "(stdin)";

/* S H R I N K _ I M A G E
 */
void
shrink_image(int w, int h, unsigned char *buffer, int Factor)
{
    unsigned char *finalpixel;	/* output pixel pointer */
    unsigned int p;		/* pixel sum/average */
    int facsq, x, y, px, py;

    facsq = Factor * Factor;
    finalpixel = buffer;

    for (y=0; y < h; y += Factor) {
	for (x=0; x < w; x += Factor) {

	    /* average factor by factor grid of pixels */

	    p = 0;
	    for (py = 0; py < Factor; py++) {

		/* add pixels from scanline to average */
		for (px = 0; px < Factor; px++) {
		    p += buffer[y*w + x+px];
		}
	    }

	    /* store resultant pixel back in buffer */
	    *finalpixel++ = p / facsq;
	}
    }
}


/*
 * Undersample image pixels
 */
void
usample_image(int w, int h, unsigned char *buffer, int Factor)
{
    int x, y;
    unsigned char *p;

    p = buffer;

    for (y=0; y < h; y += Factor)
	for (x=0; x < w; x += Factor, p++) {
	    p[0] = buffer[x + y * w];
	}
}


int width = 512;
int height = 512;
int factor = 2;

#define METH_BOXCAR 1
#define METH_UNDERSAMPLE 2
int method = METH_BOXCAR;

/*
 * U S A G E --- tell user how to invoke this program, then exit
 */
void usage(void)
{
    (void) fprintf(stderr,
		   "Usage: %s [-u] [-h] [-w width] [-n scanlines] [-s squaresize]\n\
[-f shrink_factor] [pixfile] > pixfile\n", progname);
    bu_exit (1, NULL);
}


/*
 * P A R S E _ A R G S --- Parse through command line flags
 */
void parse_args(int ac, char **av)
{
    int c;

    if (!(progname = strrchr(*av, '/')))
	progname = *av;

    /* Turn off bu_getopt's error messages */
    bu_opterr = 0;

    /* get all the option flags from the command line */
    while ((c=bu_getopt(ac, av, options)) != -1)
	switch (c) {
	    case 'f'	: if ((c = atoi(bu_optarg)) > 1)
		factor = c;
		break;
	    case 'h'	: width = height = 1024; break;
	    case 'n'	: if ((c=atoi(bu_optarg)) > 0)
		height = c;
		break;
	    case 'w'	: if ((c=atoi(bu_optarg)) > 0)
		width = c;
		break;
	    case 's'	: if ((c=atoi(bu_optarg)) > 0)
		height = width = c;
		break;
	    case 'u'	: method = METH_UNDERSAMPLE; break;
	    case '?'	:
	    default		: usage(); break;
	}

    if (bu_optind >= ac) {
	if (isatty(fileno(stdout)))
	    usage();
    }
    if (bu_optind < ac) {
	if (freopen(av[bu_optind], "r", stdin) == (FILE *)NULL) {
	    perror(av[bu_optind]);
	    bu_exit (-1, NULL);
	} else
	    filename = av[bu_optind];
    }
    if (bu_optind+1 < ac)
	(void)fprintf(stderr, "%s: Excess arguments ignored\n", progname);

}


/*
 * M A I N
 *
 * Call parse_args to handle command line arguments first, then
 * process input.
 */
int main(int ac, char **av)
{
    unsigned char *buffer = (unsigned char *)NULL;
    int size;
    int c = 0;
    int t;

    (void)parse_args(ac, av);
    if (isatty(fileno(stdin))) usage();

    /* process stdin */

    /* get buffer for image */
    size = width * height;
    if ((buffer = (unsigned char *)malloc(width*height)) == (unsigned char *)NULL) {
	(void)fprintf(stderr, "%s: cannot allocate input buffer\n",
		      progname);
	bu_exit (-1, NULL);
    }

    /* read in entire image */
    for (t=0; t < size && (c=read(0, (char *)&buffer[t], size-t)) >= 0; t += c) {
	/* do nothing */;
    }

    if (c < 0) {
	perror (filename);
	return -1;
    }

    switch (method) {
	case METH_BOXCAR : shrink_image(width, height, buffer, factor); break;
	case METH_UNDERSAMPLE : usample_image(width, height, buffer, factor);
	    break;
	default: return -1;
    }

    for (t=0; t < size && (c=write(1, (char *)&buffer[t], size-t)) >= 0;
	 t += c);

    if (c < 0) {
	perror("stdout");
	return -1;
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
