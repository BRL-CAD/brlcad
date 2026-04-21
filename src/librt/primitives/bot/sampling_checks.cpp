/*              S A M P L I N G _ C H E C K S . C P P
 * BRL-CAD
 *
 * Copyright (c) 2024-2026 United States Government as represented by
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
/** @file sampling_checks.cpp
 *
 */

#include "common.h"

#include <set>

#include "vmath.h"
#include "rt/application.h"
#include "rt/rt_instance.h"
#include "rt/shoot.h"
#include "rt/primitives/bot.h"

static bool
bot_face_normal(vect_t *n, struct rt_bot_internal *bot, int i)
{
    vect_t a,b;

    /* sanity */
    if (!n || !bot || i < 0 || (size_t)i > bot->num_faces ||
	    bot->faces[i*3+2] < 0 || (size_t)bot->faces[i*3+2] > bot->num_vertices) {
	return false;
    }

    VSUB2(a, &bot->vertices[bot->faces[i*3+1]*3], &bot->vertices[bot->faces[i*3]*3]);
    VSUB2(b, &bot->vertices[bot->faces[i*3+2]*3], &bot->vertices[bot->faces[i*3]*3]);
    VCROSS(*n, a, b);
    VUNITIZE(*n);
    if (bot->orientation == RT_BOT_CW) {
	VREVERSE(*n, *n);
    }

    return true;
}

struct coplanar_info {
    double ttol;
    int is_thin;
    struct rt_bot_internal *bot;

    int verbose;
    int curr_tri;
    std::set<int> problem_indices;
};

// Backout distance used to lift the ray origin off the face surface before
// shooting inward.  Must be large enough that tcenter + n*TOL != tcenter in
// double precision at any practical model-coordinate scale.  SQRT_SMALL_FASTF
// (~1e-18) collapses to zero once vertex coordinates exceed ~1e4 mm; use
// BN_TOL_DIST (0.0005 mm) instead so the offset is always representable.
#define RT_BOT_CHECK_TOL BN_TOL_DIST

static int
_tc_hit(struct application *ap, struct partition *PartHeadp, struct seg *segs)
{
    if (PartHeadp->pt_forw == PartHeadp)
	return 1;

    struct coplanar_info *tinfo = (struct coplanar_info *)ap->a_uptr;

    struct seg *s = (struct seg *)segs->l.forw;

    // If we didn't get a hit involving nearby triangles, but we still
    // passed the initial first-hit check, something is up.  Unfortunately
    // the conditions that might trigger this aren't exclusively thin face
    // pairings, but since it is possible go ahead and flag it.
    if (s->seg_in.hit_dist > 2*RT_BOT_CHECK_TOL) {
	tinfo->is_thin = 1;
	tinfo->problem_indices.insert(tinfo->curr_tri);
	return 0;
    }

    for (BU_LIST_FOR(s, seg, &(segs->l))) {
	if (s->seg_in.hit_dist > tinfo->ttol)
	    continue;

	double dist = s->seg_out.hit_dist - s->seg_in.hit_dist;
	if (dist < tinfo->ttol) {
	    tinfo->is_thin = 1;
	    tinfo->problem_indices.insert(s->seg_in.hit_surfno);
	    tinfo->problem_indices.insert(s->seg_out.hit_surfno);
	}
    }

    return 0;
}

static int
_tc_miss(struct application *ap)
{
    // We are shooting directly into the center of a triangle from right above
    // it.  If we miss under those conditions, it can only happen because
    // something is wrong.
    struct coplanar_info *tinfo = (struct coplanar_info *)ap->a_uptr;
    tinfo->is_thin = 1;
    tinfo->problem_indices.insert(tinfo->curr_tri);
    return 0;
}

/* I don't think this is supposed to happen with a single primitive, but just
 * in case we get an overlap report somehow flag it as trouble */
static int
_tc_overlap(struct application *UNUSED(ap),
	struct partition *UNUSED(pp),
	struct region *UNUSED(reg1),
	struct region *UNUSED(reg2),
	struct partition *UNUSED(hp))
{
    return 0;
}

