/*                   B A L L P I V O T . C P P
 * BRL-CAD
 *
 * Copyright (c) 2025 United States Government as represented by
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
/** @file ballpivot.cpp
 *
 * Wrapper function for BallPivot.hpp implementation
 */

#include "common.h"
#include "vmath.h"
#include "bu/log.h"
#include "bu/malloc.h"
#include "bg/trimesh.h"
#include "bg/ballpivot.h"
#include "BallPivot.hpp"

int bg_3d_ballpivot(
	int **faces, int *num_faces, point_t **vertices, int *num_vertices,
	const point_t *input_points_3d, const vect_t *input_normals_3d,
	int num_input_pnts, const double *radii, int radii_cnt)
{
    if (!faces || !num_faces || !vertices || !num_vertices || !input_points_3d || !input_normals_3d || num_input_pnts < 3)
	return -1;

    // Run the Ball Pivoting Algorithm (radius selection is handled inside)
    std::vector<int> face_vec;
    int nfaces = ball_pivoting_run(face_vec, input_points_3d, input_normals_3d, num_input_pnts, radii, radii_cnt);

    if (nfaces == 0) {
	*faces = NULL;
	*num_faces = 0;
	*vertices = NULL;
	*num_vertices = 0;
	return -2;
    }

    // Compact the mesh: only keep vertices actually used in the mesh
    int *comp_faces = NULL;
    point_t *comp_verts = NULL;
    int n_comp_verts = 0;
    int n_comp_faces = bg_trimesh_3d_gc(
	    &comp_faces, &comp_verts, &n_comp_verts,
	    face_vec.data(), nfaces, input_points_3d
	    );

    if (n_comp_faces < 0) {
	*faces = NULL;
	*num_faces = 0;
	*vertices = NULL;
	*num_vertices = 0;
	return -3;
    }

    *faces = comp_faces;
    *num_faces = n_comp_faces;
    *vertices = comp_verts;
    *num_vertices = n_comp_verts;

    return 0;
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8 cino=N-s
