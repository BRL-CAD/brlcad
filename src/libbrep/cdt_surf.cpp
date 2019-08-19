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

// TODO - investigate odd sampling in NIST2 face 501


#include "common.h"
#include "bn/rand.h"
#include "./cdt.h"

class SPatch {

    public:
	SPatch(double u1, double u2, double v1, double v2)
	{
	    umin = u1;
	    umax = u2;
	    vmin = v1;
	    vmax = v2;
	}

	double umin;
	double umax;
	double vmin;
	double vmax;
};

double
uline_len_est(struct cdt_surf_info *sinfo, double u1, double u2, double v)
{
    double t, lenfact, lenest;
    int active_half = (fabs(sinfo->v1 - v) < fabs(sinfo->v2 - v)) ? 0 : 1;
    t = (active_half == 0) ? 1 - fabs(sinfo->v1 - v)/fabs((sinfo->v2 - sinfo->v1)*0.5) : 1 - fabs(sinfo->v2 - v)/fabs((sinfo->v2 - sinfo->v1)*0.5);
    if (active_half == 0) {
	lenfact = sinfo->u_lower_3dlen * (1 - (t)) + sinfo->u_mid_3dlen * (t);
	lenest = (u2 - u1)/sinfo->ulen * lenfact;
    } else {
	lenfact = sinfo->u_mid_3dlen * (1 - (t)) + sinfo->u_upper_3dlen * (t);
	lenest = (u2 - u1)/sinfo->ulen * lenfact;
    }
    //if (print_debug) {
    //	bu_log("active_half: %d u1:%f u2:%f v: %f -> t: %f lenfact: %f lenest: %f\n", active_half, u1, u2, v, t, lenfact, lenest);
    //}
    return lenest;
}


double
vline_len_est(struct cdt_surf_info *sinfo, double u, double v1, double v2)
{
    double t, lenfact, lenest;
    int active_half = (fabs(sinfo->u1 - u) < fabs(sinfo->u2 - u)) ? 0 : 1;
    t = (active_half == 0) ? 1 - fabs(sinfo->u1 - u)/fabs((sinfo->u2 - sinfo->u1)*0.5) : 1 - fabs(sinfo->u2 - u)/fabs((sinfo->u2 - sinfo->u1)*0.5);
    if (active_half == 0) {
	lenfact = sinfo->v_lower_3dlen * (1 - (t)) + sinfo->v_mid_3dlen * (t);
	lenest = (v2 - v1)/sinfo->vlen * lenfact;
    } else {
	lenfact = sinfo->v_mid_3dlen * (1 - (t)) + sinfo->v_upper_3dlen * (t);
	lenest = (v2 - v1)/sinfo->vlen * lenfact;
    }

    //if (print_debug) {
    //    bu_log("active_half: %d u:%f v1:%f v2: %f -> t: %f lenfact: %f lenest: %f\n", active_half, u, v1, v2, t, lenfact, lenest);
    //}
    return lenest;
}

