/*                          P I P E . C
 * BRL-CAD
 *
 * Copyright (c) 1990-2014 United States Government as represented by
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
/** @file primitives/pipe/pipe.c
 *
 * Intersect a ray with a pipe solid.
 *
 */
/** @} */

#include "common.h"

#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>

#include <float.h>
#include <math.h>
#include "bin.h"

#include "tcl.h"

#include "bu/cv.h"
#include "vmath.h"

#include "bn.h"
#include "db.h"
#include "nmg.h"
#include "rtgeom.h"
#include "raytrace.h"
#include "wdb.h"
#include "../../librt_private.h"


struct id_pipe {
    struct bu_list l;
    int pipe_is_bend;
};


struct lin_pipe {
    struct bu_list l;
    int pipe_is_bend;
    vect_t pipe_V;				/* start point for pipe section */
    vect_t pipe_H;				/* unit vector in direction of pipe section */
    fastf_t pipe_ribase, pipe_ritop;		/* base and top inner radii */
    fastf_t pipe_ribase_sq, pipe_ritop_sq;	/* inner radii squared */
    fastf_t pipe_ridiff_sq, pipe_ridiff;	/* difference between top and base inner radii */
    fastf_t pipe_rodiff_sq, pipe_rodiff;	/* difference between top and base outer radii */
    fastf_t pipe_robase, pipe_rotop;		/* base and top outer radii */
    fastf_t pipe_robase_sq, pipe_rotop_sq;	/* outer radii squared */
    fastf_t pipe_len;				/* length of pipe segment */
    mat_t pipe_SoR;				/* Scale and rotate */
    mat_t pipe_invRoS;				/* inverse rotation and scale */
    point_t pipe_min;
    point_t pipe_max;
};


struct bend_pipe {
    struct bu_list l;
    int pipe_is_bend;
    fastf_t bend_radius;		/* distance from bend_v to center of pipe */
    fastf_t bend_or;			/* outer radius */
    fastf_t bend_ir;			/* inner radius */
    mat_t bend_invR;			/* inverse rotation matrix */
    mat_t bend_SoR;			/* Scale and rotate */
    point_t bend_V;			/* Center of bend */
    point_t bend_start;			/* Start of bend */
    point_t bend_end;			/* End of bend */
    fastf_t bend_alpha_i;		/* ratio of inner radius to bend radius */
    fastf_t bend_alpha_o;		/* ratio of outer radius to bend radius */
    fastf_t bend_angle;			/* Angle that bend goes through */
    vect_t bend_ra;			/* unit vector in plane of bend (points toward start from bend_V) */
    vect_t bend_rb;			/* unit vector in plane of bend (normal to bend_ra) */
    vect_t bend_endNorm;		/* unit vector normal to end plane */
    vect_t bend_startNorm;		/* unit vector normal to start plane */
    vect_t bend_N;			/* unit vector normal to plane of bend */
    point_t bend_bound_center;		/* center of bounding sphere */
    fastf_t bend_bound_radius_sq;	/* square of bounding sphere radius */
};


/* two orthogonal unit vectors that define an orientation */
struct pipe_orientation {
    vect_t v1;
    vect_t v2;
};


/* A plotted circle defined by a center point, an orientation at that point,
 * and a radius.
 */
struct pipe_circle {
    point_t center;
    fastf_t radius;
    struct pipe_orientation orient;
};


struct pipe_segment {
    struct wdb_pipept *cur;
    struct bu_list *pipe_segs_head;
    struct pipe_orientation orient;
    point_t last_drawn;
    int connecting_arcs;
};


struct pipe_bend {
    struct pipe_circle pipe_circle;
    struct pipe_circle bend_circle;
    vect_t bend_normal;
    point_t bend_start;
    point_t bend_end;
    fastf_t bend_angle;
};


#define PIPE_CONNECTING_ARCS 4 /* number of connecting arcs to draw between points */
#define PIPE_CIRCLE_SEGS 16    /* number of segments used to plot a circle */

#define PIPE_LINEAR_OUTER_BODY	1
#define PIPE_LINEAR_INNER_BODY	2
#define PIPE_LINEAR_TOP		3
#define PIPE_LINEAR_BASE	4
#define PIPE_BEND_OUTER_BODY	5
#define PIPE_BEND_INNER_BODY	6
#define PIPE_BEND_BASE		7
#define PIPE_BEND_TOP		8
#define PIPE_RADIUS_CHANGE      9

#define RT_PIPE_MAXHITS 128

static fastf_t
pipe_seg_bend_angle(const struct pipe_segment *seg)
{
    struct wdb_pipept *prevpt, *curpt, *nextpt;
    fastf_t dot, rad_between_segments, supplementary_angle;
    vect_t cur_to_prev, cur_to_next;

    curpt = seg->cur;
    prevpt = BU_LIST_PREV(wdb_pipept, &curpt->l);
    nextpt = BU_LIST_NEXT(wdb_pipept, &curpt->l);

    VSUB2(cur_to_prev, prevpt->pp_coord, curpt->pp_coord);
    VSUB2(cur_to_next, nextpt->pp_coord, curpt->pp_coord);
    VUNITIZE(cur_to_prev);
    VUNITIZE(cur_to_next);

    dot = VDOT(cur_to_prev, cur_to_next);
    rad_between_segments = acos(VDOT(cur_to_prev, cur_to_next));

    /* handle the cases where acos returned NaN because floating point fuzz
     * caused dot to be slightly outside the valid [-1.0, 1.0] range
     */
    if (isnan(rad_between_segments)) {
	if (dot >= 1.0) {
	    rad_between_segments = acos(1.0);
	} else if (dot <= -1.0) {
	    rad_between_segments = acos(-1.0);
	}
    }

    supplementary_angle = M_PI - rad_between_segments;

    return supplementary_angle;
}


static fastf_t
pipe_seg_dist_to_bend_endpoint(const struct pipe_segment *seg)
{
    fastf_t bend_angle, dist_to_bend_end;

    /* The fewer the radians between the segments, the more the bend is
     * pushed away from cur and toward prev and next.
     *
     * (rad < pi/2) => (dist > bendradius)
     * (rad = pi/2) => (dist = bendradius)
     * (rad > pi/2) => (dist < bendradius)
     */
    bend_angle = pipe_seg_bend_angle(seg);
    dist_to_bend_end = seg->cur->pp_bendradius * tan(bend_angle / 2.0);

    return dist_to_bend_end;
}


static void
pipe_seg_bend_normal(vect_t norm, const struct pipe_segment *seg)
{
    vect_t cur_to_prev, cur_to_next;
    struct wdb_pipept *prevpt, *curpt, *nextpt;

    curpt = seg->cur;
    prevpt = BU_LIST_PREV(wdb_pipept, &curpt->l);
    nextpt = BU_LIST_NEXT(wdb_pipept, &curpt->l);

    VSUB2(cur_to_prev, prevpt->pp_coord, curpt->pp_coord);
    VSUB2(cur_to_next, nextpt->pp_coord, curpt->pp_coord);
    VCROSS(norm, cur_to_prev, cur_to_next);
    VUNITIZE(norm);
}


static struct pipe_orientation
pipe_orient_from_normal(const vect_t norm)
{
    struct pipe_orientation orient;

    bn_vec_ortho(orient.v1, norm);
    VCROSS(orient.v2, norm, orient.v1);
    VUNITIZE(orient.v2);

    return orient;
}


static struct pipe_segment *
pipe_seg_first(struct rt_pipe_internal *pipe)
{
    struct bu_list *seghead;
    struct pipe_segment *first_seg = NULL;
    struct wdb_pipept *pt_1, *pt_2;
    vect_t cur_to_next;

    RT_PIPE_CK_MAGIC(pipe);

    /* no points */
    if (BU_LIST_IS_EMPTY(&pipe->pipe_segs_head)) {
	return NULL;
    }

    seghead = &pipe->pipe_segs_head;
    pt_1 = BU_LIST_FIRST(wdb_pipept, seghead);
    pt_2 = BU_LIST_NEXT(wdb_pipept, &pt_1->l);

    /* only one point */
    if (BU_LIST_IS_HEAD(&pt_2->l, seghead)) {
	return NULL;
    }

    BU_GET(first_seg, struct pipe_segment);
    first_seg->pipe_segs_head = seghead;
    first_seg->cur = pt_1;

    VSUB2(cur_to_next, pt_2->pp_coord, pt_1->pp_coord);
    first_seg->orient = pipe_orient_from_normal(cur_to_next);

    return first_seg;
}


static void
pipe_seg_advance(struct pipe_segment *seg)
{
    seg->cur = BU_LIST_NEXT(wdb_pipept, &seg->cur->l);
}


static int
pipe_seg_is_last(const struct pipe_segment *seg)
{
    struct wdb_pipept *nextpt;

    nextpt = BU_LIST_NEXT(wdb_pipept, &seg->cur->l);
    if (BU_LIST_IS_HEAD(nextpt, seg->pipe_segs_head)) {
	return 1;
    }

    return 0;
}


static int
pipe_seg_is_bend(const struct pipe_segment *seg)
{
    vect_t cur_to_prev, cur_to_next, norm;
    struct wdb_pipept *prevpt, *curpt, *nextpt;
    fastf_t dist_to_bend_end;

    curpt = seg->cur;
    prevpt = BU_LIST_PREV(wdb_pipept, &curpt->l);
    nextpt = BU_LIST_NEXT(wdb_pipept, &curpt->l);

    VSUB2(cur_to_prev, prevpt->pp_coord, curpt->pp_coord);
    VSUB2(cur_to_next, nextpt->pp_coord, curpt->pp_coord);
    VCROSS(norm, cur_to_prev, cur_to_next);
    VUNITIZE(cur_to_prev);
    VUNITIZE(cur_to_next);

    dist_to_bend_end = pipe_seg_dist_to_bend_endpoint(seg);

    /* in the extreme cases where the interior angle between the adjacent
     * segments is nearly 0 or pi radians, the points are considered
     * collinear
     */
    if (isinf(dist_to_bend_end)
	|| NEAR_ZERO(dist_to_bend_end, SQRT_SMALL_FASTF)
	|| VNEAR_ZERO(norm, SQRT_SMALL_FASTF))
    {
	return 0;
    }

    return 1;
}


HIDDEN int
rt_bend_pipe_prep(
    struct bu_list *head,
    fastf_t *bend_center, fastf_t *bend_start, fastf_t *bend_end,
    fastf_t bend_radius, fastf_t bend_angle,
    fastf_t od, fastf_t id,
    fastf_t prev_od, fastf_t next_od,
    point_t *min, point_t *max)
{
    struct bend_pipe *bp;
    vect_t to_start, to_end;
    mat_t R;
    point_t work;
    fastf_t f;
    fastf_t max_od;
    fastf_t max_or;
    fastf_t max_r;

    BU_GET(bp, struct bend_pipe);

    bp->pipe_is_bend = 1;
    bp->bend_or = od * 0.5;
    bp->bend_ir = id * 0.5;

    VMOVE(bp->bend_start, bend_start);
    VMOVE(bp->bend_end, bend_end);
    VMOVE(bp->bend_V, bend_center);
    VSUB2(to_start, bend_start, bend_center);
    bp->bend_radius = bend_radius;
    VSUB2(to_end, bend_end, bend_center);
    VSCALE(bp->bend_ra, to_start, 1.0 / bp->bend_radius);
    VCROSS(bp->bend_N, to_start, to_end);
    VUNITIZE(bp->bend_N);
    VCROSS(bp->bend_rb, bp->bend_N, bp->bend_ra);
    VCROSS(bp->bend_startNorm, bp->bend_ra, bp->bend_N);
    VCROSS(bp->bend_endNorm, bp->bend_N, to_end);
    VUNITIZE(bp->bend_endNorm);

    bp->bend_angle = bend_angle;

    /* angle goes from 0.0 at start to some angle less than PI */
    if (bp->bend_angle >= M_PI) {
	bu_log("Error: rt_pipe_prep: Bend section bends through more than 180 degrees\n");
	return 1;
    }

    bp->bend_alpha_i = bp->bend_ir / bp->bend_radius;
    bp->bend_alpha_o = bp->bend_or / bp->bend_radius;

    MAT_IDN(R);
    VMOVE(&R[0], bp->bend_ra);
    VMOVE(&R[4], bp->bend_rb);
    VMOVE(&R[8], bp->bend_N);

    if (bn_mat_inverse(bp->bend_invR, R) == 0) {
	BU_PUT(bp, struct bend_pipe);
	return 0; /* there is nothing to bend, that's OK */
    }


    MAT_COPY(bp->bend_SoR, R);
    bp->bend_SoR[15] *= bp->bend_radius;

    /* bounding box for entire torus */
    /* include od of previous and next segment
     * to allow for discontinuous radii
     */
    max_od = od;
    if (prev_od > max_od) {
	max_od = prev_od;
    }
    if (next_od > max_od) {
	max_od = next_od;
    }
    max_or = max_od / 2.0;
    max_r = bend_radius + max_or;

    VBLEND2(bp->bend_bound_center, 0.5, bend_start, 0.5, bend_end);
    bp->bend_bound_radius_sq = max_r * sin(bend_angle / 2.0);
    bp->bend_bound_radius_sq = bp->bend_bound_radius_sq * bp->bend_bound_radius_sq;
    bp->bend_bound_radius_sq += max_or * max_or;
    f = sqrt(bp->bend_bound_radius_sq);
    VMOVE(work, bp->bend_bound_center);
    work[X] -= f;
    work[Y] -= f;
    work[Z] -= f;
    VMINMAX(*min, *max, work);
    VMOVE(work, bp->bend_bound_center);
    work[X] += f;
    work[Y] += f;
    work[Z] += f;
    VMINMAX(*min, *max, work);

    if (head) {
	BU_LIST_INSERT(head, &bp->l);
    } else {
	BU_PUT(bp, struct bend_pipe);
    }

    return 0;

}


HIDDEN void
rt_linear_pipe_prep(
    struct bu_list *head,
    fastf_t *pt1, fastf_t id1, fastf_t od1,
    fastf_t *pt2, fastf_t id2, fastf_t od2,
    point_t *min,
    point_t *max)
{
    struct lin_pipe *lp;
    mat_t R;
    mat_t Rinv;
    mat_t S;
    point_t work;
    vect_t seg_ht;
    vect_t v1, v2;

    BU_GET(lp, struct lin_pipe);

    VMOVE(lp->pipe_V, pt1);

    VSUB2(seg_ht, pt2, pt1);
    lp->pipe_ribase = id1 / 2.0;
    lp->pipe_ribase_sq = lp->pipe_ribase * lp->pipe_ribase;
    lp->pipe_ritop = id2 / 2.0;
    lp->pipe_ritop_sq = lp->pipe_ritop * lp->pipe_ritop;
    lp->pipe_robase = od1 / 2.0;
    lp->pipe_robase_sq = lp->pipe_robase * lp->pipe_robase;
    lp->pipe_rotop = od2 / 2.0;
    lp->pipe_rotop_sq = lp->pipe_rotop * lp->pipe_rotop;
    lp->pipe_ridiff = lp->pipe_ritop - lp->pipe_ribase;
    lp->pipe_ridiff_sq = lp->pipe_ridiff * lp->pipe_ridiff;
    lp->pipe_rodiff = lp->pipe_rotop - lp->pipe_robase;
    lp->pipe_rodiff_sq = lp->pipe_rodiff * lp->pipe_rodiff;
    lp->pipe_is_bend = 0;

    lp->pipe_len = MAGNITUDE(seg_ht);
    VSCALE(seg_ht, seg_ht, 1.0 / lp->pipe_len);
    VMOVE(lp->pipe_H, seg_ht);
    bn_vec_ortho(v1, seg_ht);
    VCROSS(v2, seg_ht, v1);

    /* build R matrix */
    MAT_IDN(R);
    VMOVE(&R[0], v1);
    VMOVE(&R[4], v2);
    VMOVE(&R[8], seg_ht);

    /* Rinv is transpose */
    bn_mat_trn(Rinv, R);

    /* Build Scale matrix */
    MAT_IDN(S);
    S[10] = 1.0 / lp->pipe_len;

    /* Compute SoR and invRoS */
    bn_mat_mul(lp->pipe_SoR, S, R);
    bn_mat_mul(lp->pipe_invRoS, Rinv, S);

    VSETALL(lp->pipe_min, INFINITY);
    VSETALL(lp->pipe_max, -INFINITY);

    VJOIN2(work, pt1, od1, v1, od1, v2);
    VMINMAX(*min, *max, work);
    VMINMAX(lp->pipe_min, lp->pipe_max, work);
    VJOIN2(work, pt1, -od1, v1, od1, v2);
    VMINMAX(*min, *max, work);
    VMINMAX(lp->pipe_min, lp->pipe_max, work);
    VJOIN2(work, pt1, od1, v1, -od1, v2);
    VMINMAX(*min, *max, work);
    VMINMAX(lp->pipe_min, lp->pipe_max, work);
    VJOIN2(work, pt1, -od1, v1, -od1, v2);
    VMINMAX(*min, *max, work);
    VMINMAX(lp->pipe_min, lp->pipe_max, work);

    VJOIN2(work, pt2, od2, v1, od2, v2);
    VMINMAX(*min, *max, work);
    VMINMAX(lp->pipe_min, lp->pipe_max, work);
    VJOIN2(work, pt2, -od2, v1, od2, v2);
    VMINMAX(*min, *max, work);
    VMINMAX(lp->pipe_min, lp->pipe_max, work);
    VJOIN2(work, pt2, od2, v1, -od2, v2);
    VMINMAX(*min, *max, work);
    VMINMAX(lp->pipe_min, lp->pipe_max, work);
    VJOIN2(work, pt2, -od2, v1, -od2, v2);
    VMINMAX(*min, *max, work);
    VMINMAX(lp->pipe_min, lp->pipe_max, work);

    if (head) {
	BU_LIST_INSERT(head, &lp->l);
    } else {
	bu_free(lp, "free pipe bb lp segment");
    }
}


HIDDEN void
pipe_elements_calculate(struct bu_list *elements_head, struct rt_db_internal *ip, point_t *min, point_t *max)
{
    struct rt_pipe_internal *pip;
    struct wdb_pipept *pp1, *pp2, *pp3;
    point_t curr_pt;
    fastf_t curr_id, curr_od;


    RT_CK_DB_INTERNAL(ip);
    pip = (struct rt_pipe_internal *)ip->idb_ptr;
    RT_PIPE_CK_MAGIC(pip);

    VSETALL(*min, INFINITY);
    VSETALL(*max, -INFINITY);

    if (BU_LIST_IS_EMPTY(&(pip->pipe_segs_head))) {
	return;
    }

    pp1 = BU_LIST_FIRST(wdb_pipept, &(pip->pipe_segs_head));
    pp2 = BU_LIST_NEXT(wdb_pipept, &pp1->l);
    if (BU_LIST_IS_HEAD(&pp2->l, &(pip->pipe_segs_head))) {
	return;
    }
    pp3 = BU_LIST_NEXT(wdb_pipept, &pp2->l);
    if (BU_LIST_IS_HEAD(&pp3->l, &(pip->pipe_segs_head))) {
	pp3 = (struct wdb_pipept *)NULL;
    }

    VSETALL((*min), INFINITY);
    VSETALL((*max), -INFINITY);

    VMOVE(curr_pt, pp1->pp_coord);
    curr_od = pp1->pp_od;
    curr_id = pp1->pp_id;
    while (1) {
	vect_t n1, n2;
	vect_t norm;
	vect_t v1;
	vect_t diff;
	fastf_t angle;
	fastf_t dist_to_bend;
	point_t bend_start, bend_end, bend_center;

	VSUB2(n1, curr_pt, pp2->pp_coord);
	if (VNEAR_ZERO(n1, RT_LEN_TOL)) {
	    /* duplicate point, skip to next point */
	    goto next_pt;
	}

	if (!pp3) {
	    /* last segment */
	    rt_linear_pipe_prep(elements_head, curr_pt, curr_id, curr_od, pp2->pp_coord, pp2->pp_id, pp2->pp_od, min, max);
	    break;
	}

	VSUB2(n2, pp3->pp_coord, pp2->pp_coord);
	VCROSS(norm, n1, n2);
	VUNITIZE(n1);
	VUNITIZE(n2);
	angle = M_PI - acos(VDOT(n1, n2));
	dist_to_bend = pp2->pp_bendradius * tan(angle / 2.0);
	if (isnan(dist_to_bend) || VNEAR_ZERO(norm, SQRT_SMALL_FASTF) || NEAR_ZERO(dist_to_bend, SQRT_SMALL_FASTF)) {
	    /* points are collinear, treat as a linear segment */
	    rt_linear_pipe_prep(elements_head, curr_pt, curr_id, curr_od,
				pp2->pp_coord, pp2->pp_id, pp2->pp_od, min, max);
	    VMOVE(curr_pt, pp2->pp_coord);
	    goto next_pt;
	}

	VJOIN1(bend_start, pp2->pp_coord, dist_to_bend, n1);
	VJOIN1(bend_end, pp2->pp_coord, dist_to_bend, n2);

	VUNITIZE(norm);

	/* linear section */
	VSUB2(diff, curr_pt, bend_start);
	if (MAGNITUDE(diff) <= RT_LEN_TOL) {
	    /* do not make linear sections that are too small to raytrace */
	    VMOVE(bend_start, curr_pt);
	} else {
	    rt_linear_pipe_prep(elements_head, curr_pt, curr_id, curr_od,
				bend_start, pp2->pp_id, pp2->pp_od, min, max);
	}

	/* and bend section */
	VCROSS(v1, n1, norm);
	VJOIN1(bend_center, bend_start, -pp2->pp_bendradius, v1);
	rt_bend_pipe_prep(elements_head, bend_center, bend_start, bend_end, pp2->pp_bendradius, angle,
			  pp2->pp_od, pp2->pp_id, pp1->pp_od, pp3->pp_od, min, max);

	VMOVE(curr_pt, bend_end);
    next_pt:
	if (!pp3) {
	    break;
	}
	curr_id = pp2->pp_id;
	curr_od = pp2->pp_od;
	pp1 = pp2;
	pp2 = pp3;
	pp3 = BU_LIST_NEXT(wdb_pipept, &pp3->l);
	if (BU_LIST_IS_HEAD(&pp3->l, &(pip->pipe_segs_head))) {
	    pp3 = (struct wdb_pipept *)NULL;
	}
    }
}


HIDDEN void
pipe_elements_free(struct bu_list *head)
{
    if (head != 0) {
	struct id_pipe *p;

	while (BU_LIST_WHILE(p, id_pipe, head)) {
	    BU_LIST_DEQUEUE(&(p->l));
	    if (p->pipe_is_bend) {
		BU_PUT(p, struct lin_pipe);
	    } else {
		BU_PUT(p, struct bend_pipe);
	    }
	}
    }
}


/**
 * Calculate a bounding RPP for a pipe
 */
int
rt_pipe_bbox(
    struct rt_db_internal *ip,
    point_t *min,
    point_t *max,
    const struct bn_tol *UNUSED(tol))
{
    pipe_elements_calculate(NULL, ip, min, max);
    return 0;
}


