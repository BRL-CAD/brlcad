/*                        C D T . C P P
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
/** @file cdt.cpp
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

static void
Process_Loop_Edges(
	struct ON_Brep_CDT_State *s_cdt,
	ON_SimpleArray<BrepTrimPoint> *points,
	const ON_BrepFace &face,
	const ON_BrepLoop *loop,
	fastf_t max_dist
	)
{
    int trim_count = loop->TrimCount();

    for (int lti = 0; lti < trim_count; lti++) {
	ON_BrepTrim *trim = loop->Trim(lti);
	ON_BrepEdge *edge = trim->Edge();

	/* Provide 2D points for p2d, but we need to be aware that this will
	 * result in (trivially) degenerate 3D faces that we need to filter out
	 * when assembling a mesh */
	if (trim->m_type == ON_BrepTrim::singular) {
	    BrepTrimPoint btp;
	    const ON_BrepVertex& v1 = face.Brep()->m_V[trim->m_vi[0]];
	    ON_3dPoint *p3d = (*s_cdt->vert_pnts)[v1.m_vertex_index];
	    (*s_cdt->faces)[face.m_face_index]->strim_pnts->insert(std::make_pair(trim->m_trim_index, p3d));
	    ON_3dPoint *n3d = (*s_cdt->vert_avg_norms)[v1.m_vertex_index];
	    if (n3d) {
		(*s_cdt->faces)[face.m_face_index]->strim_norms->insert(std::make_pair(trim->m_trim_index, n3d));
	    }
	    double delta =  trim->Domain().Length() / 10.0;
	    ON_Interval trim_dom = trim->Domain();

	    for (int i = 1; i <= 10; i++) {
		btp.p3d = p3d;
		btp.n3d = n3d;
		btp.p2d = v1.Point();
		btp.t = trim->Domain().m_t[0] + (i - 1) * delta;
		btp.p2d = trim->PointAt(btp.t);
		btp.e = ON_UNSET_VALUE;
		points->Append(btp);
	    }
	    // skip last point of trim if not last trim
	    if (lti < trim_count - 1)
		continue;

	    const ON_BrepVertex& v2 = face.Brep()->m_V[trim->m_vi[1]];
	    btp.p3d = p3d;
	    btp.n3d = n3d;
	    btp.p2d = v2.Point();
	    btp.t = trim->Domain().m_t[1];
	    btp.p2d = trim->PointAt(btp.t);
	    btp.e = ON_UNSET_VALUE;
	    points->Append(btp);

	    continue;
	}

	if (!trim->m_trim_user.p) {
	    (void)getEdgePoints(s_cdt, edge, *trim, max_dist, DBL_MAX);
	    if (!trim->m_trim_user.p) {
		//bu_log("Failed to initialize trim->m_trim_user.p: Trim %d (associated with Edge %d) point count: %zd\n", trim->m_trim_index, trim->Edge()->m_edge_index, m->size());
		continue;
	    }
	}

	// If we can bound it, assemble the trim segments in order on the
	// loop array (which will in turn be used to generate the poly2tri
	// polyline for CDT)
	ON_3dPoint boxmin, boxmax;
	if (trim->GetBoundingBox(boxmin, boxmax, false)) {
	    std::map<double, BrepTrimPoint *> *param_points3d = (std::map<double, BrepTrimPoint *> *)trim->m_trim_user.p;
	    std::map<double, BrepTrimPoint*>::const_iterator i, ni;
	    for (i = param_points3d->begin(); i != param_points3d->end();) {
		BrepTrimPoint *btp = (*i).second;
		ni = ++i;
		// skip last point of trim if not last trim
		if (ni == param_points3d->end()) {
		    if (lti < trim_count - 1) {
			continue;
		    }
		}
		points->Append(*btp);
	    }
	}
    }
}

#define MAX_TRIANGULATION_ATTEMPTS 5

