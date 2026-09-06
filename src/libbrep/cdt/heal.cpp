/*                         H E A L . C P P
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * Distributed under the terms of the GNU Lesser General Public License
 * (LGPL), version 2.1.
 */

#include "common.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <utility>

#include "bn/tol.h"
#include "bu/datetime.h"
#include "../../libbg/RTree.h"
#include "heal.h"
#include "test_api.h"

namespace {

/* Curve hulls bound approximation error; segment separation is deliberately
 * larger so a sampled contour cannot hide a crossing of its source curves. */
constexpr double flatness_fraction = 0.125;
constexpr size_t operations_per_point = 64;
/* Leave room for sampled curves, the segment index, and temporary walks. */
constexpr size_t segment_storage_factor = 4;
constexpr int maximum_curve_depth = 24;
constexpr int maximum_curve_order = 32;

struct healing_budget {
    size_t points;
    size_t operations;
    int64_t deadline;
    bool limited = false;

    bool spend(size_t count = 1)
    {
	if (count > operations || bu_gettime() > deadline) {
	    limited = true;
	    return false;
	}
	operations -= count;
	return true;
    }

    bool point()
    {
	if (!points) {
	    limited = true;
	    return false;
	}
	--points;
	return spend();
    }
};

using polygon = std::vector<ON_2dPoint>;

double
segment_distance(const ON_2dPoint &p, const ON_2dPoint &a,
	const ON_2dPoint &b)
{
    const ON_2dVector direction = b - a;
    const double squared = direction.LengthSquared();
    const double fraction = squared > 0.0 ?
	std::max(0.0, std::min(1.0, ((p - a) * direction) / squared)) : 0.0;
    return p.DistanceTo(a + fraction * direction);
}

long double
cross(const ON_2dPoint &a, const ON_2dPoint &b, const ON_2dPoint &c)
{
    return ((long double)b.x - a.x) * ((long double)c.y - a.y) -
	((long double)b.y - a.y) * ((long double)c.x - a.x);
}

bool
segments_close(const ON_2dPoint &a, const ON_2dPoint &b,
	const ON_2dPoint &c, const ON_2dPoint &d, double tolerance)
{
    const long double ab_c = cross(a, b, c);
    const long double ab_d = cross(a, b, d);
    const long double cd_a = cross(c, d, a);
    const long double cd_b = cross(c, d, b);
    if ((ab_c > 0.0L) != (ab_d > 0.0L) &&
	(cd_a > 0.0L) != (cd_b > 0.0L))
	return true;
    return std::min(std::min(segment_distance(a, c, d),
	segment_distance(b, c, d)), std::min(segment_distance(c, a, b),
	segment_distance(d, a, b))) <= tolerance;
}

bool
flatten_bezier(const ON_BezierCurve &curve, double tolerance,
	int depth, polygon &points, healing_budget &budget)
{
    if (!budget.spend((size_t)curve.CVCount()))
	return false;
    ON_3dPoint first, last;
    if (!curve.GetCV(0, first) || !curve.GetCV(curve.CVCount() - 1, last))
	return false;
    bool flat = true;
    for (int i = 0; i < curve.CVCount(); ++i) {
	ON_3dPoint cv;
	if (!curve.GetCV(i, cv) || !cv.IsValid() ||
	    (curve.IsRational() && !(curve.Weight(i) > 0.0)))
	    return false;
	flat = flat && segment_distance(ON_2dPoint(cv), ON_2dPoint(first),
	    ON_2dPoint(last)) <= tolerance;
    }
    if (flat) {
	if (!budget.point())
	    return false;
	points.emplace_back(last);
	return true;
    }
    if (depth >= maximum_curve_depth)
	return false;
    ON_BezierCurve left, right;
    return curve.Split(0.5, left, right) &&
	flatten_bezier(left, tolerance, depth + 1, points, budget) &&
	flatten_bezier(right, tolerance, depth + 1, points, budget);
}

bool
sample_curve(const ON_Curve &curve, const ON_Xform &transform,
	double tolerance, polygon &points, healing_budget &budget)
{
    if (curve.Degree() >= maximum_curve_order ||
	!budget.spend((size_t)std::max(1, curve.SpanCount())))
	return false;
    ON_NurbsCurve nurbs;
    if (!curve.GetNurbForm(nurbs) || !nurbs.IsValid() ||
	!nurbs.Transform(transform) || !nurbs.ChangeDimension(2))
	return false;
    if (!budget.point())
	return false;
    points.emplace_back(nurbs.PointAtStart());
    for (int span = 0; span <= nurbs.CVCount() - nurbs.Order(); ++span) {
	if (!(nurbs.Knot(span + nurbs.Order() - 2) <
	    nurbs.Knot(span + nurbs.Order() - 1)))
	    continue;
	ON_BezierCurve bezier;
	if (!nurbs.ConvertSpanToBezier(span, bezier) ||
	    !flatten_bezier(bezier, tolerance, 0, points, budget))
	    return false;
    }
    return points.size() > 1;
}

struct planar_chart {
    ON_Xform to_plane;
    ON_Xform uv_to_plane;
    ON_Xform to_uv;
    ON_Plane plane;

