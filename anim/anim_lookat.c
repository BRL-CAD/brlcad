/*			A N I M _ L O O K A T . C
 *
 *	Given animation tables for the position of the virtual camera 
 * and a point to look at at each time, this program produces an animation
 * script to control the camera. The view is kept rightside up, whenever 
 * possible. When looking vertically up or down, the exact orientation 
 * depends on the previous orientation.
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
#include <stdio.h>
#include <math.h>
#include "machine.h"
#include "vmath.h"
#include "anim.h"

extern int optind;
extern char *optarg;

int frame = 0;
int print_ypr = 0;

main(argc,argv)
int argc;
char **argv;
{
	fastf_t time;
	vect_t eye,look,dir,prev_dir, angles;
	mat_t mat;
	int val = 0;

	VSETALL(look,0.0);
	VSETALL(eye,0.0);

	if (argc > 1)
		get_args(argc,argv);

	while (!feof(stdin)){
		VMOVE(prev_dir,dir);
		val=scanf("%lf %lf %lf %lf %lf %lf %lf",&time,eye,eye+1,eye+2,look,look+1,look+2);
		if (val < 7){
			break;
		}

		VSUBUNIT(dir,look,eye);
		anim_dir2mat(mat,dir,prev_dir);
		if (!print_ypr){
			printf("start %d;\n",frame++);
			printf("clean;\n");
	                printf("eye_pt %f %f %f;\n",eye[0],eye[1],eye[2]);
			printf("viewrot %f %f %f 0\n", -mat[1], -mat[5], -mat[9]);
	                printf("%f %f %f 0\n", mat[2], mat[6], mat[10]);
	                printf("%f %f %f 0\n", -mat[0], -mat[4], -mat[8]);
	                printf("0 0 0 1;\n");
			printf("end;\n");
		}
		else {
			anim_mat2ypr(angles,mat);
			printf("%g\t%g\t%g\t%g\t",time,eye[0],eye[1],eye[2]);
			printf("%g\t%g\t%g\n",angles[0],angles[1],angles[2]);
		}

	}
}

#define OPT_STR "f:y"

int get_args(argc,argv)
int argc;
char **argv;
{
	int c;
	while ( (c=getopt(argc,argv,OPT_STR)) != EOF) {
		switch(c){
		case 'f':
			sscanf(optarg,"%d",&frame);
			break;
		case 'y':
			print_ypr=1;
			break;
		default:
			fprintf(stderr,"Unknown option: -%c\n",c);
			return(0);
		}
	}
	return(1);
}

