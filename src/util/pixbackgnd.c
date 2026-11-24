/*                    P I X B A C K G N D . C
 * BRL-CAD
 *
 * Copyright (c) 1986-2026 United States Government as represented by
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
/** @file util/pixbackgnd.c
 *
 * Background Maker
 *
 * Given Hue and Saturation for background, make top light and bottom dark.
 * Generates a pix(5) stream on stdout.
 *
 */

#include "common.h"

#include <limits.h>
#include <stdlib.h>
#include <math.h>
#include "bio.h"

#include "vmath.h"
#include "bu/arg.h"
#include "bu/app.h"
#include "bu/exit.h"
#include "bu/log.h"
#include "bu/malloc.h"


struct pixbackgnd_args {
    struct pixbackgnd_args *self;
    int help;
    int invert;
    int file_width;
    int file_height;
    int title_height;
    int top_intensity;
    int bottom_intensity;
};

static struct pixbackgnd_args *
pixbackgnd_args_from_storage(void *storage)
{
    return storage ? *(struct pixbackgnd_args **)storage : NULL;
}

static int
pixbackgnd_high_resolution(struct bu_vls *UNUSED(msg),
	const char *UNUSED(arg), void *storage)
{
    struct pixbackgnd_args *args = pixbackgnd_args_from_storage(storage);

    if (args) {
	args->file_width = 1024;
	args->file_height = 1024;
	args->title_height = 90;
    }
    return 0;
}

static int
pixbackgnd_size(struct bu_vls *msg, const char *arg, void *storage)
{
    struct pixbackgnd_args *args = pixbackgnd_args_from_storage(storage);
    int value;

    if (!bu_arg_integer_range_from_str(msg, &value, arg, 10, 1, INT_MAX))
	return -1;
    if (args)
	args->file_width = args->file_height = value;
    return 0;
}

static const struct bu_arg_option pixbackgnd_options[] = {
    BU_ARG_FLAG("h", "help", struct pixbackgnd_args, help,
	"Print help and exit"),
    BU_ARG_ALIAS("?", NULL, "help", 1),
    BU_ARG_CUSTOM_FLAG("H", "high-resolution", "high-resolution",
	struct pixbackgnd_args, self, pixbackgnd_high_resolution,
	"Generate a 1024 by 1024 image with a 90-line title area"),
    BU_ARG_FLAG("i", "invert", struct pixbackgnd_args, invert,
	"Write scanlines from top to bottom"),
    BU_ARG_CUSTOM("s", "size", struct pixbackgnd_args, self,
	pixbackgnd_size, "pixels", "Set both output dimensions"),
    BU_ARG_INTEGER_RANGE("w", "width", struct pixbackgnd_args, file_width,
	1, 1, 1, INT_MAX, "pixels", "Set the output image width"),
    BU_ARG_INTEGER_RANGE("n", "height", struct pixbackgnd_args, file_height,
	1, 1, 1, INT_MAX, "pixels", "Set the output image height"),
    BU_ARG_INTEGER_RANGE("t", "title-height", struct pixbackgnd_args,
	title_height, 1, 1, 1, INT_MAX, "lines", "Set the title area height"),
    BU_ARG_INTEGER_RANGE("a", "top-intensity", struct pixbackgnd_args,
	top_intensity, 1, 0, 1, 255, "value", "Set the top intensity"),
    BU_ARG_INTEGER_RANGE("b", "bottom-intensity", struct pixbackgnd_args,
	bottom_intensity, 1, 0, 1, 255, "value", "Set the bottom intensity"),
    BU_ARG_OPTION_NULL
};

static const struct bu_arg_operand pixbackgnd_operands[] = {
    BU_ARG_OPERAND_NUMBER_RANGE("component", 2, 3, 1, 0.0, 1, 255.0,
	"Hue and saturation, or red, green, and blue", NULL),
    BU_ARG_OPERAND_NULL
};

static const struct bu_arg_schema pixbackgnd_schema = BU_ARG_SCHEMA(
    "pixbackgnd", "Generate a shaded PIX background",
    pixbackgnd_options, pixbackgnd_operands, BU_ARG_PARSE_OPTIONS_FIRST,
    BU_ARG_SCHEMA_META_HELP(NULL, NULL, NULL, NULL, NULL));

static const struct bu_arg_grammar pixbackgnd_grammar = {
    &pixbackgnd_schema, NULL, NULL
};

/* rgbhsv
 *
 * convert red green blue to hue saturation value
 */
