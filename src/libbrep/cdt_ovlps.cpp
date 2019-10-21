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


/* TODO list:
 *
 * 1.  As a first step, build an rtree in 3D of the mesh vertex points, basing their bbox size on the dimensions
 * of the smallest of the triangles connected to them.  Look for any points from other meshes whose boxes overlap.
 * These points are "close" and rather than producing the set of small triangles trying to resolve them while
 * leaving the original vertices in place would entail, instead take the average of those points and for each
 * vertex replace its point xyz values with the closest point on the associated surface for that avg point.  What
 * that should do is locally adjust each mesh to not overlap near vertices.  Then we calculate the triangle rtree
 * and proceed with overlap testing.
 *
 * Note that vertices on face edges will probably require different handling...
 *
 * 2.  Given closest point calculations for intersections, associate them with either edges or faces (if not near
 * an edge.)
 *
 * 3.  Identify triangles fully contained inside another mesh, based on walking the intersecting triangles by vertex
 * nearest neighbors for all the vertices that are categorized as intruding.  Any triangle that is connected to such
 * a vertex but doesn't itself intersect the mesh is entirely interior, and we'll need to find closest points for
 * its vertices as well.
 *
 * 4.  If the points from #3 involve triangle meshes for splitting that weren't directly interfering, update the
 * necessary data sets so we know which triangles to split (probably a 2D triangle/point search of some sort, since
 * unlike the tri/tri intersection tests we can't immediately localize such points on the intruding triangle mesh
 * if they're coming from a NURBS surface based gcp...)
 *
 * 5.  Do a categorization passes as outlined below so we know what specifically we need to do with each edge/triangle.
 *
 * 6.  Set up the replacement CDT problems and update the mesh.  Identify termination criteria and check for them.
 */

// The challenge with splitting triangles is to not introduce badly distorted triangles into the mesh,
// while making sure we perform splits that work towards refining overlap areas.  In the diagrams
// below, "*" represents the edge of the triangle under consideration, "-" represents the edge of a
// surrounding triangle, and + represents a new candidate point from an intersecting triangle.
// % represents an edge on a triangle from another face.
//
// ______________________   ______________________   ______________________   ______________________
// \         **         /   \         **         /   \         **         /   \         **         /
//  \       *  *       /     \       *  *       /     \       *  *       /     \       *  *       /
//   \     *    *     /       \     *    *     /       \     *    *     /       \     *    *     /
//    \   *  +   *   /         \   *      *   /         \   *      *   /         \   *  +   *   /
//     \ *        * /           \ *        * /           \ *    +   * /           \ *        * /
//      \**********/             \*****+****/             \**********/             \******+***/
//       \        /               \        /               \        /               \        /
//        \      /                 \      /                 \      /                 \      /
//         \    /                   \    /                   \    /                   \    /
//          \  /                     \  /                     \  /                     \  /
// 1         \/             2         \/             3         \/             4         \/
// ______________________   ______________________   ______________________   ______________________
// \         **         /   \         **         /   \         **         /   \         **         /
//  \       *  *       /     \       *  *       /     \       *  *       /     \       *  *       /
//   \     *    *     /       \     +    *     /       \     +    *     /       \     +    +     /
//    \   *      *   /         \   *      *   /         \   *   +  *   /         \   *      *   /
//     \ *     +  * /           \ *        * /           \ *        * /           \ *        * /
//      \******+***/             \*****+****/             \*****+****/             \*****+****/
//       \        /               \        /               \        /               \        /
//        \      /                 \      /                 \      /                 \      /
//         \    /                   \    /                   \    /                   \    /
//          \  /                     \  /                     \  /                     \  /
// 5         \/             6         \/             7         \/             8         \/
// ______________________   ______________________   ______________________   ______________________
// \         **         /   \         **         /   \         **         /   \         **         /
//  \       *  *       /     \       *  *       /     \       *  *       /     \       *  *       /
//   \     *    *     /       \     *    *     /       \     *    *     /       \     *    *     /
//    \   *  +   *   /         \   *      *   /         \   *      *   /         \   *  +   *   /
//     \ *        * /           \ *        * /           \ *    +   * /           \ *        * /
//      \**********/             \*****+****/             \**********/             \******+***/
//       %        %               %        %               %        %               %        %
//        %      %                 %      %                 %      %                 %      %
//         %    %                   %    %                   %    %                   %    %
//          %  %                     %  %                     %  %                     %  %
// 9         %%             10        %%             11        %%             12        %%
// ______________________   ______________________   ______________________   ______________________
// \         **         /   \         **         /   \         **         /   \         **         /
//  \       *  *       /     \       *  *       /     \       *  *       /     \       *  *       /
//   \     *    *     /       \     +    *     /       \     +    *     /       \     +    +     /
//    \   *      *   /         \   *      *   /         \   *   +  *   /         \   *      *   /
//     \ *     +  * /           \ *        * /           \ *        * /           \ *        * /
//      \******+***/             \*****+****/             \*****+****/             \*****+****/
//       %        %               %        %               %        %               %        %
//        %      %                 %      %                 %      %                 %      %
//         %    %                   %    %                   %    %                   %    %
//          %  %                     %  %                     %  %                     %  %
// 13        %%             14        %%             15        %%             16        %%
// ______________________   ______________________   ______________________   ______________________
// \         **         /   \         **         /   \         **         /   \         **         /
//  \       *  *       /     \       *  *       /     \       *  *       /     \       *  *       /
//   \     *    *     /       \     *    *     /       \     *    *     /       \     *    *     /
//    \   *      *   /         \   *      *   /         \   *      *   /         \   *      *   /
//     \ *+       * /           \ *+      +* /           \ *+       * /           \ *+      +* /
//      \**********/             \**********/             \*****+****/             \******+***/
//       \        /               \        /               \        /               \        /
//        \      /                 \      /                 \      /                 \      /
//         \    /                   \    /                   \    /                   \    /
//          \  /                     \  /                     \  /                     \  /
// 17        \/             18        \/             19        \/             20        \/
//
// Initial thoughts:
//
// 1. If all new candidate points are far from the triangle edges, (cases 1 and 9) we can simply
// replace the current triangle with the CDT of its interior.
//
// 2. Any time a new point is anywhere near an edge, we risk creating a long, slim triangle.  In
// those situations, we want to remove both the active triangle and the triangle sharing that edge,
// and CDT the resulting point set to make new triangles to replace both.  (cases other than 1 and 9)
//
// 3. If a candidate triangle has multiple edges with candidate points near them, perform the
// above operation for each edge - i.e. the replacement triangles from the first CDT that share the
// original un-replaced triangle edges should be used as the basis for CDTs per step 2 with
// their neighbors.  This is true both for the "current" triangle and the triangle pulled in for
// the pair processing, if the latter is an overlapping triangle.  (cases 6-8)
//
// 4. If we can't remove the edge in that fashion (i.e. we're on the edge of the face) but have a
// candidate point close to that edge, we need to split the edge (maybe near that point if we can
// manage it... right now we've only got a midpoint split...), reject any new candidate points that
// are too close to the new edges, and re- CDT the resulting set.  Any remaining overlaps will need
// to be resolved in a subsequent pass, since the same "not-too-close-to-the-edge" sampling
// constraints we deal with in the initial surface sampling will also be needed here. (cases 10-16)
//
// 5. A point close to an existing vertex will probably need to be rejected or consolidate into the
// existing vertex, depending on how the triangles work out.  We don't want to introduce very tiny
// triangles trying to resolve "close" points - in that situation we probably want to "collapse" the
// close points into a single point with the properties we need.
//
// 5. We'll probably want some sort of filter to avoid splitting very tiny triangles interfering with
// much larger triangles - otherwise we may end up with a lot of unnecessary splits of triangles
// that would have been "cleared" anyway by the breakup of the larger triangle...
//
//
// Each triangle looks like it breaks down into regions:
/*
 *
 *                         /\
 *                        /44\
 *                       /3333\
 *                      / 3333 \
 *                     /   33   \
 *                    /    /\    \
 *                   /    /  \    \
 *                  /    /    \    \
 *                 /    /      \    \
 *                / 2  /        \ 2  \
 *               /    /          \    \
 *              /    /            \    \
 *             /    /       1      \    \
 *            /    /                \    \
 *           /    /                  \    \
 *          /333 /                    \ 333\
 *         /33333______________________33333\
 *        /43333            2           33334\
 *       --------------------------------------
 */
//
// Whether points are in any of the above defined regions after the get-closest-points pass will
// determine how the triangle is handled:
//
// Points in region 1 and none of the others - split just this triangle.
// Points in region 2 and not 3/4, remove associated edge and triangulate with pair.
// Points in region 3 and not 4, remove (one after the other) both associated edges and triangulate with pairs.
// Points in region 4 - remove candidate new point - too close to existing vertex.  "Too close" will probably
// have to be based on the relative triangle dimensions, both of the interloper and the intruded-upon triangles...
// If we have a large and a small triangle interacting, should probably just break the large one down.  If we
// hit this situation with comparably sized triangles, probably need to look at a point averaging/merge of some sort.



#include "common.h"
#include <queue>
#include <numeric>
#include "bg/chull.h"
#include "bg/tri_pt.h"
#include "bg/tri_tri.h"
#include "./cdt.h"

#define TREE_LEAF_FACE_3D(pf, valp, a, b, c, d)  \
    pdv_3move(pf, pt[a]); \
    pdv_3cont(pf, pt[b]); \
    pdv_3cont(pf, pt[c]); \
    pdv_3cont(pf, pt[d]); \
    pdv_3cont(pf, pt[a]); \

#define BBOX_PLOT(pf, bb) {                 \
    fastf_t pt[8][3];                       \
    point_t min, max;		    	    \
    min[0] = bb.Min().x;                    \
    min[1] = bb.Min().y;                    \
    min[2] = bb.Min().z;		    \
    max[0] = bb.Max().x;		    \
    max[1] = bb.Max().y;		    \
    max[2] = bb.Max().z;		    \
    VSET(pt[0], max[X], min[Y], min[Z]);    \
    VSET(pt[1], max[X], max[Y], min[Z]);    \
    VSET(pt[2], max[X], max[Y], max[Z]);    \
    VSET(pt[3], max[X], min[Y], max[Z]);    \
    VSET(pt[4], min[X], min[Y], min[Z]);    \
    VSET(pt[5], min[X], max[Y], min[Z]);    \
    VSET(pt[6], min[X], max[Y], max[Z]);    \
    VSET(pt[7], min[X], min[Y], max[Z]);    \
    TREE_LEAF_FACE_3D(pf, pt, 0, 1, 2, 3);      \
    TREE_LEAF_FACE_3D(pf, pt, 4, 0, 3, 7);      \
    TREE_LEAF_FACE_3D(pf, pt, 5, 4, 7, 6);      \
    TREE_LEAF_FACE_3D(pf, pt, 1, 5, 6, 2);      \
}

double
tri_pnt_r(cdt_mesh::cdt_mesh_t &fmesh, long tri_ind)
{
    cdt_mesh::triangle_t tri = fmesh.tris_vect[tri_ind];
    ON_3dPoint *p3d = fmesh.pnts[tri.v[0]];
    ON_BoundingBox bb(*p3d, *p3d);
    for (int i = 1; i < 3; i++) {
	p3d = fmesh.pnts[tri.v[i]];
	bb.Set(*p3d, true);
    }
    double bbd = bb.Diagonal().Length();
    return bbd * 0.01;
}

class omesh_t;
class overt_t {
    public:

	overt_t(omesh_t *om, long p)
	{
	    omesh = om;
	    p_id = p;
	    closest_uedge = -1;
	    t_ind = -1;
	    update();
	}

	omesh_t *omesh;
	long p_id;

	double min_len();
	//double max_len();
	ON_BoundingBox bb;

	long closest_uedge;
	bool t_ind;
	void update();

	ON_3dPoint vpnt();

	void plot(FILE *plot);

    private:
	double v_min_edge_len;
};

class omesh_t
{
    public:
	omesh_t(cdt_mesh::cdt_mesh_t *m)
	{
	    fmesh = m;
	    init_edges(); // Need this first - init_verts uses this information
	    init_verts();
	};

	// The fmesh pnts array may have inactive vertices - we only want the
	// active verts for this portion of the processing.
	std::map<long, class overt_t *> overts;
	RTree<long, double, 3> vtree;
	void plot_vtree(const char *fname);

	// Interior edges we add and remove. Because we don't want to store the whole
	// uedge_t class in the rtree, store them in two maps to support both
	// adding/removing them and looking them up from either triangles or
	// the tree.
	std::map<cdt_mesh::uedge_t, long> interior_uedge_ids;
	std::map<long, cdt_mesh::uedge_t> interior_uedges;
	RTree<long, double, 3> iedge_tree;

	// When we consider a vertex near an edge (i.e. we're going to yank that edge
	// and retessellate) all the other unassigned overts that have that edge as
	// their assigned closest edge need to find a new one.  Make it easy to find
	// out which vertices need to do that work.
	std::map<long, std::set<long>> iedge_close_overts;

	void vert_adjust(long p_id, ON_3dPoint *p, ON_3dVector *v);

	// Add a vertex both the fmesh and the overts array.  Note - the
	// index in the fmesh pnts array needs to match with the corresponding
	// overts index, so the same index refers to the same point in both
	// containers.  The 2D point is only there for debugging information -
	// it is not (currently) wired back into the fmesh maps.
	void vert_add(ON_3dPoint *p, ON_3dVector *v, ON_2dPoint &n2dp);

	// Find close vertices
	std::set<long> overts_search(ON_BoundingBox &bb);

	// Find close (non-face-boundary) edges
	std::set<size_t> interior_uedges_search(ON_BoundingBox &bb);

	// Find close triangles
	std::set<size_t> tris_search(ON_BoundingBox &bb);

	void retessellate(std::set<size_t> &ov);

	std::set<long> ovlping_tris;

	std::set<overt_t*> intruding_pnts;

	cdt_mesh::cdt_mesh_t *fmesh;

	void plot(const char *fname);
	void plot();

	void verts_one_ring_update(long p_id);
    private:
	void init_verts();
	void init_edges();

	void edge_add(cdt_mesh::uedge_t &ue, int update_verts);
	void edge_remove(cdt_mesh::uedge_t &ue, int update_verts);

	void edge_tris_remove(cdt_mesh::uedge_t &ue);

};

double
overt_t::min_len() {
    return v_min_edge_len;
}

ON_3dPoint
overt_t::vpnt() {
    ON_3dPoint vp = *(omesh->fmesh->pnts[p_id]);
    return vp;
}

