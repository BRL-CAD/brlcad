/*         P O L Y G O N _ T R I A N G U L A T E . C P P
 * BRL-CAD
 *
 * Copyright (c) 2019-2026 United States Government as represented by
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
#include <unordered_map>

#include "clipper.hpp"

#if defined(__GNUC__) && !defined(__clang__)
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wfloat-equal"
#endif
#if defined(__clang__)
#  pragma clang diagnostic push
#  pragma clang diagnostic ignored "-Wfloat-equal"
#endif
#include "./earcut.hpp"
#include "detria.hpp"
#if defined(__GNUC__) && !defined(__clang__)
#  pragma GCC diagnostic pop
#endif
#if defined(__clang__)
#  pragma clang diagnostic pop
#endif

#include "delaunator.hpp"

#ifndef PLOT_PREFIX_STR
#  define PLOT_PREFIX_STR bg_plot3_
#endif
#include "bu/malloc.h"
#include "bv/plot3.h"
#include "bg/polygon.h"
#include "bg/plane.h"

static bool
clipper_same_point(const ClipperLib::IntPoint &a,
	const ClipperLib::IntPoint &b)
{
    return a.X == b.X && a.Y == b.Y;
}


static bool
clipper_point_on_segment(const ClipperLib::IntPoint &p,
	const ClipperLib::IntPoint &a, const ClipperLib::IntPoint &b)
{
    const long double dx = (long double)b.X - a.X;
    const long double dy = (long double)b.Y - a.Y;
    const long double px = (long double)p.X - a.X;
    const long double py = (long double)p.Y - a.Y;
    const long double length = std::sqrt(dx * dx + dy * dy);
    if (!(length > 0.0L))
	return clipper_same_point(p, a);

    /* Clipper's integer snap may move a point by a fraction of one grid
     * unit.  Treat points within two grid units of a segment as boundary
     * points, while also requiring their projection to lie on the segment. */
    if (std::fabs(px * dy - py * dx) > 2.0L * length)
	return false;
    const long double dot = px * dx + py * dy;
    return dot >= 0.0L && dot <= dx * dx + dy * dy;
}


static bool
clipper_input_path(ClipperLib::Path &path, const int *indices,
	size_t count, const point2d_t *pts, double origin_x,
	double origin_y, double scale)
{
    path.clear();
    path.reserve(count);
    for (size_t i = 0; i < count; i++) {
	if (indices[i] < 0 || !std::isfinite(pts[indices[i]][X]) ||
		!std::isfinite(pts[indices[i]][Y]))
	    return false;
	ClipperLib::IntPoint p(
	    (ClipperLib::cInt)std::llround(
		(pts[indices[i]][X] - origin_x) * scale),
	    (ClipperLib::cInt)std::llround(
		(pts[indices[i]][Y] - origin_y) * scale));
	if (path.empty() || !clipper_same_point(path.back(), p))
	    path.push_back(p);
    }
    if (path.size() > 1 && clipper_same_point(path.front(), path.back()))
	path.pop_back();

    return path.size() >= 3;
}


static int
detria_result(int **faces, int *num_faces, point2d_t **out_pts,
	int *num_outpts, const std::vector<detria::PointD> &points,
	const std::vector<std::vector<int>> &outlines,
	const std::vector<std::vector<int>> &holes)
{
    if (points.size() < 3 || outlines.empty())
	return BRLCAD_ERROR;

    detria::Triangulation<detria::PointD, int> tri;
    tri.setPoints(points);
    for (const std::vector<int> &outline : outlines)
	tri.addOutline(outline);
    for (const std::vector<int> &hole : holes)
	tri.addHole(hole);

    bool success = false;
    try {
	success = tri.triangulate(true);
    } catch (...) {
	return BRLCAD_ERROR;
    }
    if (!success)
	return BRLCAD_ERROR;

    std::vector<int> triangles;
    tri.forEachTriangle([&](const detria::Triangle<int> triangle) {
	triangles.push_back(triangle.x);
	triangles.push_back(triangle.y);
	triangles.push_back(triangle.z);
    }, true);
    if (triangles.empty())
	return BRLCAD_ERROR;

    int *new_faces = (int *)bu_calloc(triangles.size(), sizeof(int),
	"sanitized detria faces");
    std::copy(triangles.begin(), triangles.end(), new_faces);
    point2d_t *new_points = (point2d_t *)bu_calloc(points.size(),
	sizeof(point2d_t), "sanitized detria points");
    for (size_t i = 0; i < points.size(); i++)
	V2SET(new_points[i], points[i].x, points[i].y);

    *faces = new_faces;
    *num_faces = (int)(triangles.size() / 3);
    *out_pts = new_points;
    *num_outpts = (int)points.size();
    return BRLCAD_OK;
}


