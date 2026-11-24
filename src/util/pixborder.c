/*                     P I X B O R D E R . C
 * BRL-CAD
 *
 * Copyright (c) 1996-2026 United States Government as represented by
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
/** @file util/pixborder.c
 *
 * Add a 1-pixel-wide border to regions of a specified color.
 * (Do not confuse this with drawing border around the image.)
 *
 */

#include "common.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "bio.h"

#include "vmath.h"
#include "bu/arg.h"
#include "bu/app.h"
#include "bu/color.h"
#include "bu/exit.h"
#include "bu/log.h"
#include "bu/malloc.h"
#include "bn.h"
#include "dm.h"


#define ACHROMATIC -1.0
#define HUE 0
#define SAT 1
#define VAL 2

static const char *file_name;
static FILE *infp;

static int fileinput = 0;	/* Is input a file (not stdin)? */
static int autosize = 0;	/* Try to guess input dimensions? */
static int tol_using_rgb = 1; /* Compare via RGB, not HSV? */

static size_t file_width = 512;
static size_t file_height = 512;

static ssize_t left_edge = -1;
static ssize_t right_edge = -1;
static ssize_t bottom_edge = -1;
static ssize_t top_edge = -1;

#define COLORS_NEITHER 0
#define COLORS_INTERIOR 1
#define COLORS_EXTERIOR 2
#define COLORS_BOTH (COLORS_INTERIOR | COLORS_EXTERIOR)
static int colors_specified = COLORS_NEITHER;

static unsigned char border_rgb[3];
static unsigned char exterior_rgb[3];
static unsigned char interior_rgb[3];
static unsigned char rgb_tol[3];

static fastf_t border_hsv[3];
static fastf_t exterior_hsv[3];
static fastf_t interior_hsv[3];
static fastf_t hsv_tol[3];


static int
read_row(unsigned char *rp, size_t width, FILE *fp)
{
    size_t ret = fread(rp + 3, 3, width, fp);
    if (ret != width)
	return 0;
    VMOVE(rp, exterior_rgb);
    VMOVE(rp + 3 * (width + 1), exterior_rgb);
    return 1;
}


static void
fill_row(unsigned char *row, size_t pixel_count, const unsigned char color[3])
{
    size_t i;

    for (i = 0; i < pixel_count; ++i)
	VMOVE(row + 3 * i, color);
}


/*
 * Convert between RGB and HSV color models
 *
 * R, G, and B are in {0, 1, ..., 255},
 * H is in [0.0, 360.0), and S and V are in [0.0, 1.0],
 * unless S = 0.0, in which case H = ACHROMATIC.
 *
 * These two routines are adapted from
 * pp. 592-3 of J.D. Foley, A. van Dam, S.K. Feiner, and J.F. Hughes,
 * _Computer graphics: principles and practice_, 2nd ed., Addison-Wesley,
 * Reading, MA, 1990.
 */

static void rgb_to_hsv (unsigned char *rgb, fastf_t *hsv)
{
    fastf_t red, grn, blu;
    fastf_t *hue = &hsv[HUE];
    fastf_t *sat = &hsv[SAT];
    fastf_t *val = &hsv[VAL];
    fastf_t max, min;
    fastf_t delta;

    /*
     * Compute value
     */
    max = min = red = (fastf_t)rgb[RED] / 255.0;

    grn = (fastf_t)rgb[GRN] / 255.0;
    if (grn < min)
	min = grn;
    else if (grn > max)
	max = grn;

    blu = (fastf_t)rgb[BLU] / 255.0;
    if (blu < min)
	min = blu;
    else if (blu > max)
	max = blu;

    *val = max;

    /*
     * Compute saturation
     */
    delta = max - min;
    if (max > 0.0)
	*sat = delta / max;
    else
	*sat = 0.0;

    /*
     * Compute hue
     */
    if (ZERO(*sat))
	*hue = ACHROMATIC;
    else {
	if (ZERO(red - max)) /* red == max */
	    *hue = (grn - blu) / delta;
	else if (ZERO(grn - max)) /* grn == max */
	    *hue = 2.0 + (blu - red) / delta;
	else if (ZERO(blu - max)) /* blu == max */
	    *hue = 4.0 + (red - grn) / delta;

	/*
	 * Convert hue to degrees
	 */
	*hue *= 60.0;
	if (*hue < 0.0)
	    *hue += 360.0;
    }
}


