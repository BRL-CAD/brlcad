/*			G _ M E T A B A L L . C
 * BRL-CAD
 *
 * Copyright (c) 1985-2011 United States Government as represented by
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
/** @addtogroup primitives */
/** @{ */
/** @file primitives/metaball/metaball.c
 *
 * Intersect a ray with a metaball implicit surface.
 *
 * Algorithm:
 *
 * The heart of it is the set of point evaluation functions. These are
 * different for each "type" of blobby object (metaballs, blinn blobs,
 * and iso-potential contours). All are simple summation formulas at
 * the moment.
 *
 * Plot merely draws sphere type objects around the control points,
 * with size related to each points 'strength' and the primitives
 * threshold.
 *
 * Ray-tracing is incredibly hackish. The ray is walked in a fairly
 * coarse matter until the point evaluation crosses the threshold
 * value, then a basic binary search is done to refine the
 * approximated hit point.
 *
 * THIS PRIMITIVE IS INCOMPLETE AND SHOULD BE CONSIDERED EXPERIMENTAL.
 *
 */
/** @} */

#include "common.h"

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "bin.h"

#include "vmath.h"
#include "db.h"
#include "nmg.h"
#include "rtgeom.h"
#include "raytrace.h"
#include "nurb.h"
#include "wdb.h"

#include "metaball.h"

#define SQ(a) ((a)*(a))

#define PLOT_THE_BIG_BOUNDING_SPHERE 0

const char *metaballnames[] =
{
    "Metaball",
    "Isopotential",
    "Blob",
    NULL
};


int rt_metaball_lookup_type_id(const char *name)
{
    int i = 0;
    while (metaballnames[i++])
	if (!strncmp(metaballnames[i], name, 4))
	    return i;
    return -1;
}


const char *rt_metaball_lookup_type_name(const int id)
{
    return metaballnames[id];
}


/* compute the bounding sphere for a metaball cluster. center is
 * filled, and the radius is returned.
 */
fastf_t
rt_metaball_get_bounding_sphere(point_t *center, fastf_t threshold, struct rt_metaball_internal *mb)
{
    struct wdb_metaballpt *mbpt, *mbpt2;
    point_t min, max, d;
    fastf_t r = 0.0, dist, mag;
    int i;
    struct bu_list *points;

    points = &mb->metaball_ctrl_head;

    /* find a bounding box for the POINTS (NOT the surface) */
    VSETALL(min, +INFINITY);
    VSETALL(max, -INFINITY);
    for (BU_LIST_FOR(mbpt, wdb_metaballpt, points))
	for (i=0;i<3;i++) {
	    if (mbpt->coord[i] < min[i])
		min[i] = mbpt->coord[i];
	    if (mbpt->coord[i] > max[i])
		max[i] = mbpt->coord[i];
	}
    /* return -1 if no points are defined. */
    if (ZERO(min[X] - INFINITY) || min[X] > INFINITY)
	return -1;

    /* compute the center of the generated box, call that the center */
    VADD2(*center, min, max);
    VSCALE(*center, *center, 0.5);

    /* start looking for the radius... */
    for (BU_LIST_FOR(mbpt, wdb_metaballpt, points)) {
	VSUB2(d, mbpt->coord, *center);
	/* since the surface is where threshold=fldstr/mag,
	   mag=fldstr/threshold, so make that the initial value */
	dist = MAGNITUDE(d) + mbpt->fldstr/threshold;
	/* and add all the contribution */
	for (BU_LIST_FOR(mbpt2, wdb_metaballpt, points))
	    if (mbpt2 != mbpt) {
		fastf_t additive = 0.0;
		VSUB2(d, mbpt2->coord, mbpt->coord);
		mag = MAGNITUDE(d) + dist;

		switch(mb->method) {
		    case METABALL_METABALL:
			break;
		    case METABALL_ISOPOTENTIAL:
			additive = fabs(mbpt2->fldstr) * mbpt2->fldstr / mag;
			break;
		    case METABALL_BLOB:
			additive = 1.0/exp((mbpt2->sweat / (mbpt2->fldstr * mbpt2->fldstr)) * mag * mag - mbpt2->sweat);
			break;
		}

		dist += additive;
	    }
	/* then see if this is a 'defining' point */
	if (dist > r)
	    r = dist;
    }
    return r;
}

/**
 * R T _ M E T A B A L L _ B B O X
 *
 * Calculate a bounding RPP around a metaball
 */
