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
 * overlap mesh implementation
 *
 */

#include "common.h"
#include <iostream>
#include <numeric>
#include <queue>
#include <random>
#include <string>
#include "bu/str.h"
#include "bg/tri_pt.h"
#include "./cdt.h"

void
omesh_t::plot_vtree(const char *fname)
{
    FILE *plot = fopen(fname, "w");
    RTree<long, double, 3>::Iterator tree_it;
    long v_ind;
    vtree.GetFirst(tree_it);
    while (!tree_it.IsNull()) {
	v_ind = *tree_it;
	pl_color(plot, 255, 0, 0);
	overts[v_ind]->plot(plot);
	++tree_it;
    }
    fclose(plot);
}

std::string
omesh_t::sname()
{
    struct ON_Brep_CDT_State *s_cdt = (struct ON_Brep_CDT_State *)fmesh->p_cdt;
    return std::string(s_cdt->name);
}

bool
omesh_t::validate_vtree()
{
    RTree<long, double, 3>::Iterator tree_it;
    long v_ind;
    vtree.GetFirst(tree_it);
    while (!tree_it.IsNull()) {
	v_ind = *tree_it;
	overt_t *ov = overts[v_ind];
	ON_3dPoint vp = ov->vpnt();
	std::set<overt_t *> search_vert_bb = vert_search(ov->bb);
	if (!search_vert_bb.size()) {
	    std::cout << "Error: no nearby verts for vert bb " << v_ind << "??\n";
	    return false;
	}
	if (search_vert_bb.find(ov) == search_vert_bb.end()) {
	    std::cout << "Error: vert in tree, but bb search couldn't find: " << v_ind << "\n";
	    std::set<overt_t *> s2 = vert_search(ov->bb);
	    if (s2.find(ov) == s2.end()) {
		std::cout << "Second try didn't work: " << v_ind << "\n";
	    }
	    return false;
	}

	ON_BoundingBox pbb(vp, vp);
	std::set<overt_t *> search_vert = vert_search(pbb);
	if (!search_vert.size()) {
	    std::cout << "Error: vert point not contained by tree box?? " << v_ind << "??\n";
	    std::set<overt_t *> sv2 = vert_search(pbb);
	    if (sv2.find(ov) == sv2.end()) {
		std::cout << "Second try didn't work: " << v_ind << "\n";
	    }
	    return false;
	}

	++tree_it;
    }
    return true;
}

std::set<std::pair<long, long>>
omesh_t::vert_ovlps(omesh_t *other)
{
    std::set<std::pair<long, long>> vert_pairs;
    vert_pairs.clear();
    vtree.Overlaps(other->vtree, &vert_pairs);
    return vert_pairs;
}

