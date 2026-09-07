/*                    P I P E _ B R E P . C P P
 * BRL-CAD
 *
 * Copyright (c) 2008-2026 United States Government as represented by
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

#include <cmath>
#include <vector>

#include "raytrace.h"
#include "rt/geom.h"
#include "brep.h"
#include "wdb.h"
#include "primitives/brep/primitive_brep.h"


static void
generate_curves(fastf_t id, fastf_t od, ON_Plane *plane,
	ON_SimpleArray<ON_Curve*> *outer,
	ON_SimpleArray<ON_Curve*> *inner)
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


static bool
make_linear_surfaces(ON_Brep **b,
	ON_SimpleArray<ON_Curve*> *startoutercurves,
	ON_SimpleArray<ON_Curve*> *endoutercurves,
	ON_SimpleArray<ON_Curve*> *startinnercurves,
	ON_SimpleArray<ON_Curve*> *endinnercurves)
{
    if (startoutercurves->Count() != 1 || endoutercurves->Count() != 1 ||
	startinnercurves->Count() != endinnercurves->Count() ||
	startinnercurves->Count() > 1)
	return false;
    int c1ind = (*b)->AddEdgeCurve((*startoutercurves)[0]);
    if (c1ind < 0) {
	bu_log("Failed to create edge curve 1 - pipe_brep.cpp:%d\n", __LINE__);
	return false;
    }
    int c2ind = (*b)->AddEdgeCurve((*endoutercurves)[0]);
    if (c2ind < 0) {
	bu_log("Failed to create edge curve 2 - pipe_brep.cpp:%d\n", __LINE__);
	return false;
    }
    ON_BrepVertex& vert1 = (*b)->NewVertex((*b)->m_C3[c1ind]->PointAt(0), SMALL_FASTF);
    vert1.m_tolerance = 0.0;
    int vert1ind = (*b)->m_V.Count() - 1;
    ON_BrepVertex& vert2 = (*b)->NewVertex((*b)->m_C3[c2ind]->PointAt(0), SMALL_FASTF);
    vert2.m_tolerance = 0.0;
    int vert2ind = (*b)->m_V.Count() - 1;
    ON_BrepEdge* startedge = &(*b)->NewEdge((*b)->m_V[vert1ind], (*b)->m_V[vert1ind], c1ind);
    startedge->m_tolerance = 0.0;
    ON_BrepEdge* endedge = &(*b)->NewEdge((*b)->m_V[vert2ind], (*b)->m_V[vert2ind], c2ind);
    endedge->m_tolerance = 0.0;
    // startedge might point to the wrong place if adding endedge expands the capacity
    // of the edge array, so we need to fix it.
    startedge = (*b)->Edge(startedge->m_edge_index);
    ON_BrepFace *newouterface = (*b)->NewRuledFace(*startedge, false, *endedge, false);
    if (!newouterface)
	return false;
    (*b)->FlipFace(*newouterface);

    if (startinnercurves->Count() > 0) {
	int c3ind = (*b)->AddEdgeCurve((*startinnercurves)[0]);
	if (c3ind < 0) {
	    bu_log("Failed to create edge curve 3 - pipe_brep.cpp:%d\n", __LINE__);
	    return false;
	}
	int c4ind = (*b)->AddEdgeCurve((*endinnercurves)[0]);
	if (c4ind < 0) {
	    bu_log("Failed to create edge curve 4 - pipe_brep.cpp:%d\n", __LINE__);
	    return false;
	}
	ON_BrepVertex& vert3 = (*b)->NewVertex((*b)->m_C3[c3ind]->PointAt(0), SMALL_FASTF);
	vert3.m_tolerance = 0.0;
	int vert3ind = (*b)->m_V.Count() - 1;
	ON_BrepVertex& vert4 = (*b)->NewVertex((*b)->m_C3[c4ind]->PointAt(0), SMALL_FASTF);
	vert4.m_tolerance = 0.0;
	int vert4ind = (*b)->m_V.Count() - 1;
	ON_BrepEdge* startinneredge = &(*b)->NewEdge((*b)->m_V[vert3ind], (*b)->m_V[vert3ind], c3ind);
	startinneredge->m_tolerance = 0.0;
	ON_BrepEdge* endinneredge = &(*b)->NewEdge((*b)->m_V[vert4ind], (*b)->m_V[vert4ind], c4ind);
	endinneredge->m_tolerance = 0.0;
	if (!(*b)->NewRuledFace(*startinneredge, false, *endinneredge, false))
	    return false;
    }

    ON_Curve *next_outer = (*endoutercurves)[0]->DuplicateCurve();
    if (!next_outer)
	return false;
    ON_Curve *next_inner = NULL;
    if (endinnercurves->Count() > 0) {
	next_inner = (*endinnercurves)[0]->DuplicateCurve();
	if (!next_inner) {
	    delete next_outer;
	    return false;
	}
    }
    startoutercurves->Empty();
    startoutercurves->Append(next_outer);
    startinnercurves->Empty();
    if (next_inner)
	startinnercurves->Append(next_inner);
    return true;
}


static bool
make_bend_surface(const ON_Curve &start_curve, const ON_Line &axis,
	fastf_t angle, double tolerance, ON_Brep **bend, ON_Curve **end_curve)
{
    *bend = NULL;
    *end_curve = NULL;
    ON_Curve *profile = start_curve.DuplicateCurve();
    if (!profile)
	return false;
    ON_RevSurface* revsurf = ON_RevSurface::New();
    revsurf->m_curve = profile;
    revsurf->m_axis = axis;
    revsurf->m_angle = ON_Interval(2*ON_PI - angle, 2*ON_PI);
    revsurf->m_t = revsurf->m_angle;
    revsurf->m_bTransposed = false;
    revsurf->BoundingBox();
    *bend = ON_BrepRevSurface(revsurf, false, false, NULL);
    if (!*bend) {
	delete revsurf;
	return false;
    }
    if ((*bend)->m_F.Count() != 1) {
	delete *bend;
	*bend = NULL;
	return false;
    }
    for (int i = 0; i < (*bend)->m_E.Count(); ++i) {
	const ON_BrepEdge &edge = (*bend)->m_E[i];
	if (edge.IsClosed() && !brep_curves_coincident(edge,
		start_curve, tolerance, NULL)) {
	    *end_curve = edge.DuplicateCurve();
	    break;
	}
    }
    if (!*end_curve) {
	delete *bend;
	*bend = NULL;
	return false;
    }
    return true;
}


static bool
make_curved_surfaces(ON_Brep **b,
	ON_SimpleArray<ON_Curve*> *startoutercurves,
	ON_SimpleArray<ON_Curve*> *startinnercurves, fastf_t angle,
	point_t bend_center, vect_t norm, const struct bn_tol *tol)
{
    if (startoutercurves->Count() != 1 || startinnercurves->Count() > 1)
	return false;
    point_t rev;
    VADD2(rev, bend_center, norm);
    const ON_Line revaxis{ON_3dPoint(bend_center), ON_3dPoint(rev)};
    const double tolerance = (tol && tol->dist > 0.0) ?
	tol->dist : RT_LEN_TOL;

    ON_Brep *outer_bend = NULL;
    ON_Curve *outer_end = NULL;
    if (!make_bend_surface(*(*startoutercurves)[0], revaxis, angle,
	    tolerance, &outer_bend, &outer_end))
	return false;

    ON_Brep *inner_bend = NULL;
    ON_Curve *inner_end = NULL;
    if (startinnercurves->Count() > 0) {
	if (!make_bend_surface(*(*startinnercurves)[0], revaxis, angle,
		tolerance, &inner_bend, &inner_end)) {
	    delete outer_end;
	    delete outer_bend;
	    return false;
	}
    }

    outer_bend->FlipFace(outer_bend->m_F[0]);
    (*b)->Append(*outer_bend);
    delete outer_bend;
    if (inner_bend) {
	(*b)->Append(*inner_bend);
	delete inner_bend;
    }

    delete (*startoutercurves)[0];
    startoutercurves->Empty();
    startoutercurves->Append(outer_end);
    if (startinnercurves->Count() > 0) {
	delete (*startinnercurves)[0];
	startinnercurves->Empty();
	startinnercurves->Append(inner_end);
    }
    return true;
}


extern "C" void
rt_pipe_brep(ON_Brep **b, const struct rt_db_internal *ip, const struct bn_tol *tol)
{
    struct rt_pipe_internal *pip;

    struct wdb_pipe_pnt *prevp;
    struct wdb_pipe_pnt *curp;
    struct wdb_pipe_pnt *nextp;
    point_t current_point;
    vect_t x_dir, y_dir, pipe_dir;

    ON_SimpleArray<ON_Curve*> startoutercurves;
    ON_SimpleArray<ON_Curve*> startinnercurves;

    ON_SimpleArray<ON_Curve*> endoutercurves;
    ON_SimpleArray<ON_Curve*> endinnercurves;

    ON_3dPoint plane_origin;
    ON_3dVector plane_x_dir, plane_y_dir;

    ON_Plane endplane;
    ON_BrepLoop *bloop;

    RT_CK_DB_INTERNAL(ip);
    const struct rt_pipe_internal *source_pipe =
	(const struct rt_pipe_internal *)ip->idb_ptr;
    RT_PIPE_CK_MAGIC(source_pipe);

    /* Work from a local point list so duplicate cleanup never mutates the
     * database primitive supplied by the caller. */
    size_t source_point_count = 0;
    struct wdb_pipe_pnt *source_point;
    for (BU_LIST_FOR(source_point, wdb_pipe_pnt,
	    &source_pipe->pipe_segs_head))
	++source_point_count;
    std::vector<struct wdb_pipe_pnt> pipe_points;
    pipe_points.reserve(source_point_count);
    for (BU_LIST_FOR(source_point, wdb_pipe_pnt,
	    &source_pipe->pipe_segs_head)) {
	if (!pipe_points.empty()) {
	    vect_t delta;
	    VSUB2(delta, pipe_points.back().pp_coord, source_point->pp_coord);
	    if (VNEAR_ZERO(delta, RT_LEN_TOL))
		continue;
	}
	pipe_points.push_back(*source_point);
    }
    struct rt_pipe_internal working_pipe = {};
    working_pipe.pipe_magic = RT_PIPE_INTERNAL_MAGIC;
    BU_LIST_INIT(&working_pipe.pipe_segs_head);
    working_pipe.pipe_count = static_cast<int>(pipe_points.size());
    for (size_t i = 0; i < pipe_points.size(); ++i) {
	BU_LIST_INIT(&pipe_points[i].l);
	BU_LIST_INSERT(&working_pipe.pipe_segs_head, &pipe_points[i].l);
    }
    pip = &working_pipe;

    startoutercurves.SetCapacity(1);

    // make the first plane surface
    if (BU_LIST_IS_EMPTY(&pip->pipe_segs_head)) return;
    prevp = BU_LIST_FIRST(wdb_pipe_pnt, &pip->pipe_segs_head);
    curp = BU_LIST_NEXT(wdb_pipe_pnt, &prevp->l);
    nextp = BU_LIST_NEXT(wdb_pipe_pnt, &curp->l);
    if (BU_LIST_IS_HEAD(&curp->l, &pip->pipe_segs_head)) return;

    VMOVE(current_point, curp->pp_coord);

    VSUB2(pipe_dir, prevp->pp_coord, curp->pp_coord);
    bn_vec_ortho(x_dir, pipe_dir);
    VCROSS(y_dir, pipe_dir, x_dir);
    VUNITIZE(y_dir);

    plane_origin = ON_3dPoint(prevp->pp_coord);
    plane_x_dir = ON_3dVector(x_dir);
    plane_y_dir = ON_3dVector(y_dir);
    endplane = ON_Plane(plane_origin, plane_x_dir, plane_y_dir);

    generate_curves(prevp->pp_id, prevp->pp_od, &endplane, &endoutercurves, &endinnercurves);

    ON_PlaneSurface* bp = new ON_PlaneSurface();
    bp->m_plane = endplane;
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
	    endplane = ON_Plane(plane_origin, plane_x_dir, plane_y_dir);
	    generate_curves(curp->pp_id, curp->pp_od, &endplane, &endoutercurves, &endinnercurves);
	    if (!make_linear_surfaces(b, &startoutercurves, &endoutercurves,
		    &startinnercurves, &endinnercurves)) {
		bu_log("rt_pipe_brep: could not construct terminal ruled surfaces\n");
		(*b)->Destroy();
		return;
	    }
	    break;
	}

	VSUB2(n1, prevp->pp_coord, curp->pp_coord);
	if (!(VNEAR_ZERO(n1, RT_LEN_TOL))) {
	    // isn't duplicate point, proceed
	    VSUB2(n2, nextp->pp_coord, curp->pp_coord);
	    VCROSS(norm, n1, n2);
	    VUNITIZE(n1);
	    VUNITIZE(n2);
	    angle = M_PI - acos(VDOT(n1, n2));
	    dist_to_bend = curp->pp_bendradius * tan(angle/2.0);

	    if (std::isnan(dist_to_bend) ||
		VNEAR_ZERO(norm, SQRT_SMALL_FASTF) ||
		NEAR_ZERO(dist_to_bend, SQRT_SMALL_FASTF)) {
		// points are collinear, treat as linear segment
		VSUB2(pipe_dir, current_point, curp->pp_coord);
		bn_vec_ortho(x_dir, pipe_dir);
		VCROSS(y_dir, pipe_dir, x_dir);
		VUNITIZE(y_dir);
		plane_origin = ON_3dPoint(curp->pp_coord);
		plane_x_dir = ON_3dVector(x_dir);
		plane_y_dir = ON_3dVector(y_dir);
		endplane = ON_Plane(plane_origin, plane_x_dir, plane_y_dir);
		generate_curves(curp->pp_id, curp->pp_od, &endplane, &endoutercurves, &endinnercurves);
		if (!make_linear_surfaces(b, &startoutercurves,
			&endoutercurves, &startinnercurves,
			&endinnercurves)) {
		    bu_log("rt_pipe_brep: could not construct ruled surfaces\n");
		    (*b)->Destroy();
		    return;
		}
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
		endplane = ON_Plane(plane_origin, plane_x_dir, plane_y_dir);
		generate_curves(curp->pp_id, curp->pp_od, &endplane, &endoutercurves, &endinnercurves);
		if (!make_linear_surfaces(b, &startoutercurves,
			&endoutercurves, &startinnercurves,
			&endinnercurves)) {
		    bu_log("rt_pipe_brep: could not construct ruled surfaces before bend\n");
		    (*b)->Destroy();
		    return;
		}

		// Now do curved section
		VJOIN1(bend_end, curp->pp_coord, dist_to_bend, n2);
		VCROSS(v1, n1, norm);
		VJOIN1(bend_center, bend_start, -curp->pp_bendradius, v1);
		if (!make_curved_surfaces(b, &startoutercurves,
			&startinnercurves, angle, bend_center, norm, tol)) {
		    bu_log("rt_pipe_brep: could not construct bend surfaces\n");
		    (*b)->Destroy();
		    return;
		}

		VMOVE(current_point, bend_end);
	    }
	}
	prevp = curp;
	curp = nextp;
	nextp = BU_LIST_NEXT(wdb_pipe_pnt, &curp->l);
    }
    // In the case of the final segment, also create the end face.
    for (int i = 0; i < startoutercurves.Count(); ++i)
	delete startoutercurves[i];
    for (int i = 0; i < startinnercurves.Count(); ++i)
	delete startinnercurves[i];
    startoutercurves.Empty();
    startinnercurves.Empty();
    endoutercurves.Empty();
    endinnercurves.Empty();

    /* Match the terminal ruled surfaces' section frame so the cap circles
     * use the same seam vertices.  Reversing this vector produces the same
     * circle loci with seams rotated by 180 degrees, leaving naked edges. */
    VSUB2(pipe_dir, prevp->pp_coord, curp->pp_coord);
    bn_vec_ortho(x_dir, pipe_dir);
    VCROSS(y_dir, pipe_dir, x_dir);
    VUNITIZE(y_dir);
    plane_origin = ON_3dPoint(curp->pp_coord);
    plane_x_dir = ON_3dVector(x_dir);
    plane_y_dir = ON_3dVector(y_dir);
    endplane = ON_Plane(plane_origin, plane_x_dir, plane_y_dir);

    generate_curves(curp->pp_id, curp->pp_od, &endplane, &endoutercurves, &endinnercurves);

    ON_PlaneSurface* ebp = new ON_PlaneSurface();
    ebp->m_plane = endplane;
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
    (*b)->FlipFace(ebface);
    (*b)->SetTrimIsoFlags(ebface);
    for (int i = 0; i < endoutercurves.Count(); ++i)
	delete endoutercurves[i];
    for (int i = 0; i < endinnercurves.Count(); ++i)
	delete endinnercurves[i];
    endoutercurves.Empty();
    endinnercurves.Empty();

    int naked_edge_count = 0;
    for (int i = 0; i < (*b)->m_E.Count(); ++i)
	if ((*b)->m_E[i].m_ti.Count() == 1)
	    ++naked_edge_count;
    const double assembly_tolerance = (tol && tol->dist > 0.0) ?
	tol->dist : RT_LEN_TOL;
    const int merged_edge_count = brep_stitch_naked_edges(**b,
	assembly_tolerance);
    (*b)->Compact();
    (*b)->SetTolerancesBoxesAndFlags(false);
    const bool oriented = brep_orient_faces(**b);
    const double model_tolerance = (tol && tol->dist > 0.0) ? tol->dist : RT_LEN_TOL;
    for (int i = 0; i < (*b)->m_E.Count(); ++i)
	if ((*b)->m_E[i].m_tolerance < 0.0)
	    (*b)->m_E[i].m_tolerance = model_tolerance;
    ON_wString messages;
    ON_TextLog log(messages);
    const bool valid = (*b)->IsValid(&log);
    const bool solid = (*b)->IsSolid();
    if (naked_edge_count < 2 || (naked_edge_count % 2) != 0 ||
	merged_edge_count != naked_edge_count / 2 ||
	!oriented || !valid || !solid) {
	ON_String text(messages);
	bu_log("rt_pipe_brep: generated BRep is not a valid manifold solid "
	    "(naked=%d, merged=%d, oriented=%d, valid=%d, solid=%d):\n%s",
	    naked_edge_count, merged_edge_count, oriented, valid, solid,
	    text.Array());
	for (int i = 0; i < (*b)->m_E.Count(); ++i) {
	    ON_BrepEdge &edge = (*b)->m_E[i];
	    if (edge.m_ti.Count() != 1)
		continue;
	    const ON_Interval domain = edge.Domain();
	    const ON_3dPoint start = edge.PointAt(domain.Min());
	    const ON_3dPoint middle = edge.PointAt(domain.Mid());
	    bu_log("rt_pipe_brep: naked edge %d start {%g %g %g} "
		"middle {%g %g %g}\n", i, start.x, start.y, start.z,
		middle.x, middle.y, middle.z);
	}
	(*b)->Destroy();
	return;
    }
}


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
