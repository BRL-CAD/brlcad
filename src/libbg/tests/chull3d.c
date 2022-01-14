/*                      B N _ C H U L L . C
 * BRL-CAD
 *
 * Copyright (c) 2013-2022 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */

#include "common.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "bu.h"
#include "vmath.h"
#include "bg.h"

int
main(int UNUSED(argc), const char **argv)
{
    bu_setprogname(argv[0]);

    /* Cube */
    {
	int i = 0;
	int retval = 0;
	int fc = 0;
	int vc = 0;
	point_t *vert_array;
	int *faces;
	point_t *input_verts = (point_t *)bu_calloc(8, sizeof(point_t), "vertex array");
	VSET(input_verts[0], -1000.0, 1000.0, 1000.0);
	VSET(input_verts[1], 1000.0, -1000.0, 1000.0);
	VSET(input_verts[2], 1000.0, 1000.0, 1000.0);
	VSET(input_verts[3], -1000.0, -1000.0, 1000.0);
	VSET(input_verts[4], 1000.0, 1000.0, -1000.0);
	VSET(input_verts[5], -1000.0, -1000.0, -1000.0);
	VSET(input_verts[6], -1000.0, 1000.0, -1000.0);
	VSET(input_verts[7], 1000.0, -1000.0, -1000.0);

	retval = bg_3d_chull(&faces, &fc, &vert_array, &vc, (const point_t *)input_verts, 9);
	bu_log("Test #001:  Cube:\n");
	bu_log("  Vertices:\n");
	for(i = 0; i < vc; i++) {
	    point_t p1;
	    VMOVE(p1,vert_array[i]);
	    bu_log("      actual[%d]: %f, %f, %f\n", i, p1[0], p1[1], p1[2]);
	}
	bu_log("  Faces:\n");
	for(i = 0; i < fc; i++) {
	    bu_log("      face %d: %d, %d, %d\n", i, faces[i*3], faces[i*3+1], faces[i*3+2]);
	}
	if (retval != 3) {return -1;} else {bu_log("Cube Test Passed!\n");}
    }

    /* Cube with center point */
    {
	int i = 0;
	int retval = 0;
	int fc = 0;
	int vc = 0;
	point_t *vert_array;
	int *faces;
	point_t *input_verts = (point_t *)bu_calloc(9, sizeof(point_t), "vertex array");
	VSET(input_verts[0], 0.0, 0.0, 0.0);
	VSET(input_verts[1], 2.0, 0.0, 0.0);
	VSET(input_verts[2], 2.0, 2.0, 0.0);
	VSET(input_verts[3], 0.0, 2.0, 0.0);
	VSET(input_verts[4], 0.0, 0.0, 2.0);
	VSET(input_verts[5], 2.0, 0.0, 2.0);
	VSET(input_verts[6], 2.0, 2.0, 2.0);
	VSET(input_verts[7], 0.0, 2.0, 2.0);
	VSET(input_verts[8], 1.0, 1.0, 1.0);

	retval = bg_3d_chull(&faces, &fc, &vert_array, &vc, (const point_t *)input_verts, 9);
	bu_log("Test #002:  Cube With Center Point:\n");
	bu_log("  Vertices:\n");
	for(i = 0; i < vc; i++) {
	    point_t p1;
	    VMOVE(p1,vert_array[i]);
	    bu_log("      actual[%d]: %f, %f, %f\n", i, p1[0], p1[1], p1[2]);
	}
	bu_log("  Faces:\n");
	for(i = 0; i < fc; i++) {
	    bu_log("      face %d: %d, %d, %d\n", i, faces[i*3], faces[i*3+1], faces[i*3+2]);
	}
	if (retval != 3) {return -1;} else {bu_log("Cube With Center Point Test Passed!\n");}
    }

    /* Flat triangles */
    {
	int i = 0;
	int retval = 0;
	int fc = 0;
	int vc = 0;
	point_t *vert_array;
	int *faces;
	point_t *input_verts = (point_t *)bu_calloc(5, sizeof(point_t), "vertex array");
	VSET(input_verts[0], 0.0, 0.0, 0.0);
	VSET(input_verts[1], 3.0, 3.0, 3.0);
	VSET(input_verts[2], 1.0, 1.0, 1.0);
	VSET(input_verts[3], 5.0, 1.0, 1.0);
	VSET(input_verts[4], 2.0, 2.0, 2.0);

	retval = bg_3d_chull(&faces, &fc, &vert_array, &vc, (const point_t *)input_verts, 9);
	bu_log("Test #003:  Flat Triangles:\n");
	bu_log("  Vertices:\n");
	for(i = 0; i < vc; i++) {
	    point_t p1;
	    VMOVE(p1,vert_array[i]);
	    bu_log("      actual[%d]: %f, %f, %f\n", i, p1[0], p1[1], p1[2]);
	}
	if (retval != 2) {return -1;} else {bu_log("Flat Triangles Passed!\n");}
    }



    return 0;
}


/** @} */
/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