static int
deduplicated_detria(int **faces, int *num_faces, point2d_t **out_pts,
	int *num_outpts, const int *poly, size_t poly_pnts,
	const int **holes_array, const size_t *holes_npts, size_t nholes,
	const int *steiner, size_t steiner_npts, const point2d_t *pts,
	double boundary_tol)
{
    std::vector<detria::PointD> points;
    std::map<std::pair<double, double>, int> point_indices;
    auto point_index = [&](int input_index) {
	const std::pair<double, double> key(pts[input_index][X],
	    pts[input_index][Y]);
	auto found = point_indices.find(key);
	if (found != point_indices.end())
	    return found->second;
	detria::PointD point;
	point.x = key.first;
	point.y = key.second;
	const int index = (int)points.size();
	points.push_back(point);
	point_indices[key] = index;
	return index;
    };

    auto ring = [&](const int *indices, size_t count,
	    std::vector<int> &output) {
	for (size_t i = 0; i < count; i++) {
	    const int index = point_index(indices[i]);
	    if (output.empty() || output.back() != index)
		output.push_back(index);
	}
	if (output.size() > 1 && output.front() == output.back())
	    output.pop_back();
	return output.size() >= 3;
    };

    std::vector<std::vector<int>> outlines(1);
    if (!ring(poly, poly_pnts, outlines[0]))
	return BRLCAD_ERROR;
    std::vector<std::vector<int>> holes;
    holes.reserve(nholes);
    for (size_t i = 0; i < nholes; i++) {
	std::vector<int> hole;
	if (ring(holes_array[i], holes_npts[i], hole))
	    holes.push_back(std::move(hole));
    }

    auto point_on_ring = [&](const detria::PointD &point,
	    const std::vector<int> &indices) {
	for (size_t i = 0; i < indices.size(); i++) {
	    const detria::PointD &a = points[indices[i]];
	    const detria::PointD &b = points[indices[(i + 1) %
		indices.size()]];
	    const long double dx = (long double)b.x - a.x;
	    const long double dy = (long double)b.y - a.y;
	    const long double px = (long double)point.x - a.x;
	    const long double py = (long double)point.y - a.y;
	    const long double length = std::sqrt(dx * dx + dy * dy);
	    if (!(length > 0.0L))
		continue;
	    if (std::fabs(px * dy - py * dx) > boundary_tol * length)
		continue;
	    const long double dot = px * dx + py * dy;
	    if (dot >= 0.0L && dot <= dx * dx + dy * dy)
		return true;
	}
	return false;
    };

    auto point_in_ring = [&](const detria::PointD &point,
	    const std::vector<int> &indices) {
	bool inside = false;
	for (size_t i = 0, j = indices.size() - 1; i < indices.size();
		j = i++) {
	    const detria::PointD &a = points[indices[i]];
	    const detria::PointD &b = points[indices[j]];
	    if (((a.y > point.y) != (b.y > point.y)) &&
		    point.x < (b.x - a.x) * (point.y - a.y) /
		    (b.y - a.y) + a.x)
		inside = !inside;
	}
	return inside;
    };

    for (size_t i = 0; i < steiner_npts; i++) {
	const std::pair<double, double> key(pts[steiner[i]][X],
	    pts[steiner[i]][Y]);
	if (point_indices.find(key) != point_indices.end())
	    continue;
	detria::PointD point;
	point.x = key.first;
	point.y = key.second;
	bool on_boundary = point_on_ring(point, outlines[0]);
	for (size_t h = 0; !on_boundary && h < holes.size(); h++)
	    on_boundary = point_on_ring(point, holes[h]);
	if (on_boundary || !point_in_ring(point, outlines[0]))
	    continue;
	bool in_hole = false;
	for (size_t h = 0; !in_hole && h < holes.size(); h++)
	    in_hole = point_in_ring(point, holes[h]);
	if (!in_hole)
	    (void)point_index(steiner[i]);
    }

    return detria_result(faces, num_faces, out_pts, num_outpts, points,
	outlines, holes);
}


