/*                       F B C L E A R . C
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
/** @file fbclear.c
 *
 * This program is intended to be used to clear a frame buffer
 * to black, or to the specified color
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

struct fbclear_args {
    struct fbclear_args *self;
    int help;
    int reset;
    const char *framebuffer;
    int width;
    int height;
};

enum fbclear_dimension {
    FBCLEAR_SIZE,
    FBCLEAR_WIDTH,
    FBCLEAR_HEIGHT
};

static struct fbclear_args *
fbclear_args_from_storage(void *storage)
{
    return storage ? *(struct fbclear_args **)storage : NULL;
}

static int
fbclear_dimension(struct bu_vls *msg, const char *arg, void *storage,
	enum fbclear_dimension dimension)
{
    struct fbclear_args *args = fbclear_args_from_storage(storage);
    int value;

    if (!bu_arg_integer_range_from_str(msg, &value, arg, 10, 1, INT_MAX))
	return -1;
    if (args) {
	if (dimension == FBCLEAR_SIZE)
	    args->width = args->height = value;
	else if (dimension == FBCLEAR_WIDTH)
	    args->width = value;
	else
	    args->height = value;
    }
    return 0;
}

#define FBCLEAR_DIMENSION_CALLBACK(_name, _dimension) \
    static int _name(struct bu_vls *msg, const char *arg, void *storage) \
    { \
	return fbclear_dimension(msg, arg, storage, _dimension); \
    }

FBCLEAR_DIMENSION_CALLBACK(fbclear_size, FBCLEAR_SIZE)
FBCLEAR_DIMENSION_CALLBACK(fbclear_width, FBCLEAR_WIDTH)
FBCLEAR_DIMENSION_CALLBACK(fbclear_height, FBCLEAR_HEIGHT)

static int
fbclear_channel_validate(struct bu_vls *msg, const char *arg)
{
    int value;
    return bu_arg_integer_range_from_str(msg, &value, arg, 10, 0, 255) ? 0 : -1;
}

static const struct bu_arg_variant fbclear_color_variants[] = {
    BU_ARG_VARIANT("gray", "gray", 1, "One grayscale value"),
    BU_ARG_VARIANT("rgb", "red green blue", 3, "Three RGB channel values"),
    BU_ARG_VARIANT_NULL
};

static const struct bu_arg_shape fbclear_color_shape = BU_ARG_SHAPE(
    BU_ARG_SHAPE_CUSTOM, 1, 3, "one gray value or three RGB channels", NULL,
    fbclear_color_variants);

static const struct bu_arg_option fbclear_options[] = {
    BU_ARG_FLAG("h", "help", struct fbclear_args, help,
	"Print help and exit"),
    BU_ARG_ALIAS("?", NULL, "help", 1),
    BU_ARG_FLAG("c", "reset", struct fbclear_args, reset,
	"Reset the colormap, pan, and zoom while clearing"),
    BU_ARG_VALUE("F", "framebuffer", struct fbclear_args, framebuffer,
	BU_ARG_VALUE_STRING, BU_ARG_REQUIRED, "device",
	"Select the framebuffer device"),
    BU_ARG_CUSTOM("s", "size", struct fbclear_args, self, fbclear_size,
	"pixels", "Set both framebuffer dimensions"),
    BU_ARG_ALIAS("S", NULL, "size", 1),
    BU_ARG_CUSTOM("w", "width", struct fbclear_args, self, fbclear_width,
	"pixels", "Set the framebuffer width"),
    BU_ARG_ALIAS("W", NULL, "width", 1),
    BU_ARG_CUSTOM("n", "height", struct fbclear_args, self, fbclear_height,
	"pixels", "Set the framebuffer height"),
    BU_ARG_ALIAS("N", NULL, "height", 1),
    BU_ARG_OPTION_NULL
};

static const struct bu_arg_operand fbclear_operands[] = {
    BU_ARG_OPERAND_SHAPED("color", BU_ARG_VALUE_INTEGER, 0, 3,
	fbclear_channel_validate, "Clear color", NULL, &fbclear_color_shape),
    BU_ARG_OPERAND_NULL
};

static int
fbclear_validate(const struct bu_arg_schema *schema, size_t argc,
	const char **argv, size_t cursor_arg, struct bu_arg_validate_result *result)
{
    if (!result || cursor_arg < argc || result->state == BU_ARG_VALIDATE_INVALID)
	return 0;

    size_t color_count = bu_arg_schema_operand_count(schema, argc, argv);
    if (color_count == 2) {
	result->state = BU_ARG_VALIDATE_INCOMPLETE;
	result->token_start = argc;
	result->token_end = argc;
	result->expected = BU_ARG_EXPECT_OPERAND;
	result->hint = "supply one gray value or all three RGB channels";
    }
    return 0;
}

static const struct bu_arg_schema fbclear_schema = BU_ARG_SCHEMA(
    "fbclear", "Clear a framebuffer to a selected color",
    fbclear_options, fbclear_operands, BU_ARG_PARSE_INTERSPERSED,
    BU_ARG_SCHEMA_META_HELP(NULL, NULL, fbclear_validate, NULL, NULL));

static const struct bu_arg_grammar fbclear_grammar = {
    &fbclear_schema, NULL, NULL
};

int
main(int argc, char **argv)
{
    struct fbclear_args args = {NULL, 0, 0, NULL, 0, 0};
    struct bu_vls msg = BU_VLS_INIT_ZERO;
    static RGBpixel pixel;
    struct fb *fbp;
    int scr_width;
    int scr_height;
    int use_custom_pixel = 0;
    args.self = &args;

    bu_setprogname(argv[0]);
    int introspection = bu_arg_introspect(&msg, &fbclear_grammar, argc - 1,
	(const char * const *)(argv + 1));
    if (introspection != 0) {
	bu_flog(introspection > 0 ? stdout : stderr, "%s", bu_vls_cstr(&msg));
	bu_vls_free(&msg);
	return introspection > 0 ? 0 : 1;
    }
    int operand_index = bu_arg_schema_parse_complete(&fbclear_schema, &args,
	&msg, argc - 1, (const char **)(argv + 1));
    if (operand_index < 0) {
	bu_flog(stderr, "%s", bu_vls_cstr(&msg));
	bu_vls_free(&msg);
	return 1;
    }
    bu_vls_free(&msg);
    if (args.help) {
	char *help = bu_arg_schema_help(&fbclear_schema, "fbclear");
	bu_flog(stdout, "%s", help ? help : "");
	bu_free(help, "fbclear help");
	return 0;
    }

    int color_count = argc - operand_index - 1;
    for (int i = 0; i < color_count; i++) {
	int value;
	if (!bu_arg_integer_range_from_str(&msg, &value,
		argv[operand_index + i + 1], 10, 0, 255))
	    bu_exit(1, "fbclear: color: %s\n", bu_vls_cstr(&msg));
	pixel[i] = (unsigned char)value;
    }
    bu_vls_free(&msg);

    if (color_count == 3) {
	use_custom_pixel = 1;
    } else if (color_count == 1) {
	pixel[GRN] = pixel[BLU] = pixel[RED];
	use_custom_pixel = 1;
    }

    scr_width = args.width;
    scr_height = args.height;
    if ((fbp = fb_open(args.framebuffer, scr_width, scr_height)) == NULL) {
	bu_exit(2, NULL);
    }

    /* Get the screen size we were given */
    scr_width = fb_getwidth(fbp);
    scr_height = fb_getheight(fbp);

    if (args.reset) {
	if (fb_wmap(fbp, COLORMAP_NULL) < 0)
	    bu_exit(3, NULL);
	(void)fb_view(fbp, scr_width/2, scr_height/2, 1, 1);
    } else {
	ColorMap cmap;
	int xcent, ycent, xzoom, yzoom;
	if (fb_rmap(fbp, &cmap) >= 0) {
	    if (!fb_is_linear_cmap(&cmap)) {
		bu_flog(stderr, "fbclear: NOTE: non-linear colormap in effect.  -c flag loads linear colormap.\n");
	    }
	}
	(void)fb_getview(fbp, &xcent, &ycent, &xzoom, &yzoom);
	if (xzoom != 1 || yzoom != 1) {
	    bu_flog(stderr, "fbclear:  NOTE: framebuffer is zoomed.  -c will un-zoom.\n");
	}
    }

    if (use_custom_pixel) {
	fb_clear(fbp, pixel);
    } else {
	fb_clear(fbp, PIXEL_NULL);
    }
    (void)fb_close(fbp);
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
