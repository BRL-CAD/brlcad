/*                    E P A _ B R E P . C P P
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
/** @file epa_brep.cpp
 *
 * Convert a Right Parabolic Cylinder to b-rep form
 *
 */

#include "common.h"

#include "raytrace.h"
#include "rtgeom.h"
#include "brep.h"



/**
 *			R T _ E P A _ B R E P
 */
extern "C" void
rt_epa_brep(ON_Brep **b, const struct rt_db_internal *ip, const struct bn_tol *tol)
{
   
    struct rt_epa_internal	*eip;

    *b = NULL; 

    RT_CK_DB_INTERNAL(ip);
    eip = (struct rt_epa_internal *)ip->idb_ptr;
    RT_EPA_CK_MAGIC(eip);

    *b = new ON_Brep();


    ON_TextLog dump_to_stdout;
    ON_TextLog* dump = &dump_to_stdout;
    
    point_t p1_origin, p2_origin;
    ON_3dPoint plane1_origin, plane2_origin;
    ON_3dVector plane_x_dir, plane_y_dir;
    
    //  First, find plane in 3 space corresponding to the bottom face of the EPA.
   
    vect_t tmp, x_dir, y_dir;
    
    VMOVE(x_dir, eip->epa_Au);
    VCROSS(y_dir, eip->epa_Au, eip->epa_H);
    VUNITIZE(y_dir);

    VMOVE(p1_origin, eip->epa_V);
    plane1_origin = ON_3dPoint(p1_origin);
    plane_x_dir = ON_3dVector(x_dir);
    plane_y_dir = ON_3dVector(y_dir);
    const ON_Plane* epa_bottom_plane = new ON_Plane(plane1_origin, plane_x_dir, plane_y_dir); 
   
    //  Next, create an ellipse in the plane corresponding to the edge of the epa.

    ON_Ellipse* ellipse1 = new ON_Ellipse(*epa_bottom_plane, eip->epa_r1, eip->epa_r2);
    ON_NurbsCurve* ellcurve1 = ON_NurbsCurve::New();
    ellipse1->GetNurbForm((*ellcurve1));
    ellcurve1->SetDomain(0.0,1.0);
    
    // Generate the bottom cap 
    ON_SimpleArray<ON_Curve*> boundary;
    boundary.Append(ON_Curve::Cast(ellcurve1)); 
    ON_PlaneSurface* bp = new ON_PlaneSurface();
    bp->m_plane = (*epa_bottom_plane);
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
 
    //  Now, the hard part.  Need an elliptical parabolic NURBS surface
    //  First step is to create a unit parabola that represents the surface
    //  along the minor axis.

    point_t y_tmp, y_rev_dir, ep1, ep2, ep3, tmppt;
    VREVERSE(y_rev_dir, y_dir);
    VADD2(ep1, p1_origin, y_rev_dir);
    VSCALE(tmppt, eip->epa_H, 2);
    VADD2(ep2, p1_origin, tmppt);
    VADD2(ep3, p1_origin, y_dir);
    ON_3dPoint onp1 = ON_3dPoint(ep1);
    ON_3dPoint onp2 = ON_3dPoint(ep2);
    ON_3dPoint onp3 = ON_3dPoint(ep3);
     
    
    ON_NurbsCurve* parabnurbscurve = ON_NurbsCurve::New(3,false,3,3);
    parabnurbscurve->SetKnot(0, 0);
    parabnurbscurve->SetKnot(1, 0);
    parabnurbscurve->SetKnot(2, 1);
    parabnurbscurve->SetKnot(3, 1);
    parabnurbscurve->SetCV(0,ON_3dPoint(ep1));  
    parabnurbscurve->SetCV(1,ON_3dPoint(ep2));
    parabnurbscurve->SetCV(2,ON_3dPoint(ep3)); 

    // Next, rotate that curve around the height vector.

    point_t revpoint;
    VADD2(revpoint, p1_origin, eip->epa_H);
    ON_3dPoint rpnt = ON_3dPoint(revpoint);
    
    ON_Line revaxis = ON_Line(plane1_origin, rpnt); 
    ON_RevSurface* para_surf = ON_RevSurface::New();
    para_surf->m_curve = parabnurbscurve;
    para_surf->m_axis = revaxis;
    para_surf->m_angle = ON_Interval(0,2*ON_PI);
    
    // Get the NURBS form of the surface
    ON_NurbsSurface *epacurvedsurf = ON_NurbsSurface::New();
    para_surf->GetNurbForm(*epacurvedsurf, 0.0); 
    
    // Last but not least, scale the control points of the
    // resulting surface to map to the longer axis.

    for( int i = 0; i < epacurvedsurf->CVCount(0); i++ ) {
	for (int j = 0; j < epacurvedsurf->CVCount(1); j++) {
	    point_t cvpt;
	    ON_4dPoint ctrlpt;
	    epacurvedsurf->GetCV(i,j, ctrlpt);
	    VSET(cvpt, ctrlpt.x * eip->epa_r1, ctrlpt.y * eip->epa_r2, ctrlpt.z);
	    ON_4dPoint newpt = ON_4dPoint(cvpt[0],cvpt[1],cvpt[2],ctrlpt.w);
	    epacurvedsurf->SetCV(i,j, newpt);
	}
    }
  
       
    bu_log("Valid nurbs surface: %d\n", epacurvedsurf->IsValid(dump));
    epacurvedsurf->Dump(*dump);

    (*b)->m_S.Append(epacurvedsurf);
    int surfindex = (*b)->m_S.Count();
    ON_BrepFace& face = (*b)->NewFace(surfindex - 1);
    int faceindex = (*b)->m_F.Count();
    ON_BrepLoop* outerloop = (*b)->NewOuterLoop(faceindex-1);
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

