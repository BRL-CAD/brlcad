/*                          E B M . C P P
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
/** @file ebm.cpp
 *
 * Test editing of EBM (extruded bitmap) primitive parameters.
 *
 * Reference EBM: name="rt_edit_test_ebm.data", xdim=4, ydim=4,
 *   tallness=10.0mm, mat=IDN.
 *
 * A temporary data file with at least xdim*ydim=16 bytes is created
 * in /tmp, and dbi_filepath is set so the import can find it.
 *
 * ECMD_EBM_HEIGHT (e_inpara=1, e_para[0]=20):
 *   ecmd_ebm_height sets ebm->tallness = e_para[0] * local2base = 20.
 *
 * RT_PARAMS_EDIT_SCALE (es_scale=2, keypoint=(0,0,0)):
 *   edit_sscale → bn_mat_scale_about_pnt(2, keypoint=0) → mat[15]=0.5
 *   rt_ebm_mat: ebm->mat = mat * IDN → ebm->mat[15] = 0.5.
 *
 * RT_PARAMS_EDIT_TRANS (e_para=(5,5,5)):
 *   Translation applied to ebm->mat; mat[3,7,11] change.
 *
 * RT_PARAMS_EDIT_ROT (e_para=(5,5,5)):
 *   Rotation applied to ebm->mat.
 */

#include "common.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "vmath.h"
#include "bu/app.h"
#include "bu/log.h"
#include "bu/malloc.h"
#include "bu/str.h"
#include "raytrace.h"
#include "rt/rt_ecmds.h"


/* ECMD constants from edebm.c */
#define ECMD_EBM_FNAME		12053
#define ECMD_EBM_FSIZE		12054
#define ECMD_EBM_HEIGHT		12055


static const char *EBM_DATA_FILE = "/tmp/rt_edit_test_ebm.data";


static void
create_ebm_data_file(unsigned int xdim, unsigned int ydim)
{
    FILE *f = fopen(EBM_DATA_FILE, "wb");
    if (!f)
	bu_exit(1, "ERROR: Cannot create EBM data file %s\n", EBM_DATA_FILE);
    /* EBM data: xdim*ydim bytes (1 byte per pixel) */
    unsigned int n = xdim * ydim;
    unsigned char *buf = (unsigned char *)bu_calloc(n, 1, "ebm tmp");
    /* Set a checkerboard pattern so the data is non-trivial */
    for (unsigned int i = 0; i < n; i++)
	buf[i] = (unsigned char)((i % 2) ? 0xFF : 0x00);
    fwrite(buf, 1, n, f);
    bu_free(buf, "ebm tmp");
    fclose(f);
}


struct directory *
make_ebm(struct rt_wdb *wdbp)
{
    const char *objname = "ebm";
    unsigned int xdim = 4, ydim = 4;

    create_ebm_data_file(xdim, ydim);

    /* Set dbi_filepath so EBM can find the temp file in /tmp */
    /* db_close calls bu_argv_free(2, dbi_filepath), so use bu_malloc/bu_strdup */
    char **filepath = (char **)bu_malloc(3 * sizeof(char *), "dbi_filepath[3]");
    filepath[0] = bu_strdup(".");
    filepath[1] = bu_strdup("/tmp");
    filepath[2] = NULL;
    wdbp->dbip->dbi_filepath = filepath;

    struct rt_ebm_internal *ebm;
    BU_ALLOC(ebm, struct rt_ebm_internal);
    ebm->magic    = RT_EBM_INTERNAL_MAGIC;
    ebm->datasrc  = RT_EBM_SRC_FILE;
    ebm->xdim     = xdim;
    ebm->ydim     = ydim;
    ebm->tallness = 10.0;
    MAT_IDN(ebm->mat);
    ebm->buf = NULL;
    ebm->mp  = NULL;
    ebm->bip = NULL;
    bu_strlcpy(ebm->name, "rt_edit_test_ebm.data", RT_EBM_NAME_LEN);

    wdb_export(wdbp, objname, (void *)ebm, ID_EBM, 1.0);

    struct directory *dp = db_lookup(wdbp->dbip, objname, LOOKUP_QUIET);
    if (dp == RT_DIR_NULL)
	bu_exit(1, "ERROR: Unable to create ebm object\n");

    return dp;
}

