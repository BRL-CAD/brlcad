/*         P O L Y G O N _ T R I A N G U L A T E . C P P
 * BRL-CAD
 *
 * Copyright (c) 2019-2021 United States Government as represented by
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
/** @file polygon_triangulate.cpp
 *
 * libbg wrapper functions for Polygon Triangulation codes
 *
 */

#include "common.h"

#include "bio.h"

#include <array>
#include <map>
#include <set>
#include <vector>

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

#include "delaunator.hpp"

#include "poly2tri/poly2tri.h"

#include "bu/malloc.h"
#include "bn/plot3.h"
#include "bg/polygon.h"


static int
bg_poly2tri(int **faces, int *num_faces, point2d_t **out_pts, int *num_outpts,
	    const int *poly, const size_t poly_pnts,
	    const int **holes_array, const size_t *holes_npts, const size_t nholes,
	    const int *steiner, const size_t steiner_npts,
	    const point2d_t *pts)
{
    std::map<p2t::Point *, size_t> p2t_to_ind;
    std::set<p2t::Point *> p2t_pnts;
    std::set<p2t::Point *>::iterator p_it;

    // Outer polygon is defined first
    std::vector<p2t::Point*> outer_polyline;
    for (size_t i = 0; i < poly_pnts; i++) {
	p2t::Point *p = new p2t::Point(pts[poly[i]][X], pts[poly[i]][Y]);
	outer_polyline.push_back(p);
	p2t_to_ind[p] = poly[i];
	p2t_pnts.insert(p);
    }

    // Initialize the cdt structure
    p2t::CDT *cdt = new p2t::CDT(outer_polyline);

    // Add holes, if any
    for (size_t h = 0; h < nholes; h++) {
	std::vector<p2t::Point*> polyline;
	for (size_t i = 0; i < holes_npts[h]; i++) {
	    p2t::Point *p = new p2t::Point(pts[holes_array[h][i]][X], pts[holes_array[h][i]][Y]);
	    polyline.push_back(p);
	    p2t_to_ind[p] = holes_array[h][i];
	    p2t_pnts.insert(p);
	}
	cdt->AddHole(polyline);
    }

    // Add steiner points, if any
    for (size_t s = 0; s < steiner_npts; s++) {
	p2t::Point *p = new p2t::Point(pts[steiner[s]][X], pts[steiner[s]][Y]);
	p2t_to_ind[p] = steiner[s];
	p2t_pnts.insert(p);
	cdt->AddPoint(p);
    }

    // Run the core triangulation routine
    {
	try {
	    cdt->Triangulate(true, -1);
	}
	catch (...) {
	    return 1;
	}
    }

    // If we didn't get any triangles, fail
    if (!cdt->GetTriangles().size()) {
	return 1;
    }

    // Unpack the Poly2Tri faces into a C container
    std::vector<p2t::Triangle*> tris = cdt->GetTriangles();

    (*num_faces) = (int)tris.size();
    int *nfaces = (int *)bu_calloc(*num_faces * 3, sizeof(int), "faces array");
    for (size_t i = 0; i < tris.size(); i++) {
	p2t::Triangle *t = tris[i];
	nfaces[3*i] = p2t_to_ind[t->GetPoint(0)];
	nfaces[3*i+1] = p2t_to_ind[t->GetPoint(1)];
	nfaces[3*i+2] = p2t_to_ind[t->GetPoint(2)];
    }
    (*faces) = nfaces;

    // We're not generating a new points array, so if out_pts exists set it to the
    // input points array
    if (out_pts) {
	(*out_pts) = (point2d_t *)pts;
    }
    if (num_outpts) {
	// Build the set of unique points - even if num_outpts != npts the
	// caller will be able to tell if all points were used, even though
	// out_pts won't contain just the used set of points.
	std::set<p2t::Point *> p2t_active_pnts;
	for (size_t i = 0; i < tris.size(); i++) {
	    p2t::Triangle *t = tris[i];
	    for (size_t j = 0; j < 3; j++) {
		p2t_active_pnts.insert(t->GetPoint(j));
	    }
	}

	(*num_outpts) = (int) p2t_active_pnts.size();
    }

    // Cleanup
    delete cdt;

    return 0;
}


