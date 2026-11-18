/*                        S P M - F B . C
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
/** @file spm-fb.c
 *
 */

#include "common.h"

#include <limits.h>

#include "bio.h"

#include "bu/arg.h"
#include "bu/app.h"
#include "bu/exit.h"
#include "bu/log.h"
#include "bu/malloc.h"
#include "dm.h"
#include "bn/spm.h"

struct spm_fb_args {
    struct spm_fb_args *self;
    int help;
    int square;
    const char *framebuffer;
    int width;
    int height;
};

static int
spm_fb_size(struct bu_vls *msg, const char *arg, void *storage)
{
    int size;
    struct spm_fb_args *args = storage ? *(struct spm_fb_args **)storage : NULL;

    if (!bu_arg_integer_range_from_str(msg, &size, arg, 10, 1, INT_MAX))
	return -1;
    if (args) {
	args->width = size;
	args->height = size;
    }
    return 0;
}

static const struct bu_arg_option spm_fb_options[] = {
    BU_ARG_FLAG("h", "help", struct spm_fb_args, help,
	"Print help and exit"),
    BU_ARG_ALIAS("?", NULL, "help", 1),
    BU_ARG_FLAG("s", "square", struct spm_fb_args, square,
	"Resample the sphere map to a rectangular framebuffer image"),
    BU_ARG_VALUE("F", "framebuffer", struct spm_fb_args, framebuffer,
	BU_ARG_VALUE_STRING, BU_ARG_REQUIRED, "device",
	"Select the framebuffer device instead of the configured default"),
    BU_ARG_CUSTOM("S", "size", struct spm_fb_args, self, spm_fb_size,
	"pixels", "Set both framebuffer dimensions"),
    BU_ARG_INTEGER_RANGE("W", "width", struct spm_fb_args, width,
	1, 1, 1, INT_MAX, "pixels", "Set the framebuffer width"),
    BU_ARG_INTEGER_RANGE("N", "height", struct spm_fb_args, height,
	1, 1, 1, INT_MAX, "pixels", "Set the framebuffer height"),
    BU_ARG_OPTION_NULL
};

static const struct bu_arg_operand spm_fb_operands[] = {
    BU_ARG_OPERAND_INTEGER_RANGE("vsize", 1, 1, 1, 1, 1, INT_MAX,
	"Number of sphere-map latitude bins", NULL),
    BU_ARG_OPERAND("input", BU_ARG_VALUE_FILE, 0, 1,
	"Sphere-map input file (default: standard input)", NULL),
    BU_ARG_OPERAND_NULL
};

static const struct bu_arg_schema spm_fb_schema = BU_ARG_SCHEMA(
    "spm-fb", "Display an equal-area sphere map on a framebuffer",
    spm_fb_options, spm_fb_operands, BU_ARG_PARSE_OPTIONS_FIRST,
    BU_ARG_SCHEMA_META_HELP(NULL, NULL, NULL, NULL, NULL));

static const struct bu_arg_grammar spm_fb_grammar = {
    &spm_fb_schema, NULL, NULL
};


/*
 * Displays a sphere map on a framebuffer.
 */
static void
spm_fb(struct fb *fbp, bn_spm_map_t *mapp)
{
    int j;

    for (j = 0; j < mapp->ny; j++) {
	fb_write(fbp, 0, j, mapp->xbin[j], mapp->nx[j]);
#ifdef NEVER
	for (i = 0; i < mapp->nx[j]; i++) {
	    rgb[RED] = mapp->xbin[j][i*3];
	    rgb[GRN] = mapp->xbin[j][i*3+1];
	    rgb[BLU] = mapp->xbin[j][i*3+2];
	    fb_write(fbp, i, j, (unsigned char *)rgb, 1);
	}
#endif
    }
}


/*
 * Display a square sphere map on a framebuffer.
 */
static void
spm_square(struct fb *fbp, bn_spm_map_t *mapp, int width, int height)
{
    int x, y;
    unsigned char *scanline;

    scanline = (unsigned char *)bu_malloc(width * sizeof(RGBpixel),
	"spm framebuffer scanline");

    for (y = 0; y < height; y++) {
	for (x = 0; x < width; x++) {
	    bn_spm_read(mapp, &scanline[x * sizeof(RGBpixel)],
			(double)x/(double)width,
			(double)y/(double)height);
	}
	if (fb_write(fbp, 0, y, scanline, width) != width) break;
    }
    bu_free(scanline, "spm framebuffer scanline");
}


int
main(int argc, char **argv)
{
    struct spm_fb_args args = {NULL, 0, 0, NULL, 0, 0};
    struct bu_vls msg = BU_VLS_INIT_ZERO;
    bn_spm_map_t *mp;
    struct fb *fbp;
    const char *file_name = "-";
    int vsize;
    args.self = &args;

    bu_setprogname(argv[0]);
    int introspection = bu_arg_introspect(&msg, &spm_fb_grammar, argc - 1,
	(const char * const *)(argv + 1));
    if (introspection != 0) {
	bu_flog(introspection > 0 ? stdout : stderr, "%s", bu_vls_cstr(&msg));
	bu_vls_free(&msg);
	return introspection > 0 ? 0 : 1;
    }
    int operand_index = bu_arg_schema_parse_complete(&spm_fb_schema, &args,
	&msg, argc - 1, (const char **)(argv + 1));
    if (operand_index < 0) {
	bu_flog(stderr, "%s", bu_vls_cstr(&msg));
	bu_vls_free(&msg);
	return 1;
    }
    bu_vls_free(&msg);
    if (args.help) {
	char *help = bu_arg_schema_help(&spm_fb_schema, "spm-fb");
	bu_flog(stdout, "%s", help ? help : "");
	bu_free(help, "spm-fb help");
	return 0;
    }

    if (!bu_arg_integer_range_from_str(&msg, &vsize,
	    argv[operand_index + 1], 10, 1, INT_MAX)) {
	bu_exit(1, "spm-fb: vsize: %s\n", bu_vls_cstr(&msg));
    }
    bu_vls_free(&msg);
    if (operand_index + 2 < argc) {
	file_name = argv[operand_index + 2];
    } else if (isatty(fileno(stdin))) {
	char *help = bu_arg_schema_help(&spm_fb_schema, "spm-fb");
	bu_flog(stderr, "%s", help ? help : "");
	bu_free(help, "spm-fb help");
	return 1;
    }

    if ((fbp = fb_open(args.framebuffer, args.width, args.height)) == FB_NULL)
	bu_exit(12, NULL);
    int width = fb_getwidth(fbp);
    int height = fb_getheight(fbp);

    mp = bn_spm_init(vsize, sizeof(RGBpixel));
    if (mp == BN_SPM_MAP_NULL || fbp == FB_NULL)
	bu_exit(1, NULL);

    bn_spm_load(mp, file_name);

    if (args.square)
	spm_square(fbp, mp, width, height);
    else
	spm_fb(fbp, mp);

    bn_spm_free(mp);
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