void
overt_t::update() {
    // 1.  Get pnt's associated edges.
    std::set<cdt_mesh::edge_t> edges = omesh->fmesh->v2edges[p_id];

    // 2.  find the shortest edge associated with pnt
    std::set<cdt_mesh::edge_t>::iterator e_it;
    double elen = DBL_MAX;
    for (e_it = edges.begin(); e_it != edges.end(); e_it++) {
	ON_3dPoint *p1 = omesh->fmesh->pnts[(*e_it).v[0]];
	ON_3dPoint *p2 = omesh->fmesh->pnts[(*e_it).v[1]];
	double dist = p1->DistanceTo(*p2);
	elen = (dist < elen) ? dist : elen;
    }
    v_min_edge_len = elen;

    // create a bbox around pnt using length ~20% of the shortest edge length.
    ON_3dPoint vpnt = *omesh->fmesh->pnts[p_id];
    ON_BoundingBox init_bb(vpnt, vpnt);
    bb = init_bb;
    ON_3dPoint npnt = vpnt;
    double lfactor = 0.2;
    npnt.x = npnt.x + lfactor*elen;
    bb.Set(npnt, true);
    npnt = vpnt;
    npnt.x = npnt.x - lfactor*elen;
    bb.Set(npnt, true);
    npnt = vpnt;
    npnt.y = npnt.y + lfactor*elen;
    bb.Set(npnt, true);
    npnt = vpnt;
    npnt.y = npnt.y - lfactor*elen;
    bb.Set(npnt, true);
    npnt = vpnt;
    npnt.z = npnt.z + lfactor*elen;
    bb.Set(npnt, true);
    npnt = vpnt;
    npnt.z = npnt.z - lfactor*elen;
    bb.Set(npnt, true);

    double mindist = DBL_MAX;
    if (closest_uedge >= 0) {
	omesh->iedge_close_overts[closest_uedge].erase(p_id);
    }
    closest_uedge = -1;
    std::set<size_t> close_edges = omesh->interior_uedges_search(bb);
    std::set<size_t>::iterator c_it;
    for (c_it = close_edges.begin(); c_it != close_edges.end(); c_it++) {
	cdt_mesh::uedge_t ue = omesh->interior_uedges[*c_it];
	ON_3dPoint *p3d1 = omesh->fmesh->pnts[ue.v[0]];
	ON_3dPoint *p3d2 = omesh->fmesh->pnts[ue.v[1]];
	ON_Line line(*p3d1, *p3d2);
	double dline = vpnt.DistanceTo(line.ClosestPointTo(vpnt));
	if (mindist > dline) {
	    closest_uedge = *c_it;
	    mindist = dline;
	}
    }
    if (closest_uedge >= 0) {
	omesh->iedge_close_overts[closest_uedge].insert(p_id);
    }
}

void
overt_t::plot(FILE *plot)
{
    ON_3dPoint *i_p = omesh->fmesh->pnts[p_id];
    BBOX_PLOT(plot, bb);
    double r = 0.05*bb.Diagonal().Length();
    pl_color(plot, 0, 255, 0);
    plot_pnt_3d(plot, i_p, r, 0);
}

void
omesh_t::init_edges()
{
    // Walk the fmesh's rtree holding the active triangles to get all
    // edges
    std::set<cdt_mesh::uedge_t> uedges;
    RTree<size_t, double, 3>::Iterator tree_it;
    size_t t_ind;
    cdt_mesh::triangle_t tri;
    fmesh->tris_tree.GetFirst(tree_it);
    while (!tree_it.IsNull()) {
	t_ind = *tree_it;
	tri = fmesh->tris_vect[t_ind];
	cdt_mesh::uedge_t ue;
	ue = cdt_mesh::uedge_t(tri.v[0], tri.v[1]);
	edge_add(ue, 0);
	ue = cdt_mesh::uedge_t(tri.v[1], tri.v[2]);
	edge_add(ue, 0);
	ue = cdt_mesh::uedge_t(tri.v[2], tri.v[0]);
	edge_add(ue, 0);
	++tree_it;
    }

    std::set<cdt_mesh::uedge_t>::iterator u_it;
    for (u_it = uedges.begin(); u_it != uedges.end(); u_it++) {
	if (fmesh->brep_edges.find(*u_it) == fmesh->brep_edges.end()) {
	    std::cout << "Interior edge\n";
	} else {
	    std::cout << "Skip Boundary edge\n";
	}
    }
}

void
omesh_t::init_verts()
{
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
    	double fMin[3];
	fMin[0] = overts[*a_it]->bb.Min().x;
	fMin[1] = overts[*a_it]->bb.Min().y;
	fMin[2] = overts[*a_it]->bb.Min().z;
	double fMax[3];
	fMax[0] = overts[*a_it]->bb.Max().x;
	fMax[1] = overts[*a_it]->bb.Max().y;
	fMax[2] = overts[*a_it]->bb.Max().z;
	vtree.Insert(fMin, fMax, *a_it);
    }
}

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

