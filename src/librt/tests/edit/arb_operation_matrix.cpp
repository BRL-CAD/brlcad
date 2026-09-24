/*              A R B _ O P E R A T I O N _ M A T R I X . C P P
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
/** @file arb_operation_matrix.cpp
 *
 * Check complete ARB edit results from fresh database objects in mm
 * and inch databases.  Numeric point inputs are always local units.
 */

#include "common.h"

#include <math.h>
#include <string.h>

#include "bu/log.h"
#include "bu/malloc.h"
#include "bu/str.h"
#include "bu/vls.h"
#include "raytrace.h"
#include "rt/db4.h"
#include "rt/primitives/arb8.h"
#include "rt/rt_ecmds.h"

enum {
    ARB_MOVE_EDGE = 4009,
    ARB_MOVE_VERTEX = 4010,
    ARB_MOVE_FACE = 4013,
    ARB_ROTATE_FACE = 4015,
    ARB_DESCRIPTOR_COMMAND_COUNT = 4,
    ARB_EDGE_POINT_COUNT = 2,
    ARB5_BASE_EDGE_COUNT = 4,
    ARB_FACE_COUNT = 6,
    ARB_FIRST_FACE_POINT_COUNT = 4,
    ARB_POINT_COUNT = 8
};

static const fastf_t inch_to_mm = 25.4;

static void
init_cube(struct rt_arb_internal *arb, fastf_t side)
{
    memset(arb, 0, sizeof(*arb));
    arb->magic = RT_ARB_INTERNAL_MAGIC;
    VSET(arb->pt[0], 0, 0, 0);
    VSET(arb->pt[1], side, 0, 0);
    VSET(arb->pt[2], side, side, 0);
    VSET(arb->pt[3], 0, side, 0);
    VSET(arb->pt[4], 0, 0, side);
    VSET(arb->pt[5], side, 0, side);
    VSET(arb->pt[6], side, side, side);
    VSET(arb->pt[7], 0, side, side);
}

static void
init_pyramid(struct rt_arb_internal *arb, fastf_t side)
{
    init_cube(arb, side);
    VSET(arb->pt[4], side / 2, side / 2, side);
    for (int i = 5; i < ARB_POINT_COUNT; i++)
	VMOVE(arb->pt[i], arb->pt[4]);
}

static void
init_tetrahedron(struct rt_arb_internal *arb, fastf_t side)
{
    memset(arb, 0, sizeof(*arb));
    arb->magic = RT_ARB_INTERNAL_MAGIC;
    VSET(arb->pt[0], 0, 0, 0);
    VSET(arb->pt[1], side, 0, 0);
    VSET(arb->pt[2], 0, side, 0);
    VMOVE(arb->pt[3], arb->pt[0]);
    VSET(arb->pt[4], 0, 0, side);
    for (int i = 5; i < ARB_POINT_COUNT; i++)
	VMOVE(arb->pt[i], arb->pt[4]);
}

static void
init_arb6(struct rt_arb_internal *arb, fastf_t side)
{
    memset(arb, 0, sizeof(*arb));
    arb->magic = RT_ARB_INTERNAL_MAGIC;
    VSET(arb->pt[0], 0, 0, 0);
    VSET(arb->pt[1], 0, side, 0);
    VSET(arb->pt[2], 0, side, side);
    VSET(arb->pt[3], 0, 0, side / 2);
    VSET(arb->pt[4], side, side / 2, 0);
    VMOVE(arb->pt[5], arb->pt[4]);
    VSET(arb->pt[6], side, side / 2, side);
    VMOVE(arb->pt[7], arb->pt[6]);
}

static void
init_arb7(struct rt_arb_internal *arb, fastf_t side)
{
    memset(arb, 0, sizeof(*arb));
    arb->magic = RT_ARB_INTERNAL_MAGIC;
    VSET(arb->pt[0], 0, 0, 0);
    VSET(arb->pt[1], 0, side, 0);
    VSET(arb->pt[2], 0, side, side);
    VSET(arb->pt[3], 0, 0, side / 2);
    VSET(arb->pt[4], side, 0, 0);
    VSET(arb->pt[5], side, side, 0);
    VSET(arb->pt[6], side, side, side / 2);
    VMOVE(arb->pt[7], arb->pt[4]);
}

