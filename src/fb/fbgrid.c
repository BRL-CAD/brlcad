/*                        F B G R I D . C
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
/** @file fbgrid.c
 *
 */

#include "common.h"

#include <limits.h>

#include "bu/arg.h"
#include "bu/app.h"
#include "bu/color.h"
#include "bu/exit.h"
#include "bu/log.h"
#include "bu/malloc.h"
#include "dm.h"

enum fbgrid_flavor {
    FBGRID_OLD,
    FBGRID_BINARY,
    FBGRID_DECIMAL
};

enum fbgrid_dimension {
    FBGRID_SIZE,
    FBGRID_WIDTH,
    FBGRID_HEIGHT
};

struct fbgrid_args {
    struct fbgrid_args *self;
    int help;
    int clear;
    const char *framebuffer;
    int width;
    int height;
    enum fbgrid_flavor flavor;
};

static struct fbgrid_args *
fbgrid_args_from_storage(void *storage)
{
    return storage ? *(struct fbgrid_args **)storage : NULL;
}

static int
fbgrid_dimension(struct bu_vls *msg, const char *arg, void *storage,
	enum fbgrid_dimension dimension)
{
    struct fbgrid_args *args = fbgrid_args_from_storage(storage);
    int value;

    if (!bu_arg_integer_range_from_str(msg, &value, arg, 10, 1, INT_MAX))
	return -1;
    if (args) {
	if (dimension == FBGRID_SIZE)
	    args->width = args->height = value;
	else if (dimension == FBGRID_WIDTH)
	    args->width = value;
	else
	    args->height = value;
    }
    return 0;
}

#define FBGRID_DIMENSION_CALLBACK(_name, _dimension) \
    static int _name(struct bu_vls *msg, const char *arg, void *storage) \
    { \
	return fbgrid_dimension(msg, arg, storage, _dimension); \
    }

FBGRID_DIMENSION_CALLBACK(fbgrid_size, FBGRID_SIZE)
FBGRID_DIMENSION_CALLBACK(fbgrid_width, FBGRID_WIDTH)
FBGRID_DIMENSION_CALLBACK(fbgrid_height, FBGRID_HEIGHT)

#define FBGRID_FLAVOR_CALLBACK(_name, _flavor) \
    static int _name(struct bu_vls *UNUSED(msg), const char *UNUSED(arg), void *storage) \
    { \
	if (storage) \
	    *(enum fbgrid_flavor *)storage = _flavor; \
	return 0; \
    }

FBGRID_FLAVOR_CALLBACK(fbgrid_binary, FBGRID_BINARY)
FBGRID_FLAVOR_CALLBACK(fbgrid_decimal, FBGRID_DECIMAL)
FBGRID_FLAVOR_CALLBACK(fbgrid_old, FBGRID_OLD)

static const struct bu_arg_option fbgrid_options[] = {
    BU_ARG_FLAG("h", "help", struct fbgrid_args, help,
	"Print help and exit"),
    BU_ARG_ALIAS("?", NULL, "help", 1),
    BU_ARG_FLAG("c", "clear", struct fbgrid_args, clear,
	"Clear the framebuffer before drawing"),
    BU_ARG_CUSTOM_FLAG("b", "binary", "binary", struct fbgrid_args, flavor,
	fbgrid_binary, "Use power-of-two grid spacing"),
    BU_ARG_CUSTOM_FLAG("d", "decimal", "decimal", struct fbgrid_args, flavor,
	fbgrid_decimal, "Use decimal grid spacing"),
    BU_ARG_CUSTOM_FLAG("o", "old-style", "old-style", struct fbgrid_args, flavor,
	fbgrid_old, "Draw the legacy calibration pattern"),
    BU_ARG_VALUE("F", "framebuffer", struct fbgrid_args, framebuffer,
	BU_ARG_VALUE_STRING, BU_ARG_REQUIRED, "device",
	"Select the framebuffer device"),
    BU_ARG_CUSTOM("s", "size", struct fbgrid_args, self, fbgrid_size,
	"pixels", "Set both framebuffer dimensions"),
    BU_ARG_ALIAS("S", NULL, "size", 1),
    BU_ARG_CUSTOM("w", "width", struct fbgrid_args, self, fbgrid_width,
	"pixels", "Set the framebuffer width"),
    BU_ARG_ALIAS("W", NULL, "width", 1),
    BU_ARG_CUSTOM("n", "height", struct fbgrid_args, self, fbgrid_height,
	"pixels", "Set the framebuffer height"),
    BU_ARG_ALIAS("N", NULL, "height", 1),
    BU_ARG_OPTION_NULL
};

static const struct bu_arg_schema fbgrid_schema = BU_ARG_SCHEMA(
    "fbgrid", "Draw a calibration grid on a framebuffer",
    fbgrid_options, NULL, BU_ARG_PARSE_INTERSPERSED,
    BU_ARG_SCHEMA_META_HELP(NULL, NULL, NULL, NULL, NULL));

static const struct bu_arg_grammar fbgrid_grammar = {
    &fbgrid_schema, NULL, NULL
};

static void grid(struct fb *fbiop, unsigned char *line, int width, int height,
	int spacing);
