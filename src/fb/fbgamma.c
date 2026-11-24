/*                       F B G A M M A . C
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
/** @file fbgamma.c
 *
 * Load a gamma correcting colormap into a framebuffer.
 *
 */

#include "common.h"

#include <math.h>

#include "bu/arg.h"
#include "bu/app.h"
#include "bu/exit.h"
#include "bu/log.h"
#include "bu/malloc.h"
#include "dm.h"

static const unsigned char rampval[10] = {255, 128, 64, 32, 16, 8, 4, 2, 1, 0};
static int x, y, scr_width, scr_height, patch_width, patch_height;
static unsigned char *altline;
static unsigned char *line;

#define FBGAMMA_MIN_ABS_GAMMA 1.0e-10

struct fbgamma_args {
    int help;
    int high_resolution;
    int overlay;
    int image;
    const char *framebuffer;
};

static int
fbgamma_toggle(struct bu_vls *UNUSED(msg), const char *UNUSED(arg), void *storage)
{
    if (storage)
	*(int *)storage = !*(int *)storage;
    return 0;
}

static int
fbgamma_value_validate(struct bu_vls *msg, const char *arg)
{
    double gamma;

    if (!bu_arg_double_from_str(msg, &gamma, arg))
	return -1;
    if (fabs(gamma) < FBGAMMA_MIN_ABS_GAMMA) {
	if (msg)
	    bu_vls_printf(msg, "gamma magnitude must be at least %g\n",
		FBGAMMA_MIN_ABS_GAMMA);
	return -1;
    }
    return 0;
}

static const struct bu_arg_variant fbgamma_variants[] = {
    BU_ARG_VARIANT("uniform", "gamma", 1, "One gamma for all color channels"),
    BU_ARG_VARIANT("rgb", "red-gamma green-gamma blue-gamma", 3,
	"Independent red, green, and blue gamma values"),
    BU_ARG_VARIANT_NULL
};

static const struct bu_arg_shape fbgamma_shape = BU_ARG_SHAPE(
    BU_ARG_SHAPE_CUSTOM, 1, 3, "one uniform or three channel gamma values",
    NULL, fbgamma_variants);

static const struct bu_arg_option fbgamma_options[] = {
    BU_ARG_FLAG("h", "help", struct fbgamma_args, help,
	"Print help and exit"),
    BU_ARG_ALIAS("?", NULL, "help", 1),
    BU_ARG_FLAG("H", "high-resolution", struct fbgamma_args, high_resolution,
	"Request a 1024 by 1024 framebuffer"),
    BU_ARG_FLAG("o", "overlay", struct fbgamma_args, overlay,
	"Apply correction to the current colormap"),
    BU_ARG_CUSTOM_FLAG("i", "image", "image", struct fbgamma_args, image,
	fbgamma_toggle, "Toggle the calibration image"),
    BU_ARG_VALUE("F", "framebuffer", struct fbgamma_args, framebuffer,
	BU_ARG_VALUE_STRING, BU_ARG_REQUIRED, "device",
	"Select the framebuffer device"),
    BU_ARG_OPTION_NULL
};

static const struct bu_arg_operand fbgamma_operands[] = {
    BU_ARG_OPERAND_SHAPED("gamma", BU_ARG_VALUE_NUMBER, 1, 3,
	fbgamma_value_validate, "Display gamma value", NULL, &fbgamma_shape),
    BU_ARG_OPERAND_NULL
};

static int
fbgamma_validate(const struct bu_arg_schema *schema, size_t argc,
	const char **argv, size_t cursor_arg, struct bu_arg_validate_result *result)
{
    if (!result || cursor_arg < argc || result->state == BU_ARG_VALIDATE_INVALID)
	return 0;

    size_t gamma_count = bu_arg_schema_operand_count(schema, argc, argv);
    if (gamma_count == 2) {
	result->state = BU_ARG_VALIDATE_INCOMPLETE;
	result->token_start = argc;
	result->token_end = argc;
	result->expected = BU_ARG_EXPECT_OPERAND;
	result->hint = "supply one gamma or all three channel gamma values";
    }
    return 0;
}

static const struct bu_arg_schema fbgamma_schema = BU_ARG_SCHEMA(
    "fbgamma", "Load a gamma-correcting framebuffer colormap",
    fbgamma_options, fbgamma_operands, BU_ARG_PARSE_INTERSPERSED,
    BU_ARG_SCHEMA_META_HELP(NULL, NULL, fbgamma_validate, NULL, NULL));

static const struct bu_arg_grammar fbgamma_grammar = {
    &fbgamma_schema, NULL, NULL
};


static void
mk_ramp(struct fb *fb_i, int r, int g, int b, int n)
{

    /* grey ramp */
    for (x=0; x < scr_width; ++x) {
	if (r) line[x*3] = rampval[x / patch_width + 1];
	else line[x*3] = 0;
	if (g) line[x*3+1] = rampval[x / patch_width + 1];
	else line[x*3+1] = 0;
	if (b) line[x*3+2] = rampval[x / patch_width + 1];
	else line[x*3+2] = 0;
    }
    for (y=patch_height*n; y < patch_height*(n+1) && y < scr_height; ++y) {
	fb_write(fb_i, 0, y, line, scr_width);
    }

    for (x=0; x < scr_width; ++x) {
	if (r) line[x*3] = rampval[x / patch_width];
	else line[x*3] = 0;
	if (g) line[x*3+1] = rampval[x / patch_width];
	else line[x*3+1] = 0;
	if (b) line[x*3+2] = rampval[x / patch_width];
	else line[x*3+2] = 0;
    }
    for (y=patch_height*(n+1); y < patch_height*(n+2) && y < scr_height; y += 2) {
	fb_write(fb_i, 0, y, altline, scr_width);
	fb_write(fb_i, 0, y+1, line, scr_width);
    }
}


