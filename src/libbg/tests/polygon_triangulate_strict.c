/*       P O L Y G O N _ T R I A N G U L A T E _ S T R I C T . C
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by the
 * U.S. Army Research Laboratory.
 *
 * Distributed under the terms of the GNU Lesser General Public License
 * (LGPL), version 2.1.
 */

#include "common.h"

#include <math.h>

#include "bu.h"
#include "bg.h"

static int
run_case(const point2d_t *points, size_t point_count, const int *outer,
	size_t outer_count, const int **holes, const size_t *hole_counts,
	size_t hole_count, const int *steiner, size_t steiner_count,
	int expect_success, int forbidden_index)
{
    int *faces = NULL;
    int face_count = 0;
    point2d_t *out_points = NULL;
    int out_point_count = 0;
    struct bg_triangulation_report report = {0, -1, {0}};
    int ret = bg_nested_poly_triangulate_strict(&faces, &face_count,
	&out_points, &out_point_count, outer, outer_count, holes, hole_counts,
	hole_count, steiner, steiner_count, points, point_count, &report);
    if (!expect_success) {
	if (!ret || faces || face_count || out_points || out_point_count ||
		report.reason == BG_TRIANGULATION_OK || !report.message[0]) {
	    bu_free(faces, "unexpected strict faces");
	    return 1;
	}
	return 0;
    }
    if (ret || report.reason != BG_TRIANGULATION_OK || !faces ||
	    face_count <= 0 ||
	    (const void *)out_points != (const void *)points ||
	    out_point_count <= 0) {
	bu_free(faces, "strict faces");
	return 1;
    }
    for (int i = 0; i < face_count * 3; ++i) {
	if (faces[i] < 0 || (size_t)faces[i] >= point_count ||
		faces[i] == forbidden_index) {
	    bu_free(faces, "strict faces");
	    return 1;
	}
    }
    bu_free(faces, "strict faces");
    return 0;
}

int
main(int argc, const char **argv)
{
    bu_setprogname(argv[0]);
    if (argc != 1)
	return 1;

    {
	point2d_t points[67];
	int outer[67];
	for (int i = 0; i < 65; ++i) {
	    V2SET(points[i], (double)i, 0.0);
	    outer[i] = i;
	}
	V2SET(points[65], 64.0, 10.0);
	V2SET(points[66], 0.0, 10.0);
	outer[65] = 65;
	outer[66] = 66;
	if (run_case((const point2d_t *)points, 67, outer, 67, NULL,
		NULL, 0, NULL, 0, 1, -1))
	    return 2;
    }

    {
	point2d_t points[5] = {
	    {0.0, 0.0}, {1.0, 0x0.0000000000001p-1022}, {2.0, 0.0},
	    {2.0, 2.0}, {0.0, 2.0}
	};
	const int outer[5] = {0, 1, 2, 3, 4};
	if (run_case((const point2d_t *)points, 5, outer, 5, NULL, NULL,
		0, NULL, 0, 1, -1))
	    return 3;
    }

    {
	point2d_t points[17];
	int outer[16];
	for (int i = 0; i < 16; ++i) {
	    const double angle = 2.0 * acos(-1.0) * i / 16.0;
	    V2SET(points[i], cos(angle), sin(angle));
	    outer[i] = i;
	}
	V2SET(points[16], 0.0, 0.0);
	const int steiner[1] = {16};
	if (run_case((const point2d_t *)points, 17, outer, 16, NULL,
		NULL, 0, steiner, 1, 1, -1))
	    return 4;
    }

    {
	point2d_t points[5] = {
	    {0.0, 0.0}, {2.0, 0.0}, {2.0, 2.0}, {0.0, 2.0},
	    {1.0, 0.0}
	};
	const int outer[4] = {0, 1, 2, 3};
	const int steiner[1] = {4};
	if (run_case((const point2d_t *)points, 5, outer, 4, NULL, NULL,
		0, steiner, 1, 1, 4))
	    return 5;
    }

    {
	point2d_t points[5] = {
	    {0.0, 0.0}, {2.0, 0.0}, {2.0, 2.0}, {0.0, 2.0},
	    {0.0, 0.0}
	};
	const int outer[5] = {0, 1, 2, 3, 4};
	if (run_case((const point2d_t *)points, 5, outer, 5, NULL, NULL,
		0, NULL, 0, 0, -1))
	    return 6;
    }

    {
	point2d_t points[4] = {
	    {0.0, 0.0}, {2.0, 2.0}, {0.0, 2.0}, {2.0, 0.0}
	};
	const int outer[4] = {0, 1, 2, 3};
	if (run_case((const point2d_t *)points, 4, outer, 4, NULL, NULL,
		0, NULL, 0, 0, -1))
	    return 7;
    }

    {
	point2d_t points[12] = {
	    {0.0, 0.0}, {10.0, 0.0}, {10.0, 10.0}, {0.0, 10.0},
	    {2.0, 2.0}, {2.0, 8.0}, {8.0, 8.0}, {8.0, 2.0},
	    {3.0, 3.0}, {3.0, 4.0}, {4.0, 4.0}, {4.0, 3.0}
	};
	const int outer[4] = {0, 1, 2, 3};
	const int hole1[4] = {4, 5, 6, 7};
	const int hole2[4] = {8, 9, 10, 11};
	const int *holes[2] = {hole1, hole2};
	const size_t counts[2] = {4, 4};
	if (run_case((const point2d_t *)points, 12, outer, 4, holes,
		counts, 2, NULL, 0, 0, -1))
	    return 8;
    }

    {
	const double scales[2] = {0x1p900, 0x1p-900};
	const int outer[4] = {0, 1, 2, 3};
	for (int i = 0; i < 2; ++i) {
	    point2d_t points[4] = {
		{0.0, 0.0}, {scales[i], 0.0},
		{scales[i], scales[i]}, {0.0, scales[i]}
	    };
	    if (run_case((const point2d_t *)points, 4, outer, 4, NULL,
		    NULL, 0, NULL, 0, 1, -1))
		return 9 + i;
	}
    }

    {
	const double base = 0x1p500;
	const double delta = 0x1p450;
	point2d_t points[4] = {
	    {base, base}, {base + delta, base},
	    {base + delta, base + delta}, {base, base + delta}
	};
	const int outer[4] = {0, 1, 2, 3};
	if (run_case((const point2d_t *)points, 4, outer, 4, NULL, NULL,
		0, NULL, 0, 1, -1))
	    return 11;
    }

    {
	point2d_t points[4] = {
	    {0.0, 0.0}, {1.0, 0.0}, {1.0, INFINITY}, {0.0, 1.0}
	};
	const int outer[4] = {0, 1, 2, 3};
	if (run_case((const point2d_t *)points, 4, outer, 4, NULL, NULL,
		0, NULL, 0, 0, -1))
	    return 12;
	const int invalid_outer[4] = {0, 1, 4, 3};
	if (run_case((const point2d_t *)points, 4, invalid_outer, 4, NULL,
		NULL, 0, NULL, 0, 0, -1))
	    return 13;
    }

    return 0;
}
