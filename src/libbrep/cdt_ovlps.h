/*                C D T _ M E S H _ O V L P S . H
 * BRL-CAD
 *
 * Copyright (c) 2019 United States Government as represented by
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
/** @file cdt_mesh.h
 *
 * Mesh routines in support of Constrained Delaunay Triangulation of NURBS
 * B-Rep objects specific to overlap resolution
 *
 */

#ifndef __cdt_mesh_ovlps_h__
#define __cdt_mesh_ovlps_h__

#include "common.h"

#include <algorithm>
#include <vector>
#include <set>
#include <map>
#include "RTree.h"
#include "opennurbs.h"
#include "bu/color.h"
#include "bg/polygon.h"

#include "./cdt_mesh.h"

class omesh_t;
class overt_t {
    public:

        overt_t(omesh_t *om, long p)
        {
            omesh = om;
            p_id = p;
	    init = false;
        }

	// Update minimum edge length and bounding box information
        void update();

	// For situations where we don't yet have associated edges (i.e. a new
	// point is coming in based on an intruding mesh's vertex) we need to
	// supply external bounding box info
        void update(ON_BoundingBox *bbox);

	// Adjust this vertex to be as close as the surface allows to a target
	// point.  dtol is the allowable distance for the closest point to be
	// from the target before reporting failure.
	int adjust(ON_3dPoint &target_point, double dtol);

	// Determine if this vertex is on a brep face edge
        bool edge_vert();

	// Report the length of the shortest edge using this vertex
        double min_len();

	// Return the ON_3dPoint associated with this vertex
        ON_3dPoint vpnt();

	// Print 3D axis at the vertex point location
        void plot(FILE *plot);



	// Index of associated point in the omesh's fmesh container
	long p_id;

	// Bounding box centered on 3D vert, size based on smallest associated
	// edge length
        ON_BoundingBox bb;

	// Associated omesh
        omesh_t *omesh;

	// If this vertex has been aligned with a vertex in another mesh, the
	// other vertex and mesh are stored here
	std::map<omesh_t *, overt_t *> aligned;

    private:
	// If vertices are being moved, the impact is not local to a single
	// vertex - need updating in the neighborhood
        void update_ring();

	// Smallest edge length associated with this vertex
	double v_min_edge_len;

	// Set if vertex has been previously initialized - guides what the
	// update step must do.
	bool init;
};


class omesh_t
{
    public:
        omesh_t(cdt_mesh::cdt_mesh_t *m)
        {
            fmesh = m;
	    // Walk the fmesh's rtree holding the active triangles to get all
	    // vertices active in the face
	    std::set<long> averts;
	    RTree<size_t, double, 3>::Iterator tree_it;
	    size_t t_ind;
	    cdt_mesh::triangle_t tri;
	    fmesh->tris_tree.GetFirst(tree_it);
	    while (!tree_it.IsNull()) {
		t_ind = *tree_it;
		tri = fmesh->tris_vect[t_ind];
		averts.insert(tri.v[0]);
		averts.insert(tri.v[1]);
		averts.insert(tri.v[2]);
		++tree_it;
	    }

	    std::set<long>::iterator a_it;
	    for (a_it = averts.begin(); a_it != averts.end(); a_it++) {
		overts[*a_it] = new overt_t(this, *a_it);
		overts[*a_it]->update();
	    }
        };



	// Given another omesh, find which vertex pairs have overlapping
	// bounding boxes
	std::set<std::pair<long, long>> vert_ovlps(omesh_t *other);

        // Add an fmesh vertex to the overts array and tree.
        overt_t *vert_add(long, ON_BoundingBox *bb = NULL);

        // Find the closest uedge in the mesh to a point
        cdt_mesh::uedge_t closest_uedge(ON_3dPoint &p);
        // Find closest (non-face-boundary) edges
        std::set<cdt_mesh::uedge_t> interior_uedges_search(ON_BoundingBox &bb);

        // Find close triangles
        std::set<size_t> tris_search(ON_BoundingBox &bb);

        // Find close vertices
        std::set<overt_t *> vert_search(ON_BoundingBox &bb);
        overt_t * vert_closest(double *vdist, overt_t *v);


        void refinement_clear();
        bool validate_vtree();


        void plot(const char *fname,
                std::map<cdt_mesh::bedge_seg_t *, std::set<overt_t *>> *ev,
                std::map<cdt_mesh::cdt_mesh_t *, omesh_t *> &f2omap);
        void plot(std::map<cdt_mesh::bedge_seg_t *, std::set<overt_t *>> *ev,
                std::map<cdt_mesh::cdt_mesh_t *, omesh_t *> &f2omap);
        void plot_vtree(const char *fname);