void
omesh_t::plot(const char *fname)
{
    struct bu_color c = BU_COLOR_INIT_ZERO;
    unsigned char rgb[3] = {0,0,255};

    // Randomize the blue so (usually) subsequent
    // draws of different meshes won't erase the
    // previous wireframe
    std::random_device rd;
    std::mt19937 reng(rd());
    std::uniform_int_distribution<> rrange(100,250);
    int brand = rrange(reng);
    rgb[2] = (unsigned char)brand;

    FILE *plot = fopen(fname, "w");

    RTree<size_t, double, 3>::Iterator tree_it;
    size_t t_ind;
    triangle_t tri;

    bu_color_from_rgb_chars(&c, (const unsigned char *)rgb);

    pl_color(plot, 0, 0, 255);
    fmesh->tris_tree.GetFirst(tree_it);
    while (!tree_it.IsNull()) {
	t_ind = *tree_it;
	tri = fmesh->tris_vect[t_ind];
	fmesh->plot_tri(tri, &c, plot, 255, 0, 0);
	++tree_it;
    }

    double tri_r = 0;

    bu_color_rand(&c, BU_COLOR_RANDOM_LIGHTENED);
    pl_color_buc(plot, &c);
    std::set<size_t>::iterator ts_it;
    for (ts_it = intruding_tris.begin(); ts_it != intruding_tris.end(); ts_it++) {
	tri = fmesh->tris_vect[*ts_it];
	double tr = fmesh->tri_pnt_r(tri.ind);
	tri_r = (tr > tri_r) ? tr : tri_r;
	fmesh->plot_tri(tri, &c, plot, 255, 0, 0);
    }



    std::set<overt_t *>::iterator iv_it;
    std::map<uedge_t, std::vector<revt_pt_t>>::iterator es_it;
    for (es_it = edge_sets.begin(); es_it != edge_sets.end(); es_it++) {
	for (size_t i = 0; i < es_it->second.size(); i++) {
	    overt_t *iv = es_it->second[i].ov;
	    ON_3dPoint vp = iv->vpnt();
	    pl_color(plot, 255, 0, 0);
	    plot_pnt_3d(plot, &vp, tri_r, 0);
	    pl_color(plot, 0, 255, 0);
	    ON_3dPoint sp = es_it->second[i].spnt;
	    plot_pnt_3d(plot, &sp, tri_r, 0);
	}
    }

    std::map<bedge_seg_t *, std::set<overt_t *>>::iterator ev_it;
    pl_color(plot, 128, 0, 255);
    for (ev_it = edge_verts->begin(); ev_it != edge_verts->end(); ev_it++) {
	bedge_seg_t *eseg = ev_it->first;
	struct ON_Brep_CDT_State *s_cdt_edge = (struct ON_Brep_CDT_State *)eseg->p_cdt;
	int f_id1 = s_cdt_edge->brep->m_T[eseg->tseg1->trim_ind].Face()->m_face_index;
	int f_id2 = s_cdt_edge->brep->m_T[eseg->tseg2->trim_ind].Face()->m_face_index;
	cdt_mesh_t &fmesh_f1 = s_cdt_edge->fmeshes[f_id1];
	cdt_mesh_t &fmesh_f2 = s_cdt_edge->fmeshes[f_id2];
	omesh_t *o1 = fmesh_f1.omesh;
	omesh_t *o2 = fmesh_f2.omesh;
	if (o1 == this || o2 == this) {
	    std::set<overt_t *>::iterator v_it;
	    for (v_it = ev_it->second.begin(); v_it != ev_it->second.end(); v_it++) {
		overt_t *iv = *v_it;
		if (iv->omesh == this) continue;
		ON_3dPoint vp = iv->vpnt();
		plot_pnt_3d(plot, &vp, tri_r, 0);
	    }
	}
    }

    pl_color(plot, 0, 0, 255);


    fclose(plot);
}

void
omesh_t::plot()
{
    struct bu_vls fname = BU_VLS_INIT_ZERO;
    struct ON_Brep_CDT_State *s_cdt = (struct ON_Brep_CDT_State *)fmesh->p_cdt;
    bu_vls_sprintf(&fname, "%s-%d_ovlps.plot3", s_cdt->name, fmesh->f_id);
    plot(bu_vls_cstr(&fname));
    bu_vls_free(&fname);
}

overt_t *
omesh_t::vert_closest(double *vdist, overt_t *v)
{
    ON_BoundingBox fbbox = v->omesh->fmesh->bbox();
    ON_3dPoint opnt = v->bb.Center();

    // Find close verts, iterate through them and
    // find the closest
    ON_BoundingBox s_bb = v->bb;
    ON_3dPoint p = v->vpnt();
    std::set<overt_t *> nverts = vert_search(s_bb);
    bool last_try = false;
    while (!nverts.size() && !last_try) {
	ON_3dVector vmin = s_bb.Min() - s_bb.Center();
	ON_3dVector vmax = s_bb.Max() - s_bb.Center();
	vmin.Unitize();
	vmax.Unitize();
	vmin = vmin * s_bb.Diagonal().Length() * 2;
	vmax = vmax * s_bb.Diagonal().Length() * 2;
	s_bb.m_min = opnt + vmin;
	s_bb.m_max = opnt + vmax;
	if (s_bb.Includes(fbbox, true)) {
	    last_try = true;
	}
	nverts = vert_search(s_bb);
    }

    double dist = DBL_MAX;
    overt_t *nv = NULL;
    std::set<overt_t *>::iterator v_it;
    for (v_it = nverts.begin(); v_it != nverts.end(); v_it++) {
	overt_t *ov = *v_it;
	ON_3dPoint np = ov->vpnt();
	if (np.DistanceTo(p) < dist) {
	    nv = ov;
	    dist = np.DistanceTo(p);
	}
    }
    if (vdist) {
	(*vdist) = dist;
    }
    return nv;
}

