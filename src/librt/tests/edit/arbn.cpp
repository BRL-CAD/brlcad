/*                       A R B N . C P P
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
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
/** @file arbn.cpp
 *
 * Unit tests for ARBN primitive editing via edarbn.c.
 *
 * Reference ARBN: a unit cube defined by 6 axis-aligned planes.
 *
 * Tests verify:
 *   - ECMD_ARBN_PLANE_SELECT stores plane index correctly
 *   - ECMD_ARBN_PLANE_SET_DIST changes the 4th coefficient
 *   - ECMD_ARBN_PLANE_SET_NORM replaces the normal
 *   - ECMD_ARBN_PLANE_ROTATE rotates the normal
 *   - ECMD_ARBN_PLANE_ADD appends a new plane
 *   - ECMD_ARBN_PLANE_DEL removes a plane
 *   - rt_edit_arbn_get_params returns correct values
 *   - Descriptor is accessible and has the right number of commands
 */

#include "common.h"

#include <cmath>
#include <cstring>

#include "vmath.h"
#include "bu/log.h"
#include "bu/malloc.h"
#include "bu/vls.h"
#include "raytrace.h"
#include "rt/geom.h"

/* Mirror the private ECMD numbers from edarbn.c */
#define ECMD_ARBN_PLANE_SELECT    14010
#define ECMD_ARBN_PLANE_SET_DIST  14011
#define ECMD_ARBN_PLANE_SET_NORM  14012
#define ECMD_ARBN_PLANE_ROTATE    14013
#define ECMD_ARBN_PLANE_ADD       14014
#define ECMD_ARBN_PLANE_DEL       14015

enum { ARBN_CUBE_PLANE_COUNT = 6 };

/* Mirror the private state struct */
struct rt_arbn_edit_local {
    int plane_index;
};

static bool
same_planes(const struct rt_arbn_internal *actual, const plane_t *expected,
	    size_t count)
{
    if (actual->neqn != count)
	return false;
    for (size_t i = 0; i < count; ++i) {
	for (int axis = 0; axis < 4; ++axis) {
	    if (!NEAR_EQUAL(actual->eqn[i][axis], expected[i][axis],
		    VUNITIZE_TOL)) {
		bu_log("arbn plane %zu coefficient %d: expected %g, got %g\n",
		    i, axis, expected[i][axis], actual->eqn[i][axis]);
		return false;
	    }
	}
    }
    return true;
}


static struct directory *
make_arbn(struct rt_wdb *wdbp)
{
    /* Unit cube: 6 axis-aligned half-spaces */
    struct rt_arbn_internal *aip;
    BU_ALLOC(aip, struct rt_arbn_internal);
    aip->magic = RT_ARBN_INTERNAL_MAGIC;
    aip->neqn  = 6;
    aip->eqn   = (plane_t *)bu_calloc(6, sizeof(plane_t), "arbn eqn");

    /* +X plane: x <=  1  →  N=(1,0,0) d=1 */
    HSET(aip->eqn[0],  1,  0,  0,  1);
    /* -X plane: x >= -1  →  N=(-1,0,0) d=1 */
    HSET(aip->eqn[1], -1,  0,  0,  1);
    /* +Y plane */
    HSET(aip->eqn[2],  0,  1,  0,  1);
    /* -Y plane */
    HSET(aip->eqn[3],  0, -1,  0,  1);
    /* +Z plane */
    HSET(aip->eqn[4],  0,  0,  1,  1);
    /* -Z plane */
    HSET(aip->eqn[5],  0,  0, -1,  1);

    wdb_export(wdbp, "arbn", (void *)aip, ID_ARBN, 1.0);
    struct directory *dp = db_lookup(wdbp->dbip, "arbn", LOOKUP_QUIET);
    if (dp == RT_DIR_NULL)
	bu_exit(1, "ERROR: Unable to create arbn object\n");
    return dp;
}

static int
arbn_unit_matrix(struct db_i *dbip, struct db_full_path *path,
		 struct bn_tol *tol)
{
    const fastf_t inch_to_mm = 25.4;
    const fastf_t scales[] = {1.0, inch_to_mm};
    int failures = 0;

