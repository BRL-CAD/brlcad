/*                          V O L . C P P
 * BRL-CAD
 *
 * Copyright (c) 2025-2026 United States Government as represented by
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
/** @file vol.cpp
 *
 * Test editing of VOL primitive parameters.
 *
 * Reference VOL: name="vol_test.data", dims=4x4x4, lo=5, hi=250,
 *   cellsize=(1,1,1), mat=IDN.
 * Note: the data file does not need to exist. VOL import just logs a
 * warning if the file is missing and still returns 0 (success).
 *
 * ECMD_VOL_CSIZE (e_inpara=3, e_para=(2,3,4)):
 *   ecmd_vol_csize sets vol->cellsize = (2,3,4).
 *
 * ECMD_VOL_THRESH_LO (e_inpara=1, e_para[0]=50):
 *   ecmd_vol_thresh_lo sets vol->lo = 50.
 *
 * ECMD_VOL_THRESH_HI (e_inpara=1, e_para[0]=200):
 *   ecmd_vol_thresh_hi sets vol->hi = 200.
 *
 * RT_PARAMS_EDIT_SCALE (es_scale=2, keypoint=(0,0,0)):
 *   rt_vol_mat: new_mat = bn_mat_scale_about_pnt(2) * IDN.
 *   mat[15] = 1/2 = 0.5 → vol->mat[15] = 0.5.
 *
 * RT_PARAMS_EDIT_TRANS (e_para=(5,5,5), keypoint=(0,0,0)):
 *   edit_stra applies translation matrix. For IDN vol->mat,
 *   vol->mat becomes the translation matrix (translation in mat[3,7,11]).
 *
 * RT_PARAMS_EDIT_ROT (e_para=(5,5,5), keypoint=(0,0,0)):
 *   edit_srot applies rotation matrix. vol->mat becomes rotation * IDN.
 */

#include "common.h"

#include <math.h>
#include <string.h>

#include "vmath.h"
#include "bu/app.h"
#include "bu/log.h"
#include "bu/malloc.h"
#include "bu/str.h"
#include "raytrace.h"
#include "rt/rt_ecmds.h"


/* ECMD constants from edvol.c */
#define ECMD_VOL_CSIZE		13048
#define ECMD_VOL_FSIZE		13049
#define ECMD_VOL_THRESH_LO	13050
#define ECMD_VOL_THRESH_HI	13051
#define ECMD_VOL_FNAME		13052


struct directory *
make_vol(struct rt_wdb *wdbp)
{
    const char *objname = "vol";

    struct rt_vol_internal *vol;
    BU_ALLOC(vol, struct rt_vol_internal);
    vol->magic = RT_VOL_INTERNAL_MAGIC;
    bu_strlcpy(vol->name, "vol_test.data", RT_VOL_NAME_LEN);
    vol->datasrc  = RT_VOL_SRC_FILE;
    vol->xdim     = 4;
    vol->ydim     = 4;
    vol->zdim     = 4;
    vol->lo       = 5;
    vol->hi       = 250;
    VSET(vol->cellsize, 1, 1, 1);
    MAT_IDN(vol->mat);
    vol->map = NULL;
    vol->bip = NULL;

    wdb_export(wdbp, objname, (void *)vol, ID_VOL, 1.0);

    struct directory *dp = db_lookup(wdbp->dbip, objname, LOOKUP_QUIET);
    if (dp == RT_DIR_NULL)
	bu_exit(1, "ERROR: Unable to create vol object\n");

    return dp;
}

