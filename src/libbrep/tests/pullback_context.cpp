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
