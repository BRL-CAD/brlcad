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


static ON_Brep *
make_box(double half_extent)
{
    const ON_3dPoint corners[8] = {
	ON_3dPoint(-half_extent, -half_extent, -half_extent),
	ON_3dPoint( half_extent, -half_extent, -half_extent),
	ON_3dPoint( half_extent,  half_extent, -half_extent),
	ON_3dPoint(-half_extent,  half_extent, -half_extent),
	ON_3dPoint(-half_extent, -half_extent,  half_extent),
	ON_3dPoint( half_extent, -half_extent,  half_extent),
	ON_3dPoint( half_extent,  half_extent,  half_extent),
	ON_3dPoint(-half_extent,  half_extent,  half_extent)
    };
    return ON_BrepBox(corners);
}


static int
check_vertex_crossing_evaluation()
{
    static const double BOX_HALF_EXTENT = 10.0;
    static const double SPHERE_RADIUS = 10.0;
    const ON_3dPoint sphere_center(BOX_HALF_EXTENT, BOX_HALF_EXTENT, 0.0);
    ON_Brep *box = make_box(BOX_HALF_EXTENT);
    ON_Brep *sphere = ON_BrepSphere(ON_Sphere(sphere_center, SPHERE_RADIUS));
    if (box == NULL || sphere == NULL || !box->IsValid() || !sphere->IsValid()) {
	delete box;
	delete sphere;
	return 1;
    }

    ON_Brep result;
    const int status = ON_Boolean(&result, box, sphere, BOOLEAN_INTERSECT);
    delete box;
    delete sphere;


    if (status == BREP_BOOLEAN_SUCCESS) {
	if (result.m_F.Count() > 0 && result.IsValid() && result.IsSolid()) {
	    return 0;
	}
	bu_log("vertex-crossing Boolean success produced an invalid result\n");
	return 1;
    }

    if (status == BREP_BOOLEAN_INDETERMINATE && result.m_F.Count() == 0) {
	return 0;
    }

    bu_log("vertex-crossing Boolean evaluation did not fail safely: status=%d faces=%d\n",
	   status, result.m_F.Count());
	return 1;
}


int
main(int, char *argv[])
{
    bu_setprogname(argv[0]);
    return check_vertex_crossing_evaluation();
}
