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

#ifndef M_PI
#define M_PI	3.14159265358979323846
#endif
#ifndef RTOD
#define RTOD	(180/M_PI)
#endif


#define LOOKAT_SCRIPT	0
#define	LOOKAT_YPR	1
#define LOOKAT_QUAT	2


extern int bu_optind;
extern char *bu_optarg;

int frame = 0;
int print_mode = LOOKAT_SCRIPT;
int print_viewsize = 0;

main(argc,argv)
int argc;
char **argv;
{
	fastf_t time, vsize;
	vect_t eye,look,dir,prev_dir, angles, norm, temp;
	quat_t quat;
	mat_t mat;
	int val = 0;

	VSETALL(look,0.0);
	VSETALL(eye,0.0);

	if (argc > 1)
		get_args(argc,argv);

	VSET(norm, 0.0, 1.0, 0.0);
	while (!feof(stdin)){
		VMOVE(prev_dir,dir);
		val=scanf("%lf %lf %lf %lf %lf %lf %lf",&time,eye,eye+1,eye+2,look,look+1,look+2);
		if (val < 7){
			break;
		}

		if (print_viewsize) {
			VSUB2(temp, eye, look);
			vsize = MAGNITUDE(temp);
			vsize *= 2.0;
		}

		VSUBUNIT(dir,look,eye);
		anim_dirn2mat(mat,dir,norm);
		VSET(norm, mat[1],mat[5], 0.0); 
		switch (print_mode) {
		case LOOKAT_SCRIPT:
			printf("start %d;\n",frame++);
			printf("clean;\n");
			if (print_viewsize)
				printf("viewsize %.10g;\n",vsize);
	                printf("eye_pt %.10g %.10g %.10g;\n",eye[0],eye[1],eye[2]);
			printf("viewrot %.10g\t%.10g\t%.10g\t0\n", -mat[1], -mat[5], -mat[9]);
	                printf("%.10g\t%.10f\t%.10g\t0\n", mat[2], mat[6], mat[10]);
	                printf("%.10g\t%.10g\t%.10g\t0\n", -mat[0], -mat[4], -mat[8]);
	                printf("0\t0\t0\t1;\n");
			printf("end;\n");
			break;
		case LOOKAT_YPR:
			anim_mat2ypr(angles,mat);
			angles[0] *= RTOD;
			angles[1] *= RTOD;
			angles[2] *= RTOD;
			printf("%.10g",time);
			if (print_viewsize)
				printf("\t%.10g",vsize);
			printf("\t%.10g\t%.10g\t%.10g",eye[0],eye[1],eye[2]);
			printf("\t%.10g\t%.10g\t%.10g\n",angles[0],angles[1],angles[2]);
			break;
		case LOOKAT_QUAT:
			anim_mat2quat(quat,mat);
			printf("%.10g",time);
			if (print_viewsize)
				printf("\t%.10g",vsize);
			printf("\t%.10g\t%.10g\t%.10g",eye[0],eye[1],eye[2]);
			printf("\t%.10g\t%.10g\t%.10g\t%.10g\n",quat[0],quat[1],quat[2],quat[3]);
			break;
		}


	}
}

#define OPT_STR "f:yqv"

int get_args(argc,argv)
int argc;
char **argv;
{
	int c;
	while ( (c=bu_getopt(argc,argv,OPT_STR)) != EOF) {
		switch(c){
		case 'f':
			sscanf(bu_optarg,"%d",&frame);
			break;
		case 'y':
			print_mode = LOOKAT_YPR;
			break;
		case 'q':
			print_mode = LOOKAT_QUAT;
			break;
		case 'v':
			print_viewsize = 1;
			break;
		default:
			fprintf(stderr,"Unknown option: -%c\n",c);
			return(0);
		}
	}
	return(1);
}

