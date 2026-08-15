/*                        M A K E _ N A M E . C
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
/** @file libged/make_name.c
 *
 * The make_name command.
 *
 */

#include "common.h"
#include <string.h>
#include "bu/cmdschema.h"
#include "ged.h"

struct make_name_args {
    int reset_start;
};

static int make_name_counter = 0;
static const struct bu_cmd_schema *make_name_schema(void);

int
ged_make_name_core(struct ged *gedp, int argc, const char *argv[])
{
    struct bu_vls obj_name = BU_VLS_INIT_ZERO;
    struct make_name_args args = {0};
    char *cp, *tp;
    int option_end;
    int reset_requested;
    int len;

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_READ_ONLY(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	ged_cmd_help_append(gedp->ged_result_str, argv[0], argv[0]);
	return GED_HELP;
    }

    reset_requested = bu_cmd_schema_option_present(make_name_schema(), argc - 1,
	argv + 1, "s");
    option_end = bu_cmd_schema_parse(make_name_schema(), &args,
	gedp->ged_result_str, argc - 1, argv + 1);
    if (option_end < 0) {
	bu_vls_trunc(gedp->ged_result_str, 0);
	ged_cmd_help_append(gedp->ged_result_str, argv[0], argv[0]);
	return BRLCAD_ERROR;
    }

    /* The native parser tells us whether the -s alternative consumed the
     * complete command or left a template operand.  Those alternatives are
     * intentionally exclusive. */
    if (option_end == argc - 1 && reset_requested) {
	make_name_counter = args.reset_start;
	return BRLCAD_OK;
    }
    if (option_end != 0 || argc != 2) {
	ged_cmd_help_append(gedp->ged_result_str, argv[0], argv[0]);
	return BRLCAD_ERROR;
    }

    for (cp = (char *)argv[1], len = 0; *cp != '\0'; ++cp, ++len) {
	if (*cp == '@') {
	    if (*(cp + 1) == '@')
		++cp;
	    else
		break;
	}
	bu_vls_putc(&obj_name, *cp);
    }
    bu_vls_putc(&obj_name, '\0');
    tp = (*cp == '\0') ? "" : cp + 1;

    do {
	bu_vls_trunc(&obj_name, len);
	bu_vls_printf(&obj_name, "%d", make_name_counter++);
	bu_vls_strcat(&obj_name, tp);
    }
    while (db_lookup(gedp->dbip, bu_vls_addr(&obj_name), LOOKUP_QUIET) != RT_DIR_NULL);

    bu_vls_printf(gedp->ged_result_str, "%s", bu_vls_addr(&obj_name));
    bu_vls_free(&obj_name);

    return BRLCAD_OK;
}

#include "../include/plugin.h"

static const struct bu_cmd_option make_name_schema_options[] = {
    BU_CMD_OPTIONAL_INTEGER("s", NULL, struct make_name_args, reset_start, "[num]",
	"Reset the generated-name counter"),
    BU_CMD_OPTION_NULL
};
static const struct bu_cmd_operand make_name_schema_operands[] = {
    BU_CMD_OPERAND("template", BU_CMD_VALUE_STRING, 1, 1,
	"Name template containing @", NULL),
    BU_CMD_OPERAND_NULL
};
static const char * const make_name_reset_option[] = {"s", NULL};
static const struct bu_cmd_schema_case make_name_schema_cases[] = {
    BU_CMD_SCHEMA_CASE("reset", "Reset the generated-name counter",
	BU_CMD_CONDITION_ALL_OPTIONS_PRESENT, make_name_reset_option, NULL, NULL),
    BU_CMD_SCHEMA_CASE_DEFAULT("template", "Generate a name from a template",
	make_name_schema_operands, NULL),
    BU_CMD_SCHEMA_CASE_NULL
};
static const struct bu_cmd_schema make_name_cmd_schema = {
    "make_name", "Generate a unique object name", make_name_schema_options,
	NULL, BU_CMD_PARSE_OPTIONS_FIRST,
	BU_CMD_SCHEMA_META_CASES(NULL, NULL, NULL, NULL, make_name_schema_cases)
};

static const struct bu_cmd_schema *
make_name_schema(void)
{
    return &make_name_cmd_schema;
}

#define GED_MAKE_NAME_COMMANDS(X, XID) \
    X(make_name, ged_make_name_core, GED_CMD_DEFAULT, &make_name_cmd_schema) \

GED_DECLARE_COMMAND_SET_WITH_NATIVE_SCHEMA(GED_MAKE_NAME_COMMANDS)
GED_DECLARE_PLUGIN_MANIFEST_WITH_NATIVE_SCHEMA("libged_make_name", 1, GED_MAKE_NAME_COMMANDS)

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
