/*                     F B S T R E T C H . C
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
/** @file fbstretch.c
 *
 * fbstretch -- stretch a frame buffer image

 * This program converts a frame buffer image so that it is stretched or
 * compressed in the horizontal and/or vertical directions.  The image
 * scaling origin is taken to be the lower left-hand corner of the
 * display.  When compressing, pixel averaging is used by default;
 * when expanding, pixel replication is used.  Pixel averaging may be
 * meaningless for some color maps, so there is an option to use
 * sampling instead.
 *
 * The main use of this utility is to compensate for differences in
 * pixel aspect ratios among different display devices.
 *
 * Options:
 *
 * -a		"no averaging": samples for compression instead of
 * averaging pixels
 *
 * -v		"verbose": prints information about sizes and scaling
 * on the standard error output
 *
 * -x x_scale	horizontal scaling factor (default: out width/in width)
 *
 * -y y_scale	vertical scaling factor (default: out height/in height)
 *
 * -f in_fb	reads from the specified frame buffer file instead
 * of modifying the one specified by the -f option "in
 * place"
 *
 * -F out_fb	writes to the specified frame buffer file instead
 * of the one specified by the FB_FILE environment
 * variable (the default frame buffer, if no FB_FILE)
 *
 * -s size	input size (width & height set to same value)
 *
 * -w width	input width
 *
 * -n height	input height
 *
 * -S size	output size (width & height set to same value)
 *
 * -W width	output width
 *
 * -N height	output height
 *
 * out_fb	same as -F out_fb, for convenience
 *
 */

#include "common.h"

#include <assert.h>
#include <limits.h>
#include <signal.h>
#include <stdlib.h>

#include "bio.h"

#include "bu/arg.h"
#include "bu/app.h"
#include "bu/log.h"
#include "bu/malloc.h"
#include "bu/str.h"
#include "dm.h"			/* BRL-CAD package libfb.a interface */


#define EPSILON 0.0001			/* fudge for converting float to int */

static int sample = 0;			/* set: sampling; clear: averaging */
static int verbose = 0;			/* set for size info printout */
static fastf_t x_scale = -1.0;		/* horizontal scaling factor */
static fastf_t y_scale = -1.0;		/* vertical scaling factor */
static int x_compress;			/* set if compressing horizontally */
static int y_compress;			/* set if compressing vertically */
static const char *src_file = NULL;	/* source frame buffer name */
static struct fb *src_fbp = FB_NULL;	/* source frame buffer handle */
static const char *dst_file = NULL;	/* destination frame buffer name */
static struct fb *dst_fbp = FB_NULL;	/* destination frame buffer handle */
static int src_width = 512;
static int src_height = 512;		/* source image size */
static int dst_width = 0;
static int dst_height = 0;		/* destination image size */
static unsigned char *src_buf;		/* calloc()ed input scan line buffer */
static unsigned char *dst_buf;		/* calloc()ed output scan line buffer */

/* in ioutil.c */
extern void Message(const char *format, ...);
extern void Fatal(struct fb *fbiop, const char *format, ...);

struct fbstretch_args {
    struct fbstretch_args *self;
    int help;
    int sample;
    int verbose;
    const char *input_framebuffer;
    const char *output_framebuffer;
    int input_width;
    int input_height;
    int output_width;
    int output_height;
    fastf_t x_scale;
    fastf_t y_scale;
};

enum fbstretch_square_dimension {
    FBSTRETCH_INPUT_SIZE,
    FBSTRETCH_OUTPUT_SIZE
};

static int
fbstretch_square_size(struct bu_vls *msg, const char *arg, void *storage,
	enum fbstretch_square_dimension dimension)
{
    struct fbstretch_args *args = storage ?
	*(struct fbstretch_args **)storage : NULL;
    int value;

    if (!bu_arg_integer_range_from_str(msg, &value, arg, 10, 1, INT_MAX))
	return -1;
    if (args) {
	if (dimension == FBSTRETCH_INPUT_SIZE)
	    args->input_width = args->input_height = value;
	else
	    args->output_width = args->output_height = value;
    }
    return 0;
}

#define FBSTRETCH_SQUARE_CALLBACK(_name, _dimension) \
    static int _name(struct bu_vls *msg, const char *arg, void *storage) \
    { \
	return fbstretch_square_size(msg, arg, storage, _dimension); \
    }

