/*                       C H U L L . C
 * BRL-CAD
 *
 * Copyright (c) 2013 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * Copyright 2001 softSurfer, 2012 Dan Sunday
 * This code may be freely used and modified for any purpose
 * providing that this copyright notice is included with it.
 * SoftSurfer makes no warranty for this code, and cannot be held
 * liable for any real or imagined damage resulting from its use.
 * Users of this code must verify correctness for their application.
 *
 */
/** @file chull.c
 *
 * This file implements various algorithms for finding convex hull
 * of point sets in 2D and 3D.
 */

#include "common.h"
#include <stdlib.h>

#include "bn.h"

/**
 * @brief
 * Find a 2D coordinate system for a set of co-planar 3D points
 *
 * Based on the planar normal and the vector from the center point to the
 * point furthest from that center, find vectors describing a 2D coordinate system.
 *
 * @param[out]  origin_pnt Origin of 2D coordinate system in 3 space
 * @param[out]  u_axis 3D vector describing the U axis of the 2D coordinate system in 3 space
 * @param[out]  v_axis 3D vector describing the V axis of the 2D coordinate system in 3 space
 * @param       points_3d Array of 3D points
 * @param       n the number of points in the input set
 * @return 0 if successful
 */
int
bn_coplanar_2d_coord_sys(point_t *origin_pnt, vect_t *u_axis, vect_t *v_axis, const point_t *points_3d, int n)
{
    int i = 0;
    int have_normal = 0;
    plane_t plane;
    fastf_t dist_pt_pt = 0.0;
    point_t p_farthest;
    vect_t normal;
    const struct bn_tol tol = {BN_TOL_MAGIC, BN_TOL_DIST/2, BN_TOL_DIST*BN_TOL_DIST/4, 1e-6, 1-1e-6};

    /* Step 1 - find center point */
    for (i = 0; i < n; i++) {
	VADD2(*origin_pnt, *origin_pnt, points_3d[i]);
    }
    VSCALE(*origin_pnt, *origin_pnt, 1.0/n);

    /* Step 2 - find furthest points from the center point */
    VSET(p_farthest, 0, 0, 0);
    for (i = 0; i < n; i++) {
	fastf_t curr_dist = DIST_PT_PT_SQ(*origin_pnt, points_3d[i]);
	if (curr_dist > dist_pt_pt) {
	    dist_pt_pt = curr_dist;
	    VMOVE(p_farthest, points_3d[i]);
	}
    }
    VSUB2(*u_axis, p_farthest, *origin_pnt);
    VUNITIZE(*u_axis);

    /* Step 3 - find normal vector of plane holding points */
    i = 0;
    while (!have_normal && i < n){
	if (!bn_mk_plane_3pts(plane, *origin_pnt, p_farthest, points_3d[i], &tol)) {
	    VSET(normal, plane[0], plane[1], plane[2]);
	    have_normal = 1;
	}
	i++;
    }
    if (!have_normal) return -1;
    VUNITIZE(normal);

    /* Step 4 - use vectors from steps 2 and 3 to find y axis vector */
    VCROSS(*v_axis, *u_axis, normal);
    VUNITIZE(*v_axis);

    return 0;
}

/**
 * @brief
 * Find 2D coordinates for a set of co-planar 3D points
 *
 * @param[out]  points_2d Array of parameterized 2D points
 * @param       origin_pnt Origin of 2D coordinate system in 3 space
 * @param       u_axis 3D vector describing the U axis of the 2D coordinate system in 3 space
 * @param       v_axis 3D vector describing the V axis of the 2D coordinate system in 3 space
 * @param       points_3d 3D input points
 * @param       n the number of points in the input set
 * @return 0 if successful
 */
int
bn_coplanar_3d_to_2d(point2d_t **points_2d, const point_t *origin_pnt,
	             const vect_t *u_axis, const vect_t *v_axis,
		     const point_t *points_3d, int n)
{
    int i = 0;
    for (i = 0; i < n; i++) {
	vect_t temp, c, d;
	fastf_t u, v;
	VSUB2(temp, points_3d[i], *origin_pnt);
	VPROJECT(temp, *u_axis, c, d);
	u = (VDOT(c, *u_axis) > 0) ? (MAGNITUDE(c)) : (-1 * MAGNITUDE(c));
	v = (VDOT(d, *v_axis) > 0) ? (MAGNITUDE(d)) : (-1 * MAGNITUDE(d));
        V2SET((*points_2d)[i], u, v);
    }

    return 0;
}


