// This evolved from the original trimesh halfedge data structure code:

// Author: Yotam Gingold <yotam (strudel) yotamgingold.com>
// License: Public Domain.  (I, Yotam Gingold, the author, release this code into the public domain.)
// GitHub: https://github.com/yig/halfedge
//
// It ended up rather different, as we are concerned with somewhat different
// mesh properties for CDT and the halfedge data structure didn't end up being
// a good fit, but as it's derived from that source the public domain license
// is maintained for the changes as well.

#include "common.h"

#include "bu/color.h"
//#include "bn/mat.h" /* bn_vec_perp */
#include "bn/plot3.h"
#include "./cmesh.h"

// needed for implementation
#include <iostream>
#include <queue>

namespace cmesh
{

bool
cmesh_t::tri_add(triangle_t &tri)
{

    // Skip degenerate triangles, but report true since this was never
    // a valid triangle in the first place
    if (tri.v[0] == tri.v[1] || tri.v[1] == tri.v[2] || tri.v[2] == tri.v[0]) {
	return true;
    }

    // Skip duplicate triangles, but report true as this triangle is in the mesh
    // already
    if (this->tris.find(tri) != this->tris.end()) {
	// Check the normal of the included duplicate and the candidate.
	// If the original is flipped and the new one isn't, swap them out - this
	// will help with subsequent processing.
	triangle_t orig = (*this->tris.find(tri));
	ON_3dVector torig_dir = tnorm(orig);
	ON_3dVector tnew_dir = tnorm(tri);
	ON_3dVector bdir = tnorm(orig);
	bool f1 = (ON_DotProduct(torig_dir, bdir) < 0);
	bool f2 = (ON_DotProduct(tnew_dir, bdir) < 0);
	if (f1 && !f2) {
	    tri_remove(orig);
	} else {
	    return true;
	}
    }


    // Add the triangle
    this->tris.insert(tri);

    std::cout << "tris cnt: " << tris.size() << "\n";

    // Populate maps
    long i = tri.v[0];
    long j = tri.v[1];
    long k = tri.v[2];
    struct edge_t e[3];
    struct uedge_t ue[3];
    e[0].set(i, j);
    e[1].set(j, k);
    e[2].set(k, i);
    for (int ind = 0; ind < 3; ind++) {
	ue[ind].set(e[ind].v[0], e[ind].v[1]);
	this->edges2tris[e[ind]] = tri;
	this->uedges2tris[uedge_t(e[ind])].insert(tri);
	this->v2edges[e[ind].v[0]].insert(e[ind]);
	this->v2tris[tri.v[i]].insert(tri);
    }


    // Now that we know the triangle count for the unordered edges: the
    // addition of a triangle may change the boundary edge set.  Update.
    for (int ind = 0; ind < 3; ind++) {
	if (this->uedges2tris[ue[ind]].size() == 1) {
	    this->current_bedges.insert(ue[ind]);
	} else {
	    this->current_bedges.erase(ue[ind]);
	}
    }

    // With the addition of the new triangle, we need to update the
    // point->edge mappings as well.
    for (int ind = 0; ind < 3; ind++) {
	// For the directed edge associated with the new triangle,
	// we may need to add
	if (this->current_bedges.find(ue[ind]) != this->current_bedges.end()) {
	    this->edge_pnt_edges[e[ind].v[0]].insert(e[ind]);
	    this->edge_pnt_edges[e[ind].v[1]].insert(e[ind]);
	}
	// For the directed edge that now (potentially) has a mate,
	// we need to remove
	struct edge_t oe(e[ind].v[1],e[ind].v[0]);
	if (this->current_bedges.find(ue[ind]) == this->current_bedges.end()) {
	    this->edge_pnt_edges[oe.v[0]].erase(oe);
	    this->edge_pnt_edges[oe.v[1]].erase(oe);
	}
    }

#if 0
    std::map<uedge_t, std::set<triangle_t>>::iterator uet_it;
    for (uet_it = uedges2tris.begin(); uet_it != uedges2tris.end(); uet_it++) {
	std::cerr << "edge " << (*uet_it).first.v[0] << "-" << (*uet_it).first.v[1] <<  " tri cnt: " << (*uet_it).second.size() << "\n";
    }
#endif
    // Update boundary edge information
    this->boundary_edges(1);

#if 0
    op_cnt++;
    struct bu_vls pname = BU_VLS_INIT_ZERO;
    bu_vls_sprintf(&pname, "post_tri_add_tris.plot3-%05d", op_cnt);
    tris_plot(bu_vls_cstr(&pname));
    bu_vls_sprintf(&pname, "post_tri_add_boundary_edge.plot3%05d", op_cnt);
    boundary_edges_plot(bu_vls_cstr(&pname));
    bu_vls_free(&pname);
#endif

    return true;
}


void cmesh_t::tri_remove(triangle_t &tri)
{
    // Update edge maps
    long i = tri.v[0];
    long j = tri.v[1];
    long k = tri.v[2];
    struct edge_t e[3];
    struct uedge_t ue[3];
    e[0].set(i, j);
    e[1].set(j, k);
    e[2].set(k, i);
    for (int ind = 0; ind < 3; ind++) {
	ue[ind].set(e[ind].v[0], e[ind].v[1]);
	this->edges2tris[e[ind]];
	this->uedges2tris[uedge_t(e[ind])].erase(tri);
	this->v2edges[e[ind].v[0]].erase(e[ind]);
	this->v2tris[tri.v[i]].erase(tri);
    }

    // Remove the triangle
    this->tris.erase(tri);

    // Now that we know the new triangle count for the unordered edges: the
    // addition of a triangle may change the boundary edge set.  Update.
    for (int ind = 0; ind < 3; ind++) {
	if (this->uedges2tris[ue[ind]].size() == 1) {
	    this->current_bedges.insert(ue[ind]);
	} else {
	    this->current_bedges.erase(ue[ind]);
	}
    }

    // With the addition of the new triangle, we need to update the
    // point->edge mappings as well.
    for (int ind = 0; ind < 3; ind++) {
	// For the directed edge associated with the new triangle,
	// we may need to add
	if (this->current_bedges.find(ue[ind]) == this->current_bedges.end()) {
	    this->edge_pnt_edges[e[ind].v[0]].erase(e[ind]);
	    this->edge_pnt_edges[e[ind].v[1]].erase(e[ind]);
	}
	// For the directed edge that now (potentially) no longer has a mate,
	// we need to dd
	struct edge_t oe(e[ind].v[1],e[ind].v[0]);
	if (this->current_bedges.find(ue[ind]) != this->current_bedges.end()) {
	    this->edge_pnt_edges[oe.v[0]].insert(oe);
	    this->edge_pnt_edges[oe.v[1]].insert(oe);
	}
    }

    // Update boundary edge information
    this->boundary_edges(1);

#if 0
    op_cnt++;
    struct bu_vls pname = BU_VLS_INIT_ZERO;
    bu_vls_sprintf(&pname, "post_tri_remove_tris.plot3-%05d", op_cnt);
    tris_plot(bu_vls_cstr(&pname));
    bu_vls_sprintf(&pname, "post_tri_remove_boundary_edge.plot3-%05d", op_cnt);
    boundary_edges_plot(bu_vls_cstr(&pname));
    bu_vls_free(&pname);
#endif
}

std::vector<triangle_t>
cmesh_t::face_neighbors(const triangle_t &f)
{
    std::vector<triangle_t> result;
    long i = f.v[0];
    long j = f.v[1];
    long k = f.v[2];
    struct uedge_t e[3];
    e[0].set(i, j);
    e[1].set(j, k);
    e[2].set(k, i);
    for (int ind = 0; ind < 3; ind++) {
	std::set<triangle_t> faces = this->uedges2tris[e[ind]];
	std::set<triangle_t>::iterator f_it;
	for (f_it = faces.begin(); f_it != faces.end(); f_it++) {
	    if (*f_it != f) {
		result.push_back(*f_it);
	    }
	}
    }

    return result;
}


std::vector<triangle_t>
cmesh_t::vertex_face_neighbors(long vind)
{
    std::vector<triangle_t> result;
    std::set<triangle_t> faces = this->v2tris[vind];
    std::set<triangle_t>::iterator f_it;
    for (f_it = faces.begin(); f_it != faces.end(); f_it++) {
	result.push_back(*f_it);
    }
    return result;
}

std::set<uedge_t>
cmesh_t::boundary_edges(int use_brep_data)
{
    this->problem_edges.clear();
    std::set<uedge_t> result;
    std::map<uedge_t, std::set<triangle_t>>::iterator ue_it;
    for (ue_it = this->uedges2tris.begin(); ue_it != this->uedges2tris.end(); ue_it++) {
	if ((*ue_it).second.size() == 1) {
	    struct uedge_t ue((*ue_it).first);
	    int skip_pnt = 0;
	    if (use_brep_data && this->edge_pnts) {
		// If we have extra information from the Brep, we can filter out
		// some "bad" edges
		for (int ind = 0; ind < 2; ind++) {
		    ON_3dPoint *p = pnts[ue.v[ind]];
		    if (edge_pnts->find(p) == edge_pnts->end()) {
			// Strange edge count on a vertex not known
			// to be a Brep edge point - not a boundary
			// edge
			skip_pnt = 1;
		    }
		}
	    }

	    if (!skip_pnt) {
		result.insert((*ue_it).first);
	    } else {
		// Track these edges, as they represent places where subsequent
		// mesh operations will require extra care
		this->problem_edges.insert((*ue_it).first);
	    }
	}
    }
    return result;
}

edge_t
cmesh_t::find_boundary_oriented_edge(uedge_t &ue)
{
    // For the unordered boundary edge, there is exactly one
    // directional edge - find it
    std::set<edge_t>::iterator e_it;
    for (int i = 0; i < 2; i++) {
	std::set<edge_t> vedges = this->v2edges[ue.v[i]];
	for (e_it = vedges.begin(); e_it != vedges.end(); e_it++) {
	    uedge_t vue(*e_it);
	    if (vue == ue) {
		edge_t de(*e_it);
		return de;
	    }
	}
    }

    // This shouldn't happen
    edge_t empty;
    return empty;
}

std::vector<triangle_t>
cmesh_t::interior_incorrect_normals(int use_brep_data)
{
    std::vector<triangle_t> results;
    std::set<long> bedge_pnts;
    std::set<uedge_t> bedges = this->boundary_edges(use_brep_data);
    std::set<uedge_t>::iterator ue_it;
    for (ue_it = bedges.begin(); ue_it != bedges.end(); ue_it++) {
	bedge_pnts.insert((*ue_it).v[0]);
	bedge_pnts.insert((*ue_it).v[1]);
    }

    std::set<triangle_t>::iterator tr_it;
    for (tr_it = this->tris.begin(); tr_it != this->tris.end(); tr_it++) {
	ON_3dVector tdir = this->tnorm(*tr_it);
	ON_3dVector bdir = this->bnorm(*tr_it);
	if (tdir.Length() > 0 && bdir.Length() > 0 && ON_DotProduct(tdir, bdir) < 0.1) {
	    int epnt_cnt = 0;
	    for (int i = 0; i < 3; i++) {
		ON_3dPoint *p = pnts[(*tr_it).v[i]];
		epnt_cnt = (edge_pnts->find(p) == edge_pnts->end()) ? epnt_cnt : epnt_cnt + 1;
	    }
	    if (epnt_cnt == 2) {
		std::cerr << "UNCULLED problem point from surface???????\n";
		for (int i = 0; i < 3; i++) {
		    ON_3dPoint *p = pnts[(*tr_it).v[i]];
		    if (edge_pnts->find(p) == edge_pnts->end()) {
			std::cout << p->x << " " << p->y << " " << p->z << "\n";
		    }
		}
		results.clear();
		return results;
	    }
	    results.push_back(*tr_it);
	}
    }

    return results;
}


std::vector<triangle_t>
cmesh_t::singularity_triangles()
{
    std::vector<triangle_t> results;
    std::set<triangle_t> uniq_tris;
    std::set<long>::iterator s_it;

    for (s_it = sv.begin(); s_it != sv.end(); s_it++) {
	std::vector<triangle_t> faces = this->vertex_face_neighbors(*s_it);
	uniq_tris.insert(faces.begin(), faces.end());
    }

    results.assign(uniq_tris.begin(), uniq_tris.end());
    return results;
}

void
cmesh_t::set_brep_data(
	bool brev,
       	std::set<ON_3dPoint *> *e,
       	std::set<ON_3dPoint *> *s,
       	std::map<ON_3dPoint *, ON_3dPoint *> *n)
{
    this->m_bRev = brev;
    this->edge_pnts = e;
    this->singularities = s;
    this->normalmap = n;
}

std::set<uedge_t>
cmesh_t::uedges(const triangle_t &t)
{
    struct uedge_t ue[3];
    ue[0].set(t.v[0], t.v[1]);
    ue[1].set(t.v[1], t.v[2]);
    ue[2].set(t.v[2], t.v[0]);

    std::set<uedge_t> uedges;
    for (int i = 0; i < 3; i++) {
	uedges.insert(ue[i]);
    }

    return uedges;
}

ON_3dVector
cmesh_t::tnorm(const triangle_t &t)
{
    ON_3dPoint *p1 = this->pnts[t.v[0]];
    ON_3dPoint *p2 = this->pnts[t.v[1]];
    ON_3dPoint *p3 = this->pnts[t.v[2]];

    ON_3dVector e1 = *p2 - *p1;
    ON_3dVector e2 = *p3 - *p1;
    ON_3dVector tdir = ON_CrossProduct(e1, e2);
    tdir.Unitize();
    return tdir;
}

ON_3dPoint
cmesh_t::tcenter(const triangle_t &t)
{
    ON_3dPoint avgpnt(0,0,0);

    for (size_t i = 0; i < 3; i++) {
	ON_3dPoint *p3d = this->pnts[t.v[i]];
	avgpnt = avgpnt + *p3d;
    }

    ON_3dPoint cpnt = avgpnt/3.0;
    return cpnt;
}

ON_3dVector
cmesh_t::bnorm(const triangle_t &t)
{
    ON_3dPoint avgnorm(0,0,0);

    // Can't calculate this without some key Brep data
    if (!this->normalmap) return avgnorm;

    double norm_cnt = 0.0;

    for (size_t i = 0; i < 3; i++) {
	ON_3dPoint *p3d = this->pnts[t.v[i]];
	if (singularities->find(p3d) != singularities->end()) {
	    // singular vert norms are a product of multiple faces - not useful for this
	    continue;
	}
	ON_3dPoint onrm(*(*normalmap)[p3d]);
	if (this->m_bRev) {
	    onrm = onrm * -1;
	}
	avgnorm = avgnorm + onrm;
	norm_cnt = norm_cnt + 1.0;
    }

    ON_3dVector anrm = avgnorm/norm_cnt;
    anrm.Unitize();
    return anrm;
}

bool
cmesh_t::brep_edge_pnt(long v)
{
    ON_3dPoint *p = pnts[v];
    if (edge_pnts->find(p) != edge_pnts->end()) {
	return true;
    }
    return false;
}

void cmesh_t::reset()
{
    this->tris.clear();
    this->v2edges.clear();
    this->v2tris.clear();
    this->edges2tris.clear();
    this->uedges2tris.clear();
}

void cmesh_t::build(std::set<p2t::Triangle *> *cdttri, std::map<p2t::Point *, ON_3dPoint *> *pointmap)
{
    if (!cdttri || !pointmap) return;

    this->reset();
    this->pnts.clear();
    this->p2ind.clear();
    this->sv.clear();

    std::set<p2t::Triangle*>::iterator s_it;

    // Populate points
    std::set<ON_3dPoint *> uniq_p3d;
    for (s_it = cdttri->begin(); s_it != cdttri->end(); s_it++) {
	p2t::Triangle *t = *s_it;
	for (size_t j = 0; j < 3; j++) {
	    uniq_p3d.insert((*pointmap)[t->GetPoint(j)]);
	}
    }
    this->sv.clear();
    std::set<ON_3dPoint *>::iterator u_it;
    for (u_it = uniq_p3d.begin(); u_it != uniq_p3d.end(); u_it++) {
	this->pnts.push_back(*u_it);
	this->p2ind[*u_it] = this->pnts.size() - 1;
	if (this->singularities->find(*u_it) != this->singularities->end()) {
	    this->sv.insert(this->p2ind[*u_it]);
	}
    }

    // From the triangles, populate the containers
    for (s_it = cdttri->begin(); s_it != cdttri->end(); s_it++) {
	p2t::Triangle *t = *s_it;
	ON_3dPoint *pt_A = (*pointmap)[t->GetPoint(0)];
	ON_3dPoint *pt_B = (*pointmap)[t->GetPoint(1)];
	ON_3dPoint *pt_C = (*pointmap)[t->GetPoint(2)];

	// Skip degenerate triangles
	if (pt_A == pt_B || pt_B == pt_C || pt_C == pt_A) {
	    continue;
	}

	long Aind = this->p2ind[pt_A];
	long Bind = this->p2ind[pt_B];
	long Cind = this->p2ind[pt_C];
	struct triangle_t nt(Aind, Bind, Cind);
	cmesh_t::tri_add(nt);
    }

}

bool
cmesh_t::tri_problem_edges(triangle_t &t)
{
    if (!problem_edges.size()) return false;

    uedge_t ue1(t.v[0], t.v[1]);
    uedge_t ue2(t.v[1], t.v[2]);
    uedge_t ue3(t.v[2], t.v[0]);

    if (problem_edges.find(ue1) != problem_edges.end()) return true;
    if (problem_edges.find(ue2) != problem_edges.end()) return true;
    if (problem_edges.find(ue3) != problem_edges.end()) return true;
    return false;
}

std::vector<triangle_t>
cmesh_t::problem_edge_tris()
{
    std::vector<triangle_t> eresults;
    if (!problem_edges.size()) return eresults;

    std::set<triangle_t> uresults;
    std::set<uedge_t>::iterator u_it;
    std::set<triangle_t>::iterator t_it;
    for (u_it = problem_edges.begin(); u_it != problem_edges.end(); u_it++) {
	std::set<triangle_t> ptris = uedges2tris[(*u_it)];
	for (t_it = uedges2tris[(*u_it)].begin(); t_it != uedges2tris[(*u_it)].end(); t_it++) {
	    uresults.insert(*t_it);
	}
    }

    std::vector<triangle_t> results(uresults.begin(), uresults.end());
    return results;
}

bool
cmesh_t::self_intersecting_mesh()
{
    std::set<triangle_t> pedge_tris;
    std::set<uedge_t>::iterator u_it;
    std::set<triangle_t>::iterator t_it;
    for (u_it = problem_edges.begin(); u_it != problem_edges.end(); u_it++) {
	std::set<triangle_t> ptris = uedges2tris[(*u_it)];
	for (t_it = uedges2tris[(*u_it)].begin(); t_it != uedges2tris[(*u_it)].end(); t_it++) {
	    pedge_tris.insert(*t_it);
	}
    }

    std::map<uedge_t, std::set<triangle_t>>::iterator et_it;
    for (et_it = uedges2tris.begin(); et_it != uedges2tris.end(); et_it++) {
	std::set<triangle_t> uetris = et_it->second;
	if (uetris.size() > 2) {
	    size_t valid_cnt = 0;
	    for (t_it = uetris.begin(); t_it != uetris.end(); t_it++) {
		triangle_t t = *t_it;
		if (pedge_tris.find(t) == pedge_tris.end()) {
		    valid_cnt++;
		}
	    }
	    if (valid_cnt > 2) {
		std::cout << "Self intersection in mesh found\n";
		struct uedge_t pue = et_it->first;
		FILE* plot_file = fopen("self_intersecting_edge.plot3", "w");
		struct bu_color c = BU_COLOR_INIT_ZERO;
		bu_color_rand(&c, BU_COLOR_RANDOM_LIGHTENED);
		pl_color_buc(plot_file, &c);
		plot_uedge(pue, plot_file);
		fclose(plot_file);
		return true;
	    }
	}
    }

    return false;
}

void
cmesh_t::repair()
{
    // If we have edges with > 2 triangles and 3 or more of those triangles are
    // not problem_edge triangles, we have what amounts to a self-intersecting
    // mesh.  I'm not sure yet what to do about it - the obvious starting point
    // is to pick one of the triangles and yank it, along with any of its edge-
    // neighbors that overlap with any of the other triangles associated with
    // the original overloaded edge, and mark all the involved vertices as
    // uncontained.  But I'm not sure what the subsequent implications are for
    // the mesh processing...

    if (this->self_intersecting_mesh()) {
	tris_plot("self_intersecting_mesh.plot3");
	exit(1);
    }


    // *Wrong* triangles: problem edge and/or flipped normal triangles.  Handle
    // those first, so the subsequent clean-up pass doesn't have to worry about
    // errors they might introduce.
    std::vector<triangle_t> f_tris = this->interior_incorrect_normals(1);
    std::vector<triangle_t> e_tris = this->problem_edge_tris();
    seed_tris.clear();
    seed_tris.insert(e_tris.begin(), e_tris.end());
    seed_tris.insert(f_tris.begin(), f_tris.end());

    std::vector<triangle_t> s_tris_orig = this->singularity_triangles();

    while (seed_tris.size()) {
	triangle_t seed = *seed_tris.begin();

	// We use the Brep normal for this, since the triangles are
	// problem triangles and their normals cannot be relied upon.
	ON_3dPoint sp = this->tcenter(seed);
	ON_3dVector sn = this->bnorm(seed);

	csweep_t sweep;
	sweep.cmesh = this;
	sweep.polygon.problem_edges = problem_edges;
	sweep.build_2d_pnts(sp, sn);

	// build an initial loop from a nearby valid triangle
	bool tcnt = sweep.build_initial_loop(seed, true);

	if (!tcnt) {
	    std::cerr << "Could not build initial valid loop\n";
	    exit(1);
	}

	// Grow until we contain the seed and its associated problem data

	if (sweep.grow_loop(170, true) < 0) {
	    std::cerr << "Couldn't handle mesh repair\n";
	    exit(1);
	}

	// Remove everything the patch claimed
	std::set<triangle_t>::iterator v_it;
	for (v_it = sweep.visited_triangles.begin(); v_it != sweep.visited_triangles.end(); v_it++) {
	    triangle_t vt = *v_it;
	    seed_tris.erase(vt);
	    tri_remove(vt);
	}

	// Add in the replacement triangles
	for (v_it = sweep.polygon.tris.begin(); v_it != sweep.polygon.tris.end(); v_it++) {
	    triangle_t vt = *v_it;
	    tri_add(vt);
	}

	tris_plot("mesh_post_patch.plot3");
    }

    // Now that the out-and-out problem triangles have been handled,
    // remesh near singularities to try and produce more reasonable
    // triangles.

    std::vector<triangle_t> s_tris = this->singularity_triangles();
    seed_tris.insert(s_tris.begin(), s_tris.end());

    while (seed_tris.size()) {
	triangle_t seed = *seed_tris.begin();

	// We use the Brep normal for this, since the triangles are
	// problem triangles and their normals cannot be relied upon.
	ON_3dPoint sp = this->tcenter(seed);
	ON_3dVector sn = this->bnorm(seed);

	csweep_t sweep;
	sweep.cmesh = this;
	sweep.polygon.problem_edges = problem_edges;
	sweep.build_2d_pnts(sp, sn);

	// build an initial loop from the seed
	bool tcnt = sweep.build_initial_loop(seed, false);

	if (!tcnt) {
	    std::cerr << "Could not build initial valid loop\n";
	    exit(1);
	}

	// Grow until we contain the seed and its associated problem data

	double deg = 40;
	long tri_cnt = sweep.grow_loop(deg, false);
	while (tri_cnt < 10 && deg < 45) {
	    if (tri_cnt < 0) {
		std::cerr << "Couldn't handle mesh rebuild\n";
		exit(1);
	    }
	    tri_cnt = sweep.grow_loop(deg, false);
	}

	// Remove everything the patch claimed
	std::set<triangle_t>::iterator v_it;
	for (v_it = sweep.visited_triangles.begin(); v_it != sweep.visited_triangles.end(); v_it++) {
	    triangle_t vt = *v_it;
	    seed_tris.erase(vt);
	    tri_remove(vt);
	}

	// Add in the replacement triangles
	for (v_it = sweep.polygon.tris.begin(); v_it != sweep.polygon.tris.end(); v_it++) {
	    triangle_t vt = *v_it;
	    tri_add(vt);
	}

	tris_plot("mesh_post_pretty.plot3");
    }

}

void cmesh_t::plot_uedge(struct uedge_t &ue, FILE* plot_file)
{
    ON_3dPoint *p1 = this->pnts[ue.v[0]];
    ON_3dPoint *p2 = this->pnts[ue.v[1]];
    point_t bnp1, bnp2;
    VSET(bnp1, p1->x, p1->y, p1->z);
    VSET(bnp2, p2->x, p2->y, p2->z);
    pdv_3move(plot_file, bnp1);
    pdv_3cont(plot_file, bnp2);
}

void cmesh_t::boundary_edges_plot(const char *filename)
{
    FILE* plot_file = fopen(filename, "w");
    struct bu_color c = BU_COLOR_INIT_ZERO;
    bu_color_rand(&c, BU_COLOR_RANDOM_LIGHTENED);
    pl_color_buc(plot_file, &c);

    std::set<uedge_t> bedges = this->boundary_edges(1);
    std::set<uedge_t>::iterator b_it;
    for (b_it = bedges.begin(); b_it != bedges.end(); b_it++) {
	uedge_t ue = *b_it;
	plot_uedge(ue, plot_file);
    }

    if (this->problem_edges.size()) {
	pl_color(plot_file, 255, 0, 0);
	for (b_it = problem_edges.begin(); b_it != problem_edges.end(); b_it++) {
	    uedge_t ue = *b_it;
	    plot_uedge(ue, plot_file);
	}
    }

    fclose(plot_file);
}

#if 0
void cmesh_t::plot_tri(const triangle_t &t, struct bu_color *buc, FILE *plot, int r, int g, int b)
#else
void cmesh_t::plot_tri(const triangle_t &t, struct bu_color *buc, FILE *plot, int UNUSED(r), int UNUSED(g), int UNUSED(b))
#endif
{
    point_t p[3];
    point_t porig;
    point_t c = VINIT_ZERO;
    for (int i = 0; i < 3; i++) {
	ON_3dPoint *p3d = this->pnts[t.v[i]];
	VSET(p[i], p3d->x, p3d->y, p3d->z);
	c[X] += p3d->x;
	c[Y] += p3d->y;
	c[Z] += p3d->z;
    }
    c[X] = c[X]/3.0;
    c[Y] = c[Y]/3.0;
    c[Z] = c[Z]/3.0;

    for (size_t i = 0; i < 3; i++) {
	if (i == 0) {
	    VMOVE(porig, p[i]);
	    pdv_3move(plot, p[i]);
	}
	pdv_3cont(plot, p[i]);
    }
    pdv_3cont(plot, porig);
#if 0
    /* fill in the "interior" using the rgb color*/
    pl_color(plot, r, g, b);
    for (size_t i = 0; i < 3; i++) {
	pdv_3move(plot, p[i]);
	pdv_3cont(plot, c);
    }
#endif

    /* Plot the triangle normal */
    pl_color(plot, 0, 255, 255);
    {
	ON_3dVector tn = this->tnorm(t);
	vect_t tnt;
	VSET(tnt, tn.x, tn.y, tn.z);
	point_t npnt;
	VADD2(npnt, tnt, c);
	pdv_3move(plot, c);
	pdv_3cont(plot, npnt);
    }

#if 0
    /* Plot the brep normal */
    pl_color(plot, 0, 100, 0);
    {
	ON_3dVector tn = this->bnorm(t);
	tn = tn * 0.5;
	vect_t tnt;
	VSET(tnt, tn.x, tn.y, tn.z);
	point_t npnt;
	VADD2(npnt, tnt, c);
	pdv_3move(plot, c);
	pdv_3cont(plot, npnt);
    }
#endif
    /* restore previous color */
    pl_color_buc(plot, buc);
}

void cmesh_t::face_neighbors_plot(const triangle_t &f, const char *filename)
{
    FILE* plot_file = fopen(filename, "w");

    struct bu_color c = BU_COLOR_INIT_ZERO;
    bu_color_rand(&c, BU_COLOR_RANDOM_LIGHTENED);
    pl_color_buc(plot_file, &c);

    // Origin triangle has red interior
    std::vector<triangle_t> faces = this->face_neighbors(f);
    this->plot_tri(f, &c, plot_file, 255, 0, 0);

    // Neighbor triangles have blue interior
    for (size_t i = 0; i < faces.size(); i++) {
	triangle_t tri = faces[i];
	this->plot_tri(tri, &c, plot_file, 0, 0, 255);
    }

    fclose(plot_file);
}

void cmesh_t::vertex_face_neighbors_plot(long vind, const char *filename)
{
    FILE* plot_file = fopen(filename, "w");

    struct bu_color c = BU_COLOR_INIT_ZERO;
    bu_color_rand(&c, BU_COLOR_RANDOM_LIGHTENED);
    pl_color_buc(plot_file, &c);

    std::vector<triangle_t> faces = this->vertex_face_neighbors(vind);

    for (size_t i = 0; i < faces.size(); i++) {
	triangle_t tri = faces[i];
	this->plot_tri(tri, &c, plot_file, 0, 0, 255);
    }

    // Plot the vind point that is the source of the triangles
    pl_color(plot_file, 0, 255,0);
    ON_3dPoint *p = this->pnts[vind];
    vect_t pt;
    VSET(pt, p->x, p->y, p->z);
    pdv_3point(plot_file, pt);

    fclose(plot_file);
}

void cmesh_t::interior_incorrect_normals_plot(const char *filename)
{
    FILE* plot_file = fopen(filename, "w");

    struct bu_color c = BU_COLOR_INIT_ZERO;
    bu_color_rand(&c, BU_COLOR_RANDOM_LIGHTENED);
    pl_color_buc(plot_file, &c);

    std::vector<triangle_t> faces = this->interior_incorrect_normals(1);
    for (size_t i = 0; i < faces.size(); i++) {
	this->plot_tri(faces[i], &c, plot_file, 0, 255, 0);
    }
    fclose(plot_file);
}

void cmesh_t::tri_plot(triangle_t &tri, const char *filename)
{
    FILE* plot_file = fopen(filename, "w");

    struct bu_color c = BU_COLOR_INIT_ZERO;
    bu_color_rand(&c, BU_COLOR_RANDOM_LIGHTENED);
    pl_color_buc(plot_file, &c);

    this->plot_tri(tri, &c, plot_file, 255, 0, 0);

    fclose(plot_file);
}

void cmesh_t::tris_set_plot(std::set<triangle_t> &tset, const char *filename)
{
    FILE* plot_file = fopen(filename, "w");

    struct bu_color c = BU_COLOR_INIT_ZERO;
    bu_color_rand(&c, BU_COLOR_RANDOM_LIGHTENED);
    pl_color_buc(plot_file, &c);

    std::set<triangle_t>::iterator s_it;

    for (s_it = tset.begin(); s_it != tset.end(); s_it++) {
	triangle_t tri = (*s_it);
	this->plot_tri(tri, &c, plot_file, 255, 0, 0);
    }
    fclose(plot_file);
}

void cmesh_t::tris_plot(const char *filename)
{
    this->tris_set_plot(this->tris, filename);
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