static int hsv_to_rgb (fastf_t *hsv, unsigned char *rgb)
{
    fastf_t float_rgb[3] = VINIT_ZERO;
    fastf_t hue, sat, val;
    fastf_t hue_frac;
    fastf_t p, q, t;
    int hue_int;

    hue = hsv[HUE];
    sat = hsv[SAT];
    val = hsv[VAL];

    if (ZERO(sat)) {
	if (ZERO(hue - ACHROMATIC)) { /* hue == ACHROMATIC */
	    VSETALL(float_rgb, val);
	} else {
	    (void) fprintf(stderr, "Illegal HSV (%g, %g, %g)\n",
			   V3ARGS(hsv));
	    return 0;
	}
    } else {
	if (ZERO(hue - 360.0)) /* hue == 360.0 */
	    hue = 0.0;
	hue /= 60.0;
	hue_int = floor((double) hue);
	hue_frac = hue - hue_int;
	p = val * (1.0 - sat);
	q = val * (1.0 - (sat * hue_frac));
	t = val * (1.0 - (sat * (1.0 - hue_frac)));
	switch (hue_int) {
	    case 0: VSET(float_rgb, val, t, p); break;
	    case 1: VSET(float_rgb, q, val, p); break;
	    case 2: VSET(float_rgb, p, val, t); break;
	    case 3: VSET(float_rgb, p, q, val); break;
	    case 4: VSET(float_rgb, t, p, val); break;
	    case 5: VSET(float_rgb, val, p, q); break;
	    default:
		(void) fprintf(stderr, "%s:%d: This shouldn't happen\n",
			       __FILE__, __LINE__);
		bu_exit (1, NULL);
	}
    }

    rgb[RED] = float_rgb[RED] * 255;
    rgb[GRN] = float_rgb[GRN] * 255;
    rgb[BLU] = float_rgb[BLU] * 255;

    return 1;
}


static int same_rgb (unsigned char *color1, unsigned char *color2)
{
    return ((abs(color1[RED] - color2[RED]) <= (int) rgb_tol[RED]) &&
	    (abs(color1[GRN] - color2[GRN]) <= (int) rgb_tol[GRN]) &&
	    (abs(color1[BLU] - color2[BLU]) <= (int) rgb_tol[BLU]));
}


static int same_hsv (fastf_t *color1, fastf_t *color2)
{
    return ((fabs(color1[HUE] - color2[HUE]) <= hsv_tol[HUE]) &&
	    (fabs(color1[SAT] - color2[SAT]) <= hsv_tol[SAT]) &&
	    (fabs(color1[VAL] - color2[VAL]) <= hsv_tol[VAL]));
}


static int is_interior (unsigned char *pix_rgb)
{
    if (tol_using_rgb)
	return ((colors_specified == COLORS_EXTERIOR)	?
		(! same_rgb(pix_rgb, exterior_rgb))	:
		same_rgb(pix_rgb, interior_rgb));
    else {
	vect_t pix_hsv = VINIT_ZERO;

	rgb_to_hsv(pix_rgb, pix_hsv);
	return ((colors_specified == COLORS_EXTERIOR)	?
		(! same_hsv(pix_hsv, exterior_hsv))	:
		same_hsv(pix_hsv, interior_hsv));
    }
}


static int is_exterior (unsigned char *pix_rgb)
{
    if (tol_using_rgb)
	return ((colors_specified == COLORS_INTERIOR)	?
		(! same_rgb(pix_rgb, interior_rgb))	:
		same_rgb(pix_rgb, exterior_rgb));
    else {
	vect_t pix_hsv = VINIT_ZERO;

	rgb_to_hsv(pix_rgb, pix_hsv);
	return ((colors_specified == COLORS_INTERIOR)	?
		(! same_hsv(pix_hsv, interior_hsv))	:
		same_hsv(pix_hsv, exterior_hsv));
    }
}


static int is_border (unsigned char *prp, unsigned char *trp, unsigned char *nrp, int col_nm)

/* Previous row */
/* Current (this) row */
/* Next row */
/* Current column */

