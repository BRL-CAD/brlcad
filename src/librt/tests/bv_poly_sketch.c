/*                  B V _ P O L Y _ S K E T C H . C
 * BRL-CAD
 *
 * Copyright (c) 2013-2023 United States Government as represented by
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

#include "vmath.h"
#include "bu/app.h"
#include "bv.h"
#include "raytrace.h"
#include "rt/primitives/sketch.h"
#include "rt/db_diff.h"

int
main(int argc, char *argv[])
{
    struct db_i *dbip;
    struct directory *dp;

    bu_setprogname(argv[0]);

    if (argc != 2) {
	bu_exit(EXIT_FAILURE, "Usage: %s file.g", argv[0]);
    }

    // First, open the database and make sure we have poly.s

    dbip = db_open(argv[1], DB_OPEN_READONLY);
    if (dbip == DBI_NULL) {
	bu_exit(EXIT_FAILURE, "ERROR: Unable to read from %s\n", argv[1]);
    }

    if (db_dirbuild(dbip) < 0) {
	bu_exit(EXIT_FAILURE, "ERROR: Unable to read from %s\n", argv[1]);
    }

    db_update_nref(dbip, &rt_uniresource);

    dp = db_lookup(dbip, "poly.s", LOOKUP_QUIET);
    if (dp == RT_DIR_NULL)
	bu_exit(EXIT_FAILURE, "ERROR: Unable to look up object poly.s\n");

    // Create the view
    struct bview *v;
    BU_GET(v, struct bview);
    bv_init(v, NULL);

    struct bv_scene_obj *pobj = db_sketch_to_scene_obj("poly", dbip, dp, v, 0);

    if (!pobj)
	bu_exit(EXIT_FAILURE, "Failed to create scene object from poly.s\n");

    char ofile[MAXPATHLEN];
    bu_dir(ofile, MAXPATHLEN, BU_DIR_CURR, "poly_sketch_out.g", NULL);
    struct rt_wdb *wfp = wdb_fopen_v(ofile, 5);
    if (!wfp)
	bu_exit(EXIT_FAILURE, "Failed to create output database %s\n", ofile);

    struct directory *odp = db_scene_obj_to_sketch(wfp->dbip, "poly.s", pobj);
    if (odp == RT_DIR_NULL)
	bu_exit(EXIT_FAILURE, "Failed to write scene obj polygon to output database %s\n", ofile);

    const struct bn_tol tol = BN_TOL_INIT_TOL;
    int dret = db_diff_dp(dbip, wfp->dbip, dp, odp, &tol, DB_COMPARE_PARAM, NULL);
    if (dret) {
	struct rt_db_internal ointern, nintern;
	int old_id = rt_db_get_internal(&ointern, dp, dbip, NULL, &rt_uniresource);
	int new_id = rt_db_get_internal(&nintern, odp, wfp->dbip, NULL, &rt_uniresource);
	struct bu_vls msg = BU_VLS_INIT_ZERO;
	if (OBJ[old_id].ft_describe)
	    OBJ[old_id].ft_describe(&msg, &ointern, 100, dbip->dbi_base2local);
	bu_log("Original sketch info: %s\n", bu_vls_cstr(&msg));
	bu_vls_trunc(&msg, 0);
	if (OBJ[new_id].ft_describe)
	    OBJ[new_id].ft_describe(&msg, &nintern, 100, wfp->dbip->dbi_base2local);
	bu_log("Exported sketch info: %s\n", bu_vls_cstr(&msg));
	bu_vls_free(&msg);
	bu_exit(EXIT_FAILURE, "Difference between imported sketch and written sketch\n");
    }

    db_close(dbip);
    db_close(wfp->dbip);

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
