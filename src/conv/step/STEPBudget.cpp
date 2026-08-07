/*                  S T E P B U D G E T . C P P
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 */

#include "common.h"

#include "STEPBudget.h"

#include "brep/pullback.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <ctime>
#include <memory>
#include <thread>
#include <vector>

namespace {

/* Keep startup calibration short enough to be negligible for ordinary files
 * but long enough to outlive scheduler and clock granularity on common hosts. */
constexpr uint64_t kMinimumCalibrationMicroseconds = 80000;
constexpr uint64_t kMaximumCalibrationQueries = 65536;
constexpr uint64_t kMaximumCalibrationBatchQueries = 512;
constexpr unsigned int kMaximumCalibrationWorkers = 8;

/* This is the measured scalar throughput of the documented plane/sphere/torus
 * workload on the importer reference host.  It anchors a dimensionless
 * machine-speed scale; it is not a geometry tolerance or solver limit. */
constexpr double kReferencePullbackQueriesPerSecond = 45500.0;
constexpr double kMinimumAutomaticBudgetScale = 0.5;
constexpr double kMaximumAutomaticBudgetScale = 8.0;

typedef std::vector<std::unique_ptr<ON_Surface> > CalibrationProblems;

bool
make_problems(CalibrationProblems &problems)
{
    std::unique_ptr<ON_PlaneSurface> planar(new ON_PlaneSurface(ON_xy_plane));
    planar->SetDomain(0, -20.0, 20.0);
    planar->SetDomain(1, -20.0, 20.0);
    planar->SetExtents(0, planar->Domain(0));
    planar->SetExtents(1, planar->Domain(1));
    if (!planar->IsValid()) return false;
    problems.push_back(std::move(planar));

    std::unique_ptr<ON_NurbsSurface> spherical(new ON_NurbsSurface());
    const ON_Sphere sphere(ON_3dPoint::Origin, 8.0);
    if (!sphere.IsValid() || !sphere.GetNurbForm(*spherical) ||
	    !spherical->IsValid())
	return false;
    problems.push_back(std::move(spherical));

    std::unique_ptr<ON_NurbsSurface> toroidal(new ON_NurbsSurface());
    const ON_Plane plane(ON_3dPoint::Origin, ON_3dVector::ZAxis);
    const ON_Torus torus(plane, 10.0, 2.0);
    if (!torus.IsValid() || !torus.GetNurbForm(*toroidal) ||
	    !toroidal->IsValid())
	return false;
    problems.push_back(std::move(toroidal));
    return true;
}

bool
run_queries(brlcad::PullbackContext &context,
    const CalibrationProblems &problems,
    uint64_t first, uint64_t count, double &checksum)
{
    if (problems.empty()) return false;

    for (uint64_t offset = 0; offset < count; ++offset) {
	const uint64_t query = first + offset;
	const ON_Surface *surface =
	    problems[static_cast<size_t>(query % problems.size())].get();
	if (!surface) return false;
	const ON_Interval u_domain = surface->Domain(0);
	const ON_Interval v_domain = surface->Domain(1);
	if (!u_domain.IsIncreasing() || !v_domain.IsIncreasing()) return false;
	/* Avoid exact seams and poles while walking a fixed, nonrepeating set of
	 * points across both parameter dimensions. */
	const double uf = 0.025 + 0.95 * static_cast<double>(
	    (query * 37 + 11) % 1009) / 1009.0;
	const double vf = 0.025 + 0.95 * static_cast<double>(
	    (query * 61 + 17) % 1013) / 1013.0;
	const ON_3dPoint source = surface->PointAt(
	    u_domain.ParameterAt(uf), v_domain.ParameterAt(vf));
	ON_2dPoint uv = ON_2dPoint::UnsetPoint;
	ON_3dPoint lift = ON_3dPoint::UnsetPoint;
	double distance = ON_DBL_QNAN;
	if (!source.IsValid() ||
		!context.SurfaceClosestPoint(surface, source, uv, lift,
		    distance, 0, 1.0e-9, 1.0e-6) ||
		!uv.IsValid() || !lift.IsValid() || !std::isfinite(distance) ||
		distance > 1.0e-6)
	    return false;
	checksum += uv.x * 1.0e-6 + uv.y * 1.0e-7 + distance;
    }
    return std::isfinite(checksum);
}

} // namespace