int
rt_metaball_bbox(struct rt_db_internal *ip, point_t *min, point_t *max)
{
    struct rt_metaball_internal *mb;
    point_t center;
    fastf_t radius;
    mb = (struct rt_metaball_internal *)ip->idb_ptr;
    RT_METABALL_CK_MAGIC(mb);
    VSETALL(center, 0);

    radius = rt_metaball_get_bounding_sphere(&center, mb->threshold, mb);

    VSET((*min), center[X] - radius, center[Y] - radius, center[Z] - radius);
    VSET((*max), center[X] + radius, center[Y] + radius, center[Z] + radius);
    return 0;
}


/**
 * R T _ M E T A B A L L _ P R E P
 *
 * prep and build bounding volumes... unfortunately, generating the
 * bounding sphere is too 'loose' (I think) and O(n^2).
 */
int
rt_metaball_prep(struct soltab *stp, struct rt_db_internal *ip, struct rt_i *rtip)
{
    struct rt_metaball_internal *mb, *nmb;
    struct wdb_metaballpt *mbpt, *nmbpt;
    fastf_t minfstr = +INFINITY;

    if (rtip) RT_CK_RTI(rtip);

    mb = ip->idb_ptr;
    RT_METABALL_CK_MAGIC(mb);

    /* generate a copy of the metaball */
    nmb = bu_malloc(sizeof(struct rt_metaball_internal), "rt_metaball_prep: nmb");
    nmb->magic = RT_METABALL_INTERNAL_MAGIC;
    BU_LIST_INIT(&nmb->metaball_ctrl_head);
    nmb->threshold = mb->threshold;
    nmb->method = mb->method;

    /* and copy the list of control points */
    for (BU_LIST_FOR(mbpt, wdb_metaballpt, &mb->metaball_ctrl_head)) {
	nmbpt = (struct wdb_metaballpt *)bu_malloc(sizeof(struct wdb_metaballpt), "rt_metaball_prep: nmbpt");
	nmbpt->fldstr = mbpt->fldstr;
	if (mbpt->fldstr < minfstr)
	    minfstr = mbpt->fldstr;
	nmbpt->sweat = mbpt->sweat;
	VMOVE(nmbpt->coord, mbpt->coord);
	BU_LIST_INSERT(&nmb->metaball_ctrl_head, &nmbpt->l);
    }

    /* find the bounding sphere */
    stp->st_aradius = rt_metaball_get_bounding_sphere(&stp->st_center, mb->threshold, mb);
    stp->st_bradius = stp->st_aradius * 1.01;

    /* XXX magic numbers, increase if scalloping is observed. :(*/
    nmb->initstep = minfstr / 2.0;
    if (nmb->initstep < (stp->st_aradius / 200.0))
	nmb->initstep = (stp->st_aradius / 200.0);
    else if (nmb->initstep > (stp->st_aradius / 10.0))
	nmb->initstep = (stp->st_aradius / 10.0);
    nmb->finalstep = /*stp->st_aradius * */minfstr / 1e5;

    /* generate a bounding box around the sphere...
     * XXX this can be optimized greatly to reduce the BSP presense... */
    if (rt_metaball_bbox(ip, &(stp->st_min), &(stp->st_max))) return 1;
    stp->st_specific = (void *)nmb;
    return 0;
}


/**
 * R T _ M E T A B A L L _ P R I N T
 */
void
rt_metaball_print(register const struct soltab *stp)
{
    int metaball_count = 0;
    struct rt_metaball_internal *mb;
    struct wdb_metaballpt *mbpt;

    mb = (struct rt_metaball_internal *)stp->st_specific;
    RT_METABALL_CK_MAGIC(mb);
    for (BU_LIST_FOR(mbpt, wdb_metaballpt, &mb->metaball_ctrl_head)) ++metaball_count;
    bu_log("Metaball with %d points and a threshold of %g (%s rendering)\n", metaball_count, mb->threshold, rt_metaball_lookup_type_name(mb->method));
    metaball_count=0;
    for (BU_LIST_FOR(mbpt, wdb_metaballpt, &mb->metaball_ctrl_head))
	bu_log("\t%d: %g field strength at (%g, %g, %g) and 'goo' of %g\n", ++metaball_count, mbpt->fldstr, V3ARGS(mbpt->coord), mbpt->sweat);
    return;
}


/**
 * R T _ M E T A B A L L P T _ P R I N T
 */
void
rt_metaballpt_print(const struct wdb_metaballpt *metaball, double mm2local)
{
    point_t p1;

    bu_log("Metaball Point:\n");
    VSCALE(p1, metaball->coord, mm2local);
    bu_log("\tat (%g %g %g)\n", V3ARGS(p1));
    bu_log("\tfield strength = %g\n", metaball->fldstr*mm2local);
    return;
}


