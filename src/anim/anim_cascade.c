/*                  A N I M _ C A S C A D E . C
 * BRL-CAD
 *
 * Copyright (c) 1993-2013 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 *
 */
/** @file anim_cascade.c
 *
 * Purpose: Given position and orientation of main frame of reference,
 * along with the position and orientation of another frame with respect
 * to the main frame, give the absolute orientation and position of the
 * second frame.
 *
 * For example, given the position and orientation of a tank, and of
 * the turret relative to the tank, you can get the absolute position
 * and orientation of the turret at each time.  Or, optionally, given
 * position and orientation of main frame of reference, and the
 * absolute position and orientation of another frame, find the
 * position and orientation of the second frame in terms of the main
 * frame of reference. (-i option).
 *
 * Usage: anim_cascade main.table < relative.table > absolute.table
 *
 * Unless specified otherwise by options, the format of the table file
 * is in the space-delimited form:
 *
 * time x y z yaw pitch roll
 *
 */

#include "common.h"

#include <math.h>
#include "bio.h"

#include "bu.h"
#include "bn.h"
#include "anim.h"
#include "vmath.h"


#define OPT_STR "so:f:r:a:h?"

#define CASCADE_A 0
#define CASCADE_R 1
#define CASCADE_F 2


/* intentionally double for scan */
double fcenter[3], fypr[3], rcenter[3], rypr[3], acenter[3], aypr[3];
int cmd_fcen, cmd_fypr, cmd_rcen, cmd_rypr, cmd_acen, cmd_aypr;
int output_mode, read_time, print_time;

static void
usage(void)
{
    fprintf(stderr,"Usage: anim_cascade [-s] [-o(f|r|a)] [-(f|r|a)(c|y) # # #] input.table output.table\n");
}

int get_args(int argc, char **argv)
{
    int c, d;

    output_mode = CASCADE_A;
    cmd_fcen = cmd_fypr = cmd_rcen = cmd_rypr = cmd_acen = cmd_aypr = 0;
    print_time = 1;
    while ((c=bu_getopt(argc, argv, OPT_STR)) != -1) {
	switch (c) {
	    case 'f':
		d = *(bu_optarg);
		if (d == 'c') {
		    bu_sscanf(argv[bu_optind], "%lf", fcenter+0);
		    bu_sscanf(argv[bu_optind+1], "%lf", fcenter+1);
		    bu_sscanf(argv[bu_optind+2], "%lf", fcenter+2);
		    bu_optind += 3;
		    cmd_fcen = 1;
		    break;
		} else if (d =='y') {
		    bu_sscanf(argv[bu_optind], "%lf", fypr+0);
		    bu_sscanf(argv[bu_optind+1], "%lf", fypr+1);
		    bu_sscanf(argv[bu_optind+2], "%lf", fypr+2);
		    bu_optind += 3;
		    cmd_fypr = 1;
		    break;
		} else {
		    fprintf(stderr, "anim_cascade: unknown option -f%c\n", d);
		}
		break;
	    case 'r':
		d = *(bu_optarg);
		if (d == 'c') {
		    bu_sscanf(argv[bu_optind], "%lf", rcenter+0);
		    bu_sscanf(argv[bu_optind+1], "%lf", rcenter+1);
		    bu_sscanf(argv[bu_optind+2], "%lf", rcenter+2);
		    bu_optind += 3;
		    cmd_rcen = 1;
		    break;
		} else if (d =='y') {
		    bu_sscanf(argv[bu_optind], "%lf", rypr+0);
		    bu_sscanf(argv[bu_optind+1], "%lf", rypr+1);
		    bu_sscanf(argv[bu_optind+2], "%lf", rypr+2);
		    bu_optind += 3;
		    cmd_rypr = 1;
		    break;
		} else {
		    fprintf(stderr, "anim_cascade: unknown option -r%c\n", d);
		}
		break;
	    case 'a':
		d = *(bu_optarg);
		if (d == 'c') {
		    bu_sscanf(argv[bu_optind], "%lf", acenter+0);
		    bu_sscanf(argv[bu_optind+1], "%lf", acenter+1);
		    bu_sscanf(argv[bu_optind+2], "%lf", acenter+2);
		    bu_optind += 3;
		    cmd_acen = 1;
		    break;
		} else if (d =='y') {
		    bu_sscanf(argv[bu_optind], "%lf", aypr+0);
		    bu_sscanf(argv[bu_optind+1], "%lf", aypr+1);
		    bu_sscanf(argv[bu_optind+2], "%lf", aypr+2);
		    bu_optind += 3;
		    cmd_aypr = 1;
		    break;
		} else {
		    fprintf(stderr, "anim_cascade: unknown option -a%c\n", d);
		}
		break;
	    case 'o':
		d = *(bu_optarg);
		if (d == 'r') {
		    output_mode = CASCADE_R;
		} else if (d == 'f') {
		    output_mode = CASCADE_F;
		} else if (d == 'a') {
		    /* default */
		    output_mode = CASCADE_A;
		} else {
		    fprintf(stderr, "anim_cascade: unknown option -i%c\n", d);
		}
		break;
	    case 's':
		print_time = 0;
		break;
	    default:
		return 0;
	}
    }
    return 1;
}


