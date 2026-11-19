/*                       B O T _ D U M P . C
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
/** @file conv/bot_dump.c
 *
 */

#include "common.h"

#include "bu/arg.h"
#include "bu/app.h"
#include "ged.h"


struct bot_dump_args {
    int help;
    int binary;
    int normals;
    const char *output_directory;
    const char *output_file;
    const char *format;
    const char *units;
};

static const struct bu_arg_value_keyword bot_dump_formats[] = {
    {"dxf", NULL, "Drawing Interchange Format"},
    {"glb", NULL, "Binary glTF"},
    {"gltf", NULL, "glTF"},
    {"obj", NULL, "Wavefront OBJ"},
    {"sat", NULL, "ACIS SAT"},
    {"stl", NULL, "STL"},
    {NULL, NULL, NULL}
};

static const struct bu_arg_option bot_dump_options[] = {
    BU_ARG_FLAG("h", "help", struct bot_dump_args, help,
	"Print help and exit"),
    BU_ARG_ALIAS("?", NULL, "help", 1),
    BU_ARG_FLAG("b", "binary", struct bot_dump_args, binary,
	"Write binary output when supported; glTF binary output uses GLB"),
    BU_ARG_FLAG("n", "normals", struct bot_dump_args, normals,
	"Write normals when supported (OBJ; intrinsic in STL)"),
    BU_ARG_VALUE("m", "output-directory", struct bot_dump_args,
	output_directory, BU_ARG_VALUE_STRING, BU_ARG_REQUIRED, "directory",
	"Write one file per BoT into this directory"),
    BU_ARG_VALUE("o", "output-file", struct bot_dump_args, output_file,
	BU_ARG_VALUE_FILE, BU_ARG_REQUIRED, "file",
	"Write all exported geometry to this file"),
    BU_ARG_KEYWORD_VALUES("t", "format", struct bot_dump_args, format,
	"format", "Select the mesh format; otherwise infer it from the output "
	"name or default to STL", bot_dump_formats),
    BU_ARG_VALUE_VALIDATE("u", "units", struct bot_dump_args, units,
	BU_ARG_VALUE_STRING, bu_arg_units_validate, "units",
	"Convert internal millimeters to these output units"),
    BU_ARG_OPTION_NULL
};

static const struct bu_arg_operand bot_dump_operands[] = {
    BU_ARG_OPERAND("database.g", BU_ARG_VALUE_FILE, 1, 1,
	"Input BRL-CAD database", NULL),
    BU_ARG_OPERAND("bot", BU_ARG_VALUE_DB_PATH, 0, BU_ARG_COUNT_UNLIMITED,
	"BoT object or path (defaults to all BoTs)", "ged.db_path"),
    BU_ARG_OPERAND_NULL
};

static const char * const bot_dump_output_options[] = {
    "output-directory", "output-file", NULL
};

static const struct bu_arg_constraint bot_dump_constraints[] = {
    BU_ARG_CONSTRAINT(BU_ARG_CONSTRAINT_OPTION_COUNT, BU_ARG_CONDITION_ALWAYS,
	bot_dump_output_options, 0, 1,
	"--output-directory and --output-file are mutually exclusive"),
    BU_ARG_CONSTRAINT_NULL
};

static const struct bu_arg_schema bot_dump_schema = BU_ARG_SCHEMA(
    "bot_dump", "Export BoT primitives to external mesh formats",
    bot_dump_options, bot_dump_operands, BU_ARG_PARSE_OPTIONS_FIRST,
    BU_ARG_SCHEMA_META_HELP(NULL, bot_dump_constraints, NULL, NULL, NULL));

static const struct bu_arg_grammar bot_dump_grammar = {
    &bot_dump_schema, NULL, NULL
};

static int
bot_dump_help(FILE *stream, int status)
{
    char *help = bu_arg_schema_help(&bot_dump_schema, "bot_dump");
    bu_flog(stream, "%s", help ? help : "");
    bu_free(help, "bot_dump help");
    return status;
}


int
main(int argc, char *argv[])
{
    enum { BOT_DUMP_VALUE_OPTION_COUNT = 4 };
    struct bot_dump_args args = {0};
    struct bu_vls msg = BU_VLS_INIT_ZERO;
    int i, j;
    int db_index;
    int introspection;
    int operand_index;
    int ret;
    const char **av;
    struct ged *gedp;

    bu_setprogname(argv[0]);

    introspection = bu_arg_introspect(&msg, &bot_dump_grammar, argc - 1,
	(const char * const *)(argv + 1));
    if (introspection != 0) {
	bu_flog(introspection > 0 ? stdout : stderr, "%s", bu_vls_cstr(&msg));
	bu_vls_free(&msg);
	return introspection > 0 ? 0 : 1;
    }
    operand_index = bu_arg_schema_parse_complete(&bot_dump_schema, &args,
	&msg, argc - 1, (const char **)(argv + 1));
    if (operand_index < 0) {
	bu_flog(stderr, "%s", bu_vls_cstr(&msg));
	bu_vls_free(&msg);
	return 1;
    }
    bu_vls_free(&msg);
    if (args.help)
	return bot_dump_help(stdout, 0);

    db_index = operand_index + 1;
    if ((gedp = ged_open("db", argv[db_index], 1)) == GED_NULL)
	bu_exit(1, "Unable to open geometry database file (%s)\n",
		argv[db_index]);

    av = (const char **)bu_calloc((size_t)argc +
	BOT_DUMP_VALUE_OPTION_COUNT + 1, sizeof(char *),
	"alloc bot_dump arguments");

    j = 0;
    av[j++] = "bot_dump";
    if (args.binary)
	av[j++] = "-b";
    if (args.normals)
	av[j++] = "-n";
    if (args.output_directory) {
	av[j++] = "-m";
	av[j++] = args.output_directory;
    }
    if (args.output_file) {
	av[j++] = "-o";
	av[j++] = args.output_file;
    }
    if (args.format) {
	av[j++] = "-t";
	av[j++] = args.format;
    }
    if (args.units) {
	av[j++] = "-u";
	av[j++] = args.units;
    }
    for (i = db_index + 1; i < argc; i++)
	av[j++] = argv[i];
    av[j] = NULL;

    ret = ged_exec_bot_dump(gedp, j, av);
    if (bu_vls_strlen(gedp->ged_result_str) > 0)
	bu_log("%s", bu_vls_addr(gedp->ged_result_str));
    ged_close(gedp);
    bu_free((void *)av, "free bot_dump arguments");

    return ret == BRLCAD_OK ? 0 : 1;
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
