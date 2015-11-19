/*                        B E Z I E R . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2014 United States Government as represented by
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
/** @addtogroup ray */
/** @{ */
/** @file librt/bezier.c
 *
 * The following routines are for 2D Bezier curves.
 *
 * The following routines are borrowed from Graphics Gems I, Academic
 * Press, Inc., 1990, Andrew S. Glassner (editor), "A Bezier
 * Curve-based Root-finder", Philip J. Schneider.
 *
 * Modifications have been made for inclusion in BRL-CAD and to
 * generalize the codes for finding intersections with any 2D line
 * rather than just the X-axis.
 */

#include "common.h"

#include "bio.h"

#include "vmath.h"
#include "raytrace.h"
#include "rt/nurb.h"

#include "./librt_private.h"


#define SGN(_x) (((_x)<0) ? -1 : 1)
#define MAXDEPTH 64

/*
 * Count the number of times a Bezier control polygon
 * crosses the ray. This number is >= the number of roots.
 */
HIDDEN int
crossing_count(
    point2d_t *V,		/* 2D Control pts of Bezier curve */
    int degree,                 /* Degree of Bezier curve */
    point2d_t ray_start,	/* starting point for ray */
    point2d_t ray_perp)		/* unit vector perpendicular to ray direction */
{
    int i;
    int n_crossings = 0;        /* Number of crossings */
    int sign, old_sign;         /* Sign of coefficients */
    point2d_t to_pt;		/* vector from ray start to a control point */

    V2SUB2(to_pt, ray_start, V[0]);
    sign = old_sign = SGN(V2DOT(to_pt, ray_perp));
    for (i = 1; i <= degree; i++) {
	V2SUB2(to_pt, ray_start, V[i]);
	sign = SGN(V2DOT(to_pt, ray_perp));
	if (sign != old_sign) n_crossings++;
	old_sign = sign;
    }

    return n_crossings;
}


/*
 * Check if the control polygon of a Bezier curve is flat enough for
 * recursive subdivision to bottom out.
 */
HIDDEN int
control_polygon_flat_enough(
    point2d_t *V,		/* Control points */
    int degree,			/* Degree of polynomial */
    fastf_t epsilon)		/* Maximum allowable error */
{
    int i;			/* Index variable */
    double *distance;		/* Distances from pts to line */
    double max_distance_above;	/* maximum of these */
    double max_distance_below;
    double error;		/* Precision of root */
    double intercept_1,
	intercept_2,
	left_intercept,
	right_intercept;
    double a, b, c;		/* Coefficients of implicit */
    /* eqn for line from V[0]-V[deg]*/

    /* Find the perpendicular distance */
    /* from each interior control point to */
    /* line connecting V[0] and V[degree] */
    distance = (double *)bu_malloc((unsigned)(degree + 1) * sizeof(double), "control_polygon_flat_enough");
    {
	double abSquared;

	/* Derive the implicit equation for line connecting first */
	/* and last control points */
	a = V[0][Y] - V[degree][Y];
	b = V[degree][X] - V[0][X];
	c = V[0][X] * V[degree][Y] - V[degree][X] * V[0][Y];

	abSquared = 1.0 / ((a * a) + (b * b));

	for (i = 1; i < degree; i++) {
	    /* Compute distance from each of the points to that line */
	    distance[i] = a * V[i][X] + b * V[i][Y] + c;
	    if (distance[i] > 0.0) {
		distance[i] = (distance[i] * distance[i]) * abSquared;
	    }
	    if (distance[i] < 0.0) {
		distance[i] = -((distance[i] * distance[i]) * abSquared);
	    }
	}
    }


    /* Find the largest distance */
    max_distance_above = 0.0;
    max_distance_below = 0.0;
    for (i = 1; i < degree; i++) {
	if (distance[i] < 0.0) {
	    max_distance_below = FMIN(max_distance_below, distance[i]);
	};
	if (distance[i] > 0.0) {
	    max_distance_above = FMAX(max_distance_above, distance[i]);
	}
    }
    bu_free((char *)distance, "control_polygon_flat_enough");

    {
	double det, dInv;
	double a1, b1, c1, a2, b2, c2;

	if (NEAR_ZERO(a, VUNITIZE_TOL)) {
	    a1 = 1.0;
	    b1 = 1.0;
	    c1 = 0.0;
	} else {
	    /* Implicit equation for zero line */
	    a1 = 0.0;
	    b1 = 1.0;
	    c1 = 0.0;
	}

	/* Implicit equation for "above" line */
	a2 = a;
	b2 = b;
	c2 = c + max_distance_above;

	det = a1 * b2 - a2 * b1;
	dInv = 1.0/det;

	intercept_1 = (b1 * c2 - b2 * c1) * dInv;

	/* Implicit equation for "below" line */
	a2 = a;
	b2 = b;
	c2 = c + max_distance_below;

	det = a1 * b2 - a2 * b1;
	dInv = 1.0/det;

	intercept_2 = (b1 * c2 - b2 * c1) * dInv;
    }

    /* Compute intercepts of bounding box */
    left_intercept = FMIN(intercept_1, intercept_2);
    right_intercept = FMAX(intercept_1, intercept_2);

    error = 0.5 * (right_intercept-left_intercept);

    if (error < epsilon) {
	return 1;
    } else {
	return 0;
    }
}


