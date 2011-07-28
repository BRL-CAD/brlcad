/*                           R E V O L V E . C
 * BRL-CAD
 *
 * Copyright (c) 1990-2011 United States Government as represented by
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
/** @file primitives/revolve/revolve.c
 *
 * Intersect a ray with an 'revolve' primitive object.
 *
 */
/** @} */

#include "common.h"

#include <stdio.h>
#include <string.h>
#include <math.h>

#include "vmath.h"
#include "db.h"
#include "nmg.h"
#include "rtgeom.h"
#include "raytrace.h"

/* local interface header */
#include "./revolve.h"


#define START_FACE_POS	-1
#define START_FACE_NEG	-2
#define END_FACE_POS	-3
#define END_FACE_NEG	-4
#define HORIZ_SURF	-5

#define MAX_HITS	64


/* some sketch routines called here */
extern int rt_sketch_contains(struct rt_sketch_internal *, point2d_t);
extern void rt_sketch_bounds(struct rt_sketch_internal *, fastf_t *);


/**
 * R T _ R E V O L V E _ P R E P
 *
 * Given a pointer to a GED database record, and a transformation
 * matrix, determine if this is a valid REVOLVE, and if so, precompute
 * various terms of the formula.
 *
 * Returns -
 * 0 REVOLVE is OK
 * !0 Error in description
 *
 * Implicit return -
 * A struct revolve_specific is created, and its address is stored
 * in stp->st_specific for use by revolve_shot().
 */
int
rt_revolve_prep(struct soltab *stp, struct rt_db_internal *ip, struct rt_i *rtip)
{
    struct rt_revolve_internal *rip;
    struct revolve_specific *rev;

    vect_t xEnd, yEnd;

    int *endcount;
    size_t nseg, i, j, k;

    if (rtip) RT_CK_RTI(rtip);

    RT_CK_DB_INTERNAL(ip);
    rip = (struct rt_revolve_internal *)ip->idb_ptr;
    RT_REVOLVE_CK_MAGIC(rip);

    stp->st_id = ID_REVOLVE;
    stp->st_meth = &rt_functab[ID_REVOLVE];

    BU_GETSTRUCT(rev, revolve_specific);
    stp->st_specific = (genptr_t)rev;

    VMOVE(rev->v3d, rip->v3d);
    VMOVE(rev->zUnit, rip->axis3d);
    VMOVE(rev->xUnit, rip->r);
    VCROSS(rev->yUnit, rev->zUnit, rev->xUnit);

    VUNITIZE(rev->xUnit);
    VUNITIZE(rev->yUnit);
    VUNITIZE(rev->zUnit);

    rev->ang = rip->ang;
    rev->sketch_name = bu_vls_addr(&rip->sketch_name);
    rev->sk = rip->sk;

    /* calculate end plane */
    VSCALE(xEnd, rev->xUnit, cos(rev->ang));
    VSCALE(yEnd, rev->yUnit, sin(rev->ang));
    VADD2(rev->rEnd, xEnd, yEnd);
    VUNITIZE(rev->rEnd);

    /* check the sketch - degree & closed/open */

    /* count the number of times an endpoint is used:
     * if even, the point is ok
     * if odd, the point is at the end of a path
     */
    endcount = (int *)bu_calloc(rev->sk->vert_count, sizeof(int), "endcount");
    for (i=0; i<rev->sk->vert_count; i++)
	endcount[i] = 0;
    nseg = rev->sk->skt_curve.seg_count;

    for (i=0; i<nseg; i++) {
	long *lng;
	struct line_seg *lsg;
	struct carc_seg *csg;
	struct nurb_seg *nsg;
	struct bezier_seg *bsg;

	lng = (long *)rev->sk->skt_curve.segments[i];

	switch (*lng) {
	    case CURVE_LSEG_MAGIC:
		lsg = (struct line_seg *)lng;
		endcount[lsg->start]++;
		endcount[lsg->end]++;
		break;
	    case CURVE_CARC_MAGIC:
		csg = (struct carc_seg *)lng;
		if (csg->radius <= 0.0) break;
		endcount[csg->start]++;
		endcount[csg->end]++;
		break;
	    case CURVE_BEZIER_MAGIC:
		bsg = (struct bezier_seg *)lng;
		endcount[bsg->ctl_points[0]]++;
		endcount[bsg->ctl_points[bsg->degree]]++;
		break;
	    case CURVE_NURB_MAGIC:
		nsg = (struct nurb_seg *)lng;
		endcount[nsg->ctl_points[0]]++;
		endcount[nsg->ctl_points[nsg->c_size-1]]++;
		break;
	    default:
		bu_log("rt_revolve_prep: ERROR: unrecognized segment type!\n");
		break;
	}
    }

    /* convert endcounts to store which endpoints are odd */
    for (i=0, j=0; i<rip->sk->vert_count; i++) {
	if (endcount[i] % 2 != 0) {
	    /* add 'i' to list, insertion sort by vert[i][Y] */
	    for (k=j; k>0; k--) {
		if ((ZERO(rip->sk->verts[i][Y] - rip->sk->verts[endcount[k-1]][Y])
		     && rip->sk->verts[i][X] > rip->sk->verts[endcount[k-1]][X])
		    || (!ZERO(rip->sk->verts[i][Y] - rip->sk->verts[endcount[k-1]][Y])
			&& rip->sk->verts[i][Y] < rip->sk->verts[endcount[k-1]][Y])) {
		    endcount[k] = endcount[k-1];
		} else {
		    break;
		}
	    }
	    endcount[k] = i;
	    j++;
	}
    }
    while (j < rev->sk->vert_count) endcount[j++] = -1;

    rev->ends = endcount;

    /* bounding volume */
    rt_sketch_bounds(rev->sk, rev->bounds);
    if (endcount[0] != -1 && rev->bounds[0] > 0) rev->bounds[0] = 0;
    VJOIN1(stp->st_center, rev->v3d, 0.5*(rev->bounds[2]+rev->bounds[3]), rev->zUnit);
    stp->st_aradius = sqrt(0.25*(rev->bounds[3]-rev->bounds[2])*(rev->bounds[3]-rev->bounds[2]) + FMAX(rev->bounds[0]*rev->bounds[0], rev->bounds[1]*rev->bounds[1]));
    stp->st_bradius = stp->st_aradius;

    /* cheat, make bounding RPP by enclosing bounding sphere (copied from g_ehy.c) */
    stp->st_min[X] = stp->st_center[X] - stp->st_bradius;
    stp->st_max[X] = stp->st_center[X] + stp->st_bradius;
    stp->st_min[Y] = stp->st_center[Y] - stp->st_bradius;
    stp->st_max[Y] = stp->st_center[Y] + stp->st_bradius;
    stp->st_min[Z] = stp->st_center[Z] - stp->st_bradius;
    stp->st_max[Z] = stp->st_center[Z] + stp->st_bradius;

    return 0;			/* OK */
}


/**
 * R T _ R E V O L V E _ P R I N T
 */
void
rt_revolve_print(const struct soltab *stp)
{
    const struct revolve_specific *rev =
	(struct revolve_specific *)stp->st_specific;

    VPRINT("V", rev->v3d);
    VPRINT("Axis", rev->zUnit);
    VPRINT("Start", rev->xUnit);
    VPRINT("End", rev->rEnd);
    fprintf(stderr, "angle = %g\n", rev->ang);
    fprintf(stderr, "sketch = %s\n", rev->sketch_name);
}


/**
 * R T _ R E V O L V E _ S H O T
 *
 * Intersect a ray with a revolve.  If an intersection occurs, a struct
 * seg will be acquired and filled in.
 *
 * Returns -
 * 0 MISS
 * >0 HIT
 */
