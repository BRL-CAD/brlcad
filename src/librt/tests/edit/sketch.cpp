/*                       S K E T C H . C P P
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
/** @file sketch.cpp
 *
 * Test editing of SKETCH (2-D sketch) primitive parameters via
 * the new edsketch.c ECMD suite.
 *
 * Test sketch layout (4 vertices, 1 line segment):
 *
 *   verts[0] = (0, 0)
 *   verts[1] = (10, 0)
 *   verts[2] = (10, 10)
 *   verts[3] = (0, 10)
 *   segments: line 0→1
 *
 * V = (0,0,0), u_vec = (1,0,0), v_vec = (0,1,0)
 */

#include "common.h"

#include <math.h>
#include <string.h>
#include <vector>

#include "vmath.h"
#include "bu/log.h"
#include "bu/malloc.h"
#include "nmg.h"
#include "raytrace.h"
#include "rt/geom.h"
#include "rt/primitives/sketch.h"
#include "rt/rt_ecmds.h"

/* ------------------------------------------------------------------ */
/* Build the test sketch                                               */
/* ------------------------------------------------------------------ */

static struct directory *
make_test_sketch(struct rt_wdb *wdbp)
{
    struct rt_sketch_internal *skt;
    BU_ALLOC(skt, struct rt_sketch_internal);
    skt->magic = RT_SKETCH_INTERNAL_MAGIC;
    VSET(skt->V,     0, 0, 0);
    VSET(skt->u_vec, 1, 0, 0);
    VSET(skt->v_vec, 0, 1, 0);

    skt->vert_count = 4;
    skt->verts = (point2d_t *)bu_malloc(4 * sizeof(point2d_t), "sketch verts");
    V2SET(skt->verts[0],  0,  0);
    V2SET(skt->verts[1], 10,  0);
    V2SET(skt->verts[2], 10, 10);
    V2SET(skt->verts[3],  0, 10);

    /* One line segment: vert 0 → vert 1 */
    struct line_seg *ls;
    BU_ALLOC(ls, struct line_seg);
    ls->magic = CURVE_LSEG_MAGIC;
    ls->start = 0;
    ls->end   = 1;

    skt->curve.count   = 1;
    skt->curve.segment = (void **)bu_malloc(sizeof(void *), "skt segs");
    skt->curve.reverse = (int  *)bu_malloc(sizeof(int),     "skt reverse");
    skt->curve.segment[0] = (void *)ls;
    skt->curve.reverse[0] = 0;

    wdb_export(wdbp, "test_sketch", (void *)skt, ID_SKETCH, 1.0);

    struct directory *dp = db_lookup(wdbp->dbip, "test_sketch", LOOKUP_QUIET);
    if (!dp)
	bu_exit(1, "ERROR: failed to look up test_sketch\n");
    return dp;
}

struct sketch_expected {
    std::vector<fastf_t> uv;
    std::vector<int> lines;
    int vertex;
    int segment;
};

static const fastf_t sketch_inch_to_mm = 25.4;

static struct sketch_expected
sketch_initial_state(void)
{
    return {{0, 0, 10, 0, 10, 10, 0, 10}, {0, 1}, -1, -1};
}

static bool
sketch_same_state(const struct rt_edit *edit, const struct sketch_expected *expected)
{
    const struct rt_sketch_internal *sketch =
	(const struct rt_sketch_internal *)edit->es_int.idb_ptr;
    const struct rt_sketch_edit *selection =
	(const struct rt_sketch_edit *)edit->ipe_ptr;
    const point_t origin = {0, 0, 0};
    const vect_t u_axis = {1, 0, 0};
    const vect_t v_axis = {0, 1, 0};
    if (!selection || expected->uv.size() % 2 ||
	expected->lines.size() % 2 ||
	(sketch->vert_count && !sketch->verts) ||
	(sketch->curve.count &&
	 (!sketch->curve.segment || !sketch->curve.reverse)) ||
	sketch->vert_count != expected->uv.size() / 2 ||
	sketch->curve.count != expected->lines.size() / 2 ||
	selection->curr_vert != expected->vertex ||
	selection->curr_seg != expected->segment ||
	!VNEAR_EQUAL(sketch->V, origin, VUNITIZE_TOL) ||
	!VNEAR_EQUAL(sketch->u_vec, u_axis, VUNITIZE_TOL) ||
	!VNEAR_EQUAL(sketch->v_vec, v_axis, VUNITIZE_TOL))
	return false;
    for (size_t i = 0; i < expected->uv.size(); ++i) {
	if (!NEAR_EQUAL(sketch->verts[i / 2][i % 2], expected->uv[i],
		VUNITIZE_TOL))
	    return false;
    }
    for (size_t i = 0; i < sketch->curve.count; ++i) {
	const struct line_seg *line =
	    (const struct line_seg *)sketch->curve.segment[i];
	if (!line || line->magic != CURVE_LSEG_MAGIC ||
	    line->start != expected->lines[2 * i] ||
	    line->end != expected->lines[2 * i + 1] ||
	    sketch->curve.reverse[i])
	    return false;
    }
    return true;
}

static bool
sketch_initial_line_geometry(const struct rt_sketch_internal *sketch,
			     size_t expected_segments,
			     size_t expected_vertices)
{
    const struct sketch_expected initial = sketch_initial_state();
    const point_t origin = {0, 0, 0};
    const vect_t u_axis = {1, 0, 0};
    const vect_t v_axis = {0, 1, 0};
    if (!sketch->verts || !sketch->curve.segment ||
	!sketch->curve.reverse ||
	sketch->vert_count != expected_vertices ||
	expected_vertices < initial.uv.size() / 2 ||
	sketch->curve.count != expected_segments ||
	!VNEAR_EQUAL(sketch->V, origin, VUNITIZE_TOL) ||
	!VNEAR_EQUAL(sketch->u_vec, u_axis, VUNITIZE_TOL) ||
	!VNEAR_EQUAL(sketch->v_vec, v_axis, VUNITIZE_TOL))
	return false;
    for (size_t i = 0; i < initial.uv.size(); ++i)
	if (!NEAR_EQUAL(sketch->verts[i / 2][i % 2], initial.uv[i],
		VUNITIZE_TOL))
	    return false;
    const struct line_seg *line =
	(const struct line_seg *)sketch->curve.segment[0];
    return line && line->magic == CURVE_LSEG_MAGIC &&
	line->start == 0 && line->end == 1 &&
	!sketch->curve.reverse[0];
}

static bool
sketch_arc_state(const struct rt_edit *edit, fastf_t radius, int left)
{
    const struct rt_sketch_internal *sketch =
	(const struct rt_sketch_internal *)edit->es_int.idb_ptr;
    const struct rt_sketch_edit *selection =
	(const struct rt_sketch_edit *)edit->ipe_ptr;
    if (!selection || selection->curr_vert != -1 ||
	selection->curr_seg != -1 || !sketch_initial_line_geometry(sketch, 2, 4))
	return false;
    const struct carc_seg *arc =
	(const struct carc_seg *)sketch->curve.segment[1];
    return arc && arc->magic == CURVE_CARC_MAGIC && arc->start == 1 &&
	arc->end == 2 && NEAR_EQUAL(arc->radius, radius, VUNITIZE_TOL) &&
	arc->center_is_left == left && arc->orientation == 0 &&
	arc->center == -1 && !sketch->curve.reverse[1];
}

static bool
sketch_nurb_state(const struct rt_edit *edit, const fastf_t *knots,
		  const fastf_t *weights, int selected_segment)
{
    const struct rt_sketch_internal *sketch =
	(const struct rt_sketch_internal *)edit->es_int.idb_ptr;
    const struct rt_sketch_edit *selection =
	(const struct rt_sketch_edit *)edit->ipe_ptr;
    const int control_points[] = {0, 1, 2, 3};
    const size_t control_point_count =
	sizeof(control_points) / sizeof(control_points[0]);
    if (!selection || selection->curr_vert != -1 ||
	selection->curr_seg != selected_segment ||
	!sketch_initial_line_geometry(sketch, 2, 4))
	return false;
    const struct nurb_seg *nurb =
	(const struct nurb_seg *)sketch->curve.segment[1];
    if (!nurb || nurb->magic != CURVE_NURB_MAGIC || nurb->order != 3 ||
	nurb->c_size != (int)control_point_count ||
	nurb->k.k_size != nurb->order + nurb->c_size ||
	!nurb->ctl_points || !nurb->k.knots ||
	sketch->curve.reverse[1] ||
	(weights ? !nurb->weights || !RT_NURB_IS_PT_RATIONAL(nurb->pt_type)
	    : nurb->weights || RT_NURB_IS_PT_RATIONAL(nurb->pt_type)))
	return false;
    for (size_t i = 0; i < control_point_count; ++i)
	if (nurb->ctl_points[i] != control_points[i] ||
	    (weights && !NEAR_EQUAL(nurb->weights[i], weights[i], VUNITIZE_TOL)))
	    return false;
    for (int i = 0; i < nurb->k.k_size; ++i)
	if (!NEAR_EQUAL(nurb->k.knots[i], knots[i], VUNITIZE_TOL))
	    return false;
    return true;
}

static bool
sketch_line_arc_state(const struct rt_edit *edit, fastf_t radius,
		      int left, int orientation)
{
    const struct rt_sketch_internal *sketch =
	(const struct rt_sketch_internal *)edit->es_int.idb_ptr;
    const struct rt_sketch_edit *selection =
	(const struct rt_sketch_edit *)edit->ipe_ptr;
    if (!selection || selection->curr_vert != -1 ||
	selection->curr_seg != -1 ||
	!sketch_initial_line_geometry(sketch, 3, 4))
	return false;
    const struct line_seg *line =
	(const struct line_seg *)sketch->curve.segment[1];
    const struct carc_seg *arc =
	(const struct carc_seg *)sketch->curve.segment[2];
    return line && arc && line->magic == CURVE_LSEG_MAGIC &&
	line->start == 2 && line->end == 3 &&
	arc->magic == CURVE_CARC_MAGIC && arc->start == 1 &&
	arc->end == 2 && NEAR_EQUAL(arc->radius, radius, VUNITIZE_TOL) &&
	arc->center_is_left == left && arc->orientation == orientation &&
	arc->center == -1 && !sketch->curve.reverse[1] &&
	!sketch->curve.reverse[2];
}

static bool
sketch_first_arc_geometry(const struct rt_sketch_internal *sketch,
			  size_t segment_count)
{
    if (!sketch_initial_line_geometry(sketch, segment_count, 4))
	return false;
    const struct carc_seg *arc =
	(const struct carc_seg *)sketch->curve.segment[1];
    return arc && arc->magic == CURVE_CARC_MAGIC &&
	arc->start == 2 && arc->end == 3 &&
	NEAR_EQUAL(arc->radius, 8, VUNITIZE_TOL) &&
	arc->center_is_left == 1 && arc->orientation == 0 &&
	arc->center == -1 && !sketch->curve.reverse[1];
}