    bool init(const ON_BrepFace &face)
    {
	/* An analytic plane gives an exact affine pcurve projection.  Merely
	 * fitting a plane to a spline is not sufficient for this operation. */
	const ON_PlaneSurface *surface = ON_PlaneSurface::Cast(face.SurfaceOf());
	if (!surface || !surface->IsValid())
	    return false;
	const ON_Interval u = face.Domain(0);
	const ON_Interval v = face.Domain(1);
	if (!u.IsIncreasing() || !v.IsIncreasing())
	    return false;
	const ON_3dPoint origin = face.PointAt(u.Min(), v.Min());
	ON_3dVector along_u = face.PointAt(u.Max(), v.Min()) - origin;
	ON_3dVector along_v = face.PointAt(u.Min(), v.Max()) - origin;
	const double u_scale = along_u.Length() / u.Length();
	const double v_scale = along_v.Length() / v.Length();
	if (!(u_scale > 0.0) || !(v_scale > 0.0) ||
	    !std::isfinite(u_scale) || !std::isfinite(v_scale) ||
	    !along_u.Unitize() || !along_v.Unitize())
	    return false;
	plane = surface->m_plane;
	to_plane = ON_Xform::IdentityTransformation;
	for (int axis = 0; axis < 3; ++axis) {
	    to_plane[0][axis] = along_u[axis];
	    to_plane[1][axis] = along_v[axis];
	}
	to_plane[0][3] = -(along_u * origin);
	to_plane[1][3] = -(along_v * origin);
	uv_to_plane = ON_Xform::IdentityTransformation;
	uv_to_plane[0][0] = u_scale;
	uv_to_plane[1][1] = v_scale;
	uv_to_plane[0][3] = -u.Min() * u_scale;
	uv_to_plane[1][3] = -v.Min() * v_scale;
	ON_Xform plane_to_uv = ON_Xform::IdentityTransformation;
	plane_to_uv[0][0] = 1.0 / u_scale;
	plane_to_uv[1][1] = 1.0 / v_scale;
	plane_to_uv[0][3] = u.Min();
	plane_to_uv[1][3] = v.Min();
	to_uv = plane_to_uv * to_plane;
	return true;
    }