void
omesh_t::plot(const char *fname)
{
    struct bu_color c = BU_COLOR_INIT_ZERO;
    unsigned char rgb[3] = {0,0,255};
    FILE *plot = fopen(fname, "w");

    RTree<size_t, double, 3>::Iterator tree_it;
    size_t t_ind;
    cdt_mesh::triangle_t tri;

    bu_color_from_rgb_chars(&c, (const unsigned char *)rgb);

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
    std::set<long>::iterator ts_it;
    for (ts_it = ovlping_tris.begin(); ts_it != ovlping_tris.end(); ts_it++) {
	tri = fmesh->tris_vect[*ts_it];
	double tr = tri_pnt_r(*fmesh, tri.ind);
	tri_r = (tr > tri_r) ? tr : tri_r;
	fmesh->plot_tri(tri, &c, plot, 255, 0, 0);
    }

    pl_color(plot, 255, 0, 0);
    std::set<overt_t*>::iterator i_it;
    for (i_it = intruding_pnts.begin(); i_it != intruding_pnts.end(); i_it++) {
	overt_t *iv = *i_it;
	ON_3dPoint vp = iv->vpnt();
	plot_pnt_3d(plot, &vp, tri_r, 0);
    }

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


static bool NearVertCallback(long data, void *a_context) {
    std::set<long> *nverts = (std::set<long> *)a_context;
    nverts->insert(data);
    return true;
}
std::set<long>
omesh_t::overts_search(ON_BoundingBox &bb)
{
    double fMin[3], fMax[3];
    fMin[0] = bb.Min().x;
    fMin[1] = bb.Min().y;
    fMin[2] = bb.Min().z;
    fMax[0] = bb.Max().x;
    fMax[1] = bb.Max().y;
    fMax[2] = bb.Max().z;
    std::set<long> near_overts;
    size_t nhits = fmesh->tris_tree.Search(fMin, fMax, NearVertCallback, (void *)&near_overts);

    if (!nhits) {
	std::cout << "No nearby vertices\n";
	return std::set<long>();
    }

    return near_overts;
}

static bool NearIntEdgesCallback(size_t data, void *a_context) {
    std::set<size_t> *edges = (std::set<size_t> *)a_context;
    edges->insert(data);
    return true;
}
std::set<size_t>
omesh_t::interior_uedges_search(ON_BoundingBox &bb)
{
    double fMin[3]; double fMax[3];
    fMin[0] = bb.Min().x;
    fMin[1] = bb.Min().y;
    fMin[2] = bb.Min().z;
    fMax[0] = bb.Max().x;
    fMax[1] = bb.Max().y;
    fMax[2] = bb.Max().z;
    std::set<size_t> nedges;
    size_t nhits = iedge_tree.Search(fMin, fMax, NearIntEdgesCallback, (void *)&nedges);

    if (!nhits) {
	// TODO - if we've got nothing, try triangles - if we're close to any of those,
	// iterate through them and find the closest edge
	std::set<size_t> ntris = tris_search(bb);
	if (!ntris.size()) {
	    std::cout << "not close to edge or triangles...\n";
	    return std::set<size_t>();
	} else {
	    long closest_tri = -1;
	    double cdist = DBL_MAX;
	    std::set<size_t>::iterator tr_it;
	    for (tr_it = ntris.begin(); tr_it != ntris.end(); tr_it++) {
		cdt_mesh::triangle_t t = fmesh->tris_vect[*tr_it];
		// Figure out how far away the triangle is from the point in question
		point_t tp, v0, v1, v2;
		VSET(tp, bb.Center().x, bb.Center().y, bb.Center().z);
		VSET(v0, fmesh->pnts[t.v[0]]->x, fmesh->pnts[t.v[0]]->y, fmesh->pnts[t.v[0]]->z);
		VSET(v1, fmesh->pnts[t.v[1]]->x, fmesh->pnts[t.v[1]]->y, fmesh->pnts[t.v[1]]->z);
		VSET(v2, fmesh->pnts[t.v[2]]->x, fmesh->pnts[t.v[2]]->y, fmesh->pnts[t.v[2]]->z);
		double tdist = bg_tri_pt_dist(tp, v0, v1, v2);
		if (cdist > tdist) {
		    cdist = tdist;
		    closest_tri = *tr_it;
		}
	    }
	    // For the closest triangle, find the closest edge from that triangle
	    double ecdist = DBL_MAX;
	    int cedge = -1;
	    cdt_mesh::uedge_t uedges[3];
	    for (int i = 0; i < 3; i++) {
		int v1 = i;
		int v2 = (i == 2) ? 0 : i + 1;
		uedges[i] = cdt_mesh::uedge_t(fmesh->tris_vect[closest_tri].v[v1], fmesh->tris_vect[closest_tri].v[v2]);
	    }
	    for (int i = 0; i < 3; i++) {
		ON_3dPoint *p3d1 = fmesh->pnts[uedges[i].v[0]];
		ON_3dPoint *p3d2 = fmesh->pnts[uedges[i].v[1]];
		ON_Line line(*p3d1, *p3d2);
		double dline = bb.Center().DistanceTo(line.ClosestPointTo(bb.Center()));
		if (ecdist > dline) {
		    cedge = i;
		    ecdist = dline;
		}
	    }
	    nedges.insert(interior_uedge_ids[uedges[cedge]]);
	}
    }

    return nedges;
}


static bool NearTrisCallback(size_t data, void *a_context) {
    std::set<size_t> *ntris = (std::set<size_t> *)a_context;
    ntris->insert(data);
    return true;
}
std::set<size_t>
omesh_t::tris_search(ON_BoundingBox &bb)
{
    double fMin[3], fMax[3];
    fMin[0] = bb.Min().x;
    fMin[1] = bb.Min().y;
    fMin[2] = bb.Min().z;
    fMax[0] = bb.Max().x;
    fMax[1] = bb.Max().y;
    fMax[2] = bb.Max().z;
    std::set<size_t> near_tris;
    size_t nhits = fmesh->tris_tree.Search(fMin, fMax, NearTrisCallback, (void *)&near_tris);

    if (!nhits) {
	std::cout << "No nearby triangles\n";
	return std::set<size_t>();
    }

    return near_tris;
}

void
omesh_t::verts_one_ring_update(long p_id)
{
    std::set<long> mod_verts;

    // 1.  Get pnt's associated edges.
    std::set<cdt_mesh::edge_t> edges = fmesh->v2edges[p_id];

    // 2.  Collect all the vertices associated with all the edges
    // connected to the original point - these are the mvert_info
    // structures that may have a new minimum edge length after
    // the change.
    std::set<cdt_mesh::edge_t>::iterator e_it;
    for (e_it = edges.begin(); e_it != edges.end(); e_it++) {
	mod_verts.insert((*e_it).v[0]);
	mod_verts.insert((*e_it).v[1]);
    }

    // 3.  Update each vertex
    std::set<long>::iterator v_it;
    for (v_it = mod_verts.begin(); v_it != mod_verts.end(); v_it++) {
	overts[*v_it]->update();
    }
}

void
omesh_t::vert_adjust(long p_id, ON_3dPoint *p, ON_3dVector *v)
{
    (*fmesh->pnts[p_id]) = *p;
    (*fmesh->normals[fmesh->nmap[p_id]]) = *v;
    verts_one_ring_update(p_id);
}


void
omesh_t::vert_add(ON_3dPoint *p, ON_3dVector *v, ON_2dPoint &n2dp)
{
    struct ON_Brep_CDT_State *s_cdt = (struct ON_Brep_CDT_State *)fmesh->p_cdt;
    long f3ind = fmesh->add_point(new ON_3dPoint(*p));
    long fnind = fmesh->add_normal(new ON_3dPoint(*v));
    CDT_Add3DPnt(s_cdt, fmesh->pnts[f3ind], fmesh->f_id, -1, -1, -1, n2dp.x, n2dp.y);
    CDT_Add3DNorm(s_cdt, fmesh->normals[fnind], fmesh->pnts[f3ind], fmesh->f_id, -1, -1, -1, n2dp.x, n2dp.y);
    fmesh->nmap[f3ind] = fnind;

    overts[f3ind] = new overt_t(this, f3ind);
    double fMin[3];
    fMin[0] = overts[f3ind]->bb.Min().x;
    fMin[1] = overts[f3ind]->bb.Min().y;
    fMin[2] = overts[f3ind]->bb.Min().z;
    double fMax[3];
    fMax[0] = overts[f3ind]->bb.Max().x;
    fMax[1] = overts[f3ind]->bb.Max().y;
    fMax[2] = overts[f3ind]->bb.Max().z;
    vtree.Insert(fMin, fMax, f3ind);

}

void
omesh_t::edge_add(cdt_mesh::uedge_t &ue, int update_verts)
{
    if (interior_uedge_ids.find(ue) != interior_uedge_ids.end()) {
	return;
    }

    size_t nind = 0;
    if (interior_uedges.size()) {
	nind = interior_uedges.rbegin()->first + 1;
    }
    interior_uedges[nind] = ue;
    interior_uedge_ids[ue] = nind;
    ON_3dPoint *p3d1 = fmesh->pnts[ue.v[0]];
    ON_3dPoint *p3d2 = fmesh->pnts[ue.v[1]];
    ON_Line l(*p3d1, *p3d2);
    ON_BoundingBox ebb = l.BoundingBox();
    double dist = 0.5*l.Length();
    double xdist = ebb.m_max.x - ebb.m_min.x;
    double ydist = ebb.m_max.y - ebb.m_min.y;
    double zdist = ebb.m_max.z - ebb.m_min.z;
    if (xdist < dist) {
	ebb.m_min.x = ebb.m_min.x - 0.5*dist;
	ebb.m_max.x = ebb.m_max.x + 0.5*dist;
    }
    if (ydist < dist) {
	ebb.m_min.y = ebb.m_min.y - 0.5*dist;
	ebb.m_max.y = ebb.m_max.y + 0.5*dist;
    }
    if (zdist < dist) {
	ebb.m_min.z = ebb.m_min.z - 0.5*dist;
	ebb.m_max.z = ebb.m_max.z + 0.5*dist;
    }
    double fMin[3];
    fMin[0] = ebb.Min().x;
    fMin[1] = ebb.Min().y;
    fMin[2] = ebb.Min().z;
    double fMax[3];
    fMax[0] = ebb.Max().x;
    fMax[1] = ebb.Max().y;
    fMax[2] = ebb.Max().z;
    iedge_tree.Insert(fMin, fMax, nind);

    if (update_verts) {
	overts[ue.v[0]]->update();
	overts[ue.v[1]]->update();
	// Anything close to the new edges needs to assess if this edge is closer
	// than the previously selected one
	std::set<long> nearby_verts = overts_search(ebb);
	std::set<long>::iterator n_it;
	for (n_it = nearby_verts.begin(); n_it != nearby_verts.end(); n_it++) {
	    overts[*n_it]->update();
	}
    }
}

void
omesh_t::edge_remove(cdt_mesh::uedge_t &ue, int update_verts)
{
    size_t ue_id = interior_uedge_ids[ue];
    ON_3dPoint *p3d1 = fmesh->pnts[ue.v[0]];
    ON_3dPoint *p3d2 = fmesh->pnts[ue.v[1]];
    ON_Line l(*p3d1, *p3d2);
    ON_BoundingBox ebb = l.BoundingBox();
    double dist = 0.5*l.Length();
    double xdist = ebb.m_max.x - ebb.m_min.x;
    double ydist = ebb.m_max.y - ebb.m_min.y;
    double zdist = ebb.m_max.z - ebb.m_min.z;
    if (xdist < dist) {
	ebb.m_min.x = ebb.m_min.x - 0.5*dist;
	ebb.m_max.x = ebb.m_max.x + 0.5*dist;
    }
    if (ydist < dist) {
	ebb.m_min.y = ebb.m_min.y - 0.5*dist;
	ebb.m_max.y = ebb.m_max.y + 0.5*dist;
    }
    if (zdist < dist) {
	ebb.m_min.z = ebb.m_min.z - 0.5*dist;
	ebb.m_max.z = ebb.m_max.z + 0.5*dist;
    }
    double fMin[3];
    fMin[0] = ebb.Min().x;
    fMin[1] = ebb.Min().y;
    fMin[2] = ebb.Min().z;
    double fMax[3];
    fMax[0] = ebb.Max().x;
    fMax[1] = ebb.Max().y;
    fMax[2] = ebb.Max().z;

    interior_uedge_ids.erase(ue);
    interior_uedges.erase(ue_id);
    iedge_tree.Remove(fMin, fMax, ue_id);

    if (update_verts) {
	overts[ue.v[0]]->update();
	overts[ue.v[1]]->update();
	// The verts who where referencing this as their closest edge need to
	// pick a new one
	std::set<long> close_verts = iedge_close_overts[ue_id];
	std::set<long>::iterator c_it;
	for (c_it = close_verts.begin(); c_it != close_verts.end(); c_it++) {
	    overts[*c_it]->update();
	}
    }
}




ON_BoundingBox
edge_bbox(cdt_mesh::bedge_seg_t *eseg)
{
    ON_3dPoint *p3d1 = eseg->e_start;
    ON_3dPoint *p3d2 = eseg->e_end;
    ON_Line line(*p3d1, *p3d2);

    ON_BoundingBox bb = line.BoundingBox();
    bb.m_max.x = bb.m_max.x + ON_ZERO_TOLERANCE;
    bb.m_max.y = bb.m_max.y + ON_ZERO_TOLERANCE;
    bb.m_max.z = bb.m_max.z + ON_ZERO_TOLERANCE;
    bb.m_min.x = bb.m_min.x - ON_ZERO_TOLERANCE;
    bb.m_min.y = bb.m_min.y - ON_ZERO_TOLERANCE;
    bb.m_min.z = bb.m_min.z - ON_ZERO_TOLERANCE;

    double dist = p3d1->DistanceTo(*p3d2);
    double bdist = 0.5*dist;
    double xdist = bb.m_max.x - bb.m_min.x;
    double ydist = bb.m_max.y - bb.m_min.y;
    double zdist = bb.m_max.z - bb.m_min.z;
    // If we're close to the edge, we want to know - the Search callback will
    // check the precise distance and make a decision on what to do.
    if (xdist < bdist) {
        bb.m_min.x = bb.m_min.x - 0.5*bdist;
        bb.m_max.x = bb.m_max.x + 0.5*bdist;
    }
    if (ydist < bdist) {
        bb.m_min.y = bb.m_min.y - 0.5*bdist;
        bb.m_max.y = bb.m_max.y + 0.5*bdist;
    }
    if (zdist < bdist) {
        bb.m_min.z = bb.m_min.z - 0.5*bdist;
        bb.m_max.z = bb.m_max.z + 0.5*bdist;
    }

    return bb;
}

void
plot_ovlp(struct brep_face_ovlp_instance *ovlp, FILE *plot)
{
    if (!ovlp) return;
    cdt_mesh::cdt_mesh_t &imesh = ovlp->intruding_pnt_s_cdt->fmeshes[ovlp->intruding_pnt_face_ind];
    cdt_mesh::cdt_mesh_t &cmesh = ovlp->intersected_tri_s_cdt->fmeshes[ovlp->intersected_tri_face_ind];
    cdt_mesh::triangle_t i_tri = imesh.tris_vect[ovlp->intruding_pnt_tri_ind];
    cdt_mesh::triangle_t c_tri = cmesh.tris_vect[ovlp->intersected_tri_ind];
    ON_3dPoint *i_p = imesh.pnts[ovlp->intruding_pnt];

    double bb1d = tri_pnt_r(imesh, i_tri.ind);
    double bb2d = tri_pnt_r(cmesh, c_tri.ind);
    double pnt_r = (bb1d < bb2d) ? bb1d : bb2d;

    pl_color(plot, 0, 0, 255);
    cmesh.plot_tri(c_tri, NULL, plot, 0, 0, 0);
    //pl_color(plot, 255, 0, 0);
    //imesh.plot_tri(i_tri, NULL, plot, 0, 0, 0);
    pl_color(plot, 255, 0, 0);
    plot_pnt_3d(plot, i_p, pnt_r, 0);
}

void
plot_tri_npnts(
	cdt_mesh::cdt_mesh_t *fmesh,
	long t_ind,
	RTree<void *, double, 3> &rtree,
	FILE *plot)
{
    cdt_mesh::triangle_t tri = fmesh->tris_vect[t_ind];
    double pnt_r = tri_pnt_r(*fmesh, t_ind);

    pl_color(plot, 0, 0, 255);
    fmesh->plot_tri(tri, NULL, plot, 0, 0, 0);

    pl_color(plot, 255, 0, 0);
    RTree<void *, double, 3>::Iterator tree_it;
    rtree.GetFirst(tree_it);
    while (!tree_it.IsNull()) {
	ON_3dPoint *n3d = (ON_3dPoint *)*tree_it;
	plot_pnt_3d(plot, n3d, pnt_r, 0);
	++tree_it;
    }
}

void
plot_ovlps(struct ON_Brep_CDT_State *s_cdt, int fi)
{
    struct bu_vls fname = BU_VLS_INIT_ZERO;
    bu_vls_sprintf(&fname, "%s_%d_ovlps.plot3", s_cdt->name, fi);
    FILE* plot_file = fopen(bu_vls_cstr(&fname), "w");
    for (size_t i = 0; i < s_cdt->face_ovlps[fi].size(); i++) {
	plot_ovlp(s_cdt->face_ovlps[fi][i], plot_file);
    }
    fclose(plot_file);
}

class tri_dist {
public:
    tri_dist(double idist, long iind) {
	dist = idist;
	ind = iind;
    }
    bool operator< (const tri_dist &b) const {
	return dist < b.dist;
    }
    double dist;
    long ind;
};



bool
closest_surf_pnt(ON_3dPoint &s_p, ON_3dVector &s_norm, cdt_mesh::cdt_mesh_t &fmesh, ON_3dPoint *p, double tol)
{
    struct ON_Brep_CDT_State *s_cdt = (struct ON_Brep_CDT_State *)fmesh.p_cdt;
    ON_2dPoint surf_p2d;
    ON_3dPoint surf_p3d = ON_3dPoint::UnsetPoint;
    s_p = ON_3dPoint::UnsetPoint;
    s_norm = ON_3dVector::UnsetVector;
    double cdist;
    if (tol <= 0) {
	surface_GetClosestPoint3dFirstOrder(s_cdt->brep->m_F[fmesh.f_id].SurfaceOf(), *p, surf_p2d, surf_p3d, cdist);
    } else {
	surface_GetClosestPoint3dFirstOrder(s_cdt->brep->m_F[fmesh.f_id].SurfaceOf(), *p, surf_p2d, surf_p3d, cdist, 0, ON_ZERO_TOLERANCE, tol);
    }
    if (NEAR_EQUAL(cdist, DBL_MAX, ON_ZERO_TOLERANCE)) return false;
    bool seval = surface_EvNormal(s_cdt->brep->m_F[fmesh.f_id].SurfaceOf(), surf_p2d.x, surf_p2d.y, s_p, s_norm);
    if (!seval) return false;

    if (fmesh.m_bRev) {
	s_norm = s_norm * -1;
    }

    return true;
}

ON_3dPoint
lseg_closest_pnt(ON_Line &l, ON_3dPoint &p)
{
    double t;
    l.ClosestPointTo(p, &t);
    if (t > 0 && t < 1) {
	return l.PointAt(t);
    } else {
	double d1 = l.from.DistanceTo(p);
	double d2 = l.to.DistanceTo(p);
	return (d2 < d1) ? l.to : l.from;
    }
}


/*****************************************************************************
 * We're only concerned with specific categories of intersections between
 * triangles, so filter accordingly.
 * Return 0 if no intersection, 1 if coplanar intersection, 2 if non-coplanar
 * intersection.
 *****************************************************************************/
static int
tri_isect(cdt_mesh::cdt_mesh_t *fmesh1, cdt_mesh::triangle_t &t1, cdt_mesh::cdt_mesh_t *fmesh2, cdt_mesh::triangle_t &t2, point_t *isectpt1, point_t *isectpt2)
{
    int coplanar = 0;
    point_t T1_V[3];
    point_t T2_V[3];
    VSET(T1_V[0], fmesh1->pnts[t1.v[0]]->x, fmesh1->pnts[t1.v[0]]->y, fmesh1->pnts[t1.v[0]]->z);
    VSET(T1_V[1], fmesh1->pnts[t1.v[1]]->x, fmesh1->pnts[t1.v[1]]->y, fmesh1->pnts[t1.v[1]]->z);
    VSET(T1_V[2], fmesh1->pnts[t1.v[2]]->x, fmesh1->pnts[t1.v[2]]->y, fmesh1->pnts[t1.v[2]]->z);
    VSET(T2_V[0], fmesh2->pnts[t2.v[0]]->x, fmesh2->pnts[t2.v[0]]->y, fmesh2->pnts[t2.v[0]]->z);
    VSET(T2_V[1], fmesh2->pnts[t2.v[1]]->x, fmesh2->pnts[t2.v[1]]->y, fmesh2->pnts[t2.v[1]]->z);
    VSET(T2_V[2], fmesh2->pnts[t2.v[2]]->x, fmesh2->pnts[t2.v[2]]->y, fmesh2->pnts[t2.v[2]]->z);
    if (bg_tri_tri_isect_with_line(T1_V[0], T1_V[1], T1_V[2], T2_V[0], T2_V[1], T2_V[2], &coplanar, isectpt1, isectpt2)) {
	ON_3dPoint p1((*isectpt1)[X], (*isectpt1)[Y], (*isectpt1)[Z]);
	ON_3dPoint p2((*isectpt2)[X], (*isectpt2)[Y], (*isectpt2)[Z]);
	if (p1.DistanceTo(p2) < ON_ZERO_TOLERANCE) {
	    //std::cout << "skipping pnt isect(" << coplanar << "): " << (*isectpt1)[X] << "," << (*isectpt1)[Y] << "," << (*isectpt1)[Z] << "\n";
	    return 0;
	}
	ON_Line e1(*fmesh1->pnts[t1.v[0]], *fmesh1->pnts[t1.v[1]]);
	ON_Line e2(*fmesh1->pnts[t1.v[1]], *fmesh1->pnts[t1.v[2]]);
	ON_Line e3(*fmesh1->pnts[t1.v[2]], *fmesh1->pnts[t1.v[0]]);
	double p1_d1 = p1.DistanceTo(e1.ClosestPointTo(p1));
	double p1_d2 = p1.DistanceTo(e2.ClosestPointTo(p1));
	double p1_d3 = p1.DistanceTo(e3.ClosestPointTo(p1));
	double p2_d1 = p2.DistanceTo(e1.ClosestPointTo(p2));
	double p2_d2 = p2.DistanceTo(e2.ClosestPointTo(p2));
	double p2_d3 = p2.DistanceTo(e3.ClosestPointTo(p2));
	// If both points are on the same edge, it's an edge-only intersect - skip
	if (NEAR_ZERO(p1_d1, ON_ZERO_TOLERANCE) &&  NEAR_ZERO(p2_d1, ON_ZERO_TOLERANCE)) {
	    //std::cout << "edge-only intersect - e1\n";
	    return 0;
	}
	if (NEAR_ZERO(p1_d2, ON_ZERO_TOLERANCE) &&  NEAR_ZERO(p2_d2, ON_ZERO_TOLERANCE)) {
	    //std::cout << "edge-only intersect - e2\n";
	    return 0;
	}
	if (NEAR_ZERO(p1_d3, ON_ZERO_TOLERANCE) &&  NEAR_ZERO(p2_d3, ON_ZERO_TOLERANCE)) {
	    //std::cout << "edge-only intersect - e3\n";
	    return 0;
	}

	return (coplanar) ? 1 : 2;
    }

    return 0;
}

/******************************************************************************
 * As a first step, use the face bboxes to narrow down where we have potential
 * interactions between breps
 ******************************************************************************/

struct nf_info {
    std::set<std::pair<cdt_mesh::cdt_mesh_t *, cdt_mesh::cdt_mesh_t *>> *check_pairs;
    cdt_mesh::cdt_mesh_t *cmesh;
};

static bool NearFacesPairsCallback(void *data, void *a_context) {
    struct nf_info *nf = (struct nf_info *)a_context;
    cdt_mesh::cdt_mesh_t *omesh = (cdt_mesh::cdt_mesh_t *)data;
    std::pair<cdt_mesh::cdt_mesh_t *, cdt_mesh::cdt_mesh_t *> p1(nf->cmesh, omesh);
    std::pair<cdt_mesh::cdt_mesh_t *, cdt_mesh::cdt_mesh_t *> p2(omesh, nf->cmesh);
    if ((nf->check_pairs->find(p1) == nf->check_pairs->end()) && (nf->check_pairs->find(p1) == nf->check_pairs->end())) {
	nf->check_pairs->insert(p1);
    }
    return true;
}

#if 0
static bool NearFacesCallback(size_t data, void *a_context) {
    std::set<size_t> *faces = (std::set<size_t> *)a_context;
    size_t f_id = data;
    faces->insert(f_id);
    return true;
}
#endif

std::set<std::pair<cdt_mesh::cdt_mesh_t *, cdt_mesh::cdt_mesh_t *>>
possibly_interfering_face_pairs(struct ON_Brep_CDT_State **s_a, int s_cnt)
{
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
	    rtree_fmeshes.Search(fMin, fMax, NearFacesPairsCallback, (void *)&nf);
	    // Add the new box to the tree so any additional boxes can check
	    // against it as well
	    rtree_fmeshes.Insert(fMin, fMax, (void *)fmesh);
	}
    }

    return check_pairs;
}