static void
ebm_reset(struct rt_edit *s, struct rt_ebm_internal *edit_ebm)
{
    edit_ebm->tallness = 10.0;
    MAT_IDN(edit_ebm->mat);

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

    struct directory *dp = make_ebm(wdbp);

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

    struct rt_ebm_internal *edit_ebm =
	(struct rt_ebm_internal *)s->es_int.idb_ptr;

    vect_t mousevec;

    /* ================================================================
     * ECMD_EBM_HEIGHT (keyboard: e_inpara=1, e_para[0]=20)
     *
     * ecmd_ebm_height: ebm->tallness = e_para[0] * local2base = 20
     * ================================================================*/
    EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, ECMD_EBM_HEIGHT);
    s->e_inpara = 1;
    s->e_para[0] = 20.0;

    rt_edit_process(s);
    if (!NEAR_EQUAL(edit_ebm->tallness, 20.0, VUNITIZE_TOL))
	bu_exit(1, "ERROR: ECMD_EBM_HEIGHT failed: tallness=%g\n",
		edit_ebm->tallness);
    bu_log("ECMD_EBM_HEIGHT SUCCESS: tallness=%g\n", edit_ebm->tallness);

    /* ================================================================
     * ECMD_EBM_HEIGHT using es_scale (mouse-based)
     *
     * With e_inpara=0 and es_scale=1.5:
     *   ebm->tallness *= 1.5 = 10 * 1.5 = 15
     * ================================================================*/
    ebm_reset(s, edit_ebm);
    EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, ECMD_EBM_HEIGHT);
    s->e_inpara = 0;
    s->es_scale = 1.5;

    rt_edit_process(s);
    if (!NEAR_EQUAL(edit_ebm->tallness, 15.0, VUNITIZE_TOL))
	bu_exit(1, "ERROR: ECMD_EBM_HEIGHT(es_scale) failed: tallness=%g\n",
		edit_ebm->tallness);
    bu_log("ECMD_EBM_HEIGHT(es_scale) SUCCESS: tallness=%g\n", edit_ebm->tallness);

    /* ================================================================
     * RT_PARAMS_EDIT_SCALE (uniform scale=2, keypoint=(0,0,0))
     *
     * edit_sscale → bn_mat_scale_about_pnt(2, keypoint=0).
     * mat[15] = 1/2 = 0.5.
     * rt_ebm_mat: ebm->mat = mat * IDN → ebm->mat[15] = 0.5.
     * ================================================================*/
    ebm_reset(s, edit_ebm);
    EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, RT_PARAMS_EDIT_SCALE);
    s->e_inpara = 0;
    s->es_scale = 2.0;
    VSETALL(s->e_keypoint, 0.0);

    rt_edit_process(s);
    if (!NEAR_EQUAL(edit_ebm->mat[15], 0.5, VUNITIZE_TOL))
	bu_exit(1, "ERROR: RT_PARAMS_EDIT_SCALE failed: mat[15]=%g (expected 0.5)\n",
		edit_ebm->mat[15]);
    bu_log("RT_PARAMS_EDIT_SCALE SUCCESS: mat[15]=%g (encodes scale=2)\n",
	   edit_ebm->mat[15]);

    /* ================================================================
     * RT_PARAMS_EDIT_TRANS (translate; keypoint (0,0,0) → e_para=(5,5,5))
     *
     * edit_stra: translation matrix applied to ebm->mat.
     * ebm->mat[3,7,11] change.
     * ================================================================*/
    ebm_reset(s, edit_ebm);
    EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, RT_PARAMS_EDIT_TRANS);
    s->e_inpara = 1;
    VSET(s->e_para, 5, 5, 5);
    VSETALL(s->e_keypoint, 0.0);

    {
	mat_t orig_mat;
	MAT_COPY(orig_mat, edit_ebm->mat);
	rt_edit_process(s);
	int changed = 0;
	for (int i = 0; i < 16; i++) {
	    if (!NEAR_EQUAL(edit_ebm->mat[i], orig_mat[i], VUNITIZE_TOL))
		changed = 1;
	}
	if (!changed)
	    bu_exit(1, "ERROR: RT_PARAMS_EDIT_TRANS did not change ebm->mat\n");
    }
    bu_log("RT_PARAMS_EDIT_TRANS SUCCESS: mat[3]=%g mat[7]=%g mat[11]=%g\n",
	   edit_ebm->mat[3], edit_ebm->mat[7], edit_ebm->mat[11]);

    /* ================================================================
     * RT_PARAMS_EDIT_ROT XY returns BRLCAD_ERROR (unimplemented)
     * ================================================================*/
    ebm_reset(s, edit_ebm);
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
    ebm_reset(s, edit_ebm);
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
    ebm_reset(s, edit_ebm);
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
