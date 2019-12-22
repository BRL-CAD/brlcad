/*              C D T _ T R I _ I S E C T . C P P
 * BRL-CAD
 *
 * Copyright (c) 2007-2019 United States Government as represented by
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
/** @addtogroup libbrep */
/** @{ */
/** @file cdt_ovlps.cpp
 *
 * Constrained Delaunay Triangulation of NURBS B-Rep objects.
 *
 * This file defines triangle intersection logic specifically
 * tailored for the needs of CDT overlap resolution.
 *
 */

#include "common.h"
#include <queue>
#include <numeric>
#include <random>
#include "bu/str.h"
#include "bg/chull.h"
#include "bg/tri_pt.h"
#include "bg/tri_tri.h"
#include "./cdt.h"

#if 0

// These are unused for the moment, but will be needed if
// we are forced to do brute force splitting of triangles
// to reduce their size

double
tri_shortest_edge_len(cdt_mesh_t *fmesh, long t_ind)
{
    triangle_t t = fmesh->tris_vect[t_ind];
    double len = DBL_MAX;
    for (int i = 0; i < 3; i++) {
	long v0 = t.v[i];
	long v1 = (i < 2) ? t.v[i + 1] : t.v[0];
	ON_3dPoint *p1 = fmesh->pnts[v0];
	ON_3dPoint *p2 = fmesh->pnts[v1];
	double d = p1->DistanceTo(*p2);
	len = (d < len) ? d : len;
    }

    return len;
}

uedge_t
tri_shortest_edge(cdt_mesh_t *fmesh, long t_ind)
{
    uedge_t ue;
    triangle_t t = fmesh->tris_vect[t_ind];
    double len = DBL_MAX;
    for (int i = 0; i < 3; i++) {
	long v0 = t.v[i];
	long v1 = (i < 2) ? t.v[i + 1] : t.v[0];
	ON_3dPoint *p1 = fmesh->pnts[v0];
	ON_3dPoint *p2 = fmesh->pnts[v1];
	double d = p1->DistanceTo(*p2);
	if (d < len) {
	    len = d;
	    ue = uedge_t(v0, v1);
	}
    }
    return ue;
}

double
tri_longest_edge_len(cdt_mesh_t *fmesh, long t_ind)
{
    triangle_t t = fmesh->tris_vect[t_ind];
    double len = -DBL_MAX;
    for (int i = 0; i < 3; i++) {
	long v0 = t.v[i];
	long v1 = (i < 2) ? t.v[i + 1] : t.v[0];
	ON_3dPoint *p1 = fmesh->pnts[v0];
	ON_3dPoint *p2 = fmesh->pnts[v1];
	double d = p1->DistanceTo(*p2);
	len = (d > len) ? d : len;
    }

    return len;
}

uedge_t
tri_longest_edge(cdt_mesh_t *fmesh, long t_ind)
{
    uedge_t ue;
    triangle_t t = fmesh->tris_vect[t_ind];
    double len = -DBL_MAX;
    for (int i = 0; i < 3; i++) {
	long v0 = t.v[i];
	long v1 = (i < 2) ? t.v[i + 1] : t.v[0];
	ON_3dPoint *p1 = fmesh->pnts[v0];
	ON_3dPoint *p2 = fmesh->pnts[v1];
	double d = p1->DistanceTo(*p2);
	if (d > len) {
	    len = d;
	    ue = uedge_t(v0, v1);
	}
    }
    return ue;
}
#endif

