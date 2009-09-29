/*                    D S P _ B R E P . C P P
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
/** @file dsp_brep.cpp
 *
 * Convert a Displacement Map to b-rep form
 *
 */

#include "common.h"

#include "raytrace.h"
#include "rtgeom.h"
#include "brep.h"



/**
 *			R T _ D S P _ B R E P
 */
extern "C" void
rt_dsp_brep(ON_Brep **b, const struct rt_db_internal *ip, const struct bn_tol *tol)
{
   
    struct rt_dsp_internal	*dsp_ip;
    struct bu_mapped_file       *mf;
    

    *b = NULL; 

    RT_CK_DB_INTERNAL(ip);
    dsp_ip = (struct rt_dsp_internal *)ip->idb_ptr;
    RT_DSP_CK_MAGIC(dsp_ip);

    int in_cookie = bu_cv_cookie("nus");
    int out_cookie = bu_cv_cookie("hus");
    mf = dsp_ip->dsp_mp = bu_open_mapped_file(bu_vls_addr(&dsp_ip->dsp_name),"dsp");
    int count = dsp_ip->dsp_xcnt * dsp_ip->dsp_ycnt;
    mf->apbuflen = count * sizeof(unsigned short);
    mf->apbuf = bu_malloc(mf->apbuflen, "apbuf");
    bu_cv_w_cookie(mf->apbuf, out_cookie, mf->apbuflen, mf->buf, in_cookie, count);
    dsp_ip->dsp_buf = (short unsigned int*)mf->apbuf;
    
    *b = new ON_Brep();


    ON_TextLog dump_to_stdout;
    ON_TextLog* dump = &dump_to_stdout;

    switch (dsp_ip->dsp_datasrc) {
  	case RT_DSP_SRC_V4_FILE:
	case RT_DSP_SRC_FILE:
             if (!dsp_ip->dsp_mp) {
              	   bu_log("WARNING: Cannot find data file for displacement map (DSP)\n");
              	   if (bu_vls_addr(&dsp_ip->dsp_name)) {
                    bu_log("         DSP data file [%s] not found or empty\n", bu_vls_addr(&dsp_ip->dsp_name));
                } else {
                 	bu_log("         DSP data file not found or not specified\n");
                }
                return;
	     }
     	     BU_CK_MAPPED_FILE(dsp_ip->dsp_mp);
     	     break;
     	case RT_DSP_SRC_OBJ:
     	     if (!dsp_ip->dsp_bip) {
	         bu_log("WARNING: Cannot find data object for displacement map (DSP)\n");
	         if (bu_vls_addr(&dsp_ip->dsp_name)) {
	     	bu_log("         DSP data object [%s] not found or empty\n", bu_vls_addr(&dsp_ip->dsp_name));
	         } else {
	     	bu_log("         DSP data object not found or not specified\n");
	         }
	         return;
	     }
     	     RT_CK_DB_INTERNAL(dsp_ip->dsp_bip);
     	     RT_CK_BINUNIF(dsp_ip->dsp_bip->idb_ptr);
     	     break; 
    }


    // dsp.c defines utility functions for pulling dsp data - this seems like a good plan.
# define DSP(_p, _x, _y) ( \
	( \
	  (unsigned short *) \
	  ((_p)->dsp_buf) \
	)[ \
	(_y) * ((struct rt_dsp_internal *)_p)->dsp_xcnt + (_x) ] )


    // A DSP brep is broken down into faces as follows:
    //
    // 1.  The bottom face, a simple planar surface
    // 2.  The four sides, again planar faces but with one edge being trimmed
    //     by a curve describing the cross section of the DSP surface with 
    //     the plane of the face.
    // 3.  The top surface - a genaralized Bezier surface with boundaries equal
    //     to those of the four side surface intersection curves. Surface 
    //     topology is deduced as a function of the height values.
   
    // Step 1 - create the bottom face.
    
    point_t p_origin, p2, p3;
    ON_3dPoint plane_origin, plane_x_dir, plane_y_dir;
    
    VSETALL(p_origin, 0.0);
    plane_origin = ON_3dPoint(p_origin);
    
    VSET(p2, dsp_ip->dsp_xcnt - 1, 0, 0);
    plane_x_dir = ON_3dPoint(p2);

    VSET(p3, 0, dsp_ip->dsp_ycnt - 1, 0);
    plane_y_dir = ON_3dPoint(p3);

    ON_Plane *bottom_plane = new ON_Plane(plane_origin, plane_x_dir, plane_y_dir);
    ON_PlaneSurface *bottom_surf = new ON_PlaneSurface(*bottom_plane);
    (*b)->NewFace(*bottom_surf);
    
    // Next, define the top face with full resultion.

    ON_BezierSurface *bezsurf = new ON_BezierSurface(3, false, dsp_ip->dsp_xcnt, dsp_ip->dsp_ycnt);
      
    for (int y=0; y < (dsp_ip->dsp_ycnt - 1); y++ ) {
	for (int x=0; x < (dsp_ip->dsp_xcnt - 1); x++) {
	    ON_3dPoint *ctrlpt = new ON_3dPoint(x, y, DSP(dsp_ip, x, y));
	    bezsurf->SetCV(x,y,*ctrlpt);
	}
    }

    ON_NurbsSurface *tnurbssurf = ON_NurbsSurface::New();
    bezsurf->GetNurbForm(*tnurbssurf);
    (*b)->NewFace(*tnurbssurf);
	    
	    