    bool edge_on_plane(const ON_BrepEdge &edge, double tolerance,
	    double &deviation, healing_budget &budget) const
    {
	if (edge.Degree() >= maximum_curve_order ||
	    !budget.spend((size_t)std::max(1, edge.SpanCount())))
	    return false;
	ON_NurbsCurve curve;
	if (!edge.GetNurbForm(curve) || !curve.IsValid() ||
	    !budget.spend((size_t)curve.CVCount()))
	    return false;
	deviation = 0.0;
	for (int i = 0; i < curve.CVCount(); ++i) {
	    ON_3dPoint point;
	    if (!curve.GetCV(i, point) || !point.IsValid() ||
		(curve.IsRational() && !(curve.Weight(i) > 0.0)))
		return false;
	    deviation = std::max(deviation, std::abs(plane.DistanceTo(point)));
	}
	return deviation <= tolerance;
    }
};

bool
polyline_matches(const polygon &a, const polygon &b, double tolerance,
	healing_budget &budget)
{
    RTree<size_t, double, 2> tree;
    for (size_t j = 1; j < b.size(); ++j) {
	if (!budget.spend())
	    return false;
	const double minimum[2] = {std::min(b[j - 1].x, b[j].x),
	    std::min(b[j - 1].y, b[j].y)};
	const double maximum[2] = {std::max(b[j - 1].x, b[j].x),
	    std::max(b[j - 1].y, b[j].y)};
	tree.Insert(minimum, maximum, j);
    }
    for (const ON_2dPoint &point : a) {
	if (!budget.spend())
	    return false;
	const double minimum[2] = {point.x - tolerance, point.y - tolerance};
	const double maximum[2] = {point.x + tolerance, point.y + tolerance};
	bool matched = false;
	tree.Search(minimum, maximum, [&](size_t j, void *) {
	    if (!budget.spend())
		return false;
	    matched = segment_distance(point, b[j - 1], b[j]) <= tolerance;
	    return !matched;
	}, NULL);
	if (!matched)
	    return false;
    }
    return true;
}

bool
append_curve(polygon &loop, const polygon &curve, double tolerance)
{
    if (curve.size() < 2 || (!loop.empty() &&
	loop.back().DistanceTo(curve.front()) > tolerance))
	return false;
    loop.insert(loop.end(), curve.begin() + (loop.empty() ? 0 : 1), curve.end());
    return true;
}

bool
close_polygon(polygon &points, double tolerance)
{
    if (points.size() < 4 || points.front().DistanceTo(points.back()) > tolerance)
	return false;
    points.pop_back();
    return true;
}

bool
sample_loop(const ON_BrepLoop &loop, const planar_chart &chart,
	double tolerance, polygon &points, healing_budget &budget)
{
    for (int ti = 0; ti < loop.TrimCount(); ++ti) {
	const ON_BrepTrim &trim = *loop.Trim(ti);
	const ON_BrepTrim &next = *loop.Trim((ti + 1) % loop.TrimCount());
	const ON_BrepEdge *edge = trim.Edge();
	double deviation;
	if (!edge || trim.m_vi[1] != next.m_vi[0] ||
	    !chart.edge_on_plane(*edge, tolerance, deviation, budget))
	    return false;
	polygon trim_points, edge_points;
	if (!sample_curve(trim, chart.uv_to_plane, tolerance * flatness_fraction,
	    trim_points, budget) || !sample_curve(*edge, chart.to_plane,
	    tolerance * flatness_fraction, edge_points, budget) ||
	    !polyline_matches(trim_points, edge_points, tolerance / 2.0, budget) ||
	    !polyline_matches(edge_points, trim_points, tolerance / 2.0, budget) ||
	    !append_curve(points, trim_points, tolerance))
	    return false;
    }
    return close_polygon(points, tolerance);
}

struct segment {
    ON_2dPoint a, b;
    size_t loop, index;
};

bool
simple_boundaries(const std::vector<polygon> &loops, double tolerance,
	healing_budget &budget)
{
    std::vector<segment> segments;
    RTree<size_t, double, 2> tree;
    for (size_t li = 0; li < loops.size(); ++li) {
	const polygon &loop = loops[li];
	for (size_t i = 0; i < loop.size(); ++i) {
	    const segment current = {loop[i], loop[(i + 1) % loop.size()], li, i};
	    if (current.a.DistanceTo(current.b) <= tolerance * flatness_fraction)
		return false;
	    const double minimum[2] = {std::min(current.a.x, current.b.x) - tolerance,
		std::min(current.a.y, current.b.y) - tolerance};
	    const double maximum[2] = {std::max(current.a.x, current.b.x) + tolerance,
		std::max(current.a.y, current.b.y) + tolerance};
	    std::vector<size_t> candidates;
	    tree.Search(minimum, maximum, [](size_t value, void *data) {
		static_cast<std::vector<size_t> *>(data)->push_back(value);
		return true;
	    }, &candidates);
	    if (!budget.spend(candidates.size() + 1))
		return false;
	    for (size_t ci : candidates) {
		const segment &other = segments[ci];
		if (other.loop == li && ((other.index + 1) % loop.size() == i ||
		    (i + 1) % loop.size() == other.index)) {
		    const ON_2dVector a = current.b - current.a;
		    const ON_2dVector b = other.b - other.a;
		    if (a * b < 0.0 && std::abs(a.x * b.y - a.y * b.x) <=
			tolerance * std::min(a.Length(), b.Length()))
			return false;
		    continue;
		}
		if (segments_close(current.a, current.b, other.a, other.b, tolerance))
		    return false;
	    }
	    tree.Insert(minimum, maximum, segments.size());
	    segments.push_back(current);
	}
    }
    return true;
}

bool
inside(const ON_2dPoint &point, const polygon &outline, healing_budget &budget)
{
    bool result = false;
    if (!budget.spend(outline.size()))
	return false;
    for (size_t i = 0; i < outline.size(); ++i) {
	const ON_2dPoint &a = outline[i];
	const ON_2dPoint &b = outline[(i + 1) % outline.size()];
	if ((a.y > point.y) != (b.y > point.y) &&
	    point.x < (b.x - a.x) * (point.y - a.y) / (b.y - a.y) + a.x)
	    result = !result;
    }
    return result;
}

long double
area(const polygon &loop)
{
    long double sum = 0.0L;
    for (size_t i = 1; i + 1 < loop.size(); ++i)
	sum += cross(loop[0], loop[i], loop[i + 1]);
    return sum / 2.0L;
}

bool
paired_boundaries(const ON_BrepFace &face)
{
    for (int li = 0; li < face.LoopCount(); ++li) {
	const ON_BrepLoop &loop = *face.Loop(li);
	for (int ti = 0; ti < loop.TrimCount(); ++ti)
	    if (!loop.Trim(ti)->Edge() || loop.Trim(ti)->Edge()->TrimCount() != 2)
		return false;
    }
    return true;
}

using edge_walk = std::vector<std::pair<int, bool>>;

bool
trace_boundary_cycle(const ON_Brep &brep, std::set<int> &remaining,
	const std::map<int, std::vector<int>> &incident,
	edge_walk &walk, healing_budget &budget)
{
    if (remaining.empty())
	return false;
    const int start = brep.m_E[*remaining.begin()].m_vi[0];
    int vertex = start;
    do {
	if (!budget.spend())
	    return false;
	const auto next = incident.find(vertex);
	if (next == incident.end() || next->second.size() != 2)
	    return false;
	int selected = -1;
	for (int ei : next->second)
	    if (remaining.count(ei)) { selected = ei; break; }
	if (selected < 0)
	    return false;
	const ON_BrepEdge &edge = brep.m_E[selected];
	const bool reverse = edge.m_vi[0] != vertex;
	walk.emplace_back(selected, reverse);
	remaining.erase(selected);
	vertex = edge.m_vi[reverse ? 0 : 1];
    } while (vertex != start);
    return true;
}

bool
restore_outer(ON_Brep &brep, int fi, const planar_chart &chart,
	const std::vector<polygon> &holes,
	double tolerance, healing_budget &budget, cdt_healing &result)
{
    /* Existing paired hole boundaries distinguish a face with a missing
     * outline from an isolated clockwise sheet.  Losing the outline can
     * itself disconnect the surrounding shell from these inner walls. */
    if (!paired_boundaries(brep.m_F[fi]))
	return false;
    std::set<int> candidates;
    std::map<int, std::vector<int>> incident;
    std::map<int, double> deviations;
    for (int ei = 0; ei < brep.m_E.Count(); ++ei) {
	if (!budget.spend())
	    return false;
	const ON_BrepEdge &edge = brep.m_E[ei];
	if (edge.TrimCount() != 1)
	    continue;
	const int neighbor = edge.Trim(0)->Face()->m_face_index;
	if (neighbor == fi)
	    continue;
	double deviation;
	if (!chart.edge_on_plane(edge, tolerance, deviation, budget))
	    continue;
	candidates.insert(ei);
	deviations[ei] = deviation;
	incident[edge.m_vi[0]].push_back(ei);
	incident[edge.m_vi[1]].push_back(ei);
    }
    edge_walk accepted;
    while (!candidates.empty()) {
	edge_walk walk;
	if (!trace_boundary_cycle(brep, candidates, incident, walk, budget))
	    return false;
	polygon outline;
	bool usable = true;
	for (const auto &step : walk) {
	    const ON_BrepEdge &edge = brep.m_E[step.first];
	    polygon points;
	    if (!sample_curve(edge, chart.to_plane, tolerance * flatness_fraction,
		points, budget))
		return false;
	    if (step.second)
		std::reverse(points.begin(), points.end());
	    usable = append_curve(outline, points, tolerance) && usable;
	}
	if (!usable || !close_polygon(outline, tolerance))
	    continue;
	std::vector<polygon> boundaries = holes;
	boundaries.push_back(outline);
	if (!simple_boundaries(boundaries, tolerance, budget))
	    continue;
	bool encloses = true;
	for (const polygon &hole : holes)
	    encloses = inside(hole[0], outline, budget) && encloses;
	if (!encloses)
	    continue;
	if (!accepted.empty())
	    return false;
	accepted.swap(walk);
    }
    if (accepted.empty() || budget.limited)
	return false;

    /* Prepare every pcurve before editing topology, so a rejected candidate
     * cannot leave half a reconstructed loop in the owned B-Rep. */
    std::vector<std::unique_ptr<ON_NurbsCurve>> curves;
    for (const auto &entry : accepted) {
	std::unique_ptr<ON_NurbsCurve> curve(new ON_NurbsCurve());
	if (!brep.m_E[entry.first].GetNurbForm(*curve) ||
	    !curve->Transform(chart.to_uv) || !curve->ChangeDimension(2))
	    return false;
	if (entry.second && !curve->Reverse())
	    return false;
	curves.push_back(std::move(curve));
    }
    ON_BrepFace &face = brep.m_F[fi];
    ON_BrepLoop &loop = brep.NewLoop(ON_BrepLoop::outer, face);
    for (size_t i = 0; i < accepted.size(); ++i) {
	ON_BrepEdge &edge = brep.m_E[accepted[i].first];
	const int curve_index = brep.AddTrimCurve(curves[i].release());
	ON_BrepTrim &trim = brep.NewTrim(edge, accepted[i].second, loop, curve_index);
	trim.m_tolerance[0] = tolerance / chart.uv_to_plane[0][0];
	trim.m_tolerance[1] = tolerance / chart.uv_to_plane[1][1];
	for (int ti = 0; ti < edge.TrimCount(); ++ti)
	    brep.SetTrimTypeFlags(*edge.Trim(ti), false);
	result.edges.insert(edge.m_edge_index);
	result.max_deviation = std::max(result.max_deviation, deviations[edge.m_edge_index]);
    }
    if (brep.LoopDirection(loop) < 0)
	brep.FlipLoop(loop);
    brep.SetTrimIsoFlags(loop);
    brep.SortFaceLoops(face);
    result.faces.insert(fi);
    ++result.outer_loops;
    return true;
}

void
normalize_roles(ON_Brep &brep, int fi, const std::vector<polygon> &loops,
	bool supported_root, healing_budget &budget, cdt_healing &result)
{
    int root = -1;
    std::vector<int> depth(loops.size(), 0);
    for (size_t i = 0; i < loops.size(); ++i) {
	for (size_t j = 0; j < loops.size(); ++j) {
	    if (i != j && inside(loops[i][0], loops[j], budget))
		++depth[i];
	}
	if (depth[i] > 1)
	    return;
	if (!depth[i]) {
	    if (root >= 0)
		return;
	    root = (int)i;
	}
    }
    if (root < 0 || budget.limited)
	return;
    /* A lone clockwise root can be a real hole with its outline missing.
     * Paired nested rings support an annulus; a lone ring needs a closed
     * source, so ambiguous outline candidates cannot turn into filled holes. */
    if (area(loops[(size_t)root]) < 0.0L && !supported_root)
	return;
    ON_BrepFace &face = brep.m_F[fi];
    for (size_t i = 0; i < loops.size(); ++i) {
	const bool outer = (int)i == root;
	ON_BrepLoop &loop = *face.Loop((int)i);
	const bool reversed = outer != (area(loops[i]) > 0.0L);
	if (reversed)
	    brep.FlipLoop(loop);
	const ON_BrepLoop::TYPE type = outer ? ON_BrepLoop::outer : ON_BrepLoop::inner;
	if (reversed || loop.m_type != type) {
	    loop.m_type = type;
	    ++result.loop_roles;
	    result.faces.insert(fi);
	    for (int ti = 0; ti < loop.TrimCount(); ++ti)
		result.edges.insert(loop.Trim(ti)->m_ei);
	}
    }
    brep.SortFaceLoops(face);
}

struct planar_cap {
    edge_walk walk;
    std::vector<std::unique_ptr<ON_Curve>> curves;
    std::unique_ptr<ON_PlaneSurface> surface;
    ON_BoundingBox bounds;
    double area_bound = 0.0;
    double maximum_deviation = 0.0;
};

bool
simple_corner_cap(const std::vector<std::unique_ptr<ON_Curve>> &curves,
	double tolerance, healing_budget &budget)
{
    /* Tangent joins cannot satisfy the sampled segment separation test:
     * arbitrarily short arc segments approach the adjoining straight side.
     * For two straight sides and one Bezier span, positive control weights,
     * half-plane containment, and a monotone chord projection instead prove
     * a simple boundary, up to projection roundoff. */
    if (budget.limited || curves.size() != 3)
	return false;
    ON_NurbsCurve nurbs[3];
    int curved = -1;
    double magnitude = 1.0;
    for (int i = 0; i < 3; ++i) {
	if (curves[i]->Degree() >= maximum_curve_order ||
	    curves[i]->SpanCount() != 1 || !curves[i]->GetNurbForm(nurbs[i]) ||
	    !nurbs[i].IsValid() || nurbs[i].CVCount() != nurbs[i].Order() ||
	    !budget.spend((size_t)nurbs[i].CVCount()))
	    return false;
	if (nurbs[i].Degree() > 1) {
	    if (curved >= 0)
		return false;
	    curved = i;
	}
	for (int cv = 0; cv < nurbs[i].CVCount(); ++cv) {
	    ON_3dPoint point;
	    if (!nurbs[i].GetCV(cv, point) || !point.IsValid() ||
		(nurbs[i].IsRational() && !(nurbs[i].Weight(cv) > 0.0)))
		return false;
	    magnitude = std::max(magnitude, std::max(std::abs(point.x), std::abs(point.y)));
	}
    }
    if (curved < 0)
	return false;
    /* Bound arithmetic noise separately from the caller's modeling tolerance.
     * A near crossing at the modeling tolerance is still ambiguous. */
    constexpr double projection_roundoff_factor = 256.0;
    const double roundoff = projection_roundoff_factor *
	std::numeric_limits<double>::epsilon() * magnitude;
    if (roundoff > tolerance * flatness_fraction)
	return false;
    for (int i = 0; i < 3; ++i)
	if (nurbs[i].PointAtEnd().DistanceTo(nurbs[(i + 1) % 3].PointAtStart()) > roundoff)
	    return false;
    const ON_2dPoint start(nurbs[curved].PointAtStart());
    const ON_2dPoint end(nurbs[curved].PointAtEnd());
    const ON_2dPoint corner(nurbs[(curved + 1) % 3].PointAtEnd());
    const ON_2dVector chord = end - start;
    const double chord_length = chord.Length();
    const double side_lengths[2] = {end.DistanceTo(corner), corner.DistanceTo(start)};
    const long double orientation = cross(end, corner, start);
    if (chord_length <= tolerance ||
	std::abs(orientation) <= tolerance * std::max(side_lengths[0], side_lengths[1]))
	return false;
    const double sign = orientation > 0.0L ? 1.0 : -1.0;
    ON_BezierCurve bezier;
    if (!nurbs[curved].ConvertSpanToBezier(0, bezier))
	return false;
    bool interior[2] = {false, false};
    double previous = 0.0;
    for (int i = 0; i < bezier.CVCount(); ++i) {
	ON_3dPoint point;
	if (!budget.spend() || !bezier.GetCV(i, point) || !point.IsValid() ||
	    (bezier.IsRational() && !(bezier.Weight(i) > 0.0)))
	    return false;
	const ON_2dPoint uv(point);
	const double projection = (uv - start) * chord;
	if (i && projection - previous <= roundoff * chord_length)
	    return false;
	previous = projection;
	const long double side[2] = {sign * cross(end, corner, uv),
	    sign * cross(corner, start, uv)};
	for (int j = 0; j < 2; ++j) {
	    const double error = roundoff * side_lengths[j];
	    if (side[j] < -error)
		return false;
	    interior[j] = interior[j] || side[j] > error;
	}
    }
    return interior[0] && interior[1];
}

bool
prepare_planar_cap(const ON_Brep &brep, double tolerance,
	healing_budget &budget, planar_cap &cap)
{
    edge_walk &walk = cap.walk;
    /* Sample only to select a plane.  Positive-weight curve hulls below must
     * then prove that the complete boundary lies within its tolerance. */
    ON_3dPoint origin = ON_3dPoint::UnsetPoint;
    ON_3dVector along = ON_3dVector::ZeroVector;
    ON_3dVector normal = ON_3dVector::ZeroVector;
    const int plane_seed_intervals = 16;
    for (const auto &step : walk) {
	const ON_BrepEdge &edge = brep.m_E[step.first];
	for (int sample = 0; sample <= plane_seed_intervals; ++sample) {
	    if (!budget.point())
		return false;
	    const ON_3dPoint point = edge.PointAt(edge.Domain().ParameterAt(
		(double)sample / plane_seed_intervals));
	    if (!point.IsValid())
		return false;
	    if (!origin.IsValid()) {
		origin = point;
	    } else if (along.Length() <= tolerance) {
		along = point - origin;
	    } else {
		normal = ON_CrossProduct(along, point - origin);
		if (normal.Length() > tolerance * along.Length() && normal.Unitize())
		    break;
		normal = ON_3dVector::ZeroVector;
	    }
	}
	if (!normal.IsZero())
	    break;
    }
    if (normal.IsZero())
	return false;
    planar_chart chart;
    chart.plane = ON_Plane(origin, normal);
    chart.to_plane = ON_Xform::IdentityTransformation;
    for (int axis = 0; axis < 3; ++axis) {
	chart.to_plane[0][axis] = chart.plane.xaxis[axis];
	chart.to_plane[1][axis] = chart.plane.yaxis[axis];
	chart.to_plane[2][axis] = chart.plane.zaxis[axis];
    }
    chart.to_plane[0][3] = -(chart.plane.xaxis * origin);
    chart.to_plane[1][3] = -(chart.plane.yaxis * origin);
    chart.to_plane[2][3] = -(chart.plane.zaxis * origin);
    const double flatness = tolerance * flatness_fraction;
    polygon outline;
    ON_BoundingBox bounds;
    double maximum_deviation = 0.0;
    for (const auto &step : walk) {
	const ON_BrepEdge &edge = brep.m_E[step.first];
	double deviation;
	polygon points;
	std::unique_ptr<ON_Curve> curve(edge.DuplicateCurve());
	if (!chart.edge_on_plane(edge, tolerance, deviation, budget) ||
	    !edge.GetBoundingBox(cap.bounds, cap.bounds.IsValid()) || !curve ||
	    (step.second && !curve->Reverse()) || !curve->Transform(chart.to_plane) ||
	    !curve->ChangeDimension(2) || !curve->GetBoundingBox(bounds, bounds.IsValid()) ||
	    !sample_curve(*curve, ON_Xform::IdentityTransformation, flatness, points, budget))
	    return false;
	maximum_deviation = std::max(maximum_deviation, deviation);
	if (!append_curve(outline, points, tolerance))
	    return false;
	cap.curves.push_back(std::move(curve));
    }
    if (!close_polygon(outline, tolerance) ||
	(!simple_boundaries({outline}, tolerance, budget) &&
	 !simple_corner_cap(cap.curves, tolerance, budget)))
	return false;
    const double polygon_area = (double)area(outline);
    if (!std::isfinite(polygon_area) || std::fabs(polygon_area) <= tolerance * tolerance)
	return false;
    if (polygon_area < 0.0) {
	std::reverse(walk.begin(), walk.end());
	for (auto &step : walk)
	    step.second = !step.second;
	std::reverse(cap.curves.begin(), cap.curves.end());
	for (auto &curve : cap.curves)
	    if (!curve->Reverse())
		return false;
    }
    /* Each curved segment stays inside its chord's flatness tube.  Summing
     * those tube areas gives a conservative bound on the cap area error. */
    double area_bound = std::fabs(polygon_area);
    for (size_t i = 0; i < outline.size(); ++i)
	area_bound += 2.0 * flatness * outline[i].DistanceTo(
	    outline[(i + 1) % outline.size()]) + ON_PI * flatness * flatness;
    if (!std::isfinite(area_bound))
	return false;

    cap.surface.reset(new ON_PlaneSurface(chart.plane));
    for (int axis = 0; axis < 2; ++axis) {
	const ON_Interval domain(bounds.m_min[axis] - tolerance,
	    bounds.m_max[axis] + tolerance);
	if (!cap.surface->SetDomain(axis, domain.Min(), domain.Max()) ||
	    !cap.surface->SetExtents(axis, domain, false))
	    return false;
    }
    for (int axis = 0; axis < 3; ++axis) {
	cap.bounds.m_min[axis] -= tolerance;
	cap.bounds.m_max[axis] += tolerance;
    }
    cap.area_bound = area_bound;
    cap.maximum_deviation = maximum_deviation;
    return true;
}

void
append_planar_cap(ON_Brep &brep, planar_cap &plan, double tolerance,
	cdt_healing &result)
{
    ON_BrepFace &cap = brep.NewFace(brep.AddSurface(plan.surface.release()));
    ON_BrepLoop &loop = brep.NewLoop(ON_BrepLoop::outer, cap);
    for (size_t i = 0; i < plan.walk.size(); ++i) {
	ON_BrepEdge &edge = brep.m_E[plan.walk[i].first];
	result.faces.insert(edge.Trim(0)->Face()->m_face_index);
	result.edges.insert(edge.m_edge_index);
	ON_BrepTrim &trim = brep.NewTrim(edge, plan.walk[i].second, loop,
	    brep.AddTrimCurve(plan.curves[i].release()));
	trim.m_tolerance[0] = trim.m_tolerance[1] = tolerance;
    }
    brep.SetTrimIsoFlags(loop);
    ++result.capped_loops;
    result.cap_area_bound += plan.area_bound;
    result.max_deviation = std::max(result.max_deviation, plan.maximum_deviation);
}

bool
cap_boundaries(ON_Brep &brep, double tolerance,
	healing_budget &budget, cdt_healing &result)
{
    if (brep.m_F.Count() < 2)
	return false;
    std::set<int> remaining;
    std::map<int, std::vector<int>> incident;
    for (int ei = 0; ei < brep.m_E.Count(); ++ei) {
	if (!budget.spend())
	    return false;
	const ON_BrepEdge &edge = brep.m_E[ei];
	if (edge.TrimCount() != 1)
	    continue;
	remaining.insert(ei);
	incident[edge.m_vi[0]].push_back(ei);
	incident[edge.m_vi[1]].push_back(ei);
    }
    std::vector<planar_cap> caps;
    while (!remaining.empty()) {
	planar_cap cap;
	if (!trace_boundary_cycle(brep, remaining, incident, cap.walk, budget) ||
	    !prepare_planar_cap(brep, tolerance, budget, cap))
	    return false;
	/* A filled planar region lies inside its boundary's bounding box.
	 * Disjoint boxes exclude overlapping caps and nested coplanar loops
	 * which would need an annular interpretation instead of two disks. */
	for (const auto &other : caps) {
	    if (!budget.spend() || !cap.bounds.IsDisjoint(other.bounds))
		return false;
	}
	caps.push_back(std::move(cap));
    }
    if (!budget.spend(caps.size()))
	return false;
    for (auto &cap : caps)
	append_planar_cap(brep, cap, tolerance, result);
    if (!caps.empty())
	brep.SetTrimTypeFlags(false);
    return !caps.empty();
}


bool
orient_faces(ON_Brep &brep, healing_budget &budget, cdt_healing &result)
{
    std::vector<std::vector<std::pair<int, bool>>> neighbors((size_t)brep.m_F.Count());
    for (int ei = 0; ei < brep.m_E.Count(); ++ei) {
	const ON_BrepEdge &edge = brep.m_E[ei];
	if (edge.TrimCount() != 2)
	    continue;
	const ON_BrepTrim &a = *edge.Trim(0);
	const ON_BrepTrim &b = *edge.Trim(1);
	const int fa = a.Face()->m_face_index;
	const int fb = b.Face()->m_face_index;
	const bool same = (a.m_bRev3d != a.Face()->m_bRev) ==
	    (b.m_bRev3d != b.Face()->m_bRev);
	neighbors[(size_t)fa].emplace_back(fb, same);
	neighbors[(size_t)fb].emplace_back(fa, same);
    }
    std::vector<int> flipped(neighbors.size(), -1);
    for (size_t root = 0; root < neighbors.size(); ++root) {
	if (flipped[root] >= 0)
	    continue;
	flipped[root] = 0;
	std::vector<size_t> queue(1, root);
	for (size_t i = 0; i < queue.size(); ++i) {
	    const size_t face = queue[i];
	    for (const auto &next : neighbors[face]) {
		if (!budget.spend())
		    return false;
		const int expected = flipped[face] ^ next.second;
		if (flipped[(size_t)next.first] >= 0) {
		    if (flipped[(size_t)next.first] != expected)
			return false;
		} else {
		    flipped[(size_t)next.first] = expected;
		    queue.push_back((size_t)next.first);
		}
	    }
	}
	const size_t changes = (size_t)std::count_if(queue.begin(), queue.end(),
	    [&](size_t fi) { return flipped[fi] != 0; });
	if (changes > queue.size() / 2)
	    for (size_t fi : queue) flipped[fi] ^= 1;
    }
    for (size_t fi = 0; fi < flipped.size(); ++fi) {
	if (flipped[fi]) {
	    brep.FlipFace(brep.m_F[(int)fi]);
	    result.faces.insert((int)fi);
	    ++result.orientations;
	}
    }
    return true;
}

}

