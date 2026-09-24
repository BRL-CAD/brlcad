/*                         A R B 8 . C P P
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
/** @file arb8.cpp
 *
 * Test editing of ARB primitive parameters.
 *
 * Reference ARB8: unit cube, pt[0]=(0,0,0)..pt[7]=(1,1,1).
 * Keypoint = pt[0] = (0,0,0) = V1.
 *
 * rt_arb_mat: applies MAT4X3PNT to each vertex.
 *
 * RT_PARAMS_EDIT_SCALE (scale=2, keypoint=pt[0]=(0,0,0)):
 *   bn_mat_scale_about_pnt(mat, (0,0,0), 2) → mat[15]=0.5
 *   MAT4X3PNT(out, mat, in) = in * (1/0.5) = in * 2
 *   pt[0]=(0,0,0)→(0,0,0), pt[6]=(1,1,1)→(2,2,2)
 *
 * RT_PARAMS_EDIT_TRANS (e_para=(5,5,5)):
 *   Translation shifts all vertices by (5,5,5).
 *   pt[0]=(0,0,0)→(5,5,5), pt[6]=(1,1,1)→(6,6,6)
 *
 * RT_PARAMS_EDIT_ROT (e_para=(5,5,5), keypoint=pt[0]=(0,0,0)):
 *   Rotation about V1. pt[0] stays at origin.
 *   pt[6]=(1,1,1) rotates to R*(1,1,1).
 *
 * PTARB (move vertex pt[6] to (3,3,3)):
 *   edit_arb_element with e_inpara=3, e_para=(3,3,3), edit_menu=6.
 *   pt[6] → (3,3,3)
 *
 * EARB (move edge 0 to x-position +2):
 *   Tests ARB edge editing dispatch.
 */

#include "common.h"

#include <math.h>
#include <string.h>

#include "vmath.h"
#include "bu/log.h"
#include "bu/malloc.h"
#include "bu/str.h"
#include "raytrace.h"
#include "rt/db4.h"
#include "rt/rt_ecmds.h"
#include "rt/primitives/arb8.h"
#include "test_utils.h"


/* ECMD constants from edarb.c */
#define EARB		4009
#define PTARB		4010
#define ECMD_ARB_MAIN_MENU	4011
#define ECMD_ARB_SPECIFIC_MENU	4012
#define ECMD_ARB_MOVE_FACE	4013
#define ECMD_ARB_SETUP_ROTFACE	4014
#define ECMD_ARB_ROTATE_FACE	4015

static const char move_edges_menu_label[] = "Move Edges";


enum {
    ARB8_FACE_COUNT = 6,
    ARB8_VERTEX_COUNT = 8,
    ARB8_EDIT_COUNT = 12
};


struct menu_capture {
    struct rt_edit_menu_item *items;
};


static int
capture_menu(int UNUSED(argc), const char **UNUSED(argv), void *data, void *menu)
{
    struct menu_capture *capture = (struct menu_capture *)data;

    if (!capture)
	return BRLCAD_ERROR;

    capture->items = (struct rt_edit_menu_item *)menu;
    return BRLCAD_OK;
}


static struct rt_edit_menu_item *
find_menu_item(struct rt_edit_menu_item *menu, const char *label)
{
    if (!menu || !label)
	return NULL;

    for (struct rt_edit_menu_item *item = menu;
	 item->menu_string && item->menu_string[0] != '\0'; item++) {
	if (BU_STR_EQUAL(item->menu_string, label))
	    return item;
    }

    return NULL;
}


static void
select_arb_point_edit(struct rt_edit *s, struct rt_arb8_edit *a,
	struct menu_capture *capture, const char *point_label)
{
    struct rt_edit_menu_item *control_menu =
	(*EDOBJ[s->es_int.idb_type].ft_menu_item)(s->tol);
    struct rt_edit_menu_item *move_edges =
	find_menu_item(control_menu, move_edges_menu_label);
    if (!move_edges || !move_edges->menu_func)
	bu_exit(1, "ERROR: ARB control menu has no '%s' entry\n",
		move_edges_menu_label);

    capture->items = NULL;
    (*move_edges->menu_func)(s, move_edges->menu_arg, 0, 0, NULL);

    struct rt_edit_menu_item *move_point =
	find_menu_item(capture->items, point_label);
    if (!move_point || !move_point->menu_func)
	bu_exit(1, "ERROR: ARB point menu has no '%s' entry\n", point_label);

    (*move_point->menu_func)(s, move_point->menu_arg, 0, 0, NULL);
    if (s->edit_flag != PTARB || s->edit_mode != RT_PARAMS_EDIT_TRANS ||
	a->edit_menu != move_point->menu_arg)
	bu_exit(1, "ERROR: %s menu selection produced flag=%d mode=%d menu=%d\n",
		point_label, s->edit_flag, s->edit_mode, a->edit_menu);
}


static void
arb_edit_reset(struct rt_edit *s, struct rt_arb8_edit *a)
{
    a->newedge = 0;
    a->edit_menu = 0;

    VSETALL(s->e_keypoint, 0.0);
    MAT_IDN(s->acc_rot_sol);
    MAT_IDN(s->incr_change);
    s->acc_sc_sol = 1.0;
    s->e_inpara = 0;
    s->es_scale = 0.0;
    s->mv_context = 1;
    rt_edit_set_edflag(s, RT_EDIT_IDLE);
}


