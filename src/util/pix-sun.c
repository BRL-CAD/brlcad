/*                       P I X - S U N . C
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
 */
/** @file util/pix-sun.c
 *
 * Convert a BRL-CAD PIX image to a Sun rasterfile.
 */

#include "common.h"

#include <stdint.h>

#include "bio.h"

#include "bu/arg.h"
#include "bu/app.h"
#include "bu/exit.h"
#include "bu/malloc.h"
#include "icv.h"

#define DEFAULT_IMAGE_DIMENSION 512

struct pix_sun_args {
    struct pix_sun_args *self;
    int help;
    int dither;
    size_t width;
    size_t height;
};

enum pix_sun_dimension {
    PIX_SUN_SIZE,
    PIX_SUN_WIDTH,
    PIX_SUN_HEIGHT
};

static struct pix_sun_args *
pix_sun_args_from_storage(void *storage)
{
    return storage ? *(struct pix_sun_args **)storage : NULL;
}

static int
pix_sun_dimension(struct bu_vls *msg, const char *arg, void *storage,
	enum pix_sun_dimension dimension)
{
    struct pix_sun_args *args = pix_sun_args_from_storage(storage);
    size_t value;

    if (!bu_arg_size_t_range_from_str(msg, &value, arg, 10, 1, UINT32_MAX))
	return -1;
    if (args) {
	if (dimension == PIX_SUN_SIZE)
	    args->width = args->height = value;
	else if (dimension == PIX_SUN_WIDTH)
	    args->width = value;
	else
	    args->height = value;
    }
    return 0;
}

#define PIX_SUN_DIMENSION_CALLBACK(_name, _dimension) \
    static int _name(struct bu_vls *msg, const char *arg, void *storage) \
    { \
	return pix_sun_dimension(msg, arg, storage, _dimension); \
    }

PIX_SUN_DIMENSION_CALLBACK(pix_sun_size, PIX_SUN_SIZE)
PIX_SUN_DIMENSION_CALLBACK(pix_sun_width, PIX_SUN_WIDTH)
PIX_SUN_DIMENSION_CALLBACK(pix_sun_height, PIX_SUN_HEIGHT)

static const struct bu_arg_option pix_sun_options[] = {
    BU_ARG_FLAG("h", "help", struct pix_sun_args, help,
	"Print help and exit"),
    BU_ARG_ALIAS("?", NULL, "help", 1),
    BU_ARG_FLAG("d", "dither", struct pix_sun_args, dither,
	"Dither colors while quantizing"),
    BU_ARG_CUSTOM("s", "size", struct pix_sun_args, self, pix_sun_size,
	"pixels", "Set both input dimensions"),
    BU_ARG_CUSTOM("w", "width", struct pix_sun_args, self, pix_sun_width,
	"pixels", "Set the input image width"),
    BU_ARG_CUSTOM("n", "height", struct pix_sun_args, self, pix_sun_height,
	"pixels", "Set the input image height"),
    BU_ARG_OPTION_NULL
};

static const struct bu_arg_schema pix_sun_schema = BU_ARG_SCHEMA(
    "pix-sun", "Convert a BRL-CAD PIX image to a Sun rasterfile",
    pix_sun_options, NULL, BU_ARG_PARSE_OPTIONS_FIRST,
    BU_ARG_SCHEMA_META_HELP(NULL, NULL, NULL, NULL, NULL));

static const struct bu_arg_grammar pix_sun_grammar = {
    &pix_sun_schema, NULL, NULL
};

int
main(int argc, char **argv)
{
    struct pix_sun_args args = {NULL, 0, 0, DEFAULT_IMAGE_DIMENSION,
	DEFAULT_IMAGE_DIMENSION};
    struct bu_vls msg = BU_VLS_INIT_ZERO;
    icv_image_t *image;

    args.self = &args;
    bu_setprogname(argv[0]);

    int introspection = bu_arg_introspect(&msg, &pix_sun_grammar, argc - 1,
	(const char * const *)(argv + 1));
    if (introspection != 0) {
	bu_flog(introspection > 0 ? stdout : stderr, "%s", bu_vls_cstr(&msg));
	bu_vls_free(&msg);
	return introspection > 0 ? 0 : 1;
    }
    int operand_index = bu_arg_schema_parse_complete(&pix_sun_schema, &args,
	&msg, argc - 1, (const char **)(argv + 1));
    if (operand_index < 0)
	bu_exit(1, "%s", bu_vls_cstr(&msg));
    bu_vls_free(&msg);

    if (args.help) {
	char *help = bu_arg_schema_help(&pix_sun_schema, "pix-sun");
	bu_flog(stdout, "%s", help ? help : "");
	bu_free(help, "pix-sun help");
	return 0;
    }
    if (isatty(fileno(stdin)))
	bu_exit(1, "pix-sun requires redirected PIX input\n");
    if (isatty(fileno(stdout)))
	bu_exit(1, "pix-sun cannot send raster output to a terminal\n");

    setmode(fileno(stdin), O_BINARY);
    setmode(fileno(stdout), O_BINARY);
    image = icv_read(NULL, BU_MIME_IMAGE_PIX, args.width, args.height);
    if (!image)
	bu_exit(1, "pix-sun: unable to read %zu x %zu PIX input\n",
	    args.width, args.height);
    if (icv_write_sun(image, NULL, args.dither) != BRLCAD_OK) {
	icv_destroy(image);
	bu_exit(1, "pix-sun: unable to write Sun rasterfile output\n");
    }
    icv_destroy(image);
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
