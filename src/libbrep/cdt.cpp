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

	/* Provide 2D points for p2d, but we need to be aware that this will result
	 * in degenerate 3D faces that we need to filter out when assembling a mesh */
	if (trim->m_type == ON_BrepTrim::singular) {
	    BrepTrimPoint btp;
	    const ON_BrepVertex& v1 = face.Brep()->m_V[trim->m_vi[0]];
	    ON_3dPoint *p3d = (*s_cdt->vert_pnts)[v1.m_vertex_index];
	    (*s_cdt->strim_pnts)[face.m_face_index].insert(std::make_pair(trim->m_trim_index, p3d));
	    ON_3dPoint *n3d = (*s_cdt->vert_avg_norms)[v1.m_vertex_index];
	    if (n3d) {
		(*s_cdt->strim_norms)[face.m_face_index].insert(std::make_pair(trim->m_trim_index, n3d));
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
	    (void)getEdgePoints(s_cdt, edge, *trim, max_dist);
	    //bu_log("Initialized trim->m_trim_user.p: Trim %d (associated with Edge %d) point count: %zd\n", trim->m_trim_index, trim->Edge()->m_edge_index, m->size());
	}
	if (trim->m_trim_user.p) {
	    std::map<double, BrepTrimPoint *> *param_points3d = (std::map<double, BrepTrimPoint *> *) trim->m_trim_user.p;
	    //bu_log("Trim %d (associated with Edge %d) point count: %zd\n", trim->m_trim_index, trim->Edge()->m_edge_index, param_points3d->size());

	    ON_3dPoint boxmin;
	    ON_3dPoint boxmax;

	    if (trim->GetBoundingBox(boxmin, boxmax, false)) {
		double t0, t1;

		std::map<double, BrepTrimPoint*>::const_iterator i;
		std::map<double, BrepTrimPoint*>::const_iterator ni;

		trim->GetDomain(&t0, &t1);
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
}


static int
ON_Brep_CDT_Face(struct ON_Brep_CDT_State *s_cdt, std::map<const ON_Surface *, double> *s_to_maxdist, ON_BrepFace &face)
{
    ON_RTree rt_trims;
    ON_2dPointArray on_surf_points;
    const ON_Surface *s = face.SurfaceOf();
    int loop_cnt = face.LoopCount();
    ON_2dPointArray on_loop_points;
    ON_SimpleArray<BrepTrimPoint> *brep_loop_points = new ON_SimpleArray<BrepTrimPoint>[loop_cnt];
    std::map<p2t::Point *, ON_3dPoint *> *pointmap = new std::map<p2t::Point *, ON_3dPoint *>();
    std::map<ON_3dPoint *, std::set<p2t::Point *>> *on3_to_tri = new std::map<ON_3dPoint *, std::set<p2t::Point *>>();
    std::map<p2t::Point *, ON_3dPoint *> *normalmap = new std::map<p2t::Point *, ON_3dPoint *>();
    std::vector<p2t::Point*> polyline;
    p2t::CDT* cdt = NULL;

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
    std::list<std::map<double, ON_3dPoint *> *> bridgePoints;
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

    // Process through loops, building Poly2Tri polygons for facetization.
    bool outer = true;
    for (int li = 0; li < loop_cnt; li++) {
	int num_loop_points = brep_loop_points[li].Count();
	if (num_loop_points > 2) {
	    for (int i = 1; i < num_loop_points; i++) {
		// map point to last entry to 3d point
		p2t::Point *p = new p2t::Point((brep_loop_points[li])[i].p2d.x, (brep_loop_points[li])[i].p2d.y);
		polyline.push_back(p);
		(*pointmap)[p] = (brep_loop_points[li])[i].p3d;
		(*on3_to_tri)[(brep_loop_points[li])[i].p3d].insert(p);
		(*normalmap)[p] = (brep_loop_points[li])[i].n3d;
	    }
	    for (int i = 1; i < brep_loop_points[li].Count(); i++) {
		// map point to last entry to 3d point
		ON_Line *line = new ON_Line((brep_loop_points[li])[i - 1].p2d, (brep_loop_points[li])[i].p2d);
		ON_BoundingBox bb = line->BoundingBox();

		bb.m_max.x = bb.m_max.x + ON_ZERO_TOLERANCE;
		bb.m_max.y = bb.m_max.y + ON_ZERO_TOLERANCE;
		bb.m_max.z = bb.m_max.z + ON_ZERO_TOLERANCE;
		bb.m_min.x = bb.m_min.x - ON_ZERO_TOLERANCE;
		bb.m_min.y = bb.m_min.y - ON_ZERO_TOLERANCE;
		bb.m_min.z = bb.m_min.z - ON_ZERO_TOLERANCE;

		rt_trims.Insert2d(bb.Min(), bb.Max(), line);
	    }
	    if (outer) {
		cdt = new p2t::CDT(polyline);
		outer = false;
	    } else {
		cdt->AddHole(polyline);
	    }
	    polyline.clear();
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

    if (outer) {
	std::cerr << "Error: Face(" << face.m_face_index << ") cannot evaluate its outer loop and will not be facetized." << std::endl;
	return -1;
    }

    // Sample the surface, independent of the trimming curves, to get points that
    // will tie the mesh to the interior surface.
    getSurfacePoints(s_cdt, face, on_surf_points);

    // Strip out points from the surface that are on the trimming curves.  Trim
    // points require special handling for watertightness and introducing them
    // from the surface also runs the risk of adding duplicate 2D points, which
    // aren't allowed for facetization.
    for (int i = 0; i < on_surf_points.Count(); i++) {
	ON_SimpleArray<void*> results;
	const ON_2dPoint *p = on_surf_points.At(i);

	rt_trims.Search2d((const double *) p, (const double *) p, results);

	if (results.Count() > 0) {
	    bool on_edge = false;
	    for (int ri = 0; ri < results.Count(); ri++) {
		double dist;
		const ON_Line *l = (const ON_Line *) *results.At(ri);
		dist = l->MinimumDistanceTo(*p);
		if (NEAR_ZERO(dist, s_cdt->dist)) {
		    on_edge = true;
		    break;
		}
	    }
	    if (!on_edge) {
		cdt->AddPoint(new p2t::Point(p->x, p->y));
	    }
	} else {
	    cdt->AddPoint(new p2t::Point(p->x, p->y));
	}
    }

    ON_SimpleArray<void*> results;
    ON_BoundingBox bb = rt_trims.BoundingBox();

    rt_trims.Search2d((const double *) bb.m_min, (const double *) bb.m_max,
	    results);

    if (results.Count() > 0) {
	for (int ri = 0; ri < results.Count(); ri++) {
	    const ON_Line *l = (const ON_Line *)*results.At(ri);
	    delete l;
	}
    }
    rt_trims.RemoveAll();

    // All preliminary steps are complete, perform the triangulation using
    // Poly2Tri's triangulation.  NOTE: it is important that the inputs to
    // Poly2Tri satisfy its constraints - failure here could cause a crash.
    cdt->Triangulate(true, -1);

    /* Calculate any 3D points we don't already have from the loop processing */
    std::vector<p2t::Triangle*> tris = cdt->GetTriangles();
    for (size_t i = 0; i < tris.size(); i++) {

	p2t::Triangle *t = tris[i];

	for (size_t j = 0; j < 3; j++) {

	    p2t::Point *p = t->GetPoint(j);
	    if (!p) {
		bu_log("Face %d: p2t face without proper point info...\n", face.m_face_index);
		continue;
	    }

	    ON_3dPoint *op = (*pointmap)[p];
	    ON_3dPoint *onorm = (*normalmap)[p];
	    if (op && onorm) {
		// We have what we need
		continue;
	    }

	    const ON_Surface *surf = face.SurfaceOf();
	    ON_3dPoint pnt;
	    ON_3dVector norm;
	    if (surface_EvNormal(surf, p->x, p->y, pnt, norm)) {
		// Have point and normal from surface - store
		if (face.m_bRev) {
		    norm = norm * -1.0;
		}
		if (!op) {
		    op = new ON_3dPoint(pnt);
		    CDT_Add3DPnt(s_cdt, op, face.m_face_index, -1, -1, -1, p->x, p->y);
		    (*pointmap)[p] = op;
		}
		if (!onorm) {
		    onorm = new ON_3dPoint(norm);
		    s_cdt->w3dnorms->push_back(onorm);
		    (*normalmap)[p] = onorm;
		}
	    } else {
		// surface_EvNormal failed - fall back on PointAt
		if (!op) {
		    pnt = s->PointAt(p->x, p->y);
		    op = new ON_3dPoint(pnt);
		    CDT_Add3DPnt(s_cdt, op, face.m_face_index, -1, -1, -1, p->x, p->y);
		}
		(*pointmap)[p] = op;
	    }

	}

    }

    /* Stash mappings for BoT reassembly.  Because there may be subsequent
     * refinement in overlap clearing operations, we avoid immediately
     * generating the mesh. */
    int face_index = face.m_face_index;
    s_cdt->p2t_faces[face_index] = cdt;
    s_cdt->tri_to_on3_maps[face_index] = pointmap;
    s_cdt->on3_to_tri_maps[face_index] = on3_to_tri;
    s_cdt->tri_to_on3_norm_maps[face_index] = normalmap;
    s_cdt->brep_face_loop_points[face_index] = brep_loop_points;

    return 0;
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
    }
    ON_Brep* brep = s_cdt->brep;

    // Have observed at least one case (NIST2 face 237) where a small face on a
    // large surface resulted in a valid 2D triangulation that produced a
    // self-intersecting 3D mesh.  Attempt to minimize situations where 2D and
    // 3D distances get out of sync by shrinking the surfaces down to the
    // active area of the face
    //
    // TODO - we may want to do this only for faces where the surface size is
    // much larger than the bounded trimming curves in 2D, rather than just
    // shrinking everything...
    brep->ShrinkSurfaces();

    // Reparameterize the face's surface and transform the "u" and "v"
    // coordinates of all the face's parameter space trimming curves to
    // minimize distortion in the map from parameter space to 3d..
    if (!s_cdt->w3dpnts->size()) {
	for (int face_index = 0; face_index < brep->m_F.Count(); face_index++) {
	    ON_BrepFace *face = brep->Face(face_index);
	    const ON_Surface *s = face->SurfaceOf();
	    double surface_width, surface_height;
	    if (s->GetSurfaceSize(&surface_width, &surface_height)) {
		face->SetDomain(0, 0.0, surface_width);
		face->SetDomain(1, 0.0, surface_height);
	    }
	}
    }

    /* We want to use ON_3dPoint pointers and BrepVertex points, but
     * vert->Point() produces a temporary address.  If this is our first time
     * through, make stable copies of the Vertex points. */
    if (!s_cdt->w3dpnts->size()) {
	for (int index = 0; index < brep->m_V.Count(); index++) {
	    ON_BrepVertex& v = brep->m_V[index];
	    (*s_cdt->vert_pnts)[index] = new ON_3dPoint(v.Point());
	    CDT_Add3DPnt(s_cdt, (*s_cdt->vert_pnts)[index], -1, v.m_vertex_index, -1, -1, -1, -1);
	    // topologically, any vertex point will be on edges
	    s_cdt->edge_pnts->insert((*s_cdt->vert_pnts)[index]);
	}
    }

    /* If this is the first time through, get vertex normals that are the
     * average of the surface normals at the junction from faces that don't use
     * a singular trim to reference the vertex.  When subdividing the edges, we
     * use the normals as a curvature guide.
     */
    if (!s_cdt->vert_avg_norms->size()) {
	for (int index = 0; index < brep->m_V.Count(); index++) {
	    ON_BrepVertex& v = brep->m_V[index];
	    int have_calculated = 0;
	    ON_3dVector vnrml(0.0, 0.0, 0.0);

	    for (int eind = 0; eind != v.EdgeCount(); eind++) {
		ON_3dVector trim1_norm = ON_3dVector::UnsetVector;
		ON_3dVector trim2_norm = ON_3dVector::UnsetVector;
		ON_BrepEdge& edge = brep->m_E[v.m_ei[eind]];
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
		ON_3dPoint *t1pnt = new ON_3dPoint(trim1_norm);
		(*s_cdt->vert_face_norms)[v.m_vertex_index][trim1->Face()->m_face_index].insert(t1pnt);
		s_cdt->w3dnorms->push_back(t1pnt);
		ON_3dPoint *t2pnt = new ON_3dPoint(trim2_norm);
		(*s_cdt->vert_face_norms)[v.m_vertex_index][trim2->Face()->m_face_index].insert(t2pnt);
		s_cdt->w3dnorms->push_back(t2pnt);

		// Add the normals to the vnrml total
		vnrml += trim1_norm;
		vnrml += trim2_norm;
		have_calculated = 1;

	    }
	    if (!have_calculated) {
		continue;
	    }

	    // Average all the successfully calculated normals into a new unit normal
	    vnrml.Unitize();

	    // We store this as a point to keep C++ happy...  If we try to
	    // propagate the ON_3dVector type through all the CDT logic it
	    // triggers issues with the compile.
	    (*s_cdt->vert_avg_norms)[index] = new ON_3dPoint(vnrml);
	    s_cdt->w3dnorms->push_back((*s_cdt->vert_avg_norms)[index]);
	}
    }

    /* To generate watertight meshes, the faces must share 3D edge points.  To ensure
     * a uniform set of edge points, we first sample all the edges and build their
     * point sets */
    std::map<const ON_Surface *, double> s_to_maxdist;
    for (int index = 0; index < brep->m_E.Count(); index++) {
        ON_BrepEdge& edge = brep->m_E[index];
        ON_BrepTrim& trim1 = brep->m_T[edge.Trim(0)->m_trim_index];
        ON_BrepTrim& trim2 = brep->m_T[edge.Trim(1)->m_trim_index];

        // Get distance tolerances from the surface sizes
        fastf_t max_dist = 0.0;
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
            s_to_maxdist[s1] = max_dist;
            s_to_maxdist[s2] = max_dist;
        }

        // Generate the BrepTrimPoint arrays for both trims associated with this edge
	//
	// TODO - need to make this robust to changed tolerances on refinement
	// runs - if pre-existing solutions for "high level" splits exist,
	// reuse them and dig down to find where we need further refinement to
	// create new points.
        (void)getEdgePoints(s_cdt, &edge, trim1, max_dist);

    }


    // Process all of the faces we have been instructed to process, or (default) all faces.
    // Keep track of failures and successes.
    int face_failures = 0;
    int face_successes = 0;

    if ((face_cnt == 0) || !faces) {

	for (int face_index = 0; face_index < s_cdt->brep->m_F.Count(); face_index++) {
	    ON_BrepFace &face = brep->m_F[face_index];

	    if (ON_Brep_CDT_Face(s_cdt, &s_to_maxdist, face)) {
		face_failures++;
	    } else {
		face_successes++;
	    }

	}
    } else {
	for (int i = 0; i < face_cnt; i++) {
	    if (faces[i] < s_cdt->brep->m_F.Count()) {
		ON_BrepFace &face = brep->m_F[faces[i]];
		if (ON_Brep_CDT_Face(s_cdt, &s_to_maxdist, face)) {
		    face_failures++;
		} else {
		    face_successes++;
		}
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

    if (!invalid) {
	goto cdt_done;
    }

    if (se.degenerate.count > 0) {
	std::set<int> problem_pnts;
	std::set<int>::iterator pp_it;
	bu_log("%d degenerate faces\n", se.degenerate.count);
	for (int i = 0; i < se.degenerate.count; i++) {
	    int face = se.degenerate.faces[i];
	    bu_log("dface %d: %d %d %d :  %f %f %f->%f %f %f->%f %f %f \n", face, valid_faces[face*3], valid_faces[face*3+1], valid_faces[face*3+2],
		    valid_vertices[valid_faces[face*3]*3], valid_vertices[valid_faces[face*3]*3+1],valid_vertices[valid_faces[face*3]*3+2],
		    valid_vertices[valid_faces[face*3+1]*3], valid_vertices[valid_faces[face*3+1]*3+1],valid_vertices[valid_faces[face*3+1]*3+2],
		    valid_vertices[valid_faces[face*3+2]*3], valid_vertices[valid_faces[face*3+2]*3+1],valid_vertices[valid_faces[face*3+2]*3+2]);
	    problem_pnts.insert(valid_faces[face*3]);
	    problem_pnts.insert(valid_faces[face*3+1]);
	    problem_pnts.insert(valid_faces[face*3+2]);
	}
	for (pp_it = problem_pnts.begin(); pp_it != problem_pnts.end(); pp_it++) {
	    int pind = (*pp_it);
	    ON_3dPoint *p = (*s_cdt->vert_to_on)[pind];
	    if (!p) {
		bu_log("unmapped point??? %d\n", pind);
	    } else {
		struct cdt_audit_info *paudit = s_cdt->pnt_audit_info[p];
		if (!paudit) {
		    bu_log("point with no audit info??? %d\n", pind);
		} else {
		    bu_log("point %d: Face(%d) Vert(%d) Trim(%d) Edge(%d) UV(%f,%f)\n", pind, paudit->face_index, paudit->vert_index, paudit->trim_index, paudit->edge_index, paudit->surf_uv.x, paudit->surf_uv.y);
		}
	    }
	}
    }
    if (se.excess.count > 0) {
	bu_log("extra edges???\n");
    }
    if (se.unmatched.count > 0) {
	std::set<int> problem_pnts;
	std::set<int>::iterator pp_it;

	bu_log("%d unmatched edges\n", se.unmatched.count);
	for (int i = 0; i < se.unmatched.count; i++) {
	    int v1 = se.unmatched.edges[i*2];
	    int v2 = se.unmatched.edges[i*2+1];
	    bu_log("%d->%d: %f %f %f->%f %f %f\n", v1, v2,
		    valid_vertices[v1*3], valid_vertices[v1*3+1], valid_vertices[v1*3+2],
		    valid_vertices[v2*3], valid_vertices[v2*3+1], valid_vertices[v2*3+2]
		  );
	    for (int j = 0; j < valid_fcnt; j++) {
		std::set<std::pair<int,int>> fedges;
		fedges.insert(std::pair<int,int>(valid_faces[j*3],valid_faces[j*3+1]));
		fedges.insert(std::pair<int,int>(valid_faces[j*3+1],valid_faces[j*3+2]));
		fedges.insert(std::pair<int,int>(valid_faces[j*3+2],valid_faces[j*3]));
		int has_edge = (fedges.find(std::pair<int,int>(v1,v2)) != fedges.end()) ? 1 : 0;
		if (has_edge) {
		    int face = j;
		    bu_log("eface %d: %d %d %d :  %f %f %f->%f %f %f->%f %f %f \n", face, valid_faces[face*3], valid_faces[face*3+1], valid_faces[face*3+2],
			    valid_vertices[valid_faces[face*3]*3], valid_vertices[valid_faces[face*3]*3+1],valid_vertices[valid_faces[face*3]*3+2],
			    valid_vertices[valid_faces[face*3+1]*3], valid_vertices[valid_faces[face*3+1]*3+1],valid_vertices[valid_faces[face*3+1]*3+2],
			    valid_vertices[valid_faces[face*3+2]*3], valid_vertices[valid_faces[face*3+2]*3+1],valid_vertices[valid_faces[face*3+2]*3+2]);
		    problem_pnts.insert(valid_faces[j*3]);
		    problem_pnts.insert(valid_faces[j*3+1]);
		    problem_pnts.insert(valid_faces[j*3+2]);
		}
	    }
	}
	for (pp_it = problem_pnts.begin(); pp_it != problem_pnts.end(); pp_it++) {
	    int pind = (*pp_it);
	    ON_3dPoint *p = (*s_cdt->vert_to_on)[pind];
	    if (!p) {
		bu_log("unmapped point??? %d\n", pind);
	    } else {
		struct cdt_audit_info *paudit = s_cdt->pnt_audit_info[p];
		if (!paudit) {
		    bu_log("point with no audit info??? %d\n", pind);
		} else {
		    bu_log("point %d: Face(%d) Vert(%d) Trim(%d) Edge(%d) UV(%f,%f)\n", pind, paudit->face_index, paudit->vert_index, paudit->trim_index, paudit->edge_index, paudit->surf_uv.x, paudit->surf_uv.y);
		}
	    }
	}
    }
    if (se.misoriented.count > 0) {
	std::set<int> problem_pnts;
	std::set<int>::iterator pp_it;

	bu_log("%d misoriented edges\n", se.misoriented.count);
	for (int i = 0; i < se.misoriented.count; i++) {
	    int v1 = se.misoriented.edges[i*2];
	    int v2 = se.misoriented.edges[i*2+1];
	    bu_log("%d->%d: %f %f %f->%f %f %f\n", v1, v2,
		    valid_vertices[v1*3], valid_vertices[v1*3+1], valid_vertices[v1*3+2],
		    valid_vertices[v2*3], valid_vertices[v2*3+1], valid_vertices[v2*3+2]
		  );
	    for (int j = 0; j < valid_fcnt; j++) {
		std::set<std::pair<int,int>> fedges;
		fedges.insert(std::pair<int,int>(valid_faces[j*3],valid_faces[j*3+1]));
		fedges.insert(std::pair<int,int>(valid_faces[j*3+1],valid_faces[j*3+2]));
		fedges.insert(std::pair<int,int>(valid_faces[j*3+2],valid_faces[j*3]));
		int has_edge = (fedges.find(std::pair<int,int>(v1,v2)) != fedges.end()) ? 1 : 0;
		if (has_edge) {
		    int face = j;
		    bu_log("eface %d: %d %d %d :  %f %f %f->%f %f %f->%f %f %f \n", face, valid_faces[face*3], valid_faces[face*3+1], valid_faces[face*3+2],
			    valid_vertices[valid_faces[face*3]*3], valid_vertices[valid_faces[face*3]*3+1],valid_vertices[valid_faces[face*3]*3+2],
			    valid_vertices[valid_faces[face*3+1]*3], valid_vertices[valid_faces[face*3+1]*3+1],valid_vertices[valid_faces[face*3+1]*3+2],
			    valid_vertices[valid_faces[face*3+2]*3], valid_vertices[valid_faces[face*3+2]*3+1],valid_vertices[valid_faces[face*3+2]*3+2]);
		    problem_pnts.insert(valid_faces[j*3]);
		    problem_pnts.insert(valid_faces[j*3+1]);
		    problem_pnts.insert(valid_faces[j*3+2]);
		}
	    }
	}
	for (pp_it = problem_pnts.begin(); pp_it != problem_pnts.end(); pp_it++) {
	    int pind = (*pp_it);
	    ON_3dPoint *p = (*s_cdt->vert_to_on)[pind];
	    if (!p) {
		bu_log("unmapped point??? %d\n", pind);
	    } else {
		struct cdt_audit_info *paudit = s_cdt->pnt_audit_info[p];
		if (!paudit) {
		    bu_log("point with no audit info??? %d\n", pind);
		} else {
		    bu_log("point %d: Face(%d) Vert(%d) Trim(%d) Edge(%d) UV(%f,%f)\n", pind, paudit->face_index, paudit->vert_index, paudit->trim_index, paudit->edge_index, paudit->surf_uv.x, paudit->surf_uv.y);
		}
	    }
	}
    }

cdt_done:
    bu_free(valid_faces, "faces");
    bu_free(valid_vertices, "vertices");

    if (invalid) {
	return 1;
    }

    return 0;

}

static int
ON_Brep_CDT_VList_Face(
	struct bu_list *vhead,
	struct bu_list *vlfree,
	int face_index,
	int mode,
	const struct ON_Brep_CDT_State *s)
{
    point_t pt[3] = {VINIT_ZERO, VINIT_ZERO, VINIT_ZERO};
    vect_t nv[3] = {VINIT_ZERO, VINIT_ZERO, VINIT_ZERO};
    point_t pt1 = VINIT_ZERO;
    point_t pt2 = VINIT_ZERO;

    p2t::CDT *cdt = s->p2t_faces[face_index];
    std::map<p2t::Point *, ON_3dPoint *> *pointmap = s->tri_to_on3_maps[face_index];
    std::map<p2t::Point *, ON_3dPoint *> *normalmap = s->tri_to_on3_norm_maps[face_index];
    std::vector<p2t::Triangle*> tris = cdt->GetTriangles();

    switch (mode) {
	case 0:
	    // 3D shaded triangles
	    for (size_t i = 0; i < tris.size(); i++) {
		p2t::Triangle *t = tris[i];
		p2t::Point *p = NULL;
		for (size_t j = 0; j < 3; j++) {
		    p = t->GetPoint(j);
		    ON_3dPoint *op = (*pointmap)[p];
		    ON_3dPoint *onorm = (*normalmap)[p];
		    VSET(pt[j], op->x, op->y, op->z);
		    VSET(nv[j], onorm->x, onorm->y, onorm->z);
		}
		//tri one
		BN_ADD_VLIST(vlfree, vhead, nv[0], BN_VLIST_TRI_START);
		BN_ADD_VLIST(vlfree, vhead, nv[0], BN_VLIST_TRI_VERTNORM);
		BN_ADD_VLIST(vlfree, vhead, pt[0], BN_VLIST_TRI_MOVE);
		BN_ADD_VLIST(vlfree, vhead, nv[1], BN_VLIST_TRI_VERTNORM);
		BN_ADD_VLIST(vlfree, vhead, pt[1], BN_VLIST_TRI_DRAW);
		BN_ADD_VLIST(vlfree, vhead, nv[2], BN_VLIST_TRI_VERTNORM);
		BN_ADD_VLIST(vlfree, vhead, pt[2], BN_VLIST_TRI_DRAW);
		BN_ADD_VLIST(vlfree, vhead, pt[0], BN_VLIST_TRI_END);
		//bu_log("Face %d, Tri %zd: %f/%f/%f-%f/%f/%f -> %f/%f/%f-%f/%f/%f -> %f/%f/%f-%f/%f/%f\n", face_index, i, V3ARGS(pt[0]), V3ARGS(nv[0]), V3ARGS(pt[1]), V3ARGS(nv[1]), V3ARGS(pt[2]), V3ARGS(nv[2]));
	    }
	    break;
	case 1:
	    // 3D wireframe
	    for (size_t i = 0; i < tris.size(); i++) {
		p2t::Triangle *t = tris[i];
		p2t::Point *p = NULL;
		for (size_t j = 0; j < 3; j++) {
		    p = t->GetPoint(j);
		    ON_3dPoint *op = (*pointmap)[p];
		    VSET(pt[j], op->x, op->y, op->z);
		}
		//tri one
		BN_ADD_VLIST(vlfree, vhead, pt[0], BN_VLIST_LINE_MOVE);
		BN_ADD_VLIST(vlfree, vhead, pt[1], BN_VLIST_LINE_DRAW);
		BN_ADD_VLIST(vlfree, vhead, pt[2], BN_VLIST_LINE_DRAW);
		BN_ADD_VLIST(vlfree, vhead, pt[0], BN_VLIST_LINE_DRAW);
	    }
	    break;
	case 2:
	    // 2D wireframe
	    for (size_t i = 0; i < tris.size(); i++) {
		p2t::Triangle *t = tris[i];
		p2t::Point *p = NULL;

		for (size_t j = 0; j < 3; j++) {
		    if (j == 0) {
			p = t->GetPoint(2);
		    } else {
			p = t->GetPoint(j - 1);
		    }
		    pt1[0] = p->x;
		    pt1[1] = p->y;
		    pt1[2] = 0.0;
		    p = t->GetPoint(j);
		    pt2[0] = p->x;
		    pt2[1] = p->y;
		    pt2[2] = 0.0;
		    BN_ADD_VLIST(vlfree, vhead, pt1, BN_VLIST_LINE_MOVE);
		    BN_ADD_VLIST(vlfree, vhead, pt2, BN_VLIST_LINE_DRAW);
		}
	    }
	    break;
	default:
	    return -1;
    }

    return 0;
}

int ON_Brep_CDT_VList(
	struct bn_vlblock *vbp,
	struct bu_list *vlfree,
	struct bu_color *c,
	int mode,
	const struct ON_Brep_CDT_State *s)
{
    int r, g, b;
    struct bu_list *vhead = NULL;
    int have_color = 0;

   if (UNLIKELY(!c) || mode < 0) {
       return -1;
   }

   have_color = bu_color_to_rgb_ints(c, &r, &g, &b);

   if (UNLIKELY(!have_color)) {
       return -1;
   }

   vhead = bn_vlblock_find(vbp, r, g, b);

   if (UNLIKELY(!vhead)) {
       return -1;
   }

   for (int i = 0; i < s->brep->m_F.Count(); i++) {
       if (s->p2t_faces[i]) {
	   (void)ON_Brep_CDT_VList_Face(vhead, vlfree, i, mode, s);
       }
   }

   return 0;
}

static ON_3dVector
p2tTri_Normal(p2t::Triangle *t, std::map<p2t::Point *, ON_3dPoint *> *pointmap)
{
    ON_3dPoint *p1 = (*pointmap)[t->GetPoint(0)];
    ON_3dPoint *p2 = (*pointmap)[t->GetPoint(1)];
    ON_3dPoint *p3 = (*pointmap)[t->GetPoint(2)];

    ON_3dVector e1 = *p2 - *p1;
    ON_3dVector e2 = *p3 - *p1;
    ON_3dVector tdir = ON_CrossProduct(e1, e2);
    tdir.Unitize();
    return tdir;
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

    std::vector<ON_3dPoint *> vfpnts;
    std::vector<ON_3dPoint *> vfnormals;
    std::map<ON_3dPoint *, int> on_pnt_to_bot_pnt;
    std::map<ON_3dPoint *, int> on_pnt_to_bot_norm;
    std::set<p2t::Triangle*> tris_degen;
    std::set<p2t::Triangle*> tris_zero_3D_area;
    std::map<p2t::Triangle*, int> tri_brep_face;
    size_t triangle_cnt = 0;
    EdgeToTri *e2f = new EdgeToTri;

    for (int face_index = 0; face_index != s_cdt->brep->m_F.Count(); face_index++) {
	ON_BrepFace &face = s_cdt->brep->m_F[face_index];
	p2t::CDT *cdt = s_cdt->p2t_faces[face_index];
	std::map<p2t::Point *, ON_3dPoint *> *pointmap = s_cdt->tri_to_on3_maps[face_index];
	std::map<p2t::Point *, ON_3dPoint *> *normalmap = s_cdt->tri_to_on3_norm_maps[face_index];
	std::vector<p2t::Triangle*> tris = cdt->GetTriangles();
	triangle_cnt += tris.size();
	for (size_t i = 0; i < tris.size(); i++) {
	    p2t::Triangle *t = tris[i];
	    ON_3dVector tdir = p2tTri_Normal(t, pointmap);
	    tri_brep_face[t] = face_index;
	    ON_3dPoint *tpnts[3] = {NULL, NULL, NULL};
	    for (size_t j = 0; j < 3; j++) {
		p2t::Point *p = t->GetPoint(j);
		if (p) {
		    ON_3dPoint *op = (*pointmap)[p];
		    ON_3dPoint *onorm = (*normalmap)[p];
		    if (!op || !onorm) {
			/* We've got some calculating to do... */
			const ON_Surface *s = face.SurfaceOf();
			ON_3dPoint pnt;
			ON_3dVector norm;
			if (surface_EvNormal(s, p->x, p->y, pnt, norm)) {
			    if (face.m_bRev) {
				norm = norm * -1.0;
			    }
			    if (!op) {
				op = new ON_3dPoint(pnt);
				CDT_Add3DPnt(s_cdt, op, face.m_face_index, -1, -1, -1, p->x, p->y);
				vfpnts.push_back(op);
				(*pointmap)[p] = op;
				on_pnt_to_bot_pnt[op] = vfpnts.size() - 1;
				(*s_cdt->vert_to_on)[vfpnts.size() - 1] = op;
			    }
			    if (!onorm) {
				onorm = new ON_3dPoint(norm);
				s_cdt->w3dnorms->push_back(onorm);
				vfnormals.push_back(onorm);
				(*normalmap)[p] = onorm;
				on_pnt_to_bot_norm[op] = vfnormals.size() - 1;
			    }
			} else {
			    bu_log("Erm... eval failed, no normal info?\n");
			    if (!op) {
				pnt = s->PointAt(p->x, p->y);
				op = new ON_3dPoint(pnt);
				CDT_Add3DPnt(s_cdt, op, face.m_face_index, -1, -1, -1, p->x, p->y);
				vfpnts.push_back(op);
				(*pointmap)[p] = op;
				on_pnt_to_bot_pnt[op] = vfpnts.size() - 1;
				(*s_cdt->vert_to_on)[vfpnts.size() - 1] = op;
			    } else {
				if (on_pnt_to_bot_pnt.find(op) == on_pnt_to_bot_pnt.end()) {
				    vfpnts.push_back(op);
				    (*pointmap)[p] = op;
				    on_pnt_to_bot_pnt[op] = vfpnts.size() - 1;
				    (*s_cdt->vert_to_on)[vfpnts.size() - 1] = op;
				}
			    }
			}
		    } else {
			/* We've got them already, just add them */ 
			if (on_pnt_to_bot_pnt.find(op) == on_pnt_to_bot_pnt.end()) {
			    vfpnts.push_back(op);
			    vfnormals.push_back(onorm);
			    on_pnt_to_bot_pnt[op] = vfpnts.size() - 1;
			    (*s_cdt->vert_to_on)[vfpnts.size() - 1] = op;
			    on_pnt_to_bot_norm[op] = vfnormals.size() - 1;
			}
		    }
		    tpnts[j] = op;

		    // TODO - validate onorm against triangle face
		    if (tdir.Length() > 0 && ON_DotProduct(*onorm, tdir) < 0) {
			ON_3dPoint tri_cent = (*(*pointmap)[t->GetPoint(0)] + *(*pointmap)[t->GetPoint(1)] + *(*pointmap)[t->GetPoint(2)])/3;
			bu_log("Normal in wrong direction:\n");
			bu_log("Tri p1: %f %f %f\n", (*pointmap)[t->GetPoint(0)]->x, (*pointmap)[t->GetPoint(0)]->y, (*pointmap)[t->GetPoint(0)]->z);
			bu_log("Tri p2: %f %f %f\n", (*pointmap)[t->GetPoint(1)]->x, (*pointmap)[t->GetPoint(1)]->y, (*pointmap)[t->GetPoint(1)]->z);
			bu_log("Tri p3: %f %f %f\n", (*pointmap)[t->GetPoint(2)]->x, (*pointmap)[t->GetPoint(2)]->y, (*pointmap)[t->GetPoint(2)]->z);
			bu_log("Tri center: %f %f %f\n", tri_cent.x, tri_cent.y, tri_cent.z);
			bu_log("Tri norm: %f %f %f\n", tdir.x, tdir.y, tdir.z);
			bu_log("onorm: %f %f %f\n", onorm->x, onorm->y, onorm->z);
		    }
		    
		} else {
		    bu_log("Face %d: p2t face without proper point info...\n", face.m_face_index);
		}
	    }

	    /* Now that all 3D points are mapped, make sure this face isn't
	     * trivially degenerate (this can happen with singular trims) */
	    if (tpnts[0] == tpnts[1] || tpnts[1] == tpnts[2] || tpnts[2] == tpnts[0]) {
		/* degenerate */
		triangle_cnt--;
		tris_degen.insert(t);
		continue;
	    }

	    add_tri_edges(e2f, tris[i], pointmap);

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

    // We may still have a situation where three colinear points form a zero
    // area face.  This is more complex to deal with, as it requires modifying
    // non-degenerate faces in the neighborhood to incorporate different points.
    for (int face_index = 0; face_index != s_cdt->brep->m_F.Count(); face_index++) {
	// Use a distance three orders of magnitude smaller than the smallest
	// edge segment length of the face to decide if a face is degenerate
	// based on area.
	fastf_t dist = 0.001 * (*s_cdt->min_edge_seg_len)[face_index];
	p2t::CDT *cdt = s_cdt->p2t_faces[face_index];
	std::map<p2t::Point *, ON_3dPoint *> *pointmap = s_cdt->tri_to_on3_maps[face_index];
	std::vector<p2t::Triangle*> tris = cdt->GetTriangles();
	for (size_t i = 0; i < tris.size(); i++) {
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

	    /* If we have a face with 3 shared or co-linear points, it's not
	     * trivially degenerate and we need to do more work.  (This can
	     * arise when the 2D triangulation has a non-degenerate triangle
	     * that maps degenerately into 3D). For now, just build up the set
	     * of degenerate triangles. */
	    ON_Line l(*tpnts[0], *tpnts[2]);
	    int is_zero_area = 0;
	    if (l.Length() < dist) {
		ON_Line l2(*tpnts[0], *tpnts[1]);
		if (l2.Length() < dist) {
		    bu_log("completely degenerate triangle\n");
		    triangle_cnt--;
		    tris_degen.insert(t);
		    continue;
		} else {
		    if (l2.DistanceTo(*tpnts[2]) < dist) {
			is_zero_area = 1;
		    }
		}
	    } else {
		if (l.DistanceTo(*tpnts[1]) < dist) {
		    is_zero_area = 1;
		}
	    }
	    if (is_zero_area) {
		// The edges from this face are degenerate edges
		p2t::Point *p2_A = t->GetPoint(0);
		p2t::Point *p2_B = t->GetPoint(1);
		p2t::Point *p2_C = t->GetPoint(2);

		if (!s_cdt->face_degen_pnts[face_index]) {
		    s_cdt->face_degen_pnts[face_index] = new std::set<p2t::Point *>;
		}
		s_cdt->face_degen_pnts[face_index]->insert(p2_A);
		s_cdt->face_degen_pnts[face_index]->insert(p2_B);
		s_cdt->face_degen_pnts[face_index]->insert(p2_C);

		/* If we have degeneracies along an edge, the impact is not
		 * local to this face but will also impact the other face.
		 * Find it and let it know.(probably need another map - 3d pnt
		 * to trim points...) */
		ON_3dPoint *pt_A = (*pointmap)[p2_A];
		ON_3dPoint *pt_B = (*pointmap)[p2_B];
		ON_3dPoint *pt_C = (*pointmap)[p2_C];
		std::set<BrepTrimPoint *>::iterator bit;
		std::set<BrepTrimPoint *> &pAt = s_cdt->on_brep_edge_pnts[pt_A];
		std::set<BrepTrimPoint *> &pBt = s_cdt->on_brep_edge_pnts[pt_B];
		std::set<BrepTrimPoint *> &pCt = s_cdt->on_brep_edge_pnts[pt_C];
		for (bit = pAt.begin(); bit != pAt.end(); bit++) {
		    BrepTrimPoint *tpt = *bit;
		    int f2ind = s_cdt->brep->m_T[tpt->trim_ind].Face()->m_face_index;
		    if (f2ind != face_index) {
			//bu_log("Pulls in face %d\n", f2ind);
			if (!s_cdt->face_degen_pnts[f2ind]) {
			    s_cdt->face_degen_pnts[f2ind] = new std::set<p2t::Point *>;
			}
			std::set<p2t::Point *> tri_pnts = (*s_cdt->on3_to_tri_maps[f2ind])[pt_A];
			std::set<p2t::Point *>::iterator tp_it;
			for (tp_it = tri_pnts.begin(); tp_it != tri_pnts.end(); tp_it++) {
			    s_cdt->face_degen_pnts[f2ind]->insert(*tp_it);
			}
		    }
		}
		for (bit = pBt.begin(); bit != pBt.end(); bit++) {
		    BrepTrimPoint *tpt = *bit;
		    int f2ind = s_cdt->brep->m_T[tpt->trim_ind].Face()->m_face_index;
		    if (f2ind != face_index) {
			//bu_log("Pulls in face %d\n", f2ind);
			if (!s_cdt->face_degen_pnts[f2ind]) {
			    s_cdt->face_degen_pnts[f2ind] = new std::set<p2t::Point *>;
			}
			std::set<p2t::Point *> tri_pnts = (*s_cdt->on3_to_tri_maps[f2ind])[pt_B];
			std::set<p2t::Point *>::iterator tp_it;
			for (tp_it = tri_pnts.begin(); tp_it != tri_pnts.end(); tp_it++) {
			    s_cdt->face_degen_pnts[f2ind]->insert(*tp_it);
			}
		    }
		}
		for (bit = pCt.begin(); bit != pCt.end(); bit++) {
		    BrepTrimPoint *tpt = *bit;
		    int f2ind = s_cdt->brep->m_T[tpt->trim_ind].Face()->m_face_index;
		    if (f2ind != face_index) {
			//bu_log("Pulls in face %d\n", f2ind);
			if (!s_cdt->face_degen_pnts[f2ind]) {
			    s_cdt->face_degen_pnts[f2ind] = new std::set<p2t::Point *>;
			}
			std::set<p2t::Point *> tri_pnts = (*s_cdt->on3_to_tri_maps[f2ind])[pt_C];
			std::set<p2t::Point *>::iterator tp_it;
			for (tp_it = tri_pnts.begin(); tp_it != tri_pnts.end(); tp_it++) {
			    s_cdt->face_degen_pnts[f2ind]->insert(*tp_it);
			}
		    }
		}

		triangle_cnt--;
		tris_degen.insert(t);
		tris_zero_3D_area.insert(t);

	    }
	}
    }

    // TODO - it's even possible in principle for a triangulation to form a non-zero,
    // non degenerate face from 3 edge points that will "close" the solid but result
    // in a face with all of its vertices normals pointing in a very strange direction
    // compared to the surface normals at those points (not to mention looking "wrong"
    // in that the "proper" surface of the mesh will look like it is flattened in the
    // area of that triangle.  If we can recognize these cases and splitting them will
    // improve the situation, we want to handle them just as we would a zero area
    // triangle...

    /* For the non-zero-area involved triangles, we need to build new polygons
     * from each triangle and the "orphaned" points along its edge(s).  We then
     * re-tessellate in the triangle's parametric space.
     *
     * An "involved" triangle is a triangle with two of its three points in the
     * face's degen_pnts set.
     */
    for (int face_index = 0; face_index != s_cdt->brep->m_F.Count(); face_index++) {
	std::set<p2t::Point *> *fdp = s_cdt->face_degen_pnts[face_index];
	if (!fdp) {
	    continue;
	}
   	p2t::CDT *cdt = s_cdt->p2t_faces[face_index];
	std::map<p2t::Point *, ON_3dPoint *> *pointmap = s_cdt->tri_to_on3_maps[face_index];
	std::map<p2t::Point *, ON_3dPoint *> *normalmap = s_cdt->tri_to_on3_norm_maps[face_index];

	std::vector<p2t::Triangle *> *tri_add;

	if (!s_cdt->p2t_extra_faces[face_index]) {
	    tri_add = new std::vector<p2t::Triangle *>;
	    s_cdt->p2t_extra_faces[face_index] = tri_add;
	} else {
	    tri_add = s_cdt->p2t_extra_faces[face_index];
	}

	std::vector<p2t::Triangle*> tris = cdt->GetTriangles();
	for (size_t i = 0; i < tris.size(); i++) {
	    p2t::Triangle *t = tris[i];
	    int involved_pnt_cnt = 0;
	    if (tris_degen.find(t) != tris_degen.end()) {
		continue;
	    }
	    p2t::Point *t2dpnts[3] = {NULL, NULL, NULL};
	    for (size_t j = 0; j < 3; j++) {
		p2t::Point *p = t->GetPoint(j);
		t2dpnts[j] = p;
		if (fdp->find(p) != fdp->end()) {
		    involved_pnt_cnt++;
		}
	    }
	    if (involved_pnt_cnt > 1) {

		// TODO - construct the plane of this face, project 3D points
		// into that plane to get new 2D coordinates specific to this
		// face, and then make new p2t points from those 2D coordinates.
		// Won't be using the NURBS 2D for this part, so existing
		// p2t points won't help.
		std::vector<p2t::Point *>new_2dpnts;
		std::map<p2t::Point *, p2t::Point *> new2d_to_old2d;
		std::map<p2t::Point *, p2t::Point *> old2d_to_new2d;
		std::map<ON_3dPoint *, p2t::Point *> on3d_to_new2d;
		ON_3dPoint *t3dpnts[3] = {NULL, NULL, NULL};
		for (size_t j = 0; j < 3; j++) {
		    t3dpnts[j] = (*pointmap)[t2dpnts[j]];
		}
		int normal_backwards = 0;
		ON_Plane fplane(*t3dpnts[0], *t3dpnts[1], *t3dpnts[2]);

		// To verify sanity, check fplane normal against face normals
		for (size_t j = 0; j < 3; j++) {
		    ON_3dVector pv = (*(*normalmap)[t2dpnts[j]]);
		    if (ON_DotProduct(pv, fplane.Normal()) < 0) {
			normal_backwards++;
		    }
		}
		if (normal_backwards > 0) {
		    if (normal_backwards == 3) {
			fplane.Flip();
			bu_log("flipped plane\n");
		    } else {
			bu_log("Only %d of the face normals agree with the plane normal??\n", 3 - normal_backwards);
		    }
		}

		// Project the 3D face points onto the plane
		double ucent = 0;
		double vcent = 0;
		for (size_t j = 0; j < 3; j++) {
		    double u,v;
		    fplane.ClosestPointTo(*t3dpnts[j], &u, &v);
		    ucent += u;
		    vcent += v;
		    p2t::Point *np = new p2t::Point(u, v);
		    new2d_to_old2d[np] = t2dpnts[j];
		    old2d_to_new2d[t2dpnts[j]] = np;
		    on3d_to_new2d[t3dpnts[j]] = np;
		}
		ucent = ucent/3;
		vcent = vcent/3;
		ON_2dPoint tcent(ucent, vcent);

		// Any or all of the edges of the triangle may have orphan
		// points associated with them - if both edge points are
		// involved, we have to check.
		std::vector<p2t::Point *> polyline;
		for (size_t j = 0; j < 3; j++) {
		    p2t::Point *p1 = t2dpnts[j];
		    p2t::Point *p2 = (j < 2) ? t2dpnts[j+1] : t2dpnts[0];
		    ON_3dPoint *op1 = (*pointmap)[p1];
		    polyline.push_back(old2d_to_new2d[p1]);
		    if (fdp->find(p1) != fdp->end() && fdp->find(p2) != fdp->end()) {

			// Calculate a vector to use to perturb middle points off the
			// line in 2D.  Triangulation routines don't like collinear
			// points.  Since we're not using these 2D coordinates for
			// anything except the tessellation itself, as long as we
			// don't change the ordering of the polyline we should be
			// able to nudge the points off the line without ill effect.
			ON_2dPoint p2d1(p1->x, p1->y);
			ON_2dPoint p2d2(p2->x, p2->y);
			ON_2dPoint pmid = (p2d2 + p2d1)/2;
			ON_2dVector ptb = pmid - tcent;
			fastf_t edge_len = p2d1.DistanceTo(p2d2);
			ptb.Unitize();
			ptb = ptb * edge_len;

			std::set<ON_3dPoint *> edge_3d_pnts;
			std::map<double, ON_3dPoint *> ordered_new_pnts;
			ON_3dPoint *op2 = (*pointmap)[p2];
			edge_3d_pnts.insert(op1);
			edge_3d_pnts.insert(op2);
			ON_Line eline3d(*op1, *op2);
			double t1, t2;
			eline3d.ClosestPointTo(*op1, &t1);
			eline3d.ClosestPointTo(*op2, &t2);
			std::set<p2t::Point *>::iterator fdp_it;
			for (fdp_it = fdp->begin(); fdp_it != fdp->end(); fdp_it++) {
			    p2t::Point *p = *fdp_it;
			    if (p != p1 && p != p2) {
				ON_3dPoint *p3d = (*pointmap)[p];
				if (edge_3d_pnts.find(p3d) == edge_3d_pnts.end()) {
				    if (eline3d.DistanceTo(*p3d) < s_cdt->dist) {
					double tp;
					eline3d.ClosestPointTo(*p3d, &tp);
					if (tp > t1 && tp < t2) {
					    edge_3d_pnts.insert(p3d);
					    ordered_new_pnts[tp] = p3d;
					    double u,v;
					    fplane.ClosestPointTo(*p3d, &u, &v);

					    // Make a 2D point and nudge it off of the length
					    // of the edge off the edge.  Use the line parameter
					    // value to get different nudge factors for each point.
					    ON_2dPoint uvp(u, v);
					    uvp = uvp + (t2-tp)/(t2-t1)*0.01*ptb;

					    // Define the p2t point and update maps
					    p2t::Point *np = new p2t::Point(uvp.x, uvp.y);
					    new2d_to_old2d[np] = p;
					    old2d_to_new2d[p] = np;
					    on3d_to_new2d[p3d] = np;

					}
				    }
				}
			    }
			}

			// Have all new points on edge, add to polyline in edge order
			if (t1 < t2) {
			    std::map<double, ON_3dPoint *>::iterator m_it;
			    for (m_it = ordered_new_pnts.begin(); m_it != ordered_new_pnts.end(); m_it++) {
				ON_3dPoint *p3d = (*m_it).second;
				polyline.push_back(on3d_to_new2d[p3d]);
			    }
			} else {
			    std::map<double, ON_3dPoint *>::reverse_iterator m_it;
			    for (m_it = ordered_new_pnts.rbegin(); m_it != ordered_new_pnts.rend(); m_it++) {
				ON_3dPoint *p3d = (*m_it).second;
				polyline.push_back(on3d_to_new2d[p3d]);
			    }
			}
		    }

		    // I think(?) - need to close the loop
		    if (j == 2) {
			polyline.push_back(old2d_to_new2d[p2]);
		    }
		}

		// Have polyline, do CDT
		if (polyline.size() > 5) {
		    bu_log("%d polyline cnt: %zd\n", face_index, polyline.size());
		}
		if (polyline.size() > 4) {

		    // First, try ear clipping method
		    std::map<size_t, p2t::Point *> ec_to_p2t;
		    point2d_t *ec_pnts = (point2d_t *)bu_calloc(polyline.size(), sizeof(point2d_t), "ear clipping point array");
		    for (size_t k = 0; k < polyline.size()-1; k++) {
			p2t::Point *p2tp = polyline[k];
			V2SET(ec_pnts[k], p2tp->x, p2tp->y);
			ec_to_p2t[k] = p2tp;
		    }

		    int *ecfaces;
		    int num_faces;

		    // TODO - we need to check if we're getting zero area triangles back from these routines.  Unless all three
		    // triangle edges somehow have extra points, we can construct a triangle set that meets our needs by walking
		    // the edges in order and building triangles "by hand" to meet our need.  Calling the "canned" routines
		    // is an attempt to avoid that bookkeeping work, but if the tricks in place to avoid collinear point issues
		    // don't work reliably we'll need to just go with a direct build.
		    if (bg_polygon_triangulate(&ecfaces, &num_faces, NULL, NULL, ec_pnts, polyline.size()-1, EAR_CLIPPING)) {

			// Didn't work, see if poly2tri can handle it
			//bu_log("ec triangulate failed\n");
			p2t::CDT *fcdt = new p2t::CDT(polyline);
			fcdt->Triangulate(true, -1);
			std::vector<p2t::Triangle*> ftris = fcdt->GetTriangles();
			if (ftris.size() > polyline.size()) {
#if 0
			    bu_log("weird face count: %zd\n", ftris.size());
			    for (size_t k = 0; k < polyline.size(); k++) {
				bu_log("polyline[%zd]: %0.17f, %0.17f 0\n", k, polyline[k]->x, polyline[k]->y);
			    }

			    std::string pfile = std::string("polyline.plot3");
			    plot_polyline(&polyline, pfile.c_str());
			    for (size_t k = 0; k < ftris.size(); k++) {
				std::string tfile = std::string("tri-") + std::to_string(k) + std::string(".plot3");
				plot_tri(ftris[k], tfile.c_str());
				p2t::Point *p0 = ftris[k]->GetPoint(0);
				p2t::Point *p1 = ftris[k]->GetPoint(1);
				p2t::Point *p2 = ftris[k]->GetPoint(2);
				bu_log("tri[%zd]: %f %f -> %f %f -> %f %f\n", k, p0->x, p0->y, p1->x, p1->y, p2->x, p2->y);
			    }
#endif

			    bu_log("EC and Poly2Tri failed - aborting\n");
			    return -1;
			} else {
			    bu_log("Poly2Tri: found %zd faces\n", ftris.size());
			    //std::vector<p2t::Triangle *> *tri_add = s_cdt->p2t_extra_faces[face_index];
			    // TODO - translate face 2D triangles to mesh 2D triangles
			}

		    } else {
			for (int k = 0; k < num_faces; k++) {
			    p2t::Point *p2_1 = new2d_to_old2d[ec_to_p2t[ecfaces[3*k]]];
			    p2t::Point *p2_2 = new2d_to_old2d[ec_to_p2t[ecfaces[3*k+1]]];
			    p2t::Point *p2_3 = new2d_to_old2d[ec_to_p2t[ecfaces[3*k+2]]];
			    p2t::Triangle *nt = new p2t::Triangle(*p2_1, *p2_2, *p2_3);
			    tri_add->push_back(nt);
			}
			// We split the original triangle, so it's now replaced/degenerate in the tessellation
			tris_degen.insert(t);
			triangle_cnt--;
		    }
		} else {
		    // Point count doesn't indicate any need to split, we should be good...
		    //bu_log("Not enough points in polyline to require splitting\n");
		}
	    }
	}
    }

    bu_log("tri_cnt_init: %zd\n", triangle_cnt);
    for (int face_index = 0; face_index != s_cdt->brep->m_F.Count(); face_index++) {
	std::vector<p2t::Triangle *> *tri_add = s_cdt->p2t_extra_faces[face_index];
	if (!tri_add) {
	    continue;
	}
	//bu_log("adding %zd faces from %d\n", tri_add->size(), face_index);
	triangle_cnt += tri_add->size();
    }
    bu_log("tri_cnt_init+: %zd\n", triangle_cnt);


    // Know how many faces and points now - initialize BoT container.
    *fcnt = (int)triangle_cnt;
    *faces = (int *)bu_calloc(triangle_cnt*3, sizeof(int), "new faces array");
    *vcnt = (int)vfpnts.size();
    *vertices = (fastf_t *)bu_calloc(vfpnts.size()*3, sizeof(fastf_t), "new vert array");
    if (normals) {
	*ncnt = (int)vfnormals.size();
	*normals = (fastf_t *)bu_calloc(vfnormals.size()*3, sizeof(fastf_t), "new normals array");
	*fn_cnt = (int)triangle_cnt;
	*face_normals = (int *)bu_calloc(triangle_cnt*3, sizeof(int), "new face_normals array");
    }

    for (size_t i = 0; i < vfpnts.size(); i++) {
	(*vertices)[i*3] = vfpnts[i]->x;
	(*vertices)[i*3+1] = vfpnts[i]->y;
	(*vertices)[i*3+2] = vfpnts[i]->z;
    }

    if (normals) {
	for (size_t i = 0; i < vfnormals.size(); i++) {
	    (*normals)[i*3] = vfnormals[i]->x;
	    (*normals)[i*3+1] = vfnormals[i]->y;
	    (*normals)[i*3+2] = vfnormals[i]->z;
	}
    }

    // Iterate over faces, adding points and faces to BoT container.  Note: all
    // 3D points should be geometrically unique in this final container.
    int face_cnt = 0;
    triangle_cnt = 0;
    for (int face_index = 0; face_index != s_cdt->brep->m_F.Count(); face_index++) {
	p2t::CDT *cdt = s_cdt->p2t_faces[face_index];
	std::map<p2t::Point *, ON_3dPoint *> *pointmap = s_cdt->tri_to_on3_maps[face_index];
	std::vector<p2t::Triangle*> tris = cdt->GetTriangles();
	triangle_cnt += tris.size();
	int active_tris = 0;
	for (size_t i = 0; i < tris.size(); i++) {
	    p2t::Triangle *t = tris[i];
	    if (tris_degen.size() > 0 && tris_degen.find(t) != tris_degen.end()) {
		triangle_cnt--;
		continue;
	    }
	    active_tris++;
	    for (size_t j = 0; j < 3; j++) {
		p2t::Point *p = t->GetPoint(j);
		ON_3dPoint *op = (*pointmap)[p];
		int ind = on_pnt_to_bot_pnt[op];
		(*faces)[face_cnt*3 + j] = ind;
		if (normals) {
		    int nind = on_pnt_to_bot_norm[op];
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
	std::vector<p2t::Triangle *> *tri_add = s_cdt->p2t_extra_faces[face_index];
	std::map<p2t::Point *, ON_3dPoint *> *pointmap = s_cdt->tri_to_on3_maps[face_index];
	if (!tri_add) {
	    continue;
	}
	triangle_cnt += tri_add->size();
	for (size_t i = 0; i < tri_add->size(); i++) {
	    p2t::Triangle *t = (*tri_add)[i];
	    for (size_t j = 0; j < 3; j++) {
		p2t::Point *p = t->GetPoint(j);
		ON_3dPoint *op = (*pointmap)[p];
		int ind = on_pnt_to_bot_pnt[op];
		(*faces)[face_cnt*3 + j] = ind;
		if (normals) {
		    int nind = on_pnt_to_bot_norm[op];
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


    /* Clear out extra faces so they don't pollute another pass */
    for (int i = 0; i < s_cdt->brep->m_F.Count(); i++) {
	std::vector<p2t::Triangle *> *ef = s_cdt->p2t_extra_faces[i];
	if (ef) {
	    std::vector<p2t::Triangle *>::iterator trit;
	    for (trit = ef->begin(); trit != ef->end(); trit++) {
		p2t::Triangle *t = *trit;
		delete t;
	    }
	    ef->clear();
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