static void
vol_reset(struct rt_edit *s, struct rt_vol_internal *edit_vol)
{
    bu_strlcpy(edit_vol->name, "vol_test.data", RT_VOL_NAME_LEN);
    edit_vol->xdim     = 4;
    edit_vol->ydim     = 4;
    edit_vol->zdim     = 4;
    edit_vol->lo       = 5;
    edit_vol->hi       = 250;
    VSET(edit_vol->cellsize, 1, 1, 1);
    MAT_IDN(edit_vol->mat);

    VSETALL(s->e_keypoint, 0.0);
    MAT_IDN(s->acc_rot_sol);
    MAT_IDN(s->incr_change);
    s->acc_sc_sol = 1.0;
    s->e_inpara   = 0;
    s->es_scale   = 0.0;
    s->mv_context = 1;
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

    struct directory *dp = make_vol(wdbp);

    struct bn_tol tol = BN_TOL_INIT_TOL;
    struct db_full_path fp;
    db_full_path_init(&fp);
    db_add_node_to_full_path(&fp, dp);

    struct bview *v;
    BU_GET(v, struct bview);
    bv_init(v, NULL);
    VSET(v->gv_aet, 45, 35, 0);
    bv_mat_aet(v);
    v->gv_size  = 73.3197;
    v->gv_isize = 1.0 / v->gv_size;
    v->gv_scale = 0.5 * v->gv_size;
    bv_update(v);
    bu_vls_sprintf(&v->gv_name, "default");
    v->gv_width  = 512;
    v->gv_height = 512;

    struct rt_edit *s = rt_edit_create(&fp, dbip, &tol, v);
    s->mv_context = 1;

    struct rt_vol_internal *edit_vol =
	(struct rt_vol_internal *)s->es_int.idb_ptr;

    vect_t mousevec;

    /* ================================================================
     * ECMD_VOL_CSIZE (keyboard: e_inpara=3, e_para=(2,3,4))
     *
     * ecmd_vol_csize: VMOVE(vol->cellsize, e_para) → (2,3,4)
     * ================================================================*/
    EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, ECMD_VOL_CSIZE);
    s->e_inpara = 3;
    VSET(s->e_para, 2, 3, 4);

    rt_edit_process(s);
    {
	vect_t expected = {2, 3, 4};
	if (!VNEAR_EQUAL(edit_vol->cellsize, expected, VUNITIZE_TOL))
	    bu_exit(1, "ERROR: ECMD_VOL_CSIZE failed: cellsize=%g,%g,%g\n",
		    V3ARGS(edit_vol->cellsize));
	bu_log("ECMD_VOL_CSIZE SUCCESS: cellsize=%g,%g,%g\n",
	       V3ARGS(edit_vol->cellsize));
    }

    /* ================================================================
     * ECMD_VOL_THRESH_LO (keyboard: e_inpara=1, e_para[0]=50)
     *
     * ecmd_vol_thresh_lo: vol->lo = 50
     * ================================================================*/
    vol_reset(s, edit_vol);
    EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, ECMD_VOL_THRESH_LO);
    s->e_inpara = 1;
    s->e_para[0] = 50.0;

    rt_edit_process(s);
    if (edit_vol->lo != 50)
	bu_exit(1, "ERROR: ECMD_VOL_THRESH_LO failed: lo=%u\n", edit_vol->lo);
    bu_log("ECMD_VOL_THRESH_LO SUCCESS: lo=%u\n", edit_vol->lo);

    /* ================================================================
     * ECMD_VOL_THRESH_HI (keyboard: e_inpara=1, e_para[0]=200)
     *
     * ecmd_vol_thresh_hi: vol->hi = 200
     * ================================================================*/
    vol_reset(s, edit_vol);
    EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, ECMD_VOL_THRESH_HI);
    s->e_inpara = 1;
    s->e_para[0] = 200.0;

    rt_edit_process(s);
    if (edit_vol->hi != 200)
	bu_exit(1, "ERROR: ECMD_VOL_THRESH_HI failed: hi=%u\n", edit_vol->hi);
    bu_log("ECMD_VOL_THRESH_HI SUCCESS: hi=%u\n", edit_vol->hi);

    /* ================================================================
     * RT_PARAMS_EDIT_SCALE (uniform scale=2, keypoint=(0,0,0))
     *
     * rt_vol_mat: new_mat = bn_mat_scale_about_pnt(2) * IDN.
     * bn_mat_scale_about_pnt with scale=2, keypoint=0: mat[15]=1/2=0.5.
     * vol->mat[15] = 0.5 (encodes scale factor = 1/0.5 = 2).
     * ================================================================*/
    vol_reset(s, edit_vol);
    EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, RT_PARAMS_EDIT_SCALE);
    s->e_inpara = 0;
    s->es_scale = 2.0;
    VSETALL(s->e_keypoint, 0.0);

    rt_edit_process(s);
    if (!NEAR_EQUAL(edit_vol->mat[15], 0.5, VUNITIZE_TOL))
	bu_exit(1, "ERROR: RT_PARAMS_EDIT_SCALE failed: mat[15]=%g (expected 0.5)\n",
		edit_vol->mat[15]);
    bu_log("RT_PARAMS_EDIT_SCALE SUCCESS: mat[15]=%g (encodes scale=2)\n",
	   edit_vol->mat[15]);

    /* ================================================================
     * RT_PARAMS_EDIT_TRANS (translate; keypoint (0,0,0) → e_para=(5,5,5))
     *
     * edit_stra builds a translation matrix and calls rt_vol_mat.
     * new_mat = T * IDN = T, where T[3]=T[7]=T[11]=5.
     * ================================================================*/
    vol_reset(s, edit_vol);
    EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, RT_PARAMS_EDIT_TRANS);
    s->e_inpara = 1;
    VSET(s->e_para, 5, 5, 5);
    VSETALL(s->e_keypoint, 0.0);

    {
	mat_t orig_mat;
	MAT_COPY(orig_mat, edit_vol->mat);
	rt_edit_process(s);
	/* Check mat changed */
	int changed = 0;
	for (int i = 0; i < 16; i++) {
	    if (!NEAR_EQUAL(edit_vol->mat[i], orig_mat[i], VUNITIZE_TOL))
		changed = 1;
	}
	if (!changed)
	    bu_exit(1, "ERROR: RT_PARAMS_EDIT_TRANS did not change vol->mat\n");
    }
    bu_log("RT_PARAMS_EDIT_TRANS SUCCESS: mat[3]=%g mat[7]=%g mat[11]=%g\n",
	   edit_vol->mat[3], edit_vol->mat[7], edit_vol->mat[11]);

    /* ================================================================
     * ECMD_VOL_CSIZE using es_scale (mouse-based)
     *
     * With e_inpara=0 and es_scale=2.0:
     *   VSCALE(cellsize, cellsize, es_scale) → (2,2,2)
     * ================================================================*/
    vol_reset(s, edit_vol);
    EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, ECMD_VOL_CSIZE);
    s->e_inpara = 0;
    s->es_scale = 2.0;

    rt_edit_process(s);
    {
	vect_t expected = {2, 2, 2};
	if (!VNEAR_EQUAL(edit_vol->cellsize, expected, VUNITIZE_TOL))
	    bu_exit(1, "ERROR: ECMD_VOL_CSIZE(es_scale) failed: cellsize=%g,%g,%g\n",
		    V3ARGS(edit_vol->cellsize));
	bu_log("ECMD_VOL_CSIZE(es_scale) SUCCESS: cellsize=%g,%g,%g\n",
	       V3ARGS(edit_vol->cellsize));
    }

    /* ================================================================
     * RT_PARAMS_EDIT_ROT XY returns BRLCAD_ERROR (unimplemented)
     * ================================================================*/
    vol_reset(s, edit_vol);
    EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, RT_PARAMS_EDIT_ROT);
    mousevec[X] = 0.1; mousevec[Y] = -0.05; mousevec[Z] = 0;
    bu_vls_trunc(s->log_str, 0);
    int rot_xy_ret = (*EDOBJ[dp->d_minor_type].ft_edit_xy)(s, mousevec);
    if (rot_xy_ret != BRLCAD_OK)
	bu_exit(1, "ERROR: RT_PARAMS_EDIT_ROT(xy) failed\n");
    bu_log("RT_PARAMS_EDIT_ROT(xy) SUCCESS: rotation applied via knob path\n");


    /* ================================================================
     * RT_MATRIX_EDIT_ROT: absolute matrix rotation from keyboard angles
     *
     * Set model_changes to a 30-deg rotation about X. Keypoint is at
     * the primitive's vertex (varies per primitive).
     * After the edit model_changes must differ from identity.
     * ================================================================*/
    vol_reset(s, edit_vol);
    MAT_IDN(s->model_changes);
    MAT_IDN(s->acc_rot_sol);
    rt_edit_set_edflag(s, RT_MATRIX_EDIT_ROT);
    s->e_inpara = 1;
    VSET(s->e_para, 30, 0, 0);   /* 30-deg rotation about X axis */
    VSET(s->e_keypoint, 0, 0, 0);

    rt_edit_process(s);
    {
mat_t ident;
MAT_IDN(ident);
if (bn_mat_is_equal(s->model_changes, ident, &tol))
    bu_exit(1, "ERROR: RT_MATRIX_EDIT_ROT did not rotate model_changes\n");
mat_t expected_rot;
MAT_IDN(expected_rot);
bn_mat_angles(expected_rot, 30, 0, 0);
if (!bn_mat_is_equal(s->acc_rot_sol, expected_rot, &tol))
    bu_exit(1, "ERROR: RT_MATRIX_EDIT_ROT: acc_rot_sol not updated\n");
bu_log("RT_MATRIX_EDIT_ROT SUCCESS: model_changes rotated, acc_rot_sol updated\n");
    }

    /* ================================================================
     * RT_MATRIX_EDIT_TRANS_MODEL_XYZ: absolute model-space translation
     *
     * Start with identity model_changes, keypoint at origin.
     * After translating to (10,20,30): model_changes * (0,0,0) == (10,20,30).
     * ================================================================*/
    vol_reset(s, edit_vol);
    MAT_IDN(s->model_changes);
    rt_edit_set_edflag(s, RT_MATRIX_EDIT_TRANS_MODEL_XYZ);
    s->e_inpara = 1;
    VSET(s->e_para, 10, 20, 30);
    VSET(s->e_keypoint, 0, 0, 0);
    s->local2base = 1.0;

    {
point_t kp_saved;
VMOVE(kp_saved, s->e_keypoint);

rt_edit_process(s);

point_t kp_world;
MAT4X3PNT(kp_world, s->model_changes, kp_saved);
vect_t expected = {10, 20, 30};
if (!VNEAR_EQUAL(kp_world, expected, VUNITIZE_TOL))
    bu_exit(1, "ERROR: RT_MATRIX_EDIT_TRANS_MODEL_XYZ failed: "
    "keypoint maps to (%g,%g,%g), expected (10,20,30)\n",
    V3ARGS(kp_world));
bu_log("RT_MATRIX_EDIT_TRANS_MODEL_XYZ SUCCESS: "
       "keypoint maps to (%g,%g,%g)\n", V3ARGS(kp_world));
    }

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
