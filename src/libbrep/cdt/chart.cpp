/*                      C H A R T . C P P
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by the
 * U.S. Army Research Laboratory.
 *
 * Distributed under the terms of the GNU Lesser General Public License
 * (LGPL), version 2.1.
 */

#include "common.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <set>

#include "brep/util.h"
#include "chart.h"

static const double CDT_CONIC_TOLERANCE = 0.05;

static bool
cone_chart_properties(const ON_BrepFace &face, int *closed_dir,
	int *open_dir, int *singular_side, int *pole_vertex)
{
    const ON_Surface *surface = face.SurfaceOf();
    if (!surface)
	return false;
    ON_Cone cone;
    if (!surface->IsCone(&cone, CDT_CONIC_TOLERANCE))
	return false;

    int side = -1;
    int singular_count = 0;
    for (int candidate = 0; candidate < 4; ++candidate) {
	if (!surface->IsSingular(candidate))
	    continue;
	side = candidate;
	singular_count++;
    }
    if (singular_count != 1)
	return false;

    const int odir = (side == 0 || side == 2) ? 1 : 0;
    const int cdir = 1 - odir;
    if (!surface->IsClosed(cdir) || surface->IsClosed(odir))
	return false;

    const ON_BrepLoop *outer_loop = face.OuterLoop();
    if (!outer_loop)
	return false;
    int singular_trim_count = 0;
    int vertex = -1;
    ON_Surface::ISO expected_iso = ON_Surface::not_iso;
    switch (side) {
	case 0: expected_iso = ON_Surface::S_iso; break;
	case 1: expected_iso = ON_Surface::E_iso; break;
	case 2: expected_iso = ON_Surface::N_iso; break;
	case 3: expected_iso = ON_Surface::W_iso; break;
    }
    for (int ti = 0; ti < outer_loop->TrimCount(); ++ti) {
	const ON_BrepTrim *trim = outer_loop->Trim(ti);
	if (!trim || trim->m_type != ON_BrepTrim::singular)
	    continue;
	if (trim->m_iso != expected_iso ||
		trim->m_vi[0] < 0 || trim->m_vi[0] != trim->m_vi[1])
	    return false;
	vertex = trim->m_vi[0];
	singular_trim_count++;
    }
    if (singular_trim_count != 1)
	return false;

    if (closed_dir)
	*closed_dir = cdir;
    if (open_dir)
	*open_dir = odir;
    if (singular_side)
	*singular_side = side;
    if (pole_vertex)
	*pole_vertex = vertex;
    return true;
}

bool
cdt_face_uses_cone_chart(const ON_BrepFace &face)
{
    return cone_chart_properties(face, NULL, NULL, NULL, NULL);
}

static bool
normalize_ring(const std::vector<int> &source, size_t point_count,
	std::vector<int> &normalized)
{
    normalized.clear();
    for (int point : source) {
	if (point < 0 || (size_t)point >= point_count)
	    return false;
	if (normalized.empty() || normalized.back() != point)
	    normalized.push_back(point);
    }
    if (normalized.size() > 1 && normalized.front() == normalized.back())
	normalized.pop_back();
    const std::set<int> unique(normalized.begin(), normalized.end());
    return normalized.size() >= 3 && unique.size() == normalized.size();
}

static double
parameter_tolerance(const ON_Interval &domain)
{
    const double magnitude = std::max(std::fabs(domain.Min()),
	std::fabs(domain.Max()));
    return 256.0 * std::numeric_limits<double>::epsilon() *
	std::max(magnitude, domain.Length());
}