ON_3dPoint *
singular_trim_norm(struct cdt_surf_info *sinfo, fastf_t uc, fastf_t vc)
{
    if (sinfo->strim_pnts->find(sinfo->f->m_face_index) != sinfo->strim_pnts->end()) {
	//bu_log("Face %d has singular trims\n", sinfo->f->m_face_index);
	if (sinfo->strim_norms->find(sinfo->f->m_face_index) == sinfo->strim_norms->end()) {
	    //bu_log("Face %d has no singular trim normal information\n", sinfo->f->m_face_index);
	    return NULL;
	}
	std::map<int, ON_3dPoint *>::iterator m_it;
	// Check the trims to see if uc,vc is on one of them
	for (m_it = sinfo->strim_pnts->begin(); m_it != sinfo->strim_pnts->end(); m_it++) {
	    //bu_log("  trim %d\n", (*m_it).first);
	    ON_Interval trim_dom = sinfo->f->Brep()->m_T[(*m_it).first].Domain();
	    ON_2dPoint p2d1 = sinfo->f->Brep()->m_T[(*m_it).first].PointAt(trim_dom.m_t[0]);
	    ON_2dPoint p2d2 = sinfo->f->Brep()->m_T[(*m_it).first].PointAt(trim_dom.m_t[1]);
	    //bu_log("  points: %f,%f -> %f,%f\n", p2d1.x, p2d1.y, p2d2.x, p2d2.y);
	    int on_trim = 1;
	    if (NEAR_EQUAL(p2d1.x, p2d2.x, ON_ZERO_TOLERANCE)) {
		if (!NEAR_EQUAL(p2d1.x, uc, ON_ZERO_TOLERANCE)) {
		    on_trim = 0;
		}
	    } else {
		if (!NEAR_EQUAL(p2d1.x, uc, ON_ZERO_TOLERANCE) && !NEAR_EQUAL(p2d2.x, uc, ON_ZERO_TOLERANCE)) {
		    if (!((uc > p2d1.x && uc < p2d2.x) || (uc < p2d1.x && uc > p2d2.x))) {
			on_trim = 0;
		    }
		}
	    }
	    if (NEAR_EQUAL(p2d1.y, p2d2.y, ON_ZERO_TOLERANCE)) {
		if (!NEAR_EQUAL(p2d1.y, vc, ON_ZERO_TOLERANCE)) {
		    on_trim = 0;
		}
	    } else {
		if (!NEAR_EQUAL(p2d1.y, vc, ON_ZERO_TOLERANCE) && !NEAR_EQUAL(p2d2.y, vc, ON_ZERO_TOLERANCE)) {
		    if (!((vc > p2d1.y && vc < p2d2.y) || (vc < p2d1.y && vc > p2d2.y))) {
			on_trim = 0;
		    }
		}
	    }

	    if (on_trim) {
		if (sinfo->strim_norms->find((*m_it).first) != sinfo->strim_norms->end()) {
		    ON_3dPoint *vnorm = NULL;
		    vnorm = (*sinfo->strim_norms)[(*m_it).first];
		    //bu_log(" normal: %f, %f, %f\n", vnorm->x, vnorm->y, vnorm->z);
		    return vnorm;
		} else {
		    bu_log("Face %d: on singular trim, but no matching normal: %f, %f\n", sinfo->f->m_face_index, uc, vc);
		    return NULL;
		}
	    }
	}
    }
    return NULL;
}

/* If we've got trimming curves involved, we need to be more careful about respecting
 * the min edge distance.
 *
 * TODO - rather than blanket assuming the min edge distance, we should go one
 * better and find the min edge segment length for the involved edge segments.
 * This approach generates too many unnecessary triangles and is actually a
 * problem when a globally fine edge seg forces the surface to be fine at a
 * course edge. */
bool involves_trims(double *min_edge, struct cdt_surf_info *sinfo, ON_3dPoint &p1, ON_3dPoint &p2)
{
    double rtree_min_edge_dist = sinfo->max_edge;
    ON_SimpleArray<ON_RTreeLeaf> results_3d;
    bool found_trims_3d = sinfo->rt_trims_3d->Search((const double *) &p1, (const double *) &p2, results_3d);
    if (found_trims_3d && results_3d.Count() == 0) {
	std::cout << "huh?: found trims but results empty???\n";
	plot_rtree_3d(sinfo->rt_trims_3d, "rtree.plot3");
	point_t mmin, mmax;
	VSET(mmin, p1.x, p1.y, p1.z);
	VSET(mmax, p2.x, p2.y, p2.z);
	plot_bbox(mmin, mmax, "box.plot3");
	return false;
    }
    if (found_trims_3d) {
	for (int i = 0; i < results_3d.Count(); i++) {
	    ON_3dPoint ep1(results_3d[i].m_rect.m_min);
	    ON_3dPoint ep2(results_3d[i].m_rect.m_max);
	    fastf_t dist = ep1.DistanceTo(ep2);
	    std::cout << "rtree_min_edge_dist: " << rtree_min_edge_dist << ", rtree dist: " << dist << "\n";
	    if ((dist > SMALL_FASTF) && (dist < rtree_min_edge_dist))  {
		rtree_min_edge_dist = dist;
	    }
	}
    }

    (*min_edge) = rtree_min_edge_dist;

    return found_trims_3d;
}

