/*                   C D T _ O V L P S . C P P
 * BRL-CAD
 *
 * Copyright (c) 2007-2020 United States Government as represented by
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
#include <iostream>
#include <numeric>
#include <queue>
#include <random>
#include <string>
#include "bg/chull.h"
#include "bg/tri_pt.h"
#include "bg/tri_tri.h"
#include "./cdt.h"

void
plot_active_omeshes(
	std::set<std::pair<cdt_mesh_t *, cdt_mesh_t *>> &check_pairs
	)
{
    std::set<omesh_t *> a_omesh = itris_omeshes(check_pairs);
    std::set<omesh_t *>::iterator a_it;
    // Plot overlaps with refinement points
    for (a_it = a_omesh.begin(); a_it != a_omesh.end(); a_it++) {
	omesh_t *am = *a_it;
	am->plot();
    }
}


// Functions to build sets of active meshes from the active pairs set

std::set<omesh_t *>
active_omeshes(std::set<std::pair<cdt_mesh_t *, cdt_mesh_t *>> &check_pairs)
{
    std::set<omesh_t *> a_omesh;
    std::set<omesh_t *>::iterator a_it;
    // Find the meshes with actual refinement points
    std::set<std::pair<cdt_mesh_t *, cdt_mesh_t *>>::iterator cp_it;
    for (cp_it = check_pairs.begin(); cp_it != check_pairs.end(); cp_it++) {
	omesh_t *omesh1 = cp_it->first->omesh;
	omesh_t *omesh2 = cp_it->second->omesh;
	a_omesh.insert(omesh1);
	a_omesh.insert(omesh2);
    }

    return a_omesh;
}

std::set<omesh_t *>
refinement_omeshes(std::set<std::pair<cdt_mesh_t *, cdt_mesh_t *>> &check_pairs)
{
    std::set<omesh_t *> a_omesh;
    std::set<omesh_t *>::iterator a_it;
    // Find the meshes with actual refinement points
    std::set<std::pair<cdt_mesh_t *, cdt_mesh_t *>>::iterator cp_it;
    for (cp_it = check_pairs.begin(); cp_it != check_pairs.end(); cp_it++) {
	omesh_t *omesh1 = cp_it->first->omesh;
	omesh_t *omesh2 = cp_it->second->omesh;
	if (omesh1->edge_sets.size()) {
	    a_omesh.insert(omesh1);
	}
	if (omesh2->edge_sets.size()) {
	    a_omesh.insert(omesh2);
	}
    }

    return a_omesh;
}

std::set<omesh_t *>
itris_omeshes(std::set<std::pair<cdt_mesh_t *, cdt_mesh_t *>> &check_pairs)
{
    std::set<omesh_t *> a_omesh;
    std::set<omesh_t *>::iterator a_it;
    // Find the meshes with actual refinement points
    std::set<std::pair<cdt_mesh_t *, cdt_mesh_t *>>::iterator cp_it;
    for (cp_it = check_pairs.begin(); cp_it != check_pairs.end(); cp_it++) {
	omesh_t *omesh1 = cp_it->first->omesh;
	omesh_t *omesh2 = cp_it->second->omesh;
	if (omesh1->itris.size()) {
	    a_omesh.insert(omesh1);
	}
	if (omesh2->itris.size()) {
	    a_omesh.insert(omesh2);
	}
    }

    return a_omesh;
}

/******************************************************************************
 * As a first step, use the face bboxes to narrow down where we have potential
 * interactions between breps
 ******************************************************************************/

struct nf_info {
    std::set<std::pair<cdt_mesh_t *, cdt_mesh_t *>> *check_pairs;
    cdt_mesh_t *cmesh;
};

static bool NearFacesPairsCallback(void *data, void *a_context) {
    struct nf_info *nf = (struct nf_info *)a_context;
    cdt_mesh_t *omesh = (cdt_mesh_t *)data;
    std::pair<cdt_mesh_t *, cdt_mesh_t *> p1(nf->cmesh, omesh);
    std::pair<cdt_mesh_t *, cdt_mesh_t *> p2(omesh, nf->cmesh);
    if ((nf->check_pairs->find(p1) == nf->check_pairs->end()) && (nf->check_pairs->find(p1) == nf->check_pairs->end())) {
	nf->check_pairs->insert(p1);
    }
    return true;
}

std::set<std::pair<cdt_mesh_t *, cdt_mesh_t *>>
possibly_interfering_face_pairs(struct ON_Brep_CDT_State **s_a, int s_cnt)
{
    std::set<std::pair<cdt_mesh_t *, cdt_mesh_t *>> check_pairs;
    RTree<void *, double, 3> rtree_fmeshes;
    for (int i = 0; i < s_cnt; i++) {
	struct ON_Brep_CDT_State *s_i = s_a[i];
	for (int i_fi = 0; i_fi < s_i->brep->m_F.Count(); i_fi++) {
	    const ON_BrepFace *i_face = s_i->brep->Face(i_fi);
	    ON_BoundingBox bb = i_face->BoundingBox();
	    cdt_mesh_t *fmesh = &s_i->fmeshes[i_fi];
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
	    rtree_fmeshes.Search(fMin, fMax, NearFacesPairsCallback, (void *)&nf);
	    // Add the new box to the tree so any additional boxes can check
	    // against it as well
	    rtree_fmeshes.Insert(fMin, fMax, (void *)fmesh);
	}
    }

    return check_pairs;
}

overt_t *
nearby_vert(ON_3dPoint &spnt, ON_3dVector &sn, overt_t *v, omesh_t *other_m, int level)
{
    // If we already know the answer, just return it and skip all the calculations...
    if (v->aligned.find(other_m) != v->aligned.end()) {
	overt_t *ov = v->aligned[other_m];
	if (v->alevel[ov] == level) {
	    spnt = v->cpoints[ov];
	    sn = v->cnorms[ov];
	    return ov;
	} else {
	    // Alignment was done at another level - closest surface point is still
	    // OK, but vert mapping may not be - erase it and make a new determination.
	    v->aligned.erase(other_m);
	}
    }

    // Past the easy case - now we need to do some work.  Find the closest vertex point
    // and the closest surface point/normal.
    double vdist;
    overt_t *v_closest = other_m->vert_closest(&vdist, v);

    ON_3dPoint opnt = v->vpnt();
    ON_3dPoint vcpnt = v_closest->vpnt();
    double vo_to_vc = vcpnt.DistanceTo(opnt);
    double spdist = vo_to_vc * 10;
    if (v->cpoints.find(v_closest) != v->cpoints.end() && v->cnorms.find(v_closest) != v->cnorms.end()) {
	// If we already have calculated the closest surface point, just reuse it
	spnt = v->cpoints[v_closest];
	sn = v->cnorms[v_closest];
    } else {
	if (!other_m->fmesh->closest_surf_pnt(spnt, sn, &opnt, spdist)) {
	    // If there's no closest surface point within dist, there's probably
	    // an error - it should at least have found the v_closest point...
	    std::cerr << "Error - couldn't find closest point\n";
	} else {
	    v->cpoints[v_closest] = spnt;
	    v->cnorms[v_closest] = sn;
	}
    }

    // If we're extremely close, just go with v_closest
    if (vdist < 2*ON_ZERO_TOLERANCE) {
	v->alevel[v_closest] = level;
	return v_closest;
    }

    // See if the closest vert bbox overlaps the closest surface
    // point to v.  If not, there is definitely no nearby vertex defined,
    // since we know spnt is free to be defined as a vertex according
    // to other_m's vertices.
    ON_BoundingBox spbb(spnt, spnt);
    if (v_closest->bb.IsDisjoint(spbb)) {
	return NULL;
    }

    // We know v_closest isn't nearby according according to v_closest's bbox,
    // but v_closest may have an arbitrarily sized bbox depending on its
    // associated edge lengths.  Hence it may contain spnt even though (from
    // v's perspective) it isn't close.  Construct a new bbox using the
    // v_closest point and the box size of v, and repeat the bbox check for
    // spbb.
    ON_BoundingBox v_closest_v_bb(v_closest->vpnt(), v_closest->vpnt());
    ON_3dPoint c1 = vcpnt + v->bb.Diagonal()*0.5;
    ON_3dPoint c2 = vcpnt - v->bb.Diagonal()*0.5;
    v_closest_v_bb.Set(c1, true);
    v_closest_v_bb.Set(c2, true);
    if (v_closest_v_bb.IsDisjoint(spbb)) {
	return NULL;
    }

    // If the surface point is close to the closest existing vertex per its
    // bounding box size, report the vertex as nearby.  In essence this can be
    // considered an "approximate" nearby vert match, keyed to local edge
    // lengths from involved triangle edges.
    double factor = (level < INT_MAX) ? (1.0/(double)(10 + level)) : 0;
    double cvbbdiag = v_closest->bb.Diagonal().Length() * factor + 2 * ON_ZERO_TOLERANCE;
    if (vcpnt.DistanceTo(spnt) < cvbbdiag) {
	v->aligned[other_m] = v_closest;
	v->alevel[v_closest] = level;
	return v_closest;
    }

    return NULL;
}

