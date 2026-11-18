/*                        F B - P I X . C
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
 *
 */
/** @file fb-pix.c
 *
 * Program to take a frame buffer image and write a .pix image.
 *
 */

#include "common.h"

#include <errno.h>
#include <limits.h>
#include <sys/stat.h>

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

struct fb_pix_args {
    struct fb_pix_args *self;
    int help;
    int crunch;
    int inverse;
    const char *framebuffer;
    int width;
    int height;
};

static int
fb_pix_size(struct bu_vls *msg, const char *arg, void *storage)
{
    int size;
    struct fb_pix_args *args = storage ? *(struct fb_pix_args **)storage : NULL;

    if (!bu_arg_integer_range_from_str(msg, &size, arg, 10, 1, INT_MAX))
	return -1;
    if (args) {
	args->width = size;
	args->height = size;
    }
    return 0;
}

static const struct bu_arg_option fb_pix_options[] = {
    BU_ARG_FLAG("h", "help", struct fb_pix_args, help,
	"Print help and exit"),
    BU_ARG_ALIAS("?", NULL, "help", 1),
    BU_ARG_FLAG("c", "color-map", struct fb_pix_args, crunch,
	"Apply a non-linear framebuffer color map"),
    BU_ARG_FLAG("i", "inverse", struct fb_pix_args, inverse,
	"Read framebuffer scanlines from top to bottom"),
    BU_ARG_VALUE("F", "framebuffer", struct fb_pix_args, framebuffer,
	BU_ARG_VALUE_STRING, BU_ARG_REQUIRED, "device",
	"Select the framebuffer device instead of the configured default"),
    BU_ARG_CUSTOM("s", "size", struct fb_pix_args, self, fb_pix_size,
	"pixels", "Set both output dimensions (default: 512)"),
    BU_ARG_INTEGER_RANGE("w", "width", struct fb_pix_args, width,
	1, 1, 1, INT_MAX, "pixels", "Set the output width (default: 512)"),
    BU_ARG_INTEGER_RANGE("n", "height", struct fb_pix_args, height,
	1, 1, 1, INT_MAX, "pixels", "Set the output height (default: 512)"),
    BU_ARG_OPTION_NULL
};

static const struct bu_arg_operand fb_pix_operands[] = {
    BU_ARG_OPERAND("output", BU_ARG_VALUE_FILE, 0, 1,
	"PIX output file (default: standard output)", NULL),
    BU_ARG_OPERAND_NULL
};

static const struct bu_arg_schema fb_pix_schema = BU_ARG_SCHEMA(
    "fb-pix", "Save a framebuffer image as a BRL-CAD PIX image",
    fb_pix_options, fb_pix_operands, BU_ARG_PARSE_OPTIONS_FIRST,
    BU_ARG_SCHEMA_META_HELP(NULL, NULL, NULL, NULL, NULL));

static const struct bu_arg_grammar fb_pix_grammar = {
    &fb_pix_schema, NULL, NULL
};


int
main(int argc, char **argv)
{
    struct fb_pix_args args = {NULL, 0, 0, 0, NULL, 512, 512};
    struct bu_vls msg = BU_VLS_INIT_ZERO;
    struct fb *fbp;
    FILE *outfp = stdout;
    int y;

    unsigned char *scanline = NULL;	/* 1 scanline pixel buffer */
    size_t scanbytes;		/* # of bytes of scanline */
    int scanpix;		/* # of pixels of scanline */
    ColorMap cmap;		/* libfb color map */
    args.self = &args;

    bu_setprogname(argv[0]);
    int introspection = bu_arg_introspect(&msg, &fb_pix_grammar, argc - 1,
	(const char * const *)(argv + 1));
    if (introspection != 0) {
	bu_flog(introspection > 0 ? stdout : stderr, "%s", bu_vls_cstr(&msg));
	bu_vls_free(&msg);
	return introspection > 0 ? 0 : 1;
    }
    int operand_index = bu_arg_schema_parse_complete(&fb_pix_schema, &args,
	&msg, argc - 1, (const char **)(argv + 1));
    if (operand_index < 0) {
	bu_flog(stderr, "%s", bu_vls_cstr(&msg));
	bu_vls_free(&msg);
	return 1;
    }
    bu_vls_free(&msg);
    if (args.help) {
	char *help = bu_arg_schema_help(&fb_pix_schema, "fb-pix");
	bu_flog(stdout, "%s", help ? help : "");
	bu_free(help, "fb-pix help");
	return 0;
    }

    if (operand_index < argc - 1) {
	const char *file_name = argv[operand_index + 1];
	outfp = fopen(file_name, "wb");
	if (!outfp) {
	    bu_flog(stderr, "fb-pix: cannot open \"%s\" for writing: %s\n",
		file_name, strerror(errno));
	    return 1;
	}
	(void)bu_fchmod(fileno(outfp), 0444);
    } else {
	if (isatty(fileno(stdout))) {
	    char *help = bu_arg_schema_help(&fb_pix_schema, "fb-pix");
	    bu_flog(stderr, "%s", help ? help : "");
	    bu_free(help, "fb-pix help");
	    return 1;
	}
	setmode(fileno(stdout), O_BINARY);
    }

    if ((fbp = fb_open(args.framebuffer, args.width, args.height)) == NULL) {
	bu_exit(12, NULL);
    }

    V_MIN(args.height, fb_getheight(fbp));
    V_MIN(args.width, fb_getwidth(fbp));

    scanpix = args.width;
    scanbytes = scanpix * sizeof(RGBpixel);
    scanline = (unsigned char *)bu_malloc(scanbytes, "scanline");

    if (args.crunch) {
	if (fb_rmap(fbp, &cmap) == -1) {
	    args.crunch = 0;
	} else if (fb_is_linear_cmap(&cmap)) {
	    args.crunch = 0;
	}
    }

    if (!args.inverse) {
	/* Regular -- read bottom to top */
	for (y=0; y < args.height; y++) {
	    fb_read(fbp, 0, y, scanline, args.width);
	    if (args.crunch)
		cmap_crunch((RGBpixel *)scanline, scanpix, &cmap);
	    if (fwrite((char *)scanline, scanbytes, 1, outfp) != 1) {
		perror("fwrite");
		break;
	    }
	}
    } else {
	/* Inverse -- read top to bottom */
	for (y = args.height-1; y >= 0; y--) {
	    fb_read(fbp, 0, y, scanline, args.width);
	    if (args.crunch)
		cmap_crunch((RGBpixel *)scanline, scanpix, &cmap);
	    if (fwrite((char *)scanline, scanbytes, 1, outfp) != 1) {
		perror("fwrite");
		break;
	    }
	}
    }
    fb_close(fbp);
    bu_free(scanline, "scanline");
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
