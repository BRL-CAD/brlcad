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
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>
#include <map>
#include <set>
#include <vector>

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
#include "bu/str.h"
#include "bv/plot3.h"
#include "bg/polygon.h"
#include "bg/plane.h"
#include "RTree.h"

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

static bool
detria_condition_points(const std::vector<detria::PointD> &points,
	std::vector<detria::PointD> &conditioned)
{
    if (points.size() < 3)
	return false;
    double min_x = std::numeric_limits<double>::infinity();
    double min_y = std::numeric_limits<double>::infinity();
    double max_x = -std::numeric_limits<double>::infinity();
    double max_y = -std::numeric_limits<double>::infinity();
    for (const detria::PointD &point : points) {
	if (!std::isfinite(point.x) || !std::isfinite(point.y))
	    return false;
	min_x = std::min(min_x, point.x);
	min_y = std::min(min_y, point.y);
	max_x = std::max(max_x, point.x);
	max_y = std::max(max_y, point.y);
    }
    const long double center_x = ((long double)min_x + max_x) * 0.5L;
    const long double center_y = ((long double)min_y + max_y) * 0.5L;
    const long double span = std::max((long double)max_x - min_x,
	(long double)max_y - min_y);
    if (!(span > 0.0L) || !std::isfinite(span))
	return false;
    int exponent = 0;
    (void)std::frexp(span, &exponent);
    const long double scale = std::ldexp(1.0L, -exponent);
    conditioned.resize(points.size());
    for (size_t i = 0; i < points.size(); ++i) {
	conditioned[i].x = (double)(((long double)points[i].x - center_x) *
	    scale);
	conditioned[i].y = (double)(((long double)points[i].y - center_y) *
	    scale);
    }
    return true;
}

static bool
detria_postconditions(const std::vector<detria::PointD> &points,
	const std::vector<std::vector<int>> &outlines,
	const std::vector<std::vector<int>> &holes,
	const std::vector<std::pair<int, int>> &interior_constraints,
	const std::vector<int> &triangles)
{
    if (triangles.empty() || triangles.size() % 3)
	return false;
    typedef std::pair<int, int> constraint_edge;
    auto edge_key = [](int a, int b) {
	return (a < b) ? constraint_edge(a, b) : constraint_edge(b, a);
    };
    std::set<constraint_edge> boundary_edges;
    auto add_ring_edges = [&](const std::vector<int> &ring) {
	if (ring.size() < 3)
	    return false;
	for (size_t i = 0; i < ring.size(); ++i) {
	    const int a = ring[i];
	    const int b = ring[(i + 1) % ring.size()];
	    if (a < 0 || b < 0 || (size_t)a >= points.size() ||
		    (size_t)b >= points.size() || a == b ||
		    !boundary_edges.insert(edge_key(a, b)).second)
		return false;
	}
	return true;
    };
    for (const std::vector<int> &outline : outlines) {
	if (!add_ring_edges(outline))
	    return false;
    }
    for (const std::vector<int> &hole : holes) {
	if (!add_ring_edges(hole))
	    return false;
    }
    std::set<constraint_edge> internal_edges;
    for (const constraint_edge &edge : interior_constraints) {
	if (edge.first < 0 || edge.second < 0 ||
		(size_t)edge.first >= points.size() ||
		(size_t)edge.second >= points.size() ||
		edge.first == edge.second ||
		boundary_edges.find(edge_key(edge.first, edge.second)) !=
		    boundary_edges.end() ||
		!internal_edges.insert(edge_key(edge.first, edge.second)).second)
	    return false;
    }

    std::map<constraint_edge, int> edge_uses;
    long double triangle_area2 = 0.0L;
    for (size_t i = 0; i < triangles.size(); i += 3) {
	const int a = triangles[i];
	const int b = triangles[i + 1];
	const int c = triangles[i + 2];
	if (a < 0 || b < 0 || c < 0 || (size_t)a >= points.size() ||
		(size_t)b >= points.size() || (size_t)c >= points.size() ||
		a == b || b == c || c == a)
	    return false;
	const long double abx = (long double)points[b].x - points[a].x;
	const long double aby = (long double)points[b].y - points[a].y;
	const long double acx = (long double)points[c].x - points[a].x;
	const long double acy = (long double)points[c].y - points[a].y;
	const long double cross = abx * acy - aby * acx;
	if (!(cross < 0.0L))
	    return false;
	triangle_area2 -= cross;
	edge_uses[edge_key(a, b)]++;
	edge_uses[edge_key(b, c)]++;
	edge_uses[edge_key(c, a)]++;
    }
    for (const auto &edge : edge_uses) {
	if (edge.second < 1 || edge.second > 2)
	    return false;
	const bool boundary = boundary_edges.find(edge.first) !=
	    boundary_edges.end();
	if ((boundary && edge.second != 1) ||
		(!boundary && edge.second != 2))
	    return false;
    }
    for (const constraint_edge &edge : boundary_edges) {
	if (edge_uses[edge] != 1)
	    return false;
    }
    for (const constraint_edge &edge : internal_edges) {
	if (edge_uses[edge] != 2)
	    return false;
    }

    auto ring_area2 = [&](const std::vector<int> &ring) {
	long double area = 0.0L;
	for (size_t i = 0; i < ring.size(); ++i) {
	    const detria::PointD &a = points[ring[i]];
	    const detria::PointD &b = points[ring[(i + 1) % ring.size()]];
	    area += (long double)a.x * b.y - (long double)a.y * b.x;
	}
	return std::fabs(area);
    };
    long double expected_area2 = 0.0L;
    for (const std::vector<int> &outline : outlines)
	expected_area2 += ring_area2(outline);
    for (const std::vector<int> &hole : holes)
	expected_area2 -= ring_area2(hole);
    const long double area_scale = std::max(1.0L,
	std::fabs(expected_area2));
    const long double area_tolerance = 1024.0L *
	std::numeric_limits<double>::epsilon() * area_scale;
    return expected_area2 > 0.0L &&
	std::fabs(triangle_area2 - expected_area2) <= area_tolerance;
}