static const struct rt_edit_cmd_desc *
find_command(int command_id)
{
    const struct rt_edit_prim_desc *desc = EDOBJ[ID_ARB8].ft_edit_desc ?
	EDOBJ[ID_ARB8].ft_edit_desc() : NULL;
    if (!desc)
	return NULL;
    for (int i = 0; i < desc->ncmd; i++) {
	if (desc->cmds[i].cmd_id == command_id)
	    return &desc->cmds[i];
    }
    return NULL;
}

static bool
same_points(const struct rt_arb_internal *actual,
            const struct rt_arb_internal *expected)
{
    bool same = true;
    for (int i = 0; i < ARB_POINT_COUNT; i++) {
	if (!VNEAR_EQUAL(actual->pt[i], expected->pt[i], VUNITIZE_TOL)) {
	    bu_log("pt[%d]: expected (%g %g %g), got (%g %g %g)\n",
		   i, V3ARGS(expected->pt[i]), V3ARGS(actual->pt[i]));
	    same = false;
	}
    }
    return same;
}

static void
init_read_fixture(struct rt_arb_internal *target,
		  const struct rt_arb_internal *source)
{
    const fastf_t distinct_scale = 2.0;
    *target = *source;
    for (int i = 0; i < ARB_POINT_COUNT; ++i)
	VSCALE(target->pt[i], target->pt[i], distinct_scale);
}

static bool
reject_params(struct rt_db_internal *ip, const struct bn_tol *tol,
	      fastf_t local2base, const struct rt_arb_internal *before,
	      const char *text)
{
    struct rt_arb_internal *parsed = (struct rt_arb_internal *)ip->idb_ptr;
    *parsed = *before;
    int result = EDOBJ[ID_ARB8].ft_read_params(ip, text, tol, local2base);
    return result == BRLCAD_ERROR && same_points(parsed, before);
}

static int
run_param_case(const char *name, const char *unit, fastf_t local2base,
	       const struct rt_arb_internal *initial, const int *vertices,
	       size_t vertex_count)
{
    if (!EDOBJ[ID_ARB8].ft_write_params ||
	!EDOBJ[ID_ARB8].ft_read_params)
	return 1;
    struct bn_tol tol = BN_TOL_INIT_TOL;
    struct rt_db_internal ip;
    RT_DB_INTERNAL_INIT(&ip);
    ip.idb_type = ID_ARB8;
    ip.idb_meth = &OBJ[ID_ARB8];
    ip.idb_ptr = (void *)initial;

    struct bu_vls actual = BU_VLS_INIT_ZERO;
    struct bu_vls expected = BU_VLS_INIT_ZERO;
    struct bu_vls malformed = BU_VLS_INIT_ZERO;
    struct bu_vls wrong_index = BU_VLS_INIT_ZERO;
    struct bu_vls plain_crlf = BU_VLS_INIT_ZERO;
    const fastf_t base2local = 1.0 / local2base;
    EDOBJ[ID_ARB8].ft_write_params(&actual, &ip, &tol, base2local);
    for (size_t i = 0; i < vertex_count; ++i) {
	const point_t *point = &initial->pt[vertices[i]];
	bu_vls_printf(&plain_crlf, "%.9f %.9f %.9f\r\n",
	    (*point)[X] * base2local, (*point)[Y] * base2local,
	    (*point)[Z] * base2local);
	bu_vls_printf(&expected, "pt[%zu]: %.9f %.9f %.9f\n", i + 1,
	    (*point)[X] * base2local, (*point)[Y] * base2local,
	    (*point)[Z] * base2local);
	if (i + 1 == vertex_count) {
	    bu_vls_printf(&malformed, "pt[%zu]: invalid\n", i + 1);
	    bu_vls_printf(&wrong_index, "pt[1]: %.9f %.9f %.9f\n",
		(*point)[X] * base2local, (*point)[Y] * base2local,
		(*point)[Z] * base2local);
	} else {
	    bu_vls_printf(&malformed, "pt[%zu]: %.9f %.9f %.9f\n", i + 1,
		(*point)[X] * base2local, (*point)[Y] * base2local,
		(*point)[Z] * base2local);
	    bu_vls_printf(&wrong_index, "pt[%zu]: %.9f %.9f %.9f\n", i + 1,
		(*point)[X] * base2local, (*point)[Y] * base2local,
		(*point)[Z] * base2local);
	}
    }

    bool passed = BU_STR_EQUAL(bu_vls_cstr(&actual), bu_vls_cstr(&expected));
    if (!passed)
	bu_log("%s parameter text (%s):\n%s", name, unit,
	    bu_vls_cstr(&actual));

    struct rt_arb_internal parsed;
    init_read_fixture(&parsed, initial);
    ip.idb_ptr = &parsed;
    passed = EDOBJ[ID_ARB8].ft_read_params(&ip, bu_vls_cstr(&actual),
	&tol, local2base) == BRLCAD_OK && same_points(&parsed, initial) &&
	passed;

    init_read_fixture(&parsed, initial);
    passed = EDOBJ[ID_ARB8].ft_read_params(&ip,
	bu_vls_cstr(&plain_crlf), &tol, local2base) == BRLCAD_OK &&
	same_points(&parsed, initial) && passed;

    init_read_fixture(&parsed, initial);
    const struct rt_arb_internal before = parsed;
    passed = reject_params(&ip, &tol, local2base, &before,
	bu_vls_cstr(&malformed)) && passed;
    passed = reject_params(&ip, &tol, local2base, &before,
	bu_vls_cstr(&wrong_index)) && passed;
    struct bu_vls extra = BU_VLS_INIT_ZERO;
    bu_vls_strcpy(&extra, bu_vls_cstr(&expected));
    bu_vls_printf(&extra, "pt[%zu]: 0 0 0\n", vertex_count + 1);
    passed = reject_params(&ip, &tol, local2base, &before,
	bu_vls_cstr(&extra)) && passed;
    bu_log("%s\tparams\t%s\t%s\n", name, unit,
	passed ? "pass" : "fail");

    bu_vls_free(&extra);
    bu_vls_free(&plain_crlf);
    bu_vls_free(&wrong_index);
    bu_vls_free(&malformed);
    bu_vls_free(&expected);
    bu_vls_free(&actual);
    return passed ? 0 : 1;
}

