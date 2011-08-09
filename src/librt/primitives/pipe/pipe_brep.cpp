/*                    P I P E _ B R E P . C P P
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
/** @file pipe_brep.cpp
 *
 * Convert a Pipe to b-rep form
 *
 */

#include "common.h"

#include "raytrace.h"
#include "rtgeom.h"
#include "brep.h"
#include "wdb.h"


/**
 * R T _ P I P E _ B R E P
 */
void
generate_curves(double id, double od, ON_Plane *plane, ON_SimpleArray<ON_Curve*> *outer, ON_SimpleArray<ON_Curve*> *inner)
{
    ON_Circle outercirclestart = ON_Circle(*plane, od/2.0);
    ON_NurbsCurve *ocurve = ON_NurbsCurve::New();
    outercirclestart.GetNurbForm(*ocurve);
    outer->Append(ON_Curve::Cast(ocurve));
    if (id > 0.0) {
	ON_Circle innercirclestart = ON_Circle(*plane, id/2.0);
	ON_NurbsCurve *icurve = ON_NurbsCurve::New();
    	innercirclestart.GetNurbForm(*icurve);
	inner->Append(ON_Curve::Cast(icurve));
    }
}


void
make_linear_surfaces(ON_Brep **b, ON_SimpleArray<ON_Curve*> *startoutercurves, ON_SimpleArray<ON_Curve*> *endoutercurves, ON_SimpleArray<ON_Curve*> *startinnercurves, ON_SimpleArray<ON_Curve*> *endinnercurves)
{
    bu_log("make_linear_surfaces\n");
    int c1ind = (*b)->AddEdgeCurve(*(startoutercurves[0]));
    int c2ind = (*b)->AddEdgeCurve(*(endoutercurves[0]));
    ON_BrepVertex& vert1 = (*b)->NewVertex((*b)->m_C3[c1ind]->PointAt(0), SMALL_FASTF);
    vert1.m_tolerance = 0.0;
    int vert1ind = (*b)->m_V.Count() - 1;
    ON_BrepVertex& vert2 = (*b)->NewVertex((*b)->m_C3[c2ind]->PointAt(0), SMALL_FASTF);
    vert2.m_tolerance = 0.0;
    int vert2ind = (*b)->m_V.Count() - 1;
    ON_BrepEdge& startedge = (*b)->NewEdge((*b)->m_V[vert1ind], (*b)->m_V[vert1ind], c1ind);
    startedge.m_tolerance = 0.0;
    ON_BrepEdge& endedge = (*b)->NewEdge((*b)->m_V[vert2ind], (*b)->m_V[vert2ind], c2ind);
    endedge.m_tolerance = 0.0;
    startoutercurves->Empty();
    for (int i = 0; i < endoutercurves->Count(); i++) {
	ON_Curve *curve = *(endoutercurves[i]);
	startoutercurves->Append(curve);
    }
    ON_BrepFace *newouterface = (*b)->NewRuledFace(startedge, false, endedge, false);
    (*b)->FlipFace(*newouterface);
    
    if (startinnercurves->Count() > 0) {
	int c3ind = (*b)->AddEdgeCurve(ON_Curve::Cast(*(startinnercurves[0])));
	int c4ind = (*b)->AddEdgeCurve(ON_Curve::Cast(*(endinnercurves[0])));
	ON_BrepVertex& vert3 = (*b)->NewVertex((*b)->m_C3[c3ind]->PointAt(0), SMALL_FASTF);
    	vert3.m_tolerance = 0.0;
    	int vert3ind = (*b)->m_V.Count() - 1;
    	ON_BrepVertex& vert4 = (*b)->NewVertex((*b)->m_C3[c4ind]->PointAt(0), SMALL_FASTF);
    	vert4.m_tolerance = 0.0;
    	int vert4ind = (*b)->m_V.Count() - 1;
    	ON_BrepEdge& startinneredge = (*b)->NewEdge((*b)->m_V[vert3ind], (*b)->m_V[vert3ind], c3ind);
	startinneredge.m_tolerance = 0.0;
    	ON_BrepEdge& endinneredge = (*b)->NewEdge((*b)->m_V[vert4ind], (*b)->m_V[vert4ind], c4ind);
	endinneredge.m_tolerance = 0.0;
	startinnercurves->Empty();
	for (int i = 0; i < endinnercurves->Count(); i++) {
    	    ON_Curve *curve = *(endinnercurves[i]);
    	    startinnercurves->Append(curve);
    	}
       	(*b)->NewRuledFace(startinneredge, false, endinneredge, false);
    }
}