int
add_refinement_vert(overt_t *v, omesh_t *other_m, int level)
{
    // If we've already got a matching vertex, we don't change anything because
    // of this vertex.
    ON_3dPoint spnt;
    ON_3dVector sn;
    if (nearby_vert(spnt, sn, v, other_m, level)) {
	return 0;
    }

    // If we're close to a brep face edge, needs to go in edge_verts.
    // else, list as a new vert requiring a nearby vert on mesh other_m.
    //
    // What should happen in successive iterations is the following:
    // 1.  The first pass will split the brep face edge at the closest
    // point to the near-edge point in question, introducing a new
    // triangle edge and triangles in the vicinity of the nearby vertex.
    //
    // 2.  A second pass will then find the closest edge to the same
    // vertex (if the vertex is still intruding) closer to the new
    // interior edge than the original brep face edge, and the categorization
    // of the point will change to an interior point.
    ON_3dPoint vpnt = v->vpnt();
    uedge_t closest_edge = other_m->closest_uedge(vpnt);
    if (other_m->fmesh->brep_edges.find(closest_edge) != other_m->fmesh->brep_edges.end()) {
	bedge_seg_t *bseg = other_m->fmesh->ue2b_map[closest_edge];
	if (!bseg) {
	    std::cout << "couldn't find bseg pointer??\n";
	    return 0;
	} else {
	    (*other_m->edge_verts)[bseg].insert(v);
	}
    } else {
	// Point is interior - stash relevant information
	revt_pt_t rpt;
	rpt.spnt = spnt;
	rpt.sn = sn;
	rpt.ov = v;
	other_m->edge_sets[closest_edge].push_back(rpt);
    }

    return 1;
}


/* There are three levels of aggressiveness we can use when assigning refinement points:
 *
 * 1.  Only use vertices that share at least two triangles which overlap with the other mesh
 * 2.  Use all vertices from triangles that overlap
 * 3.  MAYBE TODO... Use all vertices from the other mesh that are contained by the bounding box of the
 *     overlapping triangle from this mesh.
 */
size_t
omesh_refinement_pnts(
	std::set<std::pair<cdt_mesh_t *, cdt_mesh_t *>> check_pairs,
	int level
	)
{
    std::set<omesh_t *> a_omesh = active_omeshes(check_pairs);
    std::set<omesh_t *>::iterator a_it;

    // Clear everything, even if we don't have intersecting triangles
    (*a_omesh.begin())->edge_verts->clear();
    for (a_it = a_omesh.begin(); a_it != a_omesh.end(); a_it++) {
	(*a_it)->edge_sets.clear();
	(*a_it)->ivert_ref_cnts.clear();
    }

    a_omesh = itris_omeshes(check_pairs);
    if (!a_omesh.size()) return 0;

    // Count triangles to determine which verts need attention.  If a vertex is associated
    // with two or more triangles that intersect another face, it is a refinement point
    // candidate.
    for (a_it = a_omesh.begin(); a_it != a_omesh.end(); a_it++) {
	omesh_t *am = *a_it;
	std::map<size_t, std::set<std::pair<omesh_t *, size_t>> >::iterator p_it;
	for (p_it = am->itris.begin(); p_it != am->itris.end(); p_it++) {
	    std::set<std::pair<omesh_t *, size_t>>::iterator s_it;
	    for (s_it = p_it->second.begin(); s_it != p_it->second.end(); s_it++) {
		omesh_t *im = s_it->first;
		size_t otri_ind = s_it->second;
		triangle_t tri = im->fmesh->tris_vect[otri_ind];
		for (int i = 0; i < 3; i++) {
		    overt_t *v = im->overts[tri.v[i]];
		    am->ivert_ref_cnts[v].insert(otri_ind);
		}
	    }
	}
    }
    for (a_it = a_omesh.begin(); a_it != a_omesh.end(); a_it++) {
	omesh_t *am = *a_it;
	std::map<overt_t *, std::set<long>>::iterator r_it;
	for (r_it = am->ivert_ref_cnts.begin(); r_it != am->ivert_ref_cnts.end(); r_it++) {
	    //VPCHECK(r_it->first, NULL);
	    // If there are enough activity hits, we need a nearby vertex
	    if (r_it->second.size() > 1 || level > 0) {
		add_refinement_vert(r_it->first, am, level);
	    }
	}
    }

    //plot_active_omeshes(check_pairs, NULL);

    // Add triangle intersection vertices that are close to the edge of the opposite
    // triangle, whether or not they satisfy the count criteria - these are a source
    // of potential trouble.
    if (!level) {
	for (a_it = a_omesh.begin(); a_it != a_omesh.end(); a_it++) {
	    omesh_t *am = *a_it;
	    std::map<size_t, std::set<std::pair<omesh_t *, size_t>> >::iterator p_it;
	    for (p_it = am->itris.begin(); p_it != am->itris.end(); p_it++) {
		triangle_t t1 = am->fmesh->tris_vect[p_it->first];
		std::set<std::pair<omesh_t *, size_t>>::iterator s_it;
		for (s_it = p_it->second.begin(); s_it != p_it->second.end(); s_it++) {
		    omesh_t *im = s_it->first;
		    size_t otri_ind = s_it->second;
		    triangle_t t2 = im->fmesh->tris_vect[otri_ind];
		    tri_nearedge_refine(t1, t2, level);
		}
	    }
	}
    }

    size_t rcnt = 0;
    for (a_it = a_omesh.begin(); a_it != a_omesh.end(); a_it++) {
	omesh_t *am = *a_it;
	rcnt += am->edge_sets.size();
    }

    plot_active_omeshes(check_pairs);

    return rcnt;
}

bool
omesh_smooth(std::set<std::pair<cdt_mesh_t *, cdt_mesh_t *>> check_pairs)
{
    std::set<omesh_t *> a_omesh = active_omeshes(check_pairs);
    std::set<omesh_t *>::iterator a_it;

    for (a_it = a_omesh.begin(); a_it != a_omesh.end(); a_it++) {
	omesh_t *am = *a_it;
	if (!am->fmesh->optimize()) {
	    return false;
	}
	// Need to update vertex bboxes after changing triangles!
	std::set<triangle_t>::iterator n_it;
	std::set<overt_t *> tri_verts;
	for (n_it = am->fmesh->new_tris.begin(); n_it != am->fmesh->new_tris.end(); n_it++) {
	    for (int i = 0; i < 3; i++) {
		tri_verts.insert(am->overts[(*n_it).v[i]]);
	    }
	}
	std::set<overt_t *>::iterator o_it;
	for (o_it = tri_verts.begin(); o_it != tri_verts.end(); o_it++) {
	    (*o_it)->update();
	}
    }

    return true;
}

