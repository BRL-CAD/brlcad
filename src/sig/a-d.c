/*                           A - D . C
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
/** @file a-d.c
 *
 * Ascii to double.
 *
 */

#include "common.h"

#include <math.h>
#include <ctype.h>
#include <string.h>
#include "bio.h"
#include "bu/arg.h"
#include "bu/app.h"
#include "bu/exit.h"
#include "bu/malloc.h"
#include "bu/snooze.h"


#define COMMENT_CHAR '#'

struct a_d_args {
    int help;
};

static const struct bu_arg_option a_d_options[] = {
    BU_ARG_FLAG("h", "help", struct a_d_args, help, "Print help and exit"),
    BU_ARG_ALIAS("?", NULL, "help", 1),
    BU_ARG_OPTION_NULL
};

static const struct bu_arg_operand a_d_operands[] = {
    BU_ARG_OPERAND("value", BU_ARG_VALUE_NUMBER, 0, BU_ARG_COUNT_UNLIMITED,
	"Numeric value to convert instead of reading standard input", NULL),
    BU_ARG_OPERAND_NULL
};

static const struct bu_arg_schema a_d_schema = BU_ARG_SCHEMA(
    "a-d", "Convert ASCII numbers to binary doubles", a_d_options,
    a_d_operands, BU_ARG_PARSE_OPTIONS_FIRST,
    BU_ARG_SCHEMA_META_HELP(NULL, NULL, NULL, NULL, NULL));

static const struct bu_arg_grammar a_d_grammar = {&a_d_schema, NULL, NULL};


int
main(int argc, char *argv[])
{
    double d;
    int i;
    size_t ret;
    struct a_d_args args = {0};
    struct bu_vls msg = BU_VLS_INIT_ZERO;

    bu_setprogname(argv[0]);

    int introspection = bu_arg_introspect(&msg, &a_d_grammar, argc - 1,
	(const char * const *)(argv + 1));
    if (introspection != 0) {
	bu_flog(introspection > 0 ? stdout : stderr, "%s", bu_vls_cstr(&msg));
	bu_vls_free(&msg);
	return introspection > 0 ? 0 : 1;
    }

    argc--; argv++;
    int operand_index = bu_arg_schema_parse_complete(&a_d_schema, &args,
	&msg, argc, (const char **)argv);
    if (operand_index < 0)
	bu_exit(1, "%s", bu_vls_cstr(&msg));
    bu_vls_free(&msg);
    if (args.help) {
	char *help = bu_arg_schema_help(&a_d_schema, "a-d");
	bu_flog(stdout, "%s", help ? help : "");
	bu_free(help, "a-d help");
	return 0;
    }
    argc -= operand_index;
    argv += operand_index;

    fprintf(stderr,"DEPRECATION WARNING:  This command is scheduled for removal.  Please contact the developers if you use this command.\n\n");
    bu_snooze(BU_SEC2USEC(1));

    if (isatty(fileno(stdout))) {
	bu_exit(1, "a-d requires redirected binary output\n");
    }

    if (argc > 0) {
	/* get them from the command line */
	for (i = 0; i < argc; i++) {
	    if (!bu_arg_double_from_str(&msg, &d, argv[i]))
		bu_exit(1, "a-d: value: %s", bu_vls_cstr(&msg));
	    ret = fwrite(&d, sizeof(d), 1, stdout);
	    if (ret != 1)
		perror("fwrite");
	}
    } else {
	/* get them from stdin */
	while (1) {
	    int ch;

	    while (isspace(ch = getchar()))
		;
	    if (ch == COMMENT_CHAR) {
		while (((ch = getchar()) != '\n') && (ch != EOF))
		    ;
	    }
	    if (ch == EOF)
		bu_exit(0, NULL);
	    else
		ungetc(ch, stdin);

	    if (scanf("%lf", &d) == 1) {
		ret = fwrite(&d, sizeof(d), 1, stdout);
		if (ret != 1)
		    perror("fwrite");
	    }
	    else if (feof(stdin))
		bu_exit(0, NULL);
	    else {
		bu_exit(1, "Error in input stream\n");
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
