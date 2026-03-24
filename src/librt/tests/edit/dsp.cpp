/*                          D S P . C P P
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
/** @file dsp.cpp
 *
 * Test editing of DSP (displaced surface) primitive parameters.
 *
 * Reference DSP: dsp_stom = IDN, dsp_mtos = IDN, dsp_xcnt=8, dsp_ycnt=8.
 * No data file; dsp_buf=NULL (matrix edit operations don't need data).
 *
 * ECMD_DSP_SCALE_X (e_inpara=1, e_para[0]=2):
 *   dsp_scale sets m=IDN, m[MSX=0]=2, then applies xform about keypoint.
 *   With keypoint=(0,0,0): scalemat = m.
 *   dsp_stom = dsp_stom * scalemat → dsp_stom[0] = 2.
 *
 * ECMD_DSP_SCALE_Y (e_inpara=1, e_para[0]=3):
 *   m[MSY=5]=3 → dsp_stom[5] = 3.
 *
 * ECMD_DSP_SCALE_ALT (e_inpara=1, e_para[0]=4):
 *   m[MSZ=10]=4 → dsp_stom[10] = 4.
 *
 * RT_PARAMS_EDIT_SCALE: edit_sscale with es_scale=2 calls
 *   bn_mat_scale_about_pnt(mat, keypoint, 2) → IDN*2 matrix,
 *   rt_dsp_mat applies it to dsp_stom.
 *
 * RT_PARAMS_EDIT_TRANS: edit_stra translates dsp_stom.
 * RT_PARAMS_EDIT_ROT: edit_srot rotates dsp_stom.
 */

#include "common.h"

#include <math.h>
#include <string.h>

#include "vmath.h"
#include "bu/app.h"
#include "bu/log.h"
#include "bu/malloc.h"
#include "bu/str.h"
#include "bu/vls.h"
#include "raytrace.h"
#include "rt/rt_ecmds.h"


/* ECMD constants from eddsp.c */
#define ECMD_DSP_FNAME		25056
#define ECMD_DSP_FSIZE		25057
#define ECMD_DSP_SCALE_X        25058
#define ECMD_DSP_SCALE_Y        25059
#define ECMD_DSP_SCALE_ALT      25060
#define ECMD_DSP_SET_SMOOTH     25061
#define ECMD_DSP_SET_DATASRC    25062


static const char *DSP_DATA_FILE = "/tmp/rt_edit_test_dsp.data";

static void
create_dsp_data_file(unsigned int xcnt, unsigned int ycnt)
{
    FILE *f = fopen(DSP_DATA_FILE, "wb");
    if (!f)
	bu_exit(1, "ERROR: Cannot create DSP data file %s\n", DSP_DATA_FILE);
    size_t n = xcnt * ycnt;
    unsigned short *buf = (unsigned short *)bu_calloc(n, sizeof(unsigned short), "dsp tmp");
    for (size_t i = 0; i < n; i++)
	buf[i] = (unsigned short)(i % 256);
    fwrite(buf, sizeof(unsigned short), n, f);
    bu_free(buf, "dsp tmp");
    fclose(f);
}