size_t
omesh_ovlps(std::set<std::pair<cdt_mesh_t *, cdt_mesh_t *>> check_pairs, int mode)
{
    size_t tri_isects = 0;

    // Assemble the set of mesh faces that (per the check_pairs sets) we
    // may need to process.
    std::set<omesh_t *> a_omesh = active_omeshes(check_pairs);
    std::set<omesh_t *>::iterator a_it;

    // Scrub the old data in active mesh containers (if any)
    for (a_it = a_omesh.begin(); a_it != a_omesh.end(); a_it++) {
	omesh_t *am = *a_it;
	am->itris.clear();
	am->intruding_tris.clear();
    }
    a_omesh.clear();

    // Intersect first the triangle RTrees, then any potentially
    // overlapping triangles found within the leaves.
    std::set<std::pair<cdt_mesh_t *, cdt_mesh_t *>>::iterator cp_it;
    for (cp_it = check_pairs.begin(); cp_it != check_pairs.end(); cp_it++) {
	omesh_t *omesh1 = cp_it->first->omesh;
	omesh_t *omesh2 = cp_it->second->omesh;
	struct ON_Brep_CDT_State *s_cdt1 = (struct ON_Brep_CDT_State *)omesh1->fmesh->p_cdt;
	struct ON_Brep_CDT_State *s_cdt2 = (struct ON_Brep_CDT_State *)omesh2->fmesh->p_cdt;
	if (s_cdt1 != s_cdt2) {
	    std::set<std::pair<size_t, size_t>> tris_prelim;
	    size_t ovlp_cnt = omesh1->fmesh->tris_tree.Overlaps(omesh2->fmesh->tris_tree, &tris_prelim);
	    if (ovlp_cnt) {
		std::set<std::pair<size_t, size_t>>::iterator tb_it;
		for (tb_it = tris_prelim.begin(); tb_it != tris_prelim.end(); tb_it++) {
		    triangle_t t1 = omesh1->fmesh->tris_vect[tb_it->first];
		    triangle_t t2 = omesh2->fmesh->tris_vect[tb_it->second];
		    int isect = tri_isect(t1, t2, mode);
		    if (isect) {
			omesh1->itris[t1.ind].insert(std::make_pair(omesh2, t2.ind));
			omesh2->itris[t2.ind].insert(std::make_pair(omesh1, t1.ind));
			omesh1->intruding_tris.insert(t1.ind);
			omesh2->intruding_tris.insert(t2.ind);
			a_omesh.insert(omesh1);
			a_omesh.insert(omesh2);
			tri_isects++;
		    }
		}
	    }
	}
    }

    //plot_active_omeshes(check_pairs, NULL);

    return tri_isects;
}

void
flip_tri(triangle_t &t)
{
    long tmp = t.v[2];
    t.v[2] = t.v[1];
    t.v[1] = tmp;
}

bool
orient_tri(cdt_mesh_t &fmesh, triangle_t &t)
{
    ON_3dVector tdir = fmesh.tnorm(t);
    ON_3dVector bdir = fmesh.bnorm(t);
    bool flipped_tri = (ON_DotProduct(tdir, bdir) < 0);
    if (flipped_tri) {
	long tmp = t.v[2];
	t.v[2] = t.v[1];
	t.v[1] = tmp;
	return true;
    }
    return false;
}

int
ovlp_split_interior_edge(overt_t **nv, std::set<long> &ntris, omesh_t *omesh, uedge_t &ue)
{
    std::set<overt_t *> overts_to_update;

    // Find the two triangles that we will be using to form the outer polygon
    std::set<size_t> rtris = omesh->fmesh->uedges2tris[ue];
    if (rtris.size() != 2) {
	std::cout << "Error - could not associate uedge with two triangles??\n";
	return -1;
    }
    std::set<size_t> crtris = rtris;
    triangle_t tri1 = omesh->fmesh->tris_vect[*crtris.begin()];
    crtris.erase(crtris.begin());
    triangle_t tri2 = omesh->fmesh->tris_vect[*crtris.begin()];
    crtris.erase(crtris.begin());

    cpolygon_t *polygon = omesh->fmesh->uedge_polygon(ue);

    // Add interior point.
    ON_3dPoint epnt = (*omesh->fmesh->pnts[ue.v[0]] + *omesh->fmesh->pnts[ue.v[1]]) * 0.5;
    double spdist = omesh->fmesh->pnts[ue.v[0]]->DistanceTo(*omesh->fmesh->pnts[ue.v[1]]);
    ON_3dPoint spnt;
    ON_3dVector sn;
    if (!omesh->fmesh->closest_surf_pnt(spnt, sn, &epnt, spdist*10)) {
	std::cerr << "Error - couldn't find closest point\n";
	delete polygon;
	return -1;
    }
    // Base the bbox on the edge length, since we don't have any topology yet.
    ON_BoundingBox sbb(spnt,spnt);
    ON_3dPoint sbb1 = spnt - ON_3dPoint(.1*spdist, .1*spdist, .1*spdist);
    ON_3dPoint sbb2 = spnt + ON_3dPoint(.1*spdist, .1*spdist, .1*spdist);
    sbb.Set(sbb1, true);
    sbb.Set(sbb2, true);

    // We're going to use it - add new 3D point
    long f3ind = omesh->fmesh->add_point(new ON_3dPoint(spnt));
    long fnind = omesh->fmesh->add_normal(new ON_3dPoint(sn));
    struct ON_Brep_CDT_State *s_cdt = (struct ON_Brep_CDT_State *)omesh->fmesh->p_cdt;
    CDT_Add3DPnt(s_cdt, omesh->fmesh->pnts[f3ind], omesh->fmesh->f_id, -1, -1, -1, 0, 0);
    CDT_Add3DNorm(s_cdt, omesh->fmesh->normals[fnind], omesh->fmesh->pnts[f3ind], omesh->fmesh->f_id, -1, -1, -1, 0, 0);
    omesh->fmesh->nmap[f3ind] = fnind;
    overt_t *nvrt = omesh->vert_add(f3ind, &sbb);
    overts_to_update.insert(nvrt);

    // The 'old' triangles may themselves have been newly introduced - we're now going to remove
    // them from the mesh regardless, so make sure the ntris set doesn't include them any more.
    ntris.erase(tri1.ind);
    ntris.erase(tri2.ind);

    // Split the old triangles, perform the removals and add the new triangles
    int new_tris = 0;
    std::set<triangle_t>::iterator s_it;
    std::set<triangle_t> t1_tris = tri1.split(ue, f3ind, false);
    omesh->fmesh->tri_remove(tri1);
    for (s_it = t1_tris.begin(); s_it != t1_tris.end(); s_it++) {
	triangle_t vt = *s_it;
	omesh->fmesh->tri_add(vt);
	ntris.insert(omesh->fmesh->tris_vect.size() - 1);
	// In addition to the genuinely new vertices, altered triangles
	// may need updated vert bboxes
	for (int i = 0; i < 3; i++) {
	    overts_to_update.insert(omesh->overts[vt.v[i]]);
	}
	new_tris++;
    }
    std::set<triangle_t> t2_tris = tri2.split(ue, f3ind, false);
    omesh->fmesh->tri_remove(tri2);
    for (s_it = t2_tris.begin(); s_it != t2_tris.end(); s_it++) {
	triangle_t vt = *s_it;
	omesh->fmesh->tri_add(vt);
	ntris.insert(omesh->fmesh->tris_vect.size() - 1);
	// In addition to the genuinely new vertices, altered triangles
	// may need updated vert bboxes
	for (int i = 0; i < 3; i++) {
	    overts_to_update.insert(omesh->overts[vt.v[i]]);
	}
	new_tris++;
    }

    // Have new triangles, update overts
    std::set<overt_t *>::iterator n_it;
    for (n_it = overts_to_update.begin(); n_it != overts_to_update.end(); n_it++) {
	(*n_it)->update();
    }

    (*nv) = nvrt;

    //omesh->fmesh->valid(1);

    return new_tris;
}


