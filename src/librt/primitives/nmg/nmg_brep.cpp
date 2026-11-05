/*                    N M G _ B R E P . C P P
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
/** @file nmg_brep.cpp
 *
 * b-rep support for NMG
 *
 */

#include "common.h"

#include <cmath>
#include <limits>
#include <vector>

#include "raytrace.h"
#include "rt/geom.h"
#include "wdb.h"
#include "bn.h"
#include "bg.h"
#include "bu/ptbl.h"
#include "bu/malloc.h"

/**
 *  Routine to create a planar NURBS surface from 4 points
 */
static ON_Surface*
sideSurface(const ON_3dPoint& SW, const ON_3dPoint& SE, const ON_3dPoint& NE, const ON_3dPoint& NW)
{
    ON_NurbsSurface *surf = ON_NurbsSurface::New(3, 0, 2, 2, 2, 2);
    surf->SetCV(0, 0, SW);
    surf->SetCV(1, 0, SE);
    surf->SetCV(1, 1, NE);
    surf->SetCV(0, 1, NW);
    surf->SetKnot(0, 0, 0.0);
    surf->SetKnot(0, 1, 1.0);
    surf->SetKnot(1, 0, 0.0);
    surf->SetKnot(1, 1, 1.0);
    return surf;
}


struct nmg_brep_loop
{
    struct loopuse *lu;
    std::vector<ON_2dPoint> points;
    double area;
};


static ON_2dPoint
nmg_brep_project_point(const point_t point, const point_t origin,
	const vect_t u_axis, const vect_t v_axis, double u_length,
	double v_length)
{
    vect_t offset;
    VSUB2(offset, point, origin);
    return ON_2dPoint(VDOT(offset, u_axis) / u_length,
	VDOT(offset, v_axis) / v_length);
}


static bool
nmg_brep_point_on_segment(const ON_2dPoint &point, const ON_2dPoint &first,
	const ON_2dPoint &second, double tolerance)
{
    const double dx = second.x - first.x;
    const double dy = second.y - first.y;
    const double length_squared = dx * dx + dy * dy;
    const double tolerance_squared = tolerance * tolerance;

    if (length_squared <= tolerance_squared) {
	const double px = point.x - first.x;
	const double py = point.y - first.y;
	return px * px + py * py <= tolerance_squared;
    }

    const double parameter = ((point.x - first.x) * dx +
	(point.y - first.y) * dy) / length_squared;
    if (parameter < 0.0 || parameter > 1.0)
	return false;

    const double closest_x = first.x + parameter * dx;
    const double closest_y = first.y + parameter * dy;
    const double px = point.x - closest_x;
    const double py = point.y - closest_y;
    return px * px + py * py <= tolerance_squared;
}


static bool
nmg_brep_point_in_loop(const ON_2dPoint &point,
	const std::vector<ON_2dPoint> &polygon, double tolerance)
{
    bool inside = false;

    if (polygon.size() < 3)
	return false;

    for (size_t current = 0, previous = polygon.size() - 1;
	 current < polygon.size(); previous = current++)
    {
	const ON_2dPoint &a = polygon[previous];
	const ON_2dPoint &b = polygon[current];
	if (nmg_brep_point_on_segment(point, a, b, tolerance))
	    return true;
	if ((a.y > point.y) != (b.y > point.y) &&
	    point.x < (b.x - a.x) * (point.y - a.y) /
		(b.y - a.y) + a.x)
	{
	    inside = !inside;
	}
    }
    return inside;
}


static bool
nmg_brep_loop_contains(const nmg_brep_loop &outer,
	const nmg_brep_loop &inner, double tolerance)
{
    /* Valid NMG loops do not cross.  Requiring every inner-loop vertex to be
     * inside (or within tolerance of) the outer loop avoids assigning a
     * malformed, partially overlapping loop by a fortuitous sample point. */
    for (const ON_2dPoint &point : inner.points) {
	if (!nmg_brep_point_in_loop(point, outer.points, tolerance))
	    return false;
    }
    return true;
}


