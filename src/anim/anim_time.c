/*                     A N I M _ T I M E . C
 * BRL-CAD
 *
 * Copyright (c) 1993-2011 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 *
 */
/** @file anim_time.c
 *
 * Given an animation path consiting of time stamps and 3-d points,
 * estimate new time stamps based on the distances between points, the
 * given starting and ending times, and optionally specified starting
 * and ending velocities.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <math.h>
#include <stdio.h>

#include "bu.h"
#include "vmath.h"


#define OPT_STR "ds:e:i:f:qm:v"

#define MAXLEN 64
#define MAXITS 100
#define DELTA (1.0e-6)
#define TIME_NONE 0
#define TIME_ABSOLUTE 1
#define TIME_RELATIVE 2


/* command line variables */
fastf_t inv0, inv1;
int v0_set =	 TIME_NONE;
int v1_set =	 TIME_NONE;
int query =	 0;
int verbose = 	 0;
int maxlines = 	 0;
int domem = 	 0;
int debug = 	 0;


fastf_t
gettime(fastf_t dist, fastf_t a, fastf_t b, fastf_t c, fastf_t init)
{

    fastf_t oldtime = 0.0;
    fastf_t newtime = 0.0;
    fastf_t temp = 0.0;
    int countdown, success;

    success = 0;
    countdown = MAXITS;
    oldtime = init;
    while (countdown-->0) {
	temp = (3.0*a*oldtime+2.0*b)*oldtime+c;
	if (temp<VDIVIDE_TOL) {
	    newtime = 0.75*oldtime;
	} else {
	    newtime = oldtime - (((a*oldtime+b)*oldtime+c)*oldtime-dist)/temp;
	}
	if (((newtime-oldtime)<DELTA)&&((oldtime-newtime)<DELTA)) {
	    success = 1;
	    break;
	}
	if (debug)
	    printf("c: %d %f\t%f\n", countdown, newtime, newtime-oldtime);
	oldtime = newtime;
    }
    if (!success) fprintf(stderr, "warning - max iterations reached\n");
    return newtime;

}


int get_args(int argc, char **argv)
{
    int c;

    while ((c=bu_getopt(argc, argv, OPT_STR)) != -1) {
	switch (c) {
	    case 's':
		sscanf(bu_optarg, "%lf", &inv0);
		v0_set = TIME_ABSOLUTE;
		break;
	    case 'e':
		sscanf(bu_optarg, "%lf", &inv1);
		v1_set = TIME_ABSOLUTE;
		break;
	    case 'i':
		sscanf(bu_optarg, "%lf", &inv0);
		v0_set = TIME_RELATIVE;
		if ((inv0>3.0)||(inv0<0.0)) {
		    fprintf(stderr, "anim_time: -i argument must lie between 0.0 and 3.0\n");
		    return 0;
		}
		break;
	    case 'f':
		sscanf(bu_optarg, "%lf", &inv1);
		v1_set = TIME_RELATIVE;
		if ((inv1>3.0)||(inv1<0.0)) {
		    fprintf(stderr, "anim_time: -f argument must lie between 0.0 and 3.0\n");
		    return 0;
		}
		break;
	    case 'q':
		query = 1;
		break;
	    case 'm':
		sscanf(bu_optarg, "%d", &maxlines);
		domem = 1;
		break;
	    case 'v':
		verbose = 1;
		break;
	    case 'd':
		debug = 1;
		break;
	    default:
		fprintf(stderr, "Unknown option: -%c\n", c);
		return 0;
	}
    }
    return 1;
}


