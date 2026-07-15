/*                         U N I T S . C
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
/** @file libged/units.c
 *
 * The units command.
 *
 */

#include "common.h"

#include <string.h>

#include "bu/cmd.h"
#include "bu/cmdschema.h"
#include "bu/units.h"

#include "../ged_private.h"

struct units_args {
    int short_name;
    int list_names;
};

static const struct bu_cmd_schema *units_schema(void);


int
ged_units_core(struct ged *gedp, int argc, const char *argv[])
{
    double loc2mm;
    const char *str;
    struct units_args args = {0, 0};
    int option_end = 0;
    int operand_count = 0;
    static const char *usage = "[-s] [mm|cm|m|in|ft|...]";

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);


    option_end = bu_cmd_schema_parse(units_schema(), &args, gedp->ged_result_str,
	argc - 1, argv + 1);
    if (option_end < 0) {
	bu_vls_trunc(gedp->ged_result_str, 0);
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }
    operand_count = argc - 1 - option_end;

	if ((argc > 1 && BU_STR_EQUAL(argv[1], "--")) || operand_count > 1 ||
	    (args.short_name && args.list_names) ||
	((args.short_name || args.list_names) && operand_count)) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    if (args.list_names) {
	struct bu_vls *vlsp = bu_units_strings_vls();

	bu_vls_printf(gedp->ged_result_str, "%s", bu_vls_addr(vlsp));
	bu_vls_free(vlsp);
	bu_free(vlsp, "ged_units_core: vlsp");

	return BRLCAD_OK;
    }

	/* Get units */
    if (operand_count == 0) {
	str = bu_units_string(gedp->dbip->dbi_local2base);
	if (!str) str = "Unknown_unit";

	if (args.short_name)
	    bu_vls_printf(gedp->ged_result_str, "%s", str);
	else
	    bu_vls_printf(gedp->ged_result_str, "You are editing in '%s'.  1 %s = %g mm \n",
			  str, str, gedp->dbip->dbi_local2base);

	return BRLCAD_OK;
    }

    /* Set units */
    /* Allow inputs of the form "25cm" or "3ft" */
    str = argv[1 + option_end];
    if ((loc2mm = bu_mm_value(str)) <= 0) {
	bu_vls_printf(gedp->ged_result_str,
		      "%s: unrecognized unit\nvalid units: <um|mm|cm|m|km|in|ft|yd|mi>\n",
		      str);
	return BRLCAD_ERROR;
    }

    if (db_update_ident(gedp->dbip, gedp->dbip->dbi_title, loc2mm) < 0) {
	bu_vls_printf(gedp->ged_result_str, "Warning: unable to stash working units into database\n");
    }

    gedp->dbip->dbi_local2base = loc2mm;
    gedp->dbip->dbi_base2local = 1.0 / loc2mm;

    str = bu_units_string(gedp->dbip->dbi_local2base);
    if (!str) str = "Unknown_unit";
    bu_vls_printf(gedp->ged_result_str, "You are now editing in '%s'.  1 %s = %g mm \n",
		  str, str, gedp->dbip->dbi_local2base);

    return BRLCAD_OK;
}

#include "../include/plugin.h"

static int
units_value_validate(struct bu_vls *msg, const char *arg)
{
    if (arg && bu_mm_value(arg) > 0)
	return 0;
    if (msg)
	bu_vls_printf(msg, "unrecognized unit: %s\n", arg ? arg : "");
    return -1;
}

static const char *units_um_aliases[] = {"micrometer", "micron", NULL};
static const char *units_mm_aliases[] = {"millimeter", "millimeters", NULL};
static const char *units_cm_aliases[] = {"centimeter", "centimeters", NULL};
static const char *units_m_aliases[] = {"meter", "meters", NULL};
static const char *units_km_aliases[] = {"kilometer", NULL};
static const char *units_in_aliases[] = {"inch", "inches", NULL};
static const char *units_ft_aliases[] = {"foot", "feet", NULL};
static const char *units_yd_aliases[] = {"yard", NULL};
static const char *units_mi_aliases[] = {"mile", NULL};

