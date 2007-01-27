/*                     P I X S H R I N K . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2007 United States Government as represented by
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
/** @file pixshrink.c
 *
 *	scale down a picture by a uniform factor.
 *
 *	Options
 *	h	help
 *
 *  Author -
 *	Lee A. Butler
 *
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 */
#ifndef lint
static const char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "common.h"

#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif

#include <stdlib.h>
#include <stdio.h>
#ifdef HAVE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif

#include "machine.h"
#include "bu.h"


#define UCHAR unsigned char

/* declarations to support use of bu_getopt() system call */
char *options = "uhs:w:n:f:";

char *progname = "(noname)";
char *filename = "(stdin)";

void shrink_image(int scanlen, int Width, int Height, unsigned char *buffer, int Factor), usample_image(int scanlen, int Width, int Height, unsigned char *buffer, int Factor);

/*	R E A D _ I M A G E
 *
 *	read image into memory
 */
UCHAR *read_image(int scanlen, int Width, int Height, unsigned char *buffer)
{
	int total_bytes, in_bytes;
	int	count = 0;

	if (!buffer &&
	    (buffer=(UCHAR *)malloc(scanlen * Height)) == (UCHAR *)NULL) {
		(void)fprintf(stderr, "%s: cannot allocate input buffer\n",
			progname);
		exit(-1);
	}

	total_bytes = Width * Height * 3;
	in_bytes = 0;
	while (in_bytes < total_bytes &&
	    (count=read(0, (char *)&buffer[in_bytes], (unsigned)(total_bytes-in_bytes))) >= 0)
		in_bytes += count;

	if (count < 0) {
		perror(filename);
		exit(-1);
	}

	return(buffer);
}

/*	W R I T E _ I M A G E
 */
void write_image(int Width, int Height, unsigned char *buffer)
{
	int	count = 0;
	int	out_bytes, total_bytes;

	total_bytes = Width * Height * 3;
	out_bytes = 0;

	while (out_bytes < total_bytes &&
	    (count=write(1, (char *)&buffer[out_bytes], total_bytes-out_bytes)) >= 0)
		out_bytes += count;

	if (count < 0) {
		perror("stdout");
		exit(-1);
	}
}

/*	S H R I N K _ I M A G E
 */
void
shrink_image(int scanlen, int Width, int Height, unsigned char *buffer, int Factor)
{
	UCHAR *pixelp, *finalpixel;
	unsigned int p[3];
	int facsq, x, y, px, py;

	facsq = Factor * Factor;
	finalpixel = buffer;

	for (y=0 ; y < Height ; y += Factor)
		for (x=0 ; x < Width ; x += Factor) {

			/* average factor by factor pixels */

			for (py = 0 ; py < 3 ; ++py)
				p[py] = 0;

			for (py = 0 ; py < Factor ; py++) {

				/* get first pixel in scanline */
				pixelp = &buffer[y*scanlen+x*3];

				/* add pixels from scanline to average */
				for (px = 0 ; px < Factor ; px++) {
					p[0] += *pixelp++;
					p[1] += *pixelp++;
					p[2] += *pixelp++;
				}
			}

			/* store resultant pixel back in buffer */
			for (py = 0 ; py < 3 ; ++py)
				*finalpixel++ = p[py] / facsq;
		}
}

/*
 *	Undersample image pixels
 */
void
usample_image(int scanlen, int Width, int Height, unsigned char *buffer, int Factor)
{
	register int t, x, y;

	UCHAR *p;

	p = buffer;

	for (y=0 ; y < Height ; y += Factor)
		for (x=0 ; x < Width ; x += Factor, p += 3) {
			t = x*3 + y*scanlen;
			p[0] = buffer[t];
			p[1] = buffer[t+1];
			p[2] = buffer[t+2];
		}
}


int width = 512;
int height = 512;
int scanlen;
int factor = 2;

#define METH_BOXCAR 1
#define METH_UNDERSAMPLE 2
int method = METH_BOXCAR;

/*
 *	U S A G E --- tell user how to invoke this program, then exit
 */
void usage(void)
{
	(void) fprintf(stderr,
"Usage: %s [-u] [-h] [-w width] [-n scanlines] [-s squaresize]\n\
[-f shrink_factor] [pixfile] > pixfile\n", progname);
	exit(1);
}


/*
 *	P A R S E _ A R G S --- Parse through command line flags
 */
void parse_args(int ac, char **av)
{
	int  c;

	if (!(progname = strrchr(*av, '/')))
		progname = *av;

	/* Turn off bu_getopt's error messages */
	opterr = 0;

	/* get all the option flags from the command line */
	while ((c=bu_getopt(ac,av,options)) != EOF)
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
		if (isatty(fileno(stdout)) )
			usage();
	}
	if (bu_optind < ac) {
		if (freopen(av[bu_optind], "r", stdin) == (FILE *)NULL) {
			perror(av[bu_optind]);
			exit(-1);
		} else
			filename = av[bu_optind];
	}
	if (bu_optind+1 < ac)
		(void)fprintf(stderr, "%s: Excess arguments ignored\n", progname);

}


/*
 *	M A I N
 *
 *	Call parse_args to handle command line arguments first, then
 *	process input.
 */
int main(int ac, char **av)
{
	UCHAR *buffer = (UCHAR *)NULL;

	(void)parse_args(ac, av);
	if (isatty(fileno(stdin))) usage();

	/* process stdin */
	scanlen = width * 3;
	buffer = read_image(scanlen, width, height, buffer);

	switch (method) {
	case METH_BOXCAR : shrink_image(scanlen, width, height, buffer, factor); break;
	case METH_UNDERSAMPLE : usample_image(scanlen, width, height, buffer, factor);
				break;
	default: return(-1);
	}

	write_image(width/factor, height/factor, buffer);
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
