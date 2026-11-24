/*                        P N G - B W . C
 * BRL-CAD
 *
 * Copyright (c) 1998-2026 United States Government as represented by
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
/** @file util/png-bw.c
 *
 * Convert PNG (Portable Network Graphics) format to bw
 *
 */

#include "common.h"

#include <errno.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include "png.h"
#include "bio.h"

#include "bu/arg.h"
#include "bu/app.h"
#include "bu/exit.h"
#include "bu/log.h"
#include "bu/malloc.h"
#include "bu/str.h"
#include "vmath.h"
#include "bn.h"

static png_color_16 def_backgrd={ 0, 0, 0, 0, 0 };

struct png_bw_args {
    struct png_bw_args *self;
    int help;
    int verbose;
    int red;
    int green;
    int blue;
    double red_weight;
    double green_weight;
    double blue_weight;
};

enum png_bw_channel {
    PNG_BW_RED,
    PNG_BW_GREEN,
    PNG_BW_BLUE
};

static struct png_bw_args *
png_bw_args_from_storage(void *storage)
{
    return storage ? *(struct png_bw_args **)storage : NULL;
}

static int
png_bw_scheme(struct bu_vls *msg, const char *arg, void *storage)
{
    struct png_bw_args *args = png_bw_args_from_storage(storage);
    double red_weight;
    double green_weight;
    double blue_weight;

    if (BU_STR_EQUAL(arg, "ntsc")) {
	red_weight = 0.30;
	green_weight = 0.59;
	blue_weight = 0.11;
    } else if (BU_STR_EQUAL(arg, "crt")) {
	red_weight = 0.26;
	green_weight = 0.66;
	blue_weight = 0.08;
    } else {
	if (msg)
	    bu_vls_printf(msg, "invalid luminance encoding: %s\n", arg);
	return -1;
    }
    if (args) {
	args->red_weight = red_weight;
	args->green_weight = green_weight;
	args->blue_weight = blue_weight;
	args->red = args->green = args->blue = 1;
    }
    return 0;
}

static int
png_bw_legacy_ntsc(struct bu_vls *msg, const char *UNUSED(arg), void *storage)
{
    return png_bw_scheme(msg, "ntsc", storage);
}

static int
png_bw_legacy_crt(struct bu_vls *msg, const char *UNUSED(arg), void *storage)
{
    return png_bw_scheme(msg, "crt", storage);
}

static int
png_bw_weight(struct bu_vls *msg, const char *arg, void *storage,
	enum png_bw_channel channel)
{
    struct png_bw_args *args = png_bw_args_from_storage(storage);
    double weight = 0.0;

    if (arg && !bu_arg_double_from_str(msg, &weight, arg))
	return -1;
    if (args) {
	if (channel == PNG_BW_RED) {
	    args->red = 1;
	    if (arg)
		args->red_weight = weight;
	} else if (channel == PNG_BW_GREEN) {
	    args->green = 1;
	    if (arg)
		args->green_weight = weight;
	} else {
	    args->blue = 1;
	    if (arg)
		args->blue_weight = weight;
	}
    }
    return 0;
}

#define PNG_BW_WEIGHT_CALLBACK(_name, _channel) \
    static int _name(struct bu_vls *msg, const char *arg, void *storage) \
    { \
	return png_bw_weight(msg, arg, storage, _channel); \
    }

PNG_BW_WEIGHT_CALLBACK(png_bw_red_weight, PNG_BW_RED)
PNG_BW_WEIGHT_CALLBACK(png_bw_green_weight, PNG_BW_GREEN)
PNG_BW_WEIGHT_CALLBACK(png_bw_blue_weight, PNG_BW_BLUE)

static const struct bu_arg_value_keyword png_bw_scheme_values[] = {
    {"ntsc", NULL, "NTSC luminance weights"},
    {"crt", NULL, "Historic CRT luminance weights"},
    {NULL, NULL, NULL}
};

