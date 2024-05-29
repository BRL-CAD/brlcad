/*              S A M P L I N G _ C H E C K S . C P P
 * BRL-CAD
 *
 * Copyright (c) 2024 United States Government as represented by
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

#include "./ged_lint.h"


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
    int have_above;
    int unexpected_miss;
    struct rt_bot_internal *bot;

    int verbose;
    int curr_tri;
    std::set<int> problem_indices;
};


static int
_tc_hit(struct application *ap, struct partition *PartHeadp, struct seg *segs)
{
    if (PartHeadp->pt_forw == PartHeadp)
	return 1;

    struct coplanar_info *tinfo = (struct coplanar_info *)ap->a_uptr;

    struct partition *pp = PartHeadp->pt_forw;

    struct seg *s = (struct seg *)segs->l.forw;
    if (s->seg_in.hit_dist > 2*SQRT_SMALL_FASTF) {
	if (tinfo->verbose) {
	    bu_log("	WARNING: First hit wasn't from triangle %d: distance %g\n", tinfo->curr_tri, s->seg_in.hit_dist);
	    bu_log("	in_surfno: %d\n", s->seg_in.hit_surfno);
	    bu_log("	out_surfno: %d\n", s->seg_out.hit_surfno);
	    bu_log("	center: %0.17f %0.17f %0.17f\n" , V3ARGS(ap->a_ray.r_pt));
	    bu_log("	dir: %0.17f %0.17f %0.17f\n" , V3ARGS(ap->a_ray.r_dir));
	    tinfo->problem_indices.insert(tinfo->curr_tri);
	}
	return 0;
    }

    for (BU_LIST_FOR(s, seg, &(segs->l))) {
	if (s->seg_in.hit_dist > tinfo->ttol)
	    continue;

	double dist = s->seg_out.hit_dist - s->seg_in.hit_dist;
	if (tinfo->verbose > 1) {
	    bu_log("surfno: %d\n", s->seg_in.hit_surfno);
	    bu_log("%s dist: %0.17f\n",  pp->pt_regionp->reg_name, dist);
	    bu_log("center: %0.17f %0.17f %0.17f\n" , V3ARGS(ap->a_ray.r_pt));
	    bu_log("dir: %0.17f %0.17f %0.17f\n" , V3ARGS(ap->a_ray.r_dir));
	}

	if (dist < tinfo->ttol) {
	    if (tinfo->verbose) {
		bu_log("	dist: %f\n", dist);
		bu_log("	in_surfno: %d\n", s->seg_in.hit_surfno);
		bu_log("	out_surfno: %d\n", s->seg_out.hit_surfno);
		bu_log("	%s thin dist: %0.17f\n",  pp->pt_regionp->reg_name, dist);
		bu_log("	center: %0.17f %0.17f %0.17f\n" , V3ARGS(ap->a_ray.r_pt));
		bu_log("	dir: %0.17f %0.17f %0.17f\n" , V3ARGS(ap->a_ray.r_dir));
	    }
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
    if (tinfo->verbose) {
	bu_log("		miss\n");
	bu_log("		center: %0.17f %0.17f %0.17f\n" , V3ARGS(ap->a_ray.r_pt));
	bu_log("		dir: %0.17f %0.17f %0.17f\n" , V3ARGS(ap->a_ray.r_dir));
    }
    return 0;
}

/* I don't think this is supposed to happen with a single primitive, but just
 * in case we get an overlap report somehow flag it as trouble */
static int
_tc_overlap(struct application *ap,
	struct partition *UNUSED(pp),
	struct region *UNUSED(reg1),
	struct region *UNUSED(reg2),
	struct partition *UNUSED(hp))
{
    struct coplanar_info *tinfo = (struct coplanar_info *)ap->a_uptr;
    //tinfo->is_thin = 1;
    if (tinfo->verbose) {
	bu_log("		overlap\n");
	bu_log("		center: %0.17f %0.17f %0.17f\n" , V3ARGS(ap->a_ray.r_pt));
	bu_log("		dir: %0.17f %0.17f %0.17f\n" , V3ARGS(ap->a_ray.r_dir));
    }
    return 0;
}

