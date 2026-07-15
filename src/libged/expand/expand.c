/*                         E X P A N D . C
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
/** @file libged/expand.c
 *
 * The expand command.
 *
 */

#include "common.h"

#include <string.h>

#include "bu/cmd.h"
#include "bu/cmdschema.h"
#include "bu/path.h"

#include "../ged_private.h"


static void
expand_scrape_escapes(struct bu_vls *result, const char *str)
{
    char buf[2];
    buf[1] = '\0';

    while (*str) {
	buf[0] = *str;
	if (*str != '\\') {
	    bu_vls_printf(result, "%s", buf);
	} else if (*(str+1) == '\\') {
	    bu_vls_printf(result, "%s", buf);
	    ++str;
	}
	if (*str == '\0')
	    break;
	++str;
    }
}


static const struct bu_cmd_arg_shape expand_pattern_shape = {
    BU_CMD_ARG_SHAPE_RANGE_PATTERN, 1, 1, "Database name glob pattern", NULL
};
static const struct bu_cmd_operand expand_schema_operands[] = {
    BU_CMD_OPERAND_SHAPED("expressions", BU_CMD_VALUE_STRING, 1,
	BU_CMD_COUNT_UNLIMITED, NULL, "Database names or glob expressions",
	"ged.db_path_or_pattern", &expand_pattern_shape),
    BU_CMD_OPERAND_NULL
};
static const struct bu_cmd_schema expand_cmd_schema = {
    "expand", "Expand database object glob expressions", NULL,
    expand_schema_operands, BU_CMD_PARSE_STOP_AT_FIRST_OPERAND, {NULL}
};


int
ged_expand_core(struct ged *gedp, int argc, const char *argv[])
{
    char *pattern;
    struct directory *dp;
    int whicharg;
    int operand_index;
    int parse_dummy = 0;
    int regexp, nummatch, backslashed;
    static const char *usage = "expression";

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    operand_index = bu_cmd_schema_parse_complete(&expand_cmd_schema,
	&parse_dummy, gedp->ged_result_str, argc - 1, argv + 1);
    if (operand_index < 0) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }
    argc -= operand_index + 1;
    argv += operand_index + 1;

    nummatch = 0;
    backslashed = 0;
    for (whicharg = 0; whicharg < argc; whicharg++) {
	/* If * ? or [ are present, this is a regular expression */
	pattern = (char *)argv[whicharg];
	regexp = 0;
	do {
	    if ((*pattern == '*' || *pattern == '?' || *pattern == '[') &&
		!backslashed) {
		regexp = 1;
		break;
	    }
	    if (*pattern == '\\' && !backslashed)
		backslashed = 1;
	    else
		backslashed = 0;
	} while (*pattern++);

	/* If it isn't a regexp, copy directly and continue */
	if (regexp == 0) {
	    if (db_lookup(gedp->dbip, argv[whicharg], LOOKUP_QUIET) != RT_DIR_NULL) {
		if (nummatch > 0)
		    bu_vls_printf(gedp->ged_result_str, " ");
		expand_scrape_escapes(gedp->ged_result_str, argv[whicharg]);
		++nummatch;
	    }
	    continue;
	}

	/* Search for pattern matches.
	 * If any matches are found, we do not have to worry about
	 * '\' escapes since the match coming from dp->d_namep will be
	 * clean. In the case of no matches, just copy the argument
	 * directly.
	 */

	pattern = (char *)argv[whicharg];
	FOR_ALL_DIRECTORY_START(dp, gedp->dbip)
	    if (bu_path_match(pattern, dp->d_namep, 0) != 0)
		continue;
	    /* Successful match */
	    if (nummatch == 0)
		bu_vls_printf(gedp->ged_result_str, "%s", dp->d_namep);
	    else
		bu_vls_printf(gedp->ged_result_str, " %s", dp->d_namep);
	    ++nummatch;
	FOR_ALL_DIRECTORY_END;
    }

    return BRLCAD_OK;
}

#include "../include/plugin.h"

#define GED_EXPAND_COMMANDS(X, XID) \
    X(expand, ged_expand_core, GED_CMD_DEFAULT, &expand_cmd_schema) \

GED_DECLARE_COMMAND_SET_WITH_NATIVE_SCHEMA(GED_EXPAND_COMMANDS)
GED_DECLARE_PLUGIN_MANIFEST_WITH_NATIVE_SCHEMA("libged_expand", 1, GED_EXPAND_COMMANDS)

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