static int
do_triangulation(struct ON_Brep_CDT_Face_State *f, int full_surface_sample, int cnt)
{
    int ret = 0;

    if (cnt > MAX_TRIANGULATION_ATTEMPTS) {
	std::cerr << "Error: even after " << MAX_TRIANGULATION_ATTEMPTS << " iterations could not successfully triangulate face " << f->ind << "\n";
	return 0;
    }
    // TODO - for now, don't reset before returning the error - we want to see the
    // failed mesh for diagnostics
    ON_Brep_CDT_Face_Reset(f, full_surface_sample);

    bool outer = build_poly2tri_polylines(f, full_surface_sample);
    if (outer) {
	std::cerr << "Error: Face(" << f->ind << ") cannot evaluate its outer loop and will not be facetized." << std::endl;
	return -1;
    }

    if (full_surface_sample) {
	// Sample the surface, independent of the trimming curves, to get points that
	// will tie the mesh to the interior surface.
	getSurfacePoints(f);
    }

    std::set<ON_2dPoint *>::iterator p_it;
    for (p_it = f->on_surf_points->begin(); p_it != f->on_surf_points->end(); p_it++) {
	ON_2dPoint *p = *p_it;
	p2t::Point *tp = new p2t::Point(p->x, p->y);
	(*f->p2t_to_on2_map)[tp] = p;
	f->cdt->AddPoint(tp);
    }

    // All preliminary steps are complete, perform the triangulation using
    // Poly2Tri's triangulation.  NOTE: it is important that the inputs to
    // Poly2Tri satisfy its constraints - failure here could cause a crash.
    f->cdt->Triangulate(true, -1);

    /* Calculate any 3D points we don't already have */
    populate_3d_pnts(f);

    // Trivially degenerate pass
    triangles_degenerate_trivial(f);

    // Zero area triangles
    triangles_degenerate_area(f);

    // Validate based on edges.  If we get a return > 0, adjustments were
    // made to the surface points and we need to redo the triangulation
    int eret = triangles_check_edges(f);
    if (eret < 0) {
	bu_log("Fatal failure on edge checking\n");
	return -1;
    }
    ret += eret;

    // Incorrect normals.  If we get a return > 0, adjustments were mde
    // to the surface points and we need to redo the triangulation
    int nret = triangles_incorrect_normals(f);
    if (nret < 0) {
	bu_log("Fatal failure on normals checking\n");
	return -1;
    }
    ret += nret;

    if (ret > 0) {
	return do_triangulation(f, 0, cnt+1);
    }

    if (!ret) {
	bu_log("Face %d: successful triangulation after %d passes\n", f->ind, cnt);
    }
    return ret;
}

static int
ON_Brep_CDT_Face(struct ON_Brep_CDT_Face_State *f, std::map<const ON_Surface *, double> *s_to_maxdist)
{
    struct ON_Brep_CDT_State *s_cdt = f->s_cdt;
    int face_index = f->ind;
    ON_BrepFace &face = f->s_cdt->brep->m_F[face_index];
    const ON_Surface *s = face.SurfaceOf();
    int loop_cnt = face.LoopCount();
    ON_SimpleArray<BrepTrimPoint> *brep_loop_points = (f->face_loop_points) ? f->face_loop_points : new ON_SimpleArray<BrepTrimPoint>[loop_cnt];
    f->face_loop_points = brep_loop_points;

    // Use the edge curves and loops to generate an initial set of trim polygons.
    for (int li = 0; li < loop_cnt; li++) {
	double max_dist = 0.0;
	const ON_BrepLoop *loop = face.Loop(li);
	if (s_to_maxdist->find(face.SurfaceOf()) != s_to_maxdist->end()) {
	    max_dist = (*s_to_maxdist)[face.SurfaceOf()];
	}
	Process_Loop_Edges(s_cdt, &brep_loop_points[li], face, loop, max_dist);
    }

    // Handle a variety of situations that complicate loop handling on closed surfaces
    if (s->IsClosed(0) || s->IsClosed(1)) {
	PerformClosedSurfaceChecks(s_cdt, s, face, brep_loop_points, BREP_SAME_POINT_TOLERANCE);
    }

    // Find for this face, find the minimum and maximum edge polyline segment lengths
    (*s_cdt->max_edge_seg_len)[face.m_face_index] = 0.0;
    (*s_cdt->min_edge_seg_len)[face.m_face_index] = DBL_MAX;
    for (int li = 0; li < loop_cnt; li++) {
	int num_loop_points = brep_loop_points[li].Count();
	if (num_loop_points > 1) {
	    ON_3dPoint *p1 = (brep_loop_points[li])[0].p3d;
	    ON_3dPoint *p2 = NULL;
	    for (int i = 1; i < num_loop_points; i++) {
		p2 = p1;
		p1 = (brep_loop_points[li])[i].p3d;
		fastf_t dist = p1->DistanceTo(*p2);
		if (dist > (*s_cdt->max_edge_seg_len)[face.m_face_index]) {
		    (*s_cdt->max_edge_seg_len)[face.m_face_index] = dist;
		}
		if ((dist > SMALL_FASTF) && (dist < (*s_cdt->min_edge_seg_len)[face.m_face_index]))  {
		    (*s_cdt->min_edge_seg_len)[face.m_face_index] = dist;
		}
	    }
	}
    }


    // TODO - we may need to add 2D points on trims that the edges didn't know
    // about.  Since 3D points must be shared along edges and we're using
    // synchronized numbers of parametric domain ordered 2D and 3D points to
    // make that work, we will need to track new 2D points and update the
    // corresponding 3D edge based data structures.  More than that - we must
    // also update all 2D structures in all other faces associated with the
    // edge in question to keep things in overall sync.

    // TODO - if we are going to apply clipper boolean resolution to sets of
    // face loops, that would come here - once we have assembled the loop
    // polygons for the faces. That also has the potential to generate "new" 3D
    // points on edges that are driven by 2D boolean intersections between
    // trimming loops, and may require another update pass as above.

    return do_triangulation(f, 1, 0);
}

