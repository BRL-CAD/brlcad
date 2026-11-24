/*                       F B F R A M E . C
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
/** @file fbframe.c
 *
 * Overwrite a frame (border) on the current framebuffer.  CCW from
 * the bottom: Red, Green, Blue, White
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

struct fbframe_args {
    struct fbframe_args *self;
    int help;
    const char *framebuffer;
    int width;
    int height;
};

enum fbframe_dimension {
    FBFRAME_SIZE,
    FBFRAME_WIDTH,
    FBFRAME_HEIGHT
};

static struct fbframe_args *
fbframe_args_from_storage(void *storage)
{
    return storage ? *(struct fbframe_args **)storage : NULL;
}

static int
fbframe_dimension(struct bu_vls *msg, const char *arg, void *storage,
	enum fbframe_dimension dimension)
{
    struct fbframe_args *args = fbframe_args_from_storage(storage);
    int value;

    if (!bu_arg_integer_range_from_str(msg, &value, arg, 10, 1, INT_MAX))
	return -1;
    if (args) {
	if (dimension == FBFRAME_SIZE)
	    args->width = args->height = value;
	else if (dimension == FBFRAME_WIDTH)
	    args->width = value;
	else
	    args->height = value;
    }
    return 0;
}

#define FBFRAME_DIMENSION_CALLBACK(_name, _dimension) \
    static int _name(struct bu_vls *msg, const char *arg, void *storage) \
    { \
	return fbframe_dimension(msg, arg, storage, _dimension); \
    }

FBFRAME_DIMENSION_CALLBACK(fbframe_size, FBFRAME_SIZE)
FBFRAME_DIMENSION_CALLBACK(fbframe_width, FBFRAME_WIDTH)
FBFRAME_DIMENSION_CALLBACK(fbframe_height, FBFRAME_HEIGHT)

static const struct bu_arg_option fbframe_options[] = {
    BU_ARG_FLAG("h", "help", struct fbframe_args, help,
	"Print help and exit"),
    BU_ARG_ALIAS("?", NULL, "help", 1),
    BU_ARG_VALUE("F", "framebuffer", struct fbframe_args, framebuffer,
	BU_ARG_VALUE_STRING, BU_ARG_REQUIRED, "device",
	"Select the framebuffer device"),
    BU_ARG_CUSTOM("s", "size", struct fbframe_args, self, fbframe_size,
	"pixels", "Set both frame dimensions"),
    BU_ARG_ALIAS("S", NULL, "size", 1),
    BU_ARG_CUSTOM("w", "width", struct fbframe_args, self, fbframe_width,
	"pixels", "Set the frame width"),
    BU_ARG_ALIAS("W", NULL, "width", 1),
    BU_ARG_CUSTOM("n", "height", struct fbframe_args, self, fbframe_height,
	"pixels", "Set the frame height"),
    BU_ARG_ALIAS("N", NULL, "height", 1),
    BU_ARG_OPTION_NULL
};

static const struct bu_arg_schema fbframe_schema = BU_ARG_SCHEMA(
    "fbframe", "Draw a colored border around a framebuffer",
    fbframe_options, NULL, BU_ARG_PARSE_OPTIONS_FIRST,
    BU_ARG_SCHEMA_META_HELP(NULL, NULL, NULL, NULL, NULL));

static const struct bu_arg_grammar fbframe_grammar = {
    &fbframe_schema, NULL, NULL
};

int
main(int argc, char **argv)
{
    struct fbframe_args args = {NULL, 0, NULL, 0, 0};
    struct bu_vls msg = BU_VLS_INIT_ZERO;
    int x;
    struct fb *fbp;
    int xsize;
    int ysize;
    int len;
    unsigned char *line;
    static RGBpixel white = { 255, 255, 255 };
    static RGBpixel red = { 255, 0, 0 };
    static RGBpixel green = { 0, 255, 0 };
    static RGBpixel blue = { 0, 0, 255 };
    args.self = &args;

    bu_setprogname(argv[0]);

    int introspection = bu_arg_introspect(&msg, &fbframe_grammar, argc - 1,
	(const char * const *)(argv + 1));
    if (introspection != 0) {
	bu_flog(introspection > 0 ? stdout : stderr, "%s", bu_vls_cstr(&msg));
	bu_vls_free(&msg);
	return introspection > 0 ? 0 : 1;
    }
    if (bu_arg_schema_parse_complete(&fbframe_schema, &args, &msg, argc - 1,
	    (const char **)(argv + 1)) < 0) {
	bu_flog(stderr, "%s", bu_vls_cstr(&msg));
	bu_vls_free(&msg);
	return 1;
    }
    bu_vls_free(&msg);
    if (args.help) {
	char *help = bu_arg_schema_help(&fbframe_schema, "fbframe");
	bu_flog(stdout, "%s", help ? help : "");
	bu_free(help, "fbframe help");
	return 0;
    }
    xsize = args.width;
    ysize = args.height;

    if ((fbp = fb_open(args.framebuffer, xsize, ysize)) == FB_NULL)
	bu_exit(1, NULL);

    if (xsize <= 0)
	xsize = fb_getwidth(fbp);
    if (ysize <= 0)
	ysize = fb_getheight(fbp);

    /* malloc buffer for pixel lines */
    len = (xsize > ysize) ? xsize : ysize;
    line = (unsigned char *)bu_calloc(len, sizeof(RGBpixel), "line");
    if (line == RGBPIXEL_NULL) {
	bu_flog(stderr, "fbframe: malloc failure\n");
	return 1;
    }

#define FLOOD(col) { for (x=len-1; x >= 0; x--) {COPYRGB(&line[3*x], col);} }

    /*
     * Red:	(0->510,      0)
     * Green:	(511, 0->510)
     * Blue:	(511->1,    511)
     * White:	(0, 511->1)
     */
    FLOOD(red);
    fb_writerect(fbp, 0, 0, xsize-1, 1, line);
    FLOOD(green);
    fb_writerect(fbp, xsize-1, 0, 1, ysize-1, line);
    FLOOD(blue);
    fb_writerect(fbp, 1, ysize-1, xsize-1, 1, line);
    FLOOD(white);
    fb_writerect(fbp, 0, 1, 1, ysize-1, line);

    fb_close(fbp);

    bu_free(line, "line");

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
