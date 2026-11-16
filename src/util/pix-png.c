/*                       P I X - P N G . C
 * BRL-CAD
 *
 * Copyright (c) 1998-2026 United States Government as represented by
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
/** @file util/pix-png.c
 *
 * Convert pix file to PNG (Portable Network Graphics) format
 *
 */

#include "common.h"

#include <limits.h>
#ifdef HAVE_SYS_TYPES_H
#  include <sys/types.h>
#endif
#ifdef HAVE_SYS_STAT_H
#  include <sys/stat.h>
#endif

#include "bio.h"

#include "png.h"

#include "bu/arg.h"
#include "bu/app.h"
#include "bu/exit.h"
#include "bu/log.h"
#include "bu/malloc.h"
#include "dm.h"


#define BYTESPERPIXEL 3
#define DEFAULT_IMAGE_DIMENSION 512

struct pix_png_args {
    struct pix_png_args *self;
    int help;
    int autosize;
    double gamma;
    size_t width;
    size_t height;
    const char *output;
};

enum pix_png_dimension {
    PIX_PNG_SIZE,
    PIX_PNG_WIDTH,
    PIX_PNG_HEIGHT
};

static struct pix_png_args *
pix_png_args_from_storage(void *storage)
{
    return storage ? *(struct pix_png_args **)storage : NULL;
}

static int
pix_png_autosize(struct bu_vls *UNUSED(msg), const char *UNUSED(arg),
	void *storage)
{
    struct pix_png_args *args = pix_png_args_from_storage(storage);
    if (args)
	args->autosize = 1;
    return 0;
}

static int
pix_png_dimension(struct bu_vls *msg, const char *arg, void *storage,
	enum pix_png_dimension dimension)
{
    struct pix_png_args *args = pix_png_args_from_storage(storage);
    size_t value;

    if (!bu_arg_size_t_range_from_str(msg, &value, arg, 10, 1,
	    PNG_UINT_31_MAX))
	return -1;
    if (args) {
	if (dimension == PIX_PNG_SIZE)
	    args->width = args->height = value;
	else if (dimension == PIX_PNG_WIDTH)
	    args->width = value;
	else
	    args->height = value;
	args->autosize = 0;
    }
    return 0;
}

#define PIX_PNG_DIMENSION_CALLBACK(_name, _dimension) \
    static int _name(struct bu_vls *msg, const char *arg, void *storage) \
    { \
	return pix_png_dimension(msg, arg, storage, _dimension); \
    }

PIX_PNG_DIMENSION_CALLBACK(pix_png_size, PIX_PNG_SIZE)
PIX_PNG_DIMENSION_CALLBACK(pix_png_width, PIX_PNG_WIDTH)
PIX_PNG_DIMENSION_CALLBACK(pix_png_height, PIX_PNG_HEIGHT)

static const struct bu_arg_option pix_png_options[] = {
    BU_ARG_FLAG("h", "help", struct pix_png_args, help,
	"Print help and exit"),
    BU_ARG_ALIAS("?", NULL, "help", 1),
    BU_ARG_CUSTOM_FLAG("a", "autosize", "autosize", struct pix_png_args,
	self, pix_png_autosize, "Derive dimensions from a named input file"),
    BU_ARG_VALUE("g", "gamma", struct pix_png_args, gamma,
	BU_ARG_VALUE_NUMBER, BU_ARG_REQUIRED, "value", "Set the PNG source gamma"),
    BU_ARG_CUSTOM("s", "size", struct pix_png_args, self, pix_png_size,
	"pixels", "Set both input dimensions"),
    BU_ARG_CUSTOM("w", "width", struct pix_png_args, self, pix_png_width,
	"pixels", "Set the input image width"),
    BU_ARG_CUSTOM("n", "height", struct pix_png_args, self, pix_png_height,
	"pixels", "Set the input image height"),
    BU_ARG_VALUE("o", "output", struct pix_png_args, output,
	BU_ARG_VALUE_FILE, BU_ARG_REQUIRED, "file.png", "Write PNG output to a file"),
    BU_ARG_OPTION_NULL
};

