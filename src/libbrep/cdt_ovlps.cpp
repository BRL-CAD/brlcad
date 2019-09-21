/*                   C D T _ O V L P S . C P P
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
 * This file is logic specifically for refining meshes to clear
 * overlaps between sets of objects.
 *
 */

#include "common.h"
#include <queue>
#include <numeric>
#include "bg/chull.h"
#include "bg/tri_tri.h"
#include "./cdt.h"

struct nf_info {
    std::set<std::pair<cdt_mesh::cdt_mesh_t *, cdt_mesh::cdt_mesh_t *>> *check_pairs;
    cdt_mesh::cdt_mesh_t *cmesh;
};

static bool NearFacesCallback(void *data, void *a_context) {
    struct nf_info *nf = (struct nf_info *)a_context;
    cdt_mesh::cdt_mesh_t *omesh = (cdt_mesh::cdt_mesh_t *)data;
    std::pair<cdt_mesh::cdt_mesh_t *, cdt_mesh::cdt_mesh_t *> p1(nf->cmesh, omesh);
    std::pair<cdt_mesh::cdt_mesh_t *, cdt_mesh::cdt_mesh_t *> p2(omesh, nf->cmesh);
    if ((nf->check_pairs->find(p1) == nf->check_pairs->end()) && (nf->check_pairs->find(p1) == nf->check_pairs->end())) {
	nf->check_pairs->insert(p1);
    }
    return true;
}

// Return 0 if no intersection, 1 if coplanar intersection, 2 if non-coplanar intersection
static int 
tri_isect(cdt_mesh::cdt_mesh_t *fmesh1, cdt_mesh::triangle_t &t1, cdt_mesh::cdt_mesh_t *fmesh2, cdt_mesh::triangle_t &t2)
{
    int coplanar = 0;
    point_t isectpt1 = {MAX_FASTF, MAX_FASTF, MAX_FASTF};
    point_t isectpt2 = {MAX_FASTF, MAX_FASTF, MAX_FASTF};
    
    point_t T1_V[3];
    point_t T2_V[3];
    VSET(T1_V[0], fmesh1->pnts[t1.v[0]]->x, fmesh1->pnts[t1.v[0]]->y, fmesh1->pnts[t1.v[0]]->z);
    VSET(T1_V[1], fmesh1->pnts[t1.v[1]]->x, fmesh1->pnts[t1.v[1]]->y, fmesh1->pnts[t1.v[1]]->z);
    VSET(T1_V[2], fmesh1->pnts[t1.v[2]]->x, fmesh1->pnts[t1.v[2]]->y, fmesh1->pnts[t1.v[2]]->z);
    VSET(T2_V[0], fmesh2->pnts[t2.v[0]]->x, fmesh2->pnts[t2.v[0]]->y, fmesh2->pnts[t2.v[0]]->z);
    VSET(T2_V[1], fmesh2->pnts[t2.v[1]]->x, fmesh2->pnts[t2.v[1]]->y, fmesh2->pnts[t2.v[1]]->z);
    VSET(T2_V[2], fmesh2->pnts[t2.v[2]]->x, fmesh2->pnts[t2.v[2]]->y, fmesh2->pnts[t2.v[2]]->z);
    if (bg_tri_tri_isect_with_line(T1_V[0], T1_V[1], T1_V[2], T2_V[0], T2_V[1], T2_V[2], &coplanar, &isectpt1, &isectpt2)) {
	ON_3dPoint p1(isectpt1[X], isectpt1[Y], isectpt1[Z]);
	ON_3dPoint p2(isectpt2[X], isectpt2[Y], isectpt2[Z]);
	if (p1.DistanceTo(p2) < ON_ZERO_TOLERANCE) {
	    std::cout << "skipping pnt isect(" << coplanar << "): " << isectpt1[X] << "," << isectpt1[Y] << "," << isectpt1[Z] << "\n";
	    return 0;
	}

	return (coplanar) ? 1 : 2;
    }

    return 0;
}

#if 0
struct ne_info {
    std::map<int, std::set<cdt_mesh::cpolyedge_t *>> *tris_to_edges;
    size_t tind;
};
static bool NearEdgesCallback(void *data, void *a_context) {
    struct ne_info *ne = (struct ne_info *)a_context;
    cdt_mesh::cpolyedge_t *pe  = (cdt_mesh::cpolyedge_t *)data;
    (*ne->tris_to_edges)[ne->tind].insert(pe);
    return true;
}
#endif

