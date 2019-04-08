/*                   B R E P _ C D T . C P P
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
/** @addtogroup librt */
/** @{ */
/** @file brep_cdt.cpp
 *
 * Constrained Delaunay Triangulation of NURBS B-Rep objects.
 *
 */

#include "common.h"

#include <vector>
#include <list>
#include <map>
#include <stack>
#include <iostream>
#include <algorithm>
#include <set>
#include <utility>

#include "poly2tri/poly2tri.h"

#include "assert.h"

#include "vmath.h"

#include "bu/cv.h"
#include "bu/opt.h"
#include "bu/time.h"
#include "brep.h"
#include "bn/dvec.h"

#include "raytrace.h"
#include "rt/geom.h"

#include "./brep_local.h"
#include "./brep_debug.h"

struct brep_cdt_tol {
    fastf_t min_dist;
    fastf_t max_dist;
    fastf_t within_dist;
    fastf_t cos_within_ang;
};

#define BREP_CDT_TOL_ZERO {0.0, 0.0, 0.0, 0.0}

// Digest tessellation tolerances... 
void
CDT_Tol_Setup(struct brep_cdt_tol *cdt, double dist, fastf_t md, const struct rt_tess_tol *ttol, const struct bn_tol *tol)
{
    fastf_t min_dist, max_dist, within_dist, cos_within_ang;

    max_dist = md;

    if (ttol->abs < tol->dist + ON_ZERO_TOLERANCE) {
	min_dist = tol->dist;
    } else {
	min_dist = ttol->abs;
    }

    double rel = 0.0;
    if (ttol->rel > 0.0 + ON_ZERO_TOLERANCE) {
	rel = ttol->rel * dist;
	if (max_dist < rel * 10.0) {
	    max_dist = rel * 10.0;
	}
	within_dist = rel < min_dist ? min_dist : rel;
    } else if (ttol->abs > 0.0 + ON_ZERO_TOLERANCE) {
	within_dist = min_dist;
    } else {
	within_dist = 0.01 * dist; // default to 1% of dist
    }

    if (ttol->norm > 0.0 + ON_ZERO_TOLERANCE) {
	cos_within_ang = cos(ttol->norm);
    } else {
	cos_within_ang = cos(ON_PI / 2.0);
    }

    cdt->min_dist = min_dist;
    cdt->max_dist = max_dist;
    cdt->within_dist = within_dist;
    cdt->cos_within_ang = cos_within_ang;
}

void
EdgePointsFill(const ON_NurbsCurve *nc,
	      fastf_t t1,
	      const ON_3dVector &start_tang,
	      const ON_3dPoint &start_3d,
	      fastf_t t2,
	      const ON_3dVector &end_tang,
	      const ON_3dPoint &end_3d,
	      const struct brep_cdt_tol *cdt_tol,
	      std::map<double, ON_3dPoint *> &param_points,
	      std::vector<ON_3dPoint *> *w3dpnts
	      )
{
    ON_3dPoint mid_3d = ON_3dPoint::UnsetPoint;
    ON_3dVector mid_tang = ON_3dVector::UnsetVector;
    fastf_t t = (t1 + t2) / 2.0;

    int etan = (nc->EvTangent(t, mid_3d, mid_tang)) ? 1 : 0;
    int leval = 0;

    if (!etan)
	return;

    ON_Line line3d(start_3d, end_3d);
    double dist3d = mid_3d.DistanceTo(line3d.ClosestPointTo(mid_3d));
    int leval_1 = 0;
    leval += (line3d.Length() > cdt_tol->max_dist) ? 1 : 0;
    leval += (dist3d > (cdt_tol->within_dist + ON_ZERO_TOLERANCE)) ? 1 : 0;
    leval_1 += ((start_tang * end_tang) < cdt_tol->cos_within_ang - ON_ZERO_TOLERANCE) ? 1 : 0;
    leval += (leval_1 && (dist3d > cdt_tol->min_dist + ON_ZERO_TOLERANCE)) ? 1 : 0;

    if (!leval)
	return;

    EdgePointsFill(nc, t1, start_tang, start_3d, t, mid_tang, mid_3d, cdt_tol, param_points, w3dpnts);
    ON_3dPoint *pn = new ON_3dPoint(mid_3d);
    w3dpnts->push_back(pn);
    param_points[t] = pn;
    EdgePointsFill(nc, t, mid_tang, mid_3d, t2, end_tang, end_3d, cdt_tol, param_points, w3dpnts);
}

