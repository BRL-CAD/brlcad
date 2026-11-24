/*                        F B - P N G . C
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
 *
 */
/** @file fb-png.c
 *
 * Program to take a frame buffer image and write a PNG (Portable
 * Network Graphics) format file.
 *
 */

#include "common.h"

#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <sys/stat.h>
#include "png.h"

#include "bio.h"

#include "bu/arg.h"
#include "bu/app.h"
#include "bu/exit.h"
#include "bu/log.h"
#include "bu/malloc.h"
#include "vmath.h"
#include "dm.h"

#include "pkg.h"


/* in cmap-crunch.c */
extern void cmap_crunch(RGBpixel (*scan_buf), int pixel_ct, ColorMap *colormap);


#define DEFAULT_IMAGE_DIMENSION 512

struct fb_png_args {
    struct fb_png_args *self;
    int help;
    int crunch;
    int inverse;
    const char *framebuffer;
    int width;
    int height;
    fastf_t gamma;
    const char *pixel_bytes;
};


static int
fb_png_size(struct bu_vls *msg, const char *arg, void *storage)
{
    int size;
    struct fb_png_args *args = storage ? *(struct fb_png_args **)storage : NULL;

    if (!bu_arg_integer_range_from_str(msg, &size, arg, 10, 1, INT_MAX))
	return -1;
    if (args) {
	args->width = size;
	args->height = size;
    }
    return 0;
}

static const struct bu_arg_value_keyword fb_png_pixel_bytes[] = {
    {"1", NULL, "Write an 8-bit grayscale PNG"},
    {"3", NULL, "Write a 24-bit RGB PNG"},
    {NULL, NULL, NULL}
};

static const struct bu_arg_option fb_png_options[] = {
    BU_ARG_FLAG("h", "help", struct fb_png_args, help,
	"Print help and exit"),
    BU_ARG_ALIAS("?", NULL, "help", 1),
    BU_ARG_FLAG("c", "color-map", struct fb_png_args, crunch,
	"Apply a non-linear framebuffer color map; ignored when no map is present or the map is linear"),
    BU_ARG_FLAG("i", "inverse", struct fb_png_args, inverse,
	"Read bottom-to-top, producing fourth-quadrant output with an upper-left origin"),
    BU_ARG_VALUE("F", "framebuffer", struct fb_png_args, framebuffer,
	BU_ARG_VALUE_STRING, BU_ARG_REQUIRED, "device",
	"Select the framebuffer device instead of the FB_FILE environment default"),
    BU_ARG_CUSTOM("s", "size", struct fb_png_args, self, fb_png_size,
	"pixels", "Set both output dimensions (default: 512)"),
    BU_ARG_INTEGER_RANGE("w", "width", struct fb_png_args, width,
	1, 1, 1, INT_MAX, "pixels", "Set the output width (default: 512)"),
    BU_ARG_INTEGER_RANGE("n", "height", struct fb_png_args, height,
	1, 1, 1, INT_MAX, "pixels", "Set the output height (default: 512)"),
    BU_ARG_VALUE("g", "gamma", struct fb_png_args, gamma,
	BU_ARG_VALUE_NUMBER, BU_ARG_REQUIRED, "value",
	"Set the PNG source gamma (0.6 approximates PC monitors); non-positive values omit the gAMA chunk (default: -1.0)"),
    BU_ARG_KEYWORD_VALUES("#", "pixel-bytes", struct fb_png_args,
	pixel_bytes, "1|3", "Select grayscale (1) or RGB (3) output (default: 3)",
	fb_png_pixel_bytes),
    BU_ARG_OPTION_NULL
};

static const struct bu_arg_operand fb_png_operands[] = {
    BU_ARG_OPERAND("output", BU_ARG_VALUE_FILE, 0, 1,
	"PNG output file (default: standard output)", NULL),
    BU_ARG_OPERAND_NULL
};

static const struct bu_arg_schema fb_png_schema = BU_ARG_SCHEMA(
    "fb-png", "Save a framebuffer image as a PNG image", fb_png_options,
    fb_png_operands, BU_ARG_PARSE_OPTIONS_FIRST,
    BU_ARG_SCHEMA_META_HELP(NULL, NULL, NULL, NULL, NULL));

static const struct bu_arg_grammar fb_png_grammar = {
    &fb_png_schema, NULL, NULL
};


