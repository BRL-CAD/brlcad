/*			A N I M _ O R I E N T . C
 *
 *	Convert between different orientation formats. The formats are:
 *  quaternion, yaw-pitch-roll, azimuth-elevation-twist, xyz angles, 
 *  pre-multiplication rotation matrices, and transposed matrices (inverses).
 * 	By default, the information is assumed to represent a transformation
 *  which should be an object which initially faces the x-axis, with the
 *  z-axis going up. Alternatively, the information can be interpreted as 
 *  transformations which should be applied to an object initially facing the
 *  negative z-axis, with the y-axis going up.
 *  	The conversion is done by converting each input form to a matrix, 
 *  and then converting that matrix to the desired output form.
 *	Angles may be specified in radians or degrees.
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

#ifndef M_PI
#define M_PI	3.14159265358979323846
#endif

#define YPR		0
#define XYZ		1
#define AET		2
#define QUAT		3
#define MAT		4
#define TRAN		5

#define DEGREES		0
#define RADIANS		1

#define ANIM_NORMAL          0
#define ANIM_ERROR1          1
#define ANIM_ERROR2          2

#define DTOR    M_PI/180.0
#define RTOD    180.0/M_PI

extern int optind;
extern char *optarg;

int input_mode, output_mode, length, input_units, output_units;
int input_perm, output_perm;

main(argc,argv)
int argc;
char **argv;
{
	int num_read;
	fastf_t	angle[3],quat[4],matrix[16],tmatrix[16];
	void anim_zyx2mat(),anim_ypr2mat(),anim_quat2mat(), anim_mat_print();
	int anim_mat2ypr(),anim_mat2zyx(),anim_mat2quat();

	if(!parse_args(argc,argv)) {
		fprintf(stderr,"Get_args error.\n");
		exit(0);
	}

	if (input_perm&&output_perm) {
		input_perm = output_perm = 0;
	}

	/* read data */
	num_read = length;
	while (1){

		switch (input_mode) {
		case YPR:
		case XYZ:
		case AET:
			num_read = scanf("%lf %lf %lf",angle,angle+1,angle+2);
			/* convert to radians if in degrees */
		        if (input_units==DEGREES)  {
		        	VSCALE(angle,angle,DTOR);
		        }
			break;
		case QUAT:
			num_read = scanf("%lf %lf %lf %lf", quat,quat+1,quat+2,quat+3);
			break;
		case TRAN:
			num_read = 0;
			num_read += scanf("%lf %lf %lf %lf",matrix,matrix+4,matrix+8,matrix+12);
			num_read += scanf("%lf %lf %lf %lf",matrix+1,matrix+5,matrix+9,matrix+13);
			num_read += scanf("%lf %lf %lf %lf",matrix+2,matrix+6,matrix+10,matrix+14);
			num_read += scanf("%lf %lf %lf %lf",matrix+3,matrix+7,matrix+11,matrix+15);
			break;
		case MAT:
			num_read = 0;
			num_read += scanf("%lf %lf %lf %lf",matrix,matrix+1,matrix+2,matrix+3);
			num_read += scanf("%lf %lf %lf %lf",matrix+4,matrix+5,matrix+6,matrix+7);
			num_read += scanf("%lf %lf %lf %lf",matrix+8,matrix+9,matrix+10,matrix+11);
			num_read += scanf("%lf %lf %lf %lf",matrix+12,matrix+13,matrix+14,matrix+15);
			break;
		}

		if (num_read < length)
			break;

		/* convert to (object) matrix form */
		switch (input_mode) {
		case YPR:
			anim_ypr2mat(matrix,angle);
			break;
		case AET:
			anim_y_p_r2mat(matrix, angle[0]+M_PI, -angle[1],-angle[2]);
			break;
		case XYZ:
			anim_zyx2mat(matrix,angle);
			break;
		case QUAT:
			anim_quat2mat(matrix,quat);
			break;
		}

		if (input_perm){
			anim_v_unpermute(matrix);
		}
		if (output_perm){
			anim_v_permute(matrix);
		}

		/* convert from matrix form and print result*/
		switch (output_mode) {
		case YPR:
			anim_mat2ypr(angle,matrix);
		        if (output_units==DEGREES)
		                VSCALE(angle,angle,RTOD);
			printf("%.12g\t%.12g\t%.12g\n",angle[0],angle[1],angle[2]);
			break;
		case AET:
			anim_mat2ypr(angle,matrix);
			if (angle[0] > 0.0) {
				angle[0] -= M_PI;
			} else {
				angle[0] += M_PI;
			}
			angle[1] = -angle[1];
			angle[2] = -angle[2];
			if (output_units==DEGREES)
				VSCALE(angle,angle,RTOD);
			printf("%.12g\t%.12g\t%.12g\n",angle[0],angle[1],angle[2]);
			break;
		case XYZ:
			anim_mat2zyx(angle,matrix);
		        if (output_units==DEGREES)
		                VSCALE(angle,angle,RTOD);
			printf("%.12g\t%.12g\t%.12g\n",angle[0],angle[1],angle[2]);
			break;
		case QUAT:
			anim_mat2quat(quat,matrix);
			printf("%.12g\t%.12g\t%.12g\t%.12g\n",quat[0],quat[1],quat[2],quat[3]);
			break;
		case TRAN:
			mat_trn(tmatrix,matrix);
			anim_mat_print(tmatrix,0);
			printf("\n");
			break;
		case MAT:
			anim_mat_print(matrix,0);
			printf("\n");
		}

	}
}

int parse_args(argc,argv)
int argc;
char **argv;
{
	int c;
	char *cp;

	/* defaults */
	input_mode = QUAT;
	output_mode = QUAT;
	input_units = DEGREES;
	output_units = DEGREES;
	input_perm = 0;
	output_perm = 0;
	length = 4;

	if (argc > 2) { /*read output mode */
		cp = argv[2];
		while (c=*cp++) {
			switch (c) {
			case 'q':
				output_mode = QUAT;
				break;
			case 'y':
				output_mode = YPR;
				break;
			case 'a':
				output_mode = AET;
				break;
			case 'z':
				output_mode = XYZ;
				break;
			case 'm':
				output_mode = MAT;
				break;
			case 't':
				output_mode = TRAN;
				break;
			case 'r':
				output_units = RADIANS;
				break;
			case 'v':
				output_perm = 1;
				break;
			default:
				fprintf(stderr,"anim_orient: unknown output option: %c\n",c);
				return(0);
			}
		}
	}
	if (argc > 1) { /*read input mode */
		cp = argv[1];
		while (c=*cp++) {
			switch (c) {
			case 'q':
				input_mode = QUAT;
				length = 4;
				break;
			case 'y':
				input_mode = YPR;
				length = 3;
				break;
			case 'a':
				input_mode = AET;
				length = 3;
				break;
			case 'z':
				input_mode = XYZ;
				length = 3;
				break;
			case 'm':
				input_mode = MAT;
				length = 16;
				break;
			case 't':
				input_mode = TRAN;
				length = 16;
				break;
			case 'r':
				input_units = RADIANS;
				break;
			case 'v':
				input_perm = 1;
				break;
			default:
				fprintf(stderr,"anim_orient: unknown input option: %c\n",c);
				return(0);
			}
		}
	}
	return(1);
}
