/*                    H Y P _ B R E P . C P P
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
/** @file hyp_brep.cpp
 *
 * Convert a Hyperboloid of One Sheet to b-rep form
 *
 */

#include "common.h"

#include "raytrace.h"
#include "rtgeom.h"
#include "brep.h"


/**
 * R T _ H Y P _ B R E P
 */
extern "C" void
rt_hyp_brep(ON_Brep **b, const struct rt_db_internal *ip, const struct bn_tol *)
{
    struct rt_hyp_internal *eip;

    RT_CK_DB_INTERNAL(ip);
    eip = (struct rt_hyp_internal *)ip->idb_ptr;
    RT_HYP_CK_MAGIC(eip);

    *b = ON_Brep::New();

    point_t p1_origin, p2_origin;
    ON_3dPoint plane1_origin, plane2_origin;
    ON_3dVector plane_x_dir, plane_y_dir;

    //  First, find planes corresponding to the top and bottom faces - initially
    //  the hyp must be centered around the origin.

    vect_t x_dir, y_dir;
    VSET(x_dir, 1, 0, 0);
    VSET(y_dir, 0, 1, 0);

    VSET(p1_origin, 0, 0, -0.5*MAGNITUDE(eip->hyp_Hi));
    plane1_origin = ON_3dPoint(p1_origin);
    plane_x_dir = ON_3dVector(x_dir);
    plane_y_dir = ON_3dVector(y_dir);
    const ON_Plane* hyp_bottom_plane = new ON_Plane(plane1_origin, plane_x_dir, plane_y_dir);

    VSET(p2_origin, 0, 0, 0.5*MAGNITUDE(eip->hyp_Hi));
    plane2_origin = ON_3dPoint(p2_origin);
    const ON_Plane* hyp_top_plane = new ON_Plane(plane2_origin, plane_x_dir, plane_y_dir);

    // Next, create ellipses in the planes corresponding to the edges of the hyp

    ON_Ellipse* b_ell = new ON_Ellipse(*hyp_bottom_plane, MAGNITUDE(eip->hyp_A), eip->hyp_b);
    ON_NurbsCurve* bcurve = ON_NurbsCurve::New();
    b_ell->GetNurbForm((*bcurve));
    bcurve->SetDomain(0.0, 1.0);

    ON_Ellipse* t_ell = new ON_Ellipse(*hyp_top_plane, MAGNITUDE(eip->hyp_A), eip->hyp_b);
    ON_NurbsCurve* tcurve = ON_NurbsCurve::New();
    t_ell->GetNurbForm((*tcurve));
    tcurve->SetDomain(0.0, 1.0);

    // Generate the bottom cap
    ON_SimpleArray<ON_Curve*> boundary;
    boundary.Append(ON_Curve::Cast(bcurve));
    ON_PlaneSurface* bp = new ON_PlaneSurface();
    bp->m_plane = (*hyp_bottom_plane);
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
    (*b)->FlipFace(bface);
    (*b)->SetTrimIsoFlags(bface);
    boundary.Empty();

    // Generate the top cap
    boundary.Append(ON_Curve::Cast(tcurve));
    ON_PlaneSurface* tp = new ON_PlaneSurface();
    tp->m_plane = (*hyp_top_plane);
    tp->SetDomain(0, -100.0, 100.0);
    tp->SetDomain(1, -100.0, 100.0);
    tp->SetExtents(0, bp->Domain(0));
    tp->SetExtents(1, bp->Domain(1));
    (*b)->m_S.Append(tp);
    int tsi = (*b)->m_S.Count() - 1;
    ON_BrepFace& tface = (*b)->NewFace(tsi);
    (*b)->NewPlanarFaceLoop(tface.m_face_index, ON_BrepLoop::outer, boundary, true);
    ON_BrepLoop* tloop = (*b)->m_L.Last();
    tp->SetDomain(0, tloop->m_pbox.m_min.x, tloop->m_pbox.m_max.x);
    tp->SetDomain(1, tloop->m_pbox.m_min.y, tloop->m_pbox.m_max.y);
    tp->SetExtents(0, bp->Domain(0));
    tp->SetExtents(1, bp->Domain(1));
    (*b)->SetTrimIsoFlags(tface);

    //  Now, the hard part.  Need an elliptical hyperboloic NURBS surface
    //  First step is to create a nurbs curve.

    double param_b = eip->hyp_b * eip->hyp_bnr;
    double param_c = eip->hyp_b - param_b;
    double intercept_calc = param_c*param_c/(param_b + param_c);
    double intercept_dist = param_b + param_c - intercept_calc;
    double intercept_length = intercept_dist - param_b;
    double MX = param_b;
    double MP = MX + intercept_length;
    double w = (MX/MP)/(1-MX/MP);

    point_t ep1, ep2, ep3;
    VSET(ep1, -eip->hyp_b, 0, 0.5*MAGNITUDE(eip->hyp_Hi));
    VSET(ep2, -MX, 0, 0);
    VSET(ep3, -eip->hyp_b, 0, -0.5*MAGNITUDE(eip->hyp_Hi));

    bu_log("pt1: %f, %f, %f\n", ep1[0], ep1[1], ep1[2]);
    bu_log("pt2: %f, %f, %f\n", ep2[0], ep2[1], ep2[2]);
    bu_log("pt3: %f, %f, %f\n", ep3[0], ep3[1], ep3[2]);

    ON_3dPoint onp1 = ON_3dPoint(ep1);
    ON_3dPoint onp2 = ON_3dPoint(ep2);
    ON_3dPoint onp3 = ON_3dPoint(ep3);

    ON_3dPointArray cpts(3);
    cpts.Append(onp1);
    cpts.Append(onp2);
    cpts.Append(onp3);
    ON_BezierCurve *bezcurve = new ON_BezierCurve(cpts);
    bezcurve->MakeRational();
    bezcurve->SetWeight(1, w);

    ON_NurbsCurve* tnurbscurve = ON_NurbsCurve::New();
    bezcurve->GetNurbForm(*tnurbscurve);

    ON_3dPoint revpnt1 = ON_3dPoint(p1_origin);
    ON_3dPoint revpnt2 = ON_3dPoint(p2_origin);

    ON_Line revaxis = ON_Line(revpnt1, revpnt2);
    ON_RevSurface* hyp_surf = ON_RevSurface::New();
    hyp_surf->m_curve = tnurbscurve;
    hyp_surf->m_axis = revaxis;
    hyp_surf->m_angle = ON_Interval(0, 2*ON_PI);

    // Get the NURBS form of the surface
    ON_NurbsSurface *hypcurvedsurf = ON_NurbsSurface::New();
    hyp_surf->GetNurbForm(*hypcurvedsurf, 0.0);

    for (int i = 0; i < hypcurvedsurf->CVCount(0); i++) {
	for (int j = 0; j < hypcurvedsurf->CVCount(1); j++) {
	    point_t cvpt;
	    ON_4dPoint ctrlpt;
	    hypcurvedsurf->GetCV(i, j, ctrlpt);
	    VSET(cvpt, ctrlpt.x * MAGNITUDE(eip->hyp_A)/eip->hyp_b, ctrlpt.y, ctrlpt.z);
	    ON_4dPoint newpt = ON_4dPoint(cvpt[0], cvpt[1], cvpt[2], ctrlpt.w);
	    hypcurvedsurf->SetCV(i, j, newpt);
	}
    }

    (*b)->m_S.Append(hypcurvedsurf);
    int surfindex = (*b)->m_S.Count();
    ON_BrepFace& face = (*b)->NewFace(surfindex - 1);
    (*b)->FlipFace(face);
    int faceindex = (*b)->m_F.Count();
    (*b)->NewOuterLoop(faceindex-1);

}


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
