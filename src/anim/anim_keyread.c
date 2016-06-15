/*                  A N I M _ K E Y R E A D . C
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
/** @file anim_keyread.c
 *
 * Convert an list of mged-style 'savekey' keyframes into an animation
 * table
 *
 * The output table specifies the orientation in one of three ways:
 *
 * default - quaternions in the order x, y, z, w.
 *
 * -z option - Euler angles, in the order xyz. The model axes are
 * considered to be rotated about the camera's z, y, and x axes (in
 * that order).
 *
 * -y option - Euler angles in the form yaw, pitch, roll.
 *
 * This is more or less a special case of anim_orient.c
 *
 */

#include "common.h"

#include <math.h>
#include "bio.h"

#include "bu.h"
#include "bn.h"
#include "vmath.h"
#include "anim.h"


#define OPT_STR "yzqrh?"

#define YPR 0
#define XYZ 1
#define QUATERNION 2

#define DEGREES 0
#define RADIANS 1

#define NORMAL 0
#define ERROR1 1
#define ERROR2 2


int mode;
int units;

static void
usage(void)
{
    fprintf(stderr,"Usage: anim_keyread [-y|z] key.file key.table\n");
}

int
get_args(int argc, char **argv)
{

    int c;

    mode = QUATERNION; /* default */
    units = DEGREES;

    while ((c=bu_getopt(argc, argv, OPT_STR)) != -1) {
	switch (c) {
	    case 'y':
		mode = YPR;
		break;
	    case 'z':
		mode = XYZ;
		break;
	    case 'q':
		mode = QUATERNION;
		break;
	    case 'r':
		units = RADIANS;
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
    int c;
    int count;
    fastf_t viewrot[16] = {0.0}, angle[3] = {0.0}, quat[4] = {0.0};

    /* intentionally double for scan */
    double timeval, viewsize;
    double eyept[3] = {0.0};
    double scan[4];

    if (argc == 1 && isatty(fileno(stdin)) && isatty(fileno(stdout))) {
	usage();
	return 0;
    }

    if (!get_args(argc, argv)) {
	usage();
	return 0;
    }

    while (!feof(stdin)) {
	/* read one keyframe */
	count = 0;

	count += scanf("%lf", &timeval);
	count += scanf("%lf", &viewsize);
	count += scanf("%lf %lf %lf", eyept, eyept+1, eyept+2);
	/* read in transposed matrix */
	count += scanf("%lf %lf %lf %lf", &scan[0], &scan[1], &scan[2], &scan[3]);
	/* double to fastf_t */
	viewrot[0] = scan[0];
	viewrot[4] = scan[1];
	viewrot[8] = scan[2];
	viewrot[12] = scan[3];
	count += scanf("%lf %lf %lf %lf", &scan[0], &scan[1], &scan[2], &scan[3]);
	/* double to fastf_t */
	viewrot[1] = scan[0];
	viewrot[5] = scan[1];
	viewrot[9] = scan[2];
	viewrot[13] = scan[3];
	count += scanf("%lf %lf %lf %lf", &scan[0], &scan[1], &scan[2], &scan[3]);
	/* double to fastf_t */
	viewrot[2] = scan[0];
	viewrot[6] = scan[1];
	viewrot[10] = scan[2];
	viewrot[14] = scan[3];
	count += scanf("%lf %lf %lf %lf", &scan[0], &scan[1], &scan[2], &scan[3]);
	/* double to fastf_t */
	viewrot[3] = scan[0];
	viewrot[7] = scan[1];
	viewrot[11] = scan[2];
	viewrot[15] = scan[3];

	if (feof(stdin) || count != 21)
	    break;

	printf("%.10g\t%.10g\t%.10g\t%.10g\t%.10g\t", timeval, viewsize, eyept[0], eyept[1], eyept[2]);

	if (mode==YPR) {
	    anim_v_unpermute(viewrot);
	    c = anim_mat2ypr(viewrot, angle);
	    if (c==ERROR1)
		fprintf(stderr, "Warning: yaw and roll arbitrarily defined at time = %f.\n", timeval);
	    else if (c==ERROR2)
		fprintf(stderr, "Keyread: can't interpret matrix at time = %f.\n", timeval);
	    if (units == DEGREES)
		VSCALE(angle, angle, RAD2DEG);
	    printf("%.10g\t%.10g\t%.10g\n", angle[0], angle[1], angle[2]);
	} else if (mode==XYZ) {
	    c = anim_mat2zyx(viewrot, angle);
	    if (c==ERROR1)
		fprintf(stderr, "Warning: x and z rotations arbitrarily defined at time = %f.\n", timeval);
	    else if (c==ERROR2)
		fprintf(stderr, "Keyread: can't interpret matrix at time = %f\n.", timeval);
	    if (units == DEGREES)
		VSCALE(angle, angle, RAD2DEG);
	    printf("%.10g\t%.10g\t%.10g\n", angle[X], angle[Y], angle[Z]);
	} else if (mode==QUATERNION) {
	    anim_mat2quat(quat, viewrot);
	    printf("%.10g\t%.10g\t%.10g\t%.10g\n", quat[X], quat[Y], quat[Z], quat[W]);
	}
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
