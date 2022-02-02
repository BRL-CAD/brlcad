/*                        U T I L . C P P
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
/** @file util.cpp
 *
 * Brief description
 *
 */

#include "common.h"

#include <set>
#include <map>

#include "bu/log.h"
#include "bu/str.h"
#include "bu/malloc.h"
#include "bn/tol.h"
#include "brep/defines.h"
#include "brep/util.h"
#include "shape_recognition.h"

#define COMMA ','


void
set_key(struct bu_vls *key, int k_cnt, int *k_array)
{
    for (int i = 0; i < k_cnt; i++) {
	bu_vls_printf(key, "%d", k_array[i]);
	if (i != k_cnt - 1) bu_vls_printf(key, "%c", COMMA);
    }
}


void
set_to_array(int **array, int *array_cnt, std::set<int> *set)
{
    std::set<int>::iterator s_it;
    int i = 0;
    if (*array) bu_free((*array), "free old array");
    (*array_cnt) = set->size();
    if ((*array_cnt) > 0) {
	(*array) = (int *)bu_calloc((*array_cnt), sizeof(int), "array");
	for (s_it = set->begin(); s_it != set->end(); s_it++) {
	    (*array)[i] = *s_it;
	    i++;
	}
    }
}


void
array_to_set(std::set<int> *set, int *array, int array_cnt)
{
    for (int i = 0; i < array_cnt; i++) {
	set->insert(array[i]);
    }
}


void
csg_object_params_init(struct csg_object_params *csg, struct subbrep_shoal_data *d)
{
    csg->s = d;
    csg->csg_type = 0;
    csg->negative = 0;
    csg->csg_id = -1;
    csg->bool_op = '\0';
    csg->planes = NULL;
    csg->csg_faces = NULL;
    csg->csg_verts = NULL;
}


void
csg_object_params_free(struct csg_object_params *csg)
{
    if (!csg) return;
    if (csg->planes) bu_free(csg->planes, "free planes");
    if (csg->csg_faces) bu_free(csg->csg_faces, "free faces");
    if (csg->csg_verts) bu_free(csg->csg_verts, "free verts");
}


void
subbrep_shoal_init(struct subbrep_shoal_data *data, struct subbrep_island_data *i)
{
    data->i = i;
    BU_GET(data->params, struct csg_object_params);
    csg_object_params_init(data->params, data);
    BU_GET(data->shoal_children, struct bu_ptbl);
    bu_ptbl_init(data->shoal_children, 8, "sub_params table");
    data->shoal_loops = NULL;
    data->shoal_loops_cnt = 0;
}


void
subbrep_shoal_free(struct subbrep_shoal_data *data)
{
    if (!data) return;
    csg_object_params_free(data->params);
    BU_PUT(data->params, struct csg_object_params);
    data->params = NULL;
    for (unsigned int i = 0; i < BU_PTBL_LEN(data->shoal_children); i++) {
	struct csg_object_params *c = (struct csg_object_params *)BU_PTBL_GET(data->shoal_children, i);
	csg_object_params_free(c);
	BU_PUT(c, struct csg_object_params);
    }
    bu_ptbl_free(data->shoal_children);
    BU_PUT(data->shoal_children, struct bu_ptbl);
    if (data->shoal_loops) bu_free(data->shoal_loops, "free loop array");
}


void
subbrep_island_init(struct subbrep_island_data *obj, const ON_Brep *brep)
{
    if (!obj) return;

    /* We're a B-Rep until proven otherwise */
    obj->island_type = BREP;

    obj->brep = brep;
    obj->local_brep = NULL;
    obj->local_brep_bool_op = '\0';

    obj->bbox = new ON_BoundingBox();
    ON_MinMaxInit(&(obj->bbox->m_min), &(obj->bbox->m_max));

    BU_GET(obj->nucleus, struct subbrep_shoal_data);
    subbrep_shoal_init(obj->nucleus, obj);

    BU_GET(obj->island_children, struct bu_ptbl);
    bu_ptbl_init(obj->island_children, 8, "children table");

    BU_GET(obj->subtractions, struct bu_ptbl);
    bu_ptbl_init(obj->subtractions, 8, "children table");

    BU_GET(obj->key, struct bu_vls);
    bu_vls_init(obj->key);

    obj->obj_cnt = NULL;
    obj->island_faces = NULL;
    obj->island_loops = NULL;
    obj->fol = NULL;
    obj->fil = NULL;
    obj->null_verts = NULL;
    obj->null_edges = NULL;
}