void
bezier(
    point2d_t *V,		/* Control pts */
    int degree,			/* Degree of bezier curve */
    double t,			/* Parameter value [0..1]	*/
    point2d_t *Left,		/* RETURN left half ctl pts */
    point2d_t *Right,		/* RETURN right half ctl pts */
    point2d_t eval_pt,		/* RETURN evaluated point */
    point2d_t normal)		/* RETURN unit normal at evaluated pt (may be NULL) */
{
    int i, j;			/* Index variables */
    fastf_t len;
    point2d_t tangent;
    point2d_t **Vtemp;


    Vtemp = (point2d_t **)bu_calloc(degree+1, sizeof(point2d_t *), "bezier: Vtemp array");
    for (i=0; i<=degree; i++)
	Vtemp[i] = (point2d_t *)bu_calloc(degree+1, sizeof(point2d_t),
					  "bezier: Vtemp[i] array");
    /* Copy control points */
    for (j =0; j <= degree; j++) {
	V2MOVE(Vtemp[0][j], V[j]);
    }

    /* Triangle computation */
    for (i = 1; i <= degree; i++) {
	for (j =0; j <= degree - i; j++) {
	    Vtemp[i][j][X] =
		(1.0 - t) * Vtemp[i-1][j][X] + t * Vtemp[i-1][j+1][X];
	    Vtemp[i][j][Y] =
		(1.0 - t) * Vtemp[i-1][j][Y] + t * Vtemp[i-1][j+1][Y];
	}
    }

    if (Left != NULL) {
	for (j = 0; j <= degree; j++) {
	    V2MOVE(Left[j], Vtemp[j][0]);
	}
    }
    if (Right != NULL) {
	for (j = 0; j <= degree; j++) {
	    V2MOVE(Right[j], Vtemp[degree-j][j]);
	}
    }

    V2MOVE(eval_pt, Vtemp[degree][0]);

    if (normal) {
	V2SUB2(tangent, Vtemp[degree-1][1], Vtemp[degree-1][0]);
	normal[X] = tangent[Y];
	normal[Y] = -tangent[X];
	len = sqrt(MAG2SQ(normal));
	normal[X] /= len;
	normal[Y] /= len;
    }

    for (i=0; i<=degree; i++)
	bu_free((char *)Vtemp[i], "bezier: Vtemp[i]");
    bu_free((char *)Vtemp, "bezier: Vtemp");

    return;
}


