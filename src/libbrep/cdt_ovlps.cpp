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

ON_BoundingBox
edge_bbox(struct ON_Brep_CDT_State *s_cdt, cdt_mesh::cpolyedge_t *pe)
{
    ON_BrepTrim& trim = s_cdt->brep->m_T[pe->trim_ind];
    double tcparam = (pe->trim_start + pe->trim_end) / 2.0;
    ON_3dPoint trim_2d = trim.PointAt(tcparam);
    const ON_Surface *s = trim.SurfaceOf();
    ON_3dPoint trim_3d = s->PointAt(trim_2d.x, trim_2d.y);

    ON_3dPoint *p3d1 = pe->eseg->e_start;
    ON_3dPoint *p3d2 = pe->eseg->e_end;
    ON_Line line(*p3d1, *p3d2);

    double arc_dist = 2*trim_3d.DistanceTo(line.ClosestPointTo(trim_3d));

    ON_BoundingBox bb = line.BoundingBox();
    bb.m_max.x = bb.m_max.x + ON_ZERO_TOLERANCE;
    bb.m_max.y = bb.m_max.y + ON_ZERO_TOLERANCE;
    bb.m_max.z = bb.m_max.z + ON_ZERO_TOLERANCE;
    bb.m_min.x = bb.m_min.x - ON_ZERO_TOLERANCE;
    bb.m_min.y = bb.m_min.y - ON_ZERO_TOLERANCE;
    bb.m_min.z = bb.m_min.z - ON_ZERO_TOLERANCE;

    double dist = p3d1->DistanceTo(*p3d2);
    double bdist = (0.5*dist > arc_dist) ? 0.5*dist : arc_dist;
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

static void
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

static void
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
    return surface_EvNormal(s_cdt->brep->m_F[fmesh.f_id].SurfaceOf(), surf_p2d.x, surf_p2d.y, s_p, s_norm);
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
	    rtree_fmeshes.Search(fMin, fMax, NearFacesCallback, (void *)&nf);
	    // Add the new box to the tree so any additional boxes can check
	    // against it as well
	    rtree_fmeshes.Insert(fMin, fMax, (void *)fmesh);
	}
    }

    return check_pairs;
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

struct mvert_info *
get_largest_mvert(std::set<struct mvert_info *> &verts) 
{
    double elen = 0;
    struct mvert_info *l = NULL;
    std::set<struct mvert_info *>::iterator v_it;
    for (v_it = verts.begin(); v_it != verts.end(); v_it++) {
	struct mvert_info *v = *v_it;
	if (v->e_minlen > elen) {
	    elen = v->e_minlen;
	    l = v;
	}
    }
    return l;
}

