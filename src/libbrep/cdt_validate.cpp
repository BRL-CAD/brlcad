/*                C D T _ V A L I D A T E . C P P
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
/** @file cdt_validate.cpp
 *
 * Constrained Delaunay Triangulation of NURBS B-Rep objects.
 *
 */

/* TODO:
 *
 * Refactor checking routines (degen triangles, zero area triangles, normal checks)
 * into routines that can run per-face if possible.
 *
 * Need a process for how to modify edges and faces, and in what order.  Need to
 * come up with heuristics for applying different "fixing" strategies:
 *
 * - remove problematic surface point from face tessellation
 * - insert new point(s) at midpoints of edges of problem face
 * - insert new point at the center of the problem face
 * - split edge involved with problem face and retessellate both faces on that edge
 *
 * Different strategies may be appropriate depending on the characteristics of the
 * "bad" triangle - for example, a triangle with all three brep normals nearly perpendicular
 * to the triangle normal and having only one non-edge point we should probably try to
 * handle by removing the surface point.  On the other hand, if the angle is not as
 * perpendicular and the surface area is significant, we might want to add a new point
 * on the longest edge of the triangle (which might be a shared edge with another face.)
 *
 * Also, consider whether to retessellate whole face or try to assemble "local" set of
 * faces involved, build bounding polygon and do new local tessellation.  Latter
 * is a lot more work but might avoid re-introducing new problems elsewhere in a
 * mesh during a full re-CDT.
 *
 * Need to be able to introduce new edge points in specific subranges of an edge.
 *
 * Try to come up with a system that is more systematic, rather than the somewhat
 * ad-hoc routines we have below.  Will have to be iterative, and we can't assume
 * faces that were previously "clean" but which have to be reprocessed due to a new
 * edge point have remained clean.
 *
 * Remember there will be a variety of per-face containers that need to be reset
 * for this operation to work - may want to rework cdt state container to be more
 * per-face sets as opposed to maps-of-sets per face index, so we can wipe and reset
 * a face more easily.
 *
 * These are actually the same operations that will be needed for resolving overlaps,
 * so this is worth solving correctly.
 */


#include "common.h"
#include "./cdt.h"

#define BREP_PLANAR_TOL 0.05

static ON_3dVector
p2tTri_Normal(p2t::Triangle *t, std::map<p2t::Point *, ON_3dPoint *> *pointmap)
{
    ON_3dPoint *p1 = (*pointmap)[t->GetPoint(0)];
    ON_3dPoint *p2 = (*pointmap)[t->GetPoint(1)];
    ON_3dPoint *p3 = (*pointmap)[t->GetPoint(2)];

    ON_3dVector e1 = *p2 - *p1;
    ON_3dVector e2 = *p3 - *p1;
    ON_3dVector tdir = ON_CrossProduct(e1, e2);
    tdir.Unitize();
    return tdir;
}

#if 0
static void
validate_face_normals(
	struct ON_Brep_CDT_State *s_cdt,
	std::vector<p2t::Triangle*> *tris,
	std::set<p2t::Triangle*> *tris_degen,
	int face_index)
{
    std::map<p2t::Point *, ON_3dPoint *> *pointmap = s_cdt->tri_to_on3_maps[face_index];
    std::map<p2t::Point *, ON_3dPoint *> *normalmap = s_cdt->tri_to_on3_norm_maps[face_index];
    for (size_t i = 0; i < tris->size(); i++) {
	p2t::Triangle *t = (*tris)[i];
	if (tris_degen) {
	    if (tris_degen->size() > 0 && tris_degen->find(t) != tris_degen->end()) {
		continue;
	    }
	}
	ON_3dVector tdir = p2tTri_Normal(t, pointmap);
	for (size_t j = 0; j < 3; j++) {
	    p2t::Point *p = t->GetPoint(j);
	    ON_3dPoint onorm = *(*normalmap)[p];
	    if (s_cdt->brep->m_F[face_index].m_bRev) {
		onorm = onorm * -1;
	    }
	    if (tdir.Length() > 0 && ON_DotProduct(onorm, tdir) < 0) {
		ON_3dPoint tri_cent = (*(*pointmap)[t->GetPoint(0)] + *(*pointmap)[t->GetPoint(1)] + *(*pointmap)[t->GetPoint(2)])/3;
		bu_log("Face %d: normal in wrong direction:\n", face_index);
		bu_log("Tri p1: %f %f %f\n", (*pointmap)[t->GetPoint(0)]->x, (*pointmap)[t->GetPoint(0)]->y, (*pointmap)[t->GetPoint(0)]->z);
		bu_log("Tri p2: %f %f %f\n", (*pointmap)[t->GetPoint(1)]->x, (*pointmap)[t->GetPoint(1)]->y, (*pointmap)[t->GetPoint(1)]->z);
		bu_log("Tri p3: %f %f %f\n", (*pointmap)[t->GetPoint(2)]->x, (*pointmap)[t->GetPoint(2)]->y, (*pointmap)[t->GetPoint(2)]->z);
		bu_log("Tri center: %f %f %f\n", tri_cent.x, tri_cent.y, tri_cent.z);
		bu_log("Tri norm: %f %f %f\n", tdir.x, tdir.y, tdir.z);
		bu_log("onorm: %f %f %f\n", onorm.x, onorm.y, onorm.z);
	    }
	}
    }
}
#endif