int
rt_bot_thin_check(struct bu_ptbl *ofaces, struct rt_bot_internal *bot, struct rt_i *rtip, double ttol, int verbose)
{
    if (!bot || bot->mode != RT_BOT_SOLID || !rtip || !bot->num_faces)
	return 0;

    int found_thin = 0;
    struct coplanar_info tinfo;
    tinfo.ttol = ttol;
    tinfo.verbose = verbose;
    tinfo.bot = bot;

    // Set up the raytrace
    struct application ap;
    RT_APPLICATION_INIT(&ap);
    ap.a_rt_i = rtip;     /* application uses this instance */
    ap.a_hit = _tc_hit;    /* where to go on a hit */
    ap.a_miss = _tc_miss;  /* where to go on a miss */
    ap.a_overlap = _tc_overlap;  /* where to go if an overlap is found */
    ap.a_onehit = 0;
    ap.a_resource = &rt_uniresource;
    ap.a_uptr = (void *)&tinfo;

    for (size_t i = 0; i < bot->num_faces; i++) {
	tinfo.curr_tri = (int)i;
	vect_t rnorm, n, backout;
	if (!bot_face_normal(&n, bot, i))
	    continue;

	// We want backout to get the ray origin off the triangle surface
	VMOVE(backout, n);
	VSCALE(backout, backout, RT_BOT_CHECK_TOL);
	// Reverse the triangle normal for a ray direction
	VREVERSE(rnorm, n);

	point_t rpnts[3];
	point_t tcenter;
	VMOVE(rpnts[0], &bot->vertices[bot->faces[i*3+0]*3]);
	VMOVE(rpnts[1], &bot->vertices[bot->faces[i*3+1]*3]);
	VMOVE(rpnts[2], &bot->vertices[bot->faces[i*3+2]*3]);
	VADD3(tcenter, rpnts[0], rpnts[1], rpnts[2]);
	VSCALE(tcenter, tcenter, 1.0/3.0);

	// Set up the ray
	VMOVE(ap.a_ray.r_dir, rnorm);
	VADD2(ap.a_ray.r_pt, tcenter, backout);

	// Take the solid shot
	ap.a_hit = _tc_hit;    /* where to go on a hit */
	ap.a_miss = _tc_miss;  /* where to go on a miss */
	ap.a_onehit = 0;
	tinfo.is_thin = 0;
	(void)rt_shootray(&ap);

	if (tinfo.is_thin) {
	    found_thin = 1;
	    tinfo.problem_indices.insert(i);
	}

	if (!ofaces && found_thin)
	    break;
    }

    if (ofaces) {
	std::set<int>::iterator p_it;
	for (p_it = tinfo.problem_indices.begin(); p_it != tinfo.problem_indices.end(); ++p_it) {
	    int ind = *p_it;
	    bu_ptbl_ins(ofaces, (long *)(long)ind);
	}
    }

    return found_thin;
}

/* -------------------------------------------------------------------------
 * Near-tolerance CSG-miss check
 *
 * Detects faces whose inward ray produces a segment whose thickness falls in
 * the range [VUNITIZE_TOL, BN_TOL_DIST).  Such segments are reported by the
 * BoT raytracer but would be discarded by the CSG boolweave tolerance filter,
 * creating a divergence between the BoT and the original CSG description.
 *
 * The typical cause is a subtractor primitive that protrudes just a sub-
 * tolerance distance past a base face, creating a phantom thin cap in the
 * Manifold result that is too thin to survive the CSG boolweave.
 *
 * Segments thinner than VUNITIZE_TOL are already caught by rt_bot_thin_check
 * (degenerate mesh artifacts); this check targets the disjoint window above
 * that but still below BN_TOL_DIST.
 * -------------------------------------------------------------------------*/

struct near_tol_info {
    int is_near_tol;
    int verbose;
    int curr_tri;
    std::set<int> problem_indices;
};