/* Sanitize polygon constraints using the clip2tri approach before handing
 * them to detria.  Clipper works in a scaled integer space, where it can
 * collapse duplicates, split intersections, merge overlapping holes, and
 * return an explicit outer/hole hierarchy. */
int
bg_nested_poly_triangulate_clean(int **faces, int *num_faces,
	point2d_t **out_pts,
	int *num_outpts, const int *poly, const size_t poly_pnts,
	const int **holes_array, const size_t *holes_npts,
	const size_t nholes, const int *steiner, const size_t steiner_npts,
	const point2d_t *pts, const size_t npts)
{
    if (!faces || !num_faces || !out_pts || !num_outpts || !poly ||
	    poly_pnts < 3 || !pts || npts < 3)
	return BRLCAD_ERROR;
    if (nholes && (!holes_npts || !holes_array))
	return BRLCAD_ERROR;
    if (steiner_npts && !steiner)
	return BRLCAD_ERROR;

    *faces = NULL;
    *num_faces = 0;
    *out_pts = NULL;
    *num_outpts = 0;

    std::set<int> active;
    for (size_t i = 0; i < poly_pnts; i++)
	active.insert(poly[i]);
    for (size_t h = 0; h < nholes; h++) {
	for (size_t i = 0; i < holes_npts[h]; i++)
	    active.insert(holes_array[h][i]);
    }
    for (size_t i = 0; i < steiner_npts; i++)
	active.insert(steiner[i]);

    double min_x = INFINITY;
    double min_y = INFINITY;
    double max_x = -INFINITY;
    double max_y = -INFINITY;
    for (int index : active) {
	if (index < 0 || (size_t)index >= npts ||
		!std::isfinite(pts[index][X]) ||
		!std::isfinite(pts[index][Y]))
	    return BRLCAD_ERROR;
	min_x = std::min(min_x, pts[index][X]);
	min_y = std::min(min_y, pts[index][Y]);
	max_x = std::max(max_x, pts[index][X]);
	max_y = std::max(max_y, pts[index][Y]);
    }

    const double span = std::max(max_x - min_x, max_y - min_y);
    if (!(span > SMALL_FASTF) || !std::isfinite(span))
	return BRLCAD_ERROR;
    const double origin_x = 0.5 * (min_x + max_x);
    const double origin_y = 0.5 * (min_y + max_y);
    const double scale = (double)CLIPPER_MAX / span;
    if (!(scale > 0.0) || !std::isfinite(scale))
	return BRLCAD_ERROR;

    /* Most failures need no topological change: detria rejects duplicate
     * coordinates and unconstrained samples lying on constrained edges.  Try
     * removing only those points first, preserving all original loop points. */
    const double boundary_tol = 2.0 / scale;
    if (deduplicated_detria(faces, num_faces, out_pts, num_outpts,
	    poly, poly_pnts, holes_array, holes_npts, nholes, steiner,
	    steiner_npts, pts, boundary_tol) == BRLCAD_OK)
	return BRLCAD_OK;

    ClipperLib::Clipper clipper(ClipperLib::ioStrictlySimple |
	ClipperLib::ioPreserveCollinear);
    ClipperLib::Path outer;
    if (!clipper_input_path(outer, poly, poly_pnts, pts, origin_x,
	    origin_y, scale))
	return BRLCAD_ERROR;
    ClipperLib::Paths hole_paths;

    try {
	if (!clipper.AddPath(outer, ClipperLib::ptSubject, true))
	    return BRLCAD_ERROR;
	for (size_t h = 0; h < nholes; h++) {
	    ClipperLib::Path hole;
	    if (!clipper_input_path(hole, holes_array[h], holes_npts[h],
		    pts, origin_x, origin_y, scale))
		continue;
	    hole_paths.push_back(hole);
	    if (!clipper.AddPath(hole, ClipperLib::ptClip, true))
		return BRLCAD_ERROR;
	}
    } catch (...) {
	return BRLCAD_ERROR;
    }

    ClipperLib::PolyTree clipped;
    try {
	if (!clipper.Execute(ClipperLib::ctDifference, clipped,
		ClipperLib::pftNonZero, ClipperLib::pftNonZero))
	    return BRLCAD_ERROR;
    } catch (...) {
	return BRLCAD_ERROR;
    }
    if (!clipped.Total()) {
	/* Some imported B-Reps carry geometrically inverted outer/hole loop
	 * labels.  Reconstruct containment from contour parity when the stated
	 * difference is empty, preserving an annulus rather than rejecting it. */
	clipped.Clear();
	ClipperLib::Clipper parity(ClipperLib::ioStrictlySimple |
	    ClipperLib::ioPreserveCollinear);
	try {
	    if (!parity.AddPath(outer, ClipperLib::ptSubject, true))
		return BRLCAD_ERROR;
	    for (const ClipperLib::Path &hole : hole_paths) {
		if (!parity.AddPath(hole, ClipperLib::ptSubject, true))
		    return BRLCAD_ERROR;
	    }
	    if (!parity.Execute(ClipperLib::ctUnion, clipped,
		    ClipperLib::pftEvenOdd, ClipperLib::pftEvenOdd))
		return BRLCAD_ERROR;
	} catch (...) {
	    return BRLCAD_ERROR;
	}
	if (!clipped.Total())
	    return BRLCAD_ERROR;
    }

    std::vector<detria::PointD> tri_points;
    std::map<std::pair<ClipperLib::cInt, ClipperLib::cInt>, int>
	point_indices;
    std::vector<std::vector<int>> outlines;
    std::vector<std::vector<int>> holes;
    std::vector<ClipperLib::Path> boundary_paths;
    outlines.reserve((size_t)clipped.Total());
    holes.reserve((size_t)clipped.Total());
    boundary_paths.reserve((size_t)clipped.Total());

    auto point_index = [&](const ClipperLib::IntPoint &p) {
	const std::pair<ClipperLib::cInt, ClipperLib::cInt> key(p.X, p.Y);
	auto found = point_indices.find(key);
	if (found != point_indices.end())
	    return found->second;
	detria::PointD np;
	np.x = (double)p.X / scale + origin_x;
	np.y = (double)p.Y / scale + origin_y;
	const int index = (int)tri_points.size();
	tri_points.push_back(np);
	point_indices[key] = index;
	return index;
    };

    for (ClipperLib::PolyNode *node = clipped.GetFirst(); node;
	    node = node->GetNext()) {
	if (node->Contour.size() < 3)
	    continue;
	std::vector<int> contour;
	contour.reserve(node->Contour.size());
	for (const ClipperLib::IntPoint &p : node->Contour) {
	    const int index = point_index(p);
	    if (contour.empty() || contour.back() != index)
		contour.push_back(index);
	}
	if (contour.size() > 1 && contour.front() == contour.back())
	    contour.pop_back();
	if (contour.size() < 3)
	    continue;
	if (node->IsHole())
	    holes.push_back(std::move(contour));
	else
	    outlines.push_back(std::move(contour));
	boundary_paths.push_back(node->Contour);
    }
    if (outlines.empty())
	return BRLCAD_ERROR;

    for (size_t i = 0; i < steiner_npts; i++) {
	const int input_index = steiner[i];
	if (input_index < 0 || !std::isfinite(pts[input_index][X]) ||
		!std::isfinite(pts[input_index][Y]))
	    continue;
	ClipperLib::IntPoint p(
	    (ClipperLib::cInt)std::llround(
		(pts[input_index][X] - origin_x) * scale),
	    (ClipperLib::cInt)std::llround(
		(pts[input_index][Y] - origin_y) * scale));
	const std::pair<ClipperLib::cInt, ClipperLib::cInt> key(p.X, p.Y);
	if (point_indices.find(key) != point_indices.end())
	    continue;

	bool on_boundary = false;
	for (const ClipperLib::Path &path : boundary_paths) {
	    for (size_t j = 0; j < path.size(); j++) {
		if (clipper_point_on_segment(p, path[j],
			path[(j + 1) % path.size()])) {
		    on_boundary = true;
		    break;
		}
	    }
	    if (on_boundary)
		break;
	}
	if (!on_boundary) {
	    bool inside = false;
	    int deepest = -1;
	    for (ClipperLib::PolyNode *node = clipped.GetFirst(); node;
		    node = node->GetNext()) {
		if (node->Contour.size() < 3 ||
			ClipperLib::PointInPolygon(p, node->Contour) == 0)
		    continue;
		int depth = 0;
		for (ClipperLib::PolyNode *parent = node->Parent; parent;
			parent = parent->Parent)
		    depth++;
		if (depth > deepest) {
		    deepest = depth;
		    inside = !node->IsHole();
		}
	    }
	    if (inside)
		(void)point_index(p);
	}
    }

    return detria_result(faces, num_faces, out_pts, num_outpts, tri_points,
	outlines, holes);
}


