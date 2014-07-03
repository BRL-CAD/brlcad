/*                    T E S T _ D I F F . C
 * BRL-CAD
 *
 * Copyright (c) 2014 United States Government as represented by
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

#include "common.h"

#include <string.h>

#include "tcl.h"

#include "mater.h"
#include "raytrace.h"
#include "rt/db_diff.h"

void
print_diff_summary(struct bu_ptbl *results)
{
    int i = 0;
    for (i = 0; i < (int)BU_PTBL_LEN(results); i++) {
	struct diff_result *dr = (struct diff_result *)BU_PTBL_GET(results, i);
	if (dr->param_state != DIFF_UNCHANGED) {
	    int j = 0;
	    bu_log("Object %s parameters have changed:\n", dr->obj_name);
	    for (j = 0; j < (int)BU_PTBL_LEN(dr->param_diffs); j++) {
		struct diff_avp *avp = (struct diff_avp *)BU_PTBL_GET(dr->param_diffs, j);
		if (avp->state == DIFF_ADDED)
		    bu_log("A %s: %s\n", avp->name, avp->right_value);
		if (avp->state == DIFF_REMOVED)
		    bu_log("D %s: %s\n", avp->name, avp->left_value);
		if (avp->state == DIFF_CHANGED)
		    bu_log("M %s: %s -> %s\n", avp->name, avp->left_value, avp->right_value);
	    }
	    bu_log("\n");
	}
	if (dr->attr_state != DIFF_UNCHANGED && dr->attr_state != DIFF_EMPTY) {
	    int j = 0;
	    bu_log("Object %s attributes have changed:\n", dr->obj_name);
	    for (j = 0; j < (int)BU_PTBL_LEN(dr->attr_diffs); j++) {
		struct diff_avp *avp = (struct diff_avp *)BU_PTBL_GET(dr->attr_diffs, j);
		if (avp->state == DIFF_ADDED)
		    bu_log("A %s: %s\n", avp->name, avp->right_value);
		if (avp->state == DIFF_REMOVED)
		    bu_log("D %s: %s\n", avp->name, avp->left_value);
		if (avp->state == DIFF_CHANGED)
		    bu_log("M %s: %s -> %s\n", avp->name, avp->left_value, avp->right_value);
	    }
	    bu_log("\n");
	}
	if ((dr->param_state != DIFF_UNCHANGED && dr->param_state != DIFF_EMPTY) ||
		(dr->attr_state != DIFF_UNCHANGED && dr->attr_state != DIFF_EMPTY)) {
	    bu_log("\n");
	}
    }
}

int
main(int argc, char **argv)
{
    int i = 0;
    int diff_state = 0;
    struct bu_ptbl results;
    struct db_i *dbip1 = DBI_NULL;
    struct db_i *dbip2 = DBI_NULL;
    /*struct bu_vls diff_log = BU_VLS_INIT_ZERO;*/
    struct bn_tol *diff_tol;
    BU_GET(diff_tol, struct bn_tol);
    diff_tol->dist = BN_TOL_DIST;

    BU_PTBL_INIT(&results);

    if (argc != 3) {
	bu_log("Error - please specify two .g files\n");
	bu_exit(EXIT_FAILURE, NULL);
    }

    if (!bu_file_exists(argv[1], NULL)) {
	bu_exit(1, "Cannot stat file %s\n", argv[1]);
    }

    if (!bu_file_exists(argv[2], NULL)) {
	bu_exit(1, "Cannot stat file %s\n", argv[2]);
    }

    if ((dbip1 = db_open(argv[1], DB_OPEN_READONLY)) == DBI_NULL) {
	bu_exit(1, "Cannot open geometry database file %s\n", argv[1]);
    }
    RT_CK_DBI(dbip1);
    if (db_dirbuild(dbip1) < 0) {
	db_close(dbip1);
	bu_exit(1, "db_dirbuild failed on geometry database file %s\n", argv[1]);
    }

    /* Reset the material head so we don't get warnings when the global
     * is overwritten.  This will go away when material_head ceases to
     * be a librt global.*/
    rt_new_material_head(MATER_NULL);

    if ((dbip2 = db_open(argv[2], DB_OPEN_READONLY)) == DBI_NULL) {
	bu_exit(1, "Cannot open geometry database file %s\n", argv[2]);
    }
    RT_CK_DBI(dbip2);
    if (db_dirbuild(dbip2) < 0) {
	db_close(dbip1);
	db_close(dbip2);
	bu_exit(1, "db_dirbuild failed on geometry database file %s\n", argv[1]);
    }


    diff_state = db_diff(dbip1, dbip2, diff_tol, DB_COMPARE_ALL, &results);
    if (diff_state == DIFF_UNCHANGED) {
	bu_log("No differences found.\n");
    } else {
	print_diff_summary(&results);
    }

    for (i = 0; i < (int)BU_PTBL_LEN(&results); i++) {
	struct diff_result *result = (struct diff_result *)BU_PTBL_GET(&results, i);
	diff_free_result(result);
    }

    bu_ptbl_free(&results);
    BU_PUT(diff_tol, struct bn_tol);

    db_close(dbip1);
    db_close(dbip2);
    return 0;
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
