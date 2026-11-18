/*                         P L Y - G . C
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
 *
 */
/** @file conv/ply/ply-g.c
 *
 * Program to convert the PLY format to the BRL-CAD format using GCV execution
 *
 */

#include "common.h"

#include <stdlib.h>

#include "bu/app.h"
#include "bu/arg.h"
#include "bu/log.h"
#include "bu/malloc.h"
#include "bu/vls.h"
#include "gcv/api.h"

struct ply_g_args {
    int help;
    int debug;
    int verbose;
    double scale_factor;
};

static const struct bu_arg_option ply_g_options[] = {
    BU_ARG_FLAG("h", "help", struct ply_g_args, help,
	"Print help and exit"),
    BU_ARG_ALIAS("?", NULL, "help", 1),
    BU_ARG_VALUE("s", "scale-factor", struct ply_g_args, scale_factor,
	BU_ARG_VALUE_NUMBER, BU_ARG_REQUIRED, "factor",
	"Scale input coordinates to millimeters (default 1000)"),
    BU_ARG_FLAG("d", "debug", struct ply_g_args, debug,
	"Enable GCV debugging output"),
    BU_ARG_FLAG("v", "verbose", struct ply_g_args, verbose,
	"Report conversion progress"),
    BU_ARG_OPTION_NULL
};

static const struct bu_arg_operand ply_g_operands[] = {
    BU_ARG_OPERAND("input", BU_ARG_VALUE_FILE, 1, 1,
	"Input Stanford PLY file", NULL),
    BU_ARG_OPERAND("output", BU_ARG_VALUE_FILE, 1, 1,
	"Output BRL-CAD database", NULL),
    BU_ARG_OPERAND_NULL
};

static const struct bu_arg_schema ply_g_schema = BU_ARG_SCHEMA(
    "ply-g", "Convert a Stanford PLY mesh to BRL-CAD",
    ply_g_options, ply_g_operands, BU_ARG_PARSE_OPTIONS_FIRST,
    BU_ARG_SCHEMA_META_HELP(NULL, NULL, NULL, NULL, NULL));

static const struct bu_arg_grammar ply_g_grammar = {
    &ply_g_schema, NULL, NULL
};

int
main(int argc, char *argv[])
{
    struct ply_g_args args = {0, 0, 0, 1000.0};
    struct bu_vls msg = BU_VLS_INIT_ZERO;
    struct bu_vls scale = BU_VLS_INIT_ZERO;
    const struct gcv_filter *in_filter;
    const struct gcv_filter *out_filter;
    struct gcv_context context;
    struct gcv_opts gcv_options;
    const char *output_path = NULL;
    const char *input_path;
    int introspection;
    int operand_index;

    bu_setprogname(argv[0]);
    gcv_opts_default(&gcv_options);

    introspection = bu_arg_introspect(&msg, &ply_g_grammar, argc - 1,
	(const char * const *)(argv + 1));
    if (introspection != 0) {
	bu_flog(introspection > 0 ? stdout : stderr, "%s", bu_vls_cstr(&msg));
	bu_vls_free(&msg);
	return introspection > 0 ? 0 : 1;
    }
    operand_index = bu_arg_schema_parse_complete(&ply_g_schema, &args,
	&msg, argc - 1, (const char **)(argv + 1));
    if (operand_index < 0) {
	bu_log("%s\n", bu_vls_cstr(&msg));
	bu_vls_free(&msg);
	return 1;
    }
    bu_vls_free(&msg);
    if (args.help) {
	char *help = bu_arg_schema_help(&ply_g_schema, "ply-g");
	bu_flog(stdout, "%s", help ? help : "");
	bu_free(help, "ply-g help");
	return 0;
    }
    input_path = argv[operand_index + 1];
    output_path = argv[operand_index + 2];
    gcv_options.bu_debug_flag = args.debug;
    gcv_options.verbosity_level = args.verbose;
    bu_vls_printf(&scale, "%.17g", args.scale_factor);

    /* since ply files are typically in meters, we want to pass in the scale factor
     * to gcv_exec with argc and argv as "-s scale_factor"
     */
    const char *unique_options_av[2];
    unique_options_av[0] = "-s";
    unique_options_av[1] = bu_vls_cstr(&scale);

    /* setup to call gcv */
    gcv_context_init(&context);

    /* ensure filters exist */
    in_filter = find_filter(GCV_FILTER_READ, BU_MIME_MODEL_PLY, input_path, &context);
    out_filter = find_filter(GCV_FILTER_WRITE, BU_MIME_MODEL_VND_BRLCAD_PLUS_BINARY, output_path, &context);

    if (!out_filter || !in_filter) {
	gcv_context_destroy(&context);
	bu_vls_free(&scale);
	bu_log("ERROR: could not find the conversion filters\n");
	return 1;
    }

    /* do the conversion */
    if (!gcv_execute(&context, in_filter, &gcv_options, 2, unique_options_av, input_path)) {
	gcv_context_destroy(&context);
	bu_exit(1, "failed to load input file");
    }

    gcv_options.debug_mode = 0;

    if (!gcv_execute(&context, out_filter, &gcv_options, 0, NULL, output_path)) {
	gcv_context_destroy(&context);
	bu_exit(1, "failed to export to output file");
    }

    gcv_context_destroy(&context);
    bu_vls_free(&scale);

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
