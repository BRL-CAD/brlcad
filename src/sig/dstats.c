/*                        D S T A T S . C
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
/** @file dstats.c
 *
 * Compute statistics of double precision floats.
 * Gives min, max, mode, median, mean, s.d., var, and skew.
 *
 */

#include "common.h"

#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <float.h>
#include "bio.h"

#include "bu/arg.h"
#include "bu/app.h"
#include "bu/exit.h"
#include "bu/malloc.h"

struct dstats_args {
    int help;
};

static const struct bu_arg_option dstats_options[] = {
    BU_ARG_FLAG("h", "help", struct dstats_args, help, "Print help and exit"),
    BU_ARG_ALIAS("?", NULL, "help", 1),
    BU_ARG_OPTION_NULL
};

static const struct bu_arg_operand dstats_operands[] = {
    BU_ARG_OPERAND("file.doubles", BU_ARG_VALUE_FILE, 0, 1,
	"Input double data; standard input if omitted", NULL),
    BU_ARG_OPERAND_NULL
};

static const struct bu_arg_schema dstats_schema = BU_ARG_SCHEMA(
    "dstats", "Compute statistics for double-precision values",
    dstats_options, dstats_operands, BU_ARG_PARSE_OPTIONS_FIRST,
    BU_ARG_SCHEMA_META_HELP(NULL, NULL, NULL, NULL, NULL));

static const struct bu_arg_grammar dstats_grammar = {&dstats_schema, NULL, NULL};


int
main(int ac, char *av[])
{
#define IBUFSIZE 1024		/* Max read size */
    double buf[IBUFSIZE];		/* Input buffer */

    int i, n;
    long num_values = 0;
    double *bp;
    double sum = 0.0;
    double sum2 = 0.0;
    double min = DBL_MAX;
    double max = DBL_MIN;
    double mean, var;
    FILE *fp;
    struct dstats_args args = {0};
    struct bu_vls msg = BU_VLS_INIT_ZERO;

    bu_setprogname(av[0]);

    int introspection = bu_arg_introspect(&msg, &dstats_grammar, ac - 1,
	(const char * const *)(av + 1));
    if (introspection != 0) {
	bu_flog(introspection > 0 ? stdout : stderr, "%s", bu_vls_cstr(&msg));
	bu_vls_free(&msg);
	return introspection > 0 ? 0 : 1;
    }

    ac--; av++;
    int operand_index = bu_arg_schema_parse_complete(&dstats_schema, &args,
	&msg, ac, (const char **)av);
    if (operand_index < 0)
	bu_exit(1, "%s", bu_vls_cstr(&msg));
    bu_vls_free(&msg);
    if (args.help) {
	char *help = bu_arg_schema_help(&dstats_schema, "dstats");
	bu_flog(stdout, "%s", help ? help : "");
	bu_free(help, "dstats help");
	return 0;
    }

    ac -= operand_index;
    av += operand_index;
    if (ac == 1) {
	if ((fp = fopen(av[0], "r")) == 0) {
	    bu_exit(1, "dstats: can't open \"%s\"\n", av[0]);
	}
    } else
	fp = stdin;

    if (ac == 0 && isatty(fileno(stdin)) && isatty(fileno(stdout))) {
	char *help = bu_arg_schema_help(&dstats_schema, "dstats");
	bu_exit(1, "%s", help ? help : "");
    }

    /*
     * Find min, max.
     */
    while ((n = fread(buf, sizeof(*buf), IBUFSIZE, fp)) > 0) {
	num_values += n;
	bp = &buf[0];
	for (i = 0; i < n; i++) {
	    sum += *bp;
	    sum2 += *bp * *bp;
	    if (*bp < min)
		min = *bp;
	    if (*bp > max)
		max = *bp;
	    bp++;
	}
    }
    mean = sum/(double)num_values;
    var = sum2/(double)num_values - mean * mean;

    /*
     * Display the results.
     */
    printf("Values  %14ld (%.0f x %.0f)\n", num_values,
	   sqrt((double)num_values), sqrt((double)num_values));
    printf("Min     %14.6g\n", min);
    printf("Max     %14.6g\n", max);
    printf("Mean    %14.6g\n", mean);
    printf("s.d.    %14.6g\n", sqrt(var));
    printf("Var     %14.6g\n", var);

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