void
triangles_degenerate_trivial(struct ON_Brep_CDT_State *s_cdt, struct on_brep_mesh_data *md, int face_index)
{
    p2t::CDT *cdt = (*s_cdt->faces)[face_index]->cdt;
    std::map<p2t::Point *, ON_3dPoint *> *pointmap = (*s_cdt->faces)[face_index]->p2t_to_on3_map;
    std::vector<p2t::Triangle*> tris = cdt->GetTriangles();
    for (size_t i = 0; i < tris.size(); i++) {
	p2t::Triangle *t = tris[i];
	md->tri_brep_face[t] = face_index;
	ON_3dPoint *tpnts[3] = {NULL, NULL, NULL};
	for (size_t j = 0; j < 3; j++) {
	    tpnts[j] = (*pointmap)[t->GetPoint(j)];
	}

	/* See if this triangle is trivially degenerate (this can happen with
	 * singular trims) */
	if (tpnts[0] == tpnts[1] || tpnts[1] == tpnts[2] || tpnts[2] == tpnts[0]) {
	    /* degenerate */
	    md->triangle_cnt--;
	    md->tris_degen.insert(t);
	}
    }
}

void
triangles_degenerate_area(struct ON_Brep_CDT_State *s_cdt, struct on_brep_mesh_data *md, int face_index)
{
    // Use a distance three orders of magnitude smaller than the smallest
    // edge segment length of the face to decide if a face is degenerate
    // based on area.
    fastf_t dist = 0.001 * (*s_cdt->min_edge_seg_len)[face_index];
    p2t::CDT *cdt = (*s_cdt->faces)[face_index]->cdt;
    std::map<p2t::Point *, ON_3dPoint *> *pointmap = (*s_cdt->faces)[face_index]->p2t_to_on3_map;
    std::vector<p2t::Triangle*> tris = cdt->GetTriangles();
    for (size_t i = 0; i < tris.size(); i++) {
	p2t::Triangle *t = tris[i];

	if (md->tris_degen.find(t) != md->tris_degen.end()) {
	    continue;
	}

	ON_3dPoint *tpnts[3] = {NULL, NULL, NULL};
	for (size_t j = 0; j < 3; j++) {
	    p2t::Point *p = t->GetPoint(j);
	    ON_3dPoint *op = (*pointmap)[p];
	    tpnts[j] = op;
	}

	/* If we have a face with 3 shared or co-linear points, it's not
	 * trivially degenerate and we need to do more work.  (This can
	 * arise when the 2D triangulation has a non-degenerate triangle
	 * that maps degenerately into 3D). For now, just build up the set
	 * of degenerate triangles. */
	ON_Line l(*tpnts[0], *tpnts[2]);
	int is_zero_area = 0;
	if (l.Length() < dist) {
	    ON_Line l2(*tpnts[0], *tpnts[1]);
	    if (l2.Length() < dist) {
		bu_log("completely degenerate triangle\n");
		md->triangle_cnt--;
		md->tris_degen.insert(t);
		continue;
	    } else {
		if (l2.DistanceTo(*tpnts[2]) < dist) {
		    is_zero_area = 1;
		}
	    }
	} else {
	    if (l.DistanceTo(*tpnts[1]) < dist) {
		is_zero_area = 1;
	    }
	}
	if (is_zero_area) {
	    // The edges from this face are degenerate edges
	    p2t::Point *p2_A = t->GetPoint(0);
	    p2t::Point *p2_B = t->GetPoint(1);
	    p2t::Point *p2_C = t->GetPoint(2);

	    (*s_cdt->faces)[face_index]->degen_pnts->insert(p2_A);
	    (*s_cdt->faces)[face_index]->degen_pnts->insert(p2_B);
	    (*s_cdt->faces)[face_index]->degen_pnts->insert(p2_C);

	    /* If we have degeneracies along an edge, the impact is not
	     * local to this face but will also impact the other face.
	     * Find it and let it know.(probably need another map - 3d pnt
	     * to trim points...) */
	    ON_3dPoint *pt_A = (*pointmap)[p2_A];
	    ON_3dPoint *pt_B = (*pointmap)[p2_B];
	    ON_3dPoint *pt_C = (*pointmap)[p2_C];
	    std::set<BrepTrimPoint *>::iterator bit;
	    std::set<BrepTrimPoint *> &pAt = (*s_cdt->on_brep_edge_pnts)[pt_A];
	    std::set<BrepTrimPoint *> &pBt = (*s_cdt->on_brep_edge_pnts)[pt_B];
	    std::set<BrepTrimPoint *> &pCt = (*s_cdt->on_brep_edge_pnts)[pt_C];
	    for (bit = pAt.begin(); bit != pAt.end(); bit++) {
		BrepTrimPoint *tpt = *bit;
		int f2ind = s_cdt->brep->m_T[tpt->trim_ind].Face()->m_face_index;
		if (f2ind != face_index) {
		    //bu_log("Pulls in face %d\n", f2ind);
		    std::set<p2t::Point *> tri_pnts = (*(*s_cdt->faces)[face_index]->on3_to_tri_map)[pt_A];
		    std::set<p2t::Point *>::iterator tp_it;
		    for (tp_it = tri_pnts.begin(); tp_it != tri_pnts.end(); tp_it++) {
			(*s_cdt->faces)[face_index]->degen_pnts->insert(*tp_it);
		    }
		}
	    }
	    for (bit = pBt.begin(); bit != pBt.end(); bit++) {
		BrepTrimPoint *tpt = *bit;
		int f2ind = s_cdt->brep->m_T[tpt->trim_ind].Face()->m_face_index;
		if (f2ind != face_index) {
		    //bu_log("Pulls in face %d\n", f2ind);
		    std::set<p2t::Point *> tri_pnts = (*(*s_cdt->faces)[face_index]->on3_to_tri_map)[pt_B];
		    std::set<p2t::Point *>::iterator tp_it;
		    for (tp_it = tri_pnts.begin(); tp_it != tri_pnts.end(); tp_it++) {
			(*s_cdt->faces)[face_index]->degen_pnts->insert(*tp_it);
		    }
		}
	    }
	    for (bit = pCt.begin(); bit != pCt.end(); bit++) {
		BrepTrimPoint *tpt = *bit;
		int f2ind = s_cdt->brep->m_T[tpt->trim_ind].Face()->m_face_index;
		if (f2ind != face_index) {
		    //bu_log("Pulls in face %d\n", f2ind);
		    std::set<p2t::Point *> tri_pnts = (*(*s_cdt->faces)[face_index]->on3_to_tri_map)[pt_C];
		    std::set<p2t::Point *>::iterator tp_it;
		    for (tp_it = tri_pnts.begin(); tp_it != tri_pnts.end(); tp_it++) {
			(*s_cdt->faces)[face_index]->degen_pnts->insert(*tp_it);
		    }
		}
	    }

	    md->triangle_cnt--;
	    md->tris_degen.insert(t);
	    md->tris_zero_3D_area.insert(t);
	}
    }
}