fastf_t
rt_metaball_point_value_metaball(const point_t *p, const struct bu_list *points)
{
    if (!p || !points) bu_bomb("oops");

    bu_log("ERROR: rt_metaball_point_value_metaball() is not implemented");

    /* Makes the compiler happy */
    return 0.0;
}


fastf_t
rt_metaball_point_value_iso(const point_t *p, const struct bu_list *points)
{
    struct wdb_metaballpt *mbpt;
    fastf_t ret = 0.0;
    point_t v;

    for (BU_LIST_FOR(mbpt, wdb_metaballpt, points)) {
	VSUB2(v, mbpt->coord, *p);
	ret += fabs(mbpt->fldstr) * mbpt->fldstr / MAGSQ(v);	/* f/r^2 */
    }
    return ret;
}


fastf_t
rt_metaball_point_value_blob(const point_t *p, const struct bu_list *points)
{
    struct wdb_metaballpt *mbpt;
    fastf_t ret = 0.0;
    point_t v;

    for (BU_LIST_FOR(mbpt, wdb_metaballpt, points)) {
	/* TODO: test if sweat is sufficient enough that r=0 returns a positive value? */
	/* TODO: test to see if negative contribution needs to be wiped out? */
	VSUB2(v, mbpt->coord, *p);
	ret += 1.0 / exp((mbpt->sweat/(mbpt->fldstr*mbpt->fldstr)) * MAGSQ(v) - mbpt->sweat);
    }
    return ret;
}


/* main point evaluation function, to be exposed to the ugly outside
 * world.
 */
fastf_t
rt_metaball_point_value(const point_t *p, const struct rt_metaball_internal *mb)
{
    RT_METABALL_CK_MAGIC(mb);
    switch (mb->method) {
	case METABALL_METABALL:
	    return rt_metaball_point_value_metaball(p, &mb->metaball_ctrl_head);
	case METABALL_ISOPOTENTIAL:
	    return rt_metaball_point_value_iso(p, &mb->metaball_ctrl_head);
	case METABALL_BLOB:
	    return rt_metaball_point_value_blob(p, &mb->metaball_ctrl_head);
	default:
	    break;
    }
    return 0;
}


/* returns true if the point is in/on the unscaled metaball. */
int
rt_metaball_point_inside(const point_t *p, const struct rt_metaball_internal *mb)
{
    return rt_metaball_point_value(p, mb) >= mb->threshold;
}


/*
 * Solve the surface intersection of mb with an accuracy of finalstep given that
 * one of the two points (a and b) are inside and the other is outside.
 */
int
rt_metaball_find_intersection(point_t *intersect, const struct rt_metaball_internal *mb, const point_t *a, const point_t *b, fastf_t step, const fastf_t finalstep)
{
    point_t mid;
    const point_t *midp;

    midp = (const point_t *)&mid;

    VADD2(mid, *a, *b);
    VSCALE(mid, mid, 0.5);

    if (finalstep > step) {
	VMOVE(*intersect, mid);	/* should this be the midpoint between a and b? */
	return 0;
    }

    /* should probably make a or b necessarily inside, to eliminate one point
     * computation? */
    return rt_metaball_find_intersection(intersect, mb, midp, (rt_metaball_point_inside(a, mb) == rt_metaball_point_inside(midp, mb)) ?b:a , step/2.0, finalstep);
}


/*
 * R T _ M E T A B A L L _ S H O T
 */
