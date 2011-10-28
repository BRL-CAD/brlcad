/*                    T G C _ B R E P . C P P
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
/** @file tgc_brep.cpp
 *
 * Convert a Truncated General Cone to b-rep form
 *
 */

#include "common.h"

#include "raytrace.h"
#include "rtgeom.h"
#include "brep.h"


/**
 * R T _ T G C _ B R E P
 */
extern "C" void
rt_tgc_brep(ON_Brep **b, const struct rt_db_internal *ip, const struct bn_tol *UNUSED(tol))
{
    struct rt_tgc_internal *eip;

    RT_CK_DB_INTERNAL(ip);
    eip = (struct rt_tgc_internal *)ip->idb_ptr;
    RT_TGC_CK_MAGIC(eip);

    point_t p1_origin, p2_origin;
    ON_3dPoint plane1_origin, plane2_origin;
    ON_3dVector plane1_x_dir, plane1_y_dir, plane2_x_dir, plane2_y_dir;

    double ell1_axis_len_1, ell1_axis_len_2, ell2_axis_len_1, ell2_axis_len_2;

    // First, find planes in 3 space with x and y axes along the axis
    // of the ellipses defining the top and bottom of the TGC, with
    // coordinate origins at the center of the ellipses.  This
    // information will be needed for the ruled surface definition
    // (for the sides) and also for the trimmed planes needed for the
    // top and bottom of the volume.

    vect_t tmp, x1_dir, y1_dir, x2_dir, y2_dir;

    VMOVE(x1_dir, eip->a);
    VMOVE(y1_dir, eip->b);

    VMOVE(x2_dir, eip->c);
    VMOVE(y2_dir, eip->d);

    VMOVE(p1_origin, eip->v);
    VMOVE(tmp, eip->v);
    VADD2(p2_origin, tmp, eip->h);
    plane1_origin = ON_3dPoint(p1_origin);
    plane1_x_dir = ON_3dVector(x1_dir);
    plane1_y_dir = ON_3dVector(y1_dir);
    plane2_origin = ON_3dPoint(p2_origin);
    plane2_x_dir = ON_3dVector(x2_dir);
    plane2_y_dir = ON_3dVector(y2_dir);

    const ON_Plane* ell1_plane = new ON_Plane(plane1_origin, plane1_x_dir, plane1_y_dir);
    const ON_Plane* ell2_plane = new ON_Plane(plane2_origin, plane2_x_dir, plane2_y_dir);

    // Once the planes have been created, create the ellipses within
    // the planes.
    ell1_axis_len_1 = MAGNITUDE(eip->a);
    ell1_axis_len_2 = MAGNITUDE(eip->b);
    ell2_axis_len_1 = MAGNITUDE(eip->c);
    ell2_axis_len_2 = MAGNITUDE(eip->d);

    ON_Ellipse* ellipse1 = new ON_Ellipse(*ell1_plane, ell1_axis_len_1, ell1_axis_len_2);
    ON_Ellipse* ellipse2 = new ON_Ellipse(*ell2_plane, ell2_axis_len_1, ell2_axis_len_2);

    // Generate an ON_Curves from the ellipses
    ON_NurbsCurve* ellcurve1 = ON_NurbsCurve::New();
    ellipse1->GetNurbForm((*ellcurve1));
    ON_NurbsCurve* ellcurve2 = ON_NurbsCurve::New();
    ellipse2->GetNurbForm((*ellcurve2));
    ellcurve1->SetDomain(0.0, 1.0);
    ellcurve2->SetDomain(0.0, 1.0);

    // Create the side surface with
    // ON_NurbsSurface::CreateRuledSurface and the top and bottom
    // planes by using the ellipses as outer trimming curves - define
    // UV surfaces for the top and bottom such that they contain the
    // ellipses.
    ON_SimpleArray<ON_Curve*> bottomboundary;
    bottomboundary.Append(ON_Curve::Cast(ellcurve1));
    ON_PlaneSurface* bp = new ON_PlaneSurface();
    bp->m_plane = (*ell1_plane);
    bp->SetDomain(0, -100.0, 100.0);
    bp->SetDomain(1, -100.0, 100.0);
    bp->SetExtents(0, bp->Domain(0));
    bp->SetExtents(1, bp->Domain(1));
    const int bsi = (*b)->AddSurface(bp);
    ON_BrepFace& bface = (*b)->NewFace(bsi);
    (*b)->NewPlanarFaceLoop(bface.m_face_index, ON_BrepLoop::outer, bottomboundary, true);
    const ON_BrepLoop* bloop = (*b)->m_L.Last();
    bp->SetDomain(0, bloop->m_pbox.m_min.x, bloop->m_pbox.m_max.x);
    bp->SetDomain(1, bloop->m_pbox.m_min.y, bloop->m_pbox.m_max.y);
    bp->SetExtents(0, bp->Domain(0));
    bp->SetExtents(1, bp->Domain(1));
    (*b)->SetTrimIsoFlags(bface);
    (*b)->FlipFace(bface);

    ON_SimpleArray<ON_Curve*> topboundary;
    topboundary.Append(ON_Curve::Cast(ellcurve2));
    ON_PlaneSurface* tp = new ON_PlaneSurface();
    tp->m_plane = (*ell2_plane);
    tp->SetDomain(0, -100.0, 100.0);
    tp->SetDomain(1, -100.0, 100.0);
    tp->SetExtents(0, tp->Domain(0));
    tp->SetExtents(1, tp->Domain(1));
    const int tsi = (*b)->AddSurface(tp);
    ON_BrepFace& tface = (*b)->NewFace(tsi);
    (*b)->NewPlanarFaceLoop(tface.m_face_index, ON_BrepLoop::outer, topboundary, true);
    const ON_BrepLoop* tloop = (*b)->m_L.Last();
    tp->SetDomain(0, tloop->m_pbox.m_min.x, tloop->m_pbox.m_max.x);
    tp->SetDomain(1, tloop->m_pbox.m_min.y, tloop->m_pbox.m_max.y);
    tp->SetExtents(0, tp->Domain(0));
    tp->SetExtents(1, tp->Domain(1));
    (*b)->SetTrimIsoFlags(tface);


    // Need to use NewRuledEdge here, which means valid edges need to
    // be created using the ellipses

    int ell1ind = (*b)->AddEdgeCurve(ellcurve1);
    int ell2ind = (*b)->AddEdgeCurve(ellcurve2);
    ON_BrepVertex& bottomvert1 = (*b)->NewVertex(ellcurve1->PointAt(0), SMALL_FASTF);
    bottomvert1.m_tolerance = 0.0;
    int vert1ind = (*b)->m_V.Count() - 1;
    ON_BrepVertex& topvert1 = (*b)->NewVertex(ellcurve2->PointAt(0), SMALL_FASTF);
    topvert1.m_tolerance = 0.0;
    int vert2ind = (*b)->m_V.Count() - 1;
    ON_BrepEdge& bottomedge = (*b)->NewEdge((*b)->m_V[vert1ind], (*b)->m_V[vert1ind], ell1ind);
    bottomedge.m_tolerance = 0.0;
    int bei = (*b)->m_E.Count() - 1;
    ON_BrepEdge& topedge = (*b)->NewEdge((*b)->m_V[vert2ind], (*b)->m_V[vert2ind], ell2ind);
    topedge.m_tolerance = 0.0;
    int tei = (*b)->m_E.Count() - 1;

    (*b)->NewRuledFace((*b)->m_E[bei], false, (*b)->m_E[tei], false);
}


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
