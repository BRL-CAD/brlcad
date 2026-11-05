/*                I M P L I C I T _ B R E P . C P P
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
/** @file implicit_brep.cpp
 *
 * Topological and semantic tests for faceted B-Reps generated from implicit
 * primitives.
 */

#include "common.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "brep.h"
#include "bu/app.h"
#include "raytrace.h"
#include "rt/geom.h"
#include "wdb.h"


namespace {

constexpr size_t SEMANTIC_RAY_COUNT = 8192;
constexpr double PARTITION_ERROR_LIMIT = 0.01;
constexpr double CHORD_ERROR_LIMIT = 0.01;
constexpr double ESTIMATE_ERROR_LIMIT = 0.05;
constexpr double ENDPOINT_MODEL_TOLERANCE_SCALE = 4.0;


static struct rt_i *
prepare_object(struct db_i *dbip, const char *name)
{
    struct rt_i *rtip = rt_i_create(dbip);
    if (!rtip)
	return NULL;
    if (rt_gettree(rtip, name) != 0) {
	rt_i_destroy(rtip);
	return NULL;
    }
    rt_prep_parallel(rtip, 1);
    return rtip;
}


static bool
closed_oriented_manifold(const ON_Brep &brep)
{
    bool oriented = false;
    bool boundary = true;
    const bool valid = brep.IsValid();
    const bool solid = brep.IsSolid();
    const bool manifold = brep.IsManifold(&oriented, &boundary);
    if (!valid || !solid || !manifold || !oriented || boundary ||
	brep.m_F.Count() < 1)
    {
	std::fprintf(stderr, "brep topology: valid %d solid %d manifold %d "
	    "oriented %d boundary %d faces %d edges %d\n", valid, solid,
	    manifold, oriented, boundary, brep.m_F.Count(), brep.m_E.Count());
	int abnormal_edge_count = 0;
	int reported_edge_count = 0;
	for (int edge_index = 0; edge_index < brep.m_E.Count(); ++edge_index) {
	    const ON_BrepEdge &edge = brep.m_E[edge_index];
	    if (edge.m_ti.Count() == 2)
		continue;
	    ++abnormal_edge_count;
	    if (reported_edge_count >= 32 || edge.m_vi[0] < 0 ||
		edge.m_vi[0] >= brep.m_V.Count() || edge.m_vi[1] < 0 ||
		edge.m_vi[1] >= brep.m_V.Count())
		continue;
	    const ON_3dPoint &start = brep.m_V[edge.m_vi[0]].point;
	    const ON_3dPoint &end = brep.m_V[edge.m_vi[1]].point;
	    std::fprintf(stderr, "abnormal edge %d (%d trims): "
		"(%.17g %.17g %.17g) -> (%.17g %.17g %.17g)\n",
		edge_index, edge.m_ti.Count(), start.x, start.y, start.z,
		end.x, end.y, end.z);
	    ++reported_edge_count;
	}
	if (reported_edge_count < abnormal_edge_count)
	    std::fprintf(stderr, "%d additional abnormal edges omitted\n",
		abnormal_edge_count - reported_edge_count);
	return false;
    }
    for (int edge_index = 0; edge_index < brep.m_E.Count(); ++edge_index) {
	if (brep.m_E[edge_index].m_ti.Count() != 2) {
	    std::fprintf(stderr, "brep edge %d has %d trims\n", edge_index,
		brep.m_E[edge_index].m_ti.Count());
	    return false;
	}
    }
    return true;
}


static int
check_semantics(struct db_i *dbip, const char *reference_name,
    const char *candidate_name)
{
    struct rt_i *source = prepare_object(dbip, reference_name);
    struct rt_i *candidate = prepare_object(dbip, candidate_name);
    if (!source || !candidate) {
	if (source)
	    rt_i_destroy(source);
	if (candidate)
	    rt_i_destroy(candidate);
	return 1;
    }

    struct rt_crofton_params parameters = {};
    parameters.n_rays = SEMANTIC_RAY_COUNT;
    struct rt_crofton_comparison comparison = RT_CROFTON_COMPARISON_INIT;
    const int comparison_status = rt_crofton_compare(&comparison, source,
	candidate, &parameters, 0, NULL, NULL);
    rt_i_destroy(candidate);
    rt_i_destroy(source);

    const double endpoint_tolerance =
	ENDPOINT_MODEL_TOLERANCE_SCALE * BN_TOL_DIST;
    const bool semantic_match = comparison_status == 0 &&
	comparison.ray_count == SEMANTIC_RAY_COUNT &&
	comparison.partition_mismatch_fraction <= PARTITION_ERROR_LIMIT &&
	comparison.endpoint_error_p95 <= endpoint_tolerance &&
	comparison.endpoint_error_p99 <= 2.0 * endpoint_tolerance &&
	comparison.chord_error_fraction <= CHORD_ERROR_LIMIT &&
	comparison.surface_area_error_fraction <= ESTIMATE_ERROR_LIMIT &&
	comparison.volume_error_fraction <= ESTIMATE_ERROR_LIMIT;
    std::printf("%s -> %s: status %d rays %zu partition %.6g "
	"endpoint-p95 %.6g endpoint-p99 %.6g chord %.6g "
	"surface %.6g (%.6g/%.6g) volume %.6g (%.6g/%.6g)\n",
	reference_name, candidate_name, comparison_status, comparison.ray_count,
	comparison.partition_mismatch_fraction, comparison.endpoint_error_p95,
	comparison.endpoint_error_p99, comparison.chord_error_fraction,
	comparison.surface_area_error_fraction,
	comparison.source_surface_area, comparison.candidate_surface_area,
	comparison.volume_error_fraction, comparison.source_volume,
	comparison.candidate_volume);
    return semantic_match ? 0 : 1;
}


static int
check_conversion(struct db_i *dbip, struct rt_wdb *writer,
    const char *source_name, const char *semantic_reference_name,
    const char *candidate_name)
{
    struct directory *entry = db_lookup(dbip, source_name, LOOKUP_QUIET);
    if (!entry)
	return 1;

    struct rt_db_internal internal;
    RT_DB_INTERNAL_INIT(&internal);
    if (rt_db_get_internal(&internal, entry, dbip, bn_mat_identity) < 0)
	return 1;

    struct bn_tol tolerance = BN_TOL_INIT_TOL;
    ON_Brep *brep = ON_Brep::New();
    internal.idb_meth->ft_brep(&brep, &internal, &tolerance);
    rt_db_free_internal(&internal);
    if (!brep || !closed_oriented_manifold(*brep) ||
	mk_brep(writer, candidate_name, brep) < 0)
    {
	std::fprintf(stderr, "%s did not produce a closed B-Rep\n", source_name);
	delete brep;
	return 1;
    }
    delete brep;
    db_update_nref(dbip);
    return check_semantics(dbip, semantic_reference_name, candidate_name);
}


static int
check_metaball_case(struct db_i *dbip, struct rt_wdb *writer,
    const char *source_name, const char *reference_name,
    const char *candidate_name, size_t point_count, int method,
    double threshold, const fastf_t **control_points)
{
    if (mk_metaball(writer, source_name, point_count, method, threshold,
	control_points) < 0)
    {
	return 1;
    }
    db_update_nref(dbip);
    return check_conversion(dbip, writer, source_name, reference_name,
	candidate_name);
}


static int
check_metaball(struct db_i *dbip, struct rt_wdb *writer)
{
    fastf_t control_point[5] = {0.0, 0.0, 0.0, 0.5, 1.0};
    const fastf_t *control_points[1] = {control_point};
    if (mk_sph(writer, "metaball.reference.s", control_point, 0.5) < 0)
    {
	return 1;
    }
    db_update_nref(dbip);
    int failures = check_metaball_case(dbip, writer, "metaball.s",
	"metaball.reference.s", "metaball.brep", 1,
	METABALL_ISOPOTENTIAL, 1.0, control_points);

    fastf_t iso_left[5] = {-0.45, 0.0, 0.0, 0.7, 1.0};
    fastf_t iso_right[5] = {0.45, 0.0, 0.0, 0.7, 1.0};
    const fastf_t *iso_points[2] = {iso_left, iso_right};
    failures += check_metaball_case(dbip, writer, "metaball.iso_pair.s",
	"metaball.iso_pair.s", "metaball.iso_pair.brep", 2,
	METABALL_ISOPOTENTIAL, 1.0, iso_points);

    fastf_t blob_left[5] = {-0.55, 0.0, 0.0, 1.0, 1.5};
    fastf_t blob_right[5] = {0.55, 0.0, 0.0, 1.0, 1.5};
    const fastf_t *blob_points[2] = {blob_left, blob_right};
    failures += check_metaball_case(dbip, writer, "metaball.blob_pair.s",
	"metaball.blob_pair.s", "metaball.blob_pair.brep", 2,
	METABALL_BLOB, 1.0, blob_points);
    return failures;
}


static int
check_heart_case(struct db_i *dbip, struct rt_wdb *writer,
    const char *source_name, const char *candidate_name, const point_t center,
    const vect_t x_axis, const vect_t y_axis, const vect_t z_axis, double d)
{
    if (mk_hrt(writer, source_name, center, x_axis, y_axis, z_axis, d) < 0)
	return 1;
    db_update_nref(dbip);
    return check_conversion(dbip, writer, source_name, source_name,
	candidate_name);
}


static int
check_heart(struct db_i *dbip, struct rt_wdb *writer)
{
    point_t center = {3.0, -2.0, 5.0};
    vect_t x_axis = {0.8, 0.6, 0.0};
    vect_t y_axis = {-0.6, 0.8, 0.0};
    vect_t z_axis = {0.0, 0.0, 1.0};
    int failures = check_heart_case(dbip, writer, "heart.s", "heart.brep",
	center, x_axis, y_axis, z_axis, 0.5);

    point_t scaled_center = {-4.0, 1.0, 2.0};
    vect_t scaled_x_axis = {1.36, 1.02, 0.0};
    vect_t scaled_y_axis = {-0.39, 0.52, 0.0};
    vect_t scaled_z_axis = {0.0, 0.0, 1.3};
    failures += check_heart_case(dbip, writer, "heart.scaled.s",
	"heart.scaled.brep", scaled_center, scaled_x_axis, scaled_y_axis,
	scaled_z_axis, 0.75);
    return failures;
}


static int
check_unit_exponent_superell(struct db_i *dbip, struct rt_wdb *writer)
{
    const point_t center = {3.0, -2.0, 5.0};
    const vect_t a = {5.0, 0.0, 0.0};
    const vect_t b = {0.0, 2.5, 0.0};
    const vect_t c = {0.0, 0.0, 1.25};
    struct rt_superell_internal *superell;

    if (mk_ell(writer, "superell.reference.s", center, a, b, c) < 0)
	return 1;

    BU_ALLOC(superell, struct rt_superell_internal);
    superell->magic = RT_SUPERELL_INTERNAL_MAGIC;
    VMOVE(superell->v, center);
    VMOVE(superell->a, a);
    VMOVE(superell->b, b);
    VMOVE(superell->c, c);
    superell->n = 1.0;
    superell->e = 1.0;
    if (wdb_export(writer, "superell.s", (void *)superell,
	    ID_SUPERELL, 1.0) < 0)
	return 1;

    db_update_nref(dbip);
    int failures = check_conversion(dbip, writer, "superell.s",
	"superell.reference.s", "superell.brep");
    failures += check_semantics(dbip, "superell.s", "superell.brep");
    return failures;
}


static int
check_pipe(struct db_i *dbip, struct rt_wdb *writer)
{
    const point_t coordinates[4] = {
	{0.0, 0.0, 0.0}, {0.0, 0.0, 30.0},
	{20.0, 0.0, 30.0}, {20.0, 20.0, 30.0}
    };
    struct bu_list points;
    mk_pipe_init(&points);
    for (size_t i = 0; i < 4; ++i)
	mk_add_pipe_pnt(&points, coordinates[i], 8.0, 4.0, 5.0);
    if (mk_pipe(writer, "pipe.s", &points) < 0) {
	mk_pipe_free(&points);
	return 1;
    }
    mk_pipe_free(&points);
    db_update_nref(dbip);
    return check_conversion(dbip, writer, "pipe.s", "pipe.s", "pipe.brep");
}


static int
check_rhc(struct db_i *dbip, struct rt_wdb *writer)
{
    const point_t vertex = {3.0, -2.0, 5.0};
    const vect_t height = {0.0, 6.0, 8.0};
    const vect_t breadth = {0.0, -4.0, 3.0};

    if (mk_rhc(writer, "rhc.s", vertex, height, breadth, 2.5, 1.2) < 0)
	return 1;
    db_update_nref(dbip);
    return check_conversion(dbip, writer, "rhc.s", "rhc.s", "rhc.brep");
}


static int
check_hyp(struct db_i *dbip, struct rt_wdb *writer)
{
    const point_t vertex = {3.0, -2.0, 5.0};
    const vect_t height = {0.0, 6.0, 8.0};
    const vect_t major_axis = {5.0, 0.0, 0.0};

    if (mk_hyp(writer, "hyp.s", vertex, height, major_axis, 2.5, 0.4) < 0)
	return 1;
    db_update_nref(dbip);
    return check_conversion(dbip, writer, "hyp.s", "hyp.s", "hyp.brep");
}


static int
check_eto_case(struct db_i *dbip, struct rt_wdb *writer,
    const char *source_name, const char *candidate_name, const point_t vertex,
    const vect_t normal, const vect_t major_axis, double revolution_radius,
    double minor_radius)
{
    if (mk_eto(writer, source_name, vertex, normal, major_axis,
	    revolution_radius, minor_radius) < 0)
	return 1;
    db_update_nref(dbip);

    struct directory *entry = db_lookup(dbip, source_name, LOOKUP_QUIET);
    struct rt_db_internal internal;
    RT_DB_INTERNAL_INIT(&internal);
    if (!entry || rt_db_get_internal(&internal, entry, dbip,
	    bn_mat_identity) < 0)
	return 1;
    struct bn_tol tolerance = BN_TOL_INIT_TOL;
    ON_Brep *sample_brep = ON_Brep::New();
    internal.idb_meth->ft_brep(&sample_brep, &internal, &tolerance);
    double max_residual = 0.0;
    if (sample_brep && sample_brep->m_S.Count() == 1) {
	const ON_Surface *surface = sample_brep->m_S[0];
	const ON_Interval u_domain = surface->Domain(0);
	const ON_Interval v_domain = surface->Domain(1);
	vect_t unit_normal;
	VMOVE(unit_normal, normal);
	VUNITIZE(unit_normal);
	vect_t unit_major;
	VMOVE(unit_major, major_axis);
	const double major_length = MAGNITUDE(unit_major);
	VUNITIZE(unit_major);
	const double ev = VDOT(unit_major, unit_normal);
	const double eu = std::sqrt(std::max(0.0, 1.0 - ev * ev));
	for (int i = 0; i < 64; ++i) {
	    for (int j = 0; j < 64; ++j) {
		const ON_3dPoint point = surface->PointAt(
		    u_domain.ParameterAt((i + 0.5) / 64.0),
		    v_domain.ParameterAt((j + 0.5) / 64.0));
		vect_t offset, radial;
		VSET(offset, point.x - vertex[X], point.y - vertex[Y],
		    point.z - vertex[Z]);
		const double z = VDOT(offset, unit_normal);
		VJOIN1(radial, offset, -z, unit_normal);
		const double h = MAGNITUDE(radial) - revolution_radius;
		const double major_coordinate = eu * h + ev * z;
		const double minor_coordinate = -ev * h + eu * z;
		const double residual = std::fabs(
		    major_coordinate * major_coordinate /
			(major_length * major_length) +
		    minor_coordinate * minor_coordinate /
			(minor_radius * minor_radius) -
		    1.0);
		max_residual = std::max(max_residual, residual);
	    }
	}
    }
    const bool exact_surface = max_residual <= 1.0e-10;
    if (!exact_surface)
	std::fprintf(stderr, "%s B-Rep surface equation residual %.17g\n",
	    source_name, max_residual);
    delete sample_brep;
    rt_db_free_internal(&internal);
    return (exact_surface ? 0 : 1) + check_conversion(dbip, writer,
	source_name, source_name, candidate_name);
}


static int
check_eto(struct db_i *dbip, struct rt_wdb *writer)
{
    const point_t vertex = {500.0, 500.0, 130.602};
    const vect_t normal = {0.0, 0.0, 6.15663};
    const vect_t positive_tilt = {92.3495, 0.0, 92.3495};
    const vect_t negative_tilt = {92.3495, 0.0, -92.3495};
    const point_t rotated_vertex = {-7.0, 11.0, 5.0};
    const vect_t rotated_normal = {2.0, 3.0, 6.0};
    vect_t rotated_normal_unit;
    VMOVE(rotated_normal_unit, rotated_normal);
    VUNITIZE(rotated_normal_unit);
    vect_t rotated_radial = {3.0, -2.0, 0.0};
    VUNITIZE(rotated_radial);
    vect_t rotated_tilt;
    VCOMB2(rotated_tilt, 4.0, rotated_radial, -3.0,
	rotated_normal_unit);
    vect_t axial_major;
    VSCALE(axial_major, rotated_normal_unit, 5.0);

    int failures = check_eto_case(dbip, writer, "eto-positive.s",
	"eto-positive.brep", vertex, normal, positive_tilt, 369.398, 46.1748);
    failures += check_eto_case(dbip, writer, "eto-negative.s",
	"eto-negative.brep", vertex, normal, negative_tilt, 369.398, 46.1748);
    failures += check_eto_case(dbip, writer, "eto-rotated.s",
	"eto-rotated.brep", rotated_vertex, rotated_normal, rotated_tilt,
	12.0, 2.0);
    failures += check_eto_case(dbip, writer, "eto-axial.s",
	"eto-axial.brep", rotated_vertex, rotated_normal, axial_major,
	12.0, 2.0);
    return failures;
}


static int
check_concave_arb(struct db_i *dbip, struct rt_wdb *writer)
{
    const point_t legacy_arb4_vertices[8] = {
	{0.0, 0.0, 0.0}, {0.0, 10.0, 0.0},
	{0.0, 10.0, 10.0}, {0.0, 10.0, 10.0},
	{10.0, 10.0, 0.0}, {10.0, 10.0, 0.0},
	{10.0, 10.0, 0.0}, {10.0, 10.0, 0.0}
    };
    const point_t first_diagonal_vertices[8] = {
	{0.0, 0.0, 0.0}, {10.0, 0.0, 0.0},
	{3.0, 3.0, 0.0}, {0.0, 10.0, 0.0},
	{0.0, 0.0, 10.0}, {10.0, 0.0, 10.0},
	{3.0, 3.0, 10.0}, {0.0, 10.0, 10.0}
    };
    const point_t second_diagonal_vertices[8] = {
	{0.0, 0.0, 0.0}, {10.0, 0.0, 0.0},
	{10.0, 10.0, 0.0}, {6.0, 4.0, 0.0},
	{0.0, 0.0, 10.0}, {10.0, 0.0, 10.0},
	{10.0, 10.0, 10.0}, {6.0, 4.0, 10.0}
    };
    const point_t mirrored_vertices[8] = {
	{0.0, 0.0, 0.0}, {-10.0, 0.0, 0.0},
	{-3.0, 3.0, 0.0}, {0.0, 10.0, 0.0},
	{0.0, 0.0, 10.0}, {-10.0, 0.0, 10.0},
	{-3.0, 3.0, 10.0}, {0.0, 10.0, 10.0}
    };
    if (mk_arb8(writer, "legacy-arb4.s",
	    &legacy_arb4_vertices[0][X]) < 0 ||
	mk_arb8(writer, "concave-arb-first.s",
	    &first_diagonal_vertices[0][X]) < 0 ||
	mk_arb8(writer, "concave-arb-second.s",
	    &second_diagonal_vertices[0][X]) < 0 ||
	mk_arb8(writer, "concave-arb-mirrored.s",
	    &mirrored_vertices[0][X]) < 0)
	return 1;
    db_update_nref(dbip);

    int failures = check_conversion(dbip, writer, "legacy-arb4.s",
	"legacy-arb4.s", "legacy-arb4.brep");
    failures += check_conversion(dbip, writer, "concave-arb-first.s",
	"concave-arb-first.s", "concave-arb-first.brep");
    failures += check_conversion(dbip, writer, "concave-arb-second.s",
	"concave-arb-second.s", "concave-arb-second.brep");
    failures += check_conversion(dbip, writer, "concave-arb-mirrored.s",
	"concave-arb-mirrored.s", "concave-arb-mirrored.brep");
    return failures;
}


static int
check_large_ellipsoid(struct db_i *dbip, struct rt_wdb *writer)
{
    const point_t center = VINIT_ZERO;
    const vect_t a = {-5.47441e9, 3.16065e9, 0.0};
    const vect_t b = {-3.16065e9, -5.47441e9, 0.0};
    const vect_t c = {0.0, 0.0, 6.3e9};
    if (mk_ell(writer, "large-ellipsoid.s", center, a, b, c) < 0)
	return 1;
    db_update_nref(dbip);
    return check_conversion(dbip, writer, "large-ellipsoid.s",
	"large-ellipsoid.s", "large-ellipsoid.brep");
}

} // namespace