static int
nmg_brep_add_loop(ON_Brep *brep, ON_Surface *surface, ON_BrepFace &face,
	const nmg_brep_loop &loop_data, ON_BrepLoop::TYPE loop_type,
	const point_t origin, const vect_t u_axis, const vect_t v_axis,
	double u_length, double v_length, long *brepi)
{
    struct edgeuse *eu;
    double u0, u1, v0, v1;
    surface->GetDomain(0, &u0, &u1);
    surface->GetDomain(1, &v0, &v1);

    ON_BrepLoop& loop = brep->NewLoop(loop_type, face);
    for (BU_LIST_FOR(eu, edgeuse, &loop_data.lu->down_hd)) {
	struct vertex_g *vg1 = eu->vu_p->v_p->vg_p;
	struct vertex_g *vg2 = eu->eumate_p->vu_p->v_p->vg_p;
	NMG_CK_VERTEX_G(vg1);
	NMG_CK_VERTEX_G(vg2);
	if (brepi[vg1->index] == -INT_MAX || brepi[vg2->index] == -INT_MAX)
	    return -1;

	if (brepi[eu->e_p->index] == -INT_MAX) {
	    /* Always add edges with the smaller NMG vertex index first. */
	    int vert1 = (vg1->index <= vg2->index) ?
		brepi[vg1->index] : brepi[vg2->index];
	    int vert2 = (vg1->index > vg2->index) ?
		brepi[vg1->index] : brepi[vg2->index];
	    ON_Curve *curve = new ON_LineCurve(brep->m_V[vert1].Point(),
		brep->m_V[vert2].Point());
	    curve->SetDomain(0.0, 1.0);
	    brep->m_C3.Append(curve);
	    ON_BrepEdge& edge = brep->NewEdge(brep->m_V[vert1],
		brep->m_V[vert2], brep->m_C3.Count() - 1);
	    edge.m_tolerance = 0.0;
	    brepi[eu->e_p->index] = edge.m_edge_index;
	}

	ON_BrepEdge &edge = brep->m_E[(int)brepi[eu->e_p->index]];
	const bool reversed = brepi[vg1->index] != edge.m_vi[0];
	const ON_2dPoint first = nmg_brep_project_point(vg1->coord, origin,
	    u_axis, v_axis, u_length, v_length);
	const ON_2dPoint second = nmg_brep_project_point(vg2->coord, origin,
	    u_axis, v_axis, u_length, v_length);
	const ON_2dPoint from_uv(u0 + first.x / u_length * (u1 - u0),
	    v0 + first.y / v_length * (v1 - v0));
	const ON_2dPoint to_uv(u0 + second.x / u_length * (u1 - u0),
	    v0 + second.y / v_length * (v1 - v0));
	ON_Curve *curve = new ON_LineCurve(from_uv, to_uv);
	curve->SetDomain(0.0, 1.0);
	const int curve_index = brep->m_C2.Count();
	brep->m_C2.Append(curve);
	ON_BrepTrim& trim = brep->NewTrim(edge, reversed, loop, curve_index);
	trim.m_tolerance[0] = 0.0;
	trim.m_tolerance[1] = 0.0;
    }
    return 0;
}

