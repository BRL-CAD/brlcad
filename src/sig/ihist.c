/*                         I H I S T . C
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
/** @file ihist.c
 *
 */

#include "common.h"

#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include <math.h>
#include "bio.h"

#include "bu/arg.h"
#include "bu/app.h"
#include "bu/exit.h"
#include "bu/malloc.h"

struct ihist_args {
    int help;
    int verbose;
};

static const struct bu_arg_option ihist_options[] = {
    BU_ARG_FLAG("h", "help", struct ihist_args, help, "Print help and exit"),
    BU_ARG_ALIAS("?", NULL, "help", 1),
    BU_ARG_FLAG("v", "verbose", struct ihist_args, verbose,
	"Print the complete value histogram"),
    BU_ARG_OPTION_NULL
};

static const struct bu_arg_schema ihist_schema = BU_ARG_SCHEMA(
    "ihist", "Report histogram and bit usage for signed-short values",
    ihist_options, NULL, BU_ARG_PARSE_OPTIONS_FIRST,
    BU_ARG_SCHEMA_META_HELP(NULL, NULL, NULL, NULL, NULL));

static const struct bu_arg_grammar ihist_grammar = {&ihist_schema, NULL, NULL};

int
main(int argc, char *argv[])
{
    long i, bit;
    int n;
    int max, min;
    long num, levels=0;

    long bits[16] = {0};
    long values[65536] = {0};
    long *zerop;
    short ibuf[1024] = {0};

    struct ihist_args args = {0};
    struct bu_vls msg = BU_VLS_INIT_ZERO;

    bu_setprogname(argv[0]);

    int introspection = bu_arg_introspect(&msg, &ihist_grammar, argc - 1,
	(const char * const *)(argv + 1));
    if (introspection != 0) {
	bu_flog(introspection > 0 ? stdout : stderr, "%s", bu_vls_cstr(&msg));
	bu_vls_free(&msg);
	return introspection > 0 ? 0 : 1;
    }

    int operand_index = bu_arg_schema_parse_complete(&ihist_schema, &args,
	&msg, argc - 1, (const char **)(argv + 1));
    if (operand_index < 0)
	bu_exit(1, "%s", bu_vls_cstr(&msg));
    bu_vls_free(&msg);
    if (args.help) {
	char *help = bu_arg_schema_help(&ihist_schema, "ihist");
	bu_flog(stdout, "%s", help ? help : "");
	bu_free(help, "ihist help");
	return 0;
    }
    if (isatty(fileno(stdin))) {
	char *help = bu_arg_schema_help(&ihist_schema, "ihist");
	bu_exit(1, "%s", help ? help : "");
    }

    num = 0;
    memset(values, 0, sizeof(values));
    memset(ibuf, 0, sizeof(ibuf));
    zerop = &values[32768];

    while ((n = fread(ibuf, 2, 1024, stdin)) > 0) {
	num += n;
	for (i = 0; i < n; i++) {
	    long idx = ibuf[i];
	    if (idx < 0)
		idx = 0;
	    else if (idx > LONG_MAX-1)
		idx = LONG_MAX-1;
	    zerop[idx]++;
	}
    }
    printf("%ld values\n", num);
    max = 32767;
    while (zerop[max] == 0)
	max--;
    min = -32768;
    while (zerop[min] == 0)
	min++;
    printf("Max = %d\n", max);
    printf("Min = %d\n", min);

    printf("Bits\n");
    for (i = -32768; i < 32768; i++) {
	if (zerop[i] == 0)
	    continue;
	levels++;
	for (bit = 0; bit < 16; bit++) {
	    if (i & (1<<bit))
		bits[ bit ] += zerop[i];
	}
    }
    for (bit = 0; bit < 16; bit++) {
	printf("%8ld: %10ld (%f)\n",
	       bit, bits[bit], (double)bits[bit]/(double)num);
    }

    printf("%ld levels used (%04.2f %%)\n", levels, levels/65535.0 * 100.0);
    if (args.verbose) {
	for (i = -32768; i < 32768; i++) {
	    if (zerop[i]) {
		printf("%ld %ld\n", i, zerop[i]);
	    }
	}
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