/* flags identifying which side of the surface we're calculating
 *
 *                        u_upper
 *                        (2,0)
 *
 *               u1v2---------------- u2v2
 *                |          |         |
 *                |          |         |
 *                |  u_lmid  |         |
 *    v_lower     |----------|---------|     v_upper
 *     (0,1)      |  (1,0)   |         |      (2,1)
 *                |         v|lmid     |
 *                |          |(1,1)    |
 *               u1v1-----------------u2v1
 *
 *                        u_lower
 *                         (0,0)
 */
double
_cdt_get_uv_edge_3d_len(struct cdt_surf_info *sinfo, int c1, int c2)
{
    int line_set = 0;
    double wu1, wv1, wu2, wv2, umid, vmid = 0.0;

    /* u_lower */
    if (c1 == 0 && c2 == 0) {
	wu1 = sinfo->u1;
	wu2 = sinfo->u2;
	wv1 = sinfo->v1;
	wv2 = sinfo->v1;
	umid = (sinfo->u2 - sinfo->u1)/2.0;
	vmid = sinfo->v1;
	line_set = 1;
    }

    /* u_lmid */
    if (c1 == 1 && c2 == 0) {
	wu1 = sinfo->u1;
	wu2 = sinfo->u2;
	wv1 = (sinfo->v2 - sinfo->v1)/2.0;
	wv2 = (sinfo->v2 - sinfo->v1)/2.0;
	umid = (sinfo->u2 - sinfo->u1)/2.0;
	vmid = (sinfo->v2 - sinfo->v1)/2.0;
	line_set = 1;
    }

    /* u_upper */
    if (c1 == 2 && c2 == 0) {
	wu1 = sinfo->u1;
	wu2 = sinfo->u2;
	wv1 = sinfo->v2;
	wv2 = sinfo->v2;
	umid = (sinfo->u2 - sinfo->u1)/2.0;
	vmid = sinfo->v2;
	line_set = 1;
    }

    /* v_lower */
    if (c1 == 0 && c2 == 1) {
	wu1 = sinfo->u1;
	wu2 = sinfo->u1;
	wv1 = sinfo->v1;
	wv2 = sinfo->v2;
	umid = sinfo->u1;
	vmid = (sinfo->v2 - sinfo->v1)/2.0;
	line_set = 1;
    }

    /* v_lmid */
    if (c1 == 1 && c2 == 1) {
	wu1 = (sinfo->u2 - sinfo->u1)/2.0;
	wu2 = (sinfo->u2 - sinfo->u1)/2.0;
	wv1 = sinfo->v1;
	wv2 = sinfo->v2;
	umid = (sinfo->u2 - sinfo->u1)/2.0;
	vmid = (sinfo->v2 - sinfo->v1)/2.0;
	line_set = 1;
    }

    /* v_upper */
    if (c1 == 2 && c2 == 1) {
	wu1 = sinfo->u2;
	wu2 = sinfo->u2;
	wv1 = sinfo->v1;
	wv2 = sinfo->v2;
	umid = sinfo->u2;
	vmid = (sinfo->v2 - sinfo->v1)/2.0;
	line_set = 1;
    }

    if (!line_set) {
	bu_log("Invalid edge %d, %d specified\n", c1, c2);
	return DBL_MAX;
    }

    // 1st 3d point
    ON_3dPoint p1 = sinfo->s->PointAt(wu1, wv1);

    // middle 3d point
    ON_3dPoint pmid = sinfo->s->PointAt(umid, vmid);

    // last 3d point
    ON_3dPoint p2 = sinfo->s->PointAt(wu2, wv2);

    // length
    double d1 = pmid.DistanceTo(p1);
    double d2 = p2.DistanceTo(pmid);
    return d1+d2;
}