size_t
face_fmesh_ovlps(std::set<std::pair<cdt_mesh::cdt_mesh_t *, cdt_mesh::cdt_mesh_t *>> check_pairs)
{
    size_t tri_isects = 0;
    std::set<std::pair<cdt_mesh::cdt_mesh_t *, cdt_mesh::cdt_mesh_t *>>::iterator cp_it;
    for (cp_it = check_pairs.begin(); cp_it != check_pairs.end(); cp_it++) {
	cdt_mesh::cdt_mesh_t *fmesh1 = cp_it->first;
	cdt_mesh::cdt_mesh_t *fmesh2 = cp_it->second;
	struct ON_Brep_CDT_State *s_cdt1 = (struct ON_Brep_CDT_State *)fmesh1->p_cdt;
	struct ON_Brep_CDT_State *s_cdt2 = (struct ON_Brep_CDT_State *)fmesh2->p_cdt;
	if (s_cdt1 != s_cdt2) {
	    std::set<std::pair<size_t, size_t>> tris_prelim;
	    size_t ovlp_cnt = fmesh1->tris_tree.Overlaps(fmesh2->tris_tree, &tris_prelim);
	    if (ovlp_cnt) {
		std::set<std::pair<size_t, size_t>>::iterator tb_it;
		for (tb_it = tris_prelim.begin(); tb_it != tris_prelim.end(); tb_it++) {
		    cdt_mesh::triangle_t t1 = fmesh1->tris_vect[tb_it->first];
		    cdt_mesh::triangle_t t2 = fmesh2->tris_vect[tb_it->second];
		    point_t isectpt1 = {MAX_FASTF, MAX_FASTF, MAX_FASTF};
		    point_t isectpt2 = {MAX_FASTF, MAX_FASTF, MAX_FASTF};
		    int isect = tri_isect(fmesh1, t1, fmesh2, t2, &isectpt1, &isectpt2);
		    if (isect) {
			tri_isects++;
		    }
		}
	    }
	}
    }
    return tri_isects;
}

size_t
face_omesh_ovlps(std::set<std::pair<omesh_t *, omesh_t *>> check_pairs)
{
    size_t tri_isects = 0;
    std::set<std::pair<omesh_t *, omesh_t *>>::iterator cp_it;
    for (cp_it = check_pairs.begin(); cp_it != check_pairs.end(); cp_it++) {
	cp_it->first->ovlping_tris.clear();
	cp_it->second->ovlping_tris.clear();
    }
    for (cp_it = check_pairs.begin(); cp_it != check_pairs.end(); cp_it++) {
	cdt_mesh::cdt_mesh_t *fmesh1 = cp_it->first->fmesh;
	cdt_mesh::cdt_mesh_t *fmesh2 = cp_it->second->fmesh;
	struct ON_Brep_CDT_State *s_cdt1 = (struct ON_Brep_CDT_State *)fmesh1->p_cdt;
	struct ON_Brep_CDT_State *s_cdt2 = (struct ON_Brep_CDT_State *)fmesh2->p_cdt;
	if (s_cdt1 != s_cdt2) {
	    std::set<std::pair<size_t, size_t>> tris_prelim;
	    size_t ovlp_cnt = fmesh1->tris_tree.Overlaps(fmesh2->tris_tree, &tris_prelim);
	    if (ovlp_cnt) {
		std::set<std::pair<size_t, size_t>>::iterator tb_it;
		for (tb_it = tris_prelim.begin(); tb_it != tris_prelim.end(); tb_it++) {
		    cdt_mesh::triangle_t t1 = fmesh1->tris_vect[tb_it->first];
		    cdt_mesh::triangle_t t2 = fmesh2->tris_vect[tb_it->second];
		    point_t isectpt1 = {MAX_FASTF, MAX_FASTF, MAX_FASTF};
		    point_t isectpt2 = {MAX_FASTF, MAX_FASTF, MAX_FASTF};
		    int isect = tri_isect(fmesh1, t1, fmesh2, t2, &isectpt1, &isectpt2);
		    if (isect) {
			cp_it->first->ovlping_tris.insert(t1.ind);
			cp_it->second->ovlping_tris.insert(t2.ind);
			tri_isects++;
		    }
		}
	    }
	}
    }
    for (cp_it = check_pairs.begin(); cp_it != check_pairs.end(); cp_it++) {
	cp_it->first->plot();
	cp_it->second->plot();
    }
 
    return tri_isects;
}


double
tri_shortest_edge(cdt_mesh::cdt_mesh_t *fmesh, long t_ind)
{
    cdt_mesh::triangle_t t = fmesh->tris_vect[t_ind];
    double len = DBL_MAX;
    for (int i = 0; i < 3; i++) {
	long v0 = t.v[i];
	long v1 = (i < 2) ? t.v[i + 1] : t.v[0];
	ON_3dPoint *p1 = fmesh->pnts[v0];
	ON_3dPoint *p2 = fmesh->pnts[v1];
	double d = p1->DistanceTo(*p2);
	len = (d < len) ? d : len;
    }

    return len;
}

double
tri_longest_edge(cdt_mesh::cdt_mesh_t *fmesh, long t_ind)
{
    cdt_mesh::triangle_t t = fmesh->tris_vect[t_ind];
    double len = -DBL_MAX;
    for (int i = 0; i < 3; i++) {
	long v0 = t.v[i];
	long v1 = (i < 2) ? t.v[i + 1] : t.v[0];
	ON_3dPoint *p1 = fmesh->pnts[v0];
	ON_3dPoint *p2 = fmesh->pnts[v1];
	double d = p1->DistanceTo(*p2);
	len = (d > len) ? d : len;
    }

    return len;
}

bool
projects_inside_tri(
	cdt_mesh::cdt_mesh_t *fmesh,
       	cdt_mesh::triangle_t &t,
	ON_3dPoint &sp
	)
{
    ON_3dVector tdir = fmesh->tnorm(t);
    ON_3dPoint pcenter(*fmesh->pnts[t.v[0]]);
    for (int i = 1; i < 3; i++) {
	pcenter += *fmesh->pnts[t.v[i]];
    }
    pcenter = pcenter / 3;
    ON_Plane tplane(pcenter, tdir);

    cdt_mesh::cpolygon_t polygon;
    for (int i = 0; i < 3; i++) {
	double u, v;
	ON_3dPoint op3d = *fmesh->pnts[t.v[i]];
	tplane.ClosestPointTo(op3d, &u, &v);
	std::pair<double, double> proj_2d;
	proj_2d.first = u;
	proj_2d.second = v;
	polygon.pnts_2d.push_back(proj_2d);
    }
    for (int i = 0; i < 3; i++) {
	long v1 = (i < 2) ? i + 1 : 0;
	struct cdt_mesh::edge_t e(i, v1);
	polygon.add_edge(e);
    }

    double un, vn;
    tplane.ClosestPointTo(sp, &un, &vn);
    std::pair<double, double> n2d;
    n2d.first = un;
    n2d.second = vn;
    polygon.pnts_2d.push_back(n2d);

    return polygon.point_in_polygon(polygon.pnts_2d.size() - 1, false);
}


// Characterize the point position relative to the triangles'
// structure as follows:
/*
 *
 *                         /\
 *                        /33\
 *                       /3333\
 *                      / 3333 \
 *                     /   33   \
 *                    /    /\    \
 *                   /    /  \    \
 *                  /    /    \    \
 *             0   /    /      \    \  0
 *                / 2  /        \ 2  \
 *               /    /          \    \
 *              /    /            \    \
 *             /    /       1      \    \
 *            /    /                \    \
 *           /    /                  \    \
 *          /333 /                    \ 333\
 *         /33333______________________33333\
 *        /33333            2           33333\
 *       --------------------------------------
 *
 *                          0
 *
 *  If a type 2 point is also near a brep face edge, it is
 *  elevated to type 4.
 */
// For the moment, we're defining the distance away
// from the vert and edge structures as .1 * the
// shortest triangle edge.
int
characterize_avgpnt(
	cdt_mesh::triangle_t &t,
	cdt_mesh::cdt_mesh_t *fmesh,
	ON_3dPoint &sp,
	double *dist
	)
{
    double t_se = tri_shortest_edge(fmesh, t.ind);

    // Figure out how far away the triangle is from the point in question
    point_t tp, v0, v1, v2;
    VSET(tp, sp.x, sp.y, sp.z);
    VSET(v0, fmesh->pnts[t.v[0]]->x, fmesh->pnts[t.v[0]]->y, fmesh->pnts[t.v[0]]->z);
    VSET(v1, fmesh->pnts[t.v[1]]->x, fmesh->pnts[t.v[1]]->y, fmesh->pnts[t.v[1]]->z);
    VSET(v2, fmesh->pnts[t.v[2]]->x, fmesh->pnts[t.v[2]]->y, fmesh->pnts[t.v[2]]->z);
    (*dist) = bg_tri_pt_dist(tp, v0, v1, v2);

    // Make sure the point projects inside the triangle - if it doesn't
    // it's a category 0 point
    bool t_projects = projects_inside_tri(fmesh, t, sp);

    if (!t_projects) {
	return 0;
    }

    ON_3dPoint t_pnts[3];
    ON_Line t_edges[3];
    for (int i = 0; i < 3; i++) {
	t_pnts[i] = *fmesh->pnts[t.v[i]];
    }
    for (int i = 0; i < 3; i++) {
	int j = (i < 2) ? i+1 : 0;
	t_edges[i] = ON_Line(t_pnts[i], t_pnts[j]);
    }

    double t_dpnts[3] = {DBL_MAX, DBL_MAX, DBL_MAX};
    double t_dedges[3] = {DBL_MAX, DBL_MAX, DBL_MAX};

    for (int i = 0; i < 3; i++) {
	t_dpnts[i] = t_pnts[i].DistanceTo(sp);
	t_dedges[i] = sp.DistanceTo(t_edges[i].ClosestPointTo(sp));
    }

    double t_dpmin = DBL_MAX;
    double t_demin = DBL_MAX;

    for (int i = 0; i < 3; i++) {
	t_dpmin = (t_dpnts[i] < t_dpmin) ? t_dpnts[i] : t_dpmin;
	t_demin = (t_dedges[i] < t_demin) ? t_dedges[i] : t_demin;
    }

    bool t_close_to_vert = (t_dpmin < 0.1 * t_se);
    bool t_close_to_edge = (t_demin < 0.1 * t_se);

    bool t_close_to_face_edge = false;
    if (t_close_to_edge) {
	for (int i = 0; i < 3; i++) {
	    if (NEAR_EQUAL(t_dedges[i], t_demin, ON_ZERO_TOLERANCE)) {;
		int j = (i < 2) ? i+1 : 0;
		struct cdt_mesh::uedge_t ue(t.v[i], t.v[j]);
		if (fmesh->brep_edges.find(ue) != fmesh->brep_edges.end()) {
		    t_close_to_face_edge = true;
		    break;
		}
	    }
	}
    }

    int rtype = 0;

    if (t_close_to_vert) {
	rtype = 3;
    }
    if (!t_close_to_vert && t_close_to_edge) {
	rtype = 2;
    }
    if (!t_close_to_vert && !t_close_to_edge) {
	rtype = 1;
    }

    if (rtype == 2 && t_close_to_face_edge) {
	rtype = 4;
    }

    return rtype;
}

/******************************************************************************
 * For nearby vertices that meet certain criteria, we can adjust the vertices
 * to instead use closest points from the various surfaces and eliminate
 * what would otherwise be rather thorny triangle intersection cases.
 ******************************************************************************/
struct mvert_info {
    struct ON_Brep_CDT_State *s_cdt;
    int f_id;
    long p_id;
    ON_BoundingBox bb;
    double e_minlen;
    std::map<long, struct mvert_info *> *map;
};

void
mvert_update_edge_minlen(struct mvert_info *v)
{
    struct ON_Brep_CDT_State *s_cdt = v->s_cdt;
    cdt_mesh::cdt_mesh_t &fmesh = s_cdt->fmeshes[v->f_id];

    // 1.  Get pnt's associated edges.
    std::set<cdt_mesh::edge_t> edges = fmesh.v2edges[v->p_id];

    // 2.  find the shortest edge associated with pnt
    std::set<cdt_mesh::edge_t>::iterator e_it;
    double elen = DBL_MAX;
    for (e_it = edges.begin(); e_it != edges.end(); e_it++) {
	ON_3dPoint *p1 = fmesh.pnts[(*e_it).v[0]];
	ON_3dPoint *p2 = fmesh.pnts[(*e_it).v[1]];
	double dist = p1->DistanceTo(*p2);
	elen = (dist < elen) ? dist : elen;
    }
    v->e_minlen = elen;
    //std::cout << "Min edge len: " << elen << "\n";
}

void
mvert_update_all_edge_minlens(struct mvert_info *v)
{
    struct ON_Brep_CDT_State *s_cdt = v->s_cdt;
    cdt_mesh::cdt_mesh_t &fmesh = s_cdt->fmeshes[v->f_id];

    std::set<long> verts;

    // 1.  Get pnt's associated edges.
    std::set<cdt_mesh::edge_t> edges = fmesh.v2edges[v->p_id];
    
    // 2.  Collect all the vertices associated with all the edges
    // connected to the original point - these are the mvert_info
    // structures that may have a new minimum edge length after
    // the change.
    std::set<cdt_mesh::edge_t>::iterator e_it;
    for (e_it = edges.begin(); e_it != edges.end(); e_it++) {
	verts.insert((*e_it).v[0]);
	verts.insert((*e_it).v[1]);
    }

    // 3.  Update each mvert_info minimum edge length
    std::set<long>::iterator v_it;
    for (v_it = verts.begin(); v_it != verts.end(); v_it++) {
	struct mvert_info *vu = (*v->map)[*v_it];
	mvert_update_edge_minlen(vu);
    }
}

overt_t *
get_largest_mvert(std::set<overt_t *> &verts) 
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

overt_t *
closest_mvert(std::set<overt_t *> &verts, overt_t *v) 
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

