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
#include "bu/log.h"
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
    VSET(tor->v, 10, 5, 20);
    VSET(tor->h, 1, 1, 1);
    VUNITIZE(tor->h);
    tor->r_a = 20;
    tor->r_h = 2;

    wdb_export(wdbp, objname, (void *)tor, ID_TOR, 1.0);

    struct directory *dp = db_lookup(wdbp->dbip, objname, LOOKUP_QUIET);
    if (dp == RT_DIR_NULL)
	bu_exit(1, "ERROR: Unable to create tor object: %s\n", objname);

    return dp;
}

int
tor_diff(const char *cmd, struct rt_tor_internal *ctrl, struct rt_tor_internal *t)
{
    int ret = 0;
    if (!ctrl || !t)
	return 1;

    if (!NEAR_EQUAL(ctrl->r_a, t->r_a, VUNITIZE_TOL)) {
	bu_log("ERROR(%s) unexpected change - tor parameter r_a %0.17f -> %0.17f\n", cmd, ctrl->r_a, t->r_a);
	ret = 1;
    }
    if (!VNEAR_EQUAL(ctrl->v, t->v, VUNITIZE_TOL)) {
	bu_log("ERROR(%s) unexpected change - tor parameter v %0.17f,%0.17f,%0.17f -> %0.17f,%0.17f,%0.17f\n", cmd, V3ARGS(ctrl->v), V3ARGS(t->v));
	ret = 1;
    }
    if (!VNEAR_EQUAL(ctrl->h, t->h, VUNITIZE_TOL)) {
	bu_log("ERROR(%s) unexpected change - tor parameter h %0.17f,%0.17f,%0.17f -> %0.17f,%0.17f,%0.17f\n", cmd, V3ARGS(ctrl->h), V3ARGS(t->h));
	ret = 1;
    }
    if (!NEAR_EQUAL(ctrl->r_h, t->r_h, VUNITIZE_TOL)) {
	bu_log("ERROR(%s) unexpected change - tor parameter r_h %0.17f -> %0.17f\n", cmd, ctrl->r_h, t->r_h);
	ret = 1;
    }

    return ret;
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
    struct rt_db_internal cmpintern;
    rt_db_get_internal(&cmpintern, dp, dbip, NULL, &rt_uniresource);
    struct rt_tor_internal *cmp_tor = (struct rt_tor_internal *)cmpintern.idb_ptr;

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


    // Now, begin testing modifications.

    /*************
      ECMD_TOR_R1
     **************/
    EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, ECMD_TOR_R1);

    // Directly set the scale parameter
    s->e_inpara = 0;
    s->es_scale = 2.0;

    cmp_tor->r_a = orig_tor->r_a * s->es_scale; // set cmp val to expected

    rt_solid_edit_process(s);

    if (tor_diff("ECMD_TOR_R1", cmp_tor, edit_tor))
	bu_exit(1, "ERROR: ECMD_TOR_R1 failed scaling tor parameter r_a\n");

    bu_log("ECMD_TOR_R1 SUCCESS: original r_a value %g modified via es_scale to %g\n", orig_tor->r_a, edit_tor->r_a);

    // Test e_inpara mode by restoring the original r_a value
    s->e_inpara = 1;
    s->e_para[0] = 20;

    cmp_tor->r_a = orig_tor->r_a; // set cmp val to expected

    rt_solid_edit_process(s);


    if (tor_diff("ECMD_TOR_R1", cmp_tor, edit_tor))
	bu_exit(1, "ERROR: ECMD_TOR_R1 failed scaling tor parameter r_a\n");

    bu_log("ECMD_TOR_R1 SUCCESS: r_a value restored via e_para to %g\n", edit_tor->r_a);

    /*************
      ECMD_TOR_R2
     **************/
    EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, ECMD_TOR_R2);

    // Direct mode
    s->e_inpara = 0;
    s->es_scale = 5;

    cmp_tor->r_h = orig_tor->r_h * s->es_scale; // set cmp val to expected

    rt_solid_edit_process(s);

    if (tor_diff("ECMD_TOR_R2", cmp_tor, edit_tor))
	bu_exit(1, "ERROR: ECMD_TOR_R2 failed scaling tor parameter r_h\n");

    bu_log("ECMD_TOR_R2 SUCCESS: original r_h value %g modified via es_scale to %g\n", orig_tor->r_h, edit_tor->r_h);

    // Test e_inpara mode by restoring the original r_h value
    s->e_inpara = 1;
    s->e_para[0] = 2;

    cmp_tor->r_h = orig_tor->r_h; // set cmp val to expected

    rt_solid_edit_process(s);


    if (tor_diff("ECMD_TOR_R2", cmp_tor, edit_tor))
	bu_exit(1, "ERROR: ECMD_TOR_R2 failed scaling tor parameter r_h\n");

    bu_log("ECMD_TOR_R2 SUCCESS: r_h value restored via e_para to %g\n", edit_tor->r_h);

    /**********************
       RT_SOLID_EDIT_SCALE
     **********************/
    EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, RT_SOLID_EDIT_SCALE);

    // Directly set the scale parameter
    s->e_inpara = 0;
    s->es_scale = 3.0;

    // set cmp vals to expected
    cmp_tor->r_a = orig_tor->r_a * s->es_scale;
    cmp_tor->r_h = orig_tor->r_h * s->es_scale;

    rt_solid_edit_process(s);

    if (tor_diff("RT_SOLID_EDIT_SCALE", cmp_tor, edit_tor))
	bu_exit(1, "ERROR: RT_SOLID_EDIT_SCALE failed scaling tor\n");

    bu_log("RT_SOLID_EDIT_SCALE SUCCESS: original r_a value %g modified via es_scale to %g\n", orig_tor->r_a, edit_tor->r_a);
    bu_log("RT_SOLID_EDIT_SCALE SUCCESS: original r_h value %g modified via es_scale to %g\n", orig_tor->r_h, edit_tor->r_h);

    // Test e_inpara mode by restoring the original r_a value

    // Reset
    edit_tor->r_a = orig_tor->r_a;
    edit_tor->r_h = orig_tor->r_h;

    s->e_inpara = 1;
    s->e_para[0] = 4;

    // set cmp vals to expected
    cmp_tor->r_a = orig_tor->r_a * s->e_para[0];
    cmp_tor->r_h = orig_tor->r_h * s->e_para[0];

    rt_solid_edit_process(s);

    if (tor_diff("RT_SOLID_EDIT_SCALE", cmp_tor, edit_tor))
	bu_exit(1, "ERROR: RT_SOLID_EDIT_SCALE failed scaling tor\n");

    bu_log("RT_SOLID_EDIT_SCALE SUCCESS: original r_a value %g modified via es_scale to %g\n", orig_tor->r_a, edit_tor->r_a);
    bu_log("RT_SOLID_EDIT_SCALE SUCCESS: original r_h value %g modified via es_scale to %g\n", orig_tor->r_h, edit_tor->r_h);


    /**********************
       RT_SOLID_EDIT_TRANS
     **********************/
    EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, RT_SOLID_EDIT_TRANS);

    // Reset
    s->es_scale = 1.0;
    s->e_inpara = 0;
    VSETALL(s->e_para, 0);
    edit_tor->r_a = orig_tor->r_a;
    edit_tor->r_h = orig_tor->r_h;
    cmp_tor->r_a = orig_tor->r_a;
    cmp_tor->r_h = orig_tor->r_h;

    // Set translation values
    s->e_inpara = 1;
    VSET(s->e_para, 20, 55, 40);

    // set cmp vals to expected
    VMOVE(cmp_tor->v, s->e_para);

    rt_solid_edit_process(s);

    if (tor_diff("RT_SOLID_EDIT_TRANS", cmp_tor, edit_tor))
	bu_exit(1, "ERROR: RT_SOLID_EDIT_TRANS failed translating tor\n");

    bu_log("RT_SOLID_EDIT_TRANS SUCCESS: original v value %g,%g,%g modified to %g,%g,%g\n", V3ARGS(orig_tor->v), V3ARGS(edit_tor->v));


    /**********************
       RT_SOLID_EDIT_ROT
     **********************/
    EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, RT_SOLID_EDIT_ROT);

    // Reset
    s->es_scale = 1.0;
    s->e_inpara = 0;
    VSETALL(s->e_para, 0);
    VMOVE(edit_tor->v, orig_tor->v);
    VMOVE(cmp_tor->v, orig_tor->v);

    // Set rotation values - rotate about view center
    s->e_inpara = 1;
    VSET(s->e_para, 10, 5, 20);
    s->vp->gv_rotate_about = 'v';

    // set cmp vals to expected
    VSET(cmp_tor->v, 9.40068965447370530,4.65191166950628698,20.37122362155455590);
    VSET(cmp_tor->h, 0.39407267558562348,0.63410305535343825,0.66529695741692840);

    rt_solid_edit_process(s);

    if (tor_diff("RT_SOLID_EDIT_ROT (v)", cmp_tor, edit_tor))
	bu_exit(1, "ERROR: RT_SOLID_EDIT_ROT(v) failed rotating tor\n");

    bu_log("RT_SOLID_EDIT_ROT(v) SUCCESS: original v value %g,%g,%g modified to %g,%g,%g\n", V3ARGS(orig_tor->v), V3ARGS(edit_tor->v));
    bu_log("RT_SOLID_EDIT_ROT(v) SUCCESS: original h value %g,%g,%g modified to %g,%g,%g\n", V3ARGS(orig_tor->h), V3ARGS(edit_tor->h));


    // TODO - Set rotation values - rotate about eye
    VMOVE(edit_tor->v, orig_tor->v);
    VMOVE(edit_tor->h, orig_tor->h);
    s->e_inpara = 1;
    VSET(s->e_para, 10, 5, 20);
    s->vp->gv_rotate_about = 'e';

    // set cmp vals to expected
    VMOVE(cmp_tor->v, orig_tor->v);
    VMOVE(cmp_tor->h, orig_tor->h);

    rt_solid_edit_process(s);

    if (tor_diff("RT_SOLID_EDIT_ROT (e)", cmp_tor, edit_tor))
	bu_exit(1, "ERROR: RT_SOLID_EDIT_ROT(e) failed rotating tor\n");

    bu_log("RT_SOLID_EDIT_ROT(e) SUCCESS: original v value %g,%g,%g modified to %g,%g,%g\n", V3ARGS(orig_tor->v), V3ARGS(edit_tor->v));
    bu_log("RT_SOLID_EDIT_ROT(e) SUCCESS: original h value %g,%g,%g modified to %g,%g,%g\n", V3ARGS(orig_tor->h), V3ARGS(edit_tor->h));


    // TODO - Set rotation values - rotate about model center
    VMOVE(edit_tor->v, orig_tor->v);
    VMOVE(edit_tor->h, orig_tor->h);
    s->e_inpara = 1;
    VSET(s->e_para, 10, 5, 20);
    s->vp->gv_rotate_about = 'm';

    // set cmp vals to expected
    VMOVE(cmp_tor->v, orig_tor->v);
    VMOVE(cmp_tor->h, orig_tor->h);

    rt_solid_edit_process(s);

    if (tor_diff("RT_SOLID_EDIT_ROT (e)", cmp_tor, edit_tor))
	bu_exit(1, "ERROR: RT_SOLID_EDIT_ROT(e) failed rotating tor\n");

    bu_log("RT_SOLID_EDIT_ROT(e) SUCCESS: original v value %g,%g,%g modified to %g,%g,%g\n", V3ARGS(orig_tor->v), V3ARGS(edit_tor->v));
    bu_log("RT_SOLID_EDIT_ROT(e) SUCCESS: original h value %g,%g,%g modified to %g,%g,%g\n", V3ARGS(orig_tor->h), V3ARGS(edit_tor->h));


    // TODO - Set rotation values - rotate about keypoint
    VMOVE(edit_tor->v, orig_tor->v);
    VMOVE(edit_tor->h, orig_tor->h);
    s->e_inpara = 1;
    VSET(s->e_para, 10, 5, 20);
    s->vp->gv_rotate_about = 'k';

    // set cmp vals to expected
    VMOVE(cmp_tor->v, orig_tor->v);
    VMOVE(cmp_tor->h, orig_tor->h);

    rt_solid_edit_process(s);

    if (tor_diff("RT_SOLID_EDIT_ROT (e)", cmp_tor, edit_tor))
	bu_exit(1, "ERROR: RT_SOLID_EDIT_ROT(e) failed rotating tor\n");

    bu_log("RT_SOLID_EDIT_ROT(e) SUCCESS: original v value %g,%g,%g modified to %g,%g,%g\n", V3ARGS(orig_tor->v), V3ARGS(edit_tor->v));
    bu_log("RT_SOLID_EDIT_ROT(e) SUCCESS: original h value %g,%g,%g modified to %g,%g,%g\n", V3ARGS(orig_tor->h), V3ARGS(edit_tor->h));


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