struct mvert_info *
closest_mvert(std::set<struct mvert_info *> &verts, struct mvert_info *v) 
{
    struct mvert_info *closest = NULL;
    double dist = DBL_MAX;
    struct ON_Brep_CDT_State *s_cdt1 = v->s_cdt;
    cdt_mesh::cdt_mesh_t fmesh1 = s_cdt1->fmeshes[v->f_id];
    ON_3dPoint p1 = *fmesh1.pnts[v->p_id];
    std::set<struct mvert_info *>::iterator v_it;
    for (v_it = verts.begin(); v_it != verts.end(); v_it++) {
	struct mvert_info *c = *v_it;
	struct ON_Brep_CDT_State *s_cdt2 = c->s_cdt;
	cdt_mesh::cdt_mesh_t fmesh2 = s_cdt2->fmeshes[c->f_id];
	ON_3dPoint p2 = *fmesh2.pnts[c->p_id];
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
void
adjust_mvert_pair(struct mvert_info *v1, struct mvert_info *v2)
{
    struct ON_Brep_CDT_State *s_cdt1 = v1->s_cdt;
    cdt_mesh::cdt_mesh_t &fmesh1 = s_cdt1->fmeshes[v1->f_id];
    struct ON_Brep_CDT_State *s_cdt2 = v2->s_cdt;
    cdt_mesh::cdt_mesh_t &fmesh2 = s_cdt2->fmeshes[v2->f_id];
    ON_3dPoint p1 = *fmesh1.pnts[v1->p_id];
    ON_3dPoint p2 = *fmesh2.pnts[v2->p_id];
    double pdist = p1.DistanceTo(p2);
    ON_Line l(p1,p2);
    // Weight the t parameter on the line so we are closer to the vertex
    // with the shorter edge length (i.e. less freedom to move without
    // introducing locally severe mesh distortions.)
    double t = 1 - (v2->e_minlen / (v1->e_minlen + v2->e_minlen));
    ON_3dPoint p_wavg = l.PointAt(t);
    if ((p1.DistanceTo(p_wavg) > v1->e_minlen*0.5) || (p2.DistanceTo(p_wavg) > v2->e_minlen*0.5)) {
	std::cout << "WARNING: large point shift compared to triangle edge length.\n";
    }
    ON_3dPoint s1_p, s2_p;
    ON_3dVector s1_n, s2_n;
    bool f1_eval = closest_surf_pnt(s1_p, s1_n, fmesh1, &p_wavg, pdist);
    bool f2_eval = closest_surf_pnt(s2_p, s2_n, fmesh2, &p_wavg, pdist);
    if (f1_eval && f2_eval) {
	(*fmesh1.pnts[v1->p_id]) = s1_p;
	(*fmesh1.normals[fmesh1.nmap[v1->p_id]]) = s1_n;
	(*fmesh2.pnts[v2->p_id]) = s2_p;
	(*fmesh2.normals[fmesh2.nmap[v2->p_id]]) = s2_n;

	// We just changed the vertex point values - need to update all the mvert_info
	// edge lengths which might be impacted...
	//double e1 = v1->e_minlen;
	//double e2 = v2->e_minlen;
	mvert_update_all_edge_minlens(v1);
	mvert_update_all_edge_minlens(v2);

#if 0
	std::cout << "p_wavg: " << p_wavg.x << "," << p_wavg.y << "," << p_wavg.z << "\n";
	std::cout << s_cdt1->name << " face " << fmesh1.f_id << " pnt " << v1->p_id << " (elen: " << e1 << "->" << v1->e_minlen << ") moved " << p1.DistanceTo(s1_p) << ": " << p1.x << "," << p1.y << "," << p1.z << " -> " << s1_p.x << "," << s1_p.y << "," << s1_p.z << "\n";
	std::cout << s_cdt2->name << " face " << fmesh2.f_id << " pnt " << v2->p_id << " (elen: " << e2 << "->" << v2->e_minlen << ") moved " << p2.DistanceTo(s2_p) << ": " << p2.x << "," << p2.y << "," << p2.z << " -> " << s2_p.x << "," << s2_p.y << "," << s2_p.z << "\n";
#endif
    } else {
	std::cout << "p_wavg: " << p_wavg.x << "," << p_wavg.y << "," << p_wavg.z << "\n";
	if (!f1_eval) {
	    std::cout << s_cdt1->name << " face " << fmesh1.f_id << " closest point eval failure\n";
	}
	if (!f2_eval) {
	    std::cout << s_cdt2->name << " face " << fmesh2.f_id << " closest point eval failure\n";
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
 


// return the set of verts that was adjusted - we shouldn't need to move them again
std::set<struct mvert_info *>
adjustable_verts(std::set<std::pair<cdt_mesh::cdt_mesh_t *, cdt_mesh::cdt_mesh_t *>> &check_pairs)
{
    // Get the bounding boxes of all vertices of all meshes of all breps
    // that might have possible interactions
    std::vector<struct mvert_info *> all_mverts;
    std::map<std::pair<struct ON_Brep_CDT_State *, int>, RTree<void *, double, 3>> rtrees_mpnts;
    std::map<std::pair<struct ON_Brep_CDT_State *, int>, std::map<long, struct mvert_info *>> mpnt_maps;
    vert_bboxes(&all_mverts, &rtrees_mpnts, &mpnt_maps, check_pairs); 
   
    // Iterate over mverts, checking for nearby pnts in a fashion similar to the
    // NearFacesCallback search above.  For each mvert, note potentially interfering
    // mverts - this will tell us what we need to adjust.
    std::map<struct mvert_info *, std::set<struct mvert_info *>> vert_ovlps;
    std::set<std::pair<cdt_mesh::cdt_mesh_t *, cdt_mesh::cdt_mesh_t *>>::iterator cp_it;
    for (cp_it = check_pairs.begin(); cp_it != check_pairs.end(); cp_it++) {
	cdt_mesh::cdt_mesh_t *fmesh1 = cp_it->first;
	cdt_mesh::cdt_mesh_t *fmesh2 = cp_it->second;
	struct ON_Brep_CDT_State *s_cdt1 = (struct ON_Brep_CDT_State *)fmesh1->p_cdt;
	struct ON_Brep_CDT_State *s_cdt2 = (struct ON_Brep_CDT_State *)fmesh2->p_cdt;
	if (s_cdt1 != s_cdt2) {
	    std::set<std::pair<void *, void *>> vert_pairs;
	    size_t ovlp_cnt = rtrees_mpnts[std::make_pair(s_cdt1,fmesh1->f_id)].Overlaps(rtrees_mpnts[std::make_pair(s_cdt2,fmesh2->f_id)], &vert_pairs);
	    if (ovlp_cnt) {
		struct bu_vls fname = BU_VLS_INIT_ZERO;
		bu_vls_sprintf(&fname, "%s-all_verts_%d.plot3", fmesh1->name, fmesh1->f_id);
		plot_rtree_3d(rtrees_mpnts[std::make_pair(s_cdt1,fmesh1->f_id)], bu_vls_cstr(&fname));
		bu_vls_sprintf(&fname, "%s-all_verts_%d.plot3", fmesh2->name, fmesh2->f_id);
		plot_rtree_3d(rtrees_mpnts[std::make_pair(s_cdt2,fmesh2->f_id)], bu_vls_cstr(&fname));

		std::set<std::pair<void *, void *>>::iterator v_it;
		for (v_it = vert_pairs.begin(); v_it != vert_pairs.end(); v_it++) {
		    struct mvert_info *v_first = (struct mvert_info *)v_it->first;
		    struct mvert_info *v_second = (struct mvert_info *)v_it->second;
		    vert_ovlps[v_first].insert(v_second);
		    vert_ovlps[v_second].insert(v_first);
		}
		bu_vls_free(&fname);
	    }
	}
    }
    std::cout << "Found " << vert_ovlps.size() << " vertices with box overlaps\n";

    std::map<struct mvert_info *, std::set<struct mvert_info *>>::iterator vo_it;
    for (vo_it = vert_ovlps.begin(); vo_it != vert_ovlps.end(); vo_it++) {
	struct bu_vls fname = BU_VLS_INIT_ZERO;
	struct mvert_info *v_curr= vo_it->first;
	bu_vls_sprintf(&fname, "%s-%d-%ld_ovlp_verts.plot3", v_curr->s_cdt->name, v_curr->f_id, v_curr->p_id);
	FILE* plot_file = fopen(bu_vls_cstr(&fname), "w");
	pl_color(plot_file, 0, 0, 255);
	BBOX_PLOT(plot_file, v_curr->bb);
	pl_color(plot_file, 255, 0, 0);
	std::set<struct mvert_info *>::iterator vs_it;
	for (vs_it = vo_it->second.begin(); vs_it != vo_it->second.end(); vs_it++) {
	    struct mvert_info *v_ovlp= *vs_it;
	    BBOX_PLOT(plot_file, v_ovlp->bb);
	}
	fclose(plot_file);
    }

    std::queue<std::pair<struct mvert_info *, struct mvert_info *>> vq;
    std::set<struct mvert_info *> vq_multi;
    for (vo_it = vert_ovlps.begin(); vo_it != vert_ovlps.end(); vo_it++) {
	struct mvert_info *v = vo_it->first;
	if (vo_it->second.size() > 1) {
	    vq_multi.insert(v);
	    continue;
	}
	struct mvert_info *v_other = *vo_it->second.begin();
	if (vert_ovlps[v_other].size() > 1) {
	    // The other point has multiple overlapping points
	    vq_multi.insert(v);
	    continue;
	}
	// Both v and it's companion only have one overlapping point
	vq.push(std::make_pair(v,v_other));
    }

    std::set<struct mvert_info *> adjusted;

    std::cout << "Have " << vq.size() << " simple interactions\n";
    while (!vq.empty()) {
	std::pair<struct mvert_info *, struct mvert_info *> vpair = vq.front();
	vq.pop();

	adjust_mvert_pair(vpair.first, vpair.second);
	adjusted.insert(vpair.first);
	adjusted.insert(vpair.second);
    }

    std::cout << "Have " << vq_multi.size() << " complex interactions\n";
    // If the box structure is more complicated, we need to be a bit selective
    while (vq_multi.size()) {

	struct mvert_info *l = get_largest_mvert(vq_multi);
	struct mvert_info *c = closest_mvert(vert_ovlps[l], l);
	vq_multi.erase(l);
	vq_multi.erase(c);
	vert_ovlps[l].erase(c);
	vert_ovlps[c].erase(l);
	//std::cout << "COMPLEX - adjusting 1 pair only:\n";
	adjust_mvert_pair(l, c);
	adjusted.insert(l);
	adjusted.insert(c);
    }

    return adjusted;
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
}

void
split_brep_face_edges(std::set<std::pair<cdt_mesh::cdt_mesh_t *, cdt_mesh::cdt_mesh_t *>> &check_pairs)
{
    // Get the bounding boxes of all vertices of all meshes of all breps
    // that might have possible interactions, and find close point sets
    std::vector<struct mvert_info *> all_mverts;
    std::map<std::pair<struct ON_Brep_CDT_State *, int>, RTree<void *, double, 3>> rtrees_mpnts;
    std::map<std::pair<struct ON_Brep_CDT_State *, int>, std::map<long, struct mvert_info *>> mpnt_maps;
    vert_bboxes(&all_mverts, &rtrees_mpnts, &mpnt_maps, check_pairs);

    // Iterate over mverts, checking for nearby edges.
    std::map<cdt_mesh::bedge_seg_t *, struct mvert_info *> edge_vert;
    std::set<std::pair<cdt_mesh::cdt_mesh_t *, cdt_mesh::cdt_mesh_t *>>::iterator cp_it;
    struct bu_vls fname = BU_VLS_INIT_ZERO;
    bu_vls_sprintf(&fname, "vert_edge_pairs.plot3");
    FILE* plot_file = fopen(bu_vls_cstr(&fname), "w");
    for (cp_it = check_pairs.begin(); cp_it != check_pairs.end(); cp_it++) {
	cdt_mesh::cdt_mesh_t *fmesh1 = cp_it->first;
	cdt_mesh::cdt_mesh_t *fmesh2 = cp_it->second;
	struct ON_Brep_CDT_State *s_cdt1 = (struct ON_Brep_CDT_State *)fmesh1->p_cdt;
	struct ON_Brep_CDT_State *s_cdt2 = (struct ON_Brep_CDT_State *)fmesh2->p_cdt;
	if (s_cdt1 != s_cdt2) {
	    std::set<std::pair<void *, void *>> vert_edge_pairs;
	    size_t ovlp_cnt = rtrees_mpnts[std::make_pair(s_cdt1,fmesh1->f_id)].Overlaps(s_cdt2->face_rtrees_3d[fmesh2->f_id], &vert_edge_pairs);
	    int used_verts = 0;
	    if (ovlp_cnt && vert_edge_pairs.size()) {
		std::set<std::pair<void *, void *>>::iterator v_it;
		for (v_it = vert_edge_pairs.begin(); v_it != vert_edge_pairs.end(); v_it++) {
		    struct mvert_info *v = (struct mvert_info *)v_it->first;
		    ON_3dPoint p = *fmesh1->pnts[v->p_id];
		    cdt_mesh::cpolyedge_t *pe = (cdt_mesh::cpolyedge_t *)v_it->second;

		    ON_3dPoint *p3d1 = pe->eseg->e_start;
		    ON_3dPoint *p3d2 = pe->eseg->e_end;
		    ON_Line line(*p3d1, *p3d2);
		    double d1 = p3d1->DistanceTo(p);
		    double d2 = p3d2->DistanceTo(p);
		    double dline = 2*p.DistanceTo(line.ClosestPointTo(p));
		    if (d1 > dline && d2 > dline) {
			std::cout << "ACCEPT: d1: " << d1 << ", d2: " << d2 << ", dline: " << dline << "\n";
			if (edge_vert.find(pe->eseg) != edge_vert.end()) {
			    struct ON_Brep_CDT_State *s_cdtv = v->s_cdt;
			    cdt_mesh::cdt_mesh_t fmeshv = s_cdtv->fmeshes[v->f_id];
			    ON_3dPoint pv = *fmeshv.pnts[v->p_id];
			    double dv = pv.DistanceTo(line.ClosestPointTo(pv));
			    if (dv > dline) {
				edge_vert[pe->eseg] = v;
			    }
			} else {
			    edge_vert[pe->eseg] = v;
			    used_verts++;
			}
			pl_color(plot_file, 255, 0, 0);
			BBOX_PLOT(plot_file, v->bb);
			pl_color(plot_file, 0, 0, 255);
			ON_BoundingBox edge_bb = edge_bbox(s_cdt2, pe);
			BBOX_PLOT(plot_file, edge_bb);
		    } else {
			std::cout << "REJECT: d1: " << d1 << ", d2: " << d2 << ", dline: " << dline << "\n";
		    }
		}
		std::cout << "used_verts: " << used_verts << "\n";
	    }
	}
    }
    fclose(plot_file);


    // 2.  Find the point on the edge nearest to the vert point.  (TODO - need to think about how to
    // handle multiple verts associated with same edge - may want to iterate starting with the closest
    // and see if splitting clears the others...)
    std::map<cdt_mesh::bedge_seg_t *, struct mvert_info *>::iterator ev_it;
    for (ev_it = edge_vert.begin(); ev_it != edge_vert.end(); ev_it++) {
	cdt_mesh::bedge_seg_t *eseg = ev_it->first;
	struct mvert_info *v = ev_it->second;

	ON_NurbsCurve *nc = eseg->nc;
	ON_Interval domain(eseg->edge_start, eseg->edge_end);
	struct ON_Brep_CDT_State *s_cdt = v->s_cdt;
	cdt_mesh::cdt_mesh_t &fmesh = s_cdt->fmeshes[v->f_id];
	ON_3dPoint p = *fmesh.pnts[v->p_id];
	double t;
	ON_NurbsCurve_GetClosestPoint(&t, nc, p, 0.0, &domain);
	ON_3dPoint cep = nc->PointAt(t);
	std::cout << "cep: " << cep.x << "," << cep.y << "," << cep.z << "\n";
	std::cout << "Distance: " << cep.DistanceTo(p) << "\n";
	double epdist1 = eseg->e_start->DistanceTo(cep);
	double epdist2 = eseg->e_end->DistanceTo(cep);
	double lseg_check = 0.1 * eseg->e_start->DistanceTo(*eseg->e_end);
	std::cout << "d1: " << epdist1 << ", d2: " << epdist2 << ", lseg_check: " << lseg_check << "\n";
	if (epdist1 > lseg_check && epdist2 > lseg_check) {
	    // If the point is not close to a start/end point on the edge then split the edge, replace old tris
	    // with new tris in both faces, add add new mvert_info point.  Distribute any existing verts still
	    // in the associated set with the old edge to the new edges, based on which one they're closer to.

	    // 1.  Get the triangle from each face associated with the edge segment to be split (should only
	    // be one per face, since we're working with boundary edges.)
	    struct ON_Brep_CDT_State *s_cdt_edge = (struct ON_Brep_CDT_State *)eseg->p_cdt;
	    int f_id1 = s_cdt_edge->brep->m_T[eseg->tseg1->trim_ind].Face()->m_face_index;
	    int f_id2 = s_cdt_edge->brep->m_T[eseg->tseg2->trim_ind].Face()->m_face_index;
	    cdt_mesh::cdt_mesh_t &fmesh_f1 = s_cdt_edge->fmeshes[f_id1];
	    cdt_mesh::cdt_mesh_t &fmesh_f2 = s_cdt_edge->fmeshes[f_id2];
	    // Translate the tseg verts to their parent indices in order to get
	    // valid triangle lookups
	    cdt_mesh::cpolygon_t *poly1 = eseg->tseg1->polygon;
	    cdt_mesh::cpolygon_t *poly2 = eseg->tseg2->polygon;
	    cdt_mesh::uedge_t ue1(poly1->p2o[eseg->tseg1->v[0]], poly1->p2o[eseg->tseg1->v[1]]);
	    cdt_mesh::uedge_t ue2(poly2->p2o[eseg->tseg2->v[0]], poly2->p2o[eseg->tseg2->v[1]]);
	    ON_3dPoint ue1_p1 = *fmesh_f1.pnts[ue1.v[0]];
	    ON_3dPoint ue1_p2 = *fmesh_f1.pnts[ue1.v[1]];
	    std::cout << f_id1 << " ue1: " << ue1.v[0] << "," << ue1.v[1] << ": " << ue1_p1.x << "," << ue1_p1.y << "," << ue1_p1.z << " -> " << ue1_p2.x << "," << ue1_p2.y << "," << ue1_p2.z << "\n";
	    ON_3dPoint ue2_p1 = *fmesh_f2.pnts[ue2.v[0]];
	    ON_3dPoint ue2_p2 = *fmesh_f2.pnts[ue2.v[1]];
	    std::cout << f_id2 << " ue2: " << ue2.v[0] << "," << ue2.v[1] << ": " << ue2_p1.x << "," << ue2_p1.y << "," << ue2_p1.z << " -> " << ue2_p2.x << "," << ue2_p2.y << "," << ue2_p2.z << "\n";
	    std::set<size_t> f1_tris = fmesh_f1.uedges2tris[ue1];
	    std::set<size_t> f2_tris = fmesh_f2.uedges2tris[ue2];
	    if (f_id1 == f_id2) {
		std::set<size_t> ftris;
		std::set<size_t>::iterator tr_it;
		cdt_mesh::uedge_t ue;
		ue = (f1_tris.size()) ? ue1 : ue2;
		ftris.insert(f1_tris.begin(), f1_tris.end());
		ftris.insert(f2_tris.begin(), f2_tris.end());
		std::set<cdt_mesh::bedge_seg_t *> esegs_split = split_edge_seg(s_cdt_edge, eseg, 1, &t, 1);
		if (ftris.size() != 2) {
		    std::cout << "edge is only on 1 face, but don't have 2 tri??: " << ftris.size() << "\n";
		} else {
		    long np_id = fmesh_f1.pnts.size() - 1;
		    for (tr_it = ftris.begin(); tr_it != ftris.end(); tr_it++) {
			replace_edge_split_tri(fmesh_f1, *tr_it, np_id, ue);
		    }
		}
	    } else {
		std::set<cdt_mesh::bedge_seg_t *> esegs_split = split_edge_seg(s_cdt_edge, eseg, 1, &t, 1);
		if (f1_tris.size() != 1) {
		    std::cout << "don't have 1 tri??: " << f1_tris.size() << "\n";
		} else {
		    long np_id = fmesh_f1.pnts.size() - 1;
		    replace_edge_split_tri(fmesh_f1, *f1_tris.begin(), np_id, ue1);
		}

		if (f2_tris.size() != 1) {
		    std::cout << "don't have 1 tri??: " << f2_tris.size() << "\n";
		} else {
		    long np_id = fmesh_f2.pnts.size() - 1;
		    replace_edge_split_tri(fmesh_f2, *f2_tris.begin(), np_id, ue2);
		}

	    }
	    struct bu_vls fename = BU_VLS_INIT_ZERO;
	    bu_vls_sprintf(&fename, "%s-%d_post_edge_tris.plot3", s_cdt_edge->name, fmesh_f1.f_id);
	    fmesh_f1.tris_plot(bu_vls_cstr(&fename));
	    bu_vls_sprintf(&fename, "%s-%d_post_edge_tris.plot3", s_cdt_edge->name, fmesh_f2.f_id);
	    fmesh_f2.tris_plot(bu_vls_cstr(&fename));
	    bu_vls_free(&fename);
	}
    }
}

/**************************************************************************
 * TODO - implement the various ways to refine a triangle polygon.
 **************************************************************************/

cdt_mesh::cpolygon_t *
tri_refine_polygon(cdt_mesh::cdt_mesh_t &fmesh, cdt_mesh::triangle_t &t)
{
    struct ON_Brep_CDT_State *s_cdt = (struct ON_Brep_CDT_State *)fmesh.p_cdt;
    cdt_mesh::cpolygon_t *polygon = new cdt_mesh::cpolygon_t;


    // Add triangle center point, from the 2D center point of the triangle
    // points (this is where p3d2d is needed).  Singularities are going to be a
    // particular problem. We'll probably NEED the closest 2D point to the 3D
    // triangle center point in that case, but for now just find the shortest
    // distance from the midpoint of the 2 non-singular points to the singular
    // trim and insert the new point some fraction of that distance off of the
    // midpoint towards the singularity. For other cases let's try the 2D
    // center point and see how it works - we may need the all-up closest
    // point calculation there too...
    ON_2dPoint p2d[3] = {ON_2dPoint::UnsetPoint, ON_2dPoint::UnsetPoint, ON_2dPoint::UnsetPoint};
    bool have_singular_vert = false;
    for (int i = 0; i < 3; i++) {
	if (fmesh.sv.find(t.v[i]) != fmesh.sv.end()) {
	    have_singular_vert = true;
	}
    }

    if (!have_singular_vert) {

	// Find the 2D center point.  NOTE - this definitely won't work
	// if the point is trimmed in the parent brep - really need to
	// try the GetClosestPoint routine...  may also want to do this after
	// the edge split?
	for (int i = 0; i < 3; i++) {
	    p2d[i] = ON_2dPoint(fmesh.m_pnts_2d[fmesh.p3d2d[t.v[i]]].first, fmesh.m_pnts_2d[fmesh.p3d2d[t.v[i]]].second);
	}
	ON_2dPoint cpnt = p2d[0];
	for (int i = 1; i < 3; i++) {
	    cpnt += p2d[i];
	}
	cpnt = cpnt/3.0;

	// Calculate the 3D point and normal values.
	ON_3dPoint p3d;
	ON_3dVector norm = ON_3dVector::UnsetVector;
	if (!surface_EvNormal(s_cdt->brep->m_F[fmesh.f_id].SurfaceOf(), cpnt.x, cpnt.y, p3d, norm)) {
	    p3d = s_cdt->brep->m_F[fmesh.f_id].SurfaceOf()->PointAt(cpnt.x, cpnt.y);
	}

	long f_ind2d = fmesh.add_point(cpnt);
	fmesh.m_interior_pnts.insert(f_ind2d);
	if (fmesh.m_bRev) {
	    norm = -1 * norm;
	}
	long f3ind = fmesh.add_point(new ON_3dPoint(p3d));
	long fnind = fmesh.add_normal(new ON_3dPoint(norm));

        CDT_Add3DPnt(s_cdt, fmesh.pnts[fmesh.pnts.size()-1], fmesh.f_id, -1, -1, -1, cpnt.x, cpnt.y);
        CDT_Add3DNorm(s_cdt, fmesh.normals[fmesh.normals.size()-1], fmesh.pnts[fmesh.pnts.size()-1], fmesh.f_id, -1, -1, -1, cpnt.x, cpnt.y);
	fmesh.p2d3d[f_ind2d] = f3ind;
	fmesh.p3d2d[f3ind] = f_ind2d;
	fmesh.nmap[f3ind] = fnind;

	// As we do in the repair, project all the mesh points to the plane and
	// add them so the point indices are the same.  Eventually we can be
	// more sophisticated about this, but for now it avoids potential
	// bookkeeping problems.
	ON_3dPoint sp = fmesh.tcenter(t);
	ON_3dVector sn = fmesh.bnorm(t);
	ON_Plane tri_plane(sp, sn);
	for (size_t i = 0; i < fmesh.pnts.size(); i++) {
	    double u, v;
	    ON_3dPoint op3d = (*fmesh.pnts[i]);
	    tri_plane.ClosestPointTo(op3d, &u, &v);
	    std::pair<double, double> proj_2d;
	    proj_2d.first = u;
	    proj_2d.second = v;
	    polygon->pnts_2d.push_back(proj_2d);
	    if (fmesh.brep_edge_pnt(i)) {
		polygon->brep_edge_pnts.insert(i);
	    }
	    polygon->p2o[i] = i;
	}
	struct cdt_mesh::edge_t e1(t.v[0], t.v[1]);
	struct cdt_mesh::edge_t e2(t.v[1], t.v[2]);
	struct cdt_mesh::edge_t e3(t.v[2], t.v[0]);
	polygon->add_edge(e1);
	polygon->add_edge(e2);
	polygon->add_edge(e3);

	// Let the polygon know it's got an interior (center) point.  We won't
	// do the cdt yet, because we may need to also split an edge
	polygon->interior_points.insert(f3ind);

    } else {
	// TODO - have singular vertex
	std::cout << "singular vertex in triangle\n";
    }

    return polygon;
}


static void
refine_ovlp_tris(struct ON_Brep_CDT_State *s_cdt, int face_index)
{
    std::map<int, std::set<size_t>>::iterator m_it;
    cdt_mesh::cdt_mesh_t &fmesh = s_cdt->fmeshes[face_index];
    std::set<size_t> &tri_inds = s_cdt->face_ovlp_tris[face_index];
    std::set<size_t>::iterator t_it;
    for (t_it = tri_inds.begin(); t_it != tri_inds.end(); t_it++) {
	cdt_mesh::triangle_t tri = fmesh.tris_vect[*t_it];
	bool have_face_edge = false;
	cdt_mesh::uedge_t ue;
	cdt_mesh::uedge_t t_ue[3];
	t_ue[0].set(tri.v[0], tri.v[1]);
	t_ue[1].set(tri.v[1], tri.v[2]);
	t_ue[2].set(tri.v[2], tri.v[0]);
	for (int i = 0; i < 3; i++) {
	    if (fmesh.brep_edges.find(t_ue[i]) != fmesh.brep_edges.end()) {
		ue = t_ue[i];
		have_face_edge = true;
		break;
	    }
	}

	// TODO - yank the overlapping tri and split, either with center point
	// only (SURF_TRI) or shared edge and center (EDGE_TRI).  The EDGE_TRI
	// case will require a matching split from the triangle on the opposite
	// face to maintain watertightness, and it may be convenient just to do
	// make the polygon, insert a steiner point (plus splitting the polygon
	// edge in the edge tri case) and do a mini-CDT to make the new
	// triangles (similar to the cdt_mesh repair operation's replacment for
	// bad patches).  However, if we end up with any colinearity issues we
	// might just fall back on manually constructing the three (or 5 in the
	// edge case) triangles rather than risk a CDT going wrong.
	//
	// It may make sense, depending on the edge triangle's shape, to ONLY
	// split the edge in some situations.  However, we dont' want to get
	// into a situation where we keep refining the edge and we end up with
	// a whole lot of crazy-slim triangles going to one surface point...
	cdt_mesh::cpolygon_t *polygon = tri_refine_polygon(fmesh, tri);

	if (have_face_edge) {
	    //std::cout << "EDGE_TRI: refining " << s_cdt->name << " face " << fmesh.f_id << " tri " << *t_it << "\n";
	} else {
	    //std::cout << "SURF_TRI: refining " << s_cdt->name << " face " << fmesh.f_id << " tri " << *t_it << "\n";
	}

	if (!polygon) {
	    std::cout << "Error - couldn't build polygon loop for triangle refinement\n";
	}
    }
}

/**************************************************************************
 * TODO - we're going to need near-edge awareness, but not sure yet in what
 * form.
 **************************************************************************/

static bool NearEdgesCallback(void *data, void *a_context) {
    std::set<cdt_mesh::cpolyedge_t *> *edges = (std::set<cdt_mesh::cpolyedge_t *> *)a_context;
    cdt_mesh::cpolyedge_t *pe  = (cdt_mesh::cpolyedge_t *)data;
    edges->insert(pe);
    return true;
}

void edge_check(struct brep_face_ovlp_instance *ovlp) {

    cdt_mesh::cdt_mesh_t &fmesh = ovlp->intersected_tri_s_cdt->fmeshes[ovlp->intersected_tri_face_ind];
    cdt_mesh::triangle_t tri = fmesh.tris_vect[ovlp->intersected_tri_ind];
    double fMin[3]; double fMax[3];
    ON_3dPoint *p3d = fmesh.pnts[tri.v[0]];
    ON_BoundingBox bb1(*p3d, *p3d);
    for (int i = 1; i < 3; i++) {
	p3d = fmesh.pnts[tri.v[i]];
	bb1.Set(*p3d, true);
    }
    fMin[0] = bb1.Min().x;
    fMin[1] = bb1.Min().y;
    fMin[2] = bb1.Min().z;
    fMax[0] = bb1.Max().x;
    fMax[1] = bb1.Max().y;
    fMax[2] = bb1.Max().z;
    size_t nhits = ovlp->intersected_tri_s_cdt->face_rtrees_3d[fmesh.f_id].Search(fMin, fMax, NearEdgesCallback, (void *)&ovlp->involved_edge_segs);
    if (nhits) {
	//std::cout << "Face " << fmesh.f_id << " tri " << tri.ind << " has potential edge curve interaction\n";
    }
}

int
ON_Brep_CDT_Ovlp_Resolve(struct ON_Brep_CDT_State **s_a, int s_cnt)
{
    if (!s_a) return -1;
    if (s_cnt < 1) return 0;

    // Get the bounding boxes of all faces of all breps in s_a, and find
    // possible interactions
    std::set<std::pair<cdt_mesh::cdt_mesh_t *, cdt_mesh::cdt_mesh_t *>> check_pairs;
    check_pairs = possibly_interfering_face_pairs(s_a, s_cnt);

    std::cout << "Found " << check_pairs.size() << " potentially interfering face pairs\n";
    std::set<struct mvert_info *> adjusted_verts = adjustable_verts(check_pairs);

    if (adjusted_verts.size()) {
	std::cout << "Adjusted " << adjusted_verts.size() << " vertices\n";
    }

    split_brep_face_edges(check_pairs);

    std::set<std::pair<cdt_mesh::cdt_mesh_t *, cdt_mesh::cdt_mesh_t *>>::iterator cp_it;
    for (cp_it = check_pairs.begin(); cp_it != check_pairs.end(); cp_it++) {
	cdt_mesh::cdt_mesh_t *fmesh1 = cp_it->first;
	cdt_mesh::cdt_mesh_t *fmesh2 = cp_it->second;
	struct ON_Brep_CDT_State *s_cdt1 = (struct ON_Brep_CDT_State *)fmesh1->p_cdt;
	struct ON_Brep_CDT_State *s_cdt2 = (struct ON_Brep_CDT_State *)fmesh2->p_cdt;
	if (s_cdt1 != s_cdt2) {
	    std::set<std::pair<size_t, size_t>> tris_prelim;
	    std::map<int, std::set<cdt_mesh::cpolyedge_t *>> tris_to_opp_face_edges_1;
	    std::map<int, std::set<cdt_mesh::cpolyedge_t *>> tris_to_opp_face_edges_2;
	    size_t ovlp_cnt = fmesh1->tris_tree.Overlaps(fmesh2->tris_tree, &tris_prelim);
	    if (ovlp_cnt) {
		//std::cout << "Checking " << fmesh1->name << " face " << fmesh1->f_id << " against " << fmesh2->name << " face " << fmesh2->f_id << " found " << ovlp_cnt << " box overlaps\n";
		std::set<std::pair<size_t, size_t>>::iterator tb_it;
		for (tb_it = tris_prelim.begin(); tb_it != tris_prelim.end(); tb_it++) {
		    cdt_mesh::triangle_t t1 = fmesh1->tris_vect[tb_it->first];
		    cdt_mesh::triangle_t t2 = fmesh2->tris_vect[tb_it->second];
		    point_t isectpt1 = {MAX_FASTF, MAX_FASTF, MAX_FASTF};
		    point_t isectpt2 = {MAX_FASTF, MAX_FASTF, MAX_FASTF};
		    int isect = tri_isect(fmesh1, t1, fmesh2, t2, &isectpt1, &isectpt2);
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
				//std::cout << "face " << fmesh1->f_id << " new interior point from face " << fmesh2->f_id << ": " << tp.x << "," << tp.y << "," << tp.z << "\n";
				struct brep_face_ovlp_instance *ovlp = new struct brep_face_ovlp_instance;

				VMOVE(ovlp->isect1_3d, isectpt1);
				VMOVE(ovlp->isect2_3d, isectpt2);

				ovlp->intruding_pnt_s_cdt = s_cdt2;
				ovlp->intruding_pnt_face_ind = fmesh2->f_id;
				ovlp->intruding_pnt_tri_ind = t2.ind;
				ovlp->intruding_pnt = t2.v[i];

				ovlp->intersected_tri_s_cdt = s_cdt1;
				ovlp->intersected_tri_face_ind = fmesh1->f_id;
				ovlp->intersected_tri_ind = t1.ind;

				ovlp->coplanar_intersection = (isect == 1) ? true : false;
				s_cdt1->face_ovlps[fmesh1->f_id].push_back(ovlp);
				s_cdt1->face_ovlp_tris[fmesh1->f_id].insert(t1.ind);
				s_cdt1->face_tri_ovlps[fmesh1->f_id][t1.ind].insert(ovlp);
			    }
			}

			ON_Plane plane2 = fmesh2->tplane(t2);
			for (int i = 0; i < 3; i++) {
			    ON_3dPoint tp = *fmesh1->pnts[t1.v[i]];
			    double dist = plane2.DistanceTo(tp);
			    if (dist < 0 && fabs(dist) > ON_ZERO_TOLERANCE) {
				//std::cout << "face " << fmesh2->f_id << " new interior point from face " << fmesh1->f_id << ": " << tp.x << "," << tp.y << "," << tp.z << "\n";
				fmesh1_interior_pnts.insert(t1.v[i]);
				struct brep_face_ovlp_instance *ovlp = new struct brep_face_ovlp_instance;

				VMOVE(ovlp->isect1_3d, isectpt1);
				VMOVE(ovlp->isect2_3d, isectpt2);

				ovlp->intruding_pnt_s_cdt = s_cdt1;
				ovlp->intruding_pnt_face_ind = fmesh1->f_id;
				ovlp->intruding_pnt_tri_ind = t1.ind;
				ovlp->intruding_pnt = t1.v[i];

				ovlp->intersected_tri_s_cdt = s_cdt2;
				ovlp->intersected_tri_face_ind = fmesh2->f_id;
				ovlp->intersected_tri_ind = t2.ind;

				ovlp->coplanar_intersection = (isect == 1) ? true : false;
				s_cdt2->face_ovlps[fmesh2->f_id].push_back(ovlp);
				s_cdt2->face_ovlp_tris[fmesh2->f_id].insert(t2.ind);
				s_cdt2->face_tri_ovlps[fmesh2->f_id][t2.ind].insert(ovlp);
			    }
			}

		    }
		}
	    } else {
		//std::cout << "RTREE_ISECT_EMPTY: " << fmesh1->name << " face " << fmesh1->f_id << " and " << fmesh2->name << " face " << fmesh2->f_id << "\n";
	    }
	} else {
	    // TODO: In principle we should be checking for self intersections
	    // - it can happen, particularly in sparse tessellations. That's
	    // why the above doesn't filter out same-object face overlaps, but
	    // for now ignore it.  We need to be able to ignore triangles that
	    // only share a 3D edge.
	    //std::cout << "SELF_ISECT\n";
	}
    }

    for (int i = 0; i < s_cnt; i++) {
	struct ON_Brep_CDT_State *s_i = s_a[i];
	for (int i_fi = 0; i_fi < s_i->brep->m_F.Count(); i_fi++) {
	    if (s_i->face_ovlps[i_fi].size()) {
		std::cout << s_i->name << " face " << i_fi << " overlap instance cnt " << s_i->face_ovlps[i_fi].size() << "\n";
		plot_ovlps(s_i, i_fi);
		for (size_t j = 0; j < s_i->face_ovlps[i_fi].size(); j++) {
		    edge_check(s_i->face_ovlps[i_fi][j]);
		}
		refine_ovlp_tris(s_i, i_fi);
	    }
	}

	std::map<size_t, std::set<struct brep_face_ovlp_instance *>>::iterator to_it;
	for (int i_fi = 0; i_fi < s_i->brep->m_F.Count(); i_fi++) {
	    cdt_mesh::cdt_mesh_t &cmesh = s_i->fmeshes[i_fi];
	    for (to_it = s_i->face_tri_ovlps[i_fi].begin(); to_it != s_i->face_tri_ovlps[i_fi].end(); to_it++) {
		std::set<struct brep_face_ovlp_instance *>::iterator o_it;
		std::set<ON_3dPoint *> face_pnts;
		for (o_it = to_it->second.begin(); o_it != to_it->second.end(); o_it++) {
		    struct brep_face_ovlp_instance *ovlp = *o_it;
		    ON_3dPoint *p = ovlp->intruding_pnt_s_cdt->fmeshes[ovlp->intruding_pnt_face_ind].pnts[ovlp->intruding_pnt];
		    face_pnts.insert(p);
		}

		if (face_pnts.size()) {
		    //std::cout << s_i->name << " face " << i_fi << " triangle " << to_it->first << " interior point cnt: " << face_pnts.size() << "\n";
		    std::set<ON_3dPoint *>::iterator fp_it;
		    for (fp_it = face_pnts.begin(); fp_it != face_pnts.end(); fp_it++) {
			//std::cout << "       " << (*fp_it)->x << "," << (*fp_it)->y << "," << (*fp_it)->z << "\n";
		    }
		    struct bu_vls fname = BU_VLS_INIT_ZERO;
		    bu_vls_sprintf(&fname, "%s_%d_%ld_tri.plot3", s_i->name, i_fi, to_it->first);
		    FILE* plot_file = fopen(bu_vls_cstr(&fname), "w");

		    pl_color(plot_file, 0, 0, 255);
		    cmesh.plot_tri(cmesh.tris_vect[to_it->first], NULL, plot_file, 0, 0, 0);
		    double pnt_r = tri_pnt_r(cmesh, to_it->first);
		    for (fp_it = face_pnts.begin(); fp_it != face_pnts.end(); fp_it++) {
			pl_color(plot_file, 255, 0, 0);
			plot_pnt_3d(plot_file, *fp_it, pnt_r, 0);
		    }
		    fclose(plot_file);

		    bu_vls_sprintf(&fname, "%s_%d_%ld_ovlps.plot3", s_i->name, i_fi, to_it->first);
		    FILE* plot_file_2 = fopen(bu_vls_cstr(&fname), "w");
		    for (o_it = to_it->second.begin(); o_it != to_it->second.end(); o_it++) {
			struct brep_face_ovlp_instance *ovlp = *o_it;
			cdt_mesh::cdt_mesh_t &imesh = ovlp->intruding_pnt_s_cdt->fmeshes[ovlp->intruding_pnt_face_ind];
			cdt_mesh::triangle_t i_tri = imesh.tris_vect[ovlp->intruding_pnt_tri_ind];
			pl_color(plot_file_2, 0, 255, 0);
			imesh.plot_tri(i_tri, NULL, plot_file_2, 0, 0, 0);
			ON_3dPoint p1(ovlp->isect1_3d[X], ovlp->isect1_3d[Y], ovlp->isect1_3d[Z]);
			ON_3dPoint p2(ovlp->isect2_3d[X], ovlp->isect2_3d[Y], ovlp->isect2_3d[Z]);
			pl_color(plot_file_2, 255, 255, 0);
			ON_3dPoint pavg = (p1+p2)*0.5;
			/* 
			   plot_pnt_3d(plot_file_2, &p1, pnt_r, 1);
			   plot_pnt_3d(plot_file_2, &p2, pnt_r, 1);
			   */
			//ON_3dPoint p1s = closest_surf_pnt(cmesh, &p1);
			//ON_3dPoint p2s = closest_surf_pnt(cmesh, &p2);
			ON_3dPoint p1s;
			ON_3dVector p1norm;
		       	closest_surf_pnt(p1s, p1norm, cmesh, &pavg, 0);
			pl_color(plot_file_2, 0, 255, 255);
			plot_pnt_3d(plot_file_2, &p1s, pnt_r, 1);
			//plot_pnt_3d(plot_file_2, &p2s, pnt_r, 1);
		    }
		    fclose(plot_file_2);
		}

		// TODO - surface_GetClosestPoint3dFirstOrder and trim_GetClosestPoint3dFirstOrder look like the
		// places to start.  Need to see if we can make a copy of the face surface and replace its
		// loops with the 2D triangle edges as the outer loop to get the closed point on the triangle...
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

