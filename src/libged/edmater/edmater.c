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
#include "bu/opt.h"
#include "bu/file.h"
#include "../ged_private.h"

struct edmater_args {
    const char *editor;
};

#define EDMATER_OPTIONS(args) \
    BU_OPT_STR(args, "E", NULL, editor, "editor", "Editor command"),

BU_OPT_DESC_BUILDER(edmater_options, struct edmater_args, EDMATER_OPTIONS);
static const ged_opt_spec edmater_opt_spec =
    GED_OPT("edmater", "Edit combination material properties",
	edmater_options, "options-first combinations:object+");


int
ged_edmater_core(struct ged *gedp, int argc, const char *argv[])
{
    FILE *fp;
    int i;
    int status;
    const char **av;
    char tmpfil[MAXPATHLEN];
    struct edmater_args args = {0};
    int operand_count;
    const char *command = argv[0];

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_READ_ONLY(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	ged_cmd_help_append(gedp->ged_result_str, command, command);
	return GED_HELP;
    }

    argc--; argv++;
    operand_count = bu_opt_parse_build_with_policy(gedp->ged_result_str,
	argc, argv, edmater_options, &args, BU_OPT_PARSE_OPTIONS_FIRST);
    if (operand_count < 1) {
	ged_cmd_help_append(gedp->ged_result_str, command, command);
	return BRLCAD_ERROR;
    }
    argc = operand_count;

    fp = bu_temp_file(tmpfil, MAXPATHLEN);
    if (!fp)
	return BRLCAD_ERROR;

    av = (const char **)bu_malloc(sizeof(char *) * ((size_t)argc + 3),
	"f_edmater: av");
    av[0] = "wmater";
    av[1] = tmpfil;
    for (i = 0; i < argc; ++i)
	av[i + 2] = argv[i];

    av[argc + 2] = NULL;

    (void)fclose(fp);

    if (ged_exec_wmater(gedp, argc + 2, av) & BRLCAD_ERROR) {
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
    X(edmater, ged_edmater_core, GED_CMD_DEFAULT, &edmater_opt_spec) \

GED_DECLARE_COMMAND_SET_WITH_OPT_SPEC(GED_EDMATER_COMMANDS)
GED_DECLARE_PLUGIN_MANIFEST_WITH_OPT_SPEC("libged_edmater", 1, GED_EDMATER_COMMANDS)

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
