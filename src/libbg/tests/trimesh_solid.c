/*                 T R I M E S H _ S O L I D . C
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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 */

#include "common.h"

#include "bu/app.h"
#include "bu/malloc.h"
#include "bg/trimesh.h"


static int
check_solid_diagnostics(int *faces, int face_count, int unmatched,
    int misoriented, int excess)
{
    fastf_t vertices[15] = {
	0.0, 0.0, 0.0,
	1.0, 0.0, 0.0,
	0.0, 1.0, 0.0,
	0.0, 0.0, 1.0,
	0.0, -1.0, 0.0
    };
    int *bad_edges = NULL;
    struct bg_trimesh_solid_errors errors = BG_TRIMESH_SOLID_ERRORS_INIT_NULL;
    const int expected = unmatched || misoriented || excess;
    int failed =
	((bg_trimesh_solid2(5, face_count, vertices, faces, &errors) != 0) !=
	 expected);

    failed |= errors.unmatched.count != unmatched;
    failed |= errors.misoriented.count != misoriented;
    failed |= errors.excess.count != excess;
    failed |= ((bg_trimesh_solid2(5, face_count, vertices, faces, NULL) != 0)
	!= expected);
    failed |= bg_trimesh_solid(5, face_count, vertices, faces,
	&bad_edges) != expected;
    failed |= (expected && !bad_edges) || (!expected && bad_edges);

    bg_free_trimesh_solid_errors(&errors);
    if (bad_edges)
	bu_free(bad_edges, "bad edges");

    return failed;
}


static int
test_solid_diagnostics(void)
{
    /* Reverse one tetrahedron face, then attach a fin to an edge.  Boundary,
     * orientation, and overuse defects must be reported independently. */
    int faces[15] = {
	0, 1, 2,
	0, 1, 3,
	1, 2, 3,
	2, 0, 3,
	0, 1, 4
    };

    if (check_solid_diagnostics(faces, 5, 2, 2, 1) ||
	check_solid_diagnostics(faces, 4, 0, 3, 0))
	return 1;

    faces[1] = 2;
    faces[2] = 1;
    return check_solid_diagnostics(faces, 5, 2, 0, 1) ||
	check_solid_diagnostics(faces, 4, 0, 0, 0);
}


static int
test_free_helpers(void)
{
    struct bg_trimesh_edges edges = BG_TRIMESH_EDGES_INIT_NULL;
    struct bg_trimesh_faces faces = BG_TRIMESH_FACES_INIT_NULL;

    edges.count = 1;
    edges.edges = (int *)bu_malloc(2 * sizeof(int), "test edges");
    faces.count = 1;
    faces.faces = (int *)bu_malloc(sizeof(int), "test faces");
    bg_free_trimesh_edges(&edges);
    bg_free_trimesh_faces(&faces);

    return edges.count != 0 || edges.edges || faces.count != 0 || faces.faces;
}


int
main(int argc, char **argv)
{
    bu_setprogname(argv[0]);
    if (argc != 1)
	return 1;

    return test_solid_diagnostics() || test_free_helpers();
}