struct directory *
make_dsp(struct rt_wdb *wdbp)
{
    const char *objname = "dsp";
    unsigned int xcnt = 8, ycnt = 8;

    create_dsp_data_file(xcnt, ycnt);

    /* Set dbi_filepath so DSP can find the temp file in /tmp */
    /* Matches format expected by db_close: bu_argv_free(2, ...) */
    char **filepath = (char **)bu_malloc(3 * sizeof(char *), "dbi_filepath[3]");
    filepath[0] = bu_strdup(".");
    filepath[1] = bu_strdup("/tmp");
    filepath[2] = NULL;
    wdbp->dbip->dbi_filepath = filepath;

    struct rt_dsp_internal *dsp;
    BU_ALLOC(dsp, struct rt_dsp_internal);
    dsp->magic       = RT_DSP_INTERNAL_MAGIC;
    dsp->dsp_xcnt    = xcnt;
    dsp->dsp_ycnt    = ycnt;
    dsp->dsp_smooth  = 1;
    dsp->dsp_cuttype = DSP_CUT_DIR_llUR;
    dsp->dsp_datasrc = RT_DSP_SRC_FILE;
    dsp->dsp_buf     = NULL;
    dsp->dsp_mp      = NULL;
    dsp->dsp_bip     = NULL;
    MAT_IDN(dsp->dsp_stom);
    MAT_IDN(dsp->dsp_mtos);
    BU_VLS_INIT(&dsp->dsp_name);
    /* Use just the filename (not the /tmp/ path) so bu_open_mapped_file_with_path can find it */
    bu_vls_strcpy(&dsp->dsp_name, "rt_edit_test_dsp.data");

    wdb_export(wdbp, objname, (void *)dsp, ID_DSP, 1.0);

    struct directory *dp = db_lookup(wdbp->dbip, objname, LOOKUP_QUIET);
    if (dp == RT_DIR_NULL)
	bu_exit(1, "ERROR: Unable to create dsp object\n");

    return dp;
}