static int
run_case(const char *name, const char *unit, fastf_t local2base,
         int arb_type, const struct rt_arb_internal *initial,
         const struct rt_arb_internal *expected, int command_id,
         const fastf_t *parameters, size_t parameter_count, bool expect_error)
{
    struct db_i *dbip = db_open_inmem();
    if (dbip == DBI_NULL) {
	bu_log("Cannot create ARB operation database for %s\n", name);
	return 1;
    }
    dbip->dbi_local2base = local2base;
    dbip->dbi_base2local = 1.0 / local2base;
    struct rt_wdb *wdbp = wdb_dbopen(dbip, RT_WDB_TYPE_DB_INMEM);
    struct rt_arb_internal *geometry;
    BU_ALLOC(geometry, struct rt_arb_internal);
    *geometry = *initial;
    if (!wdbp) {
	BU_PUT(geometry, struct rt_arb_internal);
	bu_log("Cannot open ARB operation database for %s\n", name);
	db_close(dbip);
	return 1;
    }

    bool ok = wdb_export(wdbp, "arb_matrix", geometry, ID_ARB8, 1.0) == 0;
    struct directory *dp = ok ? db_lookup(dbip, "arb_matrix", LOOKUP_QUIET) : RT_DIR_NULL;
    ok = ok && dp != RT_DIR_NULL;
    if (!ok)
	bu_log("Cannot create ARB operation fixture for %s\n", name);
    if (ok) {
	struct db_full_path path;
	db_full_path_init(&path);
	db_add_node_to_full_path(&path, dp);
	struct bn_tol tol = BN_TOL_INIT_TOL;
	struct rt_edit *edit = rt_edit_create(&path, dbip, &tol, NULL);
	const struct rt_edit_cmd_desc *command = find_command(command_id);
	ok = edit && command;
	if (ok) {
	    edit->mv_context = 0;
	    rt_edit_set_edflag(edit, command_id);
	    edit->e_inpara = parameter_count;
	    for (size_t i = 0; i < parameter_count; i++)
		edit->e_para[i] = parameters[i];
	    int result = rt_edit_process(edit);
	    struct rt_arb_internal *actual =
		(struct rt_arb_internal *)edit->es_int.idb_ptr;
	    int actual_type = rt_arb_std_type(&edit->es_int, &tol);
	    bool valid = rt_arb_check_points(actual, arb_type, &tol) == 0;
	    bool points_match = same_points(actual, expected);
	    ok = (expect_error ? result == BRLCAD_ERROR : result == BRLCAD_OK) &&
		actual_type == arb_type && valid && points_match;
	    for (size_t i = 0; i < parameter_count; i++) {
		if (!NEAR_EQUAL(edit->e_para[i], parameters[i], VUNITIZE_TOL))
		    ok = false;
	    }
	    if (!ok)
		bu_log("%s: result=%d, ARB type=%d, valid=%d, log=%s\n",
		       name, result, actual_type, valid, bu_vls_cstr(edit->log_str));
	}
	if (edit)
	    rt_edit_destroy(edit);
	db_free_full_path(&path);
    }
    bu_log("arb%d\t%d\t%s\t%s\t%s\n", arb_type, command_id, unit,
	   ok ? "pass" : "fail", name);
    db_close(dbip);
    return ok ? 0 : 1;
}

