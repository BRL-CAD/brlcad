/*                K N O B _ O P E R A T I O N _ M A T R I X . C P P
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
/** @file knob_operation_matrix.cpp
 *
 * Check absolute knob scale and translation sequences against complete
 * ellipsoid geometry and transformed points in millimeter and inch databases.
 */

#include "common.h"

#include "bu/log.h"
#include "raytrace.h"
#include "rt/rt_ecmds.h"

struct directory *make_ell(struct rt_wdb *);

static const fastf_t inch_to_mm = 25.4;
static const fastf_t view_size = 100.0;
static const fastf_t knob_values[] = {0.5, 1.0, 0.0};
static const fastf_t scale_values[] = {2.5, 4.0, 1.0};
static const fastf_t translation_values[] = {2.0, 3.0, 0.0};
static const int axis_scale_commands[] = {
    RT_MATRIX_EDIT_SCALE_X, RT_MATRIX_EDIT_SCALE_Y, RT_MATRIX_EDIT_SCALE_Z
};

static bool
same_ell(const struct rt_edit *edit, fastf_t a, fastf_t b, fastf_t c)
{
    const struct rt_ell_internal *ell =
	(const struct rt_ell_internal *)edit->es_int.idb_ptr;
    point_t center = {10, 5, 20};
    vect_t axis_a = {a, 0, 0};
    vect_t axis_b = {0, b, 0};
    vect_t axis_c = {0, 0, c};
    return ell && VNEAR_EQUAL(ell->v, center, VUNITIZE_TOL) &&
	VNEAR_EQUAL(ell->a, axis_a, VUNITIZE_TOL) &&
	VNEAR_EQUAL(ell->b, axis_b, VUNITIZE_TOL) &&
	VNEAR_EQUAL(ell->c, axis_c, VUNITIZE_TOL);
}

static bool
set_absolute_scale(struct rt_edit *edit, struct bview *view, fastf_t value)
{
    vect_t rotation = VINIT_ZERO;
    vect_t translation = VINIT_ZERO;
    int do_rotate = 0;
    int do_translate = 0;
    int do_scale = 0;
    return rt_edit_knob_cmd_process(edit, &rotation, &do_rotate,
	&translation, &do_translate, &do_scale, view, "aS", value,
	'k', 0, NULL) == BRLCAD_OK && !do_rotate && !do_translate &&
	do_scale && NEAR_EQUAL(edit->k.sca_abs, value, VUNITIZE_TOL);
}

static int
check_solid(struct db_full_path *path, struct db_i *dbip,
	    struct bn_tol *tol, struct bview *view, int specific,
	    const char *unit)
{
    struct rt_edit *edit = rt_edit_create(path, dbip, tol, view);
    if (!edit)
	return 1;

    int command = RT_PARAMS_EDIT_SCALE;
    if (specific) {
	const struct rt_edit_prim_desc *desc = EDOBJ[ID_ELL].ft_edit_desc();
	if (!desc || !desc->ncmd) {
	    rt_edit_destroy(edit);
	    return 1;
	}
	command = desc->cmds[0].cmd_id;
	EDOBJ[ID_ELL].ft_set_edit_mode(edit, command);
    } else {
	rt_edit_set_edflag(edit, command);
    }

    int failures = 0;
    for (size_t i = 0; i < sizeof(knob_values) / sizeof(knob_values[0]); ++i) {
	if (!set_absolute_scale(edit, view, knob_values[i])) {
	    ++failures;
	    continue;
	}
	rt_knob_edit_sca(edit, 0);
	fastf_t a = 4.0 * scale_values[i];
	fastf_t b = specific ? 3.0 : 3.0 * scale_values[i];
	fastf_t c = specific ? 2.0 : 2.0 * scale_values[i];
	if (!same_ell(edit, a, b, c) || edit->edit_flag != command ||
	    !NEAR_EQUAL(edit->acc_sc_sol, scale_values[i], VUNITIZE_TOL)) {
	    bu_log("knob solid %s %s step %zu failed\n",
		unit, specific ? "A" : "uniform", i);
	    ++failures;
	}
	/* A consumed knob factor must not be replayed by a later process call. */
	if (rt_edit_process(edit) != BRLCAD_OK ||
	    !same_ell(edit, a, b, c)) {
	    bu_log("knob solid %s %s step %zu repeated scale\n",
		unit, specific ? "A" : "uniform", i);
	    ++failures;
	}
    }

    rt_edit_destroy(edit);
    return failures;
}