	// The parent cdt_mesh container holding the original mesh data
	cdt_mesh::cdt_mesh_t *fmesh;

        // The fmesh pnts array may have inactive vertices - we only want the
        // active verts for this portion of the processing, so we maintain our
	// own map of active overlap vertices.
        std::map<long, overt_t *> overts;

	// Use an rtree for fast localized lookup
        RTree<long, double, 3> vtree;

        // Points from other meshes potentially needing refinement in this mesh
        std::map<overt_t *, std::set<long>> refinement_overts;

        // Points from this mesh inducing refinement in other meshes, and
        // triangles reported by tri_isect as intersecting from this mesh
        std::map<overt_t *, std::set<long>> intruding_overts;
        std::set<size_t> intruding_tris;
};


class ovlp_grp {
    public:
        ovlp_grp(omesh_t *m1, omesh_t *m2) {
            om1 = m1;
            om2 = m2;
        }
        omesh_t *om1;
        omesh_t *om2;
        std::set<size_t> tris1;
        std::set<size_t> tris2;
        std::set<long> verts1;
        std::set<long> verts2;
        std::set<overt_t *> overts1;
        std::set<overt_t *> overts2;

        void add_tri(omesh_t *m, size_t tind) {
            if (om1 == m) {
                tris1.insert(tind);
                cdt_mesh::triangle_t tri = om1->fmesh->tris_vect[tind];
                for (int i = 0; i < 3; i++) {
                    verts1.insert(tri.v[i]);
                    overt_t *ov = om1->overts[tri.v[i]];
                    if (!ov) {
                        std::cout << "WARNING: - no overt for tri vertex??\n";
                        continue;
                    }
                    overts1.insert(ov);
                }
                return;
            }
            if (om2 == m) {
                tris2.insert(tind);
                cdt_mesh::triangle_t tri = om2->fmesh->tris_vect[tind];
                for (int i = 0; i < 3; i++) {
                    verts2.insert(tri.v[i]);
                    overt_t *ov = om2->overts[tri.v[i]];
                    if (!ov) {
                        std::cout << "WARNING: - no overt for tri vertex??\n";
                        continue;
                    }
                    overts2.insert(ov);
                }
                return;
            }
        }

        // Each point involved in this operation must have it's closest point
        // in the other mesh involved.  If the closest point in the other mesh
        // ISN'T the closest surface point, we need to introduce that
        // point in the other mesh.
        bool characterize_all_verts();

        // Confirm that all triangles in the group are still in the fmeshes - if
        // we processed a prior group that involved a triangle incorporated into
        // this group that is now gone, this grouping is invalid and can't be
        // processed
        bool validate();

        void list_tris();
        void list_overts();

	void plot(const char *fname, int ind);

	void plot(const char *fname);

	// Start thinking about what relationships we need to track between mesh
	// points - refinement_pnts isn't enough by itself, we'll need more
	//
	// Each vertex in one mesh needs a matching vert in the other
	std::map<overt_t *, overt_t *> om1_om2_verts;
	std::map<overt_t *, overt_t *> om2_om1_verts;

	// Mappable rverts - no current opposite point, but surf closest point is new and unique
	// (i.e. can be inserted into this mesh)
	std::set<overt_t *> om1_rverts_from_om2;
	std::set<overt_t *> om2_rverts_from_om1;

	std::set<overt_t *> om1_everts_from_om2;
	std::set<overt_t *> om2_everts_from_om1;

	/* If the closest point for a vert is already assigned to another vert,
	 * the vert with the further distance is deemed unmappable - in this
	 * situation, we're going to have to do some interior edge based point
	 * insertions (i.e. the hard case).*/
	std::set<overt_t *> om1_unmappable_rverts;
	std::set<overt_t *> om2_unmappable_rverts;

	bool replaceable;

	std::map<cdt_mesh::bedge_seg_t *, std::set<overt_t *>> *edge_verts;

    private:
        void characterize_verts(int ind);
};

bool
closest_surf_pnt(ON_3dPoint &s_p, ON_3dVector &s_norm, cdt_mesh::cdt_mesh_t &fmesh, ON_3dPoint *p, double tol);

int
tri_isect(
	bool process,
	omesh_t *omesh1, cdt_mesh::triangle_t &t1,
	omesh_t *omesh2, cdt_mesh::triangle_t &t2,
	std::map<overt_t *, std::map<cdt_mesh::bedge_seg_t *, int>> *vert_edge_cnts
	);

std::vector<ovlp_grp>
find_ovlp_grps(
	std::map<std::pair<omesh_t *, size_t>, size_t> &bin_map,
	std::set<std::pair<omesh_t *, omesh_t *>> &check_pairs
	);

#endif /* __cdt_mesh_ovlps_h__ */

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