void
subbrep_island_free(struct subbrep_island_data *obj)
{
    if (!obj)
	return;

    obj->brep = NULL;
    if (obj->local_brep) {
	delete obj->local_brep;
	obj->local_brep = NULL;
    }

    delete obj->bbox;

    if (obj->nucleus) {
	subbrep_shoal_free(obj->nucleus);
	BU_PUT(obj->nucleus, struct csg_obj_params);
	obj->nucleus = NULL;
    }

    if (obj->key) {
	bu_vls_free(obj->key);
	BU_PUT(obj->key, struct bu_vls);
	obj->key = NULL;
    }

    for (unsigned int i = 0; i < BU_PTBL_LEN(obj->island_children); i++) {
	struct subbrep_shoal_data *cobj = (struct subbrep_shoal_data *)BU_PTBL_GET(obj->island_children, i);
	if (cobj) {
	    subbrep_shoal_free(cobj);
	    BU_PUT(cobj, struct subbrep_shoal_data);
	}
    }
    if (obj->island_children) {
	bu_ptbl_free(obj->island_children);
	BU_PUT(obj->island_children, struct bu_ptbl);
	obj->island_children = NULL;
    }

    /* Anything in here will be freed elsewhere */
    if (obj->subtractions) {
	bu_ptbl_free(obj->subtractions);
	BU_PUT(obj->subtractions, struct bu_ptbl);
	obj->subtractions = NULL;
    }

    if (obj->island_faces) {
	bu_free(obj->island_faces, "obj faces");
	obj->island_faces = NULL;
    }
    if (obj->island_loops) {
	bu_free(obj->island_loops, "obj loops");
	obj->island_loops = NULL;
    }
    if (obj->fol) {
	bu_free(obj->fol, "obj fol");
	obj->fol = NULL;
    }
    if (obj->fil) {
	bu_free(obj->fil, "obj fil");
	obj->fil = NULL;
    }
    if (obj->null_verts) {
	bu_free(obj->null_verts, "ignore verts");
	obj->null_verts = NULL;
    }
    if (obj->null_edges) {
	bu_free(obj->null_edges, "ignore edges");
	obj->null_edges = NULL;
    }
}

surface_t
GetSurfaceType(const ON_Surface *orig_surface)
{
    ON_Surface *surface;
    surface_t ret = SURFACE_GENERAL;
    ON_Surface *in_surface = orig_surface->Duplicate();
    // Make things a bit larger so small surfaces can be identified
    ON_Xform sf(1000);
    in_surface->Transform(sf);
    surface = in_surface->Duplicate();
    if (surface->IsPlanar(NULL, BREP_PLANAR_TOL)) {
	ret = SURFACE_PLANE;
	goto st_done;
    }
    delete surface;
    surface = in_surface->Duplicate();
    if (surface->IsSphere(NULL, BREP_SPHERICAL_TOL)) {
	ret = SURFACE_SPHERE;
	goto st_done;
    }
    delete surface;
    surface = in_surface->Duplicate();
    if (surface->IsCylinder(NULL, BREP_CYLINDRICAL_TOL)) {
	ret = SURFACE_CYLINDER;
	goto st_done;
    }
    delete surface;
    surface = in_surface->Duplicate();
    if (surface->IsCone(NULL, BREP_CONIC_TOL)) {
	ret = SURFACE_CONE;
	goto st_done;
    }
    delete surface;
    surface = in_surface->Duplicate();
    if (surface->IsTorus(NULL, BREP_TOROIDAL_TOL)) {
	ret = SURFACE_TORUS;
	goto st_done;
    }
st_done:
    delete surface;
    delete in_surface;
    return ret;
}


