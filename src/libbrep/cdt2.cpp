/*                        C D T . C P P
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
/** @file cdt.cpp
 *
 * Constrained Delaunay Triangulation of NURBS B-Rep objects.
 *
 */

#include "common.h"
#include "bg/chull.h"
#include "./cdt.h"

// EXPERIMENT - see if we can generate polygons from the loops
// For all faces, and each face loop in those faces, build the
// initial polygons strictly based on trim start/end points

int
ON_Brep_CDT_Tessellate2(struct ON_Brep_CDT_State *s_cdt)
{
    ON_Brep* brep = s_cdt->brep;

    // First, set up the edge containers that will manage the edge subdivision.  Loop
    // ordering is not the job of these containers - that's handled by the trim loop
    // polygons.  These containers maintain the association between trims in different
    // faces and the 3D edge curve information used to drive shared points.
    for (int index = 0; index < brep->m_E.Count(); index++) {
	ON_BrepEdge& edge = brep->m_E[index];
	cdt_mesh::bedge_seg_t *bseg = new cdt_mesh::bedge_seg_t;
	bseg->edge_ind = edge.m_edge_index;
	s_cdt->e2polysegs[edge.m_edge_index].insert(bseg);
    }

    // Next, for each face and each loop in each face define the initial
    // loop polygons.  Note there is no splitting of edges at this point -
    // we are simply establishing the initial closed polygons.
    for (int face_index = 0; face_index < brep->m_F.Count(); face_index++) {
	ON_BrepFace &face = s_cdt->brep->m_F[face_index];
	int loop_cnt = face.LoopCount();
	cdt_mesh::cdt_mesh_t *fmesh = &s_cdt->fmeshes[face_index];
	cdt_mesh::cpolygon_t *cpoly = NULL;

	for (int li = 0; li < loop_cnt; li++) {
	    const ON_BrepLoop *loop = face.Loop(li);
	    bool is_outer = (face.OuterLoop()->m_loop_index == loop->m_loop_index) ? true : false;
	    if (is_outer) {
		cpoly = &fmesh->outer_loop;
	    } else {
		cpoly = new cdt_mesh::cpolygon_t;
		fmesh->inner_loops[li] = cpoly;
	    }
	    cpoly->cdt_mesh = fmesh;
	    int trim_count = loop->TrimCount();

	    ON_2dPoint cp(0,0);

	    long cv, pv, fv;
	    for (int lti = 0; lti < trim_count; lti++) {
		ON_BrepTrim *trim = loop->Trim(lti);
		ON_Interval range = trim->Domain();
		if (lti == 0) {
		    // Polygon first
		    cp = trim->PointAt(range.m_t[0]);
		    pv = cpoly->add_point(cp);
		    long find = fmesh->add_point(cp);
		    cpoly->p2f[pv] = find;
		    fv = pv;

		    // Let cdt_mesh know about new information
		    ON_3dVector norm = ON_3dVector::UnsetVector;
		    if (trim->m_type != ON_BrepTrim::singular) {
			ON_3dPoint tmp1;
			surface_EvNormal(trim->SurfaceOf(), cp.x, cp.y, tmp1, norm);
		    }
		    ON_3dPoint *op3d = (*s_cdt->vert_pnts)[trim->Vertex(0)->m_vertex_index];
		    fmesh->add_point(op3d);
		    fmesh->add_normal(new ON_3dPoint(norm));

		} else {
		    pv = cv;
		}

		// NOTE: Singularities have a segment in 2D but not 3D - we're adding extra copies of pointers to
		// points in the arrays to deal with this non-uniqueness to keep a 1-1 relationship
		// between the two array indices in the polygon.  For the 3D p2ind mapping, this will mean that the
		// ON_3dPoint pointer will always point to the highest index value in the vector
		// to be assigned that particular pointer.  For tests which are concerned with 3D point
		// uniqueness, a 2d->ind->3d->ind lookup will be needed to "canonicalize"
		// the 3D index value.  (TODO In particular, this will be needed for triangle
		// comparisons.)
		//
		//
		cp = trim->PointAt(range.m_t[1]);
		cv = cpoly->add_point(cp);
		long find = fmesh->add_point(cp);
		cpoly->p2f[cv] = find;

		// Let cdt_mesh know about the 3D information as well
		ON_3dVector norm = ON_3dVector::UnsetVector;
		if (trim->m_type != ON_BrepTrim::singular) {
		    // 3D points are globally unique, but normals are not - the same edge point may
		    // have different normals from two faces at a sharp edge.  Calculate the
		    // face normal for this point on this surface.
		    ON_3dPoint tmp1;
		    surface_EvNormal(trim->SurfaceOf(), cp.x, cp.y, tmp1, norm);
		}
		ON_3dPoint *cp3d = (*s_cdt->vert_pnts)[trim->Vertex(1)->m_vertex_index];
		fmesh->add_point(cp3d);
		fmesh->add_normal(new ON_3dPoint(norm));


		struct cdt_mesh::edge_t lseg(pv, cv);
		cdt_mesh::cpolyedge_t *ne = cpoly->add_edge(lseg);
		ne->trim_ind = trim->m_trim_index;
		ne->trim_start = range.m_t[0];
		ne->trim_end = range.m_t[1];
		if (trim->m_ei >= 0) {
		    cdt_mesh::bedge_seg_t *eseg = *s_cdt->e2polysegs[trim->m_ei].begin();
		    if (eseg->tseg1 && eseg->tseg2) {
			bu_log("error - more than two trims associated with an edge\n");
			return -1;
		    }
		    if (eseg->tseg1) {
			eseg->tseg2 = ne;
		    } else {
			eseg->tseg1 = ne;
		    }
		} else {
		    // A null eseg will indicate a singularity and a need for special case
		    // splitting of the 2D edge only
		    ne->eseg = NULL;
		}
	    }
	    struct cdt_mesh::edge_t last_seg(cv, fv);
	    cpoly->add_edge(last_seg);
	}
    }

    // Process the non-linear edges first - we will need information
    // from them to handle the linear edges
    for (int index = 0; index < brep->m_E.Count(); index++) {
	ON_BrepEdge& edge = brep->m_E[index];

	const ON_Curve* crv = edge.EdgeCurveOf();
	if (!crv->IsLinear(BN_TOL_DIST)) {
	    std::set<cdt_mesh::bedge_seg_t *> &epsegs = s_cdt->e2polysegs[edge.m_edge_index];
	    std::set<cdt_mesh::bedge_seg_t *>::iterator e_it;
	    for (e_it = epsegs.begin(); e_it != epsegs.end(); e_it++) {
		cdt_mesh::bedge_seg_t *b = *e_it;
		if (!b->tseg1 || !b->tseg2) {
		    std::cout << "don't have trims\n";
		}
	    }
	}
    }

    for (int face_index = 0; face_index < brep->m_F.Count(); face_index++) {
	ON_BrepFace &face = s_cdt->brep->m_F[face_index];
	int loop_cnt = face.LoopCount();
	cdt_mesh::cdt_mesh_t *fmesh = &s_cdt->fmeshes[face_index];
	cdt_mesh::cpolygon_t *cpoly = NULL;

	//if (face_index != 27) continue;

	for (int li = 0; li < loop_cnt; li++) {
	    const ON_BrepLoop *loop = face.Loop(li);
	    bool is_outer = (face.OuterLoop()->m_loop_index == loop->m_loop_index) ? true : false;
	    if (is_outer) {
		cpoly = &fmesh->outer_loop;
	    } else {
		cpoly = fmesh->inner_loops[li];
	    }
	    std::cout << "Face: " << face_index << ", Loop: " << li << "\n";
	    cpoly->print();

	    struct bu_vls fname = BU_VLS_INIT_ZERO;
	    bu_vls_sprintf(&fname, "%d-%d-poly3d.plot3", face_index, li);
	    cpoly->polygon_plot_3d(bu_vls_cstr(&fname));
	    cpoly->cdt();
	    bu_vls_sprintf(&fname, "%d-%d-cdt.plot3", face_index, li);
	    cpoly->cdt_mesh->tris_set_plot(cpoly->tris, bu_vls_cstr(&fname));
	    bu_vls_free(&fname);

	}

	fmesh->cdt();
	struct bu_vls fname = BU_VLS_INIT_ZERO;
	bu_vls_sprintf(&fname, "%d-tris.plot3", face_index);
	fmesh->tris_plot(bu_vls_cstr(&fname));
	bu_vls_free(&fname);
    }

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

