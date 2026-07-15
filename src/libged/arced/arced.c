/*                         A R C E D . C
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
/** @file libged/arced.c
 *
 * The arced command.
 *
 */

#include "common.h"

#include <string.h>

#include "bu/cmdschema.h"

#include "ged.h"


static int
arced_path_validate(struct bu_vls *msg, const char *arg)
{
    if (arg && strchr(arg, '/'))
	return 0;

    if (msg)
	bu_vls_printf(msg, "arc path must contain a parent/member separator");
    return -1;
}


static const struct bu_cmd_operand arced_schema_operands[] = {
    BU_CMD_OPERAND_VALIDATE("arc_path", BU_CMD_VALUE_DB_PATH, 1, 1,
	arced_path_validate, "Arc path to animate", "ged.db_path"),
    BU_CMD_OPERAND("animation_command", BU_CMD_VALUE_RAW, 1,
	BU_CMD_COUNT_UNLIMITED, "Animation command and arguments", NULL),
    BU_CMD_OPERAND_NULL
};
static const struct bu_cmd_schema arced_cmd_schema = {
    "arced", "Apply an animation command to an arc", NULL,
    arced_schema_operands, BU_CMD_PARSE_STOP_AT_FIRST_OPERAND, {NULL}
};


int
ged_arced_core(struct ged *gedp, int argc, const char *argv[])
{
    struct animate *anp;
    struct directory *dp;
    mat_t stack;
    struct rt_db_internal intern;
    struct rt_comb_internal *comb;
    union tree *tp;
    static const char *usage = "a/b anim_cmd ...";
    int parse_dummy = 0;

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

    if (bu_cmd_schema_parse_complete(&arced_cmd_schema, &parse_dummy,
	gedp->ged_result_str, argc - 1, argv + 1) < 0) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    anp = db_parse_1anim(gedp->dbip, argc, (const char **)argv);
    if (!anp) {
	bu_vls_printf(gedp->ged_result_str, "arced: unable to parse command");
	return BRLCAD_ERROR;
    }
    if (anp->an_path.fp_len < 2) {
	db_free_1anim(anp);
	bu_vls_printf(gedp->ged_result_str, "arced: path spec has insufficient elements\n");
	return BRLCAD_ERROR;
    }

    /* Only the matrix rarc, lmul, and rmul directives are useful here */
    MAT_IDN(stack);

    /* Load the combination into memory */
    dp = anp->an_path.fp_names[anp->an_path.fp_len-2];
    RT_CK_DIR(dp);
    if ((dp->d_flags & RT_DIR_COMB) == 0) {
	db_free_1anim(anp);
	bu_vls_printf(gedp->ged_result_str, "%s: not a combination", dp->d_namep);
	return BRLCAD_ERROR;
    }
    if (rt_db_get_internal(&intern, dp, gedp->dbip, (fastf_t *)NULL) < 0) {
	db_free_1anim(anp);
	bu_vls_printf(gedp->ged_result_str, "Database read error, aborting");
	return BRLCAD_ERROR;
    }
    comb = (struct rt_comb_internal *)intern.idb_ptr;
    RT_CK_COMB(comb);
    if (!comb->tree) {
	bu_vls_printf(gedp->ged_result_str, "%s: empty combination", dp->d_namep);
	goto fail;
    }

    /* Search for first mention of arc */
    if ((tp = db_find_named_leaf(comb->tree, anp->an_path.fp_names[anp->an_path.fp_len-1]->d_namep)) == TREE_NULL) {
	bu_vls_printf(gedp->ged_result_str, "Unable to find instance of '%s' in combination '%s', error",
		      anp->an_path.fp_names[anp->an_path.fp_len-1]->d_namep,
		      anp->an_path.fp_names[anp->an_path.fp_len-2]->d_namep);
	goto fail;
    }

    /* Found match.  Update tl_mat in place. */
    if (!tp->tr_l.tl_mat)
	tp->tr_l.tl_mat = bn_mat_dup(bn_mat_identity);

    if (db_do_anim(anp, stack, tp->tr_l.tl_mat, NULL) < 0) {
	goto fail;
    }

    if (bn_mat_is_identity(tp->tr_l.tl_mat)) {
	bu_free((void *)tp->tr_l.tl_mat, "tl_mat");
	tp->tr_l.tl_mat = (matp_t)NULL;
    }

    if (rt_db_put_internal(dp, gedp->dbip, &intern) < 0) {
	bu_vls_printf(gedp->ged_result_str, "Database write error, aborting");
	goto fail;
    }
    db_free_1anim(anp);
    return BRLCAD_OK;

fail:
    rt_db_free_internal(&intern);
    db_free_1anim(anp);
    return BRLCAD_ERROR;
}

#include "../include/plugin.h"

#define GED_ARCED_COMMANDS(X, XID) \
    X(arced, ged_arced_core, GED_CMD_DEFAULT, &arced_cmd_schema) \

GED_DECLARE_COMMAND_SET_WITH_NATIVE_SCHEMA(GED_ARCED_COMMANDS)
GED_DECLARE_PLUGIN_MANIFEST_WITH_NATIVE_SCHEMA("libged_arced", 1, GED_ARCED_COMMANDS)

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