/**
 * Given a pointer to a GED database record, and a transformation
 * matrix, determine if this is a valid pipe solid, and if so,
 * precompute various terms of the formula.
 *
 * Returns -
 *  0 if pipe solid is OK
 * !0 if there is an error in the description
 *
 * Implicit return -
 * A struct bu_list is created, and its address is stored in
 * stp->st_specific for use by pipe_shot().
 */
int
rt_pipe_prep(struct soltab *stp, struct rt_db_internal *ip, struct rt_i *rtip)
{
    struct bu_list *head;
    fastf_t dx, dy, dz, f;

    if (rtip) {
	RT_CK_RTI(rtip);
    }

    BU_GET(head, struct bu_list);
    BU_LIST_INIT(head);

    pipe_elements_calculate(head, ip, &(stp->st_min), &(stp->st_max));

    stp->st_specific = (void *)head;

    VSET(stp->st_center,
	 (stp->st_max[X] + stp->st_min[X]) / 2,
	 (stp->st_max[Y] + stp->st_min[Y]) / 2,
	 (stp->st_max[Z] + stp->st_min[Z]) / 2);

    dx = (stp->st_max[X] - stp->st_min[X]) / 2;
    f = dx;
    dy = (stp->st_max[Y] - stp->st_min[Y]) / 2;
    if (dy > f) {
	f = dy;
    }
    dz = (stp->st_max[Z] - stp->st_min[Z]) / 2;
    if (dz > f) {
	f = dz;
    }
    stp->st_aradius = f;
    stp->st_bradius = sqrt(dx * dx + dy * dy + dz * dz);

    return 0;
}


void
rt_pipe_print(const struct soltab *stp)
{
    struct bu_list *head = (struct bu_list *)stp->st_specific;

    if (!head) {
	return;
    }
}


void
rt_pipept_print(const struct wdb_pipept *pipept, double mm2local)
{
    point_t p1;

    bu_log("Pipe Vertex:\n");
    VSCALE(p1, pipept->pp_coord, mm2local);
    bu_log("\tat (%g %g %g)\n", V3ARGS(p1));
    bu_log("\tbend radius = %g\n", pipept->pp_bendradius * mm2local);
    if (pipept->pp_id > 0.0) {
	bu_log("\tod=%g, id=%g\n",
	       pipept->pp_od * mm2local,
	       pipept->pp_id * mm2local);
    } else {
	bu_log("\tod=%g\n", pipept->pp_od * mm2local);
    }
}


void
rt_vls_pipept(
    struct bu_vls *vp,
    int seg_no,
    const struct rt_db_internal *ip,
    double mm2local)
{
    struct rt_pipe_internal *pint;
    struct wdb_pipept *pipept;
    int seg_count = 0;
    char buf[256];
    point_t p1;

    pint = (struct rt_pipe_internal *)ip->idb_ptr;
    RT_PIPE_CK_MAGIC(pint);

    pipept = BU_LIST_FIRST(wdb_pipept, &pint->pipe_segs_head);
    while (++seg_count != seg_no && BU_LIST_NOT_HEAD(&pipept->l, &pint->pipe_segs_head)) {
	pipept = BU_LIST_NEXT(wdb_pipept, &pipept->l);
    }


    sprintf(buf, "Pipe Vertex:\n");
    bu_vls_strcat(vp, buf);
    VSCALE(p1, pipept->pp_coord, mm2local);
    sprintf(buf, "\tat (%g %g %g)\n", V3ARGS(p1));
    bu_vls_strcat(vp, buf);
    sprintf(buf, "\tbend radius = %g\n", pipept->pp_bendradius * mm2local);
    bu_vls_strcat(vp, buf);
    if (pipept->pp_id > 0.0) {
	sprintf(buf, "\tod=%g, id=%g\n",
		pipept->pp_od * mm2local,
		pipept->pp_id * mm2local);
    } else {
	sprintf(buf, "\tod=%g\n", pipept->pp_od * mm2local);
    }
    bu_vls_strcat(vp, buf);
}


/**
 * Check for hits on surfaces created by discontinuous radius changes
 * from one pipe segment to the next. Can only happen when one segment
 * is a bend, because linear segments handle different radii at each
 * end. Bend segments must have constant radii .  These surfaces are
 * normal to the flow of the pipe.
 */
HIDDEN void
discont_radius_shot(
    struct xray *rp,
    point_t center,
    vect_t norm,
    fastf_t or1_sq, fastf_t ir1_sq,
    fastf_t or2_sq, fastf_t ir2_sq,
    struct hit *hits,
    int *hit_count,
    int seg_no,
    struct soltab *stp)
{
    fastf_t dist_to_plane;
    fastf_t norm_dist;
    fastf_t slant_factor;
    fastf_t t_tmp;
    point_t hit_pt;
    fastf_t radius_sq;

    /* calculate intersection with plane at center (with normal "norm") */
    dist_to_plane = VDOT(norm, center);
    norm_dist = dist_to_plane - VDOT(norm, rp->r_pt);
    slant_factor = VDOT(norm, rp->r_dir);
    if (!ZERO(slant_factor)) {
	vect_t to_center;
	struct hit *hitp;

	t_tmp = norm_dist / slant_factor;
	VJOIN1(hit_pt, rp->r_pt, t_tmp, rp->r_dir);
	VSUB2(to_center, center, hit_pt);
	radius_sq = MAGSQ(to_center);

	/* where the radius ranges overlap, there is no hit */
	if (radius_sq <= or1_sq && radius_sq >= ir1_sq &&
	    radius_sq <= or2_sq && radius_sq >= ir2_sq) {
	    return;
	}

	/* if we are within one of the radius ranges, we have a hit */
	if ((radius_sq <= or2_sq && radius_sq >= ir2_sq) ||
	    (radius_sq <= or1_sq && radius_sq >= ir1_sq)) {
	    hitp = &hits[*hit_count];
	    hitp->hit_magic = RT_HIT_MAGIC;
	    hitp->hit_dist = t_tmp;
	    hitp->hit_surfno = seg_no * 10 + PIPE_RADIUS_CHANGE;

	    /* within first range, use norm, otherwise reverse */
	    if (radius_sq <= or1_sq && radius_sq >= ir1_sq) {
		VMOVE(hitp->hit_normal, norm);
	    } else {
		VREVERSE(hitp->hit_normal, norm);
	    }
	    if ((*hit_count)++ >= RT_PIPE_MAXHITS) {
		bu_log("Too many hits (%d) on primitive (%s)\n", *hit_count, stp->st_dp->d_namep);
		return;
	    }
	}
    }
}


/**
 * check if a ray passes within a bounding sphere
 */
int
rt_in_sph(struct xray *rp, point_t center, fastf_t radius_sq)
{
    vect_t toCenter;
    vect_t toPCA;
    fastf_t dist_sq;

    VSUB2(toCenter, center, rp->r_pt);
    VCROSS(toPCA, toCenter, rp->r_dir);
    dist_sq = MAGSQ(toPCA);

    if (dist_sq <= radius_sq) {
	return 1;
    } else {
	return 0;
    }
}


HIDDEN void
bend_pipe_shot(
    struct soltab *stp,
    struct xray *rp,
    struct bend_pipe *bp,
    struct hit *hits,
    int *hit_count,
    int seg_no)
{
    vect_t dprime;		/* D' */
    vect_t pprime;		/* P' */
    vect_t work;		/* temporary vector */
    bn_poly_t C;		/* The final equation */
    bn_complex_t val[4];	/* The complex roots */
    int j;

    int root_count = 0;
    bn_poly_t A, Asqr;
    bn_poly_t X2_Y2;		/* X**2 + Y**2 */
    vect_t cor_pprime;		/* new ray origin */
    fastf_t cor_proj;
    fastf_t or_sq;		/* outside radius squared */
    fastf_t ir_sq;		/* inside radius squared */
    fastf_t or2_sq;		/* outside radius squared (from adjacent seg) */
    fastf_t ir2_sq;		/* inside radius squared (from adjacent seg) */
    int parallel;		/* set to one when ray is parallel to plane of bend */
    fastf_t dist = 0;		/* distance between ray and plane of bend */
    fastf_t tmp;
    struct id_pipe *prev;
    struct id_pipe *next;

    or_sq = bp->bend_or * bp->bend_or;
    ir_sq = bp->bend_ir * bp->bend_ir;

    tmp = VDOT(rp->r_dir, bp->bend_N);
    if (NEAR_ZERO(tmp, 0.0000005)) {
	/* ray is parallel to plane of bend */
	parallel = 1;
	dist = fabs(VDOT(rp->r_pt, bp->bend_N) -
		    VDOT(bp->bend_V, bp->bend_N));

	if (dist > bp->bend_or) {
	    /* ray is more than outer radius away from plane of bend */
	    goto check_discont_radii;
	}
    } else {
	parallel = 0;
    }

    /* Convert vector into the space of the unit torus */
    MAT4X3VEC(dprime, bp->bend_SoR, rp->r_dir);
    VUNITIZE(dprime);

    VSUB2(work, rp->r_pt, bp->bend_V);
    MAT4X3VEC(pprime, bp->bend_SoR, work);

    /* normalize distance from torus.  substitute corrected pprime
     * which contains a translation along ray direction to closest
     * approach to vertex of torus.  Translating ray origin along
     * direction of ray to closest pt. to origin of solid's coordinate
     * system, new ray origin is 'cor_pprime'.
     */
    cor_proj = VDOT(pprime, dprime);
    VSCALE(cor_pprime, dprime, cor_proj);
    VSUB2(cor_pprime, pprime, cor_pprime);

    /*
     * Given a line and a ratio, alpha, finds the equation of the unit
     * torus in terms of the variable 't'.
     *
     * The equation for the torus is:
     *
     * [ X**2 + Y**2 + Z**2 + (1 - alpha**2) ]**2 - 4*(X**2 + Y**2) = 0
     *
     * First, find X, Y, and Z in terms of 't' for this line, then
     * substitute them into the equation above.
     *
     * Wx = Dx*t + Px
     *
     * Wx**2 = Dx**2 * t**2  +  2 * Dx * Px  +  Px**2
     * 	       [0]                 [1]           [2]    dgr=2
     */
    X2_Y2.dgr = 2;
    X2_Y2.cf[0] = dprime[X] * dprime[X] + dprime[Y] * dprime[Y];
    X2_Y2.cf[1] = 2.0 * (dprime[X] * cor_pprime[X] +
			 dprime[Y] * cor_pprime[Y]);
    X2_Y2.cf[2] = cor_pprime[X] * cor_pprime[X] +
	cor_pprime[Y] * cor_pprime[Y];

    /* A = X2_Y2 + Z2 */
    A.dgr = 2;
    A.cf[0] = X2_Y2.cf[0] + dprime[Z] * dprime[Z];
    A.cf[1] = X2_Y2.cf[1] + 2.0 * dprime[Z] * cor_pprime[Z];
    A.cf[2] = X2_Y2.cf[2] + cor_pprime[Z] * cor_pprime[Z] +
	1.0 - bp->bend_alpha_o * bp->bend_alpha_o;

    /* Inline expansion of (void) bn_poly_mul(&Asqr, &A, &A) */
    /* Both polys have degree two */
    Asqr.dgr = 4;
    Asqr.cf[0] = A.cf[0] * A.cf[0];
    Asqr.cf[1] = A.cf[0] * A.cf[1] + A.cf[1] * A.cf[0];
    Asqr.cf[2] = A.cf[0] * A.cf[2] + A.cf[1] * A.cf[1] + A.cf[2] * A.cf[0];
    Asqr.cf[3] = A.cf[1] * A.cf[2] + A.cf[2] * A.cf[1];
    Asqr.cf[4] = A.cf[2] * A.cf[2];

    /* Inline expansion of bn_poly_scale(&X2_Y2, 4.0) and
     * bn_poly_sub(&C, &Asqr, &X2_Y2).
     */
    C.dgr   = 4;
    C.cf[0] = Asqr.cf[0];
    C.cf[1] = Asqr.cf[1];
    C.cf[2] = Asqr.cf[2] - X2_Y2.cf[0] * 4.0;
    C.cf[3] = Asqr.cf[3] - X2_Y2.cf[1] * 4.0;
    C.cf[4] = Asqr.cf[4] - X2_Y2.cf[2] * 4.0;

    /* It is known that the equation is 4th order.  Therefore, if the
     * root finder returns other than 4 roots, error.
     */
    if ((root_count = rt_poly_roots(&C, val, stp->st_dp->d_namep)) != 4) {
	if (root_count > 0) {
	    bu_log("pipe:  rt_poly_roots() 4!=%d\n", root_count);
	    bn_pr_roots(stp->st_name, val, root_count);
	} else if (root_count < 0) {
	    static int reported = 0;
	    bu_log("The root solver failed to converge on a solution for %s\n", stp->st_dp->d_namep);
	    if (!reported) {
		VPRINT("while shooting from:\t", rp->r_pt);
		VPRINT("while shooting at:\t", rp->r_dir);
		bu_log("Additional pipe convergence failure details will be suppressed.\n");
		reported = 1;
	    }
	}
	goto check_discont_radii;	/* MISSED */
    }

    /* Only real roots indicate an intersection in real space.
     *
     * Look at each root returned; if the imaginary part is zero or
     * sufficiently close, then use the real part as one value of 't'
     * for the intersections
     */
    for (j = 0 ; j < 4; j++) {
	if (NEAR_ZERO(val[j].im, 0.0001)) {
	    struct hit *hitp;
	    fastf_t normalized_dist;
	    fastf_t distance;
	    point_t hit_pt;
	    vect_t to_hit;
	    fastf_t angle;

	    normalized_dist = val[j].re - cor_proj;
	    distance = normalized_dist * bp->bend_radius;

	    /* check if this hit is within bend angle */
	    VJOIN1(hit_pt, rp->r_pt, distance, rp->r_dir);
	    VSUB2(to_hit, hit_pt, bp->bend_V);
	    angle = atan2(VDOT(to_hit, bp->bend_rb), VDOT(to_hit, bp->bend_ra));
	    if (angle < 0.0) {
		angle += M_2PI;
	    }
	    if (angle <= bp->bend_angle) {
		hitp = &hits[*hit_count];
		hitp->hit_magic = RT_HIT_MAGIC;
		hitp->hit_dist = distance;
		VJOIN1(hitp->hit_vpriv, pprime, normalized_dist, dprime);
		hitp->hit_surfno = seg_no * 10 + PIPE_BEND_OUTER_BODY;

		if ((*hit_count)++ >= RT_PIPE_MAXHITS) {
		    bu_log("Too many hits (%d) on primitive (%s)\n", *hit_count, stp->st_dp->d_namep);
		    return;
		}
	    }
	}
    }

    if (bp->bend_alpha_i <= 0.0) {
	goto check_discont_radii;    /* no inner torus */
    }

    if (parallel && dist > bp->bend_ir) {
	/* ray is parallel to plane of bend and more than inner radius away */
	goto check_discont_radii;
    }

    /* Now do inner torus */
    A.cf[2] = X2_Y2.cf[2] + cor_pprime[Z] * cor_pprime[Z] +
	1.0 - bp->bend_alpha_i * bp->bend_alpha_i;

    /* Inline expansion of (void) bn_poly_mul(&Asqr, &A, &A) */
    /* Both polys have degree two */
    Asqr.dgr = 4;
    Asqr.cf[0] = A.cf[0] * A.cf[0];
    Asqr.cf[1] = A.cf[0] * A.cf[1] + A.cf[1] * A.cf[0];
    Asqr.cf[2] = A.cf[0] * A.cf[2] + A.cf[1] * A.cf[1] + A.cf[2] * A.cf[0];
    Asqr.cf[3] = A.cf[1] * A.cf[2] + A.cf[2] * A.cf[1];
    Asqr.cf[4] = A.cf[2] * A.cf[2];

    /* Inline expansion of bn_poly_scale(&X2_Y2, 4.0) and
     * bn_poly_sub(&C, &Asqr, &X2_Y2).
     */
    C.dgr   = 4;
    C.cf[0] = Asqr.cf[0];
    C.cf[1] = Asqr.cf[1];
    C.cf[2] = Asqr.cf[2] - X2_Y2.cf[0] * 4.0;
    C.cf[3] = Asqr.cf[3] - X2_Y2.cf[1] * 4.0;
    C.cf[4] = Asqr.cf[4] - X2_Y2.cf[2] * 4.0;

    /* It is known that the equation is 4th order.  Therefore,
     * if the root finder returns other than 4 roots, error.
     */
    if ((root_count = rt_poly_roots(&C, val, stp->st_dp->d_namep)) != 4) {
	if (root_count > 0) {
	    bu_log("tor:  rt_poly_roots() 4!=%d\n", root_count);
	    bn_pr_roots(stp->st_name, val, root_count);
	} else if (root_count < 0) {
	    static int reported = 0;
	    bu_log("The root solver failed to converge on a solution for %s\n", stp->st_dp->d_namep);
	    if (!reported) {
		VPRINT("while shooting from:\t", rp->r_pt);
		VPRINT("while shooting at:\t", rp->r_dir);
		bu_log("Additional pipe convergence failure details will be suppressed.\n");
		reported = 1;
	    }
	}
	goto check_discont_radii;	/* MISSED */
    }

    /* Only real roots indicate an intersection in real space.
     *
     * Look at each root returned; if the imaginary part is zero or
     * sufficiently close, then use the real part as one value of 't'
     * for the intersections
     */
    for (j = 0, root_count = 0; j < 4; j++) {
	if (NEAR_ZERO(val[j].im, 0.0001)) {
	    struct hit *hitp;
	    fastf_t normalized_dist;
	    fastf_t distance;
	    point_t hit_pt;
	    vect_t to_hit;
	    fastf_t angle;

	    normalized_dist = val[j].re - cor_proj;
	    distance = normalized_dist * bp->bend_radius;

	    /* check if this hit is within bend angle */
	    VJOIN1(hit_pt, rp->r_pt, distance, rp->r_dir);
	    VSUB2(to_hit, hit_pt, bp->bend_V);
	    angle = atan2(VDOT(to_hit, bp->bend_rb), VDOT(to_hit, bp->bend_ra));
	    if (angle < 0.0) {
		angle += M_2PI;
	    }
	    if (angle <= bp->bend_angle) {
		hitp = &hits[*hit_count];
		hitp->hit_magic = RT_HIT_MAGIC;
		hitp->hit_dist = distance;
		VJOIN1(hitp->hit_vpriv, pprime, normalized_dist, dprime);
		hitp->hit_surfno = seg_no * 10 + PIPE_BEND_INNER_BODY;

		if ((*hit_count)++ >= RT_PIPE_MAXHITS) {
		    bu_log("Too many hits (%d) on primitive (%s)\n", *hit_count, stp->st_dp->d_namep);
		    return;
		}
	    }
	}
    }


check_discont_radii:
    /* check for surfaces created by discontinuous changes in radii */
    prev = BU_LIST_BACK(id_pipe, &bp->l);
    if (prev->l.magic != BU_LIST_HEAD_MAGIC) {
	if (prev->pipe_is_bend) {
	    /* do not process previous bend
	     * struct bend_pipe *bend = (struct bend_pipe *)prev;
	     * or2_sq = bend->bend_or*bend->bend_or;
	     * ir2_sq = bend->bend_ir*bend->bend_ir; */
	    or2_sq = or_sq;
	    ir2_sq = ir_sq;
	} else {
	    struct lin_pipe *lin = (struct lin_pipe *)prev;
	    or2_sq = lin->pipe_rotop_sq;
	    ir2_sq = lin->pipe_ritop_sq;
	    if (!NEAR_EQUAL(or_sq, or2_sq, RT_LEN_TOL) ||
		!NEAR_EQUAL(ir_sq, ir2_sq, RT_LEN_TOL)) {
		discont_radius_shot(rp, bp->bend_start, bp->bend_startNorm,
				    or_sq, ir_sq, or2_sq, ir2_sq, hits, hit_count, seg_no, stp);
	    }
	}
    }

    next = BU_LIST_NEXT(id_pipe, &bp->l);
    if (next->l.magic != BU_LIST_HEAD_MAGIC) {
	if (next->pipe_is_bend) {
	    struct bend_pipe *bend = (struct bend_pipe *)next;
	    or2_sq = bend->bend_or * bend->bend_or;
	    ir2_sq = bend->bend_ir * bend->bend_ir;
	    if (!NEAR_EQUAL(or_sq, or2_sq, RT_LEN_TOL) ||
		!NEAR_EQUAL(ir_sq, ir2_sq, RT_LEN_TOL)) {
		discont_radius_shot(rp, bp->bend_end, bp->bend_endNorm,
				    or_sq, ir_sq, or2_sq, ir2_sq, hits, hit_count, seg_no, stp);
	    }
	} else {
	    struct lin_pipe *lin = (struct lin_pipe *)next;
	    or2_sq = lin->pipe_robase_sq;
	    ir2_sq = lin->pipe_ribase_sq;
	    if (!NEAR_EQUAL(or_sq, or2_sq, RT_LEN_TOL) ||
		!NEAR_EQUAL(ir_sq, ir2_sq, RT_LEN_TOL)) {
		discont_radius_shot(rp, bp->bend_end, bp->bend_endNorm,
				    or_sq, ir_sq, or2_sq, ir2_sq, hits, hit_count, seg_no, stp);
	    }
	}
    }


    return;

}


