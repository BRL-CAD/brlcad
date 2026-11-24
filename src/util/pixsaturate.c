/*                   P I X S A T U R A T E . C
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
/** @file util/pixsaturate.c
 *
 * Change the saturation of a row of pixels.  If sat is
 * set to 0.0 the result will be monochromatic, if sat is made
 * 1.0, the color will not change, if sat is made greater than 1.0,
 * the color intensity is increased.
 *
 * The input and output pixel values are in the range 0..255.
 *
 * This implementation, based on a subroutine by Paul Haeberli,
 * requires 6 multiplies, 5 adds, and 3 bound checks per pixel.
 */

#include "common.h"

#include "bio.h"

#include "bu/arg.h"
#include "bu/app.h"
#include "bu/exit.h"
#include "bu/log.h"
#include "bu/malloc.h"


#define RINTLUM (79)
#define GINTLUM (156)
#define BINTLUM (21)
#define RGB_CHANNEL_COUNT 3
#define INPUT_PIXEL_COUNT (16 * 1024)
#define INPUT_BUFFER_SIZE (RGB_CHANNEL_COUNT * INPUT_PIXEL_COUNT)
#define PIXEL_MAX 255

struct pixsaturate_args {
    int help;
};

static const struct bu_arg_option pixsaturate_options[] = {
    BU_ARG_FLAG("h", "help", struct pixsaturate_args, help,
	"Print help and exit"),
    BU_ARG_ALIAS("?", NULL, "help", 1),
    BU_ARG_OPTION_NULL
};

static const struct bu_arg_operand pixsaturate_operands[] = {
    BU_ARG_OPERAND_NUMBER_RANGE("saturation", 1, 1, 1, 0.0, 0, 0.0,
	"Non-negative color saturation factor", NULL),
    BU_ARG_OPERAND_NULL
};

static const struct bu_arg_schema pixsaturate_schema = BU_ARG_SCHEMA(
    "pixsaturate", "Adjust the color saturation of a PIX image",
    pixsaturate_options, pixsaturate_operands, BU_ARG_PARSE_OPTIONS_FIRST,
    BU_ARG_SCHEMA_META_HELP(NULL, NULL, NULL, NULL, NULL));

static const struct bu_arg_grammar pixsaturate_grammar = {
    &pixsaturate_schema, NULL, NULL
};

int
main(int argc, char **argv)
{
    unsigned char pixels[INPUT_BUFFER_SIZE];
    double sat;			/* saturation */
    int bw;			/* monochrome intensity */
    int rwgt, gwgt, bwgt;
    int rt, gt, bt;
    unsigned char *cp;
    struct pixsaturate_args args = {0};
    struct bu_vls msg = BU_VLS_INIT_ZERO;

    bu_setprogname(argv[0]);

    int introspection = bu_arg_introspect(&msg, &pixsaturate_grammar, argc - 1,
	(const char * const *)(argv + 1));
    if (introspection != 0) {
	bu_flog(introspection > 0 ? stdout : stderr, "%s", bu_vls_cstr(&msg));
	bu_vls_free(&msg);
	return introspection > 0 ? 0 : 1;
    }
    int operand_index = bu_arg_schema_parse_complete(&pixsaturate_schema, &args,
	&msg, argc - 1, (const char **)(argv + 1));
    if (operand_index < 0)
	bu_exit(1, "%s", bu_vls_cstr(&msg));
    bu_vls_free(&msg);
    if (args.help) {
	char *help = bu_arg_schema_help(&pixsaturate_schema, "pixsaturate");
	bu_flog(stdout, "%s", help ? help : "");
	bu_free(help, "pixsaturate help");
	return 0;
    }

    const char *saturation = argv[operand_index + 1];
    if (!bu_arg_double_from_str(&msg, &sat, saturation))
	bu_exit(1, "%s: saturation: %s\n", bu_getprogname(), bu_vls_cstr(&msg));
    bu_vls_free(&msg);

    if (isatty(fileno(stdin)) || isatty(fileno(stdout)))
	bu_exit(1, "pixsaturate requires redirected input and output\n");

    setmode(fileno(stdin), O_BINARY);
    setmode(fileno(stdout), O_BINARY);

    rwgt = RINTLUM*(1.0-sat);
    gwgt = GINTLUM*(1.0-sat);
    bwgt = BINTLUM*(1.0-sat);

    size_t bytes_read;
    while ((bytes_read = fread(pixels, 1, sizeof(pixels), stdin)) > 0) {
	if (bytes_read % RGB_CHANNEL_COUNT)
	    bu_exit(1, "pixsaturate: input ends with an incomplete pixel\n");
	cp = pixels;
	for (size_t bytes_left = bytes_read; bytes_left > 0;
	     bytes_left -= RGB_CHANNEL_COUNT) {
	    rt = cp[0];
	    gt = cp[1];
	    bt = cp[2];
	    bw = (rwgt*rt + gwgt*gt + bwgt*bt)>>8;
	    rt = bw+sat*rt;
	    gt = bw+sat*gt;
	    bt = bw+sat*bt;
	    if (rt < 0) rt = 0; else if (rt > PIXEL_MAX) rt = PIXEL_MAX;
	    if (gt < 0) gt = 0; else if (gt > PIXEL_MAX) gt = PIXEL_MAX;
	    if (bt < 0) bt = 0; else if (bt > PIXEL_MAX) bt = PIXEL_MAX;
	    *cp++ = rt;
	    *cp++ = gt;
	    *cp++ = bt;
	}
	if (fwrite(pixels, 1, bytes_read, stdout) != bytes_read)
	    bu_exit(1, "pixsaturate: error writing output\n");
    }
    if (ferror(stdin))
	bu_exit(1, "pixsaturate: error reading input\n");

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
