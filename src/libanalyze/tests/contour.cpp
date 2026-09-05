/*               A N A L Y Z E _ C O N T O U R . C P P
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES ARE DISCLAIMED.
 */
/** @file contour.cpp
 *
 * Basic topology and resource-limit tests for analyze_mdc().
 */

#include "common.h"

#include <algorithm>
#include <climits>
#include <cmath>
#include <cstdio>
#include <set>
#include <utility>

#include "bu/app.h"
#include "bu/malloc.h"
#include "bg/trimesh.h"
#include "analyze/contour.h"
#include "analyze/polygonize.h"
#include "raytrace.h"
#include "wdb.h"

static constexpr double METRIC_RELATIVE_TOLERANCE = 0.10;

static int failures = 0;


static void
expect(bool condition, const char *message)
{
    if (condition)
	return;

    std::fprintf(stderr, "FAIL: %s\n", message);
    failures++;
}


static void
expect_metric(double actual, double expected, const char *object,
	const char *metric)
{
    double relative_error = std::fabs(actual - expected) / expected;
    if (actual >= 0.0 && relative_error <= METRIC_RELATIVE_TOLERANCE)
	return;

    std::fprintf(stderr,
	    "FAIL: %s %s is %.17g, expected %.17g (relative error %.2f%%)\n",
	    object, metric, actual, expected, 100.0 * relative_error);
    failures++;
}


static void
check_mesh(struct db_i *dbip, const char *object, int expected_components = 1,
	double expected_area = 0.0, double expected_volume = 0.0,
	size_t minimum_faces = 4, int expected_euler = INT_MAX)
{
    int *faces = NULL;
    point_t *vertices = NULL;
    size_t face_count = 0;
    size_t vertex_count = 0;
    struct analyze_mdc_params params = ANALYZE_MDC_PARAMS_DEFAULT;
    params.min_depth = 4;
    params.max_depth = 8;
    params.max_rays = 0;
    params.minimum_free_mem = 0;
    params.max_time = 0;
    params.verbosity = 0;

    enum analyze_mdc_status status = analyze_mdc(&faces, &face_count,
	    &vertices, &vertex_count, object, dbip, &params);
    expect(status == ANALYZE_MDC_OK, object);
    if (status == ANALYZE_MDC_OK) {
	expect(vertex_count >= 4, "contour has vertices");
	expect(face_count >= minimum_faces, "contour has expected resolution");
	expect(bg_trimesh_solid2(static_cast<int>(vertex_count),
		static_cast<int>(face_count),
		reinterpret_cast<fastf_t *>(vertices), faces, NULL) == 0,
		"contour is a closed oriented manifold");
	if (expected_euler != INT_MAX) {
	    std::set<std::pair<int, int>> edges;
	    for (size_t face = 0; face < face_count; face++) {
		for (size_t corner = 0; corner < 3; corner++) {
		    int a = faces[3 * face + corner];
		    int b = faces[3 * face + (corner + 1) % 3];
		    edges.insert(std::minmax(a, b));
		}
	    }
	    long long euler = static_cast<long long>(vertex_count) -
		static_cast<long long>(edges.size()) +
		static_cast<long long>(face_count);
	    expect(euler == expected_euler,
		    "contour has the expected Euler characteristic");
	}
	if (expected_area > 0.0)
	    expect_metric(bg_trimesh_area(faces, face_count, vertices,
		    vertex_count), expected_area, object, "surface area");
	if (expected_volume > 0.0)
	    expect_metric(bg_trimesh_volume(faces, face_count, vertices,
		    vertex_count), expected_volume, object, "volume");

	int *face_indices = NULL;
	int *component_offsets = NULL;
	int components = bg_trimesh_separate(&face_indices,
		&component_offsets, faces, static_cast<int>(face_count));
	expect(components == expected_components,
		"contour has the expected connected components");
	if (face_indices)
	    bu_free(face_indices, "MDC test component faces");
	if (component_offsets)
	    bu_free(component_offsets, "MDC test component offsets");
    }

    if (faces)
	bu_free(faces, "MDC test faces");
    if (vertices)
	bu_free(vertices, "MDC test vertices");
}


static bool
make_region(struct rt_wdb *wdbp, const char *region, const char *solid)
{
    struct wmember member;
    BU_LIST_INIT(&member.l);
    if (!mk_addmember(solid, &member.l, NULL, WMOP_UNION))
	return false;
    return mk_lcomb(wdbp, region, &member, 1, NULL, NULL, NULL, 0) == 0;
}