int
rt_metaball_shot(struct soltab *stp, register struct xray *rp, struct application *ap, struct seg *seghead)
{
    struct rt_metaball_internal *mb = (struct rt_metaball_internal *)stp->st_specific;
    struct seg *segp = NULL;
    int retval = 0;
    fastf_t step, distleft;
    point_t p, inc;

    /* switching behavior to retain old code for performance and correctness
     * comparisons. */
#define SHOOTALGO 3

#if SHOOTALGO == 2
    int fhin = 1;
#endif

    step = mb->initstep;
    distleft = (rp->r_max-rp->r_min) + step * 3.0;

    VMOVE(p, rp->r_pt);
    VSCALE(inc, rp->r_dir, step); /* assume it's normalized and we want to creep at step */

    /* walk back out of the solid */
    while(rt_metaball_point_value((const point_t *)&p, mb) >= mb->threshold) {
#if SHOOTALGO == 2
	fhin = -1;
#endif
	distleft += step;
	VSUB2(p, p, inc);
    }

#if SHOOTALGO == 2
    /* we hit, but not as fine-grained as we want. So back up one step,
     * cut the step size in half and start over...
     */
    {
	int mb_stat = 0, segsleft = abs(ap->a_onehit);
	point_t delta;

#define STEPBACK { distleft += step; VSUB2(p, p, inc); step *= .5; VSCALE(inc, inc, .5); }
#define STEPIN(x) { \
    --segsleft; \
    ++retval; \
    VSUB2(delta, p, rp->r_pt); \
    segp->seg_##x.hit_dist = fhin * MAGNITUDE(delta); \
    segp->seg_##x.hit_surfno = 0; }
	while (mb_stat == 0 && distleft >= -0) {
	    int in;

	    distleft -= step;
	    VADD2(p, p, inc);
	    in = rt_metaball_point_value((const point_t *)&p, mb) > mb->threshold;
	    if (mb_stat == 1)
		if ( !in )
		    if (step<=mb->finalstep) {
			STEPIN(out)
			step = mb->initstep;
			mb_stat = 0;
			if (ap->a_onehit != 0 || segsleft <= 0)
			    return retval;
		    } else
			STEPBACK
	    else
		if ( in )
		    if (step<=mb->finalstep) {
			RT_GET_SEG(segp, ap->a_resource);
			segp->seg_stp = stp;
			STEPIN(in)
			fhin = 1;
			BU_LIST_INSERT(&(seghead->l), &(segp->l));
			/* reset the ray-walk shtuff */
			mb_stat = 1;
			VADD2(p, p, inc);	/* set p to a point inside */
			step = mb->initstep;
		    } else
			STEPBACK
	}
    }
#undef STEPBACK
#undef STEPIN
#elif SHOOTALGO == 3
    {
	int mb_stat = 0, segsleft = abs(ap->a_onehit);
	point_t lastpoint;

	while (distleft >= 0.0 || mb_stat == 1) {
	    /* advance to the next point */
	    distleft -= step;
	    VMOVE(lastpoint, p);
	    VADD2(p, p, inc);
	    if (mb_stat == 1) {
		if (rt_metaball_point_value((const point_t *)&p, mb) < mb->threshold) {
		    point_t intersect, delta;
		    const point_t *pA = (const point_t *)&lastpoint;
		    const point_t *pB = (const point_t *)&p;
		    rt_metaball_find_intersection(&intersect, mb, pA, pB, step, mb->finalstep);
		    VMOVE(segp->seg_out.hit_point, intersect);
		    --segsleft;
		    ++retval;
		    VSUB2(delta, intersect, rp->r_pt);
		    segp->seg_out.hit_dist = MAGNITUDE(delta);
		    segp->seg_out.hit_surfno = 0;
		    mb_stat = 0;
		    if (ap->a_onehit != 0 && segsleft <= 0)
			return retval;
		}
	    } else {
		if (rt_metaball_point_value((const point_t *)&p, mb) > mb->threshold) {
		    point_t intersect, delta;
		    const point_t *pA = (const point_t *)&lastpoint;
		    const point_t *pB = (const point_t *)&p;
		    rt_metaball_find_intersection(&intersect, mb, pA, pB, step, mb->finalstep);
		    RT_GET_SEG(segp, ap->a_resource);
		    segp->seg_stp = stp;
		    --segsleft;
		    ++retval;
		    VMOVE(segp->seg_in.hit_point, intersect);
		    VSUB2(delta, intersect, rp->r_pt);
		    segp->seg_in.hit_dist = MAGNITUDE(delta);
		    segp->seg_in.hit_surfno = 0;
		    BU_LIST_INSERT(&(seghead->l), &(segp->l));

		    mb_stat = 1;
		    step = mb->initstep;
		}
	    }
	}
    }

#else
# error "pick a valid algo."
#endif

    return retval;
}

inline void
rt_metaball_norm_internal(vect_t *n, point_t *p, struct rt_metaball_internal *mb)
{
    struct wdb_metaballpt *mbpt;
    vect_t v;
    fastf_t a;

    VSETALL(*n, 0.0);

    switch (mb->method) {
	case METABALL_METABALL: bu_log("Sorry, strict metaballs are not yet implemented\n");
	    break;
	case METABALL_ISOPOTENTIAL:
	    for (BU_LIST_FOR(mbpt, wdb_metaballpt, &mb->metaball_ctrl_head)) {
		VSUB2(v, *p, mbpt->coord);
		a = MAGSQ(v);
		VJOIN1(*n, *n, fabs(mbpt->fldstr)*mbpt->fldstr / (SQ(a)), v);	/* f/r^4 */
	    }
	    break;
	case METABALL_BLOB:
	    for (BU_LIST_FOR(mbpt, wdb_metaballpt, &mb->metaball_ctrl_head)) {
		VSUB2(v, *p, mbpt->coord);
		a = MAGSQ(v);
		VJOIN1(*n, *n, 2.0*mbpt->sweat/SQ(mbpt->fldstr)*exp(mbpt->sweat*(1-(a/SQ(mbpt->fldstr)))) , v);
	    }
	    break;
	default: bu_log("unknown metaball method\n"); break;
    }
    VUNITIZE(*n);
}

