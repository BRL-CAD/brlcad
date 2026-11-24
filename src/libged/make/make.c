/*                         M A K E . C
 * BRL-CAD
 *
 * Copyright (c) 2008-2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file libged/make.c
 *
 * The make command.
 *
 */

#include "common.h"

#include "bu/arg.h"
#include "bu/interrupt.h"
#include "rt/func.h"
#include "rt/geom.h"

#include "../ged_private.h"


struct make_args {
    point_t origin;
    fastf_t scale;
    int help;
    int list_types;
};


static const struct bu_arg_option make_options[] = {
    BU_ARG_FLAG("h", "help", struct make_args, help, "Print command usage"),
    BU_ARG_ALIAS("H", NULL, "help", 0),
    BU_ARG_ALIAS("?", NULL, "help", 0),
    BU_ARG_FLAG("t", "types", struct make_args, list_types,
	"List supported primitive types"),
    BU_ARG_ALIAS("T", NULL, "types", 0),
    BU_ARG_VECTOR3("o", "origin", struct make_args, origin, "x/y/z",
	"Primitive origin"),
    BU_ARG_ALIAS("O", NULL, "origin", 0),
    BU_ARG_VALUE("s", "scale", struct make_args, scale, BU_ARG_VALUE_NUMBER,
	BU_ARG_REQUIRED, "scale", "Initial size scale"),
    BU_ARG_ALIAS("S", NULL, "scale", 0),
    BU_ARG_OPTION_NULL
};
static const struct bu_arg_operand make_help_operands[] = {
    BU_ARG_OPERAND("raw_arguments", BU_ARG_VALUE_RAW, 0,
	BU_ARG_COUNT_UNLIMITED, "Arguments ignored when requesting help", NULL),
    BU_ARG_OPERAND_NULL
};
static const struct bu_arg_operand make_operands[] = {
    BU_ARG_OPERAND("name", BU_ARG_VALUE_STRING, 1, 1,
	"Name for the new primitive", NULL),
    BU_ARG_OPERAND("primitive_type", BU_ARG_VALUE_KEYWORD, 1, 1,
	"Primitive type to create", "ged.primitive_type"),
    BU_ARG_OPERAND_NULL
};
static const char * const make_help_options[] = {"help", NULL};
static const char * const make_types_options[] = {"types", NULL};
static const struct bu_arg_schema_case make_cases[] = {
    BU_ARG_SCHEMA_CASE("help", "Display command help",
	BU_ARG_CONDITION_ANY_OPTION_PRESENT, make_help_options,
	make_help_operands, NULL),
    BU_ARG_SCHEMA_CASE("types", "List primitive types without creating an object",
	BU_ARG_CONDITION_ANY_OPTION_PRESENT, make_types_options, NULL, NULL),
    BU_ARG_SCHEMA_CASE_DEFAULT("create", "Create a named primitive",
	make_operands, NULL),
    BU_ARG_SCHEMA_CASE_NULL
};
static const struct bu_arg_schema make_cmd_schema =
    BU_ARG_SCHEMA_CASED("make", "Create a default primitive", make_options,
	BU_ARG_PARSE_INTERSPERSED, make_cases);


static void
print_usage(struct ged *gedp, const char *cmd)
{
    struct bu_vls types = BU_VLS_INIT_ZERO;
    char *help = ged_cmd_help("make", cmd);

    rt_obj_make_labels(&types, "|");
    if (help) {
	bu_vls_strcat(gedp->ged_result_str, help);
	bu_free(help, "make standard help");
    }
    bu_vls_printf(gedp->ged_result_str, "\nAvailable primitive types:\n  %s\n",
	bu_vls_cstr(&types));

    bu_vls_free(&types);
}