extern "C" int
bg_nested_polygon_triangulate(int **faces, int *num_faces, point2d_t **out_pts, int *num_outpts,
			      const int *poly, const size_t poly_pnts,
			      const int **holes_array, const size_t *holes_npts, const size_t nholes,
			      const int *steiner, const size_t steiner_npts,
			      const point2d_t *pts, const size_t npts, triangulation_t type)
{
    if (npts < 3 || poly_pnts < 3) return 1;
    if (!faces || !num_faces || !pts || !poly) return 1;

    if (nholes > 0) {
	if (!holes_array || !holes_npts) return 1;
    }

    //if (type == TRI_DELAUNAY && (!out_pts || !num_outpts)) return 1;

    if (type == TRI_ANY || type == TRI_CONSTRAINED_DELAUNAY) {
	int p2t_ret = bg_poly2tri(faces, num_faces, out_pts, num_outpts, poly, poly_pnts, holes_array, holes_npts, nholes, steiner, steiner_npts, pts);
	return p2t_ret;
    }

    if (type == TRI_DELAUNAY) {
	std::vector<double> coords;
	for (size_t i = 0; i < npts; i++) {
	    coords.push_back(pts[i][X]);
	    coords.push_back(pts[i][Y]);
	}
	delaunator::Delaunator d(coords);

	(*num_faces) = d.triangles.size()/3;
	(*faces) = (int *)bu_calloc(d.triangles.size(), sizeof(int), "faces");

	for (size_t i = 0; i < d.triangles.size()/3; i++) {
	    (*faces)[3*i] = (int)d.triangles[3*i];
	    (*faces)[3*i+1] = (int)d.triangles[3*i+1];
	    (*faces)[3*i+2] = (int)d.triangles[3*i+2];
	}

	return 0;
    }

    if (type == TRI_EAR_CLIPPING) {

	if (steiner_npts) return 1;

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

    /* Unimplemented type specified */
    return -1;
}

extern "C" int
bg_polygon_triangulate(int **faces, int *num_faces, point2d_t **out_pts, int *num_outpts,
	const int *steiner, const size_t steiner_pnts,
	const point2d_t *pts, const size_t npts, triangulation_t type)
{
    int ret;

    if (type == TRI_DELAUNAY && (!out_pts || !num_outpts)) return 1;

    int *verts_ind = NULL;
    verts_ind = (int *)bu_calloc(npts, sizeof(int), "vert indices");
    for (size_t i = 0; i < npts; i++) {
	verts_ind[i] = (int)i;
    }

    ret = bg_nested_polygon_triangulate(faces, num_faces, out_pts, num_outpts, verts_ind, npts, NULL, NULL, 0, steiner, steiner_pnts, pts, npts, type);

    bu_free(verts_ind, "vert indices");
    return ret;
}


extern "C" void
bg_tri_plot_2d(const char *filename, const int *faces, int num_faces, const point2d_t *pnts, int r, int g, int b)
{
    FILE* plot_file = fopen(filename, "wb");
    pl_color(plot_file, r, g, b);

    for (int k = 0; k < num_faces; k++) {
	point_t p1, p2, p3;
	VSET(p1, pnts[faces[3*k]][X], pnts[faces[3*k]][Y], 0);
	VSET(p2, pnts[faces[3*k+1]][X], pnts[faces[3*k+1]][Y], 0);
	VSET(p3, pnts[faces[3*k+2]][X], pnts[faces[3*k+2]][Y], 0);

	pdv_3move(plot_file, p1);
	pdv_3cont(plot_file, p2);
	pdv_3move(plot_file, p1);
	pdv_3cont(plot_file, p3);
	pdv_3move(plot_file, p2);
	pdv_3cont(plot_file, p3);
    }
    fclose(plot_file);
}



// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
