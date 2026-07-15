/*                       E D M A T E R . C
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
/** @file libged/edmater.c
 *
 * The edmater command.
 *
 * Relies on: rmater, editit, wmater
 *
 */

#include "common.h"

#ifdef HAVE_UNISTD_H
#   include <unistd.h>
#endif

#include "bu/app.h"
#include "bu/cmdschema.h"
#include "bu/file.h"
#include "../ged_private.h"

struct edmater_args {
    const char *editor;
};

static const struct bu_cmd_option edmater_schema_options[] = {
    BU_CMD_STRING("E", NULL, struct edmater_args, editor, "editor", "Editor command"),
    BU_CMD_OPTION_NULL
};
static const struct bu_cmd_operand edmater_schema_operands[] = {
    BU_CMD_OPERAND("combinations", BU_CMD_VALUE_DB_OBJECT, 1,
	BU_CMD_COUNT_UNLIMITED, "Combinations to edit", "ged.db_object"),
    BU_CMD_OPERAND_NULL
};
static const struct bu_cmd_schema edmater_cmd_schema = {
    "edmater", "Edit combination material properties", edmater_schema_options,
    edmater_schema_operands, BU_CMD_PARSE_OPTIONS_FIRST, {NULL}
};


int
ged_edmater_core(struct ged *gedp, int argc, const char *argv[])
{
    FILE *fp;
    int i;
    int status;
    const char **av;
    static const char *usage = "[-E editor] comb(s)";
    char tmpfil[MAXPATHLEN];
    struct edmater_args args = {0};
    int operand_index;

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_READ_ONLY(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    operand_index = bu_cmd_schema_parse_complete(&edmater_cmd_schema, &args,
	gedp->ged_result_str, argc - 1, argv + 1);
    if (operand_index < 0) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }
    argc -= operand_index + 1;
    argv += operand_index + 1;

    fp = bu_temp_file(tmpfil, MAXPATHLEN);
    if (!fp)
	return BRLCAD_ERROR;

    av = (const char **)bu_malloc(sizeof(char *)*(argc + 2), "f_edmater: av");
    av[0] = "wmater";
    av[1] = tmpfil;
    for (i = 2; i < argc + 1; ++i)
	av[i] = argv[i-1];

    av[i] = NULL;

    (void)fclose(fp);

    if (ged_exec_wmater(gedp, argc, av) & BRLCAD_ERROR) {
	bu_file_delete(tmpfil);
	bu_free((void *)av, "f_edmater: av");
	return BRLCAD_ERROR;
    }

    if (_ged_editit(gedp, args.editor, tmpfil)) {
	av[0] = "rmater";
	av[2] = NULL;
	status = ged_exec_rmater(gedp, 2, av);
    } else {
	status = BRLCAD_ERROR;
    }

    bu_file_delete(tmpfil);
    bu_free((void *)av, "ged_edmater_core: av");

    return status;
}

#include "../include/plugin.h"

#define GED_EDMATER_COMMANDS(X, XID) \
    X(edmater, ged_edmater_core, GED_CMD_DEFAULT, &edmater_cmd_schema) \

GED_DECLARE_COMMAND_SET_WITH_NATIVE_SCHEMA(GED_EDMATER_COMMANDS)
GED_DECLARE_PLUGIN_MANIFEST_WITH_NATIVE_SCHEMA("libged_edmater", 1, GED_EDMATER_COMMANDS)

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