int
rt_revolve_shot(struct soltab *stp, struct xray *rp, struct application *ap, struct seg *seghead)
{
    struct revolve_specific *rev =
	(struct revolve_specific *)stp->st_specific;
    struct seg *segp;

    struct hit *hitp;
    struct hit *hits[MAX_HITS], hit[MAX_HITS];

    size_t i, j, nseg, nhits;
    int in, out;

    fastf_t k, m, h, aa, bb;
    point_t dp, pr, xlated;
    vect_t vr, ur, norm, normS, normE;

    fastf_t start, end, angle;

    vect_t dir;
    point_t hit1, hit2;
    point2d_t hit2d, pt1, pt2;
    fastf_t a, b, c, disc, k1, k2, t1, t2;
    long *lng;
    struct line_seg *lsg;
    struct carc_seg *csg;

    nhits = 0;

    for (i=0; i<MAX_HITS; i++) hits[i] = &hit[i];

    vr[X] = VDOT(rev->xUnit, rp->r_dir);
    vr[Y] = VDOT(rev->yUnit, rp->r_dir);
    vr[Z] = VDOT(rev->zUnit, rp->r_dir);

    VSUB2(xlated, rp->r_pt, rev->v3d);
    pr[X] = VDOT(rev->xUnit, xlated);
    pr[Y] = VDOT(rev->yUnit, xlated);
    pr[Z] = VDOT(rev->zUnit, xlated);

    VMOVE(ur, vr);
    VUNITIZE(ur);

    if (rev->ang < 2*M_PI) {
	VREVERSE(normS, rev->yUnit);	/* start normal */
	start = (VDOT(normS, rev->v3d) - VDOT(normS, rp->r_pt)) / VDOT(normS, rp->r_dir);

	VCROSS(normE, rev->zUnit, rev->rEnd);	/* end normal */
	end = (VDOT(normE, rev->v3d) - VDOT(normE, rp->r_pt)) / VDOT(normE, rp->r_dir);

	VJOIN1(hit1, pr, start, vr);
	hit2d[Y] = hit1[Z];
	hit2d[X] = sqrt(hit1[X]*hit1[X] + hit1[Y]*hit1[Y]);

	VJOIN1(hit2, xlated, start, rp->r_dir);
	if (VDOT(rev->xUnit, hit2) < 0) {
	    /* set the sign of the 2D point's x coord */
	    hit2d[X] = -hit2d[X];
	}

	if (rt_sketch_contains(rev->sk, hit2d)) {
	    hit2d[X] = -hit2d[X];
	    if (rev->ang > M_PI && rt_sketch_contains(rev->sk, hit2d)) {
		/* skip it */
	    } else {
		if (nhits >= MAX_HITS) return -1; /* too many hits */
		hitp = hits[nhits++];
		hitp->hit_magic = RT_HIT_MAGIC;
		hitp->hit_dist = start;
		hitp->hit_surfno = (hit2d[X]>0) ? START_FACE_NEG : START_FACE_POS;
		VSET(hitp->hit_vpriv, -hit2d[X], hit2d[Y], 0);
	    }
	}

	VJOIN1(hit1, pr, end, vr);
	hit2d[Y] = hit1[Z];
	hit2d[X] = sqrt(hit1[X]*hit1[X] + hit1[Y]*hit1[Y]);

	VJOIN1(hit2, xlated, end, rp->r_dir);
	if (VDOT(rev->rEnd, hit2) < 0) {
	    /* set the sign of the 2D point's x coord */
	    hit2d[X] = -hit2d[X];
	}

	if (rt_sketch_contains(rev->sk, hit2d)) {
	    hit2d[X] = -hit2d[X];
	    if (rev->ang > M_PI && rt_sketch_contains(rev->sk, hit2d)) {
		/* skip it */
	    } else {
		if (nhits >= MAX_HITS) return -1; /* too many hits */
		hitp = hits[nhits++];
		hitp->hit_magic = RT_HIT_MAGIC;
		hitp->hit_dist = end;
		hitp->hit_surfno = (hit2d[X]>0) ? END_FACE_NEG : END_FACE_POS;
		VSET(hitp->hit_vpriv, -hit2d[X], hit2d[Y], 0);
	    }
	}
    }

    /**
     * calculate hyperbola parameters
     *
     * [ (x*x) / aa^2 ] - [ (y-h)^2 / bb^2 ] = 1
     *
     * x = aa cosh(t - k);
     * y = h + bb sinh(t - k);
     */

    VREVERSE(dp, pr);
    VSET(norm, ur[X], ur[Y], 0);

    k = VDOT(dp, norm) / VDOT(ur, norm);
    h = pr[Z] + k*vr[Z];

    if (NEAR_EQUAL(fabs(ur[Z]), 1.0, RT_DOT_TOL)) {
	aa = sqrt(pr[X]*pr[X] + pr[Y]*pr[Y]);
	bb = MAX_FASTF;
    } else {
	aa = sqrt((pr[X] + k*vr[X])*(pr[X] + k*vr[X]) + (pr[Y] + k*vr[Y])*(pr[Y] + k*vr[Y]));
	bb = sqrt(aa*aa * (1.0/(1 - ur[Z]*ur[Z]) - 1.0));
    }

    /**
     * if (ur[Z] == 1) {
     *	    bb = inf;
     *	    // ray becomes a line parallel to sketch's y-axis instead of a hyberbola
     * }
     * if (ur[Z] == 0) {
     *	    bb = 0;
     *	    // ray becomes a line parallel to sketch's x-axis instead of a hyperbola
     *	    // all hits must have x > aa
     * }
     */

    /* handle open sketches */
    if (!NEAR_ZERO(ur[Z], RT_DOT_TOL)) {
	for (i=0; i<rev->sk->vert_count && rev->ends[i] != -1; i++) {
	    V2MOVE(pt1, rev->sk->verts[rev->ends[i]]);
	    hit2d[Y] = pt1[Y];
	    if (NEAR_EQUAL(fabs(ur[Z]), 1.0, RT_DOT_TOL)) {
		/* ur[Z] == 1 */
		hit2d[X] = aa;
	    } else {
		hit2d[X] = aa*sqrt((hit2d[Y]-h)*(hit2d[Y]-h)/(bb*bb) + 1);
	    }
	    if (pt1[X] < 0) hit2d[X] = -fabs(hit2d[X]);
	    if (fabs(hit2d[X]) < fabs(pt1[X])) {
		/* valid hit */
		if (nhits >= MAX_HITS) return -1; /* too many hits */
		hitp = hits[nhits++];
		hitp->hit_magic = RT_HIT_MAGIC;
		hitp->hit_dist = (hit2d[Y] - pr[Z]) / vr[Z];
		hitp->hit_surfno = HORIZ_SURF;
		VJOIN1(hitp->hit_vpriv, pr, hitp->hit_dist, vr);
		hitp->hit_point[X] = hit2d[X];
		hitp->hit_point[Y] = hit2d[Y];
		hitp->hit_point[Z] = 0;

		angle = atan2(hitp->hit_vpriv[Y], hitp->hit_vpriv[X]);
		if (pt1[X] < 0) {
		    angle += M_PI;
		} else if (angle < 0) {
		    angle += 2*M_PI;
		}
		hit2d[X] = -hit2d[X];
		if (angle > rev->ang) {
		    nhits--;
		    continue;
		} else if ((angle + M_PI < rev->ang || angle - M_PI > 0)
			   && rt_sketch_contains(rev->sk, hit2d)
			   && hit2d[X] > 0) {
		    nhits--;
		    continue;
		}
		/* X and Y are used for uv(), Z is used for norm() */
		hitp->hit_vpriv[X] = pt1[X];
		hitp->hit_vpriv[Y] = angle;
		if (i+1 < rev->sk->vert_count && rev->ends[i+1] != -1 &&
		    NEAR_EQUAL(rev->sk->verts[rev->ends[i]][Y],
			       rev->sk->verts[rev->ends[i+1]][Y], SMALL)) {
		    hitp->hit_vpriv[Z] = rev->sk->verts[rev->ends[i+1]][X];
		    i++;
		    if (fabs(hit2d[X]) < fabs(hitp->hit_vpriv[Z])) {
			nhits--;
		    }
		} else {
		    hitp->hit_vpriv[Z] = 0;
		}
	    }
	}
    }

    /* find hyperbola intersection with each sketch segment */
    nseg = rev->sk->skt_curve.seg_count;
    for (i=0; i<nseg; i++) {
	lng = (long *)rev->sk->skt_curve.segments[i];

	switch (*lng) {
	    case CURVE_LSEG_MAGIC:
		lsg = (struct line_seg *)lng;
		V2MOVE(pt1, rev->sk->verts[lsg->start]);
		V2MOVE(pt2, rev->sk->verts[lsg->end]);
		V2SUB2(dir, pt2, pt1);
		if (ZERO(dir[X])) {
		    m = 1.0;
		} else {
		    m = dir[Y] / dir[X];
		}

		if (NEAR_EQUAL(fabs(ur[Z]), 1.0, RT_DOT_TOL)) {
		    /* ray is vertical line at x=aa */
		    if (FMIN(pt1[X], pt2[X]) < aa && aa < FMAX(pt1[X], pt2[X])) {
			/* check the positive side of the sketch (x > 0) */
			k1 = (m * (aa - pt1[X]) + pt1[Y] - pr[Z]) / vr[Z];
			VJOIN1(hit1, pr, k1, vr);
			angle = atan2(hit1[Y], hit1[X]);
			hit2d[X] = -aa;		/* use neg to check for overlap in contains() */
			hit2d[Y] = hit1[Z];
			if (angle < 0) {
			    angle += 2*M_PI;
			}
			if (angle < rev->ang &&
			    !((angle + M_PI < rev->ang || angle - M_PI > 0)
			      && rt_sketch_contains(rev->sk, hit2d))) {
			    if (nhits >= MAX_HITS) return -1; /* too many hits */
			    hitp = hits[nhits++];
			    hitp->hit_point[X] = -hit2d[X];
			    hitp->hit_point[Y] = hit2d[Y];
			    hitp->hit_point[Z] = 0;
			    VMOVE(hitp->hit_vpriv, hit1);
			    if (ZERO(m)) {
				hitp->hit_vpriv[Z] = 0.0;
			    } else {
				hitp->hit_vpriv[Z] = -1.0/m;
			    }
			    hitp->hit_magic = RT_HIT_MAGIC;
			    hitp->hit_dist = k1;
			    hitp->hit_surfno = i;
			}
		    }
		    if (FMIN(pt1[X], pt2[X]) < -aa && -aa < FMAX(pt1[X], pt2[X])) {
			/* check negative side of the sketch (x < 0) */
			k1 = (m * (-aa - pt1[X]) + pt1[Y] - pr[Z]) / vr[Z];
			VJOIN1(hit1, pr, k1, vr);
			angle = atan2(hit1[Y], hit1[X]);
			hit2d[X] = aa;		/* use neg to check for overlap in contains() */
			hit2d[Y] = hit1[Z];
			if (angle < 0) {
			    angle += M_PI;
			}
			if (angle < rev->ang &&
			    !((angle + M_PI < rev->ang || angle - M_PI > 0)
			      && rt_sketch_contains(rev->sk, hit2d))) {
			    if (nhits >= MAX_HITS) return -1; /* too many hits */
			    hitp = hits[nhits++];
			    hitp->hit_point[X] = -hit2d[X];
			    hitp->hit_point[Y] = hit2d[Y];
			    hitp->hit_point[Z] = 0;
			    VMOVE(hitp->hit_vpriv, hit1);
			    if (ZERO(m)) {
				hitp->hit_vpriv[Z] = 0.0;
			    } else {
				hitp->hit_vpriv[Z] = 1.0/m;
			    }
			    hitp->hit_magic = RT_HIT_MAGIC;
			    hitp->hit_dist = k1;
			    hitp->hit_surfno = i;
			}
		    }
		} else if (NEAR_ZERO(ur[Z], RT_DOT_TOL)) {
		    /* ray is horizontal line at y = h; hit2d[X] > aa */
		    if (FMIN(pt1[Y], pt2[Y]) < h && h < FMAX(pt1[Y], pt2[Y])) {
			if (ZERO(m)) {
			    hit2d[X] = pt1[X];
			} else {
			    hit2d[X] = pt1[X] + (h-pt1[Y])/m;
			}
			hit2d[Y] = h;
			if (fabs(hit2d[X]) > aa) {
			    k1 = k + sqrt(hit2d[X]*hit2d[X] - aa*aa);
			    k2 = k - sqrt(hit2d[X]*hit2d[X] - aa*aa);

			    VJOIN1(hit1, pr, k1, vr);
			    angle = atan2(hit1[Y], hit1[X]);
			    if (hit2d[X] < 0) {
				angle += M_PI;
			    } else if (angle < 0) {
				angle += 2*M_PI;
			    }
			    hit2d[X] = -hit2d[X];
			    if (angle < rev->ang &&
				!((angle + M_PI < rev->ang || angle - M_PI > 0)
				  && rt_sketch_contains(rev->sk, hit2d))) {
				if (nhits >= MAX_HITS) return -1; /* too many hits */
				hitp = hits[nhits++];
				hitp->hit_point[X] = -hit2d[X];
				hitp->hit_point[Y] = hit2d[Y];
				hitp->hit_point[Z] = 0;
				VMOVE(hitp->hit_vpriv, hit1);
				if (ZERO(m)) {
				    hitp->hit_vpriv[Z] = 0.0;
				} else {
				    hitp->hit_vpriv[Z] = (hit2d[X]>0) ? 1.0/m : -1.0/m;
				}
				hitp->hit_magic = RT_HIT_MAGIC;
				hitp->hit_dist = k1;
				hitp->hit_surfno = i;
			    }

			    VJOIN1(hit2, pr, k2, vr);
			    angle = atan2(hit2[Y], hit2[X]);
			    if (-hit2d[X] < 0) {
				angle += M_PI;
			    } else if (angle < 0) {
				angle += 2*M_PI;
			    }
			    if (angle < rev->ang &&
				!((angle + M_PI < rev->ang || angle - M_PI > 0)
				  && rt_sketch_contains(rev->sk, hit2d))) {
				if (nhits >= MAX_HITS) return -1; /* too many hits */
				hitp = hits[nhits++];
				hitp->hit_point[X] = -hit2d[X];
				hitp->hit_point[Y] = hit2d[Y];
				hitp->hit_point[Z] = 0;
				VMOVE(hitp->hit_vpriv, hit2);
				if (ZERO(m)) {
				    hitp->hit_vpriv[Z] = 0.0;
				} else {
				    hitp->hit_vpriv[Z] = (hit2d[X]>0) ? 1.0/m : -1.0/m;
				}
				hitp->hit_magic = RT_HIT_MAGIC;
				hitp->hit_dist = k2;
				hitp->hit_surfno = i;
			    }
			}
		    }
		} else {

		    a = dir[X]*dir[X]/(aa*aa) - dir[Y]*dir[Y]/(bb*bb);
		    b = 2*(dir[X]*pt1[X]/(aa*aa) - dir[Y]*(pt1[Y]-h)/(bb*bb));
		    c = pt1[X]*pt1[X]/(aa*aa) - (pt1[Y]-h)*(pt1[Y]-h)/(bb*bb) - 1;
		    disc = b*b - (4.0 * a * c);
		    if (!NEAR_ZERO(a, RT_PCOEF_TOL)) {
			if (disc > 0) {
			    disc = sqrt(disc);
			    t1 =  (-b + disc) / (2.0 * a);
			    t2 =  (-b - disc) / (2.0 * a);
			    k1 = (pt1[Y]-pr[Z] + t1*dir[Y])/vr[Z];
			    k2 = (pt1[Y]-pr[Z] + t2*dir[Y])/vr[Z];

			    if (t1 > 0 && t1 < 1) {
				if (nhits >= MAX_HITS) return -1; /* too many hits */
				VJOIN1(hit1, pr, k1, vr);
				angle = atan2(hit1[Y], hit1[X]);
				V2JOIN1(hit2d, pt1, t1, dir);
				if (hit2d[X] < 0) {
				    angle += M_PI;
				} else if (angle < 0) {
				    angle += 2*M_PI;
				}
				hit2d[X] = -hit2d[X];
				if (angle < rev->ang) {
				    if ((angle + M_PI < rev->ang || angle - M_PI > 0)
					&& rt_sketch_contains(rev->sk, hit2d)) {
					/* overlap, so ignore it */
				    } else {
					hitp = hits[nhits++];
					hitp->hit_point[X] = -hit2d[X];
					hitp->hit_point[Y] = hit2d[Y];
					hitp->hit_point[Z] = 0;
					VMOVE(hitp->hit_vpriv, hit1);
					if (ZERO(m)) {
					    hitp->hit_vpriv[Z] = 0.0;
					} else {
					    hitp->hit_vpriv[Z] = (hit2d[X]>0) ? 1.0/m : -1.0/m;
					}
					hitp->hit_magic = RT_HIT_MAGIC;
					hitp->hit_dist = k1;
					hitp->hit_surfno = i;
				    }
				}
			    }
			    if (t2 > 0 && t2 < 1) {
				if (nhits >= MAX_HITS) return -1; /* too many hits */
				VJOIN1(hit2, pr, k2, vr);
				angle = atan2(hit2[Y], hit2[X]);
				V2JOIN1(hit2d, pt1, t2, dir);
				if (hit2d[X] < 0) {
				    angle += M_PI;
				} else if (angle < 0) {
				    angle += 2*M_PI;
				}
				hit2d[X] = -hit2d[X];
				if (angle < rev->ang) {
				    if ((angle + M_PI < rev->ang || angle - M_PI > 0)
					&& rt_sketch_contains(rev->sk, hit2d)) {
					/* overlap, so ignore it */
				    } else {
					hitp = hits[nhits++];
					hitp->hit_point[X] = -hit2d[X];
					hitp->hit_point[Y] = hit2d[Y];
					hitp->hit_point[Z] = 0;
					VMOVE(hitp->hit_vpriv, hit2);
					if (ZERO(m)) {
					    hitp->hit_vpriv[Z] = 0.0;
					} else {
					    hitp->hit_vpriv[Z] = (hit2d[X]>0) ? 1.0/m : -1.0/m;
					}
					hitp->hit_magic = RT_HIT_MAGIC;
					hitp->hit_dist = k2;
					hitp->hit_surfno = i;
				    }
				}
			    }
			}
		    } else if (!NEAR_ZERO(b, RT_PCOEF_TOL)) {
			t1 = -c / b;
			k1 = (pt1[Y]-pr[Z] + t1*dir[Y])/vr[Z];
			if (t1 > 0 && t1 < 1) {
			    if (nhits >= MAX_HITS) return -1; /* too many hits */

			    VJOIN1(hit1, pr, k1, vr);
			    angle = atan2(hit1[Y], hit1[X]);
			    V2JOIN1(hit2d, pt1, t1, dir);
			    if (hit2d[X] < 0) {
				angle += M_PI;
			    } else if (angle < 0) {
				angle += 2*M_PI;
			    }
			    hit2d[X] = -hit2d[X];
			    if (angle < rev->ang) {
				if ((angle + M_PI < rev->ang || angle - M_PI > 0)
				    && rt_sketch_contains(rev->sk, hit2d)) {
				    /* overlap, so ignore it */
				} else {
				    hitp = hits[nhits++];
				    hitp->hit_point[X] = -hit2d[X];
				    hitp->hit_point[Y] = hit2d[Y];
				    hitp->hit_point[Z] = 0;
				    VMOVE(hitp->hit_vpriv, hit1);
				    if (ZERO(m)) {
					hitp->hit_vpriv[Z] = 0.0;
				    } else {
					hitp->hit_vpriv[Z] = (hit2d[X]>0) ? 1.0/m : -1.0/m;;
				    }
				    hitp->hit_magic = RT_HIT_MAGIC;
				    hitp->hit_dist = k1;
				    hitp->hit_surfno = i;
				}
			    }
			}
		    }
		}
		break;
	    case CURVE_CARC_MAGIC:
		/*
		  circle: (x-cx)^2 + (y-cy)^2 = cr^2
		  x = (1/2cx)y^2 + (-cy/cx)y + (1/2cx)(cy^2 + cx^2 - cr^2) + (1/2cx)(x^2)
		  x = f(y) + (1/2cx)x^2

		  hyperbola:
		  [(x-hx)/a]^2 - [(y-hy)/b]^2 = 1
		  x^2 = (a^2/b^2)y^2 + (-2*hy*a^2/b^2)y + (hy^2 * a^2/b^2) + a^2
		  x^2 = g(y)

		  plug the second equation into the first to get:
		  x = f(y) + (1/2cx)g(y)
		  then square that to get:
		  x^2 = {f(y) + (1/2cx)g(y)}^2 = g(y)
		  move all to one side to get:
		  0 = {f(y) + (1/2cx)g(y)}^2 - g(y)
		  this is a fourth order polynomial in y.
		*/
		{
		    bn_poly_t circleX;	/* f(y) */
		    bn_poly_t hypXsq;		/* g(y) */
		    bn_poly_t hypXsq_scaled;	/* g(y) / (2*cx) */
		    bn_poly_t sum;		/* f(y) + g(y)/(2cx) */
		    bn_poly_t sum_sq;		/* {f(y) + g(y)/(2cx)}^2 */
		    bn_poly_t answer;		/* {f(y) + g(y)/(2cx)}^2 - g(y) */
		    bn_complex_t roots[4];
		    int rootcnt;

		    fastf_t cx, cy, crsq = 0;	/* carc's (x, y) coords and radius^2 */
		    point2d_t center, radius;

		    /* calculate circle parameters */
		    csg = (struct carc_seg *)lng;

		    if (csg->radius <= 0.0) {
			/* full circle, "end" is center and "start" is on the circle */
			V2MOVE(center, rev->sk->verts[csg->end]);
			V2SUB2(radius, rev->sk->verts[csg->start], center);
			crsq = MAG2SQ(radius);
		    } else {
			point_t startpt, endpt, midpt;
			vect_t s_to_m;
			vect_t bisector;
			vect_t vertical;
			fastf_t distance;
			fastf_t magsq_s2m;

			VSET(vertical, 0, 0, 1);
			VMOVE(startpt, rev->sk->verts[csg->start]);
			startpt[Z] = 0.0;
			VMOVE(endpt, rev->sk->verts[csg->end]);
			endpt[Z] = 0.0;

			VBLEND2(midpt, 0.5, startpt, 0.5, endpt);
			VSUB2(s_to_m, midpt, startpt);
			VCROSS(bisector, vertical, s_to_m);
			VUNITIZE(bisector);
			magsq_s2m = MAGSQ(s_to_m);
			if (magsq_s2m > csg->radius*csg->radius) {
			    fastf_t max_radius;

			    max_radius = sqrt(magsq_s2m);
			    if (NEAR_EQUAL(max_radius, csg->radius, RT_LEN_TOL)) {
				csg->radius = max_radius;
			    } else {
				bu_log("Impossible radius for circular arc in extrusion (%s), is %g, cannot be more than %g!\n",
				       stp->st_dp->d_namep, csg->radius, sqrt(magsq_s2m));
				bu_log("Difference is %g\n", max_radius - csg->radius);
				return -1;
			    }
			}
			distance = sqrt(csg->radius*csg->radius - magsq_s2m);

			/* save arc center */
			if (csg->center_is_left) {
			    VJOIN1(center, midpt, distance, bisector);
			} else {
			    VJOIN1(center, midpt, -distance, bisector);
			}
		    }

		    cx = center[X];
		    cy = center[Y];

		    circleX.dgr = 2;
		    hypXsq.dgr = 2;
		    hypXsq_scaled.dgr = 2;
		    sum.dgr = 2;
		    sum_sq.dgr = 4;
		    answer.dgr = 4;

		    circleX.cf[0] = (cy*cy + cx*cx - crsq)/(2.0*cx);
		    circleX.cf[1] = -cy/cx;
		    circleX.cf[2] = 1/(2.0*cx);

		    hypXsq_scaled.cf[0] = hypXsq.cf[0] = aa*aa + h*h*aa*aa/(bb*bb);
		    hypXsq_scaled.cf[1] = hypXsq.cf[1] = -2.0*h*aa*aa/(bb*bb);
		    hypXsq_scaled.cf[2] = hypXsq.cf[2] = (aa*aa)/(bb*bb);

		    bn_poly_scale(&hypXsq_scaled, 1.0 / (2.0 * cx));
		    bn_poly_add(&sum, &hypXsq_scaled, &circleX);
		    bn_poly_mul(&sum_sq, &sum, &sum);
		    bn_poly_sub(&answer, &sum_sq, &hypXsq);

		    /* It is known that the equation is 4th order.  Therefore, if the
		     * root finder returns other than 4 roots, error.
		     */
		    rootcnt = rt_poly_roots(&answer, roots, stp->st_dp->d_namep);
		    if (rootcnt != 4) {
			if (rootcnt > 0) {
			    bu_log("tor:  rt_poly_roots() 4!=%d\n", rootcnt);
			    bn_pr_roots(stp->st_name, roots, rootcnt);
			} else if (rootcnt < 0) {
			    static int reported=0;
			    bu_log("The root solver failed to converge on a solution for %s\n", stp->st_dp->d_namep);
			    if (!reported) {
				VPRINT("while shooting from:\t", rp->r_pt);
				VPRINT("while shooting at:\t", rp->r_dir);
				bu_log("Additional torus convergence failure details will be suppressed.\n");
				reported=1;
			    }
			}
		    }

		    break;
		}
	    case CURVE_BEZIER_MAGIC:
		break;
	    case CURVE_NURB_MAGIC:
		break;
	    default:
		bu_log("rt_revolve_prep: ERROR: unrecognized segment type!\n");
		break;
	}

    }

    if (nhits%2 != 0) {
	bu_log("odd number of hits: %zu\n", nhits);
	for (i=0; i<nhits; i++) {
	    bu_log("\t(%6.2f, %6.2f)\t%6.2f\t%2d\n",
		   hits[i]->hit_point[X], hits[i]->hit_point[Y], hits[i]->hit_dist, hits[i]->hit_surfno);
	}
	return -1;
    }

    /* sort hitpoints (an arbitrary number of hits depending on sketch) */
    for (i=0; i<nhits; i+=2) {
	in = out = -1;
	for (j=0; j<nhits; j++) {
	    if (hits[j] == NULL) continue;
	    if (in == -1) {
		in = j;
		continue;
	    }
	    /* store shortest dist as 'in', second shortest as 'out' */
	    if (hits[j]->hit_dist <= hits[in]->hit_dist) {
		out = in;
		in = j;
	    } else if (out == -1 || hits[j]->hit_dist <= hits[out]->hit_dist) {
		out = j;
	    }
	}
	if (in == -1 || out == -1) {
	    bu_log("failed to find valid segment. nhits: %zu\n", nhits);
	    break;
	}

	if (ZERO(hits[in]->hit_dist - hits[out]->hit_dist)) {
	    hits[in] = NULL;
	    hits[out] = NULL;
	    continue;
	}

	RT_GET_SEG(segp, ap->a_resource);
	segp->seg_stp = stp;

	segp->seg_in = *hits[in];
	hits[in] = NULL;
	segp->seg_out = *hits[out];
	hits[out] = NULL;
	BU_LIST_INSERT(&(seghead->l), &(segp->l));
    }

    return nhits;
}