/**
 * R T _ M E T A B A L L _ N O R M
 *
 * Given ONE ray distance, return the normal and entry/exit point.
 */
void
rt_metaball_norm(register struct hit *hitp, struct soltab *stp, register struct xray *rp)
{
    if (rp) RT_CK_RAY(rp);	/* unused. */
    rt_metaball_norm_internal(&(hitp->hit_normal), &(hitp->hit_point), (struct rt_metaball_internal *)(stp->st_specific));
    return;
}


/**
 * R T _ M E T A B A L L _ C U R V E
 *
 * Return the curvature of the metaball.
 */
void
rt_metaball_curve(struct curvature *cvp, struct hit *hitp, struct soltab *stp)
{
    struct rt_metaball_internal *metaball = (struct rt_metaball_internal *)stp->st_specific;

    if (!metaball || !cvp) return;
    if (hitp) RT_CK_HIT(hitp);

    bu_log("ERROR: rt_metaball_curve() is not implemented\n");
    return;
}


/**
 * R T _ M E T A B A L L _ U V
 *
 * For a hit on the surface of an METABALL, return the (u, v)
 * coordinates of the hit point, 0 <= u, v <= 1.
 *
 * u = azimuth
 * v = elevation
 */
void
rt_metaball_uv(struct application *ap, struct soltab *stp, struct hit *hitp, struct uvcoord *uvp)
{
    struct rt_metaball_internal *metaball = (struct rt_metaball_internal *)stp->st_specific;
    vect_t work, pprime;
    fastf_t r;

    if (ap) RT_CK_APPLICATION(ap);
    if (stp) RT_CK_SOLTAB(stp);
    if (hitp) RT_CK_HIT(hitp);
    if (!uvp) return;
    if (!metaball) return;

    /* stuff stolen from sph */
    VSUB2(work, hitp->hit_point, stp->st_center);
    VSCALE(pprime, work, 1.0/MAGNITUDE(work));
    /* Assert that pprime has unit length */

    /* U is azimuth, atan() range: -pi to +pi */
    uvp->uv_u = bn_atan2(pprime[Y], pprime[X]) * bn_inv2pi;
    if (uvp->uv_u < 0)
	uvp->uv_u += 1.0;
    /*
     * V is elevation, atan() range: -pi/2 to +pi/2, because sqrt()
     * ensures that X parameter is always >0
     */
    uvp->uv_v = bn_atan2(pprime[Z],
			 sqrt(pprime[X] * pprime[X] + pprime[Y] * pprime[Y])) *
	bn_invpi + 0.5;

    /* approximation: r / (circumference, 2 * pi * aradius) */
    r = ap->a_rbeam + ap->a_diverge * hitp->hit_dist;
    uvp->uv_du = uvp->uv_dv =
	bn_inv2pi * r / stp->st_aradius;
    return;
}


/**
 * R T _ M E T A B A L L _ F R E E
 */
void
rt_metaball_free(register struct soltab *stp)
{
    struct rt_metaball_internal *metaball = (struct rt_metaball_internal *)stp->st_specific;

    bu_free((char *)metaball, "rt_metaball_internal");
}


int
rt_metaball_class(const struct soltab *stp, const fastf_t *min, const fastf_t *max, const struct bn_tol *tol)
{
    if (stp) RT_CK_SOLTAB(stp);
    if (tol) BN_CK_TOL(tol);
    if (!min) return 0;
    if (!max) return 0;

    return 0;
}


void
rt_metaball_plot_sph(struct bu_list *vhead, point_t *center, fastf_t radius)
{
    fastf_t top[16*3], middle[16*3], bottom[16*3];
    point_t a, b, c;
    int i;

    BU_CK_LIST_HEAD(vhead);

    VSET(a, radius, 0, 0);
    VSET(b, 0, radius, 0);
    VSET(c, 0, 0, radius);
    rt_ell_16pts(top, *center, a, b);
    rt_ell_16pts(bottom, *center, b, c);
    rt_ell_16pts(middle, *center, a, c);

    RT_ADD_VLIST(vhead, &top[15*ELEMENTS_PER_VECT], BN_VLIST_LINE_MOVE);
    for (i=0; i<16; i++) RT_ADD_VLIST(vhead, &top[i*ELEMENTS_PER_VECT], BN_VLIST_LINE_DRAW);
    RT_ADD_VLIST(vhead, &bottom[15*ELEMENTS_PER_VECT], BN_VLIST_LINE_MOVE);
    for (i=0; i<16; i++) RT_ADD_VLIST(vhead, &bottom[i*ELEMENTS_PER_VECT], BN_VLIST_LINE_DRAW);
    RT_ADD_VLIST(vhead, &middle[15*ELEMENTS_PER_VECT], BN_VLIST_LINE_MOVE);
    for (i=0; i<16; i++) RT_ADD_VLIST(vhead, &middle[i*ELEMENTS_PER_VECT], BN_VLIST_LINE_DRAW);
}


