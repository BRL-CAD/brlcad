/*       P O L Y G O N _ T R I A N G U L A T E _ C L E A N . C
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 */

#include "common.h"

#include <math.h>

#include "bu.h"
#include "bg.h"
#include "vmath.h"


static int
check_clean_triangulation(const point2d_t *points, size_t point_count,
	const int *outer, size_t outer_count, const int **holes,
	const size_t *hole_counts, size_t hole_count, const int *steiner,
	size_t steiner_count, double expected_area)
{
    int *faces = NULL;
    int face_count = 0;
    point2d_t *out_points = NULL;
    int out_point_count = 0;
    int ret = bg_nested_poly_triangulate_clean(&faces, &face_count,
	&out_points, &out_point_count, outer, outer_count, holes,
	hole_counts, hole_count, steiner, steiner_count, NULL, 0, points,
	point_count);
    if (ret || !faces || !out_points || face_count <= 0 ||
	    out_point_count <= 0) {
	bu_log("clean test %.17g failed: ret %d faces %d points %d\n",
	    expected_area, ret, face_count, out_point_count);
	return 1;
	}

    double area = 0.0;
    for (int i = 0; i < face_count; i++) {
	const int a = faces[3 * i];
	const int b = faces[3 * i + 1];
	const int c = faces[3 * i + 2];
	if (a < 0 || b < 0 || c < 0 || a >= out_point_count ||
		b >= out_point_count || c >= out_point_count) {
	    ret = 1;
	    break;
	}
	const double twice_area =
	    (out_points[b][X] - out_points[a][X]) *
	    (out_points[c][Y] - out_points[a][Y]) -
	    (out_points[b][Y] - out_points[a][Y]) *
	    (out_points[c][X] - out_points[a][X]);
	if (!isfinite(twice_area)) {
	    ret = 1;
	    break;
	}
	area += 0.5 * fabs(twice_area);
    }

    if (fabs(area - expected_area) > 1.0e-6) {
	bu_log("clean test %.17g area %.17g\n", expected_area, area);
	ret = 1;
	}

    bu_free(faces, "clean triangulation faces");
    bu_free(out_points, "clean triangulation points");
    return ret;
}

static int
check_clean_constraint(void)
{
    point2d_t points[6] = {
	{0.0, 0.0}, {10.0, 0.0}, {10.0, 10.0}, {0.0, 10.0},
	{2.0, 5.0}, {8.0, 5.0}
    };
    const int outer[4] = {0, 1, 2, 3};
    const int steiner[2] = {4, 5};
    const int constraints[2] = {4, 5};
    int *faces = NULL;
    int face_count = 0;
    point2d_t *out_points = NULL;
    int out_point_count = 0;
    int ret = bg_nested_poly_triangulate_clean(&faces,
	&face_count, &out_points, &out_point_count, outer, 4, NULL, NULL, 0,
	steiner, 2, constraints, 1, (const point2d_t *)points, 6);
    if (ret || !faces || !out_points || face_count <= 0 ||
	    out_point_count <= 0) {
	bu_log("constrained clean failed: ret %d faces %d points %d\n",
	    ret, face_count, out_point_count);
	ret = 1;
	goto done;
    }

    int endpoints[2] = {-1, -1};
    for (int i = 0; i < out_point_count; ++i) {
	if (V2NEAR_EQUAL(out_points[i], points[4], 1.0e-8))
	    endpoints[0] = i;
	if (V2NEAR_EQUAL(out_points[i], points[5], 1.0e-8))
	    endpoints[1] = i;
    }
    if (endpoints[0] < 0 || endpoints[1] < 0) {
	bu_log("constrained clean omitted an endpoint\n");
	ret = 1;
	goto done;
    }
    ret = 1;
    for (int i = 0; i < face_count; ++i) {
	for (int corner = 0; corner < 3; ++corner) {
	    const int first = faces[3 * i + corner];
	    const int second = faces[3 * i + (corner + 1) % 3];
	    if ((first == endpoints[0] && second == endpoints[1]) ||
		    (first == endpoints[1] && second == endpoints[0])) {
		ret = 0;
		goto done;
	    }
	}
    }

    bu_log("constrained clean omitted its certified edge\n");

done:
    if (faces)
	bu_free(faces, "clean constrained faces");
    if (out_points)
	bu_free(out_points, "clean constrained points");
    return ret;
}


