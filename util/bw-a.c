/*
 *		B W - A . C
 *
 *
 * Convert a bw file into an ASCII bitmap.
 *
 * The output of bw-a can be feed to the X11 program atobm to generated
 * a bitmap that X11 programs can use.
 *
 * It is assumed that the file is in inverse order.
 *
 * A translation sequence might be:
 *	pix-fb -w1280 -n960 -i -Fimage.pixi image.pix	# flip image.
 *	pix-bw image.pixi | halftone -R -S -w1280 -n960 | \
 *		bw-a -w1280 -n960 | atobm -name image.bm >image.bm
 *
 * Author -
 *	Christopher T. Johnson	- January 28, 1991
 *
 * Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 *  
 *  Copyright Notice -
 *	This software is Copyright (C) 1986 by the United States Army.
 *	All rights reserved.
 */
#ifndef lint
static char RCSid[] = "$Header$";
#endif

#include "conf.h"

#include <stdio.h>

#include "machine.h"
#include "externs.h"			/* For getopt */

static int	file_width = 512;
static int	file_height= 512;
static int	autosize = 0;
static char	*file_name;
static FILE	*infp;
static int	fileinput = 0;

static char	usage[] = "\
Usage: bw-a [a] [-s squarefilesize] [-w file_width] [-n file_height]\n\
	[file.bw]\n";

get_args( argc, argv )
int argc;
char **argv;
{
	int c;

	while ((c=getopt(argc, argv, "as:w:n:")) != EOF ) {
		switch(c) {
		case 'a':
			autosize = 1;
			break;
		case 's':
			file_height = file_width = atoi(optarg);
			autosize = 0;
			break;
		case 'n':
			file_height = atoi(optarg);
			autosize = 0;
			break;
		case 'w':
			file_width = atoi(optarg);
			autosize = 0;
			break;
		default:		/* '?' */
			return(0);
		}
	}

	if (optind >= argc ) {
		if (isatty(fileno(stdin)) ) return(0);
		file_name = "-";
		infp = stdin;
	} else {
		file_name = argv[optind];
		if ((infp = fopen(file_name, "r")) == NULL) {
			(void) fprintf(stderr,
			    "bw-a: cannot open \"%s\" for reading\n",
			    file_name);
			return(0);
		}
		fileinput++;
	}

	if (argc > ++optind) {
		(void) fprintf(stderr, "bw-a: excess argument(s) ignored\n");
	}
	return(1);	/* OK */
}
main(argc, argv)
int argc;
char **argv;
{
	int c;
	int cur_width = 0;
	int cur_height = 0;

	if ( !get_args(argc, argv)) {
		(void) fputs(usage, stderr);
		exit(1);
	}

	/* autosize the input? */
	if (fileinput && autosize) {
		int	w, h;
		if ( bn_common_file_size(&w, &h, file_name, 1) ) {
			file_width = w;
			file_height = h;
		} else {
			fprintf(stderr, "bw-a: unable to autosize\n");
		}
	}

	while ((c=getc(infp)) != EOF) {
		if (c < 127) {
			putchar('#');
		} else {
			putchar('-');
		}
		if (++cur_width >= file_width) {
			putchar('\n');
			cur_width=0;
			cur_height++;
		}
	}
}
