/*                  P U L L B A C K _ C O N T E X T . C P P
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

#include <atomic>
#include <chrono>
#include <cmath>
#include <thread>
#include <vector>

#include "brep/pullback.h"
#include "brep/util.h"


static bool
exercise_context(unsigned int worker)
{
    const double elevation = static_cast<double>(worker) * 3.0;
    ON_Plane plane(ON_3dPoint(0.0, 0.0, elevation), ON_3dVector::ZAxis);
    ON_PlaneSurface surface(plane);
    surface.SetDomain(0, -20.0, 20.0);
    surface.SetDomain(1, -20.0, 20.0);
    surface.SetExtents(0, surface.Domain(0));
    surface.SetExtents(1, surface.Domain(1));

    brlcad::PullbackContext context;
    for (int iteration = 0; iteration < 200; ++iteration) {
	const double u = -9.0 + static_cast<double>((iteration * 17 + worker) % 180) / 10.0;
	const double v = -8.0 + static_cast<double>((iteration * 29 + worker) % 160) / 10.0;
	const ON_3dPoint point = surface.PointAt(u, v);
	ON_2dPoint uv = ON_2dPoint::UnsetPoint;
	ON_3dPoint lifted = ON_3dPoint::UnsetPoint;
	double distance = ON_DBL_QNAN;
	if (!brlcad::surface_GetClosestPoint3dFirstOrder(context, &surface,
		point, uv, lifted, distance, 0, 1.0e-9, 1.0e-7))
	    return false;
	if (std::fabs(uv.x - u) > 1.0e-7 || std::fabs(uv.y - v) > 1.0e-7 ||
	    lifted.DistanceTo(point) > 1.0e-7 || distance > 1.0e-7)
	    return false;
    }
    return true;
}


static bool
exercise_adaptive_pullback_refinement()
{
    ON_PlaneSurface surface(ON_xy_plane);
    surface.SetDomain(0, -20.0, 20.0);
    surface.SetDomain(1, -20.0, 20.0);
    surface.SetExtents(0, surface.Domain(0));
    surface.SetExtents(1, surface.Domain(1));

    ON_NurbsCurve curve(3, false, 4, 4);
    curve.SetCV(0, ON_3dPoint(-8.0, -8.0, 0.0));
    curve.SetCV(1, ON_3dPoint(-8.0,  8.0, 0.0));
    curve.SetCV(2, ON_3dPoint( 8.0, -8.0, 0.0));
    curve.SetCV(3, ON_3dPoint( 8.0,  8.0, 0.0));
    curve.MakeClampedUniformKnotVector();
    curve.SetDomain(0.0, 1.0);

    const size_t initial_samples = 3 * static_cast<size_t>(curve.Degree()) + 1;
    PBCData *data = pullback_samples(&surface, &curve, 1.0e-6, 1.0e-3,
	1.0e-9, 1.0e-4);
    bool valid = data && data->segments && data->segments->size() == 1;
    if (valid) {
	const ON_2dPointArray *samples = data->segments->front();
	valid = samples && static_cast<size_t>(samples->Count()) > initial_samples;
	for (int i = 0; valid && i < samples->Count(); ++i)
	    valid = (*samples)[i].IsValid();
    }
    if (data) {
	if (data->segments) {
	    while (!data->segments->empty()) {
		delete data->segments->front();
		data->segments->pop_front();
	    }
	    delete data->segments;
	}
	delete data;
    }
    return valid;
}


static bool
exercise_shared_surface_cache()
{
    ON_PlaneSurface surface(ON_xy_plane);
    surface.SetDomain(0, -20.0, 20.0);
    surface.SetDomain(1, -20.0, 20.0);
    surface.SetExtents(0, surface.Domain(0));
    surface.SetExtents(1, surface.Domain(1));

    const unsigned int worker_count = 8;
    const int queries_per_worker = 200;
    brlcad::PullbackContext root;
    std::vector<std::shared_ptr<brlcad::PullbackContext> > contexts;
    contexts.reserve(worker_count);
    for (unsigned int worker = 0; worker < worker_count; ++worker)
	contexts.push_back(root.ForkWithSharedSurfaceCache());

    std::atomic<bool> valid(true);
    std::vector<std::thread> workers;
    workers.reserve(worker_count);
    for (unsigned int worker = 0; worker < worker_count; ++worker) {
	workers.emplace_back([worker, queries_per_worker, &surface, &contexts,
		&valid]() {
	    for (int iteration = 0; iteration < queries_per_worker; ++iteration) {
		const double u = -9.0 + static_cast<double>(
		    (iteration * 17 + worker) % 180) / 10.0;
		const double v = -8.0 + static_cast<double>(
		    (iteration * 29 + worker) % 160) / 10.0;
		const ON_3dPoint point = surface.PointAt(u, v);
		ON_2dPoint uv = ON_2dPoint::UnsetPoint;
		ON_3dPoint lifted = ON_3dPoint::UnsetPoint;
		double distance = ON_DBL_QNAN;
		if (!contexts[worker]->SurfaceClosestPoint(&surface, point, uv,
			lifted, distance, 0, 1.0e-9, 1.0e-7) ||
			uv.DistanceTo(ON_2dPoint(u, v)) > 1.0e-7 ||
			lifted.DistanceTo(point) > 1.0e-7)
		    valid.store(false);
	    }
	});
    }
    for (std::thread &worker : workers)
	worker.join();

    uint64_t preparations = 0;
    uint64_t queries = 0;
    uint64_t cache_hits = 0;
    for (const std::shared_ptr<brlcad::PullbackContext> &context : contexts) {
	const brlcad::PullbackStatistics stats = context->Statistics();
	preparations += stats.surfaces_prepared;
	queries += stats.closest_point_queries;
	cache_hits += stats.surface_cache_hits;
    }
    return valid.load() && preparations == 1 &&
	queries == worker_count * queries_per_worker &&
	cache_hits == queries - 1;
}


static bool
exercise_unwrapped_periodic_seed()
{
    ON_Circle circle(ON_xy_plane, 2.0);
    ON_Cylinder cylinder(circle, 5.0);
    ON_NurbsSurface surface;
    if (2 != cylinder.GetNurbForm(surface) || !surface.IsValid() ||
	    !surface.IsClosed(0))
	return false;

    const ON_Interval domains[2] = {
	surface.Domain(0), surface.Domain(1)
    };
    const bool closed[2] = {
	surface.IsClosed(0), surface.IsClosed(1)
    };
    const double period = domains[0].Length();
    const double native_u = domains[0].ParameterAt(0.17);
    const double native_v = domains[1].ParameterAt(0.43);
    const ON_3dPoint target = surface.PointAt(native_u, native_v);
    const double image = native_u + 19.0 * period;
    const ON_2dPoint seed(image + 0.03 * period,
	domains[1].ParameterAt(0.46));

    brlcad::PullbackContext context;
    ON_2dPoint uv = ON_2dPoint::UnsetPoint;
    ON_3dPoint lifted = ON_3dPoint::UnsetPoint;
    double distance = DBL_MAX;
    if (!context.SurfaceClosestPointFromSeed(&surface, target, seed, uv,
	    lifted, distance, 1.0e-8, closed, domains, 1.0e-10))
	return false;

    /* The result retains the seed's unwrapped periodic image for continuity,
     * while its surface evaluation must come from the native NURBS domain. */
    return fabs(uv.x - image) <= 1.0e-7 &&
	fabs(uv.y - native_v) <= 1.0e-7 &&
	lifted.DistanceTo(target) <= 1.0e-8 && distance <= 1.0e-8;
}


