/*         M E T A B A L L _ O P E R A T I O N _ M A T R I X . C P P
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
/** @file metaball_operation_matrix.cpp
 *
 * Exercise each advertised metaball edit from a persisted object and
 * compare the complete result in millimeter and inch databases.
 */

#include "common.h"

#include <math.h>

#include "bu/log.h"
#include "bu/str.h"
#include "bu/vls.h"
#include "raytrace.h"
#include "rt/rt_ecmds.h"

struct directory *make_metaball(struct rt_wdb *);

enum {
    METABALL_NEXT = 30121, METABALL_PREV = 30122,
    METABALL_ADD = 36089, METABALL_DEL = 36088,
    METABALL_FIELD = 36087, METABALL_MOVE = 36086,
    METABALL_PICK = 36085, METABALL_SCALE_BLOB = 30119,
    METABALL_SET_BLOB = 30120, METABALL_METHOD = 36084,
    METABALL_THRESHOLD = 36083,
    METABALL_COMMAND_COUNT = 11, METABALL_MAX_POINTS = 3
};

static const fastf_t inch_to_mm = 25.4;

struct metaball_edit_state {
    struct wdb_metaball_pnt *selected;
};

struct expected_point {
    point_t coord;
    fastf_t strength;
    fastf_t blob;
};

struct expected_state {
    int method;
    fastf_t threshold;
    size_t count;
    struct expected_point points[METABALL_MAX_POINTS];
};

static struct expected_state
initial_state(void)
{
    struct expected_state state = {};
    state.method = METABALL_METABALL;
    state.threshold = 0.5;
    state.count = 2;
    VSET(state.points[0].coord, 1, 0, 0);
    state.points[0].strength = 1;
    state.points[0].blob = 0.2;
    VSET(state.points[1].coord, -1, 0, 0);
    state.points[1].strength = 2;
    state.points[1].blob = 0.3;
    return state;
}

static struct wdb_metaball_pnt *
point_at(struct rt_metaball_internal *ball, size_t index)
{
    struct wdb_metaball_pnt *point;
    size_t current = 0;
    for (BU_LIST_FOR(point, wdb_metaball_pnt, &ball->metaball_ctrl_head)) {
	if (current++ == index)
	    return point;
    }
    return NULL;
}

static bool
state_matches(struct rt_edit *edit, const struct expected_state *expected,
	      int selected_index)
{
    struct rt_metaball_internal *actual =
	(struct rt_metaball_internal *)edit->es_int.idb_ptr;
    struct metaball_edit_state *selection =
	(struct metaball_edit_state *)edit->ipe_ptr;
    if (actual->method != expected->method ||
	!NEAR_EQUAL(actual->threshold, expected->threshold, VUNITIZE_TOL)) {
	bu_log("metaball globals: expected method=%d threshold=%g, got %d %g\n",
	    expected->method, expected->threshold, actual->method, actual->threshold);
	return false;
    }

    size_t count = 0;
    struct wdb_metaball_pnt *point;
    for (BU_LIST_FOR(point, wdb_metaball_pnt, &actual->metaball_ctrl_head)) {
	if (count >= expected->count ||
	    !VNEAR_EQUAL(point->coord, expected->points[count].coord, VUNITIZE_TOL) ||
	    !NEAR_EQUAL(point->field_strength, expected->points[count].strength,
		VUNITIZE_TOL) ||
	    !NEAR_EQUAL(point->blobbiness, expected->points[count].blob,
		VUNITIZE_TOL)) {
	    bu_log("metaball point %zu: got (%g %g %g; %g; %g)\n", count,
		V3ARGS(point->coord), point->field_strength, point->blobbiness);
	    return false;
	}
	++count;
    }
    if (count != expected->count) {
	bu_log("metaball point count: expected %zu, got %zu\n",
	    expected->count, count);
	return false;
    }
    return selection->selected ==
	(selected_index < 0 ? NULL : point_at(actual, (size_t)selected_index));
}

