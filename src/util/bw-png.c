/*                        B W - P N G . C
 * BRL-CAD
 *
 * Copyright (c) 1998-2011 United States Government as represented by
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
/** @file util/bw-png.c
 *
 * Convert bw file to PNG (Portable Network Graphics) format
 *
 */

#include "common.h"

#include <stdlib.h>
#include <math.h>
#include <zlib.h>
#include <png.h>
#include "bio.h"

#include "bu.h"
#include "vmath.h"
#include "bn.h"
#include "fb.h"


static long int file_width = 512;		/* default input width */
static long int file_height = 512;		/* default input height */
static int autosize = 0;			/* !0 to autosize input */
static int fileinput = 0;			/* file of pipe on input? */
static char *file_name;
static FILE *infp;

#define BYTESPERPIXEL 1

#define ROWSIZE (file_width * BYTESPERPIXEL)
#define SIZE (file_height * ROWSIZE)

static char usage[] = "\
Usage: bw-png [-a] [-w file_width] [-n file_height]\n\
	[-s square_file_size] [file.bw]\n";

int
get_args(int argc, char **argv)
{
    int c;

    while ((c = bu_getopt(argc, argv, "as:w:n:")) != -1) {
	switch (c) {
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
		return 0;
	}
    }

    if (bu_optind >= argc) {
	if (isatty(fileno(stdin)))
	    return 0;
	file_name = "-";
	infp = stdin;
    } else {
	file_name = argv[bu_optind];
	if ((infp = fopen(file_name, "r")) == NULL) {
	    perror(file_name);
	    (void)fprintf(stderr,
			  "bw-png: cannot open \"%s\" for reading\n",
			  file_name);
	    bu_exit (1, NULL);
	}
	fileinput++;
    }

    if (argc > ++bu_optind)
	(void)fprintf(stderr, "bw-png: excess argument(s) ignored\n");

    return 1;		/* OK */
}


int
main(int argc, char **argv)
{
    int i;
    unsigned char *scanbuf;
    unsigned char **rows;
    png_structp png_p;
    png_infop info_p;

    if (!get_args(argc, argv)) {
	(void)fputs(usage, stderr);
	bu_exit (1, NULL);
    }

    /* autosize input? */
    if (fileinput && autosize) {
	size_t w, h;
	if (fb_common_file_size(&w, &h, file_name, 1)) {
	    file_width = (long)w;
	    file_height = (long)h;
	} else {
	    fprintf(stderr, "bw-png: unable to autosize\n");
	}
    }

    png_p = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png_p)
	bu_exit(EXIT_FAILURE, "Could not create PNG write structure\n");

    info_p = png_create_info_struct(png_p);
    if (!info_p)
	bu_exit(EXIT_FAILURE, "Could not create PNG info structure\n");

    /* allocate space for the image */
    scanbuf = (unsigned char *)bu_calloc(SIZE, sizeof(unsigned char), "scanbuf");

    /* create array of pointers to rows for libpng */
    rows = (unsigned char **)bu_calloc(file_height, sizeof(unsigned char *), "rows");
    for (i=0; i<file_height; i++)
	rows[i] = scanbuf + ((file_height-i-1)*ROWSIZE);

    /* read the bw file */
    if (fread(scanbuf, SIZE, 1, infp) != 1)
	bu_exit(EXIT_FAILURE, "bw-png: Short read\n");

    png_init_io(png_p, stdout);
    png_set_filter(png_p, 0, PNG_FILTER_NONE);
    png_set_compression_level(png_p, Z_BEST_COMPRESSION);
    png_set_IHDR(png_p, info_p, file_width, file_height, 8,
		 PNG_COLOR_TYPE_GRAY, PNG_INTERLACE_NONE,
		 PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);

    png_write_info(png_p, info_p);
    png_write_image(png_p, rows);
    png_write_end(png_p, NULL);
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
