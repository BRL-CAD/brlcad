/*                   I N T E R S E C T . C P P
 * BRL-CAD
 *
 * Copyright (c) 2020-2021 United States Government as represented by
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
/** @file libged/brep/intersect.cpp
 *
 * Calculate intersections between brep object subcomponents.
 *
 */

#include "common.h"

#include "bu/color.h"
#include "raytrace.h"
#include "./ged_brep.h"

int
brep_intersect_point_point(struct rt_db_internal *intern1, struct rt_db_internal *intern2, int i, int j)
{
    RT_CK_DB_INTERNAL(intern1);
    RT_CK_DB_INTERNAL(intern2);
    struct rt_brep_internal *bi1, *bi2;
    bi1 = (struct rt_brep_internal *)intern1->idb_ptr;
    bi2 = (struct rt_brep_internal *)intern2->idb_ptr;
    RT_BREP_CK_MAGIC(bi1);
    RT_BREP_CK_MAGIC(bi2);

    const ON_Brep *brep1 = bi1->brep;
    const ON_Brep *brep2 = bi2->brep;

    if (i < 0 || i >= brep1->m_V.Count() || j < 0 || j >= brep2->m_V.Count()) {
	bu_log("Out of range: \n");
	bu_log("\t0 <= i <= %d\n", brep1->m_V.Count() - 1);
	bu_log("\t0 <= j <= %d\n", brep2->m_V.Count() - 1);
	return -1;
    }

    ON_ClassArray<ON_PX_EVENT> events;
    if (ON_Intersect(brep1->m_V[i].Point(), brep2->m_V[j].Point(), events)) {
	for (int k = 0; k < events.Count(); k++) {
	    ON_wString wstr;
	    ON_TextLog textlog(wstr);
	    events[k].Dump(textlog);
	    ON_String str = ON_String(wstr);
	    bu_log("Intersection event %d:\n %s", k + 1, str.Array());
	}
    } else {
	bu_log("No intersection.\n");
    }

    return 0;
}


int
brep_intersect_point_curve(struct rt_db_internal *intern1, struct rt_db_internal *intern2, int i, int j)
{
    RT_CK_DB_INTERNAL(intern1);
    RT_CK_DB_INTERNAL(intern2);
    struct rt_brep_internal *bi1, *bi2;
    bi1 = (struct rt_brep_internal *)intern1->idb_ptr;
    bi2 = (struct rt_brep_internal *)intern2->idb_ptr;
    RT_BREP_CK_MAGIC(bi1);
    RT_BREP_CK_MAGIC(bi2);

    const ON_Brep *brep1 = bi1->brep;
    const ON_Brep *brep2 = bi2->brep;

    if (i < 0 || i >= brep1->m_V.Count() || j < 0 || j >= brep2->m_C3.Count()) {
	bu_log("Out of range: \n");
	bu_log("\t0 <= i <= %d\n", brep1->m_V.Count() - 1);
	bu_log("\t0 <= j <= %d\n", brep2->m_C3.Count() - 1);
	return -1;
    }

    ON_ClassArray<ON_PX_EVENT> events;
    if (ON_Intersect(brep1->m_V[i].Point(), *(brep2->m_C3[j]), events)) {
	for (int k = 0; k < events.Count(); k++) {
	    ON_wString wstr;
	    ON_TextLog textlog(wstr);
	    events[k].Dump(textlog);
	    ON_String str = ON_String(wstr);
	    bu_log("Intersection event %d:\n %s", k + 1, str.Array());
	}
    } else {
	bu_log("No intersection.\n");
    }

    return 0;
}


int
brep_intersect_point_surface(struct rt_db_internal *intern1, struct rt_db_internal *intern2, int i, int j)
{
    RT_CK_DB_INTERNAL(intern1);
    RT_CK_DB_INTERNAL(intern2);
    struct rt_brep_internal *bi1, *bi2;
    bi1 = (struct rt_brep_internal *)intern1->idb_ptr;
    bi2 = (struct rt_brep_internal *)intern2->idb_ptr;
    RT_BREP_CK_MAGIC(bi1);
    RT_BREP_CK_MAGIC(bi2);

    const ON_Brep *brep1 = bi1->brep;
    const ON_Brep *brep2 = bi2->brep;

    if (i < 0 || i >= brep1->m_V.Count() || j < 0 || j >= brep2->m_S.Count()) {
	bu_log("Out of range: \n");
	bu_log("\t0 <= i <= %d\n", brep1->m_V.Count() - 1);
	bu_log("\t0 <= j <= %d\n", brep2->m_S.Count() - 1);
	return -1;
    }

    ON_ClassArray<ON_PX_EVENT> events;
    if (ON_Intersect(brep1->m_V[i].Point(), *(brep2->m_S[j]), events)) {
	for (int k = 0; k < events.Count(); k++) {
	    ON_wString wstr;
	    ON_TextLog textlog(wstr);
	    events[k].Dump(textlog);
	    ON_String str = ON_String(wstr);
	    bu_log("Intersection event %d:\n %s", k + 1, str.Array());
	}
    } else {
	bu_log("No intersection.\n");
    }

    return 0;
}