/**
 * R T _ R E V O L V E _ N O R M
 *
 * Given ONE ray distance, return the normal and entry/exit point.
 */
void
rt_revolve_norm(struct hit *hitp, struct soltab *stp, struct xray *rp)
{
    struct revolve_specific *rev =
	(struct revolve_specific *)stp->st_specific;
    vect_t n, nT;

    VJOIN1(hitp->hit_point, rp->r_pt, hitp->hit_dist, rp->r_dir);

/*
  XXX debug printing
  VPRINT("hitp", hitp->hit_point);
  VPRINT("vpriv", hitp->hit_vpriv);
*/

    switch (hitp->hit_surfno) {
	case START_FACE_POS:
	    VREVERSE(hitp->hit_normal, rev->yUnit);
	    break;
	case START_FACE_NEG:
	    VMOVE(hitp->hit_normal, rev->yUnit);
	    break;
	case END_FACE_POS:
	    VCROSS(hitp->hit_normal, rev->zUnit, rev->rEnd);
	    VUNITIZE(hitp->hit_normal);
	    break;
	case END_FACE_NEG:
	    VCROSS(hitp->hit_normal, rev->rEnd, rev->zUnit);
	    VUNITIZE(hitp->hit_normal);
	    break;
	default:
	    VMOVE(n, hitp->hit_vpriv);
	    n[Z] *= sqrt((hitp->hit_vpriv[X] * hitp->hit_vpriv[X]) + (hitp->hit_vpriv[Y] * hitp->hit_vpriv[Y]));

	    if (hitp->hit_surfno == HORIZ_SURF) {
		VSET(n, 0, 0, 1);
	    }

	    nT[X] = (rev->xUnit[X] * n[X])
		+ (rev->yUnit[X] * n[Y])
		+ (rev->zUnit[X] * n[Z]);
	    nT[Y] = (rev->xUnit[Y] * n[X])
		+ (rev->yUnit[Y] * n[Y])
		+ (rev->zUnit[Y] * n[Z]);
	    nT[Z] = (rev->xUnit[Z] * n[X])
		+ (rev->yUnit[Z] * n[Y])
		+ (rev->zUnit[Z] * n[Z]);

	    VUNITIZE(nT);

/* XXX Debug printing
   VPRINT("xUnit", rev->xUnit);
   VPRINT("yUnit", rev->yUnit);
   VPRINT("zUnit", rev->zUnit);
   VPRINT("n", n);
   VPRINT("nT", nT);
*/

	    if (VDOT(nT, rp->r_dir) < 0) {
		VMOVE(hitp->hit_normal, nT);
	    } else {
		VREVERSE(hitp->hit_normal, nT);
	    }
	    break;
    }
}


