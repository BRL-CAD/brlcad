/*                       P I X - P P M . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2009 United States Government as represented by
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
/** @file pix-ppm.c
 *
 * convert BRL .pix files to ppm
 *
 * Pixels run left-to-right, from the bottom of the screen to the top.
 *
 */

#include "common.h"

#include <stdlib.h>
#include "bio.h"

#include "bu.h"
#include "vmath.h"
#include "bn.h"
#include "fb.h"


static long int file_width = 512L;	/* default input width */
static long int file_height = 512L;	/* default input height */

static int autosize = 0;		/* !0 to autosize input */

static int fileinput = 0;		/* file of pipe on input? */
static char *file_name;
static FILE *infp;

static int pixbytes = 3;

#define ROWSIZE (file_width * pixbytes)
#define SIZE (file_width * file_height * pixbytes)

char *scanbuf;


int
get_args(int argc, char *argv[])
{
    int c;

    while ((c = bu_getopt(argc, argv, "a#:s:w:n:")) != EOF) {
	switch (c) {
	    case '#':
		pixbytes = atoi(bu_optarg);
		break;
	    case 'a':
		autosize = 1;
		break;
	    case 's':
		/* square file size */
		file_height = file_width = atol(bu_optarg);
		autosize = 0;
		break;
	    case 'w':
		file_width = atol(bu_optarg);
		autosize = 0;
		break;
	    case 'n':
		file_height = atol(bu_optarg);
		autosize = 0;
		break;

	    default:		/* '?' */
		return(0);
	}
    }

    if (bu_optind >= argc) {
	if (isatty(fileno(stdin)))
	    return(0);
	file_name = "-";
	infp = stdin;
    } else {
	file_name = argv[bu_optind];
	if ((infp = fopen(file_name, "r")) == NULL) {
	    perror(file_name);
	    bu_exit(1, "pix-ppm: cannot open \"%s\" for reading\n", file_name);
	}
	fileinput++;
    }

    if (argc > ++bu_optind)
	bu_log("pix-ppm: excess argument(s) ignored\n");

    return(1);		/* OK */
}

int
main(int argc, char *argv[])
{
    int i;
    char *row;
    static char usage[] = "Usage: pix-ppm [-a] [-#bytes] [-w file_width] [-n file_height] [-s square_file_size] [file.pix] > file.ppm";

    if (!get_args(argc, argv)) {
	bu_exit (1, "%s\n", usage);
    }

    /* autosize input? */
    if (fileinput && autosize) {
	unsigned long int w, h;
	if (fb_common_file_size(&w, &h, file_name, pixbytes)) {
	    file_width = (long)w;
	    file_height = (long)h;
	} else {
	    bu_log("pix-ppm: unable to autosize\n");
	}
    }

    /*
     * gobble up the bytes
     */
    scanbuf = bu_malloc(SIZE, "scanbuf");
    if (fread(scanbuf, 1, SIZE, infp) == 0) {
	bu_exit (1, "pix-ppm: Short read\n");
    }

    if (pixbytes == 1) {
	/* PGM magic number */
	printf("P2\n");
    } else {
	/* PPM magic number */
	printf("P6\n");
    }

    /* width height */
    printf("%lu %lu\n", file_width, file_height);

    /* maximum color component value */
    printf("255\n");
    fflush(stdout);

    /*
     * now write them out in the right order, 'cause the
     * input is upside down.
     */

    for (i = 0; i < file_height; i++) {
	row = scanbuf + (file_height-1 - i) * ROWSIZE;
	fwrite(row, 1, ROWSIZE, stdout);
    }

    bu_free(scanbuf, "scanbuf");
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
