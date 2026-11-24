/*                       F B C B A R S . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2026 United States Government as represented by
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
/** @file fbcbars.c
 *
 * Program to make vertical color scales arranged horizontally on screen.
 * The wonderful thing about standards is that there are so many to
 * choose from:
 *	FCC/EBU		Full screen is FCC pattern
 *	EIA		Uses lower and upper patterns
 *	SMPTE		Uses three patterns
 *
 * The lower portion of the screen will contain:
 *	-I, 100% white, Q, black
 *
 * In SMPTE mode, the middle portion of the screen will contain:
 *	blue, black, magenta, black, cyan, black, 75% grey
 *
 * The upper portion of the screen will contain:
 *	75% gray, yellow, cyan, green, magenta, red, blue
 *
 * In EBU/FCC mode, the whole screen will contain:
 *	100% white, yellow, cyan, green, magenta, red, blue, black
 *
 *
 * The reason for this program running directly to the framebuffer, rather
 * than generating a pix(5) format file (like pixbackgnd(1)) is twofold:
 *	1) Some framebuffers may have irregular shapes (like the SGI
 *	    3030 is "hi-res" mode.
 *	2) This program will most often be used to place an image on
 *	    a framebuffer;  other processing of this image is likely
 *	    to be a rare event.
 * Thus, this bundled version is more efficient, easier to use,
 * and more likely to have the desired effect.
 *
 * Original inspiration -
 *	"ikcolorbars" by Mike Pique, University of North Carolina.
 *
 * Details on SMPTE and FCC patterns -
 *	"bars" by Doris Kochanek, National Film Board of Canada
 *
 */

#include "common.h"

#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <stdlib.h>

#include "bio.h"

#include "bu/arg.h"
#include "bu/app.h"
#include "bu/color.h"
#include "bu/exit.h"
#include "bu/log.h"
#include "bu/malloc.h"
#include "dm.h"

enum fbcbars_mode {
    FBCBARS_EIA,
    FBCBARS_FCC,
    FBCBARS_SMPTE
};

enum fbcbars_dimension {
    FBCBARS_SIZE,
    FBCBARS_WIDTH,
    FBCBARS_HEIGHT
};

struct fbcbars_args {
    struct fbcbars_args *self;
    int help;
    const char *framebuffer;
    int width;
    int height;
    enum fbcbars_mode mode;
};

/*
 * "Since the relationship between RGB and the amplitude of YIQ in
 * the encoded signal depends on the encoder's setup value,
 * this must be defined below.  SETUP is normally 7.5 for NTSC.
 * The full luminance range is encoded into the range between
 * the setup level and 100 IRE, so Y values are effectively scaled
 * by (1-setup) when being converted to voltages.  I and Q are
 * scaled by the same amount to retain their relationship with Y.
 * To get -I and Q signals that are 40 IRE after encoding, we must
 * scale them by 1/(1-setup) before encoding."
 *
 * "The -I and Q signals are not quite standard, since there is
 * necessarily some luminance present when RGB is positive.
 * They are designed to be 40 IRE P-P after encoding."
 *
 * -- Doris Kochanek, National Film Board of Canada
 */

#define SETUP 7.5			/* black setup; 7.5 IRE */
#define COMP(x)		\
    (unsigned char)(((x) * 255.0) / (1.0 - SETUP/100.0))	/* setup compensation */

static RGBpixel fcc_all[8] = {
    /* 100% white */{255, 255, 255},
    /* yellow */	{191, 191, 0},
    /* cyan */	{0, 191, 191},
    /* green */	{0, 191, 0},
    /* magenta */	{191, 0, 191},
    /* red */	{191, 0, 0},
    /* blue */	{0, 0, 191},
    /* black */	{0, 0, 0}
};


/*
 * SMPTE bars can be useful for aligning color demodulators
 */
static RGBpixel smpte_middle[7] = {
    /* All bars at 75%, no blue, reversed side-to-side from eia_top */
    /* blue */	{0, 0, 191},
    /* black(red)*/	{0, 0, 0},
    /* magenta */	{191, 0, 191},
    /* black(green)*/{0, 0, 0},
    /* cyan */	{0, 191, 191},
    /* black(yellow)*/{0, 0, 0},
    /* 75% grey */ 	{191, 191, 191},
};


static RGBpixel eia_top[7] = {
    /* All bars at 75% */
    /* grey */ 	{191, 191, 191},
    /* yel */	{191, 191, 0},
    /* cyan */	{0, 191, 191},
    /* green */	{0, 191, 0},
    /* magenta */	{191, 0, 191},
    /* red */	{191, 0, 0},
    /* blue */	{0, 0, 191}
};


