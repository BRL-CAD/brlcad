/*                         A R C E D . C
 * BRL-CAD
 *
 * Copyright (c) 2008-2011 United States Government as represented by
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

#include "ged.h"


int
ged_arced(struct ged *gedp, int argc, const char *argv[])
{
    struct animate *anp;
    struct directory *dp;
    mat_t stack;
    struct rt_db_internal intern;
    struct rt_comb_internal *comb;
    union tree *tp;
    static const char *usage = "a/b anim_cmd ...";

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_READ_ONLY(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    if (argc < 3) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    if (!strchr(argv[1], '/')) {
	bu_vls_printf(gedp->ged_result_str, "arced: bad path specification '%s'", argv[1]);
	return GED_ERROR;
    }
    anp = db_parse_1anim(gedp->ged_wdbp->dbip, argc, (const char **)argv);
    if (!anp) {
	bu_vls_printf(gedp->ged_result_str, "arced: unable to parse command");
	return GED_ERROR;
    }
    if (anp->an_path.fp_len < 2) {
	db_free_1anim(anp);
	bu_vls_printf(gedp->ged_result_str, "arced: path spec has insufficient elements\n");
	return GED_ERROR;
    }

    /* Only the matrix rarc, lmul, and rmul directives are useful here */
    MAT_IDN(stack);

    /* Load the combination into memory */
    dp = anp->an_path.fp_names[anp->an_path.fp_len-2];
    RT_CK_DIR(dp);
    if ((dp->d_flags & RT_DIR_COMB) == 0) {
	db_free_1anim(anp);
	bu_vls_printf(gedp->ged_result_str, "%s: not a combination", dp->d_namep);
	return GED_ERROR;
    }
    /* GED_DB_GET_INTERNAL(gedp, &intern, (fastf_t *)NULL, &rt_uniresource, GED_ERROR); */
    if (rt_db_get_internal(&intern, dp, gedp->ged_wdbp->dbip, (fastf_t *)NULL, &rt_uniresource) < 0) {
	db_free_1anim(anp);
	bu_vls_printf(gedp->ged_result_str, "Database read error, aborting");
	return GED_ERROR;
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
	bu_free((genptr_t)tp->tr_l.tl_mat, "tl_mat");
	tp->tr_l.tl_mat = (matp_t)NULL;
    }

    if (rt_db_put_internal(dp, gedp->ged_wdbp->dbip, &intern, &rt_uniresource) < 0) {
	bu_vls_printf(gedp->ged_result_str, "Database write error, aborting");
	goto fail;
    }
    db_free_1anim(anp);
    return GED_OK;

fail:
    rt_db_free_internal(&intern);
    db_free_1anim(anp);
    return GED_ERROR;
}


/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