extern "C" int
cdt_test_planar_cap_hulls(void)
{
    /* A quarter-circle tangent to both legs bounds a small fillet end. */
    std::vector<std::unique_ptr<ON_Curve>> curves;
    auto arc = std::unique_ptr<ON_NurbsCurve>(new ON_NurbsCurve(2, true, 3, 3));
    const double weight = std::sqrt(0.5);
    if (!arc->SetCV(0, ON_4dPoint(1, 0, 0, 1)) ||
	!arc->SetCV(1, ON_4dPoint(0, 0, 0, weight)) ||
	!arc->SetCV(2, ON_4dPoint(0, 1, 0, 1)))
	return 1;
    for (int i = 0; i < arc->KnotCount(); ++i)
	if (!arc->SetKnot(i, i < 2 ? 0.0 : 1.0))
	    return 1;
    ON_NurbsCurve *curved = arc.get();
    curves.push_back(std::move(arc));
    curves.emplace_back(new ON_LineCurve(ON_2dPoint(0, 1), ON_2dPoint(0, 0)));
    curves.emplace_back(new ON_LineCurve(ON_2dPoint(0, 0), ON_2dPoint(1, 0)));
    const double tolerance = BN_TOL_DIST;
    healing_budget budget = {65536, 65536 * operations_per_point,
	bu_gettime() + 5000000};
    polygon outline;
    for (const auto &curve : curves) {
	polygon points;
	if (!sample_curve(*curve, ON_Xform::IdentityTransformation,
		tolerance * flatness_fraction, points, budget) ||
	    !append_curve(outline, points, tolerance))
	    return 1;
    }
    if (!close_polygon(outline, tolerance) ||
	simple_boundaries({outline}, tolerance, budget) ||
	!simple_corner_cap(curves, tolerance, budget))
	return 1;
    for (int rotation = 0; rotation < 3; ++rotation) {
	std::rotate(curves.begin(), curves.begin() + 1, curves.end());
	if (!simple_corner_cap(curves, tolerance, budget))
	    return 1;
    }
    std::reverse(curves.begin(), curves.end());
    for (auto &curve : curves)
	if (!curve->Reverse())
	    return 1;
    if (!simple_corner_cap(curves, tolerance, budget))
	return 1;
    /* A control point across a straight side makes the return curve cross
     * that side near its endpoint.  Even a sub-tolerance excursion fails. */
    if (!curved->SetCV(1, ON_4dPoint(-tolerance * weight / 2.0, 0, 0, weight)) ||
	simple_corner_cap(curves, tolerance, budget))
	return 1;
    if (!curved->SetCV(1, ON_4dPoint(0, 0, 0, -weight)) ||
	simple_corner_cap(curves, tolerance, budget))
	return 1;
    /* A returning chord projection does not prove the curve is injective. */
    if (!curved->SetCV(1, ON_4dPoint(2 * weight, 0, 0, weight)) ||
	simple_corner_cap(curves, tolerance, budget))
	return 1;
    if (!curved->SetCV(1, ON_4dPoint(0, 0, 0, weight)))
	return 1;
    healing_budget exhausted = {0, 0, bu_gettime() + 5000000};
    return simple_corner_cap(curves, tolerance, exhausted) || !exhausted.limited;
}

