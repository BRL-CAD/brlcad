/*                       P N G - P I X . C
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
/** @file util/png-pix.c
 *
 * Convert PNG (Portable Network Graphics) format to pix
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


static png_color_16 def_backgrd={ 0, 0, 0, 0, 0 };
static int verbose=0;

static char *usage="Usage:\n\t%s [-v] [png_input_file] > pix_output_file\n";

int
main(int argc, char **argv)
{
    int i;
    int c;
    FILE *fp_in;
    png_structp png_p;
    png_infop info_p;
    char header[8];
    int bit_depth;
    int color_type;
    png_color_16p input_backgrd;
    double gammaval;
    int file_width, file_height;
    unsigned char *image;
    unsigned char **rows;
    size_t ret;

    while ((c=bu_getopt(argc, argv, "v")) != -1) {
	switch (c) {
	    case 'v':
		verbose = 1;
		break;
	    default:
		bu_log("Illegal option (%c)\n", c);
		bu_log(usage, argv[0]);
		bu_exit(EXIT_FAILURE, "Illegal option\n");
	}
    }

    if (bu_optind >= argc) {
	if (isatty(fileno(stdin))) {
	    bu_log(usage, argv[0]);
	    bu_exit(EXIT_FAILURE, "Are you intending to type in a PNG format file??\n");
	}
	fp_in = stdin;
    } else {
	if ((fp_in = fopen(argv[bu_optind], "rb")) == NULL) {
	    perror(argv[bu_optind]);
	    (void)fprintf(stderr,
			  "png-pix: cannot open \"%s\" for reading\n",
			  argv[bu_optind]);
	    return 1;
	}
    }

    if (argc > ++bu_optind)
	(void)fprintf(stderr, "png-pix: excess argument(s) ignored\n");

    if (fread(header, 8, 1, fp_in) != 1)
	bu_exit(EXIT_FAILURE, "ERROR: Failed while reading file header!!!\n");

    if (png_sig_cmp((png_bytep)header, 0, 8))
	bu_exit(EXIT_FAILURE, "This is not a PNG file!!!\n");

    png_p = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png_p)
	bu_exit(EXIT_FAILURE, "png_create_read_struct() failed!!\n");

    info_p = png_create_info_struct(png_p);
    if (!info_p)
	bu_exit(EXIT_FAILURE, "png_create_info_struct() failed!!\n");

    png_init_io(png_p, fp_in);

    png_set_sig_bytes(png_p, 8);

    png_read_info(png_p, info_p);

    color_type = png_get_color_type(png_p, info_p);

    if (color_type == PNG_COLOR_TYPE_GRAY || color_type == PNG_COLOR_TYPE_GRAY_ALPHA) {
	bu_log("Warning: bw image being converted to RGB!!!\n");
	png_set_gray_to_rgb(png_p);
    }

    png_set_expand(png_p);
    bit_depth = png_get_bit_depth(png_p, info_p);
    if (bit_depth == 16)
	png_set_strip_16(png_p);

    file_width = png_get_image_width(png_p, info_p);
    file_height = png_get_image_height(png_p, info_p);

    if (verbose) {
	switch (color_type) {
	    case PNG_COLOR_TYPE_GRAY:
		bu_log("color type: b/w (bit depth=%d)\n", bit_depth);
		break;
	    case PNG_COLOR_TYPE_GRAY_ALPHA:
		bu_log("color type: b/w with alpha channel (bit depth=%d)\n", bit_depth);
		break;
	    case PNG_COLOR_TYPE_PALETTE:
		bu_log("color type: color palette (bit depth=%d)\n", bit_depth);
		break;
	    case PNG_COLOR_TYPE_RGB:
		bu_log("color type: RGB (bit depth=%d)\n", bit_depth);
		break;
	    case PNG_COLOR_TYPE_RGB_ALPHA:
		bu_log("color type: RGB with alpha channel (bit depth=%d)\n", bit_depth);
		break;
	    default:
		bu_log("Unrecognized color type (bit depth=%d)\n", bit_depth);
		break;
	}
	bu_log("Image size: %d X %d\n", file_width, file_height);
    }

    if (png_get_bKGD(png_p, info_p, &input_backgrd)) {
	if (verbose && (color_type == PNG_COLOR_TYPE_GRAY_ALPHA ||
			color_type == PNG_COLOR_TYPE_RGB_ALPHA))
	    bu_log("background color: %d %d %d\n", input_backgrd->red, input_backgrd->green, input_backgrd->blue);
	png_set_background(png_p, input_backgrd, PNG_BACKGROUND_GAMMA_FILE, 1, 1.0);
    } else
	png_set_background(png_p, &def_backgrd, PNG_BACKGROUND_GAMMA_FILE, 0, 1.0);

    if (png_get_gAMA(png_p, info_p, &gammaval)) {
	if (verbose)
	    bu_log("gamma: %g\n", gammaval);
	png_set_gAMA(png_p, info_p, gammaval);
    }

    if (verbose) {
	if (png_get_interlace_type(png_p, info_p) == PNG_INTERLACE_NONE)
	    bu_log("not interlaced\n");
	else
	    bu_log("interlaced\n");
    }

    png_read_update_info(png_p, info_p);

    /* allocate memory for image */
    image = (unsigned char *)bu_calloc(1, file_width*file_height*3, "image");

    /* create rows array */
    rows = (unsigned char **)bu_calloc(file_height, sizeof(unsigned char *), "rows");
    for (i=0; i<file_height; i++)
	rows[file_height-1-i] = image+(i*file_width*3);

    png_read_image(png_p, rows);

    ret = fwrite(image, file_width*file_height*3, 1, stdout);
    if (ret < 1)
	perror("fwrite");

    if (verbose) {
	png_timep mod_time;
	png_textp text;
	int num_text;

	png_read_end(png_p, info_p);
	if (png_get_text(png_p, info_p, &text, &num_text)) {
	    for (i=0; i<num_text; i++)
		bu_log("%s: %s\n", text[i].key, text[i].text);
	}
	if (png_get_tIME(png_p, info_p, &mod_time))
	    bu_log("Last modified: %d/%d/%d %d:%d:%d\n", mod_time->month, mod_time->day,
		   mod_time->year, mod_time->hour, mod_time->minute, mod_time->second);
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
