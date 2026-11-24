/*                      B W T H R E S H . C
 * BRL-CAD
 *
 * Copyright (c) 1990-2026 United States Government as represented by
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
/** @file util/bwthresh.c
 *
 * Threshold data in BW(5) format.
 *
 * Accepts n arguments, the threshold values.  The grey scale 0..255
 * is divided evenly into n+1 bins.  Each pixel of input is binned
 * according to the threshold values, and the appropriate bin color
 * is output.
 *
 */

#include "common.h"

#include <stdio.h>
#include "bio.h"

#include "bu/arg.h"
#include "bu/app.h"
#include "bu/exit.h"
#include "bu/log.h"
#include "bu/malloc.h"
#include "bu/str.h"


struct bwthresh_args {
    int help;
};

static const struct bu_arg_option bwthresh_options[] = {
    BU_ARG_FLAG("h", "help", struct bwthresh_args, help,
	"Print help and exit"),
    BU_ARG_ALIAS("?", NULL, "help", 1),
    BU_ARG_OPTION_NULL
};

static const struct bu_arg_operand bwthresh_operands[] = {
    BU_ARG_OPERAND_INTEGER_RANGE("threshold", 1, 255, 1, 0, 1, 255,
	"Strictly increasing grayscale threshold", NULL),
    BU_ARG_OPERAND_NULL
};

static int
bwthresh_validate(const struct bu_arg_schema *UNUSED(schema), size_t argc,
	const char **argv, size_t cursor_arg, struct bu_arg_validate_result *result)
{
    if (!result || cursor_arg < argc || result->state == BU_ARG_VALIDATE_INVALID)
	return 0;

    size_t index = argc > 0 && BU_STR_EQUAL(argv[0], "--") ? 1 : 0;
    int previous = -1;
    for (; index < argc; index++) {
	int threshold;
	if (!bu_arg_integer_range_from_str(NULL, &threshold, argv[index], 10, 0, 255))
	    return 0;
	if (threshold <= previous) {
	    result->state = BU_ARG_VALIDATE_INVALID;
	    result->token_start = index;
	    result->token_end = index + 1;
	    result->hint = "threshold values must be strictly increasing";
	    return 0;
	}
	previous = threshold;
    }
    return 0;
}

static const struct bu_arg_schema bwthresh_schema = BU_ARG_SCHEMA(
    "bwthresh", "Quantize a BW stream into intensity bins",
    bwthresh_options, bwthresh_operands, BU_ARG_PARSE_OPTIONS_FIRST,
    BU_ARG_SCHEMA_META_HELP(NULL, NULL, bwthresh_validate, NULL, NULL));

static const struct bu_arg_grammar bwthresh_grammar = {
    &bwthresh_schema, NULL, NULL
};


int
main (int argc, char **argv)
{
    int Ch;		/* The current input character */
    int *thresh_val;	/* The threshold value */
    int nm_threshs;	/* How many thresholds? */
    int i;
    unsigned char *bin_color = (unsigned char *)0;/* resultant pixel values */
    struct bwthresh_args args = {0};
    struct bu_vls msg = BU_VLS_INIT_ZERO;

    bu_setprogname(argv[0]);

    int introspection = bu_arg_introspect(&msg, &bwthresh_grammar, argc - 1,
	(const char * const *)(argv + 1));
    if (introspection != 0) {
	bu_flog(introspection > 0 ? stdout : stderr, "%s", bu_vls_cstr(&msg));
	bu_vls_free(&msg);
	return introspection > 0 ? 0 : 1;
    }
    int operand_index = bu_arg_schema_parse_complete(&bwthresh_schema, &args,
	&msg, argc - 1, (const char **)(argv + 1));
    if (operand_index < 0)
	bu_exit(1, "%s", bu_vls_cstr(&msg));
    bu_vls_free(&msg);
    if (args.help) {
	char *help = bu_arg_schema_help(&bwthresh_schema, "bwthresh");
	bu_flog(stdout, "%s", help ? help : "");
	bu_free(help, "bwthresh help");
	return 0;
    }

    setmode(fileno(stdin), O_BINARY);
    setmode(fileno(stdout), O_BINARY);

    nm_threshs = argc - operand_index - 1;
    thresh_val = (int *)bu_malloc((unsigned) (nm_threshs * sizeof(int)), "thresh_val");
    bin_color = (unsigned char *)bu_malloc((unsigned) ((nm_threshs + 1) * sizeof(unsigned char)), "bin_color");

    for (i = 0; i < nm_threshs; ++i) {
	const char *value = argv[operand_index + i + 1];
	if (!bu_arg_integer_range_from_str(&msg, thresh_val + i, value, 10, 0, 255))
	    bu_exit(1, "bwthresh: threshold: %s\n", bu_vls_cstr(&msg));
	bin_color[i] = 256 * i / nm_threshs;
    }
    bu_vls_free(&msg);
    bin_color[i] = 255;

    while ((Ch = getchar()) != EOF) {
	for (i = 0; i < nm_threshs; ++i)
	    if (Ch < thresh_val[i])
		break;
	if (putchar(bin_color[i]) == EOF)
	    bu_exit(1, "bwthresh: error writing output\n");
    }
    if (ferror(stdin))
	bu_exit(1, "bwthresh: error reading input\n");
    if (fflush(stdout) == EOF)
	bu_exit(1, "bwthresh: error writing output\n");

    bu_free(thresh_val, "thresh_val");
    bu_free(bin_color, "bin_color");
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
