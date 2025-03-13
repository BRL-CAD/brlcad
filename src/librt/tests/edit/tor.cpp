/*                         T O R . C P P
 * BRL-CAD
 *
 * Copyright (c) 2025 United States Government as represented by
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
/** @file tor.cpp
 *
 * Test editing of TOR primitive parameters.
 */

#include "common.h"

#include "vmath.h"
#include "bu/app.h"
#include "bu/avs.h"
#include "bu/env.h"
#include "bu/malloc.h"
#include "bu/process.h"
#include "bu/str.h"
#include "raytrace.h"
#include "rt/rt_ecmds.h"


struct directory *
make_tor(struct rt_wdb *wdbp)
{
    const char *objname = "tor";
    struct rt_tor_internal *tor;
    BU_ALLOC(tor, struct rt_tor_internal);
    tor->magic = RT_TOR_INTERNAL_MAGIC;
    VSET(tor->v, 0, 0, 0);
    VSET(tor->h, 0, 0, 1);
    tor->r_a = 20;
    tor->r_h = 2;

    wdb_export(wdbp, objname, (void *)tor, ID_TOR, 1.0);

    struct directory *dp = db_lookup(wdbp->dbip, objname, LOOKUP_QUIET);
    if (dp == RT_DIR_NULL)
	bu_exit(1, "ERROR: Unable to create tor object: %s\n", objname);

    return dp;
}

int
main(int argc, char *argv[])
{
    bu_setprogname(argv[0]);
    if (argc != 1)
	return BRLCAD_ERROR;

    struct db_i *dbip = db_open_inmem();
    if (dbip == DBI_NULL)
        bu_exit(1, "ERROR: Unable to create database instance\n");

    struct rt_wdb *wdbp = wdb_dbopen(dbip, RT_WDB_TYPE_DB_INMEM);

    struct directory *dp = make_tor(wdbp);

    // We'll refer to the original for reference
    struct rt_db_internal intern;
    rt_db_get_internal(&intern, dp, dbip, NULL, &rt_uniresource);
    struct rt_tor_internal *orig_tor = (struct rt_tor_internal *)intern.idb_ptr;

    struct bn_tol tol = BN_TOL_INIT_TOL;
    struct db_full_path fp;
    db_full_path_init(&fp);
    db_add_node_to_full_path(&fp, dp);
    struct bview *v;
    BU_GET(v, struct bview);
    bv_init(v, NULL);
    bu_vls_sprintf(&v->gv_name, "default");
    v->gv_width = 512;
    v->gv_height = 512;

    // Set up rt_solid_edit container
    struct rt_solid_edit *s = rt_solid_edit_create(&fp, dbip, &tol, v);
    struct rt_tor_internal *edit_tor = (struct rt_tor_internal *)s->es_int.idb_ptr;


    // Now, try modifying some of the parameters
    EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, ECMD_TOR_R1);
    s->es_scale = 2.0;
    rt_solid_edit_process(s);
    if (!NEAR_EQUAL(edit_tor->r_a, 40, VUNITIZE_TOL))
	bu_exit(1, "ERROR: ECMD_TOR_R1 failed - unable to scale tor parameter r_a\n");
    if (!VNEAR_EQUAL(edit_tor->v, orig_tor->v, VUNITIZE_TOL))
	bu_exit(1, "ERROR: ECMD_TOR_R1 changed tor parameter v\n");
    if (!VNEAR_EQUAL(edit_tor->h, orig_tor->h, VUNITIZE_TOL))
	bu_exit(1, "ERROR: ECMD_TOR_R1 changed tor parameter h\n");
    if (!NEAR_EQUAL(edit_tor->r_h, orig_tor->r_h, VUNITIZE_TOL))
	bu_exit(1, "ERROR: ECMD_TOR_R1 changed tor parameter r_h\n");

    bu_log("SUCCESS: original r_a value %g modified to %g\n", orig_tor->r_a, edit_tor->r_a);

    rt_solid_edit_destroy(s);

    db_close(dbip);

    return 0;
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8

