/*			A N I M _ F L Y . C
 *
 *	Simulate flying motion, for an airplane or virtual camera.
 *
 *  This filter operates on animation tables. Given the desired position
 *  of the airplane in each frame, anim_fly produces a table including the
 *  plane's position and orientation. A "magic factor" should be supplied 
 *  to control the severity of banking. Looping behavior can be toggled 
 *  with another option.
 *
 *  Author -
 *	Carl J. Nuzman
 *  
 *  Source -
 *      The U. S. Army Research Laboratory
 *      Aberdeen Proving Ground, Maryland  21005-5068  USA
 *  
 *  Distribution Notice -
 *      Re-distribution of this software is restricted, as described in
 *      your "Statement of Terms and Conditions for the Release of
 *      The BRL-CAD Pacakge" agreement.
 *
 *  Copyright Notice -
 *      This software is Copyright (C) 1993 by the United States Army
 *      in all countries except the USA.  All rights reserved.
 */

#include <math.h>
#include <stdio.h>
#include "machine.h"
#include "vmath.h"
#include "anim.h" 

#ifndef M_PI
#define M_PI	3.14159265358979323846
#endif

extern int bu_optind;
extern char *bu_optarg;

int loop = 1;
int print_int = 1;
fastf_t magic_factor = 1.0;

#define PREP	-1
#define START	0
#define MIDDLE	1
#define	END	2
#define STOP	3

main(argc,argv)
int argc;
char **argv;
{
	int count, status, num_read;
	fastf_t point0[4],point1[4],point2[4],point3[4], yaw, pch, rll;
	fastf_t f_prm_0(), f_prm_1(), f_prm_2();

	VSETALL(point0,0.0);
	VSETALL(point1,0.0);
	VSETALL(point2,0.0);
	VSETALL(point3,0.0);
	yaw = pch = rll = 0.0;

	if (!get_args(argc,argv))
		fprintf(stderr,"Anim_fly: Get_args error");

	count = -2;
	status = PREP;
	num_read = 4;
	while (status != STOP) {
		/* check for end of file status */
		if (num_read < 4) status = END;

		/*wait twice to prime the pump before starting calculations*/
		if (!(count++)) status = START;

		/* read one line of the table into point3, storing
			previous values in point0, point1,  and point2. */
		if (status != END) {
			VMOVEN(point0,point1,4);
			VMOVEN(point1,point2,4);
			VMOVEN(point2,point3,4);
			num_read = scanf("%lf %lf %lf %lf", point3, point3+1, point3+2, point3+3);
		}

		if (status==START) { /* do calculations for the first point*/
			get_orientation(point1,point2,point3, f_prm_0, &yaw, &pch, &rll);
			if (!(count%print_int)) {
				printf("%f %f %f %f %f %f %f\n",point1[0],point1[1],point1[2],point1[3],yaw,pch,rll);
			}
			status = MIDDLE;
		}
		else if (status==MIDDLE) {/*do calculations for all middle points*/
			get_orientation(point0,point1,point2, f_prm_1, &yaw, &pch, &rll);
			if (!(count%print_int)) {
				printf("%f %f %f %f %f %f %f\n",point1[0],point1[1],point1[2],point1[3],yaw,pch,rll);
			}
		}
		else if (status==END) { /*do calculations for the last point*/
			get_orientation(point0,point1,point2, f_prm_2, &yaw, &pch, &rll);
			if (!(count%print_int)) {
				printf("%f %f %f %f %f %f %f\n",point2[0],point2[1],point2[2],point2[3],yaw,pch,rll);
			}
			status = STOP;
		}
	}
}


get_orientation(p0,p1,p2,function, p_yaw, p_pch, p_rll)
fastf_t p0[4],p1[4],p2[4], *p_yaw, *p_pch, *p_rll;
fastf_t (*function)();
{
	int i;
	fastf_t step,vel[3],accel[3];
	fastf_t f_double_prm(),xyz2yaw(),xyz2pch(),bank();

	static fastf_t last_yaw, last_pch, last_rll;
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
	last_pch = *p_pch;
	last_rll = *p_rll;
	not_first_time = 1;
}

/* determine the yaw of the given direction vector */
fastf_t	xyz2yaw(d)
fastf_t	d[3];
{
	fastf_t yaw;
	yaw = RTOD*atan2(d[1],d[0]);
	if (yaw < 0.0) yaw += 360.0;
	return yaw;
}

/* determine the pitch of the given direction vector */
fastf_t	xyz2pch(d)
fastf_t	d[3];
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
fastf_t bank(acc,vel)
fastf_t acc[3],vel[3];
{
	fastf_t cross;

	cross = vel[1]*acc[0] - vel[0]*acc[1];
	cross *= magic_factor;
	if (cross > 90) cross = 90;
	if (cross < -90) cross = -90;
	return cross;
}

/* given f(t), f(t+h), f(t+2h), and h, calculate f'(t) */
fastf_t f_prm_0(x0,x1,x2,h)
fastf_t x0,x1,x2,h;
{
	return  -(3.0*x0 - 4.0*x1 + x2)/(2*h);
}

/* given f(t), f(t+h), f(t+2h), and h, calculate f'(t+h) */
fastf_t f_prm_1(x0,x1,x2,h)
fastf_t x0,x1,x2,h;
{
	return (x2 - x0)/(2*h);
}

/* given f(t), f(t+h), f(t+2h), and h, calculate f'(t+2h) */
fastf_t f_prm_2(x0,x1,x2,h)
fastf_t x0,x1,x2,h;
{
	return (x0 - 4.0*x1 + 3.0*x2)/(2*h);
}


/* given f(t), f(t+h), f(t+2*h),  and h, calculate f'' */
fastf_t f_double_prm(x0,x1,x2,h)
fastf_t x0,x1,x2,h;
{
	return (x0 - 2.0*x1 + x2)/(h*h);
}


/* code to read command line arguments*/
#define OPT_STR "f:p:r"
int get_args(argc,argv)
int argc;
char **argv;
{
	int c;
	while ( (c=bu_getopt(argc,argv,OPT_STR)) != EOF) {
		switch(c){
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
		default:
			fprintf(stderr,"Unknown option: -%c\n",c);
			return(0);
		}
	}
	return(1);
}

