/*              I N T E R S E C T I O N S _ T E S T . C P P
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
#include "common.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <condition_variable>
#include <exception>
#include <limits>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <vector>

#include "bu/app.h"
#include "bu/bitv.h"
#include "bu/file.h"
#include "bu/log.h"
#include "bu/parallel.h"
#include "wdb.h"
#include "art.h"
#include "art_test_mesh.h"
#include "renderer/kernel/shading/shadingray.h"

static constexpr double INTERSECTION_TOLERANCE = 1.0e-7;
static constexpr fastf_t PLATE_THICKNESS = 2;

static int
make_plate(struct rt_wdb *db)
{
    fastf_t vertices[] = {-10, -10, 0, 10, -10, 0, 10, 10, 0};
    int faces[] = {0, 1, 2};
    fastf_t thickness[] = {PLATE_THICKNESS};
    struct bu_bitv *face_mode = bu_bitv_new(1);
    const int result = mk_bot(db, "plate.s", RT_BOT_PLATE, RT_BOT_UNORIENTED,
	0, 3, 1, vertices, faces, thickness, face_mode);
    bu_bitv_free(face_mode);
    return result;
}

static bool
check_ray(BrlcadObject &object, const asf::Vector3d &origin,
    const asf::Vector3d &direction, double tmin, double tmax,
    bool expected_hit, double distance, const asf::Vector3d &normal,
    const asf::Vector3d *shading = nullptr)
{
    asr::ShadingRay ray;
    ray.m_org = origin;
    ray.m_dir = direction;
    ray.m_tmin = tmin;
    ray.m_tmax = tmax;
    asr::ProceduralObject::IntersectionResult result;
    object.intersect(ray, result);
    if (result.m_hit != expected_hit || object.intersect(ray) != expected_hit) {
	bu_log("Detailed and occlusion intersections must agree with expected hit=%d\n", expected_hit);
	return false;
    }
    if (expected_hit && (!std::isfinite(result.m_distance) ||
	!std::isfinite(asf::norm(result.m_geometric_normal)) ||
	!std::isfinite(asf::norm(result.m_shading_normal)) ||
	!NEAR_EQUAL(result.m_distance, distance, INTERSECTION_TOLERANCE) ||
	!VNEAR_EQUAL(result.m_geometric_normal, normal, INTERSECTION_TOLERANCE) ||
	!VNEAR_EQUAL(result.m_shading_normal, shading ? *shading : normal, INTERSECTION_TOLERANCE))) {
	bu_log("Unexpected boundary: t=%g geometric=(%g %g %g) shading=(%g %g %g)\n", result.m_distance,
	    V3ARGS(result.m_geometric_normal), V3ARGS(result.m_shading_normal));
	return false;
    }
    return true;
}

static bool
sphere_entry(BrlcadObject &sphere)
{
    return check_ray(sphere, asf::Vector3d(-20, 0, 0), asf::Vector3d(2, 0, 0),
	0, 100, true, 5, asf::Vector3d(-1, 0, 0));
}

static bool
check_offsets(BrlcadObject &object, const asf::Vector3d &point,
    const asf::Vector3d &incoming, const asf::Vector3d &normal, double max_offset,
    bool transmission_hit = true, double incoming_distance = 0)
{
    asf::Vector3d front, back, offset_normal;
    object.refine_and_offset(asf::Ray3d(point, incoming, 0, incoming_distance),
	normal, front, back, offset_normal);
    const auto oriented = asf::dot(normal, incoming) > 0.0 ? -normal : normal;
    if (!VNEAR_EQUAL(offset_normal, oriented, INTERSECTION_TOLERANCE) ||
	!(asf::dot(front - point, oriented) > 0.0) ||
	!(asf::dot(back - point, oriented) < 0.0) ||
	asf::norm(front - point) >= max_offset || asf::norm(back - point) >= max_offset) {
	bu_log("Incorrect surface offset or normal at (%g %g %g)\n", V3ARGS(point));
	return false;
    }
    // Reflection must clear this boundary; transmission must preserve the next hit.
    asr::ShadingRay reflected;
    reflected.m_org = front;
    reflected.m_dir = oriented;
    reflected.m_tmin = 0;
    reflected.m_tmax = max_offset;
    asr::ShadingRay transmitted = reflected;
    transmitted.m_org = back;
    transmitted.m_dir = -oriented;
    transmitted.m_tmax = INFINITY;
    const bool reflected_hit = object.intersect(reflected);
    asr::ProceduralObject::IntersectionResult transmitted_result;
    object.intersect(transmitted, transmitted_result);
    const bool transmitted_hit = transmitted_result.m_hit;
    // A transmitted ray must reach the next exit, not re-enter this face.
    const bool reentered = transmitted_hit &&
	asf::dot(transmitted_result.m_geometric_normal, transmitted.m_dir) <= 0.0;
    if (reflected_hit || transmitted_hit != transmission_hit || reentered) {
	bu_log("Offset ray failure for %s at (%g %g %g): reflected=%d transmitted=%d expected=%d\n",
	    object.get_name(), V3ARGS(point), reflected_hit, transmitted_hit, transmission_hit);
	return false;
    }
    return true;
}

static bool
check_plate(BrlcadObject &plate)
{
    bool passed = true;
    const double half_thickness = PLATE_THICKNESS / 2;
    const asf::Vector3d plate_top(0, 0, 1), plate_bottom(0, 0, -1);
    for (const double side : {-1.0, 1.0}) {
        const asf::Vector3d normal(0, 0, side);
        passed = check_ray(plate, asf::Vector3d(5, -5, side * 3), -normal,
            0, 10, true, 3 - half_thickness, normal) && passed;
        passed = check_ray(plate, asf::Vector3d(5, -5, side * 0.5), normal,
            0, 10, true, half_thickness - 0.5, normal) && passed;
    }
    passed = check_ray(plate, asf::Vector3d(5, -5, 3), asf::Vector3d(0, 0, -2),
        0, 10, true, (3 - half_thickness) / 2, plate_top) && passed;
    passed = check_ray(plate, asf::Vector3d(0, -5, 3), asf::Vector3d(1, 0, -1),
        0, 10, true, 3 - half_thickness, plate_top) && passed;
    passed = check_ray(plate, asf::Vector3d(5, -5, 3), asf::Vector3d(0, 0, -1),
        2.5, 10, true, 3 + half_thickness, plate_bottom) && passed;
    passed = check_ray(plate, asf::Vector3d(5, -5, 3), asf::Vector3d(0, 0, -1),
        0, 1.5, false, 0, plate_top) && passed;
    passed = check_offsets(plate, asf::Vector3d(5, -5, half_thickness),
        asf::Vector3d(0, 0, -1), plate_top, 1.0e-4) && passed;
    return passed;
}


static asr::ProceduralObject::IntersectionResult
query_surface(BrlcadObject &object, const asf::Vector3d &origin, const asf::Vector3d &direction)
{
    asr::ShadingRay ray;
    ray.m_org = origin;
    ray.m_dir = direction;
    ray.m_tmin = 0;
    ray.m_tmax = INFINITY;
    asr::ProceduralObject::IntersectionResult result;
    object.intersect(ray, result);
    return result;
}

static bool
check_uv(BrlcadObject &object, const asf::Vector3d &origin,
    const asf::Vector3d &direction, const asf::Vector2f &expected)
{
    const auto result = query_surface(object, origin, direction);
    if (result.m_hit && std::isfinite(result.m_uv[0]) && std::isfinite(result.m_uv[1]) &&
	NEAR_EQUAL(result.m_uv[0], expected[0], INTERSECTION_TOLERANCE) &&
	NEAR_EQUAL(result.m_uv[1], expected[1], INTERSECTION_TOLERANCE))
	return true;
    bu_log("Unexpected UV mapping for %s: hit=%d UV=(%g,%g), expected=(%g,%g)\n",
	object.get_name(), result.m_hit, result.m_hit ? result.m_uv[0] : 0.0,
	result.m_hit ? result.m_uv[1] : 0.0, expected[0], expected[1]);
    return false;
}

static bool
same_vector(const asf::Vector3d &actual, const asf::Vector3d &expected)
{
    return !VINVALID(actual) && !VINVALID(expected) &&
	VNEAR_EQUAL(actual, expected, INTERSECTION_TOLERANCE * std::max(1.0, asf::norm(expected)));
}

static bool
check_derivatives(BrlcadObject &object, const asf::Vector3d &origin,
    const asf::Vector3d &direction, bool available,
    const asf::Vector3d &dpdu = asf::Vector3d(0), const asf::Vector3d &dpdv = asf::Vector3d(0))
{
    const auto result = query_surface(object, origin, direction);
    const auto &derivatives = result.m_derivatives;
    bool passed = result.m_hit && derivatives.m_valid == available &&
	same_vector(derivatives.m_dpdu, dpdu) && same_vector(derivatives.m_dpdv, dpdv);
    if (available && passed) {
	// Shift independent rays along each known chart tangent and differentiate
	// the returned unit normals.  This also tests Boolean and transform signs.
	constexpr double DIFFERENCE_STEP = 1.0e-6;
	const std::array<asf::Vector3d, 2> tangents = {dpdu, dpdv};
	const std::array<asf::Vector3d, 2> normals = {derivatives.m_dndu, derivatives.m_dndv};
	for (size_t axis = 0; axis < tangents.size(); ++axis) {
	    const auto left = query_surface(object, origin - DIFFERENCE_STEP * tangents[axis], direction);
	    const auto right = query_surface(object, origin + DIFFERENCE_STEP * tangents[axis], direction);
	    passed = passed && left.m_hit && right.m_hit &&
		same_vector(normals[axis], (right.m_shading_normal - left.m_shading_normal) / (2 * DIFFERENCE_STEP));
	}
    } else if (!available) {
	passed = passed && same_vector(derivatives.m_dndu, asf::Vector3d(0)) &&
	    same_vector(derivatives.m_dndv, asf::Vector3d(0));
    }
    if (!passed)
	bu_log("Unexpected surface derivatives for %s: hit=%d available=%d expected=%d\n",
	    object.get_name(), result.m_hit, derivatives.m_valid, available);
    return passed;
}

static bool
check_parallel_uv(BrlcadObject &object)
{
    constexpr size_t UV_WORKERS = 8;
    constexpr size_t QUERIES_PER_WORKER = 64;
    std::array<bool, UV_WORKERS> results;
    results.fill(true);
    std::mutex mutex;
    std::condition_variable start_condition;
    size_t ready = 0;
    bool start = false;
    std::vector<std::thread> workers;
    for (size_t worker = 0; worker < UV_WORKERS; ++worker) {
	workers.emplace_back([&, worker] {
	    {
		std::unique_lock<std::mutex> lock(mutex);
		++ready;
		start_condition.notify_all();
		start_condition.wait(lock, [&] { return start; });
	    }
	    for (size_t query = 0; query < QUERIES_PER_WORKER; ++query)
		if (!check_uv(object, asf::Vector3d(-1, 0, 0), asf::Vector3d(1, 0, 0), asf::Vector2f(0.5f)))
		    results[worker] = false;
	});
    }
    {
	std::unique_lock<std::mutex> lock(mutex);
	start_condition.wait(lock, [&] { return ready == UV_WORKERS; });
	start = true;
	start_condition.notify_all();
    }
    for (auto &worker : workers) worker.join();
    return std::all_of(results.begin(), results.end(), [](bool passed) { return passed; });
}

int
main(int argc, char **argv)
{
    bu_setprogname(argv[0]);
    if (argc != 1)
	return 1;
    char filename[MAXPATHLEN];
    FILE *temporary = bu_temp_file(filename, sizeof(filename));
    if (!temporary)
	return 1;
    fclose(temporary);
    bool ok = true;
    try {
	struct rt_wdb *db = wdb_fopen(filename);
	if (!db)
	    throw std::runtime_error("Cannot create intersection fixture");
	point_t center = {0, 0, 0}, low = {-10, -10, -10}, high = {10, 10, 10};
	point_t base = {0, -15, 0};
	vect_t height = {0, 30, 0};
	bool written = mk_sph(db, "sphere.s", center, 10) == 0 &&
	    mk_rpp(db, "box.s", low, high) == 0 && mk_rcc(db, "bore.s", base, height, 4) == 0;
	struct wmember members;
	BU_LIST_INIT(&members.l);
	written = written && mk_addmember("sphere.s", &members.l, NULL, WMOP_UNION) &&
	    mk_lcomb(db, "sphere.r", &members, 1, NULL, NULL, NULL, 0) == 0;
	BU_LIST_INIT(&members.l);
	written = written && mk_addmember("box.s", &members.l, NULL, WMOP_UNION) &&
	    mk_addmember("bore.s", &members.l, NULL, WMOP_SUBTRACT) &&
	    mk_lcomb(db, "cut.r", &members, 1, NULL, NULL, NULL, 0) == 0;
	const struct { const char *name; double origin; } slabs[] = {
	    {"slab", 1.0e9}, {"origin_slab", 0}
	};
	for (const auto &slab : slabs) {
	    point_t slab_min = {slab.origin, -10, -10}, slab_max = {slab.origin + 0.01, 10, 10};
	    const std::string solid = std::string(slab.name) + ".s";
	    const std::string region = std::string(slab.name) + ".r";
	    written = written && mk_rpp(db, solid.c_str(), slab_min, slab_max) == 0;
	    BU_LIST_INIT(&members.l);
	    written = written && mk_addmember(solid.c_str(), &members.l, NULL, WMOP_UNION) &&
		mk_lcomb(db, region.c_str(), &members, 1, NULL, NULL, NULL, 0) == 0;
	}
	point_t small_center = {0, 0, 0}, large_center = {1.0e9, 0, 0};
	written = written && mk_sph(db, "small.s", small_center, 0.01) == 0 &&
	    mk_sph(db, "large.s", large_center, 1.0e6) == 0;
	for (const char *name : {"small", "large"}) {
	    const std::string solid = std::string(name) + ".s";
	    const std::string region = std::string(name) + ".r";
	    BU_LIST_INIT(&members.l);
	    written = written && mk_addmember(solid.c_str(), &members.l, NULL, WMOP_UNION) &&
		mk_lcomb(db, region.c_str(), &members, 1, NULL, NULL, NULL, 0) == 0;
	}
	written = written && make_plate(db) == 0 &&
	    art_test::make_mesh(db, "mesh.s", art_test::MeshUV::Authored) == 0 &&
	    art_test::make_mesh(db, "plain_mesh.s", art_test::MeshUV::None) == 0 &&
	    art_test::make_mesh(db, "smooth_mesh.s", art_test::MeshUV::Authored, art_test::MeshNormals::Authored) == 0;
	for (const char *name : {"mesh", "plain_mesh", "smooth_mesh", "plate"}) {
	    const std::string solid = std::string(name) + ".s";
	    const std::string region = std::string(name) + ".r";
	    BU_LIST_INIT(&members.l);
	    written = written && mk_addmember(solid.c_str(), &members.l, NULL, WMOP_UNION) &&
		mk_lcomb(db, region.c_str(), &members, 1, NULL, NULL, NULL, 0) == 0;
	}
	mat_t mesh_transform;
	MAT_IDN(mesh_transform);
	mesh_transform[0] = 2;
	mesh_transform[5] = 3;
	mesh_transform[MDX] = 30;
	BU_LIST_INIT(&members.l);
	written = written && mk_addmember("mesh.r", &members.l, mesh_transform, WMOP_UNION) &&
	    mk_lcomb(db, "uv_transform.g", &members, 0, NULL, NULL, NULL, 0) == 0;
	BU_LIST_INIT(&members.l);
	written = written && mk_addmember("smooth_mesh.r", &members.l, mesh_transform, WMOP_UNION) &&
	    mk_lcomb(db, "normal_transform.g", &members, 0, NULL, NULL, NULL, 0) == 0;
	const point_t outer_low = {-20, -20, -20}, outer_high = {20, 20, 20};
	written = written && mk_rpp(db, "outer.s", outer_low, outer_high) == 0;
	BU_LIST_INIT(&members.l);
	written = written && mk_addmember("outer.s", &members.l, NULL, WMOP_UNION) &&
	    mk_addmember("smooth_mesh.s", &members.l, NULL, WMOP_SUBTRACT) &&
	    mk_lcomb(db, "cut_mesh.r", &members, 1, NULL, NULL, NULL, 0) == 0;
	wdb_close(db);
	if (!written)
	    throw std::runtime_error("Cannot write intersection fixture");

	asr::ParamArray parameters;
	parameters.insert("database_path", filename).insert("object_path", "sphere.r");
	BrlcadObject sphere("sphere", parameters);
	ok = sphere_entry(sphere) && ok;
	parameters.insert("object_path", "plate.r");
	BrlcadObject plate("plate", parameters);
	ok = check_plate(plate) && ok;
	// Distant origins must not lose the small sphere discriminant when
	// large quadratic terms cancel. Check distance and the boundary normal.
	constexpr double FAR_ORIGIN = 1.0e8;
	const double radial_x = std::sqrt(10.0 * 10.0 - 3.0 * 3.0);
	const asf::Vector3d far_normal(-radial_x / 10.0, 0.3, 0);
	ok = check_ray(sphere, asf::Vector3d(-FAR_ORIGIN, 3, 0), asf::Vector3d(1, 0, 0),
	    0, INFINITY, true, FAR_ORIGIN - radial_x, far_normal) && ok;
	ok = check_ray(sphere, asf::Vector3d(-FAR_ORIGIN, 3, 0), asf::Vector3d(2, 0, 0),
	    0, INFINITY, true, (FAR_ORIGIN - radial_x) / 2, far_normal) && ok;
	ok = check_ray(sphere, asf::Vector3d(0, 0, 0), asf::Vector3d(0, 0, 2),
	    0, 100, true, 5, asf::Vector3d(0, 0, 1)) && ok;
	ok = check_ray(sphere, asf::Vector3d(-20, 0, 0), asf::Vector3d(2, 0, 0),
	    6, 100, true, 15, asf::Vector3d(1, 0, 0)) && ok;
	ok = check_ray(sphere, asf::Vector3d(-20, 0, 0), asf::Vector3d(2, 0, 0),
	    5 + 1.0e-8, 100, true, 15, asf::Vector3d(1, 0, 0)) && ok;
	ok = check_ray(sphere, asf::Vector3d(10 - 1.0e-8, 0, 0), asf::Vector3d(1, 0, 0),
	    0, 100, true, 1.0e-8, asf::Vector3d(1, 0, 0)) && ok;
	ok = check_ray(sphere, asf::Vector3d(-20, 0, 0), asf::Vector3d(2, 0, 0),
	    0, 5, false, 0, asf::Vector3d(0)) && ok;
	ok = check_ray(sphere, asf::Vector3d(-20, 0, 0), asf::Vector3d(0),
	    0, 100, false, 0, asf::Vector3d(0)) && ok;
	parameters.insert("object_path", "cut.r");
	BrlcadObject cut("cut", parameters);
	ok = check_ray(cut, asf::Vector3d(0, 0, 0), asf::Vector3d(1, 0, 0),
	    0, 100, true, 4, asf::Vector3d(-1, 0, 0)) && ok;

	const auto grazing = asf::normalize(asf::Vector3d(-1.0e-6, 1, 0));
	ok = check_offsets(sphere, asf::Vector3d(10, 0, 0), grazing, asf::Vector3d(1, 0, 0), 0.01) && ok;
	ok = check_offsets(sphere, asf::Vector3d(-10, 0, 0), -grazing, asf::Vector3d(-1, 0, 0), 0.01) && ok;
	const auto diagonal = asf::normalize(asf::Vector3d(1, 2, 3));
	const auto diagonal_grazing = asf::normalize(asf::cross(diagonal, asf::Vector3d(1, 0, 0)) - 1.0e-6 * diagonal);
	ok = check_offsets(sphere, 10.0 * diagonal, diagonal_grazing, diagonal, 0.01) && ok;
	parameters.insert("object_path", "origin_slab.r");
	BrlcadObject origin_slab("origin_slab", parameters);
	ok = check_offsets(origin_slab, asf::Vector3d(0), -grazing, asf::Vector3d(-1, 0, 0), 0.001) && ok;
	// A long ray can reconstruct its entry point just outside the surface.
	// Simulate one rounding step at the incoming distance, including nonunit
	// ray directions, without depending on a particular cancellation result.
	constexpr double INCOMING_LENGTH = 1.0e6;
	const double reconstruction_error = std::numeric_limits<double>::epsilon() * INCOMING_LENGTH;
	const asf::Vector3d reconstructed(-reconstruction_error, 0, 0);
	ok = check_offsets(origin_slab, reconstructed, asf::Vector3d(1, 0, 0),
	    asf::Vector3d(-1, 0, 0), 0.001, true, INCOMING_LENGTH) && ok;
	ok = check_offsets(origin_slab, reconstructed, asf::Vector3d(2, 0, 0),
	    asf::Vector3d(-1, 0, 0), 0.001, true, INCOMING_LENGTH / 2) && ok;
	// No UV query has initialized this ARB's optional mapping data yet.
	ok = check_parallel_uv(origin_slab) && ok;
	parameters.insert("object_path", "slab.r");
	BrlcadObject slab("slab", parameters);
	ok = check_offsets(slab, asf::Vector3d(1.0e9, 0, 0), -grazing,
	    asf::Vector3d(-1, 0, 0), 0.001) && ok;

	ok = check_offsets(sphere, asf::Vector3d(10, 0, 0), asf::Vector3d(1, 0, 0),
	    asf::Vector3d(1, 0, 0), 0.01, false) && ok;
	ok = check_offsets(cut, asf::Vector3d(4, 0, 0), asf::Vector3d(1, 0, 0),
	    asf::Vector3d(-1, 0, 0), 0.001) && ok;
	parameters.insert("object_path", "small.r");
	BrlcadObject small("small", parameters);
	ok = check_offsets(small, asf::Vector3d(0.01, 0, 0), grazing, asf::Vector3d(1, 0, 0), 0.0001) && ok;
	parameters.insert("object_path", "large.r");
	BrlcadObject large("large", parameters);
	ok = check_offsets(large, asf::Vector3d(1.001e9, 0, 0), grazing, asf::Vector3d(1, 0, 0), 0.001) && ok;

	ok = check_uv(sphere, asf::Vector3d(-20, 0, 0), asf::Vector3d(1, 0, 0), asf::Vector2f(0.5f, 0.5f)) && ok;
	ok = check_uv(sphere, asf::Vector3d(0), asf::Vector3d(1, 0, 0), asf::Vector2f(0, 0.5f)) && ok;
	ok = check_uv(sphere, asf::Vector3d(0, 20, 0), asf::Vector3d(0, -1, 0), asf::Vector2f(0.25f, 0.5f)) && ok;
	ok = check_uv(cut, asf::Vector3d(0), asf::Vector3d(1, 0, 0), asf::Vector2f(0.5f, 0.5f)) && ok;
	parameters.insert("object_path", "mesh.r");
	BrlcadObject mesh("mesh", parameters);
	ok = check_uv(mesh, asf::Vector3d(5, -5, 20), asf::Vector3d(0, 0, -1), asf::Vector2f(0.5f, 0.25f)) && ok;
	ok = check_uv(mesh, asf::Vector3d(5, -5, 0), asf::Vector3d(0, 0, 1), asf::Vector2f(0.5f, 0.25f)) && ok;
	parameters.insert("object_path", "/uv_transform.g/mesh.r");
	BrlcadObject transformed_mesh("transformed_mesh", parameters);
	ok = check_uv(transformed_mesh, asf::Vector3d(40, -15, 20), asf::Vector3d(0, 0, -1), asf::Vector2f(0.5f, 0.25f)) && ok;
	parameters.insert("object_path", "plain_mesh.r");
	BrlcadObject plain_mesh("plain_mesh", parameters);
	ok = check_uv(plain_mesh, asf::Vector3d(5, -5, 20), asf::Vector3d(0, 0, -1), asf::Vector2f(0)) && ok;

	parameters.insert("object_path", "smooth_mesh.r");
	BrlcadObject smooth_mesh("smooth_mesh", parameters);
	const auto smooth_normal = asf::normalize(asf::Vector3d(0.5, -0.5, 1));
	ok = check_ray(smooth_mesh, asf::Vector3d(5, -5, 20), asf::Vector3d(0, 0, -1),
	    0, 100, true, 10, asf::Vector3d(0, 0, 1), &smooth_normal) && ok;
	ok = check_ray(smooth_mesh, asf::Vector3d(5, -5, 0), asf::Vector3d(0, 0, 1),
	    0, 100, true, 10, asf::Vector3d(0, 0, 1), &smooth_normal) && ok;
	ok = check_offsets(smooth_mesh, asf::Vector3d(5, -5, 10), asf::Vector3d(0, 0, -1),
	    asf::Vector3d(0, 0, 1), 0.01) && ok;
	parameters.insert("object_path", "/normal_transform.g/smooth_mesh.r");
	BrlcadObject transformed_smooth_mesh("transformed_smooth_mesh", parameters);
	const auto transformed_normal = asf::normalize(asf::Vector3d(0.5 / 2, -0.5 / 3, 1));
	ok = check_ray(transformed_smooth_mesh, asf::Vector3d(40, -15, 20), asf::Vector3d(0, 0, -1),
	    0, 100, true, 10, asf::Vector3d(0, 0, 1), &transformed_normal) && ok;
	ok = check_offsets(transformed_smooth_mesh, asf::Vector3d(40, -15, 10), asf::Vector3d(0, 0, -1),
	    asf::Vector3d(0, 0, 1), 0.01) && ok;
	parameters.insert("object_path", "cut_mesh.r");
	BrlcadObject cut_mesh("cut_mesh", parameters);
	const auto cut_normal = -smooth_normal;
	ok = check_ray(cut_mesh, asf::Vector3d(5, -5, 0), asf::Vector3d(0, 0, 1),
	    0, 100, true, 10, asf::Vector3d(0, 0, -1), &cut_normal) && ok;
	ok = check_offsets(cut_mesh, asf::Vector3d(5, -5, 10), asf::Vector3d(0, 0, 1),
	    asf::Vector3d(0, 0, -1), 0.01) && ok;

	const asf::Vector3d sphere_dpdu(0, 20 * M_PI, 0), sphere_dpdv(0, 0, 10 * M_PI);
	ok = check_derivatives(sphere, asf::Vector3d(20, 0, 0), asf::Vector3d(-1, 0, 0),
	    true, sphere_dpdu, sphere_dpdv) && ok;
	ok = check_derivatives(sphere, asf::Vector3d(0), asf::Vector3d(1, 0, 0),
	    true, sphere_dpdu, sphere_dpdv) && ok;
	ok = check_derivatives(sphere, asf::Vector3d(0, 0, 20), asf::Vector3d(0, 0, -1), false) && ok;
	ok = check_derivatives(origin_slab, asf::Vector3d(-1, 0, 0), asf::Vector3d(1, 0, 0), false) && ok;
	const asf::Vector3d mesh_dpdu(20, 0, 0), mesh_dpdv(20, 20, 0);
	for (auto *object : {&mesh, &smooth_mesh}) {
	    ok = check_derivatives(*object, asf::Vector3d(5, -5, 20), asf::Vector3d(0, 0, -1),
		true, mesh_dpdu, mesh_dpdv) && ok;
	    ok = check_derivatives(*object, asf::Vector3d(5, -5, 0), asf::Vector3d(0, 0, 1),
		true, mesh_dpdu, mesh_dpdv) && ok;
	}
	for (double z : {0.0, 15.0})
	    ok = check_derivatives(cut_mesh, asf::Vector3d(5, -5, z),
		asf::Vector3d(0, 0, z > 10 ? -1 : 1), true, mesh_dpdu, mesh_dpdv) && ok;
	for (auto *object : {&transformed_mesh, &transformed_smooth_mesh})
	    ok = check_derivatives(*object, asf::Vector3d(40, -15, 20), asf::Vector3d(0, 0, -1),
		true, asf::Vector3d(40, 0, 0), asf::Vector3d(40, 60, 0)) && ok;
	ok = check_derivatives(plain_mesh, asf::Vector3d(5, -5, 20), asf::Vector3d(0, 0, -1), false) && ok;

	// A new worker must reuse a slot after the previous worker has left.
	std::atomic<bool> workers_ok{true};
	for (size_t i = 0; i <= MAX_PSW; ++i) {
	    std::thread worker([&] { if (!sphere_entry(sphere)) workers_ok = false; });
	    worker.join();
	}
	constexpr size_t WORKER_COUNT = 8;
	constexpr size_t RAYS_PER_WORKER = 1000;
	std::vector<std::thread> workers;
	for (size_t i = 0; i < WORKER_COUNT; ++i)
	    workers.emplace_back([&] {
		for (size_t j = 0; j < RAYS_PER_WORKER; ++j) {
		    if (!sphere_entry(sphere) || !check_ray(cut, asf::Vector3d(0, 0, 0),
			asf::Vector3d(1, 0, 0), 0, 100, true, 4, asf::Vector3d(-1, 0, 0)) ||
		    !check_ray(smooth_mesh, asf::Vector3d(5, -5, 20), asf::Vector3d(0, 0, -1),
			0, 100, true, 10, asf::Vector3d(0, 0, 1), &smooth_normal))
			workers_ok = false;
		}
	    });
	for (auto &worker : workers) worker.join();
	ok = workers_ok && ok;

	parameters.insert("object_path", "sphere.r");
	constexpr size_t OBJECT_LIFECYCLES = 32;
	for (size_t i = 0; i < OBJECT_LIFECYCLES; ++i) {
	    BrlcadObject temporary_object("temporary", parameters);
	    ok = sphere_entry(temporary_object) && ok;
	}
	auto *source = db_open_ex(filename, DB_OPEN_READONLY, DB_OPEN_COMB_INSTANCES);
	if (!source) throw std::runtime_error("Cannot open shared database fixture");
	std::unique_ptr<BrlcadObject> retained;
	try { retained.reset(new BrlcadObject("retained", parameters, source)); }
	catch (...) { db_close(source); throw; }
	db_close(source);
	ok = sphere_entry(*retained) && ok;
	retained.reset();

	parameters.insert("object_path", "absent.r");
	bool rejected = false;
	try { BrlcadObject absent("absent", parameters); }
	catch (const std::exception &) { rejected = true; }
	if (!rejected) {
	    bu_log("An invalid object path must fail construction\n");
	    ok = false;
	}
    } catch (const std::exception &error) {
	bu_log("Intersection test failed: %s\n", error.what());
	ok = false;
    }
    if (!bu_file_delete(filename)) {
	bu_log("Cannot remove intersection fixture\n");
	ok = false;
    }
    return ok ? 0 : 1;
}