static int
interior_edge_verts(omesh_t *omesh)
{
    struct ON_Brep_CDT_State *s_cdt = (struct ON_Brep_CDT_State *)omesh->fmesh->p_cdt;

    //std::cout << "Processing " << s_cdt->name << " face " << omesh->fmesh->f_id << ":\n";

    std::map<uedge_t, std::vector<revt_pt_t>>::iterator es_it;

    int new_tris = 0; 

    std::map<uedge_t, std::vector<revt_pt_t>> wedge_sets = omesh->edge_sets;

    while (wedge_sets.size()) {
	std::map<uedge_t, std::vector<revt_pt_t>> updated_esets;

	uedge_t ue = wedge_sets.begin()->first;
	std::vector<revt_pt_t> epnts = wedge_sets.begin()->second;
	wedge_sets.erase(wedge_sets.begin());

	// Find the two triangles that we will be using to form the outer polygon
	std::set<size_t> rtris = omesh->fmesh->uedges2tris[ue];
	if (rtris.size() != 2) {
	    std::cout << "Error - could not associate uedge with two triangles??\n";
	}
	std::set<size_t> crtris = rtris;
	triangle_t tri1 = omesh->fmesh->tris_vect[*crtris.begin()];
	crtris.erase(crtris.begin());
	triangle_t tri2 = omesh->fmesh->tris_vect[*crtris.begin()];
	crtris.erase(crtris.begin());

	// For the involved triangle edges that are not the edge to be removed, we will
	// need to reassess their closest edge assignment after new edges are added.
	// Build up the set of those uedges for later processing.  While we are looping,
	// get initial data for the polygon build.
	std::set<uedge_t> pnt_reassignment_edges;
	std::set<size_t>::iterator r_it;
	for (r_it = rtris.begin(); r_it != rtris.end(); r_it++) {
	    triangle_t tri = omesh->fmesh->tris_vect[*r_it];
	    std::set<uedge_t> tuedges = omesh->fmesh->uedges(tri);
	    pnt_reassignment_edges.insert(tuedges.begin(), tuedges.end());
	}
	pnt_reassignment_edges.erase(ue);


	cpolygon_t *polygon = omesh->fmesh->uedge_polygon(ue);

#if 0
	{
	FILE *t1plot = fopen("t1.plot", "w");
	struct bu_color c = BU_COLOR_INIT_ZERO;
	unsigned char rgb[3] = {0,0,255};
	bu_color_from_rgb_chars(&c, (const unsigned char *)rgb);
	omesh->fmesh->plot_tri(tri1, &c, t1plot, 255, 0, 0);
	fclose(t1plot);
	}
	{
	FILE *t2plot = fopen("t2.plot", "w");
	struct bu_color c = BU_COLOR_INIT_ZERO;
	unsigned char rgb[3] = {0,0,255};
	bu_color_from_rgb_chars(&c, (const unsigned char *)rgb);
	omesh->fmesh->plot_tri(tri2, &c, t2plot, 255, 0, 0);
	fclose(t2plot);
	}
#endif

	// Add any interior points to the polygon before cdt.
	std::set<overt_t *> overts_to_update;
	bool have_inside = false;
	for (size_t i = 0; i < epnts.size(); i++) {
	    bool skip_epnt = false;

	    VPCHECK(epnts[i].ov, NULL);

	    ON_BoundingBox sbb(epnts[i].spnt,epnts[i].spnt);
	    ON_3dVector vmin = epnts[i].ov->bb.Min() - epnts[i].ov->bb.Center();
	    ON_3dVector vmax = epnts[i].ov->bb.Max() - epnts[i].ov->bb.Center();
	    vmin.Unitize();
	    vmax.Unitize();
	    vmin = vmin * epnts[i].ov->bb.Diagonal().Length() * 0.5;
	    vmax = vmax * epnts[i].ov->bb.Diagonal().Length() * 0.5;
	    sbb.m_min = epnts[i].spnt + vmin;
	    sbb.m_max = epnts[i].spnt + vmax;


	    double vdist;
	    overt_t *v_closest = omesh->vert_closest(&vdist, epnts[i].spnt);
	    double cvbbdiag = v_closest->bb.Diagonal().Length() * 0.1;
	    if (vdist < cvbbdiag) {
		// Too close to a vertex in the current mesh, skip
		//std::cout << "skip epnt, too close to vert\n";
		continue;
	    }

	    // If the point is too close to a brep face edge,
	    // we also need to reject it to avoid creating degenerate
	    // triangles
	    std::set<uedge_t>::iterator ue_it;
	    std::set<uedge_t> b_ue_1 = omesh->fmesh->b_uedges(tri1);
	    for (ue_it = b_ue_1.begin(); ue_it != b_ue_1.end(); ue_it++) {
		uedge_t lue = *ue_it;
		double epdist = omesh->fmesh->uedge_dist(lue, epnts[i].spnt);
		ON_Line lline(*omesh->fmesh->pnts[lue.v[0]], *omesh->fmesh->pnts[lue.v[1]]);
		if (epdist < 0.001 * lline.Length()) {
		    //std::cout << "Close to brep face edge\n";
		    skip_epnt = true;
		    break;
		}
	    }
	    if (skip_epnt) {
		continue;
	    }
	    std::set<uedge_t> b_ue_2 = omesh->fmesh->b_uedges(tri2);
	    for (ue_it = b_ue_2.begin(); ue_it != b_ue_2.end(); ue_it++) {
		uedge_t lue = *ue_it;
		double epdist = omesh->fmesh->uedge_dist(lue, epnts[i].spnt);
		ON_Line lline(*omesh->fmesh->pnts[lue.v[0]], *omesh->fmesh->pnts[lue.v[1]]);
		if (epdist < 0.001 * lline.Length()) {
		    //std::cout << "Close to brep face edge\n";
		    skip_epnt = true;
		    break;
		}
	    }
	    if (skip_epnt) {
		continue;
	    }

	    bool inside = false;
	    {
	        // Use the overt point for this - the closest surface point will
	        // always pass, so it's not usable for rejection.
		ON_3dPoint ovpnt = epnts[i].ov->vpnt();
		double u, v;
		polygon->tplane.ClosestPointTo(ovpnt, &u, &v);
		std::pair<double, double> proj_2d;
		proj_2d.first = u;
		proj_2d.second = v;
		polygon->pnts_2d.push_back(proj_2d);
		inside = polygon->point_in_polygon(polygon->pnts_2d.size() - 1, false);
		polygon->pnts_2d.pop_back();
	    }
	    if (inside) {
		ON_3dPoint p = epnts[i].spnt;
		double u, v;
		polygon->tplane.ClosestPointTo(p, &u, &v);
		std::pair<double, double> proj_2d;
		proj_2d.first = u;
		proj_2d.second = v;
		polygon->pnts_2d.push_back(proj_2d);
		size_t p2dind = polygon->pnts_2d.size() - 1;

		// We're going to use it - add new 3D point to CDT
		long f3ind = omesh->fmesh->add_point(new ON_3dPoint(epnts[i].spnt));
		long fnind = omesh->fmesh->add_normal(new ON_3dPoint(epnts[i].sn));
		CDT_Add3DPnt(s_cdt, omesh->fmesh->pnts[f3ind], omesh->fmesh->f_id, -1, -1, -1, 0, 0);
		CDT_Add3DNorm(s_cdt, omesh->fmesh->normals[fnind], omesh->fmesh->pnts[f3ind], omesh->fmesh->f_id, -1, -1, -1, 0, 0);
		omesh->fmesh->nmap[f3ind] = fnind;
		overt_t *nvrt = omesh->vert_add(f3ind, &sbb);
		overts_to_update.insert(nvrt);
		polygon->p2o[p2dind] = f3ind;
		polygon->interior_points.insert(p2dind);

		have_inside = true;
	    }

	}

	if (!have_inside) {
	    // No point in doing this if none of the points are interior
	    delete polygon;
	    continue;
	}

	polygon->cdt();
	//omesh->fmesh->boundary_edges_plot("bedges.plot3");

	if (polygon->tris.size()) {
	    unsigned char rgb[3] = {0,255,255};
	    struct bu_color c = BU_COLOR_INIT_ZERO;
	    bu_color_from_rgb_chars(&c, (const unsigned char *)rgb);
	    omesh->fmesh->tri_remove(tri1);
	    omesh->fmesh->tri_remove(tri2);
	    std::set<triangle_t>::iterator v_it;
	    for (v_it = polygon->tris.begin(); v_it != polygon->tris.end(); v_it++) {
		triangle_t vt = *v_it;
		orient_tri(*omesh->fmesh, vt);
		omesh->fmesh->tri_add(vt);
		// In addition to the genuinely new vertices, altered triangles
		// may need updated vert bboxes
		for (int i = 0; i < 3; i++) {
		    overts_to_update.insert(omesh->overts[vt.v[i]]);
		}
	    }

	    new_tris += polygon->tris.size();

	    //omesh->fmesh->tris_set_plot(polygon->tris, "poly_tris.plot3");
	    //polygon->polygon_plot_in_plane("poly.plot3");
	} else {
	    std::cout << "polygon cdt failed!\n";
	    bu_file_delete("tri_replace_pair_fail.plot3");
	    polygon->polygon_plot_in_plane("tri_replace_pair_fail.plot3");
	}

	if (!omesh->fmesh->valid(1)) {
	    std::cout << "CDT broke mesh!\n";
	    polygon->polygon_plot_in_plane("poly.plot3");
	    bu_exit(1, "Fatal cdt error\n");
	}
	delete polygon;

	// Have new triangles, update overts
	std::set<overt_t *>::iterator n_it;
	for (n_it = overts_to_update.begin(); n_it != overts_to_update.end(); n_it++) {
	    (*n_it)->update();
	}

	// Reassign points to their new closest edge (may be the same edge, but we need
	// to check)
	std::set<uedge_t>::iterator pre_it;
	for (pre_it = pnt_reassignment_edges.begin(); pre_it != pnt_reassignment_edges.end(); pre_it++) {
	    uedge_t rue = *pre_it;
	    std::vector<revt_pt_t> old_epnts = wedge_sets[rue];
	    std::map<uedge_t, std::vector<revt_pt_t>>::iterator er_it = wedge_sets.find(rue);
	    wedge_sets.erase(er_it);
	    for (size_t i = 0; i < old_epnts.size(); i++) {
		overt_t *ov = old_epnts[i].ov;
		std::set<uedge_t> close_edges = omesh->interior_uedges_search(ov->bb);
		double mindist = DBL_MAX;
		uedge_t closest_uedge;
		std::set<uedge_t>::iterator c_it;
		for (c_it = close_edges.begin(); c_it != close_edges.end(); c_it++) {
		    uedge_t cue = *c_it;
		    double dline = omesh->fmesh->uedge_dist(cue, old_epnts[i].spnt);
		    if (mindist > dline) {
			closest_uedge = cue;
			mindist = dline;
		    }
		}

		wedge_sets[closest_uedge].push_back(old_epnts[i]);
	    }
	}
    }

    return new_tris;
}