int
_ged_lint_bot_thin_check(struct bu_ptbl *ofaces, struct rt_bot_internal *bot, struct rt_i *rtip, double ttol, int verbose)
{
    if (!bot || bot->mode != RT_BOT_SOLID || !rtip || !bot->num_faces)
	return 0;

    int found_thin = 0;
    struct coplanar_info tinfo;
    tinfo.ttol = ttol;
    tinfo.verbose = verbose;
    tinfo.bot = bot;

    // Set up the raytrace
    if (!BU_LIST_IS_INITIALIZED(&rt_uniresource.re_parthead))
	rt_init_resource(&rt_uniresource, 0, rtip);
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
	VSCALE(backout, backout, SQRT_SMALL_FASTF);
	// Reverse the triangle normal for a ray direction
	VREVERSE(rnorm, n);

	point_t rpnts[3];
	point_t tcenter;
	VMOVE(rpnts[0], &bot->vertices[bot->faces[i*3+0]*3]);
	VMOVE(rpnts[1], &bot->vertices[bot->faces[i*3+1]*3]);
	VMOVE(rpnts[2], &bot->vertices[bot->faces[i*3+2]*3]);
	VADD3(tcenter, rpnts[0], rpnts[1], rpnts[2]);
	VSCALE(tcenter, tcenter, 1.0/3.0);

	// Take the shot
	tinfo.is_thin = 0;
	VMOVE(ap.a_ray.r_dir, rnorm);
	VADD2(ap.a_ray.r_pt, tcenter, backout);
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


// TODO - A useful correctness audit for a BoT might be to shotline both the
// CSG and the BoT using the same rays constructed from the triangle centers -
// if the CSG reports a non-zero LOS but the BoT reports zero, we have a
// problem.
static int
_ck_up_hit(struct application *ap, struct partition *PartHeadp, struct seg *UNUSED(segs))
{
    if (PartHeadp->pt_forw == PartHeadp)
	return 1;

    struct coplanar_info *pinfo = (struct coplanar_info *)ap->a_uptr;

    struct partition *pp = PartHeadp->pt_forw;

    // If we've got something too close above our triangle, it's trouble
    if (pp->pt_inhit->hit_dist < VUNITIZE_TOL) {
	if (pinfo->verbose) {
	    bu_log("	Another triangle right above our triangle\n");
	    bu_log("	in_surfno: %d\n", pp->pt_inhit->hit_surfno);
	    bu_log("	out_surfno: %d\n", pp->pt_outhit->hit_surfno);
	    bu_log("	center: %0.17f %0.17f %0.17f\n" , V3ARGS(ap->a_ray.r_pt));
	    bu_log("	dir: %0.17f %0.17f %0.17f\n" , V3ARGS(ap->a_ray.r_dir));
	}
	pinfo->have_above = 1;
	pinfo->problem_indices.insert(pp->pt_inhit->hit_surfno);
	return 0;
    }

    return 0;
}

static int
_ck_up_miss(struct application *UNUSED(ap))
{
    return 0;
}

int
_ged_lint_bot_close_check(struct bu_ptbl *ofaces, struct rt_bot_internal *bot, struct rt_i *rtip, double ttol, int verbose)
{
    if (!bot || bot->mode != RT_BOT_SOLID || !rtip || !bot->num_faces)
	return 0;

    int have_above = 0;
    struct coplanar_info cpinfo;
    cpinfo.ttol = ttol;
    cpinfo.verbose = verbose;
    cpinfo.have_above = 0;

    // Set up the raytrace
    if (!BU_LIST_IS_INITIALIZED(&rt_uniresource.re_parthead))
	rt_init_resource(&rt_uniresource, 0, rtip);
    struct application ap;
    RT_APPLICATION_INIT(&ap);
    ap.a_rt_i = rtip;     /* application uses this instance */
    ap.a_hit = _ck_up_hit;     /* where to go on a hit */
    ap.a_miss = _ck_up_miss;   /* where to go on a miss */
    ap.a_onehit = 1;
    ap.a_resource = &rt_uniresource;
    ap.a_uptr = (void *)&cpinfo;

    for (size_t i = 0; i < bot->num_faces; i++) {
	cpinfo.curr_tri = (int)i;
	vect_t rnorm, n, backout;
	if (!bot_face_normal(&n, bot, i))
	    continue;

	point_t rpnts[3];
	point_t tcenter;
	VMOVE(rpnts[0], &bot->vertices[bot->faces[i*3+0]*3]);
	VMOVE(rpnts[1], &bot->vertices[bot->faces[i*3+1]*3]);
	VMOVE(rpnts[2], &bot->vertices[bot->faces[i*3+2]*3]);
	VADD3(tcenter, rpnts[0], rpnts[1], rpnts[2]);
	VSCALE(tcenter, tcenter, 1.0/3.0);

	VMOVE(ap.a_ray.r_dir, n);

	VREVERSE(rnorm, n);
	VMOVE(backout, rnorm);
	VSCALE(backout, backout, SMALL_FASTF);
	VADD2(ap.a_ray.r_pt, tcenter, backout);
	(void)rt_shootray(&ap);

	if (cpinfo.have_above)
	    have_above = 1;

	if (!ofaces && cpinfo.have_above)
	    break;
    }

    if (ofaces) {
	std::set<int>::iterator p_it;
	for (p_it = cpinfo.problem_indices.begin(); p_it != cpinfo.problem_indices.end(); ++p_it) {
	    int ind = *p_it;
	    bu_ptbl_ins(ofaces, (long *)(long)ind);
	}
    }

    return have_above;
}


static int
_mc_hit(struct application *UNUSED(ap), struct partition *PartHeadp, struct seg *UNUSED(segs))
{
    if (PartHeadp->pt_forw == PartHeadp)
	return 1;

    return 0;
}

static int
_mc_miss(struct application *ap)
{
    // We are shooting directly into the center of a triangle from right above
    // it.  If we miss under those conditions, it can only happen because
    // something is wrong.
    struct coplanar_info *tinfo = (struct coplanar_info *)ap->a_uptr;
    tinfo->unexpected_miss = 1;
    if (tinfo->verbose) {
	bu_log("		miss\n");
	bu_log("		center: %0.17f %0.17f %0.17f\n" , V3ARGS(ap->a_ray.r_pt));
	bu_log("		dir: %0.17f %0.17f %0.17f\n" , V3ARGS(ap->a_ray.r_dir));
    }
    return 0;
}

static int
_mc_overlap(struct application *UNUSED(ap),
	struct partition *UNUSED(pp),
	struct region *UNUSED(reg1),
	struct region *UNUSED(reg2),
	struct partition *UNUSED(hp))
{
    return 0;
}

int
_ged_lint_bot_miss_check(struct bu_ptbl *ofaces, struct rt_bot_internal *bot, struct rt_i *rtip, double ttol, int verbose)
{
    if (!bot || bot->mode != RT_BOT_SOLID || !rtip || !bot->num_faces)
	return 0;

    int found_miss = 0;
    struct coplanar_info minfo;
    minfo.ttol = ttol;
    minfo.verbose = verbose;
    minfo.unexpected_miss = 0;

    // Set up the raytrace
    if (!BU_LIST_IS_INITIALIZED(&rt_uniresource.re_parthead))
	rt_init_resource(&rt_uniresource, 0, rtip);
    struct application ap;
    RT_APPLICATION_INIT(&ap);
    ap.a_rt_i = rtip;     /* application uses this instance */
    ap.a_hit = _mc_hit;    /* where to go on a hit */
    ap.a_miss = _mc_miss;  /* where to go on a miss */
    ap.a_overlap = _mc_overlap;  /* where to go if an overlap is found */
    ap.a_onehit = 0; /* One hit isn't enough - we want to know we've got something solid */
    ap.a_resource = &rt_uniresource;
    ap.a_uptr = (void *)&minfo;

    for (size_t i = 0; i < bot->num_faces; i++) {
	minfo.curr_tri = (int)i;
	vect_t rnorm, n, backout;
	if (!bot_face_normal(&n, bot, i))
	    continue;

	// We want backout to get the ray origin off the triangle surface
	VMOVE(backout, n);
	VSCALE(backout, backout, SQRT_SMALL_FASTF);
	// Reverse the triangle normal for a ray direction
	VREVERSE(rnorm, n);

	point_t rpnts[3];
	point_t tcenter;
	VMOVE(rpnts[0], &bot->vertices[bot->faces[i*3+0]*3]);
	VMOVE(rpnts[1], &bot->vertices[bot->faces[i*3+1]*3]);
	VMOVE(rpnts[2], &bot->vertices[bot->faces[i*3+2]*3]);
	VADD3(tcenter, rpnts[0], rpnts[1], rpnts[2]);
	VSCALE(tcenter, tcenter, 1.0/3.0);

	// Take the shot
	minfo.is_thin = 0;
	VMOVE(ap.a_ray.r_dir, rnorm);
	VADD2(ap.a_ray.r_pt, tcenter, backout);
	(void)rt_shootray(&ap);

	if (minfo.unexpected_miss) {
	    found_miss = 1;
	}

	if (!ofaces && found_miss)
	    break;
    }

    if (ofaces) {
	std::set<int>::iterator p_it;
	for (p_it = minfo.problem_indices.begin(); p_it != minfo.problem_indices.end(); ++p_it) {
	    int ind = *p_it;
	    bu_ptbl_ins(ofaces, (long *)(long)ind);
	}
    }

    return found_miss;
}


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
