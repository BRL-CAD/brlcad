/*                     A N I M _ T U R N . C
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
/** @file anim_turn.c
 *	Animate front-wheel steered vehicles.
 *
 *  This is a filter which operates on animation tables. Given an
 *  animation table for the position of the front axle, anim_turn produces
 *  an animation table for position and orientation. Options provide for
 *  animating the wheels and/or steering wheel.
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
#include "vmath.h"
#include "bu.h"
#include "anim.h"

#ifndef M_PI
#define M_PI	3.14159265358979323846
#endif

int		get_args(int argc, char **argv);
extern void	anim_y_p_r2mat(fastf_t *, double, double, double);
extern void	anim_add_trans(fastf_t *, const fastf_t *, const fastf_t *);

extern int bu_optind;
extern char *bu_optarg;

int print_int = 1;
int angle_set = 0;
int turn_wheels = 0;
fastf_t length, angle, radius;
fastf_t factor = 1.0;

int
main(int argc, char **argv)
{
    int count;
    fastf_t val, time, roll_ang, yaw,sign;
    vect_t v, point, front, back, zero, temp1, temp2;
    mat_t m_from_world, m_to_world;
    double bn_atan2(double, double);

    /* initialize variables */
    VSETALL(zero, 0.0);
    VSETALL( v , 0.0 );
    VSETALL( point, 0.0 );
    VSETALL( front, 0.0 );
    VSETALL( back, 0.0 );
    VSETALL( temp1, 0.0 );
    VSETALL( temp2, 0.0 );
    for( count=0 ; count<ELEMENTS_PER_MAT ; count++ )
	m_from_world[count]=m_to_world[count]=0.0;
    length = angle = radius = roll_ang = 0.0;

    if (!get_args(argc,argv))
	fprintf(stderr,"ascript: Get_args error");

    if (!angle_set) { /* set angle if not yet done */
	scanf("%*f%*[^-0123456789]");
	VSCAN(temp1);
	scanf("%*f%*[^-0123456789]");
	VSCAN(temp2);
	angle = bn_atan2( (temp2[1]-temp1[1]),(temp2[0]-temp1[0]) );
	rewind(stdin);
    }
    count = 0;
    while (1) {
	/* read one line of table */
	val = scanf("%lf%*[^-0123456789]",&time); /*read time,ignore garbage*/
	val = scanf("%lf %lf %lf", point, point+1, point +2);
	if (val < 3) {
	    break;
	}

	/*update to and from matrices */

	if (count) { /* not first time through */
	    /* calculate matrices corrsponding to last position*/
	    anim_y_p_r2mat(m_to_world,angle,0.0,0.0);
	    anim_add_trans(m_to_world,front,zero);
	    anim_y_p_r2mat(m_from_world,-angle,0.0,0.0);
	    VREVERSE(temp1,front);
	    anim_add_trans(m_from_world,zero,temp1);

	    /* calculate new position for front and back axles */
	    /* front goes to the point, back slides along objects*/
	    /* current front to back axis */
	    MAT4X3PNT(v,m_from_world,point);/* put point in vehicle coordinates*/
	    if (v[1] > length) {
		fprintf(stderr,"anim_turn: Distance between positions greater than length of vehicle - ABORTING\n");
		break;
	    }
	    temp2[0] = v[0] - sqrt(length*length - v[1]*v[1]); /*calculate back*/
	    temp2[1] = temp2[2] = 0.0;
	    MAT4X3PNT(back,m_to_world,temp2);/*put "back" in world coordinates*/
	    VMOVE(front,point);

	    /*calculate new angle of vehicle*/
	    VSUB2(temp1,front,back);
	    angle = bn_atan2(temp1[1],temp1[0]);
	}
	else { /*first time through */
	    /*angle is already determined*/
	    VMOVE(front, point);
	}

	/*calculate turn angles and print table*/

	if (turn_wheels){
	    if (v[0] >= 0)
		sign = 1.0;
	    else
		sign = -1.0;
	    yaw = bn_atan2(sign*v[1],sign*v[0]);
	    if (radius > VDIVIDE_TOL)
		roll_ang -= sign * MAGNITUDE(v) / radius;

	    if (!(count%print_int))
		printf("%.10g %.10g %.10g 0.0\n",time,factor*RTOD*yaw,RTOD*roll_ang);
	}
	else { /* print position and orientation of vehicle */
	    if (!(count%print_int))
		printf("%.10g %.10g %.10g %.10g %.10g 0.0 0.0\n",time,front[0],front[1],front[2], RTOD * angle);
	}
	count++;
    }
    return( 0 );
}

#define OPT_STR "r:l:a:f:p:"

int get_args(int argc, char **argv)
{
    int c;
    while ( (c=bu_getopt(argc,argv,OPT_STR)) != EOF) {
	switch(c){
	case 'l':
	    sscanf(bu_optarg,"%lf",&length);
	    break;
	case 'a':
	    sscanf(bu_optarg,"%lf",&angle);
	    angle *= DTOR; /* degrees to radians */
	    angle_set = 1;
	    break;
	case 'r':
	    sscanf(bu_optarg,"%lf",&radius);
	    turn_wheels = 1;
	    break;
	case 'f':
	    turn_wheels = 1;
	    sscanf(bu_optarg,"%lf",&factor);
	    break;
	case 'p':
	    sscanf(bu_optarg,"%d",&print_int);
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
