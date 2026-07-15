/*                        S H A D E R . C
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
/** @file libged/shader.c
 *
 * The shader command.
 *
 */

#include "ged.h"

#include "bu/cmdschema.h"


static const struct bu_cmd_operand shader_schema_operands[] = {
    BU_CMD_OPERAND("combination", BU_CMD_VALUE_DB_OBJECT, 1, 1,
	"Combination whose shader is queried or set", "ged.db_object"),
    BU_CMD_OPERAND("shader", BU_CMD_VALUE_STRING, 0, BU_CMD_COUNT_UNLIMITED,
	"Shader material and arguments", NULL),
    BU_CMD_OPERAND_NULL
};
static const struct bu_cmd_schema shader_cmd_schema = {
    "shader", "Query or set a combination shader", NULL,
    shader_schema_operands, BU_CMD_PARSE_STOP_AT_FIRST_OPERAND, {NULL}
};


int
ged_shader_core(struct ged *gedp, int argc, const char *argv[])
{
    struct directory *dp;
    struct rt_db_internal intern;
    struct rt_comb_internal *comb;
    static const char *usage = "combination shader_material [shader_argument(s)]";
    int operand_index;
    int parse_dummy = 0;

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }


    operand_index = bu_cmd_schema_parse_complete(&shader_cmd_schema,
	&parse_dummy, gedp->ged_result_str, argc - 1, argv + 1);
    if (operand_index < 0) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }
    argc -= operand_index + 1;
    argv += operand_index + 1;

    GED_DB_LOOKUP(gedp, dp, argv[0], LOOKUP_NOISY, BRLCAD_ERROR);
    GED_CHECK_COMB(gedp, dp, BRLCAD_ERROR);
    GED_DB_GET_INTERN(gedp, &intern, dp, (fastf_t *)NULL, BRLCAD_ERROR);

    comb = (struct rt_comb_internal *)intern.idb_ptr;
    RT_CK_COMB(comb);

    if (argc == 1) {
	/* Return the current shader string */
	bu_vls_printf(gedp->ged_result_str, "%s", bu_vls_addr(&comb->shader));
	rt_db_free_internal(&intern);
    } else {
	if (gedp->dbip->dbi_read_only) {
	    bu_vls_printf(gedp->ged_result_str, "Sorry, this database is READ-ONLY");
	    rt_db_free_internal(&intern);

	    return BRLCAD_ERROR;
	}

	/* Replace with new shader string from command line */
	bu_vls_free(&comb->shader);

	/* Bunch up the rest of the args, space separated */
	bu_vls_from_argv(&comb->shader, argc - 1, (const char **)argv + 1);

	GED_DB_PUT_INTERN(gedp, dp, &intern, BRLCAD_ERROR);
	/* Internal representation has been freed by rt_db_put_internal */
    }

    return BRLCAD_OK;
}


#include "../include/plugin.h"

#define GED_SHADER_COMMANDS(X, XID) \
    X(shader, ged_shader_core, GED_CMD_DEFAULT, &shader_cmd_schema) \

GED_DECLARE_COMMAND_SET_WITH_NATIVE_SCHEMA(GED_SHADER_COMMANDS)
GED_DECLARE_PLUGIN_MANIFEST_WITH_NATIVE_SCHEMA("libged_shader", 1, GED_SHADER_COMMANDS)

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
