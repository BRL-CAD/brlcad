/*              S A M P L I N G _ C H E C K S . C P P
 * BRL-CAD
 *
 * Copyright (c) 2024-2025 United States Government as represented by
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
    if (s->seg_in.hit_dist > 2*SQRT_SMALL_FASTF) {
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
    RT_UNIRESOURCE_INIT();
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

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
