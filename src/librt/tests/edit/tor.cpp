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
    bv_init(v);
    VSET(v->gv_aet, 45, 35, 0);
    bv_mat_aet(v);
    v->gv_size = 73.3197;
    v->gv_isize = 1.0 / v->gv_size;
    v->gv_scale = 0.5 * v->gv_size;
    bv_update(v);
    bu_vls_sprintf(&v->gv_name, "default");
    v->gv_width = 512;
    v->gv_height = 512;

    // Set up rt_edit container
    struct rt_edit *s = rt_edit_create(&fp, dbip, &tol, v);

    // MGED normally has this set, but the user can explicitly disable
    // it.  For most of our testing, have it on.
    s->mv_context = 1;

    // We'll want to directly check and reset the working torus
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

    rt_edit_process(s);

    if (tor_diff("ECMD_TOR_R1", cmp_tor, edit_tor))
	bu_exit(1, "ERROR: ECMD_TOR_R1 failed scaling tor parameter r_a\n");

    bu_log("ECMD_TOR_R1 SUCCESS: original r_a value %g modified via es_scale to %g\n", orig_tor->r_a, edit_tor->r_a);

    // Test e_inpara mode by restoring the original r_a value
    s->e_inpara = 1;
    s->e_para[0] = 20;

    cmp_tor->r_a = orig_tor->r_a; // set cmp val to expected

    rt_edit_process(s);


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

    rt_edit_process(s);

    if (tor_diff("ECMD_TOR_R2", cmp_tor, edit_tor))
	bu_exit(1, "ERROR: ECMD_TOR_R2 failed scaling tor parameter r_h\n");

    bu_log("ECMD_TOR_R2 SUCCESS: original r_h value %g modified via es_scale to %g\n", orig_tor->r_h, edit_tor->r_h);

    // Test e_inpara mode by restoring the original r_h value
    s->e_inpara = 1;
    s->e_para[0] = 2;

    cmp_tor->r_h = orig_tor->r_h; // set cmp val to expected

    rt_edit_process(s);


    if (tor_diff("ECMD_TOR_R2", cmp_tor, edit_tor))
	bu_exit(1, "ERROR: ECMD_TOR_R2 failed scaling tor parameter r_h\n");

    bu_log("ECMD_TOR_R2 SUCCESS: r_h value restored via e_para to %g\n", edit_tor->r_h);

    /**********************
       RT_PARAMS_EDIT_SCALE
     **********************/
    EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, RT_PARAMS_EDIT_SCALE);

    // Directly set the scale parameter
    s->e_inpara = 0;
    s->es_scale = 3.0;

    // set cmp vals to expected
    cmp_tor->r_a = orig_tor->r_a * s->es_scale;
    cmp_tor->r_h = orig_tor->r_h * s->es_scale;

    rt_edit_process(s);

    if (tor_diff("RT_PARAMS_EDIT_SCALE", cmp_tor, edit_tor))
	bu_exit(1, "ERROR: RT_PARAMS_EDIT_SCALE failed scaling tor\n");

    bu_log("RT_PARAMS_EDIT_SCALE SUCCESS: original r_a value %g modified via es_scale to %g\n", orig_tor->r_a, edit_tor->r_a);
    bu_log("RT_PARAMS_EDIT_SCALE SUCCESS: original r_h value %g modified via es_scale to %g\n", orig_tor->r_h, edit_tor->r_h);

    // Test e_inpara mode by restoring the original r_a value

    // Reset
    edit_tor->r_a = orig_tor->r_a;
    edit_tor->r_h = orig_tor->r_h;

    s->e_inpara = 1;
    s->e_para[0] = 4;

    // set cmp vals to expected
    cmp_tor->r_a = orig_tor->r_a * s->e_para[0];
    cmp_tor->r_h = orig_tor->r_h * s->e_para[0];

    rt_edit_process(s);

    if (tor_diff("RT_PARAMS_EDIT_SCALE", cmp_tor, edit_tor))
	bu_exit(1, "ERROR: RT_PARAMS_EDIT_SCALE failed scaling tor\n");

    bu_log("RT_PARAMS_EDIT_SCALE SUCCESS: original r_a value %g modified via es_scale to %g\n", orig_tor->r_a, edit_tor->r_a);
    bu_log("RT_PARAMS_EDIT_SCALE SUCCESS: original r_h value %g modified via es_scale to %g\n", orig_tor->r_h, edit_tor->r_h);


    /**********************
       RT_PARAMS_EDIT_TRANS
     **********************/
    EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, RT_PARAMS_EDIT_TRANS);

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

    rt_edit_process(s);

    if (tor_diff("RT_PARAMS_EDIT_TRANS", cmp_tor, edit_tor))
	bu_exit(1, "ERROR: RT_PARAMS_EDIT_TRANS failed translating tor\n");

    bu_log("RT_PARAMS_EDIT_TRANS SUCCESS: original v value %g,%g,%g modified to %g,%g,%g\n", V3ARGS(orig_tor->v), V3ARGS(edit_tor->v));


    // Test a translate without mv_context set
    s->mv_context = 0;
    MAT_IDN(s->acc_rot_sol);
    VMOVE(edit_tor->v, orig_tor->v);
    VMOVE(cmp_tor->v, orig_tor->v);
    VSET(s->e_keypoint, 0, 0, 0);
    s->e_inpara = 1;
    VSET(s->e_para, 20, 55, 40);

    // set cmp vals to expected
    VSET(cmp_tor->v, 30, 60, 60);

    rt_edit_process(s);

    if (tor_diff("RT_PARAMS_EDIT_TRANS", cmp_tor, edit_tor))
	bu_exit(1, "ERROR: RT_PARAMS_EDIT_TRANS failed translating tor\n");

    bu_log("RT_PARAMS_EDIT_TRANS (mv_context == 0) SUCCESS: original v value %g,%g,%g modified to %g,%g,%g\n", V3ARGS(orig_tor->v), V3ARGS(edit_tor->v));


    /**********************
       RT_PARAMS_EDIT_ROT
     **********************/
    EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, RT_PARAMS_EDIT_ROT);

    // Reset
    s->mv_context = 1;
    s->es_scale = 1.0;
    s->e_inpara = 0;
    MAT_IDN(s->acc_rot_sol);
    VMOVE(edit_tor->v, orig_tor->v);
    VMOVE(cmp_tor->v, orig_tor->v);

    // Set rotation values - rotate about view center
    s->e_inpara = 1;
    VSET(s->e_para, 5, 5, 5);
    s->vp->gv_rotate_about = 'v';

    // set cmp vals to expected
    VSET(cmp_tor->v, 11.23303317584687910,4.16614044477699519,19.53105832935626651);
    VSET(cmp_tor->h, 0.57315612572226438,0.57695134825421168,0.58190995634607523);

    rt_edit_process(s);

    if (tor_diff("RT_PARAMS_EDIT_ROT (v)", cmp_tor, edit_tor))
	bu_exit(1, "ERROR: RT_PARAMS_EDIT_ROT(v) failed rotating tor\n");

    bu_log("RT_PARAMS_EDIT_ROT(v) SUCCESS: original v value %g,%g,%g modified to %g,%g,%g\n", V3ARGS(orig_tor->v), V3ARGS(edit_tor->v));
    bu_log("RT_PARAMS_EDIT_ROT(v) SUCCESS: original h value %g,%g,%g modified to %g,%g,%g\n", V3ARGS(orig_tor->h), V3ARGS(edit_tor->h));


    // Set rotation values - rotate about eye
    VMOVE(edit_tor->v, orig_tor->v);
    VMOVE(edit_tor->h, orig_tor->h);
    MAT_IDN(s->acc_rot_sol);
    s->e_inpara = 1;
    VSET(s->e_para, 5, 5, 5);
    s->vp->gv_rotate_about = 'e';

    // set cmp vals to expected
    VSET(cmp_tor->v, 11.40534719696836596,4.16282380897874571,19.36178344500957493);
    VSET(cmp_tor->h, 0.57315612572226449,0.57695134825421179,0.58190995634607534);

    rt_edit_process(s);

    if (tor_diff("RT_PARAMS_EDIT_ROT (e)", cmp_tor, edit_tor))
	bu_exit(1, "ERROR: RT_PARAMS_EDIT_ROT(e) failed rotating tor\n");

    bu_log("RT_PARAMS_EDIT_ROT(e) SUCCESS: original v value %g,%g,%g modified to %g,%g,%g\n", V3ARGS(orig_tor->v), V3ARGS(edit_tor->v));
    bu_log("RT_PARAMS_EDIT_ROT(e) SUCCESS: original h value %g,%g,%g modified to %g,%g,%g\n", V3ARGS(orig_tor->h), V3ARGS(edit_tor->h));


    // Set rotation values - rotate about model center
    VMOVE(edit_tor->v, orig_tor->v);
    VMOVE(edit_tor->h, orig_tor->h);
    MAT_IDN(s->acc_rot_sol);
    s->e_inpara = 1;
    VSET(s->e_para, 5, 5, 5);
    s->vp->gv_rotate_about = 'm';

    // set cmp vals to expected
    VSET(cmp_tor->v, 11.23303317584687910,4.16614044477699519,19.53105832935626651);
    VSET(cmp_tor->h, 0.57315612572226449,0.57695134825421179,0.58190995634607534);

    rt_edit_process(s);

    if (tor_diff("RT_PARAMS_EDIT_ROT (m)", cmp_tor, edit_tor))
	bu_exit(1, "ERROR: RT_PARAMS_EDIT_ROT(m) failed rotating tor\n");

    bu_log("RT_PARAMS_EDIT_ROT(m) SUCCESS: original v value %g,%g,%g modified to %g,%g,%g\n", V3ARGS(orig_tor->v), V3ARGS(edit_tor->v));
    bu_log("RT_PARAMS_EDIT_ROT(m) SUCCESS: original h value %g,%g,%g modified to %g,%g,%g\n", V3ARGS(orig_tor->h), V3ARGS(edit_tor->h));


    // Set rotation values - rotate about keypoint
    VMOVE(edit_tor->v, orig_tor->v);
    VMOVE(edit_tor->h, orig_tor->h);
    MAT_IDN(s->acc_rot_sol);
    s->e_inpara = 1;
    VSET(s->e_para, 5, 5, 5);
    s->vp->gv_rotate_about = 'k';
    VMOVE(s->e_keypoint, edit_tor->v);

    // set cmp vals to expected
    VMOVE(cmp_tor->v, orig_tor->v);
    VSET(cmp_tor->h, 0.57315612572226449,0.57695134825421179,0.58190995634607534);

    rt_edit_process(s);

    if (tor_diff("RT_PARAMS_EDIT_ROT (k)", cmp_tor, edit_tor))
	bu_exit(1, "ERROR: RT_PARAMS_EDIT_ROT(k) failed rotating tor\n");

    bu_log("RT_PARAMS_EDIT_ROT(k) SUCCESS: original h value %g,%g,%g modified to %g,%g,%g\n", V3ARGS(orig_tor->h), V3ARGS(edit_tor->h));


    // Test a rotation without mv_context
    s->mv_context = 0;

    VMOVE(edit_tor->v, orig_tor->v);
    VMOVE(edit_tor->h, orig_tor->h);
    MAT_IDN(s->acc_rot_sol);
    s->e_inpara = 1;
    VSET(s->e_para, 5, 5, 5);
    s->vp->gv_rotate_about = 'k';
    VMOVE(s->e_keypoint, edit_tor->v);

    // set cmp vals to expected
    VMOVE(cmp_tor->v, orig_tor->v);
    VSET(cmp_tor->h, 0.57315612572226449,0.57695134825421179,0.58190995634607534);

    rt_edit_process(s);

    if (tor_diff("RT_PARAMS_EDIT_ROT (k)", cmp_tor, edit_tor))
	bu_exit(1, "ERROR: RT_PARAMS_EDIT_ROT(k) failed rotating tor\n");

    bu_log("RT_PARAMS_EDIT_ROT(k) (mv_context == 0) SUCCESS: original h value %g,%g,%g modified to %g,%g,%g\n", V3ARGS(orig_tor->h), V3ARGS(edit_tor->h));


    /****************************
       ECMD_TOR_R1 - XY
     ****************************/
    EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, ECMD_TOR_R1);

    // Reset
    VMOVE(edit_tor->v, orig_tor->v);
    VMOVE(edit_tor->h, orig_tor->h);
    MAT_IDN(s->acc_rot_sol);
    s->acc_sc_sol = 1.0;
    s->e_inpara = 0;
    s->es_scale = 0;
    s->mv_context = 1;

    // Prepare mousevec.  xpos and ypos coordinates should be in the range of
    // BV_MIN <= val <= BV_MAX, which defines the outer limits of the pixel
    // screen from which mouse inputs would come.
    //
    // In essense, xpos and ypos are intended to simulate what a GUI toolkit
    // would provide us from a mouse cursor position.
    int xpos = 1372;
    int ypos = 1383;
    vect_t mousevec;  /* float pt -1..+1 mouse pos vect */
    /* map xpos and ypos to the -1 to +1 range*/
    mousevec[X] =  xpos * INV_BV;
    mousevec[Y] =  ypos * INV_BV;
    mousevec[Z] = 0;

    bu_vls_trunc(s->log_str, 0);
    if ((*EDOBJ[dp->d_minor_type].ft_edit_xy)(s, mousevec) == BRLCAD_ERROR)
	bu_exit(1, "ERROR: ECMD_TOR_R1 (xy) failed ft_edit_xy call: %s\n", bu_vls_cstr(s->log_str));


    // set cmp vals to expected
    VMOVE(cmp_tor->v, orig_tor->v);
    VMOVE(cmp_tor->h, orig_tor->h);
    cmp_tor->r_a = 23.37646484375000000;

    rt_edit_process(s);

    if (tor_diff("ECMD_TOR_R1 (xy)", cmp_tor, edit_tor))
	bu_exit(1, "ERROR: ECMD_TOR_R1(xy) failed scaling tor r1 param\n");

    bu_log("ECMD_TOR_R1(xy) SUCCESS: original r_a value %g modified to %g\n", orig_tor->r_a, edit_tor->r_a);


    /****************************
       RT_PARAMS_EDIT_TRANS - XY
     ****************************/
    EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, RT_PARAMS_EDIT_TRANS);

    // Reset
    VMOVE(edit_tor->v, orig_tor->v);
    VMOVE(edit_tor->h, orig_tor->h);
    edit_tor->r_a = orig_tor->r_a;
    MAT_IDN(s->acc_rot_sol);
    s->acc_sc_sol = 1.0;
    s->e_inpara = 0;
    s->es_scale = 0;

    // XY transform cares about curr_e_axes_pos
    VMOVE(s->curr_e_axes_pos, orig_tor->v);

    // Prepare mousevec.
    xpos = 1482;
    ypos = 762;
    /* map xpos and ypos to the -1 to +1 range*/
    mousevec[X] =  xpos * INV_BV;
    mousevec[Y] =  ypos * INV_BV;
    mousevec[Z] = 0;

    bu_vls_trunc(s->log_str, 0);
    if ((*EDOBJ[dp->d_minor_type].ft_edit_xy)(s, mousevec) == BRLCAD_ERROR)
	bu_exit(1, "ERROR: RT_PARAMS_EDIT_TRANS(xy) failed ft_edit_xy call: %s\n", bu_vls_cstr(s->log_str));

    // set cmp vals to expected
    VSET(cmp_tor->v, -12.61323935991339340,24.90340037137704243,22.73653941175349047);
    VMOVE(cmp_tor->h, orig_tor->h);
    cmp_tor->r_a = orig_tor->r_a;

    rt_edit_process(s);

    if (tor_diff("RT_PARAMS_EDIT_TRANS (xy)", cmp_tor, edit_tor))
	bu_exit(1, "ERROR: RT_PARAMS_EDIT_TRANS(xy) failed translating tor\n");

    bu_log("RT_PARAMS_EDIT_TRANS(xy) SUCCESS: original v value %g,%g,%g modified to %g,%g,%g\n", V3ARGS(orig_tor->v), V3ARGS(edit_tor->v));

    /****************************
       RT_PARAMS_EDIT_ROT - XY
     ****************************/
    EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, RT_PARAMS_EDIT_ROT);

    // Reset
    VMOVE(edit_tor->v, orig_tor->v);
    VMOVE(edit_tor->h, orig_tor->h);
    edit_tor->r_a = orig_tor->r_a;
    MAT_IDN(s->acc_rot_sol);
    MAT_IDN(s->incr_change);
    s->acc_sc_sol = 1.0;
    s->e_inpara = 0;
    s->es_scale = 0;
    VSET(s->e_keypoint, 0, 0, 0);
    s->mv_context = 1;

    // Not sure if we need to set these, strictly speaking, but just to be sure...
    s->vp->gv_coord = 'v';
    s->vp->gv_rotate_about = 'k';


    // The XY rotation logic in MGED is complex with a lot of options, and we
    // can't simply call ft_edit_xy to duplicate it (at least not at the
    // moment).  Instead, we reproduce the steps used in one of the possible
    // paths, starting with a pseudo-mouse input of delta x and y values as
    // would come from motion_event_handler in doevent.c:
    int dx = 6;
    int dy = -3;

    // 1.  Apply a rate factor.  This is one of the steps that can be impacted
    // by options in MGED - 0.25 is the value for the edit case of AMM_ROT when
    // mv_rateknobs isn't set.
    fastf_t fdx = dx * 0.25;
    fastf_t fdy = dy * 0.25;

    // 2.  Create a rate vector.  In MGED the setup of the arguments to the
    // knob command in doevent.c flips x and y, so do that here too.
    vect_t rvec = VINIT_ZERO;
    rvec[X] = fdy;
    rvec[Y] = fdx;

    // 3.  MGED will execute the knob command. That takes us to f_knob, which
    // in turn unpacks the options.  We don't need to do that here, and we're
    // not using the MGED state that f_knob updates, so we proceed directly to
    // the mged_edrot_xyz step of creating a new rotation matrix.
    mat_t newrot, temp1, temp2;
    MAT_IDN(newrot);
    bn_mat_angles(newrot, rvec[X], rvec[Y], rvec[Z]);

    // 4.  With the newrot matrix prepared, we proceed to mged_erot.  This is
    // another case where MGED settings can impact processing.  Per the
    // coords == 'v' case, incorporate the view's gv_rotation into newrot
    bn_mat_inv(temp1, s->vp->gv_rotation);
    bn_mat_mul(temp2, temp1, newrot);
    bn_mat_mul(newrot, temp2, s->vp->gv_rotation);

    // 5.  Set up the new rotation matrix in solid edit struct
    MAT_COPY(s->incr_change, newrot);

    // 6.  Incorporate acc_rot_sol (set to identity here, but as it is one of
    // the steps in mged_erot reproduce it for completeness.)
    bn_mat_mul2(s->incr_change, s->acc_rot_sol);

    // That completes our pseudo ft_edit_xy setup - now ready to process.
    rt_edit_process(s);

    // Set cmp vals to expected and do the diff
    VSET(cmp_tor->v, 9.4893742121206248, 5.2374047435024558, 20.18715850770182);
    VSET(cmp_tor->h, 0.55325065194604151, 0.59027005312777225, 0.58778821058423525);
    cmp_tor->r_a = orig_tor->r_a;

    if (tor_diff("RT_PARAMS_EDIT_ROT (xy)", cmp_tor, edit_tor))
	bu_exit(1, "ERROR: RT_PARAMS_EDIT_ROT(xy) failed translating tor\n");

    bu_log("RT_PARAMS_EDIT_ROT(xy) SUCCESS: original v value %g,%g,%g modified to %g,%g,%g\n", V3ARGS(orig_tor->v), V3ARGS(edit_tor->v));
    bu_log("RT_PARAMS_EDIT_ROT(xy) SUCCESS: original h value %g,%g,%g modified to %g,%g,%g\n", V3ARGS(orig_tor->h), V3ARGS(edit_tor->h));


    rt_edit_destroy(s);

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

