/*                    R E V O L V E _ B R E P . C P P
 * BRL-CAD
 *
 * Copyright (c) 2008-2014 United States Government as represented by
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
/** @file revolve_brep.cpp
 *
 * Convert a Revolved Sketch to b-rep form
 *
 */

#include "common.h"

#include "raytrace.h"
#include "rtgeom.h"
#include "nmg.h"
#include "brep.h"

extern "C" {
    extern void rt_sketch_brep(ON_Brep **bi, struct rt_db_internal *ip, const struct bn_tol *tol);
}


void FindLoops(ON_Brep **b, const ON_Line* revaxis, const fastf_t ang) {
    ON_3dPoint ptmatch, ptterminate, pstart, pend;

    int *curvearray;
    curvearray = static_cast<int*>(bu_malloc((*b)->m_C3.Count() * sizeof(int), "sketch edge list"));
    for (int i = 0; i < (*b)->m_C3.Count(); i++) {
	curvearray[i] = -1;
    }
    ON_SimpleArray<ON_Curve *> allsegments;
    ON_SimpleArray<ON_Curve *> loopsegments;
    int loop_complete;
    for (int i = 0; i < (*b)->m_C3.Count(); i++) {
	allsegments.Append((*b)->m_C3[i]);
    }

    int allcurvesassigned = 0;
    int assignedcount = 0;
    int curvecount = 0;
    int loopcount = 0;
    while (allcurvesassigned != 1) {
	int havefirstcurve = 0;
	while ((havefirstcurve == 0) && (curvecount < allsegments.Count())) {
	    if (curvearray[curvecount] == -1) {
		havefirstcurve = 1;
	    } else {
		curvecount++;
	    }
	}
	// First, sort through things to assign curves to loops.
	loop_complete = 0;
	while ((loop_complete != 1) && (allcurvesassigned != 1)) {
	    curvearray[curvecount] = loopcount;
	    ptmatch = (*b)->m_C3[curvecount]->PointAtEnd();
	    ptterminate = (*b)->m_C3[curvecount]->PointAtStart();
	    for (int i = 0; i < allsegments.Count(); i++) {
		pstart = (*b)->m_C3[i]->PointAtStart();
		pend = (*b)->m_C3[i]->PointAtEnd();
		if (NEAR_ZERO(ptmatch.DistanceTo(pstart), ON_ZERO_TOLERANCE) && (curvearray[i] == -1)) {
		    curvecount = i;
		    ptmatch = pend;
		    i = allsegments.Count();
		    if (NEAR_ZERO(pend.DistanceTo(ptterminate), ON_ZERO_TOLERANCE)) {
			loop_complete = 1;
			loopcount++;
		    }
		} else {
		    if (i == allsegments.Count() - 1) {
			loop_complete = 1; //If we reach this pass, loop had better be complete
			loopcount++;
			assignedcount = 0;
			for (int j = 0; j < allsegments.Count(); j++) {
			    if (curvearray[j] != -1) assignedcount++;
			}
			if (allsegments.Count() == assignedcount) allcurvesassigned = 1;
		    }
		}
	    }
	}
    }

    double maxdist = 0.0;
    int largest_loop_index = 0;
    for (int i = 0; i <= loopcount ; i++) {
	ON_BoundingBox lbbox;
	for (int j = 0; j < (*b)->m_C3.Count(); j++) {
	    if (curvearray[j] == i) {
		ON_Curve *currcurve = (*b)->m_C3[j];
		currcurve->GetBoundingBox(lbbox, true);
	    }
	}
	point_t minpt, maxpt;
	double currdist;
	VSET(minpt, lbbox.m_min[0], lbbox.m_min[1], lbbox.m_min[2]);
	VSET(maxpt, lbbox.m_max[0], lbbox.m_max[1], lbbox.m_max[2]);
	currdist = DIST_PT_PT(minpt, maxpt);
	if (currdist > maxdist) {
	    maxdist = currdist;
	    largest_loop_index = i;
	}
    }

    for (int i = 0; i < loopcount ; i++) {
	ON_PolyCurve* poly_curve = new ON_PolyCurve();
	for (int j = 0; j < allsegments.Count(); j++) {
	    if (curvearray[j] == i) {
		 poly_curve->Append(allsegments[j]);
	    }
	}

	ON_NurbsCurve *revcurve = ON_NurbsCurve::New();
	poly_curve->GetNurbForm(*revcurve);
	ON_RevSurface* revsurf = ON_RevSurface::New();
	revsurf->m_curve = revcurve;
	revsurf->m_axis = *revaxis;
	revsurf->m_angle = ON_Interval(0, ang);
	ON_BrepFace *face = (*b)->NewFace(*revsurf);

	if (i == largest_loop_index) {
	    (*b)->FlipFace(*face);
	}
    }

    bu_free(curvearray, "sketch edge list");
}


