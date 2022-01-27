/*                       C H U L L . C P P
 * BRL-CAD
 *
 * Copyright (c) 2013-2022 United States Government as represented by
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
/** @file chull.cpp
 *
 * This file implements various algorithms for finding convex hull
 * of point sets in 2D and 3D.
 */

#include "common.h"

#include <map>

#include <stdlib.h>

#include "bu/malloc.h"
#include "bu/log.h"
#include "bu/sort.h"
#include "bg/chull.h"

#include "./bg_private.h"


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
bg_polyline_2d_chull2(int** hull, const int *polyline, int n, const point2d_t* pnts)
{
    int i;

    /* initialize a deque D[] from bottom to top so that the
       1st three vertices of P[] are a ccw triangle */
    int* D = (int *)bu_calloc(2*n+1, sizeof(int), "dequeue");

    /* hull vertex counter */
    int h;

    /* initial bottom and top deque indices */
    int bot = n-2;
    int top = bot+3;

    /* 3rd vertex is a both bottom and top */
    D[top] = polyline[2];
    D[bot] = D[top];
    if (isLeft(pnts[polyline[0]], pnts[polyline[1]], pnts[polyline[2]]) > 0) {
	D[bot+1] = polyline[0];
	D[bot+2] = polyline[1];   /* ccw vertices are: 2,0,1,2 */
    } else {
	D[bot+1] = polyline[1];
	D[bot+2] = polyline[0];   /* ccw vertices are: 2,1,0,2 */
    }

    /* compute the hull on the deque D[] */
    for (i = 3; i < n; i++) {   /* process the rest of vertices */
	/* test if next vertex is inside the deque hull */
	if ((isLeft(pnts[D[bot]], pnts[D[bot+1]], pnts[polyline[i]]) > 0) &&
		(isLeft(pnts[D[top-1]], pnts[D[top]], pnts[polyline[i]]) > 0) )
	    continue;         /* skip an interior vertex */

	/* incrementally add an exterior vertex to the deque hull
	   get the rightmost tangent at the deque bot */
	while (isLeft(pnts[D[bot]], pnts[D[bot+1]], pnts[polyline[i]]) <= 0) {
	    bot = bot + 1;                      /* remove bot of deque */
	}
	D[bot-1] = polyline[i];    /* insert P[i] at bot of deque */
	bot = bot - 1;

	/* get the leftmost tangent at the deque top */
	while (isLeft(pnts[D[top-1]], pnts[D[top]], pnts[polyline[i]]) <= 0) {
	    top = top - 1;                      /* pop top of deque */
	}
	D[top+1] = polyline[i];    /* push P[i] onto top of deque */
	top = top + 1;
    }

    /* transcribe deque D[] to the output hull array hull[] */

    (*hull) = (int *)bu_calloc(top - bot + 2, sizeof(int), "hull");
    for (h=0; h <= (top-bot); h++) {
	(*hull)[h] = D[bot + h];
    }

    bu_free(D, "free queue");
    return h-1;
}


/* sorting comparison function for points */
static int
pnt_cmp_2d(point2d_t *p1, point2d_t *p2)
{
    if (UNLIKELY(NEAR_ZERO((*p2)[0] - (*p1)[0], SMALL_FASTF) && NEAR_ZERO((*p2)[1] - (*p1)[1], SMALL_FASTF))) return 0;
    if ((*p1)[0] < (*p2)[0]) return 1;
    if ((*p1)[0] > (*p2)[0]) return -1;
    if ((*p1)[1] < (*p2)[1]) return 1;
    if ((*p1)[1] > (*p2)[1]) return -1;

    /* should never get here */
    return 0;
}

/* bu_sort functions for points */
static int
pnt_compare_2d(const void *pnt1, const void *pnt2, void *UNUSED(arg))
{
    point2d_t *p1 = (point2d_t *)pnt1;
    point2d_t *p2 = (point2d_t *)pnt2;

    return pnt_cmp_2d(p1, p2);
}

