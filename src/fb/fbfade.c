/*                        F B F A D E . C
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
/** @file fbfade.c
 *
 * fade in or out a frame buffer image
 *
 * This program displays a frame buffer image gradually, randomly
 * selecting the pixel display sequence.  (Suggested by Gary Moss.)
 * It requires fast single-pixel write support for best effect.
 *
 * Options:
 *
 * "-H" assumes 1024x1024 default input size instead of 512x512
 *
 * "-f in_fb_file" reads from the specified frame buffer file instead
 * of assuming constant black ("fade out") value
 *
 * "-s size" is the input size (width & height)
 *
 * "-w width" is the input width
 *
 * "-n height" is the input height
 *
 * "-F out_fb_file" writes to the specified frame buffer file instead of
 * the one specified by the FB_FILE environment variable (the default
 * frame buffer, if no FB_FILE)
 *
 * "-S size" is the output size (width & height)
 *
 * "-W width" is the output width
 *
 * "-N height" is the output height
 *
 * "out_fb_file" is the same as -F out_fb_file, for convenience
 */

#include "common.h"

#include <limits.h>
#include <signal.h>
#include <stdlib.h>

#include "bio.h"

#include "bu/arg.h"
#include "bu/app.h"
#include "bu/log.h"
#include "bu/malloc.h"
#include "vmath.h"
#include "dm.h"			/* BRL-CAD package libfb.a interface */


static int hires = 0;			/* set for 1024x1024; clear for 512x512 */
static const char *in_fb_file = NULL;	/* input image name */
static const char *out_fb_file = NULL;	/* output frame buffer name */
static struct fb *fbp = FB_NULL;		/* libfb input/output handle */
static int src_width = 0;		/* input image width */
static int src_height = 0;		/* input image height */
static int dst_width = 0;		/* output frame buffer size */
static int dst_height = 0;		/* output frame buffer size */
static RGBpixel *pix;			/* input image */
static RGBpixel bg = { 0, 0, 0 };	/* background */

/* in ioutil.c */
extern void Fatal(struct fb *fbiop, const char *format, ...);

struct fbfade_args {
    struct fbfade_args *self;
    int help;
    int hires;
    const char *input_framebuffer;
    const char *output_framebuffer;
    int input_width;
    int input_height;
    int output_width;
    int output_height;
};

enum fbfade_square_dimension {
    FBFADE_INPUT_SIZE,
    FBFADE_OUTPUT_SIZE
};

static int
fbfade_square_size(struct bu_vls *msg, const char *arg, void *storage,
	enum fbfade_square_dimension dimension)
{
    struct fbfade_args *args = storage ? *(struct fbfade_args **)storage : NULL;
    int value;
    if (!bu_arg_integer_range_from_str(msg, &value, arg, 10, 1, INT_MAX))
	return -1;
    if (args) {
	if (dimension == FBFADE_INPUT_SIZE)
	    args->input_width = args->input_height = value;
	else
	    args->output_width = args->output_height = value;
    }
    return 0;
}

#define FBFADE_SQUARE_CALLBACK(_name, _dimension) \
    static int _name(struct bu_vls *msg, const char *arg, void *storage) \
    { \
	return fbfade_square_size(msg, arg, storage, _dimension); \
    }

FBFADE_SQUARE_CALLBACK(fbfade_input_size, FBFADE_INPUT_SIZE)
FBFADE_SQUARE_CALLBACK(fbfade_output_size, FBFADE_OUTPUT_SIZE)