int
brep_intersect_curve_curve(struct rt_db_internal *intern1, struct rt_db_internal *intern2, int i, int j)
{
    RT_CK_DB_INTERNAL(intern1);
    RT_CK_DB_INTERNAL(intern2);
    struct rt_brep_internal *bi1, *bi2;
    bi1 = (struct rt_brep_internal *)intern1->idb_ptr;
    bi2 = (struct rt_brep_internal *)intern2->idb_ptr;
    RT_BREP_CK_MAGIC(bi1);
    RT_BREP_CK_MAGIC(bi2);

    const ON_Brep *brep1 = bi1->brep;
    const ON_Brep *brep2 = bi2->brep;

    if (i < 0 || i >= brep1->m_C3.Count() || j < 0 || j >= brep2->m_C3.Count()) {
	bu_log("Out of range: \n");
	bu_log("\t0 <= i <= %d\n", brep1->m_C3.Count() - 1);
	bu_log("\t0 <= j <= %d\n", brep2->m_C3.Count() - 1);
	return -1;
    }

    ON_SimpleArray<ON_X_EVENT> events;
    if (ON_Intersect(brep1->m_C3[i], brep2->m_C3[j], events)) {
	for (int k = 0; k < events.Count(); k++) {
	    ON_wString wstr;
	    ON_TextLog textlog(wstr);
	    events[k].Dump(textlog);
	    ON_String str = ON_String(wstr);
	    bu_log("Intersection event %d:\n %s", k + 1, str.Array());
	}
    } else {
	bu_log("No intersection.\n");
    }

    return 0;
}


int
brep_intersect_curve_surface(struct rt_db_internal *intern1, struct rt_db_internal *intern2, int i, int j)
{
    RT_CK_DB_INTERNAL(intern1);
    RT_CK_DB_INTERNAL(intern2);
    struct rt_brep_internal *bi1, *bi2;
    bi1 = (struct rt_brep_internal *)intern1->idb_ptr;
    bi2 = (struct rt_brep_internal *)intern2->idb_ptr;
    RT_BREP_CK_MAGIC(bi1);
    RT_BREP_CK_MAGIC(bi2);

    const ON_Brep *brep1 = bi1->brep;
    const ON_Brep *brep2 = bi2->brep;

    if (i < 0 || i >= brep1->m_C3.Count() || j < 0 || j >= brep2->m_S.Count()) {
	bu_log("Out of range: \n");
	bu_log("\t0 <= i <= %d\n", brep1->m_C3.Count() - 1);
	bu_log("\t0 <= j <= %d\n", brep2->m_S.Count() - 1);
	return -1;
    }

    ON_SimpleArray<ON_X_EVENT> events;
    if (ON_Intersect(brep1->m_C3[i], brep2->m_S[j], events)) {
	for (int k = 0; k < events.Count(); k++) {
	    ON_wString wstr;
	    ON_TextLog textlog(wstr);
	    events[k].Dump(textlog);
	    ON_String str = ON_String(wstr);
	    bu_log("Intersection event %d:\n %s", k + 1, str.Array());
	}
    } else {
	bu_log("No intersection.\n");
    }

    return 0;
}


int
brep_intersect_surface_surface(struct rt_db_internal *intern1, struct rt_db_internal *intern2, int i, int j, struct bv_vlblock *vbp)
{
    RT_CK_DB_INTERNAL(intern1);
    RT_CK_DB_INTERNAL(intern2);
    struct rt_brep_internal *bi1, *bi2;
    bi1 = (struct rt_brep_internal *)intern1->idb_ptr;
    bi2 = (struct rt_brep_internal *)intern2->idb_ptr;
    RT_BREP_CK_MAGIC(bi1);
    RT_BREP_CK_MAGIC(bi2);
    struct bu_list *vlfree = intern1->idb_vlfree;

    const ON_Brep *brep1 = bi1->brep;
    const ON_Brep *brep2 = bi2->brep;

    ON_NurbsSurface surf1;
    ON_NurbsSurface surf2;

    if (i < 0 || i >= brep1->m_S.Count() || j < 0 || j >= brep2->m_S.Count()) {
	bu_log("Out of range: \n");
	bu_log("\t0 <= i <= %d\n", brep1->m_S.Count() - 1);
	bu_log("\t0 <= j <= %d\n", brep2->m_S.Count() - 1);
	return -1;
    }

    brep1->m_S[i]->GetNurbForm(surf1);
    brep2->m_S[j]->GetNurbForm(surf2);

    ON_ClassArray<ON_SSX_EVENT> events;
    if (ON_Intersect(&surf1, &surf2, events) < 0) {
	bu_log("Intersection failed\n");
	return -1;
    }

    plotsurface(surf1, vlfree, vbp, 100, 10, PURERED);
    plotsurface(surf2, vlfree, vbp, 100, 10, BLUE);

    // Plot the intersection curves (or points) (3D and 2D)
    for (int k = 0; k < events.Count(); k++) {
	switch (events[k].m_type) {
	    case ON_SSX_EVENT::ssx_overlap:
	    case ON_SSX_EVENT::ssx_tangent:
	    case ON_SSX_EVENT::ssx_transverse:
		plotcurveonsurface(events[k].m_curveA, &surf1, vlfree, vbp, 1000, PEACH);
		plotcurveonsurface(events[k].m_curveB, &surf2, vlfree, vbp, 1000, DARKVIOLET);
		plotcurve(*(events[k].m_curve3d), vlfree, vbp, 1000, GREEN);
		break;
	    case ON_SSX_EVENT::ssx_tangent_point:
	    case ON_SSX_EVENT::ssx_transverse_point:
		plotpoint(surf1.PointAt(events[k].m_pointA.x, events[k].m_pointB.y), vlfree, vbp, PEACH);
		plotpoint(surf2.PointAt(events[k].m_pointB.x, events[k].m_pointB.y), vlfree, vbp, DARKVIOLET);
		plotpoint(events[k].m_point3d, vlfree, vbp, GREEN);
		break;
	    default:
		break;
	}
    }

    return 0;
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
