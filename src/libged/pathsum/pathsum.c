/*                         P A T H S U M . C
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
/** @file libged/pathsum.c
 *
 * The paths and listeval command.
 *
 */

#include "common.h"

#include <string.h>

#include "bu/cmd.h"
#include "bu/cmdschema.h"

#include "../ged_private.h"


struct listeval_args {
    int terse;
};

static const struct bu_cmd_option listeval_schema_options[] = {
    BU_CMD_FLAG("t", NULL, struct listeval_args, terse,
	"Use terse evaluated-object output"),
    BU_CMD_OPTION_NULL
};
static const struct bu_cmd_operand pathsum_schema_operands[] = {
    BU_CMD_OPERAND("path", BU_CMD_VALUE_DB_PATH, 1, BU_CMD_COUNT_UNLIMITED,
	"Slash-separated path or space-separated path components", "ged.db_path"),
    BU_CMD_OPERAND_NULL
};
static const struct bu_cmd_schema listeval_cmd_schema = {
    "listeval", "List evaluated objects along a database path",
    listeval_schema_options, pathsum_schema_operands,
    BU_CMD_PARSE_OPTIONS_FIRST, {NULL}
};
static const struct bu_cmd_schema paths_cmd_schema = {
    "paths", "List paths matching a database path prefix", NULL,
    pathsum_schema_operands, BU_CMD_PARSE_STOP_AT_FIRST_OPERAND, {NULL}
};
static const struct bu_cmd_schema pathsum_cmd_schema = {
    "pathsum", "Report the accumulated transform along a database path", NULL,
    pathsum_schema_operands, BU_CMD_PARSE_STOP_AT_FIRST_OPERAND, {NULL}
};


static const struct bu_cmd_schema *
pathsum_schema(const char *command)
{
    if (BU_STR_EQUAL(command, "listeval"))
	return &listeval_cmd_schema;
    if (BU_STR_EQUAL(command, "paths"))
	return &paths_cmd_schema;
    return &pathsum_cmd_schema;
}


static void
pathsum_show_help(struct ged *gedp, const char *command)
{
    if (BU_STR_EQUAL(command, "listeval")) {
	bu_vls_printf(gedp->ged_result_str,
	    "Usage: %s [-t] {path}\n{path} may be specified by '/' or space separated components, but not both",
	    command);
	return;
    }

    bu_vls_printf(gedp->ged_result_str,
	"Usage: %s {path_start}\n{path_start} may be specified by '/' or space separated components, but not both",
	command);
}


int
ged_pathsum_core(struct ged *gedp, int argc, const char *argv[])
{
    int i;
    int verbose;
    int operand_index;
    const char *command;
    char *slash_path = NULL;
    struct listeval_args args = {0};
    struct _ged_trace_data gtd;

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    command = argv[0];

    /*
     * paths are matched up to last input member
     * ANY path the same up to this point is considered as matching
     */

    /* initialize gtd */
    memset((char *)(&gtd), 0, sizeof(struct _ged_trace_data));
    gtd.gtd_gedp = gedp;
    gtd.gtd_flag = _GED_CPEVAL;
    gtd.gtd_prflag = 0;

    /* find out which command was entered */
    if (BU_STR_EQUAL(argv[0], "listeval")) {
	/* want to list evaluated solid[s] */
	gtd.gtd_flag = _GED_LISTEVAL;
    }
    if (BU_STR_EQUAL(argv[0], "paths")) {
	/* want to list all matching paths */
	gtd.gtd_flag = _GED_LISTPATH;
    }

    /* must be wanting help */
    if (argc == 1) {
	pathsum_show_help(gedp, command);
	return GED_HELP;
    }

    operand_index = bu_cmd_schema_parse_complete(pathsum_schema(command), &args,
	gedp->ged_result_str, argc - 1, argv + 1);
    if (operand_index < 0) {
	pathsum_show_help(gedp, command);
	return BRLCAD_ERROR;
    }
    argc -= operand_index + 1;
    argv += operand_index + 1;
    verbose = args.terse ? 0 : 1;

    gtd.gtd_objpos = 0;

    if (argc == 1 && strchr(argv[0], '/')) {
	char *tok;
	slash_path = bu_strdup(argv[0]);
	tok = strtok(slash_path, "/");
	while (tok) {
	    if (gtd.gtd_objpos >= _GED_TRACE_MAX_LEVELS) {
		bu_vls_printf(gedp->ged_result_str, "Path exceeds %d levels\n",
		    _GED_TRACE_MAX_LEVELS);
		bu_free(slash_path, "pathsum slash path");
		return BRLCAD_ERROR;
	    }
	    if ((gtd.gtd_obj[gtd.gtd_objpos++] = db_lookup(gedp->dbip, tok, LOOKUP_NOISY)) == RT_DIR_NULL) {
		bu_free(slash_path, "pathsum slash path");
		return BRLCAD_ERROR;
	    }
	    tok = strtok((char *)NULL, "/");
	}
	bu_free(slash_path, "pathsum slash path");
    } else {
	if (argc > _GED_TRACE_MAX_LEVELS) {
	    bu_vls_printf(gedp->ged_result_str, "Path exceeds %d levels\n",
		_GED_TRACE_MAX_LEVELS);
	    return BRLCAD_ERROR;
	}
	gtd.gtd_objpos = argc;
	/* build directory pointer array for desired path */
	for (i = 0; i < gtd.gtd_objpos; i++) {
	    if ((gtd.gtd_obj[i] = db_lookup(gedp->dbip, argv[i], LOOKUP_NOISY)) == RT_DIR_NULL) {
		return BRLCAD_ERROR;
	    }
	}
    }

    if (!gtd.gtd_objpos) {
	bu_vls_printf(gedp->ged_result_str, "Invalid path\n");
	return BRLCAD_ERROR;
    }

    MAT_IDN(gtd.gtd_xform);

    ged_trace(gtd.gtd_obj[0], 0, bn_mat_identity, &gtd, verbose);

    if (gtd.gtd_prflag == 0) {
	/* path not found */
	bu_vls_printf(gedp->ged_result_str, "PATH:  ");
	/* NOTE: gtd.gtd_obj size is limited to _BV_MAX_LEVLES - make sure our loop bounds dont exceed */
	for (i = 0; i < FMIN(gtd.gtd_objpos, _GED_TRACE_MAX_LEVELS); i++) {
	    bu_vls_printf(gedp->ged_result_str, "/%s", gtd.gtd_obj[i]->d_namep);
	}
	bu_vls_printf(gedp->ged_result_str, "  NOT FOUND\n");
    }

    return BRLCAD_OK;
}


#include "../include/plugin.h"

#define GED_PATHSUM_COMMANDS(X, XID) \
    X(listeval, ged_pathsum_core, GED_CMD_DEFAULT, &listeval_cmd_schema) \
    X(paths, ged_pathsum_core, GED_CMD_DEFAULT, &paths_cmd_schema) \
    X(pathsum, ged_pathsum_core, GED_CMD_DEFAULT, &pathsum_cmd_schema) \

GED_DECLARE_COMMAND_SET_WITH_NATIVE_SCHEMA(GED_PATHSUM_COMMANDS)
GED_DECLARE_PLUGIN_MANIFEST_WITH_NATIVE_SCHEMA("libged_pathsum", 1, GED_PATHSUM_COMMANDS)

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