void
filter_surface_edge_pnts(struct ON_Brep_CDT_Face_State *f)
{
    // TODO - it's looking like a 2D check isn't going to be enough - we probably
    // need BOTH a 2D and a 3D check to make sure none of the points are in a
    // position that will cause trouble.  Will need to build a 3D RTree of the line
    // segments from the edges, as well as 2D rt_trims tree.
    std::set<ON_2dPoint *> rm_pnts;
    std::set<ON_2dPoint *>::iterator osp_it;
    for (osp_it = f->on_surf_points->begin(); osp_it != f->on_surf_points->end(); osp_it++) {
	ON_SimpleArray<void*> results;
	const ON_2dPoint *p = *osp_it;

	f->rt_trims->Search2d((const double *) p, (const double *) p, results);

	if (results.Count() > 0) {
	    bool on_edge = false;
	    for (int ri = 0; ri < results.Count(); ri++) {
		double dist;
		const ON_Line *l = (const ON_Line *) *results.At(ri);
		dist = l->MinimumDistanceTo(*p);
		if (NEAR_ZERO(dist, BN_TOL_DIST)) {
		    on_edge = true;
		    break;
		}
	    }
	    if (on_edge) {
		rm_pnts.insert((ON_2dPoint *)p);
	    }
	}
    }

    for (osp_it = rm_pnts.begin(); osp_it != rm_pnts.end(); osp_it++) {
	const ON_2dPoint *p = *osp_it;
	f->on_surf_points->erase((ON_2dPoint *)p);
    }


    // TODO - In addition to removing points on the line in 2D, we don't want points that
    // would be outside the edge polygon in projection. Find the "close" trims (if any)
    // for the candidate 3D point, then use the normals of the Brep edge points and
    // the edge direction to do a local "inside/outside" test.  Not sure yet exactly how
    // to do this - possibilities include rtree search for the area around each surface
    // point, or a nanoflann based nearest lookup for the edge points to get candidates
    // near each edge in turn - in the latter case, look for points common to the result
    // sets for both edge points to localize on that particular segment.


}

