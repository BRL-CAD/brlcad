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

/**
 *  Routine to create a planar NURBS surface from 4 points
 */
HIDDEN ON_Surface*
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

HIDDEN int
nmg_brep_face(ON_Brep **b, const struct faceuse *fu, const struct bn_tol *tol, long *brepi) {
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
    struct edgeuse *eu;

    /* Find out how many points we have, set up any uninitialized ON_Brep vertex
     * structures, and prepare a map of NMG index values to the point array indices */
    nmg_tabulate_face_g_verts(&vert_table, fg);

    for (BU_PTBL_FOR(pt, (struct vertex **), &vert_table)) {
	if (brepi[(*pt)->vg_p->index] == -INT_MAX) {
	    ON_BrepVertex& vert = (*b)->NewVertex((*pt)->vg_p->coord, SMALL_FASTF);
	    brepi[(*pt)->vg_p->index] = vert.m_vertex_index;
	}
	pnt_cnt++;
    }

    /* Prepare the 3D obr input array */
    points_3d = (point_t *)bu_calloc(pnt_cnt + 1, sizeof(point_t), "nmg points");
    for (BU_PTBL_FOR(pt, (struct vertex **), &vert_table)) {
	VSET(points_3d[pnt_index], (*pt)->vg_p->coord[0],(*pt)->vg_p->coord[1],(*pt)->vg_p->coord[2]);
	pnt_index++;
    }
    bu_ptbl_free(&vert_table);


    /* Calculate the 3D coplanar oriented bounding rectangle (obr) */
    ret += bn_3d_coplanar_obr(&obr_center, &u_axis, &v_axis, (const point_t *)points_3d, pnt_cnt);
    if (ret) {
	bu_log("Failed to get oriented bounding rectangle for NMG faceuse #%d\n", fu->index);
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

    (*b)->m_S.Append(sideSurface(p1, p2, p3, p4));
    ON_Surface *surf = (*(*b)->m_S.Last());
    int surfindex = (*b)->m_S.Count();
    ON_BrepFace& face = (*b)->NewFace(surfindex - 1);

    // With the surface and the face defined, make
    // trimming loops and create faces.  To generate UV
    // coordinates for each from and to for the
    // edgecurves, the UV origin is defined to be v1,
    // v1->v2 is defined as the U domain, and v1->v4 is
    // defined as the V domain.
    VSUB2(u_axis, points_obr[2], points_obr[1]);
    VSUB2(v_axis, points_obr[0], points_obr[1]);
    fastf_t u_axis_dist = MAGNITUDE(u_axis);
    fastf_t v_axis_dist = MAGNITUDE(v_axis);

    /* Now that we have the surface and the face, add the loops */
    for (BU_LIST_FOR(lu, loopuse, &fu->lu_hd)) {
	if (BU_LIST_FIRST_MAGIC(&lu->down_hd) != NMG_EDGEUSE_MAGIC) continue; // loop is a single vertex
	// Check if this is an inner or outer loop
	ON_BrepLoop::TYPE looptype = (lu->orientation == OT_SAME) ? ON_BrepLoop::outer : ON_BrepLoop::inner;
	ON_BrepLoop& loop = (*b)->NewLoop(looptype, face);
	for (BU_LIST_FOR(eu, edgeuse, &lu->down_hd)) {
	    vect_t ev1, ev2;
	    struct vertex_g *vg1 = eu->vu_p->v_p->vg_p;
	    struct vertex_g *vg2 = eu->eumate_p->vu_p->v_p->vg_p;
	    NMG_CK_VERTEX_G(vg1);
	    NMG_CK_VERTEX_G(vg2);
	    VMOVE(ev1, vg1->coord);
	    VMOVE(ev2, vg2->coord);
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
	    vect_t vect1, vect2, u_component, v_component;
	    double u0, u1, v0, v1;
	    ON_2dPoint from_uv, to_uv;
	    VSUB2(vect1, ev1, points_obr[0]);
	    VSUB2(vect2, ev2, points_obr[0]);
	    surf->GetDomain(0, &u0, &u1);
	    surf->GetDomain(1, &v0, &v1);
	    VPROJECT(vect1, u_axis, u_component, v_component);
	    from_uv.y = u0 + MAGNITUDE(u_component)/u_axis_dist*(u1-u0);
	    from_uv.x = v0 + MAGNITUDE(v_component)/v_axis_dist*(v1-v0);
	    VPROJECT(vect2, u_axis, u_component, v_component);
	    to_uv.y = u0 + MAGNITUDE(u_component)/u_axis_dist*(u1-u0);
	    to_uv.x = v0 + MAGNITUDE(v_component)/v_axis_dist*(v1-v0);
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
		if (nmg_brep_face(b, fu, tol, brepi)) return;
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