overt_t *
omesh_t::vert_closest(double *vdist, ON_3dPoint &opnt)
{
    ON_BoundingBox fbbox = fmesh->bbox();

    ON_3dPoint ptol(ON_ZERO_TOLERANCE, ON_ZERO_TOLERANCE, ON_ZERO_TOLERANCE);
    ON_BoundingBox s_bb(opnt, opnt);
    ON_3dPoint npnt = opnt + ptol;
    s_bb.Set(npnt, true);
    npnt = opnt - ptol;
    s_bb.Set(npnt, true);

    std::set<overt_t *> nverts = vert_search(s_bb);
    bool last_try = false;
    while (!nverts.size() && !last_try) {
	ON_3dVector vmin = s_bb.Min() - s_bb.Center();
	ON_3dVector vmax = s_bb.Max() - s_bb.Center();
	vmin.Unitize();
	vmax.Unitize();
	vmin = vmin * s_bb.Diagonal().Length() * 2;
	vmax = vmax * s_bb.Diagonal().Length() * 2;
	s_bb.m_min = opnt + vmin;
	s_bb.m_max = opnt + vmax;
	if (s_bb.Includes(fbbox, true)) {
	    last_try = true;
	}
	nverts = vert_search(s_bb);
    }

    double dist = DBL_MAX;
    overt_t *nv = NULL;
    std::set<overt_t *>::iterator v_it;
    for (v_it = nverts.begin(); v_it != nverts.end(); v_it++) {
	overt_t *ov = *v_it;
	ON_3dPoint np = ov->vpnt();
	if (np.DistanceTo(opnt) < dist) {
	    nv = ov;
	    dist = np.DistanceTo(opnt);
	}
    }
    if (vdist) {
	(*vdist) = dist;
    }
    return nv;
}

double
omesh_t::closest_pt(ON_3dPoint &cp, ON_3dVector &cn, const ON_3dPoint &op)
{
    ON_BoundingBox fbbox = fmesh->bbox();
    if (!fmesh->tris_vect.size()) {
	return DBL_MAX;
    }

    double lguess = fbbox.Diagonal().Length()/(double)fmesh->tris_vect.size();
    lguess = (lguess < ON_ZERO_TOLERANCE) ? ON_ZERO_TOLERANCE : lguess;

    ON_3dPoint ptol(lguess, lguess, lguess);
    ON_BoundingBox s_bb(op, op);
    ON_3dPoint npnt = op + ptol;
    s_bb.Set(npnt, true);
    npnt = op - ptol;
    s_bb.Set(npnt, true);

    // Find close triangles, iterate through them and
    // find the closest interior edge
    std::set<size_t> ntris = tris_search(s_bb);
    bool last_try = false;
    while (!ntris.size() && !last_try) {
	ON_3dVector vmin = s_bb.Min() - s_bb.Center();
	ON_3dVector vmax = s_bb.Max() - s_bb.Center();
	vmin.Unitize();
	vmax.Unitize();
	vmin = vmin * s_bb.Diagonal().Length() * 2;
	vmax = vmax * s_bb.Diagonal().Length() * 2;
	s_bb.m_min = op + vmin;
	s_bb.m_max = op + vmax;
	if (s_bb.Includes(fbbox, true)) {
	    last_try = true;
	}
	ntris = tris_search(s_bb);
    }

    double tdist = DBL_MAX;
    point_t closest_pt = VINIT_ZERO;
    triangle_t ctri;
    std::set<size_t>::iterator tr_it;
    for (tr_it = ntris.begin(); tr_it != ntris.end(); tr_it++) {
	triangle_t t = fmesh->tris_vect[*tr_it];

	point_t T_V[3];
	VSET(T_V[0], fmesh->pnts[t.v[0]]->x, fmesh->pnts[t.v[0]]->y, fmesh->pnts[t.v[0]]->z);
	VSET(T_V[1], fmesh->pnts[t.v[1]]->x, fmesh->pnts[t.v[1]]->y, fmesh->pnts[t.v[1]]->z);
	VSET(T_V[2], fmesh->pnts[t.v[2]]->x, fmesh->pnts[t.v[2]]->y, fmesh->pnts[t.v[2]]->z);

	point_t opt;
	point_t lclosest_pt;
	VSET(opt, op.x, op.y, op.z);

	double ltdist = bg_tri_closest_pt(&lclosest_pt, opt, T_V[0], T_V[1], T_V[2]);

	if (ltdist < tdist) {
	    VMOVE(closest_pt, lclosest_pt);
	    ctri = t;
	    tdist = ltdist;
	}
    }

    ON_3dPoint on_cp(closest_pt[X], closest_pt[Y], closest_pt[Z]);
    ON_3dVector on_cn = ctri.m->tnorm(ctri);

    cp = on_cp;
    cn = on_cn;

    return tdist;
}

