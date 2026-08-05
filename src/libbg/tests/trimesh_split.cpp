/*                 T R I M E S H _ S P L I T . C P P
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 */

#include "common.h"

#include "bu/log.h"
#include "bu/malloc.h"
#include "bg/trimesh.h"


static void
free_face_sets(int **face_sets, int *face_counts, int set_cnt)
{
    if (face_sets) {
	for (int i = 0; i < set_cnt; i++)
	    bu_free(face_sets[i], "face set");
	bu_free(face_sets, "face sets");
    }
    if (face_counts)
	bu_free(face_counts, "face counts");
}


int
main()
{
    // The first three triangles all share edge 0-1.  All three must be in
    // one component even though that edge is non-manifold.  The last triangle
    // is disjoint and must form a second component.
    int faces[] = {
	0, 1, 2,
	1, 0, 3,
	0, 1, 4,
	5, 6, 7
    };
    int **face_sets = NULL;
    int *face_counts = NULL;
    int set_cnt = bg_trimesh_split(&face_sets, &face_counts, faces, 4);

    if (set_cnt != 2 || !face_sets || !face_counts ||
	    face_counts[0] != 3 || face_counts[1] != 1) {
	bu_log("FAIL: expected connected component sizes 3 and 1, got");
	if (face_counts) {
	    for (int i = 0; i < set_cnt; i++)
		bu_log(" %d", face_counts[i]);
	}
	bu_log("\n");
	free_face_sets(face_sets, face_counts, set_cnt > 0 ? set_cnt : 0);
	return 1;
    }

    free_face_sets(face_sets, face_counts, set_cnt);
    return 0;
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
