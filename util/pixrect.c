/*
 *		P I X R E C T . C
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
 *  
 *  Copyright Notice -
 *	This software is Copyright (C) 1986-2004 by the United States Army.
 *	All rights reserved.
 */
#ifndef lint
static const char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "conf.h"

#include <stdio.h>
#include <ctype.h>

#include "machine.h"
#include "externs.h"		/* For getopt */

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
	while ((c = getopt(argc, argv, "s:w:n:x:y:X:Y:S:W:N:#:")) != EOF) {
		switch(c) {
		case 's':
			linelen   = atoi(optarg);
			inputmode = COMMAND_LINE;
			break;
		case 'w':
			linelen   = atoi(optarg);
			inputmode = COMMAND_LINE;
			break;
		case 'n':
			inputmode = COMMAND_LINE;
			break;
		case 'x':
			xorig     = atoi(optarg);
			inputmode = COMMAND_LINE;
			break;
		case 'y':
			yorig     = atoi(optarg);
			inputmode = COMMAND_LINE;
			break;
		case 'X':
			inputmode = COMMAND_LINE;
			break;
		case 'Y':
			inputmode = COMMAND_LINE;
			break;
		case 'S':
			xnum = ynum = atoi(optarg);
			inputmode = COMMAND_LINE;
			break;
		case 'W':
			xnum      = atoi(optarg);
			inputmode = COMMAND_LINE;
			break;
		case 'N':
			ynum      = atoi(optarg);
			inputmode = COMMAND_LINE;
			break;
		case '#':
			bytes_per_pixel = atoi(optarg);
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
		if (optind >= argc) {
			if (isatty(fileno(stdin))) {
				fprintf(stderr,
					"pixrect: input from sdtin\n");
				return(0);
			}
			ifp = stdin;
		} else {
			file_name = argv[optind];
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
	if (argc > ++optind)
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

	if ((buf = malloc(outbytes)) == NULL) {
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
