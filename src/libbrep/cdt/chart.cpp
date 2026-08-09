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
	int *open_dir, int *singular_side, int *pole_vertex, bool *periodic)
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
    if (surface->IsClosed(odir))
	return false;

    const ON_BrepLoop *outer_loop = face.OuterLoop();
    if (!outer_loop)
	return false;
    int singular_trim_count = 0;
    int vertex = -1;
    std::set<int> pole_vertices;
    ON_Surface::ISO expected_iso = ON_Surface::not_iso;
    switch (side) {
	case 0: expected_iso = ON_Surface::S_iso; break;
	case 1: expected_iso = ON_Surface::E_iso; break;
	case 2: expected_iso = ON_Surface::N_iso; break;
	case 3: expected_iso = ON_Surface::W_iso; break;
    }
    for (int ti = 0; ti < outer_loop->TrimCount(); ++ti) {
	const ON_BrepTrim *trim = outer_loop->Trim(ti);
	if (!trim)
	    continue;
	const ON_Interval trim_domain = trim->Domain();
	const ON_2dPoint trim_start = trim->PointAt(trim_domain.Min());
	const ON_2dPoint trim_end = trim->PointAt(trim_domain.Max());
	const ON_Interval open_domain = surface->Domain(odir);
	const double pole_parameter = (side == 0 || side == 3) ?
	    open_domain.Min() : open_domain.Max();
	const double magnitude = std::max(std::fabs(open_domain.Min()),
	    std::fabs(open_domain.Max()));
	const double pole_tolerance = 256.0 *
	    std::numeric_limits<double>::epsilon() *
	    std::max(magnitude, open_domain.Length());
	if (trim->m_vi[0] >= 0 &&
		std::fabs(trim_start[odir] - pole_parameter) <=
		pole_tolerance)
	    pole_vertices.insert(trim->m_vi[0]);
	if (trim->m_vi[1] >= 0 &&
		std::fabs(trim_end[odir] - pole_parameter) <= pole_tolerance)
	    pole_vertices.insert(trim->m_vi[1]);
	if (trim->m_type == ON_BrepTrim::singular) {
	    if (trim->m_iso != expected_iso || trim->m_vi[0] < 0 ||
		    trim->m_vi[0] != trim->m_vi[1])
		return false;
	    vertex = trim->m_vi[0];
	    singular_trim_count++;
	}
    }
    if (singular_trim_count > 1 || pole_vertices.empty())
	return false;
    if (vertex < 0)
	vertex = *pole_vertices.begin();

    if (closed_dir)
	*closed_dir = cdir;
    if (open_dir)
	*open_dir = odir;
    if (singular_side)
	*singular_side = side;
    if (pole_vertex)
	*pole_vertex = vertex;
    if (periodic)
	*periodic = surface->IsClosed(cdir);
    if (periodic && !*periodic) {
	for (int ti = 0; ti < outer_loop->TrimCount(); ++ti) {
	    const ON_BrepTrim *trim = outer_loop->Trim(ti);
	    if (trim && trim->m_type == ON_BrepTrim::seam) {
		*periodic = true;
		break;
	    }
	}
    }
    return true;
}

bool
cdt_face_uses_cone_chart(const ON_BrepFace &face)
{
    return cone_chart_properties(face, NULL, NULL, NULL, NULL, NULL);
}

struct polar_chart_info {
    int closed_dir = -1;
    int open_dir = -1;
    int first_side = -1;
    int second_side = -1;
    int first_vertex = -1;
    int second_vertex = -1;
    bool periodic = false;
};

static ON_Surface::ISO
side_iso(int side)
{
    switch (side) {
	case 0: return ON_Surface::S_iso;
	case 1: return ON_Surface::E_iso;
	case 2: return ON_Surface::N_iso;
	case 3: return ON_Surface::W_iso;
    }
    return ON_Surface::not_iso;
}

