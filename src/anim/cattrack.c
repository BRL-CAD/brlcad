/*                      C A T T R A C K . C
 * BRL-CAD
 *
 * Copyright (C) 2004-2005 United States Government as represented by
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
/** @file cattrack.c
 *  Caternary curve routines used by anim_track.c
 *
 */

#include "common.h"


#include <math.h>
#include <stdio.h>
#include "machine.h"
#include "vmath.h"
#include "anim.h"

#define MAX_REACHED	0
#define SOLVED		1

/* the total maximum number of lowest level iterations possible is
 *     T_MAX_ITS*MAX_OUT_ITS*2*MAX_ITS
 */
#define MAX_ITS		25
#define MAX_OUT_ITS	25
#define T_MAX_ITS	50

/* these tolerances are guessed at, except that it seems to be important
 * in elastic situations that T_TOL be significantly smaller than the others
 */
#define T_TOL		1.0e-14
#define G_TOL		1.0e-12
#define F_TOL		1.0e-12

/* HYPER_GET_X - get x value of a point which is a given distance along 
 * caternary curve. 
 * x(s) = arcsinh(a*s-sinh(a*c))/a + c
 * Left to calling routine to avoid dividing by zero.
 */
fastf_t hyper_get_x(fastf_t a, fastf_t c, fastf_t s, int d, int x, int cos_ang)
     /* curve parameters */
     /* arclength value  */
{
    fastf_t arg, asinh_arg;

    arg = a*s - sinh(a*c);
    asinh_arg = log(arg + sqrt(arg*arg + 1.0));

    return(asinh_arg/a + c);
}

/* HYPER_GET_S - calculate the arclength parameter of a caternary
 * curve corresponding to the given value of x.
 * s(x) = (sinh(a(x-c))+sinh(ac))/a 
 * Left to calling routime to avoid dividing by zero.
 */
fastf_t hyper_get_s(fastf_t a, fastf_t c, fastf_t x)
{
    return((sinh(a*(x-c))+sinh(a*c))/a);
}

/* HYPER_GET_Z - calculate point on the caternary curve:
 * z(x) = cosh(a*(x-c))/a + b
 * Left to calling routine to avoid dividing by zero.
 */
fastf_t hyper_get_z(fastf_t a, fastf_t b, fastf_t c, fastf_t x)
{
    fastf_t z;

    if (fabs(a)>VDIVIDE_TOL){
	z = cosh(a*(x-c))/a + b;
    } else {
	z = b;
    }
    return(z);
}

/* HYPER_GET_ANG - calculate angle corresponding to the slope of 
 * caternary curve.
 * z'(x) = sinh(a*(x-c))
 */
fastf_t hyper_get_ang(fastf_t a, fastf_t c, fastf_t x)
{
    fastf_t slope;

    slope = sinh(a*(x-c));
    return( atan2(slope, 1.0));
}

/* GET_CURVE - Find the constants a, b, and c such that the curve
 *  z = cosh(a*(x-c))/a + b
 * is tangent to circles of radii r0 and r1 located at
 * (x0,z0) and (x1,z1) and such that the curve has 
 * arclength delta_s between circles. Also find the angle where
 * the curve touches each circle. When called successively,
 * It uses the values of a,b, and c from the last call as a start.
 */
