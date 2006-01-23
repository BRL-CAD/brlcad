/*                      A N I M _ F L Y . C
 * BRL-CAD
 *
 * Copyright (c) 1993-2006 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this file; see the file named COPYING for more
 * information.
 *
 */
/** @file anim_fly.c
 *	Simulate flying motion, for an airplane or virtual camera.
 *
 *  This filter operates on animation tables. Given the desired position
 *  of the airplane in each frame, anim_fly produces a table including the
 *  plane's position and orientation. A "magic factor" should be supplied
 *  to control the severity of banking. Looping behavior can be toggled
 *  with another option.
 *
 *
 *  Author -
 *	Carl J. Nuzman
 *
 *  Source -
 *      The U. S. Army Research Laboratory
 *      Aberdeen Proving Ground, Maryland  21005-5068  USA
 */
#include "common.h"

#include <math.h>
#include <stdio.h>

#include "machine.h"
#include "bu.h"
#include "vmath.h"
#include "anim.h"

#ifndef M_PI
#define M_PI	3.14159265358979323846
#endif

#define MAXN	100

int estimate_f = 0;
fastf_t max_bank = 0;
fastf_t max_cross = 0;
int loop = 1;
int print_int = 1;
fastf_t magic_factor = 1.0;
fastf_t desired_step = 0.1;

int get_args(int argc, char **argv);

#define PREP	-1
#define START	0
#define MIDDLE	1
#define WANE	2
#define	END	3
#define STOP	4