#if 0
ON_BoundingBox
edge_bbox(bedge_seg_t *eseg)
{
    ON_3dPoint *p3d1 = eseg->e_start;
    ON_3dPoint *p3d2 = eseg->e_end;
    ON_Line line(*p3d1, *p3d2);

    ON_BoundingBox bb = line.BoundingBox();
    bb.m_max.x = bb.m_max.x + ON_ZERO_TOLERANCE;
    bb.m_max.y = bb.m_max.y + ON_ZERO_TOLERANCE;
    bb.m_max.z = bb.m_max.z + ON_ZERO_TOLERANCE;
    bb.m_min.x = bb.m_min.x - ON_ZERO_TOLERANCE;
    bb.m_min.y = bb.m_min.y - ON_ZERO_TOLERANCE;
    bb.m_min.z = bb.m_min.z - ON_ZERO_TOLERANCE;

    double dist = p3d1->DistanceTo(*p3d2);
    double bdist = 0.5*dist;
    double xdist = bb.m_max.x - bb.m_min.x;
    double ydist = bb.m_max.y - bb.m_min.y;
    double zdist = bb.m_max.z - bb.m_min.z;
    // If we're close to the edge, we want to know - the Search callback will
    // check the precise distance and make a decision on what to do.
    if (xdist < bdist) {
        bb.m_min.x = bb.m_min.x - 0.5*bdist;
        bb.m_max.x = bb.m_max.x + 0.5*bdist;
    }
    if (ydist < bdist) {
        bb.m_min.y = bb.m_min.y - 0.5*bdist;
        bb.m_max.y = bb.m_max.y + 0.5*bdist;
    }
    if (zdist < bdist) {
        bb.m_min.z = bb.m_min.z - 0.5*bdist;
        bb.m_max.z = bb.m_max.z + 0.5*bdist;
    }

    return bb;
}

ON_3dPoint
lseg_closest_pnt(ON_Line &l, ON_3dPoint &p)
{
    double t;
    l.ClosestPointTo(p, &t);
    if (t > 0 && t < 1) {
	return l.PointAt(t);
    } else {
	double d1 = l.from.DistanceTo(p);
	double d2 = l.to.DistanceTo(p);
	return (d2 < d1) ? l.to : l.from;
    }
}

class tri_dist {
public:
    tri_dist(double idist, long iind) {
	dist = idist;
	ind = iind;
    }
    bool operator< (const tri_dist &b) const {
	return dist < b.dist;
    }
    double dist;
    long ind;
};

#endif

void
isect_plot(cdt_mesh_t *fmesh1, long t1_ind, cdt_mesh_t *fmesh2, long t2_ind, point_t *isectpt1, point_t *isectpt2)
{
    FILE *plot = fopen("tri_pair.plot3", "w");
    double fpnt_r = -1.0;
    double pnt_r = -1.0;
    pl_color(plot, 0, 0, 255);
    fmesh1->plot_tri(fmesh1->tris_vect[t1_ind], NULL, plot, 0, 0, 0);
    pnt_r = fmesh1->tri_pnt_r(t1_ind);
    fpnt_r = (pnt_r > fpnt_r) ? pnt_r : fpnt_r;
    pl_color(plot, 255, 0, 0);
    fmesh2->plot_tri(fmesh2->tris_vect[t2_ind], NULL, plot, 0, 0, 0);
    pnt_r = fmesh2->tri_pnt_r(t2_ind);
    fpnt_r = (pnt_r > fpnt_r) ? pnt_r : fpnt_r;
    pl_color(plot, 255, 255, 255);
    ON_3dPoint p1((*isectpt1)[X], (*isectpt1)[Y], (*isectpt1)[Z]);
    ON_3dPoint p2((*isectpt2)[X], (*isectpt2)[Y], (*isectpt2)[Z]);
    plot_pnt_3d(plot, &p1, fpnt_r, 0);
    plot_pnt_3d(plot, &p2, fpnt_r, 0);
    pdv_3move(plot, *isectpt1);
    pdv_3cont(plot, *isectpt2);
    fclose(plot);
}

int
remote_vert_process(bool pinside, overt_t *v, omesh_t *omesh2)
{
    if (!v) {
	std::cout << "WARNING: - no overt for vertex??\n";
	return 0;
    }
    ON_3dPoint vp = v->vpnt();
    bool near_vert = false;
    std::set<overt_t *> cverts = omesh2->vert_search(v->bb);
    std::set<overt_t *>::iterator v_it;
    for (v_it = cverts.begin(); v_it != cverts.end(); v_it++) {
	ON_3dPoint cvpnt = (*v_it)->vpnt();
	double cvbbdiag = (*v_it)->bb.Diagonal().Length() * 0.1;
	if (vp.DistanceTo(cvpnt) < cvbbdiag) {
	    near_vert = true;
	    break;
	}
    }

    if (!near_vert && pinside) {
	return 1;
    }

    return 0;
}