void
make_curved_surfaces(ON_Brep **b, ON_SimpleArray<ON_Curve*> *startoutercurves, ON_SimpleArray<ON_Curve*> *startinnercurves, fastf_t angle, point_t bend_center, vect_t norm)
{
    point_t rev;
    VADD2(rev, bend_center, norm);
    
    ON_Line *revaxis = new ON_Line(ON_3dPoint(bend_center), ON_3dPoint(rev));
    ON_RevSurface* revsurf = ON_RevSurface::New();
    revsurf->m_curve = *startoutercurves[0];
    revsurf->m_axis = *revaxis;
    revsurf->m_angle = ON_Interval(2*ON_PI - angle, 2*ON_PI);
    ON_BrepFace *face = (*b)->NewFace(*revsurf);
    (*b)->FlipFace(*face);
    
    revsurf = ON_RevSurface::New();
    revsurf->m_curve = *startinnercurves[0];
    revsurf->m_axis = *revaxis;
    revsurf->m_angle = ON_Interval(2*ON_PI - angle, 2*ON_PI);
    face = (*b)->NewFace(*revsurf);
    
}


extern "C" void
rt_pipe_brep(ON_Brep **b, const struct rt_db_internal *ip, const struct bn_tol *UNUSED(tol))
{
    struct rt_pipe_internal *pip;

    register struct wdb_pipept *prevp;
    register struct wdb_pipept *curp;
    register struct wdb_pipept *nextp;
    point_t current_point;
    vect_t x_dir, y_dir, pipe_dir;

    ON_SimpleArray<ON_Curve*> startoutercurves;
    ON_SimpleArray<ON_Curve*> startinnercurves;

    ON_SimpleArray<ON_Curve*> endoutercurves;
    ON_SimpleArray<ON_Curve*> endinnercurves;

    ON_3dPoint plane_origin;
    ON_3dVector plane_x_dir, plane_y_dir;

    ON_Plane *endplane;
    ON_BrepLoop *bloop;

    RT_CK_DB_INTERNAL(ip);
    pip = (struct rt_pipe_internal *)ip->idb_ptr;
    RT_PIPE_CK_MAGIC(pip);

    if (BU_LIST_IS_EMPTY(&pip->pipe_segs_head)) return;
    prevp = BU_LIST_FIRST(wdb_pipept, &pip->pipe_segs_head);
    curp = BU_LIST_NEXT(wdb_pipept, &prevp->l);
    nextp = BU_LIST_NEXT(wdb_pipept, &curp->l);
    if (BU_LIST_IS_HEAD(&curp->l, &pip->pipe_segs_head)) return;
  
    VMOVE(current_point, prevp->pp_coord);
   
    VSUB2(pipe_dir, prevp->pp_coord, curp->pp_coord);
    bn_vec_ortho(x_dir, pipe_dir);
    VCROSS(y_dir, pipe_dir, x_dir);
    VUNITIZE(y_dir);

    plane_origin = ON_3dPoint(prevp->pp_coord);
    plane_x_dir = ON_3dVector(x_dir);
    plane_y_dir = ON_3dVector(y_dir);
    endplane = new ON_Plane(plane_origin, plane_x_dir, plane_y_dir); 
 
    generate_curves(prevp->pp_id, prevp->pp_od, endplane, &endoutercurves, &endinnercurves);
    
    ON_PlaneSurface* bp = new ON_PlaneSurface();
    bp->m_plane = (*endplane);
    bp->SetDomain(0, -100.0, 100.0);
    bp->SetDomain(1, -100.0, 100.0);
    bp->SetExtents(0, bp->Domain(0));
    bp->SetExtents(1, bp->Domain(1));
    (*b)->m_S.Append(bp);
    const int bsi = (*b)->m_S.Count() - 1;
    ON_BrepFace& bface = (*b)->NewFace(bsi);
    startoutercurves.Empty();
    startinnercurves.Empty();
    for (int i = 0; i < endoutercurves.Count(); i++) {
	ON_Curve *curve = endoutercurves[i];
	startoutercurves.Append(curve);
    }
    for (int i = 0; i < endinnercurves.Count(); i++) {
	ON_Curve *curve = endinnercurves[i];
	startinnercurves.Append(curve);
    }
    
    (*b)->NewPlanarFaceLoop(bface.m_face_index, ON_BrepLoop::outer, endoutercurves, true);
    bloop = (*b)->m_L.Last();
    bp->SetDomain(0, bloop->m_pbox.m_min.x, bloop->m_pbox.m_max.x);
    bp->SetDomain(1, bloop->m_pbox.m_min.y, bloop->m_pbox.m_max.y);
    bp->SetExtents(0, bp->Domain(0));
    bp->SetExtents(1, bp->Domain(1));
    if (prevp->pp_id > 0.0) {
	(*b)->NewPlanarFaceLoop(bface.m_face_index, ON_BrepLoop::inner, endinnercurves, true);
    }
    (*b)->SetTrimIsoFlags(bface);

    while (1) {
	vect_t n1, n2;
	vect_t norm;
	fastf_t angle;
	fastf_t dist_to_bend;
	endoutercurves.Empty();
	endinnercurves.Empty();

	if (BU_LIST_IS_HEAD(&nextp->l, &pip->pipe_segs_head)) {
	    // last segment, always linear
       	    VSUB2(pipe_dir, prevp->pp_coord, curp->pp_coord);
	    bn_vec_ortho(x_dir, pipe_dir);
	    VCROSS(y_dir, pipe_dir, x_dir);
	    VUNITIZE(y_dir);
	    plane_origin = ON_3dPoint(curp->pp_coord);
	    plane_x_dir = ON_3dVector(x_dir);
	    plane_y_dir = ON_3dVector(y_dir);
	    endplane = new ON_Plane(plane_origin, plane_x_dir, plane_y_dir); 
	    generate_curves(prevp->pp_id, prevp->pp_od, endplane, &endoutercurves, &endinnercurves);
	    make_linear_surfaces(b, &startoutercurves, &endoutercurves, &startinnercurves, &endinnercurves);
	    break;
	}

	VSUB2(n1, prevp->pp_coord, curp->pp_coord);
	if (!(VNEAR_ZERO(n1, RT_LEN_TOL))) {
	    // isn't duplicate point, proceed
    	    VSUB2(n2, nextp->pp_coord, curp->pp_coord);
    	    VCROSS(norm, n1, n2);
    	    VUNITIZE(n1);
    	    VUNITIZE(n2);
    	    angle = bn_pi - acos(VDOT(n1, n2));
    	    dist_to_bend = curp->pp_bendradius * tan(angle/2.0);
	    
    	    if (isnan(dist_to_bend) || VNEAR_ZERO(norm, SQRT_SMALL_FASTF) || NEAR_ZERO(dist_to_bend, SQRT_SMALL_FASTF)) {
    		// points are colinear, treat as linear segment 
    		VSUB2(pipe_dir, prevp->pp_coord, current_point);
	    	bn_vec_ortho(x_dir, pipe_dir);
	    	VCROSS(y_dir, pipe_dir, x_dir);
	    	VUNITIZE(y_dir);
	    	plane_origin = ON_3dPoint(current_point);
	    	plane_x_dir = ON_3dVector(x_dir);
	    	plane_y_dir = ON_3dVector(y_dir);
	    	endplane = new ON_Plane(plane_origin, plane_x_dir, plane_y_dir); 
       		generate_curves(prevp->pp_id, prevp->pp_od, endplane, &endoutercurves, &endinnercurves);
    		make_linear_surfaces(b, &startoutercurves, &endoutercurves, &startinnercurves, &endinnercurves);
		VMOVE(current_point, curp->pp_coord);
    	    } else {
		point_t bend_center;
		point_t bend_start;
		point_t bend_end;
		vect_t v1;

		VUNITIZE(norm);

		// Linear part first
		VJOIN1(bend_start, curp->pp_coord, dist_to_bend, n1);
    		VSUB2(pipe_dir, prevp->pp_coord, curp->pp_coord);
	    	bn_vec_ortho(x_dir, pipe_dir);
	    	VCROSS(y_dir, pipe_dir, x_dir);
	    	VUNITIZE(y_dir);
	    	plane_origin = ON_3dPoint(bend_start);
	    	plane_x_dir = ON_3dVector(x_dir);
	    	plane_y_dir = ON_3dVector(y_dir);
	    	endplane = new ON_Plane(plane_origin, plane_x_dir, plane_y_dir); 
       		generate_curves(prevp->pp_id, prevp->pp_od, endplane, &endoutercurves, &endinnercurves);
    		make_linear_surfaces(b, &startoutercurves, &endoutercurves, &startinnercurves, &endinnercurves);
		
		// Now do curved section
		VJOIN1(bend_end, curp->pp_coord, dist_to_bend, n2);
		VCROSS(v1, n1, norm);
		VJOIN1(bend_center, bend_start, -curp->pp_bendradius, v1);
		make_curved_surfaces(b, &startoutercurves, &startinnercurves, angle, bend_center, norm);
		startinnercurves.Empty();
		startoutercurves.Empty();
    		VSUB2(pipe_dir, curp->pp_coord, nextp->pp_coord);
	    	bn_vec_ortho(x_dir, pipe_dir);
	    	VCROSS(y_dir, pipe_dir, x_dir);
	    	VUNITIZE(y_dir);
	    	plane_origin = ON_3dPoint(bend_end);
	    	plane_x_dir = ON_3dVector(x_dir);
	    	plane_y_dir = ON_3dVector(y_dir);
	    	endplane = new ON_Plane(plane_origin, plane_x_dir, plane_y_dir); 
       		generate_curves(prevp->pp_id, prevp->pp_od, endplane, &startoutercurves, &startinnercurves);

		VMOVE(current_point, bend_end);
	    }
	}
	prevp = curp;
	curp = nextp;
	nextp = BU_LIST_NEXT(wdb_pipept, &curp->l);
    }
    // In the case of the final segment, also create the end face.
    endoutercurves.Empty();
    endinnercurves.Empty();

    VSUB2(pipe_dir, curp->pp_coord, prevp->pp_coord);
    bn_vec_ortho(x_dir, pipe_dir);
    VCROSS(y_dir, pipe_dir, x_dir);
    VUNITIZE(y_dir);
    plane_origin = ON_3dPoint(curp->pp_coord);
    plane_x_dir = ON_3dVector(x_dir);
    plane_y_dir = ON_3dVector(y_dir);
    endplane = new ON_Plane(plane_origin, plane_x_dir, plane_y_dir); 
 
    generate_curves(prevp->pp_id, prevp->pp_od, endplane, &endoutercurves, &endinnercurves);
    
    ON_PlaneSurface* ebp = new ON_PlaneSurface();
    ebp->m_plane = (*endplane);
    ebp->SetDomain(0, -100.0, 100.0);
    ebp->SetDomain(1, -100.0, 100.0);
    ebp->SetExtents(0, bp->Domain(0));
    ebp->SetExtents(1, bp->Domain(1));
    (*b)->m_S.Append(ebp);
    const int ebsi = (*b)->m_S.Count() - 1;
    ON_BrepFace& ebface = (*b)->NewFace(ebsi);
    (*b)->NewPlanarFaceLoop(ebface.m_face_index, ON_BrepLoop::outer, endoutercurves, true);
    const ON_BrepLoop* ebloop = (*b)->m_L.Last();
    ebp->SetDomain(0, ebloop->m_pbox.m_min.x, ebloop->m_pbox.m_max.x);
    ebp->SetDomain(1, ebloop->m_pbox.m_min.y, ebloop->m_pbox.m_max.y);
    ebp->SetExtents(0, ebp->Domain(0));
    ebp->SetExtents(1, ebp->Domain(1));
    if (prevp->pp_id > 0.0) {
	(*b)->NewPlanarFaceLoop(ebface.m_face_index, ON_BrepLoop::inner, endinnercurves, true);
    }
    (*b)->SetTrimIsoFlags(ebface);
}


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8