static void oldflavor(const struct fbgrid_args *args);


int
main(int argc, char **argv)
{
    struct fbgrid_args args = {NULL, 0, 0, NULL, 0, 0, FBGRID_DECIMAL};
    struct bu_vls msg = BU_VLS_INIT_ZERO;
    unsigned char *white_line;
    unsigned char *grey_line;
    unsigned char *dark_line;
    struct fb *fbp;
    int fbwidth;
    int fbheight;
    int i;
    args.self = &args;

    bu_setprogname(argv[0]);
    int introspection = bu_arg_introspect(&msg, &fbgrid_grammar, argc - 1,
	(const char * const *)(argv + 1));
    if (introspection != 0) {
	bu_flog(introspection > 0 ? stdout : stderr, "%s", bu_vls_cstr(&msg));
	bu_vls_free(&msg);
	return introspection > 0 ? 0 : 1;
    }
    if (bu_arg_schema_parse_complete(&fbgrid_schema, &args, &msg, argc - 1,
	    (const char **)(argv + 1)) < 0) {
	bu_flog(stderr, "%s", bu_vls_cstr(&msg));
	bu_vls_free(&msg);
	return 1;
    }
    bu_vls_free(&msg);
    if (args.help) {
	char *help = bu_arg_schema_help(&fbgrid_schema, "fbgrid");
	bu_flog(stdout, "%s", help ? help : "");
	bu_free(help, "fbgrid help");
	return 0;
    }

    if (args.flavor == FBGRID_OLD)
	oldflavor(&args);	/* exits */

    if ((fbp = fb_open(args.framebuffer, args.width, args.height)) == NULL)
	bu_exit(2, NULL);

    fbwidth = fb_getwidth(fbp);
    fbheight = fb_getheight(fbp);

    /* Initialize the color lines */
    white_line = (unsigned char *)bu_malloc(fbwidth * sizeof(RGBpixel), "white line");
    grey_line  = (unsigned char *)bu_malloc(fbwidth * sizeof(RGBpixel), "grey line");
    dark_line  = (unsigned char *)bu_malloc(fbwidth * sizeof(RGBpixel), "dark line");
    for (i = 0; i < fbwidth; i++) {
	white_line[3*i+RED] = white_line[3*i+GRN] = white_line[3*i+BLU] = 255;
	grey_line[3*i+RED] = grey_line[3*i+GRN] = grey_line[3*i+BLU] = 128;
	dark_line[3*i+RED] = dark_line[3*i+GRN] = dark_line[3*i+BLU] = 64;
    }

    if (args.clear)
	fb_clear(fbp, PIXEL_NULL);

    if (args.flavor == FBGRID_BINARY) {
	/* Dark lines every 8 */
	grid(fbp, dark_line, fbwidth, fbheight, 8);
	/* Grey lines every 64 */
	grid(fbp, grey_line, fbwidth, fbheight, 64);
	/* White line every 128 */
	grid(fbp, white_line, fbwidth, fbheight, 128);
    } else {
	/* DECIMAL */
	/* Dark lines every 10 */
	grid(fbp, dark_line, fbwidth, fbheight, 10);
	/* Grey lines every 50 */
	grid(fbp, grey_line, fbwidth, fbheight, 50);
	/* White line every 100 */
	grid(fbp, white_line, fbwidth, fbheight, 100);
    }

    fb_close(fbp);
    bu_free(dark_line, "dark line");
    bu_free(grey_line, "grey line");
    bu_free(white_line, "white line");
    return 0;
}


static void
grid(struct fb *fbiop, unsigned char *line, int width, int height, int spacing)
{
    int x, y;

    for (y = 0; y < height; y += spacing)
	fb_write(fbiop, 0, y, line, width);
    for (x = 0; x < width; x += spacing) {
	fb_writerect(fbiop, x, 0, 1, height, line);
    }
}


static void
oldflavor(const struct fbgrid_args *args)
{
    struct fb *fbiop;
    int x, y;
    int middle;
    int mask;
    int fb_sz;
    static RGBpixel black, white, red;

    fbiop = fb_open(args->framebuffer, args->width, args->height);
    if (fbiop == NULL) {
	bu_exit(1, NULL);
    }

    fb_sz = fb_getwidth(fbiop);
    white[RED] = white[GRN] = white[BLU] = 255;
    black[RED] = black[GRN] = black[BLU] = 0;
    red[RED] = 255;
    middle = fb_sz/2;
    fb_ioinit(fbiop);
    if (fb_sz <= 512)
	mask = 0x7;
    else
	mask = 0xf;

    for (y = fb_sz-1; y >= 0; y--) {
	for (x = 0; x < fb_sz; x++) {
	    if (x == y || x == fb_sz - y) {
		fb_wpixel(fbiop, white);
	    } else
		if (x == middle || y == middle) {
		    fb_wpixel(fbiop, red);
		} else
		    if ((x & mask) && (y & mask)) {
			fb_wpixel(fbiop, black);
		    } else {
			fb_wpixel(fbiop, white);
		    }
	}
    }
    fb_close(fbiop);
    bu_exit(0, NULL);
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
