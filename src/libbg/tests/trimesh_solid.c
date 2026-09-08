/*                  T R I M E S H _ S O L I D . C
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by the
 * U.S. Army Research Laboratory.
 *
 * Distributed under the terms of the GNU Lesser General Public License
 * (LGPL), version 2.1.
 */

#include "common.h"

#include "bu/malloc.h"
#include "bg/trimesh.h"

static int
check(int *faces, int count, int unmatched, int misoriented, int excess)
{
    fastf_t points[15] = {0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, -1, 0};
    struct bg_trimesh_solid_errors errors = BG_TRIMESH_SOLID_ERRORS_INIT_NULL;
    int *edges = NULL;
    const int expected = unmatched || misoriented || excess;
    int failed = (bg_trimesh_solid2(5, count, points, faces, &errors) != 0) != expected;
    failed |= errors.unmatched.count != unmatched;
    failed |= errors.misoriented.count != misoriented;
    failed |= errors.excess.count != excess;
    failed |= (bg_trimesh_solid2(5, count, points, faces, NULL) != 0) != expected;
    failed |= bg_trimesh_solid(5, count, points, faces, &edges) != expected;
    failed |= expected && !edges;
    bg_free_trimesh_solid_errors(&errors);
    bu_free(edges, "bad edges");
    return failed;
}

int
main(void)
{
    /* Reverse one face of a tetrahedron, then attach a fin to an edge.
     * Boundary, orientation, and overuse defects must be reported together. */
    int faces[15] = {0, 1, 2, 0, 1, 3, 1, 2, 3, 2, 0, 3, 0, 1, 4};
    if (check(faces, 5, 2, 2, 1) || check(faces, 4, 0, 3, 0))
	return 1;
    faces[1] = 2;
    faces[2] = 1;
    if (check(faces, 5, 2, 0, 1) || check(faces, 4, 0, 0, 0))
	return 2;
    return 0;
}

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
