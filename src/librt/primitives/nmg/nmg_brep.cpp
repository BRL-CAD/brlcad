/*                    N M G _ B R E P . C P P
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
* Simple line for edge geometry
*/
static ON_Curve* edgeCurve (const ON_BrepVertex& from, const ON_BrepVertex& to)
{
    ON_Curve *c3d = new ON_LineCurve(from.Point(), to.Point());
    c3d->SetDomain(0.0, 1.0);
    return c3d;
}

/**
* 
*/
static ON_Surface* sideSurface(const ON_3dPoint& SW, const ON_3dPoint& SE, const ON_3dPoint& NE, const ON_3dPoint& NW)
{
    ON_NurbsSurface *surf = new ON_NurbsSurface(3,FALSE, 2, 2, 2, 2);
    surf->SetCV(0,0,SW);
    surf->SetCV(1,0,SE);
    surf->SetCV(1,1,NE);
    surf->SetCV(0,1,NW);
    surf->SetKnot(0,0,0.0);
    surf->SetKnot(0,1,1.0);
    surf->SetKnot(1,0,0.0);
    surf->SetKnot(1,1,1.0);
    return surf;
}

extern "C" void
rt_nmg_brep(ON_Brep **b, const struct rt_db_internal *ip, const struct bn_tol *tol)
{
    struct model *m = nmg_mm();//not needed for non-tess
    struct nmgregion *r;
    struct shell *s;
    struct faceuse *fu;
    struct loopuse *lu;
    struct edgeuse *eu;
        
    RT_CK_DB_INTERNAL(ip);
    m = (struct model *)ip->idb_ptr;
    NMG_CK_MODEL(m);
   
    long brepi[m->maxindex];
    for (int i = 0; i < m->maxindex; i++) brepi[i] = -INT_MAX;
    
    *b = new ON_Brep();
    for (BU_LIST_FOR(r, nmgregion, &m->r_hd)) {
	for (BU_LIST_FOR(s, shell, &r->s_hd)) {
	    for (BU_LIST_FOR(fu, faceuse, &s->fu_hd)) {
		NMG_CK_FACEUSE(fu);
		if(fu->orientation != OT_SAME) continue;
		for (BU_LIST_FOR(lu, loopuse, &fu->lu_hd)) {
		    int edges=0;
		    // For each loop, add the edges and vertices
		    if (BU_LIST_FIRST_MAGIC(&lu->down_hd) != NMG_EDGEUSE_MAGIC) continue; // loop is a single vertex
		    for (BU_LIST_FOR(eu, edgeuse, &lu->down_hd)) {
			// Add vertices if not already added
			++edges;
			ON_BrepVertex from, to, tmp;
			struct vertex_g *vg;
			vg = eu->vu_p->v_p->vg_p;
			NMG_CK_VERTEX_G(vg);
			if (brepi[eu->vu_p->v_p->index] == -INT_MAX) {
			    from = (*b)->NewVertex(vg->coord, SMALL_FASTF);
			    brepi[eu->vu_p->v_p->index] = from.m_vertex_index;
			}
			vg = eu->eumate_p->vu_p->v_p->vg_p;
			NMG_CK_VERTEX_G(vg);
			if (brepi[eu->eumate_p->vu_p->v_p->index] == -INT_MAX) {
			    to = (*b)->NewVertex(vg->coord, SMALL_FASTF);
			    brepi[eu->eumate_p->vu_p->v_p->index] = to.m_vertex_index;
			}
			// Add edge if not already added
			if(brepi[eu->e_p->index] == -INT_MAX) {
			    /* always add edges with the small vertex index as from */
			    if (from.m_vertex_index > to.m_vertex_index) {
				tmp = from;
				from = to;
				to = tmp;
			    }
			    // Create and add 3D curve
			    (*b)->m_C3.Append(edgeCurve(from, to));
			    // Create and add 3D edge
			    ON_BrepEdge& e = (*b)->NewEdge(from, to, eu->e_p->index);
			    brepi[eu->e_p->index] = e.m_edge_index;
			}
		    }
		} 
		// Need to create ON_NurbsSurface based on plane of face
		// in order to have UV space in which to define trimming
		// loops.  Bounding points are NOT on the face plane, so 
		// another approach must be used.
		//
		// General approach:  For all loops in the faceuse, collect all the vertices.
		// Find the center point of all the vertices, and search for the point with
		// the greatest distance from that center point.  Once found, cross the vector
		// between the center point and furthest point with the normal of the face
		// and scale the resulting vector to have the same length as the vector to
		// the furthest point.  Add the two resulting vectors to find the first
		// corner point.  Mirror the first corner point across the center to find the
		// second corner point.  Cross the two vectors created by the first two corner
		// points with the face normal to get the vectors of the other two corners,
		// and scale the resulting vectors to the same magnitude as the first two.
		// These four points bound all vertices on the plane and form a suitable
		// staring point for a UV space, since all points on all the edges are equal
		// to or further than the distance between the furthest vertex and the center
		// point.
		
		// ............. .............
 		// .           .* .          .
 		// .         .  .    .       .
 		// .        .   .      .     .
 		// .       .    .       *    .
 		// .      .     .       .    .
 		// .     .      .       .    .
 		// .    .       .       .    .
 		// .   *        *       .    .
 		// .   .                .    .
 		// .   .                .    .
 		// .   .                .    .
 		// .   *.               .    .
 		// .     ...        ...*     .
 		// .       .... ....         .
 		// .           *             .
 		// ...........................
 		//
 	       	
 		
 		const struct face_g_plane *fg = fu->f_p->g.plane_p;
		struct bu_ptbl vert_table;
		nmg_tabulate_face_g_verts(&vert_table, fg);
	    	point_t tmppt, center, max_pt;
		struct vertex **pt;
	    	VSET(tmppt, 0, 0, 0);
	    	int ptcnt = 0;
	    	for (BU_PTBL_FOR(pt, (struct vertex **), &vert_table)) {
	    	    tmppt[0] += (*pt)->vg_p->coord[0];
	    	    tmppt[1] += (*pt)->vg_p->coord[1];
	    	    tmppt[2] += (*pt)->vg_p->coord[2];
		    ptcnt++;
	    	}
	    	VSET(center, tmppt[0]/ptcnt, tmppt[1]/ptcnt, tmppt[2]/ptcnt);
		bu_log("center: [%2.f,%2.f,%2.f]\n", center[0], center[1], center[2]);
		fastf_t max_dist = 0.0;
		fastf_t curr_dist;
		for (BU_PTBL_FOR(pt, (struct vertex **), &vert_table)) {
	    	    tmppt[0] = (*pt)->vg_p->coord[0];
	    	    tmppt[1] = (*pt)->vg_p->coord[1];
	    	    tmppt[2] = (*pt)->vg_p->coord[2];
		    curr_dist = DIST_PT_PT(center, tmppt);
		    if (curr_dist > max_dist) {
			max_dist = curr_dist;
			VMOVE(max_pt, tmppt);
		    }
		}
		bu_log("max_dist: %2.f\n", max_dist);
		bu_log("max_pt: [%2.f,%2.f,%2.f]\n", max_pt[0], max_pt[1], max_pt[2]);
		vect_t vtmp, v1, v2, v3, v4, vnormal;
		VSET(vnormal, fg->N[0], fg->N[1], fg->N[2]);
		VSUB2(v1, max_pt, center);
		VCROSS(vtmp, v1, vnormal);
		VADD2(v1, v1, vtmp);
		VCROSS(v2, v1, vnormal);
	        VREVERSE(v3, v1);
		VCROSS(v4, v3, vnormal);
		VADD2(v1, v1, center);
		VADD2(v2, v2, center);
		VADD2(v3, v3, center);
		VADD2(v4, v4, center);
		
				
		bu_log("v1: [%2.f,%2.f,%2.f]\n", v1[0], v1[1], v1[2]);
		bu_log("v2: [%2.f,%2.f,%2.f]\n", v2[0], v2[1], v2[2]);
		bu_log("v3: [%2.f,%2.f,%2.f]\n", v3[0], v3[1], v3[2]);
		bu_log("v4: [%2.f,%2.f,%2.f]\n", v4[0], v4[1], v4[2]);
		
		ON_3dPoint p1 = ON_3dPoint(v1);
		ON_3dPoint p2 = ON_3dPoint(v2);
		ON_3dPoint p3 = ON_3dPoint(v3);
		ON_3dPoint p4 = ON_3dPoint(v4);

		(*b)->m_S.Append(sideSurface(p1, p2, p3, p4));
		ON_Surface *surf = (*(*b)->m_S.Last());
		
		// With the surface defined, make trimming loops and
		// create faces
	    } 
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