// TODO - need to be aware when one of the mvert vertices is on
// a brep face edge.  In that situation it has much less freedom
// to move, so we need to try and adjust the other point more
// aggressively.  If they're both edge points we can try this,
// but there's a decent chance we'll need a different refinement
// in that case.
//
// TODO - for edge points, should really be searching edge closest
// point rather than surface closest point...
void
adjust_overt_pair(overt_t *v1, overt_t *v2)
{
    ON_3dPoint p1 = v1->vpnt();
    ON_3dPoint p2 = v2->vpnt();
    double pdist = p1.DistanceTo(p2);
    ON_Line l(p1,p2);
    // Weight the t parameter on the line so we are closer to the vertex
    // with the shorter edge length (i.e. less freedom to move without
    // introducing locally severe mesh distortions.)
    double t = 1 - (v2->min_len() / (v1->min_len() + v2->min_len()));
    ON_3dPoint p_wavg = l.PointAt(t);
    if ((p1.DistanceTo(p_wavg) > v1->min_len()*0.5) || (p2.DistanceTo(p_wavg) > v2->min_len()*0.5)) {
	std::cout << "WARNING: large point shift compared to triangle edge length.\n";
    }
    ON_3dPoint s1_p, s2_p;
    ON_3dVector s1_n, s2_n;
    bool f1_eval = closest_surf_pnt(s1_p, s1_n, *v1->omesh->fmesh, &p_wavg, pdist);
    bool f2_eval = closest_surf_pnt(s2_p, s2_n, *v2->omesh->fmesh, &p_wavg, pdist);
    if (f1_eval && f2_eval) {
	(*v1->omesh->fmesh->pnts[v1->p_id]) = s1_p;
	(*v1->omesh->fmesh->normals[v1->omesh->fmesh->nmap[v1->p_id]]) = s1_n;
	(*v2->omesh->fmesh->pnts[v2->p_id]) = s2_p;
	(*v2->omesh->fmesh->normals[v2->omesh->fmesh->nmap[v2->p_id]]) = s2_n;

	// We just changed the vertex point values - need to update all the mvert_info
	// edge lengths which might be impacted...
	v1->omesh->verts_one_ring_update(v1->p_id);
	v2->omesh->verts_one_ring_update(v2->p_id);

#if 0
	std::cout << "p_wavg: " << p_wavg.x << "," << p_wavg.y << "," << p_wavg.z << "\n";
	std::cout << s_cdt1->name << " face " << fmesh1.f_id << " pnt " << v1->p_id << " (elen: " << e1 << "->" << v1->e_minlen << ") moved " << p1.DistanceTo(s1_p) << ": " << p1.x << "," << p1.y << "," << p1.z << " -> " << s1_p.x << "," << s1_p.y << "," << s1_p.z << "\n";
	std::cout << s_cdt2->name << " face " << fmesh2.f_id << " pnt " << v2->p_id << " (elen: " << e2 << "->" << v2->e_minlen << ") moved " << p2.DistanceTo(s2_p) << ": " << p2.x << "," << p2.y << "," << p2.z << " -> " << s2_p.x << "," << s2_p.y << "," << s2_p.z << "\n";
#endif
    } else {
	struct ON_Brep_CDT_State *s_cdt1 = (struct ON_Brep_CDT_State *)v1->omesh->fmesh->p_cdt;
	struct ON_Brep_CDT_State *s_cdt2 = (struct ON_Brep_CDT_State *)v2->omesh->fmesh->p_cdt;
	std::cout << "p_wavg: " << p_wavg.x << "," << p_wavg.y << "," << p_wavg.z << "\n";
	if (!f1_eval) {
	    std::cout << s_cdt1->name << " face " << v1->omesh->fmesh->f_id << " closest point eval failure\n";
	}
	if (!f2_eval) {
	    std::cout << s_cdt2->name << " face " << v2->omesh->fmesh->f_id << " closest point eval failure\n";
	}
    }
}

// Get the bounding boxes of all vertices of all meshes of all breps in the
// pairs sets that might have possible interactions
void
vert_bboxes(
	std::vector<struct mvert_info *> *all_mverts,
	std::map<std::pair<struct ON_Brep_CDT_State *, int>, RTree<void *, double, 3>> *rtrees_mpnts,
	std::map<std::pair<struct ON_Brep_CDT_State *, int>, std::map<long, struct mvert_info *>> *mpnt_maps,
	std::set<std::pair<cdt_mesh::cdt_mesh_t *, cdt_mesh::cdt_mesh_t *>> &check_pairs)
{
    // Get the bounding boxes of all vertices of all meshes of all breps in
    // s_a that might have possible interactions, and find close point sets
    std::set<cdt_mesh::cdt_mesh_t *> fmeshes;
    std::set<std::pair<cdt_mesh::cdt_mesh_t *, cdt_mesh::cdt_mesh_t *>>::iterator cp_it;
    for (cp_it = check_pairs.begin(); cp_it != check_pairs.end(); cp_it++) {
	cdt_mesh::cdt_mesh_t *fmesh1 = cp_it->first;
	cdt_mesh::cdt_mesh_t *fmesh2 = cp_it->second;
	fmeshes.insert(fmesh1);
	fmeshes.insert(fmesh2);
    }
    std::set<cdt_mesh::cdt_mesh_t *>::iterator f_it;
    for (f_it = fmeshes.begin(); f_it != fmeshes.end(); f_it++) {
	cdt_mesh::cdt_mesh_t *fmesh = *f_it;
	struct ON_Brep_CDT_State *s_cdt = (struct ON_Brep_CDT_State *)fmesh->p_cdt;

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

	std::vector<struct mvert_info *> mverts;
	std::set<long>::iterator a_it;
	for (a_it = averts.begin(); a_it != averts.end(); a_it++) {
	    // 0.  Initialize mvert object.
	    struct mvert_info *mvert = new struct mvert_info;
	    mvert->s_cdt = s_cdt;
	    mvert->f_id = fmesh->f_id;
	    mvert->p_id = *a_it;
	    mvert->map = &((*mpnt_maps)[std::make_pair(s_cdt,fmesh->f_id)]);
	    mvert_update_edge_minlen(mvert);
	    // 1.  create a bbox around pnt using length ~20% of the shortest edge length.
	    ON_3dPoint vpnt = *fmesh->pnts[(*a_it)];
	    ON_BoundingBox bb(vpnt, vpnt);
	    ON_3dPoint npnt;
	    npnt = vpnt;
	    double lfactor = 0.2;
	    double elen = mvert->e_minlen;
	    npnt.x = npnt.x + lfactor*elen;
	    bb.Set(npnt, true);
	    npnt = vpnt;
	    npnt.x = npnt.x - lfactor*elen;
	    bb.Set(npnt, true);
	    npnt = vpnt;
	    npnt.y = npnt.y + lfactor*elen;
	    bb.Set(npnt, true);
	    npnt = vpnt;
	    npnt.y = npnt.y - lfactor*elen;
	    bb.Set(npnt, true);
	    npnt = vpnt;
	    npnt.z = npnt.z + lfactor*elen;
	    bb.Set(npnt, true);
	    npnt = vpnt;
	    npnt.z = npnt.z - lfactor*elen;
	    bb.Set(npnt, true);
	    mvert->bb = bb;
	    // 2.  insert result into mverts;
	    mverts.push_back(mvert);
	}
	for (size_t i = 0; i < mverts.size(); i++) {
	    double fMin[3];
	    fMin[0] = mverts[i]->bb.Min().x;
	    fMin[1] = mverts[i]->bb.Min().y;
	    fMin[2] = mverts[i]->bb.Min().z;
	    double fMax[3];
	    fMax[0] = mverts[i]->bb.Max().x;
	    fMax[1] = mverts[i]->bb.Max().y;
	    fMax[2] = mverts[i]->bb.Max().z;
	    (*rtrees_mpnts)[std::make_pair(s_cdt,fmesh->f_id)].Insert(fMin, fMax, (void *)mverts[i]);
	    (*mverts[i]->map)[mverts[i]->p_id] = mverts[i];
	}
	all_mverts->insert(all_mverts->end(), mverts.begin(), mverts.end());
    }
}
 
size_t
adjust_close_verts(std::set<std::pair<omesh_t *, omesh_t *>> &check_pairs)
{
    std::map<overt_t *, std::set<overt_t*>> vert_ovlps;
    std::set<std::pair<omesh_t *, omesh_t *>>::iterator cp_it;
    for (cp_it = check_pairs.begin(); cp_it != check_pairs.end(); cp_it++) {
   	omesh_t *omesh1 = cp_it->first;
	omesh_t *omesh2 = cp_it->second;
	struct ON_Brep_CDT_State *s_cdt1 = (struct ON_Brep_CDT_State *)omesh1->fmesh->p_cdt;
	struct ON_Brep_CDT_State *s_cdt2 = (struct ON_Brep_CDT_State *)omesh2->fmesh->p_cdt;
	if (s_cdt1 == s_cdt2) continue;
	struct bu_vls fname = BU_VLS_INIT_ZERO;
	bu_vls_sprintf(&fname, "%s-%d-vtree.plot3", s_cdt1->name, omesh1->fmesh->f_id);
	omesh1->plot_vtree(bu_vls_cstr(&fname));
	bu_vls_sprintf(&fname, "%s-%d-vtree.plot3", s_cdt2->name, omesh2->fmesh->f_id);
	omesh2->plot_vtree(bu_vls_cstr(&fname));
	bu_vls_free(&fname);
	std::set<std::pair<long, long>> vert_pairs;
	omesh1->vtree.Overlaps(omesh2->vtree, &vert_pairs);
	std::cout << "(" << s_cdt1->name << "," << omesh1->fmesh->f_id << ")+(" << s_cdt2->name << "," << omesh2->fmesh->f_id << "): " << vert_pairs.size() << " vert box overlaps\n";
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
    }

    std::cout << "Have " << vq.size() << " simple interactions\n";
    std::cout << "Have " << vq_multi.size() << " complex interactions\n";
    std::set<overt_t *> adjusted;

    while (!vq.empty()) {
	std::pair<overt_t *, overt_t *> vpair = vq.front();
	vq.pop();

	adjust_overt_pair(vpair.first, vpair.second);
	adjusted.insert(vpair.first);
	adjusted.insert(vpair.second);
    }

    // If the box structure is more complicated, we need to be a bit selective
    while (vq_multi.size()) {

	overt_t *l = get_largest_mvert(vq_multi);
	overt_t *c = closest_mvert(vert_ovlps[l], l);
	vq_multi.erase(l);
	vq_multi.erase(c);
	vert_ovlps[l].erase(c);
	vert_ovlps[c].erase(l);
	//std::cout << "COMPLEX - adjusting 1 pair only:\n";
	adjust_overt_pair(l, c);
	adjusted.insert(l);
	adjusted.insert(c);
    }

    return adjusted.size();
}

void
orient_tri(cdt_mesh::cdt_mesh_t &fmesh, cdt_mesh::triangle_t &t)
{
    ON_3dVector tdir = fmesh.tnorm(t);
    ON_3dVector bdir = fmesh.bnorm(t);
    bool flipped_tri = (ON_DotProduct(tdir, bdir) < 0);
    if (flipped_tri) {
	long tmp = t.v[2];
	t.v[2] = t.v[1];
	t.v[1] = tmp;
    }
} 

void
replace_edge_split_tri(cdt_mesh::cdt_mesh_t &fmesh, size_t t_id, long np_id, cdt_mesh::uedge_t &split_edge)
{
    cdt_mesh::triangle_t &t = fmesh.tris_vect[t_id];

    fmesh.tri_plot(t, "replacing.plot3");
    // Find the two triangle edges that weren't split - these are the starting points for the
    // new triangles
    cdt_mesh::edge_t e1, e2;
    int ecnt = 0;
    for (int i = 0; i < 3; i++) {
	long v0 = t.v[i];
	long v1 = (i < 2) ? t.v[i + 1] : t.v[0];
	cdt_mesh::edge_t ec(v0, v1);
	cdt_mesh::uedge_t uec(ec);
	if (uec != split_edge) {
	    if (!ecnt) {
		e1 = ec;
	    } else {
		e2 = ec;
	    }
	    ecnt++;
	}
    }

    cdt_mesh::triangle_t ntri1, ntri2;
    ntri1.v[0] = e1.v[0];
    ntri1.v[1] = np_id;
    ntri1.v[2] = e1.v[1];
    orient_tri(fmesh, ntri1);

    ntri2.v[0] = e2.v[0];
    ntri2.v[1] = np_id;
    ntri2.v[2] = e2.v[1];
    orient_tri(fmesh, ntri2);

    fmesh.tri_remove(t);
    fmesh.tri_add(ntri1);
    fmesh.tri_add(ntri2);

    fmesh.tri_plot(ntri1, "nt1.plot3");
    fmesh.tri_plot(ntri2, "nt2.plot3");
}