static ON_3dVector
calc_trim_vnorm(ON_BrepVertex& v, ON_BrepTrim *trim)
{
    ON_3dPoint t1, t2;
    ON_3dVector v1 = ON_3dVector::UnsetVector;
    ON_3dVector v2 = ON_3dVector::UnsetVector;
    ON_3dVector trim_norm = ON_3dVector::UnsetVector;

    ON_Interval trange = trim->Domain();
    ON_3dPoint t_2d1 = trim->PointAt(trange[0]);
    ON_3dPoint t_2d2 = trim->PointAt(trange[1]);

    ON_Plane fplane;
    const ON_Surface *s = trim->SurfaceOf();
    if (s->IsPlanar(&fplane, BREP_PLANAR_TOL)) {
	trim_norm = fplane.Normal();
	if (trim->Face()->m_bRev) {
	    trim_norm = trim_norm * -1;
	}
    } else {
	int ev1 = 0;
	int ev2 = 0;
	if (surface_EvNormal(s, t_2d1.x, t_2d1.y, t1, v1)) {
	    if (trim->Face()->m_bRev) {
		v1 = v1 * -1;
	    }
	    if (v.Point().DistanceTo(t1) < ON_ZERO_TOLERANCE) {
		ev1 = 1;
		trim_norm = v1;
	    }
	}
	if (surface_EvNormal(s, t_2d2.x, t_2d2.y, t2, v2)) {
	    if (trim->Face()->m_bRev) {
		v2 = v2 * -1;
	    }
	    if (v.Point().DistanceTo(t2) < ON_ZERO_TOLERANCE) {
		ev2 = 1;
		trim_norm = v2;
	    }
	}
	// If we got both of them, go with the closest one
	if (ev1 && ev2) {
#if 0
	    if (ON_DotProduct(v1, v2) < 0) {
		bu_log("Vertex %d(%f %f %f), trim %d: got both normals\n", v.m_vertex_index, v.Point().x, v.Point().y, v.Point().z, trim->m_trim_index);
		bu_log("v1(%f)(%f %f)(%f %f %f): %f %f %f\n", v.Point().DistanceTo(t1), t_2d1.x, t_2d1.y, t1.x, t1.y, t1.z, v1.x, v1.y, v1.z);
		bu_log("v2(%f)(%f %f)(%f %f %f): %f %f %f\n", v.Point().DistanceTo(t2), t_2d2.x, t_2d2.y, t2.x, t2.y, t2.z, v2.x, v2.y, v2.z);
	    }
#endif
	    trim_norm = (v.Point().DistanceTo(t1) < v.Point().DistanceTo(t2)) ? v1 : v2;
	}
    }

    return trim_norm;
}