int getcurve(fastf_t *pa, fastf_t *pb, fastf_t *pc, fastf_t *pth0, fastf_t *pth1, fastf_t delta_s, fastf_t *p_zero, fastf_t *p_one, fastf_t r_zero, fastf_t r_one)
     /* curve parameters */
     /* angle where curve contacts circle0,circle1 */
     /* desired arclength */
     /* radii of circle0 and circle1 */
     /* center of circle0 and circle1 */
{

    int status, i, solved;
    int ingetcurve(fastf_t *pa, fastf_t *pb, fastf_t *pc, fastf_t delta_s, fastf_t *p_zero, fastf_t *p_one);
    fastf_t theta_one, theta_zero, new_theta_zero, new_theta_one;
    fastf_t avg_theta_zero, avg_theta_one, arc_dist;
    fastf_t tang_ang, costheta;
    double stmp;
    vect_t q_zero, q_one, diff; 
    static fastf_t last_a, last_c, last_theta_one, last_theta_zero;
    static int called_before = 0;

    /*first calculate angle at which tangent line would contact circles*/
    VSUB2(diff,p_one,p_zero);
    tang_ang = atan2(diff[Z],diff[X]);
    costheta = (r_zero-r_one)/MAGNITUDE(diff);
    tang_ang += acos(costheta);

    if (!called_before){
	theta_one = tang_ang;
	theta_zero = tang_ang;
	(*pa) = 1.0;
	*pc = 0.5*(p_one[X]+p_zero[X]);
	called_before = 1;
    }
    else {
	theta_zero = last_theta_zero;
	theta_one = last_theta_one;
	*pa = last_a;
	*pc = last_c;
    }

    status = MAX_REACHED;
    for (i=0;i<T_MAX_ITS;i++){
	q_zero[X] = p_zero[X] + r_zero * cos(theta_zero);
	q_zero[Z] = p_zero[Z] + r_zero * sin(theta_zero);
	q_one[X] = p_one[X] + r_one * cos(theta_one);
	q_one[Z] = p_one[Z] + r_one * sin(theta_one);

	/* determine distance taken by arc*/
	arc_dist = r_zero * (tang_ang - theta_zero);
	arc_dist += r_one * (theta_one - tang_ang);

	ingetcurve(pa,pb,pc, delta_s-arc_dist, q_zero, q_one);

	solved = 0;
	/* refine theta_one */
	stmp = sinh( (*pa)*(q_zero[X]-(*pc)) );
	new_theta_zero = atan2(1.0,-stmp);
	avg_theta_zero = 0.5 * (theta_zero + new_theta_zero);
	if (fabs(theta_zero-avg_theta_zero)<T_TOL){
	    solved++;
	}
	theta_zero = avg_theta_zero;

	/* refine theta_two */
	stmp = sinh( (*pa)*(q_one[X]-(*pc)) );
	new_theta_one = atan2(1.0,-stmp);
	avg_theta_one = 0.5 * (theta_one + new_theta_one);
	if (fabs(theta_one-avg_theta_one)<T_TOL){
	    solved++;
	}
	theta_one = avg_theta_one;

	if (solved == 2){
	    status = SOLVED;
	    break;
	}
		
    }
    last_theta_zero = theta_zero;
    last_theta_one = theta_one;
    last_a = *pa;
    last_c = *pc;
    *pth0 = theta_zero;
    *pth1 = theta_one;
    return(status);
			
}

/* INGETCURVE - find constants a, b, and c, such that the curve
 *  z = cosh(a*(x-c))/a + b
 * passes through (x0,z0) and (x1,z1) and has arclength delta_s
 * Appropriate first guesses for a,b, and c should be given.
 */
int ingetcurve(fastf_t *pa, fastf_t *pb, fastf_t *pc, fastf_t delta_s, fastf_t *p_zero, fastf_t *p_one)
{
    int status, i, j, k;
    fastf_t adjust;
    fastf_t eff(fastf_t a, fastf_t c, fastf_t x0, fastf_t x1, fastf_t delta_s),gee(fastf_t a, fastf_t c, fastf_t x0, fastf_t x1, fastf_t delta_z);

    status = MAX_REACHED;
    i=0;
    while (i++<MAX_OUT_ITS){
	for (j=0;j<MAX_ITS;j++){
	    adjust = eff(*pa,*pc,p_zero[X],p_one[X],delta_s);
	    if ((*pa-adjust)<=0.0){
		*pa *= 0.5;
	    }
	    else {
		*pa -= adjust;
	    }
	    if (adjust<F_TOL){
		break;
	    }
	}
		
	for (k=0;k<MAX_ITS;k++){
	    adjust = gee(*pa,*pc,p_zero[X],p_one[X],(p_one[Z]-p_zero[Z]));
	    *pc -= adjust;
	    if (adjust<G_TOL){
		break;
	    }
	}

	if ((j==0)&&(k==0)){
	    status = SOLVED;
	    break;
	}
    }
    *pb = p_zero[Z] - cosh( (*pa)*(p_zero[X]-(*pc)) )/(*pa);
	
    return(status);

}

/* find Newtonian adjustment for 'a', assuming 'c' fixed*/
fastf_t eff(fastf_t a, fastf_t c, fastf_t x0, fastf_t x1, fastf_t delta_s)
{
    double f,fprime;
    double arg0, arg1, sarg0, sarg1;

    arg0 = a*(x0-c);
    arg1 = a*(x1-c);

    sarg0 = sinh(arg0);
    sarg1 = sinh(arg1);

    f = a*(sarg1-sarg0 - a*delta_s);
    fprime = sarg0 - sarg1 - arg0*cosh(arg0) + arg1*cosh(arg1);

    if (fabs(fprime) > VDIVIDE_TOL)
	return(f/fprime);
    else if ((a*a) > VDIVIDE_TOL)
	return(f/(a*a));
    else if (fabs(a) > VDIVIDE_TOL)
	return(f/a);
    else
	return(f);
}

/* find Newtonian adjustment for c, assuming 'a' fixed*/
fastf_t gee(fastf_t a, fastf_t c, fastf_t x0, fastf_t x1, fastf_t delta_z)
{
    double g, gprime, arg0, arg1;

    arg0 = a*(x0-c);
    arg1 = a*(x1-c);

    g = cosh(arg1)-cosh(arg0) - a*delta_z;
    gprime = a*(sinh(arg0) - sinh(arg1));

    if (fabs(gprime) > VDIVIDE_TOL)
	return(g/gprime);
    else if (fabs(a) > VDIVIDE_TOL)
	return(g/a);
    else
	return(g);
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
