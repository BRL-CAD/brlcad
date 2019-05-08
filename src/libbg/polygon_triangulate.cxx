/*         P O L Y G O N _ T R I A N G U L A T E . C X X
 * BRL-CAD
 *
 * Copyright (c) 2019 United States Government as represented by
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
/** @file polygon_triangulate.cxx
 *
 * libbg wrapper functions for Polygon Triangulation codes
 *
 */

#include "common.h"

#include <vector>
#include <array>

#if defined(__GNUC__) && !defined(__clang__)
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wfloat-equal"
#endif
#if defined(__clang__)
#  pragma clang diagnostic push
#  pragma clang diagnostic ignored "-Wfloat-equal"
#endif
#include "./earcut.hpp"
#if defined(__GNUC__) && !defined(__clang__)
#  pragma GCC diagnostic pop
#endif
#if defined(__clang__)
#  pragma clang diagnostic pop
#endif

#include "bu/malloc.h"
#include "bg/polygon.h"

extern "C" int
bg_nested_polygon_triangulate(int **faces, int *num_faces, point2d_t **out_pts, int *num_outpts,
	const int *poly, const size_t poly_pnts,
	const int **holes_array, const size_t *holes_npts, const size_t nholes,
	const point2d_t *pts, const size_t npts, triangulation_t type)
{
    if (npts < 3 || poly_pnts < 3) return 1;
    if (!faces || !num_faces || !pts || !poly) return 1;

    if (nholes > 0) {
	if (!holes_array || !holes_npts) return 1;
    }

    if (type == DELAUNAY && (!out_pts || !num_outpts)) return 1;

    /* Set up for ear clipping */
    using Coord = fastf_t;
    using N = uint32_t;
    using Point = std::array<Coord, 2>;
    std::vector<std::vector<Point>> polygon;
    std::vector<Point> outer_polygon;
    for (size_t i = 0; i < poly_pnts; i++) {
	Point np;
	np[0] = pts[poly[i]][X];
	np[1] = pts[poly[i]][Y];
	outer_polygon.push_back(np);
    }
    polygon.push_back(outer_polygon);

    if (holes_array) {
	for (size_t i = 0; i < nholes; i++) {
	    std::vector<Point> hole_polygon;
	    for (size_t j = 0; j < holes_npts[i]; j++) {
		Point np;
		np[0] = pts[holes_array[i][j]][X];
		np[1] = pts[holes_array[i][j]][Y];
		hole_polygon.push_back(np);
	    }
	    polygon.push_back(hole_polygon);
	}
    }

    std::vector<N> indices = mapbox::earcut<N>(polygon);

    if (indices.size() < 3) {
	return 1;
    }

    (*num_faces) = indices.size()/3;
    (*faces) = (int *)bu_calloc(indices.size(), sizeof(int), "faces");

    for (size_t i = 0; i < indices.size()/3; i++) {
	(*faces)[3*i] = (int)indices[3*i];
	(*faces)[3*i+1] = (int)indices[3*i+1];
	(*faces)[3*i+2] = (int)indices[3*i+2];
    }

    return 0;
}

extern "C" int
bg_polygon_triangulate(int **faces, int *num_faces, point2d_t **out_pts, int *num_outpts, const point2d_t *pts, const size_t npts, triangulation_t type)
{
    int ret;

    if (type == DELAUNAY && (!out_pts || !num_outpts)) return 1;

    int *verts_ind = NULL;
    verts_ind = (int *)bu_calloc(npts, sizeof(int), "vert indices");
    for (size_t i = 0; i < npts; i++) {
	verts_ind[i] = (int)i;
    }

    ret = bg_nested_polygon_triangulate(faces, num_faces, out_pts, num_outpts, verts_ind, npts, NULL, NULL, 0, pts, npts, type);

    bu_free(verts_ind, "vert indices");
    return ret;
}



// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
