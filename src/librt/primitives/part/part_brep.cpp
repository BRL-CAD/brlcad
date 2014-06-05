/*                    P A R T _ B R E P . C P P
 * BRL-CAD
 *
 * Copyright (c) 2012-2014 United States Government as represented by
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
/** @file part_brep.cpp
 *
 * Convert a PART (conical particle solid) to b-rep form
 *
 */

#include "common.h"

#include "raytrace.h"
#include "rtgeom.h"
#include "brep.h"


extern "C" void
rt_part_brep(ON_Brep **b, const struct rt_db_internal *ip, const struct bn_tol *)
{
    struct rt_part_internal *pip;

    RT_CK_DB_INTERNAL(ip);
    pip = (struct rt_part_internal *)ip->idb_ptr;
    RT_PART_CK_MAGIC(pip);

    fastf_t temp = asin((pip->part_vrad-pip->part_hrad)/MAGNITUDE(pip->part_H));
    point_t origin;
    VSET(origin, 0.0, 0.0, 0.0);

    // First, create the body of the particle. It should be a revolution of a line.
    // The body should be either a cylinder or a truncated cone which is tangent to
    // the two hemispheres.
    point_t VaddH;
    vect_t r;
    VADD2(VaddH, pip->part_V, pip->part_H);
    bn_vec_ortho(r, pip->part_H);
    VUNITIZE(r);
    point_t startpoint, endpoint;
    VJOIN2(startpoint, pip->part_V, pip->part_vrad*cos(temp), r, sin(temp)*pip->part_vrad/MAGNITUDE(pip->part_H), pip->part_H);
    VJOIN2(endpoint, VaddH, pip->part_hrad*cos(temp), r, sin(temp)*pip->part_hrad/MAGNITUDE(pip->part_H), pip->part_H);
    ON_Line line = ON_Line(ON_3dPoint(startpoint), ON_3dPoint(endpoint));
    ON_LineCurve *lcurve = new ON_LineCurve(line);

    point_t revpoint1, revpoint2;
    VMOVE(revpoint1, pip->part_V);
    VMOVE(revpoint2, VaddH);
    ON_3dPoint rpnt1 = ON_3dPoint(revpoint1);
    ON_3dPoint rpnt2 = ON_3dPoint(revpoint2);
    ON_Line revaxis(rpnt1, rpnt2);

    ON_RevSurface* part_surf = ON_RevSurface::New();
    part_surf->m_curve = lcurve;
    part_surf->m_axis = revaxis;
    part_surf->m_angle = ON_Interval(0, 2*ON_PI);
    ON_NurbsSurface *part_nurbs_surf = ON_NurbsSurface::New();
    part_surf->GetNurbForm(*part_nurbs_surf, 0.0);
    delete part_surf;

    if (!EQUAL(fabs(temp), ON_PI/2.0)) {
	(*b)->m_S.Append(part_nurbs_surf);
	int surfindex = (*b)->m_S.Count();
	ON_BrepFace& face = (*b)->NewFace(surfindex - 1);
	(*b)->FlipFace(face);
	int faceindex = (*b)->m_F.Count();
	(*b)->NewOuterLoop(faceindex-1);
    }

    // Then, create the v-hemisphere.
    vect_t minusH;
    VREVERSE(minusH, pip->part_H);
    ON_Plane vplane = ON_Plane(ON_3dPoint(origin), ON_3dPoint(minusH), ON_3dPoint(r));
    ON_Circle vcircle = ON_Circle(vplane, pip->part_vrad);
    ON_NurbsCurve *v_curve = ON_NurbsCurve::New();
    const ON_Interval subinterval = ON_Interval(0, ON_PI/2.0 + temp);
    ON_Arc v_arc(vcircle, subinterval);
    v_arc.Translate(ON_3dVector(pip->part_V));
    v_arc.GetNurbForm(*v_curve);
    ON_RevSurface* v_sph_surf = ON_RevSurface::New();
    v_sph_surf->m_curve = v_curve;
    v_sph_surf->m_axis = revaxis;
    v_sph_surf->m_angle = ON_Interval(0, 2*ON_PI);
    ON_NurbsSurface *v_nurbs_surf = ON_NurbsSurface::New();
    v_sph_surf->GetNurbForm(*v_nurbs_surf, 0.0);
    delete v_sph_surf;

    if (!EQUAL(temp, -ON_PI/2.0)) {
	(*b)->m_S.Append(v_nurbs_surf);
	int surfindex = (*b)->m_S.Count();
	ON_BrepFace& face = (*b)->NewFace(surfindex - 1);
	(*b)->FlipFace(face);
	int faceindex = (*b)->m_F.Count();
	(*b)->NewOuterLoop(faceindex-1);
    }

    // Last, the h-hemisphere.
    ON_Plane hplane = ON_Plane(ON_3dPoint(origin), ON_3dPoint(pip->part_H), ON_3dPoint(r));
    ON_Circle hcircle = ON_Circle(hplane, pip->part_hrad);
    ON_NurbsCurve *h_curve = ON_NurbsCurve::New();
    const ON_Interval subinterval2 = ON_Interval(ON_PI/2.0 - temp, 0);
    ON_Arc h_arc(hcircle, subinterval2);
    h_arc.Translate(ON_3dVector(VaddH));
    h_arc.GetNurbForm(*h_curve);
    ON_RevSurface* h_sph_surf = ON_RevSurface::New();
    h_sph_surf->m_curve = h_curve;
    h_sph_surf->m_axis = revaxis;
    h_sph_surf->m_angle = ON_Interval(0, 2*ON_PI);
    ON_NurbsSurface *h_nurbs_surf = ON_NurbsSurface::New();
    h_sph_surf->GetNurbForm(*h_nurbs_surf, 0.0);
    delete h_sph_surf;

    if (!EQUAL(temp, ON_PI/2.0)) {
	(*b)->m_S.Append(h_nurbs_surf);
	int surfindex = (*b)->m_S.Count();
	ON_BrepFace& face2 = (*b)->NewFace(surfindex - 1);
	(*b)->FlipFace(face2);
	int faceindex = (*b)->m_F.Count();
	(*b)->NewOuterLoop(faceindex-1);
    }
}


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