/**
 * R T _ R E V O L V E _ C U R V E
 *
 * Return the curvature of the revolve.
 */
void
rt_revolve_curve(struct curvature *cvp, struct hit *hitp, struct soltab *stp)
{
    if (!cvp || !hitp)
	return;
    RT_CK_HIT(hitp);
    if (stp) RT_CK_SOLTAB(stp);

    cvp->crv_c1 = cvp->crv_c2 = 0;

    /* any tangent direction */
    bn_vec_ortho(cvp->crv_pdir, hitp->hit_normal);
}


/**
 * R T _ R E V O L V E _ U V
 *
 * For a hit on the surface of an revolve, return the (u, v) coordinates
 * of the hit point, 0 <= u, v <= 1.

 * u = azimuth,  v = elevation
 */
void
rt_revolve_uv(struct application *ap, struct soltab *stp, struct hit *hitp, struct uvcoord *uvp)
{
    struct revolve_specific *rev = (struct revolve_specific *)stp->st_specific;

    point_t hitpoint;
    fastf_t angle;
    long *lng;
    struct line_seg *lsg;

    /*
    struct carc_seg *csg;
    struct nurb_seg *nsg;
    struct bezier_seg *bsg;
    */

    if (ap) RT_CK_APPLICATION(ap);

    VJOIN1(hitpoint, hitp->hit_rayp->r_pt, hitp->hit_dist, hitp->hit_rayp->r_dir);

    switch (hitp->hit_surfno) {
	case START_FACE_POS:
	case START_FACE_NEG:
	case END_FACE_POS:
	case END_FACE_NEG:
	    uvp->uv_u = fabs((hitp->hit_vpriv[X] - rev->bounds[0]) / (rev->bounds[1] - rev->bounds[0]));
	    uvp->uv_v = fabs((hitp->hit_vpriv[Y] - rev->bounds[2]) / (rev->bounds[3] - rev->bounds[2]));
	    break;
	case HORIZ_SURF:
	    hitpoint[Z] = 0;

	    /* Y is the angle: [0, 2pi] */
	    uvp->uv_u = hitp->hit_vpriv[Y] / rev->ang;

	    /* X is the coord of the endpoint that we're connecting to the revolve axis */
	    uvp->uv_v = fabs((MAGNITUDE(hitpoint) - hitp->hit_vpriv[Z])
			     / (hitp->hit_vpriv[X]- hitp->hit_vpriv[Z]));
	    /* bu_log("\tmin: %3.2f\tmax: %3.2f\n", hitp->hit_vpriv[Z], hitp->hit_vpriv[X]); */
	    break;
	default:
	    angle = atan2(hitp->hit_vpriv[Y], hitp->hit_vpriv[X]);
	    if (angle < 0) angle += 2*M_PI;
	    uvp->uv_u = angle / rev->ang;

	    lng = (long *)rev->sk->skt_curve.segments[hitp->hit_surfno];

	    switch (*lng) {
		case CURVE_LSEG_MAGIC:
		    lsg = (struct line_seg *)lng;
		    if (ZERO(1.0/hitp->hit_vpriv[Z])) {
			/* use hitpoint radius and sketch's X values */
			hitpoint[Z] = 0;
			uvp->uv_v = (MAGNITUDE(hitpoint) - rev->sk->verts[lsg->end][X]) /
			    (rev->sk->verts[lsg->start][X] - rev->sk->verts[lsg->end][X]);
		    } else {
			/* use hitpoint Z and sketch's Y values */
			uvp->uv_v = (hitpoint[Z] - rev->sk->verts[lsg->end][Y]) /
			    (rev->sk->verts[lsg->start][Y] - rev->sk->verts[lsg->end][Y]);
		    }
		    break;
		case CURVE_CARC_MAGIC:
		    /* csg = (struct carc_seg *)lng; */
		    break;
		case CURVE_BEZIER_MAGIC:
		    /* bsg = (struct bezier_seg *)lng; */
		    break;
		case CURVE_NURB_MAGIC:
		    /* nsg = (struct nurb_seg *)lng; */
		    break;
		default:
		    bu_log("rt_revolve_prep: ERROR: unrecognized segment type!\n");
		    break;
	    }

	    break;
    }
    if (uvp->uv_v > 1 || uvp->uv_v < 0 || uvp->uv_u > 1 || uvp->uv_u < 0) {
	bu_log("UV error:\t%d\t%2.2f\t%2.2f\t", hitp->hit_surfno, uvp->uv_u, uvp->uv_v);
	bu_log("\tX: (%3.2f, %3.2f)\tY: (%3.2f, %3.2f)\n", rev->bounds[0], rev->bounds[1], rev->bounds[2], rev->bounds[3]);
    }
    /* sanity checks */
    if (uvp->uv_u < 0.0)
	uvp->uv_u = 0.0;
    else if (uvp->uv_u > 1.0)
	uvp->uv_u = 1.0;
    if (uvp->uv_v < 0.0)
	uvp->uv_v = 0.0;
    else if (uvp->uv_v > 1.0)
	uvp->uv_v = 1.0;
}


