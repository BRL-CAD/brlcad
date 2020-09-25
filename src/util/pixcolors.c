/*                     P I X C O L O R S . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2020 United States Government as represented by
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
/** @file util/pixcolors.c
 *
 * Count the number of different pixel values in a PIX format image.
 * If the "-v" option is selected, list each unique pixel value to the
 * standard output.
 *
 * Options
 * v list colors
 *
 */

#include "common.h"

#include <stdlib.h>
#include <limits.h>
#include "bio.h"

#include "bu/app.h"
#include "bu/color.h"
#include "bu/getopt.h"
#include "bu/exit.h"


/* declarations to support use of bu_getopt() system call */
char options[] = "v";
char verbose = 0;
char noname[] = "(noname)";
char *progname = noname;

#define PIXELS 1024
unsigned char pixbuf[BUFSIZ*3];

/* This grotesque array provides 1 bit for each of the 2^24 possible pixel
 * values.
 * The "NOBASE" comment below allows compilation on the Gould 9000 series
 * of computers.
 */
/*NOBASE*/
unsigned char vals[1L << (24-3)];


/*
 * Main function of program
 */
void
doit(FILE *fd)
{
    unsigned long pixel, count;
    int bytes;
    int mask, i;
    unsigned long k;
    int r, g, b;


    count = 0;
    while ((bytes = fread(pixbuf, 3, PIXELS, fd)) > 0) {
	for (i = (bytes-1)*3; i >= 0; i -= 3) {
	    r = pixbuf[i];
	    g = pixbuf[i+1];
	    b = pixbuf[i+2];
	    if (r < 0)
		r = 0;
	    else if (r > UCHAR_MAX)
		r = UCHAR_MAX;
	    if (g < 0)
		g = 0;
	    else if (g > UCHAR_MAX)
		g = UCHAR_MAX;
	    if (b < 0)
		b = 0;
	    else if (b > UCHAR_MAX)
		b = UCHAR_MAX;
	    pixel = r +	(g << 8) + (b << 16);

	    if (!(vals[k=(pixel >> 3)] &
		  (mask = (1 << (pixel & 0x07))))) {
		vals[k] |= (unsigned char)mask;
		++count;
	    }
	}
    }
    printf("%lu\n", (long unsigned)count);
    if (verbose)
	for (i = 0; i < 1<<24; ++i)
	    if ((vals[i>>3] & (1<<(i & 0x07))))
		printf("%3d %3d %3d\n",
		       i & 0x0ff,
		       (i >> 8) & 0x0ff,
		       (i >> 16) & 0x0ff);
}


void usage(void)
{
    fprintf(stderr, "Usage: %s [ -v ] < PIXfile\n", progname);
    bu_exit (1, NULL);
}


/*
 * Perform miscellaneous tasks such as argument parsing and
 * I/O setup and then call "doit" to perform the task at hand
 */
int main(int ac, char **av)
{
    int c, isatty(int);

    bu_setprogname(av[0]);

    progname = *av;

    setmode(fileno(stdin), O_BINARY);
    setmode(fileno(stdout), O_BINARY);

    /* Get # of options & turn all the option flags off
     */

    /* Turn off bu_getopt's error messages */
    bu_opterr = 0;

    /* get all the option flags from the command line
     */
    while ((c=bu_getopt(ac, av, options)) != -1) {
	if (c != 'v')
	    usage();
	verbose = ! verbose;
    }


    if (bu_optind < ac-1)
	usage();
    if (bu_optind == ac-1) {
	FILE *fd;
	if ((fd=fopen(av[bu_optind], "rb")) == (FILE *)NULL) {
	    perror(av[bu_optind]);
	    bu_exit (1, NULL);
	}
	doit(fd);
    }
/* (bu_optind >= ac) if arriving here */
    else {
	if (isatty(fileno(stdin)))
	    usage();
	doit(stdin);
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