static bool
sketch_two_arc_state(const struct rt_edit *edit, bool target_present,
		     fastf_t radius, int left, int orientation)
{
    const struct rt_sketch_internal *sketch =
	(const struct rt_sketch_internal *)edit->es_int.idb_ptr;
    const struct rt_sketch_edit *selection =
	(const struct rt_sketch_edit *)edit->ipe_ptr;
    if (!selection || selection->curr_vert != -1 ||
	selection->curr_seg != -1 ||
	!sketch_first_arc_geometry(sketch, target_present ? 3 : 2))
	return false;
    if (!target_present)
	return true;
    const struct carc_seg *arc =
	(const struct carc_seg *)sketch->curve.segment[2];
    return arc && arc->magic == CURVE_CARC_MAGIC &&
	arc->start == 1 && arc->end == 2 &&
	NEAR_EQUAL(arc->radius, radius, VUNITIZE_TOL) &&
	arc->center_is_left == left && arc->orientation == orientation &&
	arc->center == -1 && !sketch->curve.reverse[2];
}

static bool
sketch_split_arc_state(const struct rt_edit *edit)
{
    const struct rt_sketch_internal *sketch =
	(const struct rt_sketch_internal *)edit->es_int.idb_ptr;
    const struct rt_sketch_edit *selection =
	(const struct rt_sketch_edit *)edit->ipe_ptr;
    /* The radius-8 arc has a half-chord of 5 and center at 10-sqrt(39). */
    const fastf_t split_u = 18.0 - sqrt(39.0);
    if (!selection || selection->curr_vert != -1 ||
	selection->curr_seg != -1 ||
	!sketch_initial_line_geometry(sketch, 3, 5) ||
	!NEAR_EQUAL(sketch->verts[4][0], split_u, VUNITIZE_TOL) ||
	!NEAR_EQUAL(sketch->verts[4][1], 5.0, VUNITIZE_TOL))
	return false;
    const struct carc_seg *first =
	(const struct carc_seg *)sketch->curve.segment[1];
    const struct carc_seg *second =
	(const struct carc_seg *)sketch->curve.segment[2];
    return first && second && first->magic == CURVE_CARC_MAGIC &&
	second->magic == CURVE_CARC_MAGIC &&
	first->start == 1 && first->end == 4 &&
	second->start == 4 && second->end == 2 &&
	NEAR_EQUAL(first->radius, 8, VUNITIZE_TOL) &&
	NEAR_EQUAL(second->radius, 8, VUNITIZE_TOL) &&
	first->center_is_left == 1 && second->center_is_left == 1 &&
	first->orientation == 0 && second->orientation == 0 &&
	first->center == -1 && second->center == -1 &&
	!sketch->curve.reverse[1] && !sketch->curve.reverse[2];
}

template <typename StateCheck>
static int
sketch_step(struct rt_edit *edit, const char *unit, const char *name,
	    int command, const fastf_t *params, int count,
	    StateCheck state_check, bool reject)
{
    if (count < 0 || count > RT_EDIT_MAXPARA || (count && !params))
	return 1;
    rt_edit_set_edflag(edit, command);
    edit->e_inpara = count;
    for (int i = 0; i < count; ++i)
	edit->e_para[i] = params[i];
    int result = rt_edit_process(edit);
    bool ok = (reject ? result != BRLCAD_OK : result == BRLCAD_OK) &&
	state_check(edit);
    bu_log("sketch\t%s\t%s\t%s\n", name, unit, ok ? "pass" : "fail");
    if (!ok)
	bu_log("sketch %s returned %d: %s\n", name, result,
		bu_vls_cstr(edit->log_str));
    return ok ? 0 : 1;
}

static int
sketch_matrix_step(struct rt_edit *edit, const char *unit, const char *name,
		   int command, const fastf_t *params, int count,
		   const struct sketch_expected *expected, bool reject = false)
{
    return sketch_step(edit, unit, name, command, params, count,
	[expected](const struct rt_edit *e) {
	    return sketch_same_state(e, expected);
	}, reject);
}

static int
sketch_arc_step(struct rt_edit *edit, const char *unit, const char *name,
		int command, const fastf_t *params, int count,
		fastf_t radius, int left, bool reject = false)
{
    return sketch_step(edit, unit, name, command, params, count,
	[radius, left](const struct rt_edit *e) {
	    return sketch_arc_state(e, radius, left);
	}, reject);
}

static int
sketch_nurb_step(struct rt_edit *edit, const char *unit, const char *name,
		 int command, const fastf_t *params, int count,
		 const fastf_t *knots, const fastf_t *weights,
		 int selected_segment, bool reject = false)
{
    return sketch_step(edit, unit, name, command, params, count,
	[knots, weights, selected_segment](const struct rt_edit *e) {
	    return sketch_nurb_state(e, knots, weights, selected_segment);
	}, reject);
}

static int
sketch_line_arc_step(struct rt_edit *edit, const char *unit, const char *name,
		     int command, const fastf_t *params, int count,
		     fastf_t radius, int left, int orientation,
		     bool reject = false)
{
    return sketch_step(edit, unit, name, command, params, count,
	[radius, left, orientation](const struct rt_edit *e) {
	    return sketch_line_arc_state(e, radius, left, orientation);
	}, reject);
}

static int
sketch_two_arc_step(struct rt_edit *edit, const char *unit, const char *name,
		    int command, const fastf_t *params, int count,
		    bool target_present, fastf_t radius, int left,
		    int orientation, bool reject = false)
{
    return sketch_step(edit, unit, name, command, params, count,
	[target_present, radius, left, orientation](const struct rt_edit *e) {
	    return sketch_two_arc_state(e, target_present, radius,
		left, orientation);
	}, reject);
}

static int
sketch_check_descriptors(void)
{
    const struct rt_edit_prim_desc *desc = EDOBJ[ID_SKETCH].ft_edit_desc();
    const struct {
	int command;
	int count;
	int types[2];
    } expected[] = {
	{ECMD_SKETCH_MOVE_VERTEX, 2,
	    {RT_EDIT_PARAM_SCALAR, RT_EDIT_PARAM_SCALAR}},
	{ECMD_SKETCH_MOVE_SEGMENT, 2,
	    {RT_EDIT_PARAM_SCALAR, RT_EDIT_PARAM_SCALAR}},
	{ECMD_SKETCH_APPEND_LINE, 2,
	    {RT_EDIT_PARAM_INTEGER, RT_EDIT_PARAM_INTEGER}},
	{ECMD_SKETCH_DELETE_VERTEX, 1, {RT_EDIT_PARAM_INTEGER, 0}},
	{ECMD_SKETCH_DELETE_SEGMENT, 1, {RT_EDIT_PARAM_INTEGER, 0}},
	{ECMD_SKETCH_SPLIT_SEGMENT, 2,
	    {RT_EDIT_PARAM_INTEGER, RT_EDIT_PARAM_SCALAR}}
    };
    if (!desc)
	return 1;
    for (const auto &entry : expected) {
	const struct rt_edit_cmd_desc *command = NULL;
	for (int i = 0; i < desc->ncmd; ++i) {
	    if (desc->cmds[i].cmd_id == entry.command) {
		command = &desc->cmds[i];
		break;
	    }
	}
	if (!command || command->nparam != entry.count || !command->params) {
	    bu_log("sketch descriptor %d has wrong arity\n", entry.command);
	    return 1;
	}
	for (int i = 0; i < entry.count; ++i) {
	    if (command->params[i].index != i ||
		command->params[i].type != entry.types[i]) {
		bu_log("sketch descriptor %d misstates parameter %d\n",
		    entry.command, i);
		return 1;
	    }
	}
    }
    bu_log("sketch\tdescriptors\tpass\n");
    return 0;
}