static bool
polar_chart_properties(const ON_BrepFace &face, polar_chart_info *info)
{
    const ON_Surface *surface = face.SurfaceOf();
    if (!surface)
	return false;

    int surface_sides[2] = {-1, -1};
    int singular_count = 0;
    for (int side = 0; side < 4; ++side) {
	if (!surface->IsSingular(side))
	    continue;
	if (singular_count >= 2)
	    return false;
	surface_sides[singular_count++] = side;
    }
    if (singular_count < 1)
	return false;
    const int first_dir = (surface_sides[0] == 0 ||
	surface_sides[0] == 2) ? 1 : 0;
    if (singular_count == 2) {
	const int second_dir = (surface_sides[1] == 0 ||
	    surface_sides[1] == 2) ? 1 : 0;
	if (first_dir != second_dir)
	    return false;
    }
    const int odir = first_dir;
    const int cdir = 1 - odir;
    if (surface->IsClosed(odir))
	return false;

    const ON_BrepLoop *outer_loop = face.OuterLoop();
    if (!outer_loop)
	return false;
    polar_chart_info candidate;
    candidate.closed_dir = cdir;
    candidate.open_dir = odir;
    candidate.periodic = surface->IsClosed(cdir);
    for (int ti = 0; ti < outer_loop->TrimCount(); ++ti) {
	const ON_BrepTrim *trim = outer_loop->Trim(ti);
	if (!trim || trim->m_type != ON_BrepTrim::singular) {
	    if (trim && trim->m_type == ON_BrepTrim::seam)
		candidate.periodic = true;
	    continue;
	}
	if (trim->m_vi[0] < 0 || trim->m_vi[0] != trim->m_vi[1])
	    return false;
	int side = -1;
	for (int si = 0; si < singular_count; ++si) {
	    if (trim->m_iso == side_iso(surface_sides[si])) {
		side = surface_sides[si];
		break;
	    }
	}
	if (side < 0)
	    return false;
	if (candidate.first_side < 0) {
	    candidate.first_side = side;
	    candidate.first_vertex = trim->m_vi[0];
	} else if (candidate.second_side < 0 &&
		candidate.first_side != side) {
	    candidate.second_side = side;
	    candidate.second_vertex = trim->m_vi[0];
	} else {
	    return false;
	}
    }
    if (candidate.first_side < 0)
	return false;

    /* Give the two-pole form a fixed low-to-high ordering. */
    if (candidate.second_side >= 0) {
	const bool first_is_low = candidate.first_side == 0 ||
	    candidate.first_side == 3;
	if (!first_is_low) {
	    std::swap(candidate.first_side, candidate.second_side);
	    std::swap(candidate.first_vertex, candidate.second_vertex);
	}
	if (candidate.first_vertex == candidate.second_vertex)
	    return false;
    }
    if (info)
	*info = candidate;
    return true;
}

bool
cdt_face_uses_polar_chart(const ON_BrepFace &face)
{
    return polar_chart_properties(face, NULL);
}

