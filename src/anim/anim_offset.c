/*                   A N I M _ O F F S E T . C
 * BRL-CAD
 *
 * Copyright (C) 1993-2005 United States Government as represented by
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
/** @file anim_offset.c
 *	Animate an object which is rigidly attached to another.
 *
 *  Given an animation table specifying the position and orientation of
 *  one object, anim_offset produces a similar table specifying the
 *  position of an object rigidly attached to it. 
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
#include "vmath.h"
#include "bu.h"
#include "anim.h"

extern int bu_optind;
extern char *bu_optarg;

int full_print = 0;
vect_t offset;

int get_args(int argc, char **argv);
extern void anim_dy_p_r2mat(fastf_t *, double, double, double);
extern void anim_add_trans(fastf_t *, const fastf_t *, const fastf_t *);

int
main(int argc, char **argv)
{
    int val;
    fastf_t yaw, pitch, roll, time;
    vect_t temp,point,zero;
    mat_t mat;

    VSETALL( temp , 0.0 );
    VSETALL( point , 0.0 );
    VSETALL( zero , 0.0 );
	
    (void) get_args(argc,argv);


    while (1) {
	/*read line from table */
	val = scanf("%lf%*[^-0123456789]",&time); /*read time,ignore garbage*/
	val = scanf("%lf %lf %lf", point, point+1, point +2);
	val = scanf("%lf %lf %lf", &yaw, &pitch, &roll);
	if (val < 3) {
	    break;
	}

	anim_dy_p_r2mat(mat,yaw, pitch,roll);
	anim_add_trans(mat,point,zero);
	MAT4X3PNT(temp,mat,offset);
		
	printf("%.10g\t%.10g\t%.10g\t%.10g",time, temp[0], temp[1], temp[2]);
	if (full_print)
	    printf("\t%.10g\t%.10g\t%.10g", yaw, pitch, roll);
	printf("\n");
    }
    return( 0 );
}

#define OPT_STR "ro:"

int get_args(int argc, char **argv)
{
    int c;
    while ( (c=bu_getopt(argc,argv,OPT_STR)) != EOF) {
	switch(c){
	case 'r':
	    full_print = 1;
	    break;
	case 'o':
	    sscanf(argv[bu_optind-1],"%lf",offset+0);
	    sscanf(argv[bu_optind],"%lf",offset+1);
	    sscanf(argv[bu_optind+1],"%lf",offset+2);
	    bu_optind += 2;
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
