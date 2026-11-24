/*                    W I N D O W _ A R G S . H
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
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

#ifndef SIG_WINDOW_ARGS_H
#define SIG_WINDOW_ARGS_H

#include <limits.h>

#include "bu/arg.h"
#include "bu/malloc.h"

#define SIG_WINDOW_SIZE_DEFAULT 512

struct sig_window_options {
    int help;
};

static const struct bu_arg_option sig_window_options[] = {
    BU_ARG_FLAG("h", "help", struct sig_window_options, help,
	"Print help and exit"),
    BU_ARG_ALIAS("?", NULL, "help", 1),
    BU_ARG_OPTION_NULL
};

static const struct bu_arg_operand sig_window_operands[] = {
    BU_ARG_OPERAND("window-size", BU_ARG_VALUE_INTEGER, 0, 1,
	"Samples per processing window (default: 512)", NULL),
    BU_ARG_OPERAND_NULL
};

static int
sig_window_args_parse(int *window_size, const char *name,
	const char *description, int maximum, int argc, char *argv[])
{
    struct sig_window_options options = {0};
    struct bu_arg_schema schema = BU_ARG_SCHEMA(
	name, description, sig_window_options, sig_window_operands,
	BU_ARG_PARSE_OPTIONS_FIRST,
	BU_ARG_SCHEMA_META_HELP(NULL, NULL, NULL, NULL, NULL));
    struct bu_arg_grammar grammar = {&schema, NULL, NULL};
    struct bu_vls msg = BU_VLS_INIT_ZERO;

    int introspection = bu_arg_introspect(&msg, &grammar, argc - 1,
	(const char * const *)(argv + 1));
    if (introspection != 0) {
	bu_flog(introspection > 0 ? stdout : stderr, "%s", bu_vls_cstr(&msg));
	bu_vls_free(&msg);
	return introspection > 0 ? 0 : -1;
    }

    argc--; argv++;
    int operand_index = bu_arg_schema_parse_complete(&schema, &options, &msg,
	argc, (const char **)argv);
    if (operand_index < 0) {
	bu_flog(stderr, "%s", bu_vls_cstr(&msg));
	bu_vls_free(&msg);
	return -1;
    }
    bu_vls_free(&msg);
    if (options.help) {
	char *help = bu_arg_schema_help(&schema, name);
	bu_flog(stdout, "%s", help ? help : "");
	bu_free(help, "signal window help");
	return 0;
    }

    argc -= operand_index;
    argv += operand_index;
    *window_size = SIG_WINDOW_SIZE_DEFAULT;
    if (argc && !bu_arg_integer_range_from_str(&msg, window_size, argv[0], 10,
	1, maximum > 0 ? maximum : INT_MAX)) {
	bu_flog(stderr, "%s: window-size: %s", name, bu_vls_cstr(&msg));
	bu_vls_free(&msg);
	return -1;
    }
    bu_vls_free(&msg);
    return 1;
}

#endif /* SIG_WINDOW_ARGS_H */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