static bool
exercise_narrow_singular_domain_guard()
{
    /* The legacy singularity detector uses an absolute 0.001 UV threshold.
     * On a narrow domain that threshold can cover a substantial, ordinary
     * part of the surface.  check_pullback_data must not move such a point to
     * the collapsed side unless doing so preserves its 3-D lift. */
    ON_NurbsSurface surface(3, false, 2, 2, 2, 2);
    if (!surface.MakeClampedUniformKnotVector(0) ||
	    !surface.MakeClampedUniformKnotVector(1))
	return false;
    surface.SetCV(0, 0, ON_3dPoint::Origin);
    surface.SetCV(0, 1, ON_3dPoint::Origin);
    surface.SetCV(1, 0, ON_3dPoint(10.0, 0.0, 0.0));
    surface.SetCV(1, 1, ON_3dPoint(10.0, 10.0, 0.0));
    surface.SetDomain(0, 0.0, 0.005);
    surface.SetDomain(1, 0.0, 1.0);
    if (!surface.IsValid() || !surface.IsSingular(3))
	return false;

    PBCData data = {};
    data.tolerance = 1.0e-5;
    data.surf = &surface;
    data.segments = new std::list<ON_2dPointArray *>();
    ON_2dPointArray *samples = new ON_2dPointArray();
    const ON_2dPoint near_side(0.0009, 0.25);
    samples->Append(near_side);
    samples->Append(ON_2dPoint(0.004, 0.75));
    data.segments->push_back(samples);
    std::list<PBCData *> pbcs;
    pbcs.push_back(&data);

    const ON_3dPoint before = surface.PointAt(near_side.x, near_side.y);
    const bool checked = check_pullback_data(pbcs);
    const ON_2dPoint after = (*samples)[0];
    const ON_3dPoint lifted = surface.PointAt(after.x, after.y);
    const bool valid = checked && after.DistanceTo(near_side) <= 1.0e-12 &&
	lifted.DistanceTo(before) <= data.tolerance;

    delete samples;
    delete data.segments;
    return valid;
}


