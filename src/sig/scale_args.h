/*                     S C A L E _ A R G S . H
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

#ifndef SIG_SCALE_ARGS_H
#define SIG_SCALE_ARGS_H

#include "bu/arg.h"
#include "bu/malloc.h"
#include "vmath.h"

struct sig_scale_args {
    int normalize;
    int has_scale;
    double scale;
};

struct sig_scale_options {
    int help;
    int normalize;
};

static const struct bu_arg_option sig_scale_options[] = {
    BU_ARG_FLAG("h", "help", struct sig_scale_options, help,
	"Print help and exit"),
    BU_ARG_ALIAS("?", NULL, "help", 1),
    BU_ARG_FLAG("n", "normalize", struct sig_scale_options, normalize,
	"Use the standard normalization scale"),
    BU_ARG_OPTION_NULL
};

static const struct bu_arg_operand sig_scale_operands[] = {
    BU_ARG_OPERAND("scale", BU_ARG_VALUE_NUMBER, 0, 1,
	"Numeric scale factor", NULL),
    BU_ARG_OPERAND_NULL
};

static int
sig_scale_args_parse(struct sig_scale_args *result, const char *name,
	const char *description, int argc, char *argv[])
{
    struct sig_scale_options options = {0};
    struct bu_arg_schema schema = BU_ARG_SCHEMA(
	name, description, sig_scale_options, sig_scale_operands,
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
	bu_free(help, "signal conversion help");
	return 0;
    }

    argc -= operand_index;
    argv += operand_index;
    if (options.normalize && argc) {
	bu_flog(stderr, "--normalize and a scale operand are mutually exclusive\n");
	return -1;
    }

    result->normalize = options.normalize;
    result->has_scale = argc == 1;
    if (result->has_scale &&
	!bu_arg_double_from_str(&msg, &result->scale, argv[0])) {
	bu_flog(stderr, "%s: scale: %s", name, bu_vls_cstr(&msg));
	bu_vls_free(&msg);
	return -1;
    }
    bu_vls_free(&msg);
    if (result->has_scale && ZERO(result->scale)) {
	bu_flog(stderr, "scale must not be zero\n");
	return -1;
    }
    return 1;
}

#endif /* SIG_SCALE_ARGS_H */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
