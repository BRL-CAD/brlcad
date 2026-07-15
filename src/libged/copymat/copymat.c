/*                         C O P Y M A T . C
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
/** @file libged/copymat.c
 *
 * The copymat command.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include "bu/cmdschema.h"

#include "../ged_private.h"


static int
copymat_arc_validate(struct bu_vls *msg, const char *arg)
{
    const char *separator;

    if (!arg || !arg[0])
	goto invalid;
    separator = strchr(arg, '/');
    if (!separator || separator == arg || !separator[1] || strchr(separator + 1, '/'))
	goto invalid;
    return 0;

invalid:
    if (msg)
	bu_vls_printf(msg, "arc must have exactly one non-empty parent/member separator");
    return -1;
}


static const struct bu_cmd_operand copymat_schema_operands[] = {
    BU_CMD_OPERAND_VALIDATE("source_arc", BU_CMD_VALUE_DB_PATH, 1, 1,
	copymat_arc_validate, "Source parent/child arc", "ged.db_path"),
    BU_CMD_OPERAND_VALIDATE("destination_arc", BU_CMD_VALUE_DB_PATH, 1, 1,
	copymat_arc_validate, "Destination parent/child arc", "ged.db_path"),
    BU_CMD_OPERAND_NULL
};
static const struct bu_cmd_schema copymat_cmd_schema = {
    "copymat", "Copy a matrix between combination arcs", NULL,
    copymat_schema_operands, BU_CMD_PARSE_STOP_AT_FIRST_OPERAND, {NULL}
};


int
ged_copymat_core(struct ged *gedp, int argc, const char *argv[])
{
    char *child = NULL;
    char *parent = NULL;
    struct bu_vls pvls = BU_VLS_INIT_ZERO;
    int sep;
    int status;
    struct db_tree_state ts;
    struct directory *dp;
    struct rt_comb_internal *comb;
    struct rt_db_internal intern;
    struct animate anp;
    union tree *tp;
    static const char *usage = "a/b c/d";
    const char *command;
    int operand_index;
    int parse_dummy = 0;

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


    operand_index = bu_cmd_schema_parse_complete(&copymat_cmd_schema,
	&parse_dummy, gedp->ged_result_str, argc - 1, argv + 1);
    if (operand_index < 0) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }
    argv += operand_index + 1;

    child = strchr(argv[1], '/') + 1;

    memset(&anp, 0, sizeof(struct animate));
    anp.magic = ANIMATE_MAGIC;

    struct rt_wdb *wdbp = wdb_dbopen(gedp->dbip, RT_WDB_TYPE_DB_DEFAULT);
    ts = wdbp->wdb_initial_tree_state;	/* struct copy */

    ts.ts_dbip = gedp->dbip;
    MAT_IDN(ts.ts_mat);
    db_full_path_init(&anp.an_path);
    if (db_follow_path_for_state(&ts, &(anp.an_path), argv[0], LOOKUP_NOISY) < 0)
    {
	bu_vls_printf(gedp->ged_result_str, "%s: cannot follow path for arc: '%s'\n", command, argv[0]);
	return BRLCAD_ERROR;
    }

    bu_vls_strcat(&pvls, argv[1]);
    parent = bu_vls_addr(&pvls);
    sep = strchr(parent, '/') - parent;
    bu_vls_trunc(&pvls, sep);
    switch (rt_db_lookup_internal(gedp->dbip, parent, &dp, &intern, LOOKUP_NOISY)) {
	case ID_COMBINATION:
	    if (dp->d_flags & RT_DIR_COMB)
		break;
	    else {
		bu_vls_printf(gedp->ged_result_str,
			      "%s: Non-combination directory <%p> '%s' for combination rt_db_internal <%p>\nThis should not happen\n",
			      command, (void *)dp, dp->d_namep, (void *)&intern);
	    }
	    /* fall through */
	default:
	bu_vls_printf(gedp->ged_result_str, "%s: Object '%s' is not a combination\n", command, parent);
	    /* fall through */
	case ID_NULL:
	    bu_vls_free(&pvls);
	    return BRLCAD_ERROR;
    }
    comb = (struct rt_comb_internal *) intern.idb_ptr;
    RT_CK_COMB(comb);

    tp = db_find_named_leaf(comb->tree, child);
    if (tp == TREE_NULL) {
	bu_vls_printf(gedp->ged_result_str, "%s: unable to find instance of '%s' in combination '%s'\n",
		      command, child, dp->d_namep);
	status = BRLCAD_ERROR;
	goto wrapup;
    }

    /*
     * Finally, copy the matrix
     */
    if (!bn_mat_is_identity(ts.ts_mat)) {
	if (tp->tr_l.tl_mat == NULL)
	    tp->tr_l.tl_mat = bn_mat_dup(ts.ts_mat);
	else
	    MAT_COPY(tp->tr_l.tl_mat, ts.ts_mat);
    } else if (tp->tr_l.tl_mat != NULL) {
	bu_free((void *) tp->tr_l.tl_mat, "tl_mat");
	tp->tr_l.tl_mat = (matp_t) 0;
    }

    if (rt_db_put_internal(dp, gedp->dbip, &intern) < 0) {
	bu_vls_printf(gedp->ged_result_str, "%s: Database write error, aborting\n", command);
	status = BRLCAD_ERROR;
	goto wrapup;
    }

    status = BRLCAD_OK;

wrapup:

    bu_vls_free(&pvls);
    if (status & BRLCAD_ERROR)
	rt_db_free_internal(&intern);
    return status;
}


#include "../include/plugin.h"

#define GED_COPYMAT_COMMANDS(X, XID) \
    X(copymat, ged_copymat_core, GED_CMD_DEFAULT, &copymat_cmd_schema) \

GED_DECLARE_COMMAND_SET_WITH_NATIVE_SCHEMA(GED_COPYMAT_COMMANDS)
GED_DECLARE_PLUGIN_MANIFEST_WITH_NATIVE_SCHEMA("libged_copymat", 1, GED_COPYMAT_COMMANDS)

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
