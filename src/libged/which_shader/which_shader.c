/*                         W H I C H _ S H A D E R . C
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
/** @file libged/which_shader.c
 *
 * The which_shader commands.
 *
 */

#include "common.h"

#include <string.h>

#include "bu/opt.h"

#include "../ged_private.h"


struct which_shader_args {
    int script_output;
};

#define WHICH_SHADER_OPTIONS(args) \
    BU_OPT_FLAG(args, "s", NULL, script_output, \
	"Use script-oriented output"),

BU_OPT_DESC_BUILDER(which_shader_options, struct which_shader_args,
    WHICH_SHADER_OPTIONS);
static const ged_opt_spec which_shader_opt_spec =
    GED_OPT("which_shader",
	"Find combinations using matching shaders", which_shader_options,
	"options-first shader_patterns:string+");


int
ged_which_shader_core(struct ged *gedp, int argc, const char *argv[])
{
    int j;
    struct directory *dp;
    struct rt_db_internal intern;
    struct rt_comb_internal *comb;
    int operand_count;
    struct which_shader_args args = {0};
    const char *command = argv[0];

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    if (argc == 1) {
	ged_cmd_help_append(gedp->ged_result_str, command, command);
	return GED_HELP;
    }


    argc--; argv++;
    operand_count = bu_opt_parse_build_with_policy(gedp->ged_result_str,
	argc, argv, which_shader_options, &args, BU_OPT_PARSE_OPTIONS_FIRST);
    if (operand_count < 1) {
	ged_cmd_help_append(gedp->ged_result_str, command, command);
	return BRLCAD_ERROR;
    }
    argc = operand_count;

    for (j = 0; j < argc; j++) {

	if (!args.script_output)
	    bu_vls_printf(gedp->ged_result_str, "Combination[s] with shader %s:\n", argv[j]);

	/* Examine all COMB nodes */
	FOR_ALL_DIRECTORY_START(dp, gedp->dbip) {
	    if (!(dp->d_flags & RT_DIR_COMB))
		continue;

	    if (rt_db_get_internal(&intern, dp, gedp->dbip, (fastf_t *)NULL) < 0) {
		bu_vls_printf(gedp->ged_result_str, "Database read error, aborting.\n");
		return BRLCAD_ERROR;
	    }
	    comb = (struct rt_comb_internal *)intern.idb_ptr;

	    if (!strstr(bu_vls_addr(&comb->shader), argv[j]))
		continue;

	    if (args.script_output)
		bu_vls_printf(gedp->ged_result_str, " %s", dp->d_namep);
	    else
		bu_vls_printf(gedp->ged_result_str, "   %s\n", dp->d_namep);
	    intern.idb_meth->ft_ifree(&intern);
	} FOR_ALL_DIRECTORY_END;
    }

    return BRLCAD_OK;
}


#include "../include/plugin.h"

#define GED_WHICH_SHADER_COMMANDS(X, XID) \
    X(which_shader, ged_which_shader_core, GED_CMD_DEFAULT, &which_shader_opt_spec) \

GED_DECLARE_COMMAND_SET_WITH_OPT_SPEC(GED_WHICH_SHADER_COMMANDS)
GED_DECLARE_PLUGIN_MANIFEST_WITH_OPT_SPEC("libged_which_shader", 1, GED_WHICH_SHADER_COMMANDS)

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