static bool
exercise_nurbs_span_bounding_boxes()
{
    ON_NurbsSurface surface(3, true, 4, 4, 8, 7);
    if (!surface.MakeClampedUniformKnotVector(0) ||
	    !surface.MakeClampedUniformKnotVector(1))
	return false;
    for (int u = 0; u < surface.CVCount(0); ++u) {
	for (int v = 0; v < surface.CVCount(1); ++v) {
	    const double x = 2.0 * u + 0.15 * v;
	    const double y = 1.7 * v - 0.1 * u;
	    const double z = std::sin(0.4 * u) * std::cos(0.3 * v);
	    const double weight = 1.0 + 0.05 * ((u + 2 * v) % 4);
	    if (!surface.SetCV(u, v, ON_4dPoint(x * weight, y * weight,
		    z * weight, weight)))
		return false;
	}
    }
    if (!surface.IsValid()) return false;

    std::vector<double> spans[2];
    for (int direction = 0; direction < 2; ++direction) {
	spans[direction].resize((size_t)surface.SpanCount(direction) + 1);
	if (!surface.GetSpanVector(direction, spans[direction].data()))
	    return false;
    }

    const auto contains = [](const ON_BoundingBox &box,
	    const ON_3dPoint &point) {
	const double tolerance = 1.0e-10 * std::max(1.0,
	    box.Diagonal().Length());
	return point.x >= box.Min().x - tolerance &&
	    point.x <= box.Max().x + tolerance &&
	    point.y >= box.Min().y - tolerance &&
	    point.y <= box.Max().y + tolerance &&
	    point.z >= box.Min().z - tolerance &&
	    point.z <= box.Max().z + tolerance;
    };

    for (size_t u = 1; u < spans[0].size(); ++u) {
	for (size_t v = 1; v < spans[1].size(); ++v) {
	    const ON_Interval whole_u(spans[0][u - 1], spans[0][u]);
	    const ON_Interval whole_v(spans[1][v - 1], spans[1][v]);
	    const ON_Interval sub_u(whole_u.ParameterAt(0.2),
		whole_u.ParameterAt(0.8));
	    const ON_Interval sub_v(whole_v.ParameterAt(0.15),
		whole_v.ParameterAt(0.7));
	    ON_BoundingBox box;
	    if (!surface_GetBoundingBox(&surface, sub_u, sub_v, box, false) ||
		    !box.IsValid())
		return false;
	    for (int ui = 0; ui <= 8; ++ui) {
		for (int vi = 0; vi <= 8; ++vi) {
		    const ON_3dPoint point = surface.PointAt(
			sub_u.ParameterAt(ui / 8.0),
			sub_v.ParameterAt(vi / 8.0));
		    if (!point.IsValid() || !contains(box, point))
			return false;
		}
	    }
	}
    }

    /* Preserve ordinary grow-box semantics on the optimized path. */
    ON_BoundingBox grown(ON_3dPoint(-100.0, -100.0, -100.0),
	ON_3dPoint(-99.0, -99.0, -99.0));
    const ON_Interval first_u(spans[0][0], spans[0][1]);
    const ON_Interval first_v(spans[1][0], spans[1][1]);
    return surface_GetBoundingBox(&surface, first_u, first_v, grown, true) &&
	contains(grown, ON_3dPoint(-100.0, -100.0, -100.0)) &&
	contains(grown, surface.PointAt(first_u.Mid(), first_v.Mid()));
}