int
main(int argc, char **argv)
{
    bu_setprogname(argv[0]);
    if (argc > 2)
	return 1;

    struct db_i *dbip = db_open_inmem();
    struct rt_wdb *writer = dbip ?
	wdb_dbopen(dbip, RT_WDB_TYPE_DB_INMEM) : NULL;
    if (!writer) {
	if (dbip)
	    db_close(dbip);
	return 1;
    }

    int failures = 0;
    if (argc == 1 || BU_STR_EQUAL(argv[1], "metaball"))
	failures += check_metaball(dbip, writer);
    if (argc == 1 || BU_STR_EQUAL(argv[1], "heart"))
	failures += check_heart(dbip, writer);
    if (argc == 1 || BU_STR_EQUAL(argv[1], "superell"))
	failures += check_unit_exponent_superell(dbip, writer);
    if (argc == 1 || BU_STR_EQUAL(argv[1], "pipe"))
	failures += check_pipe(dbip, writer);
    if (argc == 1 || BU_STR_EQUAL(argv[1], "rhc"))
	failures += check_rhc(dbip, writer);
    if (argc == 1 || BU_STR_EQUAL(argv[1], "hyp"))
	failures += check_hyp(dbip, writer);
    if (argc == 1 || BU_STR_EQUAL(argv[1], "eto"))
	failures += check_eto(dbip, writer);
    if (argc == 1 || BU_STR_EQUAL(argv[1], "arb"))
	failures += check_concave_arb(dbip, writer);
    if (argc == 1 || BU_STR_EQUAL(argv[1], "large-ellipsoid"))
	failures += check_large_ellipsoid(dbip, writer);
    if (argc == 2 && !BU_STR_EQUAL(argv[1], "metaball") &&
	!BU_STR_EQUAL(argv[1], "heart") && !BU_STR_EQUAL(argv[1], "superell") &&
	!BU_STR_EQUAL(argv[1], "pipe") && !BU_STR_EQUAL(argv[1], "rhc") &&
	!BU_STR_EQUAL(argv[1], "hyp") && !BU_STR_EQUAL(argv[1], "eto") &&
	!BU_STR_EQUAL(argv[1], "arb") &&
	!BU_STR_EQUAL(argv[1], "large-ellipsoid"))
    {
	failures++;
    }
    db_close(dbip);
    return failures ? 1 : 0;
}


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
