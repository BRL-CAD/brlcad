/*                C D T _ S I N G U L A R _ S U R F . C P P
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
/** @file cdt_singular_surf.cpp
 *
 * Special handling of faces with singular trims.
 *
 */

#include "common.h"
#include "bn/plot3.h"
#include "./cdt.h"
#include "./trimesh.h"

#define p2tEdge std::pair<p2t::Point *, p2t::Point *>

#define p2dIsSingular()

#define IsEdgePt(_a) (f->s_cdt->edge_pnts->find(_a) != f->s_cdt->edge_pnts->end())

static void
plot_face(p2t::Triangle *t, std::map<p2t::Point *, ON_3dPoint *> *pointmap, int r, int g, int b, FILE *c_plot)
{
    point_t p1, p2, p3;
    ON_3dPoint *on1 = (*pointmap)[t->GetPoint(0)];
    ON_3dPoint *on2 = (*pointmap)[t->GetPoint(1)];
    ON_3dPoint *on3 = (*pointmap)[t->GetPoint(2)];
    p1[X] = on1->x;
    p1[Y] = on1->y;
    p1[Z] = on1->z;
    p2[X] = on2->x;
    p2[Y] = on2->y;
    p2[Z] = on2->z;
    p3[X] = on3->x;
    p3[Y] = on3->y;
    p3[Z] = on3->z;
    pl_color(c_plot, r, g, b);
    pdv_3move(c_plot, p1);
    pdv_3cont(c_plot, p2);
    pdv_3move(c_plot, p1);
    pdv_3cont(c_plot, p3);
    pdv_3move(c_plot, p2);
    pdv_3cont(c_plot, p3);
}


static void
plot_faces(std::set<size_t> *faces, std::vector<trimesh::triangle_t> &farray, std::map<p2t::Point *, ON_3dPoint *> *pointmap, const char *filename)
{
    std::set<size_t>::iterator f_it;
    FILE* plot_file = fopen(filename, "w");
    int r = int(256*drand48() + 1.0);
    int g = int(256*drand48() + 1.0);
    int b = int(256*drand48() + 1.0);
    for (f_it = faces->begin(); f_it != faces->end(); f_it++) {
	p2t::Triangle *t = farray[*f_it].t;
	plot_face(t, pointmap, r, g ,b, plot_file);
    }
    fclose(plot_file);
}


