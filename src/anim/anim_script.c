/*                   A N I M _ S C R I P T . C
 * BRL-CAD
 *
 * Copyright (c) 1993-2014 United States Government as represented by
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
/** @file anim_script.c
 *
 * Turn an animation table into an animation script suitable for
 * use by rt. Anim_script.c makes a script for one object at a time (or the
 * virtual camera). Some of the available options include rotation
 * only, translation only, automatic steering, and specifying reference
 * coordinates.
 *
 */

#include "common.h"

#include <math.h>
#include <string.h>
#include <bio.h>

#include "vmath.h"
#include "bu/getopt.h"
#include "bu/str.h"
#include "bn.h"
#include "bn/anim.h"


#define OPT_STR "a:b:c:d:f:m:pqrstv:h?"


/* info from command line args */
int relative_a, relative_c, axes, translate, quaternion, rotate;/*flags*/
int steer, view, readview, permute; /* flags*/
int first_frame;
vect_t rcentroid, front;
mat_t m_axes, m_rev_axes; /* rotational analogue of centroid */
char mat_cmd[10];   /* default is lmul */

/* intentionally double for scan */
double centroid[3];
double viewsize;

static void
usage(void)
{
    fprintf(stderr,"Usage: anim_script [-v #] [-r|t|s] [-q][-p] [-a|b # # #] [-c|d # # #] [-f #] [-m cmd] [objectname] < in.table > out.script\n");
}

int
get_args(int argc, char **argv)
{

    int c, i, yes;
    double yaw, pch, rll;
    void anim_dx_y_z2mat(fastf_t *, double, double, double), anim_dz_y_x2mat(fastf_t *, double, double, double);
    rotate = translate = 1; /* defaults */
    quaternion = permute = 0;
    bu_strlcpy(mat_cmd, "lmul", sizeof(mat_cmd));
    while ((c=bu_getopt(argc, argv, OPT_STR)) != -1) {
	i=0;
	switch (c) {
	    case 'a':
		bu_optind -= 1;
		sscanf(argv[bu_optind+(i++)], "%lf", &yaw);
		sscanf(argv[bu_optind+(i++)], "%lf", &pch);
		sscanf(argv[bu_optind+(i++)], "%lf", &rll);
		bu_optind += 3;
		anim_dx_y_z2mat(m_axes, rll, -pch, yaw);
		anim_dz_y_x2mat(m_rev_axes, -rll, pch, -yaw);
		axes = 1;
		relative_a = 1;
		break;
	    case 'b':
		bu_optind -= 1;
		sscanf(argv[bu_optind+(i++)], "%lf", &yaw);
		sscanf(argv[bu_optind+(i++)], "%lf", &pch);
		sscanf(argv[bu_optind+(i++)], "%lf", &rll);
		bu_optind += 3;
		anim_dx_y_z2mat(m_axes, rll, -pch, yaw);
		anim_dz_y_x2mat(m_rev_axes, -rll, pch, -yaw);
		axes = 1;
		relative_a = 0;
		break;
	    case 'c':
		bu_optind -= 1;
		sscanf(argv[bu_optind+(i++)], "%lf", centroid);
		sscanf(argv[bu_optind+(i++)], "%lf", centroid+1);
		sscanf(argv[bu_optind+(i++)], "%lf", centroid+2);
		bu_optind += 3;
		VREVERSE(rcentroid, centroid);
		relative_c = 1;
		break;
	    case 'd':
		bu_optind -= 1;
		sscanf(argv[bu_optind+(i++)], "%lf", centroid);
		sscanf(argv[bu_optind+(i++)], "%lf", centroid+1);
		sscanf(argv[bu_optind+(i++)], "%lf", centroid+2);
		bu_optind += 3;
		VREVERSE(rcentroid, centroid);
		relative_c = 0;
		break;
	    case 'f':
		sscanf(bu_optarg, "%d", &first_frame);
		break;
	    case 'm':
		bu_strlcpy(mat_cmd, bu_optarg, sizeof(mat_cmd));
		break;
	    case 'p':
		permute = 1;
		break;
	    case 'q':
		quaternion = 1;
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
		break;
	    case 't':
		translate = 1;
		rotate = 0;
		break;
	    case 'v':
		yes = sscanf(bu_optarg, "%lf", &viewsize);
		if (!yes) viewsize = 0.0;
		if (viewsize < 0.0)
		    readview = 1;
		view = 1;
		break;
	    default:
		return 0;
	}
    }
    return 1;
}


