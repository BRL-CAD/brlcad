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
            closest_uedge = -1;
            t_ind = -1;
        }

        omesh_t *omesh;
        long p_id;

        bool edge_vert();

        double min_len();

        ON_BoundingBox bb;

        long closest_uedge;
        bool t_ind;

        ON_3dPoint vpnt();

        void plot(FILE *plot);

        double v_min_edge_len;
};


class omesh_t
{
    public:
        omesh_t(cdt_mesh::cdt_mesh_t *m)
        {
            fmesh = m;
            init_verts();
        };

        // The fmesh pnts array may have inactive vertices - we only want the
        // active verts for this portion of the processing.
        std::map<long, overt_t *> overts;
        RTree<long, double, 3> vtree;
        void add_vtree_vert(overt_t *v);
        void remove_vtree_vert(overt_t *v);
        void plot_vtree(const char *fname);
        bool validate_vtree();
        void save_vtree(const char *fname);
        void load_vtree(const char *fname);

        // Add an fmesh vertex to the overts array and tree.
        overt_t *vert_add(long, ON_BoundingBox *bb = NULL);

        // Find the closest uedge in the mesh to a point
        cdt_mesh::uedge_t closest_uedge(ON_3dPoint &p);
        // Find close (non-face-boundary) edges
        std::set<cdt_mesh::uedge_t> interior_uedges_search(ON_BoundingBox &bb);

        // Find close triangles
        std::set<size_t> tris_search(ON_BoundingBox &bb);

        // Find close vertices
        std::set<overt_t *> vert_search(ON_BoundingBox &bb);
        std::set<overt_t *> vert_search(overt_t *v);
        overt_t * vert_closest(double *vdist, overt_t *v);

        void retessellate(std::set<size_t> &ov);

        // Points from other meshes potentially needing refinement in this mesh
        std::map<overt_t *, std::set<long>> refinement_overts;

        // Points from this mesh inducing refinement in other meshes, and
        // triangles reported by tri_isect as intersecting from this mesh
        std::map<overt_t *, std::set<long>> intruding_overts;
        std::set<size_t> intruding_tris;

        void refinement_clear();
        std::set<long> refinement_split_tris();

        cdt_mesh::cdt_mesh_t *fmesh;

        void plot(const char *fname,
                std::map<cdt_mesh::bedge_seg_t *, std::set<overt_t *>> *ev,
                std::map<cdt_mesh::cdt_mesh_t *, omesh_t *> &f2omap);
        void plot(std::map<cdt_mesh::bedge_seg_t *, std::set<overt_t *>> *ev,
                std::map<cdt_mesh::cdt_mesh_t *, omesh_t *> &f2omap);

        void verts_one_ring_update(long p_id);

        void vupdate(overt_t *v);
    private:
        void init_verts();
        void rebuild_vtree();

        void edge_tris_remove(cdt_mesh::uedge_t &ue);

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

double
tri_pnt_r(cdt_mesh::cdt_mesh_t &fmesh, long tri_ind);

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
