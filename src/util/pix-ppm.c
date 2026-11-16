/*                       P I X - P P M . C
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
/** @file util/pix-ppm.c
 *
 * convert BRL .pix files to ppm
 *
 * Pixels run left-to-right, from the bottom of the screen to the top.
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


#define DEFAULT_IMAGE_DIMENSION 512

struct pix_ppm_args {
    struct pix_ppm_args *self;
    int help;
    int autosize;
    int pixel_bytes;
    size_t width;
    size_t height;
    const char *output;
};

enum pix_ppm_dimension {
    PIX_PPM_SIZE,
    PIX_PPM_WIDTH,
    PIX_PPM_HEIGHT
};

static struct pix_ppm_args *
pix_ppm_args_from_storage(void *storage)
{
    return storage ? *(struct pix_ppm_args **)storage : NULL;
}

static int
pix_ppm_autosize(struct bu_vls *UNUSED(msg), const char *UNUSED(arg),
	void *storage)
{
    struct pix_ppm_args *args = pix_ppm_args_from_storage(storage);
    if (args)
	args->autosize = 1;
    return 0;
}

static int
pix_ppm_pixel_bytes(struct bu_vls *msg, const char *arg, void *storage)
{
    struct pix_ppm_args *args = pix_ppm_args_from_storage(storage);
    int value;

    if (!bu_arg_integer_from_str(msg, &value, arg, 10))
	return -1;
    if (value != 1 && value != 3) {
	if (msg)
	    bu_vls_printf(msg, "bytes per pixel must be 1 or 3\n");
	return -1;
    }
    if (args)
	args->pixel_bytes = value;
    return 0;
}

static int
pix_ppm_dimension(struct bu_vls *msg, const char *arg, void *storage,
	enum pix_ppm_dimension dimension)
{
    struct pix_ppm_args *args = pix_ppm_args_from_storage(storage);
    size_t value;

    if (!bu_arg_size_t_range_from_str(msg, &value, arg, 10, 1, SIZE_MAX))
	return -1;
    if (args) {
	if (dimension == PIX_PPM_SIZE)
	    args->width = args->height = value;
	else if (dimension == PIX_PPM_WIDTH)
	    args->width = value;
	else
	    args->height = value;
	args->autosize = 0;
    }
    return 0;
}

#define PIX_PPM_DIMENSION_CALLBACK(_name, _dimension) \
    static int _name(struct bu_vls *msg, const char *arg, void *storage) \
    { \
	return pix_ppm_dimension(msg, arg, storage, _dimension); \
    }

PIX_PPM_DIMENSION_CALLBACK(pix_ppm_size, PIX_PPM_SIZE)
PIX_PPM_DIMENSION_CALLBACK(pix_ppm_width, PIX_PPM_WIDTH)
PIX_PPM_DIMENSION_CALLBACK(pix_ppm_height, PIX_PPM_HEIGHT)

static const struct bu_arg_option pix_ppm_options[] = {
    BU_ARG_FLAG("h", "help", struct pix_ppm_args, help,
	"Print help and exit"),
    BU_ARG_ALIAS("?", NULL, "help", 1),
    BU_ARG_CUSTOM_FLAG("a", "autosize", "autosize", struct pix_ppm_args,
	self, pix_ppm_autosize, "Derive dimensions from a named input file"),
    BU_ARG_CUSTOM("#", "bytes-per-pixel", struct pix_ppm_args, self,
	pix_ppm_pixel_bytes, "count", "Set input bytes per pixel to 1 or 3"),
    BU_ARG_CUSTOM("s", "size", struct pix_ppm_args, self, pix_ppm_size,
	"pixels", "Set both input dimensions"),
    BU_ARG_CUSTOM("w", "width", struct pix_ppm_args, self, pix_ppm_width,
	"pixels", "Set the input image width"),
    BU_ARG_CUSTOM("n", "height", struct pix_ppm_args, self, pix_ppm_height,
	"pixels", "Set the input image height"),
    BU_ARG_VALUE("o", "output", struct pix_ppm_args, output,
	BU_ARG_VALUE_FILE, BU_ARG_REQUIRED, "file", "Write PPM or PGM output to a file"),
    BU_ARG_OPTION_NULL
};

static const struct bu_arg_operand pix_ppm_operands[] = {
    BU_ARG_OPERAND("input", BU_ARG_VALUE_FILE, 0, 1,
	"Input PIX or BW file; omit to read standard input", NULL),
    BU_ARG_OPERAND_NULL
};

static const struct bu_arg_schema pix_ppm_schema = BU_ARG_SCHEMA(
    "pix-ppm", "Convert a BRL-CAD PIX or BW image to Netpbm format",
    pix_ppm_options, pix_ppm_operands, BU_ARG_PARSE_OPTIONS_FIRST,
    BU_ARG_SCHEMA_META_HELP(NULL, NULL, NULL, NULL, NULL));

static const struct bu_arg_grammar pix_ppm_grammar = {
    &pix_ppm_schema, NULL, NULL
};


static int
write_ppm(FILE *fp, const unsigned char *data, size_t width, size_t height,
	int bytes_per_pixel)
{
    size_t i;
    const unsigned char *row;
    const size_t row_size = width * (size_t)bytes_per_pixel;

    if (bytes_per_pixel == 1) {
	/* Binary PGM magic number */
	if (fprintf(fp, "P5\n") < 0)
	    return 0;
    } else {
	/* PPM magic number */
	if (fprintf(fp, "P6\n") < 0)
	    return 0;
    }

    if (fprintf(fp, "%zu %zu\n255\n", width, height) < 0 || fflush(fp) != 0)
	return 0;

    /*
     * now write them out in the right order, 'cause the
     * input is upside down.
     */

    for (i = 0; i < height; i++) {
	row = data + (height - 1 - i) * row_size;
	if (fwrite(row, 1, row_size, fp) != row_size)
	    return 0;
    }
    return !ferror(fp);
}