/*
	
     
    
    
    //  First, find plane in 3 space corresponding to the bottom face of the EPA.
   
    vect_t tmp, x_dir, y_dir;
    
    VMOVE(x_dir, eip->dsp_Au);
    VCROSS(y_dir, eip->dsp_Au, eip->dsp_H);
    VUNITIZE(y_dir);

    VMOVE(p1_origin, eip->dsp_V);
    plane1_origin = ON_3dPoint(p1_origin);
    plane_x_dir = ON_3dVector(x_dir);
    plane_y_dir = ON_3dVector(y_dir);
    const ON_Plane* dsp_bottom_plane = new ON_Plane(plane1_origin, plane_x_dir, plane_y_dir); 
   
    //  Next, create an ellipse in the plane corresponding to the edge of the dsp.

    ON_Ellipse* ellipse1 = new ON_Ellipse(*dsp_bottom_plane, eip->dsp_r1, eip->dsp_r2);
    ON_NurbsCurve* ellcurve1 = ON_NurbsCurve::New();
    ellipse1->GetNurbForm((*ellcurve1));
    ellcurve1->SetDomain(0.0,1.0);
    
    // Generate the bottom cap 
    ON_SimpleArray<ON_Curve*> boundary;
    boundary.Append(ON_Curve::Cast(ellcurve1)); 
    ON_PlaneSurface* bp = new ON_PlaneSurface();
    bp->m_plane = (*dsp_bottom_plane);
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
    
    ON_4dPoint pt01 = ON_4dPoint(0, 0, h, 1);
    dspcurvedsurf->SetCV(0,0,pt01);
    ON_4dPoint pt02 = ON_4dPoint(0, r2/2, h, 1);
    dspcurvedsurf->SetCV(0,1,pt02);
    ON_4dPoint pt03 = ON_4dPoint(0, r2, 0, 1);
    dspcurvedsurf->SetCV(0,2,pt03);
    
    ON_4dPoint pt04 = ON_4dPoint(0, 0, h/sqrt(2.), 1/sqrt(2.));
    dspcurvedsurf->SetCV(1,0,pt04);      
    ON_4dPoint pt05 = ON_4dPoint(r1/2/sqrt(2.), r2/2/sqrt(2.), h/sqrt(2.), 1/sqrt(2.));
    dspcurvedsurf->SetCV(1,1,pt05);      
    ON_4dPoint pt06 = ON_4dPoint(r1/sqrt(2.), r2/sqrt(2.), 0, 1/sqrt(2.));
    dspcurvedsurf->SetCV(1,2,pt06);
    
    ON_4dPoint pt07 = ON_4dPoint(0, 0, h, 1);
    dspcurvedsurf->SetCV(2,0,pt07);      
    ON_4dPoint pt08 = ON_4dPoint(r1/2, 0, h, 1);
    dspcurvedsurf->SetCV(2,1,pt08);      
    ON_4dPoint pt09 = ON_4dPoint(r1, 0, 0, 1);
    dspcurvedsurf->SetCV(2,2,pt09);
    
    ON_4dPoint pt10 = ON_4dPoint(0, 0, h/sqrt(2.), 1/sqrt(2.));
    dspcurvedsurf->SetCV(3,0,pt10);      
    ON_4dPoint pt11 = ON_4dPoint(r1/2/sqrt(2.), -r2/2/sqrt(2.), h/sqrt(2.), 1/sqrt(2.));
    dspcurvedsurf->SetCV(3,1,pt11);      
    ON_4dPoint pt12 = ON_4dPoint(r1/sqrt(2.), -r2/sqrt(2.), 0, 1/sqrt(2.));
    dspcurvedsurf->SetCV(3,2,pt12);
    
    ON_4dPoint pt13 = ON_4dPoint(0, 0, h, 1);
    dspcurvedsurf->SetCV(4,0,pt13);      
    ON_4dPoint pt14 = ON_4dPoint(0, -r2/2, h, 1);
    dspcurvedsurf->SetCV(4,1,pt14);      
    ON_4dPoint pt15 = ON_4dPoint(0, -r2, 0, 1);
    dspcurvedsurf->SetCV(4,2,pt15);
   
    ON_4dPoint pt16 = ON_4dPoint(0, 0, h/sqrt(2.), 1/sqrt(2.));
    dspcurvedsurf->SetCV(5,0,pt16);      
    ON_4dPoint pt17 = ON_4dPoint(-r1/2/sqrt(2.), -r2/2/sqrt(2.), h/sqrt(2.), 1/sqrt(2.));
    dspcurvedsurf->SetCV(5,1,pt17);      
    ON_4dPoint pt18 = ON_4dPoint(-r1/sqrt(2.), -r2/sqrt(2.), 0, 1/sqrt(2.));
    dspcurvedsurf->SetCV(5,2,pt18);
    
    ON_4dPoint pt19 = ON_4dPoint(0, 0, h, 1);
    dspcurvedsurf->SetCV(6,0,pt19);      
    ON_4dPoint pt20 = ON_4dPoint(-r1/2, 0, h, 1);
    dspcurvedsurf->SetCV(6,1,pt20);      
    ON_4dPoint pt21 = ON_4dPoint(-r1, 0, 0, 1);
    dspcurvedsurf->SetCV(6,2,pt21);
    
    ON_4dPoint pt22 = ON_4dPoint(0, 0, h/sqrt(2.), 1/sqrt(2.));
    dspcurvedsurf->SetCV(7,0,pt22);      
    ON_4dPoint pt23 = ON_4dPoint(-r1/2/sqrt(2.), r2/2/sqrt(2.), h/sqrt(2.), 1/sqrt(2.));
    dspcurvedsurf->SetCV(7,1,pt23);      
    ON_4dPoint pt24 = ON_4dPoint(-r1/sqrt(2.), r2/sqrt(2.), 0, 1/sqrt(2.));
    dspcurvedsurf->SetCV(7,2,pt24);
   
    ON_4dPoint pt25 = ON_4dPoint(0, 0, h, 1);
    dspcurvedsurf->SetCV(8,0,pt25);      
    ON_4dPoint pt26 = ON_4dPoint(0, r2/2, h, 1);
    dspcurvedsurf->SetCV(8,1,pt26);      
    ON_4dPoint pt27 = ON_4dPoint(0, r2, 0, 1);
    dspcurvedsurf->SetCV(8,2,pt27);
   
    bu_log("Valid nurbs surface: %d\n", dspcurvedsurf->IsValid(dump));
    dspcurvedsurf->Dump(*dump);

    (*b)->m_S.Append(dspcurvedsurf);
    int surfindex = (*b)->m_S.Count();
    ON_BrepFace& face = (*b)->NewFace(surfindex - 1);
    int faceindex = (*b)->m_F.Count();
    ON_BrepLoop* outerloop = (*b)->NewOuterLoop(faceindex-1);
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

