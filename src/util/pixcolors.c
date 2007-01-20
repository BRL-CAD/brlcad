/*                     P I X C O L O R S . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2007 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this file; see the file named COPYING for more
 * information.
 */
/** @file pixcolors.c
 *	Count the number of different pixel values in a PIX format image.
 *	If the "-v" option is selected, list each unique pixel value
 *	to the standard output.
 *
 *	Author(s)
 *	Lee A. Butler	butler@stsci.edu
 *
 *	Options
 *	v	list colors
 */
#include "common.h"

#ifdef HAVE_UNISTD_H
#  include <unistd.h>
#endif
#include <stdio.h>
#include <stdlib.h>

#include "machine.h"
#include "bu.h"


/* declarations to support use of bu_getopt() system call */
char *options = "v";
char verbose = 0;
char *progname = "(noname)";

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
 *	D O I T --- Main function of program
 */
void doit(FILE *fd)
{
	unsigned long pixel, count;
	int bytes;
	register int mask, i;
	register unsigned long k;


	count = 0;
	while ((bytes=fread(pixbuf, 3, PIXELS, fd)) > 0) {
		for (i=(bytes-1)*3 ; i >= 0 ; i -= 3) {
			pixel = pixbuf[i] +
				(pixbuf[i+1] << 8) +
				(pixbuf[i+2] << 16);

			if ( ! ( vals[k=(pixel >> 3)] &
			    (mask=(1 << (pixel & 0x07))) ) ) {
				vals[k] |= (unsigned char)mask;
				++count;
			}
		}
	}
	(void) printf("%lu\n", count);
	if (verbose)
		for (i=0 ; i < 1<<24 ; ++i)
			if ( (vals[i>>3] & (1<<(i & 0x07))) )
				(void) printf("%3d %3d %3d\n",
					i & 0x0ff,
					(i >> 8) & 0x0ff,
					(i >> 16) & 0x0ff);
}

void usage(void)
{
	(void) fprintf(stderr, "Usage: %s [ -v ] < PIXfile\n", progname);
	exit(1);
}

/*
 *	M A I N
 *
 *	Perform miscelaneous tasks such as argument parsing and
 *	I/O setup and then call "doit" to perform the task at hand
 */
int main(int ac, char **av)
{
	int  c, isatty(int);
	progname = *av;

	/* Get # of options & turn all the option flags off
	 */

	/* Turn off bu_getopt's error messages */
	opterr = 0;

	/* get all the option flags from the command line
	 */
	while ((c=bu_getopt(ac,av,options)) != EOF) {
		if ( c == 'v' ) verbose = ! verbose;
		else usage();
	}


	if (bu_optind < ac-1) {
		usage();
	} else if (bu_optind == ac-1 ) {
		FILE *fd;
		if ((fd=fopen(av[bu_optind], "r")) == (FILE *)NULL) {
			perror(av[bu_optind]);
			exit(-1);
		} else doit(fd);
	} else if (bu_optind >= ac) {
		if (isatty(fileno(stdin))) usage();
		doit(stdin);
	}

	return(0);
}

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
