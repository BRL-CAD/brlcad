/*			A N I M _ S C R I P T . C
 *
 *	Turn an animation table into an animation script suitable for
 *  use by rt. Anim_script.c makes a script for one object at a time (or the
 *  virtual camera). Some of the available options include rotation
 *  only, translation only, automatic steering, and specifying reference
 *  coordinates.
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

extern int optind;
extern char *optarg;

int last_steer, frame; /* used by steer_mat */
   /* info from command line args */
int relative_a, relative_c, axes, translate, rotate, steer, view, readview; /* flags*/
int first_frame;
fastf_t  viewsize;
vect_t centroid, rcentroid, front;
mat_t m_axes, m_rev_axes; /* rotational analogue of centroid */

main(argc,argv)
int argc;
char **argv;
{
	void anim_dx_y_z2mat(), anim_add_trans();
	fastf_t yaw, pitch, roll;
	vect_t point, zero;
	mat_t a, m_x;
	int val, go;

	frame=last_steer=go=view=relative_a=relative_c=axes=0;
	VSETALL(centroid,0);
	VSETALL(rcentroid,0);
	VSETALL(front,0);
	VSETALL(point,0);
	VSETALL(zero,0);
	yaw = pitch = roll = 0.0;
	MAT_IDN(m_axes);
	MAT_IDN(m_rev_axes);
	MAT_IDN(a);	


	if (!get_args(argc,argv))
		fprintf(stderr,"anim_script: Get_args error");
	
	if (view && (viewsize > 0.0))
                printf("viewsize %f;\n", viewsize);


	while (1) {
		/* read one line of table */
		val = scanf("%*f%*[^-0123456789]"); /*ignore time and (if it exists) column of periods from tabinterp*/
                if (readview)
                        scanf("%lf",&viewsize);
		if(translate)
			val=scanf("%lf %lf %lf", point, point+1, point+2);
		if(rotate)
			val=scanf("%lf %lf %lf",&yaw,&pitch,&roll);

		if (val < 3){ /* ie. scanf not completely successful */
			/* with steering option, must go extra loop after end of file */
			if (steer && !last_steer)
				last_steer = 1;
			else break;
		}

		/* calculate basic rotation matrix a */
		if (steer)
			go = steer_mat(a,point); /* warning: point changed by steer_mat */
		else {
			anim_dx_y_z2mat(a,roll,-pitch,yaw);/* make ypr matrix */
			go = 1;
		}

		/* make final matrix, including translation etc */
		if (axes){ /* add pre-rotation from original axes */
			mat_mul(m_x,a,m_rev_axes); 
			MAT_MOVE(a,m_x);
		}
		anim_add_trans(a,point,rcentroid); /* add translation */
		if (axes && relative_a){ /* add post-rotation back to original axes */
			mat_mul(m_x,m_axes,a);
			MAT_MOVE(a,m_x);
		}
		if (relative_c)
			anim_add_trans(a,centroid,zero); /* final translation */


		/* print one frame of script */
		if (go && view){
	                printf("start %d;\n", first_frame + frame);
			printf("clean;\n");
			if (readview)
		                printf("viewsize %f;\n", viewsize);
	                printf("eye_pt %f %f %f;\n",a[3],a[7],a[11]);
			printf("viewrot %f %f %f 0\n",-a[1],-a[5],-a[9]);
	                printf("%f %f %f 0\n", a[2], a[6], a[10]);
	                printf("%f %f %f 0\n", -a[0], -a[4],-a[8]);
	                printf("0 0 0 1;\n");
	                printf("end;\n");
		}
		else if (go){
			printf("start %d;\n", first_frame + frame);
			printf("clean;\n");
			printf("anim %s matrix lmul\n", *(argv+optind));
			anim_mat_print(a,1);
			printf("end;\n");
		}
		frame++;
	}

}

#define OPT_STR	"a:b:c:d:f:rstv:"

