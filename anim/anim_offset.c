/*			A N I M _ O F F S E T . C
 *
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

extern int bu_optind;
extern char *bu_optarg;

int full_print = 0;
vect_t offset;

main(argc,argv)
int argc;
char **argv;
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

}

#define OPT_STR "ro:"

int get_args(argc,argv)
int argc;
char **argv;
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



