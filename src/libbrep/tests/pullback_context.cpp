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


int
main()
{
    const unsigned int worker_count = 8;
    std::atomic<bool> valid(true);
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
