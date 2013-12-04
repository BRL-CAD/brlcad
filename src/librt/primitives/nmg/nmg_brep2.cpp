/*                    N M G _ B R E P . C P P
 * BRL-CAD
 *
 * Copyright (c) 2008-2013 United States Government as represented by
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

#include "raytrace.h"
#include "rtgeom.h"
#include "wdb.h"
#include "bn.h"
#include "bu.h"

#include <map>

/**
 *
 */
HIDDEN ON_Surface*
sideSurface2(const ON_3dPoint& SW, const ON_3dPoint& SE, const ON_3dPoint& NE, const ON_3dPoint& NW)
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

HIDDEN int
nmg_brep_face(ON_Brep **b, const struct faceuse *fu, const struct bn_tol *tol, long *brepi) {
    const struct face_g_plane *fg = fu->f_p->g.plane_p;
    struct bu_ptbl vert_table;
    struct vertex **pt;
    int ret = 0;
    int pnt_cnt = 0;
    point_t origin_pnt;
    vect_t u_axis, v_axis;
    point2d_t obr_2d_center;
    point2d_t obr_2d_v1, obr_2d_v2;
    point_t *points_3d = NULL;
    point2d_t *points_2d = NULL;
    point2d_t *points_obr_2d = NULL;
    point_t *points_obr_3d = NULL;
    std::map<int, int> nmg_to_array;
    struct loopuse *lu;
    struct edgeuse *eu;

    /* Find out how many points we have, set up any uninitialized ON_Brep vertex
     * structures, and prepare a map of NMG index values to the point array indices */
    nmg_tabulate_face_g_verts(&vert_table, fg);

    for (BU_PTBL_FOR(pt, (struct vertex **), &vert_table)) {
	if (nmg_to_array.find((*pt)->vg_p->index) == nmg_to_array.end()) {
	    if (brepi[(*pt)->vg_p->index] == -INT_MAX) {
		ON_BrepVertex& vert = (*b)->NewVertex((*pt)->vg_p->coord, SMALL_FASTF);
		brepi[(*pt)->vg_p->index] = vert.m_vertex_index;
	    }
	    nmg_to_array[(*pt)->vg_p->index] = pnt_cnt;
	    pnt_cnt++;
	}
    }

    /* Prepare the 3D obr input array */
    points_3d = (point_t *)bu_calloc(pnt_cnt + 1, sizeof(point_t), "nmg points");
    for (BU_PTBL_FOR(pt, (struct vertex **), &vert_table)) {
	int pnt_index = nmg_to_array.find((*pt)->vg_p->index)->second;
	VSET(points_3d[pnt_index], (*pt)->vg_p->coord[0],(*pt)->vg_p->coord[1],(*pt)->vg_p->coord[2]);
    }
    bu_ptbl_free(&vert_table);

    /* Translate the 3D array to a 2D array */

    ret += bn_coplanar_2d_coord_sys(&origin_pnt, &u_axis, &v_axis, points_3d, pnt_cnt);
    points_2d = (point2d_t *)bu_calloc(pnt_cnt + 1, sizeof(point2d_t), "points_2d");
    ret += bn_coplanar_3d_to_2d(&points_2d, (const point_t *)&origin_pnt, (const vect_t *)&u_axis, (const vect_t *)&v_axis, points_3d, pnt_cnt);
    if (ret) return 0;

    /* Find the 2D oriented bounding rectangle */
    ret = bn_2d_obr(&obr_2d_center, &obr_2d_v1, &obr_2d_v2, (const point2d_t *)points_2d, pnt_cnt);
    if (ret) return 0;

    /* Use the obr to define the 3D corner points of the NURBS surface */
    points_obr_2d = (point2d_t *)bu_calloc(3 + 1, sizeof(point2d_t), "points_2d");
    points_obr_3d = (point_t *)bu_calloc(3 + 1, sizeof(point_t), "points_3d");
    V2ADD3(points_obr_2d[2], obr_2d_center, obr_2d_v1, obr_2d_v2);
    V2SCALE(obr_2d_v1, obr_2d_v1, -1);
    V2ADD3(points_obr_2d[3], obr_2d_center, obr_2d_v1, obr_2d_v2);
    V2SCALE(obr_2d_v2, obr_2d_v2, -1);
    V2ADD3(points_obr_2d[0], obr_2d_center, obr_2d_v1, obr_2d_v2);
    V2SCALE(obr_2d_v1, obr_2d_v1, -1);
    V2ADD3(points_obr_2d[1], obr_2d_center, obr_2d_v1, obr_2d_v2);

    ret = bn_coplanar_2d_to_3d(&points_obr_3d, (const point_t *)&origin_pnt, (const vect_t *)&u_axis, (const vect_t *)&v_axis, (const point2d_t *)points_obr_2d, 3);

    ON_3dPoint p1 = ON_3dPoint(points_obr_3d[0]);
    ON_3dPoint p2 = ON_3dPoint(points_obr_3d[1]);
    ON_3dPoint p3 = ON_3dPoint(points_obr_3d[2]);
    ON_3dPoint p4 = ON_3dPoint(points_obr_3d[3]);

    (*b)->m_S.Append(sideSurface2(p1, p2, p3, p4));
    ON_Surface *surf = (*(*b)->m_S.Last());
    int surfindex = (*b)->m_S.Count();
    ON_BrepFace& face = (*b)->NewFace(surfindex - 1);

    /* Set surface UV domain to match the 2D domain above */
    point2d_t min2d, max2d;
    V2MINMAX(min2d, max2d, points_obr_2d[0]);
    V2MINMAX(min2d, max2d, points_obr_2d[1]);
    V2MINMAX(min2d, max2d, points_obr_2d[2]);
    V2MINMAX(min2d, max2d, points_obr_2d[3]);
    surf->SetDomain(0, min2d[0], max2d[0]);
    surf->SetDomain(1, min2d[1], max2d[1]);

    /* Now that we have the surface and the face, add the loops */
    for (BU_LIST_FOR(lu, loopuse, &fu->lu_hd)) {
	if (BU_LIST_FIRST_MAGIC(&lu->down_hd) != NMG_EDGEUSE_MAGIC) continue; // loop is a single vertex
	// Check if this is an inner or outer loop
	ON_BrepLoop::TYPE looptype = (lu->orientation == OT_SAME) ? ON_BrepLoop::outer : ON_BrepLoop::inner;
	ON_BrepLoop& loop = (*b)->NewLoop(looptype, face);
	for (BU_LIST_FOR(eu, edgeuse, &lu->down_hd)) {
	    struct vertex_g *vg1 = eu->vu_p->v_p->vg_p;
	    struct vertex_g *vg2 = eu->eumate_p->vu_p->v_p->vg_p;
	    NMG_CK_VERTEX_G(vg1);
	    NMG_CK_VERTEX_G(vg2);
	    // Add edge if not already added
	    if (brepi[eu->e_p->index] == -INT_MAX) {
		/* always add edges with the small vertex index as from */
		int vert1 = (vg1->index <= vg2->index) ? brepi[vg1->index] : brepi[vg2->index];
		int vert2 = (vg1->index > vg2->index) ? brepi[vg1->index] : brepi[vg2->index];
		// Create and add 3D curve
		ON_Curve* c3d = new ON_LineCurve((*b)->m_V[vert1].Point(), (*b)->m_V[vert2].Point());
		c3d->SetDomain(0.0, 1.0);
		(*b)->m_C3.Append(c3d);
		// Create and add 3D edge
		ON_BrepEdge& e = (*b)->NewEdge((*b)->m_V[vert1], (*b)->m_V[vert2] , (*b)->m_C3.Count() - 1);
		e.m_tolerance = 0.0;
		brepi[eu->e_p->index] = e.m_edge_index;
	    }
	    // Regardless of whether the edge existed as an object, it needs to be added to the trimming loop
	    ON_3dPoint vg1pt(vg1->coord);
	    int orientation = ((vg1pt != (*b)->m_V[(*b)->m_E[(int)brepi[eu->e_p->index]].m_vi[0]].Point())) ? 1 : 0;
	    // Make a 2d trimming curve, create a trim, and add the trim to the loop
	    int p1_index = nmg_to_array.find(vg1->index)->second;
	    int p2_index = nmg_to_array.find(vg2->index)->second;
	    ON_2dPoint from_uv(points_2d[p1_index][0], points_2d[p1_index][1]);
	    ON_2dPoint to_uv(points_2d[p2_index][0], points_2d[p2_index][1]);
	    ON_Curve* c2d =  new ON_LineCurve(from_uv, to_uv);
	    c2d->SetDomain(0.0, 1.0);
	    int c2i = (*b)->m_C2.Count();
	    (*b)->m_C2.Append(c2d);
	    ON_BrepTrim& trim = (*b)->NewTrim((*b)->m_E[(int)brepi[eu->e_p->index]], orientation, loop, c2i);
	    trim.m_type = ON_BrepTrim::mated;
	    trim.m_tolerance[0] = 0.0;
	    trim.m_tolerance[1] = 0.0;
	}

    }

    /* Decide whether to flip face - check NMG face normal
     * and whether the outer loop is ccw */
    bool ccw, do_flip;
    for (BU_LIST_FOR(lu, loopuse, &fu->lu_hd)) {
	if (lu->orientation == OT_SAME)
	    ccw = (nmg_loop_is_ccw(lu, fg->N, tol) == -1) ? false : true;
    }
    do_flip = ((!ccw && fu->f_p->flip) || (ccw && !fu->f_p->flip)) ? false : true;

    ON_3dVector surface_normal;
    ON_3dVector face_normal(fg->N[0], fg->N[1], fg->N[2]);
    surf->EvNormal(surf->Domain(0).Mid(), surf->Domain(1).Mid(), surface_normal);
    if (((ON_DotProduct(surface_normal, face_normal) > 0) && do_flip) ||
        ((ON_DotProduct(surface_normal, face_normal) < 0) && !do_flip))
    {
	(*b)->FlipFace(face);
    }

    return 0;
}