int
main(int argc, const char **argv)
{
    bu_setprogname(argv[0]);
    if (argc != 1)
	return 1;

    {
	point2d_t points[9] = {
	    {0.0, 0.0}, {2.0, 0.0}, {4.0, 0.0},
	    {4.0, 4.0}, {0.0, 4.0}, {0.0, 0.0},
	    {1.0, 0.0}, {2.0, 2.0}, {2.0, 2.0}
	};
	const int outer[6] = {0, 1, 2, 3, 4, 5};
	const int steiner[3] = {6, 7, 8};
	if (check_clean_triangulation((const point2d_t *)points, 9, outer,
		6, NULL, NULL,
		0, steiner, 3, 16.0))
	    return 1;
    }

    {
	point2d_t points[12] = {
	    {0.0, 0.0}, {10.0, 0.0}, {10.0, 10.0}, {0.0, 10.0},
	    {2.0, 2.0}, {2.0, 6.0}, {6.0, 6.0}, {6.0, 2.0},
	    {4.0, 4.0}, {4.0, 8.0}, {8.0, 8.0}, {8.0, 4.0}
	};
	const int outer[4] = {0, 1, 2, 3};
	const int hole1[4] = {4, 5, 6, 7};
	const int hole2[4] = {8, 9, 10, 11};
	const int *holes[2] = {hole1, hole2};
	const size_t hole_counts[2] = {4, 4};
	if (check_clean_triangulation((const point2d_t *)points, 12, outer,
		4, holes,
		hole_counts, 2, NULL, 0, 72.0))
	    return 1;
    }

    {
	point2d_t points[8] = {
	    {3.0, 3.0}, {7.0, 3.0}, {7.0, 7.0}, {3.0, 7.0},
	    {1.0, 1.0}, {1.0, 9.0}, {9.0, 9.0}, {9.0, 1.0}
	};
	const int outer[4] = {0, 1, 2, 3};
	const int hole[4] = {4, 5, 6, 7};
	const int *holes[1] = {hole};
	const size_t hole_counts[1] = {4};
	if (check_clean_triangulation((const point2d_t *)points, 8, outer,
		4, holes,
		hole_counts, 1, NULL, 0, 48.0))
	    return 1;
    }

    {
	/* A weakly-simple ring may encode a hole touching its outline at one
	 * topology vertex.  Strictly-simple Clipper output must retain the
	 * intended outer-minus-hole area. */
	point2d_t points[8] = {
	    {0.0, 0.0}, {10.0, 0.0}, {10.0, 10.0}, {0.0, 10.0},
	    {0.0, 0.0}, {0.0, 4.0}, {4.0, 4.0}, {4.0, 0.0}
	};
	const int outer[8] = {0, 1, 2, 3, 4, 5, 6, 7};
	if (check_clean_triangulation((const point2d_t *)points, 8, outer,
		8, NULL, NULL, 0, NULL, 0, 84.0))
	    return 1;
    }

    {
	/* A zero-area A-B-A whisker in a hole does not change the filled set. */
	point2d_t points[11] = {
	    {0.0, 0.0}, {10.0, 0.0}, {10.0, 10.0}, {0.0, 10.0},
	    {2.0, 2.0}, {8.0, 2.0}, {8.0, 8.0}, {2.0, 8.0},
	    {2.0, 5.0}, {1.0, 5.0}, {2.0, 5.0}
	};
	const int outer[4] = {0, 1, 2, 3};
	const int hole[7] = {4, 5, 6, 7, 8, 9, 10};
	const int *holes[1] = {hole};
	const size_t hole_counts[1] = {7};
	if (check_clean_triangulation((const point2d_t *)points, 11, outer,
		4, holes, hole_counts, 1, NULL, 0, 64.0))
	    return 1;
    }

    {
	/* Clipper rejects a wholly collinear closed path.  As a hole it
	 * removes no area and must not invalidate the containing polygon. */
	point2d_t points[11] = {
	    {0.0, 0.0}, {10.0, 0.0}, {10.0, 10.0}, {0.0, 10.0},
	    {2.0, 5.0}, {3.0, 5.0}, {4.0, 5.0}, {5.0, 5.0},
	    {6.0, 5.0}, {7.0, 5.0}, {8.0, 5.0}
	};
	const int outer[4] = {0, 1, 2, 3};
	const int hole[7] = {4, 5, 6, 7, 8, 9, 10};
	const int *holes[1] = {hole};
	const size_t hole_counts[1] = {7};
	if (check_clean_triangulation((const point2d_t *)points, 11, outer,
		4, holes, hole_counts, 1, NULL, 0, 100.0))
	    return 1;
    }

    {
	/* A hole touching the middle of an outline edge is a notch.  Clipper
	 * must split the T-junction before detria sees the constraint ring. */
	point2d_t points[7] = {
	    {0.0, 0.0}, {10.0, 0.0}, {10.0, 10.0}, {0.0, 10.0},
	    {5.0, 0.0}, {4.0, 2.0}, {6.0, 2.0}
	};
	const int outer[4] = {0, 1, 2, 3};
	const int hole[3] = {4, 5, 6};
	const int *holes[1] = {hole};
	const size_t hole_counts[1] = {3};
	if (check_clean_triangulation((const point2d_t *)points, 7, outer,
		4, holes, hole_counts, 1, NULL, 0, 98.0))
	    return 1;
    }

    if (check_clean_constraint())
	return 1;

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