bool
cdt_face_chart::native_to_chart(const ON_2dPoint &native_uv,
	ON_2dPoint &chart_uv) const
{
    if (!native_uv.IsValid())
	return false;
    if (m_type == CDT_FACE_CHART_NATIVE_UV) {
	chart_uv = native_uv;
	return true;
    }
    if (m_type != CDT_FACE_CHART_CONE_WEDGE || m_closed_dir < 0 ||
	    m_open_dir < 0)
	return false;
    const double period = m_closed_domain.Length();
    const double open_length = m_open_domain.Length();
    if (!(period > 0.0) || !(open_length > 0.0))
	return false;
    const bool low_pole = m_singular_side == 0 || m_singular_side == 3;
    const double pole = low_pole ? m_open_domain.Min() :
	m_open_domain.Max();
    double radial = low_pole ?
	(native_uv[m_open_dir] - pole) / open_length :
	(pole - native_uv[m_open_dir]) / open_length;
    const double tolerance = parameter_tolerance(m_open_domain) /
	open_length;
    if (radial < -tolerance || radial > 1.0 + tolerance)
	return false;
    radial = std::max(0.0, std::min(1.0, radial));
    const double center = m_closed_domain.Mid();
    chart_uv.x = radial * (native_uv[m_closed_dir] - center) / period;
    chart_uv.y = radial;
    return chart_uv.IsValid();
}

bool
cdt_face_chart::chart_to_native(const ON_2dPoint &chart_uv,
	ON_2dPoint &native_uv) const
{
    if (!chart_uv.IsValid())
	return false;
    if (m_type == CDT_FACE_CHART_NATIVE_UV) {
	native_uv = chart_uv;
	return true;
    }
    if (m_type != CDT_FACE_CHART_CONE_WEDGE || m_closed_dir < 0 ||
	    m_open_dir < 0)
	return false;
    const double radial = chart_uv.y;
    const double tolerance = 256.0 *
	std::numeric_limits<double>::epsilon();
    if (radial < -tolerance || radial > 1.0 + tolerance)
	return false;
    const bool low_pole = m_singular_side == 0 || m_singular_side == 3;
    const double pole = low_pole ? m_open_domain.Min() :
	m_open_domain.Max();
    native_uv[m_open_dir] = low_pole ?
	pole + radial * m_open_domain.Length() :
	pole - radial * m_open_domain.Length();
    native_uv[m_closed_dir] = radial > tolerance ?
	m_closed_domain.Mid() + chart_uv.x *
	    m_closed_domain.Length() / radial :
	m_closed_domain.Mid();
    return native_uv.IsValid();
}

long
cdt_face_chart::native_point(int chart_point) const
{
    if (chart_point < 0 || (size_t)chart_point >= vertices.size())
	return -1;
    return vertices[(size_t)chart_point].native_point;
}

bool
cdt_face_chart::build_native(const ON_BrepFace &face,
	const std::vector<std::pair<double, double>> &native_points,
	const std::vector<int> &source_outer,
	const std::vector<std::vector<int>> &source_holes,
	const std::vector<int> &source_steiner,
	const std::vector<cdt_topo_vertex_id> &topology_vertices)
{
    m_type = CDT_FACE_CHART_NATIVE_UV;
    points = native_points;
    vertices.resize(points.size());
    for (size_t i = 0; i < points.size(); ++i) {
	vertices[i].id = (cdt_chart_vertex_id)i;
	vertices[i].native_point = (long)i;
	vertices[i].topo_vertex = topology_vertices[i];
    }
    if (!normalize_ring(source_outer, points.size(), outer)) {
	m_failure = "invalid native-UV chart outline";
	return false;
    }
    holes.resize(source_holes.size());
    for (size_t hi = 0; hi < source_holes.size(); ++hi) {
	if (!normalize_ring(source_holes[hi], points.size(), holes[hi])) {
	    m_failure = "invalid native-UV chart hole";
	    return false;
	}
    }
    std::set<int> unique_steiner;
    for (int point : source_steiner) {
	if (point < 0 || (size_t)point >= points.size()) {
	    m_failure = "invalid native-UV chart Steiner point";
	    return false;
	}
	if (unique_steiner.insert(point).second)
	    steiner.push_back(point);
    }
    return validate_boundary(face, native_points);
}