void
triangles_incorrect_normals(struct ON_Brep_CDT_State *s_cdt, struct on_brep_mesh_data *md, int face_index)
{
    p2t::CDT *cdt = (*s_cdt->faces)[face_index]->cdt;
    std::map<p2t::Point *, ON_3dPoint *> *pointmap = (*s_cdt->faces)[face_index]->p2t_to_on3_map;
    std::map<p2t::Point *, ON_3dPoint *> *normalmap = (*s_cdt->faces)[face_index]->p2t_to_on3_norm_map;
    std::vector<p2t::Triangle*> tris = cdt->GetTriangles();
    for (size_t i = 0; i < tris.size(); i++) {
	p2t::Triangle *t = tris[i];
	if (md->tris_degen.size() > 0 && md->tris_degen.find(t) != md->tris_degen.end()) {
	    continue;
	}


	int invalid_face_normal = 0;
	ON_3dVector tdir = p2tTri_Normal(t, pointmap);
	p2t::Point *p[3];
	p[0] = t->GetPoint(0);
	p[1] = t->GetPoint(1);
	p[2] = t->GetPoint(2);
	int wnorm[3] = {0, 0, 0};
	for (size_t j = 0; j < 3; j++) {
	    ON_3dPoint onorm = *(*normalmap)[p[j]];
	    if (s_cdt->brep->m_F[face_index].m_bRev) {
		onorm = onorm * -1;
	    }
	    if (tdir.Length() > 0 && ON_DotProduct(onorm, tdir) < 0.1) {
		invalid_face_normal++;
		wnorm[j] = 1;
	    }
	}

	if (invalid_face_normal == 3) {
	    int tind[3] = {-1, -1, -1};
	    int edge_pnt_cnt = 0;
	    std::map<p2t::Point *, int> *p2t_trim_ind = (*s_cdt->faces)[face_index]->p2t_trim_ind;
	    for (int j = 0; j < 3; j++) {
		if (p2t_trim_ind->find(p[j]) != p2t_trim_ind->end()) {
		    tind[j] = (*p2t_trim_ind)[p[j]];
		    edge_pnt_cnt++;
		}
	    }
	    bu_log("Face %d: 3 normals in wrong direction:\n", face_index);
	    if ((tind[0] != -1) && (tind[0] == tind[1]) && (tind[1] == tind[2])) {
		bu_log("invalid face normal with all points on the same trim curve\n");
		ON_3dPoint tri_cent = (*(*pointmap)[t->GetPoint(0)] + *(*pointmap)[t->GetPoint(1)] + *(*pointmap)[t->GetPoint(2)])/3;
		bu_log("Tri p1: %f %f %f\n", (*pointmap)[t->GetPoint(0)]->x, (*pointmap)[t->GetPoint(0)]->y, (*pointmap)[t->GetPoint(0)]->z);
		bu_log("Tri p2: %f %f %f\n", (*pointmap)[t->GetPoint(1)]->x, (*pointmap)[t->GetPoint(1)]->y, (*pointmap)[t->GetPoint(1)]->z);
		bu_log("Tri p3: %f %f %f\n", (*pointmap)[t->GetPoint(2)]->x, (*pointmap)[t->GetPoint(2)]->y, (*pointmap)[t->GetPoint(2)]->z);
		bu_log("Tri center: %f %f %f\n", tri_cent.x, tri_cent.y, tri_cent.z);
		bu_log("Tri norm: %f %f %f\n", tdir.x, tdir.y, tdir.z);
		bu_log("edge trim: %d\n", (*p2t_trim_ind)[t->GetPoint(0)]);

		md->triangle_cnt--;
		md->tris_degen.insert(t);
	    }

	    if (edge_pnt_cnt == 2) {
		bu_log("problem point from surface:\n");
		for (int j = 0; j < 3; j++) {
		    if (tind[j] == -1) {
			ON_3dPoint *spt = (*pointmap)[p[j]];
			bu_log("%f %f %f\n", spt->x, spt->y, spt->z);
		    }
		}
	    }
	    if (edge_pnt_cnt == 1) {
		bu_log("two problem points from surface:\n");
		for (int j = 0; j < 3; j++) {
		    if (tind[j] == -1) {
			ON_3dPoint *spt = (*pointmap)[p[j]];
			bu_log("%f %f %f\n", spt->x, spt->y, spt->z);
		    }
		}

	    }
	    if (edge_pnt_cnt == 0) {
		bu_log("no edge pnts involved:\n");
		for (int j = 0; j < 3; j++) {
		    if (tind[j] == -1) {
			ON_3dPoint *spt = (*pointmap)[p[j]];
			bu_log("%f %f %f\n", spt->x, spt->y, spt->z);
		    }
		}


	    }
	}

	if (invalid_face_normal == 2) {
	    bu_log("Face %d: 2 normals in wrong direction:\n", face_index);
	    for (int j = 0; j < 3; j++) {
		if (wnorm[j] != 0) {
		    ON_3dPoint *spt = (*pointmap)[p[j]];
		    bu_log("%f %f %f\n", spt->x, spt->y, spt->z);
		}
	    }
	}
	if (invalid_face_normal == 1) {
	    bu_log("Face %d: 1 normal in wrong direction:\n", face_index);
	    for (int j = 0; j < 3; j++) {
		if (wnorm[j] != 0) {
		    ON_3dPoint *spt = (*pointmap)[p[j]];
		    bu_log("%f %f %f\n", spt->x, spt->y, spt->z);
		}
	    }

	}
    }
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