void
Get_Shared_Edge_Points(ON_BrepEdge &edge,
	std::vector<ON_3dPoint *> *w3dpnts,
	std::map<int, ON_3dPoint *> *vert_pnts,
	const struct rt_tess_tol *ttol,
	const struct bn_tol *tol)
{
    struct brep_cdt_tol cdt_tol = BREP_CDT_TOL_ZERO;
    std::map<double, ON_3dPoint *> *param_points = NULL;
    double cplen = 0.0;
    const ON_Curve* crv = edge.EdgeCurveOf();

    /* If we've already got the points, just return */
    if (edge.m_edge_user.p != NULL) {
	return;
    }

    /* Normalize the domain of the curve to the ControlPolygonLength() of the
     * NURBS form of the curve to attempt to minimize distortion in 3D to
     * mirror what we do for the surfaces.  (GetSurfaceSize uses this under the
     * hood for its estimates.)  */
    ON_NurbsCurve *nc = crv->NurbsCurve();
    cplen = nc->ControlPolygonLength();
    nc->SetDomain(0.0, cplen);

    CDT_Tol_Setup(&cdt_tol, cplen, cplen/10, ttol, tol);

    /* Begin point collection */
    int evals = 0;
    ON_3dPoint start_3d(0.0, 0.0, 0.0);
    ON_3dVector start_tang(0.0, 0.0, 0.0);
    ON_3dVector start_norm(0.0, 0.0, 0.0);
    ON_3dPoint end_3d(0.0, 0.0, 0.0);
    ON_3dVector end_tang(0.0, 0.0, 0.0);
    ON_3dVector end_norm(0.0, 0.0, 0.0);

    param_points = new std::map<double, ON_3dPoint *>();
    edge.m_edge_user.p = (void *)param_points;
    ON_Interval range = nc->Domain();

    evals += (nc->EvTangent(range.m_t[0], start_3d, start_tang)) ? 1 : 0;
    evals += (nc->EvTangent(range.m_t[1], end_3d, end_tang)) ? 1 : 0;

    /* For beginning and end of curve, we use vert points */
    ON_3dPoint *p0 = (*vert_pnts)[edge.Vertex(0)->m_vertex_index];
    ON_3dPoint *p2 = (*vert_pnts)[edge.Vertex(1)->m_vertex_index];
    start_3d = *(p0);
    end_3d = *(p2);
    (*param_points)[range.m_t[0]] = p0;
    (*param_points)[range.m_t[1]] = p2;

    if (nc->IsClosed()) {
	double mid_range = (range.m_t[0] + range.m_t[1]) / 2.0;
	ON_3dPoint mid_3d(0.0, 0.0, 0.0);
	ON_3dVector mid_tang(0.0, 0.0, 0.0);
	evals += (nc->EvTangent(mid_range, mid_3d, mid_tang)) ? 1 : 0;

	if (evals != 3) {
	    mid_3d =  nc->PointAt(mid_range);
	}

	ON_3dPoint *p1 = new ON_3dPoint(nc->PointAt(mid_range));
	w3dpnts->push_back(p1);
	(*param_points)[mid_range] = p1;

	EdgePointsFill(nc, range.m_t[0], start_tang, start_3d, mid_range, mid_tang, mid_3d, &cdt_tol, *param_points, w3dpnts);
	EdgePointsFill(nc, mid_range, mid_tang, mid_3d, range.m_t[1], end_tang, end_3d, &cdt_tol, *param_points, w3dpnts);

    } else {
	EdgePointsFill(nc, range.m_t[0], start_tang, start_3d, range.m_t[1], end_tang, end_3d, &cdt_tol, *param_points, w3dpnts);
    }
}


int brep_cdt_plot(struct bu_vls *vls, const char *solid_name,
                      const struct rt_tess_tol *ttol, const struct bn_tol *tol,
                      struct brep_specific* bs, struct rt_brep_internal*UNUSED(bi),
                      struct bn_vlblock *UNUSED(vbp), int UNUSED(plottype), int UNUSED(num_points))
{
    // For ease of memory cleanup, keep track of allocated points in containers
    // whose specific purpose is to hold things for cleanup (allows maps to
    // reuse pointers - we don't want to get a double free looping over a point
    // map to clean up when a point has been mapped more than once.)
    std::vector<ON_3dPoint *> working_3d_pnts;
    std::map<int, ON_3dPoint *> vert_pnts;

    //struct bu_list *vhead = bn_vlblock_find(vbp, YELLOW);
    ON_wString wstr;
    ON_TextLog tl(wstr);

