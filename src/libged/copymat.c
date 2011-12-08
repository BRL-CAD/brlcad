/*                         C O P Y M A T . C
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
/** @file libged/copymat.c
 *
 * The copymat command.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include "bio.h"

#include "./ged_private.h"


int
ged_copymat(struct ged *gedp, int argc, const char *argv[])
{
    char *child = NULL;
    char *parent = NULL;
    struct bu_vls pvls;
    int i;
    int sep;
    int status;
    struct db_tree_state ts;
    struct directory *dp;
    struct rt_comb_internal *comb;
    struct rt_db_internal intern;
    struct animate *anp;
    union tree *tp;
    static const char *usage = "a/b c/d";

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

    if (argc != 3) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    /*
     * Ensure that each argument contains exactly one slash
     */
    for (i = 1; i <= 2; ++i) {
	if (((child = strchr(argv[i], '/')) == NULL)
	    || (strchr(++child, '/') != NULL))
	{
	    bu_vls_printf(gedp->ged_result_str, "%s: bad arc: '%s'\n", argv[0], argv[i]);
	    return GED_ERROR;
	}
    }

    BU_GET(anp, struct animate);
    anp->magic = ANIMATE_MAGIC;

    ts = gedp->ged_wdbp->wdb_initial_tree_state;	/* struct copy */
    ts.ts_dbip = gedp->ged_wdbp->dbip;
    ts.ts_resp = &rt_uniresource;
    MAT_IDN(ts.ts_mat);
    db_full_path_init(&anp->an_path);
    if (child == NULL
	|| db_follow_path_for_state(&ts, &(anp->an_path), argv[1], LOOKUP_NOISY) < 0)
    {
	bu_vls_printf(gedp->ged_result_str, "%s: cannot follow path for arc: '%s'\n", argv[0], argv[1]);
	return GED_ERROR;
    }

    bu_vls_init(&pvls);
    bu_vls_strcat(&pvls, argv[2]);
    parent = bu_vls_addr(&pvls);
    sep = strchr(parent, '/') - parent;
    bu_vls_trunc(&pvls, sep);
    switch (rt_db_lookup_internal(gedp->ged_wdbp->dbip, parent, &dp, &intern, LOOKUP_NOISY, &rt_uniresource)) {
	case ID_COMBINATION:
	    if (dp->d_flags & RT_DIR_COMB)
		break;
	    else {
		bu_vls_printf(gedp->ged_result_str,
			      "%s: Non-combination directory <%p> '%s' for combination rt_db_internal <%p>\nThis should not happen\n",
			      argv[0], (void *)dp, dp->d_namep, (void *)&intern);
	    }
	    /* Fall through this case */
	default:
	    bu_vls_printf(gedp->ged_result_str, "%s: Object '%s' is not a combination\n", argv[0], parent);
	    /* Fall through this case */
	case ID_NULL:
	    bu_vls_free(&pvls);
	    return GED_ERROR;
    }
    comb = (struct rt_comb_internal *) intern.idb_ptr;
    RT_CK_COMB(comb);

    tp = db_find_named_leaf(comb->tree, child);
    if (tp == TREE_NULL) {
	bu_vls_printf(gedp->ged_result_str, "%s: unable to find instance of '%s' in combination '%s'\n",
		      argv[0], child, dp->d_namep);
	status = GED_ERROR;
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
	bu_free((genptr_t) tp->tr_l.tl_mat, "tl_mat");
	tp->tr_l.tl_mat = (matp_t) 0;
    }

    if (rt_db_put_internal(dp, gedp->ged_wdbp->dbip, &intern, &rt_uniresource) < 0) {
	bu_vls_printf(gedp->ged_result_str, "%s: Database write error, aborting\n", argv[0]);
	status = GED_ERROR;
	goto wrapup;
    }

    status = GED_OK;

wrapup:

    bu_vls_free(&pvls);
    if (status == GED_ERROR)
	rt_db_free_internal(&intern);
    return status;
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