static const struct bu_arg_option png_bw_options[] = {
    BU_ARG_FLAG("h", "help", struct png_bw_args, help,
	"Print help and exit"),
    BU_ARG_ALIAS("?", NULL, "help", 1),
    BU_ARG_FLAG("v", "verbose", struct png_bw_args, verbose,
	"Report PNG image and metadata details"),
    {"e", "encoding", "encoding", "scheme",
	"Select a luminance weighting scheme", BU_ARG_VALUE_CUSTOM,
	BU_ARG_STORAGE_OFFSET(struct png_bw_args, self), png_bw_scheme,
	NULL, NULL, NULL, 0, 0, NULL, BU_ARG_REQUIRED, NULL, NULL,
	png_bw_scheme_values, BU_ARG_VALUE_RANGE_NONE, BU_ARG_SHAPE_SCALAR},
    BU_ARG_ATTACHED_OPTIONAL("R", "red-weight", "red-weight",
	struct png_bw_args, self, png_bw_red_weight, "weight",
	"Select red, optionally with an attached weight"),
    BU_ARG_ATTACHED_OPTIONAL("G", "green-weight", "green-weight",
	struct png_bw_args, self, png_bw_green_weight, "weight",
	"Select green, optionally with an attached weight"),
    BU_ARG_ATTACHED_OPTIONAL("B", "blue-weight", "blue-weight",
	struct png_bw_args, self, png_bw_blue_weight, "weight",
	"Select blue, optionally with an attached weight"),
    BU_ARG_CUSTOM_FLAG("ntsc", NULL, "ntsc-compat", struct png_bw_args,
	self, png_bw_legacy_ntsc, "Use legacy NTSC luminance weights"),
    BU_ARG_CUSTOM_FLAG("crt", NULL, "crt-compat", struct png_bw_args,
	self, png_bw_legacy_crt, "Use legacy CRT luminance weights"),
    BU_ARG_OPTION_NULL
};

static const struct bu_arg_operand png_bw_operands[] = {
    BU_ARG_OPERAND("input", BU_ARG_VALUE_FILE, 0, 1,
	"PNG input file (default standard input)", NULL),
    BU_ARG_OPERAND_NULL
};

static const struct bu_arg_option_visibility png_bw_option_visibility[] = {
    BU_ARG_OPTION_VISIBILITY("ntsc-compat", BU_ARG_VISIBILITY_INTERNAL),
    BU_ARG_OPTION_VISIBILITY("crt-compat", BU_ARG_VISIBILITY_INTERNAL),
    BU_ARG_OPTION_VISIBILITY_NULL
};

static const struct bu_arg_presentation png_bw_presentation =
    BU_ARG_PRESENTATION(NULL, png_bw_option_visibility, NULL);

static const struct bu_arg_schema png_bw_schema = BU_ARG_SCHEMA_PRESENTED(
    "png-bw", "Convert a PNG image to BW", png_bw_options, png_bw_operands,
    BU_ARG_PARSE_OPTIONS_FIRST,
    BU_ARG_SCHEMA_META_HELP(NULL, NULL, NULL, NULL, NULL),
    &png_bw_presentation);

static const struct bu_arg_grammar png_bw_grammar = {
    &png_bw_schema, NULL, NULL
};

static int
png_bw_help(FILE *stream, int status)
{
    char *help = bu_arg_schema_help(&png_bw_schema, "png-bw");
    bu_flog(stream, "%s", help ? help : "");
    bu_free(help, "png-bw help");
    return status;
}

