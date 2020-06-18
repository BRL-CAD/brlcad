/*                   T R I M E S H _ P T _ I N . C
 * BRL-CAD
 *
 * Copyright (c) 2011-2020 United States Government as represented by
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

#include <stdio.h>

#include "bu.h"
#include "bg.h"

int
main(int argc, char **argv)
{
    int test_num = 0;

    if (argc != 2)
	bu_exit(1, "ERROR: [%s] input format is: test_number\n", argv[0]);

    bu_setprogname(argv[0]);

    sscanf(argv[1], "%d", &test_num);

    if (test_num == 1) {
	int status = -1;
	point_t TP = VINIT_ZERO;
	int num_faces = 4;
	int num_verts = 4;
	int *faces = (int *)bu_calloc(num_faces * 3, sizeof(int), "faces array");
	point_t *verts = (point_t *)bu_calloc(num_verts, sizeof(point_t), "vertices array");

	VSET(verts[0], -1, -1, -1);
	VSET(verts[1], 1, -1, -1);
	VSET(verts[2], 0, 1, -1);
	VSET(verts[3], 0, 0, 1);

	faces[0*3+0] = 3;
	faces[0*3+1] = 2;
	faces[0*3+2] = 0;

	faces[1*3+0] = 1;
	faces[1*3+1] = 3;
	faces[1*3+2] = 0;

	faces[2*3+0] = 2;
	faces[2*3+1] = 3;
	faces[2*3+2] = 1;

	faces[3*3+0] = 0;
	faces[3*3+1] = 2;
	faces[3*3+2] = 1;

	VSET(TP, 0, 0, 2);
	status = bg_trimesh_pt_in(TP, num_faces, faces, num_verts, verts);

	if (status == -1) {
	    bu_exit(-1, "Fatal error - mesh validity test failed\n");
	} else {
	    bu_log("Status: %s\n", (status) ? "inside" : "outside");
	    if (status) {
		bu_exit(-1, "Fatal error - test point is outside mesh, but reported inside\n");
	    }
	}

	VSET(TP, 0, 0, 0);
	status = bg_trimesh_pt_in(TP, num_faces, faces, num_verts, verts);

	if (status == -1) {
	    bu_exit(-1, "Fatal error - mesh validity test failed\n");
	} else {
	    bu_log("Status: %s\n", (status) ? "inside" : "outside");
	    if (!status) {
		bu_exit(-1, "Fatal error - test point is inside mesh, but reported outside\n");
	    }
	}

	return 0;
    }

    // Flip the triangles from test 1
    if (test_num == 2) {
	int status = -1;
	point_t TP = VINIT_ZERO;
	int num_faces = 4;
	int num_verts = 4;
	int *faces = (int *)bu_calloc(num_faces * 3, sizeof(int), "faces array");
	point_t *verts = (point_t *)bu_calloc(num_verts, sizeof(point_t), "vertices array");

	VSET(verts[0], -1, -1, -1);
	VSET(verts[1], 1, -1, -1);
	VSET(verts[2], 0, 1, -1);
	VSET(verts[3], 0, 0, 1);

	faces[0*3+0] = 3;
	faces[0*3+1] = 0;
	faces[0*3+2] = 2;

	faces[1*3+0] = 1;
	faces[1*3+1] = 0;
	faces[1*3+2] = 3;

	faces[2*3+0] = 2;
	faces[2*3+1] = 1;
	faces[2*3+2] = 3;

	faces[3*3+0] = 0;
	faces[3*3+1] = 1;
	faces[3*3+2] = 2;

	VSET(TP, 0, 0, 2);
	status = bg_trimesh_pt_in(TP, num_faces, faces, num_verts, verts);

	if (status == -1) {
	    bu_exit(-1, "Fatal error - mesh validity test failed\n");
	} else {
	    bu_log("Status: %s\n", (status) ? "inside" : "outside");
	}

	VSET(TP, 0, 0, 0);
	status = bg_trimesh_pt_in(TP, num_faces, faces, num_verts, verts);

	if (status == -1) {
	    bu_exit(-1, "Fatal error - mesh validity test failed\n");
	} else {
	    bu_log("Status: %s\n", (status) ? "inside" : "outside");
	}

	return 0;
    }

    bu_log("Error: unknown test number %d\n", test_num);
    return -1;
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
