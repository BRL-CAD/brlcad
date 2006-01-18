/*                       P I X R E C T . C
 * BRL-CAD
 *
 * Copyright (c) 1986-2006 United States Government as represented by
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
/** @file pixrect.c
 *
 *  Remove a portion of a potentially huge pix file.
 *
 *  Authors -
 *	Phillip Dykstra
 *	2 Oct 1985
 *
 *      Further additions by John Grosh, 1 April 1990
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
#include <ctype.h>

#include "machine.h"
#include "bu.h"


#define	INTERACTIVE	0
#define	COMMAND_LINE	1

FILE		*ifp, *ofp;		/* input and output file pointers */

static char	*file_name;

static int 	linelen;		/* input width input file */
static int 	xorig = 0;     		/* Bottom left corner to extract from */
static int 	yorig = 0;		/* Default at (0,0) pixels     */
static int 	xnum  = 0;
static int 	ynum  = 0;
static int 	bytes_per_pixel = 3;	/* Default for RGB */

static char usage[] = "\
Usage: pixrect -w in_width -n in_height -W out_width -N out_height\n\
               [-x xoffset] [-y yoffset] [-# bytes] [infile.pix]\n\
  or   pixrect [-# bytes] infile outfile (I prompt!)\n";


int
get_args(register int argc, register char **argv)
{
	register int c;
	register int inputmode = INTERACTIVE;

	/* Get info from command line arguments */
	while ((c = bu_getopt(argc, argv, "s:w:n:x:y:X:Y:S:W:N:#:")) != EOF) {
		switch(c) {
		case 's':
			linelen   = atoi(bu_optarg);
			inputmode = COMMAND_LINE;
			break;
		case 'w':
			linelen   = atoi(bu_optarg);
			inputmode = COMMAND_LINE;
			break;
		case 'n':
			inputmode = COMMAND_LINE;
			break;
		case 'x':
			xorig     = atoi(bu_optarg);
			inputmode = COMMAND_LINE;
			break;
		case 'y':
			yorig     = atoi(bu_optarg);
			inputmode = COMMAND_LINE;
			break;
		case 'X':
			inputmode = COMMAND_LINE;
			break;
		case 'Y':
			inputmode = COMMAND_LINE;
			break;
		case 'S':
			xnum = ynum = atoi(bu_optarg);
			inputmode = COMMAND_LINE;
			break;
		case 'W':
			xnum      = atoi(bu_optarg);
			inputmode = COMMAND_LINE;
			break;
		case 'N':
			ynum      = atoi(bu_optarg);
			inputmode = COMMAND_LINE;
			break;
		case '#':
			bytes_per_pixel = atoi(bu_optarg);
			break;
		default:		/* '?' */
			return(0);
		}
	}

	/* If parameters (i.e. xnum, etc.) are not entered on */
        /*    command line, obtain input in the same style as */
        /*    the original version of pixrect.c               */

	if (inputmode == INTERACTIVE) {
		if (argc != 4 && argc != 3)
			return(0);

		/* Obtain file pointers */
		if ((ifp = fopen(argv[argc-2], "r")) == NULL) {
			fprintf(stderr,"pixrect: can't open %s\n", argv[argc-1]);
			fprintf(stderr,usage);
			exit(2);
		}
		if ((ofp = fopen(argv[argc-1], "w")) == NULL) {
			fprintf(stderr,"pixrect: can't open %s\n", argv[argc]);
			fprintf(stderr, usage);
			exit(3);
		}

		/* Get info */
		printf( "Area to extract (x, y) in pixels " );
		scanf( "%d%d", &xnum, &ynum );
		printf( "Origin to extract from (0,0 is lower left) " );
		scanf( "%d%d", &xorig, &yorig );
		printf( "Scan line length of input file " );
		scanf( "%d", &linelen );
	}

	/* Make sure nessecary variables set */
	if (linelen <= 0 || xnum <= 0 || ynum <= 0) {
		fprintf(stderr, "pixrect: args for -w -W -N [-S] must be > 0\n");
		fprintf(stderr, usage);
		exit(1);
	}

	if (inputmode == COMMAND_LINE) {
		/* Obtain file pointers */
		ofp = stdout;
		if (bu_optind >= argc) {
			if (isatty(fileno(stdin))) {
				fprintf(stderr,
					"pixrect: input from sdtin\n");
				return(0);
			}
			ifp = stdin;
		} else {
			file_name = argv[bu_optind];
			if ((ifp = fopen(file_name, "r")) == NULL) {
				fprintf(stderr,
					"pixrect: cannot open \"%s\" for reading\n",
					file_name);
				return(0);
			}
		}

		if (isatty(fileno(stdout))) {
			fprintf(stderr, "pixrect: output to stdout\n\n");
			return(0);
		}
	}

#if 0
	if (argc > ++bu_optind)
		fprintf(stderr,"pixrect: excess argument(s) ignored\n");
#endif

	return(1);		/* OK */
}

/* ======================================================================= */

char	*buf;			/* output scanline buffer, malloc'd */
int	outbytes;

int
main(register int argc, register char **argv)
{
	int	row;
	long	offset;

	if (!get_args(argc,argv)) {
		fprintf(stderr, usage);
		exit(1);
	}

	outbytes = xnum * bytes_per_pixel;

	if ((buf = (char *)malloc(outbytes)) == NULL) {
		fprintf(stderr, "pixrect: malloc failed!\n");
		exit(1);
	}

	/* Move all points */
	for (row = 0 + yorig; row < ynum + yorig; row++) {
		offset = (row * linelen + xorig) * bytes_per_pixel;
		fseek(ifp, offset, 0);
		fread(buf, sizeof(*buf), outbytes, ifp);
		fwrite(buf, sizeof(*buf), outbytes, ofp);
	}

	exit(0);
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