static const struct bu_arg_operand pix_png_operands[] = {
    BU_ARG_OPERAND("input", BU_ARG_VALUE_FILE, 0, 1,
	"Input PIX file; omit to read standard input", NULL),
    BU_ARG_OPERAND_NULL
};

static const struct bu_arg_schema pix_png_schema = BU_ARG_SCHEMA(
    "pix-png", "Convert a BRL-CAD PIX image to PNG", pix_png_options,
    pix_png_operands, BU_ARG_PARSE_OPTIONS_FIRST,
    BU_ARG_SCHEMA_META_HELP(NULL, NULL, NULL, NULL, NULL));

static const struct bu_arg_grammar pix_png_grammar = {
    &pix_png_schema, NULL, NULL
};


static void
write_png(FILE *outfp, unsigned char **scanlines, png_uint_32 width,
	png_uint_32 height, double gamma)
{
    png_structp png_p;
    png_infop info_p;

    png_p = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png_p)
	bu_exit(1, "Could not create PNG write structure\n");

    info_p = png_create_info_struct(png_p);
    if (!info_p)
	bu_exit(1, "Could not create PNG info structure\n");

    png_init_io(png_p, outfp);
    png_set_filter(png_p, 0, PNG_FILTER_NONE);
    png_set_compression_level(png_p, 9);
    png_set_IHDR(png_p, info_p, width, height, 8,
		 PNG_COLOR_TYPE_RGB, PNG_INTERLACE_NONE,
		 PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);

    /*
     * From the PNG 1.0 Specification: the gAMA chunk specifies the
     * gamma characteristic of the source device.
     *
     * In this interpretation, we set the value to 1.0; indicating we
     * hadn't done any gamma correction.
     *
     * From the PNG 1.1 Specification:
     *
     * PNG images can specify, via the gAMA chunk, the power function
     * relating the desired display output with the image samples.
     *
     * In this interpretation, we could set the value to 0.6,
     * representing the value needed to un-do the 2.2 correction
     * auto-applied by PowerPoint for PC monitors.
     */
    if (gamma > 0.0)
	png_set_gAMA(png_p, info_p, gamma);

    png_write_info(png_p, info_p);
    png_write_image(png_p, scanlines);
    png_write_end(png_p, NULL);
    png_write_flush(png_p);
    png_destroy_write_struct(&png_p, &info_p);
}