/*
 * Compute intersection of chord from first control point to last
 * with ray.
 *
 * Returns :
 *
 * 0 - no intersection
 * 1 - found an intersection
 *
 * intercept - contains calculated intercept
 */
HIDDEN int
compute_x_intercept(
    point2d_t *V,               /* Control points */
    int degree,                 /* Degree of curve */
    point2d_t ray_start,	/* starting point of ray */
    point2d_t ray_dir,		/* unit ray direction */
    point2d_t intercept,	/* calculated intercept point */
    point2d_t normal)		/* calculated unit normal at intercept */
{
    fastf_t beta;
    fastf_t denom;
    fastf_t len;
    point2d_t seg_line;

    denom = (V[degree][X] - V[0][X]) * ray_dir[Y] -
	(V[degree][Y] - V[0][Y]) * ray_dir[X];

    if (ZERO(denom))
	return 0;

    beta = (V[0][Y] * ray_dir[X] - V[0][X] * ray_dir[Y] +
	    ray_start[X] * ray_dir[Y] - ray_start[Y] * ray_dir[X]) / denom;

    if (beta < 0.0 || beta > 1.0)
	return 0;

    V2SUB2(seg_line, V[degree], V[0]);
    V2JOIN1(intercept, V[0], beta, seg_line);

    /* calculate normal */
    normal[X] = seg_line[Y];
    normal[Y] = -seg_line[X];
    len = sqrt(MAG2SQ(seg_line));
    normal[X] /= len;
    normal[Y] /= len;

    return 1;
}


int
bezier_roots(
    point2d_t *w,               /* The control points */
    int degree,         	/* The degree of the polynomial */
    point2d_t **intercept,	/* list of intersections found */
    point2d_t **normal,		/* corresponding normals */
    point2d_t ray_start,	/* starting point of ray */
    point2d_t ray_dir,		/* Unit direction for ray */
    point2d_t ray_perp,		/* Unit vector normal to ray_dir */
    int depth,          	/* The depth of the recursion */
    fastf_t epsilon)		/* maximum allowable error */
{
    int i;
    point2d_t *Left,            /* New left and right */
	*Right;                 /* control polygons */
    int left_count,             /* Solution count from */
	right_count;            /* children */
    point2d_t *left_t,          /* Solutions from kids */
	*right_t;
    point2d_t *left_n,		/* normals from kids */
	*right_n;
    int total_count;
    point2d_t eval_pt;

    switch (crossing_count(w, degree, ray_start, ray_perp)) {
	case 0 : {
	    /* No solutions here */
	    return 0;
	}
	case 1 : {
	    /* Unique solution */
	    /* Stop recursion when the tree is deep enough */
	    /* if deep enough, return 1 solution at midpoint */
	    if (depth >= MAXDEPTH) {
		BU_ALLOC(*intercept, point2d_t);
		BU_ALLOC(*normal, point2d_t);
		bezier(w, degree, 0.5, NULL, NULL, *intercept[0], *normal[0]);
		return 1;
	    }
	    if (control_polygon_flat_enough(w, degree, epsilon)) {
		BU_ALLOC(*intercept, point2d_t);
		BU_ALLOC(*normal, point2d_t);
		if (!compute_x_intercept(w, degree, ray_start, ray_dir, *intercept[0], *normal[0])) {
		    bu_free((char *)(*intercept), "bezier_roots: no solution");
		    bu_free((char *)(*normal), "bezier_roots: no solution");
		    return 0;
		}
		return 1;
	    }
	    break;
	}
    }

    /* Otherwise, solve recursively after */
    /* subdividing control polygon */
    Left = (point2d_t *)bu_calloc(degree+1, sizeof(point2d_t), "bezier_roots: Left");
    Right = (point2d_t *)bu_calloc(degree+1, sizeof(point2d_t), "bezier_roots: Right");
    bezier(w, degree, 0.5, Left, Right, eval_pt, NULL);

    left_count  = bezier_roots(Left,  degree, &left_t, &left_n, ray_start, ray_dir, ray_perp, depth+1, epsilon);
    right_count = bezier_roots(Right, degree, &right_t, &right_n, ray_start, ray_dir, ray_perp, depth+1, epsilon);

    total_count = left_count + right_count;

    bu_free((char *)Left, "bezier_roots: Left");
    bu_free((char *)Right, "bezier_roots: Right");
    if (total_count) {
	*intercept = (point2d_t *)bu_calloc(total_count, sizeof(point2d_t),
					    "bezier_roots: roots compilation");
	*normal = (point2d_t *)bu_calloc(total_count, sizeof(point2d_t),
					 "bezier_roots: normal compilation");
    }

    /* Gather solutions together */
    for (i = 0; i < left_count; i++) {
	V2MOVE((*intercept)[i], left_t[i]);
	V2MOVE((*normal)[i], left_n[i]);
    }
    for (i = 0; i < right_count; i++) {
	V2MOVE((*intercept)[i+left_count], right_t[i]);
	V2MOVE((*normal)[i+left_count], right_n[i]);
    }

    if (left_count) {
	bu_free((char *)left_t, "Left roots");
	bu_free((char *)left_n, "Left normals");
    }
    if (right_count) {
	bu_free((char *)right_t, "Right roots");
	bu_free((char *)right_n, "Right normals");
    }

    /* Send back total number of solutions */
    return total_count;
}