HIDDEN void
linear_pipe_shot(
    struct soltab *stp,
    struct xray *rp,
    struct lin_pipe *lp,
    struct hit *hits,
    int *hit_count,
    int seg_no)
{
    struct hit *hitp;
    point_t work_pt;
    point_t ray_start;
    vect_t ray_dir;
    double t_tmp;
    double a, b, c;
    double descrim;

    if (lp->pipe_is_bend) {
	bu_log("linear_pipe_shot called for pipe bend\n");
	bu_bomb("linear_pipe_shot\n");
    }

    /* transform ray start point */
    VSUB2(work_pt, rp->r_pt, lp->pipe_V);
    MAT4X3VEC(ray_start, lp->pipe_SoR, work_pt);

    /* rotate ray direction */
    MAT4X3VEC(ray_dir, lp->pipe_SoR, rp->r_dir);

    /* Intersect with outer sides */
    a = ray_dir[X] * ray_dir[X]
	+ ray_dir[Y] * ray_dir[Y]
	- ray_dir[Z] * ray_dir[Z] * lp->pipe_rodiff_sq;
    b = 2.0 * (ray_start[X] * ray_dir[X]
	       + ray_start[Y] * ray_dir[Y]
	       - ray_start[Z] * ray_dir[Z] * lp->pipe_rodiff_sq
	       - ray_dir[Z] * lp->pipe_robase * lp->pipe_rodiff);
    c = ray_start[X] * ray_start[X]
	+ ray_start[Y] * ray_start[Y]
	- lp->pipe_robase * lp->pipe_robase
	- ray_start[Z] * ray_start[Z] * lp->pipe_rodiff_sq
	- 2.0 * ray_start[Z] * lp->pipe_robase * lp->pipe_rodiff;

    descrim = b * b - 4.0 * a * c;

    if (descrim > 0.0) {
	fastf_t sqrt_descrim;
	point_t hit_pt;

	sqrt_descrim = sqrt(descrim);

	t_tmp = (-b - sqrt_descrim) / (2.0 * a);
	VJOIN1(hit_pt, ray_start, t_tmp, ray_dir);
	if (hit_pt[Z] >= 0.0 && hit_pt[Z] <= 1.0) {
	    hitp = &hits[*hit_count];
	    hitp->hit_magic = RT_HIT_MAGIC;
	    hitp->hit_dist = t_tmp;
	    hitp->hit_surfno = seg_no * 10 + PIPE_LINEAR_OUTER_BODY;
	    VMOVE(hitp->hit_vpriv, hit_pt);
	    hitp->hit_vpriv[Z] = (-lp->pipe_robase - hit_pt[Z] * lp->pipe_rodiff) *
		lp->pipe_rodiff;

	    if ((*hit_count)++ >= RT_PIPE_MAXHITS) {
		bu_log("Too many hits (%d) on primitive (%s)\n", *hit_count, stp->st_dp->d_namep);
		return;
	    }
	}

	t_tmp = (-b + sqrt_descrim) / (2.0 * a);
	VJOIN1(hit_pt, ray_start, t_tmp, ray_dir);
	if (hit_pt[Z] >= 0.0 && hit_pt[Z] <= 1.0) {
	    hitp = &hits[*hit_count];
	    hitp->hit_magic = RT_HIT_MAGIC;
	    hitp->hit_dist = t_tmp;
	    hitp->hit_surfno = seg_no * 10 + PIPE_LINEAR_OUTER_BODY;
	    VMOVE(hitp->hit_vpriv, hit_pt);
	    hitp->hit_vpriv[Z] = (-lp->pipe_robase - hit_pt[Z] * lp->pipe_rodiff) *
		lp->pipe_rodiff;

	    if ((*hit_count)++ >= RT_PIPE_MAXHITS) {
		bu_log("Too many hits (%d) on primitive (%s)\n", *hit_count, stp->st_dp->d_namep);
		return;
	    }
	}
    }

    if (lp->pipe_ribase > 0.0 || lp->pipe_ritop > 0.0) {
	/* Intersect with inner sides */

	a = ray_dir[X] * ray_dir[X]
	    + ray_dir[Y] * ray_dir[Y]
	    - ray_dir[Z] * ray_dir[Z] * lp->pipe_ridiff_sq;
	b = 2.0 * (ray_start[X] * ray_dir[X]
		   + ray_start[Y] * ray_dir[Y]
		   - ray_start[Z] * ray_dir[Z] * lp->pipe_ridiff_sq
		   - ray_dir[Z] * lp->pipe_ribase * lp->pipe_ridiff);
	c = ray_start[X] * ray_start[X]
	    + ray_start[Y] * ray_start[Y]
	    - lp->pipe_ribase * lp->pipe_ribase
	    - ray_start[Z] * ray_start[Z] * lp->pipe_ridiff_sq
	    - 2.0 * ray_start[Z] * lp->pipe_ribase * lp->pipe_ridiff;

	descrim = b * b - 4.0 * a * c;

	if (descrim > 0.0) {
	    fastf_t sqrt_descrim;
	    point_t hit_pt;

	    sqrt_descrim = sqrt(descrim);

	    t_tmp = (-b - sqrt_descrim) / (2.0 * a);
	    VJOIN1(hit_pt, ray_start, t_tmp, ray_dir);
	    if (hit_pt[Z] >= 0.0 && hit_pt[Z] <= 1.0) {
		hitp = &hits[*hit_count];
		hitp->hit_magic = RT_HIT_MAGIC;
		hitp->hit_dist = t_tmp;
		hitp->hit_surfno = seg_no * 10 + PIPE_LINEAR_INNER_BODY;
		VMOVE(hitp->hit_vpriv, hit_pt);
		hitp->hit_vpriv[Z] = (-lp->pipe_ribase - hit_pt[Z] * lp->pipe_ridiff) *
		    lp->pipe_ridiff;

		if ((*hit_count)++ >= RT_PIPE_MAXHITS) {
		    bu_log("Too many hits (%d) on primitive (%s)\n", *hit_count, stp->st_dp->d_namep);
		    return;
		}
	    }

	    t_tmp = (-b + sqrt_descrim) / (2.0 * a);
	    VJOIN1(hit_pt, ray_start, t_tmp, ray_dir);
	    if (hit_pt[Z] >= 0.0 && hit_pt[Z] <= 1.0) {
		hitp = &hits[*hit_count];
		hitp->hit_magic = RT_HIT_MAGIC;
		hitp->hit_dist = t_tmp;
		hitp->hit_surfno = seg_no * 10 + PIPE_LINEAR_INNER_BODY;
		VMOVE(hitp->hit_vpriv, hit_pt);
		hitp->hit_vpriv[Z] = (-lp->pipe_ribase - hit_pt[Z] * lp->pipe_ridiff) *
		    lp->pipe_ridiff;

		if ((*hit_count)++ >= RT_PIPE_MAXHITS) {
		    bu_log("Too many hits (%d) on primitive (%s)\n", *hit_count, stp->st_dp->d_namep);
		    return;
		}
	    }
	}
    }

}


HIDDEN void
pipe_start_shot(
    struct soltab *stp,
    struct xray *rp,
    struct id_pipe *id_p,
    struct hit *hits,
    int *hit_count,
    int seg_no)
{
    point_t hit_pt;
    fastf_t t_tmp;
    fastf_t radius_sq;
    struct hit *hitp;

    if (!id_p->pipe_is_bend) {
	struct lin_pipe *lin = (struct lin_pipe *)(&id_p->l);
	fastf_t dist_to_plane;
	fastf_t norm_dist;
	fastf_t slant_factor;

	dist_to_plane = VDOT(lin->pipe_H, lin->pipe_V);
	norm_dist = dist_to_plane - VDOT(lin->pipe_H, rp->r_pt);
	slant_factor = VDOT(lin->pipe_H, rp->r_dir);
	if (!ZERO(slant_factor)) {
	    vect_t to_center;

	    t_tmp = norm_dist / slant_factor;
	    VJOIN1(hit_pt, rp->r_pt, t_tmp, rp->r_dir);
	    VSUB2(to_center, lin->pipe_V, hit_pt);
	    radius_sq = MAGSQ(to_center);
	    if (radius_sq <= lin->pipe_robase_sq && radius_sq >= lin->pipe_ribase_sq) {
		hitp = &hits[*hit_count];
		hitp->hit_magic = RT_HIT_MAGIC;
		hitp->hit_dist = t_tmp;
		hitp->hit_surfno = seg_no * 10 + PIPE_LINEAR_BASE;

		if ((*hit_count)++ >= RT_PIPE_MAXHITS) {
		    bu_log("Too many hits (%d) on primitive (%s)\n", *hit_count, stp->st_dp->d_namep);
		    return;
		}
	    }
	}
    } else if (id_p->pipe_is_bend) {
	struct bend_pipe *bend = (struct bend_pipe *)(&id_p->l);
	fastf_t dist_to_plane;
	fastf_t norm_dist;
	fastf_t slant_factor;

	dist_to_plane = VDOT(bend->bend_rb, bend->bend_start);
	norm_dist = dist_to_plane - VDOT(bend->bend_rb, rp->r_pt);
	slant_factor = VDOT(bend->bend_rb, rp->r_dir);

	if (!ZERO(slant_factor)) {
	    vect_t to_center;

	    t_tmp = norm_dist / slant_factor;
	    VJOIN1(hit_pt, rp->r_pt, t_tmp, rp->r_dir);
	    VSUB2(to_center, bend->bend_start, hit_pt);
	    radius_sq = MAGSQ(to_center);
	    if (radius_sq <= bend->bend_or * bend->bend_or && radius_sq >= bend->bend_ir * bend->bend_ir) {
		hitp = &hits[*hit_count];
		hitp->hit_magic = RT_HIT_MAGIC;
		hitp->hit_dist = t_tmp;
		hitp->hit_surfno = seg_no * 10 + PIPE_BEND_BASE;

		if ((*hit_count)++ >= RT_PIPE_MAXHITS) {
		    bu_log("Too many hits (%d) on primitive (%s)\n", *hit_count, stp->st_dp->d_namep);
		    return;
		}
	    }
	}
    }
}


HIDDEN void
pipe_end_shot(
    struct soltab *stp,
    struct xray *rp,
    struct id_pipe *id_p,
    struct hit *hits,
    int *hit_count,
    int seg_no)
{
    point_t hit_pt;
    fastf_t t_tmp;
    fastf_t radius_sq;
    struct hit *hitp;

    if (!id_p->pipe_is_bend) {
	struct lin_pipe *lin = (struct lin_pipe *)(&id_p->l);
	point_t top;
	fastf_t dist_to_plane;
	fastf_t norm_dist;
	fastf_t slant_factor;

	VJOIN1(top, lin->pipe_V, lin->pipe_len, lin->pipe_H);
	dist_to_plane = VDOT(lin->pipe_H, top);
	norm_dist = dist_to_plane - VDOT(lin->pipe_H, rp->r_pt);
	slant_factor = VDOT(lin->pipe_H, rp->r_dir);
	if (!ZERO(slant_factor)) {
	    vect_t to_center;

	    t_tmp = norm_dist / slant_factor;
	    VJOIN1(hit_pt, rp->r_pt, t_tmp, rp->r_dir);
	    VSUB2(to_center, top, hit_pt);
	    radius_sq = MAGSQ(to_center);
	    if (radius_sq <= lin->pipe_rotop_sq && radius_sq >= lin->pipe_ritop_sq) {
		hitp = &hits[*hit_count];
		hitp->hit_magic = RT_HIT_MAGIC;
		hitp->hit_dist = t_tmp;
		hitp->hit_surfno = seg_no * 10 + PIPE_LINEAR_TOP;

		if ((*hit_count)++ >= RT_PIPE_MAXHITS) {
		    bu_log("Too many hits (%d) on primitive (%s)\n", *hit_count, stp->st_dp->d_namep);
		    return;
		}
	    }
	}
    } else if (id_p->pipe_is_bend) {
	struct bend_pipe *bend = (struct bend_pipe *)(&id_p->l);
	vect_t to_end;
	vect_t plane_norm;
	fastf_t dist_to_plane;
	fastf_t norm_dist;
	fastf_t slant_factor;

	VSUB2(to_end, bend->bend_end, bend->bend_V);
	VCROSS(plane_norm, to_end, bend->bend_N);
	VUNITIZE(plane_norm);

	dist_to_plane = VDOT(plane_norm, bend->bend_end);
	norm_dist = dist_to_plane - VDOT(plane_norm, rp->r_pt);
	slant_factor = VDOT(plane_norm, rp->r_dir);

	if (!ZERO(slant_factor)) {
	    vect_t to_center;

	    t_tmp = norm_dist / slant_factor;
	    VJOIN1(hit_pt, rp->r_pt, t_tmp, rp->r_dir);
	    VSUB2(to_center, bend->bend_end, hit_pt);
	    radius_sq = MAGSQ(to_center);
	    if (radius_sq <= bend->bend_or * bend->bend_or && radius_sq >= bend->bend_ir * bend->bend_ir) {
		hitp = &hits[*hit_count];
		hitp->hit_magic = RT_HIT_MAGIC;
		hitp->hit_dist = t_tmp;
		hitp->hit_surfno = seg_no * 10 + PIPE_BEND_TOP;

		if ((*hit_count)++ >= RT_PIPE_MAXHITS) {
		    bu_log("Too many hits (%d) on primitive (%s)\n", *hit_count, stp->st_dp->d_namep);
		    return;
		}
	    }
	}
    }
}


HIDDEN void
rt_pipe_elim_dups(
    struct hit *hit,
    int *nh,
    struct xray *rp,
    struct soltab *stp)
{
    struct hit *hitp;
    struct hit *next_hit;
    int hitNo = 0;

    /* delete duplicate hits */
    while (hitNo < ((*nh) - 1)) {
	hitp = &hit[hitNo];
	next_hit = &hit[hitNo + 1];

	if (NEAR_EQUAL(hitp->hit_dist, next_hit->hit_dist, stp->st_rtip->rti_tol.dist) &&
	    hitp->hit_surfno == next_hit->hit_surfno) {
	    int i;
	    for (i = hitNo ; i < (*nh-2) ; i++) {
		hit[i] = hit[i + 2];
	    }
	    (*nh)=(*nh)-2;
	} else {
	    hitNo++;
	}
    }

    if ((*nh) == 1) {
	(*nh) = 0;
	return;
    }

    if ((*nh) == 0 || (*nh) == 2) {
	return;
    }

    hitNo = 0;
    while (hitNo < ((*nh) - 1)) {
	int hitNoPlus = hitNo + 1;
	/* struct hit *first = &hit[hitNo]; */
	struct hit *second = &hit[hitNoPlus];

	/* keep first entrance hit, eliminate all successive entrance hits */
	while (hitNoPlus < (*nh) && VDOT(second->hit_normal, rp->r_dir) < 0.0) {
	    int j;
	    for (j = hitNoPlus ; j < ((*nh) - 1) ; j++) {
		hit[j] = hit[j + 1];
	    }
	    (*nh)--;
	    second = &hit[hitNoPlus];
	}

	/* second is now an exit hit at hit[hitNoPlus] */

	/* move to next hit */
	hitNoPlus++;
	if (hitNoPlus >= (*nh)) {
	    break;
	}

	/* set second to the next hit */
	second = &hit[hitNoPlus];

	/* eliminate all exit hits (except the last one) till we find another entrance hit */
	while (hitNoPlus < (*nh) && VDOT(second->hit_normal, rp->r_dir) > 0.0) {
	    int j;
	    for (j = hitNoPlus - 1 ; j < ((*nh) - 1) ; j++) {
		hit[j] = hit[j + 1];
	    }
	    (*nh)--;
	    second = &hit[hitNoPlus];
	}
	hitNo = hitNoPlus;
    }
}


/**
 * Given ONE ray distance, return the normal and entry/exit point.
 */
void
rt_pipe_norm(struct hit *hitp, struct soltab *stp, struct xray *rp)
{
    struct bu_list *head = (struct bu_list *)stp->st_specific;
    struct id_pipe *pipe_id;
    struct lin_pipe *pipe_lin;
    struct bend_pipe *pipe_bend;
    fastf_t w;
    vect_t work;
    vect_t work1;
    int segno;
    int i;

    segno = hitp->hit_surfno / 10;

    pipe_id = BU_LIST_FIRST(id_pipe, head);
    for (i = 1; i < segno; i++) {
	pipe_id = BU_LIST_NEXT(id_pipe, &pipe_id->l);
    }

    pipe_lin = (struct lin_pipe *)pipe_id;
    pipe_bend = (struct bend_pipe *)pipe_id;

    VJOIN1(hitp->hit_point, rp->r_pt, hitp->hit_dist, rp->r_dir);
    switch (hitp->hit_surfno % 10) {
	case PIPE_LINEAR_TOP:
	    VMOVE(hitp->hit_normal, pipe_lin->pipe_H);
	    break;
	case PIPE_LINEAR_BASE:
	    VREVERSE(hitp->hit_normal, pipe_lin->pipe_H);
	    break;
	case PIPE_LINEAR_OUTER_BODY:
	    MAT4X3VEC(hitp->hit_normal, pipe_lin->pipe_invRoS, hitp->hit_vpriv);
	    VUNITIZE(hitp->hit_normal);
	    break;
	case PIPE_LINEAR_INNER_BODY:
	    MAT4X3VEC(hitp->hit_normal, pipe_lin->pipe_invRoS, hitp->hit_vpriv);
	    VUNITIZE(hitp->hit_normal);
	    VREVERSE(hitp->hit_normal, hitp->hit_normal);
	    break;
	case PIPE_BEND_OUTER_BODY:
	    w = hitp->hit_vpriv[X] * hitp->hit_vpriv[X] +
		hitp->hit_vpriv[Y] * hitp->hit_vpriv[Y] +
		hitp->hit_vpriv[Z] * hitp->hit_vpriv[Z] +
		1.0 - pipe_bend->bend_alpha_o * pipe_bend->bend_alpha_o;
	    VSET(work,
		 (w - 2.0) * hitp->hit_vpriv[X],
		 (w - 2.0) * hitp->hit_vpriv[Y],
		 w * hitp->hit_vpriv[Z]);
	    VUNITIZE(work);
	    MAT3X3VEC(hitp->hit_normal, pipe_bend->bend_invR, work);
	    break;
	case PIPE_BEND_INNER_BODY:
	    w = hitp->hit_vpriv[X] * hitp->hit_vpriv[X] +
		hitp->hit_vpriv[Y] * hitp->hit_vpriv[Y] +
		hitp->hit_vpriv[Z] * hitp->hit_vpriv[Z] +
		1.0 - pipe_bend->bend_alpha_i * pipe_bend->bend_alpha_i;
	    VSET(work,
		 (w - 2.0) * hitp->hit_vpriv[X],
		 (w - 2.0) * hitp->hit_vpriv[Y],
		 w * hitp->hit_vpriv[Z]);
	    VUNITIZE(work);
	    MAT3X3VEC(work1, pipe_bend->bend_invR, work);
	    VREVERSE(hitp->hit_normal, work1);
	    break;
	case PIPE_BEND_BASE:
	    VREVERSE(hitp->hit_normal, pipe_bend->bend_rb);
	    break;
	case PIPE_BEND_TOP:
	    VSUB2(work, pipe_bend->bend_end, pipe_bend->bend_V);
	    VCROSS(hitp->hit_normal, pipe_bend->bend_N, work);
	    VUNITIZE(hitp->hit_normal);
	    break;
	case PIPE_RADIUS_CHANGE:
	    break; /* already have normal */
	default:
	    bu_log("rt_pipe_norm: Unrecognized surfno (%d)\n", hitp->hit_surfno);
	    break;
    }
}


/**
 * Intersect a ray with a pipe.  If an intersection occurs, a struct
 * seg will be acquired and filled in.
 *
 * Returns -
 *  0 MISS
 * >0 HIT
 */
int
rt_pipe_shot(
    struct soltab *stp,
    struct xray *rp,
    struct application *ap,
    struct seg *seghead)
{
    struct bu_list *head = (struct bu_list *)stp->st_specific;
    struct id_pipe *pipe_id;
    struct seg *segp;
    struct hit hits[RT_PIPE_MAXHITS];
    int total_hits = 0;
    int seg_no;
    int i;

    pipe_start_shot(stp, rp, BU_LIST_FIRST(id_pipe, head), hits, &total_hits, 1);
    seg_no = 0;
    for (BU_LIST_FOR(pipe_id, id_pipe, head)) {
	seg_no++;
    }
    pipe_end_shot(stp, rp, BU_LIST_LAST(id_pipe, head), hits, &total_hits, seg_no);

    seg_no = 0;
    for (BU_LIST_FOR(pipe_id, id_pipe, head)) {
	seg_no++;

	if (!pipe_id->pipe_is_bend) {
	    struct lin_pipe *lin = (struct lin_pipe *)pipe_id;
	    if (!rt_in_rpp(rp, ap->a_inv_dir, lin->pipe_min, lin->pipe_max)) {
		continue;
	    }
	    linear_pipe_shot(stp, rp, lin, hits, &total_hits, seg_no);
	} else {
	    struct bend_pipe *bend = (struct bend_pipe *)pipe_id;
	    if (!rt_in_sph(rp, bend->bend_bound_center, bend->bend_bound_radius_sq)) {
		continue;
	    }
	    bend_pipe_shot(stp, rp, bend, hits, &total_hits, seg_no);
	}
    }
    if (!total_hits) {
	return 0;
    }

    /* calculate hit points and normals */
    for (i = 0 ; i < total_hits ; i++) {
	rt_pipe_norm(&hits[i], stp, rp);
    }

    /* sort the hits */
    rt_hitsort(hits, total_hits);

    /* eliminate duplicate hits */
    rt_pipe_elim_dups(hits, &total_hits, rp, stp);

    /* Build segments */
    if (total_hits % 2) {
	bu_log("rt_pipe_shot: bad number of hits on solid %s (%d)\n", stp->st_dp->d_namep, total_hits);
	bu_log("Ignoring this solid for this ray\n");
	bu_log("\tray start = (%e %e %e), ray dir = (%e %e %e)\n", V3ARGS(rp->r_pt), V3ARGS(rp->r_dir));
	for (i = 0 ; i < total_hits ; i++) {
	    point_t hit_pt;

	    bu_log("#%d, dist = %g, surfno=%d\n", i, hits[i].hit_dist, hits[i].hit_surfno);
	    VJOIN1(hit_pt, rp->r_pt, hits[i].hit_dist,  rp->r_dir);
	    bu_log("\t(%g %g %g)\n", V3ARGS(hit_pt));
	}

	return 0;
    }

    for (i = 0 ; i < total_hits ; i += 2) {
	RT_GET_SEG(segp, ap->a_resource);

	segp->seg_stp = stp;
	segp->seg_in = hits[i];
	segp->seg_out = hits[i + 1];

	BU_LIST_INSERT(&(seghead->l), &(segp->l));
    }


    if (total_hits) {
	return 1;    /* HIT */
    } else {
	return 0;    /* MISS */
    }
}


/**
 * Return the curvature of the pipe.
 */
void
rt_pipe_curve(struct curvature *cvp, struct hit *hitp, struct soltab *stp)
{
    if (!cvp || !hitp) {
	return;
    }
    if (stp) {
	RT_CK_SOLTAB(stp);
    }

    cvp->crv_c1 = cvp->crv_c2 = 0;

    /* any tangent direction */
    bn_vec_ortho(cvp->crv_pdir, hitp->hit_normal);
}


/**
 * For a hit on the surface of an pipe, return the (u, v) coordinates
 * of the hit point, 0 <= u, v <= 1.
 * u = azimuth
 * v = elevation
 */
void
rt_pipe_uv(
    struct application *ap,
    struct soltab *stp,
    struct hit *hitp,
    struct uvcoord *uvp)
{
    if (!ap || !stp || !hitp || !uvp) {
	return;
    }
}


void
rt_pipe_free(struct soltab *stp)
{
    if (stp != NULL) {
	pipe_elements_free((struct bu_list *)stp->st_specific);
	BU_PUT(stp->st_specific, struct bu_list);
    }
}


static int
pipe_circle_segments(const struct rt_view_info *info, fastf_t radius)
{
    int num_segments;

    num_segments = M_2PI * radius / info->point_spacing;

    if (num_segments < 5) {
	num_segments = 5;
    }

    return num_segments;
}


static int
pipe_bend_segments(
    const struct rt_view_info *info,
    const struct pipe_bend *bend)
{
    int num_segments;
    fastf_t arc_length;

    arc_length = (bend->bend_angle * M_1_2PI) * bend->bend_circle.radius;
    num_segments = arc_length / info->point_spacing;

    if (num_segments < 3) {
	num_segments = 3;
    }

    return num_segments;
}


static int
pipe_connecting_arcs(
    struct rt_pipe_internal *pipe,
    const struct rt_view_info *info)
{
    int dcount, num_arcs;
    struct pipe_segment *cur_seg;
    fastf_t avg_diameter, avg_circumference;

    RT_PIPE_CK_MAGIC(pipe);
    BU_CK_LIST_HEAD(info->vhead);

    cur_seg = pipe_seg_first(pipe);
    if (cur_seg == NULL) {
	return 0;
    }

    dcount = 0;
    avg_diameter = 0.0;
    while (!pipe_seg_is_last(cur_seg)) {
	avg_diameter += cur_seg->cur->pp_od;
	++dcount;

	if (cur_seg->cur->pp_id > 0.0) {
	    avg_diameter += cur_seg->cur->pp_id;
	    ++dcount;
	}

	pipe_seg_advance(cur_seg);
    }

    avg_diameter += cur_seg->cur->pp_od;
    ++dcount;

    if (cur_seg->cur->pp_id > 0.0) {
	avg_diameter += cur_seg->cur->pp_id;
	++dcount;
    }

    avg_diameter /= (double)dcount;

    BU_PUT(cur_seg, struct pipe_segment);

    avg_circumference = M_PI * avg_diameter;
    num_arcs = avg_circumference / info->curve_spacing;

    if (num_arcs < 4) {
	num_arcs = 4;
    }

    return num_arcs;
}