struct directory *
make_arb8(struct rt_wdb *wdbp)
{
    const char *objname = "arb8";

    struct rt_arb_internal *arb;
    BU_ALLOC(arb, struct rt_arb_internal);
    arb->magic = RT_ARB_INTERNAL_MAGIC;

    /* Unit cube */
    VSET(arb->pt[0], 0, 0, 0);
    VSET(arb->pt[1], 1, 0, 0);
    VSET(arb->pt[2], 1, 1, 0);
    VSET(arb->pt[3], 0, 1, 0);
    VSET(arb->pt[4], 0, 0, 1);
    VSET(arb->pt[5], 1, 0, 1);
    VSET(arb->pt[6], 1, 1, 1);
    VSET(arb->pt[7], 0, 1, 1);

    wdb_export(wdbp, objname, (void *)arb, ID_ARB8, 1.0);

    struct directory *dp = db_lookup(wdbp->dbip, objname, LOOKUP_QUIET);
    if (dp == RT_DIR_NULL)
	bu_exit(1, "ERROR: Unable to create arb8 object\n");

    return dp;
}

static void
arb8_reset(struct rt_edit *s, struct rt_arb_internal *arb, struct rt_arb8_edit *a)
{
    VSET(arb->pt[0], 0, 0, 0);
    VSET(arb->pt[1], 1, 0, 0);
    VSET(arb->pt[2], 1, 1, 0);
    VSET(arb->pt[3], 0, 1, 0);
    VSET(arb->pt[4], 0, 0, 1);
    VSET(arb->pt[5], 1, 0, 1);
    VSET(arb->pt[6], 1, 1, 1);
    VSET(arb->pt[7], 0, 1, 1);

    arb_edit_reset(s, a);
}

static void
arb5_reset(struct rt_edit *s, struct rt_arb_internal *arb, struct rt_arb8_edit *a)
{
    VSET(arb->pt[0], 0, 0, 0);
    VSET(arb->pt[1], 1, 0, 0);
    VSET(arb->pt[2], 1, 1, 0);
    VSET(arb->pt[3], 0, 1, 0);
    VSET(arb->pt[4], 0.5, 0.5, 1);
    VMOVE(arb->pt[5], arb->pt[4]);
    VMOVE(arb->pt[6], arb->pt[4]);
    VMOVE(arb->pt[7], arb->pt[4]);

    arb_edit_reset(s, a);
}


static void
arb6_reset(struct rt_edit *s, struct rt_arb_internal *arb, struct rt_arb8_edit *a)
{
    VSET(arb->pt[0], 0, 0, 0);
    VSET(arb->pt[1], 0, 1, 0);
    VSET(arb->pt[2], 0, 1, 1);
    VSET(arb->pt[3], 0, 0, 0.5);
    VSET(arb->pt[4], 1, 0.5, 0);
    VMOVE(arb->pt[5], arb->pt[4]);
    VSET(arb->pt[6], 1, 0.5, 1);
    VMOVE(arb->pt[7], arb->pt[6]);

    arb_edit_reset(s, a);
}


static void
arb7_reset(struct rt_edit *s, struct rt_arb_internal *arb, struct rt_arb8_edit *a)
{
    VSET(arb->pt[0], 0, 0, 0);
    VSET(arb->pt[1], 0, 1, 0);
    VSET(arb->pt[2], 0, 1, 1);
    VSET(arb->pt[3], 0, 0, 0.5);
    VSET(arb->pt[4], 1, 0, 0);
    VSET(arb->pt[5], 1, 1, 0);
    VSET(arb->pt[6], 1, 1, 0.5);
    VMOVE(arb->pt[7], arb->pt[4]);

    arb_edit_reset(s, a);
}


static void
test_arb_point_knob(struct rt_edit *s, struct rt_arb_internal *arb,
	struct rt_arb8_edit *a, struct menu_capture *capture,
	const char *test_name, const char *point_label, int point_index,
	const int *like_points, size_t like_point_count)
{
    select_arb_point_edit(s, a, capture, point_label);

    (*EDOBJ[s->es_int.idb_type].ft_e_axes_pos)(s, &s->es_int, s->tol);
    point_t expected;
    vect_t point_delta = {0.05, 0.0, 0.0};
    VADD2(expected, arb->pt[point_index], point_delta);
    point_t fixed_point;
    VMOVE(fixed_point, arb->pt[0]);

    s->mv_context = 0;
    rt_knob_edit_tran(s, 'm', 0, point_delta);

    for (size_t i = 0; i < like_point_count; i++) {
	int index = like_points[i];
	if (!VNEAR_EQUAL(arb->pt[index], expected, VUNITIZE_TOL))
	    bu_exit(1, "ERROR: %s knob edit did not move pt[%d]\n",
		    test_name, index);
    }
    if (!VNEAR_EQUAL(arb->pt[0], fixed_point, VUNITIZE_TOL))
	bu_exit(1, "ERROR: %s knob edit moved the base\n", test_name);

    bu_log("%s menu and knob edit SUCCESS\n", test_name);
}


static void
test_arb_geometry_helpers(struct rt_edit *s, struct rt_arb_internal *arb,
	struct rt_arb8_edit *a, const struct bn_tol *tol)
{
    enum {
	ARB4_TRI_FACE = 123,
	ARB8_BOTTOM_FACE = 1234,
	ARB8_TOP_FACE = 5678
    };
    const fastf_t half_height = 0.5;
    plane_t peqn[7] = {{0}};

    arb8_reset(s, arb, a);
    if (arb_extrude(arb, ARB8_BOTTOM_FACE, half_height, tol, peqn))
	bu_exit(1, "ERROR: ARB8 face extrusion failed\n");
    for (int i = 0; i < 4; i++) {
	if (!NEAR_EQUAL(arb->pt[i][Z], 0.0, VUNITIZE_TOL) ||
	    !NEAR_EQUAL(arb->pt[i + 4][Z], half_height, VUNITIZE_TOL))
	    bu_exit(1, "ERROR: ARB8 face extrusion moved the wrong vertices\n");
    }