static overt_t *
get_largest_overt(std::set<overt_t *> &verts)
{
    double elen = 0;
    overt_t *l = NULL;
    std::set<overt_t *>::iterator v_it;
    for (v_it = verts.begin(); v_it != verts.end(); v_it++) {
	overt_t *v = *v_it;
	if (v->min_len() > elen) {
	    elen = v->min_len();
	    l = v;
	}
    }
    return l;
}

static overt_t *
closest_overt(std::set<overt_t *> &verts, overt_t *v)
{
    overt_t *closest = NULL;
    double dist = DBL_MAX;
    ON_3dPoint p1 = *v->omesh->fmesh->pnts[v->p_id];
    std::set<overt_t *>::iterator v_it;
    for (v_it = verts.begin(); v_it != verts.end(); v_it++) {
	overt_t *c = *v_it;
	ON_3dPoint p2 = *c->omesh->fmesh->pnts[c->p_id];
	double d = p1.DistanceTo(p2);
	if (dist > d) {
	    closest = c;
	    dist = d;
	}
    }
    return closest;
}

// TODO - right now, we've locked brep face edge points.  This
// isn't ideal, as we could in principle move those points locally
// along the edge curve to align with other points.  This way we
// end up with small triangles to handle fixed edge points.  However,
// adjusting the brep edge points will involved local knowledge
// on the part of each edge vertex about the distances to the next
// edge vertices on either side, as well as updating the corresponding
// vert values throughout the data structures.
//
// Return how many vertices were actually moved a significant distance
// per their bounding box dimensions
static int
adjust_overt_pair(overt_t *v1, overt_t *v2)
{
#if 0
    v1->omesh->near_verts.erase(v2);
    v2->omesh->near_verts.erase(v1);
#endif

    // If we've got two edge vertices, no dice
    if (v1->edge_vert() && v2->edge_vert()) return 0;

    ON_3dPoint p1 = v1->vpnt();
    ON_3dPoint p2 = v2->vpnt();
    double pdist = p1.DistanceTo(p2);
    ON_3dPoint s1_p, s2_p;
    ON_3dVector s1_n, s2_n;

    if (v1->edge_vert()) {
	// It's all up to v2 - if we're too far away, skip it
	if (pdist > v2->min_len()*0.5) {
	    return 0;
	}
	return v2->adjust(p1, pdist);
    }

    if (v2->edge_vert()) {
	// It's all up to v1 - if we're too far away, skip it
	if (pdist > v1->min_len()*0.5) {
	    return 0;
	}
	return v1->adjust(p2, pdist);
    }

    ON_Line l(p1,p2);
    // Weight the t parameter on the line so we are closer to the vertex
    // with the shorter edge length (i.e. less freedom to move without
    // introducing locally severe mesh distortions.)
    double t = 1 - (v2->min_len() / (v1->min_len() + v2->min_len()));
    ON_3dPoint p_wavg = l.PointAt(t);
    if ((p1.DistanceTo(p_wavg) > v1->min_len()*0.5) || (p2.DistanceTo(p_wavg) > v2->min_len()*0.5)) {
	std::cout << "WARNING: large point shift compared to triangle edge length.\n";
    }

    int adj_cnt = 0;
    adj_cnt += v1->adjust(p_wavg, pdist);
    adj_cnt += v2->adjust(p_wavg, pdist);

    return adj_cnt;
}

static size_t
adjust_close_verts(std::set<std::pair<cdt_mesh_t *, cdt_mesh_t *>> &check_pairs)
{
    std::map<overt_t *, std::set<overt_t*>> vert_ovlps;
    std::set<std::pair<cdt_mesh_t *, cdt_mesh_t *>>::iterator cp_it;
    for (cp_it = check_pairs.begin(); cp_it != check_pairs.end(); cp_it++) {
	omesh_t *omesh1 = cp_it->first->omesh;
	omesh_t *omesh2 = cp_it->second->omesh;
	struct ON_Brep_CDT_State *s_cdt1 = (struct ON_Brep_CDT_State *)omesh1->fmesh->p_cdt;
	struct ON_Brep_CDT_State *s_cdt2 = (struct ON_Brep_CDT_State *)omesh2->fmesh->p_cdt;
	if (s_cdt1 == s_cdt2) continue;
#if 0
	struct bu_vls fname = BU_VLS_INIT_ZERO;
	bu_vls_sprintf(&fname, "%s-%d-vtree.plot3", s_cdt1->name, omesh1->fmesh->f_id);
	omesh1->plot_vtree(bu_vls_cstr(&fname));
	bu_vls_sprintf(&fname, "%s-%d-vtree.plot3", s_cdt2->name, omesh2->fmesh->f_id);
	omesh2->plot_vtree(bu_vls_cstr(&fname));
	bu_vls_free(&fname);
	std::cout << "omesh1 vtree cnt: " << omesh1->vtree.Count() << "\n";
	std::cout << "omesh2 vtree cnt: " << omesh2->vtree.Count() << "\n";
#endif
	std::set<std::pair<long, long>> vert_pairs = omesh1->vert_ovlps(omesh2);
	//std::cout << "(" << s_cdt1->name << "," << omesh1->fmesh->f_id << ")+(" << s_cdt2->name << "," << omesh2->fmesh->f_id << "): " << vert_pairs.size() << " vert box overlaps\n";
	std::set<std::pair<long, long>>::iterator v_it;
	for (v_it = vert_pairs.begin(); v_it != vert_pairs.end(); v_it++) {
	    long v_first = (long)v_it->first;
	    long v_second = (long)v_it->second;
	    overt_t *v1 = omesh1->overts[v_first];
	    overt_t *v2 = omesh2->overts[v_second];
	    vert_ovlps[v1].insert(v2);
	    vert_ovlps[v2].insert(v1);
	}
    }
    std::cout << "Found " << vert_ovlps.size() << " vertices with box overlaps\n";


    std::queue<std::pair<overt_t *, overt_t *>> vq;
    std::set<overt_t *> vq_multi;
    std::map<overt_t *, std::set<overt_t*>>::iterator vo_it;
    for (vo_it = vert_ovlps.begin(); vo_it != vert_ovlps.end(); vo_it++) {
	overt_t *v = vo_it->first;
	if (vo_it->second.size() > 1) {
	    vq_multi.insert(v);
	    continue;
	}
	overt_t *v_other = *vo_it->second.begin();
	if (vert_ovlps[v_other].size() > 1) {
	    // The other point has multiple overlapping points
	    vq_multi.insert(v);
	    continue;
	}
	// Both v and it's companion only have one overlapping point
	vq.push(std::make_pair(v,v_other));
	v->aligned[v_other->omesh] = v_other;
	v_other->aligned[v->omesh] = v;
    }

    std::cout << "Have " << vq.size() << " simple interactions\n";
    std::cout << "Have " << vq_multi.size() << " complex interactions\n";
    std::set<overt_t *> adjusted;

    int adjusted_overts = 0;
    while (!vq.empty()) {
	std::pair<overt_t *, overt_t *> vpair = vq.front();
	vq.pop();

	if (adjusted.find(vpair.first) == adjusted.end() && adjusted.find(vpair.second) == adjusted.end()) {
	    adjusted_overts += adjust_overt_pair(vpair.first, vpair.second);
	    adjusted.insert(vpair.first);
	    adjusted.insert(vpair.second);
	}
    }

    // If the box structure is more complicated, we need to be a bit selective
    while (vq_multi.size()) {
	overt_t *l = get_largest_overt(vq_multi);
	if (!l) {
	    vq_multi.erase(vq_multi.begin());
	    continue;
	}
	overt_t *c = closest_overt(vert_ovlps[l], l);
	vq_multi.erase(l);
	vq_multi.erase(c);
	vert_ovlps[l].erase(c);
	vert_ovlps[c].erase(l);
	//std::cout << "COMPLEX - adjusting 1 pair only:\n";
	if (adjusted.find(l) == adjusted.end() && adjusted.find(c) == adjusted.end()) {
	    adjusted_overts += adjust_overt_pair(l, c);
	    adjusted.insert(l);
	    adjusted.insert(c);
	    l->aligned[c->omesh] = c;
	    c->aligned[l->omesh] = l;
	}
    }

    return adjusted_overts;
}