static void
rgbhsv(double *rgb, double *hsv)
{
    double s, v, r, g, b, x;
    double h = 0.0;
    double dif = 0.0;

    r = rgb[0];
    g = rgb[1];
    b = rgb[2];
    v = ((r > g) ? r : g);
    v = ((v > b) ? v : b);
    x = ((r < g) ? r : g);
    x = ((x < b) ? x : b);
    if (!ZERO(v - x)) { /* v != x */
	dif = (double)(v - x);
	if (!ZERO(r - v)) /* r != v */
	    if (ZERO(g - v)) /* g == v */
		if (!ZERO(b - x)) /* b != x */
		    h = (double)(42.5 * (3.0 - (double)(v-b) / dif));
		else
		    h = (double)(42.5 * (1.0 + (double)(v-r) / dif));
	    else if (!ZERO(r - x)) /* r != x */
		h = (double)(42.5 * (5.0 - (double)(v-r) / dif));
	    else
		h = (double)(42.5 * (3.0 + (double)(v-g) / dif));
	else if (!ZERO(g - x)) /* g != x */
	    h = (double)(42.5 * (1.0 - (double)(v-g) / dif));
	else
	    h = (double)(42.5 * (5.0 + (double)(v-b) / dif));
    }

    if (!ZERO(v))
	s = (double)(255. * dif / (double)v);
    else
	s = 0;

    hsv[0] = h;
    hsv[1] = s;
    hsv[2] = v;
}


/* hsvrgb
 *
 * convert hue saturation and value to red, green, blue
 */
static void
hsvrgb(double *hsv, double *rgb)
{
    double r, g, b, m, n, k, foo;
    double h, s, v;
    double f;

    if (!ZERO(hsv[1])) {
	s = (double)hsv[1] / 255.0;
	h = (double)hsv[0] / 42.666;
	f = modf(h, &foo);
	v = (double)hsv[2];
	m = (double)(v * (1.0 - s) + .5);
	n = (double)(v * (1.0 - s*f) + .5);
	k = (double)(v * (1.0 - (s * (1.-f))) + .5);
	switch ((int) h) {
	    case 0:
		r = hsv[2];
		g = k;
		b = m;
		break;
	    case 1:
		r = n;
		g = hsv[2];
		b = m;
		break;
	    case 2:
		r = m;
		g = hsv[2];
		b = k;
		break;
	    case 3:
		r = m;
		g = n;
		b = hsv[2];
		break;
	    case 4:
		r = k;
		g = m;
		b = hsv[2];
		break;
	    default:
	    case 5:
		r = hsv[2];
		g = m;
		b = n;
		break;
	}
    } else
	r = g = b = hsv[2];

    rgb[0] = r;
    rgb[1] = g;
    rgb[2] = b;
}


static void
flood(unsigned char *horiz_buf, unsigned char *vert_buf, size_t scanline_size, int line)
{
    unsigned char *op;
    unsigned char *vp = &vert_buf[(size_t)line * 3];

    op = horiz_buf + scanline_size;
    while (op > horiz_buf) {
	*--op = vp[2];
	*--op = vp[1];
	*--op = *vp;
    }
    if (fwrite(horiz_buf, 1, scanline_size, stdout) != scanline_size)
	bu_exit(1, "pixbackgnd: failed to write scanline\n");
}