surface_t
subbrep_highest_order_face(struct subbrep_island_data *data)
{
    int planar = 0;
    int spherical = 0;
    int cylindrical = 0;
    int cone = 0;
    int torus = 0;
    int general = 0;
    surface_t *fstypes = (surface_t *)data->face_surface_types;
    surface_t hofo = SURFACE_PLANE;
    for (int i = 0; i < data->island_faces_cnt; i++) {
	int ind = data->island_faces[i];
	int surface_type = (int)fstypes[ind];
	switch (surface_type) {
	    case SURFACE_PLANE:
		planar++;
		if (hofo < SURFACE_PLANE) {
		    hofo = SURFACE_PLANE;
		}
		break;
	    case SURFACE_SPHERE:
		spherical++;
		if (hofo < SURFACE_SPHERE) {
		    hofo = SURFACE_SPHERE;
		}
		break;
	    case SURFACE_CYLINDER:
		cylindrical++;
		if (hofo < SURFACE_CYLINDER) {
		    hofo = SURFACE_CYLINDER;
		}
		break;
	    case SURFACE_CONE:
		cone++;
		if (hofo < SURFACE_CONE) {
		    hofo = SURFACE_CONE;
		}
		break;
	    case SURFACE_TORUS:
		torus++;
		if (hofo < SURFACE_TORUS) {
		    hofo = SURFACE_TORUS;
		}
		break;
	    default:
		general++;
		hofo = SURFACE_GENERAL;
		//std::cout << "general surface(" << used_face.m_face_index << "): " << used_face.SurfaceIndexOf() << "\n";
		break;
	}
    }
    return hofo;
}


void
subbrep_bbox(struct subbrep_island_data *obj)
{
    for (int i = 0; i < obj->fol_cnt; i++) {
	ON_3dPoint min, max;
	ON_MinMaxInit(&min, &max);
	const ON_BrepFace *face = &(obj->brep->m_F[obj->fol[i]]);
	// Bounding intervals of outer loop
	for (int ti = 0; ti < face->OuterLoop()->TrimCount(); ti++) {
	    ON_BrepTrim *trim = face->OuterLoop()->Trim(ti);
	    trim->GetBoundingBox(min, max, true);
	}
	ON_Interval u(min.x, max.x);
	ON_Interval v(min.y, max.y);
	surface_GetBoundingBox(face->SurfaceOf(), u, v, *(obj->bbox), true);
    }
    std::set<int> loops;
    array_to_set(&loops, obj->island_loops, obj->island_loops_cnt);
    for (int i = 0; i < obj->fil_cnt; i++) {
	ON_3dPoint min, max;
	ON_MinMaxInit(&min, &max);
	const ON_BrepFace *face = &(obj->brep->m_F[obj->fil[i]]);
	int loop_ind = -1;
	for (int li = 0; li < face->LoopCount(); li++) {
	    int loop_index = face->Loop(li)->m_loop_index;
	    if (loops.find(loop_index) != loops.end()) {
		loop_ind = loop_index;
		break;
	    }
	}
	if (loop_ind == -1) {
	    bu_log("Error - could not find fil loop!\n");
	    continue;
	}
	const ON_BrepLoop *loop= &(obj->brep->m_L[loop_ind]);
	for (int ti = 0; ti < loop->TrimCount(); ti++) {
	    ON_BrepTrim *trim = loop->Trim(ti);
	    trim->GetBoundingBox(min, max, true);
	}
	ON_Interval u(min.x, max.x);
	ON_Interval v(min.y, max.y);
	surface_GetBoundingBox(face->SurfaceOf(), u, v, *(obj->bbox), true);
    }
}


/* This routine is used when there is uncertainty about whether a particular
 * solid or combination is to be subtracted or unioned into a parent.  For
 * planar on planar cases, the check is whether every vertex of the candidate
 * solid is either on or one of above/below the shared face plane.  If some
 * vertices are above and some are below, the candidate solid is self
 * intersecting.
 *
 * We've filtered out non-planar connecting loop situations elsewhere in the
 * pipeline - they aren't handled here. */
