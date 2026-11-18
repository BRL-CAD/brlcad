/*                         F B - F B . C
 * BRL-CAD
 *
 * Copyright (c) 1991-2026 United States Government as represented by
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
/** @file fb-fb.c
 *
 * Program to copy the entire image on a framebuffer to another
 * framebuffer.
 *
 */

#include "common.h"

#include "bu/arg.h"
#include "bu/app.h"
#include "bu/log.h"
#include "bu/malloc.h"
#include "dm.h"

struct fb_fb_args {
    int help;
    int verbose;
    const char *output_framebuffer;
};

static const struct bu_arg_option fb_fb_options[] = {
    BU_ARG_FLAG("h", "help", struct fb_fb_args, help,
	"Print help and exit"),
    BU_ARG_ALIAS("?", NULL, "help", 1),
    BU_ARG_COUNTING_FLAG("v", "verbose", struct fb_fb_args, verbose,
	BU_ARG_VALUE_FLAG, "Increase progress reporting"),
    BU_ARG_VALUE("F", "output-framebuffer", struct fb_fb_args,
	output_framebuffer, BU_ARG_VALUE_STRING, BU_ARG_REQUIRED, "device",
	"Select the output framebuffer instead of the configured default"),
    BU_ARG_OPTION_NULL
};

static const struct bu_arg_operand fb_fb_option_output_operands[] = {
    BU_ARG_OPERAND("input-framebuffer", BU_ARG_VALUE_STRING, 1, 1,
	"Source framebuffer device", NULL),
    BU_ARG_OPERAND_NULL
};

static const struct bu_arg_operand fb_fb_positional_output_operands[] = {
    BU_ARG_OPERAND("input-framebuffer", BU_ARG_VALUE_STRING, 1, 1,
	"Source framebuffer device", NULL),
    BU_ARG_OPERAND("output-framebuffer", BU_ARG_VALUE_STRING, 0, 1,
	"Destination framebuffer device (default: configured framebuffer)", NULL),
    BU_ARG_OPERAND_NULL
};

static const char * const fb_fb_output_option[] = {"output-framebuffer", NULL};
static const struct bu_arg_schema_case fb_fb_cases[] = {
    BU_ARG_SCHEMA_CASE("option-output", "Name the output with -F",
	BU_ARG_CONDITION_ANY_OPTION_PRESENT, fb_fb_output_option,
	fb_fb_option_output_operands, NULL),
    BU_ARG_SCHEMA_CASE_DEFAULT("positional-output",
	"Name the output as an optional second operand",
	fb_fb_positional_output_operands, NULL),
    BU_ARG_SCHEMA_CASE_NULL
};

static const struct bu_arg_schema fb_fb_schema = BU_ARG_SCHEMA(
    "fb-fb", "Copy one framebuffer image to another framebuffer",
    fb_fb_options, NULL, BU_ARG_PARSE_OPTIONS_FIRST,
    BU_ARG_SCHEMA_META_HELP(NULL, NULL, NULL, NULL, fb_fb_cases));

static const struct bu_arg_grammar fb_fb_grammar = {
    &fb_fb_schema, NULL, NULL
};


int
main(int argc, char **argv)
{
    struct fb_fb_args args = {0, 0, NULL};
    struct bu_vls msg = BU_VLS_INIT_ZERO;
    int y;
    struct fb *in_fbp, *out_fbp;
    const char *in_fb_name;
    const char *out_fb_name;
    int scr_width;
    int scr_height;
    int n, m;
    int height;

    unsigned char *scanline = NULL;    /* 1 scanline pixel buffer */
    int scanbytes;              /* # of bytes of scanline */
    int scanpix;                /* # of pixels of scanline */
    int streamline;             /* # scanlines to do at once */

    bu_setprogname(argv[0]);
    int introspection = bu_arg_introspect(&msg, &fb_fb_grammar, argc - 1,
	(const char * const *)(argv + 1));
    if (introspection != 0) {
	bu_flog(introspection > 0 ? stdout : stderr, "%s", bu_vls_cstr(&msg));
	bu_vls_free(&msg);
	return introspection > 0 ? 0 : 1;
    }
    int operand_index = bu_arg_schema_parse_complete(&fb_fb_schema, &args,
	&msg, argc - 1, (const char **)(argv + 1));
    if (operand_index < 0) {
	bu_flog(stderr, "%s", bu_vls_cstr(&msg));
	bu_vls_free(&msg);
	return 1;
    }
    bu_vls_free(&msg);
    if (args.help) {
	char *help = bu_arg_schema_help(&fb_fb_schema, "fb-fb");
	bu_flog(stdout, "%s", help ? help : "");
	bu_free(help, "fb-fb help");
	return 0;
    }

    in_fb_name = argv[operand_index + 1];
    out_fb_name = args.output_framebuffer;
    if (!out_fb_name && operand_index + 2 < argc)
	out_fb_name = argv[operand_index + 2];

    if (args.verbose)
	bu_flog(stderr, "fb-fb: infb=%s, outfb=%s\n", in_fb_name,
	    out_fb_name ? out_fb_name : "(default)");

    if ((in_fbp = fb_open(in_fb_name, 0, 0)) == NULL) {
	if (in_fb_name)
	    bu_flog(stderr, "fb-fb: unable to open input '%s'\n", in_fb_name);
	bu_exit(12, NULL);
    }

    /* Get the screen size we were given */
    scr_width = fb_getwidth(in_fbp);
    scr_height = fb_getheight(in_fbp);

    if (args.verbose)
	bu_flog(stderr, "fb-fb: width=%d height=%d\n", scr_width, scr_height);

    if ((out_fbp = fb_open(out_fb_name, scr_width, scr_height)) == FB_NULL) {
	if (out_fb_name)
	    bu_flog(stderr, "fb-fb: unable to open output '%s'\n", out_fb_name);
	bu_exit(12, NULL);
    }

    scanpix = scr_width;			/* # pixels on scanline */
    streamline = 64;			/* # scanlines per block */
    scanbytes = scanpix * streamline * sizeof(RGBpixel);
    scanline = (unsigned char *)bu_malloc((size_t)scanbytes, "scanline");

    /* Bottom to top with multi-line reads & writes */
    for (y = 0; y < scr_height; y += streamline) {
	if (y+streamline > scr_height)
	    streamline = scr_height-y;
	if (args.verbose)
	    bu_flog(stderr, "fb-fb: y=%d, nlines=%d\n", y, streamline);
	n = fb_readrect(in_fbp, 0, y, scr_width, streamline,
			scanline);
	if (n <= 0) break;
	height = streamline;
	if (n != scr_width * streamline) {
	    height = (n+scr_width-1)/scr_width;
	    if (height <= 0) break;
	}
	m = fb_writerect(out_fbp, 0, y, scr_width, height,
			 scanline);
	if (m != scr_width*height)
	    bu_flog(stderr,
		    "fb-fb: fb_writerect(x=0, y=%d, w=%d, h=%d) failure, ret=%d, s/b=%d\n",
		    y, scr_width, height, m, scanbytes);
    }
    fb_close(in_fbp);
    fb_close(out_fbp);
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