static int
sketch_check_unit(fastf_t local2base, const char *unit)
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
    struct directory *dp = make_test_sketch(wdbp);
    struct db_full_path path;
    db_full_path_init(&path);
    db_add_node_to_full_path(&path, dp);
    struct bn_tol tol = BN_TOL_INIT_TOL;
    int failures = 0;
    struct sketch_expected expected = sketch_initial_state();
    const fastf_t vertex[] = {2};
    const fastf_t move[] = {sketch_inch_to_mm / local2base,
	2 * sketch_inch_to_mm / local2base};
    const fastf_t add[] = {0, sketch_inch_to_mm / local2base};
    const fastf_t move_segment[] = {sketch_inch_to_mm / local2base, 0};
    const fastf_t move_list[] = {0, sketch_inch_to_mm / local2base, 0, 2};
    const fastf_t invalid_list[] = {0, sketch_inch_to_mm / local2base, 0, 99};
    const fastf_t fractional_list[] = {0, sketch_inch_to_mm / local2base,
	0, 0.5};
    const fastf_t append[] = {0, 3};
    const fastf_t used[] = {0};
    const fastf_t split[] = {0, 0.25};
    const fastf_t bad_split_t[] = {0, NAN};
    const fastf_t bad_split_index[] = {0.5, 0.25};
    const fastf_t bad_move[] = {NAN, 0};
    const fastf_t bad_delta[] = {NAN, 0};
    const fastf_t bad_add[] = {0, NAN};
    const fastf_t first_segment[] = {0};
    const fastf_t fractional_vertex[] = {0.5};
    const fastf_t segment[] = {0};
    const fastf_t fractional_line[] = {0.5, 3};
    struct rt_edit *edit = rt_edit_create(&path, dbip, &tol, NULL);
    if (!edit) {
	failures = 1;
	goto done;
    }
    expected.vertex = 2;
    failures += sketch_matrix_step(edit, unit, "pick vertex", ECMD_SKETCH_PICK_VERTEX,
	vertex, 1, &expected);
    expected.uv[4] = sketch_inch_to_mm;
    expected.uv[5] = 2 * sketch_inch_to_mm;
    failures += sketch_matrix_step(edit, unit, "move vertex", ECMD_SKETCH_MOVE_VERTEX,
	move, 2, &expected);
    expected.uv.insert(expected.uv.end(), {0, sketch_inch_to_mm});
    expected.vertex = 4;
    failures += sketch_matrix_step(edit, unit, "add vertex", ECMD_SKETCH_ADD_VERTEX,
	add, 2, &expected);
    failures += sketch_matrix_step(edit, unit, "reject nonfinite vertex move",
	ECMD_SKETCH_MOVE_VERTEX, bad_move, 2, &expected, true);
    failures += sketch_matrix_step(edit, unit, "reject nonfinite vertex add",
	ECMD_SKETCH_ADD_VERTEX, bad_add, 2, &expected, true);
    expected.segment = 0;
    failures += sketch_matrix_step(edit, unit, "pick segment",
	ECMD_SKETCH_PICK_SEGMENT, first_segment, 1, &expected);
    expected.uv[0] += sketch_inch_to_mm;
    expected.uv[2] += sketch_inch_to_mm;
    failures += sketch_matrix_step(edit, unit, "move segment",
	ECMD_SKETCH_MOVE_SEGMENT, move_segment, 2, &expected);
    failures += sketch_matrix_step(edit, unit, "reject nonfinite segment move",
	ECMD_SKETCH_MOVE_SEGMENT, bad_delta, 2, &expected, true);
    expected.uv[1] += sketch_inch_to_mm;
    expected.uv[5] += sketch_inch_to_mm;
    failures += sketch_matrix_step(edit, unit, "move vertex list",
	ECMD_SKETCH_MOVE_VERTEX_LIST, move_list, 4, &expected);
    failures += sketch_matrix_step(edit, unit, "reject invalid vertex list",
	ECMD_SKETCH_MOVE_VERTEX_LIST, invalid_list, 4, &expected, true);
    failures += sketch_matrix_step(edit, unit, "reject fractional vertex list",
	ECMD_SKETCH_MOVE_VERTEX_LIST, fractional_list, 4, &expected, true);
    rt_edit_destroy(edit);

    edit = rt_edit_create(&path, dbip, &tol, NULL);
    if (!edit) {
	++failures;
	goto done;
    }
    expected = sketch_initial_state();
    expected.lines.insert(expected.lines.end(), {0, 3});
    failures += sketch_matrix_step(edit, unit, "append line", ECMD_SKETCH_APPEND_LINE,
	append, 2, &expected);
    expected.vertex = 2;
    failures += sketch_matrix_step(edit, unit, "pick unused vertex",
	ECMD_SKETCH_PICK_VERTEX, vertex, 1, &expected);
    expected.vertex = -1;
    expected.uv.erase(expected.uv.begin() + 4, expected.uv.begin() + 6);
    expected.lines[3] = 2;
    failures += sketch_matrix_step(edit, unit, "delete unused vertex",
	ECMD_SKETCH_DELETE_VERTEX, NULL, 0, &expected);
    expected.vertex = 0;
    failures += sketch_matrix_step(edit, unit, "pick used vertex",
	ECMD_SKETCH_PICK_VERTEX, used, 1, &expected);
    failures += sketch_matrix_step(edit, unit, "reject used vertex deletion",
	ECMD_SKETCH_DELETE_VERTEX, NULL, 0, &expected, true);
    rt_edit_destroy(edit);

    edit = rt_edit_create(&path, dbip, &tol, NULL);
    if (!edit) {
	++failures;
	goto done;
    }
    expected = sketch_initial_state();
    expected.uv.erase(expected.uv.begin() + 4, expected.uv.begin() + 6);
    failures += sketch_matrix_step(edit, unit, "delete vertex by index",
	ECMD_SKETCH_DELETE_VERTEX, vertex, 1, &expected);
    rt_edit_destroy(edit);

    edit = rt_edit_create(&path, dbip, &tol, NULL);
    if (!edit) {
	++failures;
	goto done;
    }
    expected = sketch_initial_state();
    expected.lines.clear();
    failures += sketch_matrix_step(edit, unit, "delete segment by index",
	ECMD_SKETCH_DELETE_SEGMENT, segment, 1, &expected);
    rt_edit_destroy(edit);

    edit = rt_edit_create(&path, dbip, &tol, NULL);
    if (!edit) {
	++failures;
	goto done;
    }
    expected = sketch_initial_state();
    expected.uv.insert(expected.uv.end(), {2.5, 0});
    expected.lines = {0, 4, 4, 1};
    failures += sketch_matrix_step(edit, unit, "split line", ECMD_SKETCH_SPLIT_SEGMENT,
	split, 2, &expected);
    rt_edit_destroy(edit);

    edit = rt_edit_create(&path, dbip, &tol, NULL);
    if (!edit) {
	++failures;
	goto done;
    }
    expected = sketch_initial_state();
    failures += sketch_matrix_step(edit, unit, "reject fractional vertex",
	ECMD_SKETCH_PICK_VERTEX, fractional_vertex, 1, &expected, true);
    rt_edit_destroy(edit);

    edit = rt_edit_create(&path, dbip, &tol, NULL);
    if (!edit) {
	++failures;
	goto done;
    }
    failures += sketch_matrix_step(edit, unit, "reject fractional line",
	ECMD_SKETCH_APPEND_LINE, fractional_line, 2, &expected, true);
    rt_edit_destroy(edit);

    edit = rt_edit_create(&path, dbip, &tol, NULL);
    if (!edit) {
	++failures;
	goto done;
    }
    expected = sketch_initial_state();
    failures += sketch_matrix_step(edit, unit, "reject nonfinite split",
	ECMD_SKETCH_SPLIT_SEGMENT, bad_split_t, 2, &expected, true);
    rt_edit_destroy(edit);

    edit = rt_edit_create(&path, dbip, &tol, NULL);
    if (!edit) {
	++failures;
	goto done;
    }
    failures += sketch_matrix_step(edit, unit, "reject fractional segment",
	ECMD_SKETCH_SPLIT_SEGMENT, bad_split_index, 2, &expected, true);
    rt_edit_destroy(edit);

    edit = rt_edit_create(&path, dbip, &tol, NULL);
    if (!edit) {
	++failures;
	goto done;
    }
    {
	/* Exceed the former fixed segment buffer and repeat one control point. */
	const int control_point_count = 65;
	struct rt_sketch_internal *sketch =
	    (struct rt_sketch_internal *)edit->es_int.idb_ptr;
	struct bezier_seg *bezier;
	BU_ALLOC(bezier, struct bezier_seg);
	bezier->magic = CURVE_BEZIER_MAGIC;
	bezier->degree = control_point_count - 1;
	bezier->ctl_points = (int *)bu_calloc(control_point_count, sizeof(int),
	    "sketch repeated Bezier control points");
	bu_free(sketch->curve.segment[0], "sketch line segment");
	sketch->curve.segment[0] = bezier;
	rt_edit_set_edflag(edit, ECMD_SKETCH_PICK_SEGMENT);
	edit->e_inpara = 1;
	edit->e_para[0] = 0;
	int picked = rt_edit_process(edit);
	rt_edit_set_edflag(edit, ECMD_SKETCH_MOVE_SEGMENT);
	edit->e_inpara = 2;
	edit->e_para[0] = sketch_inch_to_mm / local2base;
	edit->e_para[1] = 0;
	int moved = picked == BRLCAD_OK ? rt_edit_process(edit) : BRLCAD_ERROR;
	rt_edit_set_edflag(edit, ECMD_SKETCH_SPLIT_SEGMENT);
	edit->e_inpara = 2;
	edit->e_para[0] = 0;
	edit->e_para[1] = 0.5;
	int split_result = moved == BRLCAD_OK ? rt_edit_process(edit) : BRLCAD_OK;
	const struct sketch_expected initial = sketch_initial_state();
	bool ok = picked == BRLCAD_OK && moved == BRLCAD_OK &&
	    split_result != BRLCAD_OK &&
	    sketch->vert_count == 4 && sketch->curve.count == 1 &&
	    sketch->curve.segment[0] == bezier &&
	    bezier->degree == control_point_count - 1 &&
	    NEAR_EQUAL(sketch->verts[0][0], sketch_inch_to_mm, VUNITIZE_TOL) &&
	    NEAR_EQUAL(sketch->verts[0][1], 0, VUNITIZE_TOL);
	for (int i = 1; ok && i < 4; ++i) {
	    ok = NEAR_EQUAL(sketch->verts[i][0], initial.uv[2 * i],
		VUNITIZE_TOL) &&
		NEAR_EQUAL(sketch->verts[i][1], initial.uv[2 * i + 1],
		    VUNITIZE_TOL);
	}
	for (int i = 0; ok && i < control_point_count; ++i)
	    ok = bezier->ctl_points[i] == 0;
	bu_log("sketch\tmove repeated high-degree segment\t%s\t%s\n",
	    unit, ok ? "pass" : "fail");
	failures += ok ? 0 : 1;
    }
    rt_edit_destroy(edit);

    edit = rt_edit_create(&path, dbip, &tol, NULL);
    if (!edit) {
	++failures;
	goto done;
    }
    {
	const fastf_t arc[] = {1, 2, sketch_inch_to_mm / local2base, 1, 0};
	const fastf_t bad_index[] = {1.5, 2,
	    sketch_inch_to_mm / local2base, 1, 0};
	const fastf_t bad_arc_radius[] = {1, 2, NAN, 1, 0};
	const fastf_t bad_bezier_index[] = {0, 1.5, 2};
	const fastf_t bad_nurb_order[] = {NAN, 0, 1, 2};
	const fastf_t bad_nurb_index[] = {3, 0, 1.5, 2};
	const fastf_t new_radius[] = {1, 2 * sketch_inch_to_mm / local2base};
	const fastf_t bad_new_radius[] = {1, NAN};
	const fastf_t arc_index[] = {1};
	expected = sketch_initial_state();
	failures += sketch_matrix_step(edit, unit, "reject fractional arc vertex",
	    ECMD_SKETCH_APPEND_ARC, bad_index, 5, &expected, true);
	failures += sketch_matrix_step(edit, unit, "reject nonfinite arc radius",
	    ECMD_SKETCH_APPEND_ARC, bad_arc_radius, 5, &expected, true);
	failures += sketch_matrix_step(edit, unit, "reject fractional Bezier vertex",
	    ECMD_SKETCH_APPEND_BEZIER, bad_bezier_index, 3, &expected, true);
	failures += sketch_matrix_step(edit, unit, "reject nonfinite NURB order",
	    ECMD_SKETCH_APPEND_NURB, bad_nurb_order, 4, &expected, true);
	failures += sketch_matrix_step(edit, unit, "reject fractional NURB vertex",
	    ECMD_SKETCH_APPEND_NURB, bad_nurb_index, 4, &expected, true);
	failures += sketch_arc_step(edit, unit, "append arc",
	    ECMD_SKETCH_APPEND_ARC, arc, 5, sketch_inch_to_mm, 1);
	failures += sketch_arc_step(edit, unit, "set arc radius",
	    ECMD_SKETCH_SET_ARC_RADIUS, new_radius, 2,
	    2 * sketch_inch_to_mm, 1);
	failures += sketch_arc_step(edit, unit, "reject nonfinite arc update",
	    ECMD_SKETCH_SET_ARC_RADIUS, bad_new_radius, 2,
	    2 * sketch_inch_to_mm, 1, true);
	failures += sketch_arc_step(edit, unit, "toggle arc side",
	    ECMD_SKETCH_TOGGLE_ARC_ORIENT, arc_index, 1,
	    2 * sketch_inch_to_mm, 0);
    }
    rt_edit_destroy(edit);

    edit = rt_edit_create(&path, dbip, &tol, NULL);
    if (!edit) {
	++failures;
	goto done;
    }
    {
	const fastf_t append_nurb[] = {3, 0, 1, 2, 3};
	const fastf_t initial_knots[] = {0, 0, 0, 1, 2, 2, 2};
	const fastf_t edited_knots[] = {0, 0, 0, 1, 3, 3, 3};
	const fastf_t select_nurb[] = {1};
	const fastf_t bad_kv[] = {7, 0, 0, 0, 1, NAN, 3, 3};
	const fastf_t edited_kv[] = {7, 0, 0, 0, 1, 3, 3, 3};
	const fastf_t bad_weights[] = {1, 4, 1, NAN, 1, 1};
	const fastf_t edited_weights[] = {1, 4, 1, 2, 3, 4};
	failures += sketch_nurb_step(edit, unit, "append NURB",
	    ECMD_SKETCH_APPEND_NURB, append_nurb, 5,
	    initial_knots, NULL, -1);
	failures += sketch_nurb_step(edit, unit, "pick NURB",
	    ECMD_SKETCH_PICK_SEGMENT, select_nurb, 1,
	    initial_knots, NULL, 1);
	failures += sketch_nurb_step(edit, unit, "reject nonfinite knots",
	    ECMD_SKETCH_NURB_EDIT_KV, bad_kv, 8,
	    initial_knots, NULL, 1, true);
	failures += sketch_nurb_step(edit, unit, "edit NURB knots",
	    ECMD_SKETCH_NURB_EDIT_KV, edited_kv, 8,
	    edited_knots, NULL, 1);
	failures += sketch_nurb_step(edit, unit, "reject nonfinite weight",
	    ECMD_SKETCH_NURB_EDIT_WEIGHTS, bad_weights, 6,
	    edited_knots, NULL, 1, true);
	failures += sketch_nurb_step(edit, unit, "edit NURB weights",
	    ECMD_SKETCH_NURB_EDIT_WEIGHTS, edited_weights, 6,
	    edited_knots, edited_weights + 2, 1);
    }
    rt_edit_destroy(edit);

    edit = rt_edit_create(&path, dbip, &tol, NULL);
    if (!edit) {
	++failures;
	goto done;
    }
    {
	const fastf_t append_line[] = {2, 3};
	const fastf_t append_arc[] = {1, 2, 8.0 / local2base, 1, 0};
	const fastf_t tangent[] = {2, 1, 0};
	const fastf_t bad_angle[] = {2, 1, NAN};
	const fastf_t same_segment[] = {2, 2, 0};
	expected = sketch_initial_state();
	expected.lines.insert(expected.lines.end(), {2, 3});
	failures += sketch_matrix_step(edit, unit, "append tangent line",
	    ECMD_SKETCH_APPEND_LINE, append_line, 2, &expected);
	failures += sketch_line_arc_step(edit, unit, "append tangent arc",
	    ECMD_SKETCH_APPEND_ARC, append_arc, 5, 8, 1, 0);
	failures += sketch_line_arc_step(edit, unit, "set line-arc tangency",
	    ECMD_SKETCH_SET_TANGENCY, tangent, 3, 5, 0, 1);
	failures += sketch_line_arc_step(edit, unit, "reject nonfinite tangency",
	    ECMD_SKETCH_SET_TANGENCY, bad_angle, 3, 5, 0, 1, true);
	failures += sketch_line_arc_step(edit, unit, "reject self tangency",
	    ECMD_SKETCH_SET_TANGENCY, same_segment, 3, 5, 0, 1, true);
    }
    rt_edit_destroy(edit);

    edit = rt_edit_create(&path, dbip, &tol, NULL);
    if (!edit) {
	++failures;
	goto done;
    }
    {
	const fastf_t adjacent_arc[] = {2, 3, 8.0 / local2base, 1, 0};
	const fastf_t target_arc[] = {1, 2, 8.0 / local2base, 1, 0};
	const fastf_t tangent[] = {2, 1, 0};
	/* The adjacent arc's tangent has X component sqrt(39)/8. */
	const fastf_t tangent_radius = 40.0 / sqrt(39.0);
	failures += sketch_two_arc_step(edit, unit, "append adjacent arc",
	    ECMD_SKETCH_APPEND_ARC, adjacent_arc, 5, false, 0, 0, 0);
	failures += sketch_two_arc_step(edit, unit, "append target arc",
	    ECMD_SKETCH_APPEND_ARC, target_arc, 5, true, 8, 1, 0);
	failures += sketch_two_arc_step(edit, unit, "set arc-arc tangency",
	    ECMD_SKETCH_SET_TANGENCY, tangent, 3, true,
	    tangent_radius, 1, 0);
    }
    rt_edit_destroy(edit);

    edit = rt_edit_create(&path, dbip, &tol, NULL);
    if (!edit) {
	++failures;
	goto done;
    }
    {
	const fastf_t append_arc[] = {1, 2, 8.0 / local2base, 1, 0};
	const fastf_t split_arc[] = {1, 0.5};
	failures += sketch_arc_step(edit, unit, "append splittable arc",
	    ECMD_SKETCH_APPEND_ARC, append_arc, 5, 8, 1);
	failures += sketch_step(edit, unit, "split arc",
	    ECMD_SKETCH_SPLIT_SEGMENT, split_arc, 2,
	    [](const struct rt_edit *e) { return sketch_split_arc_state(e); },
	    false);
    }
    rt_edit_destroy(edit);

    {
	const struct {
	fastf_t radius;
	const char *name;
	} unsplittable[] = {
	    {-8, "reject full-circle split"},
	    {4, "reject undersized arc split"}
	};
	for (const auto &case_to_check : unsplittable) {
	    edit = rt_edit_create(&path, dbip, &tol, NULL);
	    if (!edit) {
		++failures;
		goto done;
	    }
	    const fastf_t append_arc[] = {1, 2,
		case_to_check.radius / local2base, 1, 0};
	    const fastf_t split_arc[] = {1, 0.5};
	    failures += sketch_arc_step(edit, unit, "append unsplittable arc",
		ECMD_SKETCH_APPEND_ARC, append_arc, 5, case_to_check.radius, 1);
	    failures += sketch_arc_step(edit, unit, case_to_check.name,
		ECMD_SKETCH_SPLIT_SEGMENT, split_arc, 2,
		case_to_check.radius, 1, true);
	    rt_edit_destroy(edit);
	}
    }

