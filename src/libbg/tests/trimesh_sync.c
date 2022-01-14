/*                    T R I M E S H _ S Y N C . C
 * BRL-CAD
 *
 * Copyright (c) 2011-2022 United States Government as represented by
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

int arbn_faces_ctrl[] = {
    0,1,2,
    3,5,4,
    6,1,0,
    7,8,1,
    6,7,1,
    3,4,9,
    10,11,9,
    4,10,9,
    8,9,1,
    11,2,1,
    9,11,1,
    5,7,4,
    6,10,4,
    7,6,4,
    10,2,11,
    6,0,2,
    10,6,2,
    7,9,8,
    5,3,9,
    7,5,9,
    INT_MAX
};

int arb8_faces_ctrl[] = {
    0,1,2,
    0,3,1,
    4,5,6,
    4,7,5,
    4,6,2,
    6,0,2,
    3,7,1,
    3,5,7,
    6,3,0,
    6,5,3,
    2,7,4,
    2,1,7,
    INT_MAX
};

int arb7_faces_ctrl[] = {
    0,1,2,
    1,3,2,
    4,2,5,
    4,6,2,
    4,5,1,
    5,3,1,
    6,0,2,
    3,5,2,
    1,6,4,
    1,0,6,
    INT_MAX
};
int arb6_faces_ctrl[] = {
    0,1,2,
    3,4,5,
    5,2,1,
    5,4,2,
    2,3,0,
    2,4,3,
    1,3,5,
    1,0,3,
    INT_MAX
};
int arb5_faces_ctrl[] = {
    0,1,2,
    3,2,4,
    4,2,1,
    3,0,2,
    1,3,4,
    1,0,3,
    INT_MAX
};
int arb4_faces_ctrl[] = {
    0,1,2,
    2,1,3,
    0,3,1,
    0,2,3,
    INT_MAX
};

void
parrays(int *ff, int *ctrl)
{
    int j = 0;
    struct bu_vls faces = BU_VLS_INIT_ZERO;
    while (ctrl[j] != INT_MAX) {
	bu_vls_printf(&faces, "%d,", ctrl[j]);
	if ((j+1) % 3 == 0)
	    bu_vls_printf(&faces, "  ");
	j++;
    }
    bu_vls_printf(&faces, "INT_MAX\n");
    j = 0;
    while (ff[j] != INT_MAX) {
	bu_vls_printf(&faces, "%d,", ff[j]);
	if ((j+1) % 3 == 0)
	    bu_vls_printf(&faces, "  ");
	j++;
    }
    bu_vls_printf(&faces, "INT_MAX");
    bu_log("%s\n", bu_vls_cstr(&faces));
    bu_vls_free(&faces);
}

// TODO - check needs to get more sophisticated.  As long as the three verts
// for a face are in the same relative order, it doesn't matter which one comes
// first.  Also, a sync is valid if ALL of the faces are reversed relative to
// the ctrl - sync doesn't guarantee which way the mesh ends up facing, so
// "all triangles inside out" is also a valid answer.
int
fcheck(const char *tname, int *ff, int *ctrl)
{
    int ret = 0;
    int i = 0;
    if (!ff || !ctrl || !tname)
	return -1;

    while (ctrl[i] != INT_MAX) {
	i++;
    }
    if (!i || i % 3) {
	bu_log("%s: error - ff array doesn't have an index count evenly dividsible by 3\n", tname);
	return -1;
    }
    int tri_cnt = i / 3;
    int misaligned = 0;
    int matched = 0;

    int ctrl_tri[3];
    int test_tri[3];
    i = 0;
    while (ctrl[i] != INT_MAX) {
	int tind[3] = {-1};
	ctrl_tri[0] = ctrl[i+0];
	ctrl_tri[1] = ctrl[i+1];
	ctrl_tri[2] = ctrl[i+2];
	test_tri[0] = ff[i+0];
	test_tri[1] = ff[i+1];
	test_tri[2] = ff[i+2];

	for (int j = 0; j < 3; j++) {
	    if (ctrl_tri[0] == test_tri[j]) {
		tind[0] = j;
		break;
	    }
	}
	if (tind[0] == -1) {
	    bu_log("%s: completely disjoint triangles at position %d\n", tname, i);
	    ret = -1;
	    goto done;
	}
	tind[1] = (tind[0]+1) % 3;
	tind[2] = (tind[0]+2) % 3;
	//bu_log("ctrl: %d %d %d\n", ctrl_tri[0], ctrl_tri[1], ctrl_tri[2]);
	//bu_log("test: %d %d %d\n", test_tri[tind[0]], test_tri[tind[1]], test_tri[tind[2]]);

	if (ctrl_tri[1] == test_tri[tind[2]] && ctrl_tri[2] == test_tri[tind[1]]) {
	    misaligned++;
	    i = i+3;
	    continue;
	}
	if (ctrl_tri[1] == test_tri[tind[1]] && ctrl_tri[2] == test_tri[tind[2]]) {
	    matched++;
	    i = i+3;
	    continue;
	}

	bu_log("%s: completely disjoint triangles at position %d\n", tname, i);
	ret = -1;
	goto done;
    }

    if (matched != tri_cnt && misaligned != tri_cnt) {
	bu_log("%s: triangle sets did not match\n", tname);
	ret = -1;
    }

done:
    if (ret == -1) {
	parrays(ff, ctrl);
    }

    return ret;
}


int
main(int UNUSED(argc), const char *argv[])
{
    int ret = 0;

    bu_setprogname(argv[0]);

    int arb4_faces_flipped_f1[13] = {0,2,1,  2,1,3,  0,3,1,  0,2,3,  INT_MAX};
    if (bg_trimesh_sync(arb4_faces_flipped_f1, arb4_faces_flipped_f1, 4) < 0)
	ret = -1;
    if (fcheck("ARB4_face1", arb4_faces_flipped_f1, arb4_faces_ctrl))
	ret = -1;

    int arb4_faces_flipped_f2[13] = {0,1,2,  2,3,1,  0,3,1,  0,2,3,  INT_MAX};
    if (bg_trimesh_sync(arb4_faces_flipped_f2, arb4_faces_flipped_f2, 4) < 0)
	ret = -1;
    if (fcheck("ARB4_face2", arb4_faces_flipped_f2, arb4_faces_ctrl))
	ret = -1;

    int arb4_faces_flipped_f3[13] = {0,1,2,  2,1,3,  0,1,3,  0,2,3,  INT_MAX};
    if (bg_trimesh_sync(arb4_faces_flipped_f3, arb4_faces_flipped_f3, 4) < 0)
	ret = -1;
    if (fcheck("ARB4_face3", arb4_faces_flipped_f3, arb4_faces_ctrl))
	ret = -1;

    int arb4_faces_flipped_f4[13] = {0,1,2,  2,1,3,  0,3,1,  0,3,2,  INT_MAX};
    if (bg_trimesh_sync(arb4_faces_flipped_f4, arb4_faces_flipped_f4, 4) < 0)
	ret = -1;
    if (fcheck("ARB4_face4", arb4_faces_flipped_f4, arb4_faces_ctrl))
	ret = -1;

    return ret;
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
