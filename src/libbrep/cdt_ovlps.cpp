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
 * 1.  set up get-closest-point calculation options, either using the all-up surface gcp or projections on triangles
 * depending on what looks to be necessary/practical.
 *
 * 2.  Visualize the gcp results in 2D and 3D.
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
 * 5.  Do a categorization pass as outlined below so we know what specifically we need to do with each triangle.
 *
 * 6.  Set up the replacement CDT problems and update the mesh.  Identify termination criteria and check for them.
 */

#include "common.h"
#include <queue>
#include <numeric>
#include "bg/chull.h"
#include "bg/tri_tri.h"
#include "./cdt.h"

static void
plot_ovlp(struct brep_face_ovlp_instance *ovlp, FILE *plot)
{
    if (!ovlp) return;
    cdt_mesh::cdt_mesh_t &imesh = ovlp->intruding_pnt_s_cdt->fmeshes[ovlp->intruding_pnt_face_ind];
    cdt_mesh::cdt_mesh_t &cmesh = ovlp->intersected_tri_s_cdt->fmeshes[ovlp->intersected_tri_face_ind];
    cdt_mesh::triangle_t i_tri = imesh.tris_vect[ovlp->intruding_pnt_tri_ind];
    cdt_mesh::triangle_t c_tri = cmesh.tris_vect[ovlp->intersected_tri_ind];
    ON_3dPoint *i_p = imesh.pnts[ovlp->intruding_pnt];

    ON_3dPoint *p3d = imesh.pnts[i_tri.v[0]];
    ON_BoundingBox bb1(*p3d, *p3d);
    for (int i = 1; i < 3; i++) {
	p3d = imesh.pnts[i_tri.v[i]];
	bb1.Set(*p3d, true);
    }
    double bb1d = bb1.Diagonal().Length();
    p3d = cmesh.pnts[c_tri.v[0]];
    ON_BoundingBox bb2(*p3d, *p3d);
    for (int i = 1; i < 3; i++) {
	p3d = cmesh.pnts[c_tri.v[i]];
	bb2.Set(*p3d, true);
    }
    double bb2d = bb2.Diagonal().Length();
    double pnt_r = (bb1d < bb2d) ? bb1d * 0.05 : bb2d * 0.05;

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
	    //std::cout << "skipping pnt isect(" << coplanar << "): " << isectpt1[X] << "," << isectpt1[Y] << "," << isectpt1[Z] << "\n";
	    return 0;
	}

	return (coplanar) ? 1 : 2;
    }

    return 0;
}

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
	    std::cout << "EDGE_TRI: refining " << s_cdt->name << " face " << fmesh.f_id << " tri " << *t_it << "\n";
	} else {
	    std::cout << "SURF_TRI: refining " << s_cdt->name << " face " << fmesh.f_id << " tri " << *t_it << "\n";
	}

	if (!polygon) {
	    std::cout << "Error - couldn't build polygon loop for triangle refinement\n";
	}
    }
}


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
int
ON_Brep_CDT_Ovlp_Resolve(struct ON_Brep_CDT_State **s_a, int s_cnt)
{
    if (!s_a) return -1;
    if (s_cnt < 1) return 0;

    // Get the bounding boxes of all faces of all breps in s_a, and find
    // possible interactions
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

    std::cout << "Found " << check_pairs.size() << " potentially interfering face pairs\n";
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
		std::cout << "Checking " << fmesh1->name << " face " << fmesh1->f_id << " against " << fmesh2->name << " face " << fmesh2->f_id << " found " << ovlp_cnt << " box overlaps\n";
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
				//std::cout << "face " << fmesh1->f_id << " new interior point from face " << fmesh2->f_id << ": " << tp.x << "," << tp.y << "," << tp.z << "\n";
				struct brep_face_ovlp_instance *ovlp = new struct brep_face_ovlp_instance;
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
	    std::cout << "SELF_ISECT\n";
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
    }

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