void edge_check() {
    // Determine if the triangle in question needs to be checked against the other brep's edges
    // We need awareness of when the OTHER mesh will need to split its edge.  That means checking
    // the triangle bboxes against the 3D edge rtree and flagging them there, so we know
    // to do a closest-point-on-edge calculation in addition to the surface point attempt.  We
    // don't want to be too reluctant to split edges or we'll end up sampling surface points too
    // close to sparse edges.  Probably need to use the same reject criteria we use in that sampling,
    // and trigger an edge split if we can't get a suitable surface point...  For coplanar problem
    // cases there's no ambiguity - we have to split the edge.
#if 0
    double fMin[3]; double fMax[3];
    ON_3dPoint *p3d = fmesh1->pnts[t1.v[0]];
    ON_BoundingBox bb1(*p3d, *p3d);
    for (int i = 1; i < 3; i++) {
	p3d = fmesh1->pnts[t1.v[i]];
	bb1.Set(*p3d, true);
    }
    fMin[0] = bb1.Min().x;
    fMin[1] = bb1.Min().y;
    fMin[2] = bb1.Min().z;
    fMax[0] = bb1.Max().x;
    fMax[1] = bb1.Max().y;
    fMax[2] = bb1.Max().z;
    struct ne_info ne1;
    ne1.tris_to_edges = &tris_to_opp_face_edges_1;
    ne1.tind = t1.ind;
    size_t nhits1 = s_cdt2->face_rtrees_3d[fmesh2->f_id].Search(fMin, fMax, NearEdgesCallback, &ne1);
    if (nhits1) {
	std::cout << "Face " << fmesh1->f_id << " has potential edge curve interaction with " << fmesh2->f_id << "\n";
    }


    p3d = fmesh2->pnts[t2.v[0]];
    ON_BoundingBox bb2(*p3d, *p3d);
    for (int i = 1; i < 3; i++) {
	p3d = fmesh2->pnts[t2.v[i]];
	bb2.Set(*p3d, true);
    }
    fMin[0] = bb2.Min().x;
    fMin[1] = bb2.Min().y;
    fMin[2] = bb2.Min().z;
    fMax[0] = bb2.Max().x;
    fMax[1] = bb2.Max().y;
    fMax[2] = bb2.Max().z;
    struct ne_info ne2;
    ne2.tris_to_edges = &tris_to_opp_face_edges_2;
    ne2.tind = t2.ind;
    size_t nhits2 = s_cdt1->face_rtrees_3d[fmesh1->f_id].Search(fMin, fMax, NearEdgesCallback, &ne2);
    if (nhits2) {
	std::cout << "Face " << fmesh2->f_id << " has potential edge curve interaction with " << fmesh1->f_id << "\n";
    }
#endif
}