int
bg_poly2tri_test(int **faces, int *num_faces, point2d_t **out_pts,
	int *num_outpts, const int *poly, const size_t poly_pnts,
	const int **holes_array, const size_t *holes_npts,
	const size_t nholes, const int *steiner, const size_t steiner_npts,
	const point2d_t *pts)
{
    if (!poly || !pts || (nholes && (!holes_array || !holes_npts)) ||
	    (steiner_npts && !steiner))
	return BRLCAD_ERROR;
    size_t npts = 0;
    for (size_t i = 0; i < poly_pnts; i++) {
	if (poly[i] < 0)
	    return BRLCAD_ERROR;
	npts = std::max(npts, (size_t)poly[i] + 1);
    }
    for (size_t h = 0; h < nholes; h++) {
	for (size_t i = 0; i < holes_npts[h]; i++) {
	    if (holes_array[h][i] < 0)
		return BRLCAD_ERROR;
	    npts = std::max(npts, (size_t)holes_array[h][i] + 1);
	}
    }
    for (size_t i = 0; i < steiner_npts; i++) {
	if (steiner[i] < 0)
	    return BRLCAD_ERROR;
	npts = std::max(npts, (size_t)steiner[i] + 1);
    }
    return bg_nested_poly_triangulate_clean(faces, num_faces, out_pts,
	num_outpts, poly, poly_pnts, holes_array, holes_npts, nholes,
	steiner, steiner_npts, pts, npts);
}

