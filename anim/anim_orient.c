/*			A N I M _ O R I E N T . C
 *
 *	Convert between different orientation formats. The formats are:
 *  quaternion, yaw-pitch-roll, xyz angles, pre-multiplication
 *  (object)  matrices, and post-multiplication (view) matrices.
 *  	The conversion is done by converting each input form to a matrix, 
 *  and then converting that matrix to the desired output form.
 *	Options include specifying angles in radians and applying a special
 *  permutation to the interior matrix in order to handle the virtual camera
 * correctly in yaw-pitch-roll form.
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
#define QUAT		2
#define V_MAT		3
#define O_MAT		4

#define DEGREES		0
#define RADIANS		1

#define NORMAL          0
#define ERROR1          1
#define ERROR2          2

#define DTOR    M_PI/180.0
#define RTOD    180.0/M_PI

extern int optind;
extern char *optarg;

int input_mode, output_mode, length, input_units, output_units, permute;

main(argc,argv)
int argc;
char **argv;
{
	int num_read;
	fastf_t	angle[3],quat[4],matrix[16],vmatrix[16];
	void anim_zyx2mat(),anim_ypr2mat(),anim_quat2mat(), anim_mat_print();
	int anim_mat2ypr(),anim_mat2zyx(),anim_mat2quat();

	if(!get_args(argc,argv))
		fprintf(stderr,"Get_args error.\n");

	/* read data */
	num_read = length;
	while (1){

		if ((input_mode==YPR)||(input_mode==XYZ)){
			num_read = scanf("%lf %lf %lf",angle,angle+1,angle+2);
		}
		else if (input_mode==QUAT){
			num_read = scanf("%lf %lf %lf %lf", quat,quat+1,quat+2,quat+3);
		}
		else if (input_mode==V_MAT) { /*transpose matrix as it's read in */
			num_read = 0;
			num_read += scanf("%lf %lf %lf %lf",matrix,matrix+4,matrix+8,matrix+12);
			num_read += scanf("%lf %lf %lf %lf",matrix+1,matrix+5,matrix+9,matrix+13);
			num_read += scanf("%lf %lf %lf %lf",matrix+2,matrix+6,matrix+10,matrix+14);
			num_read += scanf("%lf %lf %lf %lf",matrix+3,matrix+7,matrix+11,matrix+15);
		}
		else if (input_mode==O_MAT){
			num_read = 0;
			num_read += scanf("%lf %lf %lf %lf",matrix,matrix+1,matrix+2,matrix+3);
			num_read += scanf("%lf %lf %lf %lf",matrix+4,matrix+5,matrix+6,matrix+7);
			num_read += scanf("%lf %lf %lf %lf",matrix+8,matrix+9,matrix+10,matrix+11);
			num_read += scanf("%lf %lf %lf %lf",matrix+12,matrix+13,matrix+14,matrix+15);
		}

		if (num_read < length)
			break;

		/* convert to radians if in degrees */
	        if (input_units==DEGREES)  {
	                angle[0] *= DTOR;
	                angle[1] *= DTOR;
	                angle[2] *= DTOR;
	        }

		/* convert to (object) matrix form */
		if (input_mode==YPR){
			anim_ypr2mat(matrix,angle);
		}
		else if (input_mode==XYZ){
			anim_zyx2mat(matrix,angle);
		}
		else if (input_mode==QUAT){
			anim_quat2mat(matrix,quat);
		}
		else if (input_mode==V_MAT){
			;/* do nothing - already transposed on read */
		}

		if (permute){
			if (input_mode==YPR)
				anim_v_permute(matrix);
			else if (output_mode==YPR)
				anim_v_unpermute(matrix);
		}

		/* convert from matrix form and print result*/
		if (output_mode==YPR){
			anim_mat2ypr(angle,matrix);
		        if (output_units==DEGREES)
		                VSCALE(angle,angle,RTOD);
			printf("%f\t%f\t%f\n",angle[0],angle[1],angle[2]);
		}
		else if (output_mode==XYZ){
			anim_mat2zyx(angle,matrix);
		        if (output_units==DEGREES)
		                VSCALE(angle,angle,RTOD);
			printf("%f\t%f\t%f\n",angle[0],angle[1],angle[2]);
		}
		else if (output_mode==QUAT){
			anim_mat2quat(quat,matrix);
			printf("%f\t%f\t%f\t%f\n",quat[0],quat[1],quat[2],quat[3]);
		}
		else if (output_mode==V_MAT){
			mat_trn(vmatrix,matrix);
/*			transpose(matrix);*/
			anim_mat_print(vmatrix,0);
			printf("\n");
		}
		else if (output_mode==O_MAT){
			anim_mat_print(matrix,0);
			printf("\n");
		}

	}
}

#define OPT_STR "i:o:pr"

int get_args(argc,argv)
int argc;
char **argv;
{
	int c;

	/* defaults */
	input_mode = QUAT;
	output_mode = QUAT;
	input_units = DEGREES;
	output_units = DEGREES;
	permute = 0;

	while ( (c=getopt(argc,argv,OPT_STR)) != EOF) {
		switch(c){
		case 'i':
			if (*optarg == 'y'){
				input_mode = YPR;
				length = 3;
				if (*(optarg+1)=='r')
					input_units = RADIANS;
			}
			else if (*optarg == 'z'){
				input_mode = XYZ;
				length = 3;
				if (*(optarg+1)=='r')
					input_units = RADIANS;
			}
			else if (*optarg == 'q'){
				input_mode = QUAT;
				length = 4;
			}
			else if (*optarg == 'v'){
				input_mode = V_MAT;
				length = 16;
			}
			else if (*optarg == 'o'){
				input_mode = O_MAT;
				length = 16;
			}
			break;
		case 'o':
			if (*optarg == 'y'){
				output_mode = YPR;
				if (*(optarg+1)=='r')
					output_units = RADIANS;
			}
			else if (*optarg == 'z'){
				output_mode = XYZ;
				if (*(optarg+1)=='r')
					output_units = RADIANS;
			}
			else if (*optarg == 'q')
				output_mode = QUAT;
			else if (*optarg == 'v')
				output_mode = V_MAT;
			else if (*optarg == 'o')
				output_mode = O_MAT;
			break;
		case 'p':
			permute = 1;
			break;
		case 'r':
			input_units = RADIANS;
			output_units = RADIANS;
			break;
		default:
			fprintf(stderr,"Unknown option: -%c\n",c);
			return(0);
		}
	}

	return(1);
}