int
main(int argc, char **argv)
{
    int count, status, num_read, enn, i, pp;
    fastf_t *points, *cur;
    fastf_t yaw, pch, rll, stepsize, first[4], second[4];
    fastf_t f_prm_0(fastf_t x0, fastf_t x1, fastf_t x2, fastf_t h), f_prm_1(fastf_t x0, fastf_t x1, fastf_t x2, fastf_t h), f_prm_2(fastf_t x0, fastf_t x1, fastf_t x2, fastf_t h);
    void get_orientation(fastf_t *p0, fastf_t *p1, fastf_t *p2, fastf_t (*function) (/* ??? */), fastf_t *p_yaw, fastf_t *p_pch, fastf_t *p_rll);

    yaw = pch = rll = 0.0;

    if (!get_args(argc,argv))
	fprintf(stderr,"Anim_fly: Get_args error");

    /* read first two lines of table to determine the time step used */
    /* (a constant time step is assumed throughout the rest of the file)*/
    scanf("%lf %lf %lf %lf", first, first+1, first+2, first+3);
    scanf("%lf %lf %lf %lf", second, second+1, second+2, second+3);
    stepsize = second[0]-first[0];

    /* determine n, the number of points to store ahead and behind
     * the current point. 2n points are stored, minimum enn=2 */
    enn = (int) (desired_step/stepsize);
    if (enn>MAXN) enn=MAXN;
    if (enn<1) enn=1;

    /* allocate storage */
    points = (fastf_t *) bu_calloc((3*enn+1)*4, sizeof(fastf_t), "points");

    /* read the first 3n points into the storage array*/
    VMOVEN(points+4, first, 4);
    VMOVEN(points+8, second, 4);
    num_read=4; /* in order to pass test if n=1 */
    for (cur=points+12; cur<points+(4*(3*enn+1)); cur+=4){
	num_read=scanf("%lf %lf %lf %lf", cur,cur+1,cur+2,cur+3);
    }
    if (num_read<4){
	fprintf(stderr,"Anim_fly: Not enough lines in input table.\n");
	fprintf(stderr,"Increase number of lines or reduce the minimum stepsize with -s.\n");
	fprintf(stderr,"Currently the minumum step size is %g seconds.\n",desired_step);
	exit(0);
    }

    max_cross = 0;
    count = 0;
    pp = 0;
    status = START;
    while (status != STOP) {
	switch (status) {
	case START: /* first n points */
	    pp += 4;
	    get_orientation(points+pp,points+pp+4*enn,points+pp+4*2*enn, f_prm_0, &yaw, &pch, &rll);
	    if (!(count%print_int)&&!estimate_f) {
		printf("%.10g\t%.10g\t%.10g\t%.10g\t%.10g\t%.10g\t%.10g\n",points[pp+0],points[pp+1],points[pp+2],points[pp+3],yaw,pch,rll);
	    }
	    if (pp >= 4*enn)
		status=MIDDLE;
	    break;
	case MIDDLE: /* middle points (at least one)*/
	    for (i=0; i<3*enn*4; i++){
		VMOVEN(points+(4*i), points+(4*(i+1)), 4);
	    }
	    num_read=scanf("%lf %lf %lf %lf", points+(4*(3*enn)),points+(4*(3*enn)+1),points+(4*(3*enn)+2),points+(4*(3*enn)+3));
	    if (num_read < 4) {
		pp = 0;
		status = WANE;
	    }
	    get_orientation(points,points+(4*enn),points+4*(2*enn), f_prm_1, &yaw, &pch, &rll);
	    if (!(count%print_int)&&!estimate_f) {
		printf("%.10g\t%.10g\t%.10g\t%.10g\t%.10g\t%.10g\t%.10g\n",points[4*(enn)+0],points[4*(enn)+1],points[4*(enn)+2],points[4*(enn)+3],yaw,pch,rll);
	    }
	    break;
	case WANE: /* last n - 1 middle points */
	    pp += 4;
	    if (pp >= 4*enn){
		status = END;
		count--;
		pp = 0;
		break;
	    }
	    get_orientation(points+pp,points+pp+4*enn,points+pp+4*2*enn, f_prm_1, &yaw, &pch, &rll);
	    if (!(count%print_int)&&!estimate_f) {
		printf("%.10g\t%.10g\t%.10g\t%.10g\t%.10g\t%.10g\t%.10g\n",points[4*(enn)+0+pp],points[4*(enn)+1+pp],points[4*(enn)+2+pp],points[4*(enn)+3+pp],yaw,pch,rll);
	    }
	    break;
	case END: /* last n points */
	    get_orientation(points+pp,points+pp+4*enn,points+pp+4*2*enn, f_prm_2, &yaw, &pch, &rll);
	    if (!(count%print_int)&&!estimate_f) {
		printf("%.10g\t%.10g\t%.10g\t%.10g\t%.10g\t%.10g\t%.10g\n",points[8*enn+pp+0],points[8*enn+pp+1],points[8*enn+pp+2],points[8*enn+pp+3],yaw,pch,rll);
	    }
	    pp += 4;
	    if (pp >= 4*enn)
		status = STOP;
	    break;
	}
	count++;


    }

    /* Return the factor needed to achieve the requested max_bank */
    if (estimate_f){
	if (max_cross < VDIVIDE_TOL) {
	    printf("%.10g\n",0.0);
	} else {
	    printf("%.10g\n", 1000.0 * max_bank/max_cross);
	}
    }
    bu_free(points, "points");
    return( 0 );
}

void
get_orientation(fastf_t *p0, fastf_t *p1, fastf_t *p2, fastf_t (*function) (/* ??? */), fastf_t *p_yaw, fastf_t *p_pch, fastf_t *p_rll)
{
    int i;
    fastf_t step,vel[3],accel[3];
    fastf_t f_double_prm(fastf_t x0, fastf_t x1, fastf_t x2, fastf_t h),xyz2yaw(fastf_t *d),xyz2pch(fastf_t *d),bank(fastf_t *acc, fastf_t *vel);

    static fastf_t last_yaw;
    static int not_first_time, upside_down;

    step = p2[0] - p1[0];
    for (i=1;i<4;i++) {
	vel[i-1] = (*function)(p0[i],p1[i],p2[i],step);
	accel[i-1] = f_double_prm(p0[i],p1[i],p2[i],step);
    }
    *p_yaw = xyz2yaw(vel);
    *p_pch = xyz2pch(vel);
    *p_rll = bank(accel,vel);

    if (fabs(*p_pch)==90.0) /* don't change yaw if velocity vertical */
	*p_yaw = last_yaw;

    /* avoid sudden yaw changes in vertical loops */
    if (not_first_time&&loop){
	if ((fabs(last_yaw - *p_yaw)<181.0)&&(fabs(last_yaw - *p_yaw)>179.0))
	    upside_down = (upside_down) ? 0 : 1;
	if (upside_down)
	    (*p_rll) += 180;
    }

    last_yaw = *p_yaw;
    not_first_time = 1;
}