static RGBpixel botpart[5] = {
#ifndef Floating_Initializers
    /* Most systems can't handle floating-point formulas as initializers */
    { 0,		68,		114 },		/* 40 IRE -I */
    { 255,		255,		255 },		/* 100% white */
    { 69,		0,		129 },		/* 40 IRE Q */
    { 0,		0,		0 },		/* black */
    { 0,		0,		0 }		/* black */
#else
    { 0,		COMP(0.2472),	COMP(0.4123) },	/* 40 IRE -I */
    { 255,		255,		255 },		/* 100% white */
    { COMP(0.2508),	0,		COMP(0.4670) },	/* 40 IRE Q */
    { 0,		0,		0 },		/* black */
    { 0,		0,		0 }		/* black */
#endif
};


static struct fbcbars_args *
fbcbars_args_from_storage(void *storage)
{
    return storage ? *(struct fbcbars_args **)storage : NULL;
}

static int
fbcbars_dimension_value(struct bu_vls *msg, const char *arg, void *storage,
	enum fbcbars_dimension dimension)
{
    struct fbcbars_args *args = fbcbars_args_from_storage(storage);
    int value;

    if (!bu_arg_integer_range_from_str(msg, &value, arg, 10, 1, INT_MAX))
	return -1;
    if (args) {
	if (dimension == FBCBARS_SIZE)
	    args->width = args->height = value;
	else if (dimension == FBCBARS_WIDTH)
	    args->width = value;
	else
	    args->height = value;
    }
    return 0;
}

#define FBCBARS_DIMENSION_CALLBACK(_name, _dimension) \
    static int _name(struct bu_vls *msg, const char *arg, void *storage) \
    { \
	return fbcbars_dimension_value(msg, arg, storage, _dimension); \
    }

FBCBARS_DIMENSION_CALLBACK(fbcbars_size, FBCBARS_SIZE)
FBCBARS_DIMENSION_CALLBACK(fbcbars_width, FBCBARS_WIDTH)
FBCBARS_DIMENSION_CALLBACK(fbcbars_height, FBCBARS_HEIGHT)

#define FBCBARS_MODE_CALLBACK(_name, _mode) \
    static int _name(struct bu_vls *UNUSED(msg), const char *UNUSED(arg), void *storage) \
    { \
	if (storage) \
	    *(enum fbcbars_mode *)storage = _mode; \
	return 0; \
    }

FBCBARS_MODE_CALLBACK(fbcbars_eia, FBCBARS_EIA)
FBCBARS_MODE_CALLBACK(fbcbars_fcc, FBCBARS_FCC)
FBCBARS_MODE_CALLBACK(fbcbars_smpte, FBCBARS_SMPTE)

static const struct bu_arg_option fbcbars_options[] = {
    BU_ARG_FLAG("h", "help", struct fbcbars_args, help,
	"Print help and exit"),
    BU_ARG_ALIAS("?", NULL, "help", 1),
    BU_ARG_CUSTOM_FLAG("e", "eia", "eia", struct fbcbars_args, mode,
	fbcbars_eia, "Display EIA bars with -I, white, Q, and black below seven 75% colors"),
    BU_ARG_CUSTOM_FLAG("f", "fcc", "fcc", struct fbcbars_args, mode,
	fbcbars_fcc, "Display full-screen FCC/EBU bars from white through 75% colors to black"),
    BU_ARG_CUSTOM_FLAG("s", "smpte", "smpte", struct fbcbars_args, mode,
	fbcbars_smpte, "Display SMPTE color-demodulator alignment bars (default)"),
    BU_ARG_VALUE("F", "framebuffer", struct fbcbars_args, framebuffer,
	BU_ARG_VALUE_STRING, BU_ARG_REQUIRED, "device",
	"Select the framebuffer instead of the FB_FILE environment default"),
    BU_ARG_CUSTOM("S", "size", struct fbcbars_args, self, fbcbars_size,
	"pixels", "Set both framebuffer dimensions"),
    BU_ARG_CUSTOM("W", "width", struct fbcbars_args, self, fbcbars_width,
	"pixels", "Set the framebuffer width"),
    BU_ARG_CUSTOM("N", "height", struct fbcbars_args, self, fbcbars_height,
	"scanlines", "Set the framebuffer height"),
    BU_ARG_OPTION_NULL
};

static const char * const fbcbars_modes[] = {"eia", "fcc", "smpte", NULL};

static const struct bu_arg_constraint fbcbars_constraints[] = {
    BU_ARG_CONSTRAINT(BU_ARG_CONSTRAINT_OPTION_COUNT, BU_ARG_CONDITION_ALWAYS,
	fbcbars_modes, 0, 1, "select at most one color-bar standard"),
    BU_ARG_CONSTRAINT_NULL
};

