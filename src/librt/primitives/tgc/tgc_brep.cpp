/*                    T G C _ B R E P . C P P
 * BRL-CAD
 *
 * Copyright (c) 2008-2009 United States Government as represented by
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
 *			R T _ T G C _ B R E P
 */
extern "C" void
rt_tgc_brep(ON_Brep **b, const struct rt_db_internal *ip, const struct bn_tol *tol)
{
    struct rt_tgc_internal	*eip;

    *b = NULL; 

    RT_CK_DB_INTERNAL(ip);
    eip = (struct rt_tgc_internal *)ip->idb_ptr;
    RT_TGC_CK_MAGIC(eip);

    *b = new ON_Brep();
    point_t p1_origin, p2_origin;
    ON_3dPoint plane1_origin, plane2_origin;
    ON_3dVector plane1_x_dir, plane1_y_dir, plane2_x_dir, plane2_y_dir;

    double ell1_axis_len_1, ell1_axis_len_2, ell2_axis_len_1, ell2_axis_len_2;
    
    //  First, find planes in 3 space with x and y axes
    //  along the axis of the ellipses defining the top and
    //  bottom of the TGC, with coordinate origins at the 
    //  center of the ellipses.  This information will be needed
    //  for the ruled surface definition (for the sides) and 
    //  also for the trimmed planes needed for the top and bottom
    //  of the volume.
   
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
   
    //  Once the planes have been created, create the ellipses
    //  within the planes.
    ell1_axis_len_1 = MAGNITUDE(eip->a);
    ell1_axis_len_2 = MAGNITUDE(eip->b);
    ell2_axis_len_1 = MAGNITUDE(eip->c);
    ell2_axis_len_2 = MAGNITUDE(eip->d);

    ON_Ellipse* ellipse1 = new ON_Ellipse(*ell1_plane, ell1_axis_len_1, ell1_axis_len_2);
    ON_Ellipse* ellipse2 = new ON_Ellipse(*ell2_plane, ell2_axis_len_1, ell2_axis_len_2);

    //  Generate an ON_Curves from the ellipses
    ON_NurbsCurve* ellcurve1 = ON_NurbsCurve::New();
    ellipse1->GetNurbForm((*ellcurve1));
    ON_NurbsCurve* ellcurve2 = ON_NurbsCurve::New();
    ellipse2->GetNurbForm((*ellcurve2));
    ellcurve1->SetDomain(0.0,1.0);
    ellcurve2->SetDomain(0.0,1.0);
   
    //  Create the side surface with ON_NurbsSurface::CreateRuledSurface and the top
    //  and bottom planes by using the ellipses as outer trimming curves - define UV
    //  surfaces for the top and bottom such that they contain the ellipses.
    
    ON_PlaneSurface* tgc_bottom_plane = new ON_PlaneSurface((*ell1_plane));
    ON_PlaneSurface* tgc_top_plane = new ON_PlaneSurface((*ell2_plane));
    ON_Interval p1_x_extents(-ell1_axis_len_1,ell1_axis_len_1);
    ON_Interval p1_y_extents(-ell1_axis_len_2,ell1_axis_len_2);
    ON_Interval p2_x_extents(-ell2_axis_len_1,ell2_axis_len_1);
    ON_Interval p2_y_extents(-ell2_axis_len_2,ell2_axis_len_2);
    tgc_bottom_plane->SetExtents(0,p1_x_extents);
    tgc_bottom_plane->SetExtents(1,p1_y_extents);
    tgc_top_plane->SetExtents(0,p2_x_extents);
    tgc_top_plane->SetExtents(1,p2_y_extents);
    tgc_bottom_plane->SetDomain(0,0.0,1.0);
    tgc_bottom_plane->SetDomain(1,0.0,1.0);
    tgc_top_plane->SetDomain(0,0.0,1.0);
    tgc_top_plane->SetDomain(1,0.0,1.0);

    ON_NurbsSurface *tgc_bottom_surf = ON_NurbsSurface::New();
    ON_NurbsSurface *tgc_top_surf = ON_NurbsSurface::New();
    tgc_bottom_plane->GetNurbForm((*tgc_bottom_surf), 0.0);
    tgc_top_plane->GetNurbForm((*tgc_top_surf), 0.0);
    (*b)->m_S.Append(ON_Surface::Cast(tgc_bottom_surf));
    (*b)->m_S.Append(ON_Surface::Cast(tgc_top_surf));
 
    ON_Interval ell1dom = ellcurve1->Domain();
    ON_Interval ell2dom = ellcurve2->Domain();
    
    const ON_Curve *ce1 = ON_Curve::Cast(ellcurve1);
    const ON_Curve *ce2 = ON_Curve::Cast(ellcurve2);
    const ON_Interval *i1 = &ell1dom;
    const ON_Interval *i2 = &ell2dom;
    
    ON_NurbsSurface *tgc_side_surf = ON_NurbsSurface::New();
    tgc_side_surf->CreateRuledSurface((*ce1), (*ce2), i1, i2);
    
    /* Create brep with three faces*/
    (*b)->NewFace(*tgc_top_surf);
    (*b)->NewFace(*tgc_bottom_surf);
    (*b)->NewFace(*tgc_side_surf);
/*
    ON_Curve *e1 = ON_Curve::Cast(ellcurve1);
    ON_Curve *e2 = ON_Curve::Cast(ellcurve2);
 
    (*b)->m_C3.Append(e1);
    int bottomcurveind = (*b)->m_C3.Count() - 1;
    (*b)->m_C3.Append(e2);
    int topcurveind = (*b)->m_C3.Count() - 1;

    ON_3dPoint coords1 = ellcurve1->PointAt(0);
    ON_3dPoint coords2 = ellcurve2->PointAt(0);
   
    ON_BrepEdge& bottomedge = (*b)->NewEdge(bottomcurveind);
    bottomedge.m_tolerance = 0.0;
    ON_BrepEdge& topedge = (*b)->NewEdge(topcurveind);
    topedge.m_tolerance = 0.0;

    ON_2dPoint circlept1(1,.5);
    ON_2dPoint circlept2(0,0.5);
    ON_2dPoint circlept3(.5,1);
    
    ON_Circle uvcircle = ON_Circle::ON_Circle(circlept1,circlept2,circlept3);
    ON_NurbsCurve* uvnurbs = ON_NurbsCurve::New();
    uvcircle.GetNurbForm((*uvnurbs));
    uvnurbs->SetDomain(0.0,1.0);
    ON_Curve *uvcurve = ON_Curve::Cast(uvnurbs);

    (*b)->m_C3.Append(uvcurve);
    int unitcircleind = (*b)->m_C3.Count() - 1;
  
    ON_BrepLoop& toploop = (*b)->NewLoop(ON_BrepLoop::outer, (*b)->m_F[0]);
    ON_BrepLoop& bottomloop = (*b)->NewLoop(ON_BrepLoop::outer, (*b)->m_F[1]);

    ON_BrepTrim& bottomtrim = (*b)->NewTrim(bottomedge, 0, bottomloop, unitcircleind);
    ON_BrepTrim& toptrim = (*b)->NewTrim(topedge, 0, toploop, unitcircleind);
*/
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8

