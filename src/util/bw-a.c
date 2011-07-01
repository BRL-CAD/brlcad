/*                          B W - A . C
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
/** @file util/bw-a.c
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
 */

#include "common.h"

#include <stdlib.h>
#include "bio.h"

#include "bu.h"
#include "vmath.h"
#include "bn.h"
#include "fb.h"


static long int file_width = 512L;
static int autosize = 0;
static char *file_name;
static FILE *infp;
static int fileinput = 0;

static char usage[] = "\
Usage: bw-a [a] [-s squarefilesize] [-w file_width] [-n file_height]\n\
	[file.bw]\n";

int
get_args(int argc, char **argv)
{
    int c;

    while ((c=bu_getopt(argc, argv, "as:w:n:")) != -1) {
	switch (c) {
	    case 'a':
		autosize = 1;
		break;
	    case 's':
		file_width = atol(bu_optarg);
		autosize = 0;
		break;
	    case 'n':
		autosize = 0;
		break;
	    case 'w':
		file_width = atol(bu_optarg);
		autosize = 0;
		break;
	    default:		/* '?' */
		return 0;
	}
    }

    if (bu_optind >= argc) {
	if (isatty(fileno(stdin))) return 0;
	file_name = "-";
	infp = stdin;
    } else {
	file_name = argv[bu_optind];
	if ((infp = fopen(file_name, "r")) == NULL) {
	    (void) fprintf(stderr,
			   "bw-a: cannot open \"%s\" for reading\n",
			   file_name);
	    return 0;
	}
	fileinput++;
    }

    if (argc > ++bu_optind) {
	(void) fprintf(stderr, "bw-a: excess argument(s) ignored\n");
    }
    return 1;	/* OK */
}


int
main(int argc, char **argv)
{
    int c;
    long int cur_width = 0;
    long int cur_height = 0;

    if (!get_args(argc, argv)) {
	(void) fputs(usage, stderr);
	bu_exit (1, NULL);
    }

    /* autosize the input? */
    if (fileinput && autosize) {
	size_t w, h;
	if (fb_common_file_size(&w, &h, file_name, 1)) {
	    file_width = (long)w;
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
	    cur_width=0L;
	    cur_height++;
	}
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
