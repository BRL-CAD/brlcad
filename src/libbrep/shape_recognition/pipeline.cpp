/*                    P I P E L I N E . C P P
 * BRL-CAD
 *
 * Copyright (c) 2020-2022 United States Government as represented by
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
/** @file pipeline.cpp
 *
 * Brief description
 *
 */

#include "common.h"

#include <set>
#include <map>
#include <limits>

#include "bu/log.h"
#include "bu/str.h"
#include "bu/malloc.h"
#include "shape_recognition.h"

#define L3_OFFSET 6
#define L4_OFFSET 8

int
shoal_csg(struct bu_vls *msgs, surface_t surface_type, struct subbrep_shoal_data *data)
{
    //bu_log("shoal processing %s\n", bu_vls_addr(data->i->key));
    int nonlinear_edge = -1;
    int implicit_plane_ind = -1;
    std::set<int> nonplanar_surfaces;
    std::set<int>::iterator c_it;

    const ON_Brep *brep = data->i->brep;

    // If we hit something we don't handle yet, bail immediately.
    switch (surface_type) {
	case SURFACE_SPHERICAL_SECTION:
	case SURFACE_SPHERE:
	    return 0;
	    break;
	case SURFACE_TORUS:
	    return 0;
	    break;
	default:
	    break;
    }

    // Collect faces, edges and edge midpoints.
    //
    // TODO - edge midpoints are not enough in situations (like certain sphereical faces)
    // where there *is* no edge that is not coplanar with the connecting loop.  Need to
    // construct medial axis of loop in 2D space and get the corresponding surface points
    // to ensure reliable orientation of planar faces...
    //
    // see http://www.cs.unc.edu/~snoeyink/papers/medaxis.ps.gz
    //
    ON_SimpleArray<ON_3dPoint> edge_midpnts;
    std::set<int> edges;
    for (int i = 0; i < data->shoal_loops_cnt; i++) {
	const ON_BrepLoop *loop = &(brep->m_L[data->shoal_loops[i]]);
	nonplanar_surfaces.insert(loop->Face()->m_face_index);
	for (int ti = 0; ti < loop->m_ti.Count(); ti++) {
	    const ON_BrepTrim *trim = &(brep->m_T[loop->m_ti[ti]]);
	    const ON_BrepEdge *edge = &(brep->m_E[trim->m_ei]);
	    if (trim->m_ei != -1 && edge->TrimCount() > 0) {
		edges.insert(trim->m_ei);
		ON_3dPoint midpt = edge->EdgeCurveOf()->PointAt(edge->EdgeCurveOf()->Domain().Mid());
		edge_midpnts.Append(midpt);
	    }
	}
    }


    // Collect edges that link to one plane in the shoal.  Two non-planar faces
    // both in the shoal means a culled edge and verts
    std::set<int> nondegen_edges;
    std::set<int> degen_edges;
    for (c_it = edges.begin(); c_it != edges.end(); c_it++) {
	const ON_BrepEdge *edge = &(brep->m_E[*c_it]);
	int face_cnt = 0;
	for (int i = 0; i < edge->m_ti.Count(); i++) {
	    const ON_BrepTrim *trim = &(brep->m_T[edge->m_ti[i]]);
	    if (((surface_t *)data->i->face_surface_types)[trim->Face()->m_face_index] == SURFACE_PLANE) continue;
	    if (nonplanar_surfaces.find(trim->Face()->m_face_index) != nonplanar_surfaces.end())
		face_cnt++;
	}
	if (face_cnt == 2) {
	    degen_edges.insert(*c_it);
	    //bu_log("edge %d is degenerate\n", *s_it);
	} else {
	    nondegen_edges.insert(*c_it);
	}
    }

    // Update the island's info on degenerate edges and vertices
    std::set<int> nullv;
    std::set<int> nulle;
    array_to_set(&nullv, data->i->null_verts, data->i->null_vert_cnt);
    array_to_set(&nulle, data->i->null_edges, data->i->null_edge_cnt);
    for (c_it = degen_edges.begin(); c_it != degen_edges.end(); c_it++) {
	const ON_BrepEdge *edge = &(brep->m_E[*c_it]);
	nullv.insert(edge->Vertex(0)->m_vertex_index);
	nullv.insert(edge->Vertex(1)->m_vertex_index);
	nulle.insert(*c_it);
    }
    set_to_array(&(data->i->null_verts), &(data->i->null_vert_cnt), &nullv);
    set_to_array(&(data->i->null_edges), &(data->i->null_edge_cnt), &nulle);


    // Now, any non-linear edge that isn't degenerate should be supplying a
    // plane.  (We don't currently handle non-planar edges like those from a
    // sphere subtracted from a cylinder.) Liner curves are saved - they need
    // special interpretation for cylinders and cones (spheres shouldn't have
    // any.)
    ON_SimpleArray<ON_Plane> non_linear_edge_planes;
    std::set<int> linear_edges;
    std::set<int> nonlinear_edges;
    for (c_it = nondegen_edges.begin(); c_it != nondegen_edges.end(); c_it++) {
	const ON_BrepEdge *edge = &(brep->m_E[*c_it]);
	ON_Curve *ecv = edge->EdgeCurveOf()->Duplicate();
	if (!ecv->IsLinear()) {
	    if (nonlinear_edge == -1) nonlinear_edge = edge->m_edge_index;
	    ON_Plane eplane;
	    int have_planar_face = 0;
	    // First, see if the edge has a real planar face associated with it.  If it does,
	    // go with that plane.
	    for (int ti = 0; ti < edge->m_ti.Count(); ti++) {
		const ON_BrepTrim *t = &(brep->m_T[edge->m_ti[ti]]);
		const ON_BrepFace *f = t->Face();
		surface_t st = ((surface_t *)data->i->face_surface_types)[f->m_face_index];
		if (st == SURFACE_PLANE) {
		    f->SurfaceOf()->IsPlanar(&eplane, BREP_PLANAR_TOL);
		    have_planar_face = 1;
		    break;
		}
	    }
	    // No real face - deduce a plane from the curve
	    if (!have_planar_face) {
		ON_Curve *ecv2 = edge->EdgeCurveOf()->Duplicate();
		if (!ecv2->IsPlanar(&eplane, BREP_PLANAR_TOL)) {
		    if (msgs) bu_vls_printf(msgs, "%*sNonplanar edge in shoal (%s) - no go\n", L3_OFFSET, " ", bu_vls_addr(data->i->key));
		    delete ecv;
		    delete ecv2;
		    return 0;
		}
		delete ecv2;
	    }
	    non_linear_edge_planes.Append(eplane);
	    nonlinear_edges.insert(*c_it);
	} else {
	    linear_edges.insert(*c_it);
	}
	delete ecv;
    }

    // Now, build a list of unique planes
    ON_SimpleArray<ON_Plane> shoal_planes;
    for (int i = 0; i < non_linear_edge_planes.Count(); i++) {
	int have_plane = 0;
	ON_Plane p1 = non_linear_edge_planes[i];
	ON_3dVector p1n = p1.Normal();
	for (int j = 0; j < shoal_planes.Count(); j++) {
	    ON_Plane p2 = shoal_planes[j];
	    ON_3dVector p2n = p2.Normal();
	    if (p2n.IsParallelTo(p1n, 0.01) && fabs(p2.DistanceTo(p1.Origin())) < 0.001) {
		have_plane = 1;
		break;
	    }
	}
	if (!have_plane) {
	    shoal_planes.Append(p1);
	}
    }

    // Find the implicit plane, if there is one
    int lc, nc;
    int *le = NULL;
    int *ne = NULL;
    set_to_array(&le, &lc, &linear_edges);
    set_to_array(&ne, &nc, &nonlinear_edges);
    switch (surface_type) {
	case SURFACE_CYLINDRICAL_SECTION:
	case SURFACE_CYLINDER:
	    implicit_plane_ind = cyl_implicit_plane(brep, lc, le, &shoal_planes);
	    break;
	case SURFACE_CONE:
	    implicit_plane_ind = cone_implicit_plane(brep, lc, le, &shoal_planes);
	    break;
	case SURFACE_SPHERICAL_SECTION:
	case SURFACE_SPHERE:
	    //implicit_plane_ind = sph_implicit_plane(brep, nc, ne, &shoal_planes);
	    break;
	case SURFACE_TORUS:
	    break;
	default:
	    break;
    }
    if (le) bu_free(le, "linear edges");
    if (ne) bu_free(ne, "nonlinear edges");
    // -1 means no implicit plane, -2 means an error
    if (implicit_plane_ind == -2) return 0;

    // Need to make sure the normals are pointed the "right way"
    //
    // Check each normal direction to see whether it would trim away any edge
    // midpoints known to be present.  If both directions do so, we have a
    // concave situation and we're done.
    for (int i = 0; i < shoal_planes.Count(); i++) {
	ON_Plane p = shoal_planes[i];
	//bu_log("in rcc_%d.s rcc %f, %f, %f %f, %f, %f 0.1\n", i, p.origin.x, p.origin.y, p.origin.z, p.Normal().x, p.Normal().y, p.Normal().z);
	int flipped = 0;
	for (int j = 0; j < edge_midpnts.Count(); j++) {
	    ON_3dVector v = edge_midpnts[j] - p.origin;
	    double dotp = ON_DotProduct(v, p.Normal());
	    //bu_log("dotp: %f\n", dotp);
	    if (dotp > 0 && !NEAR_ZERO(dotp, BREP_PLANAR_TOL)) {
		if (!flipped) {
		    j = -1; // Check all points again
		    p.Flip();
		    flipped = 1;
		} else {
		    //bu_log("%*sConcave planes in %s - no go\n", L3_OFFSET, " ", bu_vls_addr(data->i->key));
		    return 0;
		}
	    }
	}
	if (flipped) shoal_planes[i].Flip();
    }

    if (implicit_plane_ind != -1) {
	//bu_log("have implicit plane\n");
	ON_Plane shoal_implicit_plane = shoal_planes[implicit_plane_ind];
	shoal_implicit_plane.Flip();
	BN_VMOVE(data->params->implicit_plane_origin, shoal_implicit_plane.Origin());
	BN_VMOVE(data->params->implicit_plane_normal, shoal_implicit_plane.Normal());
	data->params->have_implicit_plane = 1;

	// All well and good to have an implicit plane, but if we have face edges being cut by it 
	// we've got a no-go.
	std::set<int> shoal_connected_edges;
	std::set<int>::iterator scl_it;
	for (int i = 0; i < data->shoal_loops_cnt; i++) {
	    const ON_BrepLoop *loop = &(brep->m_L[data->shoal_loops[i]]);
	    for (int ti = 0; ti < loop->m_ti.Count(); ti++) {
		int vert_ind;
		const ON_BrepTrim *trim = &(brep->m_T[loop->m_ti[ti]]);
		if (trim->m_ei != -1) {
		    const ON_BrepEdge *edge = &(brep->m_E[trim->m_ei]);
		    if (trim->m_bRev3d) {
			vert_ind = edge->Vertex(0)->m_vertex_index;
		    } else {
			vert_ind = edge->Vertex(1)->m_vertex_index;
		    }
		    // Get vertex edges.
		    const ON_BrepVertex *v = &(brep->m_V[vert_ind]);
		    for (int ei = 0; ei < v->EdgeCount(); ei++) {
			const ON_BrepEdge *e = &(brep->m_E[v->m_ei[ei]]);
			//bu_log("insert edge %d\n", e->m_edge_index);
			shoal_connected_edges.insert(e->m_edge_index);
		    }
		}
	    }
	}
	for (scl_it = shoal_connected_edges.begin(); scl_it != shoal_connected_edges.end(); scl_it++) {
	    const ON_BrepEdge *edge= &(brep->m_E[(int)*scl_it]);
	    //bu_log("Edge: %d\n", edge->m_edge_index);
	    ON_3dPoint p1 = brep->m_V[edge->Vertex(0)->m_vertex_index].Point();
	    ON_3dPoint p2 = brep->m_V[edge->Vertex(1)->m_vertex_index].Point();
	    double dotp1 = ON_DotProduct(p1 - shoal_implicit_plane.origin, shoal_implicit_plane.Normal());
	    double dotp2 = ON_DotProduct(p2 - shoal_implicit_plane.origin, shoal_implicit_plane.Normal());
	    if (NEAR_ZERO(dotp1, BREP_PLANAR_TOL) || NEAR_ZERO(dotp2, BREP_PLANAR_TOL)) continue;
	    if ((dotp1 < 0 && dotp2 > 0) || (dotp1 > 0 && dotp2 < 0)) {
		return 0;
	    }
	}
    }

    int ndc;
    int *nde = NULL;
    set_to_array(&nde, &ndc, &nondegen_edges);
    int need_arbn = 1;
    switch (surface_type) {
	case SURFACE_CYLINDRICAL_SECTION:
	case SURFACE_CYLINDER:
	    need_arbn = cyl_implicit_params(data, &shoal_planes, implicit_plane_ind, ndc, nde, *nonplanar_surfaces.begin(), nonlinear_edge);
	    break;
	case SURFACE_CONE:
	    need_arbn = cone_implicit_params(data, &shoal_planes, implicit_plane_ind, ndc, nde, *nonplanar_surfaces.begin(), nonlinear_edge);
	    break;
	case SURFACE_SPHERICAL_SECTION:
	case SURFACE_SPHERE:
	    //need_arbn = sph_implicit_params(data, &shoal_planes, *nonplanar_surfaces.begin());
	    break;
	case SURFACE_TORUS:
	    break;
	default:
	    break;
    }
    if (need_arbn == -1) return 0;


    if (!need_arbn) {
	//bu_log("Perfect implicit found in %s\n", bu_vls_addr(data->i->key));
	return 1;
    }


    /* "Cull" any planes that are not needed to form a bounding arbn.  We do this
     * both for data minimization and because the arbn primitive doesn't tolerate
     * "extra" planes that don't contribute to the final shape. */

    // First, remove any arb planes that are parallel (*not* anti-parallel) with another plane.
    // This will always be a bounding arb plane if any planes are to be removed, so we use that knowledge
    // and start at the top.
    ON_SimpleArray<ON_Plane> uniq_planes;
    for (int i = shoal_planes.Count() - 1; i >= 0; i--) {
	int keep_plane = 1;
	ON_Plane p1 = shoal_planes[i];
	for (int j = i - 1; j >= 0; j--) {
	    ON_Plane p2 = shoal_planes[j];
	    // Check for one to avoid removing anti-parallel planes
	    if (p1.Normal().IsParallelTo(p2.Normal(), VUNITIZE_TOL) == 1) {
		keep_plane = 0;
		break;
	    }
	}
	if (keep_plane) {
	    uniq_planes.Append(p1);
	}
    }

    // Second, check for plane intersection points that are inside/outside the
    // arb.  If a set of three planes defines a point that is inside, those
    // planes are part of the final arb.  Based on the arbn prep test.
    int *planes_used = (int *)bu_calloc(uniq_planes.Count(), sizeof(int), "usage flags");
    convex_plane_usage(&uniq_planes, &planes_used);
    // Finally, based on usage tests, construct the set of planes that will define the arbn.
    // If it doesn't have 3 or more uses, it's not a net contributor to the shape.
    ON_SimpleArray<ON_Plane> arbn_planes;
    for (int i = 0; i < uniq_planes.Count(); i++) {
	//if (planes_used[i] != 0 && planes_used[i] < 3) bu_log("%d: have %d uses for plane %d\n", *data->i->obj_cnt + 1, planes_used[i], i);
	if (planes_used[i] != 0 && planes_used[i] > 2) arbn_planes.Append(uniq_planes[i]);
    }
    bu_free(planes_used, "planes_used");


    // Construct the arbn to intersect with the implicit to form the final shape
    if (arbn_planes.Count() > 3) {
	struct csg_object_params *sub_param;
	BU_GET(sub_param, struct csg_object_params);
	csg_object_params_init(sub_param, data);
	sub_param->csg_id = (*(data->i->obj_cnt))++;
	sub_param->csg_type = ARBN;
	sub_param->bool_op = '+'; // arbn is intersected with primary primitive
	sub_param->planes = (plane_t *)bu_calloc(arbn_planes.Count(), sizeof(plane_t), "planes");
	sub_param->plane_cnt = arbn_planes.Count();
	for (int i = 0; i < arbn_planes.Count(); i++) {
	    ON_Plane p = arbn_planes[i];
	    double d = p.DistanceTo(ON_3dPoint(0, 0, 0));
	    sub_param->planes[i][0] = p.Normal().x;
	    sub_param->planes[i][1] = p.Normal().y;
	    sub_param->planes[i][2] = p.Normal().z;
	    sub_param->planes[i][3] = -1 * d;
	}
	bu_ptbl_ins(data->shoal_children, (long *)sub_param);
    }


    return 1;
}


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