int
main(int argc, char **argv)
{
    fastf_t *l, *x, *y, *z;
    fastf_t temp0, temp1, temp2, start=0.0, end, v0, v1;
    int i, j, num, plen;


    fastf_t time, dist, slope, a, b, c;


    plen = 0;

    if (!get_args(argc, argv)) {
	fprintf(stderr, "Usage: anim_time [-s#] [-e#] [-d] < in.table\n");
	return 1;
    }

    if (!domem) {
	maxlines = MAXLEN;
    }

    l = (fastf_t *) bu_malloc(maxlines*sizeof(fastf_t), "l[]");
    if (verbose) {
	x = (fastf_t *) bu_malloc(maxlines*sizeof(fastf_t), "x[]");
	y = (fastf_t *) bu_malloc(maxlines*sizeof(fastf_t), "y[]");
	z = (fastf_t *) bu_malloc(maxlines*sizeof(fastf_t), "z[]");
    } else {
	x = (fastf_t *) bu_malloc(2*sizeof(fastf_t), "x[]");
	y = (fastf_t *) bu_malloc(2*sizeof(fastf_t), "y[]");
	z = (fastf_t *) bu_malloc(2*sizeof(fastf_t), "z[]");
    }
    l[0] = 0.0;

    while (plen<maxlines) {
	i = (verbose) ? plen : plen%2;
	j = (verbose) ? (plen-1) : (plen+1)%2;
	num = scanf("%lf %lf %lf %lf", &end, x+i, y+i, z+i);
	if (num<4)
	    break;
	if (plen) {
	    temp0 = x[i]-x[j];
	    temp1 = y[i]-y[j];
	    temp2 = z[i]-z[j];
	    l[plen] = sqrt(temp0*temp0+temp1*temp1+temp2*temp2);
	    l[plen] += l[plen-1];
	} else {
	    start = end;
	}

	plen++;
    }

    time = end - start;
    dist = l[plen-1];

    if (query) {
	printf("%f\n", dist);
	return 0;
    }

    if (time < VDIVIDE_TOL) {
	fprintf(stderr, "anim_time: time too small. Only %f s.\n", time);
	return 10;
    }
    if (dist < VDIVIDE_TOL) {
	fprintf(stderr, "anim_time: pathlength too small. Only %f\n", dist);
	return 10;
    }
    slope = dist/time;

    switch (v0_set) {
	case TIME_ABSOLUTE:
	    v0 = inv0;
	    break;
	case TIME_RELATIVE:
	    v0 = slope*inv0;
	    break;
	default:
	case TIME_NONE:
	    v0 = slope;
	    break;
    }

    switch (v1_set) {
	case TIME_ABSOLUTE:
	    v1 = inv1;
	    break;
	case TIME_RELATIVE:
	    v1 = slope*inv1;
	    break;
	default:
	case TIME_NONE:
	    v1 = slope;
	    break;
    }
    if (v0<0.0) {
	fprintf(stderr, "anim_time: Start velocity must be non-negative.\n");
	return 1;
    }
    if (v1<0.0) {
	fprintf(stderr, "anim_time: End velocity must be non-negative.\n");
	return 1;
    }
    if (v0>3*slope) {
	fprintf(stderr, "anim_time: Start velocity must be not be greater than %f units/s for this path.\n", 3.0*slope);
	return 1;
    }
    if (v1>3*slope) {
	fprintf(stderr, "anim_time: End velocity must not be greater than %f for this path.\n", 3.0*slope);
	return 1;
    }

    a = ((v1+v0) - 2.0*slope)/(time*time);
    b = (3*slope - (v1+2.0*v0))/time;
    c = v0;

    temp2 = 1.0/slope;
    if (verbose) {
	printf("%.12e\t%.12e\t%.12e\t%.12e\n", start, x[0], y[0], z[0]);
	for (i=1; i<plen-1; i++) {
	    temp0 = gettime(l[i], a, b, c, l[i]*temp2);
	    printf("%.12e\t%.12e\t%.12e\t%.12e\n", temp0+start, x[i], y[i], z[i]);
	}
	printf("%.12e\t%.12e\t%.12e\t%.12e\n", end, x[plen-1], y[plen-1], z[plen-1]);
    } else {
	printf("%.12e\n", start);
	for (i=1; i<plen-1; i++) {
	    temp0 = gettime(l[i], a, b, c, l[i]*temp2);
	    printf("%.12e\n", temp0+start);
	}
	printf("%.12e\n", end);
    }

    bu_free((char *) l, "l[]");
    bu_free((char *) x, "x[]");
    bu_free((char *) y, "y[]");
    bu_free((char *) z, "z[]");
    return 0;
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