/**
 * Draw a pipe circle using a given number of segments.
 */
static void
draw_pipe_circle(
    struct bu_list *vhead,
    const struct pipe_circle *circle,
    int num_segments)
{
    vect_t axis_a, axis_b;

    VSCALE(axis_a, circle->orient.v1, circle->radius);
    VSCALE(axis_b, circle->orient.v2, circle->radius);

    plot_ellipse(vhead, circle->center, axis_a, axis_b, num_segments);
}


/**
 * Draws the specified number of connecting lines between the start and end
 * circles, which are expected to be parallel (i.e. have the same orientation).
 */
static void
draw_pipe_parallel_circle_connections(
    struct bu_list *vhead,
    const struct pipe_circle *start,
    const struct pipe_circle *end,
    int num_lines)
{
    int i;
    point_t pt;
    fastf_t radian, radian_step;
    vect_t start_a, start_b, end_a, end_b;

    BU_CK_LIST_HEAD(vhead);

    radian_step = M_2PI / num_lines;

    VSCALE(start_a, start->orient.v1, start->radius);
    VSCALE(start_b, start->orient.v2, start->radius);
    VSCALE(end_a, end->orient.v1, end->radius);
    VSCALE(end_b, end->orient.v2, end->radius);

    radian = 0.0;
    for (i = 0; i < num_lines; ++i) {
	ellipse_point_at_radian(pt, start->center, start_a, start_b, radian);
	RT_ADD_VLIST(vhead, pt, BN_VLIST_LINE_MOVE);

	ellipse_point_at_radian(pt, end->center, end_a, end_b, radian);
	RT_ADD_VLIST(vhead, pt, BN_VLIST_LINE_DRAW);

	radian += radian_step;
    }
}


static void
draw_pipe_connect_points_linearly(
    struct bu_list *vhead,
    const struct wdb_pipept *startpt,
    const struct wdb_pipept *endpt,
    const struct pipe_orientation *orient,
    int num_connections)
{
    struct pipe_circle start_circle, end_circle;

    start_circle.orient = end_circle.orient = *orient;	/* struct copy */

    VMOVE(start_circle.center, startpt->pp_coord);
    VMOVE(end_circle.center, endpt->pp_coord);

    /* connect outer circles */
    start_circle.radius = startpt->pp_od / 2.0;
    end_circle.radius = endpt->pp_od / 2.0;
    draw_pipe_parallel_circle_connections(vhead, &start_circle, &end_circle,
					  num_connections);

    /* connect inner circles */
    if (startpt->pp_id > 0.0 && endpt->pp_id > 0.0) {
	start_circle.radius = startpt->pp_id / 2.0;
	end_circle.radius = endpt->pp_id / 2.0;
	draw_pipe_parallel_circle_connections(vhead, &start_circle, &end_circle,
					      num_connections);
    }
}


static void
draw_pipe_linear_seg(
    struct bu_list *vhead,
    struct pipe_segment *seg)
{
    struct wdb_pipept startpt, endpt;

    endpt = *seg->cur;
    startpt = *BU_LIST_PREV(wdb_pipept, &seg->cur->l);
    VMOVE(startpt.pp_coord, seg->last_drawn);

    draw_pipe_connect_points_linearly(vhead, &startpt, &endpt, &seg->orient,
				      seg->connecting_arcs);

    VMOVE(seg->last_drawn, endpt.pp_coord);
}


/**
 * Using the specified number of segments, draw the shortest arc on the given
 * circle which starts at (center + radius * v1) and ends at (arc_end).
 */
HIDDEN void
draw_pipe_arc(
    struct bu_list *vhead,
    struct pipe_circle arc_circle,
    const point_t arc_end,
    int num_segments)
{
    int i;
    point_t pt;
    vect_t center_to_start, center_to_end, axis_a, axis_b;
    fastf_t radians_from_start_to_end, radian, radian_step;

    BU_CK_LIST_HEAD(vhead);

    VSCALE(axis_a, arc_circle.orient.v1, arc_circle.radius);
    VSCALE(axis_b, arc_circle.orient.v2, arc_circle.radius);
    VMOVE(center_to_start, arc_circle.orient.v1);
    VSUB2(center_to_end, arc_end, arc_circle.center);
    VUNITIZE(center_to_end);

    radians_from_start_to_end = acos(VDOT(center_to_start, center_to_end));
    radian_step = radians_from_start_to_end / num_segments;

    ellipse_point_at_radian(pt, arc_circle.center, axis_a, axis_b, 0.0);
    RT_ADD_VLIST(vhead, pt, BN_VLIST_LINE_MOVE);

    radian = radian_step;
    for (i = 0; i < num_segments; ++i) {
	ellipse_point_at_radian(pt, arc_circle.center, axis_a, axis_b, radian);
	RT_ADD_VLIST(vhead, pt, BN_VLIST_LINE_DRAW);

	radian += radian_step;
    }
}


static void
pipe_seg_bend(struct pipe_bend *out_bend, const struct pipe_segment *seg)
{
    fastf_t dist_to_bend_end;
    vect_t cur_to_prev, cur_to_next;
    struct wdb_pipept *prevpt, *curpt, *nextpt;

    out_bend->pipe_circle.orient = seg->orient;
    out_bend->bend_angle = pipe_seg_bend_angle(seg);
    pipe_seg_bend_normal(out_bend->bend_normal, seg);

    curpt = seg->cur;
    prevpt = BU_LIST_PREV(wdb_pipept, &curpt->l);
    nextpt = BU_LIST_NEXT(wdb_pipept, &curpt->l);

    VSUB2(cur_to_prev, prevpt->pp_coord, curpt->pp_coord);
    VSUB2(cur_to_next, nextpt->pp_coord, curpt->pp_coord);
    VUNITIZE(cur_to_prev);
    VUNITIZE(cur_to_next);

    dist_to_bend_end = pipe_seg_dist_to_bend_endpoint(seg);
    VJOIN1(out_bend->bend_start, curpt->pp_coord, dist_to_bend_end, cur_to_prev);
    VJOIN1(out_bend->bend_end, curpt->pp_coord, dist_to_bend_end, cur_to_next);

    out_bend->bend_circle.radius = curpt->pp_bendradius;
    VCROSS(out_bend->bend_circle.orient.v1, cur_to_prev, out_bend->bend_normal);
    VCROSS(out_bend->bend_circle.orient.v2, out_bend->bend_circle.orient.v1,
	   out_bend->bend_normal);
    VJOIN1(out_bend->bend_circle.center, out_bend->bend_start, -out_bend->bend_circle.radius,
	   out_bend->bend_circle.orient.v1);
}


static struct pipe_orientation
draw_pipe_connect_circular_segs(
    struct bu_list *vhead,
    const struct pipe_bend *bend,
    int num_arcs,
    int segs_per_arc)
{
    int i;
    mat_t rot_mat;
    struct pipe_circle arc_circle;
    struct pipe_orientation end_orient;
    point_t bend_center, bend_start, bend_end, pipe_pt, arc_start, arc_end;
    vect_t bend_norm, center_to_start;
    vect_t pipe_axis_a, pipe_axis_b, pipe_r, end_pipe_r;
    fastf_t pipe_radius, bend_circle_offset, radian_step, radian;

    BU_CK_LIST_HEAD(vhead);

    /* short names */
    VMOVE(bend_center, bend->bend_circle.center);
    VMOVE(bend_norm, bend->bend_normal);
    VMOVE(bend_start, bend->bend_start);
    VMOVE(bend_end, bend->bend_end);

    pipe_radius = bend->pipe_circle.radius;
    VSCALE(pipe_axis_a, bend->pipe_circle.orient.v1, pipe_radius);
    VSCALE(pipe_axis_b, bend->pipe_circle.orient.v2, pipe_radius);

    /* calculate matrix to rotate vectors around the bend */
    {
	vect_t reverse_norm;
	VREVERSE(reverse_norm, bend_norm);
	bn_mat_arb_rot(rot_mat, bend_center, reverse_norm, bend->bend_angle);
    }

    arc_circle.orient = bend->bend_circle.orient;

    radian_step = M_2PI / num_arcs;
    radian = 0.0;
    for (i = 0; i < num_arcs; ++i) {
	/* get a vector from the pipe center (bend start) to a point on the
	 * pipe circle
	 */
	ellipse_point_at_radian(pipe_pt, bend_start, pipe_axis_a, pipe_axis_b,
				radian);
	VSUB2(pipe_r, pipe_pt, bend_start);

	/* Project the pipe vector onto the bend circle normal to get an
	 * offset from the bend circle plane. Move that distance along the
	 * bend circle normal to find the center of the arc circle.
	 */
	bend_circle_offset = VDOT(pipe_r, bend_norm);
	VJOIN1(arc_circle.center, bend_center, bend_circle_offset, bend_norm);

	/* rotate the vector around the bend to its final position */
	MAT4X3VEC(end_pipe_r, rot_mat, pipe_r);

	/* draw an arc between the start and end positions of the pipe vector */
	VADD2(arc_start, bend_start, pipe_r);
	VADD2(arc_end, bend_end, end_pipe_r);
	VSUB2(center_to_start, arc_start, arc_circle.center);
	arc_circle.radius = MAGNITUDE(center_to_start);
	draw_pipe_arc(vhead, arc_circle, arc_end, segs_per_arc);

	radian += radian_step;
    }

    /* return the final orientation of the pipe circle */
    MAT4X3VEC(end_pipe_r, rot_mat, bend->pipe_circle.orient.v1);
    VMOVE(end_orient.v1, end_pipe_r);

    MAT4X3VEC(end_pipe_r, rot_mat, bend->pipe_circle.orient.v2);
    VMOVE(end_orient.v2, end_pipe_r);

    return end_orient;
}


static void
draw_pipe_circular_seg(struct bu_list *vhead, struct pipe_segment *seg)
{
    struct pipe_bend bend;
    struct wdb_pipept *prevpt, *curpt, startpt, endpt;

    pipe_seg_bend(&bend, seg);

    curpt = seg->cur;
    prevpt = BU_LIST_PREV(wdb_pipept, &curpt->l);

    /* draw linear segment to start of bend */
    startpt = *prevpt;
    endpt = *curpt;
    VMOVE(startpt.pp_coord, seg->last_drawn);
    VMOVE(endpt.pp_coord, bend.bend_start);

    draw_pipe_connect_points_linearly(vhead, &startpt, &endpt, &seg->orient,
				      PIPE_CONNECTING_ARCS);

    VMOVE(seg->last_drawn, bend.bend_start);

    /* draw circular bend */
    bend.pipe_circle.radius = curpt->pp_od / 2.0;
    seg->orient = draw_pipe_connect_circular_segs(vhead, &bend,
						  PIPE_CONNECTING_ARCS, PIPE_CIRCLE_SEGS);

    if (prevpt->pp_id > 0.0 && curpt->pp_id > 0.0) {
	bend.pipe_circle.radius = curpt->pp_id / 2.0;
	seg->orient = draw_pipe_connect_circular_segs(vhead, &bend,
						      PIPE_CONNECTING_ARCS, PIPE_CIRCLE_SEGS);
    }

    VMOVE(seg->last_drawn, bend.bend_end);
}


static void
draw_pipe_circular_seg_adaptive(
    struct bu_list *vhead,
    struct pipe_segment *seg,
    const struct rt_view_info *info)
{
    int num_segments;
    struct pipe_bend bend;
    struct wdb_pipept *prevpt, *curpt, startpt, endpt;

    pipe_seg_bend(&bend, seg);

    curpt = seg->cur;
    prevpt = BU_LIST_PREV(wdb_pipept, &curpt->l);

    /* draw linear segment to start of bend */
    startpt = *prevpt;
    endpt = *curpt;
    VMOVE(startpt.pp_coord, seg->last_drawn);
    VMOVE(endpt.pp_coord, bend.bend_start);

    draw_pipe_connect_points_linearly(vhead, &startpt, &endpt, &seg->orient,
				      seg->connecting_arcs);

    VMOVE(seg->last_drawn, bend.bend_start);

    /* draw circular bend */
    bend.pipe_circle.radius = curpt->pp_od / 2.0;
    num_segments = pipe_bend_segments(info, &bend);
    seg->orient = draw_pipe_connect_circular_segs(vhead, &bend,
						  seg->connecting_arcs, num_segments);

    if (prevpt->pp_id > 0.0 && curpt->pp_id > 0.0) {
	bend.pipe_circle.radius = curpt->pp_id / 2.0;
	num_segments = pipe_bend_segments(info, &bend);
	seg->orient = draw_pipe_connect_circular_segs(vhead, &bend,
						      seg->connecting_arcs, num_segments);
    }

    VMOVE(seg->last_drawn, bend.bend_end);
}


static void
draw_pipe_end(struct bu_list *vhead, struct pipe_segment *seg)
{
    struct wdb_pipept *endpt;
    struct pipe_circle pipe_circle;

    endpt = seg->cur;

    VMOVE(pipe_circle.center, endpt->pp_coord);
    pipe_circle.orient = seg->orient;

    /* draw outer circle */
    pipe_circle.radius = endpt->pp_od / 2.0;
    draw_pipe_circle(vhead, &pipe_circle, PIPE_CIRCLE_SEGS);

    /* draw inner circle */
    if (endpt->pp_id > 0.0) {
	pipe_circle.radius = endpt->pp_id / 2.0;
	draw_pipe_circle(vhead, &pipe_circle, PIPE_CIRCLE_SEGS);
    }

    VMOVE(seg->last_drawn, endpt->pp_coord);
}


static void
draw_pipe_end_adaptive(
    struct bu_list *vhead,
    struct pipe_segment *seg,
    const struct rt_view_info *info)
{
    int num_segments;
    struct wdb_pipept *endpt;
    struct pipe_circle pipe_circle;

    endpt = seg->cur;

    VMOVE(pipe_circle.center, endpt->pp_coord);
    pipe_circle.orient = seg->orient;

    /* draw outer circle */
    pipe_circle.radius = endpt->pp_od / 2.0;
    num_segments = pipe_circle_segments(info, pipe_circle.radius);
    draw_pipe_circle(vhead, &pipe_circle, num_segments);

    /* draw inner circle */
    if (endpt->pp_id > 0.0) {
	pipe_circle.radius = endpt->pp_id / 2.0;
	num_segments = pipe_circle_segments(info, pipe_circle.radius);
	draw_pipe_circle(vhead, &pipe_circle, num_segments);
    }

    VMOVE(seg->last_drawn, endpt->pp_coord);
}


int
rt_pipe_adaptive_plot(
    struct rt_db_internal *ip,
    const struct rt_view_info *info)
{
    struct rt_pipe_internal *pipe;
    struct pipe_segment *cur_seg;

    BU_CK_LIST_HEAD(info->vhead);
    RT_CK_DB_INTERNAL(ip);
    pipe = (struct rt_pipe_internal *)ip->idb_ptr;
    RT_PIPE_CK_MAGIC(pipe);

    cur_seg = pipe_seg_first(pipe);
    if (cur_seg == NULL) {
	return 0;
    }

    cur_seg->connecting_arcs = pipe_connecting_arcs(pipe, info);

    draw_pipe_end_adaptive(info->vhead, cur_seg, info);
    pipe_seg_advance(cur_seg);

    while (!pipe_seg_is_last(cur_seg)) {
	if (pipe_seg_is_bend(cur_seg)) {
	    draw_pipe_circular_seg_adaptive(info->vhead, cur_seg, info);
	} else {
	    draw_pipe_linear_seg(info->vhead, cur_seg);
	}

	pipe_seg_advance(cur_seg);
    }

    draw_pipe_linear_seg(info->vhead, cur_seg);
    draw_pipe_end_adaptive(info->vhead, cur_seg, info);

    BU_PUT(cur_seg, struct pipe_segment);

    return 0;
}


int
rt_pipe_plot(
    struct bu_list *vhead,
    struct rt_db_internal *ip,
    const struct rt_tess_tol *UNUSED(ttol),
    const struct bn_tol *UNUSED(tol),
    const struct rt_view_info *UNUSED(info))
{
    struct rt_pipe_internal *pip;
    struct pipe_segment *cur_seg;

    BU_CK_LIST_HEAD(vhead);
    RT_CK_DB_INTERNAL(ip);
    pip = (struct rt_pipe_internal *)ip->idb_ptr;
    RT_PIPE_CK_MAGIC(pip);

    cur_seg = pipe_seg_first(pip);
    if (cur_seg == NULL) {
	return 0;
    }

    cur_seg->connecting_arcs = PIPE_CONNECTING_ARCS;

    draw_pipe_end(vhead, cur_seg);
    pipe_seg_advance(cur_seg);

    while (!pipe_seg_is_last(cur_seg)) {
	if (pipe_seg_is_bend(cur_seg)) {
	    draw_pipe_circular_seg(vhead, cur_seg);
	} else {
	    draw_pipe_linear_seg(vhead, cur_seg);
	}

	pipe_seg_advance(cur_seg);
    }

    draw_pipe_linear_seg(vhead, cur_seg);
    draw_pipe_end(vhead, cur_seg);

    BU_PUT(cur_seg, struct pipe_segment);

    return 0;
}


HIDDEN void
tesselate_pipe_start(
    struct wdb_pipept *pipept,
    int arc_segs,
    double sin_del,
    double cos_del,
    struct vertex ***outer_loop,
    struct vertex ***inner_loop,
    fastf_t *r1,
    fastf_t *r2,
    struct shell *s,
    const struct bn_tol *tol)
{
    struct faceuse *fu;
    struct loopuse *lu;
    struct edgeuse *eu;
    struct wdb_pipept *next;
    point_t pt;
    fastf_t orad;
    fastf_t irad;
    fastf_t x, y, xnew, ynew;
    vect_t n;
    int i;

    NMG_CK_SHELL(s);
    BN_CK_TOL(tol);

    next = BU_LIST_NEXT(wdb_pipept, &pipept->l);

    VSUB2(n, pipept->pp_coord, next->pp_coord);
    VUNITIZE(n);
    bn_vec_ortho(r1, n);
    VCROSS(r2, n, r1);

    orad = pipept->pp_od / 2.0;
    irad = pipept->pp_id / 2.0;

    if (orad <= tol->dist) {
	return;
    }

    if (irad > orad) {
	bu_log("Inner radius larger than outer radius at start of pipe solid\n");
	return;
    }

    if (NEAR_EQUAL(irad, orad , tol->dist)) {
	return;
    }


    fu = nmg_cface(s, *outer_loop, arc_segs);

    x = orad;
    y = 0.0;
    i = (-1);
    lu = BU_LIST_FIRST(loopuse, &fu->lu_hd);
    for (BU_LIST_FOR(eu, edgeuse, &lu->down_hd)) {
	VJOIN2(pt, pipept->pp_coord, x, r1, y, r2);
	(*outer_loop)[++i] = eu->vu_p->v_p;
	nmg_vertex_gv(eu->vu_p->v_p, pt);
	xnew = x * cos_del - y * sin_del;
	ynew = x * sin_del + y * cos_del;
	x = xnew;
	y = ynew;
    }

    if (irad > tol->dist) {
	struct edgeuse *new_eu;
	struct vertexuse *vu;

	/* create a loop of a single vertex using the first vertex from the inner loop */
	lu = nmg_mlv(&fu->l.magic, (struct vertex *)NULL, OT_OPPOSITE);

	vu = BU_LIST_FIRST(vertexuse, &lu->down_hd);
	eu = nmg_meonvu(vu);
	(*inner_loop)[0] = eu->vu_p->v_p;

	x = irad;
	y = 0.0;
	VJOIN2(pt, pipept->pp_coord, x, r1, y, r2);
	nmg_vertex_gv((*inner_loop)[0], pt);
	/* split edges in loop for each vertex in inner loop */
	for (i = 1; i < arc_segs; i++) {
	    new_eu = nmg_eusplit((struct vertex *)NULL, eu, 0);
	    (*inner_loop)[i] = new_eu->vu_p->v_p;
	    xnew = x * cos_del - y * sin_del;
	    ynew = x * sin_del + y * cos_del;
	    x = xnew;
	    y = ynew;
	    VJOIN2(pt, pipept->pp_coord, x, r1, y, r2);
	    nmg_vertex_gv((*inner_loop)[i], pt);
	}
    } else if (next->pp_id > tol->dist) {
	struct vertexuse *vu;

	/* make a loop of a single vertex in this face */
	lu = nmg_mlv(&fu->l.magic, (struct vertex *)NULL, OT_OPPOSITE);
	vu = BU_LIST_FIRST(vertexuse, &lu->down_hd);

	nmg_vertex_gv(vu->v_p, pipept->pp_coord);
    }

    if (nmg_calc_face_g(fu)) {
	bu_bomb("tesselate_pipe_start: nmg_calc_face_g failed\n");
    }

    for (BU_LIST_FOR(lu, loopuse, &fu->lu_hd)) {
	NMG_CK_LOOPUSE(lu);

	if (BU_LIST_FIRST_MAGIC(&lu->down_hd) != NMG_EDGEUSE_MAGIC) {
	    continue;
	}

	for (BU_LIST_FOR(eu, edgeuse, &lu->down_hd)) {
	    NMG_CK_EDGEUSE(eu);
	    eu->e_p->is_real = 1;
	}
    }
}


