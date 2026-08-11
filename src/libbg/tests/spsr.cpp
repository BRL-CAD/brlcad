/*                         S P S R . C P P
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */

#include "common.h"

#include "bu/log.h"
#include "bu/malloc.h"
#include "bg/spsr.h"
#include "bg/trimesh.h"

int
main()
{
    const int side = 8;
    const int sample_count = 6 * side * side;
    point_t *samples = (point_t *)bu_calloc(sample_count,
	sizeof(point_t), "SPSR boundary test samples");
    vect_t *normals = (vect_t *)bu_calloc(sample_count,
	sizeof(vect_t), "SPSR boundary test normals");
    int sample = 0;
    for (int face = 0; face < 6; ++face) {
	const int axis = face / 2;
	const double sign = (face % 2) ? 1.0 : -1.0;
	for (int first = 0; first < side; ++first) {
	    for (int second = 0; second < side; ++second) {
		samples[sample][axis] = sign;
		samples[sample][(axis + 1) % 3] =
		    -1.0 + 2.0 * first / (side - 1);
		samples[sample][(axis + 2) % 3] =
		    -1.0 + 2.0 * second / (side - 1);
		normals[sample][axis] = sign;
		sample++;
	    }
	}
    }

    int face_counts[3] = {0, 0, 0};
    bool valid = true;
    for (int boundary = BG_3D_SPSR_BOUNDARY_FREE;
	    boundary <= BG_3D_SPSR_BOUNDARY_DIRICHLET; ++boundary) {
	struct bg_3d_spsr_opts options = BG_3D_SPSR_OPTS_DEFAULT;
	options.depth = 5;
	options.full_depth = 3;
	options.threads = 1;
	options.scale = 1.2;
	options.btype = boundary;
	int *faces = NULL;
	int face_count = 0;
	point_t *points = NULL;
	int point_count = 0;
	const int result = bg_3d_spsr(&faces, &face_count, &points,
	    &point_count, samples, normals, sample_count, &options);
	const bool solid = !result && !bg_trimesh_solid2(point_count,
	    face_count, (fastf_t *)points, faces, NULL);
	if (result || face_count <= 0 || point_count <= 0 || !solid) {
	    bu_log("SPSR boundary %d failed: result %d, %d points, "
		"%d faces, solid %d\n", boundary, result, point_count,
		face_count, (int)solid);
	    valid = false;
	}
	face_counts[boundary - BG_3D_SPSR_BOUNDARY_FREE] = face_count;
	bu_free(faces, "SPSR boundary test faces");
	bu_free(points, "SPSR boundary test points");
    }
    bu_free(samples, "SPSR boundary test samples");
    bu_free(normals, "SPSR boundary test normals");

    if (face_counts[0] == face_counts[1] &&
	    face_counts[1] == face_counts[2]) {
	bu_log("SPSR boundary modes all produced %d faces\n",
	    face_counts[0]);
	valid = false;
    }
    return valid ? 0 : 1;
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
