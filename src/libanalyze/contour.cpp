/*                     C O N T O U R . C P P
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
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
/** @file contour.cpp
 *
 * Ray-driven manifold dual contouring for evaluated BRL-CAD geometry.
 *
 * The topology construction follows the multi-vertex cell strategy described
 * by Schaefer, Ju, and Warren, "Manifold Dual Contouring", IEEE TVCG 2007.
 * Their bottom-up vertex-clustering test is not needed here because this
 * implementation does not simplify an already contoured finest-level mesh.
 * Instead, cell-local topology tests and interior ray probes trigger octree
 * refinement before vertices are built.  If those tests prove insufficient,
 * the next step is the paper's bottom-up Euler-characteristic test during
 * octree clustering, not additional ad hoc sampling.  Unlike the historical
 * polygonizer, this implementation consumes librt partitions directly and
 * does not use libnmg for topology construction.
 */

#include "common.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <deque>
#include <limits>
#include <map>
#include <new>
#include <set>
#include <utility>
#include <vector>

#include <Eigen/SVD>

#include "bu/datetime.h"
#include "bu/env.h"
#include "bu/log.h"
#include "bu/malloc.h"
#include "bg/trimesh.h"
#include "raytrace.h"
#include "rt/func.h"

#include "analyze/contour.h"


namespace {

constexpr int DEFAULT_TARGET_DEPTH = 6;
constexpr double ROOT_PADDING_FRACTION = 0.03125;
/* Irrational sub-cell offsets keep exact CAD seams and corners from remaining
 * coincident with every level of the dyadic contour grid. */
constexpr double GRID_PHASE_OFFSETS[3] = {
    0.381966011250105, -0.236067977499790, 0.141592653589793
};
constexpr double EDGE_PARAMETER_TOLERANCE = 1.0e-8;
constexpr double NORMAL_LENGTH_TOLERANCE = 1.0e-12;
constexpr double QEF_SINGULAR_VALUE_TOLERANCE = 1.0e-10;
constexpr double NORMAL_REFINEMENT_DOT = 0.0;
constexpr double MAX_PROXY_HIT_DISTANCE_IN_CELLS = 1.0;
constexpr double MIN_THICKNESS_INCIDENCE = 0.5;
constexpr double MIN_THICKNESS_NORMAL_OPPOSITION = 0.9;
constexpr double MIN_CELLS_PER_THICKNESS = 1.0;
constexpr double CHARACTERIZATION_TIME_FRACTION = 0.2;
constexpr double CHARACTERIZATION_MAX_TIME_MS = 5000.0;
constexpr size_t CHARACTERIZATION_MAX_RAYS = 20000;
constexpr size_t CHARACTERIZATION_RAY_DIVISOR = 5;
constexpr size_t CHARACTERIZATION_MIN_RAYS = 256;
constexpr size_t MIN_LOCAL_THICKNESS_SAMPLES = 3;
constexpr size_t LOCAL_THICKNESS_QUANTILE_DIVISOR = 4;
constexpr double ESTIMATED_RAYS_PER_SURFACE_CELL = 12.0;
constexpr size_t ESTIMATED_BYTES_PER_CONTOUR_RAY = 512;
constexpr size_t RESOURCE_CHECK_INTERVAL = 256;
constexpr int64_t PROGRESS_INTERVAL_USEC = 5000000;

enum class Axis : uint8_t {
    X = 0,
    Y = 1,
    Z = 2
};

static std::array<size_t, 2>
transverse_axes(Axis axis)
{
    switch (axis) {
	case Axis::X:
	    return {{1, 2}};
	case Axis::Y:
	    return {{0, 2}};
	case Axis::Z:
	    return {{0, 1}};
    }
    return {{0, 1}};
}

static char
axis_name(Axis axis)
{
    static const char names[] = {'X', 'Y', 'Z'};
    return names[static_cast<size_t>(axis)];
}

struct Vec3 {
    double v[3] = {0.0, 0.0, 0.0};