HIDDEN void
tesselate_pipe_linear(
    fastf_t *start_pt, fastf_t orad, fastf_t irad,
    fastf_t *end_pt, fastf_t end_orad, fastf_t end_irad,
    int arc_segs,
    double sin_del,
    double cos_del,
    struct vertex ***outer_loop,
    struct vertex ***inner_loop,
    fastf_t *r1,
    fastf_t *r2,
    struct shell *s,
    const struct bn_tol *tol)
{
    struct vertex **new_outer_loop;
    struct vertex **new_inner_loop;
    struct vertex **verts[3];
    struct faceuse *fu;
    vect_t *norms;
    vect_t n;
    fastf_t slope;
    fastf_t seg_len;
    int i, j;

    NMG_CK_SHELL(s);
    BN_CK_TOL(tol);

    norms = (vect_t *)bu_calloc(arc_segs, sizeof(vect_t), "tesselate_pipe_linear: new normals");

    if (end_orad > tol->dist) {
	new_outer_loop = (struct vertex **)bu_calloc(arc_segs, sizeof(struct vertex *),
						     "tesselate_pipe_linear: new_outer_loop");
    } else {
	new_outer_loop = (struct vertex **)NULL;
    }

    if (end_irad > tol->dist) {
	new_inner_loop = (struct vertex **)bu_calloc(arc_segs, sizeof(struct vertex *),
						     "tesselate_pipe_linear: new_inner_loop");
    } else {
	new_inner_loop = (struct vertex **)NULL;
    }

    VSUB2(n, end_pt, start_pt);
    seg_len = MAGNITUDE(n);
    VSCALE(n, n, 1.0 / seg_len);
    slope = (orad - end_orad) / seg_len;

    if (orad > tol->dist && end_orad > tol->dist) {
	point_t pt;
	fastf_t x, y, xnew, ynew;
	struct faceuse *fu_prev = (struct faceuse *)NULL;

	x = 1.0;
	y = 0.0;
	VCOMB2(norms[0], x, r1, y, r2);
	VJOIN1(norms[0], norms[0], slope, n);
	VUNITIZE(norms[0]);
	for (i = 0; i < arc_segs; i++) {
	    j = i + 1;
	    if (j == arc_segs) {
		j = 0;
	    }

	    VJOIN2(pt, end_pt, x * end_orad, r1, y * end_orad, r2);
	    xnew = x * cos_del - y * sin_del;
	    ynew = x * sin_del + y * cos_del;
	    x = xnew;
	    y = ynew;
	    if (i < arc_segs - 1) {
		VCOMB2(norms[j], x, r1, y, r2);
		VJOIN1(norms[j], norms[j], slope, n);
		VUNITIZE(norms[j]);
	    }

	    if (fu_prev) {
		nmg_vertex_gv(new_outer_loop[i], pt);
		if (nmg_calc_face_g(fu_prev)) {
		    bu_log("tesselate_pipe_linear: nmg_calc_face_g failed\n");
		    nmg_kfu(fu_prev);
		} else {
		    /* assign vertexuse normals */
		    struct loopuse *lu;
		    struct edgeuse *eu;

		    NMG_CK_FACEUSE(fu_prev);

		    if (fu_prev->orientation != OT_SAME) {
			fu_prev = fu_prev->fumate_p;
		    }

		    lu = BU_LIST_FIRST(loopuse, &fu_prev->lu_hd);

		    for (BU_LIST_FOR(eu, edgeuse, &lu->down_hd)) {
			vect_t reverse_norm;
			struct edgeuse *eu_opp_use;

			eu_opp_use = BU_LIST_PNEXT_CIRC(edgeuse, &eu->eumate_p->l);
			if (eu->vu_p->v_p == new_outer_loop[i - 1]) {
			    nmg_vertexuse_nv(eu->vu_p, norms[i - 1]);
			    VREVERSE(reverse_norm, norms[i - 1]);
			    nmg_vertexuse_nv(eu_opp_use->vu_p, reverse_norm);
			} else if (eu->vu_p->v_p == (*outer_loop)[i - 1]) {
			    nmg_vertexuse_nv(eu->vu_p, norms[i - 1]);
			    VREVERSE(reverse_norm, norms[i - 1]);
			    nmg_vertexuse_nv(eu_opp_use->vu_p, reverse_norm);
			} else if (eu->vu_p->v_p == new_outer_loop[i]) {
			    nmg_vertexuse_nv(eu->vu_p, norms[i]);
			    VREVERSE(reverse_norm, norms[i]);
			    nmg_vertexuse_nv(eu_opp_use->vu_p, reverse_norm);
			} else if (eu->vu_p->v_p == (*outer_loop)[i]) {
			    nmg_vertexuse_nv(eu->vu_p, norms[i]);
			    VREVERSE(reverse_norm, norms[i]);
			    nmg_vertexuse_nv(eu_opp_use->vu_p, reverse_norm);
			} else {
			    bu_log("No vu_normal assigned at (%g %g %g)\n", V3ARGS(eu->vu_p->v_p->vg_p->coord));
			    bu_log("\ti=%d, arc_segs=%d, fu_prev = %p\n", i, arc_segs, (void *)fu_prev);
			}
		    }
		}
	    }

	    verts[0] = &(*outer_loop)[j];
	    verts[1] = &(*outer_loop)[i];
	    verts[2] = &new_outer_loop[i];

	    if ((fu = nmg_cmface(s, verts, 3)) == NULL) {
		bu_log("tesselate_pipe_linear: failed to make outer face #%d orad=%g, end_orad=%g\n",
		       i, orad , end_orad);
		continue;
	    }
	    if (!new_outer_loop[i]->vg_p) {
		nmg_vertex_gv(new_outer_loop[i], pt);
	    }

	    if (nmg_calc_face_g(fu)) {
		bu_log("tesselate_pipe_linear: nmg_calc_face_g failed\n");
		nmg_kfu(fu);
	    } else {
		/* assign vertexuse normals */
		struct loopuse *lu;
		struct edgeuse *eu;

		NMG_CK_FACEUSE(fu);

		if (fu->orientation != OT_SAME) {
		    fu = fu->fumate_p;
		}

		lu = BU_LIST_FIRST(loopuse, &fu->lu_hd);

		for (BU_LIST_FOR(eu, edgeuse, &lu->down_hd)) {
		    vect_t reverse_norm;
		    struct edgeuse *eu_opp_use;

		    eu_opp_use = BU_LIST_PNEXT_CIRC(edgeuse, &eu->eumate_p->l);
		    if (eu->vu_p->v_p == (*outer_loop)[0]) {
			nmg_vertexuse_nv(eu->vu_p, norms[0]);
			VREVERSE(reverse_norm, norms[0]);
			nmg_vertexuse_nv(eu_opp_use->vu_p, reverse_norm);
		    } else if (eu->vu_p->v_p == new_outer_loop[i]) {
			nmg_vertexuse_nv(eu->vu_p, norms[i]);
			VREVERSE(reverse_norm, norms[i]);
			nmg_vertexuse_nv(eu_opp_use->vu_p, reverse_norm);
		    } else if (eu->vu_p->v_p == (*outer_loop)[i]) {
			nmg_vertexuse_nv(eu->vu_p, norms[i]);
			VREVERSE(reverse_norm, norms[i]);
			nmg_vertexuse_nv(eu_opp_use->vu_p, reverse_norm);
		    } else if (eu->vu_p->v_p == (*outer_loop)[j]) {
			nmg_vertexuse_nv(eu->vu_p, norms[j]);
			VREVERSE(reverse_norm, norms[j]);
			nmg_vertexuse_nv(eu_opp_use->vu_p, reverse_norm);
		    } else {
			bu_log("No vu_normal assigned at (%g %g %g)\n", V3ARGS(eu->vu_p->v_p->vg_p->coord));
			bu_log("\ti=%d, arc_segs=%d, fu = %p\n", i, arc_segs, (void *)fu);
		    }
		}
	    }

	    verts[1] = verts[2];
	    verts[2] = &new_outer_loop[j];

	    if ((fu_prev = nmg_cmface(s, verts, 3)) == NULL) {
		bu_log("tesselate_pipe_linear: failed to make outer face #%d orad=%g, end_orad=%g\n",
		       i, orad , end_orad);
		continue;
	    }
	    if (i == arc_segs - 1) {
		if (nmg_calc_face_g(fu_prev)) {
		    bu_log("tesselate_pipe_linear: nmg_calc_face_g failed\n");
		    nmg_kfu(fu_prev);
		}
	    }
	}
	bu_free((char *)(*outer_loop), "tesselate_pipe_bend: outer_loop");
	*outer_loop = new_outer_loop;
    } else if (orad > tol->dist && end_orad <= tol->dist) {
	struct vertex *v = (struct vertex *)NULL;

	VSUB2(norms[0], (*outer_loop)[0]->vg_p->coord, start_pt);
	VJOIN1(norms[0], norms[0], slope * orad , n);
	VUNITIZE(norms[0]);
	for (i = 0; i < arc_segs; i++) {
	    j = i + 1;
	    if (j == arc_segs) {
		j = 0;
	    }

	    verts[0] = &(*outer_loop)[j];
	    verts[1] = &(*outer_loop)[i];
	    verts[2] = &v;

	    if ((fu = nmg_cmface(s, verts, 3)) == NULL) {
		bu_log("tesselate_pipe_linear: failed to make outer face #%d orad=%g, end_orad=%g\n",
		       i, orad , end_orad);
		continue;
	    }
	    if (i == 0) {
		nmg_vertex_gv(v, end_pt);
	    }

	    if (i < arc_segs - 1) {
		VSUB2(norms[j], (*outer_loop)[j]->vg_p->coord, start_pt);
		VJOIN1(norms[j], norms[j], slope * orad , n);
		VUNITIZE(norms[j]);
	    }

	    if (nmg_calc_face_g(fu)) {
		bu_log("tesselate_pipe_linear: nmg_calc_face_g failed\n");
		nmg_kfu(fu);
	    } else {
		struct loopuse *lu;
		struct edgeuse *eu;
		struct edgeuse *eu_opp_use;
		vect_t reverse_norm;

		NMG_CK_FACEUSE(fu);
		if (fu->orientation != OT_SAME) {
		    fu = fu->fumate_p;
		}

		lu = BU_LIST_FIRST(loopuse, &fu->lu_hd);
		for (BU_LIST_FOR(eu, edgeuse, &lu->down_hd)) {
		    eu_opp_use = BU_LIST_PNEXT_CIRC(edgeuse, &eu->eumate_p->l);
		    if (eu->vu_p->v_p == (*outer_loop)[i]) {
			nmg_vertexuse_nv(eu->vu_p, norms[i]);
			VREVERSE(reverse_norm, norms[i]);
			nmg_vertexuse_nv(eu_opp_use->vu_p, reverse_norm);
		    } else if (eu->vu_p->v_p == (*outer_loop)[j]) {
			nmg_vertexuse_nv(eu->vu_p, norms[j]);
			VREVERSE(reverse_norm, norms[j]);
			nmg_vertexuse_nv(eu_opp_use->vu_p, reverse_norm);
		    } else if (eu->vu_p->v_p == v) {
			vect_t tmp_norm;
			VBLEND2(tmp_norm, 0.5, norms[i], 0.5, norms[j]);
			VUNITIZE(tmp_norm);
			nmg_vertexuse_nv(eu->vu_p, tmp_norm);
			VREVERSE(reverse_norm, tmp_norm);
			nmg_vertexuse_nv(eu_opp_use->vu_p, reverse_norm);
		    } else {
			bu_log("No vu_normal assigned at (%g %g %g)\n", V3ARGS(eu->vu_p->v_p->vg_p->coord));
			bu_log("\ti=%d, j=%d, arc_segs=%d, fu = %p\n", i, j, arc_segs, (void *)fu);
		    }
		}
	    }
	}

	bu_free((char *)(*outer_loop), "tesselate_pipe_linear: outer_loop");
	outer_loop[0] = &v;
    } else if (orad <= tol->dist && end_orad > tol->dist) {
	point_t pt, pt_next;
	fastf_t x, y, xnew, ynew;

	x = 1.0;
	y = 0.0;
	VCOMB2(norms[0], x, r1, y, r2);
	VJOIN1(pt_next, end_pt, end_orad, norms[0]);
	VJOIN1(norms[0], norms[0], slope, n);
	VUNITIZE(norms[0]);
	for (i = 0; i < arc_segs; i++) {
	    j = i + 1;
	    if (j == arc_segs) {
		j = 0;
	    }

	    VMOVE(pt, pt_next);
	    xnew = x * cos_del - y * sin_del;
	    ynew = x * sin_del + y * cos_del;
	    x = xnew;
	    y = ynew;
	    if (i < j) {
		VCOMB2(norms[j], x, r1, y, r2);
		VJOIN1(pt_next, end_pt, end_orad, norms[j]);
		VJOIN1(norms[j], norms[j], slope, n);
		VUNITIZE(norms[j]);
	    }

	    verts[0] = &(*outer_loop)[0];
	    verts[1] = &new_outer_loop[i];
	    verts[2] = &new_outer_loop[j];

	    if ((fu = nmg_cmface(s, verts, 3)) == NULL) {
		bu_log("tesselate_pipe_linear: failed to make outer face #%d orad=%g, end_orad=%g\n",
		       i, orad , end_orad);
		continue;
	    }
	    if (!(*outer_loop)[0]->vg_p) {
		nmg_vertex_gv((*outer_loop)[0], start_pt);
	    }
	    if (!new_outer_loop[i]->vg_p) {
		nmg_vertex_gv(new_outer_loop[i], pt);
	    }
	    if (!new_outer_loop[j]->vg_p) {
		nmg_vertex_gv(new_outer_loop[j], pt_next);
	    }
	    if (nmg_calc_face_g(fu)) {
		bu_log("tesselate_pipe_linear: nmg_calc_face_g failed\n");
		nmg_kfu(fu);
	    } else {
		struct loopuse *lu;
		struct edgeuse *eu;
		struct edgeuse *eu_opp_use;
		vect_t reverse_norm;

		NMG_CK_FACEUSE(fu);
		if (fu->orientation != OT_SAME) {
		    fu = fu->fumate_p;
		}

		lu = BU_LIST_FIRST(loopuse, &fu->lu_hd);
		for (BU_LIST_FOR(eu, edgeuse, &lu->down_hd)) {
		    eu_opp_use = BU_LIST_PNEXT_CIRC(edgeuse, &eu->eumate_p->l);
		    if (eu->vu_p->v_p == new_outer_loop[i]) {
			nmg_vertexuse_nv(eu->vu_p, norms[i]);
			VREVERSE(reverse_norm, norms[i]);
			nmg_vertexuse_nv(eu_opp_use->vu_p, reverse_norm);
		    } else if (eu->vu_p->v_p == new_outer_loop[j]) {
			nmg_vertexuse_nv(eu->vu_p, norms[j]);
			VREVERSE(reverse_norm, norms[j]);
			nmg_vertexuse_nv(eu_opp_use->vu_p, reverse_norm);
		    } else if (eu->vu_p->v_p == (*outer_loop)[0]) {
			vect_t tmp_norm;
			VBLEND2(tmp_norm, 0.5, norms[i], 0.5, norms[j]);
			VUNITIZE(tmp_norm);
			nmg_vertexuse_nv(eu->vu_p, tmp_norm);
			VREVERSE(reverse_norm, tmp_norm);
			nmg_vertexuse_nv(eu_opp_use->vu_p, reverse_norm);
		    } else {
			bu_log("No vu_normal assigned at (%g %g %g)\n", V3ARGS(eu->vu_p->v_p->vg_p->coord));
			bu_log("\ti=%d, j=%d, arc_segs=%d, fu = %p\n", i, j, arc_segs, (void *)fu);
		    }
		}
	    }
	}
	bu_free((char *)(*outer_loop), "tesselate_pipe_linear: outer_loop");
	*outer_loop = new_outer_loop;
    }

    slope = (irad - end_irad) / seg_len;

    if (irad > tol->dist && end_irad > tol->dist) {
	point_t pt;
	fastf_t x, y, xnew, ynew;
	struct faceuse *fu_prev = (struct faceuse *)NULL;

	x = 1.0;
	y = 0.0;
	VCOMB2(norms[0], -x, r1, -y, r2);
	VJOIN1(norms[0], norms[0], -slope, n);
	VUNITIZE(norms[0]);
	for (i = 0; i < arc_segs; i++) {
	    j = i + 1;
	    if (j == arc_segs) {
		j = 0;
	    }

	    VJOIN2(pt, end_pt, x * end_irad, r1, y * end_irad, r2);
	    xnew = x * cos_del - y * sin_del;
	    ynew = x * sin_del + y * cos_del;
	    x = xnew;
	    y = ynew;
	    if (i < arc_segs - 1) {
		VCOMB2(norms[j], -x, r1, -y, r2);
		VJOIN1(norms[j], norms[j], -slope, n);
		VUNITIZE(norms[j]);
	    }

	    if (fu_prev) {
		nmg_vertex_gv(new_inner_loop[i], pt);
		if (nmg_calc_face_g(fu_prev)) {
		    bu_log("tesselate_pipe_linear: nmg_calc_face_g failed\n");
		    nmg_kfu(fu_prev);
		} else {
		    /* assign vertexuse normals */
		    struct loopuse *lu;
		    struct edgeuse *eu;

		    NMG_CK_FACEUSE(fu_prev);

		    if (fu_prev->orientation != OT_SAME) {
			fu_prev = fu_prev->fumate_p;
		    }

		    lu = BU_LIST_FIRST(loopuse, &fu_prev->lu_hd);

		    for (BU_LIST_FOR(eu, edgeuse, &lu->down_hd)) {
			vect_t reverse_norm;
			struct edgeuse *eu_opp_use;

			eu_opp_use = BU_LIST_PNEXT_CIRC(edgeuse, &eu->eumate_p->l);
			if (eu->vu_p->v_p == new_inner_loop[i - 1]) {
			    nmg_vertexuse_nv(eu->vu_p, norms[i - 1]);
			    VREVERSE(reverse_norm, norms[i - 1]);
			    nmg_vertexuse_nv(eu_opp_use->vu_p, reverse_norm);
			} else if (eu->vu_p->v_p == (*inner_loop)[i - 1]) {
			    nmg_vertexuse_nv(eu->vu_p, norms[i - 1]);
			    VREVERSE(reverse_norm, norms[i - 1]);
			    nmg_vertexuse_nv(eu_opp_use->vu_p, reverse_norm);
			} else if (eu->vu_p->v_p == new_inner_loop[i]) {
			    nmg_vertexuse_nv(eu->vu_p, norms[i]);
			    VREVERSE(reverse_norm, norms[i]);
			    nmg_vertexuse_nv(eu_opp_use->vu_p, reverse_norm);
			} else if (eu->vu_p->v_p == (*inner_loop)[i]) {
			    nmg_vertexuse_nv(eu->vu_p, norms[i]);
			    VREVERSE(reverse_norm, norms[i]);
			    nmg_vertexuse_nv(eu_opp_use->vu_p, reverse_norm);
			} else {
			    bu_log("No vu_normal assigned at (%g %g %g)\n", V3ARGS(eu->vu_p->v_p->vg_p->coord));
			    bu_log("\ti=%d, arc_segs=%d, fu_prev = %p\n", i, arc_segs, (void *)fu_prev);
			}
		    }
		}
	    }

	    verts[0] = &(*inner_loop)[j];
	    verts[1] = &new_inner_loop[i];
	    verts[2] = &(*inner_loop)[i];

	    if ((fu = nmg_cmface(s, verts, 3)) == NULL) {
		bu_log("tesselate_pipe_linear: failed to make inner face #%d irad=%g, end_irad=%g\n",
		       i, irad, end_irad);
		continue;
	    }
	    if (!new_inner_loop[i]->vg_p) {
		nmg_vertex_gv(new_inner_loop[i], pt);
	    }

	    if (nmg_calc_face_g(fu)) {
		bu_log("tesselate_pipe_linear: nmg_calc_face_g failed\n");
		nmg_kfu(fu);
	    } else {
		/* assign vertexuse normals */
		struct loopuse *lu;
		struct edgeuse *eu;

		NMG_CK_FACEUSE(fu);

		if (fu->orientation != OT_SAME) {
		    fu = fu->fumate_p;
		}

		lu = BU_LIST_FIRST(loopuse, &fu->lu_hd);

		for (BU_LIST_FOR(eu, edgeuse, &lu->down_hd)) {
		    vect_t reverse_norm;
		    struct edgeuse *eu_opp_use;

		    eu_opp_use = BU_LIST_PNEXT_CIRC(edgeuse, &eu->eumate_p->l);
		    if (eu->vu_p->v_p == (*inner_loop)[0]) {
			nmg_vertexuse_nv(eu->vu_p, norms[0]);
			VREVERSE(reverse_norm, norms[0]);
			nmg_vertexuse_nv(eu_opp_use->vu_p, reverse_norm);
		    } else if (eu->vu_p->v_p == new_inner_loop[i]) {
			nmg_vertexuse_nv(eu->vu_p, norms[i]);
			VREVERSE(reverse_norm, norms[i]);
			nmg_vertexuse_nv(eu_opp_use->vu_p, reverse_norm);
		    } else if (eu->vu_p->v_p == (*inner_loop)[i]) {
			nmg_vertexuse_nv(eu->vu_p, norms[i]);
			VREVERSE(reverse_norm, norms[i]);
			nmg_vertexuse_nv(eu_opp_use->vu_p, reverse_norm);
		    } else if (eu->vu_p->v_p == (*inner_loop)[j]) {
			nmg_vertexuse_nv(eu->vu_p, norms[j]);
			VREVERSE(reverse_norm, norms[j]);
			nmg_vertexuse_nv(eu_opp_use->vu_p, reverse_norm);
		    } else {
			bu_log("No vu_normal assigned at (%g %g %g)\n", V3ARGS(eu->vu_p->v_p->vg_p->coord));
			bu_log("\ti=%d, arc_segs=%d, fu = %p\n", i, arc_segs, (void *)fu);
		    }
		}
	    }

	    verts[2] = verts[0];
	    verts[0] = verts[1];
	    verts[1] = verts[2];
	    if (i == arc_segs - 1) {
		verts[2] = &new_inner_loop[0];
	    } else {
		verts[2] = &new_inner_loop[j];
	    }
	    if ((fu_prev = nmg_cmface(s, verts, 3)) == NULL) {
		bu_log("tesselate_pipe_linear: failed to make inner face #%d irad=%g, end_irad=%g\n",
		       i, irad, end_irad);
		continue;
	    }
	    if (i == arc_segs - 1) {
		if (nmg_calc_face_g(fu_prev)) {
		    bu_log("tesselate_pipe_linear: nmg_calc_face_g failed\n");
		    nmg_kfu(fu_prev);
		}
	    }

	}
	bu_free((char *)(*inner_loop), "tesselate_pipe_bend: inner_loop");
	*inner_loop = new_inner_loop;
    } else if (irad > tol->dist && end_irad <= tol->dist) {
	struct vertex *v = (struct vertex *)NULL;

	VSUB2(norms[0], (*inner_loop)[0]->vg_p->coord, start_pt);
	VJOIN1(norms[0], norms[0], -slope * irad, n);
	VUNITIZE(norms[0]);
	VREVERSE(norms[0], norms[0]);
	for (i = 0; i < arc_segs; i++) {
	    j = i + 1;
	    if (j == arc_segs) {
		j = 0;
	    }

	    verts[0] = &(*inner_loop)[i];
	    verts[1] = &(*inner_loop)[j];
	    verts[2] = &v;

	    if ((fu = nmg_cmface(s, verts, 3)) == NULL) {
		bu_log("tesselate_pipe_linear: failed to make inner face #%d irad=%g, end_irad=%g\n",
		       i, irad, end_irad);
		continue;
	    }
	    if (i == 0) {
		nmg_vertex_gv(v, end_pt);
	    }

	    if (i < arc_segs - 1) {
		VSUB2(norms[j], (*inner_loop)[j]->vg_p->coord, start_pt);
		VJOIN1(norms[j], norms[j], -slope * irad, n);
		VUNITIZE(norms[j]);
		VREVERSE(norms[j], norms[j]);
	    }

	    if (nmg_calc_face_g(fu)) {
		bu_log("tesselate_pipe_linear: nmg_calc_face_g failed\n");
		nmg_kfu(fu);
	    } else {
		struct loopuse *lu;
		struct edgeuse *eu;
		struct edgeuse *eu_opp_use;
		vect_t reverse_norm;

		NMG_CK_FACEUSE(fu);
		if (fu->orientation != OT_SAME) {
		    fu = fu->fumate_p;
		}

		lu = BU_LIST_FIRST(loopuse, &fu->lu_hd);
		for (BU_LIST_FOR(eu, edgeuse, &lu->down_hd)) {
		    eu_opp_use = BU_LIST_PNEXT_CIRC(edgeuse, &eu->eumate_p->l);
		    if (eu->vu_p->v_p == (*inner_loop)[i]) {
			nmg_vertexuse_nv(eu->vu_p, norms[i]);
			VREVERSE(reverse_norm, norms[i]);
			nmg_vertexuse_nv(eu_opp_use->vu_p, reverse_norm);
		    } else if (eu->vu_p->v_p == (*inner_loop)[j]) {
			nmg_vertexuse_nv(eu->vu_p, norms[j]);
			VREVERSE(reverse_norm, norms[j]);
			nmg_vertexuse_nv(eu_opp_use->vu_p, reverse_norm);
		    } else if (eu->vu_p->v_p == v) {
			vect_t tmp_norm;
			VBLEND2(tmp_norm, 0.5, norms[i], 0.5, norms[j]);
			VUNITIZE(tmp_norm);
			nmg_vertexuse_nv(eu->vu_p, tmp_norm);
			VREVERSE(reverse_norm, tmp_norm);
			nmg_vertexuse_nv(eu_opp_use->vu_p, reverse_norm);
		    } else {
			bu_log("No vu_normal assigned at (%g %g %g)\n", V3ARGS(eu->vu_p->v_p->vg_p->coord));
			bu_log("\ti=%d, j=%d, arc_segs=%d, fu = %p\n", i, j, arc_segs, (void *)fu);
		    }
		}
	    }
	}

	bu_free((char *)(*inner_loop), "tesselate_pipe_linear: inner_loop");
	inner_loop[0] = &v;
    } else if (irad <= tol->dist && end_irad > tol->dist) {
	point_t pt, pt_next;
	fastf_t x, y, xnew, ynew;

	x = 1.0;
	y = 0.0;
	VCOMB2(norms[0], -x, r1, -y, r2);
	VJOIN1(pt_next, end_pt, -end_irad, norms[0]);
	VJOIN1(norms[0], norms[0], -slope, n);
	VUNITIZE(norms[0]);
	for (i = 0; i < arc_segs; i++) {
	    j = i + 1;
	    if (j == arc_segs) {
		j = 0;
	    }

	    VMOVE(pt, pt_next);
	    xnew = x * cos_del - y * sin_del;
	    ynew = x * sin_del + y * cos_del;
	    x = xnew;
	    y = ynew;
	    if (i < j) {
		VCOMB2(norms[j], -x, r1, -y, r2);
		VJOIN1(pt_next, end_pt, -end_irad, norms[j]);
		VJOIN1(norms[j], norms[j], -slope, n);
		VUNITIZE(norms[j]);
	    }

	    verts[0] = &new_inner_loop[j];
	    verts[1] = &new_inner_loop[i];
	    verts[2] = &(*inner_loop)[0];

	    if ((fu = nmg_cmface(s, verts, 3)) == NULL) {
		bu_log("tesselate_pipe_linear: failed to make inner face #%d irad=%g, end_irad=%g\n",
		       i, irad, end_irad);
		continue;
	    }
	    if (!(*inner_loop)[0]->vg_p) {
		nmg_vertex_gv((*inner_loop)[0], start_pt);
	    }
	    if (!new_inner_loop[i]->vg_p) {
		nmg_vertex_gv(new_inner_loop[i], pt);
	    }
	    if (!new_inner_loop[j]->vg_p) {
		nmg_vertex_gv(new_inner_loop[j], pt_next);
	    }
	    if (nmg_calc_face_g(fu)) {
		bu_log("tesselate_pipe_linear: nmg_calc_face_g failed\n");
		nmg_kfu(fu);
	    } else {
		struct loopuse *lu;
		struct edgeuse *eu;
		struct edgeuse *eu_opp_use;
		vect_t reverse_norm;

		NMG_CK_FACEUSE(fu);
		if (fu->orientation != OT_SAME) {
		    fu = fu->fumate_p;
		}

		lu = BU_LIST_FIRST(loopuse, &fu->lu_hd);
		for (BU_LIST_FOR(eu, edgeuse, &lu->down_hd)) {
		    eu_opp_use = BU_LIST_PNEXT_CIRC(edgeuse, &eu->eumate_p->l);
		    if (eu->vu_p->v_p == new_inner_loop[i]) {
			nmg_vertexuse_nv(eu->vu_p, norms[i]);
			VREVERSE(reverse_norm, norms[i]);
			nmg_vertexuse_nv(eu_opp_use->vu_p, reverse_norm);
		    } else if (eu->vu_p->v_p == new_inner_loop[j]) {
			nmg_vertexuse_nv(eu->vu_p, norms[j]);
			VREVERSE(reverse_norm, norms[j]);
			nmg_vertexuse_nv(eu_opp_use->vu_p, reverse_norm);
		    } else if (eu->vu_p->v_p == (*inner_loop)[0]) {
			vect_t tmp_norm;
			VBLEND2(tmp_norm, 0.5, norms[i], 0.5, norms[j]);
			VUNITIZE(tmp_norm);
			nmg_vertexuse_nv(eu->vu_p, tmp_norm);
			VREVERSE(reverse_norm, tmp_norm);
			nmg_vertexuse_nv(eu_opp_use->vu_p, reverse_norm);
		    } else {
			bu_log("No vu_normal assigned at (%g %g %g)\n", V3ARGS(eu->vu_p->v_p->vg_p->coord));
			bu_log("\ti=%d, j=%d, arc_segs=%d, fu = %p\n", i, j, arc_segs, (void *)fu);
		    }
		}
	    }
	}
	bu_free((char *)(*inner_loop), "tesselate_pipe_linear: inner_loop");
	*inner_loop = new_inner_loop;
    }
    bu_free((char *)norms, "tesselate_linear_pipe: norms");
}