FBSTRETCH_SQUARE_CALLBACK(fbstretch_input_size, FBSTRETCH_INPUT_SIZE)
FBSTRETCH_SQUARE_CALLBACK(fbstretch_output_size, FBSTRETCH_OUTPUT_SIZE)

static int
fbstretch_scale(struct bu_vls *msg, const char *arg, void *storage)
{
    fastf_t value;

    if (!bu_arg_number_from_str(msg, &value, arg))
	return -1;
    if (value <= 0.0) {
	if (msg)
	    bu_vls_printf(msg, "scale factor must be positive: %s\n", arg);
	return -1;
    }
    if (storage)
	*((fastf_t *)storage) = value;
    return 0;
}

static const struct bu_arg_option fbstretch_options[] = {
    BU_ARG_FLAG("h", "help", struct fbstretch_args, help,
	"Print help and exit"),
    BU_ARG_ALIAS("?", NULL, "help", 1),
    BU_ARG_FLAG("a", "sample", struct fbstretch_args, sample,
	"Sample pixels instead of averaging during compression"),
    BU_ARG_FLAG("v", "verbose", struct fbstretch_args, verbose,
	"Report effective image sizes and scale factors"),
    BU_ARG_VALUE("f", "input-framebuffer", struct fbstretch_args,
	input_framebuffer, BU_ARG_VALUE_STRING, BU_ARG_REQUIRED, "device",
	"Read the source image from this framebuffer or PIX file"),
    BU_ARG_VALUE("F", "output-framebuffer", struct fbstretch_args,
	output_framebuffer, BU_ARG_VALUE_STRING, BU_ARG_REQUIRED, "device",
	"Write the scaled image to this framebuffer"),
    BU_ARG_CUSTOM("s", "input-size", struct fbstretch_args, self,
	fbstretch_input_size, "pixels", "Set both input dimensions"),
    BU_ARG_INTEGER_RANGE("w", "input-width", struct fbstretch_args,
	input_width, 1, 1, 1, INT_MAX, "pixels", "Set the input width"),
    BU_ARG_INTEGER_RANGE("n", "input-height", struct fbstretch_args,
	input_height, 1, 1, 1, INT_MAX, "pixels", "Set the input height"),
    BU_ARG_CUSTOM("S", "output-size", struct fbstretch_args, self,
	fbstretch_output_size, "pixels", "Set both output dimensions"),
    BU_ARG_INTEGER_RANGE("W", "output-width", struct fbstretch_args,
	output_width, 1, 1, 1, INT_MAX, "pixels", "Set the output width"),
    BU_ARG_INTEGER_RANGE("N", "output-height", struct fbstretch_args,
	output_height, 1, 1, 1, INT_MAX, "pixels", "Set the output height"),
    BU_ARG_CUSTOM("x", "x-scale", struct fbstretch_args, x_scale,
	fbstretch_scale, "factor", "Set the horizontal scale factor"),
    BU_ARG_CUSTOM("y", "y-scale", struct fbstretch_args, y_scale,
	fbstretch_scale, "factor", "Set the vertical scale factor"),
    BU_ARG_OPTION_NULL
};

static const struct bu_arg_operand fbstretch_operands[] = {
    BU_ARG_OPERAND("output", BU_ARG_VALUE_STRING, 0, 1,
	"Output framebuffer when --output-framebuffer is omitted", NULL),
    BU_ARG_OPERAND_NULL
};

static const struct bu_arg_schema fbstretch_schema = BU_ARG_SCHEMA(
    "fbstretch", "Scale a framebuffer image", fbstretch_options,
    fbstretch_operands, BU_ARG_PARSE_OPTIONS_FIRST,
    BU_ARG_SCHEMA_META_HELP(NULL, NULL, NULL, NULL, NULL));

static const struct bu_arg_grammar fbstretch_grammar = {
    &fbstretch_schema, NULL, NULL
};

static int
fbstretch_help(FILE *stream, int status)
{
    char *help = bu_arg_schema_help(&fbstretch_schema, "fbstretch");
    bu_flog(stream, "%s", help ? help : "");
    bu_free(help, "fbstretch help");
    return status;
}