int
near_edge_process(double t, double vtol)
{
    if (t > 0  && t < 1 && !NEAR_ZERO(t, vtol) && !NEAR_EQUAL(t, 1, vtol)) {
	return 1;
    }
    return 0;
}

static bool
find_intersecting_edges(
	double *lt,
	edge_t &t1_e, edge_t &t2_e,
	ON_Line &t1_nedge, ON_Line &t2_nedge,
	ON_3dPoint &p1, ON_3dPoint &p2,
	ON_Line *t1_lines, ON_Line *t2_lines,
	triangle_t &t1,	triangle_t &t2,
	double etol
	)
{
    double p1d1[3], p1d2[3];
    double p2d1[3], p2d2[3];
    for (int i = 0; i < 3; i++) {
	p1d1[i] = p1.DistanceTo(t1_lines[i].ClosestPointTo(p1));
	p2d1[i] = p2.DistanceTo(t1_lines[i].ClosestPointTo(p2));
	p1d2[i] = p1.DistanceTo(t2_lines[i].ClosestPointTo(p1));
	p2d2[i] = p2.DistanceTo(t2_lines[i].ClosestPointTo(p2));
    }

    bool nedge_1 = false;
    for (int i = 0; i < 3; i++) {
	if (NEAR_ZERO(p1d1[i], etol) &&  NEAR_ZERO(p2d1[i], etol)) {
	    //std::cout << "edge-only intersect - e1\n";
	    t1_nedge = t1_lines[i];
	    int v2 = (i < 2) ? i + 1 : 0;
	    t1_e = edge_t(t1.v[i], t1.v[v2]);
	    nedge_1 = true;
	}
    }

    bool nedge_2 = false;
    for (int i = 0; i < 3; i++) {
	if (NEAR_ZERO(p1d2[i], etol) &&  NEAR_ZERO(p2d2[i], etol)) {
	    //std::cout << "edge-only intersect - e1\n";
	    t2_nedge = t2_lines[i];
	    int v2 = (i < 2) ? i + 1 : 0;
	    t2_e = edge_t(t2.v[i], t2.v[v2]);
	    nedge_2 = true;
	}
    }

    if (!nedge_1 || !nedge_2) {
	return false;
    }

    // If the edges aren't properly aligned and we need the closest points
    double llen = -DBL_MAX;

    t1_nedge.ClosestPointTo(t2_nedge.PointAt(0), &(lt[0]));
    t1_nedge.ClosestPointTo(t2_nedge.PointAt(1), &(lt[1]));
    t2_nedge.ClosestPointTo(t1_nedge.PointAt(0), &(lt[2]));
    t2_nedge.ClosestPointTo(t1_nedge.PointAt(1), &(lt[3]));

    double ll[4];
    ll[0] = t1_nedge.PointAt(0).DistanceTo(t2_nedge.PointAt(lt[2]));
    ll[1] = t1_nedge.PointAt(1).DistanceTo(t2_nedge.PointAt(lt[3]));
    ll[2] = t2_nedge.PointAt(0).DistanceTo(t1_nedge.PointAt(lt[0]));
    ll[3] = t2_nedge.PointAt(1).DistanceTo(t1_nedge.PointAt(lt[1]));
    for (int i = 0; i < 4; i++) {
	llen = (ll[i] > llen) ? ll[i] : llen;
    }
    // Max dist from line too large, not an edge only intersect
    if ( llen > etol ) {
	return false;
    }

    return true;
}

