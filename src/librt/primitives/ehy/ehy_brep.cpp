/*                    E H Y _ B R E P . C P P
 * BRL-CAD
 *
 * Copyright (c) 2008-2011 United States Government as represented by
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
/** @file ehy_brep.cpp
 *
 * Convert a Right Parabolic Cylinder to b-rep form
 *
 */

#include "common.h"

#include "raytrace.h"
#include "rtgeom.h"
#include "brep.h"


/**
 * R T _ E H Y _ B R E P
 */
extern "C" void
rt_ehy_brep(ON_Brep **b, const struct rt_db_internal *ip, const struct bn_tol *)
{
    struct rt_ehy_internal *eip;

    RT_CK_DB_INTERNAL(ip);
    eip = (struct rt_ehy_internal *)ip->idb_ptr;
    RT_EHY_CK_MAGIC(eip);

    ON_TextLog dump_to_stdout;
    ON_TextLog* dump = &dump_to_stdout;

    point_t p1_origin;
    ON_3dPoint plane1_origin, plane2_origin;
    ON_3dVector plane_x_dir, plane_y_dir;

    //  First, find plane in 3 space corresponding to the bottom face of the EPA.

    vect_t x_dir, y_dir;

    VMOVE(x_dir, eip->ehy_Au);
    VCROSS(y_dir, eip->ehy_Au, eip->ehy_H);
    VUNITIZE(y_dir);

    VMOVE(p1_origin, eip->ehy_V);
    plane1_origin = ON_3dPoint(p1_origin);
    plane_x_dir = ON_3dVector(x_dir);
    plane_y_dir = ON_3dVector(y_dir);
    const ON_Plane* ehy_bottom_plane = new ON_Plane(plane1_origin, plane_x_dir, plane_y_dir);

    //  Next, create an ellipse in the plane corresponding to the edge of the ehy.

    ON_Ellipse* ellipse1 = new ON_Ellipse(*ehy_bottom_plane, eip->ehy_r1, eip->ehy_r2);
    ON_NurbsCurve* ellcurve1 = ON_NurbsCurve::New();
    ellipse1->GetNurbForm((*ellcurve1));
    ellcurve1->SetDomain(0.0, 1.0);

    // Generate the bottom cap
    ON_SimpleArray<ON_Curve*> boundary;
    boundary.Append(ON_Curve::Cast(ellcurve1));
    ON_PlaneSurface* bp = new ON_PlaneSurface();
    bp->m_plane = (*ehy_bottom_plane);
    bp->SetDomain(0, -100.0, 100.0);
    bp->SetDomain(1, -100.0, 100.0);
    bp->SetExtents(0, bp->Domain(0));
    bp->SetExtents(1, bp->Domain(1));
    (*b)->m_S.Append(bp);
    const int bsi = (*b)->m_S.Count() - 1;
    ON_BrepFace& bface = (*b)->NewFace(bsi);
    (*b)->NewPlanarFaceLoop(bface.m_face_index, ON_BrepLoop::outer, boundary, true);
    const ON_BrepLoop* bloop = (*b)->m_L.Last();
    bp->SetDomain(0, bloop->m_pbox.m_min.x, bloop->m_pbox.m_max.x);
    bp->SetDomain(1, bloop->m_pbox.m_min.y, bloop->m_pbox.m_max.y);
    bp->SetExtents(0, bp->Domain(0));
    bp->SetExtents(1, bp->Domain(1));
    (*b)->SetTrimIsoFlags(bface);

    //  Now, the hard part.  Need an elliptical hyperboloic NURBS surface
    //  First step is to create a nurbs curve.

    double intercept_calc = (eip->ehy_c)*(eip->ehy_c)/(MAGNITUDE(eip->ehy_H) + eip->ehy_c);
    double intercept_dist = MAGNITUDE(eip->ehy_H) + eip->ehy_c - intercept_calc;
    double intercept_length = intercept_dist - MAGNITUDE(eip->ehy_H);
    double MX = MAGNITUDE(eip->ehy_H);
    double MP = MX + intercept_length;
    double w = (MX/MP)/(1-MX/MP);

    point_t ep1, ep2, ep3;
    VSET(ep1, -eip->ehy_r1, 0, 0);
    VSET(ep2, 0, 0, w*intercept_dist);
    VSET(ep3, eip->ehy_r1, 0, 0);
    ON_3dPoint onp1 = ON_3dPoint(ep1);
    ON_3dPoint onp2 = ON_3dPoint(ep2);
    ON_3dPoint onp3 = ON_3dPoint(ep3);

    ON_3dPointArray cpts(3);
    cpts.Append(onp1);
    cpts.Append(onp2);
    cpts.Append(onp3);
    ON_BezierCurve *bcurve = new ON_BezierCurve(cpts);
    bcurve->MakeRational();
    bcurve->SetWeight(1, w);

    ON_NurbsCurve* tnurbscurve = ON_NurbsCurve::New();
    bcurve->GetNurbForm(*tnurbscurve);
    ON_NurbsCurve* hypbnurbscurve = ON_NurbsCurve::New();
    const ON_Interval subinterval = ON_Interval(0, 0.5);
    tnurbscurve->GetNurbForm(*hypbnurbscurve, 0.0, &subinterval);

    hypbnurbscurve->Dump(*dump);

    // Next, rotate that curve around the height vector.

    point_t revpoint1, revpoint2;
    VSET(revpoint1, 0, 0, 0);
    VSET(revpoint2, 0, 0, MX);
    ON_3dPoint rpnt1 = ON_3dPoint(revpoint1);
    ON_3dPoint rpnt2 = ON_3dPoint(revpoint2);

    ON_Line revaxis = ON_Line(rpnt1, rpnt2);
    ON_RevSurface* hyp_surf = ON_RevSurface::New();
    hyp_surf->m_curve = hypbnurbscurve;
    hyp_surf->m_axis = revaxis;
    hyp_surf->m_angle = ON_Interval(0, 2*ON_PI);

    // Get the NURBS form of the surface
    ON_NurbsSurface *ehycurvedsurf = ON_NurbsSurface::New();
    hyp_surf->GetNurbForm(*ehycurvedsurf, 0.0);
    ehycurvedsurf->Dump(*dump);

    // Last but not least, scale the control points of the
    // resulting surface to map to the shorter axis.

    for (int i = 0; i < ehycurvedsurf->CVCount(0); i++) {
	for (int j = 0; j < ehycurvedsurf->CVCount(1); j++) {
	    point_t cvpt;
	    ON_4dPoint ctrlpt;
	    ehycurvedsurf->GetCV(i, j, ctrlpt);
	    VSET(cvpt, ctrlpt.x, ctrlpt.y * eip->ehy_r2/eip->ehy_r1, ctrlpt.z);
	    ON_4dPoint newpt = ON_4dPoint(cvpt[0], cvpt[1], cvpt[2], ctrlpt.w);
	    ehycurvedsurf->SetCV(i, j, newpt);
	}
    }


    bu_log("Valid nurbs surface: %d\n", ehycurvedsurf->IsValid(dump));
    ehycurvedsurf->Dump(*dump);

    (*b)->m_S.Append(ehycurvedsurf);
    int surfindex = (*b)->m_S.Count();
    ON_BrepFace& face = (*b)->NewFace(surfindex - 1);
    int faceindex = (*b)->m_F.Count();
    (*b)->NewOuterLoop(faceindex-1);
    bu_log("Valid brep face: %d\n", face.IsValid(dump));
}


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
