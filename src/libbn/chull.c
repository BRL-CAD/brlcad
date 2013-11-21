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
bn_polyline_2d_hull(point_t** hull, const point_t* polyline, int n)
{
    int i;

    /* initialize a deque D[] from bottom to top so that the
       1st three vertices of P[] are a ccw triangle */
    point_t* D = (point_t *)bu_calloc(2*n+1, sizeof(fastf_t)*3, "dequeue");

    /* hull vertex counter */
    int h;

    /* initial bottom and top deque indices */
    int bot = n-2;
    int top = bot+3;

    /* 3rd vertex is a both bot and top */
    VMOVE(D[top], polyline[2]);
    VMOVE(D[bot], D[top]);
    if (isLeft(polyline[0], polyline[1], polyline[2]) > 0) {
        VMOVE(D[bot+1],polyline[0]);
        VMOVE(D[bot+2],polyline[1]);   /* ccw vertices are: 2,0,1,2 */
    }
    else {
        VMOVE(D[bot+1],polyline[1]);
        VMOVE(D[bot+2],polyline[0]);   /* ccw vertices are: 2,1,0,2 */
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
        VMOVE(D[bot-1],polyline[i]);    /* insert P[i] at bot of deque */
	bot = bot - 1;

        /* get the leftmost tangent at the deque top */
        while (isLeft(D[top-1], D[top], polyline[i]) <= 0)
            top = top - 1;                      /* pop top of deque */
        VMOVE(D[top+1],polyline[i]);    /* push P[i] onto top of deque */
	top = top + 1;
    }

    /* transcribe deque D[] to the output hull array hull[] */

    (*hull) = (point_t *)bu_calloc(top - bot + 2, sizeof(fastf_t)*3, "hull");
    for (h=0; h <= (top-bot); h++)
        VMOVE((*hull)[h],D[bot + h]);

    bu_free(D, "free queue");
    return h-1;
}

/* QSort functions for points */
HIDDEN int
pnt_compare(const void *pnt1, const void *pnt2)
{
    point_t *p1 = (point_t *)pnt1;
    point_t *p2 = (point_t *)pnt2;
    if (UNLIKELY(NEAR_ZERO((*p2)[0] - (*p1)[0], SMALL_FASTF) && NEAR_ZERO((*p2)[1] - (*p1)[1], SMALL_FASTF) && NEAR_ZERO((*p2)[2] - (*p1)[2], SMALL_FASTF))) return 0;
    if ((*p1)[0] < (*p2)[0]) return -1;
    if ((*p1)[0] > (*p2)[0]) return 1;
    if ((*p1)[1] < (*p2)[1]) return -1;
    if ((*p1)[1] > (*p2)[1]) return 1;
    if ((*p1)[2] < (*p2)[2]) return -1;
    if ((*p1)[2] > (*p2)[2]) return 1;
    /* should never get here */
    return 0;
}


/* The implementation of the Monotone Chain algorithm for
 * convex hulls of 2D point sets is a translation of
 * softSurfer's C++ implementation:
 * http://geomalgorithms.com/a10-_hull-1.html
 */
int
bn_2d_hull(point_t **hull, const point_t *pnts, int n)
{
    int i = 0;
    int bot = 0;
    int top = -1;
    int minmin = 0;
    int minmax = 0;
    int maxmin = n-1;
    int maxmax = n-1;
    float xmin = 0.0;
    float xmax = 0.0;

    point_t *points = (point_t *)bu_calloc(n + 1, sizeof(point_t), "sorted pnts");

    /* first thing, copy pnts array to something
       that can be sorted and sort it */
    for(i = 0; i < n; i++) {
	VMOVE(points[i], pnts[i]);
    }

    qsort((genptr_t)points, n, sizeof(point_t), pnt_compare);

    /* the output hull array will be used as the stack */
    (*hull) = (point_t *)bu_calloc(n + 1, sizeof(point_t), "hull array");

    /* Get the indices of points with min x-coord and min|max y-coord */
    xmin = points[0][0];
    for (i = 1; i < n; i++)
        if (!NEAR_ZERO(points[i][0] - xmin, SMALL_FASTF)) break;
    minmax = i - 1;
    if (minmax == n-1) {       /* degenerate case: all x-coords == xmin */
	top = top + 1;
        VMOVE((*hull)[top], points[minmin]);
        if (!NEAR_ZERO(points[minmax][1] - points[minmin][1], SMALL_FASTF)){ /* a  nontrivial segment */
	    top = top + 1;
	    VMOVE((*hull)[top],points[minmax]);
	}
	top = top + 1;
	VMOVE((*hull)[top],points[minmax]);
	return top+1;
    }

    /* Get the indices of points with max x-coord and min|max y-coord */
    xmax = points[n-1][0];
    for (i = n-2; i >= 0; i--)
        if (!NEAR_ZERO(points[i][0] - xmax, SMALL_FASTF)) break;
    maxmin = i+1;

    /* Compute the lower hull on the stack H */
    top = top + 1;
    VMOVE((*hull)[top],points[minmin]);      /* push  minmin point onto stack */
    i = minmax;
    while (i <= maxmin)
    {
	i = i + 1;
        /* the lower line joins points[minmin]  with points[maxmin] */
        if (isLeft( points[minmin], points[maxmin], points[i])  >= 0 && i < maxmin)
            continue;           /* ignore points[i] above or on the lower line */

        while (top > 0)         /* there are at least 2 points on the stack */
        {
            /* test if  points[i] is left of the line at the stack top */
            if (isLeft(  (*hull)[top-1], (*hull)[top], points[i]) > 0)
                 break;         /* points[i] is a new hull  vertex */
            else
                 top--;         /* pop top point off  stack */
        }
	top = top + 1;
	VMOVE((*hull)[top],points[i]);        /* push points[i] onto stack */
    }

    /* Next, compute the upper hull on the stack H above  the bottom hull */
    if (maxmax != maxmin) {     /* if  distinct xmax points */
	top = top + 1;
	VMOVE((*hull)[top],points[maxmax]);  /* push maxmax point onto stack */
    }
    bot = top;                  /* the bottom point of the upper hull stack */
    i = maxmin;
    while (i >= minmax)
    {
	i = i - 1;
        /* the upper line joins points[maxmax]  with points[minmax] */
        if (isLeft( points[maxmax], points[minmax], points[i])  >= 0 && i > minmax)
            continue;           /* ignore points[i] below or on the upper line */

        while (top > bot)     /* at least 2 points on the upper stack */
        {
            /* test if  points[i] is left of the line at the stack top */
            if (isLeft(  (*hull)[top-1], (*hull)[top], points[i]) > 0)
                 break;         /* points[i] is a new hull  vertex */
            else
		top = top - 1;  /* pop top point off stack */
        }
	top = top + 1;
        VMOVE((*hull)[top],points[i]);        /* push points[i] onto stack */
    }
    if (minmax != minmin) {
	top = top + 1;
        VMOVE((*hull)[top],points[minmin]);  /* push  joining endpoint onto stack */
    }

    bu_free(points, "free sorted points");

    return top+1;
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