static void
calc_vert_norm(struct ON_Brep_CDT_State *s_cdt, int index)
{
    ON_BrepVertex& v = s_cdt->brep->m_V[index];
    int have_calculated = 0;
    ON_3dVector vnrml(0.0, 0.0, 0.0);

    for (int eind = 0; eind != v.EdgeCount(); eind++) {
	ON_3dVector trim1_norm = ON_3dVector::UnsetVector;
	ON_3dVector trim2_norm = ON_3dVector::UnsetVector;
	ON_BrepEdge& edge = s_cdt->brep->m_E[v.m_ei[eind]];
	if (edge.TrimCount() != 2) {
	    // Don't know what to do with this yet... skip.
	    continue;
	}
	ON_BrepTrim *trim1 = edge.Trim(0);
	ON_BrepTrim *trim2 = edge.Trim(1);

	if (trim1->m_type != ON_BrepTrim::singular) {
	    trim1_norm = calc_trim_vnorm(v, trim1);
	}
	if (trim2->m_type != ON_BrepTrim::singular) {
	    trim2_norm = calc_trim_vnorm(v, trim2);
	}

	// If one of the normals is unset and the other comes from a plane, use it
	if (trim1_norm == ON_3dVector::UnsetVector && trim2_norm != ON_3dVector::UnsetVector) {
	    const ON_Surface *s2 = trim2->SurfaceOf();
	    if (!s2->IsPlanar(NULL, ON_ZERO_TOLERANCE)) {
		continue;
	    }
	    trim1_norm = trim2_norm;
	}
	if (trim1_norm != ON_3dVector::UnsetVector && trim2_norm == ON_3dVector::UnsetVector) {
	    const ON_Surface *s1 = trim1->SurfaceOf();
	    if (!s1->IsPlanar(NULL, ON_ZERO_TOLERANCE)) {
		continue;
	    }
	    trim2_norm = trim1_norm;
	}

	// If we have disagreeing normals and one of them is from a planar surface, go
	// with that one
	if (NEAR_EQUAL(ON_DotProduct(trim1_norm, trim2_norm), -1, VUNITIZE_TOL)) {
	    const ON_Surface *s1 = trim1->SurfaceOf();
	    const ON_Surface *s2 = trim2->SurfaceOf();
	    if (!s1->IsPlanar(NULL, ON_ZERO_TOLERANCE) && !s2->IsPlanar(NULL, ON_ZERO_TOLERANCE)) {
		// Normals severely disagree, no planar surface to fall back on - can't use this
		continue;
	    }
	    if (s1->IsPlanar(NULL, ON_ZERO_TOLERANCE) && s2->IsPlanar(NULL, ON_ZERO_TOLERANCE)) {
		// Two disagreeing planes - can't use this
		continue;
	    }
	    if (s1->IsPlanar(NULL, ON_ZERO_TOLERANCE)) {
		trim2_norm = trim1_norm;
	    }
	    if (s2->IsPlanar(NULL, ON_ZERO_TOLERANCE)) {
		trim1_norm = trim2_norm;
	    }
	}

	// Stash normals coming from non-singular trims at vertices for faces.  If a singular trim
	// needs a normal in 3D, want to use one of these
	int mfaceind1 = trim1->Face()->m_face_index;
	ON_3dPoint *t1pnt = new ON_3dPoint(trim1_norm);
	if (s_cdt->faces->find(mfaceind1) == s_cdt->faces->end()) {
	    struct ON_Brep_CDT_Face_State *f = ON_Brep_CDT_Face_Create(s_cdt, mfaceind1);
	    (*s_cdt->faces)[mfaceind1] = f;
	}
	(*(*s_cdt->faces)[mfaceind1]->vert_face_norms)[v.m_vertex_index].insert(t1pnt);
	(*s_cdt->faces)[mfaceind1]->w3dnorms->push_back(t1pnt);
	int mfaceind2 = trim2->Face()->m_face_index;
	ON_3dPoint *t2pnt = new ON_3dPoint(trim2_norm);
	if (s_cdt->faces->find(mfaceind2) == s_cdt->faces->end()) {
	    struct ON_Brep_CDT_Face_State *f = ON_Brep_CDT_Face_Create(s_cdt, mfaceind2);
	    (*s_cdt->faces)[mfaceind2] = f;
	}
	(*(*s_cdt->faces)[mfaceind2]->vert_face_norms)[v.m_vertex_index].insert(t1pnt);
	(*s_cdt->faces)[mfaceind2]->w3dnorms->push_back(t2pnt);

	// Add the normals to the vnrml total
	vnrml += trim1_norm;
	vnrml += trim2_norm;
	have_calculated = 1;

    }
    if (!have_calculated) {
	return;
    }

    // Average all the successfully calculated normals into a new unit normal
    vnrml.Unitize();

    // We store this as a point to keep C++ happy...  If we try to
    // propagate the ON_3dVector type through all the CDT logic it
    // triggers issues with the compile.
    (*s_cdt->vert_avg_norms)[index] = new ON_3dPoint(vnrml);
    s_cdt->w3dnorms->push_back((*s_cdt->vert_avg_norms)[index]);

}

