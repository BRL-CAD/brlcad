/*                    S K E T C H _ B R E P . C P P
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
/** @file sketch_brep.cpp
 *
 * Convert a sketch to b-rep form (does not creat a solid brep)
 *
 */

#include "common.h"

#include "raytrace.h"
#include "rtgeom.h"
#include "brep.h"

/*
void FindLoops(struct rt_sketch_internal *sk, ON_SimpleArray<ON_SimpleArray<genptr_t *> *> *loops) {
    ON_SimpleArray<genptr_t *> allsegments;
    long                *lng;
    struct line_seg     *lsg;
    struct carc_seg     *csg;
    struct bezier_seg   *bsg;
    int currentstart, currentend;
    int foundfirst = 0;
    for (int i = 0; i < sk->skt_curve.seg_count; i++) {
	allsegments.Append(sk->skt_curve.segments[i]);
    }
    int crvcnt = 0;
    while ((foundfirst != 1) && (crvcnt < allsegments.Count())) {
	    lng = (long *)allsegments[crvcnt];
	    crvcnt++;
	    switch (*lng) {
    	    	case CURVE_LSEG_MAGIC:
		    lsg = (struct line_seg *)lng;
		    int currentstart = lsg->start;
		    int currentend = lsg->end;
		    foundfirst == 1;
		    break;
		case CURVE_CARC_MAGIC:
     		    csg = (struct carc_seg *)lng;
		    int currentstart = csg->start;
		    int currentend = csg->end;
		    foundfirst == 1;
		    break;
		case CURVE_BEZIER_MAGIC:
     		    bsg = (struct bezier_seg *)lng;
		    int currentstart = bsg->ctl_points[0];
		    int currentend = bsg->ctl_points[bsg->degree];    
		    foundfirst == 1;
		    break;
	    	default:
    		    bu_log("Unhandled sketch type - FindLoops\n");
		    foundfirst == 0;
    		break;
	    }
    }
    loops->AppendNew();
    loops[0]->Append(allsegments[0]);
    allsegments.Remove(0);
    while (allsegments.Count() > 0) {
	int j = 0;
	int current_loop = loops->Count() - 1;
	while (allsegments.Count() > 0 && j < allsegments.Count()) {
	    lng = (long *)allsegments[j];
	    switch (*lng) {
		case CURVE_LSEG_MAGIC:
		    lsg = (struct line_seg *)lng;
		    if ((currentstart == lsg->end) || (currentend == lsg->start)) {
			if (currentstart == lsg->end) currentstart = lsg->start;
			if (currentend == lsg->end) currentend = lsg->end;
			loops[current_loop]->Append(allsegments[j]);
			allsegments.Remove(j);
			j == -1;
		    }
		    j++;
		    break;
		case CURVE_CARC_MAGIC:
		    csg = (struct carc_seg *)lng;
		    if ((csg->orientation == 0) &&  ((currentstart == csg->end) || (currentend == csg->start))) {
			if (currentstart == csg->end) currentstart = csg->start;
			if (currentend == csg->end) currentend = csg->end;
			loops[current_loop]->Append(allsegments[j]);
			allsegments.Remove(j);
			j == -1;
		    }
		    if ((csg->orientation != 0) &&  ((currentstart == csg->start) || (currentend == csg->end))) {
			if (currentstart == csg->start) currentstart = csg->start;
			if (currentend == csg->start) currentend = csg->end;
			loops[current_loop]->Append(allsegments[j]);
			allsegments.Remove(j);
			j == -1;
		    }
		    j++;
		    break;
		case CURVE_BEZIER_MAGIC:
		    bsg = (struct bezier_seg *)lng;
		    if ((currentstart == bsg->end) || (currentend == bsg->start)) {
			if (currentstart == bsg->end) currentstart = bsg->start;
			if (currentend == bsg->end) currentend = bsg->end;
			loops[current_loop]->Append(allsegments[j]);
			allsegments.Remove(j);
			j == -1;
		    }
		    j++;
		    break;
		default:
		    bu_log("Unhandled sketch type - FindLoops\n");
		    j++;
		    break;
	    }
	}
	if (allsegments.Count != 0) loops->AppendNew();
	loops[0]->Append(allsegments[0]);
	allsegments.Remove(0);
    }	
 */