static void
replace_edge_split_tri(cdt_mesh_t &fmesh, size_t t_id, long np_id, uedge_t &split_edge)
{
    triangle_t &t = fmesh.tris_vect[t_id];
    std::set<triangle_t> ntris = t.split(split_edge, np_id, false);

    fmesh.tri_remove(t);
    std::set<triangle_t>::iterator t_it;
    for (t_it = ntris.begin(); t_it != ntris.end(); t_it++) {
	triangle_t nt = *t_it;
	fmesh.tri_add(nt);
    }
}

int
ovlp_split_edge(
	overt_t **nv1,
	overt_t **nv2,
	std::set<bedge_seg_t *> *nsegs,
	bedge_seg_t *eseg, double t
	)
{
    int replaced_tris = 0;

    std::vector<std::pair<cdt_mesh_t *,uedge_t>> uedges = eseg->uedges();

    cdt_mesh_t *fmesh_f1 = uedges[0].first;
    cdt_mesh_t *fmesh_f2 = uedges[1].first;

    int f_id1 = fmesh_f1->f_id;
    int f_id2 = fmesh_f2->f_id;

    uedge_t ue1 = uedges[0].second;
    uedge_t ue2 = uedges[1].second;

    std::set<size_t> f1_tris = fmesh_f1->uedges2tris[ue1];
    std::set<size_t> f2_tris = fmesh_f2->uedges2tris[ue2];

    size_t tris_cnt = 0;
    if (fmesh_f1->f_id == fmesh_f2->f_id) {
	std::set<size_t> f_alltris;
	f_alltris.insert(f1_tris.begin(), f1_tris.end());
	f_alltris.insert(f2_tris.begin(), f2_tris.end());
	tris_cnt = f_alltris.size();
    } else {
	tris_cnt = f1_tris.size() + f2_tris.size();
    }
    if (tris_cnt != 2) {
	if (f1_tris.size() != 1 || ((fmesh_f1->f_id == fmesh_f2->f_id) && (f1_tris.size() != 2))) {
	    std::cerr << "FATAL: could not find expected triangle in mesh " << fmesh_f1->name << "," << f_id1 << " using uedge " << ue1.v[0] << "," << ue1.v[1] << "\n";
	    ON_3dPoint v1 = *fmesh_f1->pnts[ue1.v[0]];
	    ON_3dPoint v2 = *fmesh_f1->pnts[ue1.v[1]];
	    std::cerr << "ue1.v[0]: " << v1.x << "," << v1.y << "," << v1.z << "\n";
	    std::cerr << "ue1.v[1]: " << v2.x << "," << v2.y << "," << v2.z << "\n";
	    CDT_Audit((struct ON_Brep_CDT_State *)eseg->p_cdt);
	    CDT_Audit((struct ON_Brep_CDT_State *)fmesh_f1->p_cdt);
	    exit(1);
	}
	if (f2_tris.size() != 1 || ((fmesh_f1->f_id == fmesh_f2->f_id) && (f2_tris.size() != 2))) {
	    std::cerr << "FATAL: could not find expected triangle in mesh " << fmesh_f2->name << "," << f_id2 << " using uedge " << ue2.v[0] << "," << ue2.v[1] << "\n";
	    ON_3dPoint v1 = *fmesh_f2->pnts[ue2.v[0]];
	    ON_3dPoint v2 = *fmesh_f2->pnts[ue2.v[1]];
	    std::cerr << "ue2.v[0]: " << v1.x << "," << v1.y << "," << v1.z << "\n";
	    std::cerr << "ue2.v[1]: " << v2.x << "," << v2.y << "," << v2.z << "\n";
	    CDT_Audit((struct ON_Brep_CDT_State *)eseg->p_cdt);
	    CDT_Audit((struct ON_Brep_CDT_State *)fmesh_f2->p_cdt);
	    exit(1);
	}
	ON_3dPoint ue1_p1 = *fmesh_f1->pnts[ue1.v[0]];
	ON_3dPoint ue1_p2 = *fmesh_f1->pnts[ue1.v[1]];
	std::cout << fmesh_f1->name << "," << f_id1 << " ue1: " << ue1.v[0] << "," << ue1.v[1] << ": " << ue1_p1.x << "," << ue1_p1.y << "," << ue1_p1.z << " -> " << ue1_p2.x << "," << ue1_p2.y << "," << ue1_p2.z << "\n";
	ON_3dPoint ue2_p1 = *fmesh_f2->pnts[ue2.v[0]];
	ON_3dPoint ue2_p2 = *fmesh_f2->pnts[ue2.v[1]];
	std::cout << fmesh_f2->name << "," << f_id2 << " ue2: " << ue2.v[0] << "," << ue2.v[1] << ": " << ue2_p1.x << "," << ue2_p1.y << "," << ue2_p1.z << " -> " << ue2_p2.x << "," << ue2_p2.y << "," << ue2_p2.z << "\n";
	return -1;
    }

    // We're replacing the edge and its triangles - clear old data
    // from the containers
    fmesh_f1->brep_edges.erase(ue1);
    fmesh_f2->brep_edges.erase(ue2);
    fmesh_f1->ue2b_map.erase(ue1);
    fmesh_f2->ue2b_map.erase(ue2);

    int eind = eseg->edge_ind;
    struct ON_Brep_CDT_State *s_cdt_edge = (struct ON_Brep_CDT_State *)eseg->p_cdt;
    std::set<bedge_seg_t *> epsegs = s_cdt_edge->e2polysegs[eind];
    epsegs.erase(eseg);
    std::set<bedge_seg_t *> esegs_split = split_edge_seg(s_cdt_edge, eseg, 1, &t, 1);
    if (!esegs_split.size()) {
	std::cerr << "SPLIT FAILED!\n";
	return -1;
    }

    if (nsegs) {
	(*nsegs).insert(esegs_split.begin(), esegs_split.end());
    }

    epsegs.insert(esegs_split.begin(), esegs_split.end());
    s_cdt_edge->e2polysegs[eind].clear();
    s_cdt_edge->e2polysegs[eind] = epsegs;
    std::set<bedge_seg_t *>::iterator es_it;
    for (es_it = esegs_split.begin(); es_it != esegs_split.end(); es_it++) {
	bedge_seg_t *es = *es_it;

	std::vector<std::pair<cdt_mesh_t *,uedge_t>> nuedges = es->uedges();
	cdt_mesh_t *f1 = nuedges[0].first;
	cdt_mesh_t *f2 = nuedges[1].first;
	uedge_t ue_1 = nuedges[0].second;
	uedge_t ue_2 = nuedges[1].second;

	f1->brep_edges.insert(ue_1);
	f2->brep_edges.insert(ue_2);
	f1->ue2b_map[ue_1] = es;
	f2->ue2b_map[ue_2] = es;
    }

    // Need to return one of the inserted verts from this process - we need to
    // check if that point is going to require any splitting on any other
    // objects' faces.  Any triangle from another object "close" to the new
    // point should have a vertex that can be adjusted, or if not one needs to
    // be introduced.
    long np_id;
    if (f_id1 == f_id2) {
	std::set<size_t> ftris;
	std::set<size_t>::iterator tr_it;
	uedge_t ue;
	ue = (f1_tris.size()) ? ue1 : ue2;
	ftris.insert(f1_tris.begin(), f1_tris.end());
	ftris.insert(f2_tris.begin(), f2_tris.end());
	np_id = fmesh_f1->pnts.size() - 1;
	fmesh_f1->ep.insert(np_id);
	for (tr_it = ftris.begin(); tr_it != ftris.end(); tr_it++) {
	    replace_edge_split_tri(*fmesh_f1, *tr_it, np_id, ue);
	    replaced_tris++;
	}
	omesh_t *om = fmesh_f1->omesh;
	overt_t *nvert = om->vert_add(np_id);
	(*nv1) = nvert;
	(*nv2) = nvert;
    } else {
	np_id = fmesh_f1->pnts.size() - 1;
	fmesh_f1->ep.insert(np_id);
	replace_edge_split_tri(*fmesh_f1, *f1_tris.begin(), np_id, ue1);
	replaced_tris++;
	omesh_t *om1 = fmesh_f1->omesh;
	overt_t *nvert1 = om1->vert_add(np_id);
	(*nv1) = nvert1;

	np_id = fmesh_f2->pnts.size() - 1;
	fmesh_f2->ep.insert(np_id);
	replace_edge_split_tri(*fmesh_f2, *f2_tris.begin(), np_id, ue2);
	replaced_tris++;
    	omesh_t *om2 = fmesh_f2->omesh;
	overt_t *nvert2 = om2->vert_add(np_id);
	(*nv2) = nvert2;
    }

    return replaced_tris;
}