static void
dsp_reset(struct rt_edit *s, struct rt_dsp_internal *edit_dsp)
{
    MAT_IDN(edit_dsp->dsp_stom);
    MAT_IDN(edit_dsp->dsp_mtos);

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

    struct directory *dp = make_dsp(wdbp);

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

    struct rt_dsp_internal *edit_dsp =
	(struct rt_dsp_internal *)s->es_int.idb_ptr;

    vect_t mousevec;

    /* ================================================================
     * ECMD_DSP_SCALE_X (keyboard: e_inpara=1, e_para[0]=2)
     *
     * dsp_scale(MSX=0):
     *   m = IDN; m[0] = 2; scalemat = m (keypoint=0)
     *   dsp_stom = IDN * scalemat → dsp_stom[0] = 2
     * ================================================================*/
    EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, ECMD_DSP_SCALE_X);
    s->e_inpara = 1;
    s->e_para[0] = 2.0;
    VSETALL(s->e_keypoint, 0.0);

    rt_edit_process(s);
    if (!NEAR_EQUAL(edit_dsp->dsp_stom[MSX], 2.0, VUNITIZE_TOL))
	bu_exit(1, "ERROR: ECMD_DSP_SCALE_X failed: dsp_stom[0]=%g\n",
		edit_dsp->dsp_stom[MSX]);
    bu_log("ECMD_DSP_SCALE_X SUCCESS: dsp_stom[0]=%g\n", edit_dsp->dsp_stom[MSX]);

    /* ================================================================
     * ECMD_DSP_SCALE_Y (keyboard: e_inpara=1, e_para[0]=3)
     *
     * dsp_scale(MSY=5):
     *   m = IDN; m[5] = 3; scalemat = m (keypoint=0)
     *   dsp_stom[current] = dsp_stom * scalemat
     *   dsp_stom[5] = dsp_stom[5] * 3 = 1 * 3 = 3
     * But after ECMD_DSP_SCALE_X, dsp_stom[0]=2, [5]=1, so dsp_stom[5]=3 after.
     * ================================================================*/
    dsp_reset(s, edit_dsp);
    EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, ECMD_DSP_SCALE_Y);
    s->e_inpara = 1;
    s->e_para[0] = 3.0;
    VSETALL(s->e_keypoint, 0.0);

    rt_edit_process(s);
    if (!NEAR_EQUAL(edit_dsp->dsp_stom[MSY], 3.0, VUNITIZE_TOL))
	bu_exit(1, "ERROR: ECMD_DSP_SCALE_Y failed: dsp_stom[5]=%g\n",
		edit_dsp->dsp_stom[MSY]);
    bu_log("ECMD_DSP_SCALE_Y SUCCESS: dsp_stom[5]=%g\n", edit_dsp->dsp_stom[MSY]);

    /* ================================================================
     * ECMD_DSP_SCALE_ALT (keyboard: e_inpara=1, e_para[0]=4)
     *
     * dsp_scale(MSZ=10):
     *   m = IDN; m[10] = 4; scalemat = m
     *   dsp_stom[10] = 4
     * ================================================================*/
    dsp_reset(s, edit_dsp);
    EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, ECMD_DSP_SCALE_ALT);
    s->e_inpara = 1;
    s->e_para[0] = 4.0;
    VSETALL(s->e_keypoint, 0.0);

    rt_edit_process(s);
    if (!NEAR_EQUAL(edit_dsp->dsp_stom[MSZ], 4.0, VUNITIZE_TOL))
	bu_exit(1, "ERROR: ECMD_DSP_SCALE_ALT failed: dsp_stom[10]=%g\n",
		edit_dsp->dsp_stom[MSZ]);
    bu_log("ECMD_DSP_SCALE_ALT SUCCESS: dsp_stom[10]=%g\n", edit_dsp->dsp_stom[MSZ]);

    /* ================================================================
     * ECMD_DSP_SCALE_X using es_scale (mouse)
     *
     * With e_inpara=0 and es_scale=1.5:
     *   m[MSX] *= es_scale → m[0] = 1.0 * 1.5 = 1.5
     *   dsp_stom[0] = 1.5
     * ================================================================*/
    dsp_reset(s, edit_dsp);
    EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, ECMD_DSP_SCALE_X);
    s->e_inpara = 0;
    s->es_scale = 1.5;
    VSETALL(s->e_keypoint, 0.0);

    rt_edit_process(s);
    if (!NEAR_EQUAL(edit_dsp->dsp_stom[MSX], 1.5, VUNITIZE_TOL))
	bu_exit(1, "ERROR: ECMD_DSP_SCALE_X(es_scale) failed: dsp_stom[0]=%g\n",
		edit_dsp->dsp_stom[MSX]);
    bu_log("ECMD_DSP_SCALE_X(es_scale) SUCCESS: dsp_stom[0]=%g\n",
	   edit_dsp->dsp_stom[MSX]);

    /* ================================================================
     * RT_PARAMS_EDIT_SCALE (uniform scale=2 about (0,0,0))
     *
     * edit_sscale calls bn_mat_scale_about_pnt(mat, keypoint, 2).
     * With keypoint=(0,0,0): mat = IDN with mat[15] = 1/2 = 0.5.
     * rt_dsp_mat does: new_stom = mat * old_stom = mat * IDN = mat.
     * So dsp_stom[15] = 0.5 (encodes the scale factor as 1/s).
     * (Geometric effect: MAT4X3PNT divides by mat[15]=0.5, scaling by 2.)
     * ================================================================*/
    dsp_reset(s, edit_dsp);
    EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, RT_PARAMS_EDIT_SCALE);
    s->e_inpara = 0;
    s->es_scale = 2.0;
    VSETALL(s->e_keypoint, 0.0);

    rt_edit_process(s);
    {
	/* dsp_stom[15] should be 1/scale = 0.5 */
	if (!NEAR_EQUAL(edit_dsp->dsp_stom[15], 0.5, VUNITIZE_TOL))
	    bu_exit(1, "ERROR: RT_PARAMS_EDIT_SCALE failed: dsp_stom[15]=%g (expected 0.5)\n",
		    edit_dsp->dsp_stom[15]);
    }
    bu_log("RT_PARAMS_EDIT_SCALE SUCCESS: dsp_stom[15]=%g (encodes scale factor 2)\n",
	   edit_dsp->dsp_stom[15]);

    /* ================================================================
     * RT_PARAMS_EDIT_TRANS (translate; keypoint (0,0,0) → e_para=(5,5,5))
     *
     * edit_stra calls rt_dsp_mat with a translation matrix.
     * For dsp_stom=IDN, dsp_stom becomes the translation matrix.
     * The translation columns of dsp_stom (indices 3,7,11) change.
     * ================================================================*/
    dsp_reset(s, edit_dsp);
    EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, RT_PARAMS_EDIT_TRANS);
    s->e_inpara = 1;
    VSET(s->e_para, 5, 5, 5);
    VSETALL(s->e_keypoint, 0.0);

    {
	/* Save stom before to check it changed */
	mat_t orig_stom;
	MAT_COPY(orig_stom, edit_dsp->dsp_stom);
	rt_edit_process(s);
	/* Check if any element changed */
	int changed = 0;
	for (int i = 0; i < 16; i++) {
	    if (!NEAR_EQUAL(edit_dsp->dsp_stom[i], orig_stom[i], VUNITIZE_TOL))
		changed = 1;
	}
	if (!changed)
	    bu_exit(1, "ERROR: RT_PARAMS_EDIT_TRANS did not change dsp_stom\n");
    }
    bu_log("RT_PARAMS_EDIT_TRANS SUCCESS: dsp_stom[3]=%g dsp_stom[7]=%g dsp_stom[11]=%g\n",
	   edit_dsp->dsp_stom[3], edit_dsp->dsp_stom[7], edit_dsp->dsp_stom[11]);

    /* ================================================================
     * RT_PARAMS_EDIT_ROT XY returns BRLCAD_ERROR (unimplemented)
     * ================================================================*/
    dsp_reset(s, edit_dsp);
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
    dsp_reset(s, edit_dsp);
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
    dsp_reset(s, edit_dsp);
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

    /* ================================================================
     * ECMD_DSP_SET_SMOOTH: set and clear smooth normals flag
     * ================================================================*/
    {
	struct rt_dsp_internal *dsp2 =
	    (struct rt_dsp_internal *)s->es_int.idb_ptr;

	/* Set to 1 explicitly */
	dsp2->dsp_smooth = 0;
	EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, ECMD_DSP_SET_SMOOTH);
	s->e_inpara = 1;
	s->e_para[0] = 1.0;
	bu_vls_trunc(s->log_str, 0);
	rt_edit_process(s);
	if (dsp2->dsp_smooth != 1)
	    bu_exit(1, "ERROR: ECMD_DSP_SET_SMOOTH set=1: smooth=%u\n",
		    dsp2->dsp_smooth);
	bu_log("ECMD_DSP_SET_SMOOTH set=1 SUCCESS\n");

	/* Set explicitly to 0 */
	EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, ECMD_DSP_SET_SMOOTH);
	s->e_inpara = 1;
	s->e_para[0] = 0.0;
	bu_vls_trunc(s->log_str, 0);
	rt_edit_process(s);
	if (dsp2->dsp_smooth != 0)
	    bu_exit(1, "ERROR: ECMD_DSP_SET_SMOOTH set=0: smooth=%u\n",
		    dsp2->dsp_smooth);
	bu_log("ECMD_DSP_SET_SMOOTH set=0 SUCCESS\n");
    }

    /* ================================================================
     * ECMD_DSP_SET_DATASRC: switch from file to object data source
     * ================================================================*/
    {
	struct rt_dsp_internal *dsp3 =
	    (struct rt_dsp_internal *)s->es_int.idb_ptr;

	EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, ECMD_DSP_SET_DATASRC);
	s->e_inpara = 1;
	s->e_para[0] = 1.0;   /* 1 => RT_DSP_SRC_OBJ */
	bu_vls_trunc(s->log_str, 0);
	rt_edit_process(s);
	if (dsp3->dsp_datasrc != RT_DSP_SRC_OBJ)
	    bu_exit(1, "ERROR: ECMD_DSP_SET_DATASRC: expected 'o', got '%c'\n",
		    dsp3->dsp_datasrc);
	bu_log("ECMD_DSP_SET_DATASRC SUCCESS: datasrc='%c'\n",
	       dsp3->dsp_datasrc);
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