int
ovlp_split_edge(std::set<cdt_mesh::bedge_seg_t *> *nsegs, cdt_mesh::bedge_seg_t *eseg, double t)
{
    int replaced_tris = 0;

    // 1.  Get the triangle from each face associated with the edge segment to be split (should only
    // be one per face, since we're working with boundary edges.)
    struct ON_Brep_CDT_State *s_cdt_edge = (struct ON_Brep_CDT_State *)eseg->p_cdt;
    int f_id1 = s_cdt_edge->brep->m_T[eseg->tseg1->trim_ind].Face()->m_face_index;
    int f_id2 = s_cdt_edge->brep->m_T[eseg->tseg2->trim_ind].Face()->m_face_index;
    cdt_mesh::cdt_mesh_t &fmesh_f1 = s_cdt_edge->fmeshes[f_id1];
    cdt_mesh::cdt_mesh_t &fmesh_f2 = s_cdt_edge->fmeshes[f_id2];
    // Translate the tseg verts to their parent indices in order to get
    // valid triangle lookups
    cdt_mesh::cpolyedge_t *pe1 = eseg->tseg1;
    cdt_mesh::cpolyedge_t *pe2 = eseg->tseg2;
    cdt_mesh::cpolygon_t *poly1 = pe1->polygon;
    cdt_mesh::cpolygon_t *poly2 = pe2->polygon;
    cdt_mesh::uedge_t ue1(poly1->p2o[eseg->tseg1->v[0]], poly1->p2o[eseg->tseg1->v[1]]);
    cdt_mesh::uedge_t ue2(poly2->p2o[eseg->tseg2->v[0]], poly2->p2o[eseg->tseg2->v[1]]);
    fmesh_f1.brep_edges.erase(ue1); 
    fmesh_f2.brep_edges.erase(ue2); 
    //ON_3dPoint ue1_p1 = *fmesh_f1.pnts[ue1.v[0]];
    //ON_3dPoint ue1_p2 = *fmesh_f1.pnts[ue1.v[1]];
    //std::cout << f_id1 << " ue1: " << ue1.v[0] << "," << ue1.v[1] << ": " << ue1_p1.x << "," << ue1_p1.y << "," << ue1_p1.z << " -> " << ue1_p2.x << "," << ue1_p2.y << "," << ue1_p2.z << "\n";
    //ON_3dPoint ue2_p1 = *fmesh_f2.pnts[ue2.v[0]];
    //ON_3dPoint ue2_p2 = *fmesh_f2.pnts[ue2.v[1]];
    //std::cout << f_id2 << " ue2: " << ue2.v[0] << "," << ue2.v[1] << ": " << ue2_p1.x << "," << ue2_p1.y << "," << ue2_p1.z << " -> " << ue2_p2.x << "," << ue2_p2.y << "," << ue2_p2.z << "\n";
    std::set<size_t> f1_tris = fmesh_f1.uedges2tris[ue1];
    std::set<size_t> f2_tris = fmesh_f2.uedges2tris[ue2];

    int eind = eseg->edge_ind;
    std::set<cdt_mesh::bedge_seg_t *> epsegs = s_cdt_edge->e2polysegs[eind];
    epsegs.erase(eseg);
    std::set<cdt_mesh::bedge_seg_t *> esegs_split = split_edge_seg(s_cdt_edge, eseg, 1, &t, 1);
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
    std::set<cdt_mesh::bedge_seg_t *>::iterator es_it;
    for (es_it = esegs_split.begin(); es_it != esegs_split.end(); es_it++) {
	cdt_mesh::bedge_seg_t *es = *es_it;
	int fid1 = s_cdt_edge->brep->m_T[es->tseg1->trim_ind].Face()->m_face_index;
	int fid2 = s_cdt_edge->brep->m_T[es->tseg2->trim_ind].Face()->m_face_index;
	cdt_mesh::cdt_mesh_t &f1 = s_cdt_edge->fmeshes[fid1];
	cdt_mesh::cdt_mesh_t &f2 = s_cdt_edge->fmeshes[fid2];
	cdt_mesh::cpolyedge_t *pe_1 = es->tseg1;
	cdt_mesh::cpolyedge_t *pe_2 = es->tseg2;
	cdt_mesh::cpolygon_t *poly_1 = pe_1->polygon;
	cdt_mesh::cpolygon_t *poly_2 = pe_2->polygon;
	cdt_mesh::uedge_t ue_1(poly_1->p2o[es->tseg1->v[0]], poly_1->p2o[es->tseg1->v[1]]);
	cdt_mesh::uedge_t ue_2(poly_2->p2o[es->tseg2->v[0]], poly_2->p2o[es->tseg2->v[1]]);
	f1.brep_edges.insert(ue_1); 
	f2.brep_edges.insert(ue_2); 
    }

    long np_id;
    if (f_id1 == f_id2) {
	std::set<size_t> ftris;
	std::set<size_t>::iterator tr_it;
	cdt_mesh::uedge_t ue;
	ue = (f1_tris.size()) ? ue1 : ue2;
	ftris.insert(f1_tris.begin(), f1_tris.end());
	ftris.insert(f2_tris.begin(), f2_tris.end());
	np_id = fmesh_f1.pnts.size() - 1;
	fmesh_f1.ep.insert(np_id);
	for (tr_it = ftris.begin(); tr_it != ftris.end(); tr_it++) {
	    replace_edge_split_tri(fmesh_f1, *tr_it, np_id, ue);
	    replaced_tris++;
	}
    } else {
	np_id = fmesh_f1.pnts.size() - 1;
	fmesh_f1.ep.insert(np_id);
	replace_edge_split_tri(fmesh_f1, *f1_tris.begin(), np_id, ue1);
	replaced_tris++;
	np_id = fmesh_f2.pnts.size() - 1;
	fmesh_f2.ep.insert(np_id);
	replace_edge_split_tri(fmesh_f2, *f2_tris.begin(), np_id, ue2);
	replaced_tris++;
    }

    return replaced_tris;
}

int
split_brep_face_edges_near_verts(
	std::set<struct ON_Brep_CDT_State *> &a_cdt,
	std::set<std::pair<omesh_t *, omesh_t *>> &check_pairs
	)
{
    std::map<long, cdt_mesh::bedge_seg_t *> b_edges;
    RTree<long, double, 3> bedge_tree;
    long ecnt = 0;
    {
	std::set<struct ON_Brep_CDT_State *>::iterator a_it;
	for (a_it = a_cdt.begin(); a_it != a_cdt.end(); a_it++) {
	    struct ON_Brep_CDT_State *s_cdt = *a_it;
	    std::map<int, std::set<cdt_mesh::bedge_seg_t *>>::iterator ep_it;
	    for (ep_it = s_cdt->e2polysegs.begin(); ep_it != s_cdt->e2polysegs.end(); ep_it++) {
		std::set<cdt_mesh::bedge_seg_t *>::iterator bs_it;
		for (bs_it = ep_it->second.begin(); bs_it != ep_it->second.end(); bs_it++) {
		    cdt_mesh::bedge_seg_t *bseg = *bs_it;
		    b_edges[ecnt] = bseg;
		    ON_BoundingBox bb = edge_bbox(bseg);
		    double fMin[3];
		    fMin[0] = bb.Min().x;
		    fMin[1] = bb.Min().y;
		    fMin[2] = bb.Min().z;
		    double fMax[3];
		    fMax[0] = bb.Max().x;
		    fMax[1] = bb.Max().y;
		    fMax[2] = bb.Max().z;
		    bedge_tree.Insert(fMin, fMax, ecnt);
		    ecnt++;
		}
	    }
	}
    }

    std::set<std::pair<omesh_t *, omesh_t *>>::iterator cp_it;
    int replaced_tris = 0;

    std::set<omesh_t *> ameshes;
    for (cp_it = check_pairs.begin(); cp_it != check_pairs.end(); cp_it++) {
	ameshes.insert(cp_it->first);
	ameshes.insert(cp_it->second);
    }

    // Iterate over verts, checking for nearby edges.
    std::map<cdt_mesh::bedge_seg_t *, overt_t *> edge_vert;
    struct bu_vls fname = BU_VLS_INIT_ZERO;
    bu_vls_sprintf(&fname, "vert_edge_pairs.plot3");
    FILE* plot_file = fopen(bu_vls_cstr(&fname), "w");
    std::set<omesh_t *>::iterator a_it;
    for (a_it = ameshes.begin(); a_it != ameshes.end(); a_it++) {
	omesh_t *omesh = *a_it;
	std::set<std::pair<long, long>> vert_edge_pairs;
	size_t ovlp_cnt = omesh->vtree.Overlaps(bedge_tree, &vert_edge_pairs);
	int used_verts = 0;
	if (ovlp_cnt && vert_edge_pairs.size()) {
	    std::set<std::pair<long, long>>::iterator v_it;
	    for (v_it = vert_edge_pairs.begin(); v_it != vert_edge_pairs.end(); v_it++) {
		overt_t *v = omesh->overts[v_it->first];
		ON_3dPoint p = v->vpnt();
		cdt_mesh::bedge_seg_t *eseg = b_edges[v_it->second];

		ON_3dPoint *p3d1 = eseg->e_start;
		ON_3dPoint *p3d2 = eseg->e_end;
		ON_Line line(*p3d1, *p3d2);
		double d1 = p3d1->DistanceTo(p);
		double d2 = p3d2->DistanceTo(p);
		double dline = 2*p.DistanceTo(line.ClosestPointTo(p));
		if (d1 > dline && d2 > dline) {
		    //std::cout << "ACCEPT: d1: " << d1 << ", d2: " << d2 << ", dline: " << dline << "\n";
		    if (edge_vert.find(eseg) != edge_vert.end()) {
			ON_3dPoint pv = edge_vert[eseg]->vpnt();
			double dv = pv.DistanceTo(line.ClosestPointTo(pv));
			if (dv > dline) {
			    edge_vert[eseg] = v;
			}
		    } else {
			edge_vert[eseg] = v;
			used_verts++;
		    }
		    pl_color(plot_file, 255, 0, 0);
		    BBOX_PLOT(plot_file, v->bb);
		    pl_color(plot_file, 0, 0, 255);
		    ON_BoundingBox edge_bb = edge_bbox(eseg);
		    BBOX_PLOT(plot_file, edge_bb);
		} else {
		    //std::cout << "REJECT: d1: " << d1 << ", d2: " << d2 << ", dline: " << dline << "\n";
		}
	    }
	    //std::cout << "used_verts: " << used_verts << "\n";
	}
    }
    fclose(plot_file);


    // 2.  Find the point on the edge nearest to the vert point.  (TODO - need to think about how to
    // handle multiple verts associated with same edge - may want to iterate starting with the closest
    // and see if splitting clears the others...)
    std::map<cdt_mesh::bedge_seg_t *, overt_t *>::iterator ev_it;
    for (ev_it = edge_vert.begin(); ev_it != edge_vert.end(); ev_it++) {
	cdt_mesh::bedge_seg_t *eseg = ev_it->first;
	overt_t *v = ev_it->second;

	ON_NurbsCurve *nc = eseg->nc;
	ON_Interval domain(eseg->edge_start, eseg->edge_end);
	ON_3dPoint p = v->vpnt();
	double t;
	ON_NurbsCurve_GetClosestPoint(&t, nc, p, 0.0, &domain);
	ON_3dPoint cep = nc->PointAt(t);
	//ON_3dPoint concern(2.599,7.821, 23.563);
	//if (cep.DistanceTo(concern) < 0.01) {
	    //std::cout << "cep: " << cep.x << "," << cep.y << "," << cep.z << "\n";
	//    std::cout << "Distance: " << cep.DistanceTo(p) << "\n";
	//}
	double epdist1 = eseg->e_start->DistanceTo(cep);
	double epdist2 = eseg->e_end->DistanceTo(cep);
	double lseg_check = 0.1 * eseg->e_start->DistanceTo(*eseg->e_end);
	//std::cout << "d1: " << epdist1 << ", d2: " << epdist2 << ", lseg_check: " << lseg_check << "\n";
	if (epdist1 > lseg_check && epdist2 > lseg_check) {
	    // If the point is not close to a start/end point on the edge then split the edge.

#if 0
	    /* NOTE - need to get this information before ovlp_split_edge invalidates eseg */
	    struct ON_Brep_CDT_State *s_cdt_edge = (struct ON_Brep_CDT_State *)eseg->p_cdt;
	    int f_id1 = s_cdt_edge->brep->m_T[eseg->tseg1->trim_ind].Face()->m_face_index;
	    int f_id2 = s_cdt_edge->brep->m_T[eseg->tseg2->trim_ind].Face()->m_face_index;
#endif

	    int rtris = ovlp_split_edge(NULL, eseg, t);
	    if (rtris >= 0) {
		replaced_tris += rtris;
#if 0
		cdt_mesh::cdt_mesh_t &fmesh_f1 = s_cdt_edge->fmeshes[f_id1];
		cdt_mesh::cdt_mesh_t &fmesh_f2 = s_cdt_edge->fmeshes[f_id2];
	//std::cout << s_cdt_edge->name << " face " << fmesh_f1.f_id << " validity: " << fmesh_f1.valid(1) << "\n";
	//std::cout << s_cdt_edge->name << " face " << fmesh_f2.f_id << " validity: " << fmesh_f2.valid(1) << "\n";
		struct bu_vls fename = BU_VLS_INIT_ZERO;
		bu_vls_sprintf(&fename, "%s-%d_post_edge_tris.plot3", s_cdt_edge->name, fmesh_f1.f_id);
		fmesh_f1.tris_plot(bu_vls_cstr(&fename));
		bu_vls_sprintf(&fename, "%s-%d_post_edge_tris.plot3", s_cdt_edge->name, fmesh_f2.f_id);
		fmesh_f2.tris_plot(bu_vls_cstr(&fename));
		bu_vls_free(&fename);
#endif
	    } else {
		std::cout << "split failed\n";
	    }
	}
    }

    return replaced_tris;
}

static bool NearEdgesCallback(void *data, void *a_context) {
    std::set<cdt_mesh::cpolyedge_t *> *edges = (std::set<cdt_mesh::cpolyedge_t *> *)a_context;
    cdt_mesh::cpolyedge_t *pe  = (cdt_mesh::cpolyedge_t *)data;
    edges->insert(pe);
    return true;
}

void
check_faces_validity(std::set<std::pair<cdt_mesh::cdt_mesh_t *, cdt_mesh::cdt_mesh_t *>> &check_pairs, int UNUSED(id))
{
    std::set<cdt_mesh::cdt_mesh_t *> fmeshes;
    std::set<std::pair<cdt_mesh::cdt_mesh_t *, cdt_mesh::cdt_mesh_t *>>::iterator cp_it;
    for (cp_it = check_pairs.begin(); cp_it != check_pairs.end(); cp_it++) {
	cdt_mesh::cdt_mesh_t *fmesh1 = cp_it->first;
	cdt_mesh::cdt_mesh_t *fmesh2 = cp_it->second;
	fmeshes.insert(fmesh1);
	fmeshes.insert(fmesh2);
    }
    std::set<cdt_mesh::cdt_mesh_t *>::iterator f_it;
    for (f_it = fmeshes.begin(); f_it != fmeshes.end(); f_it++) {
	cdt_mesh::cdt_mesh_t *fmesh = *f_it;
	std::cout << "face " << fmesh->f_id << " validity: " << fmesh->valid(1) << "\n";
#if 0
	struct ON_Brep_CDT_State *s_cdt = (struct ON_Brep_CDT_State *)fmesh->p_cdt;
	std::string fpname = std::to_string(id) + std::string("_") + std::string(s_cdt->name) + std::string("_face_") + std::to_string(fmesh->f_id) + std::string(".plot3");
	fmesh->tris_plot(fpname.c_str());
#endif
    }
}

struct p_mvert_info {
    struct ON_Brep_CDT_State *s_cdt;
    int f_id;
    ON_3dPoint p;
    ON_3dVector n;
    ON_BoundingBox bb;
    bool edge_split_only;
    bool deactivate;
};

void plot_mvert_set(double r, std::set<struct p_mvert_info *> &pv)
{
    FILE *plot = fopen("mvert.plot3", "w");
    pl_color(plot, 255, 0, 0);
    std::set<struct p_mvert_info *>::iterator pm_it;
    for (pm_it = pv.begin(); pm_it != pv.end(); pm_it++) {
	plot_pnt_3d(plot, &((*pm_it)->p), r, 0);
    }
    fclose(plot);
}

#if 0
static bool NearVertsCallback(void *data, void *a_context) {
    std::set<long> *cpnts = (std::set<long> *)a_context;
    struct mvert_info *mv = (struct mvert_info *)data;
    cpnts->insert(mv->p_id);
    return true;
}
#endif