int
ged_make_core(struct ged *gedp, int argc, const char *argv[])
{
    struct directory *dp;
    const char *cmd = argv[0];
    const char *name;
    const char *type;
    int operand_index;
    struct make_args args = {{0.0, 0.0, 0.0}, 1.0, 0, 0};
    fastf_t scale;
    point_t origin;

    struct rt_db_internal internal;

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_READ_ONLY(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	print_usage(gedp, cmd);
	return GED_HELP;
    }

    argc--; argv++;
    operand_index = bu_arg_schema_parse_complete(&make_cmd_schema, &args,
	gedp->ged_result_str, argc, argv);
    if (operand_index < 0) {
	print_usage(gedp, cmd);
	return BRLCAD_ERROR;
    }
    argv += operand_index;

    if (args.help) {
	print_usage(gedp, cmd);
	return GED_HELP;
    }

    if (args.list_types) {
	rt_obj_make_labels(gedp->ged_result_str, " ");
	return GED_HELP;
    }

    name = argv[0];
    type = argv[1];
    VMOVE(origin, args.origin);
    scale = args.scale;

    GED_CHECK_EXISTS(gedp, name, LOOKUP_QUIET, BRLCAD_ERROR);
    RT_DB_INTERNAL_INIT(&internal);

    /* TODO: get rid of legacy deprecation messages? These types are alredy on the
     * non-makeable list and will be rejected by rt_obj_make(). Checking for them
     * here is solely for printing and an ugly coupling */
    if (BU_STR_EQUAL(type, "hf")) {
	bu_vls_printf(gedp->ged_result_str, "make: the height field is deprecated and not supported by this command.\nUse the dsp primitive.\n");
	return BRLCAD_ERROR;
    }
    if (BU_STR_EQUAL(type, "pg") || BU_STR_EQUAL(type, "poly")) {
	bu_vls_printf(gedp->ged_result_str, "make: the polysolid is deprecated and not supported by this command.\nUse the bot primitive.");
	return BRLCAD_ERROR;
    }

    if (rt_obj_make(type, origin, scale, &internal) != BRLCAD_OK) {
	bu_vls_printf(gedp->ged_result_str, "make: the %s primitive is not supported by this command", type);
	print_usage(gedp, cmd);
	return BRLCAD_ERROR;
    }

    /* TODO/FIXME: extrude relies on a sketch. Create said sketch as a post-process check.
     * is this how we want to handle this, should extrude's ft_make create it's own sketch,
     * or should an extrude without a sketch be legal?
     */
    if (BU_STR_EQUAL(type, "extrude")) {
	char *av[8];
	char center_str[512];
	char scale_str[128];
	struct rt_extrude_internal* extrude_ip = (struct rt_extrude_internal *)internal.idb_ptr;

	/* sanity */
	if (!extrude_ip)
	    return BRLCAD_ERROR;

	/* attach a sketch name to the extrude */
	av[0] = "make_name";
	av[1] = "skt_";
	ged_exec_make_name(gedp, 2, (const char **)av);
	if (extrude_ip->sketch_name)
	    bu_free(extrude_ip->sketch_name, "empty sketch_name");
	extrude_ip->sketch_name = bu_strdup(bu_vls_addr(gedp->ged_result_str));

	sprintf(center_str, "%f %f %f", V3ARGS(origin));
	sprintf(scale_str, "%f", scale);
	av[0] = "make";
	av[1] = "-o";
	av[2] = center_str;
	av[3] = "-s";
	av[4] = scale_str;
	av[5] = extrude_ip->sketch_name;
	av[6] = "sketch";
	av[7] = (char *)0;
	/* TODO: should probably validate the sketch was made successfully */
	ged_make_core(gedp, 7, (const char **)av);
    }

    /* no interrupts */
    (void)signal(SIGINT, SIG_IGN);

    GED_DB_DIRADD(gedp, dp, name, RT_DIR_PHONY_ADDR, 0, RT_DIR_SOLID, (void *)&internal.idb_type, BRLCAD_ERROR);
    GED_DB_PUT_INTERN(gedp, dp, &internal, BRLCAD_ERROR);

    return BRLCAD_OK;
}


#include "../include/plugin.h"

#define GED_MAKE_COMMANDS(X, XID) \
    X(make, ged_make_core, GED_CMD_DEFAULT, &make_cmd_schema) \

GED_DECLARE_COMMAND_SET_WITH_NATIVE_SCHEMA(GED_MAKE_COMMANDS)
GED_DECLARE_PLUGIN_MANIFEST_WITH_NATIVE_SCHEMA("libged_make", 1, GED_MAKE_COMMANDS)

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