static bool
edge_only_isect(
	omesh_t *omesh1, triangle_t &t1,
	omesh_t *omesh2, triangle_t &t2,
	ON_3dPoint &p1, ON_3dPoint &p2,
	ON_Line *t1_lines,
	ON_Line *t2_lines,
	double etol
	)
{
    ON_Line t1_nedge, t2_nedge;
    edge_t t1_e, t2_e;
    double lt[4];

    // If either triangle thinks this isn't an edge-only intersect, we're done
    if (!find_intersecting_edges((double *)lt, t1_e, t2_e, t1_nedge, t2_nedge,
		p1, p2, t1_lines, t2_lines, t1, t2, etol)) {
	return false;
    }


    // If both points are on the same edge, it's an edge-only intersect.  However,
    // if the vertices are such that we want to process them to align the mesh
    // and avoid sliver overlaps near the edges, we may not be able to report it
    // as such - keep checking.

    // If any close-to-edge vertices from one triangle project on the interior of the
    // nearest edge from the other triangle, report a non-edge intersect.

    int process_pnt = 0;
    double vtol = 0.01;
    overt_t *v = NULL;

    v = omesh2->overts[t2_e.v[0]];
    process_pnt += near_edge_process(lt[0], vtol);

    v = omesh2->overts[t2_e.v[1]];
    process_pnt += near_edge_process(lt[1], vtol);

    v = omesh1->overts[t1_e.v[0]];
    process_pnt += near_edge_process(lt[2], vtol);

    v = omesh1->overts[t1_e.v[1]];
    process_pnt += near_edge_process(lt[3], vtol);

    if (process_pnt) {
	return false;
    }

    // If the non-edge point of one of the triangles is clearly inside
    // the other mesh, we need to flag it for processing as well - there
    // are geometric situations where edge intersection information isn't
    // sufficient to identify all the points to consider.

    long t1_vind, t2_vind;

    // If it is an edge intersect, identify the point from each triangle not
    // on the edge.
    double cdist = -DBL_MAX;
    for (int i = 0; i < 3; i++) {
	ON_3dPoint tp = *omesh1->fmesh->pnts[t1.v[i]];
	double tdist = tp.DistanceTo(t1_nedge.ClosestPointTo(tp));
	if (tdist > cdist) {
	    t1_vind = t1.v[i];
	    cdist = tdist;
	}
    }
    cdist = -DBL_MAX;
    for (int i = 0; i < 3; i++) {
	ON_3dPoint tp = *omesh2->fmesh->pnts[t2.v[i]];
	double tdist = tp.DistanceTo(t2_nedge.ClosestPointTo(tp));
	if (tdist > cdist) {
	    t2_vind = t2.v[i];
	    cdist = tdist;
	}
    }

    // See if the triangles are inside the mesh (center point test).  If so,
    // we have more work to do.
    //
    // We use the center point for this because we are only in this logic
    // due to the triangle intersection reporting the intersection points close
    // to one of the edges.  That being the case, the center should be in the
    // direction of the inside/outside behavior of interest, and not being a
    // vertex it is not likely to be aligned with another unrelated vertex in
    // the opposite mesh due to prior refinement steps.
    struct ON_Brep_CDT_State *s_cdt2 = (struct ON_Brep_CDT_State *)omesh2->fmesh->p_cdt;
    ON_3dPoint t1_f = omesh1->fmesh->tcenter(t1);
    bool t1_pinside = on_point_inside(s_cdt2, &t1_f);
    struct ON_Brep_CDT_State *s_cdt1 = (struct ON_Brep_CDT_State *)omesh1->fmesh->p_cdt;
    ON_3dPoint t2_f = omesh2->fmesh->tcenter(t2);
    bool t2_pinside = on_point_inside(s_cdt1, &t2_f);

    // For each remote vertex, see if it is very close to a vertex on the
    // opposite mesh.  If not, and it is inside the opposite mesh, we
    // can use it as a refinement point.  Otherwise, we need to split
    // triangle edges.
    if (t1_pinside || t2_pinside) {
	v = omesh1->overts[t1_vind];
	process_pnt += remote_vert_process(t1_pinside, v, omesh2);
	v = omesh2->overts[t2_vind];
	process_pnt += remote_vert_process(t2_pinside, v, omesh1);
    }

    if (process_pnt) {
	return false;
    }

    return true;
}

