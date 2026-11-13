/*                          D S E L . C
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
/** @file dsel.c
 *
 * select some number of doubles
 *
 */

#include "common.h"

#include <stdlib.h>
#include <math.h>
#include "bio.h"

#include "bu/arg.h"
#include "bu/app.h"
#include "bu/exit.h"
#include "bu/malloc.h"

struct dsel_args {
    int help;
};

static const struct bu_arg_option dsel_options[] = {
    BU_ARG_FLAG("h", "help", struct dsel_args, help, "Print help and exit"),
    BU_ARG_ALIAS("?", NULL, "help", 0),
    BU_ARG_OPTION_NULL
};

static const struct bu_arg_operand dsel_operands[] = {
    BU_ARG_OPERAND("count", BU_ARG_VALUE_INTEGER, 1, BU_ARG_COUNT_UNLIMITED,
	"One keep count, or alternating skip and keep counts", NULL),
    BU_ARG_OPERAND_NULL
};

static const struct bu_arg_schema dsel_schema = BU_ARG_SCHEMA(
    "dsel", "Select runs of double-precision values from a stream",
    dsel_options, dsel_operands, BU_ARG_PARSE_OPTIONS_FIRST,
    BU_ARG_SCHEMA_META_HELP(NULL, NULL, NULL, NULL, NULL));

static const struct bu_arg_grammar dsel_grammar = {&dsel_schema, NULL, NULL};

static void
skip(int num)
{
    double buf[BU_PAGE_SIZE] = {0};
    int n, m;

    while (num > 0) {
	n = num > 1024 ? 1024 : num;
	if ((m = fread(buf, sizeof(*buf), n, stdin)) == 0)
	    exit(0);
	num -= m;
    }
}


static void
keep(int num)
{
    double buf[BU_PAGE_SIZE] = {0};
    int n, m;
    size_t ret;

    while (num > 0) {
	n = num > 1024 ? 1024 : num;
	if ((m = fread(buf, sizeof(*buf), n, stdin)) == 0)
	    exit(0);
	ret = fwrite(buf, sizeof(*buf), m, stdout);
	if (ret != (size_t)m)
	    perror("fwrite");
	num -= n;
    }
}


int
main(int argc, char *argv[])
{
    int nskip;	/* number to skip */
    int nkeep;	/* number to keep */
    struct dsel_args args = {0};
    struct bu_vls msg = BU_VLS_INIT_ZERO;

    bu_setprogname(argv[0]);

    int introspection = bu_arg_introspect(&msg, &dsel_grammar, argc - 1,
	(const char * const *)(argv + 1));
    if (introspection != 0) {
	bu_flog(introspection > 0 ? stdout : stderr, "%s", bu_vls_cstr(&msg));
	bu_vls_free(&msg);
	return introspection > 0 ? 0 : 1;
    }

    argc--; argv++;
    int operand_index = bu_arg_schema_parse_complete(&dsel_schema, &args,
	&msg, argc, (const char **)argv);
    if (operand_index < 0)
	bu_exit(1, "%s", bu_vls_cstr(&msg));
    bu_vls_free(&msg);
    if (args.help) {
	char *help = bu_arg_schema_help(&dsel_schema, "dsel");
	bu_flog(stdout, "%s", help ? help : "");
	bu_free(help, "dsel help");
	return 0;
    }

    argc -= operand_index;
    argv += operand_index;
    if (isatty(fileno(stdin)) || isatty(fileno(stdout))) {
	char *help = bu_arg_schema_help(&dsel_schema, "dsel");
	bu_exit(1, "%s", help ? help : "");
    }

    if (argc == 1) {
	keep(atoi(argv[0]));
	exit(0);
    }


    while (argc > 0) {
	nskip = atoi(argv[0]);
	argc--;
	argv++;
	if (nskip > 0)
	    skip(nskip);

	if (argc > 0) {
	    nkeep = atoi(argv[0]);
	    argc--;
	    argv++;
	} else {
#define INTEGER_MAX (((int) ~0) >> 1)
	    nkeep = INTEGER_MAX;
	}

	if (nkeep <= 0)
	    exit(0);
	keep(nkeep);
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
