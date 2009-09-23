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

void FindLoops(ON_Brep **b) {
    ON_SimpleArray<ON_BrepEdge *> allsegments;
    int loop_complete = 0;
    int current_loop;
    int current_segment;
    for (int i = 0; i < (*b)->m_E.Count(); i++) {
	allsegments.Append(&((*b)->m_E[i]));
    }
    while (allsegments.Count() > 0) {
	// Note that all loops start life as inner loops - the outer loop must be
	// found and labeled later.
	ON_BrepLoop& loop = (*b)->NewLoop(ON_BrepLoop::inner,(*b)->m_F[0]);
	(*b)->NewTrim(*(allsegments[allsegments.Count() - 1]), 0, loop, allsegments.Count() - 1);
	ON_BrepEdge *currentedge = allsegments[allsegments.Count() - 1];
	ON_BrepVertex *vertmatch = &((*b)->m_V[currentedge->m_vi[0]]);
	ON_BrepVertex *vertterminate = &((*b)->m_V[currentedge->m_vi[1]]);
	allsegments.Remove(allsegments.Count() - 1);
	current_segment = allsegments.Count() - 1;
	while ((allsegments.Count() > 0) && (current_segment > -1) && (loop_complete != 1)) {
	   currentedge = allsegments[current_segment];
	   ON_BrepVertex *currentvertexstart = &((*b)->m_V[currentedge->m_vi[0]]);
	   ON_BrepVertex *currentvertexend = &((*b)->m_V[currentedge->m_vi[1]]);
	   if ( (currentvertexstart == vertmatch) || (currentvertexend == vertmatch) ) {
	       if (currentvertexstart == vertmatch) vertmatch = currentvertexend;
	       if (currentvertexend == vertmatch) vertmatch = currentvertexstart;
	       (*b)->NewTrim(*(allsegments[allsegments.Count() - 1]), 0, loop, allsegments.Count() - 1);
	       allsegments.Remove(current_segment);
	       if (vertterminate == vertmatch) {
		   loop_complete = 1;
		   current_segment == -1;
	       } else {
		   current_segment = allsegments.Count() - 1;
	       }
	   } else {
	       current_segment--;
	   }
	}
    }
}    
 


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
    // and bezier segments. Create 2d, 3d and BrepEdge elements for each segment.
    // Will need to use the bboxes of each element to
    // build the overall bounding box for the face. Use bGrowBox to expand
    // a single box.
    struct line_seg *lsg;
    struct carc_seg *csg;
    struct bezier_seg *bsg;
    int edgcnt;
    ON_BoundingBox *bbox = new ON_BoundingBox();
    ON_SimpleArray<ON_Curve*> boundary;
    long *lng;
    for (int i = 0; i < (&eip->skt_curve)->seg_count; i++) {
	lng = (long *)(&eip->skt_curve)->segments[i];
	switch (*lng) {
	    case CURVE_LSEG_MAGIC:
		{
		    lsg = (struct line_seg *)lng;
		    ON_Curve* lsg3d = new ON_LineCurve((*b)->m_V[lsg->start].Point(), (*b)->m_V[lsg->end].Point());
		    lsg3d->SetDomain(0.0, 1.0);
		    lsg3d->GetBoundingBox(*bbox,true);
		    ON_Curve* lsg2d = new ON_LineCurve(ON_2dPoint(eip->verts[lsg->start][0],eip->verts[lsg->start][1]), ON_2dPoint(eip->verts[lsg->end][0],eip->verts[lsg->end][1]));
		    (*b)->m_C2.Append(lsg2d);
		    bbox->Set((*b)->m_V[lsg->start].Point(), true);
		    bbox->Set((*b)->m_V[lsg->end].Point(), true);
		    (*b)->m_C3.Append(lsg3d);
		    (*b)->NewEdge((*b)->m_V[lsg->start], (*b)->m_V[lsg->end] , (*b)->m_C3.Count() - 1);
		}
		edgcnt = (*b)->m_E.Count() - 1;
		(*b)->m_E[edgcnt].m_tolerance = 0.0;
		break;
	    case CURVE_CARC_MAGIC:
		csg = (struct carc_seg *)lng;
		if (csg->radius < 0) {
		    {
			ON_2dPoint cntrpt2d = ON_2dPoint(eip->verts[csg->end][0], eip->verts[csg->end][1]);
			ON_2dPoint edgept2d = ON_2dPoint(eip->verts[csg->start][0], eip->verts[csg->start][1]);
			ON_3dPoint cntrpt = (*b)->m_V[csg->end].Point();
			ON_3dPoint edgept = (*b)->m_V[csg->start].Point();
			ON_Circle* c3dcirc2d = new ON_Circle(cntrpt, cntrpt.DistanceTo(edgept));
			ON_Circle* c3dcirc = new ON_Circle(cntrpt,cntrpt.DistanceTo(edgept));
			ON_Curve* c3d2d = new ON_ArcCurve((const ON_Circle)*c3dcirc2d);
		    	c3d2d->ChangeDimension(2);
			ON_Curve* c3d = new ON_ArcCurve((const ON_Circle)*c3dcirc);
			c3d->SetDomain(0.0,1.0);
		    	c3d->GetBoundingBox(*bbox,true);
			(*b)->m_C2.Append(c3d2d);
			(*b)->m_C3.Append(c3d);
			(*b)->NewEdge((*b)->m_V[csg->start], (*b)->m_V[csg->end] , (*b)->m_C3.Count() - 1);
		    }
		    edgcnt = (*b)->m_E.Count() - 1;
		    (*b)->m_E[edgcnt].m_tolerance = 0.0;
		} else {
		    // need to calculated 3rd point on arc - look to sketch.c around line 581 for
		    // logic
		    ON_Arc* c3darc = new ON_Arc();
		    ON_Curve* c3d = new ON_ArcCurve();
		    c3d->GetBoundingBox(*bbox,true);
		}
		break;
	    case CURVE_BEZIER_MAGIC:
		bsg = (struct bezier_seg *)lng;
		{
		    ON_2dPointArray *bezpoints2d = new ON_2dPointArray(bsg->degree + 1);
		    for (int i = 0; i < bsg->degree + 1; i++) {
			bezpoints2d->Append(ON_2dPoint(eip->verts[bsg->ctl_points[i]][0], eip->verts[bsg->ctl_points[i]][1]));
		    }
		    ON_3dPointArray *bezpoints = new ON_3dPointArray(bsg->degree + 1);
		    for (int i = 0; i < bsg->degree + 1; i++) {
			bezpoints->Append((*b)->m_V[bsg->ctl_points[i]].Point());
		    }
		    ON_BezierCurve* bez3d2d = new ON_BezierCurve((const ON_2dPointArray)*bezpoints2d);
		    ON_BezierCurve* bez3d = new ON_BezierCurve((const ON_3dPointArray)*bezpoints);
		    ON_NurbsCurve* beznurb2d = new ON_NurbsCurve();
		    bez3d2d->GetNurbForm(*beznurb2d);
		    beznurb2d->ChangeDimension(2);
		    (*b)->m_C2.Append(beznurb2d);
		    ON_NurbsCurve* beznurb3d = new ON_NurbsCurve();
		    bez3d->GetNurbForm(*beznurb3d);
		    beznurb3d->SetDomain(0.0,1.0);
		    beznurb3d->GetBoundingBox(*bbox,true);
		    (*b)->m_C3.Append(beznurb3d);
		    (*b)->NewEdge((*b)->m_V[bsg->ctl_points[0]], (*b)->m_V[bsg->ctl_points[bsg->degree]] , (*b)->m_C3.Count() - 1);
		}
		edgcnt = (*b)->m_E.Count() - 1;
		(*b)->m_E[edgcnt].m_tolerance = 0.0;
		break;
	    default:
		bu_log("Unhandled sketch object\n");
		break;
	}
    }

    bu_log("bbox: %f,%f,%f; %f,%f,%f\n", bbox->m_min[0], bbox->m_min[1], bbox->m_min[2], bbox->m_max[0], bbox->m_max[1], bbox->m_max[2]);


    // For the purposes of BREP creation, it is necessary to identify
    // loops created by sketch segments.  This information is not stored
    // in the sketch data structures themselves, and thus must be deduced
    FindLoops(b);
/*
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

