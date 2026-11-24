/*                        U S T A T S . C
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
/** @file ustats.c
 *
 * gather statistics on a set of unsigned shorts.
 *
 * Options
 * h (or ?) help
 */

#include "common.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include "bio.h"

#include "bu/arg.h"
#include "bu/app.h"
#include "bu/malloc.h"
#include "bu/file.h"
#include "bu/exit.h"

struct ustats_args {
    int help;
};

static const struct bu_arg_option ustats_options[] = {
    BU_ARG_FLAG("h", "help", struct ustats_args, help, "Print help and exit"),
    BU_ARG_ALIAS("?", NULL, "help", 1),
    BU_ARG_OPTION_NULL
};

static const struct bu_arg_operand ustats_operands[] = {
    BU_ARG_OPERAND("file", BU_ARG_VALUE_FILE, 0, 1,
	"Input unsigned-short data; standard input if omitted", NULL),
    BU_ARG_OPERAND_NULL
};

static const struct bu_arg_schema ustats_schema = BU_ARG_SCHEMA(
    "ustats", "Compute statistics for unsigned-short values",
    ustats_options, ustats_operands, BU_ARG_PARSE_OPTIONS_FIRST,
    BU_ARG_SCHEMA_META_HELP(NULL, NULL, NULL, NULL, NULL));

static const struct bu_arg_grammar ustats_grammar = {&ustats_schema, NULL, NULL};


static void
comp_stats(FILE *fd)
{
    unsigned short *buffer=(unsigned short *)NULL;
    unsigned short min = USHRT_MAX;
    unsigned short max = 0; /* sorry, did not find USHRT_MIN */
    double doub, stdev;
    double sum = 0.0;
    double sum_sq = 0.0;
    double num = 0.0;
    int count, i;

    buffer = (unsigned short *)bu_calloc(10240, sizeof(short), "buffer");

    while ((count=fread((void *)buffer, sizeof(short), 10240, fd))) {
	for (i=0; i < count; ++i) {
	    doub = (double)buffer[i];
	    sum += doub;
	    sum_sq += doub*doub;
	    if (buffer[i] > max) max = buffer[i];
	    if (buffer[i] < min) min = buffer[i];
	}
	num += (double)count;
    }

    stdev = sqrt(((num * sum_sq) - (sum*sum)) / (num * (num-1)));

    printf("   Num: %g\n   Min: %u\n   Max: %u\n   Sum: %g\n  Mean: %g\nSStdev: %g\n",
	   num, min, max, sum, sum/num, stdev);

    bu_free(buffer, "buffer");
}


/*
 * Call parse_args to handle command line arguments first, then
 * process input.
 */
int
main(int ac, char *av[])
{
    struct ustats_args args = {0};
    struct bu_vls msg = BU_VLS_INIT_ZERO;

    bu_setprogname(av[0]);

    int introspection = bu_arg_introspect(&msg, &ustats_grammar, ac - 1,
	(const char * const *)(av + 1));
    if (introspection != 0) {
	bu_flog(introspection > 0 ? stdout : stderr, "%s", bu_vls_cstr(&msg));
	bu_vls_free(&msg);
	return introspection > 0 ? 0 : 1;
    }

    ac--; av++;
    int operand_index = bu_arg_schema_parse_complete(&ustats_schema, &args,
	&msg, ac, (const char **)av);
    if (operand_index < 0)
	bu_exit(1, "%s", bu_vls_cstr(&msg));
    bu_vls_free(&msg);
    if (args.help) {
	char *help = bu_arg_schema_help(&ustats_schema, "ustats");
	bu_flog(stdout, "%s", help ? help : "");
	bu_free(help, "ustats help");
	return 0;
    }

    ac -= operand_index;
    av += operand_index;
    if (ac == 1) {
	char *ifname = bu_file_realpath(av[0], NULL);
	/* open file of unsigned shorts */
	if (freopen(ifname, "r", stdin) == (FILE *)NULL) {
	    perror(ifname);
	    bu_free(ifname, "ifname alloc from bu_file_realpath");
	    return -1;
	}
	bu_free(ifname, "ifname alloc from bu_file_realpath");
    } else if (isatty((int)fileno(stdin))) {
	char *help = bu_arg_schema_help(&ustats_schema, "ustats");
	bu_exit(1, "%s", help ? help : "");
    }

    comp_stats(stdin);

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
