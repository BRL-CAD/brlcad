/*	A N I M _ C A S C A D E . C
 *
 *  Purpose: Given position and orientation of main frame of reference,
 * along with the position and orientation of another frame with respect
 * to the main frame, give the absolute orientation and position of the
 * second frame.
 *	For example, given the position and orientation of a tank, and of the 
 * turret relative to the tank, you can get the absolute position and 
 * orientation of the turret at each time.
 *
 * Usage:
 *	anim_cascade main.table < relative.table > absolute.table
 *
 * The format of the tables is: time x y z yaw pitch roll
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

#include "conf.h"

#include <math.h>
#include <stdio.h>

#include "machine.h"
#include "vmath.h"
#include "anim.h"


#ifndef M_PI
#define M_PI	3.14159265358979323846
#endif

#ifndef M_PI
#define RTOD	180.000/M_PI
#endif

extern int optind;
extern char *optarg;

main (argc,argv)
int argc;
char **argv;
{
	int i, val;
	fastf_t time, yaw, pitch, roll, ryaw, rpitch, rroll;
	vect_t mainv, relative, absolute, angles, rad_angles, rotated;
	mat_t m_main, m_rel, m_abs;
	FILE	*fp;



	if (!(fp=fopen(argv[1],"r"))){
		fprintf(stderr,"Couldn't open file %s.\n",argv[1]);
		return(-1);
	}
	while(!feof(fp)){
		fscanf(fp,"%*f");
		fscanf(fp,"%lf %lf %lf",mainv, mainv+1, mainv+2);
		fscanf(fp,"%lf %lf %lf", &yaw, &pitch, &roll);

		anim_dy_p_r2mat(m_main, yaw, pitch, roll);

		val=scanf("%lf",&time);
		if(val<1)
			break;
		printf("%g",time);
		val=scanf("%lf %lf %lf", relative, relative+1,relative+2);
		val=scanf("%lf %lf %lf", &ryaw, &rpitch, &rroll);
		if (val<2) break;
		MAT4X3PNT(rotated, m_main, relative);
		VADD2(absolute, rotated, mainv);
		anim_dy_p_r2mat(m_rel, ryaw, rpitch, rroll);
		mat_mul(m_abs, m_main, m_rel);
		anim_mat2ypr(rad_angles, m_abs);
		VSCALE(angles, rad_angles, RTOD);

		printf("\t%g\t%g\t%g", absolute[0], absolute[1], absolute[2]);
		printf("\t%g\t%g\t%g", angles[0], angles[1], angles[2]);
		printf("\n");
		if (val<2)
			break;
	}

	fclose(fp);
}
