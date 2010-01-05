/*                         S H O W M A T S . C
 * BRL-CAD
 *
 * Copyright (c) 2008-2009 United States Government as represented by
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
/** @file showmats.c
 *
 * The showmats command.
 *
 */

#include "common.h"

#include <string.h>
#include "bio.h"

#include "cmd.h"

#include "./ged_private.h"


struct showmats_data {
    struct ged	*smd_gedp;
    int		smd_count;
    char	*smd_child;
    mat_t	smd_mat;
};

static void Do_showmats(struct db_i		*dbip,
			struct rt_comb_internal	*comb,
			union tree		*comb_leaf,
			genptr_t		user_ptr1,
			genptr_t		user_ptr2,
			genptr_t		user_ptr3);

int
ged_showmats(struct ged *gedp, int argc, const char *argv[])
{
    struct showmats_data sm_data;
    char *parent;
    struct directory *dp;
    int max_count=1;
    static const char *usage = "path";

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    /* initialize result */
    bu_vls_trunc(&gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    if (argc != 2) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    sm_data.smd_gedp = gedp;
    MAT_IDN(sm_data.smd_mat);

    parent = strtok((char *)argv[1], "/");
    while ((sm_data.smd_child = strtok((char *)NULL, "/")) != NULL) {
	struct rt_db_internal	intern;
	struct rt_comb_internal *comb;

	if ((dp = db_lookup(gedp->ged_wdbp->dbip, parent, LOOKUP_NOISY)) == DIR_NULL)
	    return TCL_ERROR;

	bu_vls_printf(&gedp->ged_result_str, "%s\n", parent);

	if (!(dp->d_flags & DIR_COMB)) {
	    bu_vls_printf(&gedp->ged_result_str, "\tThis is not a combination\n");
	    break;
	}

	if (rt_db_get_internal(&intern, dp, gedp->ged_wdbp->dbip, (fastf_t *)NULL, &rt_uniresource) < 0) {
	    bu_vls_printf(&gedp->ged_result_str, "Database read error, aborting.\n");
	    return GED_ERROR;
	}
	comb = (struct rt_comb_internal *)intern.idb_ptr;

	sm_data.smd_count = 0;

	if (comb->tree)
	    db_tree_funcleaf(gedp->ged_wdbp->dbip, comb, comb->tree, Do_showmats,
			     (genptr_t)&sm_data, (genptr_t)NULL, (genptr_t)NULL);
	rt_db_free_internal(&intern);

	if (!sm_data.smd_count) {
	    bu_vls_printf(&gedp->ged_result_str, "%s is not a member of %s\n", sm_data.smd_child, parent);
	    return GED_ERROR;
	}
	if (sm_data.smd_count > max_count)
	    max_count = sm_data.smd_count;

	parent = sm_data.smd_child;
    }
    bu_vls_printf(&gedp->ged_result_str, "%s\n", parent);

    if (max_count > 1)
	bu_vls_printf(&gedp->ged_result_str, "\nAccumulated matrix (using first occurrence of each object):\n");
    else
	bu_vls_printf(&gedp->ged_result_str, "\nAccumulated matrix:\n");

    {
	char obuf[1024];

	bn_mat_print_guts("", sm_data.smd_mat, obuf, 1024);
	bu_vls_printf(&gedp->ged_result_str, "%s", obuf);
    }

    return GED_OK;
}

static void
Do_showmats(struct db_i			*dbip,
	    struct rt_comb_internal	*comb,
	    union tree			*comb_leaf,
	    genptr_t			user_ptr1,
	    genptr_t			user_ptr2,
	    genptr_t			user_ptr3)
{
    struct showmats_data *smdp;
    char obuf[1024];

    RT_CK_DBI(dbip);
    RT_CK_TREE(comb_leaf);

    smdp = (struct showmats_data *)user_ptr1;

    if (strcmp(comb_leaf->tr_l.tl_name, smdp->smd_child))
	return;

    smdp->smd_count++;
    if (smdp->smd_count > 1) {
	bu_vls_printf(&smdp->smd_gedp->ged_result_str, "\n\tOccurrence #%d:\n", smdp->smd_count);
    }

    bn_mat_print_guts("", comb_leaf->tr_l.tl_mat, obuf, 1024);
    bu_vls_printf(&smdp->smd_gedp->ged_result_str, "%s", obuf);

    if (smdp->smd_count == 1) {
	mat_t tmp_mat;
	if (comb_leaf->tr_l.tl_mat) {
	    bn_mat_mul(tmp_mat, smdp->smd_mat, comb_leaf->tr_l.tl_mat);
	    MAT_COPY(smdp->smd_mat, tmp_mat);
	}
    }
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