static int
nmg_brep_face(ON_Brep **b, const struct faceuse *fu, const struct bn_tol *tol,
	long *brepi)
{
    if (b == NULL || *b == NULL || fu == NULL || fu->f_p == NULL ||
	tol == NULL || brepi == NULL)
    {
	return -1;
    }
    if (fu->f_p->g.magic_p == NULL ||
	*fu->f_p->g.magic_p != NMG_FACE_G_PLANE_MAGIC)
    {
	bu_log("NMG faceuse #%lu has no supported planar face geometry.\n",
	    fu->index);
	return -1;
    }
    const struct face_g_plane *fg = fu->f_p->g.plane_p;
    struct bu_ptbl vert_table;
    struct vertex **pt;
    int ret = 0;
    int pnt_cnt = 0;
    int pnt_index = 0;
    vect_t u_axis, v_axis;
    point_t obr_center;
    point_t *points_3d = NULL;
    point_t *points_obr = NULL;
    struct loopuse *lu;

    /* Find out how many points we have, set up any uninitialized ON_Brep vertex
     * structures, and prepare a map of NMG index values to the point array indices */
    nmg_tabulate_face_g_verts(&vert_table, fg);

    for (BU_PTBL_FOR(pt, (struct vertex **), &vert_table)) {
	if ((*pt)->vg_p == NULL) {
	    bu_log("NMG faceuse #%lu has a vertex without geometry.\n",
		fu->index);
	    bu_ptbl_free(&vert_table);
	    return -1;
	}
	if (brepi[(*pt)->vg_p->index] == -INT_MAX) {
	    ON_3dPoint vp((*pt)->vg_p->coord);
	    ON_BrepVertex& vert = (*b)->NewVertex(vp, SMALL_FASTF);
	    brepi[(*pt)->vg_p->index] = vert.m_vertex_index;
	}
	pnt_cnt++;
    }

    if (pnt_cnt < 3) {
	bu_log("NMG faceuse #%lu has fewer than three geometric vertices.\n",
	    fu->index);
	bu_ptbl_free(&vert_table);
	return -1;
    }

    /* Prepare the 3D obr input array */
    points_3d = (point_t *)bu_calloc(pnt_cnt + 1, sizeof(point_t), "nmg points");
    for (BU_PTBL_FOR(pt, (struct vertex **), &vert_table)) {
	VSET(points_3d[pnt_index], (*pt)->vg_p->coord[0],(*pt)->vg_p->coord[1],(*pt)->vg_p->coord[2]);
	pnt_index++;
    }
    bu_ptbl_free(&vert_table);


    /* Calculate the 3D coplanar oriented bounding rectangle (obr) */
    ret += bg_3d_coplanar_obr(&obr_center, &u_axis, &v_axis, (const point_t *)points_3d, pnt_cnt);
    if (ret) {
	bu_log("Failed to get oriented bounding rectangle for NMG faceuse #%lu\n", fu->index);
	bu_free(points_3d, "done with failed obr 3d point inputs");
	return -1;
    }
    bu_free(points_3d, "done with obr 3d point inputs");

    /* Use the obr to define the 3D corner points of the NURBS surface */
    points_obr = (point_t *)bu_calloc(3 + 1, sizeof(point_t), "points_3d");
    VADD3(points_obr[2], obr_center, u_axis, v_axis);
    VSCALE(u_axis, u_axis, -1);
    VADD3(points_obr[3], obr_center, u_axis, v_axis);
    VSCALE(v_axis, v_axis, -1);
    VADD3(points_obr[0], obr_center, u_axis, v_axis);
    VSCALE(u_axis, u_axis, -1);
    VADD3(points_obr[1], obr_center, u_axis, v_axis);

    /* We need to orient our surface correctly according to the NMG - using
     * the openNURBS FlipFace function later does not seem to work very
     * well. If an outer loop is found in the NMG with a cw orientation,
     * factor that in in addition to the fu->f_p->flip flag. */
    int ccw = 0;
    vect_t vtmp, uv1, uv2, vnormal;
    point_t center;
    VADD2(center, points_obr[0], points_obr[1]);
    VADD2(center, center, points_obr[2]);
    VADD2(center, center, points_obr[3]);
    VSCALE(center, center, 0.25);
    for (BU_LIST_FOR(lu, loopuse, &fu->lu_hd)) {
	if (lu->orientation == OT_SAME && nmg_loop_is_ccw(lu, fg->N, tol) == -1) ccw = -1;
    }
    if (ccw != -1) {
	VSET(vnormal, fg->N[0], fg->N[1], fg->N[2]);
    } else {
	VSET(vnormal, -fg->N[0], -fg->N[1], -fg->N[2]);
    }
    if (fu->f_p->flip)
	VSET(vnormal, -vnormal[0], -vnormal[1], -vnormal[2]);
    VSUB2(uv1, points_obr[0], center);
    VSUB2(uv2, points_obr[1], center);
    VCROSS(vtmp, uv1, uv2);
    if (VDOT(vtmp, vnormal) < 0) {
	VMOVE(vtmp, points_obr[0]);
	VMOVE(points_obr[0], points_obr[1]);
	VMOVE(points_obr[1], vtmp);
	VMOVE(vtmp, points_obr[3]);
	VMOVE(points_obr[3], points_obr[2]);
	VMOVE(points_obr[2], vtmp);
    }

    /* Now that we've got our points correctly oriented for
     * the NURBS surface, proceed to create it. */
    ON_3dPoint p1 = ON_3dPoint(points_obr[0]);
    ON_3dPoint p2 = ON_3dPoint(points_obr[1]);
    ON_3dPoint p3 = ON_3dPoint(points_obr[2]);
    ON_3dPoint p4 = ON_3dPoint(points_obr[3]);

    /* Project the loops into the surface plane.  NMG permits one faceuse to
     * contain multiple disjoint OT_SAME loops, whereas OpenNURBS requires
     * exactly one outer loop per face.  Preserve the common planar surface,
     * but build one B-Rep face for each outer loop and attach every hole to
     * the smallest outer loop that contains it. */
    VSUB2(u_axis, points_obr[1], points_obr[0]);
    VSUB2(v_axis, points_obr[3], points_obr[0]);
    const fastf_t u_axis_dist = MAGNITUDE(u_axis);
    const fastf_t v_axis_dist = MAGNITUDE(v_axis);
    if (u_axis_dist <= SMALL_FASTF || v_axis_dist <= SMALL_FASTF) {
	bu_free(points_obr, "Done with obr");
	return -1;
    }

    std::vector<nmg_brep_loop> outer_loops;
    std::vector<nmg_brep_loop> inner_loops;
    for (BU_LIST_FOR(lu, loopuse, &fu->lu_hd)) {
	if (BU_LIST_FIRST_MAGIC(&lu->down_hd) != NMG_EDGEUSE_MAGIC) {
	    bu_log("NMG faceuse #%lu has a degenerate non-edge loop.\n",
		fu->index);
	    bu_free(points_obr, "Done with obr");
	    return -1;
	}
	if (lu->orientation != OT_SAME && lu->orientation != OT_OPPOSITE) {
	    bu_free(points_obr, "Done with obr");
	    return -1;
	}

	nmg_brep_loop loop_data = {lu, {}, 0.0};
	struct edgeuse *eu;
	for (BU_LIST_FOR(eu, edgeuse, &lu->down_hd)) {
	    struct vertex_g *geometry = eu->vu_p->v_p->vg_p;
	    NMG_CK_VERTEX_G(geometry);
	    loop_data.points.push_back(nmg_brep_project_point(geometry->coord,
		points_obr[0], u_axis, v_axis, u_axis_dist, v_axis_dist));
	}
	if (loop_data.points.size() < 3) {
	    bu_free(points_obr, "Done with obr");
	    return -1;
	}
	for (size_t i = 0; i < loop_data.points.size(); ++i) {
	    const ON_2dPoint &a = loop_data.points[i];
	    const ON_2dPoint &next = loop_data.points[
		(i + 1) % loop_data.points.size()];
	    loop_data.area += a.x * next.y - next.x * a.y;
	}
	loop_data.area = std::fabs(0.5 * loop_data.area);
	if (loop_data.area <= tol->dist_sq) {
	    bu_free(points_obr, "Done with obr");
	    return -1;
	}
	if (lu->orientation == OT_SAME)
	    outer_loops.push_back(std::move(loop_data));
	else
	    inner_loops.push_back(std::move(loop_data));
    }

    if (outer_loops.empty()) {
	bu_free(points_obr, "Done with obr");
	return -1;
    }

    std::vector<std::vector<size_t>> outer_holes(outer_loops.size());
    for (size_t inner_index = 0; inner_index < inner_loops.size(); ++inner_index) {
	size_t containing_outer = outer_loops.size();
	double containing_area = std::numeric_limits<double>::max();
	for (size_t outer_index = 0; outer_index < outer_loops.size(); ++outer_index) {
	    if (outer_loops[outer_index].area >= containing_area ||
		!nmg_brep_loop_contains(outer_loops[outer_index],
		    inner_loops[inner_index], tol->dist))
	    {
		continue;
	    }
	    containing_outer = outer_index;
	    containing_area = outer_loops[outer_index].area;
	}
	if (containing_outer == outer_loops.size()) {
	    bu_log("NMG faceuse #%lu has an inner loop outside every outer loop.\n",
		fu->index);
	    bu_free(points_obr, "Done with obr");
	    return -1;
	}
	outer_holes[containing_outer].push_back(inner_index);
    }

    (*b)->m_S.Append(sideSurface(p1, p2, p3, p4));
    ON_Surface *surf = (*(*b)->m_S.Last());
    const int surface_index = (*b)->m_S.Count() - 1;
    for (size_t outer_index = 0; outer_index < outer_loops.size(); ++outer_index) {
	ON_BrepFace& face = (*b)->NewFace(surface_index);
	if (nmg_brep_add_loop(*b, surf, face, outer_loops[outer_index],
		ON_BrepLoop::outer, points_obr[0], u_axis, v_axis,
		u_axis_dist, v_axis_dist, brepi))
	{
	    bu_free(points_obr, "Done with obr");
	    return -1;
	}
	for (size_t inner_index : outer_holes[outer_index]) {
	    if (nmg_brep_add_loop(*b, surf, face, inner_loops[inner_index],
		    ON_BrepLoop::inner, points_obr[0], u_axis, v_axis,
		    u_axis_dist, v_axis_dist, brepi))
	    {
		bu_free(points_obr, "Done with obr");
		return -1;
	    }
	}
    }

    bu_free(points_obr, "Done with obr");

    return 0;
}