bool
cdt_face_chart::build_cone(const ON_BrepFace &face,
	const std::vector<std::pair<double, double>> &native_points,
	const std::vector<int> &source_outer,
	const std::vector<std::vector<int>> &source_holes,
	const std::vector<int> &source_steiner,
	const std::vector<const ON_3dPoint *> &points_3d,
	const std::vector<cdt_topo_vertex_id> &topology_vertices)
{
    m_type = CDT_FACE_CHART_CONE_WEDGE;
    const double pole_coordinate =
	(m_singular_side == 0 || m_singular_side == 3) ?
	m_open_domain.Min() : m_open_domain.Max();
    const double pole_tolerance = parameter_tolerance(m_open_domain);
    std::vector<int> source_to_chart(native_points.size(), -1);
    size_t pole_source = native_points.size();
    for (size_t i = 0; i < topology_vertices.size(); ++i) {
	if (topology_vertices[i] == m_pole_topology_vertex) {
	    pole_source = i;
	    break;
	}
    }
    if (pole_source == native_points.size()) {
	for (size_t i = 0; i < native_points.size(); ++i) {
	    const ON_2dPoint native_uv(native_points[i].first,
		native_points[i].second);
	    if (std::fabs(native_uv[m_open_dir] - pole_coordinate) <=
		    pole_tolerance) {
		pole_source = i;
		break;
	    }
	}
    }
    if (pole_source == native_points.size()) {
	m_failure = "cone chart has no apex sample";
	return false;
    }

    const ON_2dPoint pole_chart_uv(0.0, 0.0);
    points.push_back(std::make_pair(pole_chart_uv.x, pole_chart_uv.y));
    cdt_chart_vertex pole_vertex;
    pole_vertex.id = 0;
    pole_vertex.native_point = (long)pole_source;
    pole_vertex.topo_vertex = m_pole_topology_vertex;
    pole_vertex.singular = true;
    vertices.push_back(pole_vertex);
    const int apex = 0;
    source_to_chart[pole_source] = apex;

    for (size_t i = 0; i < native_points.size(); ++i) {
	if (i == pole_source)
	    continue;
	const ON_2dPoint native_uv(native_points[i].first,
	    native_points[i].second);
	const bool at_pole = topology_vertices[i] ==
	    m_pole_topology_vertex ||
	    std::fabs(native_uv[m_open_dir] - pole_coordinate) <=
	    pole_tolerance;
	if (at_pole) {
	    source_to_chart[i] = apex;
	    continue;
	}
	ON_2dPoint chart_uv;
	if (!native_to_chart(native_uv, chart_uv)) {
	    m_failure = "cone point lies outside the chart domain";
	    return false;
	}
	const int chart_index = (int)points.size();
	points.push_back(std::make_pair(chart_uv.x, chart_uv.y));
	cdt_chart_vertex vertex;
	vertex.id = chart_index;
	vertex.native_point = (long)i;
	vertex.topo_vertex = topology_vertices[i];
	const double seam_tolerance = parameter_tolerance(m_closed_domain);
	if (!at_pole && std::fabs(native_uv[m_closed_dir] -
		m_closed_domain.Min()) <= seam_tolerance)
	    vertex.seam_side = -1;
	else if (!at_pole && std::fabs(native_uv[m_closed_dir] -
		m_closed_domain.Max()) <= seam_tolerance)
	    vertex.seam_side = 1;
	vertices.push_back(vertex);
	source_to_chart[i] = chart_index;
    }

    auto remap_ring = [&](const std::vector<int> &source,
	    std::vector<int> &target) {
	target.clear();
	for (int point : source) {
	    if (point < 0 || (size_t)point >= source_to_chart.size())
		return false;
	    const int mapped = source_to_chart[(size_t)point];
	    if (mapped < 0)
		return false;
	    if (target.empty() || target.back() != mapped)
		target.push_back(mapped);
	}
	if (target.size() > 1 && target.front() == target.back())
	    target.pop_back();
	const std::set<int> unique(target.begin(), target.end());
	return target.size() >= 3 && unique.size() == target.size();
    };
    if (!remap_ring(source_outer, outer)) {
	m_failure = "cone chart outline does not form one disk boundary";
	return false;
    }
    holes.resize(source_holes.size());
    for (size_t hi = 0; hi < source_holes.size(); ++hi) {
	if (!remap_ring(source_holes[hi], holes[hi])) {
	    m_failure = "cone chart hole does not form a simple boundary";
	    return false;
	}
    }

    for (int point : source_steiner) {
	if (point < 0 || (size_t)point >= source_to_chart.size()) {
	    m_failure = "invalid cone chart Steiner point";
	    return false;
	}
    }

    /* A cone is ruled from its apex to the opposite boundary.  Once that
     * boundary has been sampled to the requested curve tolerance, its chart
     * fan supplies the needed surface approximation.  Native-UV interior
     * samples can make planar Delaunay connect the apex to samples separated
     * by half a revolution, producing an axial cross-section rather than a
     * surface triangle.  Do not offer those samples to the cone prototype;
     * later metric refinement can add chart-aware samples transactionally. */
    steiner.clear();

    const double radial_tolerance = 256.0 *
	std::numeric_limits<double>::epsilon();
    for (int point : outer) {
	if (point == apex || vertices[(size_t)point].seam_side ||
		std::fabs(points[(size_t)point].second - 1.0) >
		radial_tolerance)
	    continue;
	constraints.push_back(std::make_pair(apex, point));
    }

    /* Give matching seam copies an explicit stable sample identity.  The
     * shared 3-D point pointer is only an input used to discover the current
     * edge sample; the resulting ID is its deterministic open-coordinate
     * rank and is what chart consumers retain. */
    std::map<const ON_3dPoint *, std::vector<size_t>> seam_copies;
    for (size_t i = 0; i < vertices.size(); ++i) {
	const cdt_chart_vertex &vertex = vertices[i];
	if (!vertex.seam_side || vertex.native_point < 0 ||
		(size_t)vertex.native_point >= points_3d.size() ||
		!points_3d[(size_t)vertex.native_point])
	    continue;
	seam_copies[points_3d[(size_t)vertex.native_point]].push_back(i);
    }
    struct ordered_sample {
	double parameter;
	const ON_3dPoint *point;
	long native_index;
    };
    std::vector<ordered_sample> ordered_samples;
    for (const auto &copies : seam_copies) {
	bool low = false;
	bool high = false;
	for (size_t chart_index : copies.second) {
	    low = low || vertices[chart_index].seam_side < 0;
	    high = high || vertices[chart_index].seam_side > 0;
	}
	if (!low || !high)
	    continue;
	const long native_index = vertices[copies.second[0]].native_point;
	const double parameter = m_open_dir == 0 ?
	    native_points[(size_t)native_index].first :
	    native_points[(size_t)native_index].second;
	ordered_samples.push_back({parameter, copies.first, native_index});
    }
    std::sort(ordered_samples.begin(), ordered_samples.end(),
	[](const auto &a, const auto &b) {
	    if (a.parameter < b.parameter)
		return a.parameter < b.parameter;
	    if (b.parameter < a.parameter)
		return false;
	    if (a.point->x < b.point->x)
		return a.point->x < b.point->x;
	    if (b.point->x < a.point->x)
		return false;
	    if (a.point->y < b.point->y)
		return a.point->y < b.point->y;
	    if (b.point->y < a.point->y)
		return false;
	    if (a.point->z < b.point->z)
		return a.point->z < b.point->z;
	    if (b.point->z < a.point->z)
		return false;
	    return a.native_index < b.native_index;
	});
    for (size_t sample = 0; sample < ordered_samples.size(); ++sample) {
	for (size_t chart_index : seam_copies[ordered_samples[sample].point])
	    vertices[chart_index].edge_sample = (cdt_edge_sample_id)sample;
    }
    return validate_boundary(face, native_points);
}