int
bg_detria(int **faces, int *num_faces, point2d_t **out_pts, int *num_outpts,
	const int *poly, const size_t poly_pnts,
	const int **holes_array, const size_t *holes_npts, const size_t nholes,
	const int *steiner, const size_t steiner_npts,
	const point2d_t *pts)
{
    std::unordered_map<int, int> det2pts, pts2det;
    std::set<int> active_pts;

    // Find active points
    for (size_t i = 0; i < poly_pnts; i++)
	active_pts.insert(poly[i]);
    for (size_t i = 0; i < nholes; i++) {
	for (size_t j = 0; j < holes_npts[i]; j++)
	    active_pts.insert(holes_array[i][j]);
    }
    // Note - for detria, all points added that are not part of a polygon are
    // steiner points - so the only thing we need to do with them is flag them
    // as active in the set.
    for (size_t i = 0; i < steiner_npts; i++)
	active_pts.insert(steiner[i]);

    // Set up a points array holding the active points
    std::vector<detria::PointD> tpnts;
    std::set<int>::iterator a_it;
    for (a_it = active_pts.begin(); a_it != active_pts.end(); a_it++) {
	detria::PointD npt;
	npt.x = pts[*a_it][X];
	npt.y = pts[*a_it][Y];
	tpnts.push_back(npt);
	int detind = (int)tpnts.size() - 1;
	det2pts[detind] = *a_it;
	pts2det[*a_it] = detind;
    }

    // Let the triangulation structure know about the points
    detria::Triangulation<detria::PointD, int> tri;
    tri.setPoints(tpnts);

    // Outer polygon is defined first
    std::vector<int> outer_polyline;
    for (size_t i = 0; i < poly_pnts; i++)
	outer_polyline.push_back(pts2det[poly[i]]);
    tri.addOutline(outer_polyline);

    // Next are the holes.
    // IMPORTANT: detria's addHole() stores a ReadonlySpan (raw pointer) into the
    // provided vector.  The vector must outlive the tri.triangulate() call.
    // Pre-populate inner_holes before registering any hole so that no
    // reallocation invalidates previously stored data() pointers.
    std::vector<std::vector<int>> inner_holes(nholes);
    for (size_t i = 0; i < nholes; i++) {
	std::vector<int> &hv = inner_holes[i];
	hv.reserve(holes_npts[i]);
	for (size_t j = 0; j < holes_npts[i]; j++)
	    hv.push_back(pts2det[holes_array[i][j]]);
    }
    for (size_t i = 0; i < nholes; i++) {
	tri.addHole(inner_holes[i]);
    }

    // Run the core triangulation routine
    bool tri_success = false;
    {
	try {
	    tri_success = tri.triangulate(true);
	}
	catch (...) {
	    return 1;
	}
    }

    // Did we succeed?
    if (!tri_success) {
	bu_log("bg_detria: triangulation failed: %s\n", tri.getErrorMessage().c_str());
	return 1;
    }

    // Should the result triangles be in CW order?
    bool cwTriangles = true;

    // Count the number of interior triangles so we can allocate an output array
    int tri_cnt = 0;
    tri.forEachTriangle([&](const detria::Triangle<int> &){tri_cnt++;}, cwTriangles);

    (*num_faces) = tri_cnt;
    int *nfaces = (int *)bu_calloc(*num_faces * 3, sizeof(int), "faces array");

    // We may want to report the number of unique active points, so
    // track this as we iterate the triangles
    active_pts.clear();

    int tri_ind = 0;
    tri.forEachTriangle([&](const detria::Triangle<int> triangle)
	    {
	    // `triangle` contains the point indices
	    nfaces[3*tri_ind] = det2pts[triangle.x];
	    nfaces[3*tri_ind+1] = det2pts[triangle.y];
	    nfaces[3*tri_ind+2] = det2pts[triangle.z];
	    active_pts.insert(nfaces[3*tri_ind]);
	    active_pts.insert(nfaces[3*tri_ind+1]);
	    active_pts.insert(nfaces[3*tri_ind+2]);
	    tri_ind++;
	    }, cwTriangles);

    (*faces) = nfaces;

    // We're not generating a new points array, so if out_pts exists set it to the
    // input points array
    if (out_pts) {
	(*out_pts) = (point2d_t *)pts;
    }
    if (num_outpts) {
	(*num_outpts) = (int)active_pts.size();
    }

    return 0;
}