{
    unsigned char pix_rgb[3];

    VMOVE(pix_rgb, trp + (col_nm + 1) * 3);

    /*
     * Ensure that this pixel is in a region of interest
     */
    if (! is_interior(pix_rgb))
	return 0;

    /*
     * Check its left and right neighbors
     */
    VMOVE(pix_rgb, trp + (col_nm + 0) * 3);
    if (is_exterior(pix_rgb))
	return 1;
    VMOVE(pix_rgb, trp + (col_nm + 2) * 3);
    if (is_exterior(pix_rgb))
	return 1;

    /*
     * Check its upper and lower neighbors
     */
    VMOVE(pix_rgb, nrp + (col_nm + 1) * 3);
    if (is_exterior(pix_rgb))
	return 1;
    VMOVE(pix_rgb, prp + (col_nm + 1) * 3);
    if (is_exterior(pix_rgb))
	return 1;

    /*
     * All four of its neighbors are also in the region
     */
    return 0;
}


struct pixborder_args {
    struct pixborder_args *self;
    int help;
    int autosize;
    size_t file_width;
    size_t file_height;
    ssize_t left_edge;
    ssize_t right_edge;
    ssize_t bottom_edge;
    ssize_t top_edge;
    int colors_specified;
    int tol_using_rgb;
    unsigned char border_rgb[3];
    unsigned char exterior_rgb[3];
    unsigned char interior_rgb[3];
    unsigned char rgb_tol[3];
    fastf_t border_hsv[3];
    fastf_t exterior_hsv[3];
    fastf_t interior_hsv[3];
    fastf_t hsv_tol[3];
};

enum pixborder_color_role {
    PIXBORDER_BORDER,
    PIXBORDER_EXTERIOR,
    PIXBORDER_INTERIOR,
    PIXBORDER_TOLERANCE
};

enum pixborder_size_role {
    PIXBORDER_WIDTH,
    PIXBORDER_HEIGHT,
    PIXBORDER_SQUARE,
    PIXBORDER_LEFT,
    PIXBORDER_RIGHT,
    PIXBORDER_BOTTOM,
    PIXBORDER_TOP
};

static struct pixborder_args *
pixborder_args_from_storage(void *storage)
{
    return storage ? *(struct pixborder_args **)storage : NULL;
}

static void
pixborder_args_init(struct pixborder_args *args)
{
    memset(args, 0, sizeof(*args));
    args->self = args;
    args->file_width = 512;
    args->file_height = 512;
    args->left_edge = -1;
    args->right_edge = -1;
    args->bottom_edge = -1;
    args->top_edge = -1;
    args->tol_using_rgb = 1;
    VSETALL(args->border_rgb, 1);
    VSETALL(args->exterior_rgb, 1);
    VSETALL(args->interior_rgb, 255);
    rgb_to_hsv(args->border_rgb, args->border_hsv);
    rgb_to_hsv(args->exterior_rgb, args->exterior_hsv);
    rgb_to_hsv(args->interior_rgb, args->interior_hsv);
}

static int
pixborder_rgb_consume(struct bu_vls *msg, size_t argc, const char **argv,
	void *storage, enum pixborder_color_role role)
{
    struct bu_color color = BU_COLOR_INIT_ZERO;
    struct pixborder_args *args = pixborder_args_from_storage(storage);
    unsigned char rgb[3];

    if (bu_arg_rgb_consume(msg, argc, argv, storage ? &color : NULL) < 0)
	return -1;
    if (!args)
	return 0;
    if (!bu_color_to_rgb_chars(&color, rgb))
	return -1;

    switch (role) {
	case PIXBORDER_BORDER:
	    VMOVE(args->border_rgb, rgb);
	    rgb_to_hsv(args->border_rgb, args->border_hsv);
	    break;
	case PIXBORDER_EXTERIOR:
	    VMOVE(args->exterior_rgb, rgb);
	    rgb_to_hsv(args->exterior_rgb, args->exterior_hsv);
	    args->colors_specified |= COLORS_EXTERIOR;
	    break;
	case PIXBORDER_INTERIOR:
	    VMOVE(args->interior_rgb, rgb);
	    rgb_to_hsv(args->interior_rgb, args->interior_hsv);
	    args->colors_specified |= COLORS_INTERIOR;
	    break;
	case PIXBORDER_TOLERANCE:
	    VMOVE(args->rgb_tol, rgb);
	    args->tol_using_rgb = 1;
	    break;
    }
    return 0;
}