static int
run_first_face_move(const char *name, const char *unit,
		    fastf_t local2base, int arb_type,
		    const struct rt_arb_internal *initial,
		    const struct rt_arb_internal *expected,
		    int axis, fastf_t coordinate)
{
    fastf_t parameters[4] = {0, 0, 0, 0};
    parameters[axis + 1] = coordinate / local2base;
    return run_case(name, unit, local2base, arb_type, initial, expected,
	ARB_MOVE_FACE, parameters, 4, false);
}

static int
run_unit(fastf_t local2base, const char *unit)
{
    const fastf_t side = inch_to_mm;
    const fastf_t half_side = side / 2;
    const fastf_t rotation = 15.0;
    const fastf_t slope = tan(rotation * DEG2RAD);
    struct rt_arb_internal cube, pyramid, tetrahedron, arb6, arb7, expected;
    int failures = 0;

    init_cube(&cube, side);
    const int arb8_vertices[] = {0, 1, 2, 3, 4, 5, 6, 7};
    const int arb7_vertices[] = {0, 1, 2, 3, 4, 5, 6};
    const int arb6_vertices[] = {0, 1, 2, 3, 4, 6};
    const int arb5_vertices[] = {0, 1, 2, 3, 4};
    const int arb4_vertices[] = {0, 1, 2, 4};
    failures += run_param_case("arb8", unit, local2base, &cube,
	arb8_vertices, sizeof(arb8_vertices) / sizeof(arb8_vertices[0]));
    const int face_vertices[][4] = {
	{0, 1, 2, 3}, {4, 5, 6, 7}, {0, 4, 7, 3},
	{1, 2, 6, 5}, {0, 1, 5, 4}, {2, 3, 7, 6}
    };
    const int face_axes[] = {Z, Z, X, X, Y, Y};
    const char *face_names[] = {
	"move bottom face", "move top face", "move left face",
	"move right face", "move front face", "move back face"
    };
    for (int face = 0; face < ARB_FACE_COUNT; ++face) {
	expected = cube;
	for (int i = 0; i < 4; ++i)
	    expected.pt[face_vertices[face][i]][face_axes[face]] = half_side;
	fastf_t parameters[4] = {(fastf_t)face, 0, 0, 0};
	parameters[face_axes[face] + 1] = half_side / local2base;
	failures += run_case(face_names[face], unit, local2base, ARB8,
	    &cube, &expected, ARB_MOVE_FACE, parameters, 4, false);
    }

    const int horizontal_edges[][2] = {
	{0, 1}, {1, 2}, {2, 3}, {0, 3},
	{4, 5}, {5, 6}, {6, 7}, {4, 7}
    };
    const int edge_ids[] = {0, 1, 2, 3, 6, 7, 8, 9};
    const char *edge_names[] = {
	"move bottom edge 12", "move bottom edge 23",
	"move bottom edge 34", "move bottom edge 14",
	"move top edge 56", "move top edge 67",
	"move top edge 78", "move top edge 58"
    };
    for (size_t edge = 0;
	edge < sizeof(horizontal_edges) / sizeof(horizontal_edges[0]); ++edge) {
	expected = cube;
	for (int i = 0; i < 2; ++i)
	    expected.pt[horizontal_edges[edge][i]][Z] = half_side;
	const fastf_t parameters[] = {
	    (fastf_t)edge_ids[edge],
	    cube.pt[horizontal_edges[edge][0]][X] / local2base,
	    cube.pt[horizontal_edges[edge][0]][Y] / local2base,
	    half_side / local2base
	};
	failures += run_case(edge_names[edge], unit, local2base, ARB8,
	    &cube, &expected, ARB_MOVE_EDGE, parameters, 4, false);
    }
    const int vertical_edges[][2] = {
	{0, 4}, {1, 5}, {2, 6}, {3, 7}
    };
    const int vertical_edge_ids[] = {4, 5, 10, 11};
    const char *vertical_names[] = {
	"move vertical edge 15", "move vertical edge 26",
	"move vertical edge 37", "move vertical edge 48"
    };
    for (size_t edge = 0;
	edge < sizeof(vertical_edges) / sizeof(vertical_edges[0]); ++edge) {
	expected = cube;
	for (int i = 0; i < 2; ++i)
	    expected.pt[vertical_edges[edge][i]][X] = half_side;
	const fastf_t parameters[] = {
	    (fastf_t)vertical_edge_ids[edge], half_side / local2base,
	    cube.pt[vertical_edges[edge][0]][Y] / local2base, 0
	};
	failures += run_case(vertical_names[edge], unit, local2base, ARB8,
	    &cube, &expected, ARB_MOVE_EDGE, parameters, 4, false);
    }

    const fastf_t tilt = side * slope;
    const struct {
	const char *name;
	int face, fixed_vertex;
	int changed_vertices[2];
	int changed_axis, rotation_axis;
	fastf_t initial_coordinate, tilt_sign;
    } rotations[] = {
	{"rotate bottom face", 0, 0, {2, 3}, Z, X, 0, 1},
	{"rotate top face", 1, 4, {6, 7}, Z, X, side, 1},
	{"rotate left face", 2, 0, {3, 7}, X, Z, 0, -1},
	{"rotate right face", 3, 1, {2, 6}, X, Z, side, -1},
	{"rotate front face", 4, 0, {4, 5}, Y, X, 0, -1},
	{"rotate back face", 5, 2, {6, 7}, Y, X, side, -1}
    };
    for (const auto &test : rotations) {
	expected = cube;
	for (int vertex : test.changed_vertices)
	    expected.pt[vertex][test.changed_axis] =
		test.initial_coordinate + test.tilt_sign * tilt;
	fastf_t parameters[5] = {
	    (fastf_t)test.face, (fastf_t)test.fixed_vertex, 0, 0, 0
	};
	parameters[2 + test.rotation_axis] = rotation;
	failures += run_case(test.name, unit, local2base, ARB8,
	    &cube, &expected, ARB_ROTATE_FACE, parameters, 5, false);
    }

    init_pyramid(&pyramid, side);
    failures += run_param_case("arb5", unit, local2base, &pyramid,
	arb5_vertices, sizeof(arb5_vertices) / sizeof(arb5_vertices[0]));
    expected = pyramid;
    const fastf_t base_expansion = 1.0 + half_side / side;
    for (int i = 0; i < ARB_FIRST_FACE_POINT_COUNT; ++i) {
	expected.pt[i][X] = half_side +
	    (pyramid.pt[i][X] - half_side) * base_expansion;
	expected.pt[i][Y] = half_side +
	    (pyramid.pt[i][Y] - half_side) * base_expansion;
	expected.pt[i][Z] = -half_side;
    }
    failures += run_first_face_move("move ARB5 base face", unit,
	local2base, ARB5, &pyramid, &expected, Z, -half_side);
    expected = pyramid;
    const fastf_t pyramid_ray_scale =
	(1.0 - slope / 2) / (1.0 + slope / 2);
    for (int i = 2; i < ARB_FIRST_FACE_POINT_COUNT; ++i) {
	expected.pt[i][X] = half_side +
	    pyramid_ray_scale * (pyramid.pt[i][X] - half_side);
	expected.pt[i][Y] = half_side +
	    pyramid_ray_scale * (pyramid.pt[i][Y] - half_side);
	expected.pt[i][Z] = side * (1.0 - pyramid_ray_scale);
    }
    const fastf_t first_face_rotation[] = {0, 0, rotation, 0, 0};
    failures += run_case("rotate ARB5 base face", unit, local2base,
	ARB5, &pyramid, &expected, ARB_ROTATE_FACE,
	first_face_rotation, 5, false);
    const int pyramid_base_edges[ARB5_BASE_EDGE_COUNT][ARB_EDGE_POINT_COUNT] = {
	{0, 1}, {1, 2}, {2, 3}, {0, 3}
    };
    const char *pyramid_base_edge_names[ARB5_BASE_EDGE_COUNT] = {
	"move ARB5 base edge 12", "move ARB5 base edge 23",
	"move ARB5 base edge 34", "move ARB5 base edge 14"
    };
    for (int edge = 0; edge < ARB5_BASE_EDGE_COUNT; ++edge) {
	expected = pyramid;
	for (int vertex : pyramid_base_edges[edge]) {
	    expected.pt[vertex][X] = half_side +
		(pyramid.pt[vertex][X] - half_side) / 2;
	    expected.pt[vertex][Y] = half_side +
		(pyramid.pt[vertex][Y] - half_side) / 2;
	    expected.pt[vertex][Z] = half_side;
	}
	const int first_vertex = pyramid_base_edges[edge][0];
	const fastf_t parameters[] = {
	    (fastf_t)edge,
	    expected.pt[first_vertex][X] / local2base,
	    expected.pt[first_vertex][Y] / local2base,
	    half_side / local2base
	};
	failures += run_case(pyramid_base_edge_names[edge], unit, local2base,
	    ARB5, &pyramid, &expected, ARB_MOVE_EDGE,
	    parameters, 4, false);
    }
    const char *pyramid_apex_edge_names[ARB5_BASE_EDGE_COUNT] = {
	"move ARB5 apex edge 15", "move ARB5 apex edge 25",
	"move ARB5 apex edge 35", "move ARB5 apex edge 45"
    };
    const fastf_t base_extension = side / 10;
    /* The base fixes one endpoint; the opposing side plane fixes the apex. */
    for (int edge = 0; edge < ARB5_BASE_EDGE_COUNT; ++edge) {
	expected = pyramid;
	const int corner = edge;
	for (int axis = X; axis <= Y; ++axis) {
	    const fastf_t direction =
		(pyramid.pt[corner][axis] - half_side) / half_side;
	    expected.pt[corner][axis] += direction * base_extension;
	    for (int apex_index = 4; apex_index < ARB_POINT_COUNT; ++apex_index)
		expected.pt[apex_index][axis] +=
		    direction * base_extension / 2;
	}
	for (int apex_index = 4; apex_index < ARB_POINT_COUNT; ++apex_index)
	    expected.pt[apex_index][Z] += base_extension;
	const fastf_t parameters[] = {
	    (fastf_t)(ARB5_BASE_EDGE_COUNT + edge),
	    expected.pt[corner][X] / local2base,
	    expected.pt[corner][Y] / local2base, 0
	};
	failures += run_case(pyramid_apex_edge_names[edge], unit,
	    local2base, ARB5, &pyramid, &expected, ARB_MOVE_EDGE,
	    parameters, 4, false);
    }
    expected = pyramid;
    const point_t apex = {0.6 * side, 0.4 * side, 1.25 * side};
    for (int i = 4; i < ARB_POINT_COUNT; i++)
	VMOVE(expected.pt[i], apex);
    const fastf_t vertex_parameters[] = {
	4, apex[X] / local2base, apex[Y] / local2base, apex[Z] / local2base
    };
    failures += run_case("move ARB5 apex by vertex index", unit, local2base,
	ARB5, &pyramid, &expected, ARB_MOVE_VERTEX, vertex_parameters, 4, false);

    init_tetrahedron(&tetrahedron, side);
    failures += run_param_case("arb4", unit, local2base, &tetrahedron,
	arb4_vertices, sizeof(arb4_vertices) / sizeof(arb4_vertices[0]));
    expected = tetrahedron;
    for (int i = 0; i < ARB_FIRST_FACE_POINT_COUNT; ++i)
	expected.pt[i][Z] = -half_side;
    expected.pt[1][X] = side * base_expansion;
    expected.pt[2][Y] = side * base_expansion;
    failures += run_first_face_move("move ARB4 base face", unit,
	local2base, ARB4, &tetrahedron, &expected, Z, -half_side);
    expected = tetrahedron;
    expected.pt[2][Y] = side / (1.0 + slope);
    expected.pt[2][Z] = side * slope / (1.0 + slope);
    failures += run_case("rotate ARB4 base face", unit, local2base,
	ARB4, &tetrahedron, &expected, ARB_ROTATE_FACE,
	first_face_rotation, 5, false);
    expected = tetrahedron;
    for (int i = 4; i < ARB_POINT_COUNT; i++)
	expected.pt[i][Z] = 1.25 * side;
    const fastf_t tetrahedron_parameters[] = {4, 0, 0, 1.25 * side / local2base};
    failures += run_case("move ARB4 apex by vertex index", unit, local2base,
	ARB4, &tetrahedron, &expected, ARB_MOVE_VERTEX,
	tetrahedron_parameters, 4, false);

    init_arb6(&arb6, side);
    failures += run_param_case("arb6", unit, local2base, &arb6,
	arb6_vertices, sizeof(arb6_vertices) / sizeof(arb6_vertices[0]));
    expected = arb6;
    for (int i = 0; i < ARB_FIRST_FACE_POINT_COUNT; ++i)
	expected.pt[i][X] = -half_side;
    expected.pt[0][Y] = expected.pt[3][Y] = -half_side / 2;
    expected.pt[1][Y] = expected.pt[2][Y] = side + half_side / 2;
    expected.pt[3][Z] = side / 2 - half_side / 2;
    failures += run_first_face_move("move ARB6 first face", unit,
	local2base, ARB6, &arb6, &expected, X, -half_side);
    expected = arb6;
    const fastf_t rotated_y = side / (1.0 - slope / 2);
    expected.pt[1][X] = expected.pt[2][X] = -slope * rotated_y;
    expected.pt[1][Y] = expected.pt[2][Y] = rotated_y;
    const fastf_t side_face_rotation[] = {0, 0, 0, 0, rotation};
    failures += run_case("rotate ARB6 first face", unit, local2base,
	ARB6, &arb6, &expected, ARB_ROTATE_FACE,
	side_face_rotation, 5, false);
    expected = arb6;
    const fastf_t edge_lift = side / 4;
    expected.pt[0][Z] = expected.pt[1][Z] = edge_lift;
    const fastf_t first_edge_lift[] = {
	0, 0, 0, edge_lift / local2base
    };
    failures += run_case("lift ARB6 edge 12", unit, local2base,
	ARB6, &arb6, &expected, ARB_MOVE_EDGE,
	first_edge_lift, 4, false);
    expected = arb6;
    const fastf_t edge_retraction = side / 10;
    expected.pt[1][Y] = expected.pt[2][Y] = side - edge_retraction;
    expected.pt[2][Z] = side - edge_retraction / 2;
    const fastf_t second_edge_move[] = {
	1, 0, (side - edge_retraction) / local2base, 0
    };
    failures += run_case("retract ARB6 edge 23", unit, local2base,
	ARB6, &arb6, &expected, ARB_MOVE_EDGE,
	second_edge_move, 4, false);
    expected = arb6;
    const fastf_t point_extension = side / 10;
    const fastf_t extended_x = side + point_extension;
    expected.pt[4][X] = expected.pt[5][X] = extended_x;
    /* The far top edge remains on its original sloped top plane. */
    expected.pt[6][X] = expected.pt[7][X] = extended_x;
    expected.pt[6][Z] = expected.pt[7][Z] = side + point_extension / 4;
    const fastf_t arb6_parameters[] = {
	4, extended_x / local2base, half_side / local2base, 0
    };
    failures += run_case("move ARB6 point 5 by vertex index", unit, local2base,
	ARB6, &arb6, &expected, ARB_MOVE_VERTEX, arb6_parameters, 4, false);

    expected = arb6;
    for (int i = 4; i < ARB_POINT_COUNT; i++)
	expected.pt[i][X] = extended_x;
    const fastf_t arb6_second_point[] = {
	6, extended_x / local2base, half_side / local2base, side / local2base
    };
    failures += run_case("move ARB6 point 6 by vertex index", unit, local2base,
	ARB6, &arb6, &expected, ARB_MOVE_VERTEX, arb6_second_point, 4, false);

    init_arb7(&arb7, side);
    failures += run_param_case("arb7", unit, local2base, &arb7,
	arb7_vertices, sizeof(arb7_vertices) / sizeof(arb7_vertices[0]));
    expected = arb7;
    for (int i = 0; i < ARB_FIRST_FACE_POINT_COUNT; ++i)
	expected.pt[i][X] = -half_side;
    expected.pt[2][Z] = side + half_side / 2;
    expected.pt[3][Z] = side / 2 + half_side / 2;
    failures += run_first_face_move("move ARB7 first face", unit,
	local2base, ARB7, &arb7, &expected, X, -half_side);
    expected = arb7;
    expected.pt[1][X] = expected.pt[2][X] = -slope * side;
    expected.pt[2][Z] = side + slope * side / 2;
    failures += run_case("rotate ARB7 first face", unit, local2base,
	ARB7, &arb7, &expected, ARB_ROTATE_FACE,
	side_face_rotation, 5, false);
    expected = arb7;
    expected.pt[0][Z] = expected.pt[1][Z] = edge_lift;
    failures += run_case("lift ARB7 edge 12", unit, local2base,
	ARB7, &arb7, &expected, ARB_MOVE_EDGE,
	first_edge_lift, 4, false);
    expected = arb7;
    expected.pt[1][Y] = expected.pt[2][Y] = side - edge_retraction;
    expected.pt[2][Z] = side - edge_retraction / 2;
    failures += run_case("retract ARB7 edge 23", unit, local2base,
	ARB7, &arb7, &expected, ARB_MOVE_EDGE,
	second_edge_move, 4, false);
    expected = arb7;
    expected.pt[4][X] = expected.pt[7][X] = extended_x;
    /* The opposite top corner lies on the plane through points 2, 3, 4. */
    expected.pt[6][Z] = side - half_side * side / extended_x;
    const fastf_t arb7_parameters[] = {4, extended_x / local2base, 0, 0};
    failures += run_case("move ARB7 point 5 by vertex index", unit, local2base,
	ARB7, &arb7, &expected, ARB_MOVE_VERTEX, arb7_parameters, 4, false);

    const fastf_t invalid_pyramid_vertex[] = {
	1, apex[X] / local2base, apex[Y] / local2base, apex[Z] / local2base
    };
    failures += run_case("reject unsupported ARB5 vertex", unit, local2base,
	ARB5, &pyramid, &pyramid, ARB_MOVE_VERTEX,
	invalid_pyramid_vertex, 4, true);

    const fastf_t fractional_vertex[] = {
	4.5, apex[X] / local2base, apex[Y] / local2base, apex[Z] / local2base
    };
    failures += run_case("reject fractional vertex index", unit, local2base,
	ARB5, &pyramid, &pyramid, ARB_MOVE_VERTEX,
	fractional_vertex, 4, true);

    const fastf_t unsupported_vertex[] = {
	6, side / local2base, side / local2base, 1.25 * side / local2base
    };
    failures += run_case("reject ARB8 vertex edit", unit, local2base, ARB8,
	&cube, &cube, ARB_MOVE_VERTEX, unsupported_vertex, 4, true);

    const fastf_t fractional_face[] = {0.5, 0, rotation, 0, 0};
    failures += run_case("reject fractional face index", unit, local2base,
	ARB8, &cube, &cube, ARB_ROTATE_FACE, fractional_face, 5, true);

    return failures;
}

int
rt_edit_test_arb_operation_matrix(void)
{
    const struct rt_edit_prim_desc *desc = EDOBJ[ID_ARB8].ft_edit_desc ?
	EDOBJ[ID_ARB8].ft_edit_desc() : NULL;
    const struct rt_edit_cmd_desc *vertex = find_command(ARB_MOVE_VERTEX);
    if (!desc || desc->ncmd != ARB_DESCRIPTOR_COMMAND_COUNT ||
	!find_command(ARB_MOVE_FACE) || !find_command(ARB_MOVE_EDGE) ||
	!find_command(ARB_ROTATE_FACE) || !vertex || !vertex->req_types ||
	strstr(vertex->req_types, "arb8") || !strstr(vertex->req_types, "arb5")) {
	bu_log("ARB vertex descriptor lists unsupported primitive types\n");
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