/**
 * R T _ R E V O L V E _ F R E E
 */
void
rt_revolve_free(struct soltab *stp)
{
    struct revolve_specific *revolve =
	(struct revolve_specific *)stp->st_specific;
    bu_free(revolve->ends, "endcount");
    bu_free((char *)revolve, "revolve_specific");
}


/**
 * R T _ R E V O L V E _ C L A S S
 */
int
rt_revolve_class()
{
    return 0;
}


/**
 * R T _ R E V O L V E _ P L O T
 */
int
rt_revolve_plot(struct bu_list *vhead, struct rt_db_internal *ip, const struct rt_tess_tol *ttol, const struct bn_tol *UNUSED(tol))
{
    struct rt_revolve_internal *rip;

    size_t nvert, narc, nadd, nseg, i, j, k;
    point2d_t *verts;
    struct curve *crv;

    vect_t ell[16], cir[16], ucir[16], height, xdir, ydir, ux, uy, uz, rEnd, xEnd, yEnd;
    fastf_t cos22_5 = 0.9238795325112867385;
    fastf_t cos67_5 = 0.3826834323650898373;
    int *endcount;
    point_t add, add2, add3;

    BU_CK_LIST_HEAD(vhead);
    RT_CK_DB_INTERNAL(ip);
    rip = (struct rt_revolve_internal *)ip->idb_ptr;
    RT_REVOLVE_CK_MAGIC(rip);

    nvert = rip->sk->vert_count;
    verts = rip->sk->verts;
    crv = &rip->sk->skt_curve;

    if (rip->ang < 2*M_PI) {
	narc = ceil(rip->ang * 8 * M_1_PI);
    } else {
	narc = 16;
    }
    VSCALE(xEnd, rip->r, cos(rip->ang));
    VCROSS(rEnd, rip->axis3d, rip->r);	/* using rEnd for temp storage */
    VSCALE(yEnd, rEnd, sin(rip->ang));
    VADD2(rEnd, xEnd, yEnd);

    VMOVE(uz, rip->axis3d);
    VMOVE(ux, rip->r);
    VCROSS(uy, uz, ux);

    VUNITIZE(ux);
    VUNITIZE(uy);
    VUNITIZE(uz);

    /* setup unit circle to be scaled */
    VMOVE(ucir[ 0], ux);
    VREVERSE(ucir[ 8], ucir[0]);

    VSCALE(xdir, ux, cos22_5);
    VSCALE(ydir, uy, cos67_5);
    VADD2(ucir[ 1], xdir, ydir);
    VREVERSE(ucir[ 9], ucir[1]);
    VREVERSE(xdir, xdir);
    VADD2(ucir[ 7], xdir, ydir);
    VREVERSE(ucir[15], ucir[7]);

    VSCALE(xdir, ux, M_SQRT1_2);
    VSCALE(ydir, uy, M_SQRT1_2);
    VADD2(ucir[ 2], xdir, ydir);
    VREVERSE(ucir[10], ucir[2]);
    VREVERSE(xdir, xdir);
    VADD2(ucir[ 6], xdir, ydir);
    VREVERSE(ucir[14], ucir[6]);


    VSCALE(xdir, ux, cos67_5);
    VSCALE(ydir, uy, cos22_5);
    VADD2(ucir[ 3], xdir, ydir);
    VREVERSE(ucir[11], ucir[3]);
    VREVERSE(xdir, xdir);
    VADD2(ucir[ 5], xdir, ydir);
    VREVERSE(ucir[13], ucir[5]);

    VMOVE(ucir[ 4], uy);
    VREVERSE(ucir[12], ucir[4]);

    /* find open endpoints, and determine which points are used */
    endcount = (int *)bu_calloc(rip->sk->vert_count, sizeof(int), "endcount");
    for (i=0; i<rip->sk->vert_count; i++) {
	endcount[i] = 0;
    }
    nseg = rip->sk->skt_curve.seg_count;

    for (i=0; i<nseg; i++) {
	long *lng;
	struct line_seg *lsg;
	struct carc_seg *csg;
	struct nurb_seg *nsg;
	struct bezier_seg *bsg;

	lng = (long *)rip->sk->skt_curve.segments[i];

	switch (*lng) {
	    case CURVE_LSEG_MAGIC:
		lsg = (struct line_seg *)lng;
		endcount[lsg->start]++;
		endcount[lsg->end]++;
		break;
	    case CURVE_CARC_MAGIC:
		csg = (struct carc_seg *)lng;
		if (csg->radius <= 0.0) break;
		endcount[csg->start]++;
		endcount[csg->end]++;
		break;
	    case CURVE_BEZIER_MAGIC:
		bsg = (struct bezier_seg *)lng;
		endcount[bsg->ctl_points[0]]++;
		endcount[bsg->ctl_points[bsg->degree]]++;
		break;
	    case CURVE_NURB_MAGIC:
		nsg = (struct nurb_seg *)lng;
		endcount[nsg->ctl_points[0]]++;
		endcount[nsg->ctl_points[nsg->c_size-1]]++;
		break;
	    default:
		bu_log("rt_revolve_prep: ERROR: unrecognized segment type!\n");
		break;
	}
    }

    /* draw circles */
    for (i=0; i<nvert; i++) {
	if (endcount[i] == 0) continue;
	VSCALE(height, uz, verts[i][Y]);
	for (j=0; j<narc; j++) {
	    VSCALE(cir[j], ucir[j], verts[i][X]);
	    VADD3(ell[j], rip->v3d, cir[j], height);
	}
	RT_ADD_VLIST(vhead, ell[0], BN_VLIST_LINE_MOVE);
	for (j=1; j<narc; j++) {
	    RT_ADD_VLIST(vhead, ell[j], BN_VLIST_LINE_DRAW);
	}
	if (narc < 16) {
	    VSCALE(cir[narc], rEnd, verts[i][X]);
	    VADD3(ell[narc], rip->v3d, cir[narc], height);
	    RT_ADD_VLIST(vhead, ell[narc], BN_VLIST_LINE_DRAW);
	} else {
	    RT_ADD_VLIST(vhead, ell[0], BN_VLIST_LINE_DRAW);
	}
    }

    /* convert endcounts to store which endpoints are odd */
    for (i=0, j=0; i<rip->sk->vert_count; i++) {
	if (endcount[i] % 2 != 0) {
	    /* add 'i' to list, insertion sort by vert[i][Y] */
	    for (k=j; k>0; k--) {
		if ((ZERO(verts[i][Y] - verts[endcount[k-1]][Y])
		     && verts[i][X] > verts[endcount[k-1]][X])
		    || (!ZERO(verts[i][Y] - verts[endcount[k-1]][Y])
			&& verts[i][Y] < verts[endcount[k-1]][Y])) {
		    endcount[k] = endcount[k-1];
		} else {
		    break;
		}
	    }
	    endcount[k] = i;
	    j++;
	}
    }
    nadd = j;
    while (j < rip->sk->vert_count) endcount[j++] = -1;

    /* draw sketch outlines */
    for (i=0; i<narc; i++) {
	curve_to_vlist(vhead, ttol, rip->v3d, ucir[i], uz, rip->sk, crv);
	for (j=0; j<nadd; j++) {
	    if (j+1 < nadd &&
		ZERO(verts[endcount[j]][Y] - verts[endcount[j+1]][Y])) {
		VJOIN1(add, rip->v3d, verts[endcount[j]][Y], rip->axis3d);
		VJOIN1(add2, add, verts[endcount[j]][X], ucir[i]);
		VJOIN1(add3, add, verts[endcount[j+1]][X], ucir[i]);
		RT_ADD_VLIST(vhead, add2, BN_VLIST_LINE_MOVE);
		RT_ADD_VLIST(vhead, add3, BN_VLIST_LINE_DRAW);
		j++;
	    } else {
		VJOIN1(add, rip->v3d, verts[endcount[j]][Y], rip->axis3d);
		VJOIN1(add2, add, verts[endcount[j]][X], ucir[i]);
		RT_ADD_VLIST(vhead, add, BN_VLIST_LINE_MOVE);
		RT_ADD_VLIST(vhead, add2, BN_VLIST_LINE_DRAW);
	    }
	}
    }
    if (narc < 16) {
	curve_to_vlist(vhead, ttol, rip->v3d, rEnd, uz, rip->sk, crv);
	for (j=0; j<nadd; j++) {
	    if (j+1 < nadd &&
		ZERO(verts[endcount[j]][Y] - verts[endcount[j+1]][Y])) {
		VJOIN1(add, rip->v3d, verts[endcount[j]][Y], rip->axis3d);
		VJOIN1(add2, add, verts[endcount[j]][X], rEnd);
		VJOIN1(add3, add, verts[endcount[j+1]][X], rEnd);
		RT_ADD_VLIST(vhead, add2, BN_VLIST_LINE_MOVE);
		RT_ADD_VLIST(vhead, add3, BN_VLIST_LINE_DRAW);
		j++;
	    } else {
		VJOIN1(add, rip->v3d, verts[endcount[j]][Y], rip->axis3d);
		VJOIN1(add2, add, verts[endcount[j]][X], rEnd);
		RT_ADD_VLIST(vhead, add, BN_VLIST_LINE_MOVE);
		RT_ADD_VLIST(vhead, add2, BN_VLIST_LINE_DRAW);
	    }
	}
	for (j=0; j<nadd; j+=2) {
	    if (!ZERO(verts[endcount[j]][Y] - verts[endcount[j+1]][Y])) {
		VJOIN1(add, rip->v3d, verts[endcount[j]][Y], rip->axis3d);
		VJOIN1(add2, rip->v3d, verts[endcount[j+1]][Y], rip->axis3d);
		RT_ADD_VLIST(vhead, add, BN_VLIST_LINE_MOVE);
		RT_ADD_VLIST(vhead, add2, BN_VLIST_LINE_DRAW);
	    }
	}
    }

    bu_free(endcount, "endcount");
    return 0;
}