/**
 * @brief
 * Find 3D coordinates for a set of 2D points given a coordinate system
 *
 * @param[out]  points_3d Array of 3D points
 * @param       origin_pnt Origin of 2D coordinate system in 3 space
 * @param       u_axis 3D vector describing the U axis of the 2D coordinate system in 3 space
 * @param       v_axis 3D vector describing the V axis of the 2D coordinate system in 3 space
 * @param       points_2d 2D input points
 * @param       n the number of points in the input set
 * @return 0 if successful
 */
int
bn_coplanar_2d_to_3d(point_t **points_3d, const point_t *origin_pnt,
	             const vect_t *u_axis, const vect_t *v_axis,
		     const point2d_t *points_2d, int n)
{
int i;
    vect_t u_x_component, u_y_component, u_z_component;
    vect_t v_x_component, v_y_component, v_z_component;
    vect_t x_axis, y_axis, z_axis;
    vect_t temp;
    fastf_t mag_u_x, mag_u_y, mag_u_z;
    fastf_t mag_v_x, mag_v_y, mag_v_z;
    VSET(x_axis, 1, 0, 0);
    VSET(y_axis, 0, 1, 0);
    VSET(z_axis, 0, 0, 1);
    /* Step 1 - find the 3d X, Y and Z components of u_axis and v_axis */
    VPROJECT(*u_axis, x_axis, u_x_component, temp);
    VPROJECT(temp, y_axis, u_y_component, u_z_component);
    VPROJECT(*v_axis, x_axis, v_x_component, temp);
    VPROJECT(temp, y_axis, v_y_component, v_z_component);
    mag_u_x = (VDOT(u_x_component, x_axis) > 0) ? (MAGNITUDE(u_x_component)) : (-1 * MAGNITUDE(u_x_component));
    mag_u_y = (VDOT(u_y_component, y_axis) > 0) ? (MAGNITUDE(u_y_component)) : (-1 * MAGNITUDE(u_y_component));
    mag_u_z = (VDOT(u_z_component, z_axis) > 0) ? (MAGNITUDE(u_z_component)) : (-1 * MAGNITUDE(u_z_component));
    mag_v_x = (VDOT(v_x_component, x_axis) > 0) ? (MAGNITUDE(v_x_component)) : (-1 * MAGNITUDE(v_x_component));
    mag_v_y = (VDOT(v_y_component, y_axis) > 0) ? (MAGNITUDE(v_y_component)) : (-1 * MAGNITUDE(v_y_component));
    mag_v_z = (VDOT(v_z_component, z_axis) > 0) ? (MAGNITUDE(v_z_component)) : (-1 * MAGNITUDE(v_z_component));

    /* Step 2 - for each 2D point, calculate the (x,y,z) coordinates as follows:
     * (http://math.stackexchange.com/questions/525829/how-to-find-the-3d-coordinate-of-a-2d-point-on-a-known-plane)
     */
    for (i = 0; i < n; i++) {
	vect_t temp_2d;
	VSET(temp_2d, points_2d[i][0]*mag_u_x + (points_2d)[i][1]*mag_v_x,
		(points_2d)[i][0]*mag_u_y + (points_2d)[i][1]*mag_v_y,
		(points_2d)[i][0]*mag_u_z + (points_2d)[i][1]*mag_v_z);
	VADD2((*points_3d)[i], (*origin_pnt), temp_2d);
    }

    return 0;
}


/* isLeft(): test if a point is Left|On|Right of an infinite line.
 *    Input:  three points L0, L1, and p
 *    Return: >0 for p left of the line through L0 and L1
 *            =0 for p on the line
 *            <0 for p right of the line
 */
#define isLeft(L0, L1, p) ((L1[X] - L0[X])*(p[Y] - L0[Y]) - (p[X] - L0[X])*(L1[Y] - L0[Y]))

/* The implementation of Melkman's algorithm for convex hulls of simple
 * polylines is a translation of softSurfer's implementation:
 * http://geomalgorithms.com/a12-_hull-3.html
 */