static const struct bu_arg_option fbfade_options[] = {
    BU_ARG_FLAG("h", "help", struct fbfade_args, help,
	"Print help and exit"),
    BU_ARG_ALIAS("?", NULL, "help", 1),
    BU_ARG_FLAG("H", "high-resolution", struct fbfade_args, hires,
	"Use 1024 instead of 512 for unspecified input dimensions"),
    BU_ARG_VALUE("f", "input-framebuffer", struct fbfade_args,
	input_framebuffer, BU_ARG_VALUE_STRING, BU_ARG_REQUIRED, "device",
	"Read the image to fade in from this framebuffer"),
    BU_ARG_VALUE("F", "output-framebuffer", struct fbfade_args,
	output_framebuffer, BU_ARG_VALUE_STRING, BU_ARG_REQUIRED, "device",
	"Write the fade effect to this framebuffer"),
    BU_ARG_CUSTOM("s", "input-size", struct fbfade_args, self,
	fbfade_input_size, "pixels", "Set both input dimensions"),
    BU_ARG_INTEGER_RANGE("w", "input-width", struct fbfade_args,
	input_width, 1, 1, 1, INT_MAX, "pixels", "Set the input width"),
    BU_ARG_INTEGER_RANGE("n", "input-height", struct fbfade_args,
	input_height, 1, 1, 1, INT_MAX, "pixels", "Set the input height"),
    BU_ARG_CUSTOM("S", "output-size", struct fbfade_args, self,
	fbfade_output_size, "pixels", "Set both output dimensions"),
    BU_ARG_INTEGER_RANGE("W", "output-width", struct fbfade_args,
	output_width, 1, 1, 1, INT_MAX, "pixels", "Set the output width"),
    BU_ARG_INTEGER_RANGE("N", "output-height", struct fbfade_args,
	output_height, 1, 1, 1, INT_MAX, "pixels", "Set the output height"),
    BU_ARG_OPTION_NULL
};

static const struct bu_arg_operand fbfade_operands[] = {
    BU_ARG_OPERAND("output", BU_ARG_VALUE_STRING, 0, 1,
	"Output framebuffer when --output-framebuffer is omitted", NULL),
    BU_ARG_OPERAND_NULL
};

static const struct bu_arg_schema fbfade_schema = BU_ARG_SCHEMA(
    "fbfade", "Fade a framebuffer image in or out", fbfade_options,
    fbfade_operands, BU_ARG_PARSE_OPTIONS_FIRST,
    BU_ARG_SCHEMA_META_HELP(NULL, NULL, NULL, NULL, NULL));

static const struct bu_arg_grammar fbfade_grammar = {
    &fbfade_schema, NULL, NULL
};

static int
fbfade_help(FILE *stream, int status)
{
    char *schema_help = bu_arg_schema_help(&fbfade_schema, "fbfade");
    bu_flog(stream, "%s", schema_help ? schema_help : "");
    bu_free(schema_help, "fbfade help");
    return status;
}


static void
Sig_Catcher(int sig)
{
    (void)signal(sig, SIG_DFL);

    /* The following is not guaranteed to work, but it's worth a try. */
    Fatal(fbp, "Interrupted by signal %d", sig);
}