uedge_t
omesh_t::closest_uedge(ON_3dPoint &p)
{
    uedge_t uedge;

    if (!fmesh->pnts.size()) return uedge;

    ON_BoundingBox fbbox = fmesh->bbox();
    if (!fmesh->tris_vect.size()) {
	return uedge_t();
    }

    double lguess = fbbox.Diagonal().Length()/(double)fmesh->tris_vect.size();
    lguess = (lguess < ON_ZERO_TOLERANCE) ? ON_ZERO_TOLERANCE : lguess;

    ON_3dPoint ptol(lguess, lguess, lguess);
    ON_BoundingBox s_bb(p, p);
    ON_3dPoint npnt = p + ptol;
    s_bb.Set(npnt, true);
    npnt = p - ptol;
    s_bb.Set(npnt, true);

    // Find close triangles, iterate through them and
    // find the closest interior edge
    std::set<size_t> ntris = tris_search(s_bb);
    bool last_try = false;
    while (!ntris.size() && !last_try) {
	ON_3dVector vmin = s_bb.Min() - s_bb.Center();
	ON_3dVector vmax = s_bb.Max() - s_bb.Center();
	vmin.Unitize();
	vmax.Unitize();
	vmin = vmin * s_bb.Diagonal().Length() * 2;
	vmax = vmax * s_bb.Diagonal().Length() * 2;
	s_bb.m_min = p + vmin;
	s_bb.m_max = p + vmax;
	if (s_bb.Includes(fbbox, true)) {
	    last_try = true;
	}
	ntris = tris_search(s_bb);
    }

    double uedist = DBL_MAX;
    std::set<size_t>::iterator tr_it;
    for (tr_it = ntris.begin(); tr_it != ntris.end(); tr_it++) {
	triangle_t t = fmesh->tris_vect[*tr_it];
	ON_3dPoint tp = p;
	uedge_t ue = fmesh->closest_uedge(t, tp);
	double ue_cdist = fmesh->uedge_dist(ue, p);
	if (ue_cdist < uedist) {
	    uedge = ue;
	    uedist = ue_cdist;
	}
    }

    return uedge;
}


std::set<uedge_t>
omesh_t::interior_uedges_search(ON_BoundingBox &bb)
{
    std::set<uedge_t> uedges;
    uedges.clear();

    ON_3dPoint opnt = bb.Center();

    if (!fmesh->pnts.size()) return uedges;

    ON_BoundingBox fbbox = fmesh->bbox();

    // Find close triangles, iterate through them and
    // find the closest interior edge
    ON_BoundingBox s_bb = bb;
    std::set<size_t> ntris = tris_search(s_bb);
    bool last_try = false;
    while (!ntris.size() && !last_try) {
	ON_3dVector vmin = s_bb.Min() - s_bb.Center();
	ON_3dVector vmax = s_bb.Max() - s_bb.Center();
	vmin.Unitize();
	vmax.Unitize();
	vmin = vmin * s_bb.Diagonal().Length() * 2;
	vmax = vmax * s_bb.Diagonal().Length() * 2;
	s_bb.m_min = opnt + vmin;
	s_bb.m_max = opnt + vmax;
	if (s_bb.Includes(fbbox, true)) {
	    last_try = true;
	}
	ntris = tris_search(s_bb);
    }

    std::set<size_t>::iterator tr_it;
    for (tr_it = ntris.begin(); tr_it != ntris.end(); tr_it++) {
	triangle_t t = fmesh->tris_vect[*tr_it];
	ON_3dPoint tp = bb.Center();
	uedge_t ue = fmesh->closest_interior_uedge(t, tp);
	uedges.insert(ue);
    }

    return uedges;
}