int
subbrep_brep_boolean(struct subbrep_island_data *data)
{
    const ON_Brep *brep = data->brep;
    int pos_cnt = 0;
    int neg_cnt = 0;

    /* Top level breps are unions */
    if (data->fil_cnt == 0) return 1;

    std::set<int> island_loops;
    array_to_set(&island_loops, data->island_loops, data->island_loops_cnt);

    // Collecte midpoints from all edges in the island
    ON_3dPointArray mid_points;
    for (int i = 0; i < data->island_loops_cnt; i++) {
	const ON_BrepLoop *loop = &(brep->m_L[data->island_loops[i]]);
	for (int ti = 0; ti < loop->m_ti.Count(); ti++) {
	    const ON_BrepTrim *trim = &(brep->m_T[loop->m_ti[ti]]);
	    const ON_BrepEdge *edge = &(brep->m_E[trim->m_ei]);
	    if (trim->m_ei != -1) {
		ON_3dPoint midpt = edge->EdgeCurveOf()->PointAt(edge->EdgeCurveOf()->Domain().Mid());
		mid_points.Append(midpt);
	    }
	}
    }


    for (int i = 0; i < data->fil_cnt; i++) {
	std::set<int> active_loops;
	// Get face with inner loop
	const ON_BrepFace *face = &(brep->m_F[data->fil[i]]);
	const ON_Surface *surf = face->SurfaceOf();
	surface_t stype = ((surface_t *)data->face_surface_types)[face->m_face_index];
	if (stype != SURFACE_PLANE) return -2;

	// An island with both inner and outer loops in a single face
	// is defined to be self intersecting in this context
	for (int j = 0; j < face->LoopCount(); j++) {
	    if (island_loops.find(face->m_li[j]) != island_loops.end()) active_loops.insert(face->m_li[j]);
	}
	if (active_loops.size() > 1) return -2;

	// Get face plane
	ON_Plane face_plane;
	ON_Surface *ts = surf->Duplicate();
	(void)ts->IsPlanar(&face_plane, BREP_PLANAR_TOL);
	delete ts;
	if (face->m_bRev) face_plane.Flip();

	// For each vertex in the subbrep, determine if it is above, below or on the face
	for (int j = 0; j < mid_points.Count(); j++) {
	    double distance = face_plane.DistanceTo(mid_points[j]);
	    if (distance > BREP_PLANAR_TOL) pos_cnt++;
	    if (distance < -1*BREP_PLANAR_TOL) neg_cnt++;
	}
    }

    // If we didn't get *anything* from the edge midpoints, as a last resort try the control
    // points of the surfaces included in the island via their outer loops.
    if (!pos_cnt && !neg_cnt) {
	for (int i = 0; i < data->fil_cnt; i++) {
	    // Get face with inner loop
	    const ON_BrepFace *face = &(brep->m_F[data->fil[i]]);
	    const ON_Surface *surf = face->SurfaceOf();
	    // Get face plane
	    ON_Plane face_plane;
	    ON_Surface *ts = surf->Duplicate();
	    (void)ts->IsPlanar(&face_plane, BREP_PLANAR_TOL);
	    delete ts;
	    if (face->m_bRev) face_plane.Flip();

	    for (int fo = 0; fo < data->fol_cnt; fo++) {
		const ON_BrepFace *oface = &(brep->m_F[data->fol[fo]]);
		const ON_Surface *osurf = oface->SurfaceOf();
		ON_NurbsSurface onsurf;
		if (!osurf->GetNurbForm(onsurf)) continue;
		for (int j = 0; j < onsurf.m_cv_count[0]; j++) {
		    for (int k = 0; k < onsurf.m_cv_count[1]; k++) {
			ON_3dPoint cp;
			onsurf.GetCV(j, k, cp);
			double distance = face_plane.DistanceTo(cp);
			if (distance > BREP_PLANAR_TOL) pos_cnt++;
			if (distance < -1*BREP_PLANAR_TOL) neg_cnt++;
		    }
		}
	    }
	}
    }

    // Determine what we have.  If we have both pos and neg counts > 0,
    // the proposed brep needs to be broken down further.  all pos
    // counts is a union, all neg counts is a subtraction
    if (pos_cnt && neg_cnt) return -2;
    if (pos_cnt) return 1;
    if (neg_cnt) return -1;

    // If we couldn't deduce anything, return zero - caller will have to decide what to do.
    return 0;
}


