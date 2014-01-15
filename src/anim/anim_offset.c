/*                   A N I M _ O F F S E T . C
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
/** @file anim_offset.c
 *
 * Animate an object which is rigidly attached to another.
 *
 * Given an animation table specifying the position and orientation of
 * one object, anim_offset produces a similar table specifying the
 * position of an object rigidly attached to it.
 *
 */

#include "common.h"

#include <math.h>
#include "bio.h"

#include "bu.h"
#include "bn.h"
#include "anim.h"
#include "vmath.h"


#define OPT_STR "ro:h?"


int full_print = 0;

/* intentionally double for scan */
double offset[3];

static void
usage(void)
{
    fprintf(stderr,"Usage: anim_offset -o # # # [-r] in.table out.table\n");
}

int
get_args(int argc, char **argv)
{
    int c;
    while ((c=bu_getopt(argc, argv, OPT_STR)) != -1) {
	switch (c) {
	    case 'r':
		full_print = 1;
		break;
	    case 'o':
		sscanf(argv[bu_optind-1], "%lf", offset+0);
		sscanf(argv[bu_optind], "%lf", offset+1);
		sscanf(argv[bu_optind+1], "%lf", offset+2);
		bu_optind += 2;
		break;
	    default:
		fprintf(stderr, "Unknown option: -%c\n", c);
		return 0;
	}
    }
    return 1;
}


int
main(int argc, char *argv[])
{
    int val;
    fastf_t yaw, pitch, roll;
    vect_t temp, point, zero;
    mat_t mat;

    /* intentionally double for scan */
    double timeval;
    double scan[3];

    VSETALL(temp, 0.0);
    VSETALL(point, 0.0);
    VSETALL(zero, 0.0);

    if (argc == 1 && isatty(fileno(stdin)) && isatty(fileno(stdout))){
	usage();
	return 0;
    }

    if (!get_args(argc, argv)){
	usage();
	return 0;
    }

    while (1) {
	/*read line from table */
	val = scanf("%lf%*[^-0123456789]", &timeval); /*read time, ignore garbage*/
	if (val < 1) {
	    break;
	}

	val = scanf("%lf %lf %lf", &scan[0], &scan[1], &scan[2]);
	if (val < 3) {
	    break;
	}
	/* double to fastf_t */
	VMOVE(point, scan);

	val = scanf("%lf %lf %lf", &scan[0], &scan[1], &scan[2]);
	if (val < 3) {
	    break;
	}
	/* double to fastf_t */
	yaw = scan[0];
	pitch = scan[1];
	roll = scan[2];

	anim_dy_p_r2mat(mat, yaw, pitch, roll);
	anim_add_trans(mat, point, zero);
	MAT4X3PNT(temp, mat, offset);

	printf("%.10g\t%.10g\t%.10g\t%.10g", timeval, temp[0], temp[1], temp[2]);
	if (full_print)
	    printf("\t%.10g\t%.10g\t%.10g", yaw, pitch, roll);
	printf("\n");
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