int
main(int argc, char **argv)
{
    struct png_bw_args args = {NULL, 0, 0, 0, 0, 0, 0.0, 0.0, 0.0};
    struct bu_vls msg = BU_VLS_INIT_ZERO;
    int i;
    int convert_to_bw=0;
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
    unsigned char *obuf;
    double rweight = 0.0;
    double gweight = 0.0;
    double bweight = 0.0;
    int red   = 0;
    int green = 0;
    int blue  = 0;
    int multiple_colors;
    int num_color_planes;
    int clip_high, clip_low;
    int num;
    int in, out;
    int introspection;
    int operand_index;
    int verbose;
    double value;
    size_t ret;

    args.self = &args;

    bu_setprogname(argv[0]);

    introspection = bu_arg_introspect(&msg, &png_bw_grammar, argc - 1,
	(const char * const *)(argv + 1));
    if (introspection != 0) {
	bu_flog(introspection > 0 ? stdout : stderr, "%s", bu_vls_cstr(&msg));
	bu_vls_free(&msg);
	return introspection > 0 ? 0 : 1;
    }
    operand_index = bu_arg_schema_parse_complete(&png_bw_schema, &args, &msg,
	argc - 1, (const char **)(argv + 1));
    if (operand_index < 0) {
	bu_flog(stderr, "%s", bu_vls_cstr(&msg));
	bu_vls_free(&msg);
	return 1;
    }
    bu_vls_free(&msg);
    if (args.help)
	return png_bw_help(stdout, 0);

    verbose = args.verbose;
    red = args.red;
    green = args.green;
    blue = args.blue;
    rweight = args.red_weight;
    gweight = args.green_weight;
    bweight = args.blue_weight;

    setmode(fileno(stdin), O_BINARY);
    setmode(fileno(stdout), O_BINARY);

    if (argc - 1 - operand_index == 0) {
	if (isatty(fileno(stdin))) {
	    return png_bw_help(stderr, 1);
	}
	fp_in = stdin;
    } else {
	const char *input = argv[operand_index + 1];
	if ((fp_in = fopen(input, "rb")) == NULL) {
	    perror(input);
	    bu_log ("png-bw: cannot open \"%s\" for reading\n", input);
	    bu_exit(EXIT_FAILURE, "Cannot open input file\n");
	}
    }

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

    if (color_type != PNG_COLOR_TYPE_GRAY &&
	color_type != PNG_COLOR_TYPE_GRAY_ALPHA) {
	bu_log("Warning: color image being converted to B/W!!!\n");
	convert_to_bw = 1;
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

    if (!convert_to_bw) {
	/* allocate memory for image */
	image = (unsigned char *)bu_calloc(1, file_width*file_height, "image");

	/* create rows array */
	rows = (unsigned char **)bu_calloc(file_height, sizeof(unsigned char *), "rows");
	for (i=0; i<file_height; i++)
	    rows[file_height-1-i] = image+(i*file_width);
    } else {
	/* allocate memory for image */
	image = (unsigned char *)bu_calloc(1, file_width*file_height*3, "image");

	/* create rows array */
	rows = (unsigned char **)bu_calloc(file_height, sizeof(unsigned char *), "rows");
	for (i=0; i<file_height; i++)
	    rows[file_height-1-i] = image+(i*file_width*3);
    }

    png_read_image(png_p, rows);

    if (!convert_to_bw) {
	ret = fwrite(image, file_width*file_height, 1, stdout);
	if (ret < 1)
	    perror("fwrite");
	bu_exit (0, NULL);
    }

    obuf = (unsigned char *)bu_calloc(file_width*file_height, sizeof(unsigned char), "obuf");

    /* Following code modified from pix-bw.c */

    /* Hack for multiple color planes */
    if (red + green + blue > 1 || rweight > 0.0 || gweight > 0.0 || bweight > 0.0)
	multiple_colors = 1;
    else
	multiple_colors = 0;

    num_color_planes = red + green + blue;
    if (red != 0 && !(rweight > 0.0))
	rweight = 1.0 / (double)num_color_planes;
    if (green != 0 && !(gweight > 0.0))
	gweight = 1.0 / (double)num_color_planes;
    if (blue != 0 && !(bweight > 0.0))
	bweight = 1.0 / (double)num_color_planes;

    clip_high = clip_low = 0;

    /*
     * The loops are repeated for efficiency...
     */
    num = file_width*file_height*3;
    if (multiple_colors) {
	for (in = out = 0; out < num/3; out++, in += 3) {
	    value = rweight*image[in] + gweight*image[in+1] + bweight*image[in+2];
	    if (value > 255.0) {
		obuf[out] = 255;
		clip_high++;
	    } else if (value < 0.0) {
		obuf[out] = 0;
		clip_low++;
	    } else
		obuf[out] = value;
	}
    } else if (red) {
	for (in = out = 0; out < num/3; out++, in += 3)
	    obuf[out] = image[in];
    } else if (green) {
	for (in = out = 0; out < num/3; out++, in += 3)
	    obuf[out] = image[in+1];
    } else if (blue) {
	for (in = out = 0; out < num/3; out++, in += 3)
	    obuf[out] = image[in+2];
    } else {
	/* uniform weight */
	for (in = out = 0; out < num/3; out++, in += 3)
	    obuf[out] = ((int)image[in] + (int)image[in+1] +
			 (int)image[in+2]) / 3;
    }

    ret = fwrite(obuf, sizeof(char), num/3, stdout);
    if (ret < (size_t)num/3)
	perror("fwrite");

    if (clip_high != 0 || clip_low != 0) {
	fprintf(stderr, "png-bw: clipped %d high, %d, low\n",
		clip_high, clip_low);
    }

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