int
subbrep_make_brep(struct bu_vls *UNUSED(msgs), struct subbrep_island_data *data)
{
    if (data->local_brep) return 0;
    data->local_brep = ON_Brep::New();
    const ON_Brep *brep = data->brep;
    ON_Brep *nbrep = data->local_brep;

    // We only want the subset of data that is part of this particular
    // subobject - to do that, we need to map elements from their indices in
    // the old Brep to their locations in the new.
    std::map<int, int> face_map;
    std::map<int, int> surface_map;
    std::map<int, int> edge_map;
    std::map<int, int> vertex_map;
    std::map<int, int> c3_map;

    std::set<int> faces;
    std::set<int> fil;
    array_to_set(&faces, data->island_faces, data->island_faces_cnt);
    array_to_set(&fil, data->fil, data->fil_cnt);

    // Use the set of loops to collect loops, trims, vertices, edges, faces, 2D
    // and 3D curves
    for (int i = 0; i < data->island_loops_cnt; i++) {
	const ON_BrepLoop *loop = &(brep->m_L[data->island_loops[i]]);
	const ON_BrepFace *face = loop->Face();
	// Face
	if (face_map.find(face->m_face_index) == face_map.end()) {
	    // Get Surface of Face
	    ON_Surface *ns = face->SurfaceOf()->Duplicate();
	    int nsid = nbrep->AddSurface(ns);
	    surface_map[face->SurfaceIndexOf()] = nsid;
	    // Get Face
	    ON_BrepFace &new_face = nbrep->NewFace(nsid);
	    face_map[face->m_face_index] = new_face.m_face_index;
	    if (fil.find(face->m_face_index) != fil.end()) nbrep->FlipFace(new_face);
	    if (face->m_bRev) nbrep->FlipFace(new_face);
	}
	// Loop
	ON_BrepLoop &nl = nbrep->NewLoop(ON_BrepLoop::outer, nbrep->m_F[face_map[face->m_face_index]]);
	if (loop->m_type != ON_BrepLoop::outer && loop->m_type != ON_BrepLoop::inner)
	    nl.m_type = loop->m_type;
	// Trims, etc.
	for (int ti = 0; ti < loop->m_ti.Count(); ti++) {
	    int v0i, v1i;
	    int c2i, c3i;
	    const ON_BrepTrim *trim = &(brep->m_T[loop->m_ti[ti]]);
	    const ON_BrepEdge *edge = &(brep->m_E[trim->m_ei]);
	    const ON_BrepEdge *nedge = NULL;

	    // Get the 2D curve from the trim.
	    ON_Curve *nc = trim->TrimCurveOf()->Duplicate();
	    c2i = nbrep->AddTrimCurve(nc);

	    // Edges, etc.
	    if (trim->m_ei != -1 && edge) {
		// Get the 3D curve from the edge
		if (c3_map.find(edge->EdgeCurveIndexOf()) == c3_map.end()) {
		    ON_Curve *nec = edge->EdgeCurveOf()->Duplicate();
		    c3i = nbrep->AddEdgeCurve(nec);
		    c3_map[edge->EdgeCurveIndexOf()] = c3i;
		} else {
		    c3i = c3_map[edge->EdgeCurveIndexOf()];
		}

		// Get the vertices from the edges
		if (vertex_map.find(edge->Vertex(0)->m_vertex_index) == vertex_map.end()) {
		    ON_BrepVertex& newv0 = nbrep->NewVertex(edge->Vertex(0)->Point(), edge->Vertex(0)->m_tolerance);
		    v0i = newv0.m_vertex_index;
		    vertex_map[edge->Vertex(0)->m_vertex_index] = v0i;
		} else {
		    v0i = vertex_map[edge->Vertex(0)->m_vertex_index];
		}
		if (vertex_map.find(edge->Vertex(1)->m_vertex_index) == vertex_map.end()) {
		    ON_BrepVertex& newv1 = nbrep->NewVertex(edge->Vertex(1)->Point(), edge->Vertex(1)->m_tolerance);
		    v1i = newv1.m_vertex_index;
		    vertex_map[edge->Vertex(1)->m_vertex_index] = v1i;
		} else {
		    v1i = vertex_map[edge->Vertex(1)->m_vertex_index];
		}

		// Edge
		if (edge_map.find(edge->m_edge_index) == edge_map.end()) {
		    ON_BrepEdge& new_edge = nbrep->NewEdge(nbrep->m_V[v0i], nbrep->m_V[v1i], c3i, NULL, 0);
		    edge_map[edge->m_edge_index] = new_edge.m_edge_index;
		}
		nedge = &(nbrep->m_E[edge_map[edge->m_edge_index]]);
	    }

	    // Now set up the Trim
	    if (trim->m_ei != -1 && nedge) {
		ON_BrepEdge &n_edge = nbrep->m_E[nedge->m_edge_index];
		ON_BrepTrim &nt = nbrep->NewTrim(n_edge, trim->m_bRev3d, nl, c2i);
		nt.m_tolerance[0] = trim->m_tolerance[0];
		nt.m_tolerance[1] = trim->m_tolerance[1];
		nt.m_type = trim->m_type;
		nt.m_iso = trim->m_iso;
	    } else {
		if (vertex_map.find(trim->Vertex(0)->m_vertex_index) == vertex_map.end()) {
		    ON_BrepVertex& newvs = nbrep->NewVertex(trim->Vertex(0)->Point(), trim->Vertex(0)->m_tolerance);
		    vertex_map[trim->Vertex(0)->m_vertex_index] = newvs.m_vertex_index;
		    ON_BrepTrim &nt = nbrep->NewSingularTrim(newvs, nl, trim->m_iso, c2i);
		    nt.m_type = trim->m_type;
		    nt.m_tolerance[0] = trim->m_tolerance[0];
		    nt.m_tolerance[1] = trim->m_tolerance[1];
		} else {
		    ON_BrepTrim &nt = nbrep->NewSingularTrim(nbrep->m_V[vertex_map[trim->Vertex(0)->m_vertex_index]], nl, trim->m_iso, c2i);
		    nt.m_type = trim->m_type;
		    nt.m_tolerance[0] = trim->m_tolerance[0];
		    nt.m_tolerance[1] = trim->m_tolerance[1];
		}

	    }
	}
    }

    // Make sure all the loop directions and types are correct
    for (int f = 0; f < nbrep->m_F.Count(); f++) {
	ON_BrepFace *face = &(nbrep->m_F[f]);
	if (face->m_li.Count() == 1) {
	    ON_BrepLoop& loop = nbrep->m_L[face->m_li[0]];
	    if (nbrep->LoopDirection(loop) != 1) {
		nbrep->FlipLoop(loop);
	    }
	    loop.m_type = ON_BrepLoop::outer;
	} else {
	    int i1 = 0;
	    int tmp;
	    ON_BoundingBox o_bbox, c_bbox;
	    int outer_loop_ind = face->m_li[0];
	    nbrep->m_L[outer_loop_ind].GetBoundingBox(o_bbox);
	    for (int l = 1; l < face->m_li.Count(); l++) {
		ON_BrepLoop& loop = nbrep->m_L[face->m_li[l]];
		loop.GetBoundingBox(c_bbox);

		if (c_bbox.Includes(o_bbox)) {
		    if (nbrep->m_L[outer_loop_ind].m_type == ON_BrepLoop::outer) {
			nbrep->m_L[outer_loop_ind].m_type = ON_BrepLoop::inner;
		    }
		    o_bbox = c_bbox;
		    outer_loop_ind = face->m_li[l];
		    i1 = l;
		}
	    }
	    if (nbrep->m_L[outer_loop_ind].m_type != ON_BrepLoop::outer)
		nbrep->m_L[outer_loop_ind].m_type = ON_BrepLoop::outer;
	    tmp = face->m_li[0];
	    face->m_li[0] = face->m_li[i1];
	    face->m_li[i1] = tmp;
	    for (int l = 1; l < face->m_li.Count(); l++) {
		if (nbrep->m_L[face->m_li[l]].m_type != ON_BrepLoop::inner && nbrep->m_L[face->m_li[l]].m_type != ON_BrepLoop::slit) {
		    nbrep->m_L[face->m_li[l]].m_type = ON_BrepLoop::inner;
		}
	    }
	}
    }

    nbrep->ShrinkSurfaces();
    nbrep->CullUnusedSurfaces();

    //std::cout << "new brep done: " << bu_vls_addr(data->key) << "\n";

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

