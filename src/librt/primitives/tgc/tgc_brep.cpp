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
    ell1_axis_len_2 = MAGNITUDE(eip->a);
    ell2_axis_len_1 = MAGNITUDE(eip->c);
    ell2_axis_len_2 = MAGNITUDE(eip->d);

    ON_Ellipse* ellipse1 = new ON_Ellipse(*ell1_plane, ell1_axis_len_1, ell1_axis_len_2);
    ON_Ellipse* ellipse2 = new ON_Ellipse(*ell2_plane, ell2_axis_len_1, ell2_axis_len_2);

    //  Generate an ON_Curves from the ellipses
    ON_NurbsCurve ellcurve1;
    ellipse1->GetNurbForm(ellcurve1);
    ON_NurbsCurve ellcurve2;
    ellipse2->GetNurbForm(ellcurve2);

    //  Create the side surface with ON_NurbsSurface::CreateRuledSurface and the top
    //  and bottom planes by using the ellipses as outer trimming curves - define UV
    //  surfaces for the top and bottom such that they contain the ellipses.
    
    vect_t t1, t2, uv1, uv2, uv3, uv4;
    VREVERSE(t1, plane1_x_dir);
    VREVERSE(t2, plane1_y_dir);
    VSET(uv1, 0, 0 ,0);
    VSET(uv2, 0, 0 ,0);
    VSET(uv3, 0, 0 ,0);
    VSET(uv4, 0, 0 ,0);
    VADD2(uv1, t2, t1);
    VADD2(uv2, plane1_y_dir, t1);
    VADD2(uv3, plane1_y_dir, plane1_x_dir);
    VADD2(uv4, t2, plane1_x_dir);
    ON_3dPoint p1uv1(uv1);
    ON_3dPoint p1uv2(uv2);
    ON_3dPoint p1uv3(uv3);
    ON_3dPoint p1uv4(uv4);
    ON_NurbsSurface *tgc_bottom_surf = new ON_NurbsSurface(3,FALSE, 2, 2, 2, 2);
    tgc_bottom_surf->SetCV(0,0,p1uv1);
    tgc_bottom_surf->SetCV(1,0,p1uv2);
    tgc_bottom_surf->SetCV(0,1,p1uv3);
    tgc_bottom_surf->SetCV(1,1,p1uv4);
    tgc_bottom_surf->SetKnot(0,0,0.0);
    tgc_bottom_surf->SetKnot(0,1,1.0);
    tgc_bottom_surf->SetKnot(1,0,0.0);
    tgc_bottom_surf->SetKnot(1,1,1.0);
    
    VREVERSE(t1, plane2_x_dir);
    VREVERSE(t2, plane2_y_dir);
    VSET(uv1, 0, 0 ,0);
    VSET(uv2, 0, 0 ,0);
    VSET(uv3, 0, 0 ,0);
    VSET(uv4, 0, 0 ,0);
    VADD2(uv1, t2, t1);
    VADD2(uv2, plane2_y_dir, t1);
    VADD2(uv3, plane2_y_dir, plane2_x_dir);
    VADD2(uv4, t2, plane2_x_dir);
    ON_3dPoint p2uv1(uv1);
    ON_3dPoint p2uv2(uv2);
    ON_3dPoint p2uv3(uv3);
    ON_3dPoint p2uv4(uv4);
    ON_NurbsSurface *tgc_top_surf = new ON_NurbsSurface(3,FALSE, 2, 2, 2, 2);
    tgc_top_surf->SetCV(0,0,p2uv1);
    tgc_top_surf->SetCV(1,0,p2uv2);
    tgc_top_surf->SetCV(0,1,p2uv3);
    tgc_top_surf->SetCV(1,1,p2uv4);
    tgc_top_surf->SetKnot(0,0,0.0);
    tgc_top_surf->SetKnot(0,1,1.0);
    tgc_top_surf->SetKnot(1,0,0.0);
    tgc_top_surf->SetKnot(1,1,1.0);
    
    ON_Interval ell1dom = ellcurve1.Domain();
    ON_Interval ell2dom = ellcurve2.Domain();
    
    const ON_Curve *e1 = ON_Curve::Cast(&ellcurve1);
    const ON_Curve *e2 = ON_Curve::Cast(&ellcurve2);
    const ON_Interval *i1 = &ell1dom;
    const ON_Interval *i2 = &ell2dom;
    
    ON_NurbsSurface *tgc_side_surf;
    tgc_side_surf->CreateRuledSurface((*e1), (*e2), i1, i2);
    
    /* Create brep with three faces*/
    *b = ON_Brep::New();
    (*b)->NewFace(*tgc_top_surf);
    (*b)->NewFace(*tgc_bottom_surf);
    (*b)->NewFace(*tgc_side_surf);

}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8