bool
cdt_face_chart::validate_boundary(const ON_BrepFace &face,
	const std::vector<std::pair<double, double>> &native_points)
{
    const ON_Surface *surface = face.SurfaceOf();
    if (!surface) {
	m_failure = "chart has no source surface";
	return false;
    }
    auto collapsed_edge = [&](int first, int second) {
	if (first < 0 || second < 0 || (size_t)first >= vertices.size() ||
		(size_t)second >= vertices.size() || first == second)
	    return true;
	const long native_first = vertices[(size_t)first].native_point;
	const long native_second = vertices[(size_t)second].native_point;
	if (native_first < 0 || native_second < 0 ||
		(size_t)native_first >= native_points.size() ||
		(size_t)native_second >= native_points.size())
	    return true;
	const ON_2dPoint first_uv(native_points[(size_t)native_first].first,
	    native_points[(size_t)native_first].second);
	const ON_2dPoint second_uv(native_points[(size_t)native_second].first,
	    native_points[(size_t)native_second].second);
	for (int side = 0; side < 4; ++side) {
	    if (!surface->IsSingular(side))
		continue;
	    const int direction = (side == 0 || side == 2) ? 1 : 0;
	    const ON_Interval domain = surface->Domain(direction);
	    const double boundary = (side == 0 || side == 3) ?
		domain.Min() : domain.Max();
	    const double tolerance = parameter_tolerance(domain);
	    if (std::fabs(first_uv[direction] - boundary) <= tolerance &&
		    std::fabs(second_uv[direction] - boundary) <= tolerance)
		return true;
	}
	return false;
    };
    auto ring_valid = [&](const std::vector<int> &ring) {
	if (ring.size() < 3)
	    return false;
	for (size_t i = 0; i < ring.size(); ++i) {
	    const int first = ring[i];
	    const int second = ring[(i + 1) % ring.size()];
	    if (collapsed_edge(first, second))
		return false;
	}
	return true;
    };
    if (!ring_valid(outer)) {
	m_failure = "chart outline contains a collapsed surface constraint";
	return false;
    }
    for (const std::vector<int> &hole : holes) {
	if (!ring_valid(hole)) {
	    m_failure = "chart hole contains a collapsed surface constraint";
	    return false;
	}
    }
    return true;
}

