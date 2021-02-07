/*                    R A Y D I F F . C
 * BRL-CAD
 *
 * Copyright (c) 2015-2021 United States Government as represented by
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

#include "bu/app.h"
#include "raytrace.h"
#include "analyze.h"


int
main(int argc, char **argv)
{
    size_t i;
    struct db_i *dbip = DBI_NULL;
    struct directory *dp1 = RT_DIR_NULL;
    struct directory *dp2 = RT_DIR_NULL;
    struct bn_tol tol = {BN_TOL_MAGIC, BN_TOL_DIST, BN_TOL_DIST * BN_TOL_DIST, 1.0e-6, 1.0 - 1.0e-6 };
    struct analyze_raydiff_results *results;

    bu_setprogname(argv[0]);

    if (argc != 4) {
	bu_log("Error - please specify a .g file and two objects\n");
	bu_exit(EXIT_FAILURE, NULL);
    }

    if (!bu_file_exists(argv[1], NULL)) {
	bu_exit(1, "Cannot stat file %s\n", argv[1]);
    }

    if ((dbip = db_open(argv[1], DB_OPEN_READONLY)) == DBI_NULL) {
	bu_exit(1, "Cannot open geometry database file %s\n", argv[1]);
    }

    RT_CK_DBI(dbip);
    if (db_dirbuild(dbip) < 0) {
	db_close(dbip);
	bu_exit(1, "db_dirbuild failed on geometry database file %s\n", argv[1]);
    }

    dp1 = db_lookup(dbip, argv[2], LOOKUP_QUIET);
    dp2 = db_lookup(dbip, argv[3], LOOKUP_QUIET);

    if (dp1 == RT_DIR_NULL || dp2 == RT_DIR_NULL) return 1;

    analyze_raydiff(&results, dbip, dp1->d_namep, dp2->d_namep, &tol, 1);

    /* Print results */
    for (i = 0; i < BU_PTBL_LEN(results->left); i++) {
	struct diff_seg *dseg = (struct diff_seg *)BU_PTBL_GET(results->left, i);
	bu_log("Result: LEFT diff vol (%s): %g %g %g -> %g %g %g\n", argv[2], V3ARGS(dseg->in_pt), V3ARGS(dseg->out_pt));
    }
    for (i = 0; i < BU_PTBL_LEN(results->both); i++) {
	struct diff_seg *dseg = (struct diff_seg *)BU_PTBL_GET(results->both, i);
	bu_log("Result: BOTH): %g %g %g -> %g %g %g\n", V3ARGS(dseg->in_pt), V3ARGS(dseg->out_pt));
    }
    for (i = 0; i < BU_PTBL_LEN(results->right); i++) {
	struct diff_seg *dseg = (struct diff_seg *)BU_PTBL_GET(results->right, i);
	bu_log("Result: RIGHT diff vol (%s): %g %g %g -> %g %g %g\n", argv[3], V3ARGS(dseg->in_pt), V3ARGS(dseg->out_pt));
    }

    analyze_raydiff_results_free(results);

    db_close(dbip);
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