int
ON_Brep_CDT_Tessellate(struct ON_Brep_CDT_State *s_cdt, int face_cnt, int *faces)
{

    ON_wString wstr;
    ON_TextLog tl(wstr);

    // Check for any conditions that are show-stoppers
    ON_wString wonstr;
    ON_TextLog vout(wonstr);
    if (!s_cdt->orig_brep->IsValid(&vout)) {
	bu_log("brep is NOT valid, cannot produce watertight mesh\n");
	//return -1;
    }

    // For now, edges must have 2 and only 2 trims for this to work.
    for (int index = 0; index < s_cdt->orig_brep->m_E.Count(); index++) {
        ON_BrepEdge& edge = s_cdt->orig_brep->m_E[index];
        if (edge.TrimCount() != 2) {
	    bu_log("Edge %d trim count: %d - can't (yet) do watertight meshing\n", edge.m_edge_index, edge.TrimCount());
            return -1;
        }
    }

    // We may be changing the ON_Brep data, so work on a copy
    // rather than the original object
    if (!s_cdt->brep) {

	s_cdt->brep = new ON_Brep(*s_cdt->orig_brep);

	// Attempt to minimize situations where 2D and 3D distances get out of sync
	// by shrinking the surfaces down to the active area of the face
	s_cdt->brep->ShrinkSurfaces();

    }

    ON_Brep* brep = s_cdt->brep;
    std::map<const ON_Surface *, double> s_to_maxdist;

    // If this is the first time through, there are a number of once-per-conversion
    // operations to take care of.
    if (!s_cdt->w3dpnts->size()) {
	// Reparameterize the face's surface and transform the "u" and "v"
	// coordinates of all the face's parameter space trimming curves to
	// minimize distortion in the map from parameter space to 3d.
	for (int face_index = 0; face_index < brep->m_F.Count(); face_index++) {
	    ON_BrepFace *face = brep->Face(face_index);
	    const ON_Surface *s = face->SurfaceOf();
	    double surface_width, surface_height;
	    if (s->GetSurfaceSize(&surface_width, &surface_height)) {
		face->SetDomain(0, 0.0, surface_width);
		face->SetDomain(1, 0.0, surface_height);
	    }
	}

	/* We want to use ON_3dPoint pointers and BrepVertex points, but
	 * vert->Point() produces a temporary address.  If this is our first time
	 * through, make stable copies of the Vertex points. */
	for (int index = 0; index < brep->m_V.Count(); index++) {
	    ON_BrepVertex& v = brep->m_V[index];
	    (*s_cdt->vert_pnts)[index] = new ON_3dPoint(v.Point());
	    CDT_Add3DPnt(s_cdt, (*s_cdt->vert_pnts)[index], -1, v.m_vertex_index, -1, -1, -1, -1);
	    // topologically, any vertex point will be on edges
	    s_cdt->edge_pnts->insert((*s_cdt->vert_pnts)[index]);
	}

	/* If this is the first time through, get vertex normals that are the
	 * average of the surface normals at the junction from faces that don't use
	 * a singular trim to reference the vertex.  When subdividing the edges, we
	 * use the normals as a curvature guide.
	 */
	for (int index = 0; index < brep->m_V.Count(); index++) {
	    calc_vert_norm(s_cdt, index);
	}

	/* To generate watertight meshes, the faces *must* share 3D edge points.  To ensure
	 * a uniform set of edge points, we first sample all the edges and build their
	 * point sets */
	for (int index = 0; index < brep->m_E.Count(); index++) {
	    ON_BrepEdge& edge = brep->m_E[index];
	    ON_BrepTrim& trim1 = brep->m_T[edge.Trim(0)->m_trim_index];
	    ON_BrepTrim& trim2 = brep->m_T[edge.Trim(1)->m_trim_index];

	    // Get distance tolerances from the surface sizes
	    fastf_t max_dist = 0.0;
	    fastf_t min_dist = DBL_MAX;
	    fastf_t mw, mh;
	    fastf_t md1, md2 = 0.0;
	    double sw1, sh1, sw2, sh2;
	    const ON_Surface *s1 = trim1.Face()->SurfaceOf();
	    const ON_Surface *s2 = trim2.Face()->SurfaceOf();
	    if (s1->GetSurfaceSize(&sw1, &sh1) && s2->GetSurfaceSize(&sw2, &sh2)) {
		if ((sw1 < s_cdt->dist) || (sh1 < s_cdt->dist) || sw2 < s_cdt->dist || sh2 < s_cdt->dist) {
		    return -1;
		}
		md1 = sqrt(sw1 * sh1 + sh1 * sw1) / 10.0;
		md2 = sqrt(sw2 * sh2 + sh2 * sw2) / 10.0;
		max_dist = (md1 < md2) ? md1 : md2;
		mw = (sw1 < sw2) ? sw1 : sw2;
		mh = (sh1 < sh2) ? sh1 : sh2;
		min_dist = (mw < mh) ? mw : mh;
		s_to_maxdist[s1] = max_dist;
		s_to_maxdist[s2] = max_dist;
	    }

	    // Generate the BrepTrimPoint arrays for both trims associated with this edge
	    //
	    // TODO - need to make this robust to changed tolerances on refinement
	    // runs - if pre-existing solutions for "high level" splits exist,
	    // reuse them and dig down to find where we need further refinement to
	    // create new points.
	    (void)getEdgePoints(s_cdt, &edge, trim1, max_dist, min_dist);

	}

    } else {
	/* Clear the mesh state, if this container was previously used */
	s_cdt->vfpnts->clear();
	s_cdt->vfnormals->clear();
	s_cdt->on_pnt_to_bot_pnt->clear();
	s_cdt->on_pnt_to_bot_norm->clear();
	s_cdt->tri_brep_face->clear();
    }

    // Process all of the faces we have been instructed to process, or (default) all faces.
    // Keep track of failures and successes.
    int face_failures = 0;
    int face_successes = 0;
    int fc = ((face_cnt == 0) || !faces) ? s_cdt->brep->m_F.Count() : face_cnt;
    for (int i = 0; i < fc; i++) {
	int fi = ((face_cnt == 0) || !faces) ? i : faces[i];
	if (fi < s_cdt->brep->m_F.Count()) {
	    if (s_cdt->faces->find(fi) == s_cdt->faces->end()) {
		struct ON_Brep_CDT_Face_State *f = ON_Brep_CDT_Face_Create(s_cdt, fi);
		(*s_cdt->faces)[fi] = f;
	    }
	    if (ON_Brep_CDT_Face((*s_cdt->faces)[fi], &s_to_maxdist)) {
		face_failures++;
	    } else {
		face_successes++;
	    }
	}
    }

    // If we only tessellated some of the faces, we don't have the
    // full solid mesh yet (by definition).  Return accordingly.
    if (face_failures || !face_successes || face_successes < s_cdt->brep->m_F.Count()) {
	return (face_successes) ? 1 : -1;
    }

    /* We've got face meshes and no reported failures - check to see if we have a
     * solid mesh */
    int valid_fcnt, valid_vcnt;
    int *valid_faces = NULL;
    fastf_t *valid_vertices = NULL;

    if (ON_Brep_CDT_Mesh(&valid_faces, &valid_fcnt, &valid_vertices, &valid_vcnt, NULL, NULL, NULL, NULL, s_cdt) < 0) {
	return -1;
    }

    struct bg_trimesh_solid_errors se = BG_TRIMESH_SOLID_ERRORS_INIT_NULL;
    int invalid = bg_trimesh_solid2(valid_vcnt, valid_fcnt, valid_vertices, valid_faces, &se);

    if (invalid) {
	trimesh_error_report(s_cdt, valid_fcnt, valid_vcnt, valid_faces, valid_vertices, &se);
    }

    bu_free(valid_faces, "faces");
    bu_free(valid_vertices, "vertices");

    if (invalid) {
	return 1;
    }

    return 0;

}