/**
 * R T _ R E V O L V E _ T E S S
 *
 * Returns -
 * -1 failure
 * 0 OK.  *r points to nmgregion that holds this tessellation.
 */
int
rt_revolve_tess(struct nmgregion **r, struct model *m, struct rt_db_internal *ip, const struct rt_tess_tol *UNUSED(ttol), const struct bn_tol *UNUSED(tol))
{
    struct rt_revolve_internal *rip;

    if (r) NMG_CK_REGION(*r);
    if (m) NMG_CK_MODEL(m);

    rip = (struct rt_revolve_internal *)ip->idb_ptr;
    RT_REVOLVE_CK_MAGIC(rip);

    return -1;
}


/**
 * R T _ R E V O L V E _ I M P O R T 5
 *
 * Import an REVOLVE from the database format to the internal format.
 * Note that the data read will be in network order.  This means
 * Big-Endian integers and IEEE doubles for floating point.
 *
 * Apply modeling transformations as well.
 */
int
rt_revolve_import5(struct rt_db_internal *ip, const struct bu_external *ep, const mat_t mat, const struct db_i *dbip, struct resource *resp)
{
    struct rt_revolve_internal *rip;
    fastf_t vv[ELEMENTS_PER_VECT*3 + 1];

    char *sketch_name;
    unsigned char *ptr;
    struct directory *dp;
    struct rt_db_internal tmp_ip;

    if (dbip) RT_CK_DBI(dbip);

    RT_CK_DB_INTERNAL(ip);
    BU_CK_EXTERNAL(ep);

    /* set up the internal structure */
    ip->idb_major_type = DB5_MAJORTYPE_BRLCAD;
    ip->idb_type = ID_REVOLVE;
    ip->idb_meth = &rt_functab[ID_REVOLVE];
    ip->idb_ptr = bu_malloc(sizeof(struct rt_revolve_internal), "rt_revolve_internal");
    rip = (struct rt_revolve_internal *)ip->idb_ptr;
    rip->magic = RT_REVOLVE_INTERNAL_MAGIC;

    /* Convert the data in ep->ext_buf into internal format.  Note the
     * conversion from network data (Big Endian ints, IEEE double
     * floating point) to host local data representations.
     */

    ptr = (unsigned char *)ep->ext_buf;
    sketch_name = (char *)ptr + (ELEMENTS_PER_VECT*3 + 1)*SIZEOF_NETWORK_DOUBLE;
    if (!dbip)
	rip->sk = (struct rt_sketch_internal *)NULL;
    else if ((dp=db_lookup(dbip, sketch_name, LOOKUP_NOISY)) == RT_DIR_NULL) {
	bu_log("ERROR: Cannot find sketch (%s) for extrusion\n",
	       sketch_name);
	rip->sk = (struct rt_sketch_internal *)NULL;
    } else {
	if (rt_db_get_internal(&tmp_ip, dp, dbip, bn_mat_identity, resp) != ID_SKETCH) {
	    bu_log("ERROR: Cannot import sketch (%s) for extrusion\n",
		   sketch_name);
	    bu_free(ip->idb_ptr, "extrusion");
	    return -1;
	} else
	    rip->sk = (struct rt_sketch_internal *)tmp_ip.idb_ptr;
    }

    ntohd((unsigned char *)&vv, (unsigned char *)ep->ext_buf, ELEMENTS_PER_VECT*3 + 1);

    /* Apply the modeling transformation */
    if (mat == NULL) mat = bn_mat_identity;
    MAT4X3PNT(rip->v3d, mat, &vv[0*3]);
    MAT4X3PNT(rip->axis3d, mat, &vv[1*3]);
    MAT4X3PNT(rip->r, mat, &vv[2*3]);
    rip->ang = vv[9];

    /* convert name of data location */
    bu_vls_init(&rip->sketch_name);
    bu_vls_strcpy(&rip->sketch_name, (char *)ep->ext_buf + (ELEMENTS_PER_VECT * 3 + 1) * SIZEOF_NETWORK_DOUBLE);

    return 0;			/* OK */
}