int
main(int argc, char **argv)
{
    struct fbfade_args args = {0};
    struct bu_vls msg = BU_VLS_INIT_ZERO;
    int operand_index;
    args.self = &args;

    bu_setprogname(argv[0]);

    /* Plant signal catcher. */
    {
	/* signals to catch */
	static int getsigs[] = {
#ifdef SIGHUP
	    SIGHUP,			/* hangup */
#endif
#ifdef SIGINT
	    SIGINT,			/* interrupt */
#endif
#ifdef SIGQUIT
	    SIGQUIT,		/* quit */
#endif
#ifdef SIGPIPE
	    SIGPIPE,		/* write on a broken pipe */
#endif
#ifdef SIGTERM
	    SIGTERM,		/* software termination signal */
#endif
	    0
	};
	int i;

	for (i = 0; getsigs[i] != 0; ++i)
	    if (signal(getsigs[i], SIG_IGN) != SIG_IGN)
		(void)signal(getsigs[i], Sig_Catcher);
    }

    int introspection = bu_arg_introspect(&msg, &fbfade_grammar, argc - 1,
	(const char * const *)(argv + 1));
    if (introspection != 0) {
	bu_flog(introspection > 0 ? stdout : stderr, "%s", bu_vls_cstr(&msg));
	bu_vls_free(&msg);
	return introspection > 0 ? 0 : 1;
    }
    operand_index = bu_arg_schema_parse_complete(&fbfade_schema, &args, &msg,
	argc - 1, (const char **)(argv + 1));
    if (operand_index < 0) {
	bu_flog(stderr, "%s", bu_vls_cstr(&msg));
	bu_vls_free(&msg);
	return 1;
    }
    bu_vls_free(&msg);
    if (args.help)
	return fbfade_help(stdout, 0);

    if (argc == 1 && isatty(fileno(stdin)) && isatty(fileno(stdout)))
	return fbfade_help(stderr, 1);

    hires = args.hires;
    in_fb_file = args.input_framebuffer;
    out_fb_file = args.output_framebuffer;
    src_width = args.input_width;
    src_height = args.input_height;
    dst_width = args.output_width;
    dst_height = args.output_height;

    if (argc - 1 - operand_index == 1) {
	if (out_fb_file) {
	    bu_flog(stderr,
		"output framebuffer specified by both option and operand\n");
	    return 1;
	}
	out_fb_file = argv[operand_index + 1];
    }

    /* Open frame buffer for unbuffered input. */

    if (src_width == 0)
	src_width = hires ? 1024 : 512;		/* starting default */

    if (src_height == 0)
	src_height = hires ? 1024 : 512;	/* starting default */

    if (in_fb_file != NULL) {

	if ((fbp = fb_open(in_fb_file, src_width, src_height)) == FB_NULL)
	    Fatal(fbp, "Couldn't open input frame buffer");
	else {
	    int y;
	    int wt = fb_getwidth(fbp);
	    int ht = fb_getheight(fbp);

	    /* Use smaller actual input size instead of request. */

	    V_MIN(src_width, wt);
	    V_MIN(src_height, ht);

	    if ((pix = (RGBpixel *)malloc((size_t)src_width * (size_t)src_height * sizeof(RGBpixel))) == NULL)
		Fatal(fbp, "Not enough memory for pixel array");

	    for (y = 0; y < src_height; ++y) {
		if (fb_read(fbp, 0, y, pix[y * src_width], src_width) == -1)
		    Fatal(fbp, "Error reading raster");
	    }

	    if (fb_close(fbp) == -1) {
		fbp = FB_NULL;	/* avoid second try */
		Fatal(fbp, "Error closing input frame buffer");
	    }
	}
    }

    /* Open frame buffer for unbuffered output. */

    if (dst_width == 0)
	dst_width = src_width;		/* default */

    if (dst_height == 0)
	dst_height = src_height;	/* default */

    if ((fbp = fb_open(out_fb_file, dst_width, dst_height)) == FB_NULL)
	Fatal(fbp, "Couldn't open output frame buffer");
    else {
	int wt = fb_getwidth(fbp);
	int ht = fb_getheight(fbp);

	/* Use smaller actual frame buffer size for output. */

	V_MIN(dst_width, wt);
	V_MIN(dst_height, ht);

	/* Avoid selecting pixels outside the input image. */

	V_MIN(dst_width, src_width);
	V_MIN(dst_height, src_height);
    }

    /* The following is probably an optimally fast shuffling
     * algorithm; unfortunately, it requires a huge auxiliary array.
     * The way it works is to start with an array of all pixel
     * indices, then repeat: select an entry at random from the array,
     * output that index, replace that entry with the last array
     * entry, then reduce the array size.
     */
    {
	long *loc;		/* keeps track of pixel shuffling */
	long wxh = (long)dst_width * (long)dst_height;
	/* down-counter */

	if ((loc = (long *)malloc((size_t)wxh * sizeof(long))) == NULL)
	    Fatal(fbp, "Not enough memory for location array");

	/* Initialize pixel location array to sequential order. */

	while (--wxh >= 0L)
	    loc[wxh] = wxh;

	/* Select a pixel at random, paint it, and adjust the location
	 * array.
	 */

	for (wxh = (long)dst_width * (long)dst_height; --wxh >= 0L;) {
	    long r = (long)((double)wxh * drand48());
	    long x = loc[r] % dst_width;
	    long y = loc[r] / dst_width;

	    if (fb_write(fbp, (int)x, (int)y, in_fb_file == NULL ? bg : pix[x + y * src_width], 1) == -1) {
		Fatal(fbp, "Error writing pixel");
	    }

	    loc[r] = loc[wxh];	/* track the shuffle */
	}
    }

    /* Close the frame buffer. */

    if (fb_close(fbp) == -1) {
	fbp = FB_NULL;	/* avoid second try */
	Fatal(fbp, "Error closing output frame buffer");
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