    ON_Brep* brep = bs->brep;
    if (brep == NULL || !brep->IsValid(&tl)) {
        if (wstr.Length() > 0) {
            ON_String onstr = ON_String(wstr);
            const char *isvalidinfo = onstr.Array();
            bu_vls_strcat(vls, "brep (");
            bu_vls_strcat(vls, solid_name);
            bu_vls_strcat(vls, ") is NOT valid:");
            bu_vls_strcat(vls, isvalidinfo);
        } else {
            bu_vls_strcat(vls, "brep (");
            bu_vls_strcat(vls, solid_name);
            bu_vls_strcat(vls, ") is NOT valid.");
        }
        //for now try to draw - return -1;
    }


    // Reparameterize the face's surface and transform the "u" and "v"
    // coordinates of all the face's parameter space trimming curves to
    // minimize distortion in the map from parameter space to 3d..
    for (int face_index = 0; face_index < brep->m_F.Count(); face_index++) {
        ON_BrepFace *face = brep->Face(face_index);
        const ON_Surface *s = face->SurfaceOf();
        double surface_width, surface_height;
	if (s->GetSurfaceSize(&surface_width, &surface_height)) {
	    face->SetDomain(0, 0.0, surface_width);
	    face->SetDomain(1, 0.0, surface_height);
	}
    }

    /* We want to use ON_3dPoint pointers and BrepVertex points, but vert->Point()
     * produces a temporary address.  Make stable copies of the Vertex points. */
    for (int index = 0; index < brep->m_V.Count(); index++) {
	ON_BrepVertex& v = brep->m_V[index];
	vert_pnts[index] = new ON_3dPoint(v.Point());
    }

    /* To generate watertight meshes, the faces must share 3D edge points.  To ensure
     * a uniform set of edge points, we first sample all the edges and build their
     * point sets */
    for (int index = 0; index < brep->m_E.Count(); index++) {
	ON_BrepEdge& edge = brep->m_E[index];
	if (edge.m_edge_user.p == NULL) {

	    /* TODO - howto handle singular trims, their 3D vertex points, and how to track
	     * that at the 3D edge level... */

	    // TODO - check curve direction against edge direction, adjust as appropriate

	    Get_Shared_Edge_Points(edge, &working_3d_pnts, &vert_pnts, ttol, tol);

	    bu_log("Edge %d total points: %zd\n", edge.m_edge_index, ((std::map<double, ON_3dPoint *> *)edge.m_edge_user.p)->size());
	}
    }

    /* Set up the trims */
    for (int index = 0; index < brep->m_T.Count(); index++) {
	ON_BrepTrim& trim = brep->m_T[index];
	ON_BrepEdge *edge = trim.Edge();
	double pnt_cnt = ((std::map<double, ON_3dPoint *> *)edge->m_edge_user.p)->size();

	// Use a map so we get ordered points along the trim even if we
	// have to subsequently refine.
	std::map<double, ON_2dPoint *> *tpmap = new std::map<double, ON_2dPoint *>();
	trim.m_trim_user.p = (void *)tpmap;

	// Divide up the trim domain into increments based on the number of 3D edge points.
	double delta =  trim.Domain().Length() / (pnt_cnt - 1);
	ON_Interval trim_dom = trim.Domain();
	for (int i = 1; i <= pnt_cnt; i++) {
	    double t = trim.Domain().m_t[0] + (i - 1) * delta;
	    ON_2dPoint p2d = trim.PointAt(t);
	    (*tpmap)[t] = new ON_2dPoint(p2d);
	    bu_log("Trim %d[%0.3f]: %f, %f\n", trim.m_trim_index, t, p2d.x, p2d.y);
	}
	bu_log("Trim %d (associated with edge %d) total points: %zd\n", trim.m_trim_index, edge->m_edge_index, ((std::map<double, ON_2dPoint *> *)trim.m_trim_user.p)->size());
    }

#if 0
    // For all loops, assemble 2D polygons.
    for (int index = 0; index < brep->m_L.Count(); index++) {
	const ON_BrepLoop &loop = brep->m_L[index];
	int trim_count = loop->TrimCount();
	for (int lti = 0; lti < trim_count; lti++) {
	    ON_BrepTrim *trim = loop->Trim(lti);
	    std::map<double, ON_2dPoint *> *tpmap = (std::map<double, ON_2dPoint *> *)trim.m_trim_user.p;
	}
    }
#endif

    // TODO - we may need to add 2D points on seams that the edges didn't know
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

    // TODO - get surface points and set up the final poly2tri triangulation input.
    // May want to go to integer space ala clipper to make sure all points satisfy
    // poly2tri constraints...

    // TODO - once we are generating watertight triangulations reliably, we
    // will want to return a BoT - the output will be suitable for export or
    // drawing, so just provide the correct data type for other routines to
    // work with.  Also, we can then apply the bot validity testing routines.


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