/**
 * R T _ R E V O L V E _ X F O R M
 *
 * Apply a transformation matrix to the specified 'ip' input revolve
 * object, storing the results in the specified 'op' out pointer or
 * creating a copy if NULL.
 */
int
rt_revolve_xform(
    struct rt_db_internal *op,
    const mat_t mat,
    struct rt_db_internal *ip,
    int release,
    struct db_i *dbip,
    struct resource *resp)
{
    struct rt_revolve_internal *rip, *rop;
    point_t tmp_vec;

    if (dbip) RT_CK_DBI(dbip);
    RT_CK_DB_INTERNAL(ip);
    RT_CK_RESOURCE(resp);
    rip = (struct rt_revolve_internal *)ip->idb_ptr;
    RT_REVOLVE_CK_MAGIC(rip);

    if (bu_debug&BU_DEBUG_MEM_CHECK) {
	bu_log("Barrier check at start of revolve_xform():\n");
	bu_mem_barriercheck();
    }

    if (op != ip) {
	RT_DB_INTERNAL_INIT(op);
	rop = (struct rt_revolve_internal *)bu_malloc(sizeof(struct rt_revolve_internal), "rop");
	rop->magic = RT_REVOLVE_INTERNAL_MAGIC;
	bu_vls_init(&rop->sketch_name);
	bu_vls_vlscat(&rop->sketch_name, &rip->sketch_name);
	op->idb_ptr = (genptr_t)rop;
	op->idb_meth = &rt_functab[ID_REVOLVE];
	op->idb_major_type = DB5_MAJORTYPE_BRLCAD;
	op->idb_type = ID_REVOLVE;
	if (ip->idb_avs.magic == BU_AVS_MAGIC) {
	    bu_avs_init(&op->idb_avs, ip->idb_avs.count, "avs");
	    bu_avs_merge(&op->idb_avs, &ip->idb_avs);
	}
    } else {
	rop = (struct rt_revolve_internal *)ip->idb_ptr;
    }
    MAT4X3PNT(tmp_vec, mat, rip->v3d);
    VMOVE(rop->v3d, tmp_vec);
    MAT4X3VEC(tmp_vec, mat, rip->axis3d);
    VMOVE(rop->axis3d, tmp_vec);
    V2MOVE(rop->v2d, rip->v2d);
    V2MOVE(rop->axis2d, rip->axis2d);