/**
 *			R T _ S K E T C H _ B R E P
 */
extern "C" void
rt_sketch_brep(ON_Brep **b, const struct rt_db_internal *ip, const struct bn_tol *tol)
{
   
    struct rt_sketch_internal	*eip;

    *b = NULL; 

    RT_CK_DB_INTERNAL(ip);
    eip = (struct rt_sketch_internal *)ip->idb_ptr;
    RT_SKETCH_CK_MAGIC(eip);

    *b = new ON_Brep();


    ON_TextLog dump_to_stdout;
    ON_TextLog* dump = &dump_to_stdout;
    
    ON_3dPoint plane_origin;
    ON_3dVector plane_x_dir, plane_y_dir;
    
 
    //  Find plane in 3 space corresponding to the sketch.
    
    plane_origin = ON_3dPoint(eip->V);
    plane_x_dir = ON_3dVector(eip->u_vec);
    plane_y_dir = ON_3dVector(eip->v_vec);
    const ON_Plane* sketch_plane = new ON_Plane(plane_origin, plane_x_dir, plane_y_dir); 

    // Create the plane surface and brep face now - will need to find surface extents later.
    ON_PlaneSurface *sketch_surf = new ON_PlaneSurface(*sketch_plane);
    (*b)->m_S.Append(sketch_surf);
    int surfindex = (*b)->m_S.Count();
    ON_BrepFace& face = (*b)->NewFace(surfindex - 1);
 
    //  For the brep, need the list of 3D vertex points.  In sketch, they
    //  are stored as 2D coordinates, so use the sketch_plane to define 3 space
    //  points for the vertices.
    for (int i = 0; i < eip->vert_count; i++) {
	(*b)->NewVertex(sketch_plane->PointAt(eip->verts[i][0], eip->verts[i][1]), 0.0);
    }

    // Create the brep elements corresponding to the sketch lines, curves
    // and bezier segments.
    struct line_seg *lsg;
    struct carc_seg *csg;
    struct bezier_seg *bsg;
    int edgcnt;
    ON_SimpleArray<ON_Curve*> boundary;
    long *lng;
    for (int i = 0; i < (&eip->skt_curve)->seg_count; i++) {
	lng = (long *)(&eip->skt_curve)->segments[i];
	switch (*lng) {
	    case CURVE_LSEG_MAGIC:
		lsg = (struct line_seg *)lng;
		ON_Curve* lsg3d = new ON_LineCurve((*b)->m_V[lsg->start].Point(), (*b)->m_V[lsg->end].Point());
		lsg3d->SetDomain(0.0, 1.0);
		(*b)->m_C3.Append(lsg3d);
		(*b)->NewEdge((*b)->m_V[lsg->start], (*b)->m_V[lsg->end] , (*b)->m_C3.Count() - 1);
		edgcnt = (*b)->m_E.Count() - 1;
		(*b)->m_E[edgcnt].m_tolerance = 0.0;
		break;
	    case CURVE_CARC_MAGIC:
		csg = (struct carc_seg *)lng;
		if (csg->radius < 0) {
		    ON_3dPoint cntrpt = (*b)->m_V[csg->end].Point();
		    ON_3dPoint edgept = (*b)->m_V[csg->start].Point();
		    ON_Circle* c3dcirc = new ON_Circle(cntrpt,cntrpt.DistanceTo(edgept));
		    ON_Curve* c3d = new ON_ArcCurve((const ON_Circle)*c3dcirc);
		    c3d->SetDomain(0.0,1.0);
		    (*b)->m_C3.Append(c3d);
		    (*b)->NewEdge((*b)->m_V[csg->start], (*b)->m_V[csg->end] , (*b)->m_C3.Count() - 1);
		    edgcnt = (*b)->m_E.Count() - 1;
		    (*b)->m_E[edgcnt].m_tolerance = 0.0;
		} else {
		    // need to calculated 3rd point on arc - look to sketch.c around line 581 for
		    // logic
		    ON_Arc* c3darc = new ON_Arc();
    		    ON_Curve* c3d = new ON_ArcCurve();
		}
		break;
	    case CURVE_BEZIER_MAGIC:
		bsg = (struct bezier_seg *)lng;
//		ON_BezierCurve* bez3d = new ON_BezierCurve();
//		ON_NurbsCurve* nurb3d;
//		bez3d->GetNurbForm(*nurb3d);
		break;
	    default:
		bu_log("Unhandled sketch object\n");
		break;
	}
    }



    // Once the vertices are added, set the extents of the surface to ensure
    // it will hold all the vertices.
/*    vect_t vect_diag, vect_max, parallel, orthogonal;
    VSUB2(vect_diag, pmax, pmin);
    VPROJECT(vect_min, eip->u_vec, parallel, orthogonal);
    sketch_surf.SetDomain(0, MAGNITUDE(parallel), MAGNITUDE(orthogonal));
    VPROJECT(vect_max, eip->u_vec, parallel, orthogonal);
    sketch_surf.SetDomain(1, MAGNITUDE(parallel), MAGNITUDE(orthogonal));
    sketch_surf.SetExtents(0, sketch_surf.Domain(0));
    sketch_surf.SetExtents(1, sketch_surf.Domain(1));

    // Now, append the surface to the brep surface list
    (*b)->m_S.Append(ON_Surface::Cast(&sketch_surf));

    // For the purposes of BREP creation, it is necessary to identify
    // loops created by sketch segments.  This information is not stored
    // in the sketch data structures themselves, and thus must be deduced
    ON_SimpleArray<ON_SimpleArray<struct curve*> *> loops;
    FindLoops(eip, loops);

    // Create a single brep face and loops in that face
    const int bsi = (*b)->m_S.Count() - 1;
    ON_BrepFace& bface = (*b)->NewFace(bsi);
    (*b)->NewPlanarFaceLoop(bface.m_face_index, ON_BrepLoop::outer, boundary, true);
    (*b)->SetTrimIsoFlags(bface);
	     
    
    
    //  Next, create a parabolic NURBS curve corresponding to the shape of
    //  the parabola in the two planes.
    point_t x_rev_dir, ep1, ep2, ep3, tmppt;
    VREVERSE(x_rev_dir, x_dir);

    VADD2(ep1, p1_origin, x_rev_dir);
    VSCALE(tmppt, eip->rpc_B, 2);
    VADD2(ep2, p1_origin, tmppt);
    VADD2(ep3, p1_origin, x_dir);
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
    bu_log("Valid nurbs curve: %d\n", parabnurbscurve->IsValid(dump));
    parabnurbscurve->Dump(*dump);

    // Also need a staight line from the beginning to the end to
    // complete the loop

    ON_LineCurve* straightedge = new ON_LineCurve(onp3,onp1);   
    bu_log("Valid curve: %d\n", straightedge->IsValid(dump));
    straightedge->Dump(*dump);
   
    // Generate the bottom cap 
   
    ON_PlaneSurface* bp = new ON_PlaneSurface();
    bp->m_plane = (*rpc_bottom_plane);
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
    
    // Now the side face and top cap - extrude the bottom face and set the cap flag to true
    vect_t vp2;
    VADD2(vp2,eip->rpc_V, eip->rpc_H);
    const ON_Curve* extrudepath = new ON_LineCurve(ON_3dPoint(eip->rpc_V), ON_3dPoint(vp2));
    ON_Brep& brep = *(*b);
    ON_BrepExtrudeFace(brep,0, *extrudepath, true);
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