static bool
tri_isect_basic(point_t *isectpt1, point_t *isectpt2, cdt_mesh_t *fmesh1, triangle_t &t1, cdt_mesh_t *fmesh2, triangle_t &t2)
{
    int coplanar = 0;
    point_t T1_V[3];
    point_t T2_V[3];

    VSET(T1_V[0], fmesh1->pnts[t1.v[0]]->x, fmesh1->pnts[t1.v[0]]->y, fmesh1->pnts[t1.v[0]]->z);
    VSET(T1_V[1], fmesh1->pnts[t1.v[1]]->x, fmesh1->pnts[t1.v[1]]->y, fmesh1->pnts[t1.v[1]]->z);
    VSET(T1_V[2], fmesh1->pnts[t1.v[2]]->x, fmesh1->pnts[t1.v[2]]->y, fmesh1->pnts[t1.v[2]]->z);
    VSET(T2_V[0], fmesh2->pnts[t2.v[0]]->x, fmesh2->pnts[t2.v[0]]->y, fmesh2->pnts[t2.v[0]]->z);
    VSET(T2_V[1], fmesh2->pnts[t2.v[1]]->x, fmesh2->pnts[t2.v[1]]->y, fmesh2->pnts[t2.v[1]]->z);
    VSET(T2_V[2], fmesh2->pnts[t2.v[2]]->x, fmesh2->pnts[t2.v[2]]->y, fmesh2->pnts[t2.v[2]]->z);
    int isect = bg_tri_tri_isect_with_line(T1_V[0], T1_V[1], T1_V[2], T2_V[0], T2_V[1], T2_V[2], &coplanar, isectpt1, isectpt2);

    if (!isect) {
	// No intersection - we're done
	return false;
    }

    ON_3dPoint p1((*isectpt1)[X], (*isectpt1)[Y], (*isectpt1)[Z]);
    ON_3dPoint p2((*isectpt2)[X], (*isectpt2)[Y], (*isectpt2)[Z]);
    if (p1.DistanceTo(p2) < ON_ZERO_TOLERANCE) {
	// Intersection is a single point - not volumetric
	return false;
    }

    return true;
}

static double
tri_lines(ON_Line *t1_lines, ON_Line *t2_lines,
	cdt_mesh_t *fmesh1, triangle_t &t1, cdt_mesh_t *fmesh2, triangle_t &t2)
{
    t1_lines[0] = ON_Line(*fmesh1->pnts[t1.v[0]], *fmesh1->pnts[t1.v[1]]);
    t1_lines[1] = ON_Line(*fmesh1->pnts[t1.v[1]], *fmesh1->pnts[t1.v[2]]);
    t1_lines[2] = ON_Line(*fmesh1->pnts[t1.v[2]], *fmesh1->pnts[t1.v[0]]);

    t2_lines[0] = ON_Line(*fmesh2->pnts[t2.v[0]], *fmesh2->pnts[t2.v[1]]);
    t2_lines[1] = ON_Line(*fmesh2->pnts[t2.v[1]], *fmesh2->pnts[t2.v[2]]);
    t2_lines[2] = ON_Line(*fmesh2->pnts[t2.v[2]], *fmesh2->pnts[t2.v[0]]);

    double elen_min = DBL_MAX;
    for (int i = 0; i < 3; i++) {
	elen_min = (elen_min > t1_lines[i].Length()) ? t1_lines[i].Length() : elen_min;
	elen_min = (elen_min > t2_lines[i].Length()) ? t2_lines[i].Length() : elen_min;
    }

    return elen_min;
}