/* bu_sort functions for pointers to points */
static int
pnt_compare_2d_ptr(const void *pnt1, const void *pnt2, void *UNUSED(arg))
{
    point2d_t *p1 = *(point2d_t **)pnt1;
    point2d_t *p2 = *(point2d_t **)pnt2;

    return pnt_cmp_2d(p1, p2);
}


int
bg_2d_chull2(int **hull, const point2d_t *points_2d, int n)
{
    int i = 0;
    int retval = 0;

    // Because we must sort points_2d to make a polyline for the hull builder,
    // we need to map points in their sorted positions to their original
    // indices to return a hull that references the original points_2d points.
    // We make both a sortable copy of the array for processing and a version
    // that uses individual point containers as uniquely identifiable
    // representations of those points to achieve a post-sort index lookup
    // capability via pointer mapping.
    point2d_t **pointers = (point2d_t **)bu_calloc(n + 1, sizeof(point2d_t *), "sorted points_2d pointers");
    point2d_t *psorted = (point2d_t *)bu_calloc(n + 1, sizeof(point2d_t), "sorted points_2d");
    std::map<point2d_t *, int> p2int;
    for (i = 0; i < n; i++) {
	pointers[i] = (point2d_t *)bu_calloc(1, sizeof(point2d_t), "point");
	V2MOVE(*pointers[i], points_2d[i]);
	V2MOVE(psorted[i], points_2d[i]);
	p2int[pointers[i]] = i;
    }

    bu_sort((void *)pointers, n, sizeof(point2d_t *), pnt_compare_2d_ptr, NULL);
    bu_sort((void *)psorted, n, sizeof(point2d_t), pnt_compare_2d, NULL);

    /* Once sorted, the points can be viewed as describing a simple polyline
     * and the Melkman algorithm works for a simple polyline even if it
     * isn't closed. */
    int *polyline = (int *)bu_calloc(n, sizeof(int), "polyline");
    for (i = 0; i < n; i++) {
	polyline[i] = i;
    }
    int *pchull;
    retval = bg_polyline_2d_chull2(&pchull, (const int *)polyline, n, (const point2d_t *)psorted);

    /* Map sorted indices back to original unsorted points_2d array indices */
    (*hull) = (int *)bu_calloc(retval+1, sizeof(int), "hull");
    for (i = 0; i < retval; i++) {
	(*hull)[i] = p2int[pointers[pchull[i]]];
    }

    bu_argv_free(n, pointers);
    bu_free(psorted, "free sorted points");
    bu_free(pchull, "free unmapped hull");

    return retval;
}

int
bg_2d_polyline_gc(point2d_t **opoly, int n, int *polyline, const point2d_t *pnts)
{
    if (!opoly || n <= 0 || !polyline || !pnts) {
	return -1;
    }

    point2d_t *pl = (point2d_t *)bu_calloc(n, sizeof(point2d_t), "point_2d array");
    for (int i = 0; i < n; i++) {
	V2MOVE(pl[i], pnts[polyline[i]]);
    }

    (*opoly) = pl;

    return n;
}


int
bg_3d_coplanar_chull2(int **hull, const point_t *points_3d, int n)
{
    int ret = 0;
    int hull_cnt = 0;
    point_t origin_pnt;
    vect_t u_axis, v_axis;
    point2d_t *points_tmp = (point2d_t *)bu_calloc(n + 1, sizeof(point2d_t), "points_2d");

    /* Project 3D points into temporary 2D point array.  Indices need to match up - points_3d[0]
     * needs to have its projected point in points_tmp[0], etc. */
    ret += coplanar_2d_coord_sys(&origin_pnt, &u_axis, &v_axis, points_3d, n);
    ret += coplanar_3d_to_2d(&points_tmp, (const point_t *)&origin_pnt, (const vect_t *)&u_axis, (const vect_t *)&v_axis, points_3d, n);

    if (ret) {
	bu_free(points_tmp, "temporary points");
	return 0;
    }

    hull_cnt = bg_2d_chull2(hull, (const point2d_t *)points_tmp, n);

    bu_free(points_tmp, "temporary points");

    return hull_cnt;
}


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