static const struct bu_cmd_value_keyword units_keyword_values[] = {
    {"um", units_um_aliases, "Micrometre"},
    {"mm", units_mm_aliases, "Millimetre"},
    {"cm", units_cm_aliases, "Centimetre"},
    {"m", units_m_aliases, "Metre"},
    {"km", units_km_aliases, "Kilometre"},
    {"in", units_in_aliases, "Inch"},
    {"ft", units_ft_aliases, "Foot"},
    {"yd", units_yd_aliases, "Yard"},
    {"mi", units_mi_aliases, "Mile"},
    {NULL, NULL, NULL}
};

static const struct bu_cmd_option units_options[] = {
    BU_CMD_FLAG("s", NULL, struct units_args, short_name, "Print only the current unit name"),
    BU_CMD_FLAG("t", NULL, struct units_args, list_names, "List valid unit names"),
    BU_CMD_OPTION_NULL
};

static const struct bu_cmd_operand units_operands[] = {
    {"unit", 0, 1, "Unit name or unit expression", BU_CMD_VALUE_STRING,
	units_value_validate, "ged.unit", NULL, units_keyword_values, NULL},
    BU_CMD_OPERAND_NULL
};

static int
units_schema_validate(const struct bu_cmd_schema *cmd, size_t argc, const char **argv,
	size_t cursor_arg, struct bu_cmd_validate_result *result)
{
    struct bu_cmd_schema flat = *cmd;
    int short_name = 0;
    int list_names = 0;
    size_t operands = 0;

    flat.validation.custom_validate = NULL;
    if (bu_cmd_schema_validate(&flat, argc, argv, cursor_arg, result) ||
	result->state == BU_CMD_VALIDATE_INVALID)
	return 0;

    for (size_t i = 0; i < argc; i++) {
	if (BU_STR_EQUAL(argv[i], "--")) {
	    operands = 2;
	    break;
	} else if (BU_STR_EQUAL(argv[i], "-s")) {
	    short_name++;
	} else if (BU_STR_EQUAL(argv[i], "-t")) {
	    list_names++;
	} else {
	    operands++;
	}
    }
    if (short_name > 1 || list_names > 1 || (short_name && list_names) ||
	operands > 1 || ((short_name || list_names) && operands)) {
	bu_cmd_validate_result_clear(result);
	result->state = BU_CMD_VALIDATE_INVALID;
	result->token_start = cursor_arg < argc ? cursor_arg : argc - 1;
	result->token_end = result->token_start;
	result->expected = BU_CMD_EXPECT_NONE;
	result->completion_type = BU_CMD_VALUE_STRING;
	result->hint = "use one of -s, -t, or a unit expression";
	return 0;
    }
    if (short_name || list_names || operands) {
	result->expected = BU_CMD_EXPECT_NONE;
	result->hint = short_name ? "current unit name" :
	    (list_names ? "unit name list" : "unit expression");
    }
    return 0;
}

static const struct bu_cmd_schema units_cmd_schema = {
    "units", "Get or set database units", units_options, units_operands,
    BU_CMD_PARSE_OPTIONS_FIRST, {units_schema_validate}
};

static const struct bu_cmd_schema *
units_schema(void)
{
    return &units_cmd_schema;
}

#define GED_UNITS_COMMANDS(X, XID) \
    X(units, ged_units_core, GED_CMD_DEFAULT, &units_cmd_schema) \

GED_DECLARE_COMMAND_SET_WITH_NATIVE_SCHEMA(GED_UNITS_COMMANDS)
GED_DECLARE_PLUGIN_MANIFEST_WITH_NATIVE_SCHEMA("libged_units", 1, GED_UNITS_COMMANDS)

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