int
main(int argc, char *argv[])
{
    fastf_t yaw, pitch, roll;
    vect_t point, zero;
    quat_t quat;
    mat_t a, m_x;
    int val, go, frame, last_steer,needed;

    /* intentionally double for scan */
    double scan[4];

    if (argc == 1 && isatty(fileno(stdin)) && isatty(fileno(stdout))) {
	usage();
	return 0;
    }

    if (!get_args(argc, argv)) {
	usage();
	return 0;
    }

    frame=last_steer=go=view=relative_a=relative_c=axes=0;
    VSETALL(centroid, 0);
    VSETALL(rcentroid, 0);
    VSETALL(front, 0);
    VSETALL(point, 0);
    VSETALL(zero, 0);
    yaw = pitch = roll = 0.0;
    MAT_IDN(m_axes);
    MAT_IDN(m_rev_axes);
    MAT_IDN(a);

    frame = (steer) ? first_frame -1 : first_frame;

    if (view && (viewsize > 0.0))
	printf("viewsize %.10g;\n", viewsize);

    if (rotate&&quaternion) {
	needed = 4;
    } else if (rotate || translate) {
	needed = 3;
    } else {
	needed = 1;
    }

    while (1) {
	/* read one line of table */
	val = scanf("%*f"); /*ignore time */
	if (readview)
	    val = scanf("%lf", &viewsize);
	if (translate) {
	    val = scanf("%lf %lf %lf", &scan[0], &scan[1], &scan[2]);
	    VMOVE(point, scan);
	}
	if (rotate&&quaternion) {
	    val = scanf("%lf %lf %lf %lf", &scan[0], &scan[1], &scan[2], &scan[3]);
	    HMOVE(quat, scan);
	} else if (rotate) {
	    val = scanf("%lf %lf %lf", &scan[0], &scan[1], &scan[2]);
	    yaw = scan[0];
	    pitch = scan[1];
	    roll = scan[2];
	}

	if (val < needed) {
	    /* i.e. scanf not completely successful */
	    /* with steering option, must go extra loop after end of file */
	    if (steer && !last_steer)
		last_steer = 1;
	    else break;
	}

	/* calculate basic rotation matrix a */
	if (steer)
	    go = anim_steer_mat(a, point, last_steer); /* warning: point changed by anim_steer_mat */
	else if (quaternion) {
	    anim_quat2mat(a, quat);
	    go = 1;
	} else {
	    anim_dx_y_z2mat(a, roll, -pitch, yaw);/* make ypr matrix */
	    go = 1;
	}

	/* if input orientation (presumably from quaternion) was
	 * designed to manipulate the view, first move the object
	 * to the default object position */
	if (permute)
	    anim_v_unpermute(a);

	/* make final matrix, including translation etc. */
	if (axes) {
	    /* add pre-rotation from original axes */
	    bn_mat_mul(m_x, a, m_rev_axes);
	    MAT_MOVE(a, m_x);
	}
	anim_add_trans(a, point, rcentroid); /* add translation */
	if (axes && relative_a) {
	    /* add post-rotation back to original axes */
	    bn_mat_mul(m_x, m_axes, a);
	    MAT_MOVE(a, m_x);
	}
	if (relative_c) {
	    vect_t center;
	    VMOVE(center, centroid); /* double to fast_f */
	    anim_add_trans(a, center, zero); /* final translation */
	}


	/* print one frame of script */
	if (go && view) {
	    printf("start %d;\n", frame);
	    printf("clean;\n");
	    if (readview)
		printf("viewsize %.10g;\n", viewsize);
	    printf("eye_pt %.10g %.10g %.10g;\n", a[3], a[7], a[11]);
	    /* implicit anim_v_permute */
	    printf("viewrot %.10g %.10g %.10g 0\n", -a[1], -a[5], -a[9]);
	    printf("%.10g %.10g %.10g 0\n", a[2], a[6], a[10]);
	    printf("%.10g %.10g %.10g 0\n", -a[0], -a[4], -a[8]);
	    printf("0 0 0 1;\n");
	    printf("end;\n");
	} else if (go) {
	    printf("start %d;\n", frame);
	    printf("clean;\n");
	    printf("anim %s matrix %s\n", *(argv+bu_optind), mat_cmd);
	    anim_mat_print(stdout, a, 1);
	    printf("end;\n");
	}
	frame++;
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
