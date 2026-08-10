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
#include "brep/pullback.h"
#include "chart.h"

static const double CDT_CONIC_TOLERANCE = 0.05;

static bool
cylinder_chart_coordinates(const ON_Cylinder &cylinder,
	const ON_3dPoint &point, double *angular, double *height)
{
    if (!cylinder.IsValid() || !point.IsValid() || !angular || !height)
	return false;
    const ON_3dVector offset = point - cylinder.circle.plane.origin;
    const double x = offset * cylinder.circle.plane.xaxis;
    const double y = offset * cylinder.circle.plane.yaxis;
    const double radial = hypot(x, y);
    const double axial = offset * cylinder.circle.plane.zaxis;
    if (!(radial > 0.0) || !std::isfinite(radial) ||
	    !std::isfinite(axial))
	return false;
    *angular = atan2(y, x);
    if (*angular < 0.0)
	*angular += 2.0 * ON_PI;
    *height = axial;
    return std::isfinite(*angular);
}

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
    /* Some imported two-pole surfaces encode both meridians as ordinary seam
     * trims and omit the zero-length singular trims normally used to name the
     * poles.  Recover those pole identities only when every trim is a seam
     * and each singular surface side has exactly one topology vertex. */
    if (candidate.first_side < 0 && singular_count == 2) {
	bool all_seams = outer_loop->TrimCount() > 0;
	std::map<int, std::set<int>> side_vertices;
	const ON_Interval open_domain = surface->Domain(odir);
	const double magnitude = std::max(std::fabs(open_domain.Min()),
	    std::fabs(open_domain.Max()));
	const double tolerance = 256.0 *
	    std::numeric_limits<double>::epsilon() *
	    std::max(magnitude, open_domain.Length());
	for (int ti = 0; ti < outer_loop->TrimCount(); ++ti) {
	    const ON_BrepTrim *trim = outer_loop->Trim(ti);
	    if (!trim || trim->m_type != ON_BrepTrim::seam ||
		    trim->m_vi[0] < 0 || trim->m_vi[1] < 0) {
		all_seams = false;
		break;
	    }
	    const ON_3dPoint endpoints[2] = {
		trim->PointAtStart(), trim->PointAtEnd()
	    };
	    for (int endpoint = 0; endpoint < 2; ++endpoint) {
		const double parameter = endpoints[endpoint][odir];
		for (int si = 0; si < singular_count; ++si) {
		    const int side = surface_sides[si];
		    const double pole = (side == 0 || side == 3) ?
			open_domain.Min() : open_domain.Max();
		    if (std::fabs(parameter - pole) <= tolerance)
			side_vertices[side].insert(trim->m_vi[endpoint]);
		}
	    }
	}
	if (all_seams && side_vertices[surface_sides[0]].size() == 1 &&
		side_vertices[surface_sides[1]].size() == 1) {
	    candidate.first_side = surface_sides[0];
	    candidate.first_vertex =
		*side_vertices[surface_sides[0]].begin();
	    candidate.second_side = surface_sides[1];
	    candidate.second_vertex =
		*side_vertices[surface_sides[1]].begin();
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
cdt_face_has_seam(const ON_BrepFace &face)
{
    for (int loop_index = 0; loop_index < face.LoopCount(); ++loop_index) {
	const ON_BrepLoop *loop = face.Loop(loop_index);
	if (!loop)
	    continue;
	for (int trim_index = 0; trim_index < loop->TrimCount(); ++trim_index) {
	    const ON_BrepTrim *trim = loop->Trim(trim_index);
	    if (trim && trim->m_type == ON_BrepTrim::seam)
		return true;
	}
    }
    return false;
}

bool
cdt_face_uses_cylinder_chart(const ON_BrepFace &face)
{
    const ON_Surface *surface = face.SurfaceOf();
    if (!surface || !surface->IsCylinder(NULL, CDT_CONIC_TOLERANCE))
	return false;
    if (cdt_face_has_seam(face))
	return surface->IsClosed(0) || surface->IsClosed(1);
    return true;
}

bool
cdt_face_uses_topology_chart(const ON_BrepFace &face)
{
    const ON_Surface *surface = face.SurfaceOf();
    const bool periodic_seam = surface && cdt_face_has_seam(face) &&
	(surface->IsClosed(0) || surface->IsClosed(1));
    return periodic_seam || cdt_face_uses_cone_chart(face) ||
	cdt_face_uses_polar_chart(face) ||
	cdt_face_uses_cylinder_chart(face);
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

static int
periodic_seam_side(double parameter, const ON_Interval &domain)
{
    const double tolerance = parameter_tolerance(domain);
    if (std::fabs(parameter - domain.Min()) <= tolerance)
	return -1;
    if (std::fabs(parameter - domain.Max()) <= tolerance)
	return 1;
    return 0;
}

/* A closed edge may have coincident 3-D endpoints stored on either the same or
 * opposite sides of a periodic surface domain.  Some STEP p-curves traverse
 * the edge through the opposite periodic image, retain the same parameter for
 * both endpoints, or switch bounds along a seam trim.  Orient closed paths and
 * propagate each seam side without changing the undirected 3-D edge segments. */
static std::vector<int>
orient_periodic_closed_paths(const std::vector<int> &source,
	std::vector<double> &canonical,
	const std::vector<const ON_3dPoint *> &points_3d,
	const ON_Interval &domain, const std::vector<double> *open_parameters,
	const std::vector<cdt_topo_vertex_id> *topology_vertices)
{
    std::vector<int> ring(source);
    while (ring.size() > 1 && ring.front() == ring.back())
	ring.pop_back();
    if (ring.size() < 3 || !(domain.Length() > 0.0))
	return source;

    size_t start = ring.size();
    for (size_t i = 0; i < ring.size(); ++i) {
	const int point = ring[i];
	const int next = ring[(i + 1) % ring.size()];
	if (point < 0 || next < 0 || (size_t)point >= canonical.size() ||
		(size_t)next >= canonical.size())
	    return source;
	const bool topology_endpoint = !topology_vertices ||
	    ((size_t)point < topology_vertices->size() &&
	    (*topology_vertices)[(size_t)point] != CDT_TOPOLOGY_ID_NONE);
	if (topology_endpoint &&
		periodic_seam_side(canonical[(size_t)point], domain) &&
		!periodic_seam_side(canonical[(size_t)next], domain)) {
	    start = i;
	    break;
	}
    }
    if (start == ring.size())
	return source;
    std::rotate(ring.begin(), ring.begin() + start, ring.end());
    ring.push_back(ring.front());

    const double period = domain.Length();
    size_t i = 0;
    size_t collapsed_path_count = 0;

    const auto coincident = [&](int first, int last) {
	if (first < 0 || last < 0 || (size_t)first >= points_3d.size() ||
		(size_t)last >= points_3d.size() || !points_3d[(size_t)first] ||
		!points_3d[(size_t)last])
	    return false;
	const ON_3dPoint &first_3d = *points_3d[(size_t)first];
	const ON_3dPoint &last_3d = *points_3d[(size_t)last];
	const double coordinate_scale = std::max(1.0, std::max(
	    std::max(std::fabs(first_3d.x), std::fabs(first_3d.y)),
	    std::max(std::fabs(first_3d.z), std::max(
	    std::max(std::fabs(last_3d.x), std::fabs(last_3d.y)),
	    std::fabs(last_3d.z)))));
	const double tolerance = std::max(
	    (double)BREP_EDGE_MISS_TOLERANCE, 4096.0 *
	    std::numeric_limits<double>::epsilon() * coordinate_scale);
	return first_3d.DistanceTo(last_3d) <= tolerance;
    };

    while (i + 1 < ring.size()) {
	const int first = ring[i];
	const int first_side = periodic_seam_side(
	    canonical[(size_t)first], domain);
	const int next = ring[i + 1];
	const int next_side = periodic_seam_side(
	    canonical[(size_t)next], domain);
	if (!first_side) {
	    ++i;
	    continue;
	}
	/* Consecutive seam samples with changing open coordinates belong to
	 * one side of the chart cut.  Imported p-curves may switch equivalent
	 * periodic parameter bounds partway along that topological seam. */
	if (next_side) {
	    if (next_side != first_side && open_parameters &&
		    open_parameters->size() == canonical.size()) {
		const double first_open = (*open_parameters)[(size_t)first];
		const double next_open = (*open_parameters)[(size_t)next];
		const double open_scale = std::max(1.0, std::max(
		    std::fabs(first_open), std::fabs(next_open)));
		if (std::fabs(next_open - first_open) > 4096.0 *
			std::numeric_limits<double>::epsilon() * open_scale)
		    canonical[(size_t)next] = first_side < 0 ?
			domain.Min() : domain.Max();
	    }
	    ++i;
	    continue;
	}
	/* Only a B-Rep topology endpoint can begin a closed periodic edge
	 * reconstruction.  A loose ordinary edge sample may have a p-curve
	 * exactly on a domain bound without being part of the chart seam.  If
	 * used as a start, its repeated ring index makes the entire face look
	 * like one full-winding edge and overwrites that point onto the opposite
	 * side of the cut. */
	if (topology_vertices &&
		((size_t)first >= topology_vertices->size() ||
		(*topology_vertices)[(size_t)first] == CDT_TOPOLOGY_ID_NONE)) {
	    ++i;
	    continue;
	}
	size_t end = i + 1;
	while (end < ring.size()) {
	    while (end < ring.size() && !periodic_seam_side(
		    canonical[(size_t)ring[end]], domain))
		++end;
	    if (end >= ring.size() || coincident(first, ring[end]))
		break;
	    ++end;
	}
	if (end >= ring.size())
	    break;
	const int last = ring[end];
	const int last_side = periodic_seam_side(
	    canonical[(size_t)last], domain);
	double winding = 0.0;
	bool duplicate_parameter = false;
	std::vector<std::pair<double, int>> path_parameters;
	for (size_t sample = i + 1; sample <= end; ++sample) {
	    const double previous = canonical[(size_t)ring[sample - 1]];
	    const double current = canonical[(size_t)ring[sample]];
	    double delta = current - previous;
	    delta -= std::nearbyint(delta / period) * period;
	    winding += delta;
	    if (sample < end) {
		for (const std::pair<double, int> &parameter :
			path_parameters) {
		    bool same_open_parameter = false;
		    if (open_parameters &&
			    open_parameters->size() == canonical.size()) {
			const double first_open =
			    (*open_parameters)[(size_t)ring[sample]];
			const double second_open =
			    (*open_parameters)[(size_t)parameter.second];
			const double open_scale = std::max(1.0, std::max(
			    std::fabs(first_open), std::fabs(second_open)));
			same_open_parameter = std::fabs(first_open - second_open) <=
			    4096.0 * std::numeric_limits<double>::epsilon() *
			    open_scale;
		    }
		    if (std::fabs(current - parameter.first) <=
			    parameter_tolerance(domain) &&
			    (same_open_parameter ||
			    coincident(ring[sample], parameter.second))) {
			duplicate_parameter = true;
			break;
		    }
		}
		path_parameters.push_back(std::make_pair(current,
		    ring[sample]));
	    }
	}
	double desired = 0.0;
	if (last_side != first_side)
	    desired = last_side > first_side ? period : -period;
	else
	    desired = collapsed_path_count++ % 2 ? -period : period;
	const bool reconstruct = std::fabs(winding) <= 0.5 * period ||
	    duplicate_parameter;
	if (!reconstruct && winding * desired < 0.0) {
	    std::reverse(ring.begin() + i + 1, ring.begin() + end);
	}
	if (!reconstruct && last_side == first_side)
	    canonical[(size_t)last] = desired > 0.0 ?
		domain.Max() : domain.Min();
	if (!reconstruct) {
	    /* The path already has the requested total winding, but individual
	     * imported p-curve samples may use neighboring periodic images.  Lift
	     * the established path order continuously while leaving its
	     * authoritative seam endpoints fixed. */
	    double previous = canonical[(size_t)first];
	    for (size_t sample = i + 1; sample < end; ++sample) {
		double &current = canonical[(size_t)ring[sample]];
		current += std::nearbyint((previous - current) / period) *
		    period;
		previous = current;
	    }
	}
	if (reconstruct) {
	    std::vector<double> distance(end - i + 1, 0.0);
	    bool complete = true;
	    for (size_t sample = i + 1; sample <= end; ++sample) {
		const int previous = ring[sample - 1];
		const int current = ring[sample];
		if (previous < 0 || current < 0 ||
			(size_t)previous >= points_3d.size() ||
			(size_t)current >= points_3d.size() ||
			!points_3d[(size_t)previous] ||
			!points_3d[(size_t)current]) {
		    complete = false;
		    break;
		}
		distance[sample - i] = distance[sample - i - 1] +
		    points_3d[(size_t)previous]->DistanceTo(
		    *points_3d[(size_t)current]);
	    }
	    const double total = distance.back();
	    if (complete && total > 0.0) {
		for (size_t sample = i; sample <= end; ++sample) {
		    const double fraction = distance[sample - i] / total;
		    canonical[(size_t)ring[sample]] = desired > 0.0 ?
			domain.Min() + fraction * period :
			domain.Max() - fraction * period;
		}
	    }
	}
	i = end;
    }
    return ring;
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
	for (int direction = 0; direction < 2; ++direction) {
	    const double parameter = m_periodic && direction == m_closed_dir ?
		periodic_parameter(native_uv[direction],
		    m_native_domain[direction]) : native_uv[direction];
	    chart_uv[direction] = (parameter -
		m_native_domain[direction].Min()) *
		m_metric_scale[direction];
	}
	return chart_uv.IsValid();
    }
    if (m_type == CDT_FACE_CHART_CYLINDER) {
	if (!m_surface || !m_cylinder.IsValid() ||
		!(m_cylinder.circle.radius > 0.0))
	    return false;
	const ON_3dPoint point = m_surface->PointAt(native_uv.x,
	    native_uv.y);
	double angular = 0.0;
	double height = 0.0;
	if (!cylinder_chart_coordinates(m_cylinder, point, &angular,
		&height))
	    return false;
	double unwrapped = m_cylinder_orientation > 0 ?
	    angular - m_cylinder_cut : m_cylinder_cut - angular;
	while (unwrapped < 0.0)
	    unwrapped += 2.0 * ON_PI;
	while (unwrapped >= 2.0 * ON_PI)
	    unwrapped -= 2.0 * ON_PI;
	chart_uv.x = m_cylinder.circle.radius * unwrapped;
	chart_uv.y = height;
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
	double turn = angular - m_polar_cut;
	while (turn < 0.0)
	    turn += period;
	while (turn > period)
	    turn -= period;
	chart_uv.x = width * (turn / period - 0.5);
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
	    if (m_periodic && direction == m_closed_dir)
		native_uv[direction] = periodic_parameter(
		    native_uv[direction], m_native_domain[direction]);
	}
	return native_uv.IsValid();
    }
    if (m_type == CDT_FACE_CHART_CYLINDER) {
	if (!m_surface || !m_cylinder.IsValid() ||
		!(m_cylinder.circle.radius > 0.0))
	    return false;
	double angular = m_cylinder_cut + m_cylinder_orientation *
	    chart_uv.x / m_cylinder.circle.radius;
	while (angular < 0.0)
	    angular += 2.0 * ON_PI;
	while (angular >= 2.0 * ON_PI)
	    angular -= 2.0 * ON_PI;
	const ON_3dPoint target = m_cylinder.PointAt(angular, chart_uv.y);
	brlcad::PullbackContext context;
	ON_3dPoint lifted = ON_3dPoint::UnsetPoint;
	double distance = DBL_MAX;
	double nearest_distance = DBL_MAX;
	ON_2dPoint seed = ON_2dPoint::UnsetPoint;
	for (size_t i = 0; i < points.size() &&
		i < m_source_native_points.size(); ++i) {
	    const double dx = points[i].first - chart_uv.x;
	    const double dy = points[i].second - chart_uv.y;
	    const double candidate = dx * dx + dy * dy;
	    if (candidate < nearest_distance) {
		nearest_distance = candidate;
		seed = m_source_native_points[i];
	    }
	}
	const double coordinate_scale = std::max(1.0, std::max(
	    std::max(std::fabs(target.x), std::fabs(target.y)),
	    std::fabs(target.z)));
	const double tolerance = 2048.0 *
	    std::numeric_limits<double>::epsilon() * coordinate_scale;
	bool mapped = seed.IsValid() &&
	    context.SurfaceClosestPointFromSeed(m_surface, target, seed,
		native_uv, lifted, distance, tolerance, NULL, NULL,
		tolerance);
	if (!mapped)
	    mapped = context.SurfaceClosestPoint(m_surface, target, native_uv,
		lifted, distance, 0, tolerance, tolerance);
	return mapped && native_uv.IsValid() &&
	    distance <= 4.0 * tolerance;
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
	native_uv[m_closed_dir] = width > tolerance ? m_polar_cut +
	    m_closed_domain.Length() * (chart_uv.x / width + 0.5) :
	    m_polar_cut;
	while (native_uv[m_closed_dir] < m_closed_domain.Min())
	    native_uv[m_closed_dir] += m_closed_domain.Length();
	while (native_uv[m_closed_dir] > m_closed_domain.Max())
	    native_uv[m_closed_dir] -= m_closed_domain.Length();
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

bool
cdt_face_chart::triangle_interior_sample(const long native_triangle[3],
	ON_2dPoint &native_uv) const
{
    if (!native_triangle)
	return false;
    ON_2dPoint triangle[3];
    for (int corner = 0; corner < 3; ++corner) {
	bool found = false;
	for (const cdt_chart_vertex &vertex : vertices) {
	    if (vertex.native_point != native_triangle[corner] ||
		    vertex.id < 0 || (size_t)vertex.id >= points.size())
		continue;
	    triangle[corner] = ON_2dPoint(
		points[(size_t)vertex.id].first,
		points[(size_t)vertex.id].second);
	    found = true;
	    break;
	}
	if (!found)
	    return false;
    }

    const long double abx = (long double)triangle[1].x - triangle[0].x;
    const long double aby = (long double)triangle[1].y - triangle[0].y;
    const long double acx = (long double)triangle[2].x - triangle[0].x;
    const long double acy = (long double)triangle[2].y - triangle[0].y;
    if (!(std::fabs(abx * acy - aby * acx) > 0.0L))
	return false;

    double opposite_length[3];
    double length_sum = 0.0;
    for (int corner = 0; corner < 3; ++corner) {
	const ON_2dPoint &first = triangle[(corner + 1) % 3];
	const ON_2dPoint &second = triangle[(corner + 2) % 3];
	opposite_length[corner] = first.DistanceTo(second);
	length_sum += opposite_length[corner];
    }
    if (!(length_sum > 0.0) || !std::isfinite(length_sum))
	return false;
    const ON_2dPoint chart_uv(
	(opposite_length[0] * triangle[0].x +
	 opposite_length[1] * triangle[1].x +
	 opposite_length[2] * triangle[2].x) / length_sum,
	(opposite_length[0] * triangle[0].y +
	 opposite_length[1] * triangle[1].y +
	 opposite_length[2] * triangle[2].y) / length_sum);
    return chart_to_native(chart_uv, native_uv);
}

int
cdt_face_chart::triangle_orientation(const long native_triangle[3]) const
{
    if (!native_triangle)
	return 0;
    ON_2dPoint triangle[3];
    for (int corner = 0; corner < 3; ++corner) {
	bool found = false;
	for (const cdt_chart_vertex &vertex : vertices) {
	    if (vertex.native_point != native_triangle[corner] ||
		    vertex.id < 0 || (size_t)vertex.id >= points.size())
		continue;
	    triangle[corner] = ON_2dPoint(
		points[(size_t)vertex.id].first,
		points[(size_t)vertex.id].second);
	    found = true;
	    break;
	}
	if (!found)
	    return 0;
    }
    const long double abx = (long double)triangle[1].x - triangle[0].x;
    const long double aby = (long double)triangle[1].y - triangle[0].y;
    const long double acx = (long double)triangle[2].x - triangle[0].x;
    const long double acy = (long double)triangle[2].y - triangle[0].y;
    const long double cross = abx * acy - aby * acx;
    return (cross > 0.0L) - (cross < 0.0L);
}

bool
cdt_face_chart::edge_midpoint_sample(const long native_edge[2],
	ON_2dPoint &native_uv, ON_2dPoint &chart_uv) const
{
    if (!native_edge)
	return false;
    int chart_vertex[2] = {-1, -1};
    ON_2dPoint edge[2];
    for (int endpoint = 0; endpoint < 2; ++endpoint) {
	for (const cdt_chart_vertex &vertex : vertices) {
	    if (vertex.native_point != native_edge[endpoint] ||
		    vertex.id < 0 || (size_t)vertex.id >= points.size())
		continue;
	    chart_vertex[endpoint] = (int)vertex.id;
	    edge[endpoint] = ON_2dPoint(points[(size_t)vertex.id].first,
		points[(size_t)vertex.id].second);
	    break;
	}
	if (chart_vertex[endpoint] < 0)
	    return false;
    }
    chart_uv = ON_2dPoint(0.5 * (edge[0].x + edge[1].x),
	0.5 * (edge[0].y + edge[1].y));
    return chart_to_native(chart_uv, native_uv);
}

void
cdt_face_chart::add_refinement_point(long source_point,
	const ON_2dPoint &native_uv, const ON_2dPoint &chart_uv,
	const long native_edge[2])
{
    if (source_point < 0 || !native_uv.IsValid() || !chart_uv.IsValid())
	return;
    int edge_vertices[2] = {-1, -1};
    if (native_edge) {
	for (int endpoint = 0; endpoint < 2; ++endpoint) {
	    for (const cdt_chart_vertex &candidate : vertices) {
		if (candidate.native_point == native_edge[endpoint]) {
		    edge_vertices[endpoint] = (int)candidate.id;
		    break;
		}
	    }
	}
    }
    cdt_chart_vertex vertex;
    vertex.id = (cdt_chart_vertex_id)points.size();
    vertex.native_point = source_point;
    points.push_back(std::make_pair(chart_uv.x, chart_uv.y));
    vertices.push_back(vertex);
    m_source_native_points.push_back(native_uv);
    if (edge_vertices[0] < 0 || edge_vertices[1] < 0)
	return;
    for (auto constraint = constraints.begin();
	    constraint != constraints.end(); ++constraint) {
	const bool forward = constraint->first == edge_vertices[0] &&
	    constraint->second == edge_vertices[1];
	const bool reverse = constraint->first == edge_vertices[1] &&
	    constraint->second == edge_vertices[0];
	if (!forward && !reverse)
	    continue;
	const int first = constraint->first;
	const int second = constraint->second;
	constraints.erase(constraint);
	constraints.push_back(std::make_pair(first, (int)vertex.id));
	constraints.push_back(std::make_pair((int)vertex.id, second));
	break;
    }
}

size_t
cdt_face_chart::repair_toleranced_edge_endpoint_samples(
	const std::vector<int> &native_path,
	const std::vector<const ON_3dPoint *> &points_3d,
	double tolerance)
{
    /* This operation changes only the planar triangulation embedding.  Native
     * UV and the shared 3-D B-Rep edge points remain authoritative.  Analytic,
     * polar, and singular charts require their own topology-specific handling;
     * a periodic metric or analytic-cylinder chart may additionally need an
     * interior edge sample separated from a seam. */
    if ((m_type != CDT_FACE_CHART_SURFACE_METRIC &&
	    m_type != CDT_FACE_CHART_CYLINDER) ||
	    native_path.size() < 3 || !(tolerance > 0.0) ||
	    !std::isfinite(tolerance))
	return 0;

    std::map<long, int> native_to_chart;
    std::set<long> ambiguous;
    for (const cdt_chart_vertex &vertex : vertices) {
	if (vertex.native_point < 0 || vertex.id < 0 ||
		(size_t)vertex.id >= points.size())
	    continue;
	const auto old = native_to_chart.find(vertex.native_point);
	if (old == native_to_chart.end())
	    native_to_chart[vertex.native_point] = (int)vertex.id;
	else if (old->second != vertex.id)
	    ambiguous.insert(vertex.native_point);
    }

    std::vector<int> path;
    path.reserve(native_path.size());
    for (int native : native_path) {
	const auto mapped = native_to_chart.find(native);
	if (native < 0 || (size_t)native >= points_3d.size() ||
		!points_3d[(size_t)native] ||
		!points_3d[(size_t)native]->IsValid() ||
		mapped == native_to_chart.end() ||
		ambiguous.find(native) != ambiguous.end())
	    return 0;
	path.push_back(mapped->second);
    }

    const auto same_point = [&](size_t first, size_t second) {
	return points[(size_t)path[first]] == points[(size_t)path[second]];
    };
    const auto source_distance = [&](size_t first, size_t second) {
	const int first_native = native_path[first];
	const int second_native = native_path[second];
	return points_3d[(size_t)first_native]->DistanceTo(
	    *points_3d[(size_t)second_native]);
    };
    const auto movable = [&](size_t first, size_t last) {
	for (size_t i = first; i < last; ++i) {
	    const cdt_chart_vertex &vertex = vertices[(size_t)path[i]];
	    if (vertex.topo_vertex != CDT_TOPOLOGY_ID_NONE ||
		    vertex.singular || vertex.seam_side)
		return false;
	}
	return true;
    };
    const auto redistribute = [&](size_t first, size_t last,
	    size_t movable_first, size_t movable_last,
	    double collapsed_length) {
	if (!(collapsed_length > 0.0) || collapsed_length > tolerance ||
		movable_first >= movable_last ||
		!movable(movable_first, movable_last))
	    return (size_t)0;
	/* Preserve the master edge's sampling proportions along the existing
	 * nonzero chart chord.  This turns zero-length endpoint constraints into
	 * a collinear subdivision without introducing a new boundary route. */
	std::vector<double> cumulative(last - first + 1, 0.0);
	for (size_t i = first + 1; i <= last; ++i) {
	    const double distance = source_distance(i - 1, i);
	    if (!std::isfinite(distance))
		return (size_t)0;
	    cumulative[i - first] = cumulative[i - first - 1] + distance;
	}
	const double total = cumulative.back();
	if (!(total > 0.0) || !std::isfinite(total))
	    return (size_t)0;
	const std::pair<double, double> start = points[(size_t)path[first]];
	const std::pair<double, double> finish = points[(size_t)path[last]];
	std::vector<std::pair<double, double>> replacements;
	replacements.reserve(movable_last - movable_first);
	for (size_t i = movable_first; i < movable_last; ++i) {
	    const double fraction = cumulative[i - first] / total;
	    if (!(fraction > 0.0 && fraction < 1.0))
		return (size_t)0;
	    const std::pair<double, double> replacement(
		start.first + fraction * (finish.first - start.first),
		start.second + fraction * (finish.second - start.second));
	    if (!std::isfinite(replacement.first) ||
		    !std::isfinite(replacement.second))
		return (size_t)0;
	    const std::pair<double, double> &original =
		points[(size_t)path[i]];
	    if (hypot(replacement.first - original.first,
		    replacement.second - original.second) > tolerance)
		return (size_t)0;
	    replacements.push_back(replacement);
	}
	for (size_t i = 0; i < replacements.size(); ++i) {
	    const size_t path_index = movable_first + i;
	    const std::pair<double, double> &replacement = replacements[i];
	    const std::pair<double, double> &previous = path_index == first ?
		points[(size_t)path[first]] :
		(i ? replacements[i - 1] : points[(size_t)path[first]]);
	    const std::pair<double, double> &next =
		path_index + 1 == last ? points[(size_t)path[last]] :
		(i + 1 < replacements.size() ? replacements[i + 1] :
		points[(size_t)path[last]]);
	    if (replacement == previous || replacement == next)
		return (size_t)0;
	}
	for (size_t i = 0; i < replacements.size(); ++i)
	    points[(size_t)path[movable_first + i]] = replacements[i];
	return replacements.size();
    };

    size_t repaired = 0;
    if (m_closed_dir >= 0) {
	const auto closed_coordinate = [&](size_t point) {
	    const std::pair<double, double> &coordinate =
		points[(size_t)path[point]];
	    return m_type == CDT_FACE_CHART_CYLINDER || !m_closed_dir ?
		coordinate.first : coordinate.second;
	};
	const double start_coordinate = closed_coordinate(0);
	const double end_coordinate = closed_coordinate(path.size() - 1);
	const double direction = end_coordinate - start_coordinate;
	const double direction_sign = direction > 0.0 ? 1.0 : -1.0;
	const double coordinate_scale = std::max(1.0, std::max(
	    std::fabs(start_coordinate), std::fabs(end_coordinate)));
	const double coordinate_tolerance = 4096.0 *
	    std::numeric_limits<double>::epsilon() * coordinate_scale;
	if (std::fabs(direction) <= coordinate_tolerance)
	    return 0;

	size_t first_distinct = 1;
	while (first_distinct < path.size() &&
		(closed_coordinate(first_distinct) - start_coordinate) *
		direction_sign <= coordinate_tolerance)
	    ++first_distinct;
	if (first_distinct > 1 && first_distinct < path.size()) {
	    double collapsed_length = 0.0;
	    for (size_t i = 1; i < first_distinct; ++i)
		collapsed_length += source_distance(i - 1, i);
	    repaired += redistribute(0, first_distinct, 1,
		first_distinct, collapsed_length);
	}

	size_t last_distinct = path.size() - 2;
	while (last_distinct > 0 &&
		(end_coordinate - closed_coordinate(last_distinct)) *
		direction_sign <= coordinate_tolerance)
	    --last_distinct;
	if (last_distinct + 1 < path.size() - 1) {
	    double collapsed_length = 0.0;
	    for (size_t i = last_distinct + 2; i < path.size(); ++i)
		collapsed_length += source_distance(i - 1, i);
	    repaired += redistribute(last_distinct, path.size() - 1,
		last_distinct + 1, path.size() - 1, collapsed_length);
	}
	return repaired;
    }

    size_t first_distinct = 1;
    while (first_distinct < path.size() &&
	    same_point(0, first_distinct))
	++first_distinct;
    if (first_distinct > 1 && first_distinct < path.size()) {
	double collapsed_length = 0.0;
	for (size_t i = 1; i < first_distinct; ++i)
	    collapsed_length += source_distance(i - 1, i);
	repaired += redistribute(0, first_distinct, 1, first_distinct,
	    collapsed_length);
    }

    size_t last_distinct = path.size() - 2;
    while (last_distinct > 0 &&
	    same_point(last_distinct, path.size() - 1))
	--last_distinct;
    if (last_distinct + 1 < path.size() - 1) {
	double collapsed_length = 0.0;
	for (size_t i = last_distinct + 2; i < path.size(); ++i)
	    collapsed_length += source_distance(i - 1, i);
	repaired += redistribute(last_distinct, path.size() - 1,
	    last_distinct + 1, path.size() - 1, collapsed_length);
    }
    return repaired;
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
	const std::vector<const ON_3dPoint *> &points_3d,
	const std::vector<cdt_topo_vertex_id> &topology_vertices)
{
    const ON_Surface *surface = face.SurfaceOf();
    if (!surface) {
	m_failure = "native chart has no source surface";
	return false;
    }

    m_type = CDT_FACE_CHART_SURFACE_METRIC;
    m_surface = surface;
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

    std::vector<double> lifted_closed(native_points.size(),
	std::numeric_limits<double>::quiet_NaN());
    m_closed_dir = surface->IsClosed(0) ? 0 :
	(surface->IsClosed(1) ? 1 : -1);
    m_periodic = m_closed_dir >= 0 && cdt_face_has_seam(face);
    if (m_periodic) {
	const double period = m_native_domain[m_closed_dir].Length();
	if (!(period > 0.0)) {
	    m_failure = "periodic metric chart has no valid period";
	    return false;
	}
	std::vector<double> canonical(native_points.size(), 0.0);
	std::vector<double> open_parameter(native_points.size(), 0.0);
	bool closed[2] = {surface->IsClosed(0), surface->IsClosed(1)};
	ON_Interval domains[2] = {surface->Domain(0), surface->Domain(1)};
	brlcad::PullbackContext context;
	for (size_t i = 0; i < native_points.size(); ++i) {
	    ON_2dPoint seed(native_points[i].first, native_points[i].second);
	    for (int direction = 0; direction < 2; ++direction) {
		if (closed[direction])
		    seed[direction] = periodic_parameter(seed[direction],
			domains[direction]);
		else
		    seed[direction] = std::max(domains[direction].Min(),
			std::min(domains[direction].Max(), seed[direction]));
	    }
	    ON_2dPoint projected = seed;
	    if (i < points_3d.size() && points_3d[i]) {
		ON_3dPoint lifted = ON_3dPoint::UnsetPoint;
		double distance = DBL_MAX;
		const ON_3dPoint &target = *points_3d[i];
		const double coordinate_scale = std::max(1.0, std::max(
		    std::max(std::fabs(target.x), std::fabs(target.y)),
		    std::fabs(target.z)));
		const double tolerance = std::max(
		    (double)BREP_EDGE_MISS_TOLERANCE, 4096.0 *
		    std::numeric_limits<double>::epsilon() *
		    coordinate_scale);
		if (!context.SurfaceClosestPointFromSeed(surface, target, seed,
			projected, lifted, distance, tolerance, closed,
			domains, tolerance))
		    projected = seed;
	    }
	    /* Closest-point projection returns an arbitrary periodic image.
	     * Keep that image in the same sheet as the authoritative p-curve
	     * parameter before ring lifting.  This matters for loose edges whose
	     * p-curve reaches one side of the native seam while the corresponding
	     * 3-D sample projects just inside the opposite side.  Canonicalizing
	     * those values independently turns a short boundary step into a
	     * period-long chord. */
	    const double native_parameter = m_closed_dir ?
		native_points[i].second : native_points[i].first;
	    canonical[i] = projected[m_closed_dir] +
		std::nearbyint((native_parameter -
		projected[m_closed_dir]) / period) * period;
	    open_parameter[i] = projected[1 - m_closed_dir];
	}
	const auto lift_ring = [&](const std::vector<int> &ring,
		double initial_anchor) {
	    double previous = initial_anchor;
	    bool have_previous = std::isfinite(previous);
	    for (int point : ring) {
		if (point < 0 || (size_t)point >= canonical.size())
		    continue;
		double parameter = canonical[(size_t)point];
		if (have_previous)
		    parameter += std::nearbyint((previous - parameter) /
			period) * period;
		lifted_closed[(size_t)point] = parameter;
		previous = parameter;
		have_previous = true;
	    }
	};
	const std::vector<int> oriented_outer = orient_periodic_closed_paths(
	    source_outer, canonical, points_3d,
	    m_native_domain[m_closed_dir], &open_parameter,
	    &topology_vertices);
	std::vector<std::vector<int>> oriented_holes;
	oriented_holes.reserve(source_holes.size());
	for (const std::vector<int> &hole : source_holes)
	    oriented_holes.push_back(orient_periodic_closed_paths(hole,
		canonical, points_3d, m_native_domain[m_closed_dir],
		&open_parameter, &topology_vertices));
	lift_ring(oriented_outer, std::numeric_limits<double>::quiet_NaN());
	double parameter_sum = 0.0;
	size_t parameter_count = 0;
	for (int point : oriented_outer) {
	    if (point < 0 || (size_t)point >= lifted_closed.size() ||
		    !std::isfinite(lifted_closed[(size_t)point]))
		continue;
	    parameter_sum += lifted_closed[(size_t)point];
	    parameter_count++;
	}
	const double parameter_center = parameter_count ?
	    parameter_sum / parameter_count :
	    m_native_domain[m_closed_dir].Mid();
	const double image_shift = std::nearbyint((
	    m_native_domain[m_closed_dir].Mid() - parameter_center) /
	    period) * period;
	for (double &parameter : lifted_closed) {
	    if (std::isfinite(parameter))
		parameter += image_shift;
	}
	const double image_anchor = parameter_center + image_shift;
	for (const std::vector<int> &hole : oriented_holes)
	    lift_ring(hole, image_anchor);
	for (size_t i = 0; i < native_points.size(); ++i) {
	    if (std::isfinite(lifted_closed[i]))
		continue;
	    lifted_closed[i] = canonical[i] + std::nearbyint((image_anchor -
		canonical[i]) / period) * period;
	}
	if (!normalize_ring(oriented_outer, native_points.size(), outer)) {
	    m_failure = "invalid periodic metric chart outline";
	    return false;
	}
	holes.resize(oriented_holes.size());
	for (size_t hi = 0; hi < oriented_holes.size(); ++hi) {
	    if (!normalize_ring(oriented_holes[hi], native_points.size(),
		    holes[hi])) {
		m_failure = "invalid periodic metric chart hole";
		return false;
	    }
	}
    }

    points.reserve(native_points.size());

    for (size_t i = 0; i < native_points.size(); ++i) {
	ON_2dPoint chart_point(native_points[i].first,
	    native_points[i].second);
	if (m_periodic)
	    chart_point[m_closed_dir] = lifted_closed[i];
	for (int direction = 0; direction < 2; ++direction)
	    chart_point[direction] = (chart_point[direction] -
		m_native_domain[direction].Min()) *
		m_metric_scale[direction];
	if (!chart_point.IsValid()) {
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
    if (!m_periodic && !normalize_ring(source_outer, points.size(), outer)) {
	m_failure = "invalid native-UV chart outline";
	return false;
    }
    if (!m_periodic)
	holes.resize(source_holes.size());
    for (size_t hi = 0; !m_periodic && hi < source_holes.size(); ++hi) {
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

size_t
cdt_face_chart::repair_cylinder_seam_holes()
{
    if (m_type != CDT_FACE_CHART_CYLINDER || !m_periodic ||
	    !(m_cylinder.circle.radius > 0.0) || holes.empty())
	return 0;

    const double period = 2.0 * ON_PI * m_cylinder.circle.radius;
    const double tolerance = std::max((double)BREP_EDGE_MISS_TOLERANCE,
	4096.0 * std::numeric_limits<double>::epsilon() * period);
    size_t candidate = holes.size();
    bool high_crossing = false;
    for (size_t hi = 0; hi < holes.size(); ++hi) {
	double minimum = std::numeric_limits<double>::infinity();
	double maximum = -std::numeric_limits<double>::infinity();
	for (int point : holes[hi]) {
	    if (point < 0 || (size_t)point >= points.size())
		return 0;
	    minimum = std::min(minimum, points[(size_t)point].first);
	    maximum = std::max(maximum, points[(size_t)point].first);
	}
	const bool crosses_low = minimum < -tolerance &&
	    maximum > tolerance && maximum < period - tolerance;
	const bool crosses_high = maximum > period + tolerance &&
	    minimum < period - tolerance && minimum > tolerance;
	if (!crosses_low && !crosses_high)
	    continue;
	if (candidate != holes.size())
	    return 0;
	candidate = hi;
	high_crossing = crosses_high;
    }
    if (candidate == holes.size())
	return 0;

    cdt_face_chart repaired(*this);
    std::vector<int> &hole = repaired.holes[candidate];
    if (high_crossing) {
	for (int point : hole)
	    repaired.points[(size_t)point].first -= period;
    }

    std::vector<size_t> seam_positions;
    for (size_t i = 0; i < hole.size(); ++i) {
	if (std::fabs(repaired.points[(size_t)hole[i]].first) <= tolerance)
	    seam_positions.push_back(i);
    }
    if (seam_positions.size() != 2)
	return 0;

    const auto arc = [&](size_t first, size_t last) {
	std::vector<int> result;
	for (size_t i = first;; i = (i + 1) % hole.size()) {
	    result.push_back(hole[i]);
	    if (i == last)
		break;
	}
	return result;
    };
    std::vector<int> first_arc = arc(seam_positions[0], seam_positions[1]);
    std::vector<int> second_arc = arc(seam_positions[1], seam_positions[0]);
    const auto interior_sign = [&](const std::vector<int> &path) {
	double sum = 0.0;
	for (size_t i = 1; i + 1 < path.size(); ++i)
	    sum += repaired.points[(size_t)path[i]].first;
	return (sum > 0.0) - (sum < 0.0);
    };
    std::vector<int> negative = interior_sign(first_arc) < 0 ?
	first_arc : second_arc;
    std::vector<int> positive = interior_sign(first_arc) > 0 ?
	first_arc : second_arc;
    if (negative == positive || negative.size() < 3 || positive.size() < 3)
	return 0;
    for (size_t i = 1; i + 1 < negative.size(); ++i) {
	if (repaired.points[(size_t)negative[i]].first > tolerance)
	    return 0;
    }
    for (size_t i = 1; i + 1 < positive.size(); ++i) {
	if (repaired.points[(size_t)positive[i]].first < -tolerance)
	    return 0;
    }

    for (int endpoint : {negative.front(), negative.back(),
	    positive.front(), positive.back()})
	repaired.points[(size_t)endpoint].first = 0.0;
    for (size_t i = 1; i + 1 < negative.size(); ++i)
	repaired.points[(size_t)negative[i]].first += period;

    const auto clone_at = [&](int source, double closed_coordinate) {
	cdt_chart_vertex clone = repaired.vertices[(size_t)source];
	clone.id = (cdt_chart_vertex_id)repaired.points.size();
	repaired.points.push_back(std::make_pair(closed_coordinate,
	    repaired.points[(size_t)source].second));
	repaired.vertices.push_back(clone);
	return (int)clone.id;
    };
    std::vector<int> high_arc;
    high_arc.reserve(negative.size());
    high_arc.push_back(clone_at(negative.front(), period));
    high_arc.insert(high_arc.end(), negative.begin() + 1,
	negative.end() - 1);
    high_arc.push_back(clone_at(negative.back(), period));

    const auto splice = [&](double side, std::vector<int> path) {
	const auto on_side = [&](int point) {
	    return std::fabs(repaired.points[(size_t)point].first - side) <=
		tolerance;
	};
	std::vector<size_t> starts;
	for (size_t i = 0; i < repaired.outer.size(); ++i) {
	    const size_t previous = (i + repaired.outer.size() - 1) %
		repaired.outer.size();
	    if (on_side(repaired.outer[i]) &&
		    !on_side(repaired.outer[previous]))
		starts.push_back(i);
	}
	if (starts.size() != 1)
	    return false;
	std::vector<int> rotated;
	rotated.reserve(repaired.outer.size() + path.size());
	for (size_t i = 0; i < repaired.outer.size(); ++i)
	    rotated.push_back(repaired.outer[(starts[0] + i) %
		repaired.outer.size()]);
	size_t run = 0;
	while (run < rotated.size() && on_side(rotated[run]))
	    ++run;
	if (run < 2 || run == rotated.size())
	    return false;
	const double direction = repaired.points[(size_t)rotated[run - 1]].second -
	    repaired.points[(size_t)rotated[0]].second;
	if (std::fabs(direction) <= tolerance)
	    return false;
	const double sign = direction > 0.0 ? 1.0 : -1.0;
	if ((repaired.points[(size_t)path.back()].second -
		repaired.points[(size_t)path.front()].second) * sign < 0.0)
	    std::reverse(path.begin(), path.end());
	const double enter = repaired.points[(size_t)path.front()].second;
	const double leave = repaired.points[(size_t)path.back()].second;
	std::vector<int> replacement;
	replacement.reserve(rotated.size() + path.size());
	bool inserted = false;
	for (size_t i = 0; i < run; ++i) {
	    const double open = repaired.points[(size_t)rotated[i]].second;
	    if ((open - enter) * sign < -tolerance)
		replacement.push_back(rotated[i]);
	    else if (!inserted) {
		replacement.insert(replacement.end(), path.begin(), path.end());
		inserted = true;
	    }
	    if ((open - leave) * sign > tolerance)
		replacement.push_back(rotated[i]);
	}
	if (!inserted)
	    return false;
	replacement.insert(replacement.end(), rotated.begin() + run,
	    rotated.end());
	repaired.outer.swap(replacement);
	return true;
    };

    if (!splice(period, high_arc) || !splice(0.0, positive))
	return 0;
    repaired.holes.erase(repaired.holes.begin() + candidate);
    *this = std::move(repaired);
    return 1;
}

bool
cdt_face_chart::build_cylinder(const ON_BrepFace &face,
	const std::vector<std::pair<double, double>> &native_points,
	const std::vector<int> &source_outer,
	const std::vector<std::vector<int>> &source_holes,
	const std::vector<int> &source_steiner,
	const std::vector<const ON_3dPoint *> &points_3d,
	const std::vector<cdt_topo_vertex_id> &topology_vertices,
	const ON_Cylinder &cylinder)
{
    const ON_Surface *surface = face.SurfaceOf();
    if (!surface || !cylinder.IsValid() ||
	    !(cylinder.circle.radius > 0.0)) {
	m_failure = "cylinder chart has no valid analytic surface";
	return false;
    }
    m_type = CDT_FACE_CHART_CYLINDER;
    m_surface = surface;
    m_cylinder = cylinder;

    const bool has_seam = cdt_face_has_seam(face);
    m_closed_dir = surface->IsClosed(0) ? 0 :
	(surface->IsClosed(1) ? 1 : -1);
    if (has_seam && m_closed_dir < 0) {
	m_failure = "cylinder chart seam requires a closed surface";
	return false;
    }
    if (m_closed_dir >= 0) {
	m_open_dir = 1 - m_closed_dir;
	m_closed_domain = surface->Domain(m_closed_dir);
	m_open_domain = surface->Domain(m_open_dir);
	m_periodic = true;
    }

    std::vector<double> analytic_angles(native_points.size(), 0.0);
    std::vector<double> analytic_heights(native_points.size(), 0.0);
    for (size_t i = 0; i < native_points.size(); ++i) {
	const ON_3dPoint point = i < points_3d.size() && points_3d[i] ?
	    *points_3d[i] : surface->PointAt(native_points[i].first,
	    native_points[i].second);
	if (!cylinder_chart_coordinates(cylinder, point,
		&analytic_angles[i], &analytic_heights[i])) {
	    m_failure = "native point could not be mapped to its cylinder";
	    return false;
	}
    }

    std::vector<double> boundary_angles;
    const auto collect_angles = [&](const std::vector<int> &ring) {
	for (int point : ring) {
	    if (point >= 0 && (size_t)point < analytic_angles.size())
		boundary_angles.push_back(analytic_angles[(size_t)point]);
	}
    };
    collect_angles(source_outer);
    for (const std::vector<int> &hole : source_holes)
	collect_angles(hole);
    if (boundary_angles.empty()) {
	m_failure = "cylinder chart has no boundary angles";
	return false;
    }
    if (has_seam) {
	ON_2dPoint seam_uv;
	seam_uv[m_closed_dir] = m_closed_domain.Min();
	seam_uv[m_open_dir] = m_open_domain.Mid();
	double seam_height = 0.0;
	if (!cylinder_chart_coordinates(cylinder,
		surface->PointAt(seam_uv.x, seam_uv.y), &m_cylinder_cut,
		&seam_height)) {
	    m_failure = "cylinder seam could not be mapped analytically";
	    return false;
	}
	ON_2dPoint quarter_uv = seam_uv;
	quarter_uv[m_closed_dir] = m_closed_domain.ParameterAt(0.25);
	double quarter_angle = 0.0;
	double quarter_height = 0.0;
	if (!cylinder_chart_coordinates(cylinder,
		surface->PointAt(quarter_uv.x, quarter_uv.y), &quarter_angle,
		&quarter_height)) {
	    m_failure = "cylinder orientation sample could not be mapped";
	    return false;
	}
	double delta = quarter_angle - m_cylinder_cut;
	while (delta < 0.0)
	    delta += 2.0 * ON_PI;
	while (delta >= 2.0 * ON_PI)
	    delta -= 2.0 * ON_PI;
	m_cylinder_orientation = delta <= ON_PI ? 1 : -1;
    } else {
	std::sort(boundary_angles.begin(), boundary_angles.end());
	boundary_angles.erase(std::unique(boundary_angles.begin(),
	    boundary_angles.end()), boundary_angles.end());
	double largest_gap = -1.0;
	for (size_t i = 0; i < boundary_angles.size(); ++i) {
	    const size_t next = (i + 1) % boundary_angles.size();
	    const double next_angle = next ? boundary_angles[next] :
		boundary_angles[0] + 2.0 * ON_PI;
	    const double gap = next_angle - boundary_angles[i];
	    if (gap > largest_gap) {
		largest_gap = gap;
		m_cylinder_cut = std::fmod(boundary_angles[i] +
		    0.5 * gap, 2.0 * ON_PI);
	    }
	}
    }

    std::map<const ON_3dPoint *, int> seam_images;
    if (has_seam) {
	const double seam_tolerance = parameter_tolerance(m_closed_domain);
	for (size_t i = 0; i < native_points.size() &&
		i < points_3d.size(); ++i) {
	    if (!points_3d[i])
		continue;
	    const double native_closed = m_closed_dir ?
		native_points[i].second : native_points[i].first;
	    if (std::fabs(native_closed - m_closed_domain.Min()) <=
		    seam_tolerance)
		seam_images[points_3d[i]] |= 1;
	    if (std::fabs(native_closed - m_closed_domain.Max()) <=
		    seam_tolerance)
		seam_images[points_3d[i]] |= 2;
	}
    }

    points.reserve(native_points.size());
    for (size_t i = 0; i < native_points.size(); ++i) {
	double unwrapped = m_cylinder_orientation > 0 ?
	    analytic_angles[i] - m_cylinder_cut :
	    m_cylinder_cut - analytic_angles[i];
	while (unwrapped < 0.0)
	    unwrapped += 2.0 * ON_PI;
	while (unwrapped >= 2.0 * ON_PI)
	    unwrapped -= 2.0 * ON_PI;
	if (has_seam) {
	    const double native_closed = m_closed_dir ?
		native_points[i].second : native_points[i].first;
	    const double seam_tolerance =
		parameter_tolerance(m_closed_domain);
	    const bool topology_endpoint = i < topology_vertices.size() &&
		topology_vertices[i] != CDT_TOPOLOGY_ID_NONE;
	    const auto images = i < points_3d.size() && points_3d[i] ?
		seam_images.find(points_3d[i]) : seam_images.end();
	    const bool paired_seam_sample = images != seam_images.end() &&
		images->second == 3;
	    /* The two native parameter bounds are distinct sides of the
	     * topological seam even when an imported NURBS cylinder only agrees
	     * with its fitted analytic cylinder within modeling tolerance.
	     * Topology endpoints and paired 3-D seam samples make that side
	     * authoritative.  An unpaired ordinary-edge sample may merely have a
	     * loose p-curve on the bound; keep its analytic angle in the periodic
	     * image nearest the p-curve so the edge path can lift it continuously. */
	    if ((topology_endpoint || paired_seam_sample) &&
		    std::fabs(native_closed -
		    m_closed_domain.Min()) <= seam_tolerance)
		unwrapped = 0.0;
	    else if ((topology_endpoint || paired_seam_sample) &&
		    std::fabs(native_closed -
		    m_closed_domain.Max()) <= seam_tolerance)
		unwrapped = 2.0 * ON_PI;
	    else {
		const double native_phase = (native_closed -
		    m_closed_domain.Min()) / m_closed_domain.Length() *
		    2.0 * ON_PI;
		unwrapped += std::nearbyint((native_phase - unwrapped) /
		    (2.0 * ON_PI)) * 2.0 * ON_PI;
	    }
	}
	points.push_back(std::make_pair(cylinder.circle.radius * unwrapped,
	    analytic_heights[i]));
    }
    vertices.resize(points.size());
    for (size_t i = 0; i < points.size(); ++i) {
	vertices[i].id = (cdt_chart_vertex_id)i;
	vertices[i].native_point = (long)i;
	vertices[i].topo_vertex = topology_vertices[i];
    }
    std::vector<int> oriented_outer(source_outer);
    std::vector<std::vector<int>> oriented_holes(source_holes);
    if (has_seam) {
	std::vector<double> angular(points.size(), 0.0);
	for (size_t i = 0; i < points.size(); ++i)
	    angular[i] = points[i].first / cylinder.circle.radius;
	const ON_Interval angular_domain(0.0, 2.0 * ON_PI);
	oriented_outer = orient_periodic_closed_paths(source_outer, angular,
	    points_3d, angular_domain, &analytic_heights,
	    &topology_vertices);
	for (size_t hi = 0; hi < source_holes.size(); ++hi)
	    oriented_holes[hi] = orient_periodic_closed_paths(
		source_holes[hi], angular, points_3d, angular_domain,
		&analytic_heights, &topology_vertices);
	for (size_t i = 0; i < points.size(); ++i)
	    points[i].first = cylinder.circle.radius * angular[i];
	} else if (m_closed_dir >= 0) {
	/* A boundary without an explicit seam trim can still cross an
	 * arbitrary analytic cylinder cut many times.  Lift each connected
	 * ring through adjacent periodic images instead of turning every cut
	 * crossing into a circumference-long chord.  A contractible ring must
	 * return to its initial image; a nonzero winding needs a real atlas cut
	 * and cannot be represented by this single disk chart. */
	const double period = 2.0 * ON_PI;
	std::vector<double> angular(points.size(), 0.0);
	std::vector<bool> boundary_point(points.size(), false);
	for (size_t i = 0; i < points.size(); ++i)
	    angular[i] = points[i].first / cylinder.circle.radius;
	const auto lift_ring = [&](const std::vector<int> &source,
		double anchor, double *center) {
	    std::vector<int> ring(source);
	    while (ring.size() > 1 && ring.front() == ring.back())
		ring.pop_back();
	    if (ring.size() < 3)
		return false;
	    const int first = ring.front();
	    if (first < 0 || (size_t)first >= angular.size())
		return false;
	    if (std::isfinite(anchor))
		angular[(size_t)first] += std::nearbyint((anchor -
		    angular[(size_t)first]) / period) * period;
	    boundary_point[(size_t)first] = true;
	    double sum = angular[(size_t)first];
	    for (size_t i = 1; i < ring.size(); ++i) {
		const int previous = ring[i - 1];
		const int current = ring[i];
		if (previous < 0 || current < 0 ||
			(size_t)previous >= angular.size() ||
			(size_t)current >= angular.size())
		    return false;
		angular[(size_t)current] += std::nearbyint((
		    angular[(size_t)previous] - angular[(size_t)current]) /
		    period) * period;
		boundary_point[(size_t)current] = true;
		sum += angular[(size_t)current];
	    }
	    const int last = ring.back();
	    const double winding = std::nearbyint((angular[(size_t)last] -
		angular[(size_t)first]) / period);
	    if (std::fabs(winding) > 0.5)
		return false;
	    if (center)
		*center = sum / ring.size();
	    return true;
	};
	double chart_center = 0.0;
	if (!lift_ring(oriented_outer,
		std::numeric_limits<double>::quiet_NaN(), &chart_center)) {
	    m_failure = "cylinder outline has nonzero periodic winding";
	    return false;
	}
	for (const std::vector<int> &hole : oriented_holes) {
	    if (!lift_ring(hole, chart_center, NULL)) {
		m_failure = "cylinder hole has nonzero periodic winding";
		return false;
	    }
	}
	for (size_t i = 0; i < angular.size(); ++i) {
	    if (!boundary_point[i])
		angular[i] += std::nearbyint((chart_center - angular[i]) /
		    period) * period;
	    points[i].first = cylinder.circle.radius * angular[i];
	}
    }
    if (!normalize_ring(oriented_outer, points.size(), outer)) {
	m_failure = "invalid cylinder chart outline";
	return false;
    }
    holes.resize(oriented_holes.size());
    for (size_t hi = 0; hi < oriented_holes.size(); ++hi) {
	if (!normalize_ring(oriented_holes[hi], points.size(), holes[hi])) {
	    m_failure = "invalid cylinder chart hole";
	    return false;
	}
    }
    const size_t repaired_cut_holes = has_seam ?
	repair_cylinder_seam_holes() : 0;
    if (repaired_cut_holes)
	bu_log("Face %d: opened %zu cylinder hole%s across the chart seam\n",
	    face.m_face_index, repaired_cut_holes,
	    repaired_cut_holes == 1 ? "" : "s");
    std::set<int> unique_steiner;
    for (int point : source_steiner) {
	if (point < 0 || (size_t)point >= points.size()) {
	    m_failure = "invalid cylinder chart Steiner point";
	    return false;
	}
	if (repaired_cut_holes) {
	    const double period = 2.0 * ON_PI * m_cylinder.circle.radius;
	    while (points[(size_t)point].first < 0.0)
		points[(size_t)point].first += period;
	    while (points[(size_t)point].first > period)
		points[(size_t)point].first -= period;
	}
	if (unique_steiner.insert(point).second)
	    steiner.push_back(point);
    }
    return validate_boundary(face, native_points);
}

static void
orient_pole_wedge_seam_runs(const std::vector<int> &source_outer,
	const std::vector<int> &source_to_chart,
	std::vector<std::pair<double, double>> &points,
	std::vector<cdt_chart_vertex> &vertices)
{
    size_t ring_size = source_outer.size();
    if (ring_size > 1 && source_outer.front() == source_outer.back())
	--ring_size;
    if (!ring_size)
	return;
    const auto chart_at = [&](size_t position) {
	const int source_point = source_outer[position];
	if (source_point < 0 ||
		(size_t)source_point >= source_to_chart.size())
	    return -1;
	const int chart_point = source_to_chart[(size_t)source_point];
	return chart_point >= 0 && (size_t)chart_point < vertices.size() &&
		(size_t)chart_point < points.size() ? chart_point : -1;
    };
    const auto seam_side_at = [&](size_t position) {
	const int chart_point = chart_at(position);
	return chart_point >= 0 ?
	    vertices[(size_t)chart_point].seam_side : 0;
    };
    for (size_t start = 0; start < ring_size; ++start) {
	const size_t previous_position =
	    (start + ring_size - 1) % ring_size;
	const int native_side = seam_side_at(start);
	if (!native_side ||
		seam_side_at(previous_position) == native_side)
	    continue;

	size_t last = start;
	while (seam_side_at((last + 1) % ring_size) == native_side &&
		(last + 1) % ring_size != start)
	    last = (last + 1) % ring_size;
	const size_t next_position = (last + 1) % ring_size;
	const int previous = chart_at(previous_position);
	const int next = chart_at(next_position);
	const int first_chart = chart_at(start);
	const int last_chart = chart_at(last);
	if (previous < 0 || next < 0 || first_chart < 0 ||
		last_chart < 0)
	    continue;
	const bool use_previous = !seam_side_at(previous_position);
	const bool use_next = !seam_side_at(next_position);

	const auto side_cost = [&](int side) {
	    const double first_x = 0.5 * side *
		points[(size_t)first_chart].second;
	    const double last_x = 0.5 * side *
		points[(size_t)last_chart].second;
	    const double first_dx = first_x -
		points[(size_t)previous].first;
	    const double first_dy = points[(size_t)first_chart].second -
		points[(size_t)previous].second;
	    const double last_dx = last_x - points[(size_t)next].first;
	    const double last_dy = points[(size_t)last_chart].second -
		points[(size_t)next].second;
	    return (use_previous ?
		first_dx * first_dx + first_dy * first_dy : 0.0) +
		(use_next ?
		last_dx * last_dx + last_dy * last_dy : 0.0);
	};
	const double low_cost = side_cost(-1);
	const double high_cost = side_cost(1);
	const double scale = std::max(1.0,
	    std::max(std::fabs(low_cost), std::fabs(high_cost)));
	int side = vertices[(size_t)first_chart].seam_side;
	if (std::fabs(low_cost - high_cost) >
		512.0 * DBL_EPSILON * scale)
	    side = low_cost < high_cost ? -1 : 1;
	size_t position = start;
	do {
	    const int mapped = chart_at(position);
	    vertices[(size_t)mapped].seam_side = side;
	    points[(size_t)mapped].first = 0.5 * side *
		points[(size_t)mapped].second;
	    if (position == last)
		break;
	    position = (position + 1) % ring_size;
	} while (position != start);
    }
}

int
cdt_test_pole_wedge_seam_orientation(void)
{
    const auto exercise = [](bool reversed) {
	std::vector<int> outer = {0, 1, 2, 3, 4, 5, 6, 7, 0};
	std::vector<int> source_to_chart = {0, 1, 2, 3, 4, 5, 6, 7};
	std::vector<std::pair<double, double>> points = {
	    std::make_pair(-0.5, 1.0),
	    std::make_pair(reversed ? 0.4375 : -0.4375, 1.0),
	    std::make_pair(0.0, 1.0),
	    std::make_pair(reversed ? -0.4375 : 0.4375, 1.0),
	    std::make_pair(0.5, 1.0),
	    std::make_pair(0.25, 0.5),
	    std::make_pair(0.0, 0.0),
	    std::make_pair(-0.25, 0.5)
	};
	std::vector<cdt_chart_vertex> vertices(points.size());
	for (size_t i = 0; i < vertices.size(); ++i)
	    vertices[i].id = (cdt_chart_vertex_id)i;
	vertices[0].seam_side = -1;
	vertices[4].seam_side = 1;
	vertices[5].seam_side = 1;
	vertices[7].seam_side = -1;
	orient_pole_wedge_seam_runs(outer, source_to_chart, points,
	    vertices);
	const int left_side = reversed ? 1 : -1;
	const int right_side = -left_side;
	return vertices[0].seam_side == left_side &&
	    vertices[7].seam_side == left_side &&
	    vertices[4].seam_side == right_side &&
	    vertices[5].seam_side == right_side &&
	    std::fabs(points[0].first - 0.5 * left_side) <= DBL_EPSILON &&
	    std::fabs(points[4].first - 0.5 * right_side) <= DBL_EPSILON;
    };
    if (!exercise(false))
	return 1;
    if (!exercise(true))
	return 2;
    return 0;
}

bool
cdt_face_chart::build_pole_wedge(const ON_BrepFace &face,
	const std::vector<std::pair<double, double>> &native_points,
	const std::vector<int> &source_outer,
	const std::vector<std::vector<int>> &source_holes,
	const std::vector<int> &source_steiner,
	const std::vector<int> &source_refinement,
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
    const std::set<int> refinement_points(source_refinement.begin(),
	source_refinement.end());
    if (ruled) {
	for (int point : source_steiner) {
	    if (point >= 0 && (size_t)point < inactive_ruled_point.size() &&
		    refinement_points.find(point) == refinement_points.end())
		inactive_ruled_point[(size_t)point] = true;
	}
	for (int point : source_outer) {
	    if (point >= 0 && (size_t)point < inactive_ruled_point.size())
		inactive_ruled_point[(size_t)point] = false;
	}
    }

    std::set<int> active_points(source_outer.begin(), source_outer.end());
    for (const std::vector<int> &hole : source_holes)
	active_points.insert(hole.begin(), hole.end());
    active_points.insert(source_steiner.begin(), source_steiner.end());
    active_points.insert(source_refinement.begin(),
	source_refinement.end());
    for (int point : active_points) {
	if (point < 0 || (size_t)point >= native_points.size())
	    continue;
	const size_t i = (size_t)point;
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

    /* A pullback on a periodic surface may use different equivalent images
     * for an interior edge sample and its nominal trim endpoint.  Mapping
     * each point independently into the native period can therefore reverse
     * and retrace an otherwise ordered boundary.  Lift the complete sphere
     * boundary continuously, resetting only at the pole where longitude is
     * genuinely undefined.  Analytic cone mapping already selects its lift
     * from the authoritative 3-D point below. */
    std::vector<double> lifted_closed(native_points.size(),
	std::numeric_limits<double>::quiet_NaN());
    if (!ruled && m_periodic && cdt_face_has_seam(face)) {
	const double period = m_closed_domain.Length();
	const auto is_pole_source = [&](int point) {
	    if (point < 0 || (size_t)point >= native_points.size())
		return false;
	    const ON_2dPoint uv(native_points[(size_t)point].first,
		native_points[(size_t)point].second);
	    if (topology_vertices[(size_t)point] == m_pole_topology_vertex)
		return true;
	    return topology_vertices[(size_t)point] ==
		CDT_TOPOLOGY_ID_NONE &&
		std::fabs(uv[m_open_dir] - pole_coordinate) <=
		pole_tolerance;
	};
	const auto lift_ring = [&](const std::vector<int> &ring,
		double initial_anchor) {
	    double previous = initial_anchor;
	    bool have_previous = std::isfinite(previous);
	    for (int point : ring) {
		if (point < 0 || (size_t)point >= native_points.size())
		    continue;
		if (is_pole_source(point)) {
		    have_previous = false;
		    continue;
		}
		double angular = m_closed_dir == 0 ?
		    native_points[(size_t)point].first :
		    native_points[(size_t)point].second;
		if (std::isfinite(lifted_closed[(size_t)point]))
		    angular = lifted_closed[(size_t)point];
		else if (have_previous)
		    angular += std::nearbyint((previous - angular) / period) *
			period;
		lifted_closed[(size_t)point] = angular;
		previous = angular;
		have_previous = true;
	    }
	};

	lift_ring(source_outer, std::numeric_limits<double>::quiet_NaN());
	double angular_sum = 0.0;
	size_t angular_count = 0;
	for (int point : source_outer) {
	    if (point < 0 || (size_t)point >= lifted_closed.size() ||
		    !std::isfinite(lifted_closed[(size_t)point]))
		continue;
	    angular_sum += lifted_closed[(size_t)point];
	    angular_count++;
	}
	const double angular_center = angular_count ?
	    angular_sum / angular_count : m_closed_domain.Mid();
	const double image_shift = std::nearbyint((m_closed_domain.Mid() -
	    angular_center) / period) * period;
	for (double &angular : lifted_closed) {
	    if (std::isfinite(angular))
		angular += image_shift;
	}
	const double image_anchor = angular_center + image_shift;
	for (const std::vector<int> &hole : source_holes)
	    lift_ring(hole, image_anchor);
	for (size_t i = 0; i < native_points.size(); ++i) {
	    if (std::isfinite(lifted_closed[i]) || is_pole_source((int)i))
		continue;
	    double angular = m_closed_dir == 0 ? native_points[i].first :
		native_points[i].second;
	    angular += std::nearbyint((image_anchor - angular) / period) *
		period;
	    lifted_closed[i] = angular;
	}
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
	const bool authoritative_pole_copy = m_authoritative_cone_points &&
	    topology_vertices[i] == CDT_TOPOLOGY_ID_NONE &&
	    i < points_3d.size() && pole_source < points_3d.size() &&
	    points_3d[i] && points_3d[pole_source] &&
	    points_3d[i] == points_3d[pole_source];
	bool at_pole = topology_vertices[i] == m_pole_topology_vertex;
	if (topology_vertices[i] == CDT_TOPOLOGY_ID_NONE) {
	    at_pole = m_authoritative_cone_points ?
		authoritative_pole_copy :
		std::fabs(native_uv[m_open_dir] - pole_coordinate) <=
		pole_tolerance;
	}
	if (at_pole) {
	    source_to_chart[i] = apex;
	    continue;
	}
	ON_2dPoint chart_uv;
	bool mapped_to_chart = native_to_chart(native_uv, chart_uv);
	if (mapped_to_chart && ruled && m_authoritative_cone_points &&
		m_cone.IsValid() &&
		i < points_3d.size() && points_3d[i]) {
	    double angle = 0.0;
	    double height = 0.0;
	    mapped_to_chart = m_cone.ClosestPointTo(*points_3d[i],
		&angle, &height) && std::fabs(m_cone.height) > DBL_MIN;
	    if (mapped_to_chart) {
		double turn = m_cone_orientation > 0 ?
		    angle - m_cone_seam_angle :
		    m_cone_seam_angle - angle;
		while (turn < 0.0)
		    turn += 2.0 * ON_PI;
		while (turn >= 2.0 * ON_PI)
		    turn -= 2.0 * ON_PI;
		turn /= 2.0 * ON_PI;
		const double desired =
		    (native_uv[m_closed_dir] - m_closed_domain.Min()) /
		    m_closed_domain.Length();
		turn += std::nearbyint(desired - turn);
		const double radial = height / m_cone.height;
		chart_uv.x = radial * (turn - 0.5);
		chart_uv.y = radial;
		mapped_to_chart = chart_uv.IsValid();
	    }
	}
	if (!mapped_to_chart) {
	    m_failure = std::string(chart_name) +
		" point lies outside the chart domain";
	    return false;
	}
	if (!ruled && m_periodic &&
		std::isfinite(lifted_closed[i])) {
	    chart_uv.x = chart_uv.y * (lifted_closed[i] -
		m_closed_domain.Mid()) / m_closed_domain.Length();
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
	/* Analytic closest-point angles are periodic and may select the same
	 * equivalent image for both copies of a seam sample.  The native domain
	 * bound is the authoritative side of the cut wedge. */
	if (vertex.seam_side)
	    points[(size_t)chart_index].first =
		0.5 * vertex.seam_side * chart_uv.y;
	vertices.push_back(vertex);
	source_to_chart[i] = chart_index;
    }

    /* A trim on a periodic cone or sphere may start on a native bound and
     * then continue through an equivalent unwrapped image outside that
     * domain.  The bound identifies a possible cut side, but it does not
     * identify which side contains this face.  Orient each outer seam run
     * toward its adjacent non-seam boundary samples.  This preserves the
     * native side when both choices are equivalent (notably at the pole),
     * while keeping partial pole wedges continuous across the arbitrary
     * analytic cut. */
    if (m_periodic)
	orient_pole_wedge_seam_runs(source_outer, source_to_chart, points,
	    vertices);

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
    std::set<int> unique_steiner;
    for (int point : source_steiner) {
	if (ruled && refinement_points.find(point) ==
		refinement_points.end())
	    continue;
	const int mapped = source_to_chart[(size_t)point];
	if (mapped != apex && unique_steiner.insert(mapped).second)
	    steiner.push_back(mapped);
    }

    /* A one-pole chart is a disk cut open along a seam.  Without interior
     * rays, a planar triangulation may use only the triangle between the pole
     * and the two seam copies adjacent to it.  Those copies are one 3-D
     * point, so the triangle disappears when the chart is mapped back to the
     * surface and leaves the pole uncovered.  Constrain every unobstructed
     * radial connection from the pole.  This is the natural ruling for a
     * cone and a topology-preserving fan for a spherical cap. */
    if (holes.empty() && (ruled || cdt_face_has_seam(face))) {
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
	const std::vector<int> &source_refinement,
	const std::vector<const ON_3dPoint *> &points_3d,
	const std::vector<cdt_topo_vertex_id> &topology_vertices)
{
    m_type = CDT_FACE_CHART_CONE_WEDGE;
    const ON_Surface *surface = face.SurfaceOf();
    if (!surface || !surface->IsCone(&m_cone, CDT_CONIC_TOLERANCE) ||
	    !m_cone.IsValid()) {
	m_failure = "cone chart has no valid analytic surface";
	return false;
    }
    ON_2dPoint native;
    native[m_closed_dir] = m_closed_domain.Min();
    native[m_open_dir] = m_open_domain.Mid();
    double base_height = 0.0;
    if (!m_cone.ClosestPointTo(surface->PointAt(native.x, native.y),
	    &m_cone_seam_angle, &base_height)) {
	m_failure = "cone chart could not establish its analytic seam";
	return false;
    }
    native[m_closed_dir] = m_closed_domain.ParameterAt(0.01);
    double next_angle = 0.0;
    double next_height = 0.0;
    if (!m_cone.ClosestPointTo(surface->PointAt(native.x, native.y),
	    &next_angle, &next_height)) {
	m_failure = "cone chart could not establish its analytic orientation";
	return false;
    }
    double angular_delta = next_angle - m_cone_seam_angle;
    while (angular_delta <= -ON_PI)
	angular_delta += 2.0 * ON_PI;
    while (angular_delta > ON_PI)
	angular_delta -= 2.0 * ON_PI;
    m_cone_orientation = angular_delta < 0.0 ? -1 : 1;
    return build_pole_wedge(face, native_points, source_outer, source_holes,
	source_steiner, source_refinement, points_3d, topology_vertices, true);
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
	    source_holes, source_steiner, std::vector<int>(), points_3d,
	    topology_vertices, false);

    m_polar_cut = m_closed_domain.Min();

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

    /* A valid closed surface can arrive with two seam trims whose p-curves
     * both lie on the same arbitrary periodic image.  When the complete
     * low-to-high paths contain the same authoritative 3-D samples in reverse
     * order, they are the two sides of the chart cut.  Put them on opposite
     * polar boundaries without changing their native UV or model points. */
    bool reconstructed_paired_seam = false;
    if (m_periodic && source_holes.empty()) {
	std::vector<int> ring(source_outer);
	while (ring.size() > 1 && ring.front() == ring.back())
	    ring.pop_back();
	std::vector<size_t> low_positions;
	std::vector<size_t> high_positions;
	for (size_t i = 0; i < ring.size(); ++i) {
	    const int source = ring[i];
	    if (source < 0 || (size_t)source >= source_to_chart.size())
		continue;
	    const int mapped = source_to_chart[(size_t)source];
	    if (mapped == low_pole)
		low_positions.push_back(i);
	    if (mapped == high_pole)
		high_positions.push_back(i);
	}
	if (low_positions.size() == 1 && high_positions.size() == 1) {
	    const auto ring_path = [&](size_t first, size_t last) {
		std::vector<int> path;
		for (size_t i = first;; i = (i + 1) % ring.size()) {
		    path.push_back(ring[i]);
		    if (i == last)
			break;
		}
		return path;
	    };
	    const std::vector<int> first_path = ring_path(
		low_positions[0], high_positions[0]);
	    const std::vector<int> second_path = ring_path(
		high_positions[0], low_positions[0]);
	    bool paired = first_path.size() == second_path.size() &&
		first_path.size() > 2;
	    for (size_t i = 1; paired && i + 1 < first_path.size(); ++i) {
		const int first_source = first_path[i];
		const int second_source =
		    second_path[second_path.size() - 1 - i];
		if (first_source < 0 || second_source < 0 ||
			(size_t)first_source >= points_3d.size() ||
			(size_t)second_source >= points_3d.size() ||
			!points_3d[(size_t)first_source] ||
			!points_3d[(size_t)second_source] ||
			points_3d[(size_t)first_source] !=
			points_3d[(size_t)second_source])
		    paired = false;
	    }
	    if (paired) {
		const int seam_source = first_path[1];
		const ON_2dPoint seam_uv(
		    native_points[(size_t)seam_source].first,
		    native_points[(size_t)seam_source].second);
		m_polar_cut = periodic_parameter(seam_uv[m_closed_dir],
		    m_closed_domain);
		/* Recenter every ordinary surface sample on the recovered cut.
		 * The stored seam may be anywhere in the periodic domain, not
		 * necessarily at its native minimum. */
		for (cdt_chart_vertex &vertex : vertices) {
		    if (vertex.singular || vertex.native_point < 0)
			continue;
		    const size_t source = (size_t)vertex.native_point;
		    if (source >= native_points.size())
			continue;
		    ON_2dPoint remapped;
		    if (!native_to_chart(ON_2dPoint(
			    native_points[source].first,
			    native_points[source].second), remapped)) {
			m_failure = "sphere point could not be recentered on its "
			    "seam";
			return false;
		    }
		    points[(size_t)vertex.id] =
			std::make_pair(remapped.x, remapped.y);
		}
		for (size_t i = 1; i + 1 < first_path.size(); ++i) {
		    const int sources[2] = {
			first_path[i],
			second_path[second_path.size() - 1 - i]
		    };
		    for (int side = 0; side < 2; ++side) {
			const int mapped =
			    source_to_chart[(size_t)sources[side]];
			const double width =
			    1.0 - std::fabs(points[(size_t)mapped].second);
			points[(size_t)mapped].first =
			    (side ? 0.5 : -0.5) * width;
			vertices[(size_t)mapped].seam_side =
			    side ? 1 : -1;
			vertices[(size_t)mapped].edge_sample =
			    (cdt_edge_sample_id)(i - 1);
		    }
		}
		reconstructed_paired_seam = true;
	    }
	}
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
    if (holes.empty() && !ray_count && !reconstructed_paired_seam) {
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
	if (m_authoritative_cone_points) {
	    if ((size_t)first >= points.size() ||
		    (size_t)second >= points.size())
		return true;
	    return points[(size_t)first] == points[(size_t)second];
	}
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
cdt_face_chart::partition_components(
	std::vector<cdt_face_chart> &components, std::string *failure) const
{
    /* Interpret nested loops with the even-odd fill rule.  This recovers
     * valid faces with disjoint outer regions and hole-within-hole islands
     * without changing boundary points or source identities. */
    components.clear();
    if (failure)
	failure->clear();
    if (outer.size() < 3) {
	if (failure)
	    *failure = "chart has no valid component outline";
	return false;
    }

    std::vector<std::vector<int>> rings;
    rings.reserve(holes.size() + 1);
    rings.push_back(outer);
    rings.insert(rings.end(), holes.begin(), holes.end());
    for (const std::vector<int> &ring : rings) {
	if (ring.size() < 3) {
	    if (failure)
		*failure = "chart has an invalid component boundary";
	    return false;
	}
	for (int point : ring) {
	    if (point < 0 || (size_t)point >= points.size()) {
		if (failure)
		    *failure = "chart component boundary index is invalid";
		return false;
	    }
	}
    }

    struct ring_bounds {
	double min_x = DBL_MAX;
	double min_y = DBL_MAX;
	double max_x = -DBL_MAX;
	double max_y = -DBL_MAX;
    };
    std::vector<ring_bounds> bounds(rings.size());
    for (size_t ring = 0; ring < rings.size(); ++ring) {
	for (int point : rings[ring]) {
	    bounds[ring].min_x = std::min(bounds[ring].min_x,
		points[(size_t)point].first);
	    bounds[ring].min_y = std::min(bounds[ring].min_y,
		points[(size_t)point].second);
	    bounds[ring].max_x = std::max(bounds[ring].max_x,
		points[(size_t)point].first);
	    bounds[ring].max_y = std::max(bounds[ring].max_y,
		points[(size_t)point].second);
	}
    }

    const auto orient = [&](int ia, int ib, int ic) {
	const long double abx = (long double)points[(size_t)ib].first -
	    points[(size_t)ia].first;
	const long double aby = (long double)points[(size_t)ib].second -
	    points[(size_t)ia].second;
	const long double acx = (long double)points[(size_t)ic].first -
	    points[(size_t)ia].first;
	const long double acy = (long double)points[(size_t)ic].second -
	    points[(size_t)ia].second;
	const long double cross = abx * acy - aby * acx;
	return (cross > 0.0L) - (cross < 0.0L);
    };
    const auto on_segment = [&](int point, int first, int second) {
	if (orient(first, second, point))
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
    const auto segments_intersect = [&](int a, int b, int c, int d) {
	const int o1 = orient(a, b, c);
	const int o2 = orient(a, b, d);
	const int o3 = orient(c, d, a);
	const int o4 = orient(c, d, b);
	if (!o1 && on_segment(c, a, b)) return true;
	if (!o2 && on_segment(d, a, b)) return true;
	if (!o3 && on_segment(a, c, d)) return true;
	if (!o4 && on_segment(b, c, d)) return true;
	return o1 * o2 < 0 && o3 * o4 < 0;
    };
    const auto contains = [&](const std::vector<int> &ring, int point) {
	const long double px = points[(size_t)point].first;
	const long double py = points[(size_t)point].second;
	bool inside = false;
	for (size_t i = 0, j = ring.size() - 1; i < ring.size(); j = i++) {
	    const long double ax = points[(size_t)ring[i]].first;
	    const long double ay = points[(size_t)ring[i]].second;
	    const long double bx = points[(size_t)ring[j]].first;
	    const long double by = points[(size_t)ring[j]].second;
	    if ((ay > py) != (by > py) && px < (bx - ax) *
		    (py - ay) / (by - ay) + ax)
		inside = !inside;
	}
	return inside;
    };
    const auto area = [&](const std::vector<int> &ring) {
	long double twice_area = 0.0;
	for (size_t i = 0; i < ring.size(); ++i) {
	    const std::pair<double, double> &first =
		points[(size_t)ring[i]];
	    const std::pair<double, double> &second =
		points[(size_t)ring[(i + 1) % ring.size()]];
	    twice_area += (long double)first.first * second.second -
		(long double)second.first * first.second;
	}
	return std::fabs(twice_area);
    };

    std::vector<long double> areas;
    areas.reserve(rings.size());
    for (const std::vector<int> &ring : rings)
	areas.push_back(area(ring));
    std::vector<int> parent(rings.size(), -1);
    for (size_t child = 0; child < rings.size(); ++child) {
	long double parent_area = std::numeric_limits<long double>::infinity();
	for (size_t candidate = 0; candidate < rings.size(); ++candidate) {
	    if (candidate == child || areas[candidate] <= areas[child] ||
		    areas[candidate] >= parent_area ||
		    bounds[candidate].min_x > bounds[child].min_x ||
		    bounds[candidate].min_y > bounds[child].min_y ||
		    bounds[candidate].max_x < bounds[child].max_x ||
		    bounds[candidate].max_y < bounds[child].max_y ||
		    !contains(rings[candidate], rings[child][0]))
		continue;
	    bool contains_all = true;
	    for (int point : rings[child]) {
		if (!contains(rings[candidate], point)) {
		    contains_all = false;
		    break;
		}
	    }
	    if (contains_all) {
		parent[child] = (int)candidate;
		parent_area = areas[candidate];
	    }
	}
    }

    std::vector<int> depth(rings.size(), 0);
    size_t roots = 0;
    bool nested_island = false;
    for (size_t ring = 0; ring < rings.size(); ++ring) {
	std::set<int> ancestors;
	int current = parent[ring];
	while (current >= 0) {
	    if (!ancestors.insert(current).second) {
		if (failure)
		    *failure = "chart component nesting contains a cycle";
		return false;
	    }
	    depth[ring]++;
	    current = parent[(size_t)current];
	}
	if (!depth[ring])
	    roots++;
	if (depth[ring] > 1)
	    nested_island = true;
    }
    if (roots == 1 && !nested_island && parent[0] < 0) {
	components.push_back(*this);
	return true;
    }

    for (size_t first = 0; first < rings.size(); ++first) {
	for (size_t second = first + 1; second < rings.size(); ++second) {
	    if (bounds[first].max_x < bounds[second].min_x ||
		    bounds[second].max_x < bounds[first].min_x ||
		    bounds[first].max_y < bounds[second].min_y ||
		    bounds[second].max_y < bounds[first].min_y)
		continue;
	    for (size_t i = 0; i < rings[first].size(); ++i) {
		const int a = rings[first][i];
		const int b = rings[first][(i + 1) % rings[first].size()];
		for (size_t j = 0; j < rings[second].size(); ++j) {
		    const int c = rings[second][j];
		    const int d = rings[second][
			(j + 1) % rings[second].size()];
		    if (segments_intersect(a, b, c, d)) {
			/* Preserve touching and crossing inputs for the strict
			 * triangulator's more specific diagnostics and repairs. */
			components.push_back(*this);
			return true;
		    }
		}
	    }
	}
    }

    const auto boundary_has = [](const cdt_face_chart &component,
	    int point) {
	if (std::find(component.outer.begin(), component.outer.end(), point) !=
		component.outer.end())
	    return true;
	for (const std::vector<int> &hole : component.holes) {
	    if (std::find(hole.begin(), hole.end(), point) != hole.end())
		return true;
	}
	return false;
    };
    const auto interior_has = [&](const cdt_face_chart &component,
	    int point) {
	if (point < 0 || (size_t)point >= points.size())
	    return false;
	if (!contains(component.outer, point))
	    return false;
	for (const std::vector<int> &hole : component.holes) {
	    if (contains(hole, point))
		return false;
	}
	return true;
    };
    for (size_t ring = 0; ring < rings.size(); ++ring) {
	if (depth[ring] % 2)
	    continue;
	cdt_face_chart component(*this);
	component.outer = rings[ring];
	component.holes.clear();
	for (size_t child = 0; child < rings.size(); ++child) {
	    if (parent[child] == (int)ring)
		component.holes.push_back(rings[child]);
	}
	component.steiner.clear();
	for (int point : steiner) {
	    if (point >= 0 && (size_t)point < points.size() &&
		    interior_has(component, point))
		component.steiner.push_back(point);
	}
	component.constraints.clear();
	for (const std::pair<int, int> &constraint : constraints) {
	    const bool first = boundary_has(component, constraint.first) ||
		interior_has(component, constraint.first);
	    const bool second = boundary_has(component, constraint.second) ||
		interior_has(component, constraint.second);
	    if (first && second)
		component.constraints.push_back(constraint);
	}
	components.push_back(component);
    }
    if (components.empty()) {
	if (failure)
	    *failure = "chart component partition produced no filled regions";
	return false;
    }
    return true;
}

void
cdt_face_chart::repair_nesting()
{
    /* Both charts are planar embeddings of the surface parameter domain.
     * Surface curvature does not change loop containment in a valid chart. */
    const bool planar_embedding =
	m_type == CDT_FACE_CHART_SURFACE_METRIC ||
	m_type == CDT_FACE_CHART_CYLINDER;
    if (!planar_embedding || outer.size() < 3 ||
	    holes.size() != 1 || holes[0].size() < 3)
	return;

    const auto contains = [&](const std::vector<int> &ring, int point) {
	if (point < 0 || (size_t)point >= points.size())
	    return false;
	const long double px = points[(size_t)point].first;
	const long double py = points[(size_t)point].second;
	bool inside = false;
	for (size_t i = 0, j = ring.size() - 1; i < ring.size();
		j = i++) {
	    const int ia = ring[i];
	    const int ib = ring[j];
	    if (ia < 0 || ib < 0 || (size_t)ia >= points.size() ||
		    (size_t)ib >= points.size())
		return false;
	    const long double ax = points[(size_t)ia].first;
	    const long double ay = points[(size_t)ia].second;
	    const long double bx = points[(size_t)ib].first;
	    const long double by = points[(size_t)ib].second;
	    if ((ay > py) != (by > py) && px < (bx - ax) *
		    (py - ay) / (by - ay) + ax)
		inside = !inside;
	}
	return inside;
    };

    const auto area = [&](const std::vector<int> &ring) {
	long double twice_area = 0.0;
	for (size_t i = 0; i < ring.size(); ++i) {
	    const std::pair<double, double> &first =
		points[(size_t)ring[i]];
	    const std::pair<double, double> &second =
		points[(size_t)ring[(i + 1) % ring.size()]];
	    twice_area += (long double)first.first * second.second -
		(long double)second.first * first.second;
	}
	return std::fabs(twice_area);
    };

    if (area(holes[0]) > area(outer) &&
	    !contains(outer, holes[0][0]) && contains(holes[0], outer[0]))
	outer.swap(holes[0]);
}

bool
cdt_face_chart::build(const ON_BrepFace &face,
	const std::vector<std::pair<double, double>> &native_points,
	const std::vector<int> &source_outer,
	const std::vector<std::vector<int>> &source_holes,
	const std::vector<int> &source_steiner,
	const std::vector<int> &source_refinement,
	const std::vector<const ON_3dPoint *> &points_3d,
	const std::vector<cdt_topo_vertex_id> &topology_vertices,
	cdt_topo_vertex_id preferred_pole)
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
    m_surface = NULL;
    m_cone = ON_Cone();
    m_cone_seam_angle = 0.0;
    m_cone_orientation = 1;
    m_authoritative_cone_points =
	preferred_pole != CDT_TOPOLOGY_ID_NONE;
    m_cylinder = ON_Cylinder();
    m_cylinder_cut = 0.0;
    m_cylinder_orientation = 1;
    m_source_native_points.clear();

    if (topology_vertices.size() != native_points.size()) {
	m_failure = "chart topology metadata does not match its point array";
	return false;
    }
    m_source_native_points.reserve(native_points.size());
    for (const std::pair<double, double> &point : native_points)
	m_source_native_points.push_back(ON_2dPoint(point.first,
	    point.second));

    bool built = false;
    int pole_vertex = -1;
    if (cone_chart_properties(face, &m_closed_dir, &m_open_dir,
	    &m_singular_side, &pole_vertex, &m_periodic)) {
	m_closed_domain = face.SurfaceOf()->Domain(m_closed_dir);
	m_open_domain = face.SurfaceOf()->Domain(m_open_dir);
	m_pole_topology_vertex = preferred_pole != CDT_TOPOLOGY_ID_NONE ?
	    preferred_pole : pole_vertex;
	built = build_cone(face, native_points, source_outer, source_holes,
	    source_steiner, source_refinement, points_3d,
	    topology_vertices);
	if (built)
	    repair_nesting();
	return built;
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
	built = build_polar(face, native_points, source_outer, source_holes,
	    source_steiner, points_3d, topology_vertices);
	if (built)
	    repair_nesting();
	return built;
    }
    ON_Cylinder cylinder;
    if (cdt_face_uses_cylinder_chart(face) && face.SurfaceOf() &&
	    face.SurfaceOf()->IsCylinder(&cylinder, CDT_CONIC_TOLERANCE)) {
	built = build_cylinder(face, native_points, source_outer, source_holes,
	    source_steiner, points_3d, topology_vertices, cylinder);
    } else {
	built = build_native(face, native_points, source_outer, source_holes,
	    source_steiner, points_3d, topology_vertices);
    }
    if (built)
	repair_nesting();
    return built;
}