/**
 * R T _ M E T A B A L L _ P L O T
 */
int
rt_metaball_plot(struct bu_list *vhead, struct rt_db_internal *ip, const struct rt_tess_tol *UNUSED(ttol), const struct bn_tol *UNUSED(tol), const struct rt_view_info *UNUSED(info))
{
    struct rt_metaball_internal *mb;
    struct wdb_metaballpt *mbpt;
    point_t bsc;
    fastf_t rad;

    BU_CK_LIST_HEAD(vhead);
    RT_CK_DB_INTERNAL(ip);
    mb = (struct rt_metaball_internal *)ip->idb_ptr;
    RT_METABALL_CK_MAGIC(mb);
    rad = rt_metaball_get_bounding_sphere(&bsc, mb->threshold, mb);
    /* cope with the case where 0 points are defined. */
    if (rad<0)
	return 0;
#if PLOT_THE_BIG_BOUNDING_SPHERE
    rt_metaball_plot_sph(vhead, &bsc, rad);
#endif
    for (BU_LIST_FOR(mbpt, wdb_metaballpt, &mb->metaball_ctrl_head))
	rt_metaball_plot_sph(vhead, &mbpt->coord, mbpt->fldstr / mb->threshold);
    return 0;
}

/**
 * R T _ M E T A B A L L _ I M P O R T 5
 *
 * Import an metaball/sphere from the database format to the internal
 * structure. Apply modeling transformations as well.
 */
int
rt_metaball_import5(struct rt_db_internal *ip, const struct bu_external *ep, register const fastf_t *mat, const struct db_i *dbip)
{
    struct wdb_metaballpt *mbpt;
    struct rt_metaball_internal *mb;
    fastf_t *buf;
    int metaball_count = 0, i;

    if (dbip) RT_CK_DBI(dbip);

    BU_CK_EXTERNAL(ep);
    metaball_count = ntohl(*(uint32_t *)ep->ext_buf);
    buf = (fastf_t *)bu_malloc((metaball_count*5+1)*SIZEOF_NETWORK_DOUBLE, "rt_metaball_import5: buf");
    ntohd((unsigned char *)buf, (unsigned char *)ep->ext_buf+2*SIZEOF_NETWORK_LONG, metaball_count*5+1);

    RT_CK_DB_INTERNAL(ip);
    ip->idb_major_type = DB5_MAJORTYPE_BRLCAD;
    ip->idb_type = ID_METABALL;
    ip->idb_meth = &rt_functab[ID_METABALL];
    ip->idb_ptr = bu_malloc(sizeof(struct rt_metaball_internal), "rt_metaball_internal");
    mb = (struct rt_metaball_internal *)ip->idb_ptr;
    mb->magic = RT_METABALL_INTERNAL_MAGIC;
    mb->method = ntohl(*(uint32_t *)(ep->ext_buf + SIZEOF_NETWORK_LONG));
    mb->threshold = buf[0];
    BU_LIST_INIT(&mb->metaball_ctrl_head);
    if (mat == NULL) mat = bn_mat_identity;
    for (i=1; i<=metaball_count*5; i+=5) {
	/* Apply modeling transformations */
	BU_GETSTRUCT(mbpt, wdb_metaballpt);
	mbpt->l.magic = WDB_METABALLPT_MAGIC;
	MAT4X3PNT(mbpt->coord, mat, &buf[i]);
	mbpt->fldstr = buf[i+3] / mat[15];
	mbpt->sweat = buf[i+4];
	BU_LIST_INSERT(&mb->metaball_ctrl_head, &mbpt->l);
    }

    bu_free((genptr_t)buf, "rt_metaball_import5: buf");
    return 0;		/* OK */
}


/**
 * R T _ M E T A B A L L _ E X P O R T 5
 *
 * storage is something like
 * long numpoints
 * long method
 * fastf_t threshold
 * fastf_t X1 (start point)
 * fastf_t Y1
 * fastf_t Z1
 * fastf_t fldstr1
 * fastf_t sweat1 (end point)
 * fastf_t X2 (start point)
 * ...
 */