extern "C" void
rt_revolve_brep(ON_Brep **b, const struct rt_db_internal *ip, const struct bn_tol *tol)
{
    struct rt_db_internal *tmp_internal;
    struct rt_revolve_internal *rip;
    struct rt_sketch_internal *eip;

    BU_ALLOC(tmp_internal, struct rt_db_internal);
    RT_DB_INTERNAL_INIT(tmp_internal);

    rip = (struct rt_revolve_internal *)ip->idb_ptr;
    RT_REVOLVE_CK_MAGIC(rip);
    eip = rip->skt;
    RT_SKETCH_CK_MAGIC(eip);

    ON_3dPoint plane_origin;
    ON_3dVector plane_x_dir, plane_y_dir;

    bool full_revolve = true;
    if (rip->ang < 2*ON_PI && rip->ang > 0)
	full_revolve = false;

    //  Find plane in 3 space corresponding to the sketch.

    vect_t startpoint;
    VADD2(startpoint, rip->v3d, rip->r);
    plane_origin = ON_3dPoint(startpoint);
    plane_x_dir = ON_3dVector(eip->u_vec);
    plane_y_dir = ON_3dVector(eip->v_vec);
    const ON_Plane sketch_plane = ON_Plane(plane_origin, plane_x_dir, plane_y_dir);

    //  For the brep, need the list of 3D vertex points.  In sketch, they
    //  are stored as 2D coordinates, so use the sketch_plane to define 3 space
    //  points for the vertices.
    for (size_t i = 0; i < eip->vert_count; i++) {
	(*b)->NewVertex(sketch_plane.PointAt(eip->verts[i][0], eip->verts[i][1]), 0.0);
    }

    // Create the brep elements corresponding to the sketch lines, curves
    // and bezier segments. Create 2d, 3d and BrepEdge elements for each segment.
    // Will need to use the bboxes of each element to
    // build the overall bounding box for the face. Use bGrowBox to expand
    // a single box.
    struct line_seg *lsg;
    struct carc_seg *csg;
    struct bezier_seg *bsg;
    uint32_t *lng;
    for (size_t i = 0; i < (&eip->curve)->count; i++) {
	lng = (uint32_t *)(&eip->curve)->segment[i];
	switch (*lng) {
	    case CURVE_LSEG_MAGIC: {
		lsg = (struct line_seg *)lng;
		ON_Curve* lsg3d = new ON_LineCurve((*b)->m_V[lsg->start].Point(), (*b)->m_V[lsg->end].Point());
		lsg3d->SetDomain(0.0, 1.0);
		(*b)->m_C3.Append(lsg3d);
	    }
		break;
	    case CURVE_CARC_MAGIC:
		csg = (struct carc_seg *)lng;
		if (csg->radius < 0) { {
		    ON_3dPoint cntrpt = (*b)->m_V[csg->end].Point();
		    ON_3dPoint edgept = (*b)->m_V[csg->start].Point();
		    ON_Plane cplane = ON_Plane(cntrpt, plane_x_dir, plane_y_dir);
		    ON_Circle c3dcirc = ON_Circle(cplane, cntrpt.DistanceTo(edgept));
		    ON_Curve* c3d = new ON_ArcCurve((const ON_Circle)c3dcirc);
		    c3d->SetDomain(0.0, 1.0);
		    (*b)->m_C3.Append(c3d);
		}
		} else {
		    // need to calculated 3rd point on arc - look to sketch.c around line 581 for
		    // logic
		}
		break;
	    case CURVE_BEZIER_MAGIC:
		bsg = (struct bezier_seg *)lng;
		{
		    ON_3dPointArray bezpoints = ON_3dPointArray(bsg->degree + 1);
		    for (int j = 0; j < bsg->degree + 1; j++) {
			bezpoints.Append((*b)->m_V[bsg->ctl_points[j]].Point());
		    }
		    ON_BezierCurve bez3d = ON_BezierCurve((const ON_3dPointArray)bezpoints);
		    ON_NurbsCurve* beznurb3d = ON_NurbsCurve::New();
		    bez3d.GetNurbForm(*beznurb3d);
		    beznurb3d->SetDomain(0.0, 1.0);
		    (*b)->m_C3.Append(beznurb3d);
		}
		break;
	    default:
		bu_log("Unhandled sketch object\n");
		break;
	}
    }

    vect_t endpoint;
    VADD2(endpoint, rip->v3d, rip->axis3d);
    const ON_Line& revaxis = ON_Line(ON_3dPoint(rip->v3d), ON_3dPoint(endpoint));

    FindLoops(b, &revaxis, rip->ang);

    // Create the two boundary surfaces, if it's not a full revolution
    if (!full_revolve) {
	// First, deduce the transformation matrices to calculate the position of the end surface
	// The transformation matrices are to rotate an arbitrary point around an arbitrary axis
	// Let the point A = (x, y, z), the rotation axis is p1p2 = (x2,y2,z2)-(x1,y1,z1) = (a,b,c)
	// Then T1 is to translate p1 to the origin
	// Rx is to rotate p1p2 around the X axis to the plane XOZ
	// Ry is to rotate p1p2 around the Y axis to be coincident to Z axis
	// Rz is to rotate A with the angle around Z axis (the new p1p2)
	// RxInv, RyInv, T1Inv are the inverse transformation of Rx, Ry, T1, respectively.
	// The whole transformation is A' = A*T1*Rx*Ry*Rz*Ry*Inv*Rx*Inv = A*R
	vect_t end_plane_origin, end_plane_x_dir, end_plane_y_dir;
	mat_t R;
	MAT_IDN(R);
	mat_t T1, Rx, Ry, Rz, RxInv, RyInv, T1Inv;
	MAT_IDN(T1);
	VSET(&T1[12], -rip->v3d[0], -rip->v3d[1], -rip->v3d[2]);
	MAT_IDN(Rx);
	fastf_t v = sqrt(rip->axis3d[1]*rip->axis3d[1]+rip->axis3d[2]*rip->axis3d[2]);
	VSET(&Rx[4], 0, rip->axis3d[2]/v, rip->axis3d[1]/v);
	VSET(&Rx[8], 0, -rip->axis3d[1]/v, rip->axis3d[2]/v);
	MAT_IDN(Ry);
	fastf_t u = MAGNITUDE(rip->axis3d);
	VSET(&Ry[0], v/u, 0, -rip->axis3d[0]/u);
	VSET(&Ry[8], rip->axis3d[0]/u, 0, v/u);
	MAT_IDN(Rz);
	fastf_t C, S;
	C = cos(rip->ang);
	S = sin(rip->ang);
	VSET(&Rz[0], C, S, 0);
	VSET(&Rz[4], -S, C, 0);
	bn_mat_inv(RxInv, Rx);
	bn_mat_inv(RyInv, Ry);
	bn_mat_inv(T1Inv, T1);
	mat_t temp;
	bn_mat_mul4(temp, T1, Rx, Ry, Rz);
	bn_mat_mul4(R, temp, RyInv, RxInv, T1Inv);
	VEC3X3MAT(end_plane_origin, plane_origin, R);
	VADD2(end_plane_origin, end_plane_origin, &R[12]);
	VEC3X3MAT(end_plane_x_dir, plane_x_dir, R);
	VEC3X3MAT(end_plane_y_dir, plane_y_dir, R);

	// Create the start and end surface with rt_sketch_brep()
	struct rt_sketch_internal sketch;
	sketch = *(rip->skt);
	ON_Brep *b1 = ON_Brep::New();
	VMOVE(sketch.V, plane_origin);
	VMOVE(sketch.u_vec, plane_x_dir);
	VMOVE(sketch.v_vec, plane_y_dir);
	tmp_internal->idb_ptr = (void *)(&sketch);
	rt_sketch_brep(&b1, tmp_internal, tol);
	(*b)->Append(*b1->Duplicate());

	ON_Brep *b2 = ON_Brep::New();
	VMOVE(sketch.V, end_plane_origin);
	VMOVE(sketch.u_vec, end_plane_x_dir);
	VMOVE(sketch.v_vec, end_plane_y_dir);
	tmp_internal->idb_ptr = (void *)(&sketch);
	rt_sketch_brep(&b2, tmp_internal, tol);
	(*b)->Append(*b2->Duplicate());
	(*b)->FlipFace((*b)->m_F[(*b)->m_F.Count()-1]);
    }
    bu_free(tmp_internal, "free temporary rt_db_internal");
}


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
