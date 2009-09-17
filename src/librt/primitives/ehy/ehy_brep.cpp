/*                    E H Y _ B R E P . C P P
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
/** @file ehy_brep.cpp
 *
 * Convert an ehy to b-rep form
 *
 */

#include "common.h"

#include "raytrace.h"
#include "rtgeom.h"
#include "brep.h"



/**
 *			R T _ E H Y _ B R E P
 */
extern "C" void
rt_ehy_brep(ON_Brep **b, const struct rt_db_internal *ip, const struct bn_tol *tol)
{
   
    struct rt_ehy_internal	*eip;

    *b = NULL; 

    RT_CK_DB_INTERNAL(ip);
    eip = (struct rt_ehy_internal *)ip->idb_ptr;
    RT_EHY_CK_MAGIC(eip);

    *b = new ON_Brep();


    ON_TextLog dump_to_stdout;
    ON_TextLog* dump = &dump_to_stdout;
    
    point_t p1_origin, p2_origin;
    ON_3dPoint plane1_origin, plane2_origin;
    ON_3dVector plane_x_dir, plane_y_dir;
    
    //  First, find plane in 3 space corresponding to the bottom face of the EPA.
   
    vect_t tmp, x_dir, y_dir;
    
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
    ellcurve1->SetDomain(0.0,1.0);
    
    // Generate the bottom cap 
    ON_SimpleArray<ON_Curve*> boundary;
    boundary.Append(ON_Curve::Cast(ellcurve1)); 
    ON_PlaneSurface* bp = new ON_PlaneSurface();
    bp->m_plane = (*ehy_bottom_plane);
    bp->SetDomain(0, -100.0, 100.0 );
    bp->SetDomain(1, -100.0, 100.0 );
    bp->SetExtents(0, bp->Domain(0) );
    bp->SetExtents(1, bp->Domain(1) );
    (*b)->m_S.Append(bp);
    const int bsi = (*b)->m_S.Count() - 1;
    ON_BrepFace& bface = (*b)->NewFace(bsi);
    (*b)->NewPlanarFaceLoop(bface.m_face_index, ON_BrepLoop::outer, boundary, true); 
    const ON_BrepLoop* bloop = (*b)->m_L.Last();
    bp->SetDomain(0, bloop->m_pbox.m_min.x, bloop->m_pbox.m_max.x );
    bp->SetDomain(1, bloop->m_pbox.m_min.y, bloop->m_pbox.m_max.y );
    bp->SetExtents(0,bp->Domain(0));
    bp->SetExtents(1,bp->Domain(1));
    (*b)->SetTrimIsoFlags(bface);
 
    //  Now, the hard part.  Need an elliptical hyperbolic NURBS surface
    
    ON_NurbsSurface* ehycurvedsurf = ON_NurbsSurface::New(3,true,3,3,9,3);
    ehycurvedsurf->SetKnot(0, 0, 0);
    ehycurvedsurf->SetKnot(0, 1, 0);
    ehycurvedsurf->SetKnot(0, 2, 1.571);
    ehycurvedsurf->SetKnot(0, 3, 1.571);
    ehycurvedsurf->SetKnot(0, 4, 3.142);
    ehycurvedsurf->SetKnot(0, 5, 3.142);
    ehycurvedsurf->SetKnot(0, 6, 4.713);
    ehycurvedsurf->SetKnot(0, 7, 4.713);
    ehycurvedsurf->SetKnot(0, 8, 6.284);
    ehycurvedsurf->SetKnot(0, 9, 6.284);
    ehycurvedsurf->SetKnot(1, 0, 0);
    ehycurvedsurf->SetKnot(1, 1, 0);
    ehycurvedsurf->SetKnot(1, 2, eip->ehy_r1*2);
    ehycurvedsurf->SetKnot(1, 3, eip->ehy_r1*2);

    double intercept_calc = (eip->ehy_c)*(eip->ehy_c)/(MAGNITUDE(eip->ehy_H) + eip->ehy_c);
    double intercept_dist = MAGNITUDE(eip->ehy_H) + eip->ehy_c - intercept_calc;
    double intercept_length = intercept_dist - MAGNITUDE(eip->ehy_H);
    double MX = MAGNITUDE(eip->ehy_H);
    double MP = MX + intercept_length;
    double w = 1/sqrt((MX/MP)/(1-MX/MP));
    
    double h = MAGNITUDE(eip->ehy_H);
    double r1 = eip->ehy_r1;
    double r2 = eip->ehy_r2;
    
    ON_4dPoint pt01 = ON_4dPoint(0, 0, h, 1);
    ehycurvedsurf->SetCV(0,0,pt01);
    ON_4dPoint pt02 = ON_4dPoint(0, r2/2, h, 1);
    ehycurvedsurf->SetCV(0,1,pt02);
    ON_4dPoint pt03 = ON_4dPoint(0, r2, 0, 1);
    ehycurvedsurf->SetCV(0,2,pt03);
    
    ON_4dPoint pt04 = ON_4dPoint(0, 0, h*w, w);
    ehycurvedsurf->SetCV(1,0,pt04);      
    ON_4dPoint pt05 = ON_4dPoint(r1/2*w, r2/2*w, h*w, w);
    ehycurvedsurf->SetCV(1,1,pt05);      
    ON_4dPoint pt06 = ON_4dPoint(r1*1/sqrt(2), r2*1/sqrt(2), 0, 1/sqrt(2));
    ehycurvedsurf->SetCV(1,2,pt06);
    
    ON_4dPoint pt07 = ON_4dPoint(0, 0, h, 1);
    ehycurvedsurf->SetCV(2,0,pt07);      
    ON_4dPoint pt08 = ON_4dPoint(r1/2, 0, h, 1);
    ehycurvedsurf->SetCV(2,1,pt08);      
    ON_4dPoint pt09 = ON_4dPoint(r1, 0, 0, 1);
    ehycurvedsurf->SetCV(2,2,pt09);
    
    ON_4dPoint pt10 = ON_4dPoint(0, 0, h*w, w);
    ehycurvedsurf->SetCV(3,0,pt10);      
    ON_4dPoint pt11 = ON_4dPoint(r1/2*w, -r2/2*w, h*w, w);
    ehycurvedsurf->SetCV(3,1,pt11);      
    ON_4dPoint pt12 = ON_4dPoint(r1*1/sqrt(2), -r2*1/sqrt(2), 0, 1/sqrt(2));
    ehycurvedsurf->SetCV(3,2,pt12);
    
    ON_4dPoint pt13 = ON_4dPoint(0, 0, h, 1);
    ehycurvedsurf->SetCV(4,0,pt13);      
    ON_4dPoint pt14 = ON_4dPoint(0, -r2/2, h, 1);
    ehycurvedsurf->SetCV(4,1,pt14);      
    ON_4dPoint pt15 = ON_4dPoint(0, -r2, 0, 1);
    ehycurvedsurf->SetCV(4,2,pt15);
   
    ON_4dPoint pt16 = ON_4dPoint(0, 0, h*w, w);
    ehycurvedsurf->SetCV(5,0,pt16);      
    ON_4dPoint pt17 = ON_4dPoint(-r1/2*w, -r2/2*w, h*w, w);
    ehycurvedsurf->SetCV(5,1,pt17);      
    ON_4dPoint pt18 = ON_4dPoint(-r1*1/sqrt(2), -r2*1/sqrt(2), 0, 1/sqrt(2));
    ehycurvedsurf->SetCV(5,2,pt18);
    
    ON_4dPoint pt19 = ON_4dPoint(0, 0, h, 1);
    ehycurvedsurf->SetCV(6,0,pt19);      
    ON_4dPoint pt20 = ON_4dPoint(-r1/2, 0, h, 1);
    ehycurvedsurf->SetCV(6,1,pt20);      
    ON_4dPoint pt21 = ON_4dPoint(-r1, 0, 0, 1);
    ehycurvedsurf->SetCV(6,2,pt21);
    
    ON_4dPoint pt22 = ON_4dPoint(0, 0, h*w, w);
    ehycurvedsurf->SetCV(7,0,pt22);      
    ON_4dPoint pt23 = ON_4dPoint(-r1/2*w, r2/2*w, h*w, w);
    ehycurvedsurf->SetCV(7,1,pt23);      
    ON_4dPoint pt24 = ON_4dPoint(-r1*1/sqrt(2), r2*1/sqrt(2), 0, 1/sqrt(2));
    ehycurvedsurf->SetCV(7,2,pt24);
   
    ON_4dPoint pt25 = ON_4dPoint(0, 0, h, 1);
    ehycurvedsurf->SetCV(8,0,pt25);      
    ON_4dPoint pt26 = ON_4dPoint(0, r2/2, h, 1);
    ehycurvedsurf->SetCV(8,1,pt26);      
    ON_4dPoint pt27 = ON_4dPoint(0, r2, 0, 1);
    ehycurvedsurf->SetCV(8,2,pt27);
   
    bu_log("Valid nurbs surface: %d\n", ehycurvedsurf->IsValid(dump));
    ehycurvedsurf->Dump(*dump);

    (*b)->m_S.Append(ehycurvedsurf);
    int surfindex = (*b)->m_S.Count();
    ON_BrepFace& face = (*b)->NewFace(surfindex - 1);
    int faceindex = (*b)->m_F.Count();
    ON_BrepLoop* outerloop = (*b)->NewOuterLoop(faceindex-1);
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8