static void
disp_image(struct fb *fb_i)
{

    scr_width = fb_getwidth(fb_i);
    scr_height = fb_getheight(fb_i);

    patch_width = scr_width / 8;
    patch_height = scr_height / 14;

    line = (unsigned char *) bu_malloc(scr_width*3, "line");
    altline = (unsigned char *) bu_calloc(scr_width*3, sizeof(unsigned char), "altline");

    mk_ramp(fb_i, 1, 1, 1, 0);
    mk_ramp(fb_i, 1, 0, 0, 2);
    mk_ramp(fb_i, 1, 1, 0, 4);
    mk_ramp(fb_i, 0, 1, 0, 6);
    mk_ramp(fb_i, 0, 1, 1, 8);
    mk_ramp(fb_i, 0, 0, 1, 10);
    mk_ramp(fb_i, 1, 0, 1, 12);

    (void)bu_free(line, "line");
    (void)bu_free(altline, "altline");
}


int
main(int argc, char **argv)
{
    struct fbgamma_args args = {0, 0, 0, 0, NULL};
    struct bu_vls msg = BU_VLS_INIT_ZERO;
    int i;
    int onegamma = 0;
    int fbsize;
    double gamr = 0, gamg = 0, gamb = 0;	/* gamma's */
    double f;
    ColorMap cm;
    struct fb *fbp;

    bu_setprogname(argv[0]);
    int introspection = bu_arg_introspect(&msg, &fbgamma_grammar, argc - 1,
	(const char * const *)(argv + 1));
    if (introspection != 0) {
	bu_flog(introspection > 0 ? stdout : stderr, "%s", bu_vls_cstr(&msg));
	bu_vls_free(&msg);
	return introspection > 0 ? 0 : 1;
    }
    int operand_index = bu_arg_schema_parse_complete(&fbgamma_schema, &args,
	&msg, argc - 1, (const char **)(argv + 1));
    if (operand_index < 0) {
	bu_flog(stderr, "%s", bu_vls_cstr(&msg));
	bu_vls_free(&msg);
	return 1;
    }
    bu_vls_free(&msg);
    if (args.help) {
	char *help = bu_arg_schema_help(&fbgamma_schema, "fbgamma");
	bu_flog(stdout, "%s", help ? help : "");
	bu_free(help, "fbgamma help");
	return 0;
    }

    int gamma_count = argc - operand_index - 1;
    if (gamma_count == 1) {
	/* single value for all channels */
	if (!bu_arg_double_from_str(NULL, &f, argv[operand_index + 1]))
	    bu_exit(1, "fbgamma: internal gamma validation mismatch\n");
	gamr = gamg = gamb = 1.0 / f;
	onegamma = 1;
    } else {
	/* different RGB values */
	if (!bu_arg_double_from_str(NULL, &f, argv[operand_index + 1]))
	    bu_exit(1, "fbgamma: internal red gamma validation mismatch\n");
	gamr = 1.0 / f;
	if (!bu_arg_double_from_str(NULL, &f, argv[operand_index + 2]))
	    bu_exit(1, "fbgamma: internal green gamma validation mismatch\n");
	gamg = 1.0 / f;
	if (!bu_arg_double_from_str(NULL, &f, argv[operand_index + 3]))
	    bu_exit(1, "fbgamma: internal blue gamma validation mismatch\n");
	gamb = 1.0 / f;
    }

    fbsize = args.high_resolution ? 1024 : 512;
    if ((fbp = fb_open(args.framebuffer, fbsize, fbsize)) == FB_NULL) {
	bu_exit(2, "Unable to open framebuffer\n");
    }

    /* draw the gamma image if requested */
    if (args.image) disp_image(fbp);

    /* get the starting map */
    if (args.overlay) {
	fb_rmap(fbp, &cm);
    } else {
	/* start with a linear map */
	for (i = 0; i < 256; i++) {
	    cm.cm_red[i] = cm.cm_green[i]
		= cm.cm_blue[i] = i << 8;
	}
    }

    /* apply the gamma(s) */
    for (i = 0; i < 256; i++) {
	if (gamr < 0)
	    cm.cm_red[i] = 65535 * pow((double)cm.cm_red[i] / 65535.0, -1.0/gamr);
	else
	    cm.cm_red[i] = 65535 * pow((double)cm.cm_red[i] / 65535.0, gamr);
	if (onegamma && !args.overlay) {
	    cm.cm_green[i] = cm.cm_red[i];
	    cm.cm_blue[i]  = cm.cm_red[i];
	} else {
	    if (gamg < 0)
		cm.cm_green[i] = 65535 * pow((double)cm.cm_green[i] / 65535.0, -1.0/gamg);
	    else
		cm.cm_green[i] = 65535 * pow((double)cm.cm_green[i] / 65535.0, gamg);
	    if (gamb < 0)
		cm.cm_blue[i]  = 65535 * pow((double)cm.cm_blue[i] / 65535.0, -1.0/gamb);
	    else
		cm.cm_blue[i]  = 65535 * pow((double)cm.cm_blue[i] / 65535.0, gamb);
	}
    }

    fb_wmap(fbp, &cm);
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