static const struct bu_arg_schema fbcbars_schema = BU_ARG_SCHEMA(
    "fbcbars", "Display standard color bars on a framebuffer",
    fbcbars_options, NULL, BU_ARG_PARSE_INTERSPERSED,
    BU_ARG_SCHEMA_META_HELP(NULL, fbcbars_constraints, NULL, NULL, NULL));

static const struct bu_arg_grammar fbcbars_grammar = {
    &fbcbars_schema, NULL, NULL
};

static int
fbcbars_help(FILE *stream, int status)
{
    char *help = bu_arg_schema_help(&fbcbars_schema, "fbcbars");
    bu_flog(stream, "%s", help ? help : "");
    bu_free(help, "fbcbars help");
    return status;
}

static int
fbcbars_bar_index(int position, int count, int total)
{
    return (int)((int64_t)position * count / total);
}

int
main(int argc, char **argv)
{
    struct fbcbars_args args = {NULL, 0, NULL, 0, 0, FBCBARS_SMPTE};
    struct bu_vls msg = BU_VLS_INIT_ZERO;
    int x, y;
    struct fb *fbp;
    unsigned char *scanline;
    args.self = &args;

    bu_setprogname(argv[0]);
    int introspection = bu_arg_introspect(&msg, &fbcbars_grammar, argc - 1,
	(const char * const *)(argv + 1));
    if (introspection != 0) {
	bu_flog(introspection > 0 ? stdout : stderr, "%s", bu_vls_cstr(&msg));
	bu_vls_free(&msg);
	return introspection > 0 ? 0 : 1;
    }
    if (bu_arg_schema_parse_complete(&fbcbars_schema, &args, &msg, argc - 1,
	    (const char **)(argv + 1)) < 0) {
	bu_flog(stderr, "%s", bu_vls_cstr(&msg));
	bu_vls_free(&msg);
	return 1;
    }
    bu_vls_free(&msg);
    if (args.help)
	return fbcbars_help(stdout, 0);
    if (argc == 1 && isatty(fileno(stdin)) && isatty(fileno(stdout)))
	return fbcbars_help(stderr, 1);

    if ((fbp = fb_open(args.framebuffer, args.width, args.height)) == NULL)
	bu_exit(12, NULL);

    /* Get the screen size we were actually given */
    args.width = fb_getwidth(fbp);
    args.height = fb_getheight(fbp);
    scanline = (unsigned char *)bu_calloc((size_t)args.width,
	sizeof(RGBpixel), "scanline");

    /*
     * Operation is bottom-to-top.
     */
    switch (args.mode) {
	case FBCBARS_FCC:
	    for (x=0; x<args.width; x++) {
		COPYRGB(&scanline[3*x], fcc_all[fbcbars_bar_index(x, 8, args.width)]);
	    }
	    for (y=0; y<args.height; y++)
		fb_write(fbp, 0, y, scanline, args.width);
	    break;

	case FBCBARS_EIA:
	    /*
	     * Build bottom line, and send it for 1/4th of the screen,
	     * then build the top line, and fill the rest of the screen.
	     */
	    for (x=0; x<args.width; x++) {
		COPYRGB(&scanline[3*x], botpart[fbcbars_bar_index(x, 5, args.width)]);
	    }
	    for (y=0; y<(args.height/4); y++)
		fb_write(fbp, 0, y, scanline, args.width);

	    for (x=0; x<args.width; x++) {
		COPYRGB(&scanline[3*x], eia_top[fbcbars_bar_index(x, 7, args.width)]);
	    }
	    for (; y<args.height; y++)
		fb_write(fbp, 0, y, scanline, args.width);
	    break;

	case FBCBARS_SMPTE:
	    /*
	     * Build bottom line, and send it for 3/16th of the screen,
	     * then send the SMPTE middle for 1/16th of the screen,
	     * then build the top line, and fill the rest of the screen.
	     * (Convert upper 1/4 of EIA -I white Q black to smpte)
	     */
	    for (x=0; x<args.width; x++) {
		COPYRGB(&scanline[3*x], botpart[fbcbars_bar_index(x, 5, args.width)]);
	    }
	    for (y=0; y<fbcbars_bar_index(args.height, 3, 16); y++)
		fb_write(fbp, 0, y, scanline, args.width);

	    for (x=0; x<args.width; x++) {
		COPYRGB(&scanline[3*x], smpte_middle[fbcbars_bar_index(x, 7, args.width)]);
	    }
	    for (; y<fbcbars_bar_index(args.height, 4, 16); y++)
		fb_write(fbp, 0, y, scanline, args.width);

	    for (x=0; x<args.width; x++) {
		COPYRGB(&scanline[3*x], eia_top[fbcbars_bar_index(x, 7, args.width)]);
	    }
	    for (; y<args.height; y++)
		fb_write(fbp, 0, y, scanline, args.width);
	    break;
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