// Generate a BoT with normals.
int
ON_Brep_CDT_Mesh(
	int **faces, int *fcnt,
	fastf_t **vertices, int *vcnt,
	int **face_normals, int *fn_cnt,
	fastf_t **normals, int *ncnt,
	struct ON_Brep_CDT_State *s_cdt)
{
    size_t triangle_cnt = 0;
    if (!faces || !fcnt || !vertices || !vcnt || !s_cdt) {
	return -1;
    }

    /* We can ignore the face normals if we want, but if some of the
     * return variables are non-NULL they all need to be non-NULL */
    if (face_normals || fn_cnt || normals || ncnt) {
	if (!face_normals || !fn_cnt || !normals || !ncnt) {
	    return -1;
	}
    }

#if 0
    // Do a second pass over the remaining non-degenerate faces, looking for
    // near-zero length edges.  These indicate that there may be 3D points we
    // need to consolidate.  Ideally shouldn't get this from edge points, but
    // the surface points could conceivably introduce such points.
    int consolidated_points = 0;
    for (int face_index = 0; face_index != s_cdt->brep->m_F.Count(); face_index++) {
	p2t::CDT *cdt = s_cdt->p2t_faces[face_index];
	std::map<p2t::Point *, ON_3dPoint *> *pointmap = s_cdt->tri_to_on3_maps[face_index];
	std::vector<p2t::Triangle*> tris = cdt->GetTriangles();
	triangle_cnt += tris.size();
	for (size_t i = 0; i < tris.size(); i++) {
	    std::set<ON_3dPoint *> c_cand;
	    p2t::Triangle *t = tris[i];
	    if (tris_degen.find(t) != tris_degen.end()) {
		continue;
	    }
	    ON_3dPoint *tpnts[3] = {NULL, NULL, NULL};
	    for (size_t j = 0; j < 3; j++) {
		p2t::Point *p = t->GetPoint(j);
		ON_3dPoint *op = (*pointmap)[p];
		tpnts[j] = op;
	    }
	    fastf_t d1 = tpnts[0]->DistanceTo(*tpnts[1]);
	    fastf_t d2 = tpnts[1]->DistanceTo(*tpnts[2]);
	    fastf_t d3 = tpnts[2]->DistanceTo(*tpnts[0]);
	    if (d1 < s_cdt->dist || d2 < s_cdt->dist || d3 < s_cdt->dist) {
		bu_log("found consolidation candidate\n");
		consolidated_points = 1;
	    }
	}
    }
    // After point consolidation, re-check for trivially degenerate triangles
    if (consolidated_points) {
	return 0;
    }
#endif