int
main(int UNUSED(argc), const char **argv)
{
    bu_setprogname(argv[0]);

    struct db_i *dbip = db_create_inmem();
    expect(dbip != NULL, "create in-memory database");
    if (!dbip)
	return 1;

    struct rt_wdb *wdbp = wdb_dbopen(dbip, RT_WDB_TYPE_DB_INMEM);
    expect(wdbp != NULL, "open in-memory database");
    if (!wdbp) {
	db_close(dbip);
	return 1;
    }

    constexpr double sphere_radius = 10.0;
    constexpr double torus_major_radius = 10.0;
    constexpr double torus_minor_radius = 2.5;
    constexpr double slender_torus_major_radius = 100.0;
    constexpr double slender_torus_minor_radius = 2.0;
    constexpr double lower_radius = 1.5;
    constexpr double upper_radius = 1.0;

    point_t center = VINIT_ZERO;
    point_t minimum = {-8.0, -6.0, -4.0};
    point_t maximum = {8.0, 6.0, 4.0};
    expect(mk_sph(wdbp, "sphere.s", center, sphere_radius) == 0,
	    "create test sphere");
    expect(mk_rpp(wdbp, "box.s", minimum, maximum) == 0,
	    "create test box");
    point_t thin_minimum = {-5.0, -2.5, -0.05};
    point_t thin_maximum = {5.0, 2.5, 0.05};
    expect(mk_rpp(wdbp, "thin.s", thin_minimum, thin_maximum) == 0,
	    "create thin-wall test box");

    point_t tunnel_minimum = {-10.0, -10.0, -10.0};
    point_t tunnel_maximum = {10.0, 10.0, 10.0};
    point_t tunnel_base = {0.0, 0.0, -11.0};
    vect_t tunnel_height = {0.0, 0.0, 22.0};
    expect(mk_rpp(wdbp, "tunnel_outer.s", tunnel_minimum,
	    tunnel_maximum) == 0, "create tunnel test outer box");
    expect(mk_rcc(wdbp, "tunnel_void.s", tunnel_base, tunnel_height,
	    1.0) == 0, "create tunnel test void");
    struct wmember tunnel_members;
    BU_LIST_INIT(&tunnel_members.l);
    expect(mk_addmember("tunnel_outer.s", &tunnel_members.l, NULL,
	    WMOP_UNION) != NULL, "add tunnel outer box");
    expect(mk_addmember("tunnel_void.s", &tunnel_members.l, NULL,
	    WMOP_SUBTRACT) != NULL, "subtract tunnel void");
    expect(mk_lcomb(wdbp, "tunnel.r", &tunnel_members, 1, NULL, NULL,
	    NULL, 0) == 0, "create tunnel test region");
    vect_t normal = {0.0, 0.0, 1.0};
    expect(mk_tor(wdbp, "torus.s", center, normal, torus_major_radius,
	    torus_minor_radius) == 0,
	    "create test torus");
    expect(mk_tor(wdbp, "slender_torus.s", center, normal,
	    slender_torus_major_radius, slender_torus_minor_radius) == 0,
	    "create high-aspect torus");

    point_t tgc_base = {-4.0, -3.0, -6.0};
    vect_t tgc_height = {2.0, 1.0, 12.0};
    vect_t tgc_a = {5.0, 0.0, 0.0};
    vect_t tgc_b = {0.0, 3.0, 0.0};
    vect_t tgc_c = {2.5, 0.0, 0.0};
    vect_t tgc_d = {0.0, 1.5, 0.0};
    expect(mk_tgc(wdbp, "tgc.s", tgc_base, tgc_height, tgc_a, tgc_b,
	    tgc_c, tgc_d) == 0, "create test TGC");

    point_t concave_arb[8] = {
	{500.0, -500.0, -500.0},
	{500.0, 500.0, -500.0},
	{500.0, -169.922, -304.688},
	{500.0, -500.0, 500.0},
	{400.0, -500.0, -500.0},
	{400.0, 500.0, -500.0},
	{400.0, -169.922, -304.688},
	{400.0, -500.0, 500.0}
    };
    expect(mk_arb8(wdbp, "concave.s",
	    reinterpret_cast<const fastf_t *>(concave_arb)) == 0,
	    "create sharp concave ARB");

    point_t lower_center = {-8.0, -5.0, -3.0};
    point_t upper_center = {9.0, 4.0, 6.0};
    expect(mk_sph(wdbp, "lower.s", lower_center, lower_radius) == 0,
	    "create lower disconnected sphere");
    expect(mk_sph(wdbp, "upper.s", upper_center, upper_radius) == 0,
	    "create upper disconnected sphere");
    struct wmember members;
    BU_LIST_INIT(&members.l);
    expect(mk_addmember("lower.s", &members.l, NULL, WMOP_UNION) != NULL,
	    "add lower disconnected sphere");
    expect(mk_addmember("upper.s", &members.l, NULL, WMOP_UNION) != NULL,
	    "add upper disconnected sphere");
    expect(mk_lcomb(wdbp, "disconnected.r", &members, 1, NULL, NULL, NULL,
	    0) == 0, "create disconnected region");

    constexpr double overlap_radius = 5.0;
    constexpr double overlap_offset = 3.0;
    point_t overlap_left_center = {-overlap_offset, 0.0, 0.0};
    point_t overlap_right_center = {overlap_offset, 0.0, 0.0};
    expect(mk_sph(wdbp, "overlap_left.s", overlap_left_center,
	    overlap_radius) == 0, "create left overlapping sphere");
    expect(mk_sph(wdbp, "overlap_right.s", overlap_right_center,
	    overlap_radius) == 0, "create right overlapping sphere");
    expect(make_region(wdbp, "overlap_left.r", "overlap_left.s"),
	    "create left overlapping region");
    expect(make_region(wdbp, "overlap_right.r", "overlap_right.s"),
	    "create right overlapping region");
    struct wmember overlap_members;
    BU_LIST_INIT(&overlap_members.l);
    expect(mk_addmember("overlap_left.r", &overlap_members.l, NULL,
	    WMOP_UNION) != NULL, "add left overlapping region");
    expect(mk_addmember("overlap_right.r", &overlap_members.l, NULL,
	    WMOP_UNION) != NULL, "add right overlapping region");
    expect(mk_lcomb(wdbp, "overlap.c", &overlap_members, 0, NULL, NULL,
	    NULL, 0) == 0, "create overlapping-region combination");

    vect_t box_size;
    VSUB2(box_size, maximum, minimum);

    const double sphere_area = 4.0 * M_PI * sphere_radius * sphere_radius;
    const double sphere_volume = 4.0 * M_PI * sphere_radius * sphere_radius *
	sphere_radius / 3.0;
    const double box_area = 2.0 * (box_size[X] * box_size[Y] +
	box_size[X] * box_size[Z] + box_size[Y] * box_size[Z]);
    const double box_volume = box_size[X] * box_size[Y] * box_size[Z];
    vect_t thin_size;
    VSUB2(thin_size, thin_maximum, thin_minimum);
    const double thin_area = 2.0 * (thin_size[X] * thin_size[Y] +
	thin_size[X] * thin_size[Z] + thin_size[Y] * thin_size[Z]);
    const double thin_volume =
	thin_size[X] * thin_size[Y] * thin_size[Z];
    const double torus_area = 4.0 * M_PI * M_PI * torus_major_radius *
	torus_minor_radius;
    const double torus_volume = 2.0 * M_PI * M_PI * torus_major_radius *
	torus_minor_radius * torus_minor_radius;
    const double slender_torus_area = 4.0 * M_PI * M_PI *
	slender_torus_major_radius * slender_torus_minor_radius;
    const double disconnected_area = 4.0 * M_PI *
	(lower_radius * lower_radius + upper_radius * upper_radius);
    const double disconnected_volume = 4.0 * M_PI *
	(lower_radius * lower_radius * lower_radius +
	 upper_radius * upper_radius * upper_radius) / 3.0;
    const double overlap_distance = 2.0 * overlap_offset;
    const double overlap_cap_height = overlap_radius -
	0.5 * overlap_distance;
    const double overlap_intersection_volume = M_PI *
	(4.0 * overlap_radius + overlap_distance) *
	std::pow(2.0 * overlap_radius - overlap_distance, 2.0) / 12.0;
    const double overlap_area = 8.0 * M_PI * overlap_radius *
	overlap_radius - 4.0 * M_PI * overlap_radius * overlap_cap_height;
    const double overlap_volume = 8.0 * M_PI * overlap_radius *
	overlap_radius * overlap_radius / 3.0 -
	overlap_intersection_volume;

    check_mesh(dbip, "sphere.s", 1, sphere_area, sphere_volume);
    check_mesh(dbip, "box.s", 1, box_area, box_volume);
    check_mesh(dbip, "thin.s", 1, thin_area, thin_volume, 10000);
    check_mesh(dbip, "torus.s", 1, torus_area, torus_volume, 4, 0);
    check_mesh(dbip, "slender_torus.s", 1, slender_torus_area,
	0.0, 4, 0);
    check_mesh(dbip, "tunnel.r", 1, 0.0, 0.0, 4, 0);
    check_mesh(dbip, "tgc.s");
    check_mesh(dbip, "concave.s");
    check_mesh(dbip, "disconnected.r", 2, disconnected_area,
	disconnected_volume);
    check_mesh(dbip, "overlap.c", 1, overlap_area, overlap_volume);

    {
	int *coarse_faces = NULL;
	point_t *coarse_vertices = NULL;
	size_t coarse_face_count = 0;
	size_t coarse_vertex_count = 0;
	struct analyze_mdc_params coarse = ANALYZE_MDC_PARAMS_DEFAULT;
	coarse.min_depth = 3;
	coarse.max_depth = 3;
	coarse.max_rays = 50000;
	coarse.minimum_free_mem = 0;
	coarse.max_time = 0;
	enum analyze_mdc_status coarse_status = analyze_mdc(&coarse_faces,
		&coarse_face_count, &coarse_vertices, &coarse_vertex_count,
		"tunnel.r", dbip, &coarse);
	expect(coarse_status == ANALYZE_MDC_AMBIGUOUS,
		"under-resolved tunnel requests refinement");
	expect(!coarse_faces && !coarse_vertices && !coarse_face_count &&
		coarse_vertex_count == 0,
		"under-resolved tunnel leaves outputs empty");
	if (coarse_faces)
	    bu_free(coarse_faces, "coarse MDC test faces");
	if (coarse_vertices)
	    bu_free(coarse_vertices, "coarse MDC test vertices");
    }

    {
	int *compatibility_faces = NULL;
	point_t *compatibility_vertices = NULL;
	int compatibility_face_count = 0;
	int compatibility_vertex_count = 0;
	point_t unused_seed = VINIT_ZERO;
	struct analyze_polygonize_params compatibility =
	    ANALYZE_POLYGONIZE_PARAMS_DEFAULT;
	compatibility.max_time = 0;
	compatibility.max_cycle_time = 0;
	compatibility.minimum_free_mem = 0;

	int status = analyze_polygonize(&compatibility_faces,
		&compatibility_face_count, &compatibility_vertices,
		&compatibility_vertex_count, 1.0, unused_seed, "sphere.s",
		dbip, &compatibility);
	expect(status == 0, "polygonize compatibility interface");
	if (status == 0) {
	    expect(compatibility_face_count >= 4,
		    "compatibility contour has faces");
	    expect(compatibility_vertex_count >= 4,
		    "compatibility contour has vertices");
	    expect(bg_trimesh_solid2(compatibility_vertex_count,
		    compatibility_face_count,
		    reinterpret_cast<fastf_t *>(compatibility_vertices),
		    compatibility_faces, NULL) == 0,
		    "compatibility contour is a closed oriented manifold");
	}
	if (compatibility_faces)
	    bu_free(compatibility_faces, "compatibility test faces");
	if (compatibility_vertices)
	    bu_free(compatibility_vertices, "compatibility test vertices");

	compatibility.max_time = 1;
	compatibility.time_offset = 1;
	expect(analyze_polygonize(&compatibility_faces,
		&compatibility_face_count, &compatibility_vertices,
		&compatibility_vertex_count, 1.0, unused_seed, "sphere.s",
		dbip, &compatibility) == 2,
		"compatibility interface preserves timeout status");
	expect(!compatibility_faces && !compatibility_vertices &&
		!compatibility_face_count && !compatibility_vertex_count,
		"compatibility timeout leaves outputs empty");
    }

    int *faces = NULL;
    point_t *vertices = NULL;
    size_t face_count = 0;
    size_t vertex_count = 0;
    struct analyze_mdc_params limited = ANALYZE_MDC_PARAMS_DEFAULT;
    limited.min_depth = 4;
    limited.max_depth = 4;
    limited.max_rays = 1;
    limited.minimum_free_mem = 0;
    expect(analyze_mdc(&faces, &face_count, &vertices, &vertex_count,
	    "sphere.s", dbip, &limited) == ANALYZE_MDC_RAY_LIMIT,
	    "ray limit is enforced during sampling");
    expect(!faces && !vertices && !face_count && !vertex_count,
	    "failure leaves outputs empty");

    expect(analyze_mdc(NULL, &face_count, &vertices, &vertex_count,
	    "sphere.s", dbip, NULL) == ANALYZE_MDC_INVALID_INPUT,
	    "invalid output pointer is rejected");

    db_close(dbip);
    return failures ? 1 : 0;
}

/*
 * Local Variables:
 * mode: C++
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