static int
pixborder_hsv_consume(struct bu_vls *msg, size_t argc, const char **argv,
	void *storage, enum pixborder_color_role role)
{
    struct pixborder_args *args = pixborder_args_from_storage(storage);
    fastf_t hsv[3] = VINIT_ZERO;

    if (bu_arg_vector3_consume(msg, argc, argv, hsv) < 0)
	return -1;
    if (hsv[HUE] < 0.0 || hsv[HUE] > 360.0 ||
	hsv[SAT] < 0.0 || hsv[SAT] > 1.0 ||
	hsv[VAL] < 0.0 || hsv[VAL] > 1.0) {
	if (msg)
	    bu_vls_strcat(msg, "HSV requires hue 0..360 and saturation/value 0..1\n");
	return -1;
    }
    if (ZERO(hsv[SAT]))
	hsv[HUE] = ACHROMATIC;
    if (!args)
	return 0;

    switch (role) {
	case PIXBORDER_BORDER:
	    VMOVE(args->border_hsv, hsv);
	    (void)hsv_to_rgb(args->border_hsv, args->border_rgb);
	    break;
	case PIXBORDER_EXTERIOR:
	    VMOVE(args->exterior_hsv, hsv);
	    (void)hsv_to_rgb(args->exterior_hsv, args->exterior_rgb);
	    args->colors_specified |= COLORS_EXTERIOR;
	    break;
	case PIXBORDER_INTERIOR:
	    VMOVE(args->interior_hsv, hsv);
	    (void)hsv_to_rgb(args->interior_hsv, args->interior_rgb);
	    args->colors_specified |= COLORS_INTERIOR;
	    break;
	case PIXBORDER_TOLERANCE:
	    VMOVE(args->hsv_tol, hsv);
	    args->tol_using_rgb = 0;
	    break;
    }
    return 0;
}

#define PIXBORDER_COLOR_CONSUMER(_name, _space, _role) \
    static int _name(struct bu_vls *msg, size_t argc, const char **argv, void *storage) \
    { \
	return pixborder_ ## _space ## _consume(msg, argc, argv, storage, _role); \
    }

PIXBORDER_COLOR_CONSUMER(pixborder_border_rgb, rgb, PIXBORDER_BORDER)
PIXBORDER_COLOR_CONSUMER(pixborder_exterior_rgb, rgb, PIXBORDER_EXTERIOR)
PIXBORDER_COLOR_CONSUMER(pixborder_interior_rgb, rgb, PIXBORDER_INTERIOR)
PIXBORDER_COLOR_CONSUMER(pixborder_tolerance_rgb, rgb, PIXBORDER_TOLERANCE)
PIXBORDER_COLOR_CONSUMER(pixborder_border_hsv, hsv, PIXBORDER_BORDER)
PIXBORDER_COLOR_CONSUMER(pixborder_exterior_hsv, hsv, PIXBORDER_EXTERIOR)
PIXBORDER_COLOR_CONSUMER(pixborder_interior_hsv, hsv, PIXBORDER_INTERIOR)
PIXBORDER_COLOR_CONSUMER(pixborder_tolerance_hsv, hsv, PIXBORDER_TOLERANCE)

static int
pixborder_size_parse(struct bu_vls *msg, const char *arg, void *storage,
	enum pixborder_size_role role)
{
    struct pixborder_args *args = pixborder_args_from_storage(storage);
    size_t minimum = role <= PIXBORDER_SQUARE ? 1 : 0;
    size_t value;

    if (!bu_arg_size_t_range_from_str(msg, &value, arg, 10, minimum,
	(size_t)SSIZE_MAX))
	return -1;
    if (!args)
	return 0;

    switch (role) {
	case PIXBORDER_WIDTH:
	    args->file_width = value;
	    args->autosize = 0;
	    break;
	case PIXBORDER_HEIGHT:
	    args->file_height = value;
	    args->autosize = 0;
	    break;
	case PIXBORDER_SQUARE:
	    args->file_width = value;
	    args->file_height = value;
	    args->autosize = 0;
	    break;
	case PIXBORDER_LEFT:
	    args->left_edge = (ssize_t)value;
	    break;
	case PIXBORDER_RIGHT:
	    args->right_edge = (ssize_t)value;
	    break;
	case PIXBORDER_BOTTOM:
	    args->bottom_edge = (ssize_t)value;
	    break;
	case PIXBORDER_TOP:
	    args->top_edge = (ssize_t)value;
	    break;
    }
    return 0;
}

#define PIXBORDER_SIZE_PARSER(_name, _role) \
    static int _name(struct bu_vls *msg, const char *arg, void *storage) \
    { \
	return pixborder_size_parse(msg, arg, storage, _role); \
    }

