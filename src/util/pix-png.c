/*                       P I X - P N G . C
 * BRL-CAD
 *
 * Copyright (c) 1998-2009 United States Government as represented by
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
/** @file pix-png.c
 *
 * Convert pix file to PNG (Portable Network Graphics) format
 *
 */

#include "common.h"

#include <stdlib.h>
#include <math.h>
#ifdef HAVE_SYS_TYPES_H
#  include <sys/types.h>
#endif
#ifdef HAVE_SYS_STAT_H
#  include <sys/stat.h>
#endif
#include "bio.h"

#include "zlib.h"
#include "pngconf.h"
#include "png.h"

#include "bu.h"
#include "vmath.h"
#include "bn.h"


static long int file_width = 512L;		/* default input width */
static long int file_height = 512L;		/* default input height */
static int autosize = 0;			/* !0 to autosize input */
static int fileinput = 0;			/* file of pipe on input? */
static char *file_name;
static FILE *infp, *outfp;

#define BYTESPERPIXEL 3

#define ROWSIZE (file_width * BYTESPERPIXEL)
#define SIZE (file_height * ROWSIZE)

static char usage[] = "\
Usage: pix-png [-a] [-w file_width] [-n file_height]\n\
	[-s square_file_size] [file.pix] > file.png\n";

/**
 * gamma correction value.  0.6 for sane, 1.0 for insane, negative
 * value disables writing a gAMA chunk.
 */
double out_gamma = -1.0;


int
get_args(int argc, register char **argv)
{
    register int c;

    while ((c = bu_getopt(argc, argv, "as:w:n:g:")) != EOF) {
	switch (c) {
	    case 'a':
		autosize = 1;
		break;
	    case 'g':
		out_gamma = atof(bu_optarg);
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
	/* no more args */
	file_name = "-";
	infp = stdin;
    } else {
	file_name = argv[bu_optind];
	if ((infp = fopen(file_name, "r")) == NULL) {
	    perror(file_name);
	    (void)fprintf(stderr,
			  "pix-png: cannot open \"%s\" for reading\n",
			  file_name);
	    bu_exit(1, NULL);
	}
	fileinput++;
    }

    outfp = stdout;
    if (isatty(fileno(infp))) {
	bu_log("ERROR: pix-png will not read pix data from a tty\n");
	return 0; /* not ok */
    }
    if (isatty(fileno(outfp))) {
	bu_log("ERROR: pix-png will not write png data to a tty\n");
	return 0; /* not ok */
    }

    if (argc > ++bu_optind)
	(void)fprintf(stderr, "pix-png: excess argument(s) ignored\n");

    return(1);		/* OK */
}


int
main(int argc, char *argv[])
{
    int i;
    unsigned long int w, h;
    unsigned char *scanbuf;
    unsigned char **rows;
    struct stat sb;
    png_structp png_p;
    png_infop info_p;


    if (!get_args(argc, argv)) {
	(void)fputs(usage, stderr);
	bu_exit(1, NULL);
    }

    /* autosize input? */
    if (fileinput && autosize) {
	if (fb_common_file_size(&w, &h, file_name, 3)) {
	    file_width = (long)w;
	    file_height = (long)h;
	} else {
	    fprintf(stderr, "pix-png: unable to autosize\n");
	}
    }

    png_p = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png_p)
	bu_exit(1, "Could not create PNG write structure\n");

    info_p = png_create_info_struct(png_p);
    if (!info_p)
	bu_exit(1, "Could not create PNG info structure\n");

    /* allocate space for the image */
    scanbuf = (unsigned char *)bu_calloc(SIZE, sizeof(unsigned char), "scanbuf");

    /* create array of pointers to rows for libpng */
    rows = (unsigned char **)bu_calloc(file_height, sizeof(unsigned char *), "rows");
    for (i=0; i<file_height; i++)
	rows[i] = scanbuf + ((file_height-i-1)*ROWSIZE);

    /* read the pix file */
    if (fread(scanbuf, SIZE, 1, infp) != 1)
	bu_exit(1, "pix-png: Short read\n");

    /* warn if we only read part of the input */
    if (fstat(fileno(infp), &sb) < 0) {
	perror("pix-png: unable to stat file:");
	return 1;
    }
    if (SIZE * sizeof(unsigned char) < sb.st_size) {
	bu_log("WARNING: Output PNG image dimensions are smaller than the PIX input image\n");
	bu_log("Input image is %ld pixels, output image is %ld pixels\n", sb.st_size / 3, SIZE * sizeof(unsigned char) / 3);
	if (fb_common_file_size(&w, &h, file_name, 3)) {
	    bu_log("Input PIX dimensions appear to be %ldx%ld pixels.  ", w, h);
	    if (w == h) {
		bu_log("Try using the -s%ld option.\n", w);
	    } else {
		bu_log("Try using the -w%ld -n%ld options.\n", w, h);
	    }
	}
    }

    png_init_io(png_p, outfp);
    png_set_filter(png_p, 0, PNG_FILTER_NONE);
    png_set_compression_level(png_p, Z_BEST_COMPRESSION);
    png_set_IHDR(png_p, info_p, file_width, file_height, 8,
		 PNG_COLOR_TYPE_RGB, PNG_INTERLACE_NONE,
		 PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);

    /*
     * From the PNG 1.0 Specification: the gAMA chunk specifies the
     * gamma characteristic of the source device.
     *
     * In this interpretation, we set the value to 1.0; indicating we
     * hadn't done any gamma correction.
     *
     * From the PNG 1.1 Specification:
     *
     * PNG images can specify, via the gAMA chunk, the power function
     * relating the desired display output with the image samples.
     *
     * In this interpretation, we could set the value to 0.6,
     * representing the value needed to un-do the 2.2 correction
     * auto-applied by PowerPoint for PC monitors.
     */
    png_set_gAMA(png_p, info_p, out_gamma);

    png_write_info(png_p, info_p);
    png_write_image(png_p, rows);
    png_write_end(png_p, NULL);


    /* release resources */

    png_destroy_write_struct(&png_p, &info_p);
    bu_free(scanbuf, "scanbuf");
    bu_free(rows, "rows");

    (void)fclose(infp);
    (void)fclose(outfp);

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
