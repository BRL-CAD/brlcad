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
#include "bn_private.h"

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

