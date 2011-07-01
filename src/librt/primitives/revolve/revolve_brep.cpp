/*                    R E V O L V E _ B R E P . C P P
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


void FindLoops(ON_Brep **b, const ON_Line* revaxis) {
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
	ON_BoundingBox *lbbox = new ON_BoundingBox();
	for (int j = 0; j < (*b)->m_C3.Count(); j++) {
	    if (curvearray[j] == i) {
		ON_Curve *currcurve = (*b)->m_C3[j];
		currcurve->GetBoundingBox(*lbbox, true);
	    }
	}
	point_t minpt, maxpt;
	double currdist;
	VSET(minpt, lbbox->m_min[0], lbbox->m_min[1], lbbox->m_min[2]);
	VSET(maxpt, lbbox->m_max[0], lbbox->m_max[1], lbbox->m_max[2]);
	currdist = DIST_PT_PT(minpt, maxpt);
	bu_log("currdist: %f\n", currdist);
	if (currdist > maxdist) {
	    maxdist = currdist;
	    largest_loop_index = i;
	}
    }
    bu_log("largest_loop_index = %d\n", largest_loop_index);

    bu_log("curve_array[%d, %d, %d, %d, %d, %d]\n", curvearray[0], curvearray[1], curvearray[2], curvearray[3], curvearray[4], curvearray[5]);


    for (int i = 0; i < loopcount ; i++) {
	bu_log("loopcount: %d\n", i);
	ON_PolyCurve* poly_curve = NULL;
	for (int j = 0; j < allsegments.Count(); j++) {
	    if (curvearray[j] == i) {
		if (!poly_curve) {
		    poly_curve = new ON_PolyCurve();
		    poly_curve->Append(allsegments[j]);
		} else {
		    poly_curve->Append(allsegments[j]);
		}
	    }
	}
	ON_NurbsCurve *revcurve = ON_NurbsCurve::New();
	poly_curve->GetNurbForm(*revcurve);
	ON_RevSurface* revsurf = ON_RevSurface::New();
	revsurf->m_curve = revcurve;
	revsurf->m_axis = *revaxis;
	ON_BrepFace *face = (*b)->NewFace(*revsurf);
	if (i == largest_loop_index) {
	    (*b)->FlipFace(*face);
	}
    }
}


/**
 * R T _ R E V O L V E _ B R E P
 */
extern "C" void
rt_revolve_brep(ON_Brep **b, const struct rt_db_internal *ip, const struct bn_tol *UNUSED(tol))
{
    struct rt_db_internal *tmp_internal = (struct rt_db_internal *) bu_malloc(sizeof(struct rt_db_internal), "allocate structure");
    RT_DB_INTERNAL_INIT(tmp_internal);
    struct rt_revolve_internal *rip;
    struct rt_sketch_internal *eip;

    rip = (struct rt_revolve_internal *)ip->idb_ptr;
    RT_REVOLVE_CK_MAGIC(rip);
    eip = rip->sk;
    RT_SKETCH_CK_MAGIC(eip);

    *b = ON_Brep::New();

    ON_3dPoint plane_origin;
    ON_3dVector plane_x_dir, plane_y_dir;

    ON_TextLog dump_to_stdout;
    ON_TextLog* dump = &dump_to_stdout;

    //  Find plane in 3 space corresponding to the sketch.

    plane_origin = ON_3dPoint(eip->V);
    plane_x_dir = ON_3dVector(eip->u_vec);
    plane_y_dir = ON_3dVector(eip->v_vec);
    const ON_Plane* sketch_plane = new ON_Plane(plane_origin, plane_x_dir, plane_y_dir);

    //  For the brep, need the list of 3D vertex points.  In sketch, they
    //  are stored as 2D coordinates, so use the sketch_plane to define 3 space
    //  points for the vertices.
    for (size_t i = 0; i < eip->vert_count; i++) {
	(*b)->NewVertex(sketch_plane->PointAt(eip->verts[i][0], eip->verts[i][1]), 0.0);
	int vertind = (*b)->m_V.Count() - 1;
	(*b)->m_V[vertind].Dump(*dump);
    }

    // Create the brep elements corresponding to the sketch lines, curves
    // and bezier segments. Create 2d, 3d and BrepEdge elements for each segment.
    // Will need to use the bboxes of each element to
    // build the overall bounding box for the face. Use bGrowBox to expand
    // a single box.
    struct line_seg *lsg;
    struct carc_seg *csg;
    struct bezier_seg *bsg;
    long *lng;
    for (size_t i = 0; i < (&eip->skt_curve)->seg_count; i++) {
	lng = (long *)(&eip->skt_curve)->segments[i];
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
		    ON_Circle* c3dcirc = new ON_Circle(cntrpt, cntrpt.DistanceTo(edgept));
		    ON_Curve* c3d = new ON_ArcCurve((const ON_Circle)*c3dcirc);
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
		    ON_3dPointArray *bezpoints = new ON_3dPointArray(bsg->degree + 1);
		    for (int j = 0; j < bsg->degree + 1; j++) {
			bezpoints->Append((*b)->m_V[bsg->ctl_points[j]].Point());
		    }
		    ON_BezierCurve* bez3d = new ON_BezierCurve((const ON_3dPointArray)*bezpoints);
		    ON_NurbsCurve* beznurb3d = ON_NurbsCurve::New();
		    bez3d->GetNurbForm(*beznurb3d);
		    beznurb3d->SetDomain(0.0, 1.0);
		    (*b)->m_C3.Append(beznurb3d);
		}
		break;
	    default:
		bu_log("Unhandled sketch object\n");
		break;
	}
    }


    const ON_Line& revaxis = ON_Line(ON_3dPoint(rip->v3d), ON_3dPoint(rip->axis3d));

    FindLoops(b, &revaxis);

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
