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

#include "bu/app.h"
#include "bu/cmdschema.h"
#include "bu/exit.h"
#include "bu/log.h"
#include "bu/malloc.h"
#include "gcv/api.h"


struct three_dm_args {
    int randomize_colors;
    long verbosity;
    const char *output_path;
    int help;
};

static const struct bu_cmd_option three_dm_options[] = {
    BU_CMD_FLAG("r", NULL, struct three_dm_args, randomize_colors,
	"Randomize object colors"),
    BU_CMD_COUNTING_LONG_FLAG("v", "verbosity", struct three_dm_args, verbosity,
	"Increase verbosity level"),
    BU_CMD_FILE("o", "output", struct three_dm_args, output_path, "file.g",
	"Set output filename"),
    BU_CMD_FLAG("h", "help", struct three_dm_args, help, "Print help and exit"),
    BU_CMD_ALIAS_SHORT("?", "help", 1),
    BU_CMD_OPTION_NULL
};

static const struct bu_cmd_operand three_dm_operands[] = {
    BU_CMD_OPERAND("input_file", BU_CMD_VALUE_FILE, 1, 1,
	"Input Rhino .3dm file", NULL),
    BU_CMD_OPERAND("output_file", BU_CMD_VALUE_FILE, 0, 1,
	"Output BRL-CAD database", NULL),
    BU_CMD_OPERAND_NULL
};

static const char *three_dm_output_option[] = {"output", NULL};
static const struct bu_cmd_constraint three_dm_constraints[] = {
    BU_CMD_CONSTRAINT_OPERANDS(BU_CMD_CONDITION_ANY_OPTION_PRESENT,
	three_dm_output_option, 1, 1,
	"--output accepts exactly one input file"),
    BU_CMD_CONSTRAINT_NULL
};

static const struct bu_cmd_schema three_dm_schema = {
    "3dm-g", "Convert a Rhino .3dm model to a BRL-CAD database",
    three_dm_options, three_dm_operands, BU_CMD_PARSE_INTERSPERSED,
    BU_CMD_SCHEMA_CONSTRAINTS(NULL, three_dm_constraints)
};


int
main(int argc, char **argv)
{
    const char * const usage =
	"Usage: 3dm-g [-r] [-v] [-o output_file.g] [-h] input_file.3dm [output_file.g]\n";

    const struct gcv_filter *out_filter;
    const struct gcv_filter *in_filter;

    struct gcv_context context;
    struct gcv_opts gcv_options;
    const char *input_path;
    struct three_dm_args args = {0, 0, NULL, 0};

    bu_setprogname(argv[0]);
    gcv_opts_default(&gcv_options);

    argc--; argv++;
    int help_requested = bu_cmd_schema_option_present(&three_dm_schema,
	(size_t)argc, (const char **)argv, "help");
    int operand_index = help_requested ?
	bu_cmd_schema_parse(&three_dm_schema, &args, NULL, argc, (const char **)argv) :
	bu_cmd_schema_parse_complete(&three_dm_schema, &args, NULL, argc,
	    (const char **)argv);

    /* requested help or bad usage */
    if (args.help || operand_index < 0) {
	char *help = bu_cmd_schema_describe(&three_dm_schema);
	bu_log("%s\nOptions:\n%s", usage, help);
	if (help) bu_free(help, "help str");
	return 1;
    }

    input_path = argv[operand_index];
    if (!args.output_path)
	args.output_path = argv[operand_index + 1];
    gcv_options.randomize_colors = args.randomize_colors;
    gcv_options.verbosity_level = args.verbosity;

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