static int
check_matrix(struct db_full_path *path, struct db_i *dbip,
	     struct bn_tol *tol, struct bview *view, int command,
	     const char *unit)
{
    struct rt_edit *edit = rt_edit_create(path, dbip, tol, view);
    if (!edit)
	return 1;
    rt_edit_set_edflag(edit, command);

    int failures = 0;
    for (size_t i = 0; i < sizeof(knob_values) / sizeof(knob_values[0]); ++i) {
	if (!set_absolute_scale(edit, view, knob_values[i])) {
	    ++failures;
	    continue;
	}
	rt_knob_edit_sca(edit, 1);
	point_t center = {10, 5, 20};
	point_t actual;
	MAT4X3PNT(actual, edit->model_changes, center);
	bool correct = VNEAR_EQUAL(actual, center, VUNITIZE_TOL);
	for (int axis = 0; axis < 3; ++axis) {
	    point_t probe;
	    point_t expected;
	    VMOVE(probe, center);
	    VMOVE(expected, center);
	    probe[axis] += 1.0;
	    fastf_t factor = (command == RT_MATRIX_EDIT_SCALE ||
		command == axis_scale_commands[axis]) ? scale_values[i] : 1.0;
	    expected[axis] += factor;
	    MAT4X3PNT(actual, edit->model_changes, probe);
	    correct = correct && VNEAR_EQUAL(actual, expected, VUNITIZE_TOL);
	}
	if (!correct || !same_ell(edit, 4, 3, 2)) {
	    bu_log("knob matrix %s command %d step %zu failed\n",
		unit, command, i);
	    ++failures;
	}
    }

    rt_edit_destroy(edit);
    return failures;
}

static int
check_translation(struct db_full_path *path, struct db_i *dbip,
		  struct bn_tol *tol, struct bview *view,
		  fastf_t local2base, const char *unit)
{
    struct rt_edit *edit = rt_edit_create(path, dbip, tol, view);
    if (!edit)
	return 1;
    edit->mv_context = 1;

    int failures = 0;
    for (size_t i = 0;
	 i < sizeof(translation_values) / sizeof(translation_values[0]); ++i) {
	vect_t rotation = VINIT_ZERO;
	vect_t translation = VINIT_ZERO;
	int do_rotate = 0;
	int do_translate = 0;
	int do_scale = 0;
	const struct rt_ell_internal *ell =
	    (const struct rt_ell_internal *)edit->es_int.idb_ptr;
	VMOVE(edit->curr_e_axes_pos, ell->v);
	if (rt_edit_knob_cmd_process(edit, &rotation, &do_rotate,
		&translation, &do_translate, &do_scale, view, "aX",
		translation_values[i], 'k', 0, NULL) != BRLCAD_OK ||
	    do_rotate || !do_translate || do_scale) {
	    ++failures;
	    continue;
	}
	rt_knob_edit_tran(edit, 'm', 0, translation);
	point_t expected_center = {
	    10.0 + translation_values[i] * local2base, 5.0, 20.0
	};
	vect_t expected_a = {4, 0, 0};
	vect_t expected_b = {0, 3, 0};
	vect_t expected_c = {0, 0, 2};
	ell = (const struct rt_ell_internal *)edit->es_int.idb_ptr;
	if (!VNEAR_EQUAL(ell->v, expected_center, VUNITIZE_TOL) ||
	    !VNEAR_EQUAL(ell->a, expected_a, VUNITIZE_TOL) ||
	    !VNEAR_EQUAL(ell->b, expected_b, VUNITIZE_TOL) ||
	    !VNEAR_EQUAL(ell->c, expected_c, VUNITIZE_TOL)) {
	    bu_log("knob translate %s step %zu failed\n", unit, i);
	    ++failures;
	}
    }

    rt_edit_destroy(edit);
    return failures;
}

static int
check_unit(fastf_t local2base, const char *unit)
{
    struct db_i *dbip = db_open_inmem();
    if (dbip == DBI_NULL)
	return 1;
    dbip->dbi_local2base = local2base;
    dbip->dbi_base2local = 1.0 / local2base;
    struct rt_wdb *wdbp = wdb_dbopen(dbip, RT_WDB_TYPE_DB_INMEM);
    if (!wdbp) {
	db_close(dbip);
	return 1;
    }
    struct directory *dp = make_ell(wdbp);
    struct db_full_path path;
    db_full_path_init(&path);
    db_add_node_to_full_path(&path, dp);
    struct bn_tol tol = BN_TOL_INIT_TOL;
    struct bview *view;
    BU_GET(view, struct bview);
    bv_init(view, NULL);
    view->gv_local2base = local2base;
    view->gv_base2local = 1.0 / local2base;
    view->gv_coord = 'm';
    view->gv_size = view_size;
    view->gv_isize = 1.0 / view->gv_size;
    view->gv_scale = 0.5 * view->gv_size;
    bv_update(view);

    int failures = check_solid(&path, dbip, &tol, view, 0, unit);
    failures += check_solid(&path, dbip, &tol, view, 1, unit);
    failures += check_matrix(&path, dbip, &tol, view,
	RT_MATRIX_EDIT_SCALE, unit);
    for (size_t i = 0;
	 i < sizeof(axis_scale_commands) / sizeof(axis_scale_commands[0]); ++i)
	failures += check_matrix(&path, dbip, &tol, view,
		axis_scale_commands[i], unit);
    failures += check_translation(&path, dbip, &tol, view,
	local2base, unit);

    bv_free(view);
    db_free_full_path(&path);
    db_close(dbip);
    bu_log("knob operation matrix %s: %s\n", unit,
	failures ? "fail" : "pass");
    return failures;
}

int
rt_edit_test_knob_operation_matrix(void)
{
    int failures = check_unit(1.0, "mm");
    failures += check_unit(inch_to_mm, "in");
    return failures ? BRLCAD_ERROR : BRLCAD_OK;
}

/*
 * Local Variables:
 * mode: C++
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