struct bezier_2d_list *
bezier_subdivide(struct bezier_2d_list *bezier_in, int degree, fastf_t epsilon, int depth)
{
    struct bezier_2d_list *bz_l, *bz_r, *new_head;
    struct bezier_2d_list *left_rtrn, *rt_rtrn;
    point2d_t pt;

    /* create a new head */
    BU_ALLOC(new_head, struct bezier_2d_list);

    BU_LIST_INIT(&new_head->l);
    if (depth >= MAXDEPTH) {
	BU_LIST_APPEND(&new_head->l, &bezier_in->l);
	return new_head;
    }

    if (control_polygon_flat_enough(bezier_in->ctl, degree, epsilon)) {
	BU_LIST_APPEND(&new_head->l, &bezier_in->l);
	return new_head;
    }

    /* allocate memory for left and right curves */
    BU_ALLOC(bz_l, struct bezier_2d_list);
    BU_LIST_INIT(&bz_l->l);
    BU_ALLOC(bz_r, struct bezier_2d_list);
    BU_LIST_INIT(&bz_r->l);
    bz_l->ctl = (point2d_t *)bu_calloc(degree + 1, sizeof(point2d_t),
				       "bezier_subdivide: bz_l->ctl");
    bz_r->ctl = (point2d_t *)bu_calloc(degree + 1, sizeof(point2d_t),
				       "bezier_subdivide: bz_r->ctl");

    /* subdivide at t = 0.5 */
    bezier(bezier_in->ctl, degree, 0.5, bz_l->ctl, bz_r->ctl, pt, NULL);

    /* eliminate original */
    BU_LIST_DEQUEUE(&bezier_in->l);
    bu_free((char *)bezier_in->ctl, "bezier_subdivide: bezier_in->ctl");
    bu_free((char *)bezier_in, "bezier_subdivide: bezier_in");

    /* recurse on left curve */
    left_rtrn = bezier_subdivide(bz_l, degree, epsilon, depth+1);

    /* recurse on right curve */
    rt_rtrn = bezier_subdivide(bz_r, degree, epsilon, depth+1);

    BU_LIST_APPEND_LIST(&new_head->l, &left_rtrn->l);
    BU_LIST_APPEND_LIST(&new_head->l, &rt_rtrn->l);
    bu_free((char *)left_rtrn, "bezier_subdivide: left_rtrn (head)");
    bu_free((char *)rt_rtrn, "bezier_subdivide: rt_rtrn (head)");

    return new_head;
}


/** @} */
/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