extern "C" int
bg_nested_poly_triangulate(int **faces, int *num_faces, point2d_t **out_pts, int *num_outpts,
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
	int detria_ret = bg_detria(faces, num_faces, out_pts, num_outpts, poly, poly_pnts, holes_array, holes_npts, nholes, steiner, steiner_npts, pts);
	return detria_ret;
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

	// earcut.hpp's concept of steiner points isn't the same as detria's,
	// so we need to pass if steiner points have been supplied.
	if (steiner_npts) return 1;

	/* -------- Orientation Enforcement (outer CCW, holes CW) -------- */
	auto strip_dup_and_orient = [&](const int *idx, size_t cnt,
		bool want_ccw,
		std::vector<int> &out_indices)->bool
	{
	    if (cnt < 3) return false;
	    out_indices.assign(idx, idx + cnt);

	    // Remove duplicate closing point if present (index list refers to points;
	    // duplicates more meaningfully checked via coordinates).
	    // If first and last coordinates coincide, drop last index.
	    if (cnt >= 2) {
		const point2d_t &p0 = pts[out_indices.front()];
		const point2d_t &pl = pts[out_indices.back()];
		if (NEAR_EQUAL(p0[X], pl[X], SMALL_FASTF) && NEAR_EQUAL(p0[Y], pl[Y], SMALL_FASTF)) {
		    out_indices.pop_back();
		    if (out_indices.size() < 3) return false;
		}
	    }

	    // Determine winding
	    int dir = bg_polygon_direction(out_indices.size(), pts, out_indices.data());
	    if (dir == 0) {
		/* Near-degenerate; leave as-is (earcut may ignore) */
		return out_indices.size() >= 3;
	    }

	    bool is_ccw = (dir == BG_CCW);
	    if (want_ccw && !is_ccw) {
		std::reverse(out_indices.begin(), out_indices.end());
	    } else if (!want_ccw && is_ccw) {
		std::reverse(out_indices.begin(), out_indices.end());
	    }
	    return true;
	};

	std::vector<int> outer_oriented;
	if (!strip_dup_and_orient(poly, poly_pnts, true, outer_oriented)) {
	    return 1; /* invalid outer ring */
	}

	std::vector<std::vector<int>> hole_oriented;
	hole_oriented.resize(nholes);
	for (size_t h = 0; h < nholes; h++) {
	    if (!strip_dup_and_orient(holes_array[h], holes_npts[h], false, hole_oriented[h])) {
		/* Skip degenerate hole (size <3 or invalid). We silently
		 * drop it rather than failing the whole triangulation. */
		hole_oriented[h].clear();
	    }
	}

	/* Set up for ear clipping */
	using Coord = fastf_t;
	using N = uint32_t;
	using Point = std::array<Coord, 2>;
	std::vector<std::vector<Point>> polygon;

	/* map from flattened earcut vertex index -> original point index.
	 * We originally added this when looking into steiner point support
	 * with earcut.hpp - that didn't pan out, but we'll leave this
	 * in place in case it ends up being of interest down the road. */
	std::vector<int> index_map;
	index_map.reserve(outer_oriented.size()
		+ [&](){
		size_t hc = 0;
		for (size_t hi = 0; hi < nholes; hi++)
		hc += hole_oriented[hi].size();
		return hc;
		}());

	// Outer ring (already CCW)
	std::vector<Point> outer_polygon;
	outer_polygon.reserve(outer_oriented.size());
	for (size_t i = 0; i < outer_oriented.size(); i++) {
	    int ind = outer_oriented[i];
	    Point np;
	    np[0] = pts[ind][X];
	    np[1] = pts[ind][Y];
	    outer_polygon.push_back(np);
	    index_map.push_back(ind);
	}
	polygon.push_back(std::move(outer_polygon));

	// Holes (each oriented CW)
	for (size_t h = 0; h < nholes; h++) {
	    if (hole_oriented[h].size() < 3) continue; // dropped/degenerate
	    std::vector<Point> hole_polygon;
	    hole_polygon.reserve(hole_oriented[h].size());
	    for (size_t j = 0; j < hole_oriented[h].size(); j++) {
		int ind = hole_oriented[h][j];
		Point np;
		np[0] = pts[ind][X];
		np[1] = pts[ind][Y];
		hole_polygon.push_back(np);
		index_map.push_back(ind);
	    }
	    polygon.push_back(std::move(hole_polygon));
	}

	std::vector<N> indices = mapbox::earcut<N>(polygon);
	if (indices.size() < 3) {
	    return 1;
	}

	(*num_faces) = (int)(indices.size()/3);
	(*faces) = (int *)bu_calloc(indices.size(), sizeof(int), "faces");

	/* translate earcut’s local indices back to original point indices */
	for (size_t i = 0; i < indices.size()/3; i++) {
	    (*faces)[3*i]     = index_map[indices[3*i]];
	    (*faces)[3*i + 1] = index_map[indices[3*i + 1]];
	    (*faces)[3*i + 2] = index_map[indices[3*i + 2]];
	}

	return 0;
    }

    /* Unimplemented type specified */
    return -1;
}

