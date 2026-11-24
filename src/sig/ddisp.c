/*                         D D I S P . C
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
 */
/** @file ddisp.c
 *
 * Data Display - shows doubles on a framebuffer in various ways.
 *
 */

#include "common.h"

#include <string.h>

#include "bio.h"

#include "bu/arg.h"
#include "bu/app.h"
#include "bu/color.h"
#include "bu/exit.h"
#include "bu/malloc.h"
#include "bu/snooze.h"
#include "dm.h"


#define VERT 1
#define BARS 2
#define DEFAULT_WIDTH 512

struct ddisp_args {
    int help;
    int vertical;
    int bars;
    int pause;
    int clear;
};

static const struct bu_arg_option ddisp_options[] = {
    BU_ARG_FLAG("h", "help", struct ddisp_args, help, "Print help and exit"),
    BU_ARG_ALIAS("H", NULL, "help", 0),
    BU_ARG_ALIAS("?", NULL, "help", 1),
    BU_ARG_FLAG("v", "vertical", struct ddisp_args, vertical,
	"Plot each sample as a vertical-positioned point"),
    BU_ARG_FLAG("b", "bars", struct ddisp_args, bars,
	"Plot each sample as a vertical bar"),
    BU_ARG_FLAG("p", "pause", struct ddisp_args, pause,
	"Pause three seconds after each record"),
    BU_ARG_FLAG("c", "clear", struct ddisp_args, clear,
	"Clear the framebuffer before each record"),
    BU_ARG_OPTION_NULL
};

static const struct bu_arg_operand ddisp_operands[] = {
    BU_ARG_OPERAND_INTEGER_RANGE("width", 0, 1, 1, 1, 1, BU_PAGE_SIZE,
	"Input values per display record (default: 512)", NULL),
    BU_ARG_OPERAND_NULL
};

static const struct bu_arg_schema ddisp_schema = BU_ARG_SCHEMA(
    "ddisp", "Plot a stream of doubles on a framebuffer", ddisp_options,
    ddisp_operands, BU_ARG_PARSE_OPTIONS_FIRST,
    BU_ARG_SCHEMA_META_HELP(NULL, NULL, NULL, NULL, NULL));

static const struct bu_arg_grammar ddisp_grammar = {&ddisp_schema, NULL, NULL};


static void
lineout(struct fb *fbp, double *dat, int n)
{
    static int y = 0;
    int i, value;
    RGBpixel lbuf[1024*4];

    if (n > fb_getwidth(fbp)) n = fb_getwidth(fbp);

    for (i = 0; i < n; i++) {
	/* Magnitude version */
	value = dat[i] * 255.9;
	if (value < 0) value = 0;
	else if (value > 255) value = 255;
	lbuf[i][RED] = lbuf[i][GRN] = lbuf[i][BLU] = value;
    }
    fb_write(fbp, 0, y, (unsigned char *)lbuf, n);

    /* Next screen position */
    y = (y + 1) % fb_getheight(fbp);
}


/*
 * Display doubles.
 * +/- 1.0 in, becomes +/- 128 from center Y.
 */
static void
disp_inten(struct fb *fbp, double *buf, int size)
{
    int x, y;
    RGBpixel color;

/* color.red = color.green = color.blue = 255;*/

    if (size > fb_getwidth(fbp)) size = fb_getwidth(fbp);

    for (x = 0; x < size; x++) {
	y = buf[x] * 128;
#ifdef OVERLAY
	fb_read(fbp, x, y+255, color, 1);
#else
	color[RED] = color[BLU] = 0;
#endif
	color[GRN] = 255;
	fb_write(fbp, x, y+255, color, 1);
    }
}


/*
 * Display doubles.
 * +/- 1.0 in, becomes +/- 128 from center Y.
 */
static void
disp_bars(struct fb *fbp, double *buf, int size)
{
    int x, y;
    RGBpixel color;

/* color.red = color.green = color.blue = 255;*/

    if (size > fb_getwidth(fbp)) size = fb_getwidth(fbp);

    for (x = 0; x < size; x++) {
	if (buf[x] > 1.0) {
	    y = 128;
	} else if (buf[x] < -1.0) {
	    y = -128;
	} else {
	    y = buf[x] * 128;
	}
#ifdef OVERLAY
	fb_read(fbp, x, y+255, color, 1);
#else
	color[RED] = color[BLU] = 0;
#endif
	color[GRN] = 255;
	if (y > 0) {
	    while (y >= 0) {
		fb_write(fbp, x, y+255, color, 1);
		y--;
	    }
	} else {
	    while (y <= 0) {
		fb_write(fbp, x, y+255, color, 1);
		y++;
	    }
	}
    }
}


int
main(int argc, char **argv)
{
    struct fb *fbp = NULL;
    double buf[BU_PAGE_SIZE];

    int n, L;
    int mode = 0;
    int fbsize = DEFAULT_WIDTH;
    struct ddisp_args args = {0};
    struct bu_vls msg = BU_VLS_INIT_ZERO;

    bu_setprogname(argv[0]);

    int introspection = bu_arg_introspect(&msg, &ddisp_grammar, argc - 1,
	(const char * const *)(argv + 1));
    if (introspection != 0) {
	bu_flog(introspection > 0 ? stdout : stderr, "%s", bu_vls_cstr(&msg));
	bu_vls_free(&msg);
	return introspection > 0 ? 0 : 1;
    }
    argc--; argv++;
    int operand_index = bu_arg_schema_parse_complete(&ddisp_schema, &args,
	&msg, argc, (const char **)argv);
    if (operand_index < 0)
	bu_exit(1, "%s", bu_vls_cstr(&msg));
    bu_vls_free(&msg);
    if (args.help) {
	char *help = bu_arg_schema_help(&ddisp_schema, "ddisp");
	bu_flog(stdout, "%s", help ? help : "");
	bu_free(help, "ddisp help");
	return 0;
    }
    argc -= operand_index;
    argv += operand_index;

    if (args.vertical && args.bars)
	bu_exit(1, "ddisp: --vertical and --bars are mutually exclusive\n");
    if (argc && !bu_arg_integer_range_from_str(&msg, &L, argv[0], 10,
	1, BU_PAGE_SIZE))
	bu_exit(1, "ddisp: width: %s", bu_vls_cstr(&msg));
    if (!argc)
	L = DEFAULT_WIDTH;
    if (args.vertical)
	mode = VERT;
    else if (args.bars)
	mode = BARS;

    if (isatty(fileno(stdin))) {
	bu_exit(1, "ddisp requires redirected binary input\n");
    }

    if ((fbp = fb_open(NULL, fbsize, fbsize)) == FB_NULL) {
	bu_exit(2, "Unable to open framebuffer\n");
    }

    while ((n = fread(buf, sizeof(*buf), L, stdin)) > 0) {
	/* XXX - width hack */
	if (n > fb_getwidth(fbp))
	    n = fb_getwidth(fbp);

	if (args.clear)
	    fb_clear(fbp, PIXEL_NULL);
	if (mode == VERT)
	    disp_inten(fbp, buf, n);
	else if (mode == BARS)
	    disp_bars(fbp, buf, n);
	else
	    lineout(fbp, buf, n);
	if (args.pause)
	    bu_snooze(BU_SEC2USEC(3));
    }
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