    for (fastf_t scale : scales) {
	dbip->dbi_local2base = scale;
	dbip->dbi_base2local = 1.0 / scale;
	const char *unit = EQUAL(scale, 1.0) ? "mm" : "in";
	struct rt_edit *edit = rt_edit_create(path, dbip, tol, NULL);
	if (!edit) {
	    bu_log("arbn\tset distance\t%s\tfail: edit creation\n", unit);
	    ++failures;
	    continue;
	}
	struct rt_arbn_internal *arbn =
	    (struct rt_arbn_internal *)edit->es_int.idb_ptr;
	if (arbn->neqn != ARBN_CUBE_PLANE_COUNT) {
	    bu_log("arbn\tset distance\t%s\tfail: fixture has %zu planes\n",
		unit, arbn->neqn);
	    ++failures;
	    rt_edit_destroy(edit);
	    continue;
	}
	plane_t expected[ARBN_CUBE_PLANE_COUNT + 1];
	for (size_t i = 0; i < arbn->neqn; ++i)
	    HMOVE(expected[i], arbn->eqn[i]);
	rt_edit_set_edflag(edit, ECMD_ARBN_PLANE_SELECT);
	edit->e_inpara = 1;
	edit->e_para[0] = 0.5;
	bool rejected = rt_edit_process(edit) == BRLCAD_ERROR &&
	    ((struct rt_arbn_edit_local *)edit->ipe_ptr)->plane_index == -1 &&
	    same_planes(arbn, expected, ARBN_CUBE_PLANE_COUNT);
	bu_log("arbn\treject fractional index\t%s\t%s\n", unit,
	    rejected ? "pass" : "fail");
	if (!rejected)
	    ++failures;
	edit->e_inpara = 1;
	edit->e_para[0] = 0;
	bool selected = rt_edit_process(edit) == BRLCAD_OK &&
	    ((struct rt_arbn_edit_local *)edit->ipe_ptr)->plane_index == 0;
	rt_edit_set_edflag(edit, ECMD_ARBN_PLANE_SET_DIST);
	edit->e_inpara = 0;
	rejected = rt_edit_process(edit) == BRLCAD_ERROR &&
	    same_planes(arbn, expected, ARBN_CUBE_PLANE_COUNT);
	bu_log("arbn\treject missing distance\t%s\t%s\n", unit,
	    rejected ? "pass" : "fail");
	if (!rejected)
	    ++failures;
	edit->e_inpara = 1;
	edit->e_para[0] = inch_to_mm / scale;
	expected[0][3] = inch_to_mm;
	bool passed = selected && rt_edit_process(edit) == BRLCAD_OK &&
	    same_planes(arbn, expected, ARBN_CUBE_PLANE_COUNT) &&
	    NEAR_EQUAL(edit->e_para[0], inch_to_mm / scale, VUNITIZE_TOL);
	fastf_t values[4] = {0};
	passed = EDOBJ[ID_ARBN].ft_edit_get_params(edit,
	    ECMD_ARBN_PLANE_SET_DIST, values) == 1 &&
	    NEAR_EQUAL(values[0], inch_to_mm / scale, VUNITIZE_TOL) &&
	    passed;
	bu_log("arbn\tset distance\t%s\t%s\n", unit,
	    passed ? "pass" : "fail");
	if (!passed) {
	    bu_log("distance=%g, input=%g, readback=%g, selection=%d, log=%s\n",
		arbn->eqn[0][3], edit->e_para[0], values[0],
		((struct rt_arbn_edit_local *)edit->ipe_ptr)->plane_index,
		bu_vls_cstr(edit->log_str));
	    ++failures;
	}
	rt_edit_set_edflag(edit, ECMD_ARBN_PLANE_SET_NORM);
	edit->e_inpara = 3;
	VSET(edit->e_para, 2, 0, 0);
	passed = rt_edit_process(edit) == BRLCAD_OK &&
	    same_planes(arbn, expected, ARBN_CUBE_PLANE_COUNT) &&
	    NEAR_EQUAL(edit->e_para[0], 2, VUNITIZE_TOL);
	bu_log("arbn\tset normal\t%s\t%s\n", unit,
	    passed ? "pass" : "fail");
	if (!passed)
	    ++failures;

	edit->e_inpara = 3;
	VSETALL(edit->e_para, 0);
	passed = rt_edit_process(edit) == BRLCAD_ERROR &&
	    same_planes(arbn, expected, ARBN_CUBE_PLANE_COUNT);
	bu_log("arbn\treject zero normal\t%s\t%s\n", unit,
	    passed ? "pass" : "fail");
	if (!passed)
	    ++failures;

	rt_edit_set_edflag(edit, ECMD_ARBN_PLANE_ROTATE);
	edit->e_inpara = 3;
	VSET(edit->e_para, 0, 0, 30);
	expected[0][X] = sqrt(3.0) / 2;
	expected[0][Y] = 0.5;
	passed = rt_edit_process(edit) == BRLCAD_OK &&
	    same_planes(arbn, expected, ARBN_CUBE_PLANE_COUNT) &&
	    NEAR_EQUAL(edit->e_para[Z], 30, VUNITIZE_TOL);
	bu_log("arbn\trotate normal\t%s\t%s\n", unit,
	    passed ? "pass" : "fail");
	if (!passed)
	    ++failures;
	edit->e_inpara = 2;
	rejected = rt_edit_process(edit) == BRLCAD_ERROR &&
	    same_planes(arbn, expected, ARBN_CUBE_PLANE_COUNT);
	bu_log("arbn\treject short rotation\t%s\t%s\n", unit,
	    rejected ? "pass" : "fail");
	if (!rejected)
	    ++failures;
	rt_edit_destroy(edit);

	edit = rt_edit_create(path, dbip, tol, NULL);
	if (!edit) {
	    bu_log("arbn\tadd/delete plane\t%s\tfail: edit creation\n", unit);
	    ++failures;
	    continue;
	}
	arbn = (struct rt_arbn_internal *)edit->es_int.idb_ptr;
	const size_t original_count = arbn->neqn;
	if (original_count != ARBN_CUBE_PLANE_COUNT) {
	    bu_log("arbn\tadd/delete plane\t%s\tfail: fixture has %zu planes\n",
		unit, original_count);
	    ++failures;
	    rt_edit_destroy(edit);
	    continue;
	}
	for (size_t i = 0; i < original_count; ++i)
	    HMOVE(expected[i], arbn->eqn[i]);
	rt_edit_set_edflag(edit, ECMD_ARBN_PLANE_DEL);
	edit->e_inpara = 0;
	rejected = rt_edit_process(edit) == BRLCAD_ERROR &&
	    same_planes(arbn, expected, original_count);
	bu_log("arbn\treject delete without selection\t%s\t%s\n", unit,
	    rejected ? "pass" : "fail");
	if (!rejected)
	    ++failures;
	rt_edit_set_edflag(edit, ECMD_ARBN_PLANE_ADD);
	edit->e_inpara = 4;
	HSET(edit->e_para, 0, 0, 0, inch_to_mm / scale);
	rejected = rt_edit_process(edit) == BRLCAD_ERROR &&
	    same_planes(arbn, expected, original_count) &&
	    ((struct rt_arbn_edit_local *)edit->ipe_ptr)->plane_index == -1;
	bu_log("arbn\treject zero add normal\t%s\t%s\n", unit,
	    rejected ? "pass" : "fail");
	if (!rejected)
	    ++failures;
	edit->e_inpara = 4;
	HSET(edit->e_para, 0, 0, 2, 2 * inch_to_mm / scale);
	HSET(expected[original_count], 0, 0, 1, inch_to_mm);
	passed = rt_edit_process(edit) == BRLCAD_OK &&
	    same_planes(arbn, expected, original_count + 1);
	if (passed) {
	    const point_t expected_normal = {0, 0, 1};
	    passed = VNEAR_EQUAL(arbn->eqn[original_count], expected_normal,
		VUNITIZE_TOL) &&
		NEAR_EQUAL(arbn->eqn[original_count][3], inch_to_mm,
		    VUNITIZE_TOL) &&
		NEAR_EQUAL(edit->e_para[3], 2 * inch_to_mm / scale,
		    VUNITIZE_TOL) &&
		((struct rt_arbn_edit_local *)edit->ipe_ptr)->plane_index ==
		    (int)original_count;
	}
	bu_log("arbn\tadd plane\t%s\t%s\n", unit,
	    passed ? "pass" : "fail");
	if (!passed)
	    ++failures;
	if (arbn->neqn == original_count + 1) {
	    rt_edit_set_edflag(edit, ECMD_ARBN_PLANE_DEL);
	    edit->e_inpara = 0;
	    bool deleted = rt_edit_process(edit) == BRLCAD_OK &&
		same_planes(arbn, expected, original_count) &&
		((struct rt_arbn_edit_local *)edit->ipe_ptr)->plane_index == -1;
	    bu_log("arbn\tdelete plane\t%s\t%s\n", unit,
		deleted ? "pass" : "fail");
	    if (!deleted)
		++failures;
	}
	rt_edit_destroy(edit);
    }
    return failures;
}