bool
cdt_topology_references_safe(const ON_Brep *brep, std::string *reason,
	bool require_paired_edges)
{
    const auto fail = [reason](const char *message) {
	if (reason)
	    *reason = message;
	return false;
    };
    if (!brep || brep->m_F.Count() <= 0 || brep->m_V.Count() <= 0)
	return fail("missing faces or vertices");
    std::vector<int> loop_owners((size_t)brep->m_L.Count(), 0);
    std::vector<int> trim_owners((size_t)brep->m_T.Count(), 0);
    std::vector<int> trim_edges((size_t)brep->m_T.Count(), 0);
    for (int vi = 0; vi < brep->m_V.Count(); ++vi) {
	const ON_BrepVertex &vertex = brep->m_V[vi];
	/* Unattached vertex records do not participate in tessellation. */
	if (!vertex.m_ei.Count())
	    continue;
	if (vertex.m_vertex_index != vi || !vertex.point.IsValid())
	    return fail("invalid vertex or unstable indexing");
	for (int i = 0; i < vertex.m_ei.Count(); ++i) {
	    const int ei = vertex.m_ei[i];
	    if (ei < 0 || ei >= brep->m_E.Count() ||
		(brep->m_E[ei].m_vi[0] != vi && brep->m_E[ei].m_vi[1] != vi))
		return fail("invalid vertex edge reference");
	}
    }
    for (int ei = 0; ei < brep->m_E.Count(); ++ei) {
	const ON_BrepEdge &edge = brep->m_E[ei];
	if (edge.m_edge_index != ei ||
	    (require_paired_edges ? edge.TrimCount() != 2 : edge.TrimCount() > 2) ||
	    edge.m_c3i < 0 || edge.m_c3i >= brep->m_C3.Count() ||
	    !edge.EdgeCurveOf() || !edge.Domain().IsIncreasing())
	    return fail("edge lacks supported trims, a curve, or stable indexing");
	for (int end = 0; end < 2; ++end) {
	    const int vi = edge.m_vi[end];
	    if (vi < 0 || vi >= brep->m_V.Count() || edge.Vertex(end) != &brep->m_V[vi] ||
		brep->m_V[vi].m_vertex_index != vi || !brep->m_V[vi].point.IsValid() ||
		brep->m_V[vi].m_ei.Search(ei) < 0)
		return fail("invalid edge endpoint reference");
	}
	for (int i = 0; i < edge.TrimCount(); ++i) {
	    const int ti = edge.m_ti[i];
	    if (ti < 0 || ti >= brep->m_T.Count() || brep->m_T[ti].m_ei != ei ||
		++trim_edges[(size_t)ti] != 1)
		return fail("invalid or duplicate edge trim reference");
	}
    }
    for (int fi = 0; fi < brep->m_F.Count(); ++fi) {
	const ON_BrepFace &face = brep->m_F[fi];
	if (face.m_face_index != fi || face.m_si < 0 || face.m_si >= brep->m_S.Count() ||
	    !face.SurfaceOf() || face.LoopCount() <= 0)
	    return fail("face lacks a surface, trim loops, or stable indexing");
	for (int i = 0; i < face.LoopCount(); ++i) {
	    const int li = face.m_li[i];
	    if (li < 0 || li >= brep->m_L.Count() || ++loop_owners[(size_t)li] != 1)
		return fail("invalid or duplicate face loop reference");
	    const ON_BrepLoop &loop = brep->m_L[li];
	    if (loop.m_loop_index != li || loop.m_fi != fi || loop.Face() != &face ||
		loop.TrimCount() <= 0)
		return fail("invalid loop face or trim references");
	    for (int j = 0; j < loop.TrimCount(); ++j) {
		const int ti = loop.m_ti[j];
		if (ti < 0 || ti >= brep->m_T.Count() || ++trim_owners[(size_t)ti] != 1)
		    return fail("invalid or duplicate loop trim reference");
		const ON_BrepTrim &trim = brep->m_T[ti];
		if (trim.m_trim_index != ti || trim.m_li != li || trim.Loop() != &loop ||
		    trim.m_c2i < 0 || trim.m_c2i >= brep->m_C2.Count() ||
		    !trim.TrimCurveOf() || !trim.Domain().IsIncreasing() ||
		    trim.m_vi[0] < 0 || trim.m_vi[0] >= brep->m_V.Count() ||
		    trim.m_vi[1] < 0 || trim.m_vi[1] >= brep->m_V.Count())
		    return fail("invalid trim curve, loop, or vertex reference");
		if (trim.m_type == ON_BrepTrim::singular) {
		    if (trim.m_ei != -1 || trim.m_vi[0] != trim.m_vi[1] || trim_edges[(size_t)ti])
			return fail("invalid singular trim references");
		    const ON_BrepVertex &vertex = brep->m_V[trim.m_vi[0]];
		    if (vertex.m_vertex_index != trim.m_vi[0] || !vertex.point.IsValid())
			return fail("invalid singular trim vertex");
		} else {
		    if (trim.m_ei < 0 || trim.m_ei >= brep->m_E.Count() || trim_edges[(size_t)ti] != 1)
			return fail("invalid trim edge reference");
		    const ON_BrepEdge &edge = brep->m_E[trim.m_ei];
		    if (trim.m_vi[0] != edge.m_vi[trim.m_bRev3d ? 1 : 0] ||
			trim.m_vi[1] != edge.m_vi[trim.m_bRev3d ? 0 : 1])
			return fail("inconsistent trim and edge endpoints");
		}
	    }
	}
    }
    if (std::find(loop_owners.begin(), loop_owners.end(), 0) != loop_owners.end() ||
	std::find(trim_owners.begin(), trim_owners.end(), 0) != trim_owners.end())
	return fail("unreferenced loops or trims");
    return true;
}