PIXBORDER_SIZE_PARSER(pixborder_width, PIXBORDER_WIDTH)
PIXBORDER_SIZE_PARSER(pixborder_height, PIXBORDER_HEIGHT)
PIXBORDER_SIZE_PARSER(pixborder_square, PIXBORDER_SQUARE)
PIXBORDER_SIZE_PARSER(pixborder_left, PIXBORDER_LEFT)
PIXBORDER_SIZE_PARSER(pixborder_right, PIXBORDER_RIGHT)
PIXBORDER_SIZE_PARSER(pixborder_bottom, PIXBORDER_BOTTOM)
PIXBORDER_SIZE_PARSER(pixborder_top, PIXBORDER_TOP)

static const struct bu_arg_variant pixborder_rgb_variants[] = {
    BU_ARG_VARIANT("packed", "red/green/blue", 1,
	"One packed RGB value"),
    BU_ARG_VARIANT("channels", "red green blue", 3,
	"Three RGB channel values"),
    BU_ARG_VARIANT_NULL
};

static const struct bu_arg_variant pixborder_hsv_variants[] = {
    BU_ARG_VARIANT("packed", "hue/saturation/value", 1,
	"One packed HSV value"),
    BU_ARG_VARIANT("components", "hue saturation value", 3,
	"Three HSV component values"),
    BU_ARG_VARIANT_NULL
};

static const struct bu_arg_shape pixborder_rgb_shape = BU_ARG_SHAPE(
    BU_ARG_SHAPE_RGB, 1, 3, "packed RGB or three channels", NULL,
    pixborder_rgb_variants);

static const struct bu_arg_shape pixborder_hsv_shape = BU_ARG_SHAPE(
    BU_ARG_SHAPE_VECTOR3, 1, 3, "packed HSV or three components", NULL,
    pixborder_hsv_variants);

static const struct bu_arg_option pixborder_options[] = {
    BU_ARG_FLAG("h", "help", struct pixborder_args, help,
	"Print help and exit"),
    BU_ARG_ALIAS("?", NULL, "help", 1),
    BU_ARG_FLAG("a", "autosize", struct pixborder_args, autosize,
	"Infer dimensions from a named input file"),
    BU_ARG_OPTION_SHAPED("b", "border-rgb", "border-rgb",
	struct pixborder_args, self, BU_ARG_VALUE_COLOR, "red green blue",
	"Set the RGB border color using channels from 0 to 255 (default: 1 1 1)", BU_ARG_REQUIRED,
	&pixborder_rgb_shape, pixborder_border_rgb),
    BU_ARG_OPTION_SHAPED("B", "border-hsv", "border-hsv",
	struct pixborder_args, self, BU_ARG_VALUE_COLOR, "hue saturation value",
	"Set the HSV border color (H 0..360; S and V 0..1)", BU_ARG_REQUIRED,
	&pixborder_hsv_shape, pixborder_border_hsv),
    BU_ARG_OPTION_SHAPED("e", "exterior-rgb", "exterior-rgb",
	struct pixborder_args, self, BU_ARG_VALUE_COLOR, "red green blue",
	"Set the RGB exterior color using channels from 0 to 255 (default: 1 1 1)", BU_ARG_REQUIRED,
	&pixborder_rgb_shape, pixborder_exterior_rgb),
    BU_ARG_OPTION_SHAPED("E", "exterior-hsv", "exterior-hsv",
	struct pixborder_args, self, BU_ARG_VALUE_COLOR, "hue saturation value",
	"Set the HSV exterior color (H 0..360; S and V 0..1)", BU_ARG_REQUIRED,
	&pixborder_hsv_shape, pixborder_exterior_hsv),
    BU_ARG_OPTION_SHAPED("i", "interior-rgb", "interior-rgb",
	struct pixborder_args, self, BU_ARG_VALUE_COLOR, "red green blue",
	"Set the RGB interior color using channels from 0 to 255 (default: 255 255 255)", BU_ARG_REQUIRED,
	&pixborder_rgb_shape, pixborder_interior_rgb),
    BU_ARG_OPTION_SHAPED("I", "interior-hsv", "interior-hsv",
	struct pixborder_args, self, BU_ARG_VALUE_COLOR, "hue saturation value",
	"Set the HSV interior color (H 0..360; S and V 0..1)", BU_ARG_REQUIRED,
	&pixborder_hsv_shape, pixborder_interior_hsv),
    BU_ARG_OPTION_SHAPED("t", "rgb-tolerance", "rgb-tolerance",
	struct pixborder_args, self, BU_ARG_VALUE_COLOR, "red green blue",
	"Set per-channel RGB matching tolerance from 0 to 255 (default: 0 0 0)",
	BU_ARG_REQUIRED, &pixborder_rgb_shape, pixborder_tolerance_rgb),
    BU_ARG_OPTION_SHAPED("T", "hsv-tolerance", "hsv-tolerance",
	struct pixborder_args, self, BU_ARG_VALUE_COLOR, "hue saturation value",
	"Set HSV matching tolerance (H 0..360; S and V 0..1)", BU_ARG_REQUIRED,
	&pixborder_hsv_shape, pixborder_tolerance_hsv),
    BU_ARG_CUSTOM("s", "size", struct pixborder_args, self,
	pixborder_square, "pixels", "Set both image dimensions"),
    BU_ARG_CUSTOM("w", "width", struct pixborder_args, self,
	pixborder_width, "pixels", "Set image width (default: 512)"),
    BU_ARG_CUSTOM("n", "height", struct pixborder_args, self,
	pixborder_height, "pixels", "Set image height (default: 512)"),
    BU_ARG_CUSTOM("x", "left", struct pixborder_args, self,
	pixborder_left, "column", "Set the leftmost processed column (default: 0)"),
    BU_ARG_CUSTOM("X", "right", struct pixborder_args, self,
	pixborder_right, "column", "Set the rightmost processed column (default: image edge)"),
    BU_ARG_CUSTOM("y", "bottom", struct pixborder_args, self,
	pixborder_bottom, "row", "Set the lowest processed row (default: 0)"),
    BU_ARG_CUSTOM("Y", "top", struct pixborder_args, self,
	pixborder_top, "row", "Set the highest processed row (default: image edge)"),
    BU_ARG_OPTION_NULL
};