int
rt_metaball_export5(struct bu_external *ep, const struct rt_db_internal *ip, double local2mm, const struct db_i *dbip)
{
    struct rt_metaball_internal *mb;
    struct wdb_metaballpt *mbpt;
    int metaball_count = 0, i = 1;
    fastf_t *buf = NULL;

    if (dbip) RT_CK_DBI(dbip);

    RT_CK_DB_INTERNAL(ip);
    if (ip->idb_type != ID_METABALL)
	return -1;
    mb = (struct rt_metaball_internal *)ip->idb_ptr;
    RT_METABALL_CK_MAGIC(mb);
    if (mb->metaball_ctrl_head.magic == 0) return -1;

    /* Count number of points */
    for (BU_LIST_FOR(mbpt, wdb_metaballpt, &mb->metaball_ctrl_head)) metaball_count++;

    BU_CK_EXTERNAL(ep);
    ep->ext_nbytes = SIZEOF_NETWORK_DOUBLE*(1+5*metaball_count) + 3*SIZEOF_NETWORK_LONG;
    ep->ext_buf = (genptr_t)bu_malloc(ep->ext_nbytes, "metaball external");
    if (ep->ext_buf == NULL)
	bu_bomb("Failed to allocate DB space!\n");
    *(uint32_t *)ep->ext_buf = htonl(metaball_count);
    *(uint32_t *)(ep->ext_buf + SIZEOF_NETWORK_LONG) = htonl(mb->method);

    /* pack the point data */
    buf = (fastf_t *)bu_malloc((metaball_count*5+1)*SIZEOF_NETWORK_DOUBLE, "rt_metaball_export5: buf");
    buf[0] = mb->threshold;
    for (BU_LIST_FOR(mbpt, wdb_metaballpt, &mb->metaball_ctrl_head), i+=5) {
	VSCALE(&buf[i], mbpt->coord, local2mm);
	buf[i+3] = mbpt->fldstr * local2mm;
	buf[i+4] = mbpt->sweat;
    }
    htond((unsigned char *)ep->ext_buf + 2*SIZEOF_NETWORK_LONG, (unsigned char *)buf, 1 + 5 * metaball_count);
    bu_free((genptr_t)buf, "rt_metaball_export5: buf");
    return 0;
}


/**
 * R T _ M E T A B A L L _ D E S C R I B E
 *
 * Make human-readable formatted presentation of this solid. First
 * line describes type of solid. Additional lines are indented one
 * tab, and give parameter values.
 */
int
rt_metaball_describe(struct bu_vls *str, const struct rt_db_internal *ip, int verbose, double UNUSED(mm2local))
{
    int metaball_count = 0;
    char buf[BUFSIZ];
    struct rt_metaball_internal *mb;
    struct wdb_metaballpt *mbpt;

    RT_CK_DB_INTERNAL(ip);
    mb = (struct rt_metaball_internal *)ip->idb_ptr;
    RT_METABALL_CK_MAGIC(mb);
    for (BU_LIST_FOR(mbpt, wdb_metaballpt, &mb->metaball_ctrl_head)) metaball_count++;

    snprintf(buf, BUFSIZ, "Metaball with %d points and a threshold of %g (%s rendering)\n", metaball_count, mb->threshold, rt_metaball_lookup_type_name(mb->method));
    bu_vls_strcat(str, buf);

    if (!verbose)
	return 0;

    metaball_count=0;
    for (BU_LIST_FOR(mbpt, wdb_metaballpt, &mb->metaball_ctrl_head)) {
	switch(mb->method) {
	    case METABALL_ISOPOTENTIAL:
		snprintf(buf, BUFSIZ, "\t%d: %g field strength at (%g, %g, %g)\n",
			 ++metaball_count, mbpt->fldstr, V3ARGS(mbpt->coord));
		break;
	    case METABALL_METABALL:
	    case METABALL_BLOB:
		snprintf(buf, BUFSIZ, "\t%d: %g field strength at (%g, %g, %g) and blobbiness factor of %g\n",
			 ++metaball_count, mbpt->fldstr, V3ARGS(mbpt->coord), mbpt->sweat);
		break;
	    default:
		bu_bomb("Bad metaball method");	/* asplode */
	}
	bu_vls_strcat(str, buf);
    }
    return 0;
}


/**
 * R T _ M E T A B A L L _ I F R E E
 *
 * Free the storage associated with the rt_db_internal version of this
 * solid.  This only effects the in-memory copy.
 */