HIDDEN void
tesselate_pipe_bend(
    fastf_t *bend_start,
    fastf_t *bend_end,
    fastf_t *bend_center,
    fastf_t orad,
    fastf_t irad,
    int arc_segs,
    double sin_del,
    double cos_del,
    struct vertex ***outer_loop,
    struct vertex ***inner_loop,
    fastf_t *start_r1,
    fastf_t *start_r2,
    struct shell *s,
    const struct bn_tol *tol,
    const struct rt_tess_tol *ttol)
{
    struct vertex **new_outer_loop = NULL;
    struct vertex **new_inner_loop = NULL;
    struct vert_root *vertex_tree = NULL;
    struct vertex **vertex_array = NULL;
    fastf_t bend_radius;
    fastf_t bend_angle;
    fastf_t x, y, xnew, ynew;
    fastf_t delta_angle;
    mat_t rot;
    vect_t b1;
    vect_t b2;
    vect_t r1, r2;
    vect_t r1_tmp, r2_tmp;
    vect_t bend_norm;
    vect_t to_start;
    vect_t to_end;
    vect_t norm;
    point_t origin;
    point_t center;
    point_t old_center;
    int bend_segs = 1;	/* minimum number of edges along bend */
    int bend_seg;
    int tol_segs;
    int i, j, k;

    NMG_CK_SHELL(s);
    BN_CK_TOL(tol);
    RT_CK_TESS_TOL(ttol);
    vertex_tree = create_vert_tree();

    VMOVE(r1, start_r1);
    VMOVE(r2, start_r2);

    /* Calculate vector b1, unit vector in direction from
     * bend center to start point
     */
    VSUB2(to_start, bend_start, bend_center);
    bend_radius = MAGNITUDE(to_start);
    VSCALE(b1, to_start, 1.0 / bend_radius);

    /* bend_norm is normal to plane of bend */
    VSUB2(to_end, bend_end, bend_center);
    VCROSS(bend_norm, b1, to_end);
    VUNITIZE(bend_norm);

    /* b1, b2, and bend_norm form a RH coord, system */
    VCROSS(b2, bend_norm, b1);

    bend_angle = atan2(VDOT(to_end, b2), VDOT(to_end, b1));
    if (bend_angle < 0.0) {
	bend_angle += M_2PI;
    }

    /* calculate number of segments to use along bend */
    if (ttol->abs > 0.0 && ttol->abs < bend_radius + orad) {
	tol_segs = ceil(bend_angle / (2.0 * acos(1.0 - ttol->abs / (bend_radius + orad))));
	if (tol_segs > bend_segs) {
	    bend_segs = tol_segs;
	}
    }
    if (ttol->rel > 0.0) {
	tol_segs = ceil(bend_angle / (2.0 * acos(1.0 - ttol->rel)));
	if (tol_segs > bend_segs) {
	    bend_segs = tol_segs;
	}
    }
    if (ttol->norm > 0.0) {
	tol_segs = ceil(bend_angle / (2.0 * ttol->norm));
	if (tol_segs > bend_segs) {
	    bend_segs = tol_segs;
	}
    }

    /* add starting loops to the vertex tree */
    vertex_array = (struct vertex **)bu_calloc((bend_segs + 1) * arc_segs, sizeof(struct vertex *), "vertex array in pipe.c");
    for (i = 0 ; i < arc_segs ; i++) {
	struct vertex *v = (*outer_loop)[i];
	struct vertex_g *vg = v->vg_p;
	j = Add_vert(vg->coord[X], vg->coord[Y], vg->coord[Z], vertex_tree, tol->dist_sq);
	vertex_array[j] = v;
    }

    delta_angle = bend_angle / (fastf_t)(bend_segs);

    VSETALL(origin, 0.0);
    bn_mat_arb_rot(rot, origin, bend_norm, delta_angle);

    VMOVE(old_center, bend_start);
    for (bend_seg = 0; bend_seg < bend_segs; bend_seg++) {

	new_outer_loop = (struct vertex **)bu_calloc(arc_segs, sizeof(struct vertex *),
						     "tesselate_pipe_bend(): new_outer_loop");

	MAT4X3VEC(r1_tmp, rot, r1);
	MAT4X3VEC(r2_tmp, rot, r2);
	VMOVE(r1, r1_tmp);
	VMOVE(r2, r2_tmp);

	VSUB2(r1_tmp, old_center, bend_center);
	MAT4X3PNT(r2_tmp, rot, r1_tmp);
	VADD2(center, r2_tmp, bend_center);

	x = orad;
	y = 0.0;
	for (i = 0; i < arc_segs; i++) {
	    struct faceuse *fu;
	    struct vertex **verts[3];
	    point_t pt;

	    j = i + 1;
	    if (j == arc_segs) {
		j = 0;
	    }

	    VJOIN2(pt, center, x, r1, y, r2);
	    k = Add_vert(pt[X], pt[Y], pt[Z], vertex_tree, tol->dist_sq);

	    verts[0] = &(*outer_loop)[j];
	    verts[1] = &(*outer_loop)[i];
	    verts[2] = &new_outer_loop[i];

	    if (i != j && j != k && i != k) {
		if ((fu = nmg_cmface(s, verts, 3)) == NULL) {
		    bu_log("tesselate_pipe_bend(): nmg_cmface failed\n");
		    bu_bomb("tesselate_pipe_bend\n");
		}
		if (!new_outer_loop[i]->vg_p) {
		    nmg_vertex_gv(new_outer_loop[i], pt);
		}
		if (nmg_calc_face_g(fu)) {
		    bu_log("tesselate_pipe_bend: nmg_calc_face_g failed\n");
		    nmg_kfu(fu);
		} else {
		    struct loopuse *lu;
		    struct edgeuse *eu;

		    vertex_array[k] = new_outer_loop[i];

		    NMG_CK_FACEUSE(fu);
		    if (fu->orientation != OT_SAME) {
			fu = fu->fumate_p;
		    }

		    lu = BU_LIST_FIRST(loopuse, &fu->lu_hd);
		    for (BU_LIST_FOR(eu, edgeuse, &lu->down_hd)) {
			struct edgeuse *eu_opp_use;

			NMG_CK_EDGEUSE(eu);
			eu_opp_use = BU_LIST_PNEXT_CIRC(edgeuse, &eu->eumate_p->l);

			if (eu->vu_p->v_p == (*outer_loop)[j]) {
			    VSUB2(norm, (*outer_loop)[j]->vg_p->coord, old_center);
			    VUNITIZE(norm);
			    nmg_vertexuse_nv(eu->vu_p, norm);
			    VREVERSE(norm, norm);
			    nmg_vertexuse_nv(eu_opp_use->vu_p, norm);
			} else if (eu->vu_p->v_p == (*outer_loop)[i]) {
			    VSUB2(norm, (*outer_loop)[i]->vg_p->coord, old_center);
			    VUNITIZE(norm);
			    nmg_vertexuse_nv(eu->vu_p, norm);
			    VREVERSE(norm, norm);
			    nmg_vertexuse_nv(eu_opp_use->vu_p, norm);
			} else if (eu->vu_p->v_p == new_outer_loop[i]) {
			    VSUB2(norm, new_outer_loop[i]->vg_p->coord, center);
			    VUNITIZE(norm);
			    nmg_vertexuse_nv(eu->vu_p, norm);
			    VREVERSE(norm, norm);
			    nmg_vertexuse_nv(eu_opp_use->vu_p, norm);
			} else {
			    bu_log("No vu_normal assigned at (%g %g %g)\n", V3ARGS(eu->vu_p->v_p->vg_p->coord));
			    bu_log("\ti=%d, j=%d, arc_segs=%d, fu = %p\n", i, j, arc_segs, (void *)fu);
			}
		    }
		}
	    } else {
		verts[2] = &vertex_array[k];
		new_outer_loop[i] = vertex_array[k];
	    }

	    xnew = x * cos_del - y * sin_del;
	    ynew = x * sin_del + y * cos_del;
	    x = xnew;
	    y = ynew;

	    VJOIN2(pt, center, x, r1, y, r2);
	    k = Add_vert(pt[X], pt[Y], pt[Z], vertex_tree, tol->dist_sq);

	    verts[1] = verts[2];
	    verts[2] = &new_outer_loop[j];

	    if (i != j && j != k && i != k) {
		if ((fu = nmg_cmface(s, verts, 3)) == NULL) {
		    bu_log("tesselate_pipe_bend(): nmg_cmface failed\n");
		    bu_bomb("tesselate_pipe_bend\n");
		}
		if (!(*verts[2])->vg_p) {
		    nmg_vertex_gv(*verts[2], pt);
		}
		if (nmg_calc_face_g(fu)) {
		    bu_log("tesselate_pipe_bend: nmg_calc_face_g failed\n");
		    nmg_kfu(fu);
		} else {
		    struct loopuse *lu;
		    struct edgeuse *eu;

		    vertex_array[k] = new_outer_loop[j];

		    NMG_CK_FACEUSE(fu);
		    if (fu->orientation != OT_SAME) {
			fu = fu->fumate_p;
		    }

		    lu = BU_LIST_FIRST(loopuse, &fu->lu_hd);
		    for (BU_LIST_FOR(eu, edgeuse, &lu->down_hd)) {
			struct edgeuse *eu_opp_use;

			NMG_CK_EDGEUSE(eu);
			eu_opp_use = BU_LIST_PNEXT_CIRC(edgeuse, &eu->eumate_p->l);

			if (eu->vu_p->v_p == (*outer_loop)[j]) {
			    VSUB2(norm, (*outer_loop)[j]->vg_p->coord, old_center);
			    VUNITIZE(norm);
			    nmg_vertexuse_nv(eu->vu_p, norm);
			    VREVERSE(norm, norm);
			    nmg_vertexuse_nv(eu_opp_use->vu_p, norm);
			} else if (eu->vu_p->v_p == new_outer_loop[i]) {
			    VSUB2(norm, new_outer_loop[i]->vg_p->coord, center);
			    VUNITIZE(norm);
			    nmg_vertexuse_nv(eu->vu_p, norm);
			    VREVERSE(norm, norm);
			    nmg_vertexuse_nv(eu_opp_use->vu_p, norm);
			} else if (eu->vu_p->v_p == new_outer_loop[j]) {
			    VSUB2(norm, new_outer_loop[j]->vg_p->coord, center);
			    VUNITIZE(norm);
			    nmg_vertexuse_nv(eu->vu_p, norm);
			    VREVERSE(norm, norm);
			    nmg_vertexuse_nv(eu_opp_use->vu_p, norm);
			} else {
			    bu_log("No vu_normal assigned at (%g %g %g)\n", V3ARGS(eu->vu_p->v_p->vg_p->coord));
			    bu_log("\ti=%d, j=%d, arc_segs=%d, fu = %p\n", i, j, arc_segs, (void *)fu);
			}
		    }
		}
	    } else {
		verts[2] = &vertex_array[k];
		new_outer_loop[j] = vertex_array[k];
	    }
	}

	bu_free((char *)(*outer_loop), "tesselate_pipe_bend: outer_loop");
	*outer_loop = new_outer_loop;
	VMOVE(old_center, center);
    }

    /* release resources, sanity */
    free_vert_tree(vertex_tree);
    bu_free((char *)vertex_array, "vertex array in pipe.c");
    vertex_tree = NULL;
    vertex_array = NULL;

    if (irad <= tol->dist) {
	VMOVE(start_r1, r1);
	VMOVE(start_r2, r2);
	return;
    }

    VMOVE(r1, start_r1);
    VMOVE(r2, start_r2);

    VMOVE(old_center, bend_start);
    for (bend_seg = 0; bend_seg < bend_segs; bend_seg++) {

	new_inner_loop = (struct vertex **)bu_calloc(arc_segs, sizeof(struct vertex *),
						     "tesselate_pipe_bend(): new_inner_loop");

	MAT4X3VEC(r1_tmp, rot, r1);
	MAT4X3VEC(r2_tmp, rot, r2);
	VMOVE(r1, r1_tmp);
	VMOVE(r2, r2_tmp);

	VSUB2(r1_tmp, old_center, bend_center);
	MAT4X3PNT(r2_tmp, rot, r1_tmp);
	VADD2(center, r2_tmp, bend_center);

	x = irad;
	y = 0.0;
	for (i = 0; i < arc_segs; i++) {
	    struct faceuse *fu;
	    struct vertex **verts[3];
	    point_t pt;

	    j = i + 1;
	    if (j == arc_segs) {
		j = 0;
	    }

	    VJOIN2(pt, center, x, r1, y, r2);
	    verts[0] = &(*inner_loop)[i];
	    verts[1] = &(*inner_loop)[j];
	    verts[2] = &new_inner_loop[i];

	    if ((fu = nmg_cmface(s, verts, 3)) == NULL) {
		bu_log("tesselate_pipe_bend(): nmg_cmface failed\n");
		bu_bomb("tesselate_pipe_bend\n");
	    }
	    if (!new_inner_loop[i]->vg_p) {
		nmg_vertex_gv(new_inner_loop[i], pt);
	    }
	    if (nmg_calc_face_g(fu)) {
		bu_log("tesselate_pipe_bend: nmg_calc_face_g failed\n");
		nmg_kfu(fu);
	    } else {
		struct loopuse *lu;
		struct edgeuse *eu;

		NMG_CK_FACEUSE(fu);
		if (fu->orientation != OT_SAME) {
		    fu = fu->fumate_p;
		}

		lu = BU_LIST_FIRST(loopuse, &fu->lu_hd);
		for (BU_LIST_FOR(eu, edgeuse, &lu->down_hd)) {
		    struct edgeuse *eu_opp_use;

		    NMG_CK_EDGEUSE(eu);
		    eu_opp_use = BU_LIST_PNEXT_CIRC(edgeuse, &eu->eumate_p->l);

		    if (eu->vu_p->v_p == (*inner_loop)[j]) {
			VSUB2(norm, old_center, (*inner_loop)[j]->vg_p->coord);
			VUNITIZE(norm);
			nmg_vertexuse_nv(eu->vu_p, norm);
			VREVERSE(norm, norm);
			nmg_vertexuse_nv(eu_opp_use->vu_p, norm);
		    } else if (eu->vu_p->v_p == (*inner_loop)[i]) {
			VSUB2(norm, old_center, (*inner_loop)[i]->vg_p->coord);
			VUNITIZE(norm);
			nmg_vertexuse_nv(eu->vu_p, norm);
			VREVERSE(norm, norm);
			nmg_vertexuse_nv(eu_opp_use->vu_p, norm);
		    } else if (eu->vu_p->v_p == new_inner_loop[i]) {
			VSUB2(norm, center, new_inner_loop[i]->vg_p->coord);
			VUNITIZE(norm);
			nmg_vertexuse_nv(eu->vu_p, norm);
			VREVERSE(norm, norm);
			nmg_vertexuse_nv(eu_opp_use->vu_p, norm);
		    } else {
			bu_log("No vu_normal assigned at (%g %g %g)\n", V3ARGS(eu->vu_p->v_p->vg_p->coord));
			bu_log("\ti=%d, j=%d, arc_segs=%d, fu = %p\n", i, j, arc_segs, (void *)fu);
		    }
		}
	    }

	    xnew = x * cos_del - y * sin_del;
	    ynew = x * sin_del + y * cos_del;
	    x = xnew;
	    y = ynew;
	    VJOIN2(pt, center, x, r1, y, r2);

	    verts[0] = verts[2];
	    verts[2] = &new_inner_loop[j];

	    if ((fu = nmg_cmface(s, verts, 3)) == NULL) {
		bu_log("tesselate_pipe_bend(): nmg_cmface failed\n");
		bu_bomb("tesselate_pipe_bend\n");
	    }
	    if (!(*verts[2])->vg_p) {
		nmg_vertex_gv(*verts[2], pt);
	    }
	    if (nmg_calc_face_g(fu)) {
		bu_log("tesselate_pipe_bend: nmg_calc_face_g failed\n");
		nmg_kfu(fu);
	    } else {
		struct loopuse *lu;
		struct edgeuse *eu;

		NMG_CK_FACEUSE(fu);
		if (fu->orientation != OT_SAME) {
		    fu = fu->fumate_p;
		}

		lu = BU_LIST_FIRST(loopuse, &fu->lu_hd);
		for (BU_LIST_FOR(eu, edgeuse, &lu->down_hd)) {
		    struct edgeuse *eu_opp_use;

		    NMG_CK_EDGEUSE(eu);
		    eu_opp_use = BU_LIST_PNEXT_CIRC(edgeuse, &eu->eumate_p->l);

		    if (eu->vu_p->v_p == (*inner_loop)[j]) {
			VSUB2(norm, old_center, (*inner_loop)[j]->vg_p->coord);
			VUNITIZE(norm);
			nmg_vertexuse_nv(eu->vu_p, norm);
			VREVERSE(norm, norm);
			nmg_vertexuse_nv(eu_opp_use->vu_p, norm);
		    } else if (eu->vu_p->v_p == new_inner_loop[i]) {
			VSUB2(norm, center, new_inner_loop[i]->vg_p->coord);
			VUNITIZE(norm);
			nmg_vertexuse_nv(eu->vu_p, norm);
			VREVERSE(norm, norm);
			nmg_vertexuse_nv(eu_opp_use->vu_p, norm);
		    } else if (eu->vu_p->v_p == new_inner_loop[j]) {
			VSUB2(norm, center, new_inner_loop[j]->vg_p->coord);
			VUNITIZE(norm);
			nmg_vertexuse_nv(eu->vu_p, norm);
			VREVERSE(norm, norm);
			nmg_vertexuse_nv(eu_opp_use->vu_p, norm);
		    } else {
			bu_log("No vu_normal assigned at (%g %g %g)\n", V3ARGS(eu->vu_p->v_p->vg_p->coord));
			bu_log("\ti=%d, j=%d, arc_segs=%d, fu = %p\n", i, j, arc_segs, (void *)fu);
		    }
		}
	    }
	}
	bu_free((char *)(*inner_loop), "tesselate_pipe_bend: inner_loop");
	*inner_loop = new_inner_loop;
	VMOVE(old_center, center);
    }
    VMOVE(start_r1, r1);
    VMOVE(start_r2, r2);
}