int
main (int argc, char *argv[])
{
    int val;
    /* intentionally double for scan */
    double elapsed, yaw1, pitch1, roll1, yaw2, pitch2, roll2;
    double cen1[3], cen2[3];

    vect_t rad_ang_ans, cen_ans, ang_ans, rotated = VINIT_ZERO;
    mat_t m_rot1, m_rot2, m_ans;
    int one_time, read_cen1, read_cen2, read_rot1, read_rot2;

    if (argc == 1 && isatty(fileno(stdin)) && isatty(fileno(stdout))){
	usage();
	return 0;
    }

    if (!get_args(argc, argv)){
	usage();
	return 0;
    }

    read_cen1 = read_cen2 = read_rot1 = read_rot2 = 1;

    switch (output_mode) {
	case CASCADE_A:
	    if (cmd_fcen) {
		VMOVE(cen1, fcenter);
		read_cen1 = 0;
	    }
	    if (cmd_rcen) {
		VMOVE(cen2, rcenter);
		read_cen2 = 0;
	    }
	    if (cmd_fypr) {
		anim_dy_p_r2mat(m_rot1, fypr[0], fypr[1], fypr[2]);
		read_rot1 = 0;
	    }
	    if (cmd_rypr) {
		anim_dy_p_r2mat(m_rot2, rypr[0], rypr[1], rypr[2]);
		read_rot2 = 0;
	    }
	    break;
	case CASCADE_R:
	    if (cmd_fcen) {
		VMOVE(cen1, fcenter);
		read_cen1 = 0;
	    }
	    if (cmd_acen) {
		VMOVE(cen2, acenter);
		read_cen2 = 0;
	    }
	    if (cmd_fypr) {
		anim_dy_p_r2mat(m_rot1, fypr[0], fypr[1], fypr[2]);
		read_rot1 = 0;
	    }
	    if (cmd_aypr) {
		anim_dy_p_r2mat(m_rot2, aypr[0], aypr[1], aypr[2]);
		read_rot2 = 0;
	    }
	    break;
	case CASCADE_F:
	    if (cmd_acen) {
		VMOVE(cen1, acenter);
		read_cen1 = 0;
	    }
	    if (cmd_rcen) {
		VMOVE(cen2, rcenter);
		read_cen2 = 0;
	    }
	    if (cmd_aypr) {
		anim_dy_p_r2mat(m_rot1, aypr[0], aypr[1], aypr[2]);
		read_rot1 = 0;
	    }
	    if (cmd_rypr) {
		anim_dy_p_r2mat(m_rot2, rypr[0], rypr[1], rypr[2]);
		read_rot2 = 0;
	    }
	    break;
	default:
	    break;
    }


    one_time = (!(read_cen1||read_cen2||read_rot1||read_rot2));
    read_time = one_time ? 0 : print_time;
    elapsed = 0.0;

    val = 3;
    while (1) {
	if (read_time) {
	    val=scanf("%lf", &elapsed);
	    if (val < 1) break;
	}
	if (read_cen1)
	    val =scanf("%lf %lf %lf", cen1, cen1+1, cen1+2);
	if (read_rot1) {
	    val=scanf("%lf %lf %lf", &yaw1, &pitch1, &roll1);
	    anim_dy_p_r2mat(m_rot1, yaw1, pitch1, roll1);
	}
	if (read_cen2) {
	    val=scanf("%lf %lf %lf", cen2, cen2+1, cen2+2);
	}
	if (read_rot2) {
	    val=scanf("%lf %lf %lf", &yaw2, &pitch2, &roll2);
	    anim_dy_p_r2mat(m_rot2, yaw2, pitch2, roll2);
	}
	if (val<3) break;

	if (output_mode==CASCADE_R) {
	    anim_tran(m_rot1);
	    VSUB2(rotated, cen2, cen1);
	    MAT4X3PNT(cen_ans, m_rot1, rotated);
	    bn_mat_mul(m_ans, m_rot1, m_rot2);
	} else if (output_mode==CASCADE_F) {
	    anim_tran(m_rot2);
	    bn_mat_mul(m_ans, m_rot1, m_rot2);
	    MAT4X3PNT(rotated, m_ans, cen2);
	    VSUB2(cen_ans, cen1, rotated);
	} else {
	    MAT4X3PNT(rotated, m_rot1, cen2);
	    VADD2(cen_ans, rotated, cen1);
	    bn_mat_mul(m_ans, m_rot1, m_rot2);
	}
	anim_mat2ypr(m_ans, rad_ang_ans);
	VSCALE(ang_ans, rad_ang_ans, RAD2DEG);

	if (print_time) {
	    printf("%g", elapsed);
	}
	printf("\t%.12g\t%.12g\t%.12g", cen_ans[0], cen_ans[1], cen_ans[2]);
	printf("\t%.12g\t%.12g\t%.12g", ang_ans[0], ang_ans[1], ang_ans[2]);
	printf("\n");

	if (one_time) break;
    }
    return 0;
}


/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
