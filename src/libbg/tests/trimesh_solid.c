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

#include "bu.h"
#include "bg.h"


static int
test_solid_diagnostics(void)
{
    fastf_t vertices[][3] = {
	{0.0, 0.0, 0.0},
	{1.0, 0.0, 0.0},
	{0.0, 1.0, 0.0},
	{0.0, 0.0, 1.0},
	{1.0, 1.0, 1.0}
    };
    int solid_faces[] = {
	0, 2, 1,
	0, 1, 3,
	0, 3, 2,
	1, 2, 3
    };
    int reversed_faces[] = {
	0, 1, 2,
	0, 1, 3,
	0, 3, 2,
	1, 2, 3
    };
    int mixed_error_faces[] = {
	0, 1, 2,
	0, 1, 3,
	0, 3, 4,
	1, 2, 3
    };
    int *bad_edges = NULL;
    struct bg_trimesh_solid_errors errors = BG_TRIMESH_SOLID_ERRORS_INIT_NULL;
    int failed = 0;

    if (bg_trimesh_solid(4, 4, &vertices[0][0], solid_faces, &bad_edges) || bad_edges)
	failed = 1;

    if (bad_edges) {
	bu_free(bad_edges, "bad edges");
	bad_edges = NULL;
    }

    if (bg_trimesh_solid(4, 4, &vertices[0][0], reversed_faces, &bad_edges) != 1
	|| !bad_edges) {
	failed = 1;
    }

    if (bad_edges)
	bu_free(bad_edges, "bad edges");

    if (!bg_trimesh_solid2(5, 4, &vertices[0][0], mixed_error_faces, &errors)
	|| !errors.unmatched.count || !errors.misoriented.count) {
	failed = 1;
    }
    bg_free_trimesh_solid_errors(&errors);

    return failed;
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