bool
cdt_heal_topology(const ON_Brep &source, double tolerance,
	size_t max_points, size_t max_bytes, long max_time_ms, cdt_healing &result,
	bool cap_boundary)
{
    if (!source.m_F.Count() || !(tolerance > 0.0) || !std::isfinite(tolerance) ||
	!max_points || !max_bytes || max_time_ms <= 0)
	return false;
    if (source.SizeOf() > max_bytes / 2) {
	result.limited = true;
	return false;
    }
    if (!cdt_topology_references_safe(&source, NULL, false))
	return false;
    const size_t point_budget = std::min(max_points,
	max_bytes / (sizeof(segment) * segment_storage_factor));
    const size_t operation_budget = point_budget > SIZE_MAX / operations_per_point ?
	SIZE_MAX : point_budget * operations_per_point;
    const int64_t now = bu_gettime();
    const int64_t duration = std::min((int64_t)max_time_ms,
	(INT64_MAX - now) / 1000) * 1000;
    healing_budget budget = {point_budget, operation_budget, now + duration};
    std::unique_ptr<ON_Brep> candidate(new ON_Brep(source));
    bool closed_source = true;
    for (int ei = 0; ei < candidate->m_E.Count(); ++ei)
	closed_source = closed_source && candidate->m_E[ei].TrimCount() != 1;
    for (int fi = 0; fi < candidate->m_F.Count(); ++fi) {
	if (!budget.spend())
	    break;
	const ON_BrepFace &face = candidate->m_F[fi];
	bool all_inner = face.LoopCount() > 0;
	for (int li = 0; li < face.LoopCount(); ++li)
	    all_inner = all_inner && face.Loop(li)->m_type == ON_BrepLoop::inner;
	planar_chart chart;
	if (!all_inner || !chart.init(face))
	    continue;
	std::vector<polygon> loops((size_t)face.LoopCount());
	bool usable = true;
	for (int li = 0; li < face.LoopCount(); ++li) {
	    if (!sample_loop(*face.Loop(li), chart, tolerance, loops[(size_t)li], budget)) {
		usable = false;
		break;
	    }
	}
	if (!usable || !simple_boundaries(loops, tolerance, budget))
	    continue;
	bool disjoint_holes = true;
	for (size_t i = 0; i < loops.size(); ++i) {
	    disjoint_holes = disjoint_holes && area(loops[i]) < 0.0L;
	    for (size_t j = 0; j < loops.size(); ++j)
		if (i != j && inside(loops[i][0], loops[j], budget)) disjoint_holes = false;
	}
	if (disjoint_holes && restore_outer(*candidate, fi, chart, loops,
	    tolerance, budget, result))
	    continue;
	normalize_roles(*candidate, fi, loops,
	    closed_source || (loops.size() > 1 && paired_boundaries(face)), budget, result);
    }
    if (cap_boundary && !budget.limited)
	cap_boundaries(*candidate, tolerance, budget, result);
    if (budget.limited || !orient_faces(*candidate, budget, result)) {
	result.limited = budget.limited;
	return false;
    }
    for (int ei = 0; ei < candidate->m_E.Count(); ++ei) {
	if (!candidate->m_E[ei].TrimCount()) {
	    result.edges.insert(ei);
	    candidate->DeleteEdge(candidate->m_E[ei], true);
	    ++result.unused_edges;
	} else {
	    result.original_edges.push_back(ei);
	}
    }
    if (!result.faces.size() && !result.unused_edges)
	return false;
    candidate->SetTrimBoundingBoxes(false);
    if (result.unused_edges) {
	if (!candidate->Compact() ||
	    candidate->m_F.Count() != source.m_F.Count() + result.capped_loops ||
	    candidate->m_E.Count() != (int)result.original_edges.size())
	    return false;
    }
    if (!budget.spend()) {
	result.limited = true;
	return false;
    }
    result.brep = std::move(candidate);
    return true;
}

/*
 * Local Variables:
 * tab-width: 8
 * mode: C++
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
