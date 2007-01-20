/*                  A N I M _ K E Y R E A D . C
 * BRL-CAD
 *
 * Copyright (c) 1993-2007 United States Government as represented by
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
/** @file anim_keyread.c
 *	Convert an list of mged-style 'savekey' keyframes into an
 *  animation table
 *
 *  The output table specifies the orientation in one of three ways:
 *
 *	default	 - quaternions in the order x, y, z, w.
 *
 *	-z option - Eulers angles, in the order xyz. The model axes are
 *		considered to be rotated about the camera's z, y, and
 *		x axes (in that order).
 *
 *	-y option - Eulers angles in the form yaw, pitch, roll.
 *
 *  This is more or less a special case of anim_orient.c
 *
 *  Author -
 *	Carl J. Nuzman
 *
 *  Source -
 *      The U. S. Army Research Laboratory
 *      Aberdeen Proving Ground, Maryland  21005-5068  USA
 */

#include "common.h"


#include <stdio.h>
#include <math.h>
#include "machine.h"
#include "vmath.h"
#include "bu.h"

#ifndef M_PI
#define M_PI	3.14159265358979323846
#endif

#define YPR		0
#define XYZ		1
#define QUATERNION	2

#define DEGREES		0
#define RADIANS		1

#define NORMAL		0
#define ERROR1		1
#define ERROR2		2

#define DTOR    M_PI/180.0
#define RTOD    180.0/M_PI

int mode;
int units;

extern int bu_optind;
extern char *bu_optarg;

int get_args(int argc, char **argv);
extern void anim_v_unpermute(fastf_t *);

int
main(int argc, char **argv)
{
    int c;
    fastf_t time, viewsize;

    fastf_t eyept[3], viewrot[16], angle[3], quat[4];
    int anim_mat2ypr(fastf_t *, fastf_t *), anim_mat2zyx(const fastf_t *, fastf_t *), anim_mat2quat(fastf_t *, const fastf_t *);

    if (!get_args(argc,argv))
	fprintf(stderr,"anim_keyread: get_args error");

    while (!feof(stdin)){  /* read one keyframe */
	scanf("%lf", &time);
	scanf("%lf", &viewsize);
	scanf("%lf %lf %lf", eyept, eyept+1, eyept+2);
	/* read in transposed matrix */
	scanf("%lf %lf %lf %lf", viewrot+0, viewrot+4, viewrot+8, viewrot+12);
	scanf("%lf %lf %lf %lf", viewrot+1, viewrot+5, viewrot+9, viewrot+13);
	scanf("%lf %lf %lf %lf", viewrot+2, viewrot+6, viewrot+10, viewrot+14);
	scanf("%lf %lf %lf %lf", viewrot+3, viewrot+7, viewrot+11, viewrot+15);

	if (feof(stdin)) break;

	printf("%.10g\t%.10g\t%.10g\t%.10g\t%.10g\t", time, viewsize,
	       eyept[0], eyept[1], eyept[2]);


	if (mode==YPR) {
	    anim_v_unpermute(viewrot);
	    c = anim_mat2ypr(angle,viewrot);
	    if (c==ERROR1)
		fprintf(stderr,"Warning: yaw and roll arbitrarily defined at time = %f.\n",time);
	    else if (c==ERROR2)
		fprintf(stderr,"Keyread: can't interpret matrix at time = %f.\n",time);
	    if (units == DEGREES)
		VSCALE(angle,angle,RTOD);
	    printf("%.10g\t%.10g\t%.10g\n",angle[0],angle[1],angle[2]);
	}
	else if (mode==XYZ) {
	    c = anim_mat2zyx(angle,viewrot);
	    if (c==ERROR1)
		fprintf(stderr,"Warning: x and z rotations arbitrarily defined at time = %f.\n",time);
	    else if (c==ERROR2)
		fprintf(stderr,"Keyread: can't interpret matrix at time = %f\n.",time);
	    if (units == DEGREES)
		VSCALE(angle,angle,RTOD);
	    printf("%.10g\t%.10g\t%.10g\n",angle[X],angle[Y],angle[Z]);
	}
	else if (mode==QUATERNION){
	    anim_mat2quat(quat,viewrot);
	    printf("%.10g\t%.10g\t%.10g\t%.10g\n",quat[X],quat[Y],quat[Z],quat[W]);
	}
    }
    return( 0 );
}


#define OPT_STR "yzqr"

int get_args(int argc, char **argv)
{

    int c;

    mode = QUATERNION; /* default */
    units = DEGREES;

    while ( (c=bu_getopt(argc,argv,OPT_STR)) != EOF) {
	switch(c){
	case 'y':
	    mode = YPR;
	    break;
	case 'z':
	    mode = XYZ;
	    break;
	case 'q':
	    mode = QUATERNION;
	    break;
	case 'r':
	    units = RADIANS;
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