static bool
param_queries_match(struct rt_edit *edit, const struct expected_state *expected,
		    fastf_t base2local)
{
    int (*get_params)(struct rt_edit *, int, fastf_t *) =
	EDOBJ[ID_METABALL].ft_edit_get_params;
    if (!get_params)
	return false;

    fastf_t values[3] = VINIT_ZERO;
    if (get_params(edit, METABALL_THRESHOLD, values) != 1 ||
	!NEAR_EQUAL(values[0], expected->threshold, VUNITIZE_TOL) ||
	get_params(edit, METABALL_METHOD, values) != 1 ||
	!NEAR_EQUAL(values[0], (fastf_t)expected->method, VUNITIZE_TOL) ||
	get_params(edit, METABALL_FIELD, values) != 0 ||
	get_params(edit, METABALL_MOVE, values) != 0)
	return false;

    struct metaball_edit_state *selection =
	(struct metaball_edit_state *)edit->ipe_ptr;
    selection->selected = point_at(
	(struct rt_metaball_internal *)edit->es_int.idb_ptr, 0);
    if (!selection->selected ||
	get_params(edit, METABALL_FIELD, values) != 1 ||
	!NEAR_EQUAL(values[0], expected->points[0].strength * base2local,
	    VUNITIZE_TOL) ||
	get_params(edit, METABALL_SCALE_BLOB, values) != 1 ||
	!NEAR_EQUAL(values[0], expected->points[0].blob, VUNITIZE_TOL) ||
	get_params(edit, METABALL_SET_BLOB, values) != 1 ||
	!NEAR_EQUAL(values[0], expected->points[0].blob, VUNITIZE_TOL))
	return false;

    const int point_queries[] = {METABALL_PICK, METABALL_MOVE, METABALL_ADD};
    for (int command : point_queries) {
	if (get_params(edit, command, values) != 3 ||
	    !NEAR_EQUAL(values[X], expected->points[0].coord[X] * base2local,
		VUNITIZE_TOL) ||
	    !ZERO(values[Y]) || !ZERO(values[Z]))
	    return false;
    }
    return get_params(edit, METABALL_DEL, values) == 0 &&
	get_params(edit, METABALL_FIELD, NULL) == -1;
}

static int
run_param_case(struct db_i *dbip, struct db_full_path *path,
	       struct bn_tol *tol, fastf_t local2base, const char *unit)
{
    struct rt_edit *edit = rt_edit_create(path, dbip, tol, NULL);
    if (!edit)
	return 1;
    if (!EDOBJ[ID_METABALL].ft_write_params ||
	!EDOBJ[ID_METABALL].ft_read_params) {
	rt_edit_destroy(edit);
	return 1;
    }

    struct bu_vls actual = BU_VLS_INIT_ZERO;
    struct bu_vls expected = BU_VLS_INIT_ZERO;
    const struct expected_state base = initial_state();
    const fastf_t base2local = 1.0 / local2base;
    EDOBJ[ID_METABALL].ft_write_params(&actual, &edit->es_int, tol,
	base2local);
    bu_vls_printf(&expected, "method: %d\nthreshold: %.9f\n",
	base.method, base.threshold);
    for (size_t i = 0; i < base.count; ++i) {
	const struct expected_point *point = &base.points[i];
	bu_vls_printf(&expected,
	    "point[%zu]: %.9f %.9f %.9f field_strength=%.9f blobbiness=%.9f\n",
	    i, point->coord[X] * base2local,
	    point->coord[Y] * base2local,
	    point->coord[Z] * base2local,
	    point->strength * base2local, point->blob);
    }

    bool passed = BU_STR_EQUAL(bu_vls_cstr(&actual),
	bu_vls_cstr(&expected));
    if (!passed)
	bu_log("metaball parameter text (%s):\n%s", unit,
	    bu_vls_cstr(&actual));
    struct metaball_edit_state *selection =
	(struct metaball_edit_state *)edit->ipe_ptr;
    selection->selected = NULL;
    bool read_ok = EDOBJ[ID_METABALL].ft_read_params(&edit->es_int,
	bu_vls_cstr(&actual), tol, local2base) == BRLCAD_OK;
    passed = read_ok && state_matches(edit, &base, -1) &&
	param_queries_match(edit, &base, base2local) && passed;

    struct expected_state optional = base;
    optional.count = 1;
    VSET(optional.points[0].coord, local2base, 0, 0);
    optional.points[0].strength = local2base;
    optional.points[0].blob = 1.0;
    selection->selected = NULL;
    read_ok = EDOBJ[ID_METABALL].ft_read_params(&edit->es_int,
	"method: 0\r\nthreshold: 0.5\r\npoint[0]: 1 0 0\r\n",
	tol, local2base) == BRLCAD_OK;
    passed = read_ok && state_matches(edit, &optional, -1) && passed;