static void
Stretch_Fatal(const char *str)
{
    if (src_fbp != FB_NULL && fb_close(src_fbp) == -1) {
	Message("Error closing input frame buffer");
	src_fbp = FB_NULL;
    }

    if (dst_fbp != FB_NULL && fb_close(dst_fbp) == -1) {
	Message("Error closing output frame buffer");
	src_fbp = FB_NULL;
    }

    Fatal(FB_NULL, "%s", str);
    /* NOT REACHED */
}


static void
Sig_Catcher(int sig)
{
    (void)signal(sig, SIG_DFL);

    bu_exit(EXIT_FAILURE, "Interrupted by signal %d\n", sig);
}


int
main(int argc, char **argv)
{
    struct fbstretch_args args = {
	.input_width = 512,
	.input_height = 512,
	.x_scale = -1.0,
	.y_scale = -1.0
    };
    struct bu_vls msg = BU_VLS_INIT_ZERO;
    int operand_index;
    args.self = &args;

    bu_setprogname(argv[0]);

    /* Plant signal catcher. */
    {
	static int getsigs[] = {
	    /* signals to catch */
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

    int introspection = bu_arg_introspect(&msg, &fbstretch_grammar, argc - 1,
	(const char * const *)(argv + 1));
    if (introspection != 0) {
	bu_flog(introspection > 0 ? stdout : stderr, "%s", bu_vls_cstr(&msg));
	bu_vls_free(&msg);
	return introspection > 0 ? 0 : 1;
    }
    operand_index = bu_arg_schema_parse_complete(&fbstretch_schema, &args,
	&msg, argc - 1, (const char **)(argv + 1));
    if (operand_index < 0) {
	bu_flog(stderr, "%s", bu_vls_cstr(&msg));
	bu_vls_free(&msg);
	return 1;
    }
    bu_vls_free(&msg);
    if (args.help)
	return fbstretch_help(stdout, 0);
    if (argc == 1 && isatty(fileno(stdin)) && isatty(fileno(stdout)))
	return fbstretch_help(stderr, 1);

    sample = args.sample;
    verbose = args.verbose;
    src_file = args.input_framebuffer;
    dst_file = args.output_framebuffer;
    src_width = args.input_width;
    src_height = args.input_height;
    dst_width = args.output_width;
    dst_height = args.output_height;
    x_scale = args.x_scale;
    y_scale = args.y_scale;

    if (argc - 1 - operand_index == 1) {
	if (dst_file) {
	    bu_flog(stderr,
		"output framebuffer specified by both option and operand\n");
	    return 1;
	}
	dst_file = argv[operand_index + 1];
    }

    if (dst_file == NULL)
	dst_file = getenv("FB_FILE");

    /* Figure out what scale factors to use before messing up size info. */

    if (x_scale < 0.0) {
	if (src_width == 0 || dst_width == 0)
	    x_scale = 1.0;
	else
	    x_scale = (double)dst_width / (double)src_width;
    }

    if (y_scale < 0.0) {
	if (src_height == 0 || dst_height == 0)
	    y_scale = 1.0;
	else
	    y_scale = (double)dst_height / (double)src_height;
    }

    if (verbose)
	Message("Scale factors %gx%g", x_scale, y_scale);

    /* Open frame buffer(s) for unbuffered input/output. */

    if ((src_fbp = fb_open(src_file == NULL ? dst_file : src_file,
			   src_width, src_height
	     )
	    ) == FB_NULL
	)
	Stretch_Fatal("Couldn't open input image");
    else {
	int wt, ht;	/* actual frame buffer size */

	/* Use smaller input size in preference to requested size. */

	if ((wt = fb_getwidth(src_fbp)) < src_width)
	    src_width = wt;

	if ((ht = fb_getheight(src_fbp)) < src_height)
	    src_height = ht;

	if (verbose)
	    Message("Source image %dx%d", src_width, src_height);

	if (dst_width == 0)
	    dst_width = src_width * x_scale + EPSILON;

	if (dst_height == 0)
	    dst_height = src_height * y_scale + EPSILON;

	if (verbose)
	    Message("Requested output size %dx%d",
		    dst_width, dst_height
		);

	if (src_file == NULL
	    || (dst_file != NULL && BU_STR_EQUAL(src_file, dst_file))
	    )
	    dst_fbp = src_fbp;	/* No No No Not a Second Time */
	else if ((dst_fbp = fb_open(dst_file, dst_width, dst_height))
		 == FB_NULL
	    )
	    Stretch_Fatal("Couldn't open output frame buffer");

	/* Use smaller output size in preference to requested size. */

	if ((wt = fb_getwidth(dst_fbp)) < dst_width)
	    dst_width = wt;

	if ((ht = fb_getheight(dst_fbp)) < dst_height)
	    dst_height = ht;

	if (verbose)
	    Message("Destination image %dx%d",
		    dst_width, dst_height
		);
    }

    /* Determine compression/expansion directions. */

    x_compress = x_scale < 1 - EPSILON;
    y_compress = y_scale < 1 - EPSILON;

    /* Allocate input/output scan line buffers.  These could overlap, but
       I decided to keep them separate for simplicity.  The algorithms are
       arranged so that source and destination can access the same image; if
       at some future time offsets are supported, that would no longer hold.
       calloc is used instead of malloc just to avoid integer overflow. */

    if ((src_buf = (unsigned char *)calloc(
	     y_compress ? (int)(1 / y_scale + 1 - EPSILON) * src_width
	     : src_width,
	     sizeof(RGBpixel)
	     )
	    ) == NULL
	|| (dst_buf = (unsigned char *)calloc(
		y_compress ? dst_width
		: (int)(y_scale + 1 - EPSILON) * dst_width,
		sizeof(RGBpixel)
		)
	    ) == NULL
	)
	Stretch_Fatal("Insufficient memory for scan line buffers.");

#define Src(x, y)	(&src_buf[(x) + src_width * (y) * sizeof(RGBpixel)])
#define Dst(x, y)	(&dst_buf[(x) + dst_width * (y) * sizeof(RGBpixel)])

    /* Do the horizontal/vertical expansion/compression.  I wanted to merge
       these but didn't like the extra bookkeeping overhead in the loops. */

    if (x_compress && y_compress) {
	int src_x, src_y;	/* source rect. pixel coords. */
	int dst_x, dst_y;	/* destination pixel coords. */
	int top_x, top_y;	/* source rect. upper bounds */
	int bot_x, bot_y;	/* source rect. lower bounds */

	/* Compute coords. of source rectangle and destination pixel. */

	dst_y = 0;
    ccyloop:
	if (dst_y >= dst_height)
	    goto done;	/* that's all folks */

	bot_y = dst_y / y_scale + EPSILON;

	if ((top_y = (dst_y + 1) / y_scale + EPSILON) > src_height)
	    top_y = src_height;

	if (top_y <= bot_y) {
	    /* End of image. */

	    /* Clear beginning of output scan line buffer. */

	    dst_x = src_width * y_scale + EPSILON;

	    if (dst_x < dst_width)
		++dst_x;	/* sometimes needed */

	    while (--dst_x >= 0) {
		assert(dst_x < dst_width);
		Dst(dst_x, 0)[RED] = 0;
		Dst(dst_x, 0)[GRN] = 0;
		Dst(dst_x, 0)[BLU] = 0;
	    }

	    /* Clear out top margin. */

	    for (; dst_y < dst_height; ++dst_y)
		if (fb_write(dst_fbp, 0, dst_y,
			     (unsigned char *)Dst(0, 0),
			     dst_width
			) == -1
		    )
		    Stretch_Fatal("Error writing top margin");

	    goto done;	/* that's all folks */
	}

	assert(0 <= bot_y && bot_y < top_y && top_y <= src_height);
	assert(0 <= dst_y && dst_y <= bot_y);
	assert(top_y - bot_y <= (int)(1 / y_scale + 1 - EPSILON));

	/* Fill input scan line buffer. */

	for (src_y = bot_y; src_y < top_y; ++src_y)
	    if (fb_read(src_fbp, 0, src_y,
			(unsigned char *)Src(0, src_y - bot_y),
			src_width
		    ) == -1
		)
		Stretch_Fatal("Error reading scan line");

	dst_x = 0;
    ccxloop:
	if (dst_x >= dst_width)
	    goto ccflush;

	bot_x = dst_x / x_scale + EPSILON;

	if ((top_x = (dst_x + 1) / x_scale + EPSILON) > src_width)
	    top_x = src_width;

	if (top_x <= bot_x) {
	ccflush:		/* End of band; flush buffer. */

	    if (fb_write(dst_fbp, 0, dst_y,
			 (unsigned char *)Dst(0, 0),
			 dst_width
		    ) == -1
		)
		Stretch_Fatal("Error writing scan line");

	    ++dst_y;
	    goto ccyloop;
	}

	assert(0 <= bot_x && bot_x < top_x && top_x <= src_width);
	assert(0 <= dst_x && dst_x <= bot_x);
	assert(top_x - bot_x <= (int)(1 / x_scale + 1 - EPSILON));

	/* Copy sample or averaged source pixel(s) to destination. */

	if (sample) {
	    Dst(dst_x, 0)[RED] = Src(bot_x, 0)[RED];
	    Dst(dst_x, 0)[GRN] = Src(bot_x, 0)[GRN];
	    Dst(dst_x, 0)[BLU] = Src(bot_x, 0)[BLU];
	} else {
	    int sum[3];	/* pixel value accumulator */
	    float tally;	/* # of pixels accumulated */

	    /* "Read in" source rectangle and average pixels. */

	    sum[RED] = sum[GRN] = sum[BLU] = 0;

	    for (src_y = top_y - bot_y; --src_y >= 0;)
		for (src_x = bot_x; src_x < top_x; ++src_x) {
		    sum[RED] += Src(src_x, src_y)[RED];
		    sum[GRN] += Src(src_x, src_y)[GRN];
		    sum[BLU] += Src(src_x, src_y)[BLU];
		}

	    tally = (top_x - bot_x) * (top_y - bot_y);
	    assert(tally > 0.0);
	    Dst(dst_x, 0)[RED] = sum[RED] / tally + 0.5;
	    Dst(dst_x, 0)[GRN] = sum[GRN] / tally + 0.5;
	    Dst(dst_x, 0)[BLU] = sum[BLU] / tally + 0.5;
	}

	++dst_x;
	goto ccxloop;
    } else if (x_compress && !y_compress) {
	int src_x, src_y;	/* source rect. pixel coords. */
	int dst_x, dst_y;	/* dest. rect. pixel coords. */
	int bot_x, top_x;	/* source rectangle bounds */
	int bot_y, top_y;	/* destination rect. bounds */

	/* Compute coords. of source and destination rectangles. */

	src_y = (dst_height - 1) / y_scale + EPSILON;
    ceyloop:
	if (src_y < 0)
	    goto done;	/* that's all folks */

	bot_y = src_y * y_scale + EPSILON;

	if ((top_y = (src_y + 1) * y_scale + EPSILON) > dst_height)
	    top_y = dst_height;

	assert(0 <= src_y && src_y <= bot_y && src_y < src_height);
	assert(bot_y < top_y && top_y <= dst_height);
	assert(top_y - bot_y <= (int)(y_scale + 1 - EPSILON));

	/* Fill input scan line buffer. */

	if (fb_read(src_fbp, 0, src_y, (unsigned char *)Src(0, 0),
		    src_width
		) == -1
	    )
	    Stretch_Fatal("Error reading scan line");

	dst_x = 0;
    cexloop:
	if (dst_x >= dst_width)
	    goto ceflush;

	bot_x = dst_x / x_scale + EPSILON;

	if ((top_x = (dst_x + 1) / x_scale + EPSILON) > src_width)
	    top_x = src_width;

	if (top_x <= bot_x) {
	ceflush:		/* End of band; flush buffer. */

	    for (dst_y = top_y; --dst_y >= bot_y;)
		if (fb_write(dst_fbp, 0, dst_y,
			     (unsigned char *)Dst(0, dst_y - bot_y
				 ),
			     dst_width
			) == -1
		    )
		    Stretch_Fatal("Error writing scan line");

	    --src_y;
	    goto ceyloop;
	}

	assert(0 <= bot_x && bot_x < top_x && top_x <= src_width);
	assert(0 <= dst_x && dst_x <= bot_x);
	assert(top_x - bot_x <= (int)(1 / x_scale + 1 - EPSILON));

	/* Replicate sample or averaged source pixel(s) to dest. */

	if (sample) {
	    for (dst_y = top_y - bot_y; --dst_y >= 0;) {
		Dst(dst_x, dst_y)[RED] = Src(bot_x, 0)[RED];
		Dst(dst_x, dst_y)[GRN] = Src(bot_x, 0)[GRN];
		Dst(dst_x, dst_y)[BLU] = Src(bot_x, 0)[BLU];
	    }
	} else {
	    int sum[3];	/* pixel value accumulator */
	    float tally;	/* # of pixels accumulated */

	    /* "Read in" source rectangle and average pixels. */

	    sum[RED] = sum[GRN] = sum[BLU] = 0;

	    for (src_x = bot_x; src_x < top_x; ++src_x) {
		sum[RED] += Src(src_x, 0)[RED];
		sum[GRN] += Src(src_x, 0)[GRN];
		sum[BLU] += Src(src_x, 0)[BLU];
	    }

	    tally = top_x - bot_x;
	    assert(tally > 0.0);
	    sum[RED] = sum[RED] / tally + 0.5;
	    sum[GRN] = sum[GRN] / tally + 0.5;
	    sum[BLU] = sum[BLU] / tally + 0.5;

	    for (dst_y = top_y - bot_y; --dst_y >= 0;) {
		Dst(dst_x, dst_y)[RED] = sum[RED];
		Dst(dst_x, dst_y)[GRN] = sum[GRN];
		Dst(dst_x, dst_y)[BLU] = sum[BLU];
	    }
	}

	++dst_x;
	goto cexloop;
    } else if (!x_compress && y_compress) {
	int src_x, src_y;	/* source rect. pixel coords. */
	int dst_x, dst_y;	/* dest. rect. pixel coords. */
	int bot_x, top_x;	/* destination rect. bounds */
	int bot_y, top_y;	/* source rectangle bounds */

	assert(dst_width >= src_width);	/* (thus no right margin) */

	/* Compute coords. of source and destination rectangles. */

	dst_y = 0;
    ecyloop:
	if (dst_y >= dst_height)
	    goto done;	/* that's all folks */

	bot_y = dst_y / y_scale + EPSILON;

	if ((top_y = (dst_y + 1) / y_scale + EPSILON) > src_height)
	    top_y = src_height;

	if (top_y <= bot_y) {
	    /* End of image. */

	    /* Clear output scan line buffer. */

	    for (dst_x = dst_width; --dst_x >= 0;) {
		Dst(dst_x, 0)[RED] = 0;
		Dst(dst_x, 0)[GRN] = 0;
		Dst(dst_x, 0)[BLU] = 0;
	    }

	    /* Clear out top margin. */

	    for (; dst_y < dst_height; ++dst_y)
		if (fb_write(dst_fbp, 0, dst_y,
			     (unsigned char *)Dst(0, 0),
			     dst_width
			) == -1
		    )
		    Stretch_Fatal("Error writing top margin");

	    goto done;	/* that's all folks */
	}

	assert(0 <= bot_y && bot_y < top_y && top_y <= src_height);
	assert(0 <= dst_y && dst_y <= bot_y);
	assert(top_y - bot_y <= (int)(1 / y_scale + 1 - EPSILON));

	/* Fill input scan line buffer. */

	for (src_y = bot_y; src_y < top_y; ++src_y)
	    if (fb_read(src_fbp, 0, src_y,
			(unsigned char *)Src(0, src_y - bot_y),
			src_width
		    ) == -1
		)
		Stretch_Fatal("Error reading scan line");

	src_x = (dst_width - 1) / x_scale + EPSILON;
    ecxloop:
	if (src_x < 0) {
	    /* End of band; flush buffer. */
	    if (fb_write(dst_fbp, 0, dst_y,
			 (unsigned char *)Dst(0, 0),
			 dst_width
		    ) == -1
		)
		Stretch_Fatal("Error writing scan line");

	    ++dst_y;
	    goto ecyloop;
	}

	bot_x = src_x * x_scale + EPSILON;

	if ((top_x = (src_x + 1) * x_scale + EPSILON) > dst_width)
	    top_x = dst_width;

	assert(0 <= src_x && src_x <= bot_x && src_x <= src_width);
	assert(bot_x < top_x && top_x <= dst_width);
	assert(top_x - bot_x <= (int)(x_scale + 1 - EPSILON));

	/* Replicate sample or averaged source pixel(s) to dest. */

	if (sample) {
	    for (dst_x = top_x; --dst_x >= bot_x;) {
		Dst(dst_x, 0)[RED] = Src(src_x, 0)[RED];
		Dst(dst_x, 0)[GRN] = Src(src_x, 0)[GRN];
		Dst(dst_x, 0)[BLU] = Src(src_x, 0)[BLU];
	    }
	} else {
	    int sum[3];	/* pixel value accumulator */
	    float tally;	/* # of pixels accumulated */

	    /* "Read in" source rectangle and average pixels. */

	    sum[RED] = sum[GRN] = sum[BLU] = 0;

	    for (src_y = top_y - bot_y; --src_y >= 0;) {
		sum[RED] += Src(src_x, src_y)[RED];
		sum[GRN] += Src(src_x, src_y)[GRN];
		sum[BLU] += Src(src_x, src_y)[BLU];
	    }

	    tally = top_y - bot_y;
	    assert(tally > 0.0);
	    sum[RED] = sum[RED] / tally + 0.5;
	    sum[GRN] = sum[GRN] / tally + 0.5;
	    sum[BLU] = sum[BLU] / tally + 0.5;

	    for (dst_x = top_x; --dst_x >= bot_x;) {
		Dst(dst_x, 0)[RED] = sum[RED];
		Dst(dst_x, 0)[GRN] = sum[GRN];
		Dst(dst_x, 0)[BLU] = sum[BLU];
	    }
	}

	--src_x;
	goto ecxloop;
    } else if (!x_compress && !y_compress) {
	int src_x, src_y;	/* source pixel coords. */
	int dst_x, dst_y;	/* dest. rect. pixel coords. */
	int bot_x, bot_y;	/* dest. rect. lower bounds */
	int top_x, top_y;	/* dest. rect. upper bounds */

	assert(dst_width >= src_width);	/* (thus no right margin) */

	/* Compute coords. of source and destination rectangles. */

	src_y = (dst_height - 1) / y_scale + EPSILON;
    eeyloop:
	if (src_y < 0)
	    goto done;	/* that's all folks */

	bot_y = src_y * y_scale + EPSILON;

	if ((top_y = (src_y + 1) * y_scale + EPSILON) > dst_height)
	    top_y = dst_height;

	assert(0 <= src_y && src_y <= bot_y && src_y < src_height);
	assert(bot_y < top_y && top_y <= dst_height);
	assert(top_y - bot_y <= (int)(y_scale + 1 - EPSILON));

	/* Fill input scan line buffer. */
	if (fb_read(src_fbp, 0, src_y, (unsigned char *)Src(0, 0),
		    src_width
		) == -1
	    )
	    Stretch_Fatal("Error reading scan line");

	src_x = (dst_width - 1) / x_scale + EPSILON;
    eexloop:
	if (src_x < 0) {
	    /* End of band; flush buffer. */

	    for (dst_y = top_y; --dst_y >= bot_y;)
		if (fb_write(dst_fbp, 0, dst_y,
			     (unsigned char *)Dst(0, dst_y - bot_y
				 ),
			     dst_width
			) == -1
		    )
		    Stretch_Fatal("Error writing scan line");

	    --src_y;
	    goto eeyloop;
	}

	bot_x = src_x * x_scale + EPSILON;

	if ((top_x = (src_x + 1) * x_scale + EPSILON) > dst_width)
	    top_x = dst_width;

	assert(0 <= src_x && src_x <= bot_x && src_x <= src_width);
	assert(bot_x < top_x && top_x <= dst_width);
	assert(top_x - bot_x <= (int)(x_scale + 1 - EPSILON));

	/* Replicate sample source pixel to destination. */

	for (dst_y = top_y - bot_y; --dst_y >= 0;)
	    for (dst_x = top_x; --dst_x >= bot_x;) {
		Dst(dst_x, dst_y)[RED] = Src(src_x, 0)[RED];
		Dst(dst_x, dst_y)[GRN] = Src(src_x, 0)[GRN];
		Dst(dst_x, dst_y)[BLU] = Src(src_x, 0)[BLU];
	    }

	--src_x;
	goto eexloop;
    }

done:
    /* Close the frame buffers. */

    assert(src_fbp != FB_NULL && dst_fbp != FB_NULL);

    if (fb_close(src_fbp) == -1)
	Message("Error closing input frame buffer");

    if (dst_fbp != src_fbp && fb_close(dst_fbp) == -1)
	Message("Error closing output frame buffer");

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
