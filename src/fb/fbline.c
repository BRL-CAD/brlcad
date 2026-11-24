/*                        F B L I N E . C
 * BRL-CAD
 *
 * Copyright (c) 1988-2026 United States Government as represented by
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
/** @file fbline.c
 *
 * Draw a single 2-D line on the framebuffer, in a given color
 *
 * Based rather heavily on plot-fb.c
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
#include "vmath.h"
#include "dm.h"
#include "pkg.h"


struct coords {
    short x;
    short y;
}; 		/* Cartesian coordinates */

struct stroke {
    struct stroke *next;	/* next in list, or NULL */
    struct coords pixel;	/* starting scan, nib */
    short xsign;		/* 0 or +1 */
    short ysign;		/* -1, 0, or +1 */
    int ymajor; 		/* true if Y is major dir. */
#undef major
#undef minor
    short major;		/* major dir delta (nonneg) */
    short minor;		/* minor dir delta (nonneg) */
    short e;			/* DDA error accumulator */
    short de;			/* increment for `e' */
}; /* rasterization descriptor */

static const char *framebuffer = NULL;
struct fb *fbp;			/* Current framebuffer */

static int screen_width = 512;	/* default input width */
static int screen_height = 512;	/* default input height */
static int clear = 0;

RGBpixel pixcolor = { 255, 255, 255 };

struct fbline_args {
    struct fbline_args *self;
    int help;
    int clear;
    const char *framebuffer;
    int width;
    int height;
    int red;
    int green;
    int blue;
};

enum fbline_dimension {
    FBLINE_SIZE,
    FBLINE_WIDTH,
    FBLINE_HEIGHT
};

static struct fbline_args *
fbline_args_from_storage(void *storage)
{
    return storage ? *(struct fbline_args **)storage : NULL;
}

static int
fbline_dimension_value(struct bu_vls *msg, const char *arg, void *storage,
	enum fbline_dimension dimension)
{
    struct fbline_args *args = fbline_args_from_storage(storage);
    int value;

    if (!bu_arg_integer_range_from_str(msg, &value, arg, 10, 1, INT_MAX))
	return -1;
    if (args) {
	if (dimension == FBLINE_SIZE)
	    args->width = args->height = value;
	else if (dimension == FBLINE_WIDTH)
	    args->width = value;
	else
	    args->height = value;
    }
    return 0;
}

#define FBLINE_DIMENSION_CALLBACK(_name, _dimension) \
    static int _name(struct bu_vls *msg, const char *arg, void *storage) \
    { \
	return fbline_dimension_value(msg, arg, storage, _dimension); \
    }

FBLINE_DIMENSION_CALLBACK(fbline_size, FBLINE_SIZE)
FBLINE_DIMENSION_CALLBACK(fbline_width, FBLINE_WIDTH)
FBLINE_DIMENSION_CALLBACK(fbline_height, FBLINE_HEIGHT)

static const struct bu_arg_option fbline_options[] = {
    BU_ARG_FLAG("h", "help", struct fbline_args, help,
	"Print help and exit"),
    BU_ARG_ALIAS("?", NULL, "help", 1),
    BU_ARG_FLAG("c", "clear", struct fbline_args, clear,
	"Clear the framebuffer before drawing"),
    BU_ARG_VALUE("F", "framebuffer", struct fbline_args, framebuffer,
	BU_ARG_VALUE_STRING, BU_ARG_REQUIRED, "device",
	"Select the framebuffer instead of the FB_FILE environment default"),
    BU_ARG_CUSTOM("s", "size", struct fbline_args, self, fbline_size,
	"pixels", "Set both framebuffer dimensions"),
    BU_ARG_ALIAS("S", NULL, "size", 1),
    BU_ARG_CUSTOM("w", "width", struct fbline_args, self, fbline_width,
	"pixels", "Set the framebuffer width"),
    BU_ARG_ALIAS("W", NULL, "width", 1),
    BU_ARG_CUSTOM("n", "height", struct fbline_args, self, fbline_height,
	"scanlines", "Set the framebuffer height"),
    BU_ARG_ALIAS("N", NULL, "height", 1),
    BU_ARG_INTEGER_RANGE("r", "red", struct fbline_args, red,
	1, 0, 1, 255, "level", "Set the red color component (default 255)"),
    BU_ARG_INTEGER_RANGE("g", "green", struct fbline_args, green,
	1, 0, 1, 255, "level", "Set the green color component (default 255)"),
    BU_ARG_INTEGER_RANGE("b", "blue", struct fbline_args, blue,
	1, 0, 1, 255, "level", "Set the blue color component (default 255)"),
    BU_ARG_OPTION_NULL
};

static const struct bu_arg_operand fbline_operands[] = {
    BU_ARG_OPERAND_INTEGER_RANGE("x1", 1, 1, 1, SHRT_MIN, 1, SHRT_MAX,
	"Starting x coordinate", NULL),
    BU_ARG_OPERAND_INTEGER_RANGE("y1", 1, 1, 1, SHRT_MIN, 1, SHRT_MAX,
	"Starting y coordinate", NULL),
    BU_ARG_OPERAND_INTEGER_RANGE("x2", 1, 1, 1, SHRT_MIN, 1, SHRT_MAX,
	"Ending x coordinate", NULL),
    BU_ARG_OPERAND_INTEGER_RANGE("y2", 1, 1, 1, SHRT_MIN, 1, SHRT_MAX,
	"Ending y coordinate", NULL),
    BU_ARG_OPERAND_NULL
};

