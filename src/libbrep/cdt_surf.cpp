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
	for (m_it = (*sinfo->strim_pnts)[sinfo->f->m_face_index].begin(); m_it != (*sinfo->strim_pnts)[sinfo->f->m_face_index].end(); m_it++) {
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
		if ((*sinfo->strim_norms)[sinfo->f->m_face_index].find((*m_it).first) != (*sinfo->strim_norms)[sinfo->f->m_face_index].end()) {
		    ON_3dPoint *vnorm = NULL;
		    vnorm = (*sinfo->strim_norms)[sinfo->f->m_face_index][(*m_it).first];
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
bool involves_trims(double *min_edge, struct ON_Brep_CDT_State *s_cdt, struct cdt_surf_info *sinfo, fastf_t u1, fastf_t u2, fastf_t v1, fastf_t v2)
{
    bool ret = false;
    ON_SimpleArray<void*> results;
    ON_2dPoint pmin(u1, v1);
    ON_2dPoint pmax(u2, v2);
    sinfo->rt_trims->Search2d((const double *) &pmin, (const double *) &pmax, results);
    ret = (results.Count() > 0);
    if (!min_edge) {
	return ret;
    }

    if (results.Count() > 0) {
	double min_edge_dist = DBL_MAX;
	ON_BoundingBox uvbb(ON_2dPoint(u1,v1),ON_2dPoint(u2,v2));

	ON_SimpleArray<BrepTrimPoint> *brep_loop_points = s_cdt->brep_face_loop_points[sinfo->f->m_face_index];
	if (!brep_loop_points) {
	    (*min_edge) = min_edge_dist;
	    return ret;
	}
	for (int li = 0; li < sinfo->f->LoopCount(); li++) {
	    int num_loop_points = brep_loop_points[li].Count();
	    if (num_loop_points > 1) {
		ON_3dPoint *p1 = (brep_loop_points[li])[0].p3d;
		ON_3dPoint *p2 = NULL;
		const ON_2dPoint *p2d1 = &(brep_loop_points[li])[0].p2d;
		const ON_2dPoint *p2d2 = NULL;
		for (int i = 1; i < num_loop_points; i++) {
		    p2 = p1;
		    p1 = (brep_loop_points[li])[i].p3d;
		    p2d2 = p2d1;
		    p2d1 = &(brep_loop_points[li])[i].p2d;
		    // Overlaps bbox? 
		    if (uvbb.IsPointIn(*p2d1, false) || uvbb.IsPointIn(*p2d2, false)) {
			fastf_t dist = p1->DistanceTo(*p2);
			if ((dist > SMALL_FASTF) && (dist < min_edge_dist))  {
			    min_edge_dist = dist;
			}
		    }
		}
	    }
	}

	//bu_log("Face %d: %f\n", sinfo->f->m_face_index, min_edge_dist);
	(*min_edge) = min_edge_dist;
    }

    return ret;
}

/* The "left" and "below" parameters tell this particular iteration of the subdivision
 * which UV points should be stored.  This bookkeeping is done to avoid introducing
 * duplicated points at shared subdivision edges. */
static void
getSurfacePoints(
	         struct ON_Brep_CDT_State *s_cdt,
	         struct cdt_surf_info *sinfo,
		 fastf_t u1,
		 fastf_t u2,
		 fastf_t v1,
		 fastf_t v2,
		 fastf_t min_dist,
		 fastf_t within_dist,
		 fastf_t cos_within_ang,
		 ON_2dPointArray &on_surf_points,
		 bool left,
		 bool below)
{
    double ldfactor = 2.0;
    int split_u = 0;
    int split_v = 0;
    ON_2dPoint p2d(0.0, 0.0);
    fastf_t u = (u1 + u2) / 2.0;
    fastf_t v = (v1 + v2) / 2.0;
    fastf_t udist = u2 - u1;
    fastf_t vdist = v2 - v1;

    if ((udist < min_dist + ON_ZERO_TOLERANCE)
	    || (vdist < min_dist + ON_ZERO_TOLERANCE)) {
	return;
    }

    double est1 = uline_len_est(sinfo, u1, u2, v1);
    double est2 = uline_len_est(sinfo, u1, u2, v2);
    double est3 = vline_len_est(sinfo, u1, v1, v2);
    double est4 = vline_len_est(sinfo, u2, v1, v2);

    //bu_log("(min: %f) est1, est2, est3, est4: %f, %f, %f, %f\n", min_dist, est1, est2, est3, est4);
    if (est1 < 0.01*within_dist && est2 < 0.01*within_dist) {
	//bu_log("e12 Small estimates: %f, %f\n", est1, est2);
	return;
    }
    if (est3 < 0.01*within_dist && est4 < 0.01*within_dist) {
	//bu_log("e34 Small estimates: %f, %f\n", est3, est4);
	return;
    }

    if (udist > ldfactor * vdist) {
	split_u = 1;
    }

    if (vdist > ldfactor * udist) {
	split_v = 1;
    }

    if (!split_u || !split_v) {
	// Don't know if we're splitting in at least one direction - check if we're close
	// enough to trims to need to worry about edges
	double min_edge_len;
	if (involves_trims(&min_edge_len, s_cdt, sinfo, u1, u2, v1, v2)) {
	    if (est1 > 2*min_edge_len || est2 > 2*min_edge_len) {
		split_u = 1;
	    }

	    if (est3 > 2*min_edge_len || est4 > 2*min_edge_len) {
		split_v = 1;
	    }
	}
    }


    if (!split_u || !split_v) {
	// Don't know if we're splitting in at least one direction - check dot products
	ON_3dPoint mid(0.0, 0.0, 0.0);
	ON_3dVector norm_mid(0.0, 0.0, 0.0);
	ON_3dPoint p[4] = {ON_3dPoint(), ON_3dPoint(), ON_3dPoint(), ON_3dPoint()};
	ON_3dVector norm[4] = {ON_3dVector(), ON_3dVector(), ON_3dVector(), ON_3dVector()};
	if ((surface_EvNormal(sinfo->s, u1, v1, p[0], norm[0]))
		&& (surface_EvNormal(sinfo->s, u2, v1, p[1], norm[1])) // for u
		&& (surface_EvNormal(sinfo->s, u2, v2, p[2], norm[2]))
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


	    if (dist < min_dist + ON_ZERO_TOLERANCE) {
		return;
	    }

	    udot = (VNEAR_EQUAL(norm[0], norm[1], ON_ZERO_TOLERANCE)) ? 1.0 : norm[0] * norm[1];
	    vdot = (VNEAR_EQUAL(norm[0], norm[3], ON_ZERO_TOLERANCE)) ? 1.0 : norm[0] * norm[3];
	    if (udot < cos_within_ang - ON_ZERO_TOLERANCE) {
		split_u = 1;
	    }
	    if (vdot < cos_within_ang - ON_ZERO_TOLERANCE) {
		split_v = 1;
	    }
	}
    }

    if (split_u && split_v) {
	//bu_log("split: both cos_within_ang\n");
	if (left) {
	    p2d.Set(u1, v);
	    on_surf_points.Append(p2d);
	}
	if (below) {
	    p2d.Set(u, v1);
	    on_surf_points.Append(p2d);
	}
	//center
	p2d.Set(u, v);
	on_surf_points.Append(p2d);
	//right
	p2d.Set(u2, v);
	on_surf_points.Append(p2d);
	//top
	p2d.Set(u, v2);
	on_surf_points.Append(p2d);

	getSurfacePoints(s_cdt, sinfo, u1, u, v1, v, min_dist, within_dist,
		cos_within_ang, on_surf_points, left, below);
	getSurfacePoints(s_cdt, sinfo, u1, u, v, v2, min_dist, within_dist,
		cos_within_ang, on_surf_points, left, false);
	getSurfacePoints(s_cdt, sinfo, u, u2, v1, v, min_dist, within_dist,
		cos_within_ang, on_surf_points, false, below);
	getSurfacePoints(s_cdt, sinfo, u, u2, v, v2, min_dist, within_dist,
		cos_within_ang, on_surf_points, false, false);
	return;
    }
    if (split_u) {
	//bu_log("split: udot cos_within_ang\n");
	if (below) {
	    p2d.Set(u, v1);
	    on_surf_points.Append(p2d);
	}
	//top
	p2d.Set(u, v2);
	on_surf_points.Append(p2d);
	getSurfacePoints(s_cdt, sinfo, u1, u, v1, v2, min_dist, within_dist,
		cos_within_ang, on_surf_points, left, below);
	getSurfacePoints(s_cdt, sinfo, u, u2, v1, v2, min_dist, within_dist,
		cos_within_ang, on_surf_points, false, below);
	return;
    }
    if (split_v) {
	//bu_log("split: vdot cos_within_ang\n");
	if (left) {
	    p2d.Set(u1, v);
	    on_surf_points.Append(p2d);
	}
	//right
	p2d.Set(u2, v);
	on_surf_points.Append(p2d);

	getSurfacePoints(s_cdt, sinfo, u1, u2, v1, v, min_dist, within_dist,
		cos_within_ang, on_surf_points, left, below);
	getSurfacePoints(s_cdt, sinfo, u1, u2, v, v2, min_dist, within_dist,
		cos_within_ang, on_surf_points, left, false);
	return;
    }

    if (left) {
	p2d.Set(u1, v);
	on_surf_points.Append(p2d);
    }
    if (below) {
	p2d.Set(u, v1);
	on_surf_points.Append(p2d);
    }
    //center
    p2d.Set(u, v);
    on_surf_points.Append(p2d);
    //right
    p2d.Set(u2, v);
    on_surf_points.Append(p2d);
    //top
    p2d.Set(u, v2);
    on_surf_points.Append(p2d);
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
getSurfacePoints(struct ON_Brep_CDT_State *s_cdt,
	         const ON_BrepFace &face,
		 ON_2dPointArray &on_surf_points,
		 ON_RTree *rt_trims
		 )
{
    double surface_width, surface_height;

    const ON_Surface *s = face.SurfaceOf();
    const ON_Brep *brep = face.Brep();

    if (s->GetSurfaceSize(&surface_width, &surface_height)) {
	double dist = 0.0;
	double min_dist = 0.0;
	double within_dist = 0.0;
	double cos_within_ang = 0.0;

	if ((surface_width < s_cdt->dist) || (surface_height < s_cdt->dist)) {
	    return;
	}

	if (!s_cdt->on2_to_on3_maps[face.m_face_index]) {
	    std::map<ON_2dPoint *, ON_3dPoint *> *on2to3 = new std::map<ON_2dPoint *, ON_3dPoint *>();
	    s_cdt->on2_to_on3_maps[face.m_face_index] = on2to3;
	}

	struct cdt_surf_info sinfo;
	sinfo.s = s;
	sinfo.f = &face;
	sinfo.rt_trims = rt_trims;
	sinfo.strim_pnts = s_cdt->strim_pnts;
	sinfo.strim_norms = s_cdt->strim_norms;
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
	sinfo.min_edge = (*s_cdt->min_edge_seg_len)[face.m_face_index];
	sinfo.max_edge = (*s_cdt->max_edge_seg_len)[face.m_face_index];

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

	if (s_cdt->abs < s_cdt->dist + ON_ZERO_TOLERANCE) {
	    min_dist = s_cdt->dist;
	} else {
	    min_dist = s_cdt->abs;
	}

	double rel = 0.0;
	if (s_cdt->rel > 0.0 + ON_ZERO_TOLERANCE) {
	    rel = s_cdt->rel * dist;
	    within_dist = rel < min_dist ? min_dist : rel;
	    //if (s_cdt->abs < s_cdt->dist + ON_ZERO_TOLERANCE) {
	    //    min_dist = within_dist;
	    //}
	} else if ((s_cdt->abs > 0.0 + ON_ZERO_TOLERANCE)
		   && (s_cdt->norm < 0.0 + ON_ZERO_TOLERANCE)) {
	    within_dist = min_dist;
	} else if ((s_cdt->abs > 0.0 + ON_ZERO_TOLERANCE)
		   || (s_cdt->norm > 0.0 + ON_ZERO_TOLERANCE)) {
	    within_dist = dist;
	} else {
	    within_dist = 0.01 * dist; // default to 1% minimum surface distance
	}

	if (s_cdt->norm > 0.0 + ON_ZERO_TOLERANCE) {
	    cos_within_ang = cos(s_cdt->norm);
	} else {
	    cos_within_ang = cos(ON_PI / 2.0);
	}



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
	if (uclosed && vclosed) {
	    ON_2dPoint p(0.0, 0.0);
	    double midx = (min.x + max.x) / 2.0;
	    double midy = (min.y + max.y) / 2.0;

	    //left bottom
	    p.Set(min.x, min.y);
	    on_surf_points.Append(p);

	    //left midy
	    p.Set(min.x, midy);
	    on_surf_points.Append(p);

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
	    getSurfacePoints(s_cdt, &sinfo, min.x, midx, min.y, midy, min_dist, within_dist,
			     cos_within_ang, on_surf_points, true, true);

	    //midx bottom
	    p.Set(midx, min.y);
	    on_surf_points.Append(p);

	    //midx midy
	    p.Set(midx, midy);
	    on_surf_points.Append(p);


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
	    getSurfacePoints(s_cdt, &sinfo, midx, max.x, min.y, midy, min_dist, within_dist,
			     cos_within_ang, on_surf_points, false, true);

	    //right bottom
	    p.Set(max.x, min.y);
	    on_surf_points.Append(p);

	    //right midy
	    p.Set(max.x, midy);
	    on_surf_points.Append(p);

	    //left top
	    p.Set(min.x, max.y);
	    on_surf_points.Append(p);

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
	    getSurfacePoints(s_cdt, &sinfo, min.x, midx, midy, max.y, min_dist, within_dist,
			     cos_within_ang, on_surf_points, true, false);

	    //midx top
	    p.Set(midx, max.y);
	    on_surf_points.Append(p);

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
	    getSurfacePoints(s_cdt, &sinfo, midx, max.x, midy, max.y, min_dist, within_dist,
			     cos_within_ang, on_surf_points, false, false);

	    //left top
	    p.Set(max.x, max.y);
	    on_surf_points.Append(p);

	} else if (uclosed) {
	    ON_2dPoint p(0.0, 0.0);
	    double midx = (min.x + max.x) / 2.0;

	    //left bottom
	    p.Set(min.x, min.y);
	    on_surf_points.Append(p);

	    //left top
	    p.Set(min.x, max.y);
	    on_surf_points.Append(p);

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
	    getSurfacePoints(s_cdt, &sinfo, min.x, midx, min.y, max.y, min_dist,
			     within_dist, cos_within_ang, on_surf_points, true, true);

	    //midx bottom
	    p.Set(midx, min.y);
	    on_surf_points.Append(p);

	    //midx top
	    p.Set(midx, max.y);
	    on_surf_points.Append(p);

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
	    getSurfacePoints(s_cdt, &sinfo, midx, max.x, min.y, max.y, min_dist,
			     within_dist, cos_within_ang, on_surf_points, false, true);

	    //right bottom
	    p.Set(max.x, min.y);
	    on_surf_points.Append(p);

	    //right top
	    p.Set(max.x, max.y);
	    on_surf_points.Append(p);

	} else if (vclosed) {

	    ON_2dPoint p(0.0, 0.0);
	    double midy = (min.y + max.y) / 2.0;

	    //left bottom
	    p.Set(min.x, min.y);
	    on_surf_points.Append(p);

	    //midy left
	    p.Set(min.x, midy);
	    on_surf_points.Append(p);

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
	    getSurfacePoints(s_cdt, &sinfo, min.x, max.x, min.y, midy, min_dist,
			     within_dist, cos_within_ang, on_surf_points, true, true);

	    //right bottom
	    p.Set(max.x, min.y);
	    on_surf_points.Append(p);

	    //midy right
	    p.Set(max.x, midy);
	    on_surf_points.Append(p);

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
	    getSurfacePoints(s_cdt, &sinfo, min.x, max.x, midy, max.y, min_dist,
			     within_dist, cos_within_ang, on_surf_points, true, false);

	    //left top
	    p.Set(min.x, max.y);
	    on_surf_points.Append(p);

	    //right top
	    p.Set(max.x, max.y);
	    on_surf_points.Append(p);
	} else {
	    ON_2dPoint p(0.0, 0.0);

	    //left bottom
	    p.Set(min.x, min.y);
	    on_surf_points.Append(p);

	    //left top
	    p.Set(min.x, max.y);
	    on_surf_points.Append(p);

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
	    getSurfacePoints(s_cdt, &sinfo, min.x, max.x, min.y, max.y, min_dist,
			     within_dist, cos_within_ang, on_surf_points, true, true);

	    //right bottom
	    p.Set(max.x, min.y);
	    on_surf_points.Append(p);

	    //right top
	    p.Set(max.x, max.y);
	    on_surf_points.Append(p);
	}
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