    double &operator[](size_t i) { return v[i]; }
    double operator[](size_t i) const { return v[i]; }
};

static Vec3
operator+(const Vec3 &a, const Vec3 &b)
{
    Vec3 result;
    for (size_t i = 0; i < 3; i++)
	result[i] = a[i] + b[i];
    return result;
}

static Vec3
operator-(const Vec3 &a, const Vec3 &b)
{
    Vec3 result;
    for (size_t i = 0; i < 3; i++)
	result[i] = a[i] - b[i];
    return result;
}

static Vec3
operator*(const Vec3 &a, double scale)
{
    Vec3 result;
    for (size_t i = 0; i < 3; i++)
	result[i] = a[i] * scale;
    return result;
}

static Vec3
operator/(const Vec3 &a, double scale)
{
    return a * (1.0 / scale);
}

static double
dot(const Vec3 &a, const Vec3 &b)
{
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

static double
magnitude_squared(const Vec3 &a)
{
    return dot(a, a);
}

static bool
finite(const Vec3 &a)
{
    return std::isfinite(a[0]) && std::isfinite(a[1]) &&
	std::isfinite(a[2]);
}

static Vec3
normalized(const Vec3 &a)
{
    double length = std::sqrt(magnitude_squared(a));
    if (!std::isfinite(length) || length <= NORMAL_LENGTH_TOLERANCE)
	return Vec3();
    return a / length;
}

struct SurfaceHit {
    double coordinate = 0.0;
    Vec3 point;
    Vec3 normal;
};

struct Interval {
    SurfaceHit in;
    SurfaceHit out;
};

struct Line {
    std::vector<Interval> intervals;
    std::vector<SurfaceHit> hits;

    bool inside(double coordinate) const
    {
	auto after = std::upper_bound(intervals.begin(), intervals.end(),
		coordinate, [](double value, const Interval &interval) {
		    return value < interval.in.coordinate;
		});
	if (after == intervals.begin())
	    return false;

	const Interval &candidate = *--after;
	/* Grid phasing avoids persistent exact-boundary samples.  Shrinking the
	 * interval by the ray tolerance can exceed a finest cell and create a
	 * topologically false sign change, so use the traced interval directly. */
	return coordinate > candidate.in.coordinate &&
	    coordinate < candidate.out.coordinate;
    }
};


static bool
local_wall_thickness(const Interval &interval, Axis axis, double &thickness)
{
    size_t coordinate_axis = static_cast<size_t>(axis);
    double in_incidence = -interval.in.normal[coordinate_axis];
    double out_incidence = interval.out.normal[coordinate_axis];
    if (in_incidence < MIN_THICKNESS_INCIDENCE ||
	out_incidence < MIN_THICKNESS_INCIDENCE ||
	dot(interval.in.normal, interval.out.normal) >
	    -MIN_THICKNESS_NORMAL_OPPOSITION)
	return false;

    double chord = interval.out.coordinate - interval.in.coordinate;
    thickness = chord * std::min(in_incidence, out_incidence);
    return std::isfinite(thickness) && thickness > SMALL_FASTF;
}

struct LineKey {
    Axis axis = Axis::X;
    int u2 = 0;
    int v2 = 0;

    bool operator<(const LineKey &other) const
    {
	if (axis != other.axis)
	    return axis < other.axis;
	if (u2 != other.u2)
	    return u2 < other.u2;
	return v2 < other.v2;
    }
};

struct GridEdge {
    Axis axis = Axis::X;
    int i = 0;
    int j = 0;
    int k = 0;
    int size = 1;

    bool operator<(const GridEdge &other) const
    {
	if (axis != other.axis)
	    return axis < other.axis;
	if (i != other.i)
	    return i < other.i;
	if (j != other.j)
	    return j < other.j;
	if (k != other.k)
	    return k < other.k;
	return size < other.size;
    }
};

struct CellKey {
    int i = 0;
    int j = 0;
    int k = 0;
    int size = 1;

    bool operator<(const CellKey &other) const
    {
	if (i != other.i)
	    return i < other.i;
	if (j != other.j)
	    return j < other.j;
	if (k != other.k)
	    return k < other.k;
	return size < other.size;
    }
};

struct GridPoint {
    int i = 0;
    int j = 0;
    int k = 0;

    bool operator<(const GridPoint &other) const
    {
	if (i != other.i)
	    return i < other.i;
	if (j != other.j)
	    return j < other.j;
	return k < other.k;
    }
};

struct AtomicEdge {
    Axis axis = Axis::X;
    int i = 0;
    int j = 0;
    int k = 0;

    bool operator<(const AtomicEdge &other) const
    {
	if (axis != other.axis)
	    return axis < other.axis;
	if (i != other.i)
	    return i < other.i;
	if (j != other.j)
	    return j < other.j;
	return k < other.k;
    }
};

enum class EdgeState {
    Empty,
    Crossing,
    Ambiguous
};

struct EdgeSample {
    EdgeState state = EdgeState::Empty;
    SurfaceHit hit;
    bool start_inside = false;
};

struct CellData {
    std::map<GridEdge, int> edge_vertices;
};

struct RayResult {
    std::vector<Interval> intervals;
};

struct ContourControl {
    const analyze_mdc_params &params;
    int64_t start_time;
    size_t ray_count = 0;
    enum analyze_mdc_status status = ANALYZE_MDC_OK;

    bool available()
    {
	if (status != ANALYZE_MDC_OK)
	    return false;

	if (params.max_rays && ray_count >= params.max_rays) {
	    status = ANALYZE_MDC_RAY_LIMIT;
	    return false;
	}

	if (params.max_time > 0 &&
		(bu_gettime() - start_time) / 1000000 >= params.max_time) {
	    status = ANALYZE_MDC_TIMEOUT;
	    return false;
	}

	if (params.minimum_free_mem &&
		ray_count % RESOURCE_CHECK_INTERVAL == 0) {
	    ssize_t available_memory = bu_mem(BU_MEM_AVAIL, NULL);
	    if (available_memory >= 0 &&
		    static_cast<size_t>(available_memory) <
		    params.minimum_free_mem) {
		status = ANALYZE_MDC_MEMORY_LIMIT;
		return false;
	    }
	}

	return true;
    }
};

static SurfaceHit
make_surface_hit(struct application *ap, const struct hit *hit,
	struct soltab *stp, int flip, bool entering, Axis axis)
{
    SurfaceHit result;

    point_t point;
    vect_t normal = VINIT_ZERO;
    VJOIN1(point, ap->a_ray.r_pt, hit->hit_dist, ap->a_ray.r_dir);

    /* Some historic TGC intersections report a bad surface number.  Their
     * boundary transition is still valid, and supplies a conservative
     * outward normal without pretending the hit belongs to a particular
     * cap or body surface. */
    bool invalid_tgc_surface = stp && stp->st_id == ID_TGC &&
	(hit->hit_surfno < 1 || hit->hit_surfno > 3);
    if (invalid_tgc_surface) {
	VSCALE(normal, ap->a_ray.r_dir, entering ? -1.0 : 1.0);
    } else {
	struct hit normal_hit = *hit;
	RT_HIT_NORMAL(normal, &normal_hit, stp, &ap->a_ray, flip);
    }

    for (size_t i = 0; i < 3; i++) {
	result.point[i] = point[i];
	result.normal[i] = normal[i];
    }
    result.normal = normalized(result.normal);
    if (!finite(result.normal) ||
	    magnitude_squared(result.normal) <= NORMAL_LENGTH_TOLERANCE) {
	result.normal[static_cast<size_t>(axis)] = entering ? -1.0 : 1.0;
    }

    result.coordinate = result.point[static_cast<size_t>(axis)];
    return result;
}

static int
contour_hit(struct application *ap, struct partition *part_head,
	struct seg *UNUSED(segments))
{
    RT_CK_APPLICATION(ap);
    RayResult *result = static_cast<RayResult *>(ap->a_uptr);
    Axis axis = static_cast<Axis>(ap->a_user);

    for (struct partition *part = part_head->pt_forw;
	    part != part_head; part = part->pt_forw) {
	if (!part->pt_inhit || !part->pt_outhit || !part->pt_inseg ||
		!part->pt_outseg)
	    continue;
	if (!std::isfinite(part->pt_inhit->hit_dist) ||
		!std::isfinite(part->pt_outhit->hit_dist))
	    continue;

	struct soltab *in_stp = part->pt_inseg->seg_stp;
	struct soltab *out_stp = part->pt_outseg->seg_stp;
	Interval interval;
	interval.in = make_surface_hit(ap, part->pt_inhit, in_stp,
		part->pt_inflip, true, axis);
	interval.out = make_surface_hit(ap, part->pt_outhit, out_stp,
		part->pt_outflip, false, axis);
	if (!std::isfinite(interval.in.coordinate) ||
		!std::isfinite(interval.out.coordinate))
	    continue;
	if (interval.out.coordinate < interval.in.coordinate)
	    std::swap(interval.in, interval.out);
	if (interval.out.coordinate > interval.in.coordinate)
	    result->intervals.push_back(interval);
    }

    return 0;
}

static int
contour_miss(struct application *ap)
{
    RT_CK_APPLICATION(ap);
    return 0;
}

static void
finalize_line(Line &line, std::vector<Interval> &raw, double tolerance)
{
    if (raw.empty())
	return;

    std::sort(raw.begin(), raw.end(),
	    [](const Interval &a, const Interval &b) {
		return a.in.coordinate < b.in.coordinate;
	    });

    for (const auto &candidate : raw) {
	if (line.intervals.empty() ||
		candidate.in.coordinate >
		line.intervals.back().out.coordinate + tolerance) {
	    line.intervals.push_back(candidate);
	    continue;
	}

	Interval &current = line.intervals.back();
	if (candidate.out.coordinate > current.out.coordinate)
	    current.out = candidate.out;
    }

    for (const auto &interval : line.intervals) {
	line.hits.push_back(interval.in);
	line.hits.push_back(interval.out);
    }
}

class ContourGrid {
public:
    ContourGrid(struct rt_i *rtip, const point_t root_min, double root_size,
	    int target_depth, int max_depth, ContourControl &control,
	    struct resource &resource) :
	m_rtip(rtip), m_resource(resource), m_resolution(1 << max_depth),
	m_target_size(1 << (max_depth - target_depth)),
	m_cell_size(root_size / m_resolution), m_control(control)
    {
	for (size_t i = 0; i < 3; i++)
	    m_min[i] = root_min[i];
    }

    enum analyze_mdc_status build(std::vector<Vec3> &vertices,
	    std::vector<std::array<int, 3>> &faces)
    {
	begin_phase("surface discovery");
	if (!discover_surface())
	    return m_control.status;
	if (m_active_cells.empty())
	    return ANALYZE_MDC_NO_SURFACE;
	begin_phase("surface closure");
	if (!close_surface_cells())
	    return m_control.status;
	size_t topology_revision = 0;
	do {
	    topology_revision = m_topology_revision;
	    begin_phase("2:1 leaf balancing");
	    if (!balance_surface_cells())
		return m_control.status;
	    begin_phase("transition closure");
	    if (!close_surface_cells())
		return m_control.status;
	} while (topology_revision != m_topology_revision);
	begin_phase("vertex construction");
	if (m_control.params.verbosity && m_unresolved_thickness_cells)
	    bu_log("MDC: %zu finest-depth cell(s) remain larger than the "
		    "locally sampled thickness\n",
		    m_unresolved_thickness_cells);
	if (m_control.params.verbosity && m_unresolved_topology_cells)
	    bu_log("MDC: %zu finest-depth cell(s) retain sampled "
		    "multi-crossing detail\n", m_unresolved_topology_cells);
	if (m_control.params.verbosity && m_unresolved_normal_cells)
	    bu_log("MDC: %zu finest-depth cell(s) retain high normal "
		    "variation\n", m_unresolved_normal_cells);

	for (const CellKey &cell : m_active_cells) {
	    enum analyze_mdc_status status = build_cell(cell, vertices);
	    if (status != ANALYZE_MDC_OK)
		return status;
	}
	begin_phase("face construction");

	enum analyze_mdc_status status = build_faces(vertices, faces);
	if (status != ANALYZE_MDC_OK)
	    return status;
	if (m_control.params.verbosity && m_proxy_hit_count)
	    bu_log("MDC: reconstructed %zu near-endpoint crossing sample(s) "
		    "from adjacent ray hits\n", m_proxy_hit_count);
	if (vertices.empty() || faces.empty())
	    return ANALYZE_MDC_NO_SURFACE;
	if (vertices.size() >
		static_cast<size_t>(std::numeric_limits<int>::max()) ||
		faces.size() >
		static_cast<size_t>(std::numeric_limits<int>::max()))
	    return ANALYZE_MDC_MEMORY_LIMIT;

	std::vector<fastf_t> packed_vertices(3 * vertices.size());
	std::vector<int> packed_faces(3 * faces.size());
	for (size_t i = 0; i < vertices.size(); i++) {
	    for (size_t axis = 0; axis < 3; axis++)
		packed_vertices[3 * i + axis] = vertices[i][axis];
	}
	for (size_t i = 0; i < faces.size(); i++) {
	    for (size_t corner = 0; corner < 3; corner++)
		packed_faces[3 * i + corner] = faces[i][corner];
	}
	struct bg_trimesh_solid_errors mesh_errors =
	    BG_TRIMESH_SOLID_ERRORS_INIT_NULL;
	if (bg_trimesh_solid2(static_cast<int>(vertices.size()),
		static_cast<int>(faces.size()), packed_vertices.data(),
		packed_faces.data(), &mesh_errors) != 0) {
	    if (m_control.params.verbosity)
		bu_log("MDC: %zu vertices and %zu faces did not form a "
			"closed oriented manifold (%d unmatched, %d excess, "
			"%d misoriented edges; %d degenerate faces)\n",
			vertices.size(), faces.size(),
			mesh_errors.unmatched.count, mesh_errors.excess.count,
			mesh_errors.misoriented.count,
			mesh_errors.degenerate.count);
	    bg_free_trimesh_solid_errors(&mesh_errors);
	    return ANALYZE_MDC_NOT_MANIFOLD;
	}
	bg_free_trimesh_solid_errors(&mesh_errors);

	return ANALYZE_MDC_OK;
    }

private:
    struct Node {
	int i = 0;
	int j = 0;
	int k = 0;
	int size = 0;
    };

    struct DisjointSet {
	int parent[12];

	DisjointSet()
	{
	    for (int i = 0; i < 12; i++)
		parent[i] = i;
	}

	int find(int value)
	{
	    if (parent[value] != value)
		parent[value] = find(parent[value]);
	    return parent[value];
	}

	void join(int a, int b)
	{
	    a = find(a);
	    b = find(b);
	    if (a != b)
		parent[b] = a;
	}
    };

    static constexpr int FACE_EDGES[6][4] = {
	{0, 1, 2, 3}, {4, 5, 6, 7},
	{0, 9, 4, 8}, {2, 10, 6, 11},
	{3, 11, 7, 8}, {1, 10, 5, 9}
    };

    struct Component {
	std::vector<int> local_edges;
	std::vector<SurfaceHit> hits;
    };

    struct LeafAnalysis {
	bool has_crossing = false;
	bool needs_refinement = false;
	bool normal_refinement = false;
    };

    struct rt_i *m_rtip;
    struct resource &m_resource;
    Vec3 m_min;
    int m_resolution;
    int m_target_size;
    double m_cell_size;
    ContourControl &m_control;
    size_t m_unresolved_thickness_cells = 0;
    size_t m_unresolved_topology_cells = 0;
    size_t m_unresolved_normal_cells = 0;
    size_t m_proxy_hit_count = 0;
    size_t m_topology_revision = 0;
    std::map<LineKey, Line> m_lines;
    std::map<GridPoint, bool> m_point_signs;
    std::map<GridEdge, EdgeSample> m_edges;
    std::set<CellKey> m_active_cells;
    std::map<CellKey, CellData> m_cells;
    const char *m_phase = "initialization";
    int64_t m_last_progress_time = 0;

    void report_progress(bool force = false)
    {
	if (!m_control.params.verbosity)
	    return;
	int64_t now = bu_gettime();
	if (!force && now - m_last_progress_time < PROGRESS_INTERVAL_USEC)
	    return;
	m_last_progress_time = now;
	bu_log("MDC: %s: %zu active leaves, %zu cached rays, %.1f seconds "
		"elapsed\n", m_phase, m_active_cells.size(), m_lines.size(),
		static_cast<double>(now - m_control.start_time) / 1000000.0);
    }

    void begin_phase(const char *phase)
    {
	m_phase = phase;
	report_progress(true);
    }

    bool valid_cell(const CellKey &cell) const
    {
	return cell.i >= 0 && cell.j >= 0 && cell.k >= 0 &&
	    cell.i + cell.size <= m_resolution &&
	    cell.j + cell.size <= m_resolution &&
	    cell.k + cell.size <= m_resolution;
    }

    bool valid_unit_cell(const CellKey &cell) const
    {
	return cell.i >= 0 && cell.j >= 0 && cell.k >= 0 &&
	    cell.i < m_resolution && cell.j < m_resolution &&
	    cell.k < m_resolution;
    }

    double world(size_t axis, int doubled_index) const
    {
	return m_min[axis] + 0.5 * doubled_index * m_cell_size;
    }

    double sampling_tolerance() const
    {
	return std::max(m_rtip->rti_tol.dist,
		m_cell_size * EDGE_PARAMETER_TOLERANCE);
    }

    bool cast_line(const LineKey &key, Line &line)
    {
	if (!m_control.available())
	    return false;

	RayResult result;
	struct application ap;
	RT_APPLICATION_INIT(&ap);

	ap.a_rt_i = m_rtip;
	ap.a_hit = contour_hit;
	ap.a_miss = contour_miss;
	ap.a_overlap = rt_defoverlap;
	ap.a_logoverlap = rt_silent_logoverlap;
	ap.a_onehit = 0;
	ap.a_resource = &m_resource;
	ap.a_uptr = &result;
	ap.a_user = static_cast<int>(key.axis);

	size_t axis = static_cast<size_t>(key.axis);
	std::array<size_t, 2> transverse = transverse_axes(key.axis);

	VSETALL(ap.a_ray.r_pt, 0.0);
	VSETALL(ap.a_ray.r_dir, 0.0);
	ap.a_ray.r_pt[axis] = m_min[axis] - m_cell_size;
	ap.a_ray.r_pt[transverse[0]] = world(transverse[0], key.u2);
	ap.a_ray.r_pt[transverse[1]] = world(transverse[1], key.v2);
	ap.a_ray.r_dir[axis] = 1.0;

	rt_shootray(&ap);
	m_control.ray_count++;
	report_progress();

	finalize_line(line, result.intervals, sampling_tolerance());
	return true;
    }

    Line *line(const LineKey &key)
    {
	auto found = m_lines.find(key);
	if (found != m_lines.end())
	    return &found->second;

	auto inserted = m_lines.emplace(key, Line());
	if (!cast_line(key, inserted.first->second)) {
	    m_lines.erase(inserted.first);
	    return NULL;
	}
	return &inserted.first->second;
    }

    bool segment_has_hit(const LineKey &key, Axis axis, int low_index,
	    int size, bool &evidence, bool &multiple_crossings,
	    std::vector<double> &thicknesses)
    {
	Line *sample_line = line(key);
	if (!sample_line)
	    return false;

	size_t coordinate_axis = static_cast<size_t>(axis);
	double low = world(coordinate_axis, 2 * low_index);
	double high = world(coordinate_axis, 2 * (low_index + size));
	double tolerance = sampling_tolerance();
	auto hit = std::lower_bound(sample_line->hits.begin(),
		sample_line->hits.end(), low - tolerance,
		[](const SurfaceHit &candidate, double coordinate) {
		    return candidate.coordinate < coordinate;
		});
	const SurfaceHit *previous_reliable_hit = NULL;
	for (; hit != sample_line->hits.end() &&
		hit->coordinate <= high + tolerance; ++hit) {
	    evidence = true;
	    if (hit->coordinate <= low + tolerance ||
		    hit->coordinate >= high - tolerance ||
		    std::fabs(hit->normal[coordinate_axis]) <
		    MIN_THICKNESS_INCIDENCE)
		continue;
	    if (previous_reliable_hit &&
		    dot(previous_reliable_hit->normal, hit->normal) <=
		    -MIN_THICKNESS_NORMAL_OPPOSITION)
		multiple_crossings = true;
	    previous_reliable_hit = &*hit;
	}

	for (const Interval &interval : sample_line->intervals) {
	    if (interval.out.coordinate < low - tolerance ||
		    interval.in.coordinate > high + tolerance)
		continue;
	    double thickness = 0.0;
	    if (local_wall_thickness(interval, axis, thickness))
		thicknesses.push_back(thickness);
	}
	return true;
    }

    bool probe_node(const Node &node, bool &edge_evidence,
	    bool &interior_evidence, bool &multiple_crossings,
	    double &local_thickness)
    {
	edge_evidence = false;
	interior_evidence = false;
	multiple_crossings = false;
	local_thickness = 0.0;
	std::vector<double> thicknesses;
	std::array<int, 3> low = {{node.i, node.j, node.k}};

	for (int axis_value = 0; axis_value < 3; axis_value++) {
	    Axis axis = static_cast<Axis>(axis_value);
	    std::array<size_t, 2> transverse = transverse_axes(axis);
	    for (int u_side = 0; u_side < 2; u_side++) {
		for (int v_side = 0; v_side < 2; v_side++) {
		    int u = low[transverse[0]] +
			(u_side ? node.size : 0);
		    int v = low[transverse[1]] +
			(v_side ? node.size : 0);
		    if (!segment_has_hit({axis, 2 * u, 2 * v}, axis,
			    low[axis_value], node.size, edge_evidence,
			    multiple_crossings, thicknesses))
			return false;
		}
	    }
	}

	/* Corner signs and edge intersections cannot reveal a tunnel that enters
	 * and leaves through cell-face interiors.  Quarter and center chords give
	 * each octree node independent evidence of those closed excursions.  A
	 * finite probe set cannot guarantee discovery below the finest sampling
	 * scale.  Observed excursions drive refinement to the configured limit;
	 * unresolved interior-only surfaces fail, and the completed mesh receives
	 * a separate oriented-manifold check. */
	int interior_samples = (node.size == 1) ? 1 : 2;
	for (int axis_value = 0; axis_value < 3; axis_value++) {
	    Axis axis = static_cast<Axis>(axis_value);
	    std::array<size_t, 2> transverse = transverse_axes(axis);
	    for (int u_sample = 0; u_sample < interior_samples; u_sample++) {
		int u2 = 2 * low[transverse[0]] +
		    ((node.size == 1) ? 1 : (u_sample ?
		    3 * node.size / 2 : node.size / 2));
		for (int v_sample = 0; v_sample < interior_samples;
			v_sample++) {
		    int v2 = 2 * low[transverse[1]] +
			((node.size == 1) ? 1 : (v_sample ?
			3 * node.size / 2 : node.size / 2));
		    if (!segment_has_hit({axis, u2, v2}, axis,
			    low[axis_value], node.size, interior_evidence,
			    multiple_crossings, thicknesses))
			return false;
		}
	    }

	    if (node.size > 1) {
		int center_u2 = 2 * low[transverse[0]] + node.size;
		int center_v2 = 2 * low[transverse[1]] + node.size;
		if (!segment_has_hit({axis, center_u2, center_v2}, axis,
			low[axis_value], node.size, interior_evidence,
			multiple_crossings, thicknesses))
		    return false;
	    }
	}

	if (thicknesses.size() >= MIN_LOCAL_THICKNESS_SAMPLES) {
	    std::sort(thicknesses.begin(), thicknesses.end());
	    local_thickness = thicknesses[
		thicknesses.size() / LOCAL_THICKNESS_QUANTILE_DIVISOR];
	}
	return true;
    }

    GridEdge local_edge(const CellKey &cell, int edge) const
    {
	const int size = cell.size;
	switch (edge) {
	    case 0: return {Axis::X, cell.i, cell.j, cell.k, size};
	    case 1: return {Axis::Y, cell.i + size, cell.j, cell.k, size};
	    case 2: return {Axis::X, cell.i, cell.j + size, cell.k, size};
	    case 3: return {Axis::Y, cell.i, cell.j, cell.k, size};
	    case 4: return {Axis::X, cell.i, cell.j, cell.k + size, size};
	    case 5: return {Axis::Y, cell.i + size, cell.j,
		cell.k + size, size};
	    case 6: return {Axis::X, cell.i, cell.j + size,
		cell.k + size, size};
	    case 7: return {Axis::Y, cell.i, cell.j,
		cell.k + size, size};
	    case 8: return {Axis::Z, cell.i, cell.j, cell.k, size};
	    case 9: return {Axis::Z, cell.i + size, cell.j,
		cell.k, size};
	    case 10: return {Axis::Z, cell.i + size, cell.j + size,
		cell.k, size};
	    default: return {Axis::Z, cell.i, cell.j + size,
		cell.k, size};
	}
    }

    LineKey edge_line(const GridEdge &edge) const
    {
	if (edge.axis == Axis::X)
	    return {Axis::X, 2 * edge.j, 2 * edge.k};
	if (edge.axis == Axis::Y)
	    return {Axis::Y, 2 * edge.i, 2 * edge.k};
	return {Axis::Z, 2 * edge.i, 2 * edge.j};
    }

    int edge_segment(const GridEdge &edge) const
    {
	if (edge.axis == Axis::X)
	    return edge.i;
	if (edge.axis == Axis::Y)
	    return edge.j;
	return edge.k;
    }

    bool point_inside(const GridPoint &point, bool &inside)
    {
	auto found = m_point_signs.find(point);
	if (found != m_point_signs.end()) {
	    inside = found->second;
	    return true;
	}

	/* A contour vertex must have one sign shared by every incident edge.
	 * Fixed-direction parity provides that invariant; mixing independent
	 * per-axis classifications can give a shared corner contradictory signs
	 * near a tangency.  The deterministic grid phase keeps exact boundary
	 * coincidences from persisting through octree levels. */
	Line *x_line = line({Axis::X, 2 * point.j, 2 * point.k});
	if (!x_line)
	    return false;
	inside = x_line->inside(world(0, 2 * point.i));
	m_point_signs.emplace(point, inside);
	return true;
    }

    EdgeSample sample_edge(const GridEdge &edge)
    {
	auto found = m_edges.find(edge);
	if (found != m_edges.end())
	    return found->second;

	EdgeSample sample;
	Line *sample_line = line(edge_line(edge));
	if (!sample_line) {
	    sample.state = EdgeState::Ambiguous;
	    return sample;
	}

	int segment = edge_segment(edge);
	size_t axis = static_cast<size_t>(edge.axis);
	double low = world(axis, 2 * segment);
	double high = world(axis, 2 * (segment + edge.size));
	double tolerance = sampling_tolerance();
	GridPoint start = {edge.i, edge.j, edge.k};
	GridPoint end = start;
	if (edge.axis == Axis::X)
	    end.i += edge.size;
	else if (edge.axis == Axis::Y)
	    end.j += edge.size;
	else
	    end.k += edge.size;
	bool start_inside = false;
	bool end_inside = false;
	if (!point_inside(start, start_inside) ||
		!point_inside(end, end_inside)) {
	    sample.state = EdgeState::Ambiguous;
	    return sample;
	}
	sample.start_inside = start_inside;
	std::vector<SurfaceHit> hits;
	auto hit = std::lower_bound(sample_line->hits.begin(),
		sample_line->hits.end(), low - tolerance,
		[](const SurfaceHit &candidate, double coordinate) {
		    return candidate.coordinate < coordinate;
		});
	for (; hit != sample_line->hits.end() &&
		hit->coordinate <= high + tolerance; ++hit)
	    hits.push_back(*hit);

	if (start_inside == end_inside) {
	    /* Dual edges are defined by their shared endpoint signs.  Hits on a
	     * same-sign segment are either a sub-cell excursion, handled by node
	     * probes, or an isolated tangency and do not emit a contour face. */
	    sample.state = EdgeState::Empty;
	} else if (hits.size() == 1) {
	    sample.state = EdgeState::Crossing;
	    sample.hit = hits.front();
	} else if (hits.empty()) {
	    /* High-aspect analytic surfaces can place directionally equivalent
	     * ray intersections on opposite sides of a grid endpoint.  Preserve
	     * the topology defined by the shared vertex signs only when the edge
	     * ray supplies nearby Hermite data; a larger disagreement remains a
	     * hard ambiguity. */
	    const SurfaceHit *nearest_hit = NULL;
	    double nearest_distance = std::numeric_limits<double>::infinity();
	    for (const SurfaceHit &candidate : sample_line->hits) {
		double distance = candidate.coordinate < low ?
		    low - candidate.coordinate : (candidate.coordinate > high ?
		    candidate.coordinate - high : 0.0);
		if (distance < nearest_distance) {
		    nearest_distance = distance;
		    nearest_hit = &candidate;
		}
	    }
	    if (nearest_hit && nearest_distance <=
		    MAX_PROXY_HIT_DISTANCE_IN_CELLS * m_cell_size) {
		sample.state = EdgeState::Crossing;
		sample.hit = *nearest_hit;
		sample.hit.coordinate =
		    std::max(low, std::min(high, sample.hit.coordinate));
		sample.hit.point[axis] = sample.hit.coordinate;
		m_proxy_hit_count++;
	    } else {
		sample.state = EdgeState::Ambiguous;
	    }
	} else {
	    sample.state = EdgeState::Ambiguous;
	}

	if (sample.state == EdgeState::Ambiguous &&
		m_control.params.verbosity > 1) {
	    double nearest = std::numeric_limits<double>::infinity();
	    for (const SurfaceHit &candidate : sample_line->hits) {
		double distance = candidate.coordinate < low ?
		    low - candidate.coordinate : (candidate.coordinate > high ?
		    candidate.coordinate - high : 0.0);
		nearest = std::min(nearest, distance);
	    }
	    bu_log("MDC: %c edge (%d,%d,%d size %d) has %zu interior "
		    "hit(s), endpoint states %d/%d, nearest hit %.6g cells\n",
		    axis_name(edge.axis), edge.i, edge.j, edge.k, edge.size,
		    hits.size(), start_inside ? 1 : 0, end_inside ? 1 : 0,
		    nearest / m_cell_size);
	}

	m_edges.emplace(edge, sample);
	return sample;
    }

    LeafAnalysis analyze_leaf(const CellKey &cell)
    {
	LeafAnalysis analysis;
	bool active_edges[12] = {};
	std::vector<Vec3> crossing_normals;

	for (int edge = 0; edge < 12; edge++) {
	    EdgeSample sample = sample_edge(local_edge(cell, edge));
	    if (sample.state == EdgeState::Ambiguous)
		analysis.needs_refinement = true;
	    active_edges[edge] = sample.state == EdgeState::Crossing;
	    analysis.has_crossing =
		analysis.has_crossing || active_edges[edge];
	    if (active_edges[edge])
		crossing_normals.push_back(sample.hit.normal);
	}

	/* A cell whose Hermite normals turn through more than 90 degrees can
	 * collapse opposite sides of a curved or thin feature onto the same dual
	 * edge.  Refine before constructing vertices; the configured depth and
	 * resource budgets bound this local curvature criterion.  Orthogonal
	 * feature normals are intentionally retained so CAD corners do not refine
	 * indefinitely. */
	for (size_t i = 0; i < crossing_normals.size(); i++) {
	    for (size_t j = i + 1; j < crossing_normals.size(); j++) {
		if (dot(crossing_normals[i], crossing_normals[j]) <
			NORMAL_REFINEMENT_DOT) {
		    analysis.normal_refinement = true;
		    break;
		}
	    }
	    if (analysis.normal_refinement)
		break;
	}

	/* The cell boundary graph must pair every edge crossing on each face.
	 * Odd counts cannot form the disk-like surface sections required by
	 * manifold dual contouring and therefore request local refinement.  Four
	 * crossings are retained here: build_cell resolves their two possible
	 * pairings with a face-center sign sample. */
	for (int face = 0; face < 6; face++) {
	    int crossing_count = 0;
	    for (int position = 0; position < 4; position++)
		if (active_edges[FACE_EDGES[face][position]])
		    crossing_count++;
	    if (crossing_count != 0 && crossing_count != 2 &&
		    crossing_count != 4) {
		analysis.needs_refinement = true;
		if (cell.size == 1 && m_control.params.verbosity > 1) {
		    bu_log("MDC: cell (%d,%d,%d) face %d has %d crossings:",
			    cell.i, cell.j, cell.k, face, crossing_count);
		    for (int position = 0; position < 4; position++) {
			int edge = FACE_EDGES[face][position];
			bu_log(" %d=%d/%d", edge,
				active_edges[edge] ? 1 : 0,
				sample_edge(local_edge(cell, edge)).start_inside ?
				1 : 0);
		    }
		    bu_log("\n");
		}
	    }
	}

	return analysis;
    }

    void activate_cell(const CellKey &cell,
	    std::vector<CellKey> *activated = NULL)
    {
	auto inserted = m_active_cells.insert(cell);
	if (inserted.second) {
	    m_topology_revision++;
	    if (activated)
		activated->push_back(cell);
	}
    }

    bool refine_node(const Node &node,
	    std::vector<CellKey> *activated = NULL)
    {
	bool edge_evidence = false;
	bool interior_evidence = false;
	bool multiple_crossings = false;
	double local_thickness = 0.0;
	if (!probe_node(node, edge_evidence, interior_evidence,
		multiple_crossings, local_thickness))
	    return false;
	if (!edge_evidence && !interior_evidence)
	    return true;

	CellKey cell = {node.i, node.j, node.k, node.size};
	LeafAnalysis analysis;
	bool requires_analysis = node.size <= m_target_size;
	if (requires_analysis)
	    analysis = analyze_leaf(cell);
	if (m_control.status != ANALYZE_MDC_OK)
	    return false;

	/* A sign-changing dual edge represents one boundary crossing.  Refine
	 * until an axis chord through a thin wall spans at least one cell, which
	 * prevents its entry and exit from collapsing into the same edge segment.
	 * Requiring multiple cells here would also oversample the tangent
	 * directions of large flat sheets; geometric detail is handled separately
	 * by the contour topology and Hermite QEF. */
	bool thickness_refinement = local_thickness > 0.0 &&
	    m_cell_size * node.size >
	    local_thickness / MIN_CELLS_PER_THICKNESS;
	bool interior_only_surface = interior_evidence && !edge_evidence;
	bool needs_refinement = node.size > m_target_size ||
	    thickness_refinement || multiple_crossings ||
	    interior_only_surface || analysis.needs_refinement ||
	    analysis.normal_refinement;
	if (!needs_refinement) {
	    if (analysis.has_crossing)
		activate_cell(cell, activated);
	    return true;
	}

	if (node.size == 1) {
	    if (thickness_refinement)
		m_unresolved_thickness_cells++;
	    if (analysis.normal_refinement)
		m_unresolved_normal_cells++;
	    if (interior_only_surface) {
		if (m_control.params.verbosity)
		    bu_log("MDC: unresolved interior-only surface at maximum "
			    "depth near cell (%d,%d,%d)\n", node.i, node.j,
			    node.k);
		m_control.status = ANALYZE_MDC_AMBIGUOUS;
		return false;
	    }
	    /* Opposed crossings make the node refine, but at maximum depth they
	     * do not alone prove the represented boundary is non-manifold: a
	     * sharp concavity may cross one interior chord twice while its
	     * sign-changing edges still define valid topology.  Record these
	     * cases for diagnostics and rely on the final mesh validation. */
	    if (multiple_crossings)
		m_unresolved_topology_cells++;
	    if (!analysis.needs_refinement) {
		if (analysis.has_crossing)
		    activate_cell(cell, activated);
		return true;
	    }
	    /* An isolated tangential hit does not bound a volume to contour when
	     * the cell has no sign-changing edge.  Opposed closed excursions were
	     * handled separately above. */
	    if (!analysis.has_crossing)
		return true;
	    if (m_control.params.verbosity)
		bu_log("MDC: local surface ambiguity remains at maximum "
			"depth near cell (%d,%d,%d)\n", node.i, node.j,
			node.k);
	    m_control.status = ANALYZE_MDC_AMBIGUOUS;
	    return false;
	}

	int child_size = node.size / 2;
	for (int z_child = 0; z_child < 2; z_child++) {
	    for (int y_child = 0; y_child < 2; y_child++) {
		for (int x_child = 0; x_child < 2; x_child++) {
		    Node child = {
			node.i + x_child * child_size,
			node.j + y_child * child_size,
			node.k + z_child * child_size,
			child_size
		    };
		    if (!refine_node(child, activated))
			return false;
		}
	    }
	}
	return true;
    }

    bool discover_surface()
    {
	int baseline_size = m_resolution >> m_control.params.min_depth;
	for (int k = 0; k < m_resolution; k += baseline_size) {
	    for (int j = 0; j < m_resolution; j += baseline_size) {
		for (int i = 0; i < m_resolution; i += baseline_size) {
		    if (!refine_node({i, j, k, baseline_size}))
			return false;
		}
	    }
	}
	return true;
    }

    std::set<CellKey>::const_iterator
    find_active_leaf(const CellKey &unit_cell) const
    {
	if (!valid_unit_cell(unit_cell))
	    return m_active_cells.end();

	for (int size = 1; size <= m_resolution; size *= 2) {
	    CellKey key = {
		(unit_cell.i / size) * size,
		(unit_cell.j / size) * size,
		(unit_cell.k / size) * size,
		size
	    };
	    auto found = m_active_cells.find(key);
	    if (found != m_active_cells.end())
		return found;
	}
	return m_active_cells.end();
    }

    bool overlaps_active_leaf(const CellKey &candidate) const
    {
	CellKey origin = {candidate.i, candidate.j, candidate.k, 1};
	if (find_active_leaf(origin) != m_active_cells.end())
	    return true;

	/* Dyadic leaves are either disjoint or one contains the other.  An
	 * ancestor containing candidate's origin was checked above, so any
	 * remaining overlap must be a descendant whose origin is inside the
	 * candidate.  Restrict the ordered-set scan to its i extent instead of
	 * scanning the entire active surface. */
	CellKey lower = {candidate.i, 0, 0, 1};
	for (auto active = m_active_cells.lower_bound(lower);
		active != m_active_cells.end() &&
		active->i < candidate.i + candidate.size; ++active) {
	    if (active->j >= candidate.j &&
		    active->j < candidate.j + candidate.size &&
		    active->k >= candidate.k &&
		    active->k < candidate.k + candidate.size)
		return true;
	}
	return false;
    }

    CellKey nonoverlapping_neighbor(const CellKey &unit_cell,
	    int preferred_size) const
    {
	int size = preferred_size;
	while (size > 1) {
	    CellKey candidate = {
		(unit_cell.i / size) * size,
		(unit_cell.j / size) * size,
		(unit_cell.k / size) * size,
		size
	    };
	    if (!overlaps_active_leaf(candidate))
		return candidate;
	    size /= 2;
	}
	return {unit_cell.i, unit_cell.j, unit_cell.k, 1};
    }

    bool close_surface_cells()
    {
	std::deque<CellKey> pending(m_active_cells.begin(),
		m_active_cells.end());
	while (!pending.empty()) {
	    CellKey cell = pending.front();
	    pending.pop_front();
	    if (m_active_cells.find(cell) == m_active_cells.end())
		continue;

	    if (m_control.status != ANALYZE_MDC_OK)
		return false;

	    std::vector<std::pair<AtomicEdge, EdgeSample>> crossings;
	    bool requires_refinement = false;
	    if (!collect_atomic_crossings(cell, crossings,
		    &requires_refinement))
		return false;
	    if (requires_refinement && cell.size > 1) {
		std::vector<CellKey> activated;
		if (!split_active_leaf(cell, activated))
		    return false;
		pending.insert(pending.end(), activated.begin(),
			activated.end());
		continue;
	    }
	    for (const auto &crossing : crossings) {
		for (const CellKey &incident :
			incident_cells(crossing.first)) {
		    if (!valid_unit_cell(incident) ||
			    find_active_leaf(incident) !=
			    m_active_cells.end())
			continue;

		    CellKey neighbor = nonoverlapping_neighbor(incident,
			    cell.size);
		    if (!valid_cell(neighbor))
			continue;
		    std::vector<CellKey> activated;
		    if (!refine_node({neighbor.i, neighbor.j,
			    neighbor.k, neighbor.size}, &activated))
			return false;
		    pending.insert(pending.end(), activated.begin(),
			    activated.end());
		}
	    }
	}
	return true;
    }
    bool split_active_leaf(const CellKey &cell,
	    std::vector<CellKey> &activated)
    {
	if (cell.size <= 1)
	    return true;
	if (m_active_cells.erase(cell))
	    m_topology_revision++;
	int child_size = cell.size / 2;
	for (int z_child = 0; z_child < 2; z_child++) {
	    for (int y_child = 0; y_child < 2; y_child++) {
		for (int x_child = 0; x_child < 2; x_child++) {
		    Node child = {
			cell.i + x_child * child_size,
			cell.j + y_child * child_size,
			cell.k + z_child * child_size,
			child_size
		    };
		    if (!refine_node(child, &activated))
			return false;
		}
	    }
	}
	return true;
    }

    bool balance_surface_cells()
    {
	std::deque<CellKey> pending(m_active_cells.begin(),
		m_active_cells.end());
	std::set<CellKey> queued(m_active_cells.begin(), m_active_cells.end());
	auto enqueue = [&](const CellKey &cell) {
	    if (m_active_cells.find(cell) != m_active_cells.end() &&
		    queued.insert(cell).second)
		pending.push_back(cell);
	};

	while (!pending.empty()) {
	    CellKey cell = pending.front();
	    pending.pop_front();
	    queued.erase(cell);
	    if (m_active_cells.find(cell) == m_active_cells.end())
		continue;

	    if (m_control.status != ANALYZE_MDC_OK)
		return false;

	    std::vector<std::pair<AtomicEdge, EdgeSample>> crossings;
	    if (!collect_atomic_crossings(cell, crossings))
		return false;
	    for (const auto &crossing : crossings) {
		std::array<CellKey, 4> incident =
		    incident_cells(crossing.first);
		int smallest = m_resolution;
		CellKey largest;
		largest.size = 0;
		for (const CellKey &unit : incident) {
		    auto leaf = find_active_leaf(unit);
		    if (leaf == m_active_cells.end())
			continue;
		    smallest = std::min(smallest, leaf->size);
		    if (leaf->size > largest.size)
			largest = *leaf;
		}
		if (largest.size > 2 * smallest) {
		    std::vector<CellKey> activated;
		    if (!split_active_leaf(largest, activated))
			return false;
		    for (const CellKey &leaf : activated)
			enqueue(leaf);
		    for (const CellKey &unit : incident) {
			auto leaf = find_active_leaf(unit);
			if (leaf != m_active_cells.end())
			    enqueue(*leaf);
		    }
		    break;
		}
	    }
	}
	return true;
    }
    bool face_center_inside(const CellKey &cell, int face)
    {
	int x2 = 2 * cell.i + cell.size;
	int y2 = 2 * cell.j + cell.size;
	int z2 = 2 * cell.k + cell.size;
	LineKey key;
	double coordinate = 0.0;

	if (face == 0 || face == 1) {
	    key = {Axis::X, y2,
		2 * (cell.k + ((face == 1) ? cell.size : 0))};
	    coordinate = world(0, x2);
	} else if (face == 2 || face == 3) {
	    key = {Axis::X,
		2 * (cell.j + ((face == 3) ? cell.size : 0)), z2};
	    coordinate = world(0, x2);
	} else {
	    key = {Axis::Y,
		2 * (cell.i + ((face == 5) ? cell.size : 0)), z2};
	    coordinate = world(1, y2);
	}

	Line *sample_line = line(key);
	if (!sample_line)
	    return false;
	return sample_line->inside(coordinate);
    }

    Vec3 solve_qef(const std::vector<SurfaceHit> &hits,
	    const CellKey &cell) const
    {
	/* Ju, Losasso, Schaefer, and Warren, "Dual Contouring of Hermite
	 * Data", SIGGRAPH 2002, place a dual vertex by minimizing the planes
	 * defined by edge-intersection points and normals.  Solving relative to
	 * the mass point improves conditioning; falling back to that point also
	 * keeps a poorly constrained solution inside its owning cell. */
	Vec3 mass;
	for (const auto &hit : hits)
	    mass = mass + hit.point;
	mass = mass / static_cast<double>(hits.size());

	Eigen::Matrix3d ata = Eigen::Matrix3d::Zero();
	Eigen::Vector3d atb = Eigen::Vector3d::Zero();
	for (const auto &hit : hits) {
	    Eigen::Vector3d normal(hit.normal[0], hit.normal[1], hit.normal[2]);
	    Eigen::Vector3d delta(hit.point[0] - mass[0],
		    hit.point[1] - mass[1], hit.point[2] - mass[2]);
	    ata += normal * normal.transpose();
	    atb += normal * normal.dot(delta);
	}

	Eigen::JacobiSVD<Eigen::Matrix3d> svd(ata,
		Eigen::ComputeFullU | Eigen::ComputeFullV);
	svd.setThreshold(QEF_SINGULAR_VALUE_TOLERANCE);
	Eigen::Vector3d displacement = svd.solve(atb);
	Vec3 candidate = mass;
	for (size_t i = 0; i < 3; i++)
	    candidate[i] += displacement[static_cast<Eigen::Index>(i)];

	Vec3 cell_min;
	Vec3 cell_max;
	const int cell_index[3] = {cell.i, cell.j, cell.k};
	for (size_t i = 0; i < 3; i++) {
	    cell_min[i] = world(i, 2 * cell_index[i]);
	    cell_max[i] = cell_min[i] + m_cell_size * cell.size;
	}

	if (!finite(candidate))
	    return mass;
	for (size_t i = 0; i < 3; i++) {
	    if (candidate[i] < cell_min[i] || candidate[i] > cell_max[i])
		return mass;
	}
	return candidate;
    }

    enum analyze_mdc_status build_cell(const CellKey &cell,
	    std::vector<Vec3> &vertices)
    {
	bool active_edges[12] = {};
	EdgeSample samples[12];
	if (m_control.status != ANALYZE_MDC_OK)
	    return m_control.status;

	int crossing_count = 0;
	for (int edge = 0; edge < 12; edge++) {
	    GridEdge grid_edge = local_edge(cell, edge);
	    samples[edge] = sample_edge(grid_edge);
	    if (samples[edge].state == EdgeState::Ambiguous) {
		if (m_control.params.verbosity)
		    bu_log("MDC: ambiguous %c edge (%d,%d,%d size %d) "
			    "in cell (%d,%d,%d size %d)\n",
			    axis_name(grid_edge.axis), grid_edge.i,
			    grid_edge.j, grid_edge.k, grid_edge.size,
			    cell.i, cell.j, cell.k, cell.size);
		return ANALYZE_MDC_AMBIGUOUS;
	    }
	    active_edges[edge] =
		samples[edge].state == EdgeState::Crossing;
	    if (active_edges[edge])
		crossing_count++;
	}

	if (!crossing_count)
	    return ANALYZE_MDC_OK;

	DisjointSet components;
	for (int face = 0; face < 6; face++) {
	    int crossings[4] = {};
	    int count = 0;
	    for (int position = 0; position < 4; position++) {
		int edge = FACE_EDGES[face][position];
		if (active_edges[edge])
		    crossings[count++] = position;
	    }

	    if (count == 2) {
		components.join(FACE_EDGES[face][crossings[0]],
			FACE_EDGES[face][crossings[1]]);
	    } else if (count == 4) {
		bool center_inside = face_center_inside(cell, face);
		if (m_control.status != ANALYZE_MDC_OK)
		    return m_control.status;
		bool corner0_inside = samples[FACE_EDGES[face][0]].start_inside;
		if (center_inside == corner0_inside) {
		    components.join(FACE_EDGES[face][0],
			    FACE_EDGES[face][1]);
		    components.join(FACE_EDGES[face][2],
			    FACE_EDGES[face][3]);
		} else {
		    components.join(FACE_EDGES[face][3],
			    FACE_EDGES[face][0]);
		    components.join(FACE_EDGES[face][1],
			    FACE_EDGES[face][2]);
		}
	    } else if (count != 0) {
		if (m_control.params.verbosity)
		    bu_log("MDC: cell (%d,%d,%d size %d) face %d has %d "
			    "edge crossings\n", cell.i, cell.j, cell.k,
			    cell.size, face, count);
		return ANALYZE_MDC_AMBIGUOUS;
	    }
	}

	std::map<int, Component> grouped;
	for (int edge = 0; edge < 12; edge++) {
	    if (!active_edges[edge])
		continue;
	    int root = components.find(edge);
	    grouped[root].local_edges.push_back(edge);
	    grouped[root].hits.push_back(samples[edge].hit);
	}

	CellData data;
	for (const auto &entry : grouped) {
	    if (vertices.size() >=
		    static_cast<size_t>(std::numeric_limits<int>::max()))
		return ANALYZE_MDC_MEMORY_LIMIT;
	    const Component &component = entry.second;
	    Vec3 vertex = solve_qef(component.hits, cell);
	    int vertex_index = static_cast<int>(vertices.size());
	    vertices.push_back(vertex);
	    for (int local : component.local_edges)
		data.edge_vertices.emplace(local_edge(cell, local),
			vertex_index);
	}
	m_cells.emplace(cell, std::move(data));
	return ANALYZE_MDC_OK;
    }

    EdgeSample sample_atomic_edge(const AtomicEdge &edge)
    {
	/* Adaptive dual-contouring faces are owned by finest-grid (atomic)
	 * sign-changing edges.  Re-sampling here prevents a crossing observed on
	 * a coarse edge from being assigned to an atomic segment whose endpoints
	 * do not actually bracket that crossing. */
	GridEdge minimal = {edge.axis, edge.i, edge.j, edge.k, 1};
	return sample_edge(minimal);
    }

    /* Closure, balancing, and face emission must see the same minimal edge
     * set.  Enumerating each leaf edge at finest-grid resolution also reveals
     * coarse edges that contain multiple crossings; those leaves refine until
     * their cell vertices can represent the atomic boundary topology. */
    bool collect_atomic_crossings(const CellKey &cell,
	    std::vector<std::pair<AtomicEdge, EdgeSample>> &crossings,
	    bool *requires_refinement = NULL)
    {
	if (requires_refinement)
	    *requires_refinement = false;

	for (int local = 0; local < 12; local++) {
	    GridEdge edge = local_edge(cell, local);
	    size_t first_crossing = crossings.size();
	    for (int offset = 0; offset < edge.size; offset++) {
		AtomicEdge key = {edge.axis, edge.i, edge.j, edge.k};
		if (edge.axis == Axis::X)
		    key.i += offset;
		else if (edge.axis == Axis::Y)
		    key.j += offset;
		else
		    key.k += offset;
		EdgeSample sample = sample_atomic_edge(key);
		if (sample.state == EdgeState::Ambiguous) {
		    if (m_control.status == ANALYZE_MDC_OK)
			m_control.status = ANALYZE_MDC_AMBIGUOUS;
		    return false;
		}
		if (sample.state == EdgeState::Crossing)
		    crossings.emplace_back(key, sample);
	    }

	    if (requires_refinement) {
		EdgeSample coarse = sample_edge(edge);
		if (m_control.status != ANALYZE_MDC_OK)
		    return false;
		size_t atomic_count = crossings.size() - first_crossing;
		size_t coarse_count =
		    coarse.state == EdgeState::Crossing ? 1 : 0;
		if (coarse.state == EdgeState::Ambiguous ||
			atomic_count != coarse_count)
		    *requires_refinement = true;
	    }
	}
	return true;
    }

    std::array<CellKey, 4> incident_cells(const AtomicEdge &edge) const
    {
	if (edge.axis == Axis::X)
	    return {{{edge.i, edge.j - 1, edge.k - 1, 1},
		{edge.i, edge.j, edge.k - 1, 1},
		{edge.i, edge.j, edge.k, 1},
		{edge.i, edge.j - 1, edge.k, 1}}};
	if (edge.axis == Axis::Y)
	    return {{{edge.i - 1, edge.j, edge.k - 1, 1},
		{edge.i - 1, edge.j, edge.k, 1},
		{edge.i, edge.j, edge.k, 1},
		{edge.i, edge.j, edge.k - 1, 1}}};
	return {{{edge.i - 1, edge.j - 1, edge.k, 1},
	    {edge.i, edge.j - 1, edge.k, 1},
	    {edge.i, edge.j, edge.k, 1},
	    {edge.i - 1, edge.j, edge.k, 1}}};
    }

    std::map<CellKey, CellData>::const_iterator
    find_leaf(const CellKey &unit_cell) const
    {
	if (!valid_unit_cell(unit_cell))
	    return m_cells.end();

	for (int size = 1; size <= m_resolution; size *= 2) {
	    CellKey key = {
		(unit_cell.i / size) * size,
		(unit_cell.j / size) * size,
		(unit_cell.k / size) * size,
		size
	    };
	    auto found = m_cells.find(key);
	    if (found != m_cells.end())
		return found;
	}
	return m_cells.end();
    }

    bool same_line(const GridEdge &edge, const AtomicEdge &atomic) const
    {
	if (edge.axis != atomic.axis)
	    return false;
	if (edge.axis == Axis::X)
	    return edge.j == atomic.j && edge.k == atomic.k;
	if (edge.axis == Axis::Y)
	    return edge.i == atomic.i && edge.k == atomic.k;
	return edge.i == atomic.i && edge.j == atomic.j;
    }

    bool vertex_for_edge(
	    const std::map<CellKey, CellData>::const_iterator &cell,
	    const AtomicEdge &edge, const SurfaceHit &hit,
	    const std::vector<Vec3> &vertices, int &vertex) const
    {
	int segment = edge.axis == Axis::X ? edge.i :
	    (edge.axis == Axis::Y ? edge.j : edge.k);
	for (int local = 0; local < 12; local++) {
	    GridEdge candidate = local_edge(cell->first, local);
	    int start = edge_segment(candidate);
	    if (!same_line(candidate, edge) || segment < start ||
		    segment >= start + candidate.size)
		continue;
	    auto found = cell->second.edge_vertices.find(candidate);
	    if (found == cell->second.edge_vertices.end())
		return false;
	    vertex = found->second;
	    return true;
	}

	/* A coarse leaf may span two quadrants around an edge owned by a finer
	 * neighbor.  Its cell vertex still participates in the transition face;
	 * choose the nearest component when the fine edge is internal to a
	 * coarse face rather than one of the coarse cell's twelve edges. */
	std::set<int> candidates;
	for (const auto &entry : cell->second.edge_vertices)
	    candidates.insert(entry.second);
	double nearest_distance = std::numeric_limits<double>::infinity();
	for (int candidate : candidates) {
	    double distance = magnitude_squared(vertices[candidate] - hit.point);
	    if (distance < nearest_distance) {
		nearest_distance = distance;
		vertex = candidate;
	    }
	}
	return std::isfinite(nearest_distance);
    }

    static void add_triangle(std::vector<std::array<int, 3>> &faces,
	    int a, int b, int c)
    {
	if (a == b || b == c || a == c)
	    return;
	faces.push_back({{a, b, c}});
    }

    enum analyze_mdc_status build_faces(const std::vector<Vec3> &vertices,
	    std::vector<std::array<int, 3>> &faces)
    {
	std::map<AtomicEdge, EdgeSample> crossings;
	for (const auto &cell : m_cells) {
	    std::vector<std::pair<AtomicEdge, EdgeSample>> cell_crossings;
	    if (!collect_atomic_crossings(cell.first, cell_crossings))
		return m_control.status;
	    crossings.insert(cell_crossings.begin(), cell_crossings.end());
	}

	for (const auto &crossing : crossings) {
	    const AtomicEdge &edge = crossing.first;
	    std::array<CellKey, 4> incident = incident_cells(edge);
	    int quad[4] = {};
	    for (size_t i = 0; i < incident.size(); i++) {
		auto cell = find_leaf(incident[i]);
		if (cell == m_cells.end() ||
			!vertex_for_edge(cell, edge, crossing.second.hit,
			    vertices, quad[i])) {
		    if (m_control.params.verbosity) {
			auto active = find_active_leaf(incident[i]);
			bu_log("MDC: adaptive %c edge (%d,%d,%d) is "
				"missing incident leaf %zu",
				axis_name(edge.axis), edge.i, edge.j,
				edge.k, i);
			if (active != m_active_cells.end())
			    bu_log("; active cell is (%d,%d,%d size %d)",
				    active->i, active->j, active->k,
				    active->size);
			bu_log("\n");
		    }
		    return ANALYZE_MDC_AMBIGUOUS;
		}
	    }

	    /* The incident-cell order has a +axis normal.  A ray leaving the
	     * solid therefore uses that order; a ray entering it reverses the
	     * winding.  This remains stable for collapsed transition quads. */
	    if (!crossing.second.start_inside)
		std::swap(quad[1], quad[3]);

	    double diagonal02 = magnitude_squared(
		    vertices[quad[0]] - vertices[quad[2]]);
	    double diagonal13 = magnitude_squared(
		    vertices[quad[1]] - vertices[quad[3]]);
	    if (diagonal02 <= diagonal13) {
		add_triangle(faces, quad[0], quad[1], quad[2]);
		add_triangle(faces, quad[0], quad[2], quad[3]);
	    } else {
		add_triangle(faces, quad[0], quad[1], quad[3]);
		add_triangle(faces, quad[1], quad[2], quad[3]);
	    }
	}
	return ANALYZE_MDC_OK;
    }
};

constexpr int ContourGrid::FACE_EDGES[6][4];

static bool
finite_bounds(const point_t minimum, const point_t maximum)
{
    for (size_t i = 0; i < 3; i++) {
	if (!std::isfinite(minimum[i]) || !std::isfinite(maximum[i]) ||
		maximum[i] <= minimum[i])
	    return false;
    }
    return true;
}

static int
target_depth(const point_t minimum, const point_t maximum,
	const analyze_mdc_params &params)
{
    double extent = 0.0;
    for (size_t i = 0; i < 3; i++)
	extent = std::max(extent, maximum[i] - minimum[i]);
    extent *= 1.0 + 2.0 * ROOT_PADDING_FRACTION;

    int depth = std::max(params.min_depth, DEFAULT_TARGET_DEPTH);
    if (params.feature_size > 0.0) {
	depth = params.min_depth;
	while (depth < params.max_depth &&
		extent / static_cast<double>(1 << depth) >
		params.feature_size)
	    depth++;
    }
    return std::min(depth, params.max_depth);
}


struct AutomaticRayBudget {
    size_t effective = 1;
    size_t estimated = 1;
    size_t grid_limit = 1;
    size_t memory_limit = std::numeric_limits<size_t>::max();
};


static size_t
bounded_size(long double value)
{
    if (!std::isfinite(value) ||
	value >= static_cast<long double>(
	    std::numeric_limits<size_t>::max()))
	return std::numeric_limits<size_t>::max();
    return value <= 1.0 ? 1 : static_cast<size_t>(std::ceil(value));
}


static AutomaticRayBudget
automatic_ray_budget(const analyze_mdc_params &params, double root_size,
    int initial_depth, double surface_area, size_t characterization_rays)
{
    AutomaticRayBudget budget;
    const int finest_resolution = 1 << params.max_depth;
    const int initial_resolution = 1 << initial_depth;
    const double finest_cell_size =
	root_size / static_cast<double>(finest_resolution);
    double target_cell_size =
	root_size / static_cast<double>(initial_resolution);
    if (params.feature_size > 0.0)
	target_cell_size = std::min(target_cell_size, params.feature_size);
    target_cell_size = std::max(target_cell_size, finest_cell_size);

    long double estimated_contour_rays = 3.0L *
	static_cast<long double>(2 * initial_resolution + 1) *
	static_cast<long double>(2 * initial_resolution + 1);
    if (std::isfinite(surface_area) && surface_area > 0.0) {
	long double surface_estimate = ESTIMATED_RAYS_PER_SURFACE_CELL *
	    static_cast<long double>(surface_area) /
	    static_cast<long double>(target_cell_size * target_cell_size);
	estimated_contour_rays =
	    std::max(estimated_contour_rays, surface_estimate);
    }
    budget.estimated = bounded_size(
	static_cast<long double>(characterization_rays) +
	estimated_contour_rays);

    long double grid_lines = 3.0L *
	static_cast<long double>(2 * finest_resolution + 1) *
	static_cast<long double>(2 * finest_resolution + 1);
    budget.grid_limit = bounded_size(
	static_cast<long double>(characterization_rays) + grid_lines);

    ssize_t available_memory = bu_mem(BU_MEM_AVAIL, NULL);
    if (available_memory >= 0) {
	size_t available = static_cast<size_t>(available_memory);
	size_t usable = available > params.minimum_free_mem ?
	    available - params.minimum_free_mem : 0;
	budget.memory_limit = std::max<size_t>(1,
		usable / ESTIMATED_BYTES_PER_CONTOUR_RAY);
    }

    budget.effective = std::min(budget.grid_limit, budget.memory_limit);
    if (params.max_rays)
	budget.effective = std::min(budget.effective, params.max_rays);
    return budget;
}


static bool
valid_params(const analyze_mdc_params &params)
{
    return std::isfinite(params.feature_size) && params.feature_size >= 0.0 &&
	params.min_depth >= 1 && params.max_time >= 0 &&
	params.max_depth >= params.min_depth &&
	params.max_depth <= ANALYZE_MDC_MAX_DEPTH;
}

} /* namespace */


extern "C" enum analyze_mdc_status
analyze_mdc(int **faces, size_t *num_faces, point_t **vertices,
	size_t *num_vertices, const char *object, struct db_i *dbip,
	const struct analyze_mdc_params *parameters)
{
    if (faces)
	*faces = NULL;
    if (num_faces)
	*num_faces = 0;
    if (vertices)
	*vertices = NULL;
    if (num_vertices)
	*num_vertices = 0;

    if (!faces || !num_faces || !vertices || !num_vertices || !object ||
	    !object[0] || !dbip)
	return ANALYZE_MDC_INVALID_INPUT;

    const analyze_mdc_params defaults = ANALYZE_MDC_PARAMS_DEFAULT;
    analyze_mdc_params params = parameters ? *parameters : defaults;
    if (!valid_params(params))
	return ANALYZE_MDC_INVALID_INPUT;

    struct rt_i *rtip = rt_i_create(dbip);
    if (!rtip)
	return ANALYZE_MDC_PREP_FAILED;
    if (rt_gettree(rtip, object) < 0) {
	rt_i_destroy(rtip);
	return ANALYZE_MDC_PREP_FAILED;
    }
    rt_prep_parallel(rtip, 1);
    if (!rtip->stats.nsolids || !rtip->stats.nregions ||
	    !finite_bounds(rtip->mdl_min, rtip->mdl_max)) {
	rt_i_destroy(rtip);
	return ANALYZE_MDC_PREP_FAILED;
    }

    point_t root_min;
    double extent = 0.0;
    for (size_t i = 0; i < 3; i++)
	extent = std::max(extent, rtip->mdl_max[i] - rtip->mdl_min[i]);
    double root_size = extent * (1.0 + 2.0 * ROOT_PADDING_FRACTION);
    if (!std::isfinite(root_size) || root_size <= 0.0) {
	rt_i_destroy(rtip);
	return ANALYZE_MDC_PREP_FAILED;
    }
    const double finest_cell_size =
	root_size / static_cast<double>(1 << params.max_depth);
    for (size_t i = 0; i < 3; i++) {
	double center = rtip->mdl_min[i] +
	    0.5 * (rtip->mdl_max[i] - rtip->mdl_min[i]);
	root_min[i] = center - 0.5 * root_size +
	    GRID_PHASE_OFFSETS[i] * finest_cell_size;
    }

    int initial_depth = target_depth(rtip->mdl_min, rtip->mdl_max, params);

    const size_t requested_ray_ceiling = params.max_rays;
    ContourControl control = {params, bu_gettime()};
    double sampled_surface_area = 0.0;
    size_t characterization_rays = CHARACTERIZATION_MAX_RAYS;
    if (requested_ray_ceiling)
	characterization_rays = std::min(characterization_rays,
		requested_ray_ceiling / CHARACTERIZATION_RAY_DIVISOR);
    if (characterization_rays >= CHARACTERIZATION_MIN_RAYS) {
	struct rt_crofton_params crofton_params = {};
	crofton_params.n_rays = characterization_rays;
	if (params.max_time > 0)
	    crofton_params.time_ms = std::min(
		    CHARACTERIZATION_MAX_TIME_MS,
		    1000.0 * params.max_time *
		    CHARACTERIZATION_TIME_FRACTION);
	struct rt_crofton_result samples = RT_CROFTON_RESULT_INIT;
	int sample_status = rt_crofton_collect(&samples, rtip,
		&crofton_params, 0, rtip->mdl_min, rtip->mdl_max);
	control.ray_count += samples.ray_count;
	characterization_rays = samples.ray_count;
	if (sample_status > 0)
	    sampled_surface_area = samples.surface_area;
	rt_crofton_result_free(&samples);
    } else {
	characterization_rays = 0;
    }

    AutomaticRayBudget ray_budget = automatic_ray_budget(params, root_size,
	    initial_depth, sampled_surface_area, characterization_rays);
    params.max_rays = ray_budget.effective;
    if (params.verbosity) {
	bu_log("MDC: Crofton characterization used %zu rays; estimated "
		"surface area %.6g mm^2\n", characterization_rays,
		sampled_surface_area);
	bu_log("MDC: ray budget %zu (geometry estimate %zu, grid bound %zu, "
		"memory bound %zu%s)\n", ray_budget.effective,
		ray_budget.estimated, ray_budget.grid_limit,
		ray_budget.memory_limit,
		requested_ray_ceiling ? ", user ceiling applied" : "");
    }

    struct resource resource = RT_RESOURCE_INIT_ZERO;
    rt_init_resource(&resource, 0, rtip);
    enum analyze_mdc_status status = ANALYZE_MDC_NO_SURFACE;
    try {
	if (params.verbosity)
	    bu_log("MDC: adaptive contour for %s starts at depth %d and "
		    "refines locally through depth %d\n", object,
		    initial_depth, params.max_depth);

	std::vector<Vec3> mesh_vertices;
	std::vector<std::array<int, 3>> mesh_faces;
	ContourGrid grid(rtip, root_min, root_size, initial_depth,
		params.max_depth, control, resource);
	status = grid.build(mesh_vertices, mesh_faces);
	if (status == ANALYZE_MDC_OK) {
	    *vertices = static_cast<point_t *>(bu_calloc(
		    mesh_vertices.size(), sizeof(point_t), "MDC vertices"));
	    *faces = static_cast<int *>(bu_calloc(
		    3 * mesh_faces.size(), sizeof(int), "MDC faces"));
	    for (size_t i = 0; i < mesh_vertices.size(); i++) {
		for (size_t axis = 0; axis < 3; axis++)
		    (*vertices)[i][axis] = mesh_vertices[i][axis];
	    }
	    for (size_t i = 0; i < mesh_faces.size(); i++) {
		for (size_t corner = 0; corner < 3; corner++)
		    (*faces)[3 * i + corner] = mesh_faces[i][corner];
	    }
	    *num_vertices = mesh_vertices.size();
	    *num_faces = mesh_faces.size();
	}
    } catch (const std::bad_alloc &) {
	status = ANALYZE_MDC_MEMORY_LIMIT;
    }

    rt_clean_resource_basic(rtip, &resource);
    BU_PTBL_SET(&rtip->rti_resources, 0, NULL);
    rt_i_destroy(rtip);
    return status;
}

/*
 * Local Variables:
 * mode: C++
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