/* determine the yaw of the given direction vector */
fastf_t	xyz2yaw(fastf_t *d)
{
    fastf_t yaw;
    yaw = RTOD*atan2(d[1],d[0]);
    if (yaw < 0.0) yaw += 360.0;
    return yaw;
}

/* determine the pitch of the given direction vector */
fastf_t	xyz2pch(fastf_t *d)
{
    fastf_t x;
    x = sqrt(d[0]*d[0] + d[1]*d[1]);
    return (RTOD*atan2(d[2],x));

}

/* given the 3-d velocity and acceleration of an imaginary aircraft,
   find the amount of bank the aircraft would need to undergo.
   Algorithm: the bank angle is proportional to the cross product
   of the horizontal velocity and horizontal acceleration, up to a
   maximum bank of 90 degrees in either direction. */
fastf_t bank(fastf_t *acc, fastf_t *vel)
{
    fastf_t cross;

    cross = vel[1]*acc[0] - vel[0]*acc[1];

    if (estimate_f) {
	max_cross = ( fabs(cross) > max_cross) ? fabs(cross) : max_cross;
    }

    cross *= magic_factor;

    if (cross > 90) cross = 90;
    if (cross < -90) cross = -90;
    return cross;
}

/* given f(t), f(t+h), f(t+2h), and h, calculate f'(t) */
fastf_t f_prm_0(fastf_t x0, fastf_t x1, fastf_t x2, fastf_t h)
{
    return  -(3.0*x0 - 4.0*x1 + x2)/(2*h);
}

/* given f(t), f(t+h), f(t+2h), and h, calculate f'(t+h) */
fastf_t f_prm_1(fastf_t x0, fastf_t x1, fastf_t x2, fastf_t h)
{
    return (x2 - x0)/(2*h);
}

/* given f(t), f(t+h), f(t+2h), and h, calculate f'(t+2h) */
fastf_t f_prm_2(fastf_t x0, fastf_t x1, fastf_t x2, fastf_t h)
{
    return (x0 - 4.0*x1 + 3.0*x2)/(2*h);
}


/* given f(t), f(t+h), f(t+2*h),  and h, calculate f'' */
fastf_t f_double_prm(fastf_t x0, fastf_t x1, fastf_t x2, fastf_t h)
{
    return (x0 - 2.0*x1 + x2)/(h*h);
}


/* code to read command line arguments*/
#define OPT_STR "b:f:p:s:r"
int get_args(int argc, char **argv)
{
    int c;

    estimate_f = 0;
    while ( (c=bu_getopt(argc,argv,OPT_STR)) != EOF) {
	switch(c){
	case 'b':
	    sscanf(bu_optarg,"%lf",&max_bank);
	    estimate_f = 1;
	    break;
	case 'f':
	    sscanf(bu_optarg,"%lf",&magic_factor);
	    magic_factor *= 0.001; /* to put factors in a more reasonable range */
	    break;
	case 'p':
	    sscanf(bu_optarg,"%d",&print_int);
	    break;
	case 'r':
	    loop = 0;
	    break;
	case 's':
	    sscanf(bu_optarg, "%lf", &desired_step);
	    break;
	default:
	    fprintf(stderr,"Unknown option: -%c\n",c);
	    return(0);
	}
    }
    return(1);
}


/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