extern "C" void
rt_nmg_brep(ON_Brep **b, const struct rt_db_internal *ip, const struct bn_tol *tol)
{
    struct model *m;
    struct nmgregion *r;
    struct shell *s;
    struct faceuse *fu;

    if (b == NULL || *b == NULL || ip == NULL || tol == NULL)
	return;

    // Verify NMG
    RT_CK_DB_INTERNAL(ip);
    m = (struct model *)ip->idb_ptr;
    NMG_CK_MODEL(m);

    // Both NMG and brep structures reuse components between faces.  In order to track
    // when the conversion routine has already handled an NMG element, use an array.
    long *brepi = static_cast<long*>(bu_malloc(m->maxindex * sizeof(long), "rt_nmg_brep: brepi[]"));
    for (int i = 0; i < m->maxindex; i++) brepi[i] = -INT_MAX;

    // Iterate over all faces in the NMG
    for (BU_LIST_FOR(r, nmgregion, &m->r_hd)) {
	for (BU_LIST_FOR(s, shell, &r->s_hd)) {
	    for (BU_LIST_FOR(fu, faceuse, &s->fu_hd)) {
		NMG_CK_FACEUSE(fu);
		if (fu->orientation != OT_SAME) continue;
		if (nmg_brep_face(b, fu, tol, brepi)) {
		    (*b)->Destroy();
		    bu_free(brepi, "rt_nmg_brep: brepi[]");
		    return;
		}
	    }
	    (*b)->SetTrimIsoFlags();
	}
    }
    (*b)->SetTrimTypeFlags(false);
    bu_free(brepi, "rt_nmg_brep: brepi[]");
}


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