void
ON_Singular_Face_Process(struct ON_Brep_CDT_Face_State *f)
{
    p2t::CDT *cdt = f->cdt;
    std::vector<p2t::Triangle*> tris = cdt->GetTriangles();

    // Assemble the set of unique 2D poly2tri points
    std::set<p2t::Point *> uniq_p2d;
    for (size_t i = 0; i < tris.size(); i++) {
	p2t::Triangle *t = tris[i];
	for (size_t j = 0; j < 3; j++) {
	    uniq_p2d.insert(t->GetPoint(j));
	}
    }
    // Assign all poly2tri 2D points a trimesh index
    std::map<p2t::Point *, trimesh::index_t> p2dind;
    std::map<trimesh::index_t, p2t::Point *> ind2p2d;
    std::set<p2t::Point *>::iterator u_it;
    {
	trimesh::index_t ind = 0;
	for (u_it = uniq_p2d.begin(); u_it != uniq_p2d.end(); u_it++) {
	    p2dind[*u_it] = ind;
	    ind2p2d[ind] = *u_it;
	    ind++;
	}
    }

    // Assemble the faces array
    std::vector<trimesh::triangle_t> triangles;
    for (size_t i = 0; i < tris.size(); i++) {
	p2t::Triangle *t = tris[i];
	if (f->tris_degen->find(t) != f->tris_degen->end()) {
	    continue;
	}

	trimesh::triangle_t tmt;
	for (size_t j = 0; j < 3; j++) {
	    tmt.v[j] = p2dind[t->GetPoint(j)];
	}
	tmt.t = t;
	triangles.push_back(tmt);
    }

    // Build the mesh topology information - we are going to have to 'walk' the mesh
    // locally to do this, so we need neighbor lookups
    std::vector<trimesh::edge_t> edges;
    trimesh::unordered_edges_from_triangles(triangles.size(), &triangles[0], edges);
    trimesh::trimesh_t mesh;
    mesh.build(uniq_p2d.size(), triangles.size(), &triangles[0], edges.size(), &edges[0]);


    // Identify any singular points for later reference
    std::map<ON_3dPoint *,std::set<p2t::Point *>> p2t_singular_pnts;
    std::map<p2t::Point *, ON_3dPoint *> *pointmap = f->p2t_to_on3_map;
    for (size_t i = 0; i < tris.size(); i++) {
	p2t::Triangle *t = tris[i];
	if (f->tris_degen->find(t) != f->tris_degen->end()) {
	    continue;
	}
	for (size_t j = 0; j < 3; j++) {
	    p2t::Point *p2t = t->GetPoint(j);
	    ON_3dPoint *p3d = (*pointmap)[p2t];
	    if (f->s_cdt->singular_vert_to_norms->find(p3d) != f->s_cdt->singular_vert_to_norms->end()) {
		p2t_singular_pnts[p3d].insert(p2t);
	    }
	}
    }

    // Find all the triangles using a singular point as a vertex - that's our
    // starting triangle set.
    //
    // TODO - initially, the code is going to pretty much assume one singularity per face,
    // but that's not a good assumption in general - ultimately, we should build up sets
    // of faces for each singularity for separate processing.
    if (p2t_singular_pnts.size() != 1) {
	bu_log("odd singularity point count, aborting\n");
    }
    std::map<ON_3dPoint *,std::set<p2t::Point *>>::iterator ap_it;

    // In a multi-singularity version of this code this set will have to be specific to each point
    std::set<size_t> singularity_triangles;

    for (ap_it = p2t_singular_pnts.begin(); ap_it != p2t_singular_pnts.end(); ap_it++) {
	std::set<p2t::Point *>::iterator p_it;
	std::set<p2t::Point *> &p2tspnts = ap_it->second;
	for (p_it = p2tspnts.begin(); p_it != p2tspnts.end(); p_it++) {
	    p2t::Point *p2d = *p_it;
	    trimesh::index_t ind = p2dind[p2d];
	    std::vector<trimesh::index_t> faces = mesh.vertex_face_neighbors(ind);
	    for (size_t i = 0; i < faces.size(); i++) {
		std::cout << "face index " << faces[i] << "\n";
		singularity_triangles.insert(faces[i]);
	    }
	}
    }

    bu_file_delete("singularity_triangles.plot3");
    plot_faces(&singularity_triangles, triangles, pointmap, "singularity_triangles.plot3");

    // Find the best fit plane for the vertices of the triangles involved with
    // the singular point

    // Make sure all of the triangles can be projected to the plane
    // successfully, without flipping triangles.  If not, the mess will have be
    // handled in coordinated subsections (doable but hard!) and for now we
    // will punt in that situation.

    // Walk out along the mesh, collecting all the triangles from the face
    // that can be projected onto the plane without undo loss of area or normal
    // issues.  Once the terminating criteria are reached, we have the set
    // of triangles that will provide our "replace and remesh" subset of the
    // original face.

    // Walk around the outer triangles (edge use count internal to the subset will
    // be critical here) and construct the bounding polygon of the subset for
    // poly2Tri.

    // Project all points in the subset into the plane, getting XY coordinates
    // for poly2Tri.  Use the previously constructed loop and assemble the
    // polyline and internal point set for the new triangulation, keeping a
    // map from the new XY points to the original p2t points.

    // Perform the new triangulation, assemble the new p2t Triangle set using
    // the mappings, and check the new 3D mesh thus created for issues.  If it
    // is clean, replace the original subset of the parent face with the new
    // triangle set, else fail.  Note that a consequence of this will be that
    // only one of the original singularity trim points will end up being used.

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