extern "C" int
bg_poly_triangulate(int **faces, int *num_faces, point2d_t **out_pts, int *num_outpts,
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

    ret = bg_nested_poly_triangulate(faces, num_faces, out_pts, num_outpts, verts_ind, npts, NULL, NULL, 0, steiner, steiner_pnts, pts, npts, type);

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

extern "C" int
bg_polygon_triangulate(int **faces, int *num_faces, point_t **out_pts, int *num_outpts,
	struct bg_polygon *p, triangulation_t type)
{
    if (!faces || !num_faces || !out_pts || !num_outpts || !p)
	return -1;

    // Fit the outer contour to get a 2D plane (bg_polygon is in principle a 3D data structure)
    point_t pcenter;
    vect_t  pnorm;
    plane_t pl;
    if (bg_fit_plane(&pcenter, &pnorm, p->contour[0].num_points, p->contour[0].point)) {
	return -1;
    }
    bg_plane_pt_nrml(&pl, pcenter, pnorm);

    // Count all points
    int pnt_cnt = 0;
    for (size_t i = 0; i < p->num_contours; ++i) {
	pnt_cnt += p->contour[i].num_points;
    }

    // Translate the bg_polygon into bg_nested_poly_triangulate inputs
    point2d_t *pnts_2d = (point2d_t *)bu_calloc(pnt_cnt, sizeof(point2d_t), "projected points");
    int *ocontour = NULL;
    int ocontour_cnt = 0;
    int **holes_array = (int **)bu_calloc(p->num_contours - 1, sizeof(int *), "holes");
    size_t *holes_npts = (size_t *)bu_calloc(p->num_contours - 1, sizeof(size_t), "holes_cnt");
    int curr_pnt = 0;
    for (size_t i = 0; i < p->num_contours; ++i) {
	int *cpnts = (int *)bu_calloc(p->contour[i].num_points, sizeof(int), "point indices");
	if (i > 0) {
	    holes_array[i-1] = cpnts;
	    holes_npts[i-1] = p->contour[i].num_points;
	} else {
	    ocontour = cpnts;
	    ocontour_cnt = p->contour[i].num_points;
	}
	for (size_t j = 0; j < p->contour[i].num_points; ++j) {
	    vect2d_t p2d;
	    bg_plane_closest_pt(&p2d[0], &p2d[1], &pl, &p->contour[i].point[j]);
	    V2MOVE(pnts_2d[curr_pnt], p2d);
	    cpnts[j] = curr_pnt;
	    curr_pnt++;
	}
    }

    int *tri_faces = NULL;
    int tri_num_faces = 0;
    point2d_t *tri_out_pts = NULL;
    int tri_num_outpts = 0;
    int ret = bg_nested_poly_triangulate(&tri_faces, &tri_num_faces, &tri_out_pts, &tri_num_outpts, ocontour, ocontour_cnt, (const int **)holes_array, (const size_t *)holes_npts, p->num_contours - 1, NULL, 0, pnts_2d, pnt_cnt, type);


    // Translate 2D plane points into 3D points
    point_t *pnts_3d = (point_t *)bu_calloc(pnt_cnt, sizeof(point_t), "3D points");
    for (int i = 0; i < pnt_cnt; i++) {
	bg_plane_pt_at(&pnts_3d[i], &pl, pnts_2d[i][0], pnts_2d[i][1]);
    }

    // Assign outputs
    *faces = tri_faces;
    *num_faces = tri_num_faces;
    *out_pts = pnts_3d;
    *num_outpts = tri_num_outpts;

    // Clean up 2D and translation arrays
    bu_free(ocontour, "free ocontour");
    for (size_t i = 0; i < p->num_contours - 1; i++) {
	bu_free(holes_array[i], "free holes array");
    }
    bu_free(holes_npts, "hole cnts");
    bu_free(pnts_2d, "2d pnts");

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