void
get_intruding_points(std::set<std::pair<omesh_t *, omesh_t *>> &check_pairs)
{
    std::set<std::pair<omesh_t *, omesh_t *>>::iterator cp_it;
    for (cp_it = check_pairs.begin(); cp_it != check_pairs.end(); cp_it++) {
	omesh_t *omesh1 = cp_it->first;
	omesh_t *omesh2 = cp_it->second;
	struct ON_Brep_CDT_State *s_cdt1 = (struct ON_Brep_CDT_State *)omesh1->fmesh->p_cdt;
	struct ON_Brep_CDT_State *s_cdt2 = (struct ON_Brep_CDT_State *)omesh2->fmesh->p_cdt;
	if (s_cdt1 == s_cdt2) {
	    // TODO: In principle we should be checking for self intersections
	    // - it can happen, particularly in sparse tessellations - but
	    // for now ignore it.
	    //std::cout << "SELF_ISECT\n";
	    continue;
	}

	std::set<std::pair<size_t, size_t>> tris_prelim;
	size_t ovlp_cnt = omesh1->fmesh->tris_tree.Overlaps(omesh2->fmesh->tris_tree, &tris_prelim);
	if (!ovlp_cnt) continue;
	std::set<std::pair<size_t, size_t>>::iterator tb_it;
	for (tb_it = tris_prelim.begin(); tb_it != tris_prelim.end(); tb_it++) {
	    cdt_mesh::triangle_t t1 = omesh1->fmesh->tris_vect[tb_it->first];
	    cdt_mesh::triangle_t t2 = omesh2->fmesh->tris_vect[tb_it->second];
	    point_t isectpt1 = {MAX_FASTF, MAX_FASTF, MAX_FASTF};
	    point_t isectpt2 = {MAX_FASTF, MAX_FASTF, MAX_FASTF};
	    int isect = tri_isect(omesh1->fmesh, t1, omesh2->fmesh, t2, &isectpt1, &isectpt2);
	    if (!isect) continue;

	    // Using triangle planes and then an inside/outside test, determine
	    // which point(s) from the opposite triangle are "inside" the
	    // meshes.  Each of these points is an "overlap instance" that the
	    // opposite mesh will have to try and adjust itself to to resolve.
	    ON_Plane plane1 = omesh1->fmesh->tplane(t1);
	    for (int i = 0; i < 3; i++) {
		if (omesh1->intruding_pnts.find(omesh2->overts[t2.v[i]]) != omesh1->intruding_pnts.end()) continue;
		ON_3dPoint tp = *omesh2->fmesh->pnts[t2.v[i]];
		double dist = plane1.DistanceTo(tp);
		if (dist < 0 && fabs(dist) > ON_ZERO_TOLERANCE && on_point_inside(s_cdt1, &tp)) {
		    std::cout << s_cdt1->name << " face " << omesh1->fmesh->f_id << " test point inside:\n";
		    std::cout << "tp: " << tp.x << "," << tp.y << "," << tp.z << "\n";
		    std::cout << "ip: " << omesh2->overts[t2.v[i]]->vpnt().x << "," << omesh2->overts[t2.v[i]]->vpnt().y << "," << omesh2->overts[t2.v[i]]->vpnt().z << "\n";
		    omesh1->intruding_pnts.insert(omesh2->overts[t2.v[i]]);
#if 0
			closest_surf_pnt(np->p, np->n, *fmesh1, &tp, 2*t1_longest);
			ON_BoundingBox bb(np->p, np->p);
			bb.m_max.x = bb.m_max.x + t1_longest;
			bb.m_max.y = bb.m_max.y + t1_longest;
			bb.m_max.z = bb.m_max.z + t1_longest;
			bb.m_min.x = bb.m_min.x - t1_longest;
			bb.m_min.y = bb.m_min.y - t1_longest;
			bb.m_min.z = bb.m_min.z - t1_longest;
			np->bb = bb;
			// TODO - check if this point is too close to an existing vert
			// point to insert. If so, tweak the vert point to match this point
			double fMin[3]; double fMax[3];
			fMin[0] = bb.Min().x;
			fMin[1] = bb.Min().y;
			fMin[2] = bb.Min().z;
			fMax[0] = bb.Max().x;
			fMax[1] = bb.Max().y;
			fMax[2] = bb.Max().z;
			std::set<long> cpnts;
			bool add_pnt = true;
			size_t npnts = rtrees_mpnts[std::make_pair(s_cdt1,fmesh1->f_id)].Search(fMin, fMax, NearVertsCallback, (void *)&cpnts);
			if (npnts) {
			    std::set<long>::iterator c_it;
			    for (c_it = cpnts.begin(); c_it != cpnts.end(); c_it++) {
				ON_3dPoint *p = fmesh1->pnts[*c_it];
				std::cout << p->DistanceTo(np->p) << "\n";
			    }
			}
			if (add_pnt) {
			    (*face_npnts)[fmesh1].insert(np);
			    added_verts[fmesh1].insert(std::make_pair(fmesh2, t2.v[i]));
			}
#endif
		}
	    }

	    ON_Plane plane2 = omesh2->fmesh->tplane(t2);
	    for (int i = 0; i < 3; i++) {
		if (omesh2->intruding_pnts.find(omesh1->overts[t1.v[i]]) != omesh2->intruding_pnts.end()) continue;
		ON_3dPoint tp = *omesh1->fmesh->pnts[t1.v[i]];
		double dist = plane2.DistanceTo(tp);
		if (dist < 0 && fabs(dist) > ON_ZERO_TOLERANCE && on_point_inside(s_cdt2, &tp)) {
		    std::cout << s_cdt2->name << " face " << omesh2->fmesh->f_id << " test point inside:\n";
		    std::cout << "tp: " << tp.x << "," << tp.y << "," << tp.z << "\n";
		    std::cout << "ip: " << omesh1->overts[t1.v[i]]->vpnt().x << "," << omesh1->overts[t1.v[i]]->vpnt().y << "," << omesh1->overts[t1.v[i]]->vpnt().z << "\n";
		    omesh2->intruding_pnts.insert(omesh1->overts[t1.v[i]]);
		}
	    }
	}
    }
}

void
process_near_edge_pnts(std::map<cdt_mesh::cdt_mesh_t *, std::set<struct p_mvert_info *>> *face_npnts)
{
    std::map<cdt_mesh::bedge_seg_t *, std::set<struct p_mvert_info *>> esplits;
    std::map<cdt_mesh::cdt_mesh_t *, std::set<struct p_mvert_info *>>::iterator f_it;
    for (f_it = face_npnts->begin(); f_it != face_npnts->end(); f_it++) {
	cdt_mesh::cdt_mesh_t *fmesh = f_it->first;
	struct ON_Brep_CDT_State *s_cdt = (struct ON_Brep_CDT_State *)fmesh->p_cdt;
	std::set<struct p_mvert_info *>::iterator pm_it;
	std::map<cdt_mesh::cpolyedge_t *, struct p_mvert_info *> to_split;
	for (pm_it = f_it->second.begin(); pm_it != f_it->second.end(); pm_it++) {
	    struct p_mvert_info *pmv = *pm_it;
	    // If this point is already off there's no point in looking again
	    if (pmv->deactivate) continue;
	    double fMin[3]; double fMax[3];
	    fMin[0] = pmv->bb.Min().x;
	    fMin[1] = pmv->bb.Min().y;
	    fMin[2] = pmv->bb.Min().z;
	    fMax[0] = pmv->bb.Max().x;
	    fMax[1] = pmv->bb.Max().y;
	    fMax[2] = pmv->bb.Max().z;
	    std::set<cdt_mesh::cpolyedge_t *> nedges;
	    size_t nhits = s_cdt->face_rtrees_3d[fmesh->f_id].Search(fMin, fMax, NearEdgesCallback, (void *)&nedges);

	    if (nhits) {
		std::set<cdt_mesh::cpolyedge_t *>::iterator n_it;
		cdt_mesh::bedge_seg_t *closest_edge = NULL;
		double closest_dist = DBL_MAX;
		for (n_it = nedges.begin(); n_it != nedges.end(); n_it++) {
		    cdt_mesh::bedge_seg_t *eseg = (*n_it)->eseg;
		    double lseg_dist = eseg->e_start->DistanceTo(*eseg->e_end);
		    ON_NurbsCurve *nc = eseg->nc;
		    ON_Interval domain(eseg->edge_start, eseg->edge_end);
		    double t;
		    ON_NurbsCurve_GetClosestPoint(&t, nc, pmv->p, 0.0, &domain);
		    ON_3dPoint cep = nc->PointAt(t);
		    double ecdist = cep.DistanceTo(pmv->p);
		    double epdist1 = eseg->e_start->DistanceTo(cep);
		    double epdist2 = eseg->e_end->DistanceTo(cep);
		    double lseg_check = 0.05 * lseg_dist;
		    FILE *plot_file = NULL;
		    if (epdist1 > lseg_check && epdist2 > lseg_check) {
			// If the point is not close to a start/end point on the edge then split the edge.
			// Check closest point to edge for each segment.  If
			// closest edge point is close to the edge per the line
			// segment length, we need to split the edge curve to keep
			// its approximating polyline as far as possible from the
			// new point.

			if (ecdist < 0.2*lseg_dist) {
			    double etol = s_cdt->brep->m_E[eseg->edge_ind].m_tolerance;
			    etol = (etol > 0) ? etol : ON_ZERO_TOLERANCE;
			    //std::cout << "etol,closest_dist,ecdist,lseg_dist: " << etol << "," << closest_dist << "," << ecdist << "," << lseg_dist << "\n";

			    if (closest_dist > ecdist) {
				closest_dist = ecdist;
				closest_edge = eseg;
				// TODO - probably should base this on what kind of triangle would
				// get created if we don't settle for only splitting the edge, if that's
				// practical...
				//
				// Alternately (and maybe better), can we adjust
				// the intruding vertex to use the point that will
				// come from the split?
				if (ecdist <= etol || ecdist < 0.02*lseg_dist) {
				    // If the point is actually ON the edge (to
				    // within ON_ZERO_TOLERANCE or the brep edge's
				    // tolerance) we'll be introducing a new point
				    // on the edge AT that point, and no additional
				    // work is needed on that point.  If that's the
				    // case set the flag, otherwise, the point
				    // stays "live" and feeds into the next step.
				    pmv->edge_split_only = true;
				    //std::cout << "edge split only\n";
				    //plot_file = fopen("edge_mvert_eonly.plot3", "w");
				} else {
				    //plot_file = fopen("edge_mvert.plot3", "w");
				}

			    }
			}
		    } else {
			//std::cout << "too close to existing edge point...\n";
#if 0
			std::cout << "closest_dist,ecdist,lseg_dist: " << closest_dist << "," << ecdist << "," << lseg_dist << "\n";
			std::cout << "d1: " << epdist1 << ", d2: " << epdist2 << ", lseg_check: " << lseg_check << "\n";
#endif
			pmv->deactivate = true;
			//plot_file = fopen("edge_mvert_deactivate.plot3", "w");
		    }

		    if (plot_file) {
#if 1
			pl_color(plot_file, 0, 0, 255);
			point_t bnp1, bnp2;
			VSET(bnp1, eseg->e_start->x, eseg->e_start->y, eseg->e_start->z);
			VSET(bnp2, eseg->e_end->x, eseg->e_end->y, eseg->e_end->z);
			pdv_3move(plot_file, bnp1);
			pdv_3cont(plot_file, bnp2);
			pl_color(plot_file, 255, 0, 0);
			plot_pnt_3d(plot_file, &cep, 0.03*lseg_dist, 0);
			pl_color(plot_file, 0, 255, 0);
			plot_pnt_3d(plot_file, &pmv->p, 0.03*lseg_dist, 0);
#endif
			fclose(plot_file);
		    }

		}
		if (closest_edge) {
		    esplits[closest_edge].insert(pmv);
		}
	    }
	}
    }

    std::set<cdt_mesh::bedge_seg_t *> split_segs;

    std::map<cdt_mesh::bedge_seg_t *, std::set<struct p_mvert_info *>>::iterator es_it;
    for (es_it = esplits.begin(); es_it != esplits.end(); es_it++) {
	std::set<cdt_mesh::bedge_seg_t *> asegs;
	asegs.insert(es_it->first);
	if (split_segs.find(es_it->first) != split_segs.end()) {
	    std::cout << "ERROR - splitting on stale seg!\n";
	}
	// If we have multiple p_mvert points associated with this edge, we'll need
	// multiple splits on that edge - or, more precisely, we'll need to identify
	// the closest of the new edges that replaced the original after the first split
	// and split that one.
	//
	// We initialize the active segments set with the initial segment, and then
	// maintain the set with all unsplit segments that have replaced the original
	// edge segment or one of its replacements.
	std::set<struct p_mvert_info *>::iterator pm_it;
	for (pm_it = es_it->second.begin(); pm_it != es_it->second.end(); pm_it++) {
	    struct p_mvert_info *pmv = *pm_it;
	    if (pmv->deactivate) continue;
	    cdt_mesh::bedge_seg_t *closest_edge = NULL;
	    double split_t = -1.0;
	    double closest_dist = DBL_MAX;
	    std::set<cdt_mesh::bedge_seg_t *>::iterator e_it;
	    std::cout << "asegs size: " << asegs.size() << "\n";
	    for (e_it = asegs.begin(); e_it != asegs.end(); e_it++) {
		cdt_mesh::bedge_seg_t *eseg = *e_it;
		ON_NurbsCurve *nc = eseg->nc;
		ON_Interval domain(eseg->edge_start, eseg->edge_end);
		double t;
		ON_NurbsCurve_GetClosestPoint(&t, nc, pmv->p, 0.0, &domain);
		ON_3dPoint cep = nc->PointAt(t);
		double ecdist = cep.DistanceTo(pmv->p);
		std::cout << "closest_dist: " << closest_dist << "\n";
		std::cout << "ecdist: " << ecdist << "\n";
		if (closest_dist > ecdist) {
		    closest_dist = ecdist;
		    closest_edge = eseg;
		    split_t = t;
		}
	    }

	    if (closest_edge) {
		std::cout << "edge split\n";
		std::cout << "estart: " << closest_edge->e_start->x << "," << closest_edge->e_start->y << "," << closest_edge->e_start->z << "\n";
		std::cout << "eend  : " << closest_edge->e_end->x << "," << closest_edge->e_end->y << "," << closest_edge->e_end->z << "\n";
		asegs.erase(closest_edge);
		split_segs.insert(closest_edge);
		std::set<cdt_mesh::bedge_seg_t *> nsegs;

#if 1
		/* NOTE - need to get this information before ovlp_split_edge invalidates closest_edge */
		struct ON_Brep_CDT_State *s_cdt_edge = (struct ON_Brep_CDT_State *)closest_edge->p_cdt;
		int f_id1 = s_cdt_edge->brep->m_T[closest_edge->tseg1->trim_ind].Face()->m_face_index;
		int f_id2 = s_cdt_edge->brep->m_T[closest_edge->tseg2->trim_ind].Face()->m_face_index;
#endif
		ovlp_split_edge(&nsegs, closest_edge, split_t);
#if 1
		cdt_mesh::cdt_mesh_t &fmesh_f1 = s_cdt_edge->fmeshes[f_id1];
		cdt_mesh::cdt_mesh_t &fmesh_f2 = s_cdt_edge->fmeshes[f_id2];
		std::cout << s_cdt_edge->name << " face " << fmesh_f1.f_id << " validity: " << fmesh_f1.valid(1) << "\n";
		std::cout << s_cdt_edge->name << " face " << fmesh_f2.f_id << " validity: " << fmesh_f2.valid(1) << "\n";
		struct bu_vls fename = BU_VLS_INIT_ZERO;
		bu_vls_sprintf(&fename, "%s-%d_post_edge_tris.plot3", s_cdt_edge->name, fmesh_f1.f_id);
		fmesh_f1.tris_plot(bu_vls_cstr(&fename));
		bu_vls_sprintf(&fename, "%s-%d_post_edge_tris.plot3", s_cdt_edge->name, fmesh_f2.f_id);
		fmesh_f2.tris_plot(bu_vls_cstr(&fename));
		bu_vls_free(&fename);
#endif

		asegs.insert(nsegs.begin(), nsegs.end());
	    }
	}
    }
}