static const struct bu_arg_schema fbline_schema = BU_ARG_SCHEMA(
    "fbline", "Draw a colored line on a framebuffer",
    fbline_options, fbline_operands, BU_ARG_PARSE_INTERSPERSED,
    BU_ARG_SCHEMA_META_HELP(NULL, NULL, NULL, NULL, NULL));

static const struct bu_arg_grammar fbline_grammar = {
    &fbline_schema, NULL, NULL
};

static int
fbline_help(FILE *stream, int status)
{
    char *help = bu_arg_schema_help(&fbline_schema, "fbline");
    bu_flog(stream, "%s", help ? help : "");
    bu_free(help, "fbline help");
    return status;
}



/*
 * Limit generated positions to edges of screen
 * Really should clip to screen, instead.
 */
void
edgelimit(struct coords *ppos)
{
    V_MIN(ppos->x, screen_width-1);
    V_MIN(ppos->y, screen_height-1);
}


/*
 * Raster - rasterize stroke.
 *
 * Method:
 * Modified Bresenham algorithm.  Guaranteed to mark a dot for
 * a zero-length stroke.
 */
void
Raster(struct stroke *vp)
{
    short dy;		/* raster within active band */

    /*
     * Write the color of this vector on all pixels.
     */
    for (dy = vp->pixel.y; dy < screen_height;) {

	/* set the appropriate pixel in the buffer */
	fb_write(fbp, vp->pixel.x, dy, pixcolor, 1);

	if (vp->major-- == 0)
	    return;		/* Done */

	if (vp->e < 0) {
	    /* advance major & minor */
	    dy += vp->ysign;
	    vp->pixel.x += vp->xsign;
	    vp->e += vp->de;
	} else {
	    /* advance major only */
	    if (vp->ymajor)	/* Y is major dir */
		++dy;
	    else			/* X is major dir */
		vp->pixel.x += vp->xsign;
	    vp->e -= vp->minor;
	}
    }
    fprintf(stderr, "line left screen?\n");
}


/*
 * set up DDA parameters
 */
void
BuildStr(struct coords *pt1, struct coords *pt2)
{
    struct stroke cur_stroke;
    struct stroke *vp = &cur_stroke;

    /* arrange for pt1 to have the smaller Y-coordinate: */
    if (pt1->y > pt2->y) {
	struct coords *temp;	/* temporary for swap */

	temp = pt1;		/* swap pointers */
	pt1 = pt2;
	pt2 = temp;
    }

    /* set up DDA parameters for stroke */
    vp->pixel = *pt1;		/* initial pixel */
    vp->major = pt2->y - vp->pixel.y;	/* always nonnegative */
    vp->ysign = vp->major ? 1 : 0;
    vp->minor = pt2->x - vp->pixel.x;
    if ((vp->xsign = vp->minor ? (vp->minor > 0 ? 1 : -1) : 0) < 0)
	vp->minor = -vp->minor;

    /* if Y is not really major, correct the assignments */
    if (!(vp->ymajor = vp->minor <= vp->major)) {
	short temp;	/* temporary for swap */

	temp = vp->minor;
	vp->minor = vp->major;
	vp->major = temp;
    }

    vp->e = vp->major / 2 - vp->minor;	/* initial DDA error */
    vp->de = vp->major - vp->minor;

    Raster(vp);
}


int
main(int argc, char **argv)
{
    struct fbline_args args = {NULL, 0, 0, NULL, 512, 512, 255, 255, 255};
    struct bu_vls msg = BU_VLS_INIT_ZERO;
    struct coords start, end;
    int operand_index;
    int x1, y1, x2, y2;
    args.self = &args;

    bu_setprogname(argv[0]);
    int introspection = bu_arg_introspect(&msg, &fbline_grammar, argc - 1,
	(const char * const *)(argv + 1));
    if (introspection != 0) {
	bu_flog(introspection > 0 ? stdout : stderr, "%s", bu_vls_cstr(&msg));
	bu_vls_free(&msg);
	return introspection > 0 ? 0 : 1;
    }
    operand_index = bu_arg_schema_parse_complete(&fbline_schema, &args, &msg,
	argc - 1, (const char **)(argv + 1));
    if (operand_index < 0) {
	bu_flog(stderr, "%s", bu_vls_cstr(&msg));
	bu_vls_free(&msg);
	return 1;
    }
    bu_vls_free(&msg);
    if (args.help)
	return fbline_help(stdout, 0);
    if (!bu_arg_integer_from_str(NULL, &x1, argv[operand_index + 1], 10) ||
	!bu_arg_integer_from_str(NULL, &y1, argv[operand_index + 2], 10) ||
	!bu_arg_integer_from_str(NULL, &x2, argv[operand_index + 3], 10) ||
	!bu_arg_integer_from_str(NULL, &y2, argv[operand_index + 4], 10))
	bu_exit(1, "fbline: internal coordinate validation mismatch\n");

    framebuffer = args.framebuffer;
    screen_width = args.width;
    screen_height = args.height;
    clear = args.clear;
    pixcolor[RED] = (unsigned char)args.red;
    pixcolor[GRN] = (unsigned char)args.green;
    pixcolor[BLU] = (unsigned char)args.blue;

    if ((fbp = fb_open(framebuffer, screen_width, screen_height)) == NULL)
	bu_exit(12, NULL);

    if (clear) {
	fb_clear(fbp, PIXEL_NULL);
    }
    screen_width = fb_getwidth(fbp);
    screen_height = fb_getheight(fbp);

    start.x = (short)x1;
    start.y = (short)y1;
    end.x = (short)x2;
    end.y = (short)y2;
    edgelimit(&start);
    edgelimit(&end);
    BuildStr(&start, &end);	/* pixels */

    fb_close(fbp);
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