int
bn_polyline_2d_chull(point2d_t** hull, const point2d_t* polyline, int n)
{
    int i;

    /* initialize a deque D[] from bottom to top so that the
       1st three vertices of P[] are a ccw triangle */
    point2d_t* D = (point2d_t *)bu_calloc(2*n+1, sizeof(fastf_t)*3, "dequeue");

    /* hull vertex counter */
    int h;

    /* initial bottom and top deque indices */
    int bot = n-2;
    int top = bot+3;

    /* 3rd vertex is a both bottom and top */
    V2MOVE(D[top], polyline[2]);
    V2MOVE(D[bot], D[top]);
    if (isLeft(polyline[0], polyline[1], polyline[2]) > 0) {
        V2MOVE(D[bot+1],polyline[0]);
        V2MOVE(D[bot+2],polyline[1]);   /* ccw vertices are: 2,0,1,2 */
    }
    else {
        V2MOVE(D[bot+1],polyline[1]);
        V2MOVE(D[bot+2],polyline[0]);   /* ccw vertices are: 2,1,0,2 */
    }

    /* compute the hull on the deque D[] */
    for (i = 3; i < n; i++) {   /* process the rest of vertices */
        /* test if next vertex is inside the deque hull */
        if ((isLeft(D[bot], D[bot+1], polyline[i]) > 0) &&
            (isLeft(D[top-1], D[top], polyline[i]) > 0) )
                 continue;         /* skip an interior vertex */

        /* incrementally add an exterior vertex to the deque hull
           get the rightmost tangent at the deque bot */
        while (isLeft(D[bot], D[bot+1], polyline[i]) <= 0)
            bot = bot + 1;                      /* remove bot of deque */
        V2MOVE(D[bot-1],polyline[i]);    /* insert P[i] at bot of deque */
	bot = bot - 1;

        /* get the leftmost tangent at the deque top */
        while (isLeft(D[top-1], D[top], polyline[i]) <= 0)
            top = top - 1;                      /* pop top of deque */
        V2MOVE(D[top+1],polyline[i]);    /* push P[i] onto top of deque */
	top = top + 1;
    }

    /* transcribe deque D[] to the output hull array hull[] */

    (*hull) = (point2d_t *)bu_calloc(top - bot + 2, sizeof(fastf_t)*3, "hull");
    for (h=0; h <= (top-bot); h++)
        V2MOVE((*hull)[h],D[bot + h]);

    bu_free(D, "free queue");
    return h-1;
}

/* QSort functions for points */
HIDDEN int
pnt_compare_2d(const void *pnt1, const void *pnt2)
{
    point2d_t *p1 = (point2d_t *)pnt1;
    point2d_t *p2 = (point2d_t *)pnt2;
    if (UNLIKELY(NEAR_ZERO((*p2)[0] - (*p1)[0], SMALL_FASTF) && NEAR_ZERO((*p2)[1] - (*p1)[1], SMALL_FASTF))) return 0;
    if ((*p1)[0] < (*p2)[0]) return 1;
    if ((*p1)[0] > (*p2)[0]) return -1;
    if ((*p1)[1] < (*p2)[1]) return 1;
    if ((*p1)[1] > (*p2)[1]) return -1;
    /* should never get here */
    return 0;
}


int
bn_2d_chull(point2d_t **hull, const point2d_t *points_2d, int n)
{
    int i = 0;
    int retval = 0;
    point2d_t *points = (point2d_t *)bu_calloc(n + 1, sizeof(point2d_t), "sorted points_2d");

    /* copy points_2d array to something
       that can be sorted and sort it */
    for(i = 0; i < n; i++) {
	V2MOVE(points[i], points_2d[i]);
    }

    qsort((genptr_t)points, n, sizeof(point2d_t), pnt_compare_2d);

    /* Once sorted, the points can be viewed as describing a simple polyline
     * and the Melkman algorithm works for a simple polyline even if it
     * isn't closed. */
    retval = bn_polyline_2d_chull(hull, (const point2d_t *)points, n);

    bu_free(points, "free sorted points");

    return retval;
}

int
bn_3d_coplanar_chull(point_t **hull, const point_t *points_3d, int n)
{
    int ret = 0;
    int hull_cnt = 0;
    point_t origin_pnt;
    vect_t u_axis, v_axis;
    point2d_t *hull_2d = (point2d_t *)bu_malloc(sizeof(point2d_t *), "hull pointer");
    point2d_t *points_tmp = (point2d_t *)bu_calloc(n + 1, sizeof(point2d_t), "points_2d");

    ret += bn_coplanar_2d_coord_sys(&origin_pnt, &u_axis, &v_axis, points_3d, n);
    ret += bn_coplanar_3d_to_2d(&points_tmp, (const point_t *)&origin_pnt, (const vect_t *)&u_axis, (const vect_t *)&v_axis, points_3d, n);
    if (ret) return 0;
    hull_cnt = bn_2d_chull(&hull_2d, (const point2d_t *)points_tmp, n);
    (*hull) = (point_t *)bu_calloc(hull_cnt + 1, sizeof(point_t), "hull array");
    ret = bn_coplanar_2d_to_3d(hull, (const point_t *)&origin_pnt, (const vect_t *)&u_axis, (const vect_t *)&v_axis, (const point2d_t *)hull_2d, hull_cnt);
    if (ret) return 0;
    return hull_cnt;
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