int
main(int argc, char *argv[])
{
    struct pix_ppm_args args = {NULL, 0, 0, 3, DEFAULT_IMAGE_DIMENSION,
	DEFAULT_IMAGE_DIMENSION, NULL};
    struct bu_vls msg = BU_VLS_INIT_ZERO;
    const char *input = NULL;
    FILE *infp = stdin;
    FILE *outfp = stdout;
    unsigned char *scanbuf;
    size_t image_size;
    int status = 0;
    args.self = &args;

    bu_setprogname(argv[0]);

    int introspection = bu_arg_introspect(&msg, &pix_ppm_grammar, argc - 1,
	(const char * const *)(argv + 1));
    if (introspection != 0) {
	bu_flog(introspection > 0 ? stdout : stderr, "%s", bu_vls_cstr(&msg));
	bu_vls_free(&msg);
	return introspection > 0 ? 0 : 1;
    }
    int operand_index = bu_arg_schema_parse_complete(&pix_ppm_schema, &args,
	&msg, argc - 1, (const char **)(argv + 1));
    if (operand_index < 0)
	bu_exit(1, "%s", bu_vls_cstr(&msg));
    bu_vls_free(&msg);
    if (args.help) {
	char *help = bu_arg_schema_help(&pix_ppm_schema, "pix-ppm");
	bu_flog(stdout, "%s", help ? help : "");
	bu_free(help, "pix-ppm help");
	return 0;
    }

    if (operand_index < argc - 1)
	input = argv[operand_index + 1];
    if (!input && isatty(fileno(stdin)))
	bu_exit(1, "pix-ppm requires an input file or redirected input\n");
    if (!args.output && isatty(fileno(stdout)))
	bu_exit(1, "pix-ppm cannot send image output to a terminal\n");
    if (input && (infp = fopen(input, "rb")) == NULL)
	bu_exit(1, "pix-ppm: cannot open \"%s\" for reading\n", input);
    if (args.output && (outfp = fopen(args.output, "wb+")) == NULL)
	bu_exit(1, "pix-ppm: cannot open \"%s\" for writing\n", args.output);

    setmode(fileno(infp), O_BINARY);
    setmode(fileno(outfp), O_BINARY);
    setmode(fileno(stdout), O_BINARY);

    if (args.autosize) {
	size_t w, h;
	if (!input)
	    bu_exit(1, "pix-ppm: --autosize requires a named input file\n");
	if (!fb_common_file_size(&w, &h, input, args.pixel_bytes))
	    bu_exit(1, "pix-ppm: unable to autosize \"%s\"\n", input);
	args.width = w;
	args.height = h;
    }

    if (args.width > SIZE_MAX / (size_t)args.pixel_bytes ||
	args.height > SIZE_MAX / (args.width * (size_t)args.pixel_bytes))
	bu_exit(1, "pix-ppm: image dimensions are too large\n");
    image_size = args.width * args.height * (size_t)args.pixel_bytes;

    scanbuf = (unsigned char *)bu_malloc(image_size, "scanbuf");
    if (fread(scanbuf, 1, image_size, infp) != image_size)
	bu_exit(1, "%s: Short read\n", bu_getprogname());

    if (!write_ppm(outfp, scanbuf, args.width, args.height,
	    args.pixel_bytes))
	status = 1;

    /* user requested separate file and redirected output */
    if (!isatty(fileno(stdout)) && outfp != stdout &&
	!write_ppm(stdout, scanbuf, args.width, args.height, args.pixel_bytes))
	status = 1;

    bu_free(scanbuf, "scanbuf");
    if (infp != stdin)
	fclose(infp);
    if (outfp != stdout)
	fclose(outfp);
    return status;
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
