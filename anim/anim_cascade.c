/*	A N I M _ C A S C A D E . C
 *
 *  Purpose: Given position and orientation of main frame of reference,
 * along with the position and orientation of another frame with respect
 * to the main frame, give the absolute orientation and position of the
 * second frame.
 *	For example, given the position and orientation of a tank, and of the 
 * turret relative to the tank, you can get the absolute position and 
 * orientation of the turret at each time.
 * 	Or, optionally, given position and orientation of main frame of 
 * reference, and the absolute position and orientation of another frame,
 * find the position and orientation of the second frame in terms of the main
 * frame of reference. (-i option).
 *
 * Usage:
 *	anim_cascade main.table < relative.table > absolute.table
 *
 * The format of the tables is: 
 *  time x y z yaw pitch roll
 * unless specified otherwise by options.
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

vect_t wcenter, wypr, rcenter, rypr;
int cmd_wcen, cmd_wypr, cmd_rcen, cmd_rypr;
int inverse_mode, read_time, print_time;

main (argc,argv)
int argc;
char **argv;
{
	int i, val;
	fastf_t time, yaw, pitch, roll, ryaw, rpitch, rroll;
	vect_t mainv, relative, absolute, angles, rad_angles, rotated;
	mat_t m_main, m_rel, m_abs;
	FILE	*fp;
	int one_time;

	if (!get_args(argc,argv))
		fprintf(stderr,"anim_cascade: Argument error.");

	one_time = (cmd_wcen&&cmd_rcen&&cmd_wypr&&cmd_rypr);
	read_time = one_time ? 0 : print_time;
	time = 0.0;

	if (cmd_wcen)
		VMOVE(mainv, wcenter);
	if (cmd_rcen)
		VMOVE(relative, rcenter);
	if (cmd_wypr){
		anim_dy_p_r2mat(m_main, wypr[0], wypr[1], wypr[2]);
	}
	if (cmd_rypr){
		anim_dy_p_r2mat(m_rel, rypr[0], rypr[1], rypr[2]);
	}

	val = 3;
	while(1){
		if (read_time) {
			val=scanf("%lf",&time);
			if (val < 1) break;
		}
		if (!cmd_wcen) 
			val =scanf("%lf %lf %lf",mainv, mainv+1, mainv+2);
		if (!cmd_wypr) {
			val=scanf("%lf %lf %lf", &yaw, &pitch, &roll);
			anim_dy_p_r2mat(m_main, yaw, pitch, roll);
		}
		if (!cmd_rcen) {
			val=scanf("%lf %lf %lf", relative, relative+1,relative+2);
		}
		if (!cmd_rypr) {
			val=scanf("%lf %lf %lf", &ryaw, &rpitch, &rroll);
			anim_dy_p_r2mat(m_rel, ryaw, rpitch, rroll);
		}
		if (val<3) break;
		
		if (inverse_mode) {
			anim_tran(m_main);
			VSUB2(rotated,relative,mainv);
			MAT4X3PNT(absolute, m_main, rotated);
		} else {
			MAT4X3PNT(rotated, m_main, relative);
			VADD2(absolute, rotated, mainv);
		}
		mat_mul(m_abs, m_main, m_rel);
		anim_mat2ypr(rad_angles, m_abs);
		VSCALE(angles, rad_angles, RTOD);

		if (print_time){
			printf("%g",time);
		}
		printf("\t%g\t%g\t%g", absolute[0], absolute[1], absolute[2]);
		printf("\t%g\t%g\t%g", angles[0], angles[1], angles[2]);
		printf("\n");
		
		if (one_time) break;
	}

}

#define OPT_STR "siw:r:"

int get_args(argc,argv)
int argc;
char **argv;
{
	int c,d;

	inverse_mode = cmd_wcen = cmd_wypr = cmd_rcen = cmd_rypr = 0;
	print_time = 1;
	while ( (c=getopt(argc,argv,OPT_STR)) != EOF) {
		switch(c){
		case 'w':
			d = *(optarg);
			if (d == 'c'){
				sscanf(argv[optind],"%lf",wcenter+0);
				sscanf(argv[optind+1],"%lf",wcenter+1);
				sscanf(argv[optind+2],"%lf",wcenter+2);
				optind += 3;
				cmd_wcen = 1;
				break;
			} else if ( d =='y'){
				sscanf(argv[optind],"%lf",wypr+0);
				sscanf(argv[optind+1],"%lf",wypr+1);
				sscanf(argv[optind+2],"%lf",wypr+2);
				optind += 3;
				cmd_wypr = 1;
				break;
			} else {
				fprintf(stderr,"anim_cascade: unknown option -w%c\n", d);
			}
			break;
		case 'r':
			d = *(optarg);
			if (d == 'c'){
				sscanf(argv[optind],"%lf",rcenter+0);
				sscanf(argv[optind+1],"%lf",rcenter+1);
				sscanf(argv[optind+2],"%lf",rcenter+2);
				optind += 3;
				cmd_rcen = 1;
				break;
			} else if ( d =='y'){
				sscanf(argv[optind],"%lf",rypr+0);
				sscanf(argv[optind+1],"%lf",rypr+1);
				sscanf(argv[optind+2],"%lf",rypr+2);
				optind += 3;
				cmd_rypr = 1;
				break;
			} else {
				fprintf(stderr,"anim_cascade: unknown option -r%c\n", d);
			}
			break;
		case 'i':
			inverse_mode = 1;
			break;
		case 's':
			print_time = 0;
			break;
		default:
			fprintf(stderr,"anim_cascade: unknown option: -%c\n",c);
			return(0);
		}
	}
	return(1);
}