void
tri_retessellate(cdt_mesh::cdt_mesh_t *fmesh, long t_ind, std::set<struct p_mvert_info *> &npnts)
{
    cdt_mesh::triangle_t t = fmesh->tris_vect[t_ind];
    struct ON_Brep_CDT_State *s_cdt = (struct ON_Brep_CDT_State *)fmesh->p_cdt;
    cdt_mesh::cpolygon_t *polygon = new cdt_mesh::cpolygon_t;

    std::map<struct p_mvert_info *, long> pmv2f;

    std::set<struct p_mvert_info *>::iterator pmv_it;
    for (pmv_it = npnts.begin(); pmv_it != npnts.end(); pmv_it++) {
	struct p_mvert_info *pmv = *pmv_it;
	long f3ind = fmesh->add_point(new ON_3dPoint(pmv->p));
	long fnind = fmesh->add_normal(new ON_3dPoint(pmv->n));

        CDT_Add3DPnt(s_cdt, fmesh->pnts[fmesh->pnts.size()-1], fmesh->f_id, -1, -1, -1, 0, 0);
        CDT_Add3DNorm(s_cdt, fmesh->normals[fmesh->normals.size()-1], fmesh->pnts[fmesh->pnts.size()-1], fmesh->f_id, -1, -1, -1, 0, 0);
	//fmesh.p2d3d[f_ind2d] = f3ind;  for now, we're not putting in 2D versions of overlap points...
	//fmesh.p3d2d[f3ind] = f_ind2d;
	fmesh->nmap[f3ind] = fnind;
	pmv2f[pmv] = f3ind;
    }

    // As we do in the repair, project all the mesh points to the plane and
    // add them so the point indices are the same.  Eventually we can be
    // more sophisticated about this, but for now it avoids potential
    // bookkeeping problems.
    ON_3dPoint sp = fmesh->tcenter(t);
    ON_3dVector sn = fmesh->bnorm(t);
    ON_Plane tri_plane(sp, sn);
    std::map<long, long> t2p;
    for (size_t i = 0; i < 3; i++) {
	double u, v;
	ON_3dPoint op3d = (*fmesh->pnts[t.v[i]]);
	tri_plane.ClosestPointTo(op3d, &u, &v);
	std::pair<double, double> proj_2d;
	proj_2d.first = u;
	proj_2d.second = v;
	polygon->pnts_2d.push_back(proj_2d);
	if (fmesh->brep_edge_pnt(t.v[i])) {
	    polygon->brep_edge_pnts.insert(i);
	}
	polygon->p2o[i] = t.v[i];
	t2p[t.v[i]] = i;
    }
    struct cdt_mesh::edge_t e1(t2p[t.v[0]], t2p[t.v[1]]);
    struct cdt_mesh::edge_t e2(t2p[t.v[1]], t2p[t.v[2]]);
    struct cdt_mesh::edge_t e3(t2p[t.v[2]], t2p[t.v[0]]);
    polygon->add_edge(e1);
    polygon->add_edge(e2);
    polygon->add_edge(e3);

    // Let the polygon know we've got interior points
    std::map<struct p_mvert_info *, long>::iterator f_it;
    for (f_it = pmv2f.begin(); f_it != pmv2f.end(); f_it++) {
	double u, v;
	tri_plane.ClosestPointTo(f_it->first->p, &u, &v);
	std::pair<double, double> proj_2d;
	proj_2d.first = u;
	proj_2d.second = v;
	polygon->pnts_2d.push_back(proj_2d);
	polygon->p2o[polygon->pnts_2d.size() - 1] = f_it->second;
	polygon->interior_points.insert(polygon->pnts_2d.size() - 1);
    }

    polygon->polygon_plot("poly2d.plot3");
    fmesh->polygon_plot_3d(polygon, "poly3d.plot3");

    fmesh->tri_plot(t, "tremove.plot3");

    polygon->cdt(TRI_DELAUNAY);

    std::cout << "cdt tris cnt: " << polygon->tris.size() << "\n";

    if (polygon->tris.size()) {
	fmesh->tri_remove(t);

	std::set<cdt_mesh::triangle_t>::iterator v_it;
	for (v_it = polygon->tris.begin(); v_it != polygon->tris.end(); v_it++) {
	    cdt_mesh::triangle_t vt = *v_it;
	    orient_tri(*fmesh, vt);
	    fmesh->tri_add(vt);
	    fmesh->tri_plot(vt, "tadd.plot3");
	}
    }
}

int
ON_Brep_CDT_Ovlp_Resolve(struct ON_Brep_CDT_State **s_a, int s_cnt)
{
    std::set<std::pair<cdt_mesh::cdt_mesh_t *, cdt_mesh::cdt_mesh_t *>>::iterator p_it;
    if (!s_a) return -1;
    if (s_cnt < 1) return 0;

    // Get the bounding boxes of all faces of all breps in s_a, and find
    // possible interactions
    std::set<std::pair<cdt_mesh::cdt_mesh_t *, cdt_mesh::cdt_mesh_t *>> check_pairs;
    check_pairs = possibly_interfering_face_pairs(s_a, s_cnt);

    //std::cout << "Found " << check_pairs.size() << " potentially interfering face pairs\n";
    if (!check_pairs.size()) return 0;

    check_faces_validity(check_pairs, 0);

    int face_ov_cnt = face_fmesh_ovlps(check_pairs);
    std::cout << "Initial overlap cnt: " << face_ov_cnt << "\n";
    if (!face_ov_cnt) return 0;

    // Make omesh containers for all the cdt_meshes in play
    std::set<cdt_mesh::cdt_mesh_t *> afmeshes;
    std::vector<omesh_t *> omeshes;
    std::map<cdt_mesh::cdt_mesh_t *, omesh_t *> f2omap;
    for (p_it = check_pairs.begin(); p_it != check_pairs.end(); p_it++) {
	afmeshes.insert(p_it->first);
	afmeshes.insert(p_it->second);
    }
    std::set<cdt_mesh::cdt_mesh_t *>::iterator af_it;
    for (af_it = afmeshes.begin(); af_it != afmeshes.end(); af_it++) {
	cdt_mesh::cdt_mesh_t *fmesh = *af_it;
	omeshes.push_back(new omesh_t(fmesh));
	f2omap[fmesh] = omeshes[omeshes.size() - 1];
    }
    std::set<std::pair<omesh_t *, omesh_t *>> ocheck_pairs;
    for (p_it = check_pairs.begin(); p_it != check_pairs.end(); p_it++) {
	omesh_t *o1 = f2omap[p_it->first];
	omesh_t *o2 = f2omap[p_it->second];
	ocheck_pairs.insert(std::make_pair(o1, o2));
    }
    face_ov_cnt = face_omesh_ovlps(ocheck_pairs);

    size_t avcnt = adjust_close_verts(ocheck_pairs);

    if (avcnt) {
	std::cout << "Adjusted " << avcnt << " vertices\n";
	check_faces_validity(check_pairs, 1);
    }

    face_ov_cnt = face_omesh_ovlps(ocheck_pairs);
    std::cout << "Post vert adjustment overlap cnt: " << face_ov_cnt << "\n";


    // Boundary edges are handled at a brep object level, not a face level - handle
    // them as an independent process
    std::set<struct ON_Brep_CDT_State *> a_cdt;
    for (p_it = check_pairs.begin(); p_it != check_pairs.end(); p_it++) {
	a_cdt.insert((struct ON_Brep_CDT_State *)p_it->first->p_cdt);
	a_cdt.insert((struct ON_Brep_CDT_State *)p_it->second->p_cdt);
    }
    int sbfvtri_cnt = split_brep_face_edges_near_verts(a_cdt, ocheck_pairs);
    if (sbfvtri_cnt) {
	std::cout << "Replaced " << sbfvtri_cnt << " triangles by splitting edges near vertices\n";
	check_faces_validity(check_pairs, 2);
	face_ov_cnt = face_omesh_ovlps(ocheck_pairs);
	std::cout << "Post edges-near-verts split overlap cnt: " << face_ov_cnt << "\n";
    }

#if 0
    int sbfetri_cnt = split_brep_face_edges_near_edges(check_pairs);
    if (sbfetri_cnt) {
	std::cout << "Replaced " << sbfetri_cnt << " triangles by splitting edges near edges\n";
	check_faces_validity(check_pairs, 3);
    }
#endif

    get_intruding_points(ocheck_pairs);

    //process_near_edge_pnts(face_npnts);

    face_ov_cnt = face_omesh_ovlps(ocheck_pairs);
    //std::cout << "Post interior-near-edge split overlap cnt: " << face_ov_cnt << "\n";

#if 0


    int tneigh_cnt = 0;

    std::map<cdt_mesh::cdt_mesh_t *, std::set<struct p_mvert_info *>>::iterator f_it;
    for (f_it = face_npnts->begin(); f_it != face_npnts->end(); f_it++) {
	cdt_mesh::cdt_mesh_t *fmesh = f_it->first;
	struct ON_Brep_CDT_State *s_cdt = (struct ON_Brep_CDT_State *)fmesh->p_cdt;
	std::cout << "Refining " << s_cdt->name << " face " << fmesh->f_id << " with " << f_it->second.size() << " points.\n";
	std::set<struct p_mvert_info *>::iterator pm_it;

	std::map<long, std::set<struct p_mvert_info *>> tri_npnts;

	fmesh->tris_plot("face.plot3");
	plot_mvert_set(0.1,f_it->second);

	for (pm_it = f_it->second.begin(); pm_it != f_it->second.end(); pm_it++) {
	    struct p_mvert_info *pmv = *pm_it;

	    if (pmv->deactivate || pmv->edge_split_only) continue;
	    // TODO -  after the edge pass, search face triangle Rtree for closest
	    // triangles, and find the closest one where it can produce a valid
	    // projection into the triangle plane.  Associate the point with the
	    // triangle.  The set of points associated with the triangles will then
	    // be characterized, and as appropriate indiviual or pairs of triangles
	    // will be re-tessellated and replaced to incorporate the new points.
	    double fMin[3];
	    fMin[0] = pmv->bb.Min().x;
	    fMin[1] = pmv->bb.Min().y;
	    fMin[2] = pmv->bb.Min().z;
	    double fMax[3];
	    fMax[0] = pmv->bb.Max().x;
	    fMax[1] = pmv->bb.Max().y;
	    fMax[2] = pmv->bb.Max().z;
	    std::set<size_t> near_faces;
	    fmesh->tris_tree.Search(fMin, fMax, NearFacesCallback, (void *)&near_faces);
	    std::set<size_t>::iterator n_it;
	    std::set<tri_dist> atris;
	    int point_type = 0;
	    fmesh->tris_ind_set_plot(near_faces, "near_faces.plot3");
	    for (n_it = near_faces.begin(); n_it != near_faces.end(); n_it++) {
		double dist;
		cdt_mesh::triangle_t tri = fmesh->tris_vect[*n_it];
		int ptype = characterize_avgpnt(tri, fmesh, (pmv)->p, &dist);
		if (dist >= 0) {
		    atris.insert(tri_dist(dist, tri.ind));
		} else {
		    std::cout << "odd distance: " << dist << "\n";
		    std::cout << "TP: " << (pmv)->p.x << "," << (pmv)->p.y << "," << (pmv)->p.z << "\n";
		    cdt_mesh::triangle_t otri = fmesh->tris_vect[*n_it];
		    fmesh->tri_plot(otri, "otri.plot3");

		    std::cout << "V0: " << fmesh->pnts[otri.v[0]]->x << "," << fmesh->pnts[otri.v[0]]->y << "," << fmesh->pnts[otri.v[0]]->z << "\n";
		    std::cout << "V1: " << fmesh->pnts[otri.v[1]]->x << "," << fmesh->pnts[otri.v[1]]->y << "," << fmesh->pnts[otri.v[1]]->z << "\n";
		    std::cout << "V2: " << fmesh->pnts[otri.v[2]]->x << "," << fmesh->pnts[otri.v[2]]->y << "," << fmesh->pnts[otri.v[2]]->z << "\n";
		}
		point_type = (ptype > point_type) ? ptype : point_type;
	    }
	    std::cout << "Point/tri characterization: " << point_type << "\n";

	    // Plot point and neighborhood triangles
	    struct bu_vls fname = BU_VLS_INIT_ZERO;
	    bu_vls_sprintf(&fname, "%s_tri_neighborhood-type_%d_%d.plot3", s_cdt->name, point_type, tneigh_cnt);
	    FILE *plot = fopen(bu_vls_cstr(&fname), "w");
	    bu_vls_free(&fname);
	    double fpnt_r = -1.0;
	    int cnt = 0;
	    std::set<tri_dist>::iterator a_it;
	    for (a_it = atris.begin(); a_it != atris.end(); a_it++) {
		if (point_type == 1 && cnt == 1) break;
		if ((point_type == 2 || point_type == 4) && cnt == 2) break;
		if (point_type == 1 && cnt == 3) break;
		//std::cout << "dist: " << a_it->dist << "\n";
		pl_color(plot, 0, 0, 255);
		fmesh->plot_tri(fmesh->tris_vect[a_it->ind], NULL, plot, 0, 0, 0);
		double pnt_r = tri_pnt_r(*fmesh, a_it->ind);
		fpnt_r = (pnt_r > fpnt_r) ? pnt_r : fpnt_r;
		cnt++;
	    }
	    pl_color(plot, 255, 0, 0);
	    plot_pnt_3d(plot, &((pmv)->p), fpnt_r, 0);
	    fclose(plot);
	    tneigh_cnt++;

	    // For now, just assign the point to its closest triangle.  Eventually we'll want to incorporate
	    // the type/edge information above for better triangulations.
	    tri_npnts[atris.begin()->ind].insert(pmv);
	}


	// Points assigned to triangles - retessellate and replace
	std::map<long, std::set<struct p_mvert_info *>>::iterator tp_it;
	for (tp_it = tri_npnts.begin(); tp_it != tri_npnts.end(); tp_it++) {
	    tri_retessellate(fmesh, tp_it->first, tp_it->second);
	}

    }
#endif

    face_ov_cnt = face_omesh_ovlps(ocheck_pairs);
    std::cout << "Post tri split overlap cnt: " << face_ov_cnt << "\n";
    check_faces_validity(check_pairs, 4);

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