static const struct bu_arg_operand pixborder_operands[] = {
    BU_ARG_OPERAND("input", BU_ARG_VALUE_FILE, 0, 1,
	"PIX input file (default: standard input)", NULL),
    BU_ARG_OPERAND_NULL
};

static const struct bu_arg_schema pixborder_schema = BU_ARG_SCHEMA(
    "pixborder", "Outline selected color regions in a PIX image",
    pixborder_options, pixborder_operands, BU_ARG_PARSE_OPTIONS_FIRST,
    BU_ARG_SCHEMA_META_HELP(NULL, NULL, NULL, NULL, NULL));

static const struct bu_arg_grammar pixborder_grammar = {
    &pixborder_schema, NULL, NULL
};

static int
pixborder_help(FILE *stream, int status)
{
    char *help = bu_arg_schema_help(&pixborder_schema, "pixborder");
    bu_flog(stream, "%s", help ? help : "");
    bu_free(help, "pixborder help");
    return status;
}


int
main (int argc, char **argv)
{
    struct pixborder_args args;
    struct bu_vls msg = BU_VLS_INIT_ZERO;
    char *outbuf;
    unsigned char *inrow[3];
    int introspection;
    int operand_index;
    size_t i;
    ssize_t next_row;
    ssize_t prev_row;
    ssize_t this_row;
    size_t col_nm;
    size_t row_nm;

    bu_setprogname(argv[0]);
    pixborder_args_init(&args);

    introspection = bu_arg_introspect(&msg, &pixborder_grammar, argc - 1,
	(const char * const *)(argv + 1));
    if (introspection != 0) {
	bu_flog(introspection > 0 ? stdout : stderr, "%s", bu_vls_cstr(&msg));
	bu_vls_free(&msg);
	return introspection > 0 ? 0 : 1;
    }
    operand_index = bu_arg_schema_parse_complete(&pixborder_schema, &args,
	&msg, argc - 1, (const char **)(argv + 1));
    if (operand_index < 0) {
	bu_flog(stderr, "%s", bu_vls_cstr(&msg));
	bu_vls_free(&msg);
	return 1;
    }
    bu_vls_free(&msg);
    if (args.help)
	return pixborder_help(stdout, 0);

    setmode(fileno(stdin), O_BINARY);
    setmode(fileno(stdout), O_BINARY);

    autosize = args.autosize;
    file_width = args.file_width;
    file_height = args.file_height;
    left_edge = args.left_edge;
    right_edge = args.right_edge;
    bottom_edge = args.bottom_edge;
    top_edge = args.top_edge;
    colors_specified = args.colors_specified;
    tol_using_rgb = args.tol_using_rgb;
    VMOVE(border_rgb, args.border_rgb);
    VMOVE(exterior_rgb, args.exterior_rgb);
    VMOVE(interior_rgb, args.interior_rgb);
    VMOVE(rgb_tol, args.rgb_tol);
    VMOVE(border_hsv, args.border_hsv);
    VMOVE(exterior_hsv, args.exterior_hsv);
    VMOVE(interior_hsv, args.interior_hsv);
    VMOVE(hsv_tol, args.hsv_tol);

    if (operand_index >= argc - 1) {
	if (isatty(fileno(stdin)))
	    return pixborder_help(stderr, 1);
	file_name = "stdin";
	infp = stdin;
    } else {
	file_name = argv[operand_index + 1];
	infp = fopen(file_name, "rb");
	if (!infp) {
	    perror(file_name);
	    bu_flog(stderr, "Cannot open file '%s'\n", file_name);
	    return 1;
	}
	fileinput = 1;
    }

    /*
     * Autosize the input if appropriate
     */
    if (fileinput && autosize) {
	size_t w, h;

	if (fb_common_file_size(&w, &h, file_name, 3)) {
	    file_width = (long)w;
	    file_height = (long)h;
	} else
	    (void) fprintf(stderr, "pixborder: unable to autosize\n");
    }

    if (left_edge == -1)
	left_edge = 0;
    if (right_edge == -1)
	right_edge = file_width - 1;
    if (bottom_edge == -1)
	bottom_edge = 0;
    if (top_edge == -1)
	top_edge = file_height - 1;

    /*
     * Allocate a 1-scanline output buffer
     * and a circular input buffer of 3 scanlines
     */
    outbuf = (char *)bu_malloc(3*file_width, "outbuf");
    for (i = 0; i < 3; ++i)
	inrow[i] = (unsigned char *)bu_malloc(3*(file_width + 2), "inrow[i]");
    prev_row = 0;
    this_row = 1;
    next_row = 2;

    /*
     * Initialize previous-row buffer
     */
    fill_row(inrow[prev_row], file_width + 2, exterior_rgb);

    /*
     * Initialize current- and next-row buffers
     */
    if ((! read_row(inrow[this_row], file_width, infp))
	|| (! read_row(inrow[next_row], file_width, infp)))
    {
	perror(file_name);
	fprintf(stderr, "pixborder:  fread() error\n");
	bu_exit (1, NULL);
    }

    /*
     * Do the filtering
     */
    for (row_nm = 0; row_nm < file_height; ++row_nm) {
	size_t ret;

	/*
	 * Fill the output-scanline buffer
	 */
	if (((ssize_t)row_nm < bottom_edge) || ((ssize_t)row_nm > top_edge)) {
	    ret = fwrite(inrow[this_row] + 3, 3, file_width, stdout);
	    if (ret != file_width) {
		perror("stdout");
		bu_exit (2, NULL);
	    }
	} else {
	    for (col_nm = 0; col_nm < file_width; ++col_nm) {
		unsigned char *color_ptr;

		if (((ssize_t)col_nm >= left_edge) && ((ssize_t)col_nm <= right_edge)
		    && is_border(inrow[prev_row], inrow[this_row],
				 inrow[next_row], col_nm))
		    color_ptr = border_rgb;
		else
		    color_ptr = inrow[this_row] + (col_nm + 1) * 3;

		VMOVE(outbuf + col_nm * 3, color_ptr);
	    }

	    /*
	     * Write the output scanline
	     */
	    ret = fwrite(outbuf, 3, file_width, stdout);
	    if (ret != file_width) {
		perror("stdout");
		bu_exit (2, NULL);
	    }
	}

	/*
	 * Advance the circular input buffer
	 */
	prev_row = this_row;
	this_row = next_row;
	i = next_row;
	next_row = ++i % 3;

	/*
	 * Grab the next input scanline
	 */
	if (row_nm < file_height - 2) {
	    if (! read_row(inrow[next_row], file_width, infp)) {
		perror(file_name);
		(void) fprintf(stderr, "pixborder:  fread() error\n");
		bu_exit (1, NULL);
	    }
	} else {
	    fill_row(inrow[next_row], file_width + 2, exterior_rgb);
	}
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
