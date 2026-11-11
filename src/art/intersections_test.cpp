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

#include <atomic>
#include <exception>
#include <memory>
#include <stdexcept>
#include <thread>
#include <vector>

#include "bu/app.h"
#include "bu/file.h"
#include "bu/log.h"
#include "bu/parallel.h"
#include "wdb.h"
#include "art.h"
#include "renderer/kernel/shading/shadingray.h"

static constexpr double INTERSECTION_TOLERANCE = 1.0e-7;

static bool
check_ray(BrlcadObject &object, const asf::Vector3d &origin,
    const asf::Vector3d &direction, double tmin, double tmax,
    bool expected_hit, double distance, const asf::Vector3d &normal)
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
    if (expected_hit && (!NEAR_EQUAL(result.m_distance, distance, INTERSECTION_TOLERANCE) ||
	!VNEAR_EQUAL(result.m_geometric_normal, normal, INTERSECTION_TOLERANCE) ||
	!VNEAR_EQUAL(result.m_shading_normal, normal, INTERSECTION_TOLERANCE))) {
	bu_log("Unexpected boundary: t=%g normal=(%g %g %g)\n", result.m_distance,
	    V3ARGS(result.m_geometric_normal));
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
	wdb_close(db);
	if (!written)
	    throw std::runtime_error("Cannot write intersection fixture");

	asr::ParamArray parameters;
	parameters.insert("database_path", filename).insert("object_path", "sphere.r");
	BrlcadObject sphere("sphere", parameters);
	ok = sphere_entry(sphere) && ok;
	ok = check_ray(sphere, asf::Vector3d(0, 0, 0), asf::Vector3d(0, 0, 2),
	    0, 100, true, 5, asf::Vector3d(0, 0, 1)) && ok;
	ok = check_ray(sphere, asf::Vector3d(-20, 0, 0), asf::Vector3d(2, 0, 0),
	    6, 100, true, 15, asf::Vector3d(1, 0, 0)) && ok;
	ok = check_ray(sphere, asf::Vector3d(-20, 0, 0), asf::Vector3d(2, 0, 0),
	    0, 5, false, 0, asf::Vector3d(0)) && ok;
	ok = check_ray(sphere, asf::Vector3d(-20, 0, 0), asf::Vector3d(0),
	    0, 100, false, 0, asf::Vector3d(0)) && ok;
	parameters.insert("object_path", "cut.r");
	BrlcadObject cut("cut", parameters);
	ok = check_ray(cut, asf::Vector3d(0, 0, 0), asf::Vector3d(1, 0, 0),
	    0, 100, true, 4, asf::Vector3d(-1, 0, 0)) && ok;

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
			asf::Vector3d(1, 0, 0), 0, 100, true, 4, asf::Vector3d(-1, 0, 0)))
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