extern "C" void
rt_nmg_brep2(ON_Brep **b, const struct rt_db_internal *ip, const struct bn_tol *tol)
{
    struct model *m;
    struct nmgregion *r;
    struct shell *s;
    struct faceuse *fu;

    // Verify NMG
    RT_CK_DB_INTERNAL(ip);
    m = (struct model *)ip->idb_ptr;
    NMG_CK_MODEL(m);

    // Both NMG and brep structures re-use components between faces.  In order to track
    // when the conversion routine has already handled an NMG element, use an array.
    long *brepi = static_cast<long*>(bu_malloc(m->maxindex * sizeof(long), "rt_nmg_brep: brepi[]"));
    for (int i = 0; i < m->maxindex; i++) brepi[i] = -INT_MAX;

    // Iterate over all faces in the NMG
    for (BU_LIST_FOR(r, nmgregion, &m->r_hd)) {
	for (BU_LIST_FOR(s, shell, &r->s_hd)) {
	    for (BU_LIST_FOR(fu, faceuse, &s->fu_hd)) {
		NMG_CK_FACEUSE(fu);
		if (fu->orientation != OT_SAME) continue;
		nmg_brep_face(b, fu, tol, brepi);
	    }
	    (*b)->SetTrimIsoFlags();
	}
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