    optional.points[0].strength = 2.0 * local2base;
    read_ok = EDOBJ[ID_METABALL].ft_read_params(&edit->es_int,
	"method: 0\nthreshold: 0.5\npoint[0]: 1 0 0 field_strength=2\n",
	tol, local2base) == BRLCAD_OK;
    passed = read_ok && state_matches(edit, &optional, -1) && passed;
    bu_log("metaball\tparams\t%s\t%s\n", unit,
	passed ? "pass" : "fail");
    bu_vls_free(&expected);
    bu_vls_free(&actual);
    rt_edit_destroy(edit);
    return passed ? 0 : 1;
}

static int
run_bad_param_case(struct db_i *dbip, struct db_full_path *path,
	   struct bn_tol *tol, fastf_t local2base, const char *unit,
	   const char *name, const char *params)
{
    struct rt_edit *edit = rt_edit_create(path, dbip, tol, NULL);
    if (!edit)
	return 1;

    struct metaball_edit_state *selection =
	(struct metaball_edit_state *)edit->ipe_ptr;
    selection->selected = point_at(
	(struct rt_metaball_internal *)edit->es_int.idb_ptr, 0);
    const struct expected_state base = initial_state();
    int result = EDOBJ[ID_METABALL].ft_read_params(&edit->es_int, params,
	tol, local2base);
    bool unchanged = state_matches(edit, &base, 0);
    bool passed = result == BRLCAD_ERROR && unchanged;
    if (!passed)
	bu_log("metaball parameter rejection: status=%d unchanged=%d\n",
	    result, unchanged);
    bu_log("metaball\tparams\t%s\t%s\t%s\n", unit,
	passed ? "pass" : "fail", name);
    rt_edit_destroy(edit);
    return passed ? 0 : 1;
}

static bool
commands_match(void)
{
    const int commands[] = {
	METABALL_THRESHOLD, METABALL_METHOD, METABALL_PICK,
	METABALL_NEXT, METABALL_PREV, METABALL_MOVE, METABALL_ADD,
	METABALL_DEL, METABALL_FIELD, METABALL_SCALE_BLOB,
	METABALL_SET_BLOB
    };
    const struct rt_edit_prim_desc *desc = EDOBJ[ID_METABALL].ft_edit_desc ?
	EDOBJ[ID_METABALL].ft_edit_desc() : NULL;
    if (!desc || desc->ncmd != METABALL_COMMAND_COUNT)
	return false;
    for (int i = 0; i < desc->ncmd; ++i) {
	bool found = false;
	for (int command : commands)
	    found |= desc->cmds[i].cmd_id == command;
	if (!found)
	    return false;
    }
    return true;
}

static int
run_case(struct db_i *dbip, struct db_full_path *path, struct bn_tol *tol,
	 struct bview *view, const char *unit, const char *name, int command,
	 const fastf_t *parameters, size_t parameter_count, int selected_before,
	 int selected_after, const struct expected_state *expected,
	 bool expect_error = false)
{
    struct rt_edit *edit = rt_edit_create(path, dbip, tol, view);
    bool passed = edit != NULL;
    if (passed) {
	struct rt_metaball_internal *ball =
	    (struct rt_metaball_internal *)edit->es_int.idb_ptr;
	struct metaball_edit_state *selection =
	    (struct metaball_edit_state *)edit->ipe_ptr;
	selection->selected = selected_before < 0 ? NULL :
	    point_at(ball, (size_t)selected_before);
	if (command == METABALL_NEXT || command == METABALL_PREV) {
	    EDOBJ[ID_METABALL].ft_set_edit_mode(edit, command);
	} else {
	    rt_edit_set_edflag(edit, command);
	    edit->e_inpara = (int)parameter_count;
	    for (size_t i = 0; i < parameter_count; ++i)
		edit->e_para[i] = parameters[i];
	    int result = rt_edit_process(edit);
	    passed = expect_error ? result == BRLCAD_ERROR : result == BRLCAD_OK;
	}
	passed = state_matches(edit, expected, selected_after) && passed &&
	    NEAR_EQUAL(edit->local2base, dbip->dbi_local2base, VUNITIZE_TOL);
	for (size_t i = 0; i < parameter_count; ++i) {
	    if (isnan(parameters[i]) ? !isnan(edit->e_para[i]) :
		!NEAR_EQUAL(edit->e_para[i], parameters[i], VUNITIZE_TOL))
		passed = false;
	}
	if (!passed)
	    bu_log("%s: %s\n", name, bu_vls_cstr(edit->log_str));
    }
    bu_log("metaball\t%d\t%s\t%s\t%s\n", command, unit,
	passed ? "pass" : "fail", name);
    if (edit)
	rt_edit_destroy(edit);
    return passed ? 0 : 1;
}