static bool
getSurfacePoint(
	         struct cdt_surf_info *sinfo,
		 SPatch &sp,
		 std::queue<SPatch> &nq
		 )
{
    fastf_t u1 = sp.umin;
    fastf_t u2 = sp.umax;
    fastf_t v1 = sp.vmin;
    fastf_t v2 = sp.vmax;
    double ldfactor = 2.0;
    int split_u = 0;
    int split_v = 0;
    ON_2dPoint p2d(0.0, 0.0);
    fastf_t u = (u1 + u2) / 2.0;
    fastf_t v = (v1 + v2) / 2.0;
    fastf_t udist = u2 - u1;
    fastf_t vdist = v2 - v1;

    if ((udist < sinfo->min_dist + ON_ZERO_TOLERANCE)
	    || (vdist < sinfo->min_dist + ON_ZERO_TOLERANCE)) {
	return false;
    }

    double est1 = uline_len_est(sinfo, u1, u2, v1);
    double est2 = uline_len_est(sinfo, u1, u2, v2);
    double est3 = vline_len_est(sinfo, u1, v1, v2);
    double est4 = vline_len_est(sinfo, u2, v1, v2);

    double uavg = (est1+est2)/2.0;
    double vavg = (est3+est4)/2.0;

#if 0
    double umin = (est1 < est2) ? est1 : est2;
    double vmin = (est3 < est4) ? est3 : est4;
    double umax = (est1 > est2) ? est1 : est2;
    double vmax = (est3 > est4) ? est3 : est4;

    bu_log("umin,vmin: %f, %f\n", umin, vmin);
    bu_log("umax,vmax: %f, %f\n", umax, vmax);
    bu_log("uavg,vavg: %f, %f\n", uavg, vavg);
    bu_log("min_edge %f\n", sinfo->min_edge);
#endif
    if (est1 < 0.01*sinfo->within_dist && est2 < 0.01*sinfo->within_dist) {
	//bu_log("e12 Small estimates: %f, %f\n", est1, est2);
	return false;
    }
    if (est3 < 0.01*sinfo->within_dist && est4 < 0.01*sinfo->within_dist) {
	//bu_log("e34 Small estimates: %f, %f\n", est3, est4);
	return false;
    }

    if (uavg < sinfo->min_edge && vavg < sinfo->min_edge) {
	return false;
    }


    if (uavg > ldfactor * vavg) {
	split_u = 1;
    }

    if (vavg > ldfactor * uavg) {
	split_v = 1;
    }

    ON_3dPoint p[4] = {ON_3dPoint(), ON_3dPoint(), ON_3dPoint(), ON_3dPoint()};
    ON_3dVector norm[4] = {ON_3dVector(), ON_3dVector(), ON_3dVector(), ON_3dVector()};
    bool ev_success = true;

    if (!split_u || !split_v) {
	// Don't know if we're splitting in at least one direction - check if we're close
	// enough to trims to need to worry about edges
	if ((surface_EvNormal(sinfo->s, u1, v1, p[0], norm[0]))
		&& (surface_EvNormal(sinfo->s, u2, v2, p[2], norm[2]))) {

	    double min_edge_len;
	    if (involves_trims(&min_edge_len, sinfo, p[0], p[2])) {
		if (uavg > min_edge_len && vavg > min_edge_len) {
		    split_u = 1;
		}

		if (uavg > min_edge_len && vavg > min_edge_len) {
		    split_v = 1;
		}
	    }

	} else {
	    ev_success = false;
	}
    }

    if (ev_success && (!split_u || !split_v)) {
	// Don't know if we're splitting in at least one direction - check dot products
	ON_3dPoint mid(0.0, 0.0, 0.0);
	ON_3dVector norm_mid(0.0, 0.0, 0.0);
	if ((surface_EvNormal(sinfo->s, u2, v1, p[1], norm[1])) // for u
		&& (surface_EvNormal(sinfo->s, u1, v2, p[3], norm[3]))
		&& (surface_EvNormal(sinfo->s, u, v, mid, norm_mid))) {
	    double udot;
	    double vdot;
	    ON_Line line1(p[0], p[2]);
	    ON_Line line2(p[1], p[3]);
	    double dist = mid.DistanceTo(line1.ClosestPointTo(mid));
	    V_MAX(dist, mid.DistanceTo(line2.ClosestPointTo(mid)));

	    for (int i = 0; i < 4; i++) {
		fastf_t uc = (i == 0 || i == 3) ? u1 : u2;
		fastf_t vc = (i == 0 || i == 1) ? v1 : v2;
		ON_3dPoint *vnorm = singular_trim_norm(sinfo, uc, vc);
		if (vnorm && ON_DotProduct(*vnorm, norm_mid) > 0) {
		    //bu_log("vert norm %f %f %f works\n", vnorm->x, vnorm->y, vnorm->z);
		    norm[i] = *vnorm;
		}
	    }


	    if (dist < sinfo->min_dist + ON_ZERO_TOLERANCE) {
		return false;
	    }

	    udot = (VNEAR_EQUAL(norm[0], norm[1], ON_ZERO_TOLERANCE)) ? 1.0 : norm[0] * norm[1];
	    vdot = (VNEAR_EQUAL(norm[0], norm[3], ON_ZERO_TOLERANCE)) ? 1.0 : norm[0] * norm[3];
	    if (udot < sinfo->cos_within_ang - ON_ZERO_TOLERANCE) {
		split_u = 1;
	    }
	    if (vdot < sinfo->cos_within_ang - ON_ZERO_TOLERANCE) {
		split_v = 1;
	    }
	}
    }

    if (split_u && split_v) {
	nq.push(SPatch(u1, u, v1, v));
	nq.push(SPatch(u1, u, v, v2));
	nq.push(SPatch(u, u2, v1, v));
	nq.push(SPatch(u, u2, v, v2));
	return true;
    }
    if (split_u) {
	nq.push(SPatch(u1, u, v1, v2));
	nq.push(SPatch(u, u2, v1, v2));
	return true;
    }
    if (split_v) {
	nq.push(SPatch(u1, u2, v1, v));
	nq.push(SPatch(u1, u2, v, v2));
	return true;
    }

    return false;
}

