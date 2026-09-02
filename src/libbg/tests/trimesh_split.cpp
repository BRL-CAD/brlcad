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

#include <algorithm>
#include <cstdlib>
#include <vector>

#include "bu/app.h"
#include "bu/log.h"
#include "bu/malloc.h"
#include "bg/trimesh.h"

struct separate_case {
    const char *name;
    std::vector<int> faces;
    std::vector<int> expected_indices;
    std::vector<int> expected_offsets;
};


static bool
run_case(const separate_case &test_case)
{
    int *face_indices = NULL;
    int *component_offsets = NULL;
    int component_count = bg_trimesh_separate(&face_indices,
	&component_offsets, test_case.faces.data(),
	(int)(test_case.faces.size() / 3));

    bool passed = component_count == (int)test_case.expected_offsets.size() - 1;
    for (size_t i = 0; passed && i < test_case.expected_indices.size(); ++i)
	passed = face_indices[i] == test_case.expected_indices[i];
    for (size_t i = 0; passed && i < test_case.expected_offsets.size(); ++i)
	passed = component_offsets[i] == test_case.expected_offsets[i];

    if (!passed)
	bu_log("FAIL: %s\n", test_case.name);
    if (face_indices)
	bu_free(face_indices, "test component face indices");
    if (component_offsets)
	bu_free(component_offsets, "test component offsets");
    return passed;
}


static bool
test_component_gc()
{
    int faces[] = {
	2, 5, 7,
	7, 5, 9,
	0, 1, 3
    };
    point_t points[10];
    for (int i = 0; i < 10; ++i)
	VSET(points[i], i, i + 0.25, i + 0.5);

    int *face_indices = NULL;
    int *component_offsets = NULL;
    int component_count = bg_trimesh_separate(&face_indices,
	&component_offsets, faces, 3);
    if (component_count != 2) {
	bu_log("FAIL: component extraction setup\n");
	return false;
    }

    // bg_trimesh_separate returns original face indices.  Gather the selected
    // triples before using bg_trimesh_3d_gc to make a self-contained mesh.
    int first = component_offsets[0];
    int component_face_count = component_offsets[1] - first;
    std::vector<int> component_faces((size_t)component_face_count * 3);
    for (int i = 0; i < component_face_count; ++i) {
	int input_face = face_indices[first + i];
	std::copy_n(&faces[(size_t)input_face * 3], 3,
	    &component_faces[(size_t)i * 3]);
    }

    int *compact_faces = NULL;
    point_t *compact_points = NULL;
    int compact_point_count = 0;
    int compact_face_count = bg_trimesh_3d_gc(&compact_faces,
	&compact_points, &compact_point_count, component_faces.data(),
	component_face_count, points);

    int expected_faces[] = {0, 1, 2, 2, 1, 3};
    int expected_point_indices[] = {2, 5, 7, 9};
    bool passed = compact_face_count == 2 && compact_point_count == 4;
    for (size_t i = 0; passed && i < 6; ++i)
	passed = compact_faces[i] == expected_faces[i];
    for (size_t i = 0; passed && i < 4; ++i)
	passed = VEQUAL(compact_points[i], points[expected_point_indices[i]]);

    if (!passed)
	bu_log("FAIL: component extraction with bg_trimesh_3d_gc\n");
    bu_free(face_indices, "test component face indices");
    bu_free(component_offsets, "test component offsets");
    bu_free(compact_faces, "test compact faces");
    bu_free(compact_points, "test compact points");
    return passed;
}


int
main(int UNUSED(argc), char **argv)
{
    bu_setprogname(argv[0]);

    std::vector<separate_case> cases = {
	{
	    "non-manifold edge",
	    {
		0, 1, 2,
		1, 0, 3,
		0, 1, 4,
		5, 6, 7
	    },
	    {0, 1, 2, 3},
	    {0, 3, 4}
	},
	{
	    "vertex-only contact remains separate",
	    {
		0, 1, 2,
		0, 3, 4
	    },
	    {0, 1},
	    {0, 1, 2}
	},
	{
	    "duplicate and reversed faces",
	    {
		0, 1, 2,
		0, 1, 2,
		2, 1, 0,
		3, 4, 5
	    },
	    {0, 1, 2, 3},
	    {0, 3, 4}
	},
	{
	    "degenerate shared self-edge",
	    {
		0, 0, 1,
		0, 0, 2,
		3, 4, 5
	    },
	    {0, 1, 2},
	    {0, 2, 3}
	},
	{
	    "deterministic component order",
	    {
		10, 11, 12,
		0, 1, 2,
		12, 11, 13,
		2, 1, 3
	    },
	    {0, 2, 1, 3},
	    {0, 2, 4}
	}
    };

    for (const separate_case &test_case : cases) {
	if (!run_case(test_case))
	    return EXIT_FAILURE;
    }

    if (!test_component_gc())
	return EXIT_FAILURE;

    int *face_indices = (int *)1;
    int *component_offsets = (int *)1;
    if (bg_trimesh_separate(&face_indices, &component_offsets, NULL, 0) != 0 ||
	face_indices != NULL || component_offsets != NULL) {
	bu_log("FAIL: empty mesh\n");
	return EXIT_FAILURE;
    }

    int face[] = {0, 1, 2};
    if (bg_trimesh_separate(NULL, &component_offsets, face, 1) != -1 ||
	bg_trimesh_separate(&face_indices, NULL, face, 1) != -1 ||
	bg_trimesh_separate(&face_indices, &component_offsets, NULL, 1) != -1 ||
	bg_trimesh_separate(&face_indices, &component_offsets, face, -1) != -1) {
	bu_log("FAIL: invalid argument handling\n");
	return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