int get_args(argc,argv)
int argc;
char **argv;
{
	
	int c, i;
	double yaw,pch,rll;
	void anim_dx_y_z2mat(), anim_dz_y_x2mat();
	rotate = translate = 1; /* defaults */
	while ( (c=getopt(argc,argv,OPT_STR)) != EOF) {
		i=0;
		switch(c){
		case 'a':
			optind -= 1;
                        sscanf(argv[optind+(i++)],"%lf", &yaw );
                        sscanf(argv[optind+(i++)],"%lf", &pch );
                        sscanf(argv[optind+(i++)],"%lf", &rll );
			optind += 3;
			anim_dx_y_z2mat(m_axes, rll, -pch, yaw);
			anim_dz_y_x2mat(m_rev_axes, -rll, pch, -yaw);
			axes = 1;
			relative_a = 1;
                        break;
		case 'b':
			optind -= 1;
                        sscanf(argv[optind+(i++)],"%lf", &yaw );
                        sscanf(argv[optind+(i++)],"%lf", &pch );
                        sscanf(argv[optind+(i++)],"%lf", &rll );
			optind += 3;
			anim_dx_y_z2mat(m_axes, rll, -pch, yaw);
			anim_dz_y_x2mat(m_rev_axes, -rll, pch, -yaw);
			axes = 1;
			relative_a = 0;
                        break;
		case 'c':
			optind -= 1;
                        sscanf(argv[optind+(i++)],"%lf",centroid);
                        sscanf(argv[optind+(i++)],"%lf",centroid+1);
                        sscanf(argv[optind+(i++)],"%lf",centroid+2);
			optind += 3;
			VREVERSE(rcentroid,centroid);
			relative_c = 1;
                        break;
		case 'd':
			optind -= 1;
                        sscanf(argv[optind+(i++)],"%lf",centroid);
                        sscanf(argv[optind+(i++)],"%lf",centroid+1);
                        sscanf(argv[optind+(i++)],"%lf",centroid+2);
			optind += 3;
			VREVERSE(rcentroid,centroid);
			relative_c = 0;
                        break;
		case 'f':
			sscanf(optarg,"%d",&first_frame);
			break;
		case 'r':
			rotate = 1;
			translate = 0;
			break;
		case 's':
			steer = 1;
			relative_a = 0;
			rotate = 0;
			translate = 1;
			frame = -1;
			break;
		case 't':
			translate = 1;
			rotate = 0;
			break;
		case 'v':
			sscanf(optarg,"%lf",&viewsize);
			if (viewsize < 0.0)
				readview = 1;
			view = 1;
			break;
		default:
			fprintf(stderr,"Unknown option: -%c\n",c);
			return(0);
		}
	}
	return(1);
}

/*STEER_MAT - given the next frame's position, remember the value of
the previous frame's position and calculate a matrix which points the x-axis
in the direction defined by those two positions. Return new matrix, and the
remembered value of the current position, as arguments; return 1 as the 
normal value, and 0 when there is not yet information to remember.
*/
int steer_mat(mat,point)
mat_t  mat;
vect_t point;
{
	void anim_dir2mat(), anim_add_trans(), anim_view_rev();
	static vect_t p1, p2, p3;
	vect_t dir, dir2;

	VMOVE(p1,p2);
	VMOVE(p2,p3);
	VMOVE(p3,point);
	if (frame == 0){ /* first frame*/
		VSUBUNIT(dir,p3,p2);
		VMOVE(dir2,dir);
	}
	else if (last_steer){ /*last frame*/
		VSUBUNIT(dir,p2,p1);
		VMOVE(dir2,dir);
	}
	else if (frame > 0){ /*normal*/
		VSUBUNIT(dir,p3,p1);
		VSUBUNIT(dir2,p2,p1);/*needed for vertical case*/
	}
	else return(0); /* return signal 'don't print yet */

	anim_dir2mat(mat,dir,dir2); /* create basic rotation matrix */
/*	if (view){
		anim_view_rev(mat);
	}
*/
	VMOVE(point,p2); /* for main's purposes, the current point is p2 */

	return(1); /* return signal go 'ahead and print' */
}