    arb8_reset(s, arb, a);
    if (arb_permute(arb, "2143", tol))
	bu_exit(1, "ERROR: ARB8 vertex permutation failed\n");
    point_t first = {1.0, 0.0, 0.0};
    point_t second = {0.0, 0.0, 0.0};
    if (!VNEAR_EQUAL(arb->pt[0], first, VUNITIZE_TOL) ||
	!VNEAR_EQUAL(arb->pt[1], second, VUNITIZE_TOL))
	bu_exit(1, "ERROR: ARB8 vertex permutation produced wrong points\n");
    if (!arb_permute(arb, "9999", tol) ||
	!VNEAR_EQUAL(arb->pt[0], first, VUNITIZE_TOL))
	bu_exit(1, "ERROR: invalid ARB8 permutation changed geometry\n");

    arb8_reset(s, arb, a);
    if (arb_mirror_face_axis(arb, peqn, ARB8_TOP_FACE, "z", tol))
	bu_exit(1, "ERROR: ARB8 face mirroring failed\n");
    if (!NEAR_EQUAL(arb->pt[0][Z], -1.0, VUNITIZE_TOL) ||
	!NEAR_EQUAL(arb->pt[4][Z], 1.0, VUNITIZE_TOL))
	bu_exit(1, "ERROR: ARB8 face mirroring produced wrong points\n");
    if (!arb_mirror_face_axis(arb, peqn, ARB8_TOP_FACE, "bad", tol) ||
	!NEAR_EQUAL(arb->pt[0][Z], -1.0, VUNITIZE_TOL))
	bu_exit(1, "ERROR: invalid ARB8 mirror axis changed geometry\n");

    arb8_reset(s, arb, a);
    VSET(arb->pt[2], 0.0, 1.0, 0.0);
    VMOVE(arb->pt[3], arb->pt[0]);
    VSET(arb->pt[4], 0.0, 0.0, 1.0);
    for (int i = 5; i < 8; i++)
	VMOVE(arb->pt[i], arb->pt[4]);
    int type = 0;
    int uvec[8], svec[11];
    if (!rt_arb_get_cgtype(&type, arb, tol, uvec, svec) || type != ARB4)
	bu_exit(1, "ERROR: ARB4 extrusion fixture is not an ARB4\n");
    if (arb_extrude(arb, ARB4_TRI_FACE, 1.0, tol, peqn))
	bu_exit(1, "ERROR: ARB4 face extrusion failed\n");
    if (!rt_arb_get_cgtype(&type, arb, tol, uvec, svec) || type != ARB6)
	bu_exit(1, "ERROR: ARB4 extrusion did not create an ARB6\n");
}