// Find the point on the edge nearest to the point, and split the edge at that point.
//
// TODO - probably need to get more aggressive about splitting near verts as the
// iteration count increases - if there's no other way to resolve the mesh, that's
// what we need to do, small triangles or not...
static int
bedge_split_at_t(
	overt_t **nv,
	bedge_seg_t *eseg, double t,
	std::set<bedge_seg_t *> *nsegs
	)
{
    int replaced_tris = 0;
    ON_NurbsCurve *nc = eseg->nc;
    ON_3dPoint cep = nc->PointAt(t);
    double epdist1 = eseg->e_start->DistanceTo(cep);
    double epdist2 = eseg->e_end->DistanceTo(cep);
    double lseg_check = 0.1 * eseg->e_start->DistanceTo(*eseg->e_end);
    if (epdist1 > lseg_check && epdist2 > lseg_check) {
	// If the point is not close to a start/end point on the edge then split the edge.
	overt_t *nv2;
	int rtris = ovlp_split_edge(nv, &nv2, nsegs, eseg, t);
	if (rtris >= 0) {
	    replaced_tris += rtris;
	} else {
	    std::cout << "split failed\n";
	}
    }
//    else {
//	std::cout << "split point too close to vertices - skip\n";
//    }
    return replaced_tris;
}

int
bedge_split_near_verts(
	std::set<overt_t *> *nverts,
	std::map<bedge_seg_t *, std::set<overt_t *>> &edge_verts
	)
{
    int replaced_tris = 0;
    std::map<bedge_seg_t *, std::set<overt_t *>>::iterator ev_it;
    int evcnt = 0;
    for (ev_it = edge_verts.begin(); ev_it != edge_verts.end(); ev_it++) {
	//std::cout << "ev (" << evcnt+1 << " of " << edge_verts.size() <<")\n";
	std::set<overt_t *> verts = ev_it->second;
	std::set<bedge_seg_t *> segs;
	segs.insert(ev_it->first);

	while (verts.size()) {
	    overt_t *v = *verts.begin();
	    ON_3dPoint p = v->vpnt();
	    verts.erase(v);

#if 0
	    if (PPCHECK(p)) {
		std::cout << v->omesh->sname() << ":\n";
		std::cout << "center " << p.x << " " << p.y << " " << p.z << "\n";
	    }
#endif

	    bedge_seg_t *eseg_split = NULL;
	    double split_t = -1.0;
	    double closest_dist = DBL_MAX;
	    std::set<bedge_seg_t *>::iterator e_it;
	    //std::cout << "segs size: " << segs.size() << "\n";
	    for (e_it = segs.begin(); e_it != segs.end(); e_it++) {
		bedge_seg_t *eseg = *e_it;
		ON_NurbsCurve *nc = eseg->nc;
		ON_Interval domain(eseg->edge_start, eseg->edge_end);
		double t;
		ON_NurbsCurve_GetClosestPoint(&t, nc, p, 0.0, &domain);
		ON_3dPoint cep = nc->PointAt(t);
		double ecdist = cep.DistanceTo(p);
		if (closest_dist > ecdist) {
		    //std::cout << "closest_dist: " << closest_dist << "\n";
		    //std::cout << "ecdist: " << ecdist << "\n";
		    closest_dist = ecdist;
		    eseg_split = eseg;
		    split_t = t;
		}
	    }

	    std::set<bedge_seg_t *> nsegs;
	    overt_t *nv = NULL;
	    int ntri_cnt = bedge_split_at_t(&nv, eseg_split, split_t, &nsegs);
	    if (ntri_cnt) {
		segs.erase(eseg_split);
		replaced_tris += ntri_cnt;
		segs.insert(nsegs.begin(), nsegs.end());
		nverts->insert(nv);
	    }
	}
	evcnt++;
    }

    return replaced_tris;
}


// TODO - need to add a check for triangles with all three vertices on the
// same brep face edge - c.s face 4 appears to have some triangles appearing
// of that sort, which are messing with the ovlp resolution...
bool
check_faces_validity(std::set<std::pair<cdt_mesh_t *, cdt_mesh_t *>> &check_pairs)
{
    int verbosity = 0;
    std::set<cdt_mesh_t *> fmeshes;
    std::set<struct ON_Brep_CDT_State *> cdt_states;
    std::set<std::pair<cdt_mesh_t *, cdt_mesh_t *>>::iterator cp_it;
    for (cp_it = check_pairs.begin(); cp_it != check_pairs.end(); cp_it++) {
	cdt_mesh_t *fmesh1 = cp_it->first;
	cdt_mesh_t *fmesh2 = cp_it->second;
	fmeshes.insert(fmesh1);
	fmeshes.insert(fmesh2);
	struct ON_Brep_CDT_State *s_cdt1 = (struct ON_Brep_CDT_State *)fmesh1->p_cdt;
	struct ON_Brep_CDT_State *s_cdt2 = (struct ON_Brep_CDT_State *)fmesh2->p_cdt;
	cdt_states.insert(s_cdt1);
	cdt_states.insert(s_cdt2);
    }
    if (verbosity > 0) {
	std::cout << "Full face validity check results:\n";
    }
    bool valid = true;

    std::set<struct ON_Brep_CDT_State *>::iterator s_it;
    for (s_it = cdt_states.begin(); s_it != cdt_states.end(); s_it++) {
	struct ON_Brep_CDT_State *s_cdt = *s_it;
	if (!CDT_Audit(s_cdt)) {
	    std::cerr << "Invalid: " << s_cdt->name << " edge data\n";
	    valid = false;
	    bu_exit(1, "urk\n");
	}
    }

    std::set<cdt_mesh_t *>::iterator f_it;
    for (f_it = fmeshes.begin(); f_it != fmeshes.end(); f_it++) {
	cdt_mesh_t *fmesh = *f_it;
	if (!fmesh->valid(verbosity)) {
	    valid = false;
#if 1
	    struct ON_Brep_CDT_State *s_cdt = (struct ON_Brep_CDT_State *)fmesh->p_cdt;
	    std::string fpname = std::string(s_cdt->name) + std::string("_face_") + std::to_string(fmesh->f_id) + std::string(".plot3");
	    fmesh->tris_plot(fpname.c_str());
	    std::cerr << "Invalid: " << s_cdt->name << " face " << fmesh->f_id << "\n";
#endif
	}
    }
    if (!valid) {
#if 1
	for (f_it = fmeshes.begin(); f_it != fmeshes.end(); f_it++) {
	    cdt_mesh_t *fmesh = *f_it;
	    fmesh->valid(2);
	}
#endif
	std::cout << "fatal mesh damage\n";
	//bu_exit(1, "fatal mesh damage");
    }

    return valid;
}