static int
arbn_transform_matrix(struct db_i *dbip, struct db_full_path *path,
		      struct bn_tol *tol, struct bview *view)
{
    const fastf_t inch_to_mm = 25.4;
    const fastf_t scales[] = {1.0, inch_to_mm};
    const struct {
	int command;
	const char *name;
    } cases[] = {
	{RT_PARAMS_EDIT_TRANS, "translate"},
	{RT_PARAMS_EDIT_SCALE, "scale"},
	{RT_PARAMS_EDIT_ROT, "rotate"}
    };
    int failures = 0;

    view->gv_rotate_about = 'k';
    for (fastf_t scale : scales) {
	dbip->dbi_local2base = scale;
	dbip->dbi_base2local = 1.0 / scale;
	const char *unit = EQUAL(scale, 1.0) ? "mm" : "in";
	for (const auto &test : cases) {
	    struct rt_edit *edit = rt_edit_create(path, dbip, tol, view);
	    if (!edit) {
		bu_log("arbn\t%s\t%s\tfail: edit creation\n",
		    test.name, unit);
		++failures;
		continue;
	    }
	    struct rt_arbn_internal *arbn =
		(struct rt_arbn_internal *)edit->es_int.idb_ptr;
	    if (arbn->neqn != ARBN_CUBE_PLANE_COUNT) {
		bu_log("arbn\t%s\t%s\tfail: fixture has %zu planes\n",
		    test.name, unit, arbn->neqn);
		++failures;
		rt_edit_destroy(edit);
		continue;
	    }
	    plane_t expected[ARBN_CUBE_PLANE_COUNT];
	    for (size_t i = 0; i < arbn->neqn; ++i)
		HMOVE(expected[i], arbn->eqn[i]);
	    point_t numeric_input = VINIT_ZERO;
	    edit->mv_context = 1;
	    VSETALL(edit->e_keypoint, 0);
	    EDOBJ[ID_ARBN].ft_set_edit_mode(edit, test.command);
	    switch (test.command) {
		case RT_PARAMS_EDIT_TRANS: {
		    const vect_t shift = {
			inch_to_mm, inch_to_mm / 2, -inch_to_mm / 4
		    };
		    VSCALE(numeric_input, shift, 1.0 / scale);
		    edit->e_inpara = 3;
		    VMOVE(edit->e_para, numeric_input);
		    for (size_t i = 0; i < arbn->neqn; ++i)
			expected[i][W] += VDOT(expected[i], shift);
		    break;
		}
		case RT_PARAMS_EDIT_SCALE:
		    edit->e_inpara = 1;
		    edit->e_para[0] = numeric_input[0] = 2.5;
		    for (size_t i = 0; i < arbn->neqn; ++i)
			expected[i][W] *= 2.5;
		    break;
		case RT_PARAMS_EDIT_ROT:
		    edit->e_inpara = 3;
		    VSET(numeric_input, 90, 0, 0);
		    VMOVE(edit->e_para, numeric_input);
		    for (size_t i = 0; i < arbn->neqn; ++i) {
			const fastf_t y = expected[i][Y];
			expected[i][Y] = -expected[i][Z];
			expected[i][Z] = y;
		    }
		    break;
	    }
	    bool passed = rt_edit_process(edit) == BRLCAD_OK &&
		same_planes(arbn, expected, ARBN_CUBE_PLANE_COUNT) &&
		VNEAR_EQUAL(edit->e_para, numeric_input, VUNITIZE_TOL);
	    bu_log("arbn\t%s\t%s\t%s\n", test.name, unit,
		passed ? "pass" : "fail");
	    if (!passed) {
		bu_log("arbn transform result: %s\n",
		    bu_vls_cstr(edit->log_str));
		++failures;
	    }
	    rt_edit_destroy(edit);
	}
    }
    return failures;
}