int
rt_edit_test_arb8(void)
{
    struct db_i *dbip = db_open_inmem();
    if (dbip == DBI_NULL)
	bu_exit(1, "ERROR: Unable to create database instance\n");

    struct rt_wdb *wdbp = wdb_dbopen(dbip, RT_WDB_TYPE_DB_INMEM);

    struct directory *dp = make_arb8(wdbp);

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

    struct rt_arb_internal *arb =
	(struct rt_arb_internal *)s->es_int.idb_ptr;
    struct rt_arb8_edit *a = (struct rt_arb8_edit *)s->ipe_ptr;
    struct menu_capture captured_menu = {NULL};
    if (rt_edit_map_clbk_set(s->m, ECMD_MENU_SET, BU_CLBK_DURING,
		capture_menu, &captured_menu) != BRLCAD_OK)
	bu_exit(1, "ERROR: Unable to register ARB menu capture callback\n");

    vect_t mousevec;

    /* ================================================================
     * RT_PARAMS_EDIT_SCALE (scale=2 about keypoint pt[0]=(0,0,0))
     *
     * rt_arb_mat with bn_mat_scale_about_pnt(2, (0,0,0)):
     *   mat[15]=0.5; MAT4X3PNT(out, mat, in) = in * 2
     *   pt[0]=(0,0,0)→(0,0,0), pt[6]=(1,1,1)→(2,2,2)
     * ================================================================*/
    rt_edit_set_edflag(s, RT_PARAMS_EDIT_SCALE);
    s->edit_mode = RT_PARAMS_EDIT_SCALE;
    s->e_inpara = 0;
    s->es_scale = 2.0;
    VSET(s->e_keypoint, 0, 0, 0);

    rt_edit_process(s);
    {
	point_t exp_p0 = {0, 0, 0}, exp_p6 = {2, 2, 2};
	if (!VNEAR_EQUAL(arb->pt[0], exp_p0, VUNITIZE_TOL) ||
	    !VNEAR_EQUAL(arb->pt[6], exp_p6, VUNITIZE_TOL))
	    bu_exit(1, "ERROR: RT_PARAMS_EDIT_SCALE failed\n"
		    "  pt[0]=%g,%g,%g  pt[6]=%g,%g,%g\n",
		    V3ARGS(arb->pt[0]), V3ARGS(arb->pt[6]));
	bu_log("RT_PARAMS_EDIT_SCALE SUCCESS: pt[0]=%g,%g,%g pt[6]=%g,%g,%g\n",
	       V3ARGS(arb->pt[0]), V3ARGS(arb->pt[6]));
    }

    /* ================================================================
     * RT_PARAMS_EDIT_TRANS (translate; keypoint (0,0,0) → e_para=(5,5,5))
     *
     * All vertices shift by (5,5,5).
     * pt[0]=(0,0,0)→(5,5,5), pt[6]=(1,1,1)→(6,6,6)
     * ================================================================*/
    arb8_reset(s, arb, a);
    rt_edit_set_edflag(s, RT_PARAMS_EDIT_TRANS);
    s->edit_mode = RT_PARAMS_EDIT_TRANS;
    s->e_inpara = 1;
    VSET(s->e_para, 5, 5, 5);
    VSET(s->e_keypoint, 0, 0, 0);

    rt_edit_process(s);
    {
	point_t exp_p0 = {5, 5, 5}, exp_p6 = {6, 6, 6};
	if (!VNEAR_EQUAL(arb->pt[0], exp_p0, VUNITIZE_TOL) ||
	    !VNEAR_EQUAL(arb->pt[6], exp_p6, VUNITIZE_TOL))
	    bu_exit(1, "ERROR: RT_PARAMS_EDIT_TRANS failed\n"
		    "  pt[0]=%g,%g,%g  pt[6]=%g,%g,%g\n",
		    V3ARGS(arb->pt[0]), V3ARGS(arb->pt[6]));
	bu_log("RT_PARAMS_EDIT_TRANS SUCCESS: pt[0]=%g,%g,%g pt[6]=%g,%g,%g\n",
	       V3ARGS(arb->pt[0]), V3ARGS(arb->pt[6]));
    }

    /* ================================================================
     * RT_PARAMS_EDIT_ROT (rotate about keypoint (0,0,0), angles=5,5,5)
     *
     * Rotation about V1=pt[0]=(0,0,0).
     * pt[0] stays at origin. pt[6]=(1,1,1) rotates.
     * ================================================================*/
    arb8_reset(s, arb, a);
    MAT_IDN(s->acc_rot_sol);
    rt_edit_set_edflag(s, RT_PARAMS_EDIT_ROT);
    s->edit_mode = RT_PARAMS_EDIT_ROT;
    s->e_inpara = 1;
    VSET(s->e_para, 5, 5, 5);
    s->vp->gv_rotate_about = 'k';
    VSET(s->e_keypoint, 0, 0, 0);

    rt_edit_process(s);
    {
	point_t exp_p0 = {0, 0, 0};
	if (!VNEAR_EQUAL(arb->pt[0], exp_p0, VUNITIZE_TOL))
	    bu_exit(1, "ERROR: RT_PARAMS_EDIT_ROT(k): pt[0] moved: %g,%g,%g\n",
		    V3ARGS(arb->pt[0]));
	point_t orig_p6 = {1, 1, 1};
	if (VNEAR_EQUAL(arb->pt[6], orig_p6, VUNITIZE_TOL))
	    bu_exit(1, "ERROR: RT_PARAMS_EDIT_ROT(k): pt[6] not rotated (%g,%g,%g)\n",
		    V3ARGS(arb->pt[6]));
	bu_log("RT_PARAMS_EDIT_ROT(k) SUCCESS: pt[0]=%g,%g,%g pt[6]=%g,%g,%g\n",
	       V3ARGS(arb->pt[0]), V3ARGS(arb->pt[6]));
    }

    /* ================================================================
     * RT_PARAMS_EDIT_TRANS XY: verify vertices moved
     * ================================================================*/
    arb8_reset(s, arb, a);
    rt_edit_set_edflag(s, RT_PARAMS_EDIT_TRANS);
    s->edit_mode = RT_PARAMS_EDIT_TRANS;
    VMOVE(s->curr_e_axes_pos, arb->pt[0]);
    {
	int xpos = 1300, ypos = 800;
	mousevec[X] = xpos * INV_BV;
	mousevec[Y] = ypos * INV_BV;
	mousevec[Z] = 0;
    }
    bu_vls_trunc(s->log_str, 0);
    if ((*EDOBJ[dp->d_minor_type].ft_edit_xy)(s, mousevec) == BRLCAD_ERROR)
	bu_exit(1, "ERROR: RT_PARAMS_EDIT_TRANS(xy) failed: %s\n",
		bu_vls_cstr(s->log_str));
    rt_edit_process(s);
    {
	point_t orig_p0 = {0, 0, 0};
	if (VNEAR_EQUAL(arb->pt[0], orig_p0, SQRT_SMALL_FASTF))
	    bu_exit(1, "ERROR: RT_PARAMS_EDIT_TRANS(xy) did not move pt[0]\n");
    }
    bu_log("RT_PARAMS_EDIT_TRANS(xy) SUCCESS: pt[0]=%g,%g,%g\n",
	   V3ARGS(arb->pt[0]));

    /* ================================================================
     * RT_PARAMS_EDIT_ROT XY returns BRLCAD_ERROR (unimplemented)
     * ================================================================*/
    arb8_reset(s, arb, a);
    rt_edit_set_edflag(s, RT_PARAMS_EDIT_ROT);
    s->edit_mode = RT_PARAMS_EDIT_ROT;
    mousevec[X] = 0.1; mousevec[Y] = -0.05; mousevec[Z] = 0;
    bu_vls_trunc(s->log_str, 0);
    int rot_xy_ret = (*EDOBJ[dp->d_minor_type].ft_edit_xy)(s, mousevec);
    if (rot_xy_ret != BRLCAD_OK)
	bu_exit(1, "ERROR: RT_PARAMS_EDIT_ROT(xy) failed\n");
    bu_log("RT_PARAMS_EDIT_ROT(xy) SUCCESS: rotation applied via knob path\n");

    /* ================================================================
     * PTARB ARB5 point 5: both direct parameters and knob translation must
     * retain the primitive-specific edit flag.  Point 5 is stored at pt[4]
     * and duplicated in pt[5..7].
     * ================================================================*/
    arb5_reset(s, arb, a);
    select_arb_point_edit(s, a, &captured_menu, "Move Point 5");
    s->e_inpara = 3;
    VSET(s->e_para, 0.75, 0.75, 1.25);
    s->mv_context = 0;
    rt_edit_process(s);
    {
	point_t expected = {0.75, 0.75, 1.25};
	point_t expected_base = {0, 0, 0};
	if (!VNEAR_EQUAL(arb->pt[4], expected, VUNITIZE_TOL) ||
	    !VNEAR_EQUAL(arb->pt[5], expected, VUNITIZE_TOL) ||
	    !VNEAR_EQUAL(arb->pt[6], expected, VUNITIZE_TOL) ||
	    !VNEAR_EQUAL(arb->pt[7], expected, VUNITIZE_TOL))
	    bu_exit(1, "ERROR: PTARB ARB5 point 5 parameter edit failed\n");
	if (!VNEAR_EQUAL(arb->pt[0], expected_base, VUNITIZE_TOL))
	    bu_exit(1, "ERROR: PTARB ARB5 point 5 moved the base\n");
	bu_log("PTARB ARB5 point 5 parameter edit SUCCESS\n");
    }

    /* Mouse point edits use base-unit coordinates, even in an inch database. */
    arb5_reset(s, arb, a);
    select_arb_point_edit(s, a, &captured_menu, "Move Point 5");
    s->local2base = 25.4;
    s->base2local = 1.0 / s->local2base;
    MAT_IDN(v->gv_model2view);
    MAT_IDN(v->gv_view2model);
    MAT_IDN(s->e_invmat);
    VMOVE(s->curr_e_axes_pos, arb->pt[4]);
    vect_t knob_state;
    VSET(knob_state, 7.0, 8.0, 9.0);
    VMOVE(s->k.tra_m_abs, knob_state);
    VMOVE(s->k.tra_v_abs, knob_state);
    VSET(mousevec, 0.65, 0.65, 0.0);
    if (EDOBJ[dp->d_minor_type].ft_edit_xy(s, mousevec) != BRLCAD_OK)
	bu_exit(1, "ERROR: ARB5 mouse point move failed\n");
    point_t expected_mouse_point;
    VSET(expected_mouse_point, mousevec[X], mousevec[Y], 1.0);
    for (int i = 4; i < 8; i++) {
	if (!VNEAR_EQUAL(arb->pt[i], expected_mouse_point, VUNITIZE_TOL))
	    bu_exit(1, "ERROR: ARB5 mouse point move changed pt[%d] incorrectly\n", i);
    }
    point_t arb5_view_target;
    VSET(arb5_view_target, mousevec[X], mousevec[Y], s->curr_e_axes_pos[Z]);
    if (!edit_test_mouse_knobs_match(s, arb5_view_target))
	bu_exit(1, "ERROR: ARB5 mouse point knobs missed cursor\n");
    s->local2base = 1.0;
    s->base2local = 1.0;

    const int arb5_point5[] = {4, 5, 6, 7};
    arb5_reset(s, arb, a);
    test_arb_point_knob(s, arb, a, &captured_menu, "PTARB ARB5 point 5",
	    "Move Point 5", 4, arb5_point5, sizeof(arb5_point5) / sizeof(arb5_point5[0]));

    const int arb6_point5[] = {4, 5};
    arb6_reset(s, arb, a);
    test_arb_point_knob(s, arb, a, &captured_menu, "PTARB ARB6 point 5",
	    "Move Point 5", 4, arb6_point5, sizeof(arb6_point5) / sizeof(arb6_point5[0]));

    const int arb6_point6[] = {6, 7};
    arb6_reset(s, arb, a);
    test_arb_point_knob(s, arb, a, &captured_menu, "PTARB ARB6 point 6",
	    "Move Point 6", 6, arb6_point6, sizeof(arb6_point6) / sizeof(arb6_point6[0]));

    const int arb7_point5[] = {4, 7};
    arb7_reset(s, arb, a);
    test_arb_point_knob(s, arb, a, &captured_menu, "PTARB ARB7 point 5",
	    "Move Point 5", 4, arb7_point5, sizeof(arb7_point5) / sizeof(arb7_point5[0]));


    /* ================================================================
     * RT_MATRIX_EDIT_ROT: absolute matrix rotation from keyboard angles
     *
     * Set model_changes to a 30-deg rotation about X. Keypoint is at
     * the primitive's vertex (varies per primitive).
     * After the edit model_changes must differ from identity.
     * ================================================================*/
    arb8_reset(s, arb, a);
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
    arb8_reset(s, arb, a);
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
     * ECMD_ARB_MOVE_FACE — move bottom face (face 0 = 1234) to z=0.5.
     *
     * The unit cube has bottom face 1234 at z=0.  Moving it to pass
     * through (0,0,0.5) should push pt[0]-pt[3] from z=0 to z=0.5.
     * The top face (pt[4]-pt[7]) remains at z=1.
     *
     * Reasoning: face 0 has normal (0,0,±1).  After the move:
     *   D' = VDOT(normal, (0,0,0.5)) → plane equation is z=0.5.
     * rt_arb_calc_points then recalculates every vertex from plane
     * intersections — the four bottom vertices all land at z=0.5.
     * ================================================================*/
    arb8_reset(s, arb, a);
    a->edit_menu = 0;   /* face 0 = 1234 (bottom), in mv8_menu: arg=1 → edit_menu=0 */
    rt_edit_set_edflag(s, ECMD_ARB_MOVE_FACE);
    s->local2base = 25.4;
    s->base2local = 1.0 / s->local2base;
    s->e_inpara = 4;
    s->e_para[0] = 0.0;  /* face index */
    s->e_para[1] = 0.0;
    s->e_para[2] = 0.0;
    s->e_para[3] = 0.5 / s->local2base;
    s->mv_context = 0;            /* use s->e_para directly, no inv-mat */
    bu_vls_trunc(s->log_str, 0);
    rt_edit_process(s);
    if (!NEAR_EQUAL(s->e_para[3], 0.5 / s->local2base, VUNITIZE_TOL))
	bu_exit(1, "ERROR: ARB face move changed local-unit input\n");
    s->e_inpara = 4;
    rt_edit_process(s);
    {
	fastf_t exp_z = 0.5;
	for (int i = 0; i < 4; i++) {
	    if (!NEAR_EQUAL(arb->pt[i][Z], exp_z, VUNITIZE_TOL))
		bu_exit(1, "ERROR: ECMD_ARB_MOVE_FACE: pt[%d].z=%g (expected %g)\n",
			i, arb->pt[i][Z], exp_z);
	}
	/* top face should be unchanged at z=1 */
	for (int i = 4; i < 8; i++) {
	    if (!NEAR_EQUAL(arb->pt[i][Z], 1.0, VUNITIZE_TOL))
		bu_exit(1, "ERROR: ECMD_ARB_MOVE_FACE: top pt[%d].z=%g (expected 1)\n",
			i, arb->pt[i][Z]);
	}
	bu_log("ECMD_ARB_MOVE_FACE SUCCESS: bottom face moved to z=%g, "
	       "pt[0]=%g,%g,%g pt[3]=%g,%g,%g\n",
	       exp_z, V3ARGS(arb->pt[0]), V3ARGS(arb->pt[3]));
    }

    /* ================================================================
     * EARB — move edge 12 (between pt[0] and pt[1]) to z=0.5.
     *
     * edge 0 = "Move Edge 12" in edge8_menu → edit_menu=0.
     * earb8[0] = {pt1=0, pt2=1, bp1=2 (face 1584, x=0),
     *             bp2=3 (face 2376, x=1), ...}
     *
     * mv_edge logic:
     *   edge_dir = pt[1]-pt[0] = (1,0,0)
     *   Line through thru=(0,0,0.5) with dir=(1,0,0):
     *     p(t) = (t, 0, 0.5)
     *   Intersect with bp1 (x=0 plane): t=0  → new pt[0] = (0, 0, 0.5)
     *   Intersect with bp2 (x=1 plane): t=1  → new pt[1] = (1, 0, 0.5)
     *
     * Expected: pt[0]=(0,0,0.5), pt[1]=(1,0,0.5).
     * ================================================================*/
    arb8_reset(s, arb, a);
    a->edit_menu = 0;   /* edge 0 = edge 12 (pt[0] to pt[1]) */
    a->newedge   = 0;   /* compute edge direction from existing vertices */
    rt_edit_set_edflag(s, EARB);
    s->edit_mode = RT_PARAMS_EDIT_TRANS;
    s->e_inpara  = 4;
    s->e_para[0] = 0.0;  /* edge index */
    s->e_para[1] = 0.0;
    s->e_para[2] = 0.0;
    s->e_para[3] = 0.5 / s->local2base;
    s->mv_context = 0;
    bu_vls_trunc(s->log_str, 0);
    rt_edit_process(s);
    if (!NEAR_EQUAL(s->e_para[3], 0.5 / s->local2base, VUNITIZE_TOL))
	bu_exit(1, "ERROR: ARB edge move changed local-unit input\n");
    s->e_inpara = 4;
    rt_edit_process(s);
    {
	point_t exp0 = {0, 0, 0.5}, exp1 = {1, 0, 0.5};
	if (!VNEAR_EQUAL(arb->pt[0], exp0, VUNITIZE_TOL) ||
	    !VNEAR_EQUAL(arb->pt[1], exp1, VUNITIZE_TOL))
	    bu_exit(1, "ERROR: EARB: pt[0]=%g,%g,%g (expected 0,0,0.5)  "
		    "pt[1]=%g,%g,%g (expected 1,0,0.5)\n",
		    V3ARGS(arb->pt[0]), V3ARGS(arb->pt[1]));
	bu_log("EARB SUCCESS: edge 12 moved to z=0.5: "
	       "pt[0]=%g,%g,%g  pt[1]=%g,%g,%g\n",
	       V3ARGS(arb->pt[0]), V3ARGS(arb->pt[1]));
    }

    /* Mouse edge and face moves update the knobs to the cursor. */
    vect_t mouse_knob_state;
    VSET(mouse_knob_state, 7.0, 8.0, 9.0);
    arb8_reset(s, arb, a);
    a->edit_menu = 0;
    rt_edit_set_edflag(s, EARB);
    MAT_IDN(v->gv_model2view);
    MAT_IDN(v->gv_view2model);
    MAT_IDN(s->e_invmat);
    VMOVE(s->curr_e_axes_pos, arb->pt[0]);
    VMOVE(s->k.tra_m_abs, mouse_knob_state);
    VMOVE(s->k.tra_v_abs, mouse_knob_state);
    VSET(mousevec, 0.0, 0.2, 0.0);
    if (EDOBJ[dp->d_minor_type].ft_edit_xy(s, mousevec) != BRLCAD_OK)
	bu_exit(1, "ERROR: ARB8 mouse edge move failed\n");
    if (!NEAR_EQUAL(arb->pt[0][Y], mousevec[Y], VUNITIZE_TOL) ||
	!NEAR_EQUAL(arb->pt[1][Y], mousevec[Y], VUNITIZE_TOL))
	bu_exit(1, "ERROR: ARB8 mouse edge move changed the wrong geometry\n");
    point_t edge_view_target;
    VSET(edge_view_target, mousevec[X], mousevec[Y], s->curr_e_axes_pos[Z]);
    if (!edit_test_mouse_knobs_match(s, edge_view_target))
	bu_exit(1, "ERROR: ARB8 mouse edge knobs missed cursor\n");

    arb8_reset(s, arb, a);
    a->edit_menu = 2; /* x=0 side face */
    struct bu_vls plane_error = BU_VLS_INIT_ZERO;
    if (rt_arb_calc_planes(&plane_error, arb, ARB8, a->es_peqn, s->tol)) {
	bu_exit(1, "ERROR: ARB8 mouse face setup failed: %s\n",
		bu_vls_cstr(&plane_error));
    }
    bu_vls_free(&plane_error);
    rt_edit_set_edflag(s, ECMD_ARB_MOVE_FACE);
    VMOVE(s->curr_e_axes_pos, arb->pt[0]);
    VMOVE(s->k.tra_m_abs, mouse_knob_state);
    VMOVE(s->k.tra_v_abs, mouse_knob_state);
    VSET(mousevec, 0.2, 0.0, 0.0);
    if (EDOBJ[dp->d_minor_type].ft_edit_xy(s, mousevec) != BRLCAD_OK)
	bu_exit(1, "ERROR: ARB8 mouse face move failed\n");
    const int face_points[] = {0, 3, 4, 7};
    for (size_t i = 0; i < sizeof(face_points) / sizeof(face_points[0]); i++) {
	if (!NEAR_EQUAL(arb->pt[face_points[i]][X], mousevec[X], VUNITIZE_TOL))
	    bu_exit(1, "ERROR: ARB8 mouse face move changed pt[%d] incorrectly\n",
		    face_points[i]);
    }
    point_t face_view_target;
    VSET(face_view_target, mousevec[X], mousevec[Y], s->curr_e_axes_pos[Z]);
    if (!edit_test_mouse_knobs_match(s, face_view_target))
	bu_exit(1, "ERROR: ARB8 mouse face knobs missed cursor\n");

    s->local2base = 1.0;
    s->base2local = 1.0;

    /* ================================================================
     * ECMD_ARB_SETUP_ROTFACE + ECMD_ARB_ROTATE_FACE (non-interactive)
     *
     * Use the new e_para[0] path to set fixv=1 (vertex 0, 0-based)
     * without a callback.  After setup, edit_flag becomes
     * ECMD_ARB_ROTATE_FACE.  Then apply a 45-deg X-axis rotation to
     * face 4 (edit_menu=4, the top face) and verify that the plane
     * normal changes.
     * ================================================================*/
    arb8_reset(s, arb, a);
    a->edit_menu = 4;  /* face 4 (top face in arb8 ordering) */
    /* Supply fixv via e_para[0] (1-based) */
    s->e_inpara = 1;
    s->e_para[0] = 1.0;  /* fix vertex 1 = arb->pt[0] */
    bu_vls_trunc(s->log_str, 0);
    /* ECMD_ARB_SETUP_ROTFACE transitions to ECMD_ARB_ROTATE_FACE */
    rt_edit_set_edflag(s, ECMD_ARB_SETUP_ROTFACE);
    rt_edit_process(s);
    if (s->edit_flag != ECMD_ARB_ROTATE_FACE)
	bu_exit(1, "ERROR: ECMD_ARB_SETUP_ROTFACE: edit_flag not ECMD_ARB_ROTATE_FACE (got %d)\n",
		s->edit_flag);
    if (a->fixv != 0)  /* should be 0-based now (1-based input minus 1) */
	bu_exit(1, "ERROR: ECMD_ARB_SETUP_ROTFACE: fixv=%d (expected 0)\n", a->fixv);
    bu_log("ECMD_ARB_SETUP_ROTFACE SUCCESS: fixv=%d edit_flag=ECMD_ARB_ROTATE_FACE\n",
	   a->fixv);

    /* Now apply a 45-degree X rotation via ECMD_ARB_ROTATE_FACE */
    plane_t orig_peqn;
    HMOVE(orig_peqn, a->es_peqn[a->edit_menu]);
    MAT_IDN(s->acc_rot_sol);
    MAT_IDN(s->model_changes);
    s->e_inpara = 3;
    s->e_para[0] = 45.0; s->e_para[1] = 0.0; s->e_para[2] = 0.0;
    s->mv_context = 0;
    VMOVE(s->e_keypoint, arb->pt[a->fixv]);
    if (rt_edit_process(s) != BRLCAD_OK)
	bu_exit(1, "ERROR: ECMD_ARB_ROTATE_FACE reported failure\n");
    /* The plane normal must differ from the original */
    if (VEQUAL(a->es_peqn[a->edit_menu], orig_peqn))
	bu_exit(1, "ERROR: ECMD_ARB_ROTATE_FACE: plane normal unchanged after 45-deg rotation\n");
    bu_log("ECMD_ARB_ROTATE_FACE SUCCESS: normal=(%.3f,%.3f,%.3f) D=%.3f\n",
	   a->es_peqn[a->edit_menu][0], a->es_peqn[a->edit_menu][1],
	   a->es_peqn[a->edit_menu][2], a->es_peqn[a->edit_menu][W]);

    arb8_reset(s, arb, a);
    rt_edit_set_edflag(s, ECMD_ARB_ROTATE_FACE);
    s->e_inpara = 5;
    s->e_para[0] = 4.0;
    s->e_para[1] = 0.0;
    s->e_para[2] = 45.0;
    s->e_para[3] = 0.0;
    s->e_para[4] = 0.0;
    MAT_IDN(s->acc_rot_sol);
    MAT_IDN(s->model_changes);
    s->mv_context = 0;
    VMOVE(s->e_keypoint, arb->pt[0]);
    if (rt_edit_process(s) != BRLCAD_OK)
	bu_exit(1, "ERROR: extended face rotation reported failure\n");
    if (!NEAR_EQUAL(s->e_para[0], 4.0, VUNITIZE_TOL) ||
	!NEAR_EQUAL(s->e_para[2], 45.0, VUNITIZE_TOL) ||
	ZERO(a->es_peqn[4][Y]))
	bu_exit(1, "ERROR: ARB extended face rotation changed input or left plane unchanged\n");

    /* The same face operation must survive the interactive rotation knob
     * adapter, which supplies an incremental matrix rather than parameters.
     */
    arb8_reset(s, arb, a);
    a->edit_menu = 4;
    s->e_inpara = 1;
    s->e_para[0] = 1.0;
    rt_edit_set_edflag(s, ECMD_ARB_SETUP_ROTFACE);
    rt_edit_process(s);
    plane_t knob_orig_peqn;
    HMOVE(knob_orig_peqn, a->es_peqn[a->edit_menu]);
    mat_t knob_rot;
    MAT_IDN(knob_rot);
    bn_mat_angles(knob_rot, 45.0, 0.0, 0.0);
    rt_knob_edit_rot(s, 'm', 'm', 0, knob_rot);
    if (VEQUAL(a->es_peqn[a->edit_menu], knob_orig_peqn))
	bu_exit(1, "ERROR: ECMD_ARB_ROTATE_FACE knob edit did not rotate face\n");
    bu_log("ECMD_ARB_ROTATE_FACE knob edit SUCCESS\n");

    test_arb_geometry_helpers(s, arb, a, &tol);

    /* The public editing entry point must reject invalid type and menu indexes
     * before using them to address the type-specific editing tables. */
    plane_t invalid_planes[6] = {{0}};
    vect_t invalid_position = VINIT_ZERO;
    bu_vls_trunc(s->log_str, 0);
    if (rt_arb_edit(s->log_str, arb, NULL, ARB4 - 1, 0,
	    RT_ARB_EDIT_DEFAULT, invalid_position, invalid_planes, &tol) != 1)
	bu_exit(1, "ERROR: rt_arb_edit accepted an invalid ARB type\n");

    bu_vls_trunc(s->log_str, 0);
    if (rt_arb_edit(s->log_str, arb, NULL, ARB8, ARB8_EDIT_COUNT,
	    RT_ARB_EDIT_DEFAULT, invalid_position, invalid_planes, &tol) != 1)
	bu_exit(1, "ERROR: rt_arb_edit accepted an invalid edit index\n");
    if (!strstr(bu_vls_cstr(s->log_str), "bad edit index"))
	bu_exit(1, "ERROR: rt_arb_edit did not report the invalid edit index\n");
    bu_log("rt_arb_edit invalid input rejection SUCCESS\n");

    arb8_reset(s, arb, a);
    a->edit_menu = ARB8_EDIT_COUNT;
    rt_edit_set_edflag(s, EARB);
    MAT_IDN(v->gv_model2view);
    MAT_IDN(v->gv_view2model);
    MAT_IDN(s->e_invmat);
    VMOVE(s->curr_e_axes_pos, arb->pt[0]);
    VSET(mousevec, 0.25, 0.25, 0.0);
    if (EDOBJ[dp->d_minor_type].ft_edit_xy(s, mousevec) != BRLCAD_ERROR)
	bu_exit(1, "ERROR: ARB mouse edge edit hid an invalid menu index\n");

    arb8_reset(s, arb, a);
    a->edit_menu = ARB8_EDIT_COUNT;
    rt_edit_set_edflag(s, EARB);
    s->e_inpara = 3;
    VSET(s->e_para, 0.25, 0.25, 0.0);
    if (rt_edit_process(s) != BRLCAD_ERROR)
	bu_exit(1, "ERROR: ARB parameter edge edit hid an invalid menu index\n");

    arb8_reset(s, arb, a);
    a->edit_menu = ARB8_FACE_COUNT;
    rt_edit_set_edflag(s, ECMD_ARB_MOVE_FACE);
    if (EDOBJ[dp->d_minor_type].ft_edit_xy(s, mousevec) != BRLCAD_ERROR)
	bu_exit(1, "ERROR: ARB mouse face edit accepted an invalid face\n");

    arb8_reset(s, arb, a);
    rt_edit_set_edflag(s, ECMD_ARB_MOVE_FACE);
    s->e_inpara = 4;
    VSET(s->e_para, ARB8_FACE_COUNT, 0.0, 0.0);
    s->e_para[3] = 0.5;
    if (rt_edit_process(s) != BRLCAD_ERROR)
	bu_exit(1, "ERROR: ARB parameter face edit accepted an invalid face\n");

    arb8_reset(s, arb, a);
    rt_edit_set_edflag(s, ECMD_ARB_ROTATE_FACE);
    s->e_inpara = 5;
    VSET(s->e_para, ARB8_FACE_COUNT, 0.0, 45.0);
    s->e_para[3] = 0.0;
    s->e_para[4] = 0.0;
    if (rt_edit_process(s) != BRLCAD_ERROR)
	bu_exit(1, "ERROR: ARB face rotation accepted an invalid face\n");

    arb8_reset(s, arb, a);
    rt_edit_set_edflag(s, ECMD_ARB_ROTATE_FACE);
    s->e_inpara = 5;
    VSET(s->e_para, 0.0, ARB8_VERTEX_COUNT, 45.0);
    s->e_para[3] = 0.0;
    s->e_para[4] = 0.0;
    if (rt_edit_process(s) != BRLCAD_ERROR)
	bu_exit(1, "ERROR: ARB face rotation accepted an invalid fixed vertex\n");

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