done:
    db_free_full_path(&path);
    db_close(dbip);
    return failures;
}

static int
sketch_operation_matrix(void)
{
    return sketch_check_descriptors() + sketch_check_unit(1.0, "mm") +
	sketch_check_unit(sketch_inch_to_mm, "in");
}


/* ------------------------------------------------------------------ */
/* Main test                                                           */
/* ------------------------------------------------------------------ */

int
rt_edit_test_sketch(void)
{
    struct db_i *dbip = db_open_inmem();
    if (dbip == DBI_NULL)
	bu_exit(1, "ERROR: Unable to create in-memory database\n");

    struct rt_wdb *wdbp = wdb_dbopen(dbip, RT_WDB_TYPE_DB_INMEM);
    struct directory *dp = make_test_sketch(wdbp);

    struct bn_tol tol = BN_TOL_INIT_TOL;
    struct db_full_path fp;
    db_full_path_init(&fp);
    db_add_node_to_full_path(&fp, dp);

    struct bview *v;
    BU_GET(v, struct bview);
    bv_init(v, NULL);
    VSET(v->gv_aet, 45, 35, 0);
    bv_mat_aet(v);
    v->gv_size = 100.0;
    v->gv_isize = 1.0 / v->gv_size;
    v->gv_scale = 50.0;
    bv_update(v);
    bu_vls_sprintf(&v->gv_name, "default");
    v->gv_width  = 512;
    v->gv_height = 512;

    struct rt_edit *s = rt_edit_create(&fp, dbip, &tol, v);
    s->mv_context = 1;
    s->local2base = 1.0;

    struct rt_sketch_internal *skt =
	(struct rt_sketch_internal *)s->es_int.idb_ptr;
    struct rt_sketch_edit *se = (struct rt_sketch_edit *)s->ipe_ptr;

    /* ================================================================
     * Initial state: ipe_ptr initialized with curr_vert/curr_seg = -1
     * ================================================================*/
    if (!se)
	bu_exit(1, "ERROR: ipe_ptr not allocated\n");
    if (se->curr_vert != -1 || se->curr_seg != -1)
	bu_exit(1, "ERROR: initial state wrong (expected -1,-1)\n");
    bu_log("SKETCH initial state SUCCESS: curr_vert=%d curr_seg=%d\n",
	   se->curr_vert, se->curr_seg);

    /* The plane origin is a local length; its axes are unitless directions. */
    const fastf_t local2base = 25.4;
    s->local2base = local2base;
    s->base2local = 1.0 / local2base;
    EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, ECMD_SKETCH_SET_PLANE);
    s->e_inpara = 9;
    VSET(&s->e_para[0], 1, 2, 3);
    VSET(&s->e_para[3], 2, 0, 0);
    VSET(&s->e_para[6], 0, 3, 0);
    rt_edit_process(s);
    point_t expected_origin = {25.4, 50.8, 76.2};
    vect_t expected_u = {1, 0, 0};
    vect_t expected_v = {0, 1, 0};
    if (!VNEAR_EQUAL(skt->V, expected_origin, VUNITIZE_TOL) ||
	!VNEAR_EQUAL(skt->u_vec, expected_u, VUNITIZE_TOL) ||
	!VNEAR_EQUAL(skt->v_vec, expected_v, VUNITIZE_TOL))
	bu_exit(1, "ERROR: non-mm sketch plane: origin=(%g,%g,%g)\n",
		V3ARGS(skt->V));
    VSET(skt->V, 0, 0, 0);
    s->local2base = 1.0;
    s->base2local = 1.0;
    bu_log("ECMD_SKETCH_SET_PLANE non-mm units SUCCESS\n");

    /* ================================================================
     * ECMD_SKETCH_PICK_VERTEX  (select vertex 2)
     * ================================================================*/
    EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, ECMD_SKETCH_PICK_VERTEX);
    s->e_inpara = 1;
    s->e_para[0] = 2.0;

    rt_edit_process(s);
    if (se->curr_vert != 2)
	bu_exit(1, "ERROR: ECMD_SKETCH_PICK_VERTEX: expected curr_vert=2, got %d\n",
		se->curr_vert);
    bu_log("ECMD_SKETCH_PICK_VERTEX SUCCESS: curr_vert=%d\n", se->curr_vert);

    /* ================================================================
     * ECMD_SKETCH_MOVE_VERTEX  (move vertex 2 to (20,20))
     * ================================================================*/
    EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, ECMD_SKETCH_MOVE_VERTEX);
    s->e_inpara = 2;
    s->e_para[0] = 20.0;
    s->e_para[1] = 20.0;

    rt_edit_process(s);
    if (!NEAR_EQUAL(skt->verts[2][0], 20.0, VUNITIZE_TOL) ||
	!NEAR_EQUAL(skt->verts[2][1], 20.0, VUNITIZE_TOL))
	bu_exit(1, "ERROR: ECMD_SKETCH_MOVE_VERTEX: expected (20,20), got (%g,%g)\n",
		skt->verts[2][0], skt->verts[2][1]);
    bu_log("ECMD_SKETCH_MOVE_VERTEX SUCCESS: verts[2]=(%g,%g)\n",
	   skt->verts[2][0], skt->verts[2][1]);

    /* Restore verts[2] */
    V2SET(skt->verts[2], 10, 10);

    /* ================================================================
     * ECMD_SKETCH_PICK_SEGMENT  (select segment 0)
     * ================================================================*/
    EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, ECMD_SKETCH_PICK_SEGMENT);
    s->e_inpara = 1;
    s->e_para[0] = 0.0;

    rt_edit_process(s);
    if (se->curr_seg != 0)
	bu_exit(1, "ERROR: ECMD_SKETCH_PICK_SEGMENT: expected curr_seg=0, got %d\n",
		se->curr_seg);
    bu_log("ECMD_SKETCH_PICK_SEGMENT SUCCESS: curr_seg=%d\n", se->curr_seg);

    /* ================================================================
     * ECMD_SKETCH_MOVE_SEGMENT  (translate line seg 0 by (+5, +5))
     * Segment 0 is line_seg with start=0 (0,0) and end=1 (10,0).
     * After move: verts[0]=(5,5), verts[1]=(15,5).
     * ================================================================*/
    EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, ECMD_SKETCH_MOVE_SEGMENT);
    s->e_inpara = 2;
    s->e_para[0] = 5.0;
    s->e_para[1] = 5.0;

    rt_edit_process(s);
    if (!NEAR_EQUAL(skt->verts[0][0], 5.0,  VUNITIZE_TOL) ||
	!NEAR_EQUAL(skt->verts[0][1], 5.0,  VUNITIZE_TOL) ||
	!NEAR_EQUAL(skt->verts[1][0], 15.0, VUNITIZE_TOL) ||
	!NEAR_EQUAL(skt->verts[1][1], 5.0,  VUNITIZE_TOL))
	bu_exit(1,
		"ERROR: ECMD_SKETCH_MOVE_SEGMENT: expected verts[0]=(5,5) verts[1]=(15,5), "
		"got (%g,%g) (%g,%g)\n",
		skt->verts[0][0], skt->verts[0][1],
		skt->verts[1][0], skt->verts[1][1]);
    bu_log("ECMD_SKETCH_MOVE_SEGMENT SUCCESS: verts[0]=(%g,%g) verts[1]=(%g,%g)\n",
	   skt->verts[0][0], skt->verts[0][1],
	   skt->verts[1][0], skt->verts[1][1]);

    /* Restore verts */
    V2SET(skt->verts[0],  0,  0);
    V2SET(skt->verts[1], 10,  0);

    /* ================================================================
     * ECMD_SKETCH_APPEND_LINE  (add line 2→3; curve should now have 2 segs)
     * ================================================================*/
    EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, ECMD_SKETCH_APPEND_LINE);
    s->e_inpara = 2;
    s->e_para[0] = 2.0;
    s->e_para[1] = 3.0;

    rt_edit_process(s);
    if (skt->curve.count != 2)
	bu_exit(1, "ERROR: ECMD_SKETCH_APPEND_LINE: expected 2 segments, got %zu\n",
		skt->curve.count);
    {
	struct line_seg *ls = (struct line_seg *)skt->curve.segment[1];
	if (!ls || ls->magic != CURVE_LSEG_MAGIC || ls->start != 2 || ls->end != 3)
	    bu_exit(1, "ERROR: ECMD_SKETCH_APPEND_LINE: new segment wrong\n");
    }
    bu_log("ECMD_SKETCH_APPEND_LINE SUCCESS: curve now has %zu segments\n",
	   skt->curve.count);

    /* ================================================================
     * ECMD_SKETCH_APPEND_ARC  (add arc from vert 1 to vert 2, r=8,
     *                          center_is_left=1, ccw=0)
     * e_inpara=5: [0]=start [1]=end [2]=radius [3]=center_is_left [4]=orientation
     * ================================================================*/
    EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, ECMD_SKETCH_APPEND_ARC);
    s->e_inpara = 5;
    s->e_para[0] = 1.0;  /* start vert */
    s->e_para[1] = 2.0;  /* end vert */
    s->e_para[2] = 8.0;  /* radius */
    s->e_para[3] = 1.0;  /* center_is_left = 1 */
    s->e_para[4] = 0.0;  /* orientation: ccw */

    rt_edit_process(s);
    if (skt->curve.count != 3)
	bu_exit(1, "ERROR: ECMD_SKETCH_APPEND_ARC: expected 3 segments, got %zu\n",
		skt->curve.count);
    {
	struct carc_seg *cs = (struct carc_seg *)skt->curve.segment[2];
	if (!cs || cs->magic != CURVE_CARC_MAGIC || cs->start != 1 || cs->end != 2 ||
	    !NEAR_EQUAL(cs->radius, 8.0, VUNITIZE_TOL) ||
	    cs->center_is_left != 1 || cs->orientation != 0)
	    bu_exit(1, "ERROR: ECMD_SKETCH_APPEND_ARC: new segment wrong\n");
    }
    bu_log("ECMD_SKETCH_APPEND_ARC SUCCESS: curve now has %zu segments\n",
	   skt->curve.count);

    /* Exercise the selected arc's orientation and radius in inch units. */
    {
	struct carc_seg *arc = (struct carc_seg *)skt->curve.segment[2];
	se->curr_seg = 2;
	rt_edit_set_edflag(s, ECMD_SKETCH_TOGGLE_ARC_ORIENT);
	s->e_inpara = 0;
	if (EDOBJ[dp->d_minor_type].ft_edit(s) != BRLCAD_OK || arc->center_is_left != 0)
	    bu_exit(1, "ERROR: sketch arc orientation was not toggled\n");
	if (EDOBJ[dp->d_minor_type].ft_edit(s) != BRLCAD_OK || arc->center_is_left != 1)
	    bu_exit(1, "ERROR: sketch arc orientation was not restored\n");

	s->local2base = 25.4;
	s->base2local = 1.0 / s->local2base;
	rt_edit_set_edflag(s, ECMD_SKETCH_SET_ARC_RADIUS);
	s->e_inpara = 1;
	s->e_para[0] = 1.0;
	if (EDOBJ[dp->d_minor_type].ft_edit(s) != BRLCAD_OK ||
	    !NEAR_EQUAL(arc->radius, 25.4, VUNITIZE_TOL))
	    bu_exit(1, "ERROR: sketch arc radius did not convert local units\n");
	s->e_inpara = 1;
	s->e_para[0] = 8.0 / s->local2base;
	if (EDOBJ[dp->d_minor_type].ft_edit(s) != BRLCAD_OK ||
	    !NEAR_EQUAL(arc->radius, 8.0, VUNITIZE_TOL))
	    bu_exit(1, "ERROR: sketch arc radius was not restored\n");
	s->local2base = 1.0;
	s->base2local = 1.0;

	rt_edit_set_edflag(s, ECMD_SKETCH_TOGGLE_SEGMENT_REVERSE);
	s->e_inpara = 0;
	if (EDOBJ[dp->d_minor_type].ft_edit(s) != BRLCAD_OK ||
	    skt->curve.reverse[2] != 1)
	    bu_exit(1, "ERROR: sketch segment reverse was not toggled\n");
	if (EDOBJ[dp->d_minor_type].ft_edit(s) != BRLCAD_OK ||
	    skt->curve.reverse[2] != 0)
	    bu_exit(1, "ERROR: sketch segment reverse was not restored\n");

	rt_edit_set_edflag(s, ECMD_SKETCH_SET_TANGENCY);
	s->e_inpara = 2;
	s->e_para[0] = 1.0;
	s->e_para[1] = 0.0;
	if (EDOBJ[dp->d_minor_type].ft_edit(s) != BRLCAD_OK ||
	    !NEAR_EQUAL(arc->radius, 5.0, VUNITIZE_TOL))
	    bu_exit(1, "ERROR: sketch arc tangency did not set the expected radius\n");
	rt_edit_set_edflag(s, ECMD_SKETCH_SET_ARC_RADIUS);
	s->e_inpara = 1;
	s->e_para[0] = 8.0;
	if (EDOBJ[dp->d_minor_type].ft_edit(s) != BRLCAD_OK)
	    bu_exit(1, "ERROR: sketch arc radius could not be restored\n");

	s->e_inpara = 2;
	s->e_para[0] = 999.0;
	s->e_para[1] = 1.0;
	if (rt_edit_process(s) != BRLCAD_ERROR ||
	    !NEAR_EQUAL(arc->radius, 8.0, VUNITIZE_TOL) ||
	    !bu_vls_strlen(s->log_str))
	    bu_exit(1, "ERROR: invalid sketch segment did not report a failure\n");
	bu_vls_trunc(s->log_str, 0);
	s->e_inpara = 0;
    }

    /* ================================================================
     * ECMD_SKETCH_APPEND_BEZIER  (quadratic Bezier: verts 0, 1, 2)
     * e_inpara=3 → degree=2; e_para[0..2] are control point indices
     * ================================================================*/
    EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, ECMD_SKETCH_APPEND_BEZIER);
    s->e_inpara = 3;
    s->e_para[0] = 0.0;  /* cp 0 */
    s->e_para[1] = 1.0;  /* cp 1 */
    s->e_para[2] = 2.0;  /* cp 2 */

    rt_edit_process(s);
    if (skt->curve.count != 4)
	bu_exit(1, "ERROR: ECMD_SKETCH_APPEND_BEZIER (quad): expected 4 segments, got %zu\n",
		skt->curve.count);
    {
	struct bezier_seg *bs = (struct bezier_seg *)skt->curve.segment[3];
	if (!bs || bs->magic != CURVE_BEZIER_MAGIC || bs->degree != 2 ||
	    bs->ctl_points[0] != 0 || bs->ctl_points[1] != 1 || bs->ctl_points[2] != 2)
	    bu_exit(1, "ERROR: ECMD_SKETCH_APPEND_BEZIER (quad): new segment wrong\n");
    }
    bu_log("ECMD_SKETCH_APPEND_BEZIER (degree=2) SUCCESS: curve now has %zu segments\n",
	   skt->curve.count);

    /* ================================================================
     * ECMD_SKETCH_APPEND_BEZIER  (cubic Bezier: degree=3, verts 0,1,2,3)
     * e_inpara=4 → degree=3; e_para[0..3] are control point indices
     * This test validates that RT_EDIT_MAXPARA > 3 is properly used.
     * ================================================================*/
    EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, ECMD_SKETCH_APPEND_BEZIER);
    s->e_inpara = 4;
    s->e_para[0] = 0.0;  /* cp 0 */
    s->e_para[1] = 1.0;  /* cp 1 */
    s->e_para[2] = 2.0;  /* cp 2 */
    s->e_para[3] = 3.0;  /* cp 3 */

    rt_edit_process(s);
    if (skt->curve.count != 5)
	bu_exit(1, "ERROR: ECMD_SKETCH_APPEND_BEZIER (cubic): expected 5 segments, got %zu\n",
		skt->curve.count);
    {
	struct bezier_seg *bs = (struct bezier_seg *)skt->curve.segment[4];
	if (!bs || bs->magic != CURVE_BEZIER_MAGIC || bs->degree != 3 ||
	    bs->ctl_points[0] != 0 || bs->ctl_points[1] != 1 ||
	    bs->ctl_points[2] != 2 || bs->ctl_points[3] != 3)
	    bu_exit(1, "ERROR: ECMD_SKETCH_APPEND_BEZIER (cubic): new segment wrong\n");
    }
    bu_log("ECMD_SKETCH_APPEND_BEZIER (degree=3) SUCCESS: curve now has %zu segments\n",
	   skt->curve.count);

    /* ================================================================
     * ECMD_SKETCH_DELETE_VERTEX (should fail: vertex 0 is in use)
     * Verify by checking that vert_count is unchanged after the attempt.
     * ================================================================*/
    se->curr_vert = 0;
    {
	size_t vc_before = skt->vert_count;
	EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, ECMD_SKETCH_DELETE_VERTEX);
	rt_edit_process(s);
	if (skt->vert_count != vc_before)
	    bu_exit(1, "ERROR: ECMD_SKETCH_DELETE_VERTEX incorrectly deleted in-use vertex\n");
    }
    bu_log("ECMD_SKETCH_DELETE_VERTEX (in-use vertex) correctly refused\n");
    se->curr_vert = -1;

    /* ================================================================
     * ECMD_SKETCH_DELETE_SEGMENT (delete the last segment, index 4)
     * After: curve.count should be 4
     * ================================================================*/
    se->curr_seg = 4;
    EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, ECMD_SKETCH_DELETE_SEGMENT);
    s->e_inpara = 0;

    rt_edit_process(s);
    if (skt->curve.count != 4)
	bu_exit(1, "ERROR: ECMD_SKETCH_DELETE_SEGMENT: expected 4 segments, got %zu\n",
		skt->curve.count);
    if (se->curr_seg != -1)
	bu_exit(1, "ERROR: ECMD_SKETCH_DELETE_SEGMENT: curr_seg not reset to -1\n");
    bu_log("ECMD_SKETCH_DELETE_SEGMENT SUCCESS: curve now has %zu segments\n",
	   skt->curve.count);

    /* ================================================================
     * RT_PARAMS_EDIT_SCALE: uniform scale about keypoint
     * ================================================================*/
    rt_edit_set_edflag(s, RT_PARAMS_EDIT_SCALE);
    s->e_inpara = 0;
    s->es_scale = 2.0;

    rt_edit_process(s);
    /* V should have been scaled - just verify it ran without crash */
    bu_log("RT_PARAMS_EDIT_SCALE SUCCESS\n");

    /* ================================================================
     * RT_MATRIX_EDIT_ROT: matrix rotation
     * ================================================================*/
    rt_edit_set_edflag(s, RT_MATRIX_EDIT_ROT);
    MAT_IDN(s->model_changes);
    MAT_IDN(s->acc_rot_sol);
    s->e_inpara = 1;
    VSET(s->e_para, 30, 0, 0);
    VSET(s->e_keypoint, 0, 0, 0);

    rt_edit_process(s);
    {
	mat_t ident;
	MAT_IDN(ident);
	if (bn_mat_is_equal(s->model_changes, ident, &tol))
	    bu_exit(1, "ERROR: RT_MATRIX_EDIT_ROT did not rotate model_changes\n");
	bu_log("RT_MATRIX_EDIT_ROT SUCCESS\n");
    }

    /* ================================================================
     * RT_MATRIX_EDIT_TRANS_MODEL_XYZ: absolute translation
     * ================================================================*/
    rt_edit_set_edflag(s, RT_MATRIX_EDIT_TRANS_MODEL_XYZ);
    MAT_IDN(s->model_changes);
    s->e_inpara = 1;
    VSET(s->e_para, 10, 20, 30);
    VSET(s->e_keypoint, 0, 0, 0);
    s->local2base = 1.0;

    rt_edit_process(s);
    {
	point_t kp_world;
	MAT4X3PNT(kp_world, s->model_changes, s->e_keypoint);
	vect_t expected = {10, 20, 30};
	if (!VNEAR_EQUAL(kp_world, expected, VUNITIZE_TOL))
	    bu_exit(1, "ERROR: RT_MATRIX_EDIT_TRANS_MODEL_XYZ failed\n");
	bu_log("RT_MATRIX_EDIT_TRANS_MODEL_XYZ SUCCESS: keypoint maps to (%g,%g,%g)\n",
	       V3ARGS(kp_world));
    }

    /* ================================================================
     * ECMD_SKETCH_MOVE_VERTEX_LIST: move vertices 0 and 2 by delta (5, -3)
     * Before: v0=(0,0), v1=(10,0), v2=(10,10), v3=(0,10)
     * After:  v0=(5,-3), v1=(10,0), v2=(15,7), v3=(0,10)
     * ================================================================*/
    {
	/* Re-read skt after previous edit operations may have modified it */
	struct rt_sketch_internal *skt2 =
	    (struct rt_sketch_internal *)s->es_int.idb_ptr;
	/* Reset vertex 0 and 2 to known values first */
	V2SET(skt2->verts[0],  0,  0);
	V2SET(skt2->verts[2], 10, 10);

	EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, ECMD_SKETCH_MOVE_VERTEX_LIST);
	s->e_inpara = 4;
	s->e_para[0] = 5.0;   /* U delta */
	s->e_para[1] = -3.0;  /* V delta */
	s->e_para[2] = 0.0;   /* vertex index 0 */
	s->e_para[3] = 2.0;   /* vertex index 2 */
	s->local2base = 1.0;
	bu_vls_trunc(s->log_str, 0);

	rt_edit_process(s);

	point2d_t exp0 = { 5, -3};
	point2d_t exp1 = {10,  0};  /* unchanged */
	point2d_t exp2 = {15,  7};
	point2d_t exp3 = { 0, 10};  /* unchanged */

	if (!V2NEAR_EQUAL(skt2->verts[0], exp0, VUNITIZE_TOL) ||
	    !V2NEAR_EQUAL(skt2->verts[1], exp1, VUNITIZE_TOL) ||
	    !V2NEAR_EQUAL(skt2->verts[2], exp2, VUNITIZE_TOL) ||
	    !V2NEAR_EQUAL(skt2->verts[3], exp3, VUNITIZE_TOL))
	    bu_exit(1,
		"ERROR: ECMD_SKETCH_MOVE_VERTEX_LIST: "
		"v0=(%g,%g) v1=(%g,%g) v2=(%g,%g) v3=(%g,%g); "
		"expected (5,-3) (10,0) (15,7) (0,10)\n",
		skt2->verts[0][0], skt2->verts[0][1],
		skt2->verts[1][0], skt2->verts[1][1],
		skt2->verts[2][0], skt2->verts[2][1],
		skt2->verts[3][0], skt2->verts[3][1]);
	bu_log("ECMD_SKETCH_MOVE_VERTEX_LIST SUCCESS: "
	       "v0=(%g,%g) v2=(%g,%g)\n",
	       skt2->verts[0][0], skt2->verts[0][1],
	       skt2->verts[2][0], skt2->verts[2][1]);
    }

    /* ================================================================
     * ECMD_SKETCH_SPLIT_SEGMENT: line segment split at t=0.5
     *
     * Start state after previous tests: segment[0] is line 0→1
     * (verts[0] and verts[1] were restored to (0,0) and (10,0)).
     *
     * After split at t=0.5:
     *   - new vertex midpoint at (5, 0) becomes verts[vc] where vc
     *     was skt->vert_count before the split
     *   - segment[0] becomes line 0 → new_vi
     *   - new  segment[1] becomes line new_vi → 1
     *   - curve.count increases by 1
     * ================================================================*/
    {
	struct rt_sketch_internal *skt3 =
	    (struct rt_sketch_internal *)s->es_int.idb_ptr;

	/* Reset the first segment to line 0→1 and restore verts */
	{
	    struct line_seg *ls = (struct line_seg *)skt3->curve.segment[0];
	    ls->start = 0;
	    ls->end   = 1;
	}
	V2SET(skt3->verts[0],  0,  0);
	V2SET(skt3->verts[1], 10,  0);

	size_t vc_before = skt3->vert_count;
	size_t sc_before = skt3->curve.count;

	EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, ECMD_SKETCH_SPLIT_SEGMENT);
	s->e_inpara  = 2;
	s->e_para[0] = 0.0;  /* segment index */
	s->e_para[1] = 0.5;  /* t = midpoint */
	bu_vls_trunc(s->log_str, 0);

	rt_edit_process(s);
	if (bu_vls_strlen(s->log_str))
	    bu_exit(1, "ERROR: ECMD_SKETCH_SPLIT_SEGMENT (line): %s\n",
		    bu_vls_cstr(s->log_str));

	/* One new vertex should have been added */
	if (skt3->vert_count != vc_before + 1)
	    bu_exit(1,
		"ERROR: ECMD_SKETCH_SPLIT_SEGMENT (line): "
		"vert_count should be %zu, got %zu\n",
		vc_before + 1, skt3->vert_count);

	/* One new segment should have been inserted */
	if (skt3->curve.count != sc_before + 1)
	    bu_exit(1,
		"ERROR: ECMD_SKETCH_SPLIT_SEGMENT (line): "
		"curve.count should be %zu, got %zu\n",
		sc_before + 1, skt3->curve.count);

	/* The split vertex (last added) should be at (5, 0) */
	point2d_t exp_mid = { 5.0, 0.0 };
	if (!V2NEAR_EQUAL(skt3->verts[vc_before], exp_mid, VUNITIZE_TOL))
	    bu_exit(1,
		"ERROR: ECMD_SKETCH_SPLIT_SEGMENT (line): "
		"split vertex should be (5,0), got (%g,%g)\n",
		skt3->verts[vc_before][0], skt3->verts[vc_before][1]);

	/* Segment[0] should now end at the split vertex */
	{
	    struct line_seg *ls0 = (struct line_seg *)skt3->curve.segment[0];
	    if (ls0->magic != CURVE_LSEG_MAGIC || ls0->start != 0 ||
		ls0->end   != (int)vc_before)
		bu_exit(1,
		    "ERROR: ECMD_SKETCH_SPLIT_SEGMENT (line): "
		    "first half wrong (start=%d end=%d)\n",
		    ls0->start, ls0->end);
	}
	/* Segment[1] should run from split vertex to original end */
	{
	    struct line_seg *ls1 = (struct line_seg *)skt3->curve.segment[1];
	    if (ls1->magic != CURVE_LSEG_MAGIC || ls1->start != (int)vc_before ||
		ls1->end   != 1)
		bu_exit(1,
		    "ERROR: ECMD_SKETCH_SPLIT_SEGMENT (line): "
		    "second half wrong (start=%d end=%d)\n",
		    ls1->start, ls1->end);
	}
	bu_log("ECMD_SKETCH_SPLIT_SEGMENT (line, t=0.5) SUCCESS: "
	       "new vertex %zu at (%g,%g), curve.count=%zu\n",
	       vc_before,
	       skt3->verts[vc_before][0], skt3->verts[vc_before][1],
	       skt3->curve.count);
    }

    /* ================================================================
     * ECMD_SKETCH_SPLIT_SEGMENT: Bezier segment split at t=0.5
     *
     * Build a quadratic Bezier (degree=2) with control points:
     *   P0=(0,0) P1=(5,10) P2=(10,0)
     * which are existing verts 0, (new) and 1.
     *
     * First, add a dedicated control-point vertex (5,10):
     * Append it as a new vert, then append the bezier.
     * Then split it at t=0.5.
     *
     * de Casteljau at t=0.5:
     *   Q10 = (1-0.5)*P0 + 0.5*P1 = (2.5, 5)
     *   Q11 = (1-0.5)*P1 + 0.5*P2 = (7.5, 5)
     *   Q20 = (1-0.5)*Q10+ 0.5*Q11= (5,   5)  ← split vertex
     * Left:  P0, Q10, Q20  = (0,0) (2.5,5) (5,5)
     * Right: Q20, Q11, P2  = (5,5) (7.5,5) (10,0)
     * ================================================================*/
    {
	struct rt_sketch_internal *skt4 =
	    (struct rt_sketch_internal *)s->es_int.idb_ptr;

	/* Add control-point vertex (5,10) */
	size_t cp_vi = skt4->vert_count;
	skt4->verts = (point2d_t *)bu_realloc(skt4->verts,
		(skt4->vert_count + 1) * sizeof(point2d_t), "verts");
	V2SET(skt4->verts[cp_vi],  5, 10);
	skt4->vert_count++;

	/* Append quadratic Bezier: verts 0, cp_vi, 1 */
	EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, ECMD_SKETCH_APPEND_BEZIER);
	s->e_inpara  = 3;
	s->e_para[0] = 0.0;           /* P0 */
	s->e_para[1] = (fastf_t)cp_vi; /* P1 */
	s->e_para[2] = 1.0;            /* P2 */
	rt_edit_process(s);

	size_t bez_seg_idx = skt4->curve.count - 1; /* just-appended bezier */
	size_t vc_before   = skt4->vert_count;
	size_t sc_before   = skt4->curve.count;

	/* Split the bezier at t=0.5 */
	EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, ECMD_SKETCH_SPLIT_SEGMENT);
	s->e_inpara  = 2;
	s->e_para[0] = (fastf_t)bez_seg_idx;
	s->e_para[1] = 0.5;
	bu_vls_trunc(s->log_str, 0);

	rt_edit_process(s);
	if (bu_vls_strlen(s->log_str))
	    bu_exit(1, "ERROR: ECMD_SKETCH_SPLIT_SEGMENT (bezier): %s\n",
		    bu_vls_cstr(s->log_str));

	/* For degree=2, de Casteljau adds 3 new vertices (L[1],L[2]=split,R[1]) */
	if (skt4->vert_count != vc_before + 3)
	    bu_exit(1,
		"ERROR: ECMD_SKETCH_SPLIT_SEGMENT (bezier): "
		"expected vert_count=%zu, got %zu\n",
		vc_before + 3, skt4->vert_count);
	if (skt4->curve.count != sc_before + 1)
	    bu_exit(1,
		"ERROR: ECMD_SKETCH_SPLIT_SEGMENT (bezier): "
		"expected curve.count=%zu, got %zu\n",
		sc_before + 1, skt4->curve.count);

	/* Q20 (split vertex) is L[2] = verts[vc_before+1] */
	int split_vi = vc_before + 1; /* L[d] = Q[d][0] when k reaches d */
	/* Actually L[1]=Q[1][0], L[2]=Q[2][0]=split: vc_before, vc_before+1 */
	/* verts[vc_before]   = Q10 = (2.5, 5) */
	/* verts[vc_before+1] = Q20 = (5,   5) = split vertex */
	/* verts[vc_before+2] = Q11 = (7.5, 5) (right interior) */
	point2d_t exp_q10 = {2.5, 5.0};
	point2d_t exp_q20 = {5.0, 5.0};
	point2d_t exp_q11 = {7.5, 5.0};

	if (!V2NEAR_EQUAL(skt4->verts[vc_before],     exp_q10, VUNITIZE_TOL) ||
	    !V2NEAR_EQUAL(skt4->verts[vc_before + 1], exp_q20, VUNITIZE_TOL) ||
	    !V2NEAR_EQUAL(skt4->verts[vc_before + 2], exp_q11, VUNITIZE_TOL))
	    bu_exit(1,
		"ERROR: ECMD_SKETCH_SPLIT_SEGMENT (bezier): "
		"wrong de Casteljau points: "
		"got (%g,%g) (%g,%g) (%g,%g)\n",
		skt4->verts[vc_before][0],     skt4->verts[vc_before][1],
		skt4->verts[vc_before + 1][0], skt4->verts[vc_before + 1][1],
		skt4->verts[vc_before + 2][0], skt4->verts[vc_before + 2][1]);

	/* Left bezier: P0, Q10, Q20 */
	{
	    struct bezier_seg *bsL =
		(struct bezier_seg *)skt4->curve.segment[bez_seg_idx];
	    if (bsL->magic != CURVE_BEZIER_MAGIC || bsL->degree != 2 ||
		bsL->ctl_points[0] != 0 ||
		bsL->ctl_points[1] != (int)vc_before ||
		bsL->ctl_points[2] != (int)(vc_before + 1))
		bu_exit(1,
		    "ERROR: ECMD_SKETCH_SPLIT_SEGMENT (bezier): left half wrong\n");
	}
	/* Right bezier: Q20, Q11, P2  (P2 = vert[1]) */
	{
	    struct bezier_seg *bsR =
		(struct bezier_seg *)skt4->curve.segment[bez_seg_idx + 1];
	    if (bsR->magic != CURVE_BEZIER_MAGIC || bsR->degree != 2 ||
		bsR->ctl_points[0] != split_vi ||
		bsR->ctl_points[1] != (int)(vc_before + 2) ||
		bsR->ctl_points[2] != 1)
		bu_exit(1,
		    "ERROR: ECMD_SKETCH_SPLIT_SEGMENT (bezier): right half wrong\n");
	}
	bu_log("ECMD_SKETCH_SPLIT_SEGMENT (bezier degree=2, t=0.5) SUCCESS: "
	       "Q10=(%g,%g) split=(%g,%g) Q11=(%g,%g)\n",
	       skt4->verts[vc_before][0],     skt4->verts[vc_before][1],
	       skt4->verts[vc_before + 1][0], skt4->verts[vc_before + 1][1],
	       skt4->verts[vc_before + 2][0], skt4->verts[vc_before + 2][1]);
    }

    /* ================================================================
     * ECMD_SKETCH_PICK_VERTEX via mouse proximity (ft_edit_xy path)
     *
     * Reset model_changes to identity so the projection used internally
     * (model2objview = gv_model2view * model_changes) equals gv_model2view.
     * Then aim the cursor at the view-space projection of vertex 0's 3D
     * position and verify that vertex 0 is selected.
     * ================================================================*/
    {
	/* Use identity model_changes for a clean test */
	MAT_IDN(s->model_changes);

	/* Reset curr_vert */
	se->curr_vert = -1;

	struct rt_sketch_internal *skt5 =
	    (struct rt_sketch_internal *)s->es_int.idb_ptr;

	/* Project vertex 0 to view space via model2objview (= gv_model2view now) */
	point_t v0_3d;
	VJOIN2(v0_3d, skt5->V,
	       skt5->verts[0][0], skt5->u_vec,
	       skt5->verts[0][1], skt5->v_vec);
	mat_t m2ov;
	bn_mat_mul(m2ov, v->gv_model2view, s->model_changes);
	point_t v0_view;
	MAT4X3PNT(v0_view, m2ov, v0_3d);

	/* Aim cursor directly at vertex 0's view position */
	vect_t cursor;
	VSET(cursor, v0_view[X], v0_view[Y], 0.0);

	rt_edit_set_edflag(s, ECMD_SKETCH_PICK_VERTEX);
	(*EDOBJ[dp->d_minor_type].ft_edit_xy)(s, cursor);
	rt_edit_process(s);

	if (se->curr_vert < 0)
	    bu_exit(1,
		"ERROR: ECMD_SKETCH_PICK_VERTEX (proximity): "
		"curr_vert not set\n");
	if ((size_t)se->curr_vert >= skt5->vert_count)
	    bu_exit(1,
		"ERROR: ECMD_SKETCH_PICK_VERTEX (proximity): "
		"curr_vert=%d out of range\n", se->curr_vert);
	/* Vertex 0 is at (0,0) which maps to the same view position as the
	 * cursor, so it must be the closest vertex. */
	if (se->curr_vert != 0)
	    bu_exit(1,
		"ERROR: ECMD_SKETCH_PICK_VERTEX (proximity): "
		"expected vertex 0 (at view origin), got vertex %d\n",
		se->curr_vert);

	bu_log("ECMD_SKETCH_PICK_VERTEX (proximity) SUCCESS: "
	       "cursor at view (%g,%g) → nearest vertex %d\n",
	       cursor[X], cursor[Y], se->curr_vert);
    }

    /* ================================================================
     * ECMD_SKETCH_APPEND_NURB: append a quadratic (order=3) NURB with
     * 4 control points (c_size=4, must be >= order=3).
     *
     * Using existing vertices 0..3: (0,0) (10,0) (10,10) (0,10).
     * After the call:
     *   - curve.count increases by 1
     *   - New segment is CURVE_NURB_MAGIC, order=3, c_size=4, rational=0
     *   - k_size = 4 + 3 = 7
     *   - Auto clamped-uniform knot vector:
     *       [0, 0, 0, 1, 2, 2, 2]  (3 repeated zeros, interior at 1,
     *        2 = c_size-order+1 repeated end value)
     * ================================================================*/
    {
	struct rt_sketch_internal *skt_n =
	    (struct rt_sketch_internal *)s->es_int.idb_ptr;

	size_t sc_before = skt_n->curve.count;

	EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, ECMD_SKETCH_APPEND_NURB);
	/* e_para[0] = order=3; e_para[1..4] = vert indices 0,1,2,3 */
	s->e_inpara  = 5;
	s->e_para[0] = 3.0;  /* order */
	s->e_para[1] = 0.0;  /* ctrl pt 0 */
	s->e_para[2] = 1.0;  /* ctrl pt 1 */
	s->e_para[3] = 2.0;  /* ctrl pt 2 */
	s->e_para[4] = 3.0;  /* ctrl pt 3 */
	bu_vls_trunc(s->log_str, 0);

	rt_edit_process(s);
	if (bu_vls_strlen(s->log_str))
	    bu_exit(1, "ERROR: ECMD_SKETCH_APPEND_NURB: %s\n",
		    bu_vls_cstr(s->log_str));

	if (skt_n->curve.count != sc_before + 1)
	    bu_exit(1,
		"ERROR: ECMD_SKETCH_APPEND_NURB: expected curve.count=%zu, got %zu\n",
		sc_before + 1, skt_n->curve.count);

	size_t nurb_idx = sc_before;  /* index of newly appended segment */
	void *nseg = skt_n->curve.segment[nurb_idx];
	if (!nseg || *(uint32_t *)nseg != CURVE_NURB_MAGIC)
	    bu_exit(1, "ERROR: ECMD_SKETCH_APPEND_NURB: wrong segment type\n");

	struct nurb_seg *ns = (struct nurb_seg *)nseg;
	if (ns->order != 3 || ns->c_size != 4)
	    bu_exit(1,
		"ERROR: ECMD_SKETCH_APPEND_NURB: order=%d c_size=%d (expected 3,4)\n",
		ns->order, ns->c_size);
	if (ns->weights != NULL)
	    bu_exit(1, "ERROR: ECMD_SKETCH_APPEND_NURB: weights should be NULL\n");
	if (RT_NURB_IS_PT_RATIONAL(ns->pt_type))
	    bu_exit(1, "ERROR: ECMD_SKETCH_APPEND_NURB: pt_type should be non-rational\n");

	/* Expected k_size = 3 + 4 = 7 */
	if (ns->k.k_size != 7)
	    bu_exit(1,
		"ERROR: ECMD_SKETCH_APPEND_NURB: k_size=%d (expected 7)\n",
		ns->k.k_size);

	/* Expected clamped uniform knot vector: [0,0,0,1,2,2,2] */
	fastf_t expected_kv[7] = {0.0, 0.0, 0.0, 1.0, 2.0, 2.0, 2.0};
	int kv_ok = 1;
	for (int i = 0; i < 7; i++) {
	    if (fabs(ns->k.knots[i] - expected_kv[i]) > VUNITIZE_TOL)
		{ kv_ok = 0; break; }
	}
	if (!kv_ok) {
	    bu_log("ERROR: ECMD_SKETCH_APPEND_NURB: knot vector = [");
	    for (int i = 0; i < 7; i++) bu_log("%g ", ns->k.knots[i]);
	    bu_log("], expected [0,0,0,1,2,2,2]\n");
	    bu_exit(1, "ECMD_SKETCH_APPEND_NURB: wrong knot vector\n");
	}
	/* Control points should be 0,1,2,3 */
	if (ns->ctl_points[0] != 0 || ns->ctl_points[1] != 1 ||
	    ns->ctl_points[2] != 2 || ns->ctl_points[3] != 3)
	    bu_exit(1, "ERROR: ECMD_SKETCH_APPEND_NURB: wrong control points\n");

	bu_log("ECMD_SKETCH_APPEND_NURB (order=3, c_size=4) SUCCESS: "
	       "kv=[0,0,0,1,2,2,2] curve.count=%zu\n", skt_n->curve.count);

	/* ============================================================
	 * ECMD_SKETCH_NURB_EDIT_KV: replace the knot vector of the
	 * NURB segment we just created.
	 *
	 * Replace [0,0,0,1,2,2,2] with [0,0,0,0.5,2,2,2] — shifting
	 * the single interior knot from 1 to 0.5.
	 * ============================================================*/

	/* Select the NURB segment */
	se->curr_seg = (int)nurb_idx;

	EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, ECMD_SKETCH_NURB_EDIT_KV);
	/* e_para[0] = k_size=7; e_para[1..7] = new knot values */
	s->e_inpara  = 8;
	s->e_para[0] = 7.0;   /* k_size */
	s->e_para[1] = 0.0;
	s->e_para[2] = 0.0;
	s->e_para[3] = 0.0;
	s->e_para[4] = 0.5;   /* moved interior knot */
	s->e_para[5] = 2.0;
	s->e_para[6] = 2.0;
	s->e_para[7] = 2.0;
	bu_vls_trunc(s->log_str, 0);

	rt_edit_process(s);
	if (bu_vls_strlen(s->log_str))
	    bu_exit(1, "ERROR: ECMD_SKETCH_NURB_EDIT_KV: %s\n",
		    bu_vls_cstr(s->log_str));

	fastf_t expected_kv2[7] = {0.0, 0.0, 0.0, 0.5, 2.0, 2.0, 2.0};
	int kv2_ok = 1;
	for (int i = 0; i < 7; i++) {
	    if (fabs(ns->k.knots[i] - expected_kv2[i]) > VUNITIZE_TOL)
		{ kv2_ok = 0; break; }
	}
	if (!kv2_ok) {
	    bu_log("ERROR: ECMD_SKETCH_NURB_EDIT_KV: knot vector = [");
	    for (int i = 0; i < 7; i++) bu_log("%g ", ns->k.knots[i]);
	    bu_log("], expected [0,0,0,0.5,2,2,2]\n");
	    bu_exit(1, "ECMD_SKETCH_NURB_EDIT_KV: wrong knot vector\n");
	}
	bu_log("ECMD_SKETCH_NURB_EDIT_KV SUCCESS: kv=[0,0,0,0.5,2,2,2]\n");

	/* ============================================================
	 * ECMD_SKETCH_NURB_EDIT_WEIGHTS: make the segment rational by
	 * setting 4 weights = {1, 2, 2, 1}.
	 *
	 * After: pt_type should be rational, weights array filled.
	 * ============================================================*/
	EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, ECMD_SKETCH_NURB_EDIT_WEIGHTS);
	/* e_para[0]=seg_index; e_para[1]=c_size=4; e_para[2..5]=weights */
	s->e_inpara  = 6;
	s->e_para[0] = (fastf_t)nurb_idx;  /* segment index */
	s->e_para[1] = 4.0;                /* c_size */
	s->e_para[2] = 1.0;
	s->e_para[3] = 2.0;
	s->e_para[4] = 2.0;
	s->e_para[5] = 1.0;
	bu_vls_trunc(s->log_str, 0);

	rt_edit_process(s);
	if (bu_vls_strlen(s->log_str))
	    bu_exit(1, "ERROR: ECMD_SKETCH_NURB_EDIT_WEIGHTS: %s\n",
		    bu_vls_cstr(s->log_str));

	if (!RT_NURB_IS_PT_RATIONAL(ns->pt_type))
	    bu_exit(1,
		"ERROR: ECMD_SKETCH_NURB_EDIT_WEIGHTS: pt_type not rational\n");
	if (!ns->weights)
	    bu_exit(1, "ERROR: ECMD_SKETCH_NURB_EDIT_WEIGHTS: weights array is NULL\n");

	fastf_t expected_w[4] = {1.0, 2.0, 2.0, 1.0};
	for (int i = 0; i < 4; i++) {
	    if (fabs(ns->weights[i] - expected_w[i]) > VUNITIZE_TOL)
		bu_exit(1,
		    "ERROR: ECMD_SKETCH_NURB_EDIT_WEIGHTS: weight[%d]=%g "
		    "(expected %g)\n", i, ns->weights[i], expected_w[i]);
	}
	bu_log("ECMD_SKETCH_NURB_EDIT_WEIGHTS SUCCESS: "
	       "weights=[1,2,2,1] rational=%d\n",
	       RT_NURB_IS_PT_RATIONAL(ns->pt_type));

	/* ============================================================
	 * ECMD_SKETCH_DELETE_SEGMENT: delete the NURB segment and verify
	 * that NURB-specific memory (knots, ctl_points, weights) is freed
	 * without a crash (valgrind would catch leaks in CI).
	 * ============================================================*/
	se->curr_seg = (int)nurb_idx;
	rt_edit_set_edflag(s, ECMD_SKETCH_DELETE_SEGMENT);
	s->e_inpara = 0;
	bu_vls_trunc(s->log_str, 0);

	rt_edit_process(s);
	if (bu_vls_strlen(s->log_str))
	    bu_exit(1, "ERROR: ECMD_SKETCH_DELETE_SEGMENT (nurb): %s\n",
		    bu_vls_cstr(s->log_str));

	if (skt_n->curve.count != sc_before)
	    bu_exit(1,
		"ERROR: ECMD_SKETCH_DELETE_SEGMENT (nurb): "
		"expected count=%zu, got %zu\n",
		sc_before, skt_n->curve.count);
	bu_log("ECMD_SKETCH_DELETE_SEGMENT (nurb) SUCCESS: "
	       "curve.count back to %zu\n", skt_n->curve.count);

	/* ============================================================
	 * Error-path checks for ECMD_SKETCH_APPEND_NURB:
	 *   (a) c_size < order → curve.count must NOT increase
	 *   (b) out-of-range control point index → curve.count must NOT increase
	 * ============================================================*/
	{
	    size_t sc_err = skt_n->curve.count;

	    /* (a) order=4 but only 2 control points (c_size=2 < order=4) */
	    EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, ECMD_SKETCH_APPEND_NURB);
	    s->e_inpara  = 3;
	    s->e_para[0] = 4.0;  /* order */
	    s->e_para[1] = 0.0;
	    s->e_para[2] = 1.0;
	    rt_edit_process(s);
	    if (skt_n->curve.count != sc_err)
		bu_exit(1,
		    "ERROR: ECMD_SKETCH_APPEND_NURB should reject c_size < order "
		    "(count changed from %zu to %zu)\n",
		    sc_err, skt_n->curve.count);
	    bu_log("ECMD_SKETCH_APPEND_NURB (c_size<order) correctly rejected\n");

	    /* (b) order=2, 3 control points but last index is bogus */
	    EDOBJ[dp->d_minor_type].ft_set_edit_mode(s, ECMD_SKETCH_APPEND_NURB);
	    s->e_inpara  = 4;
	    s->e_para[0] = 2.0;     /* order */
	    s->e_para[1] = 0.0;
	    s->e_para[2] = 1.0;
	    s->e_para[3] = 9999.0;  /* bogus vertex index */
	    rt_edit_process(s);
	    if (skt_n->curve.count != sc_err)
		bu_exit(1,
		    "ERROR: ECMD_SKETCH_APPEND_NURB should reject out-of-range vert\n");
	    bu_log("ECMD_SKETCH_APPEND_NURB (bad vert index) correctly rejected\n");
	}
    }

    /* New vertex coordinates are lengths, but its index is unitless. */
    s->local2base = 25.4;
    s->base2local = 1.0 / s->local2base;
    size_t old_vert_count = skt->vert_count;
    rt_edit_set_edflag(s, ECMD_SKETCH_ADD_VERTEX);
    s->e_inpara = 2;
    s->e_para[0] = 1.0;
    s->e_para[1] = 2.0;
    if (EDOBJ[dp->d_minor_type].ft_edit(s) != BRLCAD_OK ||
	skt->vert_count != old_vert_count + 1 ||
	se->curr_vert != (int)old_vert_count ||
	!NEAR_EQUAL(skt->verts[old_vert_count][0], 25.4, VUNITIZE_TOL) ||
	!NEAR_EQUAL(skt->verts[old_vert_count][1], 50.8, VUNITIZE_TOL))
	bu_exit(1, "ERROR: sketch add vertex did not convert local units\n");
    rt_edit_destroy(s);
    db_free_full_path(&fp);
    bv_free(v);
    db_close(dbip);
    return sketch_operation_matrix() ? BRLCAD_ERROR : BRLCAD_OK;
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