static int
run_unit(fastf_t local2base, const char *unit)
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
    struct directory *dp = make_metaball(wdbp);
    struct db_full_path path;
    db_full_path_init(&path);
    db_add_node_to_full_path(&path, dp);
    struct bn_tol tol = BN_TOL_INIT_TOL;
    struct bview view = {};
    MAT_IDN(view.gv_view2model);
    struct expected_state base = initial_state();
    struct expected_state expected;
    int failures = run_param_case(dbip, &path, &tol, local2base, unit);
    const struct {
	const char *name;
	const char *params;
    } bad_params[] = {
	{"missing method", "threshold: 0.5\n"},
	{"invalid method", "method: x\nthreshold: 0.5\n"},
	{"unsupported method", "method: 3\nthreshold: 0.5\n"},
	{"invalid threshold", "method: 0\nthreshold: x\n"},
	{"nonfinite threshold", "method: 0\nthreshold: nan\n"},
	{"invalid point", "method: 0\nthreshold: 0.5\npoint[0]: 1 0\n"},
	{"missing delimiter", "method: 0\nthreshold: 0.5\npoint[0: 1 0 0 field_strength=1 blobbiness=0.2\n"},
	{"nonfinite point", "method: 0\nthreshold: 0.5\npoint[0]: nan 0 0 field_strength=1 blobbiness=0.2\n"},
	{"invalid strength", "method: 0\nthreshold: 0.5\npoint[0]: 1 0 0 field_strength=x\n"},
	{"invalid blobbiness", "method: 0\nthreshold: 0.5\npoint[0]: 1 0 0 field_strength=1 blobbiness=x\n"},
	{"late invalid point", "method: 0\nthreshold: 0.5\npoint[0]: 1 0 0 field_strength=1 blobbiness=0.2\npoint[1]: invalid\n"},
	{"unexpected point index", "method: 0\nthreshold: 0.5\npoint[1]: 1 0 0 field_strength=1 blobbiness=0.2\n"},
	{"trailing text", "method: 0\nthreshold: 0.5\npoint[0]: 1 0 0 field_strength=1 blobbiness=0.2 extra\n"},
	{"blank point line", "method: 0\nthreshold: 0.5\n\npoint[0]: 1 0 0\n"},
    };
    for (const auto &bad : bad_params)
	failures += run_bad_param_case(dbip, &path, &tol, local2base,
	    unit, bad.name, bad.params);
    if (!EQUAL(local2base, 1.0))
	failures += run_bad_param_case(dbip, &path, &tol, local2base,
	    unit, "unit conversion overflow",
	    "method: 0\nthreshold: 0.5\npoint[0]: 1e308 0 0\n");

    expected = base;
    expected.threshold = 0.7;
    const fastf_t threshold[] = {0.7};
    failures += run_case(dbip, &path, &tol, &view, unit, "set threshold",
	METABALL_THRESHOLD, threshold, 1, -1, -1, &expected);

    expected = base;
    expected.method = METABALL_BLOB;
    const fastf_t method[] = {METABALL_BLOB};
    failures += run_case(dbip, &path, &tol, &view, unit, "set method",
	METABALL_METHOD, method, 1, -1, -1, &expected);

    expected = base;
    const fastf_t zero_method[] = {METABALL_METABALL};
    failures += run_case(dbip, &path, &tol, &view, unit, "set method zero",
	METABALL_METHOD, zero_method, 1, -1, -1, &expected);

    const fastf_t pick[] = {1.0 / local2base, 0, 0};
    failures += run_case(dbip, &path, &tol, &view, unit, "pick point",
	METABALL_PICK, pick, 3, -1, 0, &base);
    failures += run_case(dbip, &path, &tol, NULL, unit,
	"pick point without view", METABALL_PICK, pick, 3, -1, 0, &base);

    failures += run_case(dbip, &path, &tol, &view, unit, "next point",
	METABALL_NEXT, NULL, 0, 0, 1, &base);
    failures += run_case(dbip, &path, &tol, &view, unit, "previous point",
	METABALL_PREV, NULL, 0, 1, 0, &base);

    expected = base;
    expected.points[0].coord[X] += 2.0 * inch_to_mm;
    const fastf_t move[] = {2.0 * inch_to_mm / local2base, 0, 0};
    failures += run_case(dbip, &path, &tol, &view, unit, "move point",
	METABALL_MOVE, move, 3, 0, 0, &expected);

    expected = base;
    expected.count = 3;
    expected.points[2] = expected.points[1];
    VSET(expected.points[1].coord, 2.0 * inch_to_mm, 0, 0);
    expected.points[1].strength = 1.0;
    expected.points[1].blob = 1.0;
    const fastf_t add[] = {2.0 * inch_to_mm / local2base, 0, 0};
    failures += run_case(dbip, &path, &tol, &view, unit, "add point",
	METABALL_ADD, add, 3, -1, 1, &expected);

    expected = base;
    expected.count = 1;
    expected.points[0] = expected.points[1];
    failures += run_case(dbip, &path, &tol, &view, unit, "delete point",
	METABALL_DEL, NULL, 0, 0, 0, &expected);

    expected = base;
    expected.points[0].strength = 2.0;
    const fastf_t double_value[] = {2.0};
    failures += run_case(dbip, &path, &tol, &view, unit, "scale field",
	METABALL_FIELD, double_value, 1, 0, 0, &expected);

    expected = base;
    expected.points[0].blob = 0.4;
    failures += run_case(dbip, &path, &tol, &view, unit, "scale blobbiness",
	METABALL_SCALE_BLOB, double_value, 1, 0, 0, &expected);

    expected = base;
    expected.points[0].blob = 0.5;
    const fastf_t set_blob[] = {0.5};
    failures += run_case(dbip, &path, &tol, &view, unit, "set blobbiness",
	METABALL_SET_BLOB, set_blob, 1, 0, 0, &expected);

    const fastf_t fractional_method[] = {1.5};
    failures += run_case(dbip, &path, &tol, &view, unit,
	"reject fractional method", METABALL_METHOD, fractional_method, 1,
	-1, -1, &base, true);
    const fastf_t invalid_method[] = {METABALL_BLOB + 1.0};
    failures += run_case(dbip, &path, &tol, &view, unit,
	"reject out-of-range method", METABALL_METHOD, invalid_method, 1,
	-1, -1, &base, true);
    const fastf_t nonfinite[] = {NAN};
    failures += run_case(dbip, &path, &tol, &view, unit,
	"reject nonfinite threshold", METABALL_THRESHOLD, nonfinite, 1,
	-1, -1, &base, true);
    failures += run_case(dbip, &path, &tol, &view, unit,
	"reject nonfinite field", METABALL_FIELD, nonfinite, 1,
	0, 0, &base, true);
    failures += run_case(dbip, &path, &tol, &view, unit,
	"reject nonfinite blobbiness", METABALL_SET_BLOB, nonfinite, 1,
	0, 0, &base, true);
    const fastf_t short_point[] = {1.0 / local2base, 0};
    failures += run_case(dbip, &path, &tol, &view, unit,
	"reject short pick", METABALL_PICK, short_point, 2,
	-1, -1, &base, true);
    failures += run_case(dbip, &path, &tol, &view, unit,
	"reject short move", METABALL_MOVE, short_point, 2,
	0, 0, &base, true);
    failures += run_case(dbip, &path, &tol, &view, unit,
	"reject short add", METABALL_ADD, short_point, 2,
	-1, -1, &base, true);
    const fastf_t nonfinite_point[] = {NAN, 0, 0};
    failures += run_case(dbip, &path, &tol, &view, unit,
	"reject nonfinite move", METABALL_MOVE, nonfinite_point, 3,
	0, 0, &base, true);
    failures += run_case(dbip, &path, &tol, &view, unit,
	"reject nonfinite add", METABALL_ADD, nonfinite_point, 3,
	-1, -1, &base, true);
    failures += run_case(dbip, &path, &tol, &view, unit,
	"reject unselected delete", METABALL_DEL, NULL, 0,
	-1, -1, &base, true);

    db_free_full_path(&path);
    db_close(dbip);
    return failures;
}

int
rt_edit_test_metaball_operation_matrix(void)
{
    if (!commands_match()) {
	bu_log("Metaball descriptor differs from operation matrix\n");
	return BRLCAD_ERROR;
    }
    bu_log("primitive\tcommand_id\tunits\tresult\toperation\n");
    int failures = run_unit(1.0, "mm");
    failures += run_unit(inch_to_mm, "in");
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
