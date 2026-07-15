/*                         C O P Y E V A L . C
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
/** @file libged/copyeval.c
 *
 * The copyeval command.
 *
 */

#include "common.h"

#include <string.h>

#include "bu/cmd.h"
#include "bu/cmdschema.h"

#include "../ged_private.h"


static const struct bu_cmd_operand copyeval_schema_operands[] = {
    BU_CMD_OPERAND("input_path", BU_CMD_VALUE_DB_PATH, 1, 1,
	"Path to an existing primitive", "ged.db_path"),
    BU_CMD_OPERAND("output_object", BU_CMD_VALUE_STRING, 1, 1,
	"Name for the transformed primitive copy", NULL),
    BU_CMD_OPERAND_NULL
};
static const struct bu_cmd_schema copyeval_cmd_schema = {
    "copyeval", "Copy a primitive with its path transform applied", NULL,
    copyeval_schema_operands, BU_CMD_PARSE_STOP_AT_FIRST_OPERAND, {NULL}
};


int
ged_copyeval_core(struct ged *gedp, int argc, const char *argv[])
{
    static const char *usage = "path_to_old_prim new_prim";
    struct _ged_trace_data gtd;
    struct directory *dp;
    struct rt_db_internal *ip;
    struct rt_db_internal internal = RT_DB_INTERNAL_INIT_ZERO;
    struct rt_db_internal new_int = RT_DB_INTERNAL_INIT_ZERO;

    int endpos = 0;
    int i;
    int operand_index;
    int parse_dummy = 0;
    const char *command;
    mat_t start_mat;

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_READ_ONLY(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);
    command = argv[0];

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }


    operand_index = bu_cmd_schema_parse_complete(&copyeval_cmd_schema,
	&parse_dummy, gedp->ged_result_str, argc - 1, argv + 1);
    if (operand_index < 0) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }
    argv += operand_index + 1;

    /* initialize gtd */
    memset(&gtd, 0, sizeof(gtd));
    gtd.gtd_gedp = gedp;
    gtd.gtd_flag = _GED_CPEVAL;
    gtd.gtd_prflag = 0;

    /* check if new solid name already exists in description */
    GED_CHECK_EXISTS(gedp, argv[1], LOOKUP_QUIET, BRLCAD_ERROR);

    MAT_IDN(start_mat);

    /* build directory pointer array for desired path */

    if (strchr(argv[0], '/')) {
	char *path_copy = bu_strdup(argv[0]);
	char *tok = strtok(path_copy, "/");
	while (tok) {
	    if (endpos >= _GED_TRACE_MAX_LEVELS) {
		bu_vls_printf(gedp->ged_result_str, "Path exceeds %d levels\n",
		    _GED_TRACE_MAX_LEVELS);
		bu_free(path_copy, "copyeval path copy");
		return BRLCAD_ERROR;
	    }
	    gtd.gtd_obj[endpos] = db_lookup(gedp->dbip, tok, LOOKUP_NOISY);
	    if (gtd.gtd_obj[endpos] == RT_DIR_NULL) {
		bu_free(path_copy, "copyeval path copy");
		return BRLCAD_ERROR;
	    }
	    endpos++;
	    tok = strtok((char *)NULL, "/");
	}
	bu_free(path_copy, "copyeval path copy");
    } else {
	GED_DB_LOOKUP(gedp, gtd.gtd_obj[endpos], argv[0], LOOKUP_NOISY, BRLCAD_ERROR & GED_QUIET);
	endpos++;
    }

    if (endpos < 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", command, usage);
	return BRLCAD_ERROR;
    }

    gtd.gtd_objpos = endpos - 1;

    GED_DB_GET_INTERN(gedp, &internal, gtd.gtd_obj[endpos - 1], bn_mat_identity, BRLCAD_ERROR);

    if (endpos > 1) {
	/* Make sure that final component in path is a solid */
	if (internal.idb_type == ID_COMBINATION) {
	    rt_db_free_internal(&internal);
	    bu_vls_printf(gedp->ged_result_str, "final component on path must be a primitive!\n");
	    return BRLCAD_ERROR;
	}

	/* Accumulate the matrices */
	ged_trace(gtd.gtd_obj[0], 0, start_mat, &gtd, 1);

	if (gtd.gtd_prflag == 0) {
	    bu_vls_printf(gedp->ged_result_str, "PATH:  ");

	    for (i = 0; i < gtd.gtd_objpos; i++)
		bu_vls_printf(gedp->ged_result_str, "/%s", gtd.gtd_obj[i]->d_namep);

	    bu_vls_printf(gedp->ged_result_str, "  NOT FOUND\n");
	    rt_db_free_internal(&internal);
	    return BRLCAD_ERROR;
	}

	/* Have found the desired path - wdb_xform is the transformation matrix */
	/* wdb_xform matrix calculated in wdb_trace() */

	/* create the new solid */
	RT_DB_INTERNAL_INIT(&new_int);
	if (rt_generic_xform(&new_int, gtd.gtd_xform, &internal, 0, gedp->dbip)) {
	    rt_db_free_internal(&internal);
	    bu_vls_printf(gedp->ged_result_str, "ged_copyeval: rt_generic_xform failed\n");
	    return BRLCAD_ERROR;
	}

	ip = &new_int;
    } else
	ip = &internal;

    /* should call GED_DB_DIRADD() but need to deal with freeing the
     * internals on failure.
     */
    dp = db_diradd(gedp->dbip, argv[1], RT_DIR_PHONY_ADDR, 0, gtd.gtd_obj[endpos-1]->d_flags, (void *)&ip->idb_type);
    if (dp == RT_DIR_NULL) {
	rt_db_free_internal(&internal);
	if (ip == &new_int)
	    rt_db_free_internal(&new_int);
	bu_vls_printf(gedp->ged_result_str, "An error has occurred while adding a new object to the database.");
	return BRLCAD_ERROR;
    }

    /* should call GED_DB_DIRADD() but need to deal with freeing the
     * internals on failure.
     */
    if (rt_db_put_internal(dp, gedp->dbip, ip) < 0) {
	/* if (ip == &new_int) then new_int gets freed by the rt_db_put_internal above
	 * regardless of whether it succeeds or not. At this point only internal needs
	 * to be freed. On the other hand if (ip == &internal), the internal gets freed
	 * freed by the rt_db_put_internal above. In this case memory for new_int has
	 * not been allocated.
	 */
	if (ip == &new_int)
	    rt_db_free_internal(&internal);

	bu_vls_printf(gedp->ged_result_str, "Database write error, aborting");
	return BRLCAD_ERROR;
    }

    /* see previous comment */
    if (ip == &new_int)
	rt_db_free_internal(&internal);

    return BRLCAD_OK;
}


#include "../include/plugin.h"

#define GED_COPYEVAL_COMMANDS(X, XID) \
    X(copyeval, ged_copyeval_core, GED_CMD_DEFAULT, &copyeval_cmd_schema) \

GED_DECLARE_COMMAND_SET_WITH_NATIVE_SCHEMA(GED_COPYEVAL_COMMANDS)
GED_DECLARE_PLUGIN_MANIFEST_WITH_NATIVE_SCHEMA("libged_copyeval", 1, GED_COPYEVAL_COMMANDS)

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