static int
detria_result(int **faces, int *num_faces, point2d_t **out_pts,
	int *num_outpts, const std::vector<detria::PointD> &points,
	const std::vector<std::vector<int>> &outlines,
	const std::vector<std::vector<int>> &holes,
	const std::vector<std::pair<int, int>> &interior_constraints = {})
{
    if (points.size() < 3 || outlines.empty())
	return BRLCAD_ERROR;

    std::vector<detria::PointD> conditioned;
    if (!detria_condition_points(points, conditioned))
	return BRLCAD_ERROR;

    detria::Triangulation<detria::PointD, int> tri;
    tri.setPoints(conditioned);
    for (const std::vector<int> &outline : outlines)
	tri.addOutline(outline);
    for (const std::vector<int> &hole : holes)
	tri.addHole(hole);
    for (const std::pair<int, int> &constraint : interior_constraints)
	tri.setConstrainedEdge(constraint.first, constraint.second);

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

    if (!detria_postconditions(conditioned, outlines, holes,
	    interior_constraints, triangles))
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
bg_nested_poly_triangulate_clean_constraints(int **faces, int *num_faces,
	point2d_t **out_pts,
	int *num_outpts, const int *poly, const size_t poly_pnts,
	const int **holes_array, const size_t *holes_npts,
	const size_t nholes, const int *steiner, const size_t steiner_npts,
	const int *constraints, const size_t constraint_cnt,
	const point2d_t *pts, const size_t npts)
{
    if (!faces || !num_faces || !out_pts || !num_outpts || !poly ||
	    poly_pnts < 3 || !pts || npts < 3)
	return BRLCAD_ERROR;
    if (nholes && (!holes_npts || !holes_array))
	return BRLCAD_ERROR;
    if (steiner_npts && !steiner)
	return BRLCAD_ERROR;
    if (constraint_cnt && !constraints)
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
    for (size_t i = 0; i < constraint_cnt; ++i) {
	active.insert(constraints[2 * i]);
	active.insert(constraints[2 * i + 1]);
    }

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

    if (!constraint_cnt && deduplicated_detria(faces, num_faces, out_pts, num_outpts,
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
    std::vector<ClipperLib::IntPoint> tri_integer_points;
    std::map<std::pair<ClipperLib::cInt, ClipperLib::cInt>, int>
	point_indices;
    std::map<const ClipperLib::PolyNode *, std::vector<int>> node_contours;
    std::vector<ClipperLib::Path> boundary_paths;
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
	tri_integer_points.push_back(p);
	point_indices[key] = index;
	return index;
    };

    /* Clipper correctly cancels retraced and overlapping contour segments,
     * but it may omit collinear vertices from the surviving segment.  Those
     * vertices are shared B-Rep boundary samples in downstream callers.
     * Index the original boundary points and reinsert every one which lies
     * on a cleaned contour edge, in edge order. */
    std::vector<ClipperLib::IntPoint> input_boundary_points;
    std::set<std::pair<ClipperLib::cInt, ClipperLib::cInt>>
	input_boundary_keys;
    const auto add_input_boundary_point = [&](int input_index) {
	ClipperLib::IntPoint point(
	    (ClipperLib::cInt)std::llround(
		(pts[input_index][X] - origin_x) * scale),
	    (ClipperLib::cInt)std::llround(
		(pts[input_index][Y] - origin_y) * scale));
	const std::pair<ClipperLib::cInt, ClipperLib::cInt> key(
	    point.X, point.Y);
	if (input_boundary_keys.insert(key).second)
	    input_boundary_points.push_back(point);
    };
    for (size_t i = 0; i < poly_pnts; ++i)
	add_input_boundary_point(poly[i]);
    for (size_t hole = 0; hole < nholes; ++hole) {
	for (size_t i = 0; i < holes_npts[hole]; ++i)
	    add_input_boundary_point(holes_array[hole][i]);
    }
    RTree<size_t, double, 2> input_boundary_index;
    for (size_t i = 0; i < input_boundary_points.size(); ++i) {
	const double location[2] = {
	    (double)input_boundary_points[i].X,
	    (double)input_boundary_points[i].Y
	};
	input_boundary_index.Insert(location, location, i);
    }
    const auto collect_boundary_point = [](size_t point, void *context) {
	std::vector<size_t> *points = (std::vector<size_t> *)context;
	points->push_back(point);
	return true;
    };
    const auto append_clean_edge = [&](std::vector<int> &contour,
	    const ClipperLib::IntPoint &first,
	    const ClipperLib::IntPoint &second) {
	const int first_index = point_index(first);
	if (contour.empty() || contour.back() != first_index)
	    contour.push_back(first_index);
	const double minimum[2] = {
	    (double)std::min(first.X, second.X),
	    (double)std::min(first.Y, second.Y)
	};
	const double maximum[2] = {
	    (double)std::max(first.X, second.X),
	    (double)std::max(first.Y, second.Y)
	};
	std::vector<size_t> candidates;
	input_boundary_index.Search(minimum, maximum,
	    collect_boundary_point, &candidates);
	struct ordered_point {
	    long double parameter;
	    ClipperLib::IntPoint point;
	};
	std::vector<ordered_point> ordered;
	const long double dx = (long double)second.X - first.X;
	const long double dy = (long double)second.Y - first.Y;
	for (size_t candidate : candidates) {
	    if (candidate >= input_boundary_points.size())
		continue;
	    const ClipperLib::IntPoint &point =
		input_boundary_points[candidate];
	    if (!clipper_point_on_segment(point, first, second) ||
		    clipper_same_point(point, first) ||
		    clipper_same_point(point, second))
		continue;
	    const long double parameter =
		((long double)point.X - first.X) * dx +
		((long double)point.Y - first.Y) * dy;
	    ordered.push_back({parameter, point});
	}
	std::sort(ordered.begin(), ordered.end(),
	    [](const ordered_point &a, const ordered_point &b) {
		if (a.parameter < b.parameter)
		    return true;
		if (b.parameter < a.parameter)
		    return false;
		if (a.point.X != b.point.X)
		    return a.point.X < b.point.X;
		return a.point.Y < b.point.Y;
	    });
	for (const ordered_point &point : ordered) {
	    const int index = point_index(point.point);
	    if (contour.empty() || contour.back() != index)
		contour.push_back(index);
	}
    };

    for (ClipperLib::PolyNode *node = clipped.GetFirst(); node;
	    node = node->GetNext()) {
	if (node->Contour.size() < 3)
	    continue;
	std::vector<int> contour;
	contour.reserve(node->Contour.size());
	for (size_t i = 0; i < node->Contour.size(); ++i)
	    append_clean_edge(contour, node->Contour[i],
		node->Contour[(i + 1) % node->Contour.size()]);
	if (contour.size() > 1 && contour.front() == contour.back())
	    contour.pop_back();
	std::set<int> seen_contour_points;
	std::vector<int> simple_contour;
	simple_contour.reserve(contour.size());
	for (int point : contour) {
	    if (seen_contour_points.insert(point).second)
		simple_contour.push_back(point);
	}
	contour.swap(simple_contour);
	if (contour.size() < 3)
	    continue;
	node_contours[node] = std::move(contour);
	boundary_paths.push_back(node->Contour);
    }

    bool have_outline = false;
    for (const auto &entry : node_contours) {
	if (!entry.first->IsHole()) {
	    have_outline = true;
	    break;
	}
    }
    if (!have_outline)
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

    /* Retain only constraint endpoints which survived contour cleanup.  An
     * endpoint on a canceled retrace is intentionally absent.  Component
     * membership and boundary crossings are checked again below before a
     * surviving edge is given to detria. */
    std::vector<std::pair<int, int>> cleaned_constraints;
    std::set<std::pair<int, int>> unique_cleaned_constraints;
    for (size_t i = 0; i < constraint_cnt; ++i) {
	const int first_input = constraints[2 * i];
	const int second_input = constraints[2 * i + 1];
	if (first_input < 0 || second_input < 0 ||
		(size_t)first_input >= npts ||
		(size_t)second_input >= npts)
	    return BRLCAD_ERROR;
	const std::pair<ClipperLib::cInt, ClipperLib::cInt> first_key(
	    (ClipperLib::cInt)std::llround(
		(pts[first_input][X] - origin_x) * scale),
	    (ClipperLib::cInt)std::llround(
		(pts[first_input][Y] - origin_y) * scale));
	const std::pair<ClipperLib::cInt, ClipperLib::cInt> second_key(
	    (ClipperLib::cInt)std::llround(
		(pts[second_input][X] - origin_x) * scale),
	    (ClipperLib::cInt)std::llround(
		(pts[second_input][Y] - origin_y) * scale));
	const auto first = point_indices.find(first_key);
	const auto second = point_indices.find(second_key);
	if (first == point_indices.end() || second == point_indices.end() ||
		first->second == second->second)
	    continue;
	const std::pair<int, int> edge = first->second < second->second ?
	    std::make_pair(first->second, second->second) :
	    std::make_pair(second->second, first->second);
	if (unique_cleaned_constraints.insert(edge).second)
	    cleaned_constraints.push_back(edge);
    }

    /* A strictly-simple Clipper result may contain several filled
     * components which touch at a vertex.  detria's multi-outline mode does
     * not represent that hierarchy reliably, so triangulate every filled
     * PolyNode with only its direct hole children.  Islands nested inside a
     * hole are filled nodes in their own right and are handled separately. */
    std::vector<int> combined_faces;
    for (ClipperLib::PolyNode *node = clipped.GetFirst(); node;
	    node = node->GetNext()) {
	if (node->IsHole())
	    continue;
	const auto outline_entry = node_contours.find(node);
	if (outline_entry == node_contours.end() ||
		outline_entry->second.size() < 3)
	    continue;

	std::vector<const ClipperLib::PolyNode *> hole_nodes;
	std::vector<std::vector<int>> component_holes;
	for (ClipperLib::PolyNode *child : node->Childs) {
	    const auto hole_entry = node_contours.find(child);
	    if (!child->IsHole() || hole_entry == node_contours.end() ||
		    hole_entry->second.size() < 3)
		continue;
	    hole_nodes.push_back(child);
	    component_holes.push_back(hole_entry->second);
	}

	std::vector<int> local_to_global;
	std::map<int, int> global_to_local;
	const auto add_component_point = [&](int global_index) {
	    const auto existing = global_to_local.find(global_index);
	    if (existing != global_to_local.end())
		return existing->second;
	    const int local_index = (int)local_to_global.size();
	    local_to_global.push_back(global_index);
	    global_to_local[global_index] = local_index;
	    return local_index;
	};
	std::vector<int> local_outline;
	local_outline.reserve(outline_entry->second.size());
	for (int point : outline_entry->second)
	    local_outline.push_back(add_component_point(point));
	std::vector<std::vector<int>> local_holes(component_holes.size());
	for (size_t hole = 0; hole < component_holes.size(); ++hole) {
	    local_holes[hole].reserve(component_holes[hole].size());
	    for (int point : component_holes[hole])
		local_holes[hole].push_back(add_component_point(point));
	}

	for (size_t point = 0; point < tri_integer_points.size(); ++point) {
	    if (global_to_local.find((int)point) != global_to_local.end())
		continue;
	    if (ClipperLib::PointInPolygon(tri_integer_points[point],
		    node->Contour) != 1)
		continue;
	    bool inside_hole = false;
	    for (const ClipperLib::PolyNode *hole : hole_nodes) {
		if (ClipperLib::PointInPolygon(tri_integer_points[point],
			hole->Contour) == 1) {
		    inside_hole = true;
		    break;
		}
	    }
	    if (!inside_hole)
		(void)add_component_point((int)point);
	}
	std::vector<detria::PointD> local_points;
	local_points.reserve(local_to_global.size());
	for (int global_index : local_to_global)
	    local_points.push_back(tri_points[(size_t)global_index]);

	std::set<std::pair<int, int>> local_boundary_edges;
	std::vector<std::pair<int, int>> local_boundary_segments;
	const auto index_local_boundary = [&](const std::vector<int> &ring) {
	    for (size_t i = 0; i < ring.size(); ++i) {
		const int first = ring[i];
		const int second = ring[(i + 1) % ring.size()];
		const std::pair<int, int> edge = first < second ?
		    std::make_pair(first, second) :
		    std::make_pair(second, first);
		local_boundary_edges.insert(edge);
		local_boundary_segments.push_back(std::make_pair(first, second));
	    }
	};
	index_local_boundary(local_outline);
	for (const std::vector<int> &hole : local_holes)
	    index_local_boundary(hole);
	const auto orient = [&](int first, int second, int point) {
	    const long double abx = (long double)local_points[second].x -
		local_points[first].x;
	    const long double aby = (long double)local_points[second].y -
		local_points[first].y;
	    const long double acx = (long double)local_points[point].x -
		local_points[first].x;
	    const long double acy = (long double)local_points[point].y -
		local_points[first].y;
	    const long double cross = abx * acy - aby * acx;
	    return (cross > 0.0L) - (cross < 0.0L);
	};
	const auto on_segment = [&](int point, int first, int second) {
	    if (orient(first, second, point))
		return false;
	    return local_points[point].x >= std::min(local_points[first].x,
		local_points[second].x) &&
		local_points[point].x <= std::max(local_points[first].x,
		local_points[second].x) &&
		local_points[point].y >= std::min(local_points[first].y,
		local_points[second].y) &&
		local_points[point].y <= std::max(local_points[first].y,
		local_points[second].y);
	};
	const auto segments_intersect = [&](const std::pair<int, int> &first,
		const std::pair<int, int> &second) {
	    const int o1 = orient(first.first, first.second, second.first);
	    const int o2 = orient(first.first, first.second, second.second);
	    const int o3 = orient(second.first, second.second, first.first);
	    const int o4 = orient(second.first, second.second, first.second);
	    if (!o1 && on_segment(second.first, first.first, first.second))
		return true;
	    if (!o2 && on_segment(second.second, first.first, first.second))
		return true;
	    if (!o3 && on_segment(first.first, second.first, second.second))
		return true;
	    if (!o4 && on_segment(first.second, second.first, second.second))
		return true;
	    return o1 * o2 < 0 && o3 * o4 < 0;
	};
	const auto point_in_ring = [&](long double x, long double y,
		const std::vector<int> &ring) {
	    bool inside = false;
	    for (size_t i = 0, j = ring.size() - 1; i < ring.size();
		    j = i++) {
		const detria::PointD &first = local_points[(size_t)ring[i]];
		const detria::PointD &second = local_points[(size_t)ring[j]];
		if (((first.y > y) != (second.y > y)) &&
			x < ((long double)second.x - first.x) *
			(y - first.y) / ((long double)second.y - first.y) +
			first.x)
		    inside = !inside;
	    }
	    return inside;
	};
	std::vector<std::pair<int, int>> local_constraints;
	for (const std::pair<int, int> &constraint : cleaned_constraints) {
	    const auto first = global_to_local.find(constraint.first);
	    const auto second = global_to_local.find(constraint.second);
	    if (first == global_to_local.end() ||
		    second == global_to_local.end() ||
		    first->second == second->second)
		continue;
	    const std::pair<int, int> edge = first->second < second->second ?
		std::make_pair(first->second, second->second) :
		std::make_pair(second->second, first->second);
	    if (local_boundary_edges.find(edge) != local_boundary_edges.end())
		continue;
	    const long double midpoint_x = 0.5L *
		(local_points[(size_t)edge.first].x +
		local_points[(size_t)edge.second].x);
	    const long double midpoint_y = 0.5L *
		(local_points[(size_t)edge.first].y +
		local_points[(size_t)edge.second].y);
	    if (!point_in_ring(midpoint_x, midpoint_y, local_outline))
		continue;
	    bool rejected = false;
	    for (const std::vector<int> &hole : local_holes) {
		if (point_in_ring(midpoint_x, midpoint_y, hole)) {
		    rejected = true;
		    break;
		}
	    }
	    for (const std::pair<int, int> &boundary : local_boundary_segments) {
		if (rejected)
		    break;
		if (edge.first == boundary.first ||
			edge.first == boundary.second ||
			edge.second == boundary.first ||
			edge.second == boundary.second)
		    continue;
		if (segments_intersect(edge, boundary))
		    rejected = true;
	    }
	    for (const std::pair<int, int> &accepted : local_constraints) {
		if (rejected)
		    break;
		if (edge.first == accepted.first ||
			edge.first == accepted.second ||
			edge.second == accepted.first ||
			edge.second == accepted.second)
		    continue;
		if (segments_intersect(edge, accepted))
		    rejected = true;
	    }
	    if (!rejected)
		local_constraints.push_back(edge);
	}
	int *local_faces = NULL;
	int local_face_count = 0;
	point2d_t *local_output_points = NULL;
	int local_output_point_count = 0;
	const std::vector<std::vector<int>> local_outlines(1,
	    local_outline);
	const int status = detria_result(&local_faces, &local_face_count,
	    &local_output_points, &local_output_point_count, local_points,
	    local_outlines, local_holes, local_constraints);
	if (status != BRLCAD_OK || !local_faces || local_face_count <= 0 ||
		!local_output_points || local_output_point_count !=
		(int)local_to_global.size()) {
	    if (local_faces)
		bu_free(local_faces, "component detria faces");
	    if (local_output_points)
		bu_free(local_output_points, "component detria points");
	    return BRLCAD_ERROR;
	}
	for (int i = 0; i < 3 * local_face_count; ++i) {
	    const int local_index = local_faces[i];
	    if (local_index < 0 ||
		    (size_t)local_index >= local_to_global.size()) {
		bu_free(local_faces, "component detria faces");
		bu_free(local_output_points, "component detria points");
		return BRLCAD_ERROR;
	    }
	    combined_faces.push_back(local_to_global[(size_t)local_index]);
	}
	bu_free(local_faces, "component detria faces");
	bu_free(local_output_points, "component detria points");
    }
    if (combined_faces.empty() || combined_faces.size() % 3 ||
	    combined_faces.size() >
	    (size_t)std::numeric_limits<int>::max() * 3 ||
	    tri_points.size() > (size_t)std::numeric_limits<int>::max())
	return BRLCAD_ERROR;

    *faces = (int *)bu_calloc(combined_faces.size(), sizeof(int),
	"sanitized component detria faces");
    std::copy(combined_faces.begin(), combined_faces.end(), *faces);
    *num_faces = (int)(combined_faces.size() / 3);
    *out_pts = (point2d_t *)bu_calloc(tri_points.size(),
	sizeof(point2d_t), "sanitized component detria points");
    for (size_t i = 0; i < tri_points.size(); ++i)
	V2SET((*out_pts)[i], tri_points[i].x, tri_points[i].y);
    *num_outpts = (int)tri_points.size();
    return BRLCAD_OK;
}

int
bg_nested_poly_triangulate_clean(int **faces, int *num_faces,
	point2d_t **out_pts, int *num_outpts,
	const int *poly, const size_t poly_pnts,
	const int **holes_array, const size_t *holes_npts,
	const size_t nholes, const int *steiner, const size_t steiner_npts,
	const point2d_t *pts, const size_t npts)
{
    return bg_nested_poly_triangulate_clean_constraints(faces, num_faces,
	out_pts, num_outpts, poly, poly_pnts, holes_array, holes_npts,
	nholes, steiner, steiner_npts, NULL, 0, pts, npts);
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

static int
bg_triangulation_report_set(struct bg_triangulation_report *report,
	int reason, int input_index, const char *message)
{
    if (report) {
	report->reason = reason;
	report->input_index = input_index;
	bu_strlcpy(report->message, message ? message : "",
	    sizeof(report->message));
    }
    return reason == BG_TRIANGULATION_OK ? BRLCAD_OK : BRLCAD_ERROR;
}

static int
bg_detria(int **faces, int *num_faces, point2d_t **out_pts, int *num_outpts,
	const int *poly, const size_t poly_pnts,
	const int **holes_array, const size_t *holes_npts, const size_t nholes,
	const int *steiner, const size_t steiner_npts,
	const int *constraints, const size_t constraint_cnt,
	const point2d_t *pts, const size_t npts,
	struct bg_triangulation_report *report)
{
    bg_triangulation_report_set(report, BG_TRIANGULATION_INVALID_INPUT, -1,
	"invalid constrained triangulation input");
    if (!faces || !num_faces)
	return BRLCAD_ERROR;
    *faces = NULL;
    *num_faces = 0;
    if (out_pts)
	*out_pts = NULL;
    if (num_outpts)
	*num_outpts = 0;
    if (!poly || poly_pnts < 3 || !pts ||
	    npts < 3 || (nholes && (!holes_array || !holes_npts)) ||
	    (steiner_npts && !steiner) || (constraint_cnt && !constraints))
	return BRLCAD_ERROR;

    auto valid_index = [&](int index) {
	return index >= 0 && (size_t)index < npts &&
	    std::isfinite(pts[index][X]) && std::isfinite(pts[index][Y]);
    };
    auto normalize_ring = [&](const int *indices, size_t count,
	    std::vector<int> &ring) {
	if (!indices || count < 3)
	    return false;
	for (size_t i = 0; i < count; ++i) {
	    if (!valid_index(indices[i]))
		return false;
	    if (ring.empty() || ring.back() != indices[i])
		ring.push_back(indices[i]);
	}
	if (ring.size() > 1 && ring.front() == ring.back())
	    ring.pop_back();
	std::set<int> unique(ring.begin(), ring.end());
	return ring.size() >= 3 && unique.size() == ring.size();
    };

    std::vector<int> outer;
    if (!normalize_ring(poly, poly_pnts, outer))
	return BRLCAD_ERROR;
    std::vector<std::vector<int>> hole_rings(nholes);
    for (size_t h = 0; h < nholes; ++h) {
	if (!normalize_ring(holes_array[h], holes_npts[h], hole_rings[h]))
	    return BRLCAD_ERROR;
    }
    bg_triangulation_report_set(report, BG_TRIANGULATION_INVALID_PSLG, -1,
	"invalid or duplicate chart constraint topology");

    std::map<std::pair<double, double>, int> constraint_coordinates;
    std::map<int, size_t> constraint_ring;
    std::set<std::pair<int, int>> constraint_edges;
    struct input_segment {
	int a;
	int b;
	size_t ring;
    };
    std::vector<input_segment> segments;
    std::vector<double> ring_max_x(nholes + 1,
	-std::numeric_limits<double>::infinity());
    auto register_ring = [&](const std::vector<int> &ring, size_t ring_id) {
	for (size_t i = 0; i < ring.size(); ++i) {
	    const int index = ring[i];
	    ring_max_x[ring_id] = std::max(ring_max_x[ring_id],
		pts[index][X]);
	    const std::pair<double, double> coordinate(pts[index][X],
		pts[index][Y]);
	    const auto old_coordinate = constraint_coordinates.find(coordinate);
	    if (old_coordinate != constraint_coordinates.end() &&
		    old_coordinate->second != index) {
		char message[128];
		std::snprintf(message, sizeof(message),
		    "chart vertices %d and %d share a constraint coordinate",
		    old_coordinate->second, index);
		bg_triangulation_report_set(report,
		    BG_TRIANGULATION_INVALID_PSLG, index, message);
		return false;
	    }
	    constraint_coordinates[coordinate] = index;
	    const auto old_ring = constraint_ring.find(index);
	    if (old_ring != constraint_ring.end() && old_ring->second != ring_id) {
		char message[128];
		std::snprintf(message, sizeof(message),
		    "chart vertex %d belongs to rings %zu and %zu", index,
		    old_ring->second, ring_id);
		bg_triangulation_report_set(report,
		    BG_TRIANGULATION_INVALID_PSLG, index, message);
		return false;
	    }
	    constraint_ring[index] = ring_id;
	    const int next = ring[(i + 1) % ring.size()];
	    const std::pair<int, int> edge = (index < next) ?
		std::make_pair(index, next) : std::make_pair(next, index);
	    if (!constraint_edges.insert(edge).second) {
		char message[128];
		std::snprintf(message, sizeof(message),
		    "constraint edge %d-%d is duplicated", edge.first,
		    edge.second);
		bg_triangulation_report_set(report,
		    BG_TRIANGULATION_INVALID_PSLG, index, message);
		return false;
	    }
	    segments.push_back({index, next, ring_id});
	}
	return true;
    };
    if (!register_ring(outer, 0))
	return BRLCAD_ERROR;
    for (size_t h = 0; h < nholes; ++h) {
	if (!register_ring(hole_rings[h], h + 1))
	    return BRLCAD_ERROR;
    }
    std::vector<std::pair<int, int>> manual_constraints;
    manual_constraints.reserve(constraint_cnt);
    for (size_t i = 0; i < constraint_cnt; ++i) {
	const int first = constraints[2 * i];
	const int second = constraints[2 * i + 1];
	if (!valid_index(first) || !valid_index(second) || first == second)
	    return BRLCAD_ERROR;
	const std::pair<int, int> edge = (first < second) ?
	    std::make_pair(first, second) : std::make_pair(second, first);
	if (!constraint_edges.insert(edge).second) {
	    char message[128];
	    std::snprintf(message, sizeof(message),
		"constraint edge %d-%d is duplicated", edge.first,
		edge.second);
	    bg_triangulation_report_set(report,
		BG_TRIANGULATION_INVALID_PSLG, (int)i, message);
	    return BRLCAD_ERROR;
	}
	for (int index : {first, second}) {
	    const std::pair<double, double> coordinate(pts[index][X],
		pts[index][Y]);
	    const auto old_coordinate = constraint_coordinates.find(coordinate);
	    if (old_coordinate != constraint_coordinates.end() &&
		    old_coordinate->second != index) {
		char message[128];
		std::snprintf(message, sizeof(message),
		    "chart vertices %d and %d share a constraint coordinate",
		    old_coordinate->second, index);
		bg_triangulation_report_set(report,
		    BG_TRIANGULATION_INVALID_PSLG, (int)i, message);
		return BRLCAD_ERROR;
	    }
	    constraint_coordinates[coordinate] = index;
	}
	manual_constraints.push_back(std::make_pair(first, second));
	segments.push_back({first, second,
	    std::numeric_limits<size_t>::max()});
    }

    RTree<size_t, double, 2> segment_index;
    for (size_t i = 0; i < segments.size(); ++i) {
	const input_segment &segment = segments[i];
	double minimum[2] = {
	    std::min(pts[segment.a][X], pts[segment.b][X]),
	    std::min(pts[segment.a][Y], pts[segment.b][Y])
	};
	double maximum[2] = {
	    std::max(pts[segment.a][X], pts[segment.b][X]),
	    std::max(pts[segment.a][Y], pts[segment.b][Y])
	};
	segment_index.Insert(minimum, maximum, i);
    }
    auto collect_segment = [](size_t segment, void *context) {
	std::vector<size_t> *matches = (std::vector<size_t> *)context;
	matches->push_back(segment);
	return true;
    };

    auto orient = [&](int ia, int ib, int ic) {
	const long double abx = (long double)pts[ib][X] - pts[ia][X];
	const long double aby = (long double)pts[ib][Y] - pts[ia][Y];
	const long double acx = (long double)pts[ic][X] - pts[ia][X];
	const long double acy = (long double)pts[ic][Y] - pts[ia][Y];
	const long double cross = abx * acy - aby * acx;
	return (cross > 0.0L) - (cross < 0.0L);
    };
    auto on_segment = [&](int ip, const input_segment &segment) {
	if (orient(segment.a, segment.b, ip))
	    return false;
	const long double px = pts[ip][X];
	const long double py = pts[ip][Y];
	return px >= std::min((long double)pts[segment.a][X],
		(long double)pts[segment.b][X]) &&
	    px <= std::max((long double)pts[segment.a][X],
		(long double)pts[segment.b][X]) &&
	    py >= std::min((long double)pts[segment.a][Y],
		(long double)pts[segment.b][Y]) &&
	    py <= std::max((long double)pts[segment.a][Y],
		(long double)pts[segment.b][Y]);
    };
    auto segments_intersect = [&](const input_segment &a,
	    const input_segment &b) {
	const int o1 = orient(a.a, a.b, b.a);
	const int o2 = orient(a.a, a.b, b.b);
	const int o3 = orient(b.a, b.b, a.a);
	const int o4 = orient(b.a, b.b, a.b);
	if (!o1 && on_segment(b.a, a)) return true;
	if (!o2 && on_segment(b.b, a)) return true;
	if (!o3 && on_segment(a.a, b)) return true;
	if (!o4 && on_segment(a.b, b)) return true;
	return o1 * o2 < 0 && o3 * o4 < 0;
    };
    for (size_t i = 0; i < segments.size(); ++i) {
	const input_segment &segment = segments[i];
	double minimum[2] = {
	    std::min(pts[segment.a][X], pts[segment.b][X]),
	    std::min(pts[segment.a][Y], pts[segment.b][Y])
	};
	double maximum[2] = {
	    std::max(pts[segment.a][X], pts[segment.b][X]),
	    std::max(pts[segment.a][Y], pts[segment.b][Y])
	};
	std::vector<size_t> candidates;
	segment_index.Search(minimum, maximum, collect_segment,
	    &candidates);
	for (size_t j : candidates) {
	    if (j <= i)
		continue;
	    if (segments[i].a == segments[j].a ||
		    segments[i].a == segments[j].b ||
		    segments[i].b == segments[j].a ||
		    segments[i].b == segments[j].b)
		continue;
	    if (segments_intersect(segments[i], segments[j])) {
		char message[128];
		std::snprintf(message, sizeof(message),
		    "constraints %d-%d and %d-%d intersect",
		    segments[i].a, segments[i].b, segments[j].a,
		    segments[j].b);
		bg_triangulation_report_set(report,
		    BG_TRIANGULATION_CROSSING_CONSTRAINTS, (int)i,
		    message);
		return BRLCAD_ERROR;
	    }
	}
    }

    auto point_in_ring = [&](int point, size_t ring_id) {
	if (ring_id >= ring_max_x.size() ||
		pts[point][X] > ring_max_x[ring_id])
	    return false;
	double minimum[2] = {pts[point][X], pts[point][Y]};
	double maximum[2] = {ring_max_x[ring_id], pts[point][Y]};
	std::vector<size_t> candidates;
	segment_index.Search(minimum, maximum, collect_segment,
	    &candidates);
	int winding = 0;
	for (size_t candidate : candidates) {
	    const input_segment &segment = segments[candidate];
	    if (segment.ring != ring_id)
		continue;
	    const int a = segment.a;
	    const int b = segment.b;
	    if (pts[a][Y] <= pts[point][Y]) {
		if (pts[b][Y] > pts[point][Y] && orient(a, b, point) > 0)
		    ++winding;
	    } else if (pts[b][Y] <= pts[point][Y] &&
		    orient(a, b, point) < 0) {
		--winding;
	    }
	}
	return winding != 0;
    };
    for (size_t h = 0; h < hole_rings.size(); ++h) {
	if (!point_in_ring(hole_rings[h][0], 0)) {
	    bg_triangulation_report_set(report,
		BG_TRIANGULATION_INVALID_NESTING, (int)h,
		"hole is not contained by the chart outline");
	    return BRLCAD_ERROR;
	}
	for (size_t other = 0; other < hole_rings.size(); ++other) {
	    if (h != other && point_in_ring(hole_rings[h][0],
		    other + 1)) {
		bg_triangulation_report_set(report,
		    BG_TRIANGULATION_INVALID_NESTING, (int)h,
		    "nested hole requires an explicit island outline");
		return BRLCAD_ERROR;
	    }
	}
    }
    for (const std::pair<int, int> &edge : manual_constraints) {
	for (int index : {edge.first, edge.second}) {
	    if (constraint_ring.find(index) != constraint_ring.end())
		continue;
	    if (!point_in_ring(index, 0))
		return BRLCAD_ERROR;
	    for (size_t h = 0; h < hole_rings.size(); ++h) {
		if (point_in_ring(index, h + 1))
		    return BRLCAD_ERROR;
	    }
	}
    }

    std::vector<int> accepted_steiner;
    std::map<std::pair<double, double>, int> active_coordinates =
	constraint_coordinates;
    for (size_t i = 0; i < steiner_npts; ++i) {
	const int index = steiner[i];
	if (!valid_index(index))
	    return BRLCAD_ERROR;
	const std::pair<double, double> coordinate(pts[index][X],
	    pts[index][Y]);
	if (active_coordinates.find(coordinate) != active_coordinates.end())
	    continue;
	bool on_constraint = false;
	double query[2] = {pts[index][X], pts[index][Y]};
	std::vector<size_t> candidates;
	segment_index.Search(query, query, collect_segment, &candidates);
	for (size_t candidate : candidates) {
	    if (on_segment(index, segments[candidate])) {
		on_constraint = true;
		break;
	    }
	}
	if (on_constraint || !point_in_ring(index, 0))
	    continue;
	bool in_hole = false;
	for (size_t h = 0; h < hole_rings.size(); ++h) {
	    if (point_in_ring(index, h + 1)) {
		in_hole = true;
		break;
	    }
	}
	if (!in_hole) {
	    active_coordinates[coordinate] = index;
	    accepted_steiner.push_back(index);
	}
    }

    std::set<int> active_pts(outer.begin(), outer.end());
    for (const std::vector<int> &hole : hole_rings)
	active_pts.insert(hole.begin(), hole.end());
    for (const std::pair<int, int> &edge : manual_constraints) {
	active_pts.insert(edge.first);
	active_pts.insert(edge.second);
    }
    active_pts.insert(accepted_steiner.begin(), accepted_steiner.end());

    std::map<int, int> det2pts;
    std::map<int, int> pts2det;
    std::vector<detria::PointD> raw_points;
    for (int index : active_pts) {
	detria::PointD point;
	point.x = pts[index][X];
	point.y = pts[index][Y];
	const int detria_index = (int)raw_points.size();
	raw_points.push_back(point);
	det2pts[detria_index] = index;
	pts2det[index] = detria_index;
    }
    std::vector<detria::PointD> conditioned;
    if (!detria_condition_points(raw_points, conditioned)) {
	bg_triangulation_report_set(report, BG_TRIANGULATION_INVALID_INPUT, -1,
	    "chart coordinates cannot be conditioned safely");
	return BRLCAD_ERROR;
    }

    std::vector<int> outer_polyline;
    for (int index : outer)
	outer_polyline.push_back(pts2det[index]);
    std::vector<std::vector<int>> inner_holes(hole_rings.size());
    for (size_t h = 0; h < hole_rings.size(); ++h) {
	for (int index : hole_rings[h])
	    inner_holes[h].push_back(pts2det[index]);
    }

    detria::Triangulation<detria::PointD, int> tri;
    tri.setPoints(conditioned);
    tri.addOutline(outer_polyline);
    for (const std::vector<int> &hole : inner_holes)
	tri.addHole(hole);
    std::vector<std::pair<int, int>> detria_constraints;
    detria_constraints.reserve(manual_constraints.size());
    for (const std::pair<int, int> &edge : manual_constraints) {
	const std::pair<int, int> mapped(pts2det[edge.first],
	    pts2det[edge.second]);
	tri.setConstrainedEdge(mapped.first, mapped.second);
	detria_constraints.push_back(mapped);
    }

    bg_triangulation_report_set(report, BG_TRIANGULATION_DETRIA_FAILED, -1,
	"detria rejected the validated chart");
    bool tri_success = false;
    try {
	tri_success = tri.triangulate(true);
    } catch (...) {
	return BRLCAD_ERROR;
    }
    if (!tri_success) {
	bu_log("bg_detria: triangulation failed: %s\n", tri.getErrorMessage().c_str());
	bg_triangulation_report_set(report, BG_TRIANGULATION_DETRIA_FAILED, -1,
	    tri.getErrorMessage().c_str());
	return BRLCAD_ERROR;
    }

    std::vector<int> triangles;
    tri.forEachTriangle([&](const detria::Triangle<int> triangle) {
	triangles.push_back(triangle.x);
	triangles.push_back(triangle.y);
	triangles.push_back(triangle.z);
    }, true);
    std::vector<std::vector<int>> outlines(1, outer_polyline);
    if (!detria_postconditions(conditioned, outlines, inner_holes,
	    detria_constraints, triangles)) {
	bg_triangulation_report_set(report,
	    BG_TRIANGULATION_POSTCONDITION_FAILED, -1,
	    "detria output failed chart certification");
	return BRLCAD_ERROR;
	}

    *num_faces = (int)(triangles.size() / 3);
    *faces = (int *)bu_calloc(triangles.size(), sizeof(int), "faces array");
    std::set<int> output_points;
    for (size_t i = 0; i < triangles.size(); ++i) {
	(*faces)[i] = det2pts[triangles[i]];
	output_points.insert((*faces)[i]);
    }
    if (out_pts)
	*out_pts = (point2d_t *)pts;
    if (num_outpts)
	*num_outpts = (int)output_points.size();
    bg_triangulation_report_set(report, BG_TRIANGULATION_OK, -1,
	"certified constrained triangulation");
    return BRLCAD_OK;
}

extern "C" int
bg_nested_poly_triangulate_strict(int **faces, int *num_faces,
	point2d_t **out_pts, int *num_outpts,
	const int *poly, const size_t poly_pnts,
	const int **holes_array, const size_t *holes_npts, const size_t nholes,
	const int *steiner, const size_t steiner_npts,
	const point2d_t *pts, const size_t npts,
	struct bg_triangulation_report *report)
{
    return bg_detria(faces, num_faces, out_pts, num_outpts, poly,
	poly_pnts, holes_array, holes_npts, nholes, steiner, steiner_npts,
	NULL, 0, pts, npts, report);
}

extern "C" int
bg_nested_poly_triangulate_constraints_strict(int **faces, int *num_faces,
	point2d_t **out_pts, int *num_outpts,
	const int *poly, const size_t poly_pnts,
	const int **holes_array, const size_t *holes_npts, const size_t nholes,
	const int *steiner, const size_t steiner_npts,
	const int *constraints, const size_t constraint_cnt,
	const point2d_t *pts, const size_t npts,
	struct bg_triangulation_report *report)
{
    return bg_detria(faces, num_faces, out_pts, num_outpts, poly,
	poly_pnts, holes_array, holes_npts, nholes, steiner, steiner_npts,
	constraints, constraint_cnt, pts, npts, report);
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
	int detria_ret = bg_detria(faces, num_faces, out_pts, num_outpts,
	    poly, poly_pnts, holes_array, holes_npts, nholes, steiner,
	    steiner_npts, NULL, 0, pts, npts, NULL);
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
