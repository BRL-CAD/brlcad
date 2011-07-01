/*                       P I X R E C T . C
 * BRL-CAD
 *
 * Copyright (c) 1986-2011 United States Government as represented by
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
/** @file util/pixrect.c
 *
 * Remove a portion of a potentially huge pix file.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <ctype.h>
#include "bio.h"

#include "bu.h"


#define INTERACTIVE 0
#define COMMAND_LINE 1

FILE *ifp, *ofp;		/* input and output file pointers */

static char *file_name;

static int linelen;		/* input width input file */
static int xorig = 0;     		/* Bottom left corner to extract from */
static int yorig = 0;		/* Default at (0, 0) pixels */
static int xnum  = 0;
static int ynum  = 0;
static int bytes_per_pixel = 3;	/* Default for RGB */

static const char usage[] = "\
Usage: pixrect -w in_width -n in_height -W out_width -N out_height\n\
	       [-x xoffset] [-y yoffset] [-# bytes] [infile.pix]\n\
  or   pixrect [-# bytes] infile outfile (I prompt!)\n";


int
get_args(int argc, char **argv)
{
    int c;
    int inputmode = INTERACTIVE;
    int ret;

    /* Get info from command line arguments */
    while ((c = bu_getopt(argc, argv, "s:w:n:x:y:X:Y:S:W:N:#:")) != -1) {
	switch (c) {
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
		return 0;
	}
    }

    /* If parameters (i.e. xnum, etc.) are not entered on */
    /* command line, obtain input in the same style as */
    /* the original version of pixrect.c */

    if (inputmode == INTERACTIVE) {
	if (argc != 4 && argc != 3)
	    return 0;

	/* Obtain file pointers */
	if ((ifp = fopen(argv[argc-2], "r")) == NULL) {
	    fprintf(stderr, "%s", usage);
	    bu_exit(2, "pixrect: can't open %s\n", argv[argc-1]);
	}
	if ((ofp = fopen(argv[argc-1], "w")) == NULL) {
	    fprintf(stderr, "%s", usage);
	    bu_exit(3, "pixrect: can't open %s\n", argv[argc]);
	}

	/* Get info */
	printf("Area to extract (x, y) in pixels ");
	ret = scanf("%d%d", &xnum, &ynum);
	if (ret != 2)
	    perror("scanf");

	printf("Origin to extract from (0, 0 is lower left) ");
	ret = scanf("%d%d", &xorig, &yorig);
	if (ret != 2)
	    perror("scanf");

	printf("Scan line length of input file ");
	ret = scanf("%d", &linelen);
	if (ret != 1)
	    perror("scanf");
    }

    /* Make sure nessecary variables set */
    if (linelen <= 0 || xnum <= 0 || ynum <= 0) {
	fprintf(stderr, "%s", usage);
	bu_exit(1, "pixrect: args for -w -W -N [-S] must be > 0\n");
    }

    if (inputmode == COMMAND_LINE) {
	/* Obtain file pointers */
	ofp = stdout;
	if (bu_optind >= argc) {
	    if (isatty(fileno(stdin))) {
		fprintf(stderr,
			"pixrect: input from sdtin\n");
		return 0;
	    }
	    ifp = stdin;
	} else {
	    file_name = argv[bu_optind];
	    if ((ifp = fopen(file_name, "r")) == NULL) {
		fprintf(stderr,
			"pixrect: cannot open \"%s\" for reading\n",
			file_name);
		return 0;
	    }
	}

	if (isatty(fileno(stdout))) {
	    fprintf(stderr, "pixrect: output to stdout\n\n");
	    return 0;
	}
    }

    return 1;		/* OK */
}


/* ======================================================================= */

char *buf;			/* output scanline buffer, malloc'd */
int outbytes;

int
main(int argc, char **argv)
{
    int row;
    long offset;
    size_t ret;

    if (!get_args(argc, argv)) {
	bu_exit(1, "%s", usage);
    }

    outbytes = xnum * bytes_per_pixel;

    if ((buf = (char *)malloc(outbytes)) == NULL) {
	fprintf(stderr, "pixrect: malloc failed!\n");
	bu_exit (1, NULL);
    }

    /* Move all points */
    for (row = 0 + yorig; row < ynum + yorig; row++) {
	offset = (row * linelen + xorig) * bytes_per_pixel;
	fseek(ifp, offset, 0);
	ret = fread(buf, sizeof(*buf), outbytes, ifp);
	if (ret < (size_t)outbytes)
	    perror("fread");
	ret = fwrite(buf, sizeof(*buf), outbytes, ofp);
	if (ret < (size_t)outbytes)
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