int
ON_Brep_CDT_Ovlp_Resolve(struct ON_Brep_CDT_State **s_a, int s_cnt)
{
    if (!s_a) return -1;
    if (s_cnt < 1) return 0;

    // Get the bounding boxes of all faces of all breps in s_a, and
    std::set<std::pair<cdt_mesh::cdt_mesh_t *, cdt_mesh::cdt_mesh_t *>> check_pairs;
    RTree<void *, double, 3> rtree_fmeshes;
    for (int i = 0; i < s_cnt; i++) {
	struct ON_Brep_CDT_State *s_i = s_a[i];
	for (int i_fi = 0; i_fi < s_i->brep->m_F.Count(); i_fi++) {
	    const ON_BrepFace *i_face = s_i->brep->Face(i_fi);
	    ON_BoundingBox bb = i_face->BoundingBox();
	    cdt_mesh::cdt_mesh_t *fmesh = &s_i->fmeshes[i_fi];
	    struct nf_info nf;
	    nf.cmesh = fmesh;
	    nf.check_pairs = &check_pairs;
	    double fMin[3];
	    fMin[0] = bb.Min().x;
	    fMin[1] = bb.Min().y;
	    fMin[2] = bb.Min().z;
	    double fMax[3];
	    fMax[0] = bb.Max().x;
	    fMax[1] = bb.Max().y;
	    fMax[2] = bb.Max().z;
	    // Check the new box against the existing tree, and add any new
	    // interaction pairs to check_pairs
	    rtree_fmeshes.Search(fMin, fMax, NearFacesCallback, (void *)&nf);
	    // Add the new box to the tree so any additional boxes can check
	    // against it as well
	    rtree_fmeshes.Insert(fMin, fMax, (void *)fmesh);
	}
    }

    std::cout << "check_pairs: " << check_pairs.size() << "\n";
    std::set<std::pair<cdt_mesh::cdt_mesh_t *, cdt_mesh::cdt_mesh_t *>>::iterator cp_it;
    for (cp_it = check_pairs.begin(); cp_it != check_pairs.end(); cp_it++) {
	cdt_mesh::cdt_mesh_t *fmesh1 = cp_it->first;
	cdt_mesh::cdt_mesh_t *fmesh2 = cp_it->second;
	struct ON_Brep_CDT_State *s_cdt1 = (struct ON_Brep_CDT_State *)fmesh1->p_cdt;
	struct ON_Brep_CDT_State *s_cdt2 = (struct ON_Brep_CDT_State *)fmesh2->p_cdt;
	if (s_cdt1 != s_cdt2) {
	    std::cout << "Checking " << fmesh1->name << " face " << fmesh1->f_id << " against " << fmesh2->name << " face " << fmesh2->f_id << "\n";
	    std::set<std::pair<size_t, size_t>> tris_prelim;
	    std::map<int, std::set<cdt_mesh::cpolyedge_t *>> tris_to_opp_face_edges_1;
	    std::map<int, std::set<cdt_mesh::cpolyedge_t *>> tris_to_opp_face_edges_2;
	    size_t ovlp_cnt = fmesh1->tris_tree.Overlaps(fmesh2->tris_tree, &tris_prelim);
	    if (ovlp_cnt) {
		std::cout << "   found " << ovlp_cnt << " box overlaps\n";
		std::set<std::pair<size_t, size_t>>::iterator tb_it;
		for (tb_it = tris_prelim.begin(); tb_it != tris_prelim.end(); tb_it++) {
		    cdt_mesh::triangle_t t1 = fmesh1->tris_vect[tb_it->first];
		    cdt_mesh::triangle_t t2 = fmesh2->tris_vect[tb_it->second];
		    int isect = tri_isect(fmesh1, t1, fmesh2, t2);
		    if (isect) {


			//std::cout << "isect(" << coplanar << "): " << isectpt1[X] << "," << isectpt1[Y] << "," << isectpt1[Z] << " -> " << isectpt2[X] << "," << isectpt2[Y] << "," << isectpt2[Z] << "\n";

			// Using triangle planes, determine which point(s) from the opposite triangle are
			// "inside" the meshes.  Each of these points is an "overlap instance" that the
			// opposite mesh will have to try and adjust itself to to resolve.
			std::set<size_t> fmesh1_interior_pnts;
			std::set<size_t> fmesh2_interior_pnts;
			ON_Plane plane1 = fmesh1->tplane(t1);
			for (int i = 0; i < 3; i++) {
			    ON_3dPoint tp = *fmesh2->pnts[t2.v[i]];
			    double dist = plane1.DistanceTo(tp);
			    if (dist < 0 && fabs(dist) > ON_ZERO_TOLERANCE) {
				std::cout << "face " << fmesh1->f_id << " new interior point from face " << fmesh2->f_id << ": " << tp.x << "," << tp.y << "," << tp.z << "\n";
				struct brep_face_ovlp_instance *ovlp = new struct brep_face_ovlp_instance;
				ovlp->intruding_pnt_src_mesh = fmesh2;
				ovlp->intruding_pnt = t2.v[i];
				ovlp->local_intersected_triangle = t1.ind;
				ovlp->coplanar_intersection = (isect == 1) ? true : false;
				s_cdt1->face_ovlps[fmesh1->f_id].push_back(ovlp);
			    }
			}

			ON_Plane plane2 = fmesh2->tplane(t2);
			for (int i = 0; i < 3; i++) {
			    ON_3dPoint tp = *fmesh1->pnts[t1.v[i]];
			    double dist = plane2.DistanceTo(tp);
			    if (dist < 0 && fabs(dist) > ON_ZERO_TOLERANCE) {
				std::cout << "face " << fmesh2->f_id << " new interior point from face " << fmesh1->f_id << ": " << tp.x << "," << tp.y << "," << tp.z << "\n";
				fmesh1_interior_pnts.insert(t1.v[i]);
				struct brep_face_ovlp_instance *ovlp = new struct brep_face_ovlp_instance;
				ovlp->intruding_pnt_src_mesh = fmesh1;
				ovlp->intruding_pnt = t1.v[i];
				ovlp->local_intersected_triangle = t2.ind;
				ovlp->coplanar_intersection = (isect == 1) ? true : false;
				s_cdt2->face_ovlps[fmesh2->f_id].push_back(ovlp);
			    }
			}

		    }
		}
	    } else {
		std::cout << "RTREE_ISECT_EMPTY: " << fmesh1->name << " face " << fmesh1->f_id << " and " << fmesh2->name << " face " << fmesh2->f_id << "\n";
	    }
	} else {
	    // TODO: In principle we should be checking for self intersections
	    // - it can happen, particularly in sparse tessellations. That's
	    // why the above doesn't filter out same-object face overlaps, but
	    // for now ignore it.  We need to be able to ignore triangles that
	    // only share a 3D edge.
	}
    }

    for (int i = 0; i < s_cnt; i++) {
	struct ON_Brep_CDT_State *s_i = s_a[i];
	for (int i_fi = 0; i_fi < s_i->brep->m_F.Count(); i_fi++) {
	    if (s_i->face_ovlps[i_fi].size()) {
		std::cout << s_i->name << " face " << i_fi << " overlap cnt " << s_i->face_ovlps[i_fi].size() << "\n";
	    }
	}
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