static bool
tri_near_verts_isect(point_t *isectpt1, point_t *isectpt2, cdt_mesh_t *fmesh1, triangle_t &t1, cdt_mesh_t *fmesh2, triangle_t &t2, double etol)
{
    // See if we're intersecting very close to triangle vertices
    bool vert_isect = false;
    double t1_i1_vdists[3];
    double t1_i2_vdists[3];
    double t2_i1_vdists[3];
    double t2_i2_vdists[3];

    ON_3dPoint p1((*isectpt1)[X], (*isectpt1)[Y], (*isectpt1)[Z]);
    ON_3dPoint p2((*isectpt2)[X], (*isectpt2)[Y], (*isectpt2)[Z]);

    for (int i = 0; i < 3; i++) {
	t1_i1_vdists[i] = fmesh1->pnts[t1.v[i]]->DistanceTo(p1);
	t2_i1_vdists[i] = fmesh2->pnts[t2.v[i]]->DistanceTo(p1);
    	t1_i2_vdists[i] = fmesh1->pnts[t1.v[i]]->DistanceTo(p2);
	t2_i2_vdists[i] = fmesh2->pnts[t2.v[i]]->DistanceTo(p2);
    }
    for (int i = 0; i < 3; i++) {
	if (t1_i1_vdists[i] < etol && t1_i2_vdists[i] < etol) {
	    vert_isect = true;
	}
	if (t2_i1_vdists[i] < etol && t2_i2_vdists[i] < etol) {
	    vert_isect = true;
	}
    }

    return vert_isect;
}

/*****************************************************************************
 * We're only concerned with specific categories of intersections between
 * triangles, so filter accordingly.
 * Return 0 if no intersection, 1 if coplanar intersection, 2 if non-coplanar
 * intersection.
 *****************************************************************************/
int
tri_isect(
	omesh_t *omesh1, triangle_t &t1,
       	omesh_t *omesh2, triangle_t &t2
	)
{
    cdt_mesh_t *fmesh1 = omesh1->fmesh;
    cdt_mesh_t *fmesh2 = omesh2->fmesh;
    point_t isectpt1 = {MAX_FASTF, MAX_FASTF, MAX_FASTF};
    point_t isectpt2 = {MAX_FASTF, MAX_FASTF, MAX_FASTF};


    if (!tri_isect_basic(&isectpt1, &isectpt2, fmesh1, t1, fmesh2, t2)) {
	return 0;
    }

    //isect_plot(fmesh1, t1.ind, fmesh2, t2.ind, &isectpt1, &isectpt2);

    // Past the trivial cases - we're going to have to know something about the
    // triangles' dimensions and boundaries to characterize the intersection
    ON_Line t1_lines[3];
    ON_Line t2_lines[3];
    double elen_min = tri_lines((ON_Line *)&t1_lines, (ON_Line *)&t2_lines, fmesh1, t1, fmesh2, t2);
    double etol = 0.0001*elen_min;

    // This category of intersection isn't one that warrants an intersection return.
    if (tri_near_verts_isect(&isectpt1, &isectpt2, fmesh1, t1, fmesh2, t2, etol)) {
	return 0;
    }

    // Check for edge-only intersections
    ON_Line nedge;
    ON_3dPoint p1(isectpt1[X], isectpt1[Y], isectpt1[Z]);
    ON_3dPoint p2(isectpt2[X], isectpt2[Y], isectpt2[Z]);
    bool near_edge_isect = edge_only_isect(omesh1, t1, omesh2, t2, p1, p2, (ON_Line *)t1_lines, (ON_Line *)t2_lines, 0.3*elen_min);

    if (near_edge_isect) {
	return 0;
    }

    return 1;
}

// Assuming t1 and t2 intersect, identify any refinement vertices that need processing
int
tri_nearedge_refine(
	omesh_t *omesh1, triangle_t &t1,
       	omesh_t *omesh2, triangle_t &t2
	)
{
    cdt_mesh_t *fmesh1 = omesh1->fmesh;
    cdt_mesh_t *fmesh2 = omesh2->fmesh;
    point_t isectpt1 = {MAX_FASTF, MAX_FASTF, MAX_FASTF};
    point_t isectpt2 = {MAX_FASTF, MAX_FASTF, MAX_FASTF};

    if (!tri_isect_basic(&isectpt1, &isectpt2, fmesh1, t1, fmesh2, t2)) {
	return 0;
    }
#if 0
    ON_Line t1_lines[3];
    ON_Line t2_lines[3];
    double elen_min = tri_lines(&t1_lines, &t2_lines, fmesh1, t1, fmesh2, t2);
    double etol = 0.0001*elen_min;
#endif

    return 0;
}

/** @} */

// Local Variables:
// mode: C++
// tab-width: 8
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8

