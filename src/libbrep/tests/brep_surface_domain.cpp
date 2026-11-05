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
#include "brep/intersect.h"


static const double SURFACE_DOMAIN_TEST_TOLERANCE = 1.0e-6;


static int
check_restricted_surface_domain()
{
    ON_PlaneSurface surface(ON_xy_plane);
    surface.SetDomain(0, 0.0, 1.0);
    surface.SetDomain(1, 0.0, 1.0);
    surface.SetExtents(0, surface.Domain(0));
    surface.SetExtents(1, surface.Domain(1));

    const ON_Interval u_domain(0.0, 0.5);
    const ON_Interval v_domain(0.0, 0.5);
    ON_ClassArray<ON_PX_EVENT> events;
    const ON_3dPoint outside = surface.PointAt(0.75, 0.25);
    if (ON_Intersect(outside, surface, events,
	SURFACE_DOMAIN_TEST_TOLERANCE, &u_domain, &v_domain) ||
	events.Count() != 0)
    {
	bu_log("restricted point-surface intersection accepted an outside point\n");
	return 1;
    }

    const ON_3dPoint inside = surface.PointAt(0.25, 0.25);
    if (!ON_Intersect(inside, surface, events,
	SURFACE_DOMAIN_TEST_TOLERANCE, &u_domain, &v_domain) ||
	events.Count() != 1)
    {
	bu_log("restricted point-surface intersection rejected an inside point\n");
	return 1;
    }

    const ON_PX_EVENT &event = events[0];
    if (event.m_b.x < u_domain.Min() - SURFACE_DOMAIN_TEST_TOLERANCE ||
	event.m_b.x > u_domain.Max() + SURFACE_DOMAIN_TEST_TOLERANCE ||
	event.m_b.y < v_domain.Min() - SURFACE_DOMAIN_TEST_TOLERANCE ||
	event.m_b.y > v_domain.Max() + SURFACE_DOMAIN_TEST_TOLERANCE)
    {
	bu_log("restricted point-surface intersection returned outside parameters\n");
	return 1;
    }

    return 0;
}


int
main(int, char *argv[])
{
    bu_setprogname(argv[0]);
    return check_restricted_surface_domain();
}