bool
cdt_face_chart::build(const ON_BrepFace &face,
	const std::vector<std::pair<double, double>> &native_points,
	const std::vector<int> &source_outer,
	const std::vector<std::vector<int>> &source_holes,
	const std::vector<int> &source_steiner,
	const std::vector<const ON_3dPoint *> &points_3d,
	const std::vector<cdt_topo_vertex_id> &topology_vertices)
{
    points.clear();
    vertices.clear();
    outer.clear();
    holes.clear();
    steiner.clear();
    constraints.clear();
    m_failure.clear();
    m_id = face.m_face_index;
    m_type = CDT_FACE_CHART_NATIVE_UV;
    m_closed_dir = -1;
    m_open_dir = -1;
    m_singular_side = -1;
    m_pole_topology_vertex = CDT_TOPOLOGY_ID_NONE;

    if (topology_vertices.size() != native_points.size()) {
	m_failure = "chart topology metadata does not match its point array";
	return false;
    }

    int pole_vertex = -1;
    if (!cone_chart_properties(face, &m_closed_dir, &m_open_dir,
	    &m_singular_side, &pole_vertex))
	return build_native(face, native_points, source_outer, source_holes,
	    source_steiner, topology_vertices);
    m_closed_domain = face.SurfaceOf()->Domain(m_closed_dir);
    m_open_domain = face.SurfaceOf()->Domain(m_open_dir);
    m_pole_topology_vertex = pole_vertex;
    return build_cone(face, native_points, source_outer, source_holes,
	source_steiner, points_3d, topology_vertices);
}