int
main(int argc, char **argv)
{
    struct fb_png_args args = {NULL, 0, 0, 0, NULL,
	DEFAULT_IMAGE_DIMENSION, DEFAULT_IMAGE_DIMENSION, -1.0, "3"};
    struct bu_vls msg = BU_VLS_INIT_ZERO;
    unsigned char *scanline;	/* scanline pixel buffer */
    size_t scanbytes;		/* bytes in scanline */
    int scanpix;			/* pixels in scanline */
    ColorMap cmap;		/* libfb color map */
    struct fb *fbp;
    FILE *outfp = stdout;
    int y;
    int got;
    int pixbytes;
    png_structp png_p;
    png_infop info_p;
    args.self = &args;

    bu_setprogname(argv[0]);
    int introspection = bu_arg_introspect(&msg, &fb_png_grammar, argc - 1,
	(const char * const *)(argv + 1));
    if (introspection != 0) {
	bu_flog(introspection > 0 ? stdout : stderr, "%s", bu_vls_cstr(&msg));
	bu_vls_free(&msg);
	return introspection > 0 ? 0 : 1;
    }
    int operand_index = bu_arg_schema_parse_complete(&fb_png_schema, &args,
	&msg, argc - 1, (const char **)(argv + 1));
    if (operand_index < 0) {
	bu_flog(stderr, "%s", bu_vls_cstr(&msg));
	bu_vls_free(&msg);
	return 1;
    }
    bu_vls_free(&msg);
    if (args.help) {
	char *help = bu_arg_schema_help(&fb_png_schema, "fb-png");
	bu_flog(stdout, "%s", help ? help : "");
	bu_free(help, "fb-png help");
	return 0;
    }

    if (operand_index < argc - 1) {
	const char *file_name = argv[operand_index + 1];
	outfp = fopen(file_name, "wb");
	if (!outfp) {
	    bu_flog(stderr, "fb-png: cannot open \"%s\" for writing: %s\n",
		file_name, strerror(errno));
	    return 1;
	}
	(void)bu_fchmod(fileno(outfp), 0444);
    } else {
	if (isatty(fileno(stdout))) {
	    char *help = bu_arg_schema_help(&fb_png_schema, "fb-png");
	    bu_flog(stderr, "%s", help ? help : "");
	    bu_free(help, "fb-png help");
	    return 1;
	}
	setmode(fileno(stdout), O_BINARY);
    }
    pixbytes = args.pixel_bytes[0] - '0';

    png_p = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png_p) {
	bu_exit(EXIT_FAILURE, "Could not create PNG write structure\n");
    }

    info_p = png_create_info_struct(png_p);
    if (!info_p) {
	bu_exit(EXIT_FAILURE, "Could not create PNG info structure\n");
    }

    if ((fbp = fb_open(args.framebuffer, args.width, args.height)) == NULL) {
	bu_exit(12, NULL);
    }

    /* If actual screen is smaller than requested size, trim down */
    V_MIN(args.height, fb_getheight(fbp));
    V_MIN(args.width, fb_getwidth(fbp));

    scanpix = args.width;
    scanbytes = (size_t)scanpix * sizeof(RGBpixel);
    scanline = (unsigned char *)bu_malloc(scanbytes, "scanline");

    if (args.crunch) {
	if (fb_rmap(fbp, &cmap) == -1) {
	    args.crunch = 0;
	} else if (fb_is_linear_cmap(&cmap)) {
	    args.crunch = 0;
	}
    }

    png_init_io(png_p, outfp);
    png_set_filter(png_p, 0, PNG_FILTER_NONE);
    png_set_compression_level(png_p, 9);
    png_set_IHDR(png_p, info_p,
		 args.width, args.height, 8,
		 pixbytes == 3 ? PNG_COLOR_TYPE_RGB : PNG_COLOR_TYPE_GRAY,
		 PNG_INTERLACE_NONE,
		 PNG_COMPRESSION_TYPE_DEFAULT,
		 PNG_FILTER_TYPE_DEFAULT);

    /* default to no gamma correction */
    if (args.gamma > 0.0)
	png_set_gAMA(png_p, info_p, args.gamma);

    png_write_info(png_p, info_p);

    if (args.inverse) {
	/* Read bottom to top */
	for (y=0; y < args.height; y++) {
	    if (pixbytes == 3)
		got = fb_read(fbp, 0, y, scanline, args.width);
	    else
		got = fb_bwreadrect(fbp, 0, y, args.width, 1, scanline);

	    if (got != args.width) {
		bu_log("fb-png: Read of scanline %d returned %d, expected %d, aborting.\n",
		       y, got, args.width);
		break;
	    }
	    if (args.crunch)
		cmap_crunch((RGBpixel *)scanline, scanpix, &cmap);
	    png_write_row(png_p, scanline);
	}
    } else {
	/* Read top to bottom */
	for (y = args.height-1; y >= 0; y--) {
	    if (pixbytes == 3)
		got = fb_read(fbp, 0, y, scanline, args.width);
	    else
		got = fb_bwreadrect(fbp, 0, y, args.width, 1, scanline);

	    if (got != args.width) {
		bu_log("fb-png: Read of scanline %d returned %d, expected %d, aborting.\n",
		       y, got, args.width);
		break;
	    }
	    if (args.crunch)
		cmap_crunch((RGBpixel *)scanline, scanpix, &cmap);
	    png_write_row(png_p, scanline);
	}
    }
    fb_close(fbp);
    png_write_end(png_p, NULL);
    png_destroy_write_struct(&png_p, &info_p);
    bu_free(scanline, "scanline");
    if (outfp != stdout)
	fclose(outfp);
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