HIDDEN void
tesselate_pipe_end(
    struct wdb_pipept *pipept,
    int arc_segs,
    struct vertex ***outer_loop,
    struct vertex ***inner_loop,
    struct shell *s,
    const struct bn_tol *tol)
{
    struct wdb_pipept *prev;
    struct faceuse *fu;
    struct loopuse *lu;

    NMG_CK_SHELL(s);
    BN_CK_TOL(tol);

    if (pipept->pp_od <= tol->dist) {
	return;
    }

    if (NEAR_EQUAL(pipept->pp_od, pipept->pp_id, tol->dist)) {
	return;
    }

    if ((fu = nmg_cface(s, *outer_loop, arc_segs)) == NULL) {
	bu_log("tesselate_pipe_end(): nmg_cface failed\n");
	return;
    }
    fu = fu->fumate_p;
    if (nmg_calc_face_g(fu)) {
	bu_log("tesselate_pipe_end: nmg_calc_face_g failed\n");
	nmg_kfu(fu);
	return;
    }

    prev = BU_LIST_PREV(wdb_pipept, &pipept->l);

    if (pipept->pp_id > tol->dist) {
	struct vertex **verts;
	int i;

	verts = (struct vertex **)bu_calloc(arc_segs, sizeof(struct vertex *),
					    "tesselate_pipe_end: verts");
	for (i = 0; i < arc_segs; i++) {
	    verts[i] = (*inner_loop)[i];
	}

	fu = nmg_add_loop_to_face(s, fu, verts, arc_segs, OT_OPPOSITE);

	bu_free((char *)verts, "tesselate_pipe_end: verts");
    } else if (prev->pp_id > tol->dist) {
	struct vertexuse *vu;

	/* make a loop of a single vertex in this face */
	lu = nmg_mlv(&fu->l.magic, (struct vertex *)NULL, OT_OPPOSITE);
	vu = BU_LIST_FIRST(vertexuse, &lu->down_hd);

	nmg_vertex_gv(vu->v_p, pipept->pp_coord);
    }

    for (BU_LIST_FOR(lu, loopuse, &fu->lu_hd)) {
	struct edgeuse *eu;

	NMG_CK_LOOPUSE(lu);

	if (BU_LIST_FIRST_MAGIC(&lu->down_hd) != NMG_EDGEUSE_MAGIC) {
	    continue;
	}

	for (BU_LIST_FOR(eu, edgeuse, &lu->down_hd)) {
	    NMG_CK_EDGEUSE(eu);
	    eu->e_p->is_real = 1;
	}
    }
}


/**
 * XXXX Still needs vertexuse normals!
 */
int
rt_pipe_tess(
    struct nmgregion **r,
    struct model *m,
    struct rt_db_internal *ip,
    const struct rt_tess_tol *ttol,
    const struct bn_tol *tol)
{
    struct wdb_pipept *pp1;
    struct wdb_pipept *pp2;
    struct wdb_pipept *pp3;
    point_t curr_pt;
    struct shell *s;
    struct rt_pipe_internal *pip;
    int arc_segs = 6;			/* minimum number of segments for a circle */
    int tol_segs;
    fastf_t max_diam = 0.0;
    fastf_t pipe_size;
    fastf_t curr_od, curr_id;
    double delta_angle;
    double sin_del;
    double cos_del;
    point_t min_pt;
    point_t max_pt;
    vect_t min_to_max;
    vect_t r1, r2;
    struct vertex **outer_loop;
    struct vertex **inner_loop;

    RT_CK_DB_INTERNAL(ip);
    pip = (struct rt_pipe_internal *)ip->idb_ptr;
    RT_PIPE_CK_MAGIC(pip);

    BN_CK_TOL(tol);
    RT_CK_TESS_TOL(ttol);
    NMG_CK_MODEL(m);

    *r = (struct nmgregion *)NULL;

    if (BU_LIST_IS_EMPTY(&pip->pipe_segs_head)) {
	return 0;    /* nothing to tesselate */
    }

    pp1 = BU_LIST_FIRST(wdb_pipept, &pip->pipe_segs_head);

    VMOVE(min_pt, pp1->pp_coord);
    VMOVE(max_pt, pp1->pp_coord);

    /* find max diameter */
    for (BU_LIST_FOR(pp1, wdb_pipept, &pip->pipe_segs_head)) {
	if (pp1->pp_od > SMALL_FASTF && pp1->pp_od > max_diam) {
	    max_diam = pp1->pp_od;
	}

	VMINMAX(min_pt, max_pt, pp1->pp_coord);
    }

    if (max_diam <= tol->dist) {
	return 0;    /* nothing to tesselate */
    }

    /* calculate pipe size for relative tolerance */
    VSUB2(min_to_max, max_pt, min_pt);
    pipe_size = MAGNITUDE(min_to_max);

    /* calculate number of segments for circles */
    if (ttol->abs > SMALL_FASTF && ttol->abs * 2.0 < max_diam) {
	tol_segs = ceil(M_PI / acos(1.0 - 2.0 * ttol->abs / max_diam));
	if (tol_segs > arc_segs) {
	    arc_segs = tol_segs;
	}
    }
    if (ttol->rel > SMALL_FASTF && 2.0 * ttol->rel * pipe_size < max_diam) {
	tol_segs = ceil(M_PI / acos(1.0 - 2.0 * ttol->rel * pipe_size / max_diam));
	if (tol_segs > arc_segs) {
	    arc_segs = tol_segs;
	}
    }
    if (ttol->norm > SMALL_FASTF) {
	tol_segs = ceil(M_PI / ttol->norm);
	if (tol_segs > arc_segs) {
	    arc_segs = tol_segs;
	}
    }

    *r = nmg_mrsv(m);
    s = BU_LIST_FIRST(shell, &(*r)->s_hd);

    outer_loop = (struct vertex **)bu_calloc(arc_segs, sizeof(struct vertex *),
					     "rt_pipe_tess: outer_loop");
    inner_loop = (struct vertex **)bu_calloc(arc_segs, sizeof(struct vertex *),
					     "rt_pipe_tess: inner_loop");
    delta_angle = M_2PI / (double)arc_segs;
    sin_del = sin(delta_angle);
    cos_del = cos(delta_angle);

    pp1 = BU_LIST_FIRST(wdb_pipept, &(pip->pipe_segs_head));
    tesselate_pipe_start(pp1, arc_segs, sin_del, cos_del,
			 &outer_loop, &inner_loop, r1, r2, s, tol);

    pp2 = BU_LIST_NEXT(wdb_pipept, &pp1->l);
    if (BU_LIST_IS_HEAD(&pp2->l, &(pip->pipe_segs_head))) {
	return 0;
    }
    pp3 = BU_LIST_NEXT(wdb_pipept, &pp2->l);
    if (BU_LIST_IS_HEAD(&pp3->l, &(pip->pipe_segs_head))) {
	pp3 = (struct wdb_pipept *)NULL;
    }

    VMOVE(curr_pt, pp1->pp_coord);
    curr_od = pp1->pp_od;
    curr_id = pp1->pp_id;
    while (1) {
	vect_t n1, n2;
	vect_t norm;
	vect_t v1;
	fastf_t angle;
	fastf_t dist_to_bend;
	point_t bend_start, bend_end, bend_center;

	if (!pp3) {
	    /* last segment */
	    tesselate_pipe_linear(curr_pt, curr_od / 2.0, curr_id / 2.0,
				  pp2->pp_coord, pp2->pp_od / 2.0, pp2->pp_id / 2.0,
				  arc_segs, sin_del, cos_del, &outer_loop, &inner_loop, r1, r2, s, tol);
	    break;
	}

	VSUB2(n1, curr_pt, pp2->pp_coord);
	if (VNEAR_ZERO(n1, VUNITIZE_TOL)) {
	    /* duplicate point, skip to next point */
	    goto next_pt;
	}

	VSUB2(n2, pp3->pp_coord, pp2->pp_coord);
	VCROSS(norm, n1, n2);
	if (VNEAR_ZERO(norm, VUNITIZE_TOL)) {
	    /* points are collinear, treat as a linear segment */
	    tesselate_pipe_linear(curr_pt, curr_od / 2.0, curr_id / 2.0,
				  pp2->pp_coord, pp2->pp_od / 2.0, pp2->pp_id / 2.0,
				  arc_segs, sin_del, cos_del, &outer_loop, &inner_loop, r1, r2, s, tol);

	    VMOVE(curr_pt, pp2->pp_coord);
	    curr_id = pp2->pp_id;
	    curr_od = pp2->pp_od;
	    goto next_pt;
	}

	VUNITIZE(n1);
	VUNITIZE(n2);
	VUNITIZE(norm);

	/* linear section */
	angle = M_PI - acos(VDOT(n1, n2));
	dist_to_bend = pp2->pp_bendradius * tan(angle / 2.0);
	VJOIN1(bend_start, pp2->pp_coord, dist_to_bend, n1);
	tesselate_pipe_linear(curr_pt, curr_od / 2.0, curr_id / 2.0,
			      bend_start, pp2->pp_od / 2.0, pp2->pp_id / 2.0,
			      arc_segs, sin_del, cos_del, &outer_loop, &inner_loop, r1, r2, s, tol);

	/* and bend section */
	VJOIN1(bend_end, pp2->pp_coord, dist_to_bend, n2);
	VCROSS(v1, n1, norm);
	VJOIN1(bend_center, bend_start, -pp2->pp_bendradius, v1);
	tesselate_pipe_bend(bend_start, bend_end, bend_center, curr_od / 2.0, curr_id / 2.0,
			    arc_segs, sin_del, cos_del, &outer_loop, &inner_loop,
			    r1, r2, s, tol, ttol);

	VMOVE(curr_pt, bend_end);
	curr_id = pp2->pp_id;
	curr_od = pp2->pp_od;
    next_pt:
	pp1 = pp2;
	pp2 = pp3;
	pp3 = BU_LIST_NEXT(wdb_pipept, &pp3->l);
	if (BU_LIST_IS_HEAD(&pp3->l, &(pip->pipe_segs_head))) {
	    pp3 = (struct wdb_pipept *)NULL;
	}
    }

    tesselate_pipe_end(pp2, arc_segs, &outer_loop, &inner_loop, s, tol);

    bu_free((char *)outer_loop, "rt_pipe_tess: outer_loop");
    bu_free((char *)inner_loop, "rt_pipe_tess: inner_loop");

    nmg_rebound(m, tol);
    nmg_edge_fuse(&s->l.magic, tol);

    return 0;
}


int
rt_pipe_import4(
    struct rt_db_internal *ip,
    const struct bu_external *ep,
    const fastf_t *mat,
    const struct db_i *dbip)
{
    struct exported_pipept *exp_pipept;
    struct wdb_pipept *ptp;
    struct wdb_pipept tmp;
    struct rt_pipe_internal *pip;
    union record *rp;

    /* must be double for import and export */
    double scan[ELEMENTS_PER_VECT];

    if (dbip) {
	RT_CK_DBI(dbip);
    }

    BU_CK_EXTERNAL(ep);
    rp = (union record *)ep->ext_buf;
    /* Check record type */
    if (rp->u_id != DBID_PIPE) {
	bu_log("rt_pipe_import4: defective record\n");
	return -1;
    }

    RT_CK_DB_INTERNAL(ip);
    ip->idb_major_type = DB5_MAJORTYPE_BRLCAD;
    ip->idb_type = ID_PIPE;
    ip->idb_meth = &OBJ[ID_PIPE];
    BU_ALLOC(ip->idb_ptr, struct rt_pipe_internal);

    pip = (struct rt_pipe_internal *)ip->idb_ptr;
    pip->pipe_magic = RT_PIPE_INTERNAL_MAGIC;
    pip->pipe_count = ntohl(*(uint32_t *)rp->pwr.pwr_pt_count);

    /*
     * Walk the array of segments in reverse order, allocating a
     * linked list of segments in internal format, using exactly the
     * same structures as libwdb.
     */
    BU_LIST_INIT(&pip->pipe_segs_head);
    if (mat == NULL) {
	mat = bn_mat_identity;
    }
    for (exp_pipept = &rp->pwr.pwr_data[pip->pipe_count - 1]; exp_pipept >= &rp->pwr.pwr_data[0]; exp_pipept--) {
	bu_cv_ntohd((unsigned char *)&scan[0], exp_pipept->epp_id, 1);
	tmp.pp_id = scan[0]; /* convert double to fastf_t */

	bu_cv_ntohd((unsigned char *)&scan[1], exp_pipept->epp_od, 1);
	tmp.pp_od = scan[1]; /* convert double to fastf_t */

	bu_cv_ntohd((unsigned char *)&scan[2], exp_pipept->epp_bendradius, 1);
	tmp.pp_bendradius = scan[2]; /* convert double to fastf_t */

	bu_cv_ntohd((unsigned char *)scan, exp_pipept->epp_coord, ELEMENTS_PER_VECT);
	VMOVE(tmp.pp_coord, scan); /* convert double to fastf_t */

	/* Apply modeling transformations */
	BU_ALLOC(ptp, struct wdb_pipept);
	ptp->l.magic = WDB_PIPESEG_MAGIC;
	MAT4X3PNT(ptp->pp_coord, mat, tmp.pp_coord);
	ptp->pp_id = tmp.pp_id / mat[15];
	ptp->pp_od = tmp.pp_od / mat[15];
	ptp->pp_bendradius = tmp.pp_bendradius / mat[15];
	BU_LIST_APPEND(&pip->pipe_segs_head, &ptp->l);
    }

    return 0;			/* OK */
}


int
rt_pipe_export4(
    struct bu_external *ep,
    const struct rt_db_internal *ip,
    double local2mm,
    const struct db_i *dbip)
{
    struct rt_pipe_internal *pip;
    struct bu_list *headp;
    struct exported_pipept *epp;
    struct wdb_pipept *ppt;
    struct wdb_pipept tmp;
    int count;
    int ngran;
    int nbytes;
    union record *rec;

    if (dbip) {
	RT_CK_DBI(dbip);
    }

    RT_CK_DB_INTERNAL(ip);
    pip = (struct rt_pipe_internal *)ip->idb_ptr;
    RT_PIPE_CK_MAGIC(pip);

    if (pip->pipe_segs_head.magic == 0) {
	return -1; /* no segments provided, empty pipe is bogus */
    }
    headp = &pip->pipe_segs_head;

    /* Count number of points */
    count = 0;
    for (BU_LIST_FOR(ppt, wdb_pipept, headp)) {
	count++;
    }

    if (count < 1) {
	return -4;    /* Not enough for 1 pipe! */
    }

    /* Determine how many whole granules will be required */
    nbytes = sizeof(struct pipewire_rec) +
	(count - 1) * sizeof(struct exported_pipept);
    ngran = (nbytes + sizeof(union record) - 1) / sizeof(union record);

    BU_CK_EXTERNAL(ep);
    ep->ext_nbytes = ngran * sizeof(union record);
    ep->ext_buf = (uint8_t *)bu_calloc(1, ep->ext_nbytes, "pipe external");
    rec = (union record *)ep->ext_buf;

    rec->pwr.pwr_id = DBID_PIPE;
    *(uint32_t *)rec->pwr.pwr_count = htonl(ngran - 1);	/* # EXTRA grans */
    *(uint32_t *)rec->pwr.pwr_pt_count = htonl(count);

    /* Convert the pipe segments to external form */
    epp = &rec->pwr.pwr_data[0];
    for (BU_LIST_FOR(ppt, wdb_pipept, headp), epp++) {
	double scan[ELEMENTS_PER_POINT];

	/* Convert from user units to mm */
	VSCALE(tmp.pp_coord, ppt->pp_coord, local2mm);
	tmp.pp_id = ppt->pp_id * local2mm;
	tmp.pp_od = ppt->pp_od * local2mm;
	tmp.pp_bendradius = ppt->pp_bendradius * local2mm;


	VMOVE(scan, tmp.pp_coord); /* convert fastf_t to double */
	bu_cv_htond(epp->epp_coord, (unsigned char *)scan, ELEMENTS_PER_POINT);

	scan[0] = tmp.pp_id; /* convert fastf_t to double */
	bu_cv_htond(epp->epp_id, (unsigned char *)&scan[0], 1);

	scan[1] = tmp.pp_od; /* convert fastf_t to double */
	bu_cv_htond(epp->epp_od, (unsigned char *)&scan[1], 1);

	scan[2] = tmp.pp_bendradius; /* convert fastf_t to double */
	bu_cv_htond(epp->epp_bendradius, (unsigned char *)&scan[2], 1);
    }

    return 0;
}


int
rt_pipe_import5(
    struct rt_db_internal *ip,
    const struct bu_external *ep,
    const fastf_t *mat,
    const struct db_i *dbip)
{
    struct wdb_pipept *ptp;
    struct rt_pipe_internal *pip;

    /* must be double for import and export */
    double *vec;

    size_t total_count;
    int double_count;
    int byte_count;
    unsigned long pipe_count;
    int i;

    if (dbip) {
	RT_CK_DBI(dbip);
    }
    BU_CK_EXTERNAL(ep);

    pipe_count = ntohl(*(uint32_t *)ep->ext_buf);
    double_count = pipe_count * 6;
    byte_count = double_count * SIZEOF_NETWORK_DOUBLE;
    total_count = 4 + byte_count;
    BU_ASSERT_LONG(ep->ext_nbytes, == , total_count);

    RT_CK_DB_INTERNAL(ip);
    ip->idb_major_type = DB5_MAJORTYPE_BRLCAD;
    ip->idb_type = ID_PIPE;
    ip->idb_meth = &OBJ[ID_PIPE];
    BU_ALLOC(ip->idb_ptr, struct rt_pipe_internal);

    pip = (struct rt_pipe_internal *)ip->idb_ptr;
    pip->pipe_magic = RT_PIPE_INTERNAL_MAGIC;
    pip->pipe_count = pipe_count;

    vec = (double *)bu_malloc(byte_count, "rt_pipe_import5: vec");

    /* Convert from database (network) to internal (host) format */
    bu_cv_ntohd((unsigned char *)vec, (unsigned char *)ep->ext_buf + 4, double_count);

    /*
     * Walk the array of segments in reverse order, allocating a
     * linked list of segments in internal format, using exactly the
     * same structures as libwdb.
     */
    BU_LIST_INIT(&pip->pipe_segs_head);
    if (mat == NULL) {
	mat = bn_mat_identity;
    }
    for (i = 0; i < double_count; i += 6) {
	/* Apply modeling transformations */
	BU_ALLOC(ptp, struct wdb_pipept);
	ptp->l.magic = WDB_PIPESEG_MAGIC;
	MAT4X3PNT(ptp->pp_coord, mat, &vec[i]);
	ptp->pp_id =		vec[i + 3] / mat[15];
	ptp->pp_od =		vec[i + 4] / mat[15];
	ptp->pp_bendradius =	vec[i + 5] / mat[15];
	BU_LIST_INSERT(&pip->pipe_segs_head, &ptp->l);
    }

    bu_free((void *)vec, "rt_pipe_import5: vec");
    return 0;			/* OK */
}


int
rt_pipe_export5(struct bu_external *ep, const struct rt_db_internal *ip, double local2mm, const struct db_i *dbip)
{
    struct rt_pipe_internal *pip;
    struct bu_list *headp;
    struct wdb_pipept *ppt;
    int total_count;
    int double_count;
    int byte_count;
    unsigned long pipe_count;
    int i = 0;

    /* must be double for import and export */
    double *vec;

    if (dbip) {
	RT_CK_DBI(dbip);
    }

    RT_CK_DB_INTERNAL(ip);
    pip = (struct rt_pipe_internal *)ip->idb_ptr;
    RT_PIPE_CK_MAGIC(pip);

    if (pip->pipe_segs_head.magic == 0) {
	return -1; /* no segments provided, empty pipe is bogus */
    }
    headp = &pip->pipe_segs_head;

    /* Count number of points */
    pipe_count = 0;
    for (BU_LIST_FOR(ppt, wdb_pipept, headp)) {
	pipe_count++;
    }

    if (pipe_count <= 1) {
	return -4;    /* Not enough for 1 pipe! */
    }

    double_count = pipe_count * 6;
    byte_count = double_count * SIZEOF_NETWORK_DOUBLE;
    total_count = 4 + byte_count;
    vec = (double *)bu_malloc(byte_count, "rt_pipe_export5: vec");

    BU_CK_EXTERNAL(ep);
    ep->ext_nbytes = total_count;
    ep->ext_buf = (uint8_t *)bu_malloc(ep->ext_nbytes, "pipe external");

    *(uint32_t *)ep->ext_buf = htonl(pipe_count);

    /* Convert the pipe segments to external form */
    for (BU_LIST_FOR(ppt, wdb_pipept, headp), i += 6) {
	/* Convert from user units to mm */
	VSCALE(&vec[i], ppt->pp_coord, local2mm);
	vec[i + 3] = ppt->pp_id * local2mm;
	vec[i + 4] = ppt->pp_od * local2mm;
	vec[i + 5] = ppt->pp_bendradius * local2mm;
    }

    /* Convert from internal (host) to database (network) format */
    bu_cv_htond((unsigned char *)ep->ext_buf + 4, (unsigned char *)vec, double_count);

    bu_free((void *)vec, "rt_pipe_export5: vec");
    return 0;
}


/**
 * Make human-readable formatted presentation of this solid.  First
 * line describes type of solid.  Additional lines are indented one
 * tab, and give parameter values.
 */
int
rt_pipe_describe(
    struct bu_vls *str,
    const struct rt_db_internal *ip,
    int verbose,
    double mm2local)
{
    struct rt_pipe_internal *pip;
    struct wdb_pipept *ptp;
    char buf[256];
    int segno = 0;

    RT_CK_DB_INTERNAL(ip);
    pip = (struct rt_pipe_internal *)ip->idb_ptr;
    RT_PIPE_CK_MAGIC(pip);

    sprintf(buf, "pipe with %d points\n", pip->pipe_count);
    bu_vls_strcat(str, buf);

    if (!verbose) {
	return 0;
    }

    for (BU_LIST_FOR(ptp, wdb_pipept, &pip->pipe_segs_head)) {
	sprintf(buf, "\t%d ", segno++);
	bu_vls_strcat(str, buf);
	sprintf(buf, "\tbend radius = %g", INTCLAMP(ptp->pp_bendradius * mm2local));
	bu_vls_strcat(str, buf);
	sprintf(buf, "  od=%g", INTCLAMP(ptp->pp_od * mm2local));
	bu_vls_strcat(str, buf);
	if (ptp->pp_id > 0) {
	    sprintf(buf, ", id  = %g", INTCLAMP(ptp->pp_id * mm2local));
	    bu_vls_strcat(str, buf);
	}
	bu_vls_strcat(str, "\n");

	sprintf(buf, "\t  at=(%g, %g, %g)\n",
		INTCLAMP(ptp->pp_coord[X] * mm2local),
		INTCLAMP(ptp->pp_coord[Y] * mm2local),
		INTCLAMP(ptp->pp_coord[Z] * mm2local));
	bu_vls_strcat(str, buf);

    }

    return 0;
}