bool
cdt_face_uses_topology_chart(const ON_BrepFace &face)
{
    return cdt_face_uses_cone_chart(face) ||
	cdt_face_uses_polar_chart(face);
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

static double
periodic_parameter(double parameter, const ON_Interval &domain)
{
    const double tolerance = parameter_tolerance(domain);
    if (parameter >= domain.Min() - tolerance &&
	    parameter <= domain.Max() + tolerance)
	return std::max(domain.Min(), std::min(domain.Max(), parameter));
    const double period = domain.Length();
    if (!(period > 0.0))
	return parameter;
    double remainder = std::fmod(parameter - domain.Min(), period);
    if (remainder < 0.0)
	remainder += period;
    if (std::fabs(remainder) <= tolerance && parameter > domain.Min())
	return domain.Max();
    return domain.Min() + remainder;
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
    if (m_type == CDT_FACE_CHART_SURFACE_METRIC) {
	for (int direction = 0; direction < 2; ++direction)
	    chart_uv[direction] = (native_uv[direction] -
		m_native_domain[direction].Min()) *
		m_metric_scale[direction];
	return chart_uv.IsValid();
    }
    if ((m_type != CDT_FACE_CHART_CONE_WEDGE &&
	    m_type != CDT_FACE_CHART_POLAR) ||
	    m_closed_dir < 0 || m_open_dir < 0)
	return false;
    const double period = m_closed_domain.Length();
    const double open_length = m_open_domain.Length();
    if (!(period > 0.0) || !(open_length > 0.0))
	return false;
    const double angular = m_periodic ? periodic_parameter(
	native_uv[m_closed_dir], m_closed_domain) :
	native_uv[m_closed_dir];
    if (m_type == CDT_FACE_CHART_POLAR &&
	    m_second_singular_side >= 0) {
	double latitude = 2.0 *
	    (native_uv[m_open_dir] - m_open_domain.Mid()) / open_length;
	const double tolerance = parameter_tolerance(m_open_domain) /
	    open_length;
	if (latitude < -1.0 - tolerance || latitude > 1.0 + tolerance)
	    return false;
	latitude = std::max(-1.0, std::min(1.0, latitude));
	const double width = 1.0 - std::fabs(latitude);
	chart_uv.x = width * (angular -
	    m_closed_domain.Mid()) / period;
	chart_uv.y = latitude;
	return chart_uv.IsValid();
    }
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
    chart_uv.x = radial * (angular - center) / period;
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
    if (m_type == CDT_FACE_CHART_SURFACE_METRIC) {
	for (int direction = 0; direction < 2; ++direction) {
	    if (!(m_metric_scale[direction] > 0.0))
		return false;
	    native_uv[direction] = m_native_domain[direction].Min() +
		chart_uv[direction] / m_metric_scale[direction];
	}
	return native_uv.IsValid();
    }
    if ((m_type != CDT_FACE_CHART_CONE_WEDGE &&
	    m_type != CDT_FACE_CHART_POLAR) ||
	    m_closed_dir < 0 || m_open_dir < 0)
	return false;
    if (m_type == CDT_FACE_CHART_POLAR &&
	    m_second_singular_side >= 0) {
	const double latitude = chart_uv.y;
	const double tolerance = 256.0 *
	    std::numeric_limits<double>::epsilon();
	if (latitude < -1.0 - tolerance || latitude > 1.0 + tolerance)
	    return false;
	const double width = 1.0 - std::fabs(latitude);
	native_uv[m_open_dir] = m_open_domain.Mid() +
	    0.5 * latitude * m_open_domain.Length();
	native_uv[m_closed_dir] = width > tolerance ?
	    m_closed_domain.Mid() + chart_uv.x *
		m_closed_domain.Length() / width :
	    m_closed_domain.Mid();
	return native_uv.IsValid();
    }
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
    const ON_Surface *surface = face.SurfaceOf();
    if (!surface) {
	m_failure = "native chart has no source surface";
	return false;
    }

    m_type = CDT_FACE_CHART_SURFACE_METRIC;
    m_native_domain[0] = surface->Domain(0);
    m_native_domain[1] = surface->Domain(1);
    double surface_size[2] = {0.0, 0.0};
    const bool have_surface_size = surface->GetSurfaceSize(
	&surface_size[0], &surface_size[1]);
    for (int direction = 0; direction < 2; ++direction) {
	const double domain_length = m_native_domain[direction].Length();
	m_metric_scale[direction] = 1.0;
	if (have_surface_size && domain_length > 0.0 &&
		std::isfinite(surface_size[direction]) &&
		surface_size[direction] > 0.0)
	    m_metric_scale[direction] = surface_size[direction] /
		domain_length;
    }

    points.reserve(native_points.size());
    for (const auto &point : native_points) {
	ON_2dPoint chart_point;
	if (!native_to_chart(ON_2dPoint(point.first, point.second),
		chart_point)) {
	    m_failure = "native point could not be mapped to its metric chart";
	    return false;
	}
	points.push_back(std::make_pair(chart_point.x, chart_point.y));
    }
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
cdt_face_chart::build_pole_wedge(const ON_BrepFace &face,
	const std::vector<std::pair<double, double>> &native_points,
	const std::vector<int> &source_outer,
	const std::vector<std::vector<int>> &source_holes,
	const std::vector<int> &source_steiner,
	const std::vector<const ON_3dPoint *> &points_3d,
	const std::vector<cdt_topo_vertex_id> &topology_vertices, bool ruled)
{
    const char *chart_name = ruled ? "cone" : "sphere";
    const double pole_coordinate =
	(m_singular_side == 0 || m_singular_side == 3) ?
	m_open_domain.Min() : m_open_domain.Max();
    const double pole_tolerance = parameter_tolerance(m_open_domain);
    std::vector<int> source_to_chart(native_points.size(), -1);
    std::vector<bool> inactive_ruled_point(native_points.size(), false);
    if (ruled) {
	for (int point : source_steiner) {
	    if (point >= 0 && (size_t)point < inactive_ruled_point.size())
		inactive_ruled_point[(size_t)point] = true;
	}
	for (int point : source_outer) {
	    if (point >= 0 && (size_t)point < inactive_ruled_point.size())
		inactive_ruled_point[(size_t)point] = false;
	}
    }
    for (size_t i = 0; i < native_points.size(); ++i) {
	const ON_2dPoint native_uv(native_points[i].first,
	    native_points[i].second);
	if (std::fabs(native_uv[m_open_dir] - pole_coordinate) <=
		pole_tolerance &&
		topology_vertices[i] != CDT_TOPOLOGY_ID_NONE &&
		topology_vertices[i] != m_pole_topology_vertex) {
	    m_failure = std::string(chart_name) +
		" face has multiple topological pole vertices and requires "
		"chart decomposition";
	    return false;
	}
    }
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
	m_failure = std::string(chart_name) + " chart has no pole sample";
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
	if (inactive_ruled_point[i])
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
	    m_failure = std::string(chart_name) +
		" point lies outside the chart domain";
	    return false;
	}
	const int chart_index = (int)points.size();
	points.push_back(std::make_pair(chart_uv.x, chart_uv.y));
	cdt_chart_vertex vertex;
	vertex.id = chart_index;
	vertex.native_point = (long)i;
	vertex.topo_vertex = topology_vertices[i];
	const double seam_tolerance = parameter_tolerance(m_closed_domain);
	if (m_periodic && !at_pole &&
		std::fabs(native_uv[m_closed_dir] -
		m_closed_domain.Min()) <= seam_tolerance)
	    vertex.seam_side = -1;
	else if (m_periodic && !at_pole &&
		std::fabs(native_uv[m_closed_dir] -
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
	m_failure = std::string(chart_name) +
	    " chart outline does not form one disk boundary";
	return false;
    }
    holes.resize(source_holes.size());
    for (size_t hi = 0; hi < source_holes.size(); ++hi) {
	if (!remap_ring(source_holes[hi], holes[hi])) {
	    m_failure = std::string(chart_name) +
		" chart hole does not form a simple boundary";
	    return false;
	}
    }

    for (int point : source_steiner) {
	if (point < 0 || (size_t)point >= source_to_chart.size()) {
	    m_failure = std::string("invalid ") + chart_name +
		" chart Steiner point";
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
    if (!ruled) {
	std::set<int> unique_steiner;
	for (int point : source_steiner) {
	    const int mapped = source_to_chart[(size_t)point];
	    if (mapped != apex && unique_steiner.insert(mapped).second)
		steiner.push_back(mapped);
	}
    }

    if (ruled && holes.empty()) {
	const auto apex_position = std::find(outer.begin(), outer.end(), apex);
	if (apex_position == outer.end()) {
	    m_failure = "cone chart outline does not contain its apex";
	    return false;
	}
	const size_t apex_offset = (size_t)(apex_position - outer.begin());
	const int ray_points[2] = {
	    outer[(apex_offset + outer.size() - 1) % outer.size()],
	    outer[(apex_offset + 1) % outer.size()]
	};
	const auto on_boundary_ray = [&](int point) {
	    for (int ray_point : ray_points) {
		const long double cross =
		    (long double)points[(size_t)ray_point].first *
			points[(size_t)point].second -
		    (long double)points[(size_t)ray_point].second *
			points[(size_t)point].first;
		const long double scale = std::max(1.0L,
		    std::fabs((long double)points[(size_t)ray_point].first *
			points[(size_t)point].second) +
		    std::fabs((long double)points[(size_t)ray_point].second *
			points[(size_t)point].first));
		if (std::fabs(cross) <= 512.0L *
			std::numeric_limits<double>::epsilon() * scale)
		    return true;
	    }
	    return false;
	};
	const auto orient = [&](int ia, int ib, double x, double y) {
	    const long double abx =
		(long double)points[(size_t)ib].first -
		points[(size_t)ia].first;
	    const long double aby =
		(long double)points[(size_t)ib].second -
		points[(size_t)ia].second;
	    const long double acx = (long double)x -
		points[(size_t)ia].first;
	    const long double acy = (long double)y -
		points[(size_t)ia].second;
	    const long double cross = abx * acy - aby * acx;
	    return (cross > 0.0L) - (cross < 0.0L);
	};
	const auto on_segment = [&](int point, int first, int second) {
	    if (orient(first, second, points[(size_t)point].first,
		    points[(size_t)point].second))
		return false;
	    return points[(size_t)point].first >= std::min(
		points[(size_t)first].first, points[(size_t)second].first) &&
		points[(size_t)point].first <= std::max(
		points[(size_t)first].first, points[(size_t)second].first) &&
		points[(size_t)point].second >= std::min(
		points[(size_t)first].second, points[(size_t)second].second) &&
		points[(size_t)point].second <= std::max(
		points[(size_t)first].second, points[(size_t)second].second);
	};
	const auto segment_intersects = [&](int a, int b, int c, int d) {
	    const int o1 = orient(a, b, points[(size_t)c].first,
		points[(size_t)c].second);
	    const int o2 = orient(a, b, points[(size_t)d].first,
		points[(size_t)d].second);
	    const int o3 = orient(c, d, points[(size_t)a].first,
		points[(size_t)a].second);
	    const int o4 = orient(c, d, points[(size_t)b].first,
		points[(size_t)b].second);
	    if (!o1 && on_segment(c, a, b)) return true;
	    if (!o2 && on_segment(d, a, b)) return true;
	    if (!o3 && on_segment(a, c, d)) return true;
	    if (!o4 && on_segment(b, c, d)) return true;
	    return o1 * o2 < 0 && o3 * o4 < 0;
	};
	const auto point_in_outer = [&](double x, double y) {
	    int winding = 0;
	    for (size_t i = 0; i < outer.size(); ++i) {
		const int a = outer[i];
		const int b = outer[(i + 1) % outer.size()];
		if (points[(size_t)a].second <= y) {
		    if (points[(size_t)b].second > y &&
			    orient(a, b, x, y) > 0)
			++winding;
		} else if (points[(size_t)b].second <= y &&
			orient(a, b, x, y) < 0) {
		    --winding;
		}
	    }
	    return winding != 0;
	};
	const auto valid_fan_segment = [&](int point) {
	    const double midpoint_x = 0.5 *
		(points[(size_t)apex].first + points[(size_t)point].first);
	    const double midpoint_y = 0.5 *
		(points[(size_t)apex].second + points[(size_t)point].second);
	    if (!point_in_outer(midpoint_x, midpoint_y))
		return false;
	    for (size_t i = 0; i < outer.size(); ++i) {
		const int a = outer[i];
		const int b = outer[(i + 1) % outer.size()];
		if (a == apex || b == apex || a == point || b == point)
		    continue;
		if (segment_intersects(apex, point, a, b))
		    return false;
	    }
	    return true;
	};
	for (int point : outer) {
	    if (point == apex || on_boundary_ray(point) ||
		    !valid_fan_segment(point))
		continue;
	    constraints.push_back(std::make_pair(apex, point));
	}
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
cdt_face_chart::build_cone(const ON_BrepFace &face,
	const std::vector<std::pair<double, double>> &native_points,
	const std::vector<int> &source_outer,
	const std::vector<std::vector<int>> &source_holes,
	const std::vector<int> &source_steiner,
	const std::vector<const ON_3dPoint *> &points_3d,
	const std::vector<cdt_topo_vertex_id> &topology_vertices)
{
    m_type = CDT_FACE_CHART_CONE_WEDGE;
    return build_pole_wedge(face, native_points, source_outer, source_holes,
	source_steiner, points_3d, topology_vertices, true);
}

bool
cdt_face_chart::build_polar(const ON_BrepFace &face,
	const std::vector<std::pair<double, double>> &native_points,
	const std::vector<int> &source_outer,
	const std::vector<std::vector<int>> &source_holes,
	const std::vector<int> &source_steiner,
	const std::vector<const ON_3dPoint *> &points_3d,
	const std::vector<cdt_topo_vertex_id> &topology_vertices)
{
    m_type = CDT_FACE_CHART_POLAR;
    if (m_second_singular_side < 0)
	return build_pole_wedge(face, native_points, source_outer,
	    source_holes, source_steiner, points_3d, topology_vertices,
	    false);

    const double open_tolerance = parameter_tolerance(m_open_domain);
    std::vector<int> source_to_chart(native_points.size(), -1);
    size_t low_source = native_points.size();
    size_t high_source = native_points.size();
    for (size_t i = 0; i < native_points.size(); ++i) {
	const double open_parameter = m_open_dir == 0 ?
	    native_points[i].first : native_points[i].second;
	if (topology_vertices[i] == m_pole_topology_vertex ||
		std::fabs(open_parameter - m_open_domain.Min()) <=
		open_tolerance)
	    low_source = std::min(low_source, i);
	if (topology_vertices[i] == m_second_pole_topology_vertex ||
		std::fabs(open_parameter - m_open_domain.Max()) <=
		open_tolerance)
	    high_source = std::min(high_source, i);
    }
    if (low_source == native_points.size() ||
	    high_source == native_points.size()) {
	m_failure = "two-pole sphere chart is missing a pole sample";
	return false;
    }

    const auto add_pole = [&](size_t source, double latitude,
	    cdt_topo_vertex_id topology) {
	const int index = (int)points.size();
	points.push_back(std::make_pair(0.0, latitude));
	cdt_chart_vertex vertex;
	vertex.id = index;
	vertex.native_point = (long)source;
	vertex.topo_vertex = topology;
	vertex.singular = true;
	vertices.push_back(vertex);
	return index;
    };
    const int low_pole = add_pole(low_source, -1.0,
	m_pole_topology_vertex);
    const int high_pole = add_pole(high_source, 1.0,
	m_second_pole_topology_vertex);

    const double seam_tolerance = parameter_tolerance(m_closed_domain);
    for (size_t i = 0; i < native_points.size(); ++i) {
	const ON_2dPoint native_uv(native_points[i].first,
	    native_points[i].second);
	const double open_parameter = native_uv[m_open_dir];
	if (topology_vertices[i] == m_pole_topology_vertex ||
		std::fabs(open_parameter - m_open_domain.Min()) <=
		open_tolerance) {
	    source_to_chart[i] = low_pole;
	    continue;
	}
	if (topology_vertices[i] == m_second_pole_topology_vertex ||
		std::fabs(open_parameter - m_open_domain.Max()) <=
		open_tolerance) {
	    source_to_chart[i] = high_pole;
	    continue;
	}
	ON_2dPoint chart_uv;
	if (!native_to_chart(native_uv, chart_uv)) {
	    m_failure = "sphere point lies outside the polar chart domain";
	    return false;
	}
	const int chart_index = (int)points.size();
	points.push_back(std::make_pair(chart_uv.x, chart_uv.y));
	cdt_chart_vertex vertex;
	vertex.id = chart_index;
	vertex.native_point = (long)i;
	vertex.topo_vertex = topology_vertices[i];
	if (m_periodic && std::fabs(native_uv[m_closed_dir] -
		m_closed_domain.Min()) <= seam_tolerance)
	    vertex.seam_side = -1;
	else if (m_periodic && std::fabs(native_uv[m_closed_dir] -
		m_closed_domain.Max()) <= seam_tolerance)
	    vertex.seam_side = 1;
	vertices.push_back(vertex);
	source_to_chart[i] = chart_index;
    }

    outer.clear();
    for (int point : source_outer) {
	if (point < 0 || (size_t)point >= source_to_chart.size()) {
	    m_failure = "invalid two-pole sphere outline point";
	    return false;
	}
	const int mapped = source_to_chart[(size_t)point];
	if (outer.empty() || outer.back() != mapped)
	    outer.push_back(mapped);
    }
    if (outer.size() > 1 && outer.front() == outer.back())
	outer.pop_back();
    const std::set<int> unique_outer(outer.begin(), outer.end());
    if (outer.size() < 4 || unique_outer.size() != outer.size()) {
	m_failure = "sphere poles and seam do not form one disk boundary";
	return false;
    }
    holes.resize(source_holes.size());
    for (size_t hi = 0; hi < source_holes.size(); ++hi) {
	for (int point : source_holes[hi]) {
	    if (point < 0 || (size_t)point >= source_to_chart.size()) {
		m_failure = "invalid polar chart hole point";
		return false;
	    }
	    const int mapped = source_to_chart[(size_t)point];
	    if (holes[hi].empty() || holes[hi].back() != mapped)
		holes[hi].push_back(mapped);
	}
	if (holes[hi].size() > 1 &&
		holes[hi].front() == holes[hi].back())
	    holes[hi].pop_back();
	const std::set<int> unique_hole(holes[hi].begin(), holes[hi].end());
	if (holes[hi].size() < 3 ||
		unique_hole.size() != holes[hi].size()) {
	    m_failure = "polar chart hole is not a simple boundary";
	    return false;
	}
    }

    std::set<int> unique_steiner;
    for (int point : source_steiner) {
	if (point < 0 || (size_t)point >= source_to_chart.size()) {
	    m_failure = "invalid sphere chart Steiner point";
	    return false;
	}
	const int mapped = source_to_chart[(size_t)point];
	if (mapped != low_pole && mapped != high_pole &&
		unique_steiner.insert(mapped).second)
	    steiner.push_back(mapped);
    }

    /* Longitude lines are straight within each polar half of this chart.
     * Constrain sampled cap rays independently.  A chord spanning the two
     * halves would miss the equatorial kink and can cross a nearby ray. */
    std::set<int> boundary(outer.begin(), outer.end());
    struct meridian_sample {
	double angular;
	double polar;
	int point;
    };
    std::vector<meridian_sample> samples;
    for (int point : steiner) {
	if (!holes.empty())
	    break;
	if (boundary.find(point) != boundary.end() ||
		vertices[(size_t)point].seam_side)
	    continue;
	if (vertices[(size_t)point].native_point < 0)
	    continue;
	const double polar = points[(size_t)point].second;
	const double width = 1.0 - std::fabs(polar);
	if (!(width > 0.0))
	    continue;
	samples.push_back({points[(size_t)point].first / width,
	    polar, point});
    }
    std::sort(samples.begin(), samples.end(), [](const auto &a,
	const auto &b) {
	if (a.angular < b.angular)
	    return a.angular < b.angular;
	if (b.angular < a.angular)
	    return false;
	if (a.polar < b.polar)
	    return a.polar < b.polar;
	if (b.polar < a.polar)
	    return false;
	return a.point < b.point;
    });
    const double angular_tolerance = 4.0 *
	sqrt(std::numeric_limits<double>::epsilon());

    const auto orient = [&](int ia, int ib, double x, double y) {
	const long double abx = (long double)points[(size_t)ib].first -
	    points[(size_t)ia].first;
	const long double aby = (long double)points[(size_t)ib].second -
	    points[(size_t)ia].second;
	const long double acx = (long double)x -
	    points[(size_t)ia].first;
	const long double acy = (long double)y -
	    points[(size_t)ia].second;
	const long double cross = abx * acy - aby * acx;
	return (cross > 0.0L) - (cross < 0.0L);
    };
    const auto segment_intersects = [&](int a, int b, int c, int d) {
	const auto on_segment = [&](int p, int first, int second) {
	    if (orient(first, second, points[(size_t)p].first,
		    points[(size_t)p].second))
		return false;
	    return points[(size_t)p].first >= std::min(
		points[(size_t)first].first, points[(size_t)second].first) &&
		points[(size_t)p].first <= std::max(
		points[(size_t)first].first, points[(size_t)second].first) &&
		points[(size_t)p].second >= std::min(
		points[(size_t)first].second, points[(size_t)second].second) &&
		points[(size_t)p].second <= std::max(
		points[(size_t)first].second, points[(size_t)second].second);
	};
	const int o1 = orient(a, b, points[(size_t)c].first,
	    points[(size_t)c].second);
	const int o2 = orient(a, b, points[(size_t)d].first,
	    points[(size_t)d].second);
	const int o3 = orient(c, d, points[(size_t)a].first,
	    points[(size_t)a].second);
	const int o4 = orient(c, d, points[(size_t)b].first,
	    points[(size_t)b].second);
	if (!o1 && on_segment(c, a, b)) return true;
	if (!o2 && on_segment(d, a, b)) return true;
	if (!o3 && on_segment(a, c, d)) return true;
	if (!o4 && on_segment(b, c, d)) return true;
	return o1 * o2 < 0 && o3 * o4 < 0;
    };
    const auto point_in_ring = [&](double x, double y,
	    const std::vector<int> &ring) {
	int winding = 0;
	for (size_t i = 0; i < ring.size(); ++i) {
	    const int a = ring[i];
	    const int b = ring[(i + 1) % ring.size()];
	    if (points[(size_t)a].second <= y) {
		if (points[(size_t)b].second > y && orient(a, b, x, y) > 0)
		    ++winding;
	    } else if (points[(size_t)b].second <= y &&
		    orient(a, b, x, y) < 0) {
		--winding;
	    }
	}
	return winding != 0;
    };
    const auto append_constraint = [&](int a, int b) {
	if (a == b)
	    return false;
	const double midpoint_x = 0.5 * (points[(size_t)a].first +
	    points[(size_t)b].first);
	const double midpoint_y = 0.5 * (points[(size_t)a].second +
	    points[(size_t)b].second);
	if (!point_in_ring(midpoint_x, midpoint_y, outer))
	    return false;
	for (const std::vector<int> &hole : holes) {
	    if (point_in_ring(midpoint_x, midpoint_y, hole))
		return false;
	}
	const auto crosses_ring = [&](const std::vector<int> &ring) {
	    for (size_t i = 0; i < ring.size(); ++i) {
		const int c = ring[i];
		const int d = ring[(i + 1) % ring.size()];
		if (a == c || a == d || b == c || b == d)
		    continue;
		if (segment_intersects(a, b, c, d))
		    return true;
	    }
	    return false;
	};
	if (crosses_ring(outer))
	    return false;
	for (const std::vector<int> &hole : holes) {
	    if (crosses_ring(hole))
		return false;
	}
	for (const std::pair<int, int> &constraint : constraints) {
	    if (a == constraint.first || a == constraint.second ||
		    b == constraint.first || b == constraint.second)
		continue;
	    if (segment_intersects(a, b, constraint.first,
		    constraint.second))
		return false;
	}
	constraints.push_back(std::make_pair(a, b));
	return true;
    };
    size_t begin = 0;
    size_t ray_count = 0;
    while (begin < samples.size()) {
	size_t end = begin + 1;
	while (end < samples.size() &&
		std::fabs(samples[end].angular - samples[begin].angular) <=
		angular_tolerance)
	    ++end;
	std::sort(samples.begin() + begin, samples.begin() + end,
	    [](const auto &a, const auto &b) {
		if (a.polar < b.polar)
		    return true;
		if (b.polar < a.polar)
		    return false;
		return a.point < b.point;
	    });
	int previous = low_pole;
	size_t middle = begin;
	while (middle < end && samples[middle].polar < 0.0) {
	    ray_count += append_constraint(previous,
		samples[middle].point) ? 1 : 0;
	    previous = samples[middle].point;
	    ++middle;
	}
	size_t upper = middle;
	while (upper < end && !(samples[upper].polar > 0.0)) {
	    if (previous != samples[upper].point)
		ray_count += append_constraint(previous,
		    samples[upper].point) ? 1 : 0;
	    previous = samples[upper].point;
	    ++upper;
	}
	if (upper > middle) {
	    for (size_t i = upper; i < end; ++i) {
		ray_count += append_constraint(previous,
		    samples[i].point) ? 1 : 0;
		previous = samples[i].point;
	    }
	    ray_count += append_constraint(previous, high_pole) ? 1 : 0;
	} else if (upper < end) {
	    previous = samples[upper].point;
	    for (size_t i = upper + 1; i < end; ++i) {
		ray_count += append_constraint(previous,
		    samples[i].point) ? 1 : 0;
		previous = samples[i].point;
	    }
	    ray_count += append_constraint(previous, high_pole) ? 1 : 0;
	}
	begin = end;
    }
    if (holes.empty() && !ray_count) {
	m_failure = "two-pole sphere chart has no interior cap samples";
	return false;
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
    m_second_singular_side = -1;
    m_periodic = false;
    m_pole_topology_vertex = CDT_TOPOLOGY_ID_NONE;
    m_second_pole_topology_vertex = CDT_TOPOLOGY_ID_NONE;

    if (topology_vertices.size() != native_points.size()) {
	m_failure = "chart topology metadata does not match its point array";
	return false;
    }

    int pole_vertex = -1;
    if (cone_chart_properties(face, &m_closed_dir, &m_open_dir,
	    &m_singular_side, &pole_vertex, &m_periodic)) {
	m_closed_domain = face.SurfaceOf()->Domain(m_closed_dir);
	m_open_domain = face.SurfaceOf()->Domain(m_open_dir);
	m_pole_topology_vertex = pole_vertex;
	return build_cone(face, native_points, source_outer, source_holes,
	    source_steiner, points_3d, topology_vertices);
    }

    polar_chart_info polar_info;
    if (polar_chart_properties(face, &polar_info)) {
	m_closed_dir = polar_info.closed_dir;
	m_open_dir = polar_info.open_dir;
	m_singular_side = polar_info.first_side;
	m_second_singular_side = polar_info.second_side;
	m_periodic = polar_info.periodic;
	m_pole_topology_vertex = polar_info.first_vertex;
	m_second_pole_topology_vertex = polar_info.second_vertex;
	m_closed_domain = face.SurfaceOf()->Domain(m_closed_dir);
	m_open_domain = face.SurfaceOf()->Domain(m_open_dir);
	return build_polar(face, native_points, source_outer, source_holes,
	    source_steiner, points_3d, topology_vertices);
    }
    return build_native(face, native_points, source_outer, source_holes,
	source_steiner, topology_vertices);
}