int
main()
{
    const unsigned int worker_count = 8;
    std::atomic<bool> valid(true);

    /* Disabling a CPU-work deadline must not disable independent stall
     * cancellation, and the caller must be able to distinguish the reason. */
    brlcad::SetPullbackWorkLimit(NULL, NULL, 0, 1);
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    if (!brlcad::PullbackWorkCancelled() ||
	    !brlcad::PullbackWorkStalled() ||
	    brlcad::PullbackWorkDeadlineExpired())
	valid.store(false);
    brlcad::ClearPullbackWorkLimit();

    /* A worker that is descheduled or blocked must not spend its geometry
     * budget.  The same bounded busy loop must eventually consume it. */
    brlcad::SetPullbackWorkLimit(NULL, NULL, 10, 0);
    std::this_thread::sleep_for(std::chrono::milliseconds(25));
    if (brlcad::PullbackWorkCancelled() ||
	    brlcad::PullbackWorkDeadlineExpired())
	valid.store(false);
    volatile double work = 1.0;
    const std::chrono::steady_clock::time_point work_ceiling =
	std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (!brlcad::PullbackWorkCancelled() &&
	    std::chrono::steady_clock::now() < work_ceiling) {
	for (int iteration = 1; iteration <= 10000; ++iteration)
	    work += std::sin(work + static_cast<double>(iteration));
    }
    if (!brlcad::PullbackWorkDeadlineExpired())
	valid.store(false);
    brlcad::ClearPullbackWorkLimit();

    /* Nested workers join one critical-path budget instead of restarting an
     * independent timer. */
    const brlcad::PullbackWorkBudgetHandle shared_budget =
	brlcad::CreatePullbackWorkBudget(10);
    brlcad::SetPullbackWorkLimit(NULL, NULL, shared_budget, 0);
    std::thread budget_helper([shared_budget]() {
	brlcad::SetPullbackWorkLimit(NULL, NULL, shared_budget, 0);
	volatile double helper_work = 1.0;
	const std::chrono::steady_clock::time_point helper_ceiling =
	    std::chrono::steady_clock::now() + std::chrono::seconds(2);
	while (!brlcad::PullbackWorkCancelled() &&
		std::chrono::steady_clock::now() < helper_ceiling) {
	    for (int iteration = 1; iteration <= 10000; ++iteration)
		helper_work += std::cos(helper_work +
		    static_cast<double>(iteration));
	}
	brlcad::ClearPullbackWorkLimit();
    });
    budget_helper.join();
    if (!brlcad::PullbackWorkCancelled() ||
	    !brlcad::PullbackWorkDeadlineExpired())
	valid.store(false);
    brlcad::ClearPullbackWorkLimit();

    /* A completed operation refreshes only the no-progress timer. */
    brlcad::SetPullbackWorkLimit(NULL, NULL, 0, 50);
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
    brlcad::PullbackWorkProgress();
    if (brlcad::PullbackWorkCancelled() || brlcad::PullbackWorkStalled())
	valid.store(false);
    brlcad::ClearPullbackWorkLimit();

    brlcad::SetPullbackWorkLimit(NULL, NULL, 0, 50);
    brlcad::PropagatePullbackWorkStop(false, true);
    if (!brlcad::PullbackWorkStalled() ||
	    brlcad::PullbackWorkDeadlineExpired())
	valid.store(false);
    brlcad::ClearPullbackWorkLimit();
    std::vector<std::thread> workers;
    workers.reserve(worker_count);
    for (unsigned int worker = 0; worker < worker_count; ++worker) {
	workers.emplace_back([worker, &valid]() {
	    if (!exercise_context(worker))
		valid.store(false);
	});
    }
    for (std::thread &worker : workers)
	worker.join();

    /* The source-compatible adapter must remain usable without a context. */
    ON_PlaneSurface surface(ON_xy_plane);
    surface.SetDomain(0, -1.0, 1.0);
    surface.SetDomain(1, -1.0, 1.0);
    surface.SetExtents(0, surface.Domain(0));
    surface.SetExtents(1, surface.Domain(1));
    ON_2dPoint uv = ON_2dPoint::UnsetPoint;
    ON_3dPoint lifted = ON_3dPoint::UnsetPoint;
    double distance = ON_DBL_QNAN;
    if (!surface_GetClosestPoint3dFirstOrder(&surface, ON_3dPoint(0.25, -0.5, 0.0),
	    uv, lifted, distance, 0, 1.0e-9, 1.0e-7))
	valid.store(false);

    /* A closed adaptive sample ring must not acquire opposing endpoint
     * tangents merely because the local cubic fitter treats it as open. */
    ON_2dPointArray closed_samples;
    closed_samples.Append(ON_2dPoint(1.0, 0.0));
    closed_samples.Append(ON_2dPoint(0.0, 1.0));
    closed_samples.Append(ON_2dPoint(-1.0, 0.0));
    closed_samples.Append(ON_2dPoint(0.0, -1.0));
    closed_samples.Append(ON_2dPoint(1.0, 0.0));
    ON_Curve *closed_curve = interpolateCurve(closed_samples);
    if (!closed_curve || !closed_curve->IsValid() || !closed_curve->IsClosed() ||
	    closed_curve->Dimension() != 2) {
	valid.store(false);
    } else {
	ON_3dVector start_tangent = closed_curve->TangentAt(closed_curve->Domain().Min());
	ON_3dVector end_tangent = closed_curve->TangentAt(closed_curve->Domain().Max());
	if (!start_tangent.Unitize() || !end_tangent.Unitize() ||
		start_tangent * end_tangent < 0.0)
	    valid.store(false);
    }
    delete closed_curve;

    /* A successful open-surface refinement contains more UV samples than the
     * original parameter/distance arrays.  Exercise that path directly so
     * bounds-checked and sanitizer builds catch cross-indexing the arrays. */
    if (!exercise_adaptive_pullback_refinement())
	valid.store(false);

    /* Related edge jobs may share immutable surface span boxes without
     * sharing mutable closest-point state or rebuilding the cache. */
    if (!exercise_shared_surface_cache())
	valid.store(false);

    /* Newton continuity may retain an integral-period UV image, but a closed
     * OpenNURBS surface must always be evaluated in its declared domain. */
    if (!exercise_unwrapped_periodic_seed())
	valid.store(false);

    /* UV proximity alone cannot prove that a sample lies on a singular side;
     * the proposed snap must also preserve its represented 3-D point. */
    if (!exercise_narrow_singular_domain_guard())
	valid.store(false);

    /* NURBS sub-span bounds must remain conservative while avoiding a copy of
     * the complete surface for each prepared span. */
    if (!exercise_nurbs_span_bounding_boxes())
	valid.store(false);

    return valid.load() ? 0 : 1;
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