    /* For the non-zero-area triangles sharing an edge with a non-trivially
     * degenerate zero area triangle, we need to build new polygons from each
     * triangle and the "orphaned" points along the edge(s).  We then
     * re-tessellate in the triangle's parametric space.
     *
     * An "involved" triangle is a triangle with two of its three points in the
     * face's degen_pnts set (i.e. it shares an edge with a non-trivially
     * degenerate zero-area triangle.)
     */
    for (int face_index = 0; face_index != s_cdt->brep->m_F.Count(); face_index++) {
	struct ON_Brep_CDT_Face_State *f = (*s_cdt->faces)[face_index];
	triangles_rebuild_involved(f);
    }

    for (int face_index = 0; face_index != s_cdt->brep->m_F.Count(); face_index++) {
	struct ON_Brep_CDT_Face_State *f = (*s_cdt->faces)[face_index];
	triangle_cnt += f->cdt->GetTriangles().size();
	triangle_cnt -= f->tris_degen->size();
	triangle_cnt += f->p2t_extra_faces->size();
	/*if (f->p2t_extra_faces->size()) {
	    bu_log("adding %zd faces from %d\n", f->p2t_extra_faces->size(), face_index);
	}*/

	//bu_log("Face %d - %zu\n", face_index, triangle_cnt);
    }

    bu_log("tri_cnt: %zd\n", triangle_cnt);

    // Know how many faces and points now - initialize BoT container.
    *fcnt = (int)triangle_cnt;
    *faces = (int *)bu_calloc(triangle_cnt*3, sizeof(int), "new faces array");
    *vcnt = (int)s_cdt->vfpnts->size();
    *vertices = (fastf_t *)bu_calloc(s_cdt->vfpnts->size()*3, sizeof(fastf_t), "new vert array");
    if (normals) {
	*ncnt = (int)s_cdt->vfnormals->size();
	*normals = (fastf_t *)bu_calloc(s_cdt->vfnormals->size()*3, sizeof(fastf_t), "new normals array");
	*fn_cnt = (int)triangle_cnt;
	*face_normals = (int *)bu_calloc(triangle_cnt*3, sizeof(int), "new face_normals array");
    }

    for (size_t i = 0; i < s_cdt->vfpnts->size(); i++) {
	(*vertices)[i*3] = (*s_cdt->vfpnts)[i]->x;
	(*vertices)[i*3+1] = (*s_cdt->vfpnts)[i]->y;
	(*vertices)[i*3+2] = (*s_cdt->vfpnts)[i]->z;
    }

    if (normals) {
	for (size_t i = 0; i < s_cdt->vfnormals->size(); i++) {
	    (*normals)[i*3] = (*s_cdt->vfnormals)[i]->x;
	    (*normals)[i*3+1] = (*s_cdt->vfnormals)[i]->y;
	    (*normals)[i*3+2] = (*s_cdt->vfnormals)[i]->z;
	}
    }