int
omesh_interior_edge_verts(std::set<std::pair<cdt_mesh_t *, cdt_mesh_t *>> &check_pairs)
{
    std::set<omesh_t *> omeshes = refinement_omeshes(check_pairs);
    //std::cout << "Need to split triangles in " << omeshes.size() << " meshes\n";

    int rcnt = 0;
    std::set<omesh_t *>::iterator o_it;
    for (o_it = omeshes.begin(); o_it != omeshes.end(); o_it++) {
	omesh_t *omesh = *o_it;
	rcnt += interior_edge_verts(omesh);
    }

    return rcnt;
}



static void
make_omeshes(std::set<std::pair<cdt_mesh_t *, cdt_mesh_t *>> *check_pairs,
        std::map<bedge_seg_t *, std::set<overt_t *>> *edge_verts)
{
    // Make omesh containers for all the cdt_meshes in play
    std::set<std::pair<cdt_mesh_t *, cdt_mesh_t *>>::iterator p_it;
    std::set<cdt_mesh_t *> afmeshes;
    std::vector<omesh_t *> omeshes;
    for (p_it = check_pairs->begin(); p_it != check_pairs->end(); p_it++) {
	afmeshes.insert(p_it->first);
	afmeshes.insert(p_it->second);
    }
    std::set<cdt_mesh_t *>::iterator af_it;
    for (af_it = afmeshes.begin(); af_it != afmeshes.end(); af_it++) {
	cdt_mesh_t *fmesh = *af_it;
	omeshes.push_back(new omesh_t(fmesh));
	if (fmesh->omesh) {
	    delete fmesh->omesh;
	}
	fmesh->omesh = omeshes[omeshes.size() - 1];
	fmesh->omesh->check_pairs = check_pairs;
	fmesh->omesh->edge_verts = edge_verts;
    }
}

int
ON_Brep_CDT_Ovlp_Resolve(struct ON_Brep_CDT_State **s_a, int s_cnt, double lthresh)
{
    if (!s_a) return -1;
    if (s_cnt < 1) return 0;

    std::map<bedge_seg_t *, std::set<overt_t *>> edge_verts;

    // Get the bounding boxes of all faces of all breps in s_a, and find
    // possible interactions
    std::set<std::pair<cdt_mesh_t *, cdt_mesh_t *>> check_pairs;
    check_pairs = possibly_interfering_face_pairs(s_a, s_cnt);

    //std::cout << "Found " << check_pairs.size() << " potentially interfering face pairs\n";
    if (!check_pairs.size()) return 0;

    // Sanity check - are we valid?
    if (!check_faces_validity(check_pairs)) {
	bu_log("ON_Brep_CDT_Ovlp_Resolve:  invalid inputs - not attempting overlap processing!\n");
	return -1;
    }

    make_omeshes(&check_pairs, &edge_verts);

    int face_ov_cnt = omesh_ovlps(check_pairs, 0);
    if (!face_ov_cnt) {
	return 0;
    }

    // Report our starting overlap count
    bu_log("Initial overlap cnt: %d\n", face_ov_cnt);

    plot_active_omeshes(check_pairs);

    // If we're going to process, initialize refinement points
    omesh_refinement_pnts(check_pairs, 0);


    int bedge_replaced_tris = INT_MAX;

    while (bedge_replaced_tris) {
	int avcnt = 0;
	// The simplest operation is to find vertices close to each other
	// with enough freedom of movement (per triangle edge length) that
	// we can shift the two close neighbors to surface points that are
	// both the respective closest points to a center point between the
	// two originals.
	avcnt = adjust_close_verts(check_pairs);
	if (avcnt) {
	    // If we adjusted, recalculate overlapping tris and refinement points
	    //std::cout << "Adjusted " << avcnt << " vertices\n";
	    face_ov_cnt = omesh_ovlps(check_pairs, 0);
	    omesh_refinement_pnts(check_pairs, 0);
	    check_faces_validity(check_pairs);
	}

	// Process edge_verts
	size_t evcnt = 0;
	while (evcnt < edge_verts.size()) {
	    std::set<overt_t *> nverts;
	    evcnt = edge_verts.size();
	    bedge_replaced_tris = bedge_split_near_verts(&nverts, edge_verts);
	    face_ov_cnt = omesh_ovlps(check_pairs, 0);
	    omesh_refinement_pnts(check_pairs, 0);
	    check_faces_validity(check_pairs);
	}
    }

    // Once edge splits are handled, use remaining closest points and find
    // nearest interior edge curve, building sets of points near each interior
    // edge.  Then, for all interior edges, yank the two triangles associated
    // with that edge, build a polygon with interior points and tessellate.  We
    // need this to introduce no changes to the meshes as a condition of
    // termination...
    int interior_replaced_tris = INT_MAX;
    while (interior_replaced_tris) {
	interior_replaced_tris = omesh_interior_edge_verts(check_pairs);
	if (interior_replaced_tris) {
	    omesh_smooth(check_pairs);
	}
	face_ov_cnt = omesh_ovlps(check_pairs, 0);
	omesh_refinement_pnts(check_pairs, 0);
	check_faces_validity(check_pairs);
    }

    // For handling any overlaps that have persisted to this point, we need to
    // be more aggressive about getting nearby verts in place
    face_ov_cnt = omesh_ovlps(check_pairs, 1);
    if (face_ov_cnt) {
	omesh_refinement_pnts(check_pairs, INT_MAX);
	// Process edge_verts
	size_t evcnt = 0;
	while (evcnt < edge_verts.size()) {
	    std::set<overt_t *> nverts;
	    evcnt = edge_verts.size();
	    bedge_replaced_tris = bedge_split_near_verts(&nverts, edge_verts);
	    face_ov_cnt = omesh_ovlps(check_pairs, 1);
	    omesh_refinement_pnts(check_pairs, INT_MAX);
	    check_faces_validity(check_pairs);
	}
	interior_replaced_tris = INT_MAX;
	while (interior_replaced_tris) {
	    interior_replaced_tris = omesh_interior_edge_verts(check_pairs);
	    if (interior_replaced_tris) {
		omesh_smooth(check_pairs);
	    }
	    face_ov_cnt = omesh_ovlps(check_pairs, 1);
	    omesh_refinement_pnts(check_pairs, INT_MAX);
	    check_faces_validity(check_pairs);
	}
    }

    // We should now have all the points we need - any remaining work involves
    // selecting the correct triangles.
    resolve_ovlp_grps(check_pairs, lthresh);

    // Refine areas of the mesh with overlapping triangles that have aligned
    // vertices but have different triangulations of those vertices.
    //shared_cdts(check_pairs);

    // Make sure everything is still OK and do final overlap check
    bool final_valid = check_faces_validity(check_pairs);
    face_ov_cnt = omesh_ovlps(check_pairs, 1);
    bu_log("Post-processing overlap cnt: %d\n", face_ov_cnt);
    plot_active_omeshes(check_pairs);

    return (face_ov_cnt > 0 || !final_valid) ? -1 : 0;
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

