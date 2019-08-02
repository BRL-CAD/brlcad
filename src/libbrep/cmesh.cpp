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

namespace cmesh
{


// TODO - check if we are self-intersecting any boundary loops by adding in this
// triangle
bool
cmesh_t::add(triangle_t &tri, int check)
{

    // Skip degenerate triangles, but report true since this was never
    // a valid triangle in the first place
    if (tri.v[0] == tri.v[1] || tri.v[1] == tri.v[2] || tri.v[2] == tri.v[0]) {
	return true;
    }

    // Skip duplicate triangles, but report true as this triangle is in the mesh
    // already
    if (this->tris.find(tri) != this->tris.end()) {
	return true;
    }

    if (check) {
	return false;
    }

    // Add the triangle
    this->tris.insert(tri);

    // Populate maps
    long i = tri.v[0];
    long j = tri.v[1];
    long k = tri.v[2];
    this->v2tris[i].insert(tri);
    this->v2tris[j].insert(tri);
    this->v2tris[k].insert(tri);
    struct edge_t e1(i, j);
    struct edge_t e2(j, k);
    struct edge_t e3(k, i);
    this->edges2tris[e1] = tri;
    this->edges2tris[e2] = tri;
    this->edges2tris[e3] = tri;
    this->uedges2tris[uedge_t(e1)].insert(tri);
    this->uedges2tris[uedge_t(e2)].insert(tri);
    this->uedges2tris[uedge_t(e3)].insert(tri);

    return true;
}


void cmesh_t::remove(triangle_t &tri)
{
    // Update edge maps
    long i = tri.v[0];
    long j = tri.v[1];
    long k = tri.v[2];
    struct edge_t e1(i, j);
    struct edge_t e2(j, k);
    struct edge_t e3(k, i);

    this->v2edges[i].erase(e1);
    this->v2edges[j].erase(e2);
    this->v2edges[k].erase(e3);

    this->v2tris[i].erase(tri);
    this->v2tris[j].erase(tri);
    this->v2tris[k].erase(tri);

    this->edges2tris.erase(e1);
    this->edges2tris.erase(e2);
    this->edges2tris.erase(e3);

    this->uedges2tris[uedge_t(e1)].erase(tri);
    this->uedges2tris[uedge_t(e2)].erase(tri);
    this->uedges2tris[uedge_t(e3)].erase(tri);

    // Remove the triangle
    this->tris.erase(tri);
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
cmesh_t::boundary_edges()
{
    std::set<uedge_t> result;
    std::map<uedge_t, std::set<triangle_t>>::iterator ue_it;
    for (ue_it = this->uedges2tris.begin(); ue_it != this->uedges2tris.end(); ue_it++) {
	if ((*ue_it).second.size() == 1) {
	    result.insert((*ue_it).first);
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

std::vector<std::vector<long>>
cmesh_t::boundary_loops()
{
    std::set<edge_t>::iterator e_it;
    std::vector<std::vector<long>> results;

    std::set<uedge_t> bedges = this->boundary_edges();

    if (!bedges.size()) {
	std::cerr << "No boundary edges in mesh??\n";
	return results;
    }

    std::set<uedge_t> unadded(bedges.begin(), bedges.end());
    std::vector<long> *wl = new std::vector<long>;

    uedge_t fedge = *(unadded.begin());
    unadded.erase(fedge);
    edge_t dedge = this->find_boundary_oriented_edge(fedge);

    long first_v = dedge.v[0];
    long prev_v = dedge.v[0];
    long curr_v = dedge.v[1];

    wl->push_back(first_v);
    wl->push_back(curr_v);

    while (unadded.size()) {
	std::set<edge_t> candidates;
	std::set<edge_t> vedges = this->v2edges[curr_v];
	uedge_t cue(prev_v, curr_v);
	for (e_it = vedges.begin(); e_it != vedges.end(); e_it++) {
	    uedge_t vue(*e_it);
	    if (cue == vue) continue;
	    if (bedges.find(vue) == bedges.end()) continue;
	    candidates.insert(*e_it);
	}
	if (candidates.size() != 1) {
	    std::cerr << "No viable candidate boundary edge??\n";
	    results.clear();
	    return results;
	}
	edge_t nedge = *(candidates.begin());
	prev_v = dedge.v[0];
	curr_v = dedge.v[1];
	wl->push_back(curr_v);
	uedge_t nuedge(nedge);
	unadded.erase(nuedge);
	if (curr_v == first_v) {
	    results.push_back(*wl);
	    delete wl;
	    if (unadded.size()) {
		wl = new std::vector<long>;

		fedge = *(unadded.begin());
		unadded.erase(fedge);
		dedge = this->find_boundary_oriented_edge(fedge);

		first_v = dedge.v[0];
		prev_v = dedge.v[0];
		curr_v = dedge.v[1];

		wl->push_back(first_v);
		wl->push_back(curr_v);
	    }
	}
    }
    if (curr_v != first_v) {
	std::cerr << "Unable to close loop!\n";
	results.clear();
	return results;
    }

    // TODO If we have more than one loop, need to determine which is the outer
    // loop.  A difficult problem in general, but CDT projections we should be able
    // to use the bbox of the 3D points to find the largest box
    if (results.size() > 1) {
    }

    return results;
}

std::set<long>
cmesh_t::interior_points()
{
    std::set<long> results;
    std::set<long> bedge_pnts;
    std::set<uedge_t> bedges = this->boundary_edges();
    std::set<uedge_t>::iterator ue_it;
    for (ue_it = bedges.begin(); ue_it != bedges.end(); ue_it++) {
	bedge_pnts.insert((*ue_it).v[0]);
	bedge_pnts.insert((*ue_it).v[1]);
    }
    std::set<triangle_t>::iterator tr_it;
    for (tr_it = this->tris.begin(); tr_it != this->tris.end(); tr_it++) {
	for (int j = 0; j < 3; j++) {
	    long vind = (*tr_it).v[j];
	    if (bedge_pnts.find(vind) == bedge_pnts.end()) {
		results.insert(vind);
	    }
	}
    }

    return results;
}

void
cmesh_t::set_brep_data(bool brev, std::set<ON_3dPoint *> *s, std::map<ON_3dPoint *, ON_3dPoint *> *n)
{
    this->m_bRev = brev;
    this->singularities = s;
    this->normalmap = n;
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

ON_3dVector
cmesh_t::bnorm(const triangle_t &t)
{
    ON_3dPoint avgnorm(0,0,0);

    // Can't calculate this without some key Brep data
    if (!this->normalmap || !this->singularities) return avgnorm;

    double norm_cnt = 0.0;

    for (size_t i = 0; i < 3; i++) {
	ON_3dPoint *p3d = this->pnts[t.v[i]];
	if (singularities->find(p3d) != singularities->end()) {
	    // singular vert norms are a product of multiple faces - not useful for this
	    continue;
	}
	ON_3dPoint onrm(*(*normalmap)[p3d]);
	// TODO - do I need to flip this?  Previous code was accidentally not flipping...  maybe tnorm is backwards too?
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

void cmesh_t::reset()
{
    this->pnts_2d.clear();
    this->p2d2ind.clear();
    this->pnts.clear();
    this->p2ind.clear();
    this->tris.clear();
    this->v2edges.clear();
    this->v2tris.clear();
    this->edges2tris.clear();
    this->uedges2tris.clear();
    this->type = 0;
    this->m_bRev = false;
    this->singularities = NULL;
    this->normalmap = NULL;
}

void cmesh_t::build_3d(std::set<p2t::Triangle *> *cdttri, std::map<p2t::Point *, ON_3dPoint *> *pointmap)
{
    if (!cdttri || !pointmap) return;

    this->reset();

    std::set<p2t::Triangle*>::iterator s_it;

    // 3D mesh
    this->type = 0;

    // Populate points
    std::set<ON_3dPoint *> uniq_p3d;
    for (s_it = cdttri->begin(); s_it != cdttri->end(); s_it++) {
	p2t::Triangle *t = *s_it;
	for (size_t j = 0; j < 3; j++) {
	    uniq_p3d.insert((*pointmap)[t->GetPoint(j)]);
	}
    }
    std::set<ON_3dPoint *>::iterator u_it;
    for (u_it = uniq_p3d.begin(); u_it != uniq_p3d.end(); u_it++) {
	this->pnts.push_back(*u_it);
	this->p2ind[*u_it] = this->pnts.size() - 1;
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
	cmesh_t::add(nt, 0);
    }

}


void cmesh_t::boundary_edges_plot(const char *filename)
{
    FILE* plot_file = fopen(filename, "w");
    struct bu_color c = BU_COLOR_INIT_ZERO;
    bu_color_rand(&c, BU_COLOR_RANDOM_LIGHTENED);
    pl_color_buc(plot_file, &c);

    std::set<uedge_t> bedges = this->boundary_edges();
    std::set<uedge_t>::iterator b_it;
    for (b_it = bedges.begin(); b_it != bedges.end(); b_it++) {
	uedge_t ue = *b_it;
	if (this->type == 0) {
	    // 3D
	    ON_3dPoint *p1 = this->pnts[ue.v[0]];
	    ON_3dPoint *p2 = this->pnts[ue.v[1]];
	    point_t bnp1, bnp2;
	    VSET(bnp1, p1->x, p1->y, p1->z);
	    VSET(bnp2, p2->x, p2->y, p2->z);
	    pdv_3move(plot_file, bnp1);
	    pdv_3cont(plot_file, bnp2);
	}
	if (this->type == 1) {
	    // 2D
	    ON_2dPoint *p1 = this->pnts_2d[ue.v[0]];
	    ON_2dPoint *p2 = this->pnts_2d[ue.v[1]];
	    point_t bnp1, bnp2;
	    VSET(bnp1, p1->x, p1->y, 0);
	    VSET(bnp2, p2->x, p2->y, 0);
	    pdv_3move(plot_file, bnp1);
	    pdv_3cont(plot_file, bnp2);
	}
    }

    fclose(plot_file);
}


void cmesh_t::boundary_loops_plot(const char *filename)
{
    FILE* plot_file = fopen(filename, "w");

    std::vector<std::vector<long>> loops = this->boundary_loops();
    point_t bnp;
    for (size_t i = 0; i < loops.size(); i++) {
	struct bu_color c = BU_COLOR_INIT_ZERO;
	bu_color_rand(&c, BU_COLOR_RANDOM_LIGHTENED);
	pl_color_buc(plot_file, &c);

	if (this->type == 0) {
	    ON_3dPoint *p = this->pnts[loops[i][0]];
	    VSET(bnp, p->x, p->y, p->z);
	    pdv_3move(plot_file, bnp);
	}
	if (this->type == 1) {
	    ON_2dPoint *p = this->pnts_2d[loops[i][0]];
	    VSET(bnp, p->x, p->y, 0);
	    pdv_3move(plot_file, bnp);
	}
	for (size_t j = 1; j < loops[i].size(); j++) {
	    if (this->type == 0) {
		// 3D
		ON_3dPoint *p = this->pnts[loops[i][j]];
		VSET(bnp, p->x, p->y, p->z);
		pdv_3cont(plot_file, bnp);
	    }
	    if (this->type == 1) {
		// 2D
		ON_2dPoint *p = this->pnts_2d[loops[i][j]];
		VSET(bnp, p->x, p->y, 0);
		pdv_3cont(plot_file, bnp);
	    }
	}
    }

    fclose(plot_file);
}

void cmesh_t::plot_tri(const triangle_t &t, struct bu_color *buc, FILE *plot, int r, int g, int b)
{
    point_t p[3];
    point_t porig;
    point_t c = VINIT_ZERO;
    if (this->type == 0) {
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
    }

    if (this->type == 1) {
    	for (int i = 0; i < 3; i++) {
	    ON_2dPoint *p2d = this->pnts_2d[t.v[i]];
	    VSET(p[i], p2d->x, p2d->y, 0);
	    c[X] += p2d->x;
	    c[Y] += p2d->y;
	}
	c[X] = c[X]/3.0;
	c[Y] = c[Y]/3.0;
	c[Z] = 0;
    }

    for (size_t i = 0; i < 3; i++) {
	if (i == 0) {
	    VMOVE(porig, p[i]);
	    pdv_3move(plot, p[i]);
	}
	pdv_3cont(plot, p[i]);
    }
    pdv_3cont(plot, porig);

    /* fill in the "interior" using the rgb color*/
    pl_color(plot, r, g, b);
    for (size_t i = 0; i < 3; i++) {
	pdv_3move(plot, p[i]);
	pdv_3cont(plot, c);
    }


    /* Plot the triangle normal */
    pl_color(plot, 0, 255, 255);
    ON_3dVector tn = this->tnorm(t);
    vect_t tnt;
    VSET(tnt, tn.x, tn.y, tn.z);
    point_t npnt;
    VADD2(npnt, tnt, c);
    pdv_3move(plot, c);
    pdv_3cont(plot, npnt);

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
    if (this->type == 0) {
	ON_3dPoint *p = this->pnts[vind];
	vect_t pt;
	VSET(pt, p->x, p->y, p->z);
	pdv_3point(plot_file, pt);
    }
    if (this->type == 1) {
   	ON_2dPoint *p = this->pnts_2d[vind];
	pd_point(plot_file, p->x, p->y);
    }
    fclose(plot_file);
}

void cmesh_t::tris_plot(const char *filename)
{
    FILE* plot_file = fopen(filename, "w");

    struct bu_color c = BU_COLOR_INIT_ZERO;
    bu_color_rand(&c, BU_COLOR_RANDOM_LIGHTENED);
    pl_color_buc(plot_file, &c);

    std::set<triangle_t>::iterator s_it;

    for (s_it = this->tris.begin(); s_it != this->tris.end(); s_it++) {
	triangle_t tri = (*s_it);
	this->plot_tri(tri, &c, plot_file, 255, 0, 0);
    }
    fclose(plot_file);
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
