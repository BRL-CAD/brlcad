/* BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License version 2.1.
 */

#include "common.h"

#include "bu/app.h"
#include "bu/log.h"

#include "brep.h"
#include "brep/boolean.h"
#include "brep/util.h"


static int
check_point(const ON_Brep *brep, const ON_3dPoint &point, bool expected,
	    const char *description)
{
    bool inside = ON_BrepPointInside(point, brep);
    if (inside == expected) {
	return 0;
    }

    bu_log("%s: expected %s, got %s\n", description,
	   expected ? "inside" : "outside",
	   inside ? "inside" : "outside");
    return 1;
}


static int
check_trimmed_bounding_box()
{
    static const double TRIMMED_HALF_EXTENT = 1.0;
    static const double SUPPORT_HALF_EXTENT = 100.0;
    static const double BOUNDING_TOLERANCE_SCALE = 100.0;
    static const double SUPPORT_SAMPLE_FRACTION = 0.5;
    static const double BOUNDING_TOLERANCE =
	ON_ZERO_TOLERANCE * BOUNDING_TOLERANCE_SCALE;
    ON_3dPointArray boundary_points;
    boundary_points.Append(ON_3dPoint(-TRIMMED_HALF_EXTENT, -TRIMMED_HALF_EXTENT, 0.0));
    boundary_points.Append(ON_3dPoint( TRIMMED_HALF_EXTENT, -TRIMMED_HALF_EXTENT, 0.0));
    boundary_points.Append(ON_3dPoint( TRIMMED_HALF_EXTENT,  TRIMMED_HALF_EXTENT, 0.0));
    boundary_points.Append(ON_3dPoint(-TRIMMED_HALF_EXTENT,  TRIMMED_HALF_EXTENT, 0.0));
    boundary_points.Append(boundary_points[0]);
    ON_PolylineCurve boundary_curve(boundary_points);
    ON_Brep *brep = ON_BrepTrimmedPlane(ON_Plane::World_xy, boundary_curve);
    if (brep == NULL) {
	bu_log("unable to create trimmed plane for bound test\n");
	return 1;
    }

    ON_PlaneSurface *surface = brep->m_S.Count() == 1 ?
	ON_PlaneSurface::Cast(brep->m_S[0]) : NULL;
    if (surface == NULL ||
	!surface->SetDomain(0, -SUPPORT_HALF_EXTENT, SUPPORT_HALF_EXTENT) ||
	!surface->SetDomain(1, -SUPPORT_HALF_EXTENT, SUPPORT_HALF_EXTENT) ||
	!surface->SetExtents(0, ON_Interval(-SUPPORT_HALF_EXTENT,
					 SUPPORT_HALF_EXTENT)) ||
	!surface->SetExtents(1, ON_Interval(-SUPPORT_HALF_EXTENT,
					 SUPPORT_HALF_EXTENT))) {
	bu_log("unable to enlarge trimmed-plane support surface\n");
	delete brep;
	return 1;
    }
    brep->DestroyRuntimeCache();

    ON_BoundingBox trimmed_bbox;
    int failures = 0;
    if (!brep_GetTrimmedBoundingBox(*brep, trimmed_bbox, false) ||
	!trimmed_bbox.IsValid()) {
	bu_log("unable to calculate trimmed box bound\n");
	failures++;
    } else {
	for (int corner_index = 0;
	     corner_index + 1 < boundary_points.Count();
	     ++corner_index) {
	    if (!trimmed_bbox.IsPointIn(boundary_points[corner_index])) {
		bu_log("trimmed plane bound excludes a boundary corner\n");
		failures++;
	    }
	}
	const ON_3dPoint support_only_point(
	    SUPPORT_HALF_EXTENT * SUPPORT_SAMPLE_FRACTION, 0.0, 0.0);
	if (trimmed_bbox.IsPointIn(support_only_point) ||
	    trimmed_bbox.m_min.x < -TRIMMED_HALF_EXTENT - BOUNDING_TOLERANCE ||
	    trimmed_bbox.m_min.y < -TRIMMED_HALF_EXTENT - BOUNDING_TOLERANCE ||
	    trimmed_bbox.m_min.z < -BOUNDING_TOLERANCE ||
	    trimmed_bbox.m_max.x > TRIMMED_HALF_EXTENT + BOUNDING_TOLERANCE ||
	    trimmed_bbox.m_max.y > TRIMMED_HALF_EXTENT + BOUNDING_TOLERANCE ||
	    trimmed_bbox.m_max.z > BOUNDING_TOLERANCE) {
	    bu_log("trimmed plane bound includes extended support geometry\n");
	    failures++;
	}
    }

    delete brep;
    return failures;
}


int
main(int, char *argv[])
{
    bu_setprogname(argv[0]);

    ON_Sphere sphere(ON_3dPoint::Origin, 10.0);
    ON_Brep *brep = ON_BrepSphere(sphere);
    if (brep == NULL || !brep->IsValid() || !brep->IsSolid()) {
	delete brep;
	return 1;
    }

    int failures = 0;
    failures += check_trimmed_bounding_box();
    failures += check_point(brep, ON_3dPoint(1.0, 2.0, 3.0), true,
	    "interior point");
    failures += check_point(brep, ON_3dPoint(12.0, 0.0, 0.0), false,
	    "exterior point");
    failures += check_point(brep, ON_3dPoint(10.0, 0.0, 0.0), false,
	    "boundary point");

    delete brep;
    return failures;
}