void
rt_metaball_ifree(struct rt_db_internal *ip)
{
    register struct rt_metaball_internal *metaball;
    register struct wdb_metaballpt *mbpt;

    RT_CK_DB_INTERNAL(ip);

    metaball = (struct rt_metaball_internal*)ip->idb_ptr;
    RT_METABALL_CK_MAGIC(metaball);

    if (metaball->metaball_ctrl_head.magic != 0)
	while (BU_LIST_WHILE(mbpt, wdb_metaballpt, &metaball->metaball_ctrl_head)) {
	    BU_LIST_DEQUEUE(&(mbpt->l));
	    bu_free((char *)mbpt, "wdb_metaballpt");
	}
    bu_free(ip->idb_ptr, "metaball ifree");
    ip->idb_ptr = GENPTR_NULL;
}


/**
 * R T _ M E T A B A L L _ A D D _ P O I N T
 *
 * Add a single point to an existing metaball.
 */
int
rt_metaball_add_point (struct rt_metaball_internal *mb, const point_t *loc, const fastf_t fldstr, const fastf_t goo)
{
    struct wdb_metaballpt *mbpt;

    BU_GETSTRUCT(mbpt, wdb_metaballpt);
    mbpt->l.magic = WDB_METABALLPT_MAGIC;
    VMOVE(mbpt->coord, *loc);
    mbpt->fldstr = fldstr;
    mbpt->sweat = goo;
    BU_LIST_INSERT(&mb->metaball_ctrl_head, &mbpt->l);

    return 0;
}


/**
 * R T _ M E T A B A L L _ P A R A M S
 *
 */
int
rt_metaball_params(struct pc_pc_set *UNUSED(ps), const struct rt_db_internal *ip)
{
    if (ip) RT_CK_DB_INTERNAL(ip);

    return 0;			/* OK */
}

/**
 * R T _ M E T A B A L L _ G E T
 *
 * db get/g2asc
 */
int
rt_metaball_get(struct bu_vls *logstr, const struct rt_db_internal *intern, const char *UNUSED(attr))
{
    struct rt_metaball_internal *mb = (struct rt_metaball_internal *)intern->idb_ptr;
    struct wdb_metaballpt *mbpt = NULL;

    RT_METABALL_CK_MAGIC(mb);

    /* write crap in */
    bu_vls_printf(logstr, "metaball %d %.25G {", mb->method, mb->threshold);
    for (BU_LIST_FOR(mbpt, wdb_metaballpt, &mb->metaball_ctrl_head))
	    bu_vls_printf(logstr, "{%.25G %.25G %.25G %.25G %.25G}",
			    V3ARGS(mbpt->coord), mbpt->fldstr, mbpt->sweat);
    bu_vls_printf(logstr, "}");

    return 0;
}

/**
 * R T _ M E T A B A L L _ A D J U S T
 *
 * used for db put/asc2g
 */
int
rt_metaball_adjust(struct bu_vls *logstr, struct rt_db_internal *intern, int argc, const char **argv)
{
    struct rt_metaball_internal *mb;
    const char *pts, *pend;;

    if(argc != 3)  {
	bu_vls_printf(logstr, "Invalid number of arguments: %d\n", argc);
	return BRLCAD_ERROR;
    }

    RT_CK_DB_INTERNAL(intern);
    mb = (struct rt_metaball_internal *)intern->idb_ptr;
    RT_METABALL_CK_MAGIC(mb);

    if( strlen(*argv) != 1 || (**argv < '0' || **argv > '2') ) {
	bu_vls_printf(logstr, "Invalid method type, must be one of 0, 1, or 2.");
	return BRLCAD_ERROR;
    }
    mb->method = *argv[0] - '0';
    sscanf(argv[1], "%lG", &mb->threshold);
    BU_LIST_INIT(&mb->metaball_ctrl_head);

    pts = argv[2];
    pend = pts + strlen(pts);

    while(1) {
	point_t loc;
	const point_t *locp;
	fastf_t fldstr, goo;
	int len;

	locp = (const point_t *)&loc;

	while( pts < pend && *pts != '{' ) ++pts;
	if(pts >= pend) break;
	len = sscanf(pts, "{%lG %lG %lG %lG %lG}", loc+X, loc+Y, loc+Z, &fldstr, &goo);
	if(len == EOF) break;
	if(len != 5) {
	    bu_vls_printf(logstr, "Failed to parse point information: \"%s\"", pts);
	    return BRLCAD_ERROR;
	}
	pts++;
	if(rt_metaball_add_point (mb, locp, fldstr, goo)) {
	    bu_vls_printf(logstr, "Failure adding point: {%f %f %f %f %f}", V3ARGS(loc), fldstr, goo);
	    return BRLCAD_ERROR;
	}
    }

    return BRLCAD_OK;
}


/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