    if (release && ip != op) {
	rop->sk = rip->sk;
	rip->sk = (struct rt_sketch_internal *)NULL;
	rt_db_free_internal(ip);
    } else if (rip->sk) {
	rop->sk = rt_copy_sketch(rip->sk);
    } else {
	rop->sk = (struct rt_sketch_internal *)NULL;
    }

    if (bu_debug&BU_DEBUG_MEM_CHECK) {
	bu_log("Barrier check at end of revolve_xform():\n");
	bu_mem_barriercheck();
    }

    return 0;
}


/**
 * R T _ R E V O L V E _ E X P O R T 5
 *
 * Export an REVOLVE from internal form to external format.  Note that
 * this means converting all integers to Big-Endian format and
 * floating point data to IEEE double.
 *
 * Apply the transformation to mm units as well.
 */
int
rt_revolve_export5(struct bu_external *ep, const struct rt_db_internal *ip, double local2mm, const struct db_i *dbip)
{
    struct rt_revolve_internal *rip;
    fastf_t vec[ELEMENTS_PER_VECT*3 + 1];
    unsigned char *ptr;

    if (dbip) RT_CK_DBI(dbip);

    RT_CK_DB_INTERNAL(ip);
    if (ip->idb_type != ID_REVOLVE) return -1;
    rip = (struct rt_revolve_internal *)ip->idb_ptr;
    RT_REVOLVE_CK_MAGIC(rip);

    BU_CK_EXTERNAL(ep);
    ep->ext_nbytes = SIZEOF_NETWORK_DOUBLE * (ELEMENTS_PER_VECT*3 + 1) + bu_vls_strlen(&rip->sketch_name) + 1;
    ep->ext_buf = (genptr_t)bu_calloc(1, ep->ext_nbytes, "revolve external");

    ptr = (unsigned char *)ep->ext_buf;

    /* Since libwdb users may want to operate in units other than mm,
     * we offer the opportunity to scale the solid (to get it into mm)
     * on the way out.
     */
    VSCALE(&vec[0*3], rip->v3d, local2mm);
    VSCALE(&vec[1*3], rip->axis3d, local2mm);
    VSCALE(&vec[2*3], rip->r, local2mm);
    vec[9] = rip->ang;

    htond(ptr, (unsigned char *)vec, ELEMENTS_PER_VECT*3 + 1);
    ptr += (ELEMENTS_PER_VECT*3 + 1) * SIZEOF_NETWORK_DOUBLE;

    bu_strlcpy((char *)ptr, bu_vls_addr(&rip->sketch_name), bu_vls_strlen(&rip->sketch_name) + 1);

    return 0;
}


/**
 * R T _ R E V O L V E _ D E S C R I B E
 *
 * Make human-readable formatted presentation of this solid.  First
 * line describes type of solid.  Additional lines are indented one
 * tab, and give parameter values.
 */
int
rt_revolve_describe(struct bu_vls *str, const struct rt_db_internal *ip, int verbose, double mm2local)
{
    struct rt_revolve_internal *rip =
	(struct rt_revolve_internal *)ip->idb_ptr;
    char buf[256];

    RT_REVOLVE_CK_MAGIC(rip);
    bu_vls_strcat(str, "truncated general revolve (REVOLVE)\n");

    if (!verbose)
	return 0;

    sprintf(buf, "\tV (%g, %g, %g)\n",
	    INTCLAMP(rip->v3d[X] * mm2local),
	    INTCLAMP(rip->v3d[Y] * mm2local),
	    INTCLAMP(rip->v3d[Z] * mm2local));
    bu_vls_strcat(str, buf);

    sprintf(buf, "\tAxis (%g, %g, %g)\n",
	    INTCLAMP(rip->axis3d[X] * mm2local),
	    INTCLAMP(rip->axis3d[Y] * mm2local),
	    INTCLAMP(rip->axis3d[Z] * mm2local));
    bu_vls_strcat(str, buf);

    sprintf(buf, "\tR (%g, %g, %g)\n",
	    INTCLAMP(rip->r[X] * mm2local),
	    INTCLAMP(rip->r[Y] * mm2local),
	    INTCLAMP(rip->r[Z] * mm2local));
    bu_vls_strcat(str, buf);

    sprintf(buf, "\tAngle=%g\n", INTCLAMP(rip->ang * RAD2DEG));
    bu_vls_strcat(str, buf);

    sprintf(buf, "\tsketch name: ");
    bu_vls_strcat(str, buf);
    bu_vls_vlscat(str, &rip->sketch_name);

    return 0;
}


/**
 * R T _ R E V O L V E _ I F R E E
 *
 * Free the storage associated with the rt_db_internal version of this
 * solid.
 */
void
rt_revolve_ifree(struct rt_db_internal *ip)
{
    struct rt_revolve_internal *revolve_ip;

    RT_CK_DB_INTERNAL(ip);

    revolve_ip = (struct rt_revolve_internal *)ip->idb_ptr;
    RT_REVOLVE_CK_MAGIC(revolve_ip);
    revolve_ip->magic = 0;			/* sanity */

    if (BU_VLS_IS_INITIALIZED(&revolve_ip->sketch_name))
	bu_vls_free(&revolve_ip->sketch_name);
    else
	bu_log("Freeing bogus revolve, VLS string not initialized\n");

    bu_free((char *)revolve_ip, "revolve ifree");
    ip->idb_ptr = GENPTR_NULL;	/* sanity */
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