std::set<size_t>
omesh_t::tris_search(ON_BoundingBox &bb)
{
    return fmesh->tris_search(bb);
}

static bool NearVertsCallback(long data, void *a_context) {
    std::set<long> *nverts = (std::set<long> *)a_context;
    nverts->insert(data);
    return true;
}
std::set<overt_t *>
omesh_t::vert_search(ON_BoundingBox &bb)
{
    double fMin[3], fMax[3];
    fMin[0] = bb.Min().x - ON_ZERO_TOLERANCE;
    fMin[1] = bb.Min().y - ON_ZERO_TOLERANCE;
    fMin[2] = bb.Min().z - ON_ZERO_TOLERANCE;
    fMax[0] = bb.Max().x + ON_ZERO_TOLERANCE;
    fMax[1] = bb.Max().y + ON_ZERO_TOLERANCE;
    fMax[2] = bb.Max().z + ON_ZERO_TOLERANCE;
    std::set<long> vtree_near_verts;
    size_t nhits = vtree.Search(fMin, fMax, NearVertsCallback, (void *)&vtree_near_verts);

    if (!nhits) {
	return std::set<overt_t *>();
    }

    std::set<overt_t *> near_overts;
    std::set<long>::iterator n_it;
    for (n_it = vtree_near_verts.begin(); n_it != vtree_near_verts.end(); n_it++) {
	near_overts.insert(overts[*n_it]);
    }

    return near_overts;
}

overt_t *
omesh_t::vert_add(long f3ind, ON_BoundingBox *bb)
{
    overt_t *nv = new overt_t(this, f3ind);
    if (bb) {
	nv->update(bb);
    } else {
	nv->update();
    }
    overts[f3ind] = nv;
    return nv;
}

std::set<omesh_t *>
scdt_meshes(std::set<std::pair<cdt_mesh_t *, cdt_mesh_t *>> *check_pairs, struct ON_Brep_CDT_State *s_cdt)
{
    std::set<omesh_t *> check_meshes;
    std::set<std::pair<cdt_mesh_t *, cdt_mesh_t *>>::iterator o_it;
    for (o_it = check_pairs->begin(); o_it != check_pairs->end(); o_it++) {
	if ((struct ON_Brep_CDT_State *)o_it->first->omesh->fmesh->p_cdt == s_cdt) {
	    check_meshes.insert(o_it->first->omesh);
	}
	if ((struct ON_Brep_CDT_State *)o_it->second->omesh->fmesh->p_cdt == s_cdt) {
	    check_meshes.insert(o_it->second->omesh);
	}
    }

    return check_meshes;
}


bool
omesh_t::closest_nearby_mesh_point(ON_3dPoint &s_p, ON_3dVector &s_n, ON_3dPoint *p, struct ON_Brep_CDT_State *s_cdt)
{
    std::set<omesh_t *> check_meshes = scdt_meshes(check_pairs, s_cdt);

    ON_BoundingBox pbb(*p, *p);

    bool have_dist = false;
    double cdist = DBL_MAX;
    ON_3dPoint cp;
    ON_3dVector cn;
    std::set<omesh_t *>::iterator om_it;
    for (om_it = check_meshes.begin(); om_it != check_meshes.end(); om_it++) {
	omesh_t *om = *om_it;
	if (om->fmesh->bbox().IsDisjoint(pbb)) continue;
	ON_3dPoint om_cp;
	ON_3dVector om_cn;
	double ldist = om->closest_pt(om_cp, om_cn, *p);
	if (ldist < DBL_MAX && cdist > ldist) {
	    cdist = ldist;
	    cp = om_cp;
	    cn = om_cn;
	    have_dist = true;
	}
    }
    if (!have_dist) {
	cdist = closest_pt(cp, cn, *p);
    }

    s_p = cp;
    s_n = cn;

    return (cdist < DBL_MAX);
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