/**
 * Free the storage associated with the rt_db_internal version of this solid.
 */
void
rt_pipe_ifree(struct rt_db_internal *ip)
{
    struct rt_pipe_internal *pip;
    struct wdb_pipept *ptp;

    RT_CK_DB_INTERNAL(ip);
    pip = (struct rt_pipe_internal *)ip->idb_ptr;
    RT_PIPE_CK_MAGIC(pip);

    if (pip->pipe_segs_head.magic != 0) {
	while (BU_LIST_WHILE(ptp, wdb_pipept, &pip->pipe_segs_head)) {
	    BU_LIST_DEQUEUE(&(ptp->l));
	    bu_free((char *)ptp, "wdb_pipept");
	}
    }
    bu_free(ip->idb_ptr, "pipe ifree");
    ip->idb_ptr = ((void *)0);
}


/**
 * Check pipe solid.  Bend radius must be at least as large as the
 * outer radius.  All bends must have constant diameters.  No
 * consecutive LINEAR sections without BENDS unless the LINEAR
 * sections are collinear.  Inner diameter must be less than outer
 * diameter.
 */
int
rt_pipe_ck(const struct bu_list *headp)
{
    int error_count = 0;
    struct wdb_pipept *cur, *prev, *next;
    fastf_t old_bend_dist = 0.0;
    fastf_t new_bend_dist;
    fastf_t v2_len = 0.0;

    prev = BU_LIST_FIRST(wdb_pipept, headp);

    if (prev->pp_id >= prev->pp_od) {
	bu_log("Inner diameter (%gmm) has to be less than outer diameter (%gmm)\n",
	       prev->pp_id, prev->pp_od);
	error_count++;
    }

    if (prev->pp_bendradius < prev->pp_od * 0.5) {
	bu_log("Bend radius (%gmm) is less than outer radius at (%g %g %g)\n",
	       prev->pp_bendradius, V3ARGS(prev->pp_coord));
	error_count++;
    }

    cur = BU_LIST_NEXT(wdb_pipept, &prev->l);
    next = BU_LIST_NEXT(wdb_pipept, &cur->l);
    while (BU_LIST_NOT_HEAD(&next->l, headp)) {
	vect_t v1, v2, norm;
	fastf_t v1_len;
	fastf_t angle;
	fastf_t local_vdot;

	if (cur->pp_id >= cur->pp_od) {
	    bu_log("Inner diameter (%gmm) has to be less than outer diameter (%gmm)\n",
		   cur->pp_id, cur->pp_od);
	    error_count++;
	}

	if (cur->pp_bendradius < cur->pp_od * 0.5) {
	    bu_log("Bend radius (%gmm) is less than outer radius at (%g %g %g)\n",
		   cur->pp_bendradius, V3ARGS(cur->pp_coord));
	    error_count++;
	}

	VSUB2(v1, prev->pp_coord, cur->pp_coord);
	v1_len = MAGNITUDE(v1);
	VUNITIZE(v1);

	VSUB2(v2, next->pp_coord, cur->pp_coord);
	v2_len = MAGNITUDE(v2);
	VUNITIZE(v2);

	VCROSS(norm, v1, v2);
	if (VNEAR_ZERO(norm, SQRT_SMALL_FASTF)) {
	    new_bend_dist = 0.0;
	    goto next_pt;
	}

	local_vdot = VDOT(v1, v2);
	/* protect against fuzzy overflow/underflow, clamp unitized
	 * vectors in order to prevent acos() from throwing an
	 * exception (or crashing).
	 */
	CLAMP(local_vdot, -1.0, 1.0);

	angle = M_PI - acos(local_vdot);
	new_bend_dist = cur->pp_bendradius * tan(angle / 2.0);

	if (new_bend_dist + old_bend_dist > v1_len) {
	    fastf_t vdot;
	    error_count++;
	    bu_log("Bend radii (%gmm) at (%g %g %g) and (%gmm) at (%g %g %g) are too large\n",
		   prev->pp_bendradius, V3ARGS(prev->pp_coord),
		   cur->pp_bendradius, V3ARGS(cur->pp_coord));
	    bu_log("for pipe segment between (%g %g %g) and (%g %g %g)\n",
		   V3ARGS(prev->pp_coord), V3ARGS(cur->pp_coord));
	    bu_log("failed test: %g + %g > %g\n", new_bend_dist, old_bend_dist, v1_len);
	    vdot = VDOT(v1, v2);
	    bu_log("angle(%g) = M_PI(%g) - acos(VDOT(v1, v2)(%g))(%g)\n", angle, M_PI, vdot, acos(vdot));
	    bu_log("v1: (%g %g %g)\n", V3ARGS(v1));
	    bu_log("v2: (%g %g %g)\n", V3ARGS(v2));
	}
    next_pt:
	old_bend_dist = new_bend_dist;
	prev = cur;
	cur = next;
	next = BU_LIST_NEXT(wdb_pipept, &cur->l);
    }

    if (cur->pp_id >= cur->pp_od) {
	bu_log("Inner diameter (%gmm) has to be less than outer diameter (%gmm)\n",
	       cur->pp_id, cur->pp_od);
	error_count++;
    }

    if (old_bend_dist > v2_len) {
	error_count++;
	bu_log("last segment (%g %g %g) to (%g %g %g) is too short to allow\n",
	       V3ARGS(prev->pp_coord), V3ARGS(cur->pp_coord));
	bu_log("bend radius of %gmm\n", prev->pp_bendradius);
    }
    return error_count;
}


/**
 * Examples -
 * db get name V# => get coordinates for vertex #
 * db get name I# => get inner radius for vertex #
 * db get name O# => get outer radius for vertex #
 * db get name R# => get bendradius for vertex #
 * db get name P# => get all data for vertex #
 * db get name N ==> get number of vertices
 */
int
rt_pipe_get(
    struct bu_vls *logstr,
    const struct rt_db_internal *intern,
    const char *attr)
{
    struct rt_pipe_internal *pip = (struct rt_pipe_internal *)intern->idb_ptr;
    struct wdb_pipept *ptp;
    int seg_no;
    int num_segs = 0;

    RT_PIPE_CK_MAGIC(pip);

    /* count segments */
    for (BU_LIST_FOR(ptp, wdb_pipept, &pip->pipe_segs_head)) {
	num_segs++;
    }

    if (attr == (char *)NULL) {
	bu_vls_strcat(logstr, "pipe");

	seg_no = 0;
	for (BU_LIST_FOR(ptp, wdb_pipept, &pip->pipe_segs_head)) {
	    bu_vls_printf(logstr, " V%d { %.25G %.25G %.25G } O%d %.25G I%d %.25G R%d %.25G",
			  seg_no, V3ARGS(ptp->pp_coord),
			  seg_no, ptp->pp_od,
			  seg_no, ptp->pp_id,
			  seg_no, ptp->pp_bendradius);
	    seg_no++;
	}
    } else if (attr[0] == 'N') {
	bu_vls_printf(logstr, "%d", num_segs);
    } else {
	int curr_seg = 0;

	seg_no = atoi(&attr[1]);
	if (seg_no < 0 || seg_no >= num_segs) {
	    bu_vls_printf(logstr, "segment number out of range (0 - %d)", num_segs - 1);
	    return BRLCAD_ERROR;
	}

	/* find the desired vertex */
	for (BU_LIST_FOR(ptp, wdb_pipept, &pip->pipe_segs_head)) {
	    if (curr_seg == seg_no) {
		break;
	    }
	    curr_seg++;
	}

	switch (attr[0]) {
	    case 'V':
		bu_vls_printf(logstr, "%.25G %.25G %.25G", V3ARGS(ptp->pp_coord));
		break;
	    case 'I':
		bu_vls_printf(logstr, "%.25G", ptp->pp_id);
		break;
	    case 'O':
		bu_vls_printf(logstr, "%.25G", ptp->pp_od);
		break;
	    case 'R':
		bu_vls_printf(logstr, "%.25G", ptp->pp_bendradius);
		break;
	    case 'P':
		bu_vls_printf(logstr, " V%d { %.25G %.25G %.25G } I%d %.25G O%d %.25G R%d %.25G",
			      seg_no, V3ARGS(ptp->pp_coord),
			      seg_no, ptp->pp_id,
			      seg_no, ptp->pp_od,
			      seg_no, ptp->pp_bendradius);
		break;
	    default:
		bu_vls_printf(logstr, "unrecognized attribute (%c), choices are V, I, O, R, or P", attr[0]);
		return BRLCAD_ERROR;
	}
    }

    return BRLCAD_OK;
}


int
rt_pipe_adjust(
    struct bu_vls *logstr,
    struct rt_db_internal *intern,
    int argc,
    const char **argv)
{
    struct rt_pipe_internal *pip;
    struct wdb_pipept *ptp;
    Tcl_Obj *obj, *list;
    int seg_no;
    int num_segs;
    int curr_seg;
    fastf_t tmp;
    char *v_str;


    RT_CK_DB_INTERNAL(intern);
    pip = (struct rt_pipe_internal *)intern->idb_ptr;
    RT_PIPE_CK_MAGIC(pip);

    while (argc >= 2) {

	/* count vertices */
	num_segs = 0;
	if (pip->pipe_segs_head.forw) {
	    for (BU_LIST_FOR(ptp, wdb_pipept, &pip->pipe_segs_head)) {
		num_segs++;
	    }
	} else {
	    BU_LIST_INIT(&pip->pipe_segs_head);
	}

	if (!isdigit((int)argv[0][1])) {
	    bu_vls_printf(logstr, "no vertex number specified");
	    return BRLCAD_ERROR;
	}

	seg_no = atoi(&argv[0][1]);
	if (seg_no == num_segs) {
	    struct wdb_pipept *new_pt;

	    BU_ALLOC(new_pt, struct wdb_pipept);
	    if (num_segs > 0) {
		ptp = BU_LIST_LAST(wdb_pipept, &pip->pipe_segs_head);
		*new_pt = *ptp;		/* struct copy */
		BU_LIST_INSERT(&pip->pipe_segs_head, &new_pt->l);
		ptp = new_pt;
	    } else {
		VSETALL(new_pt->pp_coord, 0.0);
		new_pt->pp_id = 0.0;
		new_pt->pp_od = 10.0;
		new_pt->pp_bendradius = 20.0;
		BU_LIST_INSERT(&pip->pipe_segs_head, &new_pt->l);
		ptp = new_pt;
	    }
	    num_segs++;
	}
	if (seg_no < 0 || seg_no >= num_segs) {
	    bu_vls_printf(logstr, "vertex number out of range");
	    return BRLCAD_ERROR;
	}

	/* get the specified vertex */
	curr_seg = 0;
	for (BU_LIST_FOR(ptp, wdb_pipept, &pip->pipe_segs_head)) {
	    if (curr_seg == seg_no) {
		break;
	    }
	    curr_seg++;
	}


	switch (argv[0][0]) {
	    case 'V':
		obj = Tcl_NewStringObj(argv[1], -1);
		list = Tcl_NewListObj(0, NULL);
		Tcl_ListObjAppendList(brlcad_interp, list, obj);
		v_str = Tcl_GetStringFromObj(list, NULL);
		while (isspace((int)*v_str)) {
		    v_str++;
		}
		if (*v_str == '\0') {
		    bu_vls_printf(logstr, "incomplete vertex specification");
		    Tcl_DecrRefCount(list);
		    return BRLCAD_ERROR;
		}
		ptp->pp_coord[0] = atof(v_str);
		v_str = bu_next_token(v_str);
		if (*v_str == '\0') {
		    bu_vls_printf(logstr, "incomplete vertex specification");
		    Tcl_DecrRefCount(list);
		    return BRLCAD_ERROR;
		}
		ptp->pp_coord[1] = atof(v_str);
		v_str = bu_next_token(v_str);
		if (*v_str == '\0') {
		    bu_vls_printf(logstr, "incomplete vertex specification");
		    Tcl_DecrRefCount(list);
		    return BRLCAD_ERROR;
		}
		ptp->pp_coord[2] = atof(v_str);
		Tcl_DecrRefCount(list);
		break;
	    case 'I':
		ptp->pp_id = atof(argv[1]);
		break;
	    case 'O':
		tmp = atof(argv[1]);
		if (tmp <= 0.0) {
		    bu_vls_printf(logstr, "outer diameter cannot be 0.0 or less");
		    return BRLCAD_ERROR;
		}
		ptp->pp_od = tmp;
		break;
	    case 'R':
		ptp->pp_bendradius = atof(argv[1]);
		break;
	    default:
		bu_vls_printf(logstr, "unrecognized attribute, choices are V, I, O, or R");
		return BRLCAD_ERROR;
	}

	argc -= 2;
	argv += 2;
    }

    return (rt_pipe_ck(&pip->pipe_segs_head) == 0) ? TCL_OK : BRLCAD_ERROR;
}


int
rt_pipe_params(struct pc_pc_set *UNUSED(ps), const struct rt_db_internal *ip)
{
    if (ip) {
	RT_CK_DB_INTERNAL(ip);
    }

    return 0;			/* OK */
}


void
rt_pipe_surf_area(fastf_t *area, struct rt_db_internal *ip)
{
    struct bu_list head;
    point_t min, max;
    struct id_pipe *p;
    fastf_t len_sq;
    struct lin_pipe *lin;
    struct bend_pipe *bend;
    fastf_t prev_ir, prev_or, start_ir, start_or, end_ir, end_or, tmpval;
    point_t last_end, first_start;
    vect_t *end_normal, *start_normal;
    int connected;
    char overlap;

    BU_LIST_INIT(&head);

    pipe_elements_calculate(&head, ip, &min, &max);

    /* The following calculation establishes if the last pipe segment
     * is in fact connected to the first one. The last end point is
     * checked to be equal to the first start point, and the normals
     * are checked to be either 0 or 180 degree to each other: abs(dot product) == 1.
     * If the first/last segments are connected, the starting/ending
     * surfaces will cancel each other where overlapping.
     */
    p = BU_LIST_LAST(id_pipe, &head);
    if (!p->pipe_is_bend) {
	lin = (struct lin_pipe *)p;
	prev_ir = lin->pipe_ritop;
	prev_or = lin->pipe_rotop;
	VJOIN1(last_end, lin->pipe_V, lin->pipe_len, lin->pipe_H);
	end_normal = &(lin->pipe_H);
    } else {
	bend = (struct bend_pipe *)p;
	prev_ir = bend->bend_ir;
	prev_or = bend->bend_or;
	VMOVE(last_end, bend->bend_end);
	end_normal = &(bend->bend_endNorm);
    }

    p = BU_LIST_FIRST(id_pipe, &head);
    if (!p->pipe_is_bend) {
	lin = (struct lin_pipe *)p;
	VMOVE(first_start, lin->pipe_V);
	start_normal = &(lin->pipe_H);
    } else {
	bend = (struct bend_pipe *)p;
	VMOVE(first_start, bend->bend_start);
	start_normal = &(bend->bend_startNorm);
    }

    connected = VNEAR_EQUAL(first_start, last_end, RT_LEN_TOL)
	&& NEAR_EQUAL(fabs(VDOT(*start_normal, *end_normal)), 1.0, RT_DOT_TOL);

    *area = 0;
    /* The the total surface area is calculated as a sum of the areas for:
     * + outer lateral pipe surface;
     * + inner lateral pipe surface;
     * + unconnected/non-overlapping cross-section surface at the ends of the pipe segments;
     */
    for (BU_LIST_FOR(p, id_pipe, &head)) {
	if (!p->pipe_is_bend) {
	    lin = (struct lin_pipe *)p;
	    /* Lateral Surface Area = PI * (r_base + r_top) * sqrt(pipe_len^2 + (r_base-r_top)^2) */
	    len_sq = lin->pipe_len * lin->pipe_len;
	    *area += M_PI * (lin->pipe_robase + lin->pipe_rotop)
		* (sqrt(len_sq + lin->pipe_rodiff_sq)      /* outer surface */
		   + sqrt(len_sq + lin->pipe_ridiff_sq));  /* inner surface */
	    start_or = lin->pipe_robase;
	    start_ir = lin->pipe_ribase;
	    end_or = lin->pipe_rotop;
	    end_ir = lin->pipe_ritop;
	} else {
	    bend = (struct bend_pipe *)p;
	    /* Torus Surface Area = 4 * PI^2 * r_bend * r_pipe
	     * Bend Surface Area = torus_area * (bend_angle / (2*PI))
	     *                   = 2 * PI * bend_angle * r_bend * r_pipe
	     * Inner + Outer Area = 2 * PI * bend_angle * r_bend * (r_outer + r_inner)
	     */
	    *area += M_2PI * bend->bend_angle * bend->bend_radius * (bend->bend_ir + bend->bend_or);
	    start_or = end_or = bend->bend_or;
	    start_ir = end_ir = bend->bend_ir;
	}
	if (connected) {
	    overlap = 0;
	    /* For the case of equality we consider only the start overlapping,
	     * to not have both start and prev report an overlap at the same time.
	     * This way we have simplified cases to handle, as overlaps will always
	     * come in pairs, and some pairs can be excluded too.
	     */
	    if (start_ir >= prev_ir && start_ir <= prev_or) {
		overlap += 1;
	    }
	    if (start_or >= prev_ir && start_or <= prev_or) {
		overlap += 2;
	    }
	    if (prev_ir > start_ir && prev_ir < start_or) {
		overlap += 4;
	    }
	    if (prev_or > start_ir && prev_or < start_or) {
		overlap += 8;
	    }
	    /* Overlaps are expected always in pairs, so the sum of the set digits should be always 2 */
	    switch (overlap) {
		case 0: /* no overlap between the cross sections, the areas are both added, nothing to fix */
		    break;
		case 3: /* start cross section contained completely by the prev cross section, we swap start_ir with prev_or */
		case 9: /* section between start_ir and prev_or overlap, we swap them */
		case 12: /* prev cross section contained completely by the start cross section, we swap start_ir with prev_or */
		    tmpval = start_ir;
		    start_ir = prev_or;
		    prev_or = tmpval;
		    break;
		case 6: /* section between prev_ir and start_or overlap, we swap them */
		    tmpval = prev_ir;
		    prev_ir = start_or;
		    start_or = tmpval;
		    break;
		default:
		    bu_log("rt_pipe_surf_area: Unexpected cross-section overlap code: (%d)\n", overlap);
		    break;
	    }
	} else {
	    /* not connected, both areas are added regardless of overlaps */
	    /* this can happen only on the first segment, and all further segments will be connected: */
	    connected = 1;
	}
	tmpval = 0;
	/* start cross section */
	if (!NEAR_EQUAL(start_or, start_ir, RT_LEN_TOL)) {
	    tmpval += (start_or + start_ir) * (start_or - start_ir);
	}
	/* previous end cross section */
	if (!NEAR_EQUAL(start_or, start_ir, RT_LEN_TOL)) {
	    tmpval += (prev_or + prev_ir) * (prev_or - prev_ir);
	}
	*area += M_PI * tmpval;
	prev_or = end_or;
	prev_ir = end_ir;
    }

    pipe_elements_free(&head);

}


HIDDEN void
pipe_elem_volume_and_centroid(struct id_pipe *p, fastf_t *vol, point_t *cent)
{
    fastf_t crt_vol, cs;
    point_t cp;

    /* Note: the centroid is premultiplied with the corresponding partial volume ! */
    if (!p->pipe_is_bend) {
	struct lin_pipe *lin = (struct lin_pipe *)p;
	/* Volume = PI * (r_base*r_base + r_top*r_top + r_base*r_top) * pipe_len / 3 */
	crt_vol = M_PI * lin->pipe_len / 3
	    * (lin->pipe_robase*lin->pipe_robase + lin->pipe_robase*lin->pipe_rotop + lin->pipe_rotop*lin->pipe_rotop
	       - lin->pipe_ribase*lin->pipe_ribase - lin->pipe_ribase*lin->pipe_ritop - lin->pipe_ritop*lin->pipe_ritop);
	*vol += crt_vol;

	if (cent != NULL) {
	    /* centroid coefficient from the middle-point of the base
	     * (premultiplied with the volume):
	     * cbase = 1/12 * PI * pipe_len * pipe_len * (3*rtop^2 + 2*rtop*rbase + rbase^2)
	     */
	    cs = M_PI * lin->pipe_len * lin->pipe_len / 12.0
		* (3*(lin->pipe_rotop + lin->pipe_ritop)*(lin->pipe_rotop - lin->pipe_ritop)
		   + 2*(lin->pipe_robase*lin->pipe_rotop - lin->pipe_ribase*lin->pipe_ritop)
		   + (lin->pipe_robase + lin->pipe_ribase)*(lin->pipe_robase - lin->pipe_ribase));
	    VCOMB2(cp, crt_vol, lin->pipe_V, cs, lin->pipe_H);
	    VADD2(*cent, *cent, cp);
	}
    } else {
	struct bend_pipe *bend = (struct bend_pipe *)p;
	/* Torus Volume = 2 * PI^2 * r_bend * r_pipe^2
	 * Bend Volume = torus_area * (bend_angle / (2*PI))
	 *             = PI * bend_angle * r_bend * r_pipe^2
	 * Mass Volume = Displacement Volume - Inner Volume = PI * bend_angle * r_bend * (r_outer^2 - r_inner^2)
	 */
	crt_vol = M_PI * bend->bend_angle * bend->bend_radius * (bend->bend_or + bend->bend_ir) * (bend->bend_or - bend->bend_ir);
	*vol += crt_vol;

	if (cent != NULL) {
	    /* The centroid sits on the line between bend_V and the
	     * middle point of bend_start, at distance
	     * cos(bend->bend_angle/4)*bend->bend_radius */
	    VCOMB2(cp, 0.5, bend->bend_start, 0.5, bend->bend_end);
	    VSUB2(cp, cp, bend->bend_V);
	    VUNITIZE(cp);
	    cs = crt_vol * cos(bend->bend_angle/4) * bend->bend_radius;
	    VCOMB2(cp, crt_vol, bend->bend_V, cs, cp);
	    VADD2(*cent, *cent, cp);
	}
    }
}


void
rt_pipe_volume(fastf_t *vol, struct rt_db_internal *ip)
{
    struct bu_list head;
    point_t min, max;
    struct id_pipe *p;

    BU_LIST_INIT(&head);

    pipe_elements_calculate(&head, ip, &min, &max);

    *vol = 0;
    for (BU_LIST_FOR(p, id_pipe, &head)) {
	pipe_elem_volume_and_centroid(p, vol, NULL);
    }

    pipe_elements_free(&head);
}


void
rt_pipe_centroid(point_t *cent, struct rt_db_internal *ip)
{
    struct bu_list head;
    point_t min, max;
    struct id_pipe *p;
    fastf_t vol;

    BU_LIST_INIT(&head);

    pipe_elements_calculate(&head, ip, &min, &max);

    VSETALL(*cent, 0);
    vol = 0;

    for (BU_LIST_FOR(p, id_pipe, &head)) {
	pipe_elem_volume_and_centroid(p, &vol, cent);
    }
    VSCALE(*cent, *cent, 1/vol);
    pipe_elements_free(&head);
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
