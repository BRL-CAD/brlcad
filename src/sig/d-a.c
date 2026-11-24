/*                           D - A . C
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
/** @file d-a.c
 *
 * double to ascii.
 *
 */

#include "common.h"

#include "bio.h"

#include "bu/arg.h"
#include "bu/app.h"
#include "bu/exit.h"
#include "bu/malloc.h"
#include "bu/snooze.h"

struct d_a_args {
    int help;
    int number;
};

static const struct bu_arg_option d_a_options[] = {
    BU_ARG_FLAG("h", "help", struct d_a_args, help, "Print help and exit"),
    BU_ARG_ALIAS("?", NULL, "help", 1),
    BU_ARG_FLAG("n", "number", struct d_a_args, number,
	"Prefix each value with its zero-based index"),
    BU_ARG_OPTION_NULL
};

static const struct bu_arg_schema d_a_schema = BU_ARG_SCHEMA(
    "d-a", "Convert binary doubles to ASCII numbers", d_a_options, NULL,
    BU_ARG_PARSE_OPTIONS_FIRST,
    BU_ARG_SCHEMA_META_HELP(NULL, NULL, NULL, NULL, NULL));

static const struct bu_arg_grammar d_a_grammar = {&d_a_schema, NULL, NULL};


int
main(int argc, char *argv[])
{
    double d;
    struct d_a_args args = {0};
    struct bu_vls msg = BU_VLS_INIT_ZERO;

    bu_setprogname(argv[0]);

    int introspection = bu_arg_introspect(&msg, &d_a_grammar, argc - 1,
	(const char * const *)(argv + 1));
    if (introspection != 0) {
	bu_flog(introspection > 0 ? stdout : stderr, "%s", bu_vls_cstr(&msg));
	bu_vls_free(&msg);
	return introspection > 0 ? 0 : 1;
    }
    int operand_index = bu_arg_schema_parse_complete(&d_a_schema, &args,
	&msg, argc - 1, (const char **)(argv + 1));
    if (operand_index < 0)
	bu_exit(1, "%s", bu_vls_cstr(&msg));
    bu_vls_free(&msg);
    if (args.help) {
	char *help = bu_arg_schema_help(&d_a_schema, "d-a");
	bu_flog(stdout, "%s", help ? help : "");
	bu_free(help, "d-a help");
	return 0;
    }

    fprintf(stderr,"DEPRECATION WARNING:  This command is scheduled for removal.  Please contact the developers if you use this command.\n\n");
    bu_snooze(BU_SEC2USEC(1));

    if (isatty(fileno(stdin)))
	bu_exit(1, "d-a requires redirected binary input\n");

    if (args.number) {
	long n;
	n = 0;
	while (fread(&d, sizeof(d), 1, stdin) == 1) {
	    printf("%ld %9g\n", n++, d);
	}
    } else {
	while (fread(&d, sizeof(d), 1, stdin) == 1) {
	    printf("%9g\n", d);
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
