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


/* TODO - the approach for creating surfaces for NMG faces below is horribly non-optimal.
 * A better approach would be along the lines of:
 *
 * 1.  Since the outer NMG boundary (lu->orientation == OT_SAME) should be a simple polyline, use the Melkman Algorithm
 * to construct a convex hull in order N time.  The Geometric Tools implementation of step 3 indicates we can't
 * have three colinear points in the input to that step, so (since the NMG itself makes no such guarantees) we run
 * all input face outer loops through the hull building process.
 * http://geomalgorithms.com/a12-_hull-3.html
 *
 * If the NMG outer polyline can be a non-simple polyline (i.e. it self intersects), we'll have to either use the
 * Monotone Chain Algorithm to compute the 2D convex hull, or (*possibly* faster at the expense of a somewhat larger
 * surface) use the BFP bounded error approximation and scale it to ensure it contains all points.
 * http://geomalgorithms.com/a10-_hull-1.html
 * http://geomalgorithms.com/a11-_hull-2.html
 *
 * 3.  Calculate the minimal rectangle using rotating calipers - see:
 * http://geomalgorithms.com/a08-_containers.html#Minimal%20Rectangle
 * http://www.geometrictools.com/LibMathematics/Containment/Containment.html
 * http://code.google.com/p/replay/source/browse/trunk/include/replay/bounding_rectangle.hpp
 *
 * 4.  Translate the resulting minimal rectangle into a NURBS surface and define the in-surface
 * 2D loop based on the 3D nmg loop.  This should be very similar to what is done below...
 */


HIDDEN int
nmg_brep_face(ON_Brep **b, const struct faceuse *fu, const struct bn_tol *tol, long *brepi) {
    const struct face_g_plane *fg = fu->f_p->g.plane_p;
    struct bu_ptbl vert_table;
    struct vertex **pt;
    int ret = 0;
    int pnt_cnt = 0;
    point_t origin_pnt;
    vect_t u_axis, v_axis, v1, v2;
    point2d_t obr_2d_center;
    point2d_t obr_2d_v1, obr_2d_v2;
    point_t *points_3d = NULL;
    point2d_t *points_2d = NULL;
    point2d_t *points_obr_2d = NULL;
    point_t *points_obr_3d = NULL;
    point_t obr_output_pnts[4+1] = {{0}};
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

    /* Use the obr to define the NURBS surface */
    points_obr_2d = (point2d_t *)bu_calloc(3 + 1, sizeof(point2d_t), "points_2d");
    points_obr_3d = (point_t *)bu_calloc(3 + 1, sizeof(point_t), "points_3d");
    V2MOVE(points_obr_2d[0], obr_2d_center);
    V2ADD2(points_obr_2d[1], obr_2d_v1, obr_2d_center);
    V2ADD2(points_obr_2d[2], obr_2d_v2, obr_2d_center);
    ret = bn_coplanar_2d_to_3d(&points_obr_3d, (const point_t *)&origin_pnt, (const vect_t *)&u_axis, (const vect_t *)&v_axis, (const point2d_t *)points_obr_2d, 3);
    VSUB2(v1, points_obr_3d[1], points_obr_3d[0]);
    VSUB2(v2, points_obr_3d[2], points_obr_3d[0]);
    VSCALE(v1, v1, 1.001);
    VSCALE(v2, v2, 1.001);
    VADD3(obr_output_pnts[2], points_obr_3d[0], v1, v2);
    VSCALE(v1, v1, -1);
    VADD3(obr_output_pnts[3], points_obr_3d[0], v1, v2);
    VSCALE(v2, v2, -1);
    VADD3(obr_output_pnts[0], points_obr_3d[0], v1, v2);
    VSCALE(v1, v1, -1);
    VADD3(obr_output_pnts[1], points_obr_3d[0], v1, v2);
    ON_3dPoint p1 = ON_3dPoint(obr_output_pnts[0]);
    ON_3dPoint p2 = ON_3dPoint(obr_output_pnts[1]);
    ON_3dPoint p3 = ON_3dPoint(obr_output_pnts[2]);
    ON_3dPoint p4 = ON_3dPoint(obr_output_pnts[3]);
    (*b)->m_S.Append(sideSurface2(p1, p4, p3, p2));
    ON_Surface *surf = (*(*b)->m_S.Last());
    int surfindex = (*b)->m_S.Count();
    ON_BrepFace& face = (*b)->NewFace(surfindex - 1);
    /* TODO - set surface UV domain to match the 2D domain above */

    /* TODO - activate handling of cw outer loop orientation */
    /*for (BU_LIST_FOR(lu, loopuse, &fu->lu_hd)) {
      if (lu->orientation == OT_SAME && nmg_loop_is_ccw(lu, fg->N, tol) == -1) ccw = -1;
      }
      if (ccw != -1) {
      VSET(vnormal, fg->N[0], fg->N[1], fg->N[2]);
      } else {
      VSET(vnormal, -fg->N[0], -fg->N[1], -fg->N[2]);
      }
      if (fu->f_p->flip)
      VSET(vnormal, -vnormal[0], -vnormal[1], -vnormal[2]);
      */

    /* Now that we have the surface and the face, add the loops */
    for (BU_LIST_FOR(lu, loopuse, &fu->lu_hd)) {
	int edges=0;
	if (BU_LIST_FIRST_MAGIC(&lu->down_hd) != NMG_EDGEUSE_MAGIC) continue; // loop is a single vertex
	// Check if this is an inner or outer loop
	ON_BrepLoop::TYPE looptype = (lu->orientation == OT_SAME) ? ON_BrepLoop::outer : ON_BrepLoop::inner;
	ON_BrepLoop& loop = (*b)->NewLoop(looptype, face);
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