void
getSurfacePoints(struct ON_Brep_CDT_Face_State *f)
{
    double surface_width, surface_height;

    int face_index = f->ind;
    ON_BrepFace &face = f->s_cdt->brep->m_F[face_index];
    const ON_Surface *s = face.SurfaceOf();
    const ON_Brep *brep = face.Brep();

    if (s->GetSurfaceSize(&surface_width, &surface_height)) {
	double dist = 0.0;
	double min_dist = 0.0;
	double within_dist = 0.0;
	double cos_within_ang = 0.0;

	if ((surface_width < BN_TOL_DIST) || (surface_height < BN_TOL_DIST)) {
	    return;
	}

	struct cdt_surf_info sinfo;
	sinfo.s = s;
	sinfo.f = &face;
	sinfo.rt_trims = f->rt_trims;
	sinfo.rt_trims_3d = f->rt_trims_3d;
	sinfo.strim_pnts = f->strim_pnts;
	sinfo.strim_norms = f->strim_norms;
	double t1, t2;
	s->GetDomain(0, &t1, &t2);
	sinfo.ulen = fabs(t2 - t1);
	s->GetDomain(1, &t1, &t2);
	sinfo.vlen = fabs(t2 - t1);
	s->GetDomain(0, &sinfo.u1, &sinfo.u2);
	s->GetDomain(1, &sinfo.v1, &sinfo.v2);
	sinfo.u_lower_3dlen = _cdt_get_uv_edge_3d_len(&sinfo, 0, 0);
	sinfo.u_mid_3dlen   = _cdt_get_uv_edge_3d_len(&sinfo, 1, 0);
	sinfo.u_upper_3dlen = _cdt_get_uv_edge_3d_len(&sinfo, 2, 0);
	sinfo.v_lower_3dlen = _cdt_get_uv_edge_3d_len(&sinfo, 0, 1);
	sinfo.v_mid_3dlen   = _cdt_get_uv_edge_3d_len(&sinfo, 1, 1);
	sinfo.v_upper_3dlen = _cdt_get_uv_edge_3d_len(&sinfo, 2, 1);
	sinfo.min_edge = (*f->s_cdt->min_edge_seg_len)[face_index];
	sinfo.max_edge = (*f->s_cdt->max_edge_seg_len)[face_index];

	// may be a smaller trimmed subset of surface so worth getting
	// face boundary
	bool bGrowBox = false;
	ON_3dPoint min, max;
	for (int li = 0; li < face.LoopCount(); li++) {
	    for (int ti = 0; ti < face.Loop(li)->TrimCount(); ti++) {
		const ON_BrepTrim *trim = face.Loop(li)->Trim(ti);
		trim->GetBoundingBox(min, max, bGrowBox);
		bGrowBox = true;
	    }
	}

	ON_BoundingBox tight_bbox;
	if (brep->GetTightBoundingBox(tight_bbox)) {
	    // Note: this needs to be based on the smallest dimension of the
	    // box, not the diagonal, in case we've got something really long
	    // and narrow.
	    fastf_t d1 = tight_bbox.m_max[0] - tight_bbox.m_min[0];
	    fastf_t d2 = tight_bbox.m_max[1] - tight_bbox.m_min[1];
	    dist = (d1 < d2) ? d1 : d2;
	}

	if (f->s_cdt->tol.abs < BN_TOL_DIST + ON_ZERO_TOLERANCE) {
	    min_dist = BN_TOL_DIST;
	} else {
	    min_dist = f->s_cdt->tol.abs;
	}

	double rel = 0.0;
	if (f->s_cdt->tol.rel > 0.0 + ON_ZERO_TOLERANCE) {
	    rel = f->s_cdt->tol.rel * dist;
	    within_dist = rel < min_dist ? min_dist : rel;
	    //if (s_cdt->abs < s_cdt->dist + ON_ZERO_TOLERANCE) {
	    //    min_dist = within_dist;
	    //}
	} else if ((f->s_cdt->tol.abs > 0.0 + ON_ZERO_TOLERANCE)
		&& (f->s_cdt->tol.norm < 0.0 + ON_ZERO_TOLERANCE)) {
	    within_dist = min_dist;
	} else if ((f->s_cdt->tol.abs > 0.0 + ON_ZERO_TOLERANCE)
		|| (f->s_cdt->tol.norm > 0.0 + ON_ZERO_TOLERANCE)) {
	    within_dist = dist;
	} else {
	    within_dist = 0.01 * dist; // default to 1% minimum surface distance
	}

	if (f->s_cdt->tol.norm > 0.0 + ON_ZERO_TOLERANCE) {
	    cos_within_ang = cos(f->s_cdt->tol.norm);
	} else {
	    cos_within_ang = cos(ON_PI / 2.0);
	}

	sinfo.min_dist = min_dist;
	sinfo.within_dist = within_dist;
	sinfo.cos_within_ang = cos_within_ang;

	std::queue<SPatch> spq1, spq2;

	/**
	 * Sample portions of the surface to collect sufficient points
	 * to capture the surface shape according to the settings
	 *
	 * M = max
	 * m = min
	 * o = mid
	 *
	 *    umvM------uovM-------uMvM
	 *     |          |         |
	 *     |          |         |
	 *     |          |         |
	 *    umvo------uovo-------uMvo
	 *     |          |         |
	 *     |          |         |
	 *     |          |         |
	 *    umvm------uovm-------uMvm
	 *
	 * left bottom  = umvm
	 * left midy    = umvo
	 * left top     = umvM
	 * midx bottom  = uovm
	 * midx midy    = uovo
	 * midx top     = uovM
	 * right bottom = uMvm
	 * right midy   = uMvo
	 * right top    = uMvM
	 */

	ON_BOOL32 uclosed = s->IsClosed(0);
	ON_BOOL32 vclosed = s->IsClosed(1);
	double midx = (min.x + max.x) / 2.0;
	double midy = (min.y + max.y) / 2.0;

	if (uclosed && vclosed) {
	    /*
	     *     #--------------------#
	     *     |          |         |
	     *     |          |         |
	     *     |          |         |
	     *    umvo------uovo--------#
	     *     |          |         |
	     *     |          |         |
	     *     |          |         |
	     *    umvm------uovm--------#
	     */
	    spq1.push(SPatch(min.x, midx, min.y, midy));

	    /*
	     *     #----------#---------#
	     *     |          |         |
	     *     |          |         |
	     *     |          |         |
	     *     #--------uovo-------uMvo
	     *     |          |         |
	     *     |          |         |
	     *     |          |         |
	     *     #--------uovm-------uMvm
	     */
	    spq1.push(SPatch(midx, max.x, min.y, midy));

	    /*
	     *    umvM------uovM--------#
	     *     |          |         |
	     *     |          |         |
	     *     |          |         |
	     *    umvo------uovo--------#
	     *     |          |         |
	     *     |          |         |
	     *     |          |         |
	     *     #----------#---------#
	     */
	    spq1.push(SPatch(min.x, midx, midy, max.y));

	    /*
	     *     #--------uovM------ uMvM
	     *     |          |         |
	     *     |          |         |
	     *     |          |         |
	     *     #--------uovo-------uMvo
	     *     |          |         |
	     *     |          |         |
	     *     |          |         |
	     *     #----------#---------#
	     */
	    spq1.push(SPatch(midx, max.x, midy, max.y));

	} else if (uclosed) {

	    /*
	     *    umvM------uovM--------#
	     *     |          |         |
	     *     |          |         |
	     *     |          |         |
	     *     #----------#---------#
	     *     |          |         |
	     *     |          |         |
	     *     |          |         |
	     *    umvm------uovm--------#
	     */
	    spq1.push(SPatch(min.x, midx, min.y, max.y));

	    /*
	     *     #--------uovM------ uMvM
	     *     |          |         |
	     *     |          |         |
	     *     |          |         |
	     *     #----------#---------#
	     *     |          |         |
	     *     |          |         |
	     *     |          |         |
	     *     #--------uovm-------uMvm
	     */
	    spq1.push(SPatch(midx, max.x, min.y, max.y));

	} else if (vclosed) {

	    /*
	     *     #----------#---------#
	     *     |          |         |
	     *     |          |         |
	     *     |          |         |
	     *    umvo--------#--------uMvo
	     *     |          |         |
	     *     |          |         |
	     *     |          |         |
	     *    umvm--------#--------uMvm
	     */
	    spq1.push(SPatch(min.x, max.x, min.y, midy));

	    /*
	     *    umvM--------#------- uMvM
	     *     |          |         |
	     *     |          |         |
	     *     |          |         |
	     *    umvo--------#--------uMvo
	     *     |          |         |
	     *     |          |         |
	     *     |          |         |
	     *     #----------#---------#
	     */
	    spq1.push(SPatch(min.x, max.x, midy, max.y));

	} else {

	    /*
	     *    umvM--------#------- uMvM
	     *     |          |         |
	     *     |          |         |
	     *     |          |         |
	     *     #----------#---------#
	     *     |          |         |
	     *     |          |         |
	     *     |          |         |
	     *    umvm--------#--------uMvm
	     */
	    spq1.push(SPatch(min.x, max.x, min.y, max.y));
	}

	std::queue<SPatch> *wq = &spq1;
	std::queue<SPatch> *nq = &spq2;
	int split_depth = 0;

	while (!wq->empty()) {
	    SPatch sp = wq->front();
	    wq->pop();
	    if (!getSurfacePoint(&sinfo, sp, *nq)) {
		ON_BoundingBox bb(ON_2dPoint(sp.umin,sp.vmin),ON_2dPoint(sp.umax, sp.vmax));
		sinfo.leaf_bboxes.insert(new ON_BoundingBox(bb));
	    }

	    // Once we've processed the current level,
	    // work on the next if we need to
	    if (wq->empty() && !nq->empty()) {
		std::queue<SPatch> *tq = wq;
		wq = nq;
		nq = tq;
		// Let the counter know we're going deeper
		split_depth++;
	    }
	}

	// TODO - we need to reject surface points that would be outside a brep
	// face boundary edge in local planar projection.  Think we can do this
	// by taking the plane from the closest edge segment's points (would
	// the 3D RTree be useful for finding that?), projecting the candidate
	// point onto it, and checking it's vector dirction to the closest
	// point on the segment agains the cross product of the plane and the
	// edge segment's direction vector.  If it's outside, reject.  If it's
	// too close to the segment in projection (< 0.1 * the segment length,
	// for example), reject.  Else OK.  Edge segments have to utterly
	// dominate surface point behavor, because they are hard limits and we
	// can't afford anything that would potentially produce unrepairable
	// triangles near the face edge.

	float *prand;
	std::set<ON_BoundingBox *>::iterator b_it;
	/* We want to jitter sampled 2D points out of linearity */
	bn_rand_init(prand, 0);
	for (b_it = sinfo.leaf_bboxes.begin(); b_it != sinfo.leaf_bboxes.end(); b_it++) {
	    ON_3dPoint p3d = (*b_it)->Center();
	    ON_3dPoint pmax = (*b_it)->Max();
	    ON_3dPoint pmin = (*b_it)->Min();
	    double ulen = pmax.x - pmin.x;
	    double vlen = pmax.y - pmin.y;
	    double px = p3d.x + (bn_rand_half(prand) * 0.3*ulen);
	    double py = p3d.y + (bn_rand_half(prand) * 0.3*vlen);
	    f->on_surf_points->insert(new ON_2dPoint(px,py));
	}

	// Strip out points from the surface that are on the trimming curves.  Trim
	// points require special handling for watertightness and introducing them
	// from the surface also runs the risk of adding duplicate 2D points, which
	// aren't allowed for facetization.
	filter_surface_edge_pnts(f);

    }
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