int
rt_edit_test_arbn(void)
{
    struct db_i *dbip = db_open_inmem();
    if (dbip == DBI_NULL)
	bu_exit(1, "ERROR: Unable to create database\n");

    struct rt_wdb *wdbp = wdb_dbopen(dbip, RT_WDB_TYPE_DB_INMEM);
    struct directory *dp = make_arbn(wdbp);

    struct bn_tol tol = BN_TOL_INIT_TOL;
    struct db_full_path fp;
    db_full_path_init(&fp);
    db_add_node_to_full_path(&fp, dp);

    struct bview *v;
    BU_GET(v, struct bview);
    bv_init(v, NULL);
    VSET(v->gv_aet, 45, 35, 0);
    bv_mat_aet(v);
    v->gv_size = 10.0;
    v->gv_isize = 0.1;
    v->gv_scale = 5.0;
    bv_update(v);
    bu_vls_sprintf(&v->gv_name, "default");
    v->gv_width  = 512;
    v->gv_height = 512;

    struct rt_edit *s = rt_edit_create(&fp, dbip, &tol, v);
    s->mv_context = 1;
    s->local2base = 1.0;
    s->base2local = 1.0;

    struct rt_arbn_internal *aip =
	(struct rt_arbn_internal *)s->es_int.idb_ptr;
    RT_ARBN_CK_MAGIC(aip);

    /* ================================================================
     * Test 1: descriptor is accessible and has 6 commands
     * ================================================================*/
    const struct rt_edit_prim_desc *desc =
	(*EDOBJ[dp->d_minor_type].ft_edit_desc)();
    if (!desc)
	bu_exit(1, "ERROR: arbn edit_desc is NULL\n");
    if (desc->ncmd != 6)
	bu_exit(1, "ERROR: arbn descriptor should have 6 cmds, got %d\n",
		desc->ncmd);
    bu_log("TEST 1 PASS: arbn descriptor has %d commands\n", desc->ncmd);

    /* ================================================================
     * Test 2: ECMD_ARBN_PLANE_SELECT — select plane 2
     * ================================================================*/
    (*EDOBJ[dp->d_minor_type].ft_set_edit_mode)(s, ECMD_ARBN_PLANE_SELECT);
    s->e_inpara = 1;
    s->e_para[0] = 2.0;
    rt_edit_process(s);

    {
	struct rt_arbn_edit_local *e = (struct rt_arbn_edit_local *)s->ipe_ptr;
	if (e->plane_index != 2)
	    bu_exit(1, "ERROR: plane_select stored %d (expected 2)\n",
		    e->plane_index);
	bu_log("TEST 2 PASS: plane_index = %d\n", e->plane_index);
    }

    /* ================================================================
     * Test 3: ECMD_ARBN_PLANE_SET_DIST — change d of plane 2 to 5
     * ================================================================*/
    (*EDOBJ[dp->d_minor_type].ft_set_edit_mode)(s, ECMD_ARBN_PLANE_SET_DIST);
    s->e_inpara = 1;
    s->e_para[0] = 5.0;
    rt_edit_process(s);

    if (!NEAR_EQUAL(aip->eqn[2][3], 5.0, SMALL_FASTF))
	bu_exit(1, "ERROR: plane_set_dist: d=%g (expected 5)\n", aip->eqn[2][3]);
    bu_log("TEST 3 PASS: plane 2 d = %g\n", aip->eqn[2][3]);

    /* ================================================================
     * Test 4: ECMD_ARBN_PLANE_SET_NORM — replace normal of plane 2
     * ================================================================*/
    (*EDOBJ[dp->d_minor_type].ft_set_edit_mode)(s, ECMD_ARBN_PLANE_SET_NORM);
    s->e_inpara = 3;
    VSET(s->e_para, 0.0, 1.0, 0.0);
    rt_edit_process(s);

    {
	vect_t expected = {0, 1, 0};
	vect_t got;
	VSET(got, aip->eqn[2][0], aip->eqn[2][1], aip->eqn[2][2]);
	if (!VNEAR_EQUAL(got, expected, SMALL_FASTF))
	    bu_exit(1, "ERROR: plane_set_norm: N=(%g,%g,%g) expected (0,1,0)\n",
		    V3ARGS(got));
	bu_log("TEST 4 PASS: plane 2 normal = (%g,%g,%g)\n", V3ARGS(got));
    }

    /* ================================================================
     * Test 5: ECMD_ARBN_PLANE_ROTATE — rotate plane 2 normal by 90 deg around Z
     * Normal is currently (0,1,0); after 90-deg Z rotation → (-1,0,0).
     * ================================================================*/
    (*EDOBJ[dp->d_minor_type].ft_set_edit_mode)(s, ECMD_ARBN_PLANE_ROTATE);
    s->e_inpara = 3;
    VSET(s->e_para, 0.0, 0.0, 90.0);   /* rx=0, ry=0, rz=90 */
    rt_edit_process(s);

    {
	vect_t expected = {-1, 0, 0};
	vect_t got;
	VSET(got, aip->eqn[2][0], aip->eqn[2][1], aip->eqn[2][2]);
	if (!VNEAR_EQUAL(got, expected, 1e-9))
	    bu_exit(1, "ERROR: plane_rotate: N=(%g,%g,%g) expected (-1,0,0)\n",
		    V3ARGS(got));
	bu_log("TEST 5 PASS: plane 2 rotated normal = (%g,%g,%g)\n", V3ARGS(got));
    }

    /* ================================================================
     * Test 6: ECMD_ARBN_PLANE_ADD — append a 7th plane
     * ================================================================*/
    size_t before_n = aip->neqn;
    (*EDOBJ[dp->d_minor_type].ft_set_edit_mode)(s, ECMD_ARBN_PLANE_ADD);
    s->e_inpara = 4;
    s->e_para[0] = 1.0;   /* nx */
    s->e_para[1] = 1.0;   /* ny */
    s->e_para[2] = 1.0;   /* nz */
    s->e_para[3] = 3.0;   /* d  */
    rt_edit_process(s);

    if (aip->neqn != before_n + 1)
	bu_exit(1, "ERROR: plane_add: neqn=%zu (expected %zu)\n",
		aip->neqn, before_n + 1);
    bu_log("TEST 6 PASS: neqn now %zu (was %zu)\n", aip->neqn, before_n);

    /* ================================================================
     * Test 7: ECMD_ARBN_PLANE_DEL — remove the newly-selected plane
     * After PLANE_ADD, the new plane is auto-selected.
     * ================================================================*/
    size_t before_del = aip->neqn;
    (*EDOBJ[dp->d_minor_type].ft_set_edit_mode)(s, ECMD_ARBN_PLANE_DEL);
    s->e_inpara = 0;
    rt_edit_process(s);

    if (aip->neqn != before_del - 1)
	bu_exit(1, "ERROR: plane_del: neqn=%zu (expected %zu)\n",
		aip->neqn, before_del - 1);
    bu_log("TEST 7 PASS: neqn now %zu (was %zu)\n", aip->neqn, before_del);

    /* ================================================================
     * Test 8: rt_edit_arbn_get_params returns plane index
     * ================================================================*/
    /* Re-select plane 0 so we have something to query */
    (*EDOBJ[dp->d_minor_type].ft_set_edit_mode)(s, ECMD_ARBN_PLANE_SELECT);
    s->e_inpara = 1;
    s->e_para[0] = 0.0;
    rt_edit_process(s);

    fastf_t vals[4] = {0};
    int nv = (*EDOBJ[dp->d_minor_type].ft_edit_get_params)(s, ECMD_ARBN_PLANE_SELECT, vals);
    if (nv != 1 || (int)vals[0] != 0)
	bu_exit(1, "ERROR: get_params(SELECT): nv=%d vals[0]=%g\n", nv, vals[0]);
    bu_log("TEST 8 PASS: get_params(SELECT) returns plane_index=%g\n", vals[0]);

    rt_edit_destroy(s);
    int failures = arbn_unit_matrix(dbip, &fp, &tol);
    failures += arbn_transform_matrix(dbip, &fp, &tol, v);
    db_close(dbip);
    if (!failures)
	bu_log("All ARBN edit tests PASSED\n");
    return failures ? BRLCAD_ERROR : BRLCAD_OK;
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