int
main(int argc, char *argv[])
{
    size_t i;
    size_t w, h;
    size_t row_size;
    size_t image_size;
    unsigned char *scanbuf;
    unsigned char **rows;
    struct stat sb;
    const char *input = NULL;
    FILE *infp = stdin;
    FILE *outfp = stdout;
    struct pix_png_args args = {NULL, 0, 0, -1.0,
	DEFAULT_IMAGE_DIMENSION, DEFAULT_IMAGE_DIMENSION, NULL};
    struct bu_vls msg = BU_VLS_INIT_ZERO;
    args.self = &args;

    bu_setprogname(argv[0]);

    int introspection = bu_arg_introspect(&msg, &pix_png_grammar, argc - 1,
	(const char * const *)(argv + 1));
    if (introspection != 0) {
	bu_flog(introspection > 0 ? stdout : stderr, "%s", bu_vls_cstr(&msg));
	bu_vls_free(&msg);
	return introspection > 0 ? 0 : 1;
    }
    int operand_index = bu_arg_schema_parse_complete(&pix_png_schema, &args,
	&msg, argc - 1, (const char **)(argv + 1));
    if (operand_index < 0)
	bu_exit(1, "%s", bu_vls_cstr(&msg));
    bu_vls_free(&msg);
    if (args.help) {
	char *help = bu_arg_schema_help(&pix_png_schema, "pix-png");
	bu_flog(stdout, "%s", help ? help : "");
	bu_free(help, "pix-png help");
	return 0;
    }

    if (operand_index < argc - 1)
	input = argv[operand_index + 1];
    if (!input && isatty(fileno(stdin)))
	bu_exit(1, "pix-png requires an input file or redirected input\n");
    if (!args.output && isatty(fileno(stdout)))
	bu_exit(1, "pix-png cannot send binary output to a terminal\n");
    if (input && (infp = fopen(input, "rb")) == NULL)
	bu_exit(1, "pix-png: cannot open \"%s\" for reading\n", input);
    if (args.output && (outfp = fopen(args.output, "wb+")) == NULL)
	bu_exit(1, "pix-png: cannot open \"%s\" for writing\n", args.output);

    setmode(fileno(infp), O_BINARY);
    setmode(fileno(outfp), O_BINARY);
    setmode(fileno(stdout), O_BINARY);

    if (args.autosize) {
	if (!input)
	    bu_exit(1, "pix-png: --autosize requires a named input file\n");
	if (!fb_common_file_size(&w, &h, input, BYTESPERPIXEL))
	    bu_exit(1, "pix-png: unable to autosize \"%s\"\n", input);
	args.width = w;
	args.height = h;
    }

    if (args.width > PNG_UINT_31_MAX || args.height > PNG_UINT_31_MAX ||
	args.width > SIZE_MAX / BYTESPERPIXEL ||
	args.height > SIZE_MAX / (args.width * BYTESPERPIXEL))
	bu_exit(1, "pix-png: image dimensions are too large\n");
    row_size = args.width * BYTESPERPIXEL;
    image_size = args.height * row_size;

    /* allocate space for the image */
    scanbuf = (unsigned char *)bu_calloc(image_size, sizeof(unsigned char),
	"scanbuf");

    /* create array of pointers to rows for libpng */
    rows = (unsigned char **)bu_calloc(args.height, sizeof(unsigned char *),
	"rows");
    for (i = 0; i < args.height; i++)
	rows[i] = scanbuf + ((args.height - i - 1) * row_size);

    /* read the pix file */
    if (fread(scanbuf, image_size, 1, infp) != 1)
	bu_exit(1, "%s: Short read\n", bu_getprogname());

    /* warn if we only read part of the input */
    if (fstat(fileno(infp), &sb) < 0) {
	perror("unable to stat file:");
	bu_exit(1, "ERROR: %s cannot proceed.", bu_getprogname());
    }

    if (image_size < (size_t)sb.st_size) {
	bu_log("WARNING: Output PNG image dimensions are smaller than the PIX input image\n");
	bu_log("Input image is %lu pixels, output image is %zu pixels\n",
	    (unsigned long)sb.st_size / BYTESPERPIXEL,
	    image_size / BYTESPERPIXEL);
	if (input && fb_common_file_size(&w, &h, input, BYTESPERPIXEL)) {
	    bu_log("Input PIX dimensions appear to be %zux%zu pixels.  ", w, h);
	    if (w == h) {
		bu_log("Try using the -s%zu option.\n", w);
	    } else {
		bu_log("Try using the -w%zu -n%zu options.\n", w, h);
	    }
	}
    }

    /* write out the data */
    write_png(outfp, rows, (png_uint_32)args.width,
	(png_uint_32)args.height, args.gamma);

    /* user requested separate file and redirected output */
    if (!isatty(fileno(stdout)) && outfp != stdout) {
	write_png(stdout, rows, (png_uint_32)args.width,
	    (png_uint_32)args.height, args.gamma);
    }

    /* release resources */

    bu_free(scanbuf, "scanbuf");
    bu_free(rows, "rows");

    if (infp != stdin)
	fclose(infp);
    if (outfp != stdout)
	fclose(outfp);

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