int
main(int argc, char **argv)
{
    int i = 0;
    int line = 0;
    size_t scanline_size;
    size_t column_size;
    unsigned char *horiz_buf = NULL;
    unsigned char *vert_buf = NULL;
    unsigned char *vp = NULL;
    double RGB[3] = {128.0, 128.0, 128.0};
    double HSV[3] = {0.0, 0.0, 0.0};
    double deltav;
    struct pixbackgnd_args args = {NULL, 0, 0, 512, 512, 80, 240, 50};
    struct bu_vls msg = BU_VLS_INIT_ZERO;
    args.self = &args;

    bu_setprogname(argv[0]);

    int introspection = bu_arg_introspect(&msg, &pixbackgnd_grammar, argc - 1,
	(const char * const *)(argv + 1));
    if (introspection != 0) {
	bu_flog(introspection > 0 ? stdout : stderr, "%s", bu_vls_cstr(&msg));
	bu_vls_free(&msg);
	return introspection > 0 ? 0 : 1;
    }
    int operand_index = bu_arg_schema_parse_complete(&pixbackgnd_schema, &args,
	&msg, argc - 1, (const char **)(argv + 1));
    if (operand_index < 0)
	bu_exit(1, "%s", bu_vls_cstr(&msg));
    bu_vls_free(&msg);
    if (args.help) {
	char *help = bu_arg_schema_help(&pixbackgnd_schema, "pixbackgnd");
	bu_flog(stdout, "%s", help ? help : "");
	bu_free(help, "pixbackgnd help");
	return 0;
    }
    if (isatty(fileno(stdout)))
	bu_exit(1, "pixbackgnd requires redirected standard output\n");

    int operand_count = argc - operand_index - 1;
    if (operand_count == 2) {
	if (!bu_arg_double_range_from_str(NULL, &HSV[0],
		argv[operand_index + 1], 0.0, 255.0) ||
	    !bu_arg_double_range_from_str(NULL, &HSV[1],
		argv[operand_index + 2], 0.0, 255.0))
	    bu_exit(1, "pixbackgnd: internal HSV validation failure\n");
	HSV[2] = args.top_intensity;
	hsvrgb(HSV, RGB);
    } else {
	for (i = 0; i < 3; i++) {
	    if (!bu_arg_double_range_from_str(NULL, &RGB[i],
		    argv[operand_index + i + 1], 0.0, 255.0))
		bu_exit(1, "pixbackgnd: internal RGB validation failure\n");
	}
	rgbhsv(RGB, HSV);
	HSV[2] = args.top_intensity;
    }
    if (args.file_height <= 4 ||
	args.title_height >= args.file_height - 4)
	bu_exit(1, "pixbackgnd: title height must leave room for the white stripe and gradient\n");

    int file_width = args.file_width;
    int file_height = args.file_height;
    int title_height = args.title_height;
    int h_end = args.bottom_intensity;

    if ((size_t)file_width > SIZE_MAX / 3 ||
	(size_t)file_height > SIZE_MAX / 3) {
	bu_exit(1, "pixbackgnd: image dimensions are too large\n");
    }
    scanline_size = (size_t)file_width * 3;
    column_size = (size_t)file_height * 3;

    setmode(fileno(stdout), O_BINARY);

    horiz_buf = (unsigned char *)bu_calloc(scanline_size,
	sizeof(unsigned char), "horiz_buf");
    vert_buf = (unsigned char *)bu_calloc(column_size,
	sizeof(unsigned char), "vert_buf");

    /*
     * First stage -- prepare the vert_buf with one pixel
     * for each scanline, since each scanline has uniform color.
     * For ease of thinking about it, this is done top-to-bottom.
     */
    line = 0;
    vp = vert_buf;
    if (title_height > 0) {
	/* Make top area with initial HSV */
	for (; line<title_height; line++) {
	    *vp++ = RGB[0];
	    *vp++ = RGB[1];
	    *vp++ = RGB[2];
	}

	/* A white stripe, 4 lines high */
	for (i=0; i<4; i++, line++) {
	    *vp++ = 250;
	    *vp++ = 250;
	    *vp++ = 250;
	}
    }

    /* Do rest with V dropping from start to end values */
    if (HSV[2] > h_end) {
	/* Go from bright at the top to dim at the bottom */
	deltav = (HSV[2]-h_end) / (double)(file_height-line);

	for (; line<file_height; line++) {
	    HSV[2] -= deltav;
	    hsvrgb(HSV, RGB);
	    *vp++ = RGB[0];
	    *vp++ = RGB[1];
	    *vp++ = RGB[2];
	}
    } else {
	/* Go from dim at the top to bright at the bottom */
	deltav = (h_end-HSV[2]) / (double)(file_height-line);

	for (; line<file_height; line++) {
	    HSV[2] += deltav;
	    hsvrgb(HSV, RGB);
	    *vp++ = RGB[0];
	    *vp++ = RGB[1];
	    *vp++ = RGB[2];
	}
    }

    /*
     * Second stage -- flood each value across the horiz_buf
     * and write the scanline out.  Here we proceed bottom-to-top
     * for pix(5) format.
     */

    if (!args.invert) {
	for (line = file_height-1; line >= 0; line--)
	    flood(horiz_buf, vert_buf, scanline_size, line);
    } else {
	/* Inverted: top-to-bottom */
	for (line=0; line < file_height; line++)
	    flood(horiz_buf, vert_buf, scanline_size, line);
    }

    bu_free(horiz_buf, "horiz_buf");
    bu_free(vert_buf, "vert_buf");

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