    // Iterate over faces, adding points and faces to BoT container.  Note: all
    // 3D points should be geometrically unique in this final container.
    int face_cnt = 0;
    triangle_cnt = 0;
    for (int face_index = 0; face_index != s_cdt->brep->m_F.Count(); face_index++) {
	struct ON_Brep_CDT_Face_State *f = (*s_cdt->faces)[face_index];
	p2t::CDT *cdt = f->cdt;
	std::map<p2t::Point *, ON_3dPoint *> *pointmap = f->p2t_to_on3_map;
	std::vector<p2t::Triangle*> tris = cdt->GetTriangles();
	triangle_cnt += tris.size();
	int active_tris = 0;
	for (size_t i = 0; i < tris.size(); i++) {
	    p2t::Triangle *t = tris[i];
	    if (f->tris_degen->size() > 0 && f->tris_degen->find(t) != f->tris_degen->end()) {
		triangle_cnt--;
		continue;
	    }
	    active_tris++;
	    for (size_t j = 0; j < 3; j++) {
		p2t::Point *p = t->GetPoint(j);
		ON_3dPoint *op = (*pointmap)[p];
		int ind = (*s_cdt->on_pnt_to_bot_pnt)[op];
		(*faces)[face_cnt*3 + j] = ind;
		if (normals) {
		    int nind = (*s_cdt->on_pnt_to_bot_norm)[op];
		    (*face_normals)[face_cnt*3 + j] = nind;
		}
	    }
	    if (s_cdt->brep->m_F[face_index].m_bRev) {
		int ftmp = (*faces)[face_cnt*3 + 1];
		(*faces)[face_cnt*3 + 1] = (*faces)[face_cnt*3 + 2];
		(*faces)[face_cnt*3 + 2] = ftmp;
		if (normals) {
		    int fntmp = (*face_normals)[face_cnt*3 + 1];
		    (*face_normals)[face_cnt*3 + 1] = (*face_normals)[face_cnt*3 + 2];
		    (*face_normals)[face_cnt*3 + 2] = fntmp;
		}
	    }

	    face_cnt++;
	}
	//bu_log("initial face count for %d: %d\n", face_index, active_tris);
    }
    //bu_log("tri_cnt_1: %zd\n", triangle_cnt);
    //bu_log("face_cnt: %d\n", face_cnt);

    for (int face_index = 0; face_index != s_cdt->brep->m_F.Count(); face_index++) {
	std::vector<p2t::Triangle *> *tri_add = (*s_cdt->faces)[face_index]->p2t_extra_faces;
	std::map<p2t::Point *, ON_3dPoint *> *pointmap = (*s_cdt->faces)[face_index]->p2t_to_on3_map;
	if (!tri_add->size()) {
	    continue;
	}
	triangle_cnt += tri_add->size();
	for (size_t i = 0; i < tri_add->size(); i++) {
	    p2t::Triangle *t = (*tri_add)[i];
	    for (size_t j = 0; j < 3; j++) {
		p2t::Point *p = t->GetPoint(j);
		ON_3dPoint *op = (*pointmap)[p];
		int ind = (*s_cdt->on_pnt_to_bot_pnt)[op];
		(*faces)[face_cnt*3 + j] = ind;
		if (normals) {
		    int nind = (*s_cdt->on_pnt_to_bot_norm)[op];
		    (*face_normals)[face_cnt*3 + j] = nind;
		}
	    }
	    if (s_cdt->brep->m_F[face_index].m_bRev) {
		int ftmp = (*faces)[face_cnt*3 + 1];
		(*faces)[face_cnt*3 + 1] = (*faces)[face_cnt*3 + 2];
		(*faces)[face_cnt*3 + 2] = ftmp;
		if (normals) {
		    int fntmp = (*face_normals)[face_cnt*3 + 1];
		    (*face_normals)[face_cnt*3 + 1] = (*face_normals)[face_cnt*3 + 2];
		    (*face_normals)[face_cnt*3 + 2] = fntmp;
		}
	    }

	    face_cnt++;
	}
	//bu_log("added faces for %d: %zd\n", face_index, tri_add->size());
	//bu_log("tri_cnt_2: %zd\n", triangle_cnt);
	//bu_log("face_cnt_2: %d\n", face_cnt);


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