brlcad::step::BudgetCalibration
brlcad::step::CalibrateImportBudgets(unsigned int max_workers)
{
    BudgetCalibration result;
    CalibrationProblems problems;
    if (!make_problems(problems)) return result;

    PullbackContext scalar_context;
    double checksum = 0.0;
    if (!run_queries(scalar_context, problems, 0, 9, checksum)) return result;

    uint64_t completed = 0;
    uint64_t batch = 16;
    const std::chrono::steady_clock::time_point scalar_wall_started =
	std::chrono::steady_clock::now();
    const std::clock_t scalar_cpu_started = std::clock();
    bool valid = true;
    for (;;) {
	const uint64_t permitted = std::min(batch,
	    kMaximumCalibrationQueries - completed);
	if (!permitted || !run_queries(scalar_context, problems,
		completed + 9, permitted, checksum)) {
	    valid = false;
	    break;
	}
	completed += permitted;
	const std::clock_t scalar_cpu_now = std::clock();
	const bool cpu_clock_valid = scalar_cpu_started !=
	    static_cast<std::clock_t>(-1) && scalar_cpu_now !=
	    static_cast<std::clock_t>(-1) &&
	    scalar_cpu_now >= scalar_cpu_started;
	const uint64_t elapsed = cpu_clock_valid ? static_cast<uint64_t>(
	    static_cast<long double>(scalar_cpu_now - scalar_cpu_started) *
	    1000000.0L / static_cast<long double>(CLOCKS_PER_SEC)) :
	    static_cast<uint64_t>(
		std::chrono::duration_cast<std::chrono::microseconds>(
		    std::chrono::steady_clock::now() -
		    scalar_wall_started).count());
	if (elapsed >= kMinimumCalibrationMicroseconds ||
		completed >= kMaximumCalibrationQueries) {
	    result.scalar_queries = completed;
	    result.scalar_microseconds = elapsed;
	    break;
	}
	batch = std::min<uint64_t>(kMaximumCalibrationBatchQueries,
	    std::min<uint64_t>(batch * 2,
		kMaximumCalibrationQueries - completed));
    }
    if (!valid || !result.scalar_queries || !result.scalar_microseconds)
	return result;

    result.scalar_queries_per_second =
	static_cast<double>(result.scalar_queries) * 1.0e6 /
	static_cast<double>(result.scalar_microseconds);
    result.parallel_workers = std::max(1U, std::min(max_workers,
	kMaximumCalibrationWorkers));
    if (result.parallel_workers > 1) {
	std::vector<std::shared_ptr<PullbackContext> > contexts;
	contexts.reserve(result.parallel_workers);
	for (unsigned int worker = 0; worker < result.parallel_workers; ++worker)
	    contexts.push_back(scalar_context.ForkWithSharedSurfaceCache());
	std::vector<int> successes(result.parallel_workers, 0);
	std::vector<double> checksums(result.parallel_workers, 0.0);
	std::vector<std::thread> workers;
	workers.reserve(result.parallel_workers);
	const std::chrono::steady_clock::time_point parallel_started =
	    std::chrono::steady_clock::now();
	const std::clock_t parallel_cpu_started = std::clock();
	for (unsigned int worker = 0; worker < result.parallel_workers; ++worker) {
	    workers.emplace_back([worker, &problems, &contexts, &successes,
		&checksums, &result]() {
		successes[worker] = run_queries(*contexts[worker], problems,
		    100000 + static_cast<uint64_t>(worker) *
			result.scalar_queries,
		    result.scalar_queries, checksums[worker]) ? 1 : 0;
	    });
	}
	for (std::thread &worker : workers) worker.join();
	const uint64_t parallel_us = static_cast<uint64_t>(
	    std::chrono::duration_cast<std::chrono::microseconds>(
		std::chrono::steady_clock::now() - parallel_started).count());
	const std::clock_t parallel_cpu_finished = std::clock();
	if (parallel_cpu_started != static_cast<std::clock_t>(-1) &&
		parallel_cpu_finished != static_cast<std::clock_t>(-1) &&
		parallel_cpu_finished >= parallel_cpu_started)
	    result.parallel_cpu_microseconds = static_cast<uint64_t>(
		static_cast<long double>(parallel_cpu_finished -
		    parallel_cpu_started) * 1000000.0L /
		static_cast<long double>(CLOCKS_PER_SEC));
	if (parallel_us && std::find(successes.begin(), successes.end(), 0) ==
		successes.end())
	    result.parallel_queries_per_second =
		static_cast<double>(result.scalar_queries) *
		static_cast<double>(result.parallel_workers) * 1.0e6 /
		static_cast<double>(parallel_us);
    } else {
	result.parallel_queries_per_second = result.scalar_queries_per_second;
    }
    if (result.parallel_cpu_microseconds &&
	    result.parallel_queries_per_second > 0.0)
	result.parallel_cpu_queries_per_second =
	    static_cast<double>(result.scalar_queries) *
	    static_cast<double>(result.parallel_workers) * 1.0e6 /
	    static_cast<double>(result.parallel_cpu_microseconds);
    else if (result.parallel_queries_per_second > 0.0)
	result.parallel_cpu_queries_per_second =
	    result.parallel_queries_per_second /
	    static_cast<double>(result.parallel_workers);
    else
	result.parallel_cpu_queries_per_second =
	    result.scalar_queries_per_second;

    /* A scalar CPU clock makes the baseline insensitive to descheduling, but
     * geometry workers can still slow each other through cache and memory
     * bandwidth contention.  Compare like-for-like CPU throughput under the
     * configured parallel load and budget for the slower measured regime.
     * This avoids false timeouts caused by the converter's own concurrency
     * without granting extra work merely because an unrelated process ran. */
    const double limiting_queries_per_second = std::min(
	result.scalar_queries_per_second,
	result.parallel_cpu_queries_per_second);
    result.scale = std::max(kMinimumAutomaticBudgetScale,
	std::min(kMaximumAutomaticBudgetScale,
	    kReferencePullbackQueriesPerSecond /
	    limiting_queries_per_second));
    result.valid = true;
    return result;
}

/*
 * Local Variables:
 * tab-width: 8
 * mode: C++
 * c-file-style: "stroustrup"
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
