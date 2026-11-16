/*                        S U R F A C E _ S A M P L I N G . C P P
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
#include <cmath>
#include <cstring>
#include <memory>
#include <limits>
#include <stdexcept>
#include <thread>
#include <vector>

#include "bu/app.h"
#include "bu/bitv.h"
#include "bu/file.h"
#include "bu/log.h"
#include "raytrace.h"
#include "rt/surface.h"
#include "wdb.h"

namespace {
constexpr size_t SampleCount = 65536;
constexpr size_t WorkerCount = 4;
constexpr double IntegrationTolerance = 0.02;
constexpr double SurfaceTolerance = 1.0e-7;
constexpr double CentroidTolerance = 0.005;

struct DestroyResource {
    void operator()(struct resource *resource) const {
	rt_clean_resource_basic(nullptr, resource);
	delete resource;
    }
};
struct DestroyRaytrace {
    void operator()(struct rt_i *rtip) const { if (rtip) rt_i_destroy(rtip); }
};

class PreparedRegion {
    // Geometry destruction uses registered resource storage before its release.
    std::vector<std::unique_ptr<struct resource, DestroyResource>> resources;
    std::unique_ptr<struct rt_i, DestroyRaytrace> rtip;
public:
    struct rt_surface_sampler sampler = {};
    PreparedRegion(const char *file, const char *object)
	: rtip(rt_dirbuild_ex(file, nullptr, 0, DB_OPEN_COMB_INSTANCES))
    {
	if (!rtip || rt_gettree(rtip.get(), object) < 0)
	    throw std::runtime_error("Cannot load surface-sampling fixture");
	rt_prep_parallel(rtip.get(), 1);
	if (rt_surface_sampler_init(&sampler, rtip.get()) != 0)
	    throw std::runtime_error("Cannot initialize surface sampler");
	for (size_t i = 0; i < WorkerCount; ++i) {
	    std::unique_ptr<struct resource, DestroyResource> resource(new struct resource{});
	    rt_init_resource(resource.get(), static_cast<int>(i), rtip.get());
	    resources.push_back(std::move(resource));
	}
    }
    struct resource *resource(size_t worker = 0) const { return resources[worker].get(); }
};

std::array<double, RT_SURFACE_SAMPLE_DIMENSIONS>
variates(size_t index)
{
    // Distinct radical-inverse bases exercise all five independent dimensions.
    const unsigned int bases[RT_SURFACE_SAMPLE_DIMENSIONS] = {2, 3, 5, 7, 11};
    std::array<double, RT_SURFACE_SAMPLE_DIMENSIONS> result = {};
    for (size_t dimension = 0; dimension < result.size(); ++dimension) {
	size_t integer = index;
	double power = 1.0;
	while (integer) {
	    power /= bases[dimension];
	    result[dimension] += power * (integer % bases[dimension]);
	    integer /= bases[dimension];
	}
    }
    return result;
}

bool
close_integral(const char *label, double actual, double expected)
{
    const double scale = std::abs(expected);
    if (std::isfinite(actual) && std::abs(actual - expected) <= IntegrationTolerance * scale)
	return true;
    bu_log("%s: estimated %g, expected %g\n", label, actual, expected);
    return false;
}

struct SurfaceCase {
    const char *name;
    double area;
    bool cavity;
    size_t maximum_crossings;
    double centroid_x;
};

struct Moments {
    double area = 0;
    double cavity_area = 0;
    double unweighted_area = 0;
    double light_mis = 0;
    double x = 0;
    double x2 = 0;
    size_t hits = 0;
    size_t four_crossings = 0;
    size_t maximum_crossings = 0;
    bool valid = true;
};

Moments
integrate(PreparedRegion &object, const SurfaceCase &geometry, size_t begin, size_t end, size_t worker)
{
    Moments sum;
    const bool shell = geometry.cavity;
    const double reference_density = 1.0 / object.sampler.area_scale;
    for (size_t index = begin; index < end; ++index) {
	const auto random = variates(index);
	struct rt_surface_sample sample = {};
	const int status = rt_surface_sampler_sample(&sample, &object.sampler,
	    object.resource(worker), random.data());
	if (status == RT_SURFACE_SAMPLE_MISS)
	    continue;
	if (status != RT_SURFACE_SAMPLE_HIT) {
	    sum.valid = false;
	    break;
	}
	const auto &surface = sample.surface;
	const bool cavity = shell && MAGNITUDE(surface.point) < 1.5;
	const double radius = cavity ? 1.0 : 2.0;
	const double bsdf_density = 0.5 / (4.0 * M_PI * radius * radius);
	const double mis_weight = reference_density * reference_density /
	    (reference_density * reference_density + bsdf_density * bsdf_density);
	sum.area += sample.area_weight;
	sum.cavity_area += cavity ? sample.area_weight : 0.0;
	// Negative control: keeping one crossing without its multiplicity loses area.
	sum.unweighted_area += 2.0 * object.sampler.area_scale;
	sum.light_mis += sample.area_weight * mis_weight;
	// Center before integrating so translations do not amplify area noise.
	sum.x += sample.area_weight * (surface.point[0] - geometry.centroid_x);
	sum.x2 += sample.area_weight * surface.point[0] * surface.point[0];
	++sum.hits;
	sum.four_crossings += sample.crossing_count == 4;
	sum.maximum_crossings = std::max(sum.maximum_crossings, sample.crossing_count);
	sum.valid = sum.valid && sample.solid && sample.region && sample.crossing_count > 0 &&
	    NEAR_EQUAL(MAGNITUDE(surface.geometric_normal), 1.0, SurfaceTolerance) &&
	    NEAR_EQUAL(MAGNITUDE(surface.shading_normal), 1.0, SurfaceTolerance) &&
	    !VINVALID(surface.point) && !VINVALID(surface.derivatives.dpdu) &&
	    !VINVALID(surface.derivatives.dpdv) && !VINVALID(surface.derivatives.dndu) &&
	    !VINVALID(surface.derivatives.dndv);
	if (shell) {
	    if (surface.derivatives_available) {
		vect_t expected_u, expected_v;
		const double orientation = cavity ? -1.0 : 1.0;
		VSCALE(expected_u, surface.derivatives.dpdu, orientation / radius);
		VSCALE(expected_v, surface.derivatives.dpdv, orientation / radius);
		sum.valid = sum.valid && VNEAR_EQUAL(surface.derivatives.dndu, expected_u, SurfaceTolerance) &&
		    VNEAR_EQUAL(surface.derivatives.dndv, expected_v, SurfaceTolerance);
	    }
	    sum.valid = sum.valid &&
		BU_STR_EQUAL(sample.solid->st_name, cavity ? "inner.s" : "outer.s") &&
		NEAR_EQUAL(MAGNITUDE(surface.point), radius, SurfaceTolerance) &&
		((cavity && sample.flipped && VDOT(surface.point, surface.geometric_normal) < 0) ||
		 (!cavity && !sample.flipped && VDOT(surface.point, surface.geometric_normal) > 0));
	}
    }
    return sum;
}

bool
check_region(const char *file, const SurfaceCase &geometry)
{
    const char *name = geometry.name;
    const double area = geometry.area;
    const bool shell = geometry.cavity;
    PreparedRegion object(file, name);
    const Moments sum = integrate(object, geometry, 1, SampleCount + 1, 0);
    const double centroid = sum.x / (SampleCount * area);
    bool ok = sum.valid && sum.hits > 0 && sum.hits < SampleCount &&
	sum.maximum_crossings == geometry.maximum_crossings &&
	close_integral(name, sum.area / SampleCount, area) &&
	std::abs(centroid) <= CentroidTolerance * object.sampler.radius;
    if (shell) {
	const double outer_area = 16.0 * M_PI;
	const double inner_area = 4.0 * M_PI;
	const double reference_density = 1.0 / object.sampler.area_scale;
	double exact_light_mis = 0.0, exact_bsdf_mis = 0.0;
	for (double patch_area : {outer_area, inner_area}) {
	    const double q = 0.5 / patch_area;
	    const double w = reference_density * reference_density /
		(reference_density * reference_density + q * q);
	    exact_light_mis += patch_area * w;
	    exact_bsdf_mis += patch_area * (1.0 - w);
	}
	ok = close_integral("cavity area", sum.cavity_area / SampleCount, inner_area) && ok;
	ok = close_integral("second moment", sum.x2 / SampleCount,
	    (outer_area * 4.0 + inner_area) / 3.0) && ok;
	ok = close_integral("weighted MIS", sum.light_mis / SampleCount, exact_light_mis) && ok;
	ok = close_integral("combined MIS", sum.light_mis / SampleCount + exact_bsdf_mis, area) && ok;
	ok = close_integral("unweighted negative control", sum.unweighted_area / SampleCount, outer_area) && ok;
	ok = sum.four_crossings > 0 && sum.unweighted_area < 0.9 * sum.area && ok;
	std::array<Moments, WorkerCount> threaded;
	std::vector<std::thread> threads;
	for (size_t worker = 0; worker < WorkerCount; ++worker) {
	    threads.emplace_back([&, worker] {
		threaded[worker] = integrate(object, geometry, worker * SampleCount / WorkerCount + 1,
		    (worker + 1) * SampleCount / WorkerCount + 1, worker);
	    });
	}
	for (auto &thread : threads) thread.join();
	double threaded_area = 0.0;
	for (const auto &part : threaded) {
	    ok = part.valid && ok;
	    threaded_area += part.area;
	}
	ok = NEAR_EQUAL(threaded_area, sum.area, SurfaceTolerance * SampleCount) && ok;
    }
    bu_log("%s: area=%g expected=%g hits=%zu/%zu cavity=%g status=%s\n", name,
	sum.area / SampleCount, area, sum.hits, SampleCount, sum.cavity_area / SampleCount,
	ok ? "passed" : "FAILED");
    return ok;
}

bool
check_rejections(const char *file)
{
    PreparedRegion object(file, "sphere.r");
    struct rt_surface_sample output = {};
    output.area_weight = -17.0;
    output.crossing_count = 19;
    std::array<unsigned char, sizeof(output)> original;
    std::memcpy(original.data(), &output, sizeof(output));
    auto random = variates(1);
    for (size_t dimension = 0; dimension < random.size(); ++dimension) {
	for (double invalid : {-1.0, 1.0, std::numeric_limits<double>::infinity(),
	    std::numeric_limits<double>::quiet_NaN()}) {
	    auto bad = random;
	    bad[dimension] = invalid;
	    if (rt_surface_sampler_sample(&output, &object.sampler, object.resource(), bad.data()) !=
		RT_SURFACE_SAMPLE_ERROR || std::memcmp(original.data(), &output, sizeof(output)) != 0)
		return false;
	}
    }
    // A short chord near the reference sphere's equator misses the inner solid.
    const std::array<double, RT_SURFACE_SAMPLE_DIMENSIONS> miss = {0.5, 0.0, 0.5, 0.05, 0.5};
    if (rt_surface_sampler_sample(&output, &object.sampler, object.resource(), miss.data()) !=
	RT_SURFACE_SAMPLE_MISS || std::memcmp(original.data(), &output, sizeof(output)) != 0)
	return false;
    // A coincident endpoint pair has no line and contributes zero, without retry.
    random[2] = random[0]; random[3] = random[1];
    if (rt_surface_sampler_sample(&output, &object.sampler, object.resource(), random.data()) !=
	RT_SURFACE_SAMPLE_MISS || std::memcmp(original.data(), &output, sizeof(output)) != 0)
	return false;
    if (rt_surface_sampler_sample(&output, &object.sampler, nullptr, random.data()) !=
	RT_SURFACE_SAMPLE_ERROR)
	return false;
    struct rt_surface_sampler unprepared = {};
    const struct rt_surface_sampler marker = unprepared;
    struct db_i *database = db_open_inmem();
    if (!database) return false;
    struct rt_i *empty = rt_i_create(database);
    db_close(database);
    if (!empty) return false;
    const int status = rt_surface_sampler_init(&unprepared, empty);
    rt_i_destroy(empty);
    bool ok = status == -1 && unprepared.rtip == marker.rtip &&
	ZERO(unprepared.radius) && ZERO(unprepared.area_scale);
    for (const char *name : {"half.r", "plate.r"}) {
	std::unique_ptr<struct rt_i, DestroyRaytrace> rejected(rt_dirbuild_ex(file, nullptr, 0, DB_OPEN_COMB_INSTANCES));
	if (!rejected || rt_gettree(rejected.get(), name) < 0) return false;
	rt_prep_parallel(rejected.get(), 1);
	ok = rt_surface_sampler_init(&unprepared, rejected.get()) == -1 && ok;
    }
    std::unique_ptr<struct rt_i, DestroyRaytrace> multiple(rt_dirbuild_ex(file, nullptr, 0, DB_OPEN_COMB_INSTANCES));
    if (!multiple || rt_gettree(multiple.get(), "sphere.r") < 0 ||
	rt_gettree(multiple.get(), "box.r") < 0)
	return false;
    rt_prep_parallel(multiple.get(), 1);
    return rt_surface_sampler_init(&unprepared, multiple.get()) == -1 && ok;
}

bool
write_fixture(const char *filename)
{
    struct rt_wdb *db = wdb_fopen(filename);
    if (!db) return false;
    point_t center = {0, 0, 0}, low = {-1, -1, -1}, high = {1, 1, 1};
    const vect_t half_normal = {1, 0, 0};
    const point_t far_center = {1.0e9, 0, 0};
    bool ok = mk_half(db, "half.s", half_normal, 0.0) == 0 &&
	mk_sph(db, "outer.s", center, 2.0) == 0 &&
	mk_sph(db, "inner.s", center, 1.0) == 0 &&
	mk_sph(db, "small.s", center, 0.01) == 0 &&
	mk_sph(db, "far.s", far_center, 1.0e6) == 0 && mk_rpp(db, "box.s", low, high) == 0;
    // Independently wound cube faces and one zero-thickness triangular sheet.
    fastf_t vertices[] = {-1,-1,-1, 1,-1,-1, 1,1,-1, -1,1,-1,
	-1,-1,1, 1,-1,1, 1,1,1, -1,1,1};
    int faces[] = {0,2,1, 0,3,2, 4,5,6, 4,6,7, 0,4,7, 0,7,3,
	1,2,6, 1,6,5, 0,1,5, 0,5,4, 3,7,6, 3,6,2};
    fastf_t sheet[] = {-1,-1,0, 1,-1,0, -1,1,0};
    int triangle[] = {0,1,2};
    ok = mk_bot(db, "mesh.s", RT_BOT_SOLID, RT_BOT_CCW, 0,
	8, 12, vertices, faces, nullptr, nullptr) == 0 && ok;
    ok = mk_bot(db, "sheet.s", RT_BOT_SURFACE, RT_BOT_CCW, 0,
	3, 1, sheet, triangle, nullptr, nullptr) == 0 && ok;
    fastf_t thickness[] = {0.2};
    struct bu_bitv *face_mode = bu_bitv_new(1);
    ok = mk_bot(db, "plate.s", RT_BOT_PLATE, RT_BOT_CCW, 0,
	3, 1, sheet, triangle, thickness, face_mode) == 0 && ok;
    bu_bitv_free(face_mode);
    const struct {
	const char *name;
	const char *solid;
	const char *other;
	int operation;
    } regions[] = {
	{"sphere.r", "outer.s", nullptr, WMOP_UNION},
	{"shell.r", "outer.s", "inner.s", WMOP_SUBTRACT},
	{"box.r", "box.s", nullptr, WMOP_UNION},
	{"mesh.r", "mesh.s", nullptr, WMOP_UNION},
	{"sheet.r", "sheet.s", nullptr, WMOP_UNION},
	{"plate.r", "plate.s", nullptr, WMOP_UNION},
	{"half.r", "half.s", nullptr, WMOP_UNION},
	{"clipped.r", "box.s", "half.s", WMOP_SUBTRACT},
	{"union.r", "outer.s", "inner.s", WMOP_UNION},
	{"small.r", "small.s", nullptr, WMOP_UNION},
	{"far.r", "far.s", nullptr, WMOP_UNION}
    };
    for (const auto &region : regions) {
	struct wmember members;
	BU_LIST_INIT(&members.l);
	ok = mk_addmember(region.solid, &members.l, nullptr, WMOP_UNION) && ok;
	if (region.other)
	    ok = mk_addmember(region.other, &members.l, nullptr, region.operation) && ok;
	ok = mk_lcomb(db, region.name, &members, 1, nullptr, nullptr, nullptr, 0) == 0 && ok;
    }
    wdb_close(db);
    return ok;
}
} // namespace

int
main(int argc, char **argv)
{
    bu_setprogname(argv[0]);
    if (argc != 1) return 1;
    char filename[MAXPATHLEN];
    FILE *file = bu_temp_file(filename, sizeof(filename));
    if (!file) return 1;
    fclose(file);
    bool ok = false;
    try {
	ok = write_fixture(filename);
	if (ok) {
	    const SurfaceCase cases[] = {
		{"sphere.r", 16.0 * M_PI, false, 2, 0.0},
		{"shell.r", 20.0 * M_PI, true, 4, 0.0},
		{"box.r", 24.0, false, 2, 0.0},
		{"mesh.r", 24.0, false, 2, 0.0},
		{"sheet.r", 2.0, false, 1, -1.0 / 3.0},
		{"clipped.r", 16.0, false, 2, 0.5},
		{"union.r", 16.0 * M_PI, false, 2, 0.0},
		{"small.r", 4.0 * M_PI * 0.01 * 0.01, false, 2, 0.0},
		{"far.r", 4.0 * M_PI * 1.0e6 * 1.0e6, false, 2, 1.0e9}
	    };
	    for (const auto &geometry : cases)
		ok = check_region(filename, geometry) && ok;
	    ok = check_rejections(filename) && ok;
	}
    } catch (const std::exception &error) {
	bu_log("Surface sampler: %s\n", error.what());
	ok = false;
    }
    bu_file_delete(filename);
    return ok ? 0 : 1;
}

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