static int
_ntc_hit(struct application *ap, struct partition *PartHeadp, struct seg *segs)
{
    if (PartHeadp->pt_forw == PartHeadp)
	return 1;

    struct near_tol_info *ninfo = (struct near_tol_info *)ap->a_uptr;

    struct seg *s = (struct seg *)segs->l.forw;
    for (BU_LIST_FOR(s, seg, &(segs->l))) {
	/* Only examine segments whose entrance is near the surface.
	 * The ray starts at centroid + n*RT_BOT_CHECK_TOL (= BN_TOL_DIST), so
	 * a genuine surface entrance lands at ~BN_TOL_DIST from origin.
	 * 3× gives comfortable margin for floating-point spread without
	 * letting deeper geometry interfere. */
	if (s->seg_in.hit_dist > 3.0 * RT_BOT_CHECK_TOL)
	    continue;

	double thickness = s->seg_out.hit_dist - s->seg_in.hit_dist;

	/* Flag segments in [VUNITIZE_TOL, BN_TOL_DIST).
	 * - Below VUNITIZE_TOL: already reported by rt_bot_thin_check.
	 * - At or above BN_TOL_DIST: normal geometry the CSG boolweave keeps.
	 * - This window: real BoT hit that boolweave would discard as noise. */
	if (thickness >= VUNITIZE_TOL && thickness < RT_BOT_CHECK_TOL) {
	    ninfo->is_near_tol = 1;
	    ninfo->problem_indices.insert(s->seg_in.hit_surfno);
	    ninfo->problem_indices.insert(s->seg_out.hit_surfno);
	}
    }
    return 0;
}

static int
_ntc_miss(struct application *UNUSED(ap))
{
    /* A miss from the face-normal inward shot means the face is degenerate;
     * that case is handled by rt_bot_thin_check, not here. */
    return 0;
}

static int
_ntc_overlap(struct application *UNUSED(ap),
	struct partition *UNUSED(pp),
	struct region *UNUSED(reg1),
	struct region *UNUSED(reg2),
	struct partition *UNUSED(hp))
{
    return 0;
}

int
rt_bot_csg_miss_check(struct bu_ptbl *ofaces, struct rt_bot_internal *bot, struct rt_i *rtip, int verbose)
{
    if (!bot || bot->mode != RT_BOT_SOLID || !rtip || !bot->num_faces)
	return 0;

    int found_near_tol = 0;
    struct near_tol_info ninfo;
    ninfo.verbose = verbose;

    struct application ap;
    RT_APPLICATION_INIT(&ap);
    ap.a_rt_i = rtip;
    ap.a_hit = _ntc_hit;
    ap.a_miss = _ntc_miss;
    ap.a_overlap = _ntc_overlap;
    ap.a_onehit = 0;
    ap.a_resource = &rt_uniresource;
    ap.a_uptr = (void *)&ninfo;

    for (size_t i = 0; i < bot->num_faces; i++) {
	ninfo.curr_tri = (int)i;
	ninfo.is_near_tol = 0;

	vect_t rnorm, n, backout;
	if (!bot_face_normal(&n, bot, i))
	    continue;

	VMOVE(backout, n);
	VSCALE(backout, backout, RT_BOT_CHECK_TOL);
	VREVERSE(rnorm, n);

	point_t rpnts[3];
	point_t tcenter;
	VMOVE(rpnts[0], &bot->vertices[bot->faces[i*3+0]*3]);
	VMOVE(rpnts[1], &bot->vertices[bot->faces[i*3+1]*3]);
	VMOVE(rpnts[2], &bot->vertices[bot->faces[i*3+2]*3]);
	VADD3(tcenter, rpnts[0], rpnts[1], rpnts[2]);
	VSCALE(tcenter, tcenter, 1.0/3.0);

	VMOVE(ap.a_ray.r_dir, rnorm);
	VADD2(ap.a_ray.r_pt, tcenter, backout);

	(void)rt_shootray(&ap);

	if (ninfo.is_near_tol) {
	    found_near_tol = 1;
	    ninfo.problem_indices.insert(i);
	}

	if (!ofaces && found_near_tol)
	    break;
    }

    if (ofaces) {
	std::set<int>::iterator p_it;
	for (p_it = ninfo.problem_indices.begin(); p_it != ninfo.problem_indices.end(); ++p_it) {
	    int ind = *p_it;
	    bu_ptbl_ins(ofaces, (long *)(long)ind);
	}
    }

    return found_near_tol;
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
