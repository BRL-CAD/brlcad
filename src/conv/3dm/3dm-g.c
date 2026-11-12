/*                         3 D M - G . C
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
/** @file 3dm-g.cpp
 *
 * Conversion of Rhino models (.3dm files) into BRL-CAD databases.
 *
 */


#include "common.h"

#include "bu/arg.h"
#include "bu/app.h"
#include "bu/exit.h"
#include "bu/log.h"
#include "bu/malloc.h"
#include "gcv/api.h"

struct rhino_args {
    int randomize_colors;
    int verbosity;
    const char *output_path;
    int print_help;
};

static const struct bu_arg_option rhino_options[] = {
    BU_ARG_FLAG("r", NULL, struct rhino_args, randomize_colors,
	"Randomize object colors"),
    BU_ARG_COUNTING_FLAG("v", "verbosity", struct rhino_args, verbosity,
	BU_ARG_VALUE_INTEGER, "Increase verbosity level"),
    BU_ARG_VALUE("o", "output", struct rhino_args, output_path,
	BU_ARG_VALUE_FILE, BU_ARG_REQUIRED, "file.g", "Set output filename"),
    BU_ARG_FLAG("h", "help", struct rhino_args, print_help, "Print help and exit"),
    BU_ARG_ALIAS("?", NULL, "help", 1),
    BU_ARG_OPTION_NULL
};

static const struct bu_arg_operand rhino_operands[] = {
    BU_ARG_OPERAND("input_file.3dm", BU_ARG_VALUE_FILE, 1, 1,
	"Input Rhino database", NULL),
    BU_ARG_OPERAND("output_file.g", BU_ARG_VALUE_FILE, 0, 1,
	"Output BRL-CAD database", NULL),
    BU_ARG_OPERAND_NULL
};

static const struct bu_arg_schema rhino_schema = BU_ARG_SCHEMA(
    "3dm-g", "Convert Rhino 3DM geometry to a BRL-CAD database", rhino_options,
    rhino_operands, BU_ARG_PARSE_INTERSPERSED,
    BU_ARG_SCHEMA_META_HELP(NULL, NULL, NULL, NULL, NULL));

static void
rhino_help(void)
{
    char *help = bu_arg_schema_help(&rhino_schema, bu_getprogname());
    if (help)
	bu_log("%s", help);
    bu_free(help, "help str");
}


int
main(int argc, char **argv)
{
    const struct gcv_filter *out_filter;
    const struct gcv_filter *in_filter;

    struct gcv_context context;
    struct gcv_opts gcv_options;
    struct rhino_args args = {0};
    const char *input_path;
    struct bu_vls msg = BU_VLS_INIT_ZERO;

    bu_setprogname(argv[0]);
    gcv_opts_default(&gcv_options);

    /* parse standard options */
    argc--; argv++;
    int operand_index = bu_arg_schema_parse_complete(&rhino_schema, &args, &msg,
	argc, (const char **)argv);

    /* requested help or bad usage */
    if (operand_index < 0) {
	bu_log("%s", bu_vls_cstr(&msg));
	bu_vls_free(&msg);
	return 1;
    }
    bu_vls_free(&msg);
    if (args.print_help) {
	rhino_help();
	return 0;
    }

    argc -= operand_index;
    argv += operand_index;
    if ((argc == 1 && !args.output_path) || (argc == 2 && args.output_path)) {
	bu_log("specify the output exactly once, either positionally or with --output\n");
	rhino_help();
	return 1;
    }

    input_path = argv[0];
    if (!args.output_path)
	args.output_path = argv[1];

    gcv_options.randomize_colors = (unsigned)args.randomize_colors;
    gcv_options.verbosity_level = (unsigned)args.verbosity;

    gcv_context_init(&context);

    in_filter = find_filter(GCV_FILTER_READ, BU_MIME_MODEL_VND_RHINO, input_path, &context);
    out_filter = find_filter(GCV_FILTER_WRITE, BU_MIME_MODEL_VND_BRLCAD_PLUS_BINARY, NULL, &context);

    if (!out_filter) {
	gcv_context_destroy(&context);
	bu_bomb("could not find the BRL-CAD writer filter");
    }

    if (!in_filter) {
	bu_log("could not find the Rhino reader filter");
	gcv_context_destroy(&context);
	return 1;
    }


    if (!gcv_execute(&context, in_filter, &gcv_options, 0, NULL, input_path)) {
	gcv_context_destroy(&context);
	bu_exit(1, "failed to load input file");
    }

    gcv_options.debug_mode = 0;

    if (!gcv_execute(&context, out_filter, &gcv_options, 0, NULL, args.output_path)) {
	gcv_context_destroy(&context);
	bu_exit(1, "failed to export to output file");
    }

    gcv_context_destroy(&context);

    return 0;
}


/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
