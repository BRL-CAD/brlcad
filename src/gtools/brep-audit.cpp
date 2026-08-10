/*                     B R E P - A U D I T . C P P
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 */
/** @file brep-audit.cpp
 *
 * Isolated realization checks for BRep wireframes and fast shaded meshes.
 */

#include "common.h"

#include <algorithm>
#include <cerrno>
#include <cfloat>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <limits>
#include <random>
#include <sstream>
#include <string>
#include <vector>

#ifndef _WIN32
#  include <sys/resource.h>
#endif

#include "bu/app.h"
#include "bu/file.h"
#include "bu/opt.h"
#include "bu/datetime.h"
#include "bv/vlist.h"
#include "brep/cdt.h"
#include "brep/surfacetree.h"
#include "brep/util.h"
#include "raytrace.h"
#include "rt/geom.h"
#include "rt/primitives/brep.h"
#include "../librt/librt_private.h"

struct geom_result {
    int ret = BRLCAD_ERROR;
    size_t vertices = 0;
    size_t primitives = 0;
    size_t commands = 0;
    size_t invalid_indices = 0;
    bool finite = true;
    bool have_bbox = false;
    point_t bmin = VINIT_ZERO;
    point_t bmax = VINIT_ZERO;
    double seconds = 0.0;
    size_t peak_rss_bytes = 0;
    std::vector<std::string> issues;
};

struct prep_face_failure {
    int face_index = -1;
    brlcad::SurfaceTree::FailureReason reason =
	brlcad::SurfaceTree::FAILURE_NONE;
    ON_Interval u = ON_Interval(ON_UNSET_VALUE, ON_UNSET_VALUE);
    ON_Interval v = ON_Interval(ON_UNSET_VALUE, ON_UNSET_VALUE);
    int depth = -1;
};

struct prep_result {
    int ret = BRLCAD_ERROR;
    int face_count = 0;
    const char *failure_reason = "database_internal_load";
    double seconds = 0.0;
    size_t peak_rss_bytes = 0;
    bool have_bbox = false;
    point_t bmin = VINIT_ZERO;
    point_t bmax = VINIT_ZERO;
    std::vector<prep_face_failure> failures;
};

struct surface_tree_profile {
    prep_face_failure failure;
    int surface_index = -1;
    int loop_count = 0;
    int trim_count = 0;
    double seconds = 0.0;
    size_t nodes = 0;
    size_t leaves = 0;
    size_t curve_leaves = 0;
    int maximum_depth = 0;
    const char *surface_type = "none";
    bool native_nurbs = false;
    bool rational = false;
    int order[2] = {0, 0};
    int cv_count[2] = {0, 0};
    int span_count[2] = {0, 0};
    ON_Interval domain[2];
    int nurb_form_status = 0;
    bool nurb_form_available = false;
    bool nurb_form_rational = false;
    int nurb_form_order[2] = {0, 0};
    int nurb_form_cv_count[2] = {0, 0};
    int nurb_form_span_count[2] = {0, 0};
};

struct surface_tree_result {
    int ret = BRLCAD_ERROR;
    int face_count = 0;
    int depth_limit = BREP_MAX_FT_DEPTH;
    const char *failure_reason = "database_internal_load";
    double seconds = 0.0;
    size_t peak_rss_bytes = 0;
    bool have_bbox = false;
    point_t bmin = VINIT_ZERO;
    point_t bmax = VINIT_ZERO;
    std::vector<surface_tree_profile> faces;
};

struct surface_tree_totals {
    size_t objects = 0;
    size_t successful_objects = 0;
    size_t failed_objects = 0;
    size_t faces = 0;
    size_t failed_faces = 0;
    size_t loops = 0;
    size_t trims = 0;
    size_t nodes = 0;
    size_t leaves = 0;
    size_t curve_leaves = 0;
    size_t native_nurbs_faces = 0;
    size_t rational_faces = 0;
    size_t nurb_form_unavailable = 0;
    size_t nurb_form_status_one = 0;
    size_t nurb_form_status_two = 0;
    size_t nurb_form_status_other = 0;
    int maximum_depth = 0;
    size_t maximum_leaves = 0;
    int maximum_leaves_face = -1;
    double maximum_seconds = 0.0;
    int maximum_seconds_face = -1;
    int maximum_order[2] = {0, 0};
    int maximum_cv_count[2] = {0, 0};
    int maximum_span_count[2] = {0, 0};
    int maximum_nurb_form_order[2] = {0, 0};
    int maximum_nurb_form_cv_count[2] = {0, 0};
    int maximum_nurb_form_span_count[2] = {0, 0};
    double seconds = 0.0;
};

struct ray_audit_interval {
    double in_dist = 0.0;
    double out_dist = 0.0;
    double in_normal_dot = 0.0;
    double out_normal_dot = 0.0;
    int in_face = -1;
    int out_face = -1;
};

struct ray_audit_shot {
    int hit_count = 0;
    bool valid = true;
    bool prepared_selected = false;
    int fallback = RT_BREP_PREPARED_FALLBACK_UNSUPPORTED;
    size_t intersected_leaves = 0;
    size_t solver_calls = 0;
    size_t stored_roots = 0;
    size_t root_overflow = 0;
    size_t raw_hits = 0;
    size_t after_near_miss = 0;
    size_t after_near_hit = 0;
    size_t after_grazing = 0;
    size_t after_duplicates = 0;
    size_t after_direction_cleanup = 0;
    size_t final_hits = 0;
    size_t fixed_after_near_miss = 0;
    size_t fixed_after_near_hit = 0;
    size_t fixed_after_grazing = 0;
    size_t fixed_after_duplicates = 0;
    size_t fixed_after_direction_cleanup = 0;
    size_t fixed_cleanup_mismatches = 0;
    size_t trim_equivalence_mismatches = 0;
    size_t face_trim_status_mismatches = 0;
    size_t face_trim_hit_class_mismatches = 0;
    size_t face_trim_adjacency_mismatches = 0;
    size_t face_trim_equivalence_mismatches = 0;
    std::vector<struct rt_brep_trace_root> roots;
    std::vector<ray_audit_interval> intervals;
};

enum ray_audit_sample_kind {
    RAY_AUDIT_AXIS_CHORD = 0,
    RAY_AUDIT_RANDOM_CHORD,
    RAY_AUDIT_FACE_NORMAL_CHORD,
    RAY_AUDIT_EDGE_NORMAL_CHORD,
    RAY_AUDIT_EDGE_GRAZE_CHORD
};

struct ray_audit_sample {
    point_t origin = VINIT_ZERO;
    vect_t direction = VINIT_ZERO;
    double chord_length = 0.0;
    size_t pair_index = 0;
    bool reverse = false;
    ray_audit_sample_kind kind = RAY_AUDIT_RANDOM_CHORD;
    int face_index = -1;
    int edge_index = -1;
};

struct ray_audit_feature_target {
    point_t point = VINIT_ZERO;
    vect_t direction = VINIT_ZERO;
    ray_audit_sample_kind kind = RAY_AUDIT_FACE_NORMAL_CHORD;
    int face_index = -1;
    int edge_index = -1;
};

struct ray_audit_prep {
    int ret = BRLCAD_ERROR;
    int requested_depth = -1;
    struct rt_brep_prep_stats stats = {};
    double wall_seconds = 0.0;
};

struct ray_audit_telemetry {
    size_t rays = 0;
    size_t hit_rays = 0;
    size_t miss_rays = 0;
    size_t invalid_rays = 0;
    size_t segments = 0;
    size_t orientation_anomalies = 0;
    size_t prepared_eligible = 0;
    size_t prepared_selected = 0;
    size_t fallback[RT_BREP_PREPARED_FALLBACK_COUNT] = {};
    size_t solver_calls = 0;
    size_t candidate_surface_spans = 0;
    size_t surface_subdivision_boxes = 0;
    size_t surface_isolated_boxes = 0;
    size_t surface_fold_attempts = 0;
    size_t surface_fold_certified = 0;
    size_t surface_workspace_exhausted = 0;
    size_t surface_box_overflow = 0;
    size_t local_root_overflow = 0;
    size_t physical_event_overflow = 0;
    size_t physical_complete = 0;
    size_t physical_unresolved = 0;
    size_t physical_state_failures = 0;
    size_t seam_certified = 0;
    size_t singular_certified = 0;
    size_t regular_pair_certified = 0;
    size_t regular_stream_certified = 0;
    size_t edge_certified = 0;
    size_t vertex_certified = 0;
    size_t fixed_equivalence_mismatch_rays = 0;
    size_t local_legacy_mismatch_rays = 0;
};

struct ray_audit_depth_run {
    ray_audit_prep prep;
    ray_audit_telemetry telemetry;
    std::vector<ray_audit_shot> shots;
};

struct ray_audit_comparison {
    size_t differing_rays = 0;
    size_t candidate_invalid_rays = 0;
    size_t reference_invalid_rays = 0;
    size_t candidate_reversal_mismatches = 0;
    size_t reference_reversal_mismatches = 0;
    size_t disposition_differences = 0;
    size_t face_differences = 0;
    double maximum_distance_delta = 0.0;
    std::vector<size_t> differing_ray_indices;
    std::vector<size_t> face_difference_ray_indices;
    std::vector<size_t> candidate_reversal_indices;
    std::vector<size_t> reference_reversal_indices;
};

struct audit_prepared_brep {
    struct rt_i *rtip = NULL;
    struct soltab *stp = NULL;
    struct resource resource = {};
    bool resource_initialized = false;
};

struct trim_query_spec {
    int face_index = -1;
    ON_2dPoint uv = ON_2dPoint::UnsetPoint;
};

struct trim_polygon_range {
    int trim_index = -1;
    bool have_points = false;
    ON_2dPoint raw_start = ON_2dPoint::UnsetPoint;
    ON_2dPoint raw_end = ON_2dPoint::UnsetPoint;
    ON_2dPoint curve_start = ON_2dPoint::UnsetPoint;
    ON_2dPoint curve_end = ON_2dPoint::UnsetPoint;
    ON_2dPoint cover_start = ON_2dPoint::UnsetPoint;
    ON_2dPoint cover_end = ON_2dPoint::UnsetPoint;
    ON_2dPoint cover_min = ON_2dPoint::UnsetPoint;
    ON_2dPoint cover_max = ON_2dPoint::UnsetPoint;
};

struct trim_polygon_loop {
    int loop_index = -1;
    ON_BrepLoop::TYPE type = ON_BrepLoop::unknown;
    size_t trim_count = 0;
    size_t invalid_samples = 0;
    double maximum_join_gap = 0.0;
    double closing_gap = 0.0;
    double cover_closing_gap = 0.0;
    double signed_area = 0.0;
    double minimum_boundary_distance = INFINITY;
    size_t crossings = 0;
    size_t vertical_crossings = 0;
    bool inside = false;
    bool have_bbox = false;
    ON_2dPoint bbox_min = ON_2dPoint::UnsetPoint;
    ON_2dPoint bbox_max = ON_2dPoint::UnsetPoint;
    ON_2dPoint query_image = ON_2dPoint::UnsetPoint;
    std::vector<trim_polygon_range> trim_ranges;
    std::vector<ON_2dPoint> points;
};


static void
surface_tree_node_counts(const brlcad::BBNode *node, int depth,
    size_t &nodes, size_t &leaves, int &maximum_depth)
{
    if (!node)
	return;
    nodes++;
    maximum_depth = std::max(maximum_depth, depth);
    const std::vector<brlcad::BBNode *> &children = node->get_children();
    if (children.empty()) {
	leaves++;
	return;
    }
    for (std::vector<brlcad::BBNode *>::const_iterator child =
	    children.begin(); child != children.end(); ++child)
	surface_tree_node_counts(*child, depth + 1, nodes, leaves,
	    maximum_depth);
}

static size_t
peak_rss_bytes()
{
#ifndef _WIN32
    struct rusage usage_info;
    if (getrusage(RUSAGE_SELF, &usage_info) != 0)
	return 0;
#  ifdef __APPLE__
    return (size_t)usage_info.ru_maxrss;
#  else
    return (size_t)usage_info.ru_maxrss * 1024U;
#  endif
#else
    return 0;
#endif
}

static bool
set_memory_limit(long memory_limit_mib)
{
    if (memory_limit_mib <= 0)
	return true;
#ifndef _WIN32
    const rlim_t scale = (rlim_t)1024 * (rlim_t)1024;
    const rlim_t limit = (rlim_t)memory_limit_mib * scale;
    if (limit / scale != (rlim_t)memory_limit_mib)
	return false;
    struct rlimit current;
    if (getrlimit(RLIMIT_AS, &current) != 0)
	return false;
    if (current.rlim_max != RLIM_INFINITY && limit > current.rlim_max)
	return false;
    current.rlim_cur = limit;
    return setrlimit(RLIMIT_AS, &current) == 0;
#else
    return false;
#endif
}

static std::string
json_quote(const char *str)
{
    std::ostringstream out;
    out << '"';
    const unsigned char *p = (const unsigned char *)(str ? str : "");
    while (*p) {
	switch (*p) {
	    case '"': out << "\\\""; break;
	    case '\\': out << "\\\\"; break;
	    case '\b': out << "\\b"; break;
	    case '\f': out << "\\f"; break;
	    case '\n': out << "\\n"; break;
	    case '\r': out << "\\r"; break;
	    case '\t': out << "\\t"; break;
	    default:
		if (*p < 0x20) {
		    out << "\\u" << std::hex << std::setw(4) << std::setfill('0')
			<< (unsigned int)*p << std::dec << std::setfill(' ');
		} else {
		    out << (char)*p;
		}
	}
	p++;
    }
    out << '"';
    return out.str();
}

static void
bbox_add(point_t bmin, point_t bmax, bool *have_bbox, const point_t p)
{
    if (!*have_bbox) {
	VMOVE(bmin, p);
	VMOVE(bmax, p);
	*have_bbox = true;
	return;
    }
    VMINMAX(bmin, bmax, p);
}

static bool
drawable_vlist_cmd(int cmd)
{
    switch (cmd) {
	case BV_VLIST_LINE_MOVE:
	case BV_VLIST_LINE_DRAW:
	case BV_VLIST_POLY_MOVE:
	case BV_VLIST_POLY_DRAW:
	case BV_VLIST_POLY_END:
	case BV_VLIST_TRI_MOVE:
	case BV_VLIST_TRI_DRAW:
	case BV_VLIST_TRI_END:
	case BV_VLIST_POINT_DRAW:
	    return true;
	default:
	    return false;
    }
}

static struct db_i *
open_db(const char *path)
{
    struct db_i *dbip = db_open(path, DB_OPEN_READONLY);
    if (dbip == DBI_NULL)
	return DBI_NULL;
    if (db_dirbuild(dbip) < 0) {
	db_close(dbip);
	return DBI_NULL;
    }
    return dbip;
}

static int
load_brep(struct db_i *dbip, struct directory *dp, struct rt_db_internal *intern)
{
    RT_DB_INTERNAL_INIT(intern);
    if (rt_db_get_internal(intern, dp, dbip, NULL) < 0)
	return BRLCAD_ERROR;
    if (intern->idb_minor_type != DB5_MINORTYPE_BRLCAD_BREP) {
	rt_db_free_internal(intern);
	return BRLCAD_ERROR;
    }
    return BRLCAD_OK;
}

static bool
parse_trim_query(const char *arg, trim_query_spec &query)
{
    if (!arg || !arg[0])
	return false;
    char *end = NULL;
    errno = 0;
    const long face_index = strtol(arg, &end, 10);
    if (errno || end == arg || *end != ',' || face_index < 0 ||
	    face_index > INT_MAX)
	return false;
    const char *u_start = end + 1;
    errno = 0;
    const double u = strtod(u_start, &end);
    if (errno || end == u_start || *end != ',' || !std::isfinite(u))
	return false;
    const char *v_start = end + 1;
    errno = 0;
    const double v = strtod(v_start, &end);
    if (errno || end == v_start || *end || !std::isfinite(v))
	return false;
    query.face_index = (int)face_index;
    query.uv = ON_2dPoint(u, v);
    return true;
}

static const char *
trim_loop_type_name(ON_BrepLoop::TYPE type)
{
    switch (type) {
	case ON_BrepLoop::outer: return "outer";
	case ON_BrepLoop::inner: return "inner";
	case ON_BrepLoop::slit: return "slit";
	case ON_BrepLoop::crvonsrf: return "curve_on_surface";
	case ON_BrepLoop::ptonsrf: return "point_on_surface";
	case ON_BrepLoop::unknown: return "unknown";
	case ON_BrepLoop::type_count: return "type_count";
    }
    return "invalid";
}

static double
trim_periodic_delta(double first, double second, bool closed,
    double period)
{
    double delta = first - second;
    if (closed && period > 0.0 && std::isfinite(period))
	delta -= nearbyint(delta / period) * period;
    return delta;
}

static double
trim_periodic_distance(const ON_2dPoint &first, const ON_2dPoint &second,
    const bool closed[2], const double period[2])
{
    const double du = trim_periodic_delta(first.x, second.x, closed[0],
	period[0]);
    const double dv = trim_periodic_delta(first.y, second.y, closed[1],
	period[1]);
    return hypot(du, dv);
}

static ON_2dPoint
trim_unwrap_after(const ON_2dPoint &point, const ON_2dPoint &previous,
    const bool closed[2], const double period[2])
{
    ON_2dPoint unwrapped(point);
    if (closed[0] && period[0] > 0.0)
	unwrapped.x += nearbyint((previous.x - unwrapped.x) / period[0]) *
	    period[0];
    if (closed[1] && period[1] > 0.0)
	unwrapped.y += nearbyint((previous.y - unwrapped.y) / period[1]) *
	    period[1];
    return unwrapped;
}

static double
trim_point_segment_distance(const ON_2dPoint &query,
    const ON_2dPoint &first, const ON_2dPoint &second)
{
    const double dx = second.x - first.x;
    const double dy = second.y - first.y;
    const double length_squared = dx * dx + dy * dy;
    if (!(length_squared > 0.0))
	return query.DistanceTo(first);
    const double parameter = std::max(0.0, std::min(1.0,
	((query.x - first.x) * dx + (query.y - first.y) * dy) /
	length_squared));
    return hypot(query.x - (first.x + parameter * dx),
	query.y - (first.y + parameter * dy));
}

static void
trim_polygon_classify(trim_polygon_loop &result, const ON_2dPoint &query,
    const bool closed[2], const double period[2])
{
    if (result.points.size() < 2)
	return;
    result.bbox_min = result.points[0];
    result.bbox_max = result.points[0];
    for (size_t i = 1; i < result.points.size(); ++i) {
	result.bbox_min.x = std::min(result.bbox_min.x, result.points[i].x);
	result.bbox_min.y = std::min(result.bbox_min.y, result.points[i].y);
	result.bbox_max.x = std::max(result.bbox_max.x, result.points[i].x);
	result.bbox_max.y = std::max(result.bbox_max.y, result.points[i].y);
    }
    result.have_bbox = true;
    result.query_image = query;
    if (closed[0] && period[0] > 0.0) {
	const double center = 0.5 * (result.bbox_min.x + result.bbox_max.x);
	result.query_image.x += nearbyint((center - result.query_image.x) /
	    period[0]) * period[0];
    }
    if (closed[1] && period[1] > 0.0) {
	const double center = 0.5 * (result.bbox_min.y + result.bbox_max.y);
	result.query_image.y += nearbyint((center - result.query_image.y) /
	    period[1]) * period[1];
    }

    const size_t count = result.points.size();
    for (size_t current = 0, previous = count - 1; current < count;
	    previous = current++) {
	const ON_2dPoint &first = result.points[previous];
	const ON_2dPoint &second = result.points[current];
	result.minimum_boundary_distance = std::min(
	    result.minimum_boundary_distance,
	    trim_point_segment_distance(result.query_image, first, second));
	result.signed_area += first.x * second.y - second.x * first.y;
	if (((second.y <= result.query_image.y &&
		result.query_image.y < first.y) ||
		(first.y <= result.query_image.y &&
		result.query_image.y < second.y)) &&
		result.query_image.x <
		(first.x - second.x) *
		(result.query_image.y - second.y) /
		(first.y - second.y) + second.x)
	    result.crossings++;
	if (((second.x <= result.query_image.x &&
		result.query_image.x < first.x) ||
		(first.x <= result.query_image.x &&
		 result.query_image.x < second.x)) &&
		result.query_image.y <
		(first.y - second.y) *
		(result.query_image.x - second.x) /
		(first.x - second.x) + second.y)
	    result.vertical_crossings++;
    }
    result.signed_area *= 0.5;
    result.inside = (result.crossings & 1U) != 0;
}

static trim_polygon_loop
trim_sample_loop(const ON_BrepFace &face, const ON_BrepLoop &loop,
    size_t segments_per_trim, const ON_2dPoint &query,
    const bool closed[2], const double period[2])
{
    trim_polygon_loop result;
    result.loop_index = loop.m_loop_index;
    result.type = loop.m_type;
    result.trim_count = (size_t)std::max(0, loop.TrimCount());
    result.trim_ranges.reserve(result.trim_count);
    result.points.reserve(result.trim_count * (segments_per_trim + 1));
    ON_2dPoint first_raw = ON_2dPoint::UnsetPoint;
    ON_2dPoint previous_raw_end = ON_2dPoint::UnsetPoint;
    bool have_first = false;
    bool have_previous_end = false;

    for (int trim_index = 0; trim_index < loop.TrimCount(); ++trim_index) {
	const ON_BrepTrim *trim = loop.Trim(trim_index);
	if (!trim) {
	    result.invalid_samples += segments_per_trim + 1;
	    continue;
	}
	const ON_Interval domain = trim->Domain();
	trim_polygon_range range;
	range.trim_index = trim->m_trim_index;
	const ON_Curve *trim_curve = trim->TrimCurveOf();
	if (trim_curve) {
	    const ON_Interval curve_domain = trim_curve->Domain();
	    const ON_3dPoint curve_start = trim_curve->PointAt(
		curve_domain.Min());
	    const ON_3dPoint curve_end = trim_curve->PointAt(
		curve_domain.Max());
	    range.curve_start = ON_2dPoint(curve_start.x, curve_start.y);
	    range.curve_end = ON_2dPoint(curve_end.x, curve_end.y);
	}
	ON_2dPoint trim_start = ON_2dPoint::UnsetPoint;
	ON_2dPoint trim_end = ON_2dPoint::UnsetPoint;
	bool have_trim_start = false;
	bool have_trim_end = false;
	for (size_t sample = 0; sample <= segments_per_trim; ++sample) {
	    const double fraction = (double)sample /
		(double)segments_per_trim;
	    const ON_3dPoint evaluated = trim->PointAt(
		domain.ParameterAt(fraction));
	    const ON_2dPoint raw(evaluated.x, evaluated.y);
	    if (!std::isfinite(raw.x) || !std::isfinite(raw.y)) {
		result.invalid_samples++;
		continue;
	    }
	    if (!have_trim_start) {
		trim_start = raw;
		have_trim_start = true;
	    }
	    trim_end = raw;
	    have_trim_end = true;
	    ON_2dPoint unwrapped(raw);
	    if (!result.points.empty())
		unwrapped = trim_unwrap_after(raw, result.points.back(), closed,
		    period);
	    result.points.push_back(unwrapped);
	    if (!range.have_points) {
		range.raw_start = raw;
		range.cover_start = unwrapped;
		range.cover_min = unwrapped;
		range.cover_max = unwrapped;
		range.have_points = true;
	    }
	    range.raw_end = raw;
	    range.cover_end = unwrapped;
	    range.cover_min.x = std::min(range.cover_min.x, unwrapped.x);
	    range.cover_min.y = std::min(range.cover_min.y, unwrapped.y);
	    range.cover_max.x = std::max(range.cover_max.x, unwrapped.x);
	    range.cover_max.y = std::max(range.cover_max.y, unwrapped.y);
	}
	result.trim_ranges.push_back(range);
	if (have_trim_start) {
	    if (!have_first) {
		first_raw = trim_start;
		have_first = true;
	    }
	    if (have_previous_end)
		result.maximum_join_gap = std::max(result.maximum_join_gap,
		    trim_periodic_distance(previous_raw_end, trim_start, closed,
			period));
	}
	if (have_trim_end) {
	    previous_raw_end = trim_end;
	    have_previous_end = true;
	}
    }
    if (have_first && have_previous_end) {
	result.closing_gap = trim_periodic_distance(previous_raw_end,
	    first_raw, closed, period);
	result.maximum_join_gap = std::max(result.maximum_join_gap,
	    result.closing_gap);
    }
    if (result.points.size() > 1)
	result.cover_closing_gap = result.points.front().DistanceTo(
	    result.points.back());
    trim_polygon_classify(result, query, closed, period);
    (void)face;
    return result;
}

static int
trim_query(struct db_i *dbip, const char *db_path, struct directory *dp,
    const trim_query_spec &query)
{
    struct rt_db_internal intern;
    if (load_brep(dbip, dp, &intern) != BRLCAD_OK)
	return 2;
    struct rt_brep_internal *bi =
	(struct rt_brep_internal *)intern.idb_ptr;
    RT_BREP_CK_MAGIC(bi);
    if (query.face_index < 0 || query.face_index >= bi->brep->m_F.Count()) {
	std::cerr << "Face index is out of range: " << query.face_index << "\n";
	rt_db_free_internal(&intern);
	return 2;
    }
    const ON_BrepFace &face = bi->brep->m_F[query.face_index];
    const ON_Surface *surface = face.SurfaceOf();
    if (!surface) {
	std::cerr << "Face has no surface: " << query.face_index << "\n";
	rt_db_free_internal(&intern);
	return 2;
    }
    const ON_Interval domain[2] = {surface->Domain(0), surface->Domain(1)};
    const bool closed[2] = {surface->IsClosed(0), surface->IsClosed(1)};
    const double period[2] = {domain[0].Length(), domain[1].Length()};

    brlcad::CurveTree curve_tree(&face);
    const brlcad::BRNode *closest = NULL;
    double closest_distance = -1.0;
    size_t candidates = 0;
    const bool production_trimmed = curve_tree.isTrimmed(query.uv,
	&closest, closest_distance, BREP_EDGE_MISS_TOLERANCE, &candidates);
    const brlcad::BRNode *exact_closest = NULL;
    double exact_closest_distance = -1.0;
    size_t exact_candidates = 0;
    const bool exact_trimmed = curve_tree.isTrimmed(query.uv,
	&exact_closest, exact_closest_distance, BREP_EDGE_MISS_TOLERANCE,
	&exact_candidates, false);

    ON_2dPoint crossing_query(query.uv);
    for (int direction = 0; direction < 2; ++direction) {
	if (!closed[direction] || !(period[direction] > 0.0))
	    continue;
	crossing_query[direction] -= floor(
	    (crossing_query[direction] - domain[direction].Min()) /
	    period[direction]) * period[direction];
	if (crossing_query[direction] >= domain[direction].Max())
	    crossing_query[direction] = domain[direction].Min();
    }
    struct curve_crossing {
	int trim_index;
	int adjoining_face;
	bool inner;
	bool vertical;
	double distance;
	ON_Interval parameter;
	ON_Interval curve_domain;
	ON_3dPoint start;
	ON_3dPoint end;
	point_t minimum;
	point_t maximum;
    };
    std::vector<curve_crossing> curve_crossings;
    const bool horizontal_classification_ray = closed[1];
    std::list<const brlcad::BRNode *> curve_leaves;
    curve_tree.getLeaves(curve_leaves);
    for (std::list<const brlcad::BRNode *>::const_iterator leaf =
	    curve_leaves.begin(); leaf != curve_leaves.end(); ++leaf) {
	curve_crossing event;
	event.distance = -1.0;
	if (!(horizontal_classification_ray ?
		(*leaf)->crossesRight(crossing_query, event.distance, 0.0) :
		(*leaf)->crossesAbove(crossing_query, event.distance, 0.0)))
	    continue;
	event.trim_index = (*leaf)->trimIndex();
	event.adjoining_face = (*leaf)->m_adj_face_index;
	event.inner = (*leaf)->m_innerTrim;
	event.vertical = (*leaf)->m_Vertical;
	event.parameter = (*leaf)->parameterInterval();
	event.curve_domain = (*leaf)->curveDomain();
	event.start = (*leaf)->startPoint();
	event.end = (*leaf)->endPoint();
	(*leaf)->GetBBox(event.minimum, event.maximum);
	curve_crossings.push_back(event);
    }

    static const size_t levels[] = {16, 64, 256, 1024, 4096};
    bool classifications[sizeof(levels) / sizeof(levels[0])] = {};
    bool level_valid[sizeof(levels) / sizeof(levels[0])] = {};
    std::cout << std::setprecision(17)
	<< "{\"format\":\"brlcad-brep-trim-query-v1\""
	<< ",\"database\":" << json_quote(db_path)
	<< ",\"object\":" << json_quote(dp->d_namep)
	<< ",\"face\":" << query.face_index
	<< ",\"query_uv\":[" << query.uv.x << "," << query.uv.y << "]"
	<< ",\"surface\":{\"domain_u\":[" << domain[0].Min() << ","
	<< domain[0].Max() << "],\"domain_v\":[" << domain[1].Min()
	<< "," << domain[1].Max() << "],\"closed_u\":"
	<< (closed[0] ? "true" : "false") << ",\"closed_v\":"
	<< (closed[1] ? "true" : "false") << "}"
	<< ",\"production\":{\"trimmed\":"
	<< (production_trimmed ? "true" : "false")
	<< ",\"closest_distance\":" << closest_distance
	<< ",\"candidates\":" << candidates
	<< ",\"prepared_leaf_images\":"
	<< curve_tree.preparedLeafImageCount()
	<< ",\"closest_adjoining_face\":"
	<< (closest ? closest->m_adj_face_index : -1) << "}"
	<< ",\"exact\":{\"trimmed\":"
	<< (exact_trimmed ? "true" : "false")
	<< ",\"closest_distance\":" << exact_closest_distance
	<< ",\"candidates\":" << exact_candidates
	<< ",\"closest_adjoining_face\":"
	<< (exact_closest ? exact_closest->m_adj_face_index : -1) << "}"
	<< ",\"crossing_query_uv\":[" << crossing_query.x << ","
	<< crossing_query.y << "]"
	<< ",\"classification_ray\":" << json_quote(
	    horizontal_classification_ray ? "positive_u" : "positive_v")
	<< ",\"curve_crossings_mode\":\"exact_native_half_open\""
	<< ",\"curve_crossings\":[";
    for (size_t crossing_index = 0;
	    crossing_index < curve_crossings.size(); ++crossing_index) {
	if (crossing_index)
	    std::cout << ",";
	const curve_crossing &event = curve_crossings[crossing_index];
	std::cout << "{\"trim\":" << event.trim_index
	    << ",\"adjoining_face\":" << event.adjoining_face
	    << ",\"inner\":" << (event.inner ? "true" : "false")
	    << ",\"vertical\":" << (event.vertical ? "true" : "false")
	    << ",\"distance\":" << event.distance
	    << ",\"parameter\":[" << event.parameter.Min() << ","
	    << event.parameter.Max() << "]"
	    << ",\"curve_domain\":[" << event.curve_domain.Min() << ","
	    << event.curve_domain.Max() << "]"
	    << ",\"start\":[" << event.start.x << "," << event.start.y
	    << "]"
	    << ",\"end\":[" << event.end.x << "," << event.end.y << "]"
	    << ",\"bbox\":[[" << event.minimum[X] << ","
	    << event.minimum[Y] << "],[" << event.maximum[X] << ","
	    << event.maximum[Y] << "]]}";
    }
    std::cout << "]"
	<< ",\"levels\":[";

    for (size_t level = 0; level < sizeof(levels) / sizeof(levels[0]);
	    ++level) {
	if (level)
	    std::cout << ",";
	std::vector<trim_polygon_loop> loops;
	loops.reserve((size_t)face.LoopCount());
	bool any_outer = false;
	bool inside_outer = false;
	bool inside_inner = false;
	bool parity = false;
	bool valid = true;
	double minimum_distance = INFINITY;
	for (int loop_slot = 0; loop_slot < face.LoopCount(); ++loop_slot) {
	    const ON_BrepLoop *loop = face.Loop(loop_slot);
	    if (!loop)
		continue;
	    loops.push_back(trim_sample_loop(face, *loop, levels[level],
		query.uv, closed, period));
	    const trim_polygon_loop &sampled = loops.back();
	    valid = valid && sampled.invalid_samples == 0 &&
		sampled.points.size() >= 3 &&
		sampled.cover_closing_gap <= BREP_EDGE_MISS_TOLERANCE;
	    minimum_distance = std::min(minimum_distance,
		sampled.minimum_boundary_distance);
	    if (sampled.type == ON_BrepLoop::outer) {
		any_outer = true;
		inside_outer = inside_outer || sampled.inside;
	    } else if (sampled.type == ON_BrepLoop::inner) {
		inside_inner = inside_inner || sampled.inside;
	    }
	    if (sampled.type == ON_BrepLoop::outer ||
		    sampled.type == ON_BrepLoop::inner ||
		    sampled.type == ON_BrepLoop::unknown)
		parity = parity != sampled.inside;
	}
	const bool polygon_inside = any_outer ?
	    inside_outer && !inside_inner : parity;
	classifications[level] = polygon_inside;
	level_valid[level] = valid;
	std::cout << "{\"segments_per_trim\":" << levels[level]
	    << ",\"valid\":" << (valid ? "true" : "false")
	    << ",\"inside\":" << (polygon_inside ? "true" : "false")
	    << ",\"trimmed\":" << (polygon_inside ? "false" : "true")
	    << ",\"matches_production\":";
	if (valid)
	    std::cout << (production_trimmed != polygon_inside ?
		"true" : "false");
	else
	    std::cout << "null";
	std::cout << ",\"minimum_boundary_distance\":" << minimum_distance
	    << ",\"stable_from_previous\":"
	    << (level > 0 && classifications[level] ==
		classifications[level - 1] ? "true" : "false")
	    << ",\"loops\":[";
	for (size_t loop_index = 0; loop_index < loops.size(); ++loop_index) {
	    const trim_polygon_loop &sampled = loops[loop_index];
	    if (loop_index)
		std::cout << ",";
	    std::cout << "{\"loop\":" << sampled.loop_index
		<< ",\"type\":" << json_quote(
		    trim_loop_type_name(sampled.type))
		<< ",\"trims\":" << sampled.trim_count
		<< ",\"points\":" << sampled.points.size()
		<< ",\"invalid_samples\":" << sampled.invalid_samples
		<< ",\"inside\":" << (sampled.inside ? "true" : "false")
		<< ",\"crossings\":" << sampled.crossings
		<< ",\"vertical_crossings\":"
		<< sampled.vertical_crossings
		<< ",\"signed_area\":" << sampled.signed_area
		<< ",\"minimum_boundary_distance\":"
		<< sampled.minimum_boundary_distance
		<< ",\"maximum_topology_join_gap\":"
		<< sampled.maximum_join_gap
		<< ",\"closing_gap\":" << sampled.closing_gap
		<< ",\"cover_closing_gap\":" << sampled.cover_closing_gap
		<< ",\"lift_closed\":" <<
		(sampled.cover_closing_gap <= BREP_EDGE_MISS_TOLERANCE ?
		 "true" : "false")
		<< ",\"query_image\":[" << sampled.query_image.x << ","
		<< sampled.query_image.y << "]"
		<< ",\"bbox\":[[" << sampled.bbox_min.x << ","
		<< sampled.bbox_min.y << "],[" << sampled.bbox_max.x << ","
		<< sampled.bbox_max.y << "]]"
		<< ",\"trim_ranges\":[";
	    for (size_t range_index = 0;
		    range_index < sampled.trim_ranges.size(); ++range_index) {
		if (range_index)
		    std::cout << ",";
		const trim_polygon_range &range =
		    sampled.trim_ranges[range_index];
		std::cout << "{\"trim\":" << range.trim_index
		    << ",\"valid\":" << (range.have_points ? "true" : "false");
		if (range.have_points) {
		    std::cout << ",\"raw_start\":[" << range.raw_start.x << ","
			<< range.raw_start.y << "]"
		    << ",\"raw_end\":[" << range.raw_end.x << ","
			<< range.raw_end.y << "]"
			<< ",\"curve_start\":[" << range.curve_start.x << ","
			<< range.curve_start.y << "]"
			<< ",\"curve_end\":[" << range.curve_end.x << ","
			<< range.curve_end.y << "]"
			<< ",\"cover_start\":[" << range.cover_start.x << ","
			<< range.cover_start.y << "]"
			<< ",\"cover_end\":[" << range.cover_end.x << ","
			<< range.cover_end.y << "]"
			<< ",\"cover_bbox\":[[" << range.cover_min.x << ","
			<< range.cover_min.y << "],[" << range.cover_max.x << ","
			<< range.cover_max.y << "]]";
		}
		std::cout << "}";
	    }
	    std::cout << "]}";
	}
	std::cout << "]}";
    }
    const size_t level_count = sizeof(levels) / sizeof(levels[0]);
    const bool converged = level_valid[level_count - 1] &&
	level_valid[level_count - 2] && level_valid[level_count - 3] &&
	classifications[level_count - 1] == classifications[level_count - 2] &&
	classifications[level_count - 2] == classifications[level_count - 3];
    std::cout << "],\"converged\":" << (converged ? "true" : "false")
	<< ",\"oracle_applicable\":" << (converged ? "true" : "false")
	<< ",\"final_matches_production\":";
    if (converged)
	std::cout << (production_trimmed != classifications[level_count - 1] ?
	    "true" : "false");
    else
	std::cout << "null";
    std::cout << "}\n";
    rt_db_free_internal(&intern);
    return converged ? 0 : 1;
}

static const char *
surface_tree_failure_name(brlcad::SurfaceTree::FailureReason reason)
{
    switch (reason) {
	case brlcad::SurfaceTree::FAILURE_NONE:
	    return "none";
	case brlcad::SurfaceTree::FAILURE_NULL_SURFACE:
	    return "null_surface";
	case brlcad::SurfaceTree::FAILURE_BOUNDING_BOX:
	    return "bounding_box";
	case brlcad::SurfaceTree::FAILURE_NORMAL_EVALUATION:
	    return "normal_evaluation";
	case brlcad::SurfaceTree::FAILURE_SUBDIVISION:
	    return "subdivision";
    }
    return "unknown";
}

static void
diagnose_surface_tree_failures(struct db_i *dbip, struct directory *dp,
	prep_result *result)
{
    struct rt_db_internal intern;
    if (load_brep(dbip, dp, &intern) != BRLCAD_OK)
	return;
    struct rt_brep_internal *bi =
	(struct rt_brep_internal *)intern.idb_ptr;
    RT_BREP_CK_MAGIC(bi);
    result->face_count = bi->brep->m_F.Count();
    for (int i = 0; i < result->face_count; ++i) {
	brlcad::SurfaceTree tree(&bi->brep->m_F[i], true, 8);
	if (tree.Valid())
	    continue;
	prep_face_failure failure;
	failure.face_index = i;
	failure.reason = tree.Failure();
	failure.u = tree.FailureDomain(0);
	failure.v = tree.FailureDomain(1);
	failure.depth = tree.FailureDepth();
	result->failures.push_back(failure);
    }
    rt_db_free_internal(&intern);
}

static prep_result
raytrace_prep_result(struct db_i *dbip, struct directory *dp)
{
    prep_result result;
    struct rt_db_internal intern;
    if (load_brep(dbip, dp, &intern) != BRLCAD_OK)
	return result;
    result.failure_reason = "none";
    struct rt_brep_internal *bi =
	(struct rt_brep_internal *)intern.idb_ptr;
    RT_BREP_CK_MAGIC(bi);
    const ON_BoundingBox bbox = bi->brep->BoundingBox();
    if (bbox.IsValid()) {
	VMOVE(result.bmin, bbox.m_min);
	VMOVE(result.bmax, bbox.m_max);
	result.have_bbox = true;
    }
    result.face_count = bi->brep->m_F.Count();

    struct rt_i *rtip = rt_dirbuild_inmem(NULL, 0, NULL, 0);
    struct soltab *stp = (struct soltab *)bu_calloc(1,
	sizeof(struct soltab), "brep audit prep soltab");
    stp->l.magic = RT_SOLTAB_MAGIC;
    stp->l2.magic = RT_SOLTAB2_MAGIC;
    stp->st_rtip = rtip;
    stp->st_id = ID_BREP;
    stp->st_meth = &OBJ[ID_BREP];

    int64_t start = bu_gettime();
    result.ret = rtip ? OBJ[ID_BREP].ft_prep(stp, &intern, rtip) :
	BRLCAD_ERROR;
    result.seconds = (bu_gettime() - start) / 1000000.0;
    result.peak_rss_bytes = peak_rss_bytes();
    if (result.ret == BRLCAD_OK) {
	VMOVE(result.bmin, stp->st_min);
	VMOVE(result.bmax, stp->st_max);
	result.have_bbox = true;
    }
    if (stp->st_specific && stp->st_meth && stp->st_meth->ft_free)
	stp->st_meth->ft_free(stp);
    bu_free(stp, "brep audit prep soltab");
    if (rtip)
	rt_i_destroy(rtip);
    rt_db_free_internal(&intern);

    if (result.ret != BRLCAD_OK)
	diagnose_surface_tree_failures(dbip, dp, &result);
    if (result.ret != BRLCAD_OK) {
	if (result.face_count == 0)
	    result.failure_reason = "empty_brep";
	else if (!result.failures.empty())
	    result.failure_reason = "surface_tree";
	else
	    result.failure_reason = "unknown_prep";
    }
    return result;
}

static surface_tree_result
surface_tree_profile_result(struct db_i *dbip, struct directory *dp,
    int depth_limit)
{
    surface_tree_result result;
    result.depth_limit = depth_limit;
    struct rt_db_internal intern;
    if (load_brep(dbip, dp, &intern) != BRLCAD_OK)
	return result;
    result.failure_reason = "none";
    struct rt_brep_internal *bi =
	(struct rt_brep_internal *)intern.idb_ptr;
    RT_BREP_CK_MAGIC(bi);
    const ON_BoundingBox bbox = bi->brep->BoundingBox();
    if (bbox.IsValid()) {
	VMOVE(result.bmin, bbox.m_min);
	VMOVE(result.bmax, bbox.m_max);
	result.have_bbox = true;
    }
    result.face_count = bi->brep->m_F.Count();
    result.ret = result.face_count ? BRLCAD_OK : BRLCAD_ERROR;
    if (!result.face_count)
	result.failure_reason = "empty_brep";
    int64_t total_start = bu_gettime();
    result.faces.reserve((size_t)result.face_count);
    for (int i = 0; i < result.face_count; ++i) {
	const ON_BrepFace &face = bi->brep->m_F[i];
	const ON_Surface *surface = face.SurfaceOf();
	surface_tree_profile profile;
	profile.failure.face_index = i;
	profile.surface_index = face.m_si;
	profile.loop_count = face.LoopCount();
	for (int loop_index = 0; loop_index < profile.loop_count;
		++loop_index)
	    profile.trim_count += face.Loop(loop_index)->TrimCount();
	if (surface) {
	    const ON_ClassId *class_id = surface->ClassId();
	    profile.surface_type = class_id ? class_id->ClassName() :
		"unknown";
	    profile.domain[0] = surface->Domain(0);
	    profile.domain[1] = surface->Domain(1);
	    const ON_NurbsSurface *nurbs = ON_NurbsSurface::Cast(surface);
	    if (nurbs) {
		profile.native_nurbs = true;
		profile.rational = nurbs->IsRational();
		for (int direction = 0; direction < 2; ++direction) {
		    profile.order[direction] = nurbs->Order(direction);
		    profile.cv_count[direction] = nurbs->CVCount(direction);
		    profile.span_count[direction] = nurbs->SpanCount(direction);
		}
	    }
	    ON_NurbsSurface nurb_form;
	    profile.nurb_form_status = surface->GetNurbForm(nurb_form);
	    if (profile.nurb_form_status > 0) {
		profile.nurb_form_available = true;
		profile.nurb_form_rational = nurb_form.IsRational();
		for (int direction = 0; direction < 2; ++direction) {
		    profile.nurb_form_order[direction] =
			nurb_form.Order(direction);
		    profile.nurb_form_cv_count[direction] =
			nurb_form.CVCount(direction);
		    profile.nurb_form_span_count[direction] =
			nurb_form.SpanCount(direction);
		}
	    }
	}
	int64_t start = bu_gettime();
	brlcad::SurfaceTree tree(&face, true, depth_limit);
	profile.seconds = (bu_gettime() - start) / 1000000.0;
	if (tree.Valid()) {
	    surface_tree_node_counts(tree.getRootNode(), 0, profile.nodes,
		profile.leaves, profile.maximum_depth);
	    if (tree.m_ctree) {
		std::list<const brlcad::BRNode *> curve_leaves;
		tree.m_ctree->getLeaves(curve_leaves);
		profile.curve_leaves = curve_leaves.size();
	    }
	} else {
	    result.ret = BRLCAD_ERROR;
	    result.failure_reason = "surface_tree";
	    profile.failure.reason = tree.Failure();
	    profile.failure.u = tree.FailureDomain(0);
	    profile.failure.v = tree.FailureDomain(1);
	    profile.failure.depth = tree.FailureDepth();
	}
	result.faces.push_back(profile);
    }
    result.seconds = (bu_gettime() - total_start) / 1000000.0;
    result.peak_rss_bytes = peak_rss_bytes();
    rt_db_free_internal(&intern);
    return result;
}

static const char *
ray_audit_fallback_name(int fallback)
{
    switch (fallback) {
	case RT_BREP_PREPARED_FALLBACK_NONE: return "none";
	case RT_BREP_PREPARED_FALLBACK_NON_SOLID: return "non_solid";
	case RT_BREP_PREPARED_FALLBACK_PLATE: return "plate";
	case RT_BREP_PREPARED_FALLBACK_UNSUPPORTED: return "unsupported";
	case RT_BREP_PREPARED_FALLBACK_SURFACE_WORKSPACE:
	    return "surface_workspace";
	case RT_BREP_PREPARED_FALLBACK_SURFACE_BOXES:
	    return "surface_boxes";
	case RT_BREP_PREPARED_FALLBACK_UNCERTIFIED: return "uncertified";
	case RT_BREP_PREPARED_FALLBACK_LOCAL_WORKSPACE:
	    return "local_workspace";
	case RT_BREP_PREPARED_FALLBACK_ROOT_COVERAGE:
	    return "root_coverage";
	case RT_BREP_PREPARED_FALLBACK_EVENT_CLASS: return "event_class";
	case RT_BREP_PREPARED_FALLBACK_HIT_BUILD: return "hit_build";
	case RT_BREP_PREPARED_FALLBACK_HIT_WORKSPACE:
	    return "hit_workspace";
	case RT_BREP_PREPARED_FALLBACK_PARTITION: return "partition";
    }
    return "invalid";
}

static void
ray_audit_free_prepared(audit_prepared_brep &prepared)
{
    if (prepared.stp) {
	if (prepared.stp->st_specific && prepared.stp->st_meth &&
		prepared.stp->st_meth->ft_free)
	    prepared.stp->st_meth->ft_free(prepared.stp);
	bu_free(prepared.stp, "brep ray audit soltab");
	prepared.stp = NULL;
    }
    if (prepared.resource_initialized && prepared.rtip) {
	rt_clean_resource_basic(prepared.rtip, &prepared.resource);
	BU_PTBL_SET(&prepared.rtip->rti_resources, 0, NULL);
	prepared.resource_initialized = false;
    }
    if (prepared.rtip) {
	rt_i_destroy(prepared.rtip);
	prepared.rtip = NULL;
    }
}

static bool
ray_audit_prepare(struct db_i *dbip, struct directory *dp, int depth,
    const struct bn_tol *tol, audit_prepared_brep &prepared,
    ray_audit_prep &result)
{
    result.requested_depth = depth;
    struct rt_db_internal intern;
    if (!tol || load_brep(dbip, dp, &intern) != BRLCAD_OK)
	return false;
    prepared.rtip = rt_dirbuild_inmem(NULL, 0, NULL, 0);
    if (!prepared.rtip ||
	    !_rt_brep_set_surface_tree_depth(prepared.rtip, depth)) {
	rt_db_free_internal(&intern);
	ray_audit_free_prepared(prepared);
	return false;
    }
    prepared.rtip->rti_tol = *tol;
    prepared.stp = (struct soltab *)bu_calloc(1, sizeof(struct soltab),
	"brep ray audit soltab");
    prepared.stp->l.magic = RT_SOLTAB_MAGIC;
    prepared.stp->l2.magic = RT_SOLTAB2_MAGIC;
    prepared.stp->st_rtip = prepared.rtip;
    prepared.stp->st_dp = dp;
    prepared.stp->st_id = ID_BREP;
    prepared.stp->st_meth = &OBJ[ID_BREP];

    const int64_t start = bu_gettime();
    result.ret = OBJ[ID_BREP].ft_prep(prepared.stp, &intern,
	prepared.rtip);
    result.wall_seconds = (bu_gettime() - start) / 1000000.0;
    rt_db_free_internal(&intern);
    if (result.ret != BRLCAD_OK ||
	    !_rt_brep_prep_stats(prepared.stp, &result.stats) ||
	    result.stats.surface_tree_depth_limit != depth) {
	result.ret = BRLCAD_ERROR;
	ray_audit_free_prepared(prepared);
	return false;
    }
    rt_init_resource(&prepared.resource, 0, prepared.rtip);
    prepared.resource_initialized = true;
    return true;
}

static double
ray_audit_rand01(std::mt19937_64 &rng)
{
    return std::generate_canonical<double, 53>(rng);
}

static void
ray_audit_point_on_sphere(double radius, const point_t center, point_t point,
    std::mt19937_64 &rng)
{
    const double theta = 2.0 * M_PI * ray_audit_rand01(rng);
    const double z = 2.0 * ray_audit_rand01(rng) - 1.0;
    const double radial = sqrt(std::max(0.0, 1.0 - z * z));
    VSET(point, center[X] + radius * radial * cos(theta),
	center[Y] + radius * radial * sin(theta), center[Z] + radius * z);
}

static void
ray_audit_add_chord(std::vector<ray_audit_sample> &samples,
    const point_t first, const point_t second, size_t pair_index,
    ray_audit_sample_kind kind, int face_index = -1, int edge_index = -1)
{
    vect_t delta;
    VSUB2(delta, second, first);
    const double length = MAGNITUDE(delta);
    if (!(length > SMALL_FASTF) || !std::isfinite(length))
	return;
    VSCALE(delta, delta, 1.0 / length);
    ray_audit_sample forward;
    VMOVE(forward.origin, first);
    VMOVE(forward.direction, delta);
    forward.chord_length = length;
    forward.pair_index = pair_index;
    forward.kind = kind;
    forward.face_index = face_index;
    forward.edge_index = edge_index;
    samples.push_back(forward);
    ray_audit_sample reverse;
    VMOVE(reverse.origin, second);
    VREVERSE(reverse.direction, delta);
    reverse.chord_length = length;
    reverse.pair_index = pair_index;
    reverse.reverse = true;
    reverse.kind = kind;
    reverse.face_index = face_index;
    reverse.edge_index = edge_index;
    samples.push_back(reverse);
}

static std::vector<ray_audit_sample>
ray_audit_samples(const point_t bmin, const point_t bmax,
    size_t random_chords, double minimum_radius)
{
    static const uint64_t seed = UINT64_C(0x9e3779b97f4a7c15);
    point_t center;
    vect_t diagonal;
    VADD2SCALE(center, bmin, bmax, 0.5);
    VSUB2(diagonal, bmax, bmin);
    const double radius = std::max(0.55 * MAGNITUDE(diagonal),
	minimum_radius);
    std::vector<ray_audit_sample> samples;
    samples.reserve(2 * (random_chords + 3));
    size_t pair_index = 0;
    for (int axis = 0; axis < 3; ++axis) {
	point_t first;
	point_t second;
	VMOVE(first, center);
	VMOVE(second, center);
	first[axis] -= radius;
	second[axis] += radius;
	ray_audit_add_chord(samples, first, second, pair_index++,
	    RAY_AUDIT_AXIS_CHORD);
    }
    std::mt19937_64 rng(seed);
    while (pair_index < random_chords + 3) {
	point_t first;
	point_t second;
	ray_audit_point_on_sphere(radius, center, first, rng);
	ray_audit_point_on_sphere(radius, center, second, rng);
	const size_t before = samples.size();
	ray_audit_add_chord(samples, first, second, pair_index,
	    RAY_AUDIT_RANDOM_CHORD);
	if (samples.size() != before)
	    pair_index++;
    }
    return samples;
}

static const char *
ray_audit_sample_kind_name(ray_audit_sample_kind kind)
{
    switch (kind) {
	case RAY_AUDIT_AXIS_CHORD:
	    return "axis";
	case RAY_AUDIT_RANDOM_CHORD:
	    return "random";
	case RAY_AUDIT_FACE_NORMAL_CHORD:
	    return "face_normal";
	case RAY_AUDIT_EDGE_NORMAL_CHORD:
	    return "edge_normal";
	case RAY_AUDIT_EDGE_GRAZE_CHORD:
	    return "edge_graze";
    }
    return "unknown";
}

static bool
ray_audit_add_feature_chord(std::vector<ray_audit_sample> &samples,
    const ray_audit_feature_target &target, const point_t center,
    double radius, size_t pair_index)
{
    vect_t offset;
    VSUB2(offset, target.point, center);
    const double projection = VDOT(offset, target.direction);
    const double discriminant = projection * projection -
	(VDOT(offset, offset) - radius * radius);
    if (!(discriminant > 0.0) || !std::isfinite(discriminant))
	return false;
    const double root = sqrt(discriminant);
    const double lower = -projection - root;
    const double upper = -projection + root;
    point_t first;
    point_t second;
    VJOIN1(first, target.point, lower, target.direction);
    VJOIN1(second, target.point, upper, target.direction);
    const size_t before = samples.size();
    ray_audit_add_chord(samples, first, second, pair_index, target.kind,
	target.face_index, target.edge_index);
    return samples.size() != before;
}

static void
ray_audit_add_feature_target(std::vector<ray_audit_feature_target> &targets,
    const ON_3dPoint &point, const ON_3dVector &direction,
    ray_audit_sample_kind kind, int face_index, int edge_index)
{
    ON_3dVector unit = direction;
    if (!point.IsValid() || !unit.IsValid() || !unit.Unitize())
	return;
    ray_audit_feature_target target;
    VSET(target.point, point.x, point.y, point.z);
    VSET(target.direction, unit.x, unit.y, unit.z);
    target.kind = kind;
    target.face_index = face_index;
    target.edge_index = edge_index;
    targets.push_back(target);
}

static void
ray_audit_select_feature_targets(
    std::vector<ray_audit_feature_target> &selected,
    const std::vector<ray_audit_feature_target> &available, size_t count)
{
    if (!count || available.empty())
	return;
    count = std::min(count, available.size());
    selected.reserve(selected.size() + count);
    for (size_t i = 0; i < count; ++i) {
	const size_t index = std::min(available.size() - 1,
	    ((2 * i + 1) * available.size()) / (2 * count));
	selected.push_back(available[index]);
    }
}

static size_t
ray_audit_add_feature_samples(std::vector<ray_audit_sample> &samples,
    const ON_Brep &brep, const point_t bmin, const point_t bmax,
    size_t requested_count, double minimum_radius)
{
    if (!requested_count)
	return 0;
    std::vector<ray_audit_feature_target> face_targets;
    std::vector<ray_audit_feature_target> edge_normal_targets;
    std::vector<ray_audit_feature_target> edge_graze_targets;

    for (int face_index = 0; face_index < brep.m_F.Count(); ++face_index) {
	const ON_BrepFace &face = brep.m_F[face_index];
	const ON_Surface *surface = face.SurfaceOf();
	if (!surface)
	    continue;
	const ON_Interval u = surface->Domain(0);
	const ON_Interval v = surface->Domain(1);
	if (!u.IsIncreasing() || !v.IsIncreasing())
	    continue;
	ON_3dPoint point;
	ON_3dVector normal;
	if (!surface_EvNormal(surface, u.Mid(), v.Mid(), point, normal))
	    continue;
	if (face.m_bRev)
	    normal.Reverse();
	ray_audit_add_feature_target(face_targets, point, normal,
	    RAY_AUDIT_FACE_NORMAL_CHORD, face_index, -1);
    }

    for (int edge_index = 0; edge_index < brep.m_E.Count(); ++edge_index) {
	const ON_BrepEdge &edge = brep.m_E[edge_index];
	for (int trim_slot = 0; trim_slot < edge.m_ti.Count(); ++trim_slot) {
	    const int trim_index = edge.m_ti[trim_slot];
	    if (trim_index < 0 || trim_index >= brep.m_T.Count())
		continue;
	    const ON_BrepTrim &trim = brep.m_T[trim_index];
	    const ON_Surface *surface = trim.SurfaceOf();
	    const ON_Interval domain = trim.Domain();
	    const int face_index = trim.FaceIndexOf();
	    if (!surface || !domain.IsIncreasing() || face_index < 0 ||
		    face_index >= brep.m_F.Count())
		continue;
	    ON_3dPoint uv;
	    ON_3dVector uv_tangent;
	    ON_3dPoint point;
	    ON_3dVector du;
	    ON_3dVector dv;
	    if (!trim.Ev1Der(domain.Mid(), uv, uv_tangent) ||
		    !surface->Ev1Der(uv.x, uv.y, point, du, dv))
		continue;
	    ON_3dVector tangent = uv_tangent.x * du + uv_tangent.y * dv;
	    ON_3dVector normal = ON_CrossProduct(du, dv);
	    if (!tangent.Unitize() || !normal.Unitize())
		continue;
	    if (brep.m_F[face_index].m_bRev)
		normal.Reverse();
	    const ON_3dVector conormal = ON_CrossProduct(normal, tangent);
	    ray_audit_add_feature_target(edge_normal_targets, point, normal,
		RAY_AUDIT_EDGE_NORMAL_CHORD, face_index, edge_index);
	    ray_audit_add_feature_target(edge_graze_targets, point, conormal,
		RAY_AUDIT_EDGE_GRAZE_CHORD, face_index, edge_index);
	    break;
	}
    }

    const std::vector<ray_audit_feature_target> *categories[] = {
	&face_targets, &edge_normal_targets, &edge_graze_targets
    };
    size_t quotas[3] = {};
    size_t assigned = 0;
    while (assigned < requested_count) {
	bool advanced = false;
	for (size_t category = 0; category < 3 && assigned < requested_count;
		category++) {
	    if (quotas[category] >= categories[category]->size())
		continue;
	    quotas[category]++;
	    assigned++;
	    advanced = true;
	}
	if (!advanced)
	    break;
    }
    std::vector<ray_audit_feature_target> selected;
    for (size_t category = 0; category < 3; ++category)
	ray_audit_select_feature_targets(selected, *categories[category],
	    quotas[category]);

    point_t center;
    vect_t diagonal;
    VADD2SCALE(center, bmin, bmax, 0.5);
    VSUB2(diagonal, bmax, bmin);
    const double radius = std::max(0.55 * MAGNITUDE(diagonal),
	minimum_radius);
    size_t pair_index = samples.size() / 2;
    const size_t before = samples.size();
    for (size_t i = 0; i < selected.size(); ++i)
	if (ray_audit_add_feature_chord(samples, selected[i], center, radius,
		pair_index))
	    pair_index++;
    return (samples.size() - before) / 2;
}

static double
ray_audit_distance_tolerance(const ray_audit_sample &sample,
    const ray_audit_shot *first, const ray_audit_shot *second,
    const struct bn_tol *tol)
{
    double scale = std::max(1.0, sample.chord_length);
    for (int coordinate = 0; coordinate < 3; ++coordinate)
	scale = std::max(scale, fabs(sample.origin[coordinate]));
    const ray_audit_shot *shots[2] = {first, second};
    for (int shot_index = 0; shot_index < 2; ++shot_index) {
	if (!shots[shot_index])
	    continue;
	for (size_t i = 0; i < shots[shot_index]->intervals.size(); ++i) {
	    scale = std::max(scale,
		fabs(shots[shot_index]->intervals[i].in_dist));
	    scale = std::max(scale,
		fabs(shots[shot_index]->intervals[i].out_dist));
	}
    }
    return std::max(tol ? (double)tol->dist : 0.0,
	65536.0 * DBL_EPSILON * scale);
}

static ray_audit_shot
ray_audit_shoot(audit_prepared_brep &prepared,
    const ray_audit_sample &sample, const struct bn_tol *tol,
    struct rt_brep_shot_trace *trace, bool legacy_only)
{
    ray_audit_shot result;
    struct application ap;
    struct seg seghead;
    struct xray ray;
    RT_APPLICATION_INIT(&ap);
    ap.a_rt_i = prepared.rtip;
    ap.a_resource = &prepared.resource;
    ap.a_onehit = 0;
    VMOVE(ray.r_pt, sample.origin);
    VMOVE(ray.r_dir, sample.direction);
    ray.magic = RT_RAY_MAGIC;
    BU_LIST_INIT(&seghead.l);
    result.hit_count = legacy_only ?
	_rt_brep_shot_legacy_trace(prepared.stp, &ray, &ap, &seghead, trace) :
	_rt_brep_shot_trace(prepared.stp, &ray, &ap, &seghead, trace);
    result.prepared_selected = trace->prepared_production_selected == 1;
    result.fallback = trace->prepared_production_fallback;
    result.intersected_leaves = trace->intersected_leaves;
    result.solver_calls = trace->solver_calls;
    result.stored_roots = trace->stored_roots;
    result.root_overflow = trace->root_overflow;
    result.raw_hits = trace->raw_hits;
    result.after_near_miss = trace->after_near_miss;
    result.after_near_hit = trace->after_near_hit;
    result.after_grazing = trace->after_grazing;
    result.after_duplicates = trace->after_duplicates;
    result.after_direction_cleanup = trace->after_direction_cleanup;
    result.final_hits = trace->final_hits;
    result.fixed_after_near_miss = trace->fixed_after_near_miss;
    result.fixed_after_near_hit = trace->fixed_after_near_hit;
    result.fixed_after_grazing = trace->fixed_after_grazing;
    result.fixed_after_duplicates = trace->fixed_after_duplicates;
    result.fixed_after_direction_cleanup =
	trace->fixed_after_direction_cleanup;
    result.fixed_cleanup_mismatches = trace->fixed_cleanup_mismatches;
    result.trim_equivalence_mismatches = trace->trim_equivalence_mismatches;
    result.face_trim_status_mismatches =
	trace->face_trim_status_mismatches;
    result.face_trim_hit_class_mismatches =
	trace->face_trim_hit_class_mismatches;
    result.face_trim_adjacency_mismatches =
	trace->face_trim_adjacency_mismatches;
    result.face_trim_equivalence_mismatches =
	trace->face_trim_equivalence_mismatches;
    result.roots.assign(trace->roots,
	trace->roots + std::min(trace->stored_roots,
	    (size_t)RT_BREP_TRACE_MAX_ROOTS));

    struct seg *segp;
    for (BU_LIST_FOR(segp, seg, &seghead.l)) {
	ray_audit_interval interval;
	interval.in_dist = segp->seg_in.hit_dist;
	interval.out_dist = segp->seg_out.hit_dist;
	interval.in_normal_dot = VDOT(segp->seg_in.hit_normal,
	    sample.direction);
	interval.out_normal_dot = VDOT(segp->seg_out.hit_normal,
	    sample.direction);
	interval.in_face = segp->seg_in.hit_surfno;
	interval.out_face = segp->seg_out.hit_surfno;
	result.intervals.push_back(interval);
    }
    while (BU_LIST_WHILE(segp, seg, &seghead.l)) {
	BU_LIST_DEQUEUE(&segp->l);
	struct resource *resource = &prepared.resource;
	RT_FREE_SEG(segp, resource);
    }
    std::sort(result.intervals.begin(), result.intervals.end(),
	[](const ray_audit_interval &first, const ray_audit_interval &second) {
	    if (first.in_dist < second.in_dist)
		return true;
	    if (second.in_dist < first.in_dist)
		return false;
	    return first.out_dist < second.out_dist;
	});
    const double distance_tolerance = ray_audit_distance_tolerance(sample,
	&result, NULL, tol);
    if (result.hit_count < 0 ||
	    (size_t)result.hit_count != 2 * result.intervals.size() ||
	    trace->final_segments != result.intervals.size())
	result.valid = false;
    for (size_t i = 0; i < result.intervals.size(); ++i) {
	const ray_audit_interval &interval = result.intervals[i];
	if (!std::isfinite(interval.in_dist) ||
		!std::isfinite(interval.out_dist) ||
		!std::isfinite(interval.in_normal_dot) ||
		!std::isfinite(interval.out_normal_dot) ||
		interval.in_normal_dot > 1.0e-8 ||
		interval.out_normal_dot < -1.0e-8 ||
		interval.in_dist > interval.out_dist + distance_tolerance ||
		(i && interval.in_dist <
		    result.intervals[i - 1].out_dist - distance_tolerance))
	    result.valid = false;
    }
    return result;
}

static void
ray_audit_accumulate(ray_audit_telemetry &total,
    const ray_audit_shot &shot, const struct rt_brep_shot_trace &trace)
{
    total.rays++;
    if (shot.intervals.empty())
	total.miss_rays++;
    else
	total.hit_rays++;
    total.invalid_rays += shot.valid ? 0 : 1;
    total.segments += shot.intervals.size();
    for (size_t i = 0; i < shot.intervals.size(); ++i)
	if (shot.intervals[i].in_normal_dot > 1.0e-8 ||
		shot.intervals[i].out_normal_dot < -1.0e-8)
	    total.orientation_anomalies++;
    total.prepared_eligible += trace.prepared_production_eligible;
    total.prepared_selected += trace.prepared_production_selected;
    if (trace.prepared_production_fallback >= 0 &&
	    trace.prepared_production_fallback <
		RT_BREP_PREPARED_FALLBACK_COUNT)
	total.fallback[trace.prepared_production_fallback]++;
    total.solver_calls += trace.solver_calls;
    total.candidate_surface_spans += trace.candidate_surface_spans;
    total.surface_subdivision_boxes += trace.surface_subdivision_boxes;
    total.surface_isolated_boxes += trace.surface_isolated_boxes;
    total.surface_fold_attempts += trace.surface_fold_attempts;
    total.surface_fold_certified += trace.surface_fold_complete;
    total.surface_workspace_exhausted += trace.surface_workspace_exhausted;
    total.surface_box_overflow += trace.surface_box_overflow;
    total.local_root_overflow += trace.local_root_overflow;
    total.physical_event_overflow += trace.physical_event_overflow;
    total.physical_complete += trace.physical_event_complete;
    total.physical_unresolved += trace.physical_event_unresolved;
    total.physical_state_failures += trace.physical_event_state_failures;
    total.seam_certified += trace.physical_event_seam_certified;
    total.singular_certified += trace.physical_event_singular_certified;
    total.regular_pair_certified +=
	trace.physical_event_regular_pair_certified;
    total.regular_stream_certified +=
	trace.physical_event_regular_stream_certified;
    total.edge_certified += trace.physical_event_edge_certified;
    total.vertex_certified += trace.physical_event_vertex_certified;
    if (trace.fixed_leaf_mismatches || trace.fixed_hit_mismatches ||
	    trace.fixed_cleanup_mismatches ||
	    trace.trim_equivalence_mismatches ||
	    trace.face_trim_equivalence_mismatches)
	total.fixed_equivalence_mismatch_rays++;
    if (trace.root_event_mismatches || trace.local_event_stage_mismatches ||
	    trace.local_event_hit_mismatches ||
	    trace.local_event_final_mismatches)
	total.local_legacy_mismatch_rays++;
}

static ray_audit_depth_run
ray_audit_run_depth(struct db_i *dbip, struct directory *dp, int depth,
    const struct bn_tol *tol, const std::vector<ray_audit_sample> &samples,
    bool legacy_only)
{
    ray_audit_depth_run run;
    audit_prepared_brep prepared;
    if (!ray_audit_prepare(dbip, dp, depth, tol, prepared, run.prep))
	return run;
    run.shots.reserve(samples.size());
    struct rt_brep_shot_trace *trace =
	(struct rt_brep_shot_trace *)bu_calloc(1, sizeof(*trace),
	    "brep ray audit trace");
    for (size_t i = 0; i < samples.size(); ++i) {
	ray_audit_shot shot = ray_audit_shoot(prepared, samples[i], tol,
	    trace, legacy_only);
	ray_audit_accumulate(run.telemetry, shot, *trace);
	run.shots.push_back(shot);
    }
    bu_free(trace, "brep ray audit trace");
    ray_audit_free_prepared(prepared);
    return run;
}

static bool
ray_audit_shots_equivalent(const ray_audit_sample &sample,
    const ray_audit_shot &first, const ray_audit_shot &second,
    const struct bn_tol *tol, double &maximum_delta, size_t &face_differences)
{
    if (!first.valid || !second.valid ||
	    first.intervals.size() != second.intervals.size())
	return false;
    const double distance_tolerance = ray_audit_distance_tolerance(sample,
	&first, &second, tol);
    bool equivalent = true;
    for (size_t i = 0; i < first.intervals.size(); ++i) {
	const double in_delta = fabs(first.intervals[i].in_dist -
	    second.intervals[i].in_dist);
	const double out_delta = fabs(first.intervals[i].out_dist -
	    second.intervals[i].out_dist);
	maximum_delta = std::max(maximum_delta, std::max(in_delta, out_delta));
	if (in_delta > distance_tolerance || out_delta > distance_tolerance)
	    equivalent = false;
	if (first.intervals[i].in_face != second.intervals[i].in_face ||
		first.intervals[i].out_face != second.intervals[i].out_face)
	    face_differences++;
    }
    return equivalent;
}

static bool
ray_audit_reversal_equivalent(const ray_audit_sample &forward_sample,
    const ray_audit_shot &forward, const ray_audit_shot &reverse,
    const struct bn_tol *tol)
{
    if (!forward.valid || !reverse.valid ||
	    forward.intervals.size() != reverse.intervals.size())
	return false;
    const double distance_tolerance = ray_audit_distance_tolerance(
	forward_sample, &forward, &reverse, tol);
    for (size_t i = 0; i < forward.intervals.size(); ++i) {
	const ray_audit_interval &first = forward.intervals[i];
	const ray_audit_interval &second =
	    reverse.intervals[reverse.intervals.size() - i - 1];
	if (fabs(second.in_dist -
		(forward_sample.chord_length - first.out_dist)) >
		distance_tolerance ||
		fabs(second.out_dist -
		    (forward_sample.chord_length - first.in_dist)) >
		distance_tolerance)
	    return false;
    }
    return true;
}

static void
ray_audit_store_example(std::vector<size_t> &examples, size_t index)
{
    if (examples.size() < 16)
	examples.push_back(index);
}

static ray_audit_comparison
ray_audit_compare(const std::vector<ray_audit_sample> &samples,
    const ray_audit_depth_run &candidate,
    const ray_audit_depth_run &reference, const struct bn_tol *tol)
{
    ray_audit_comparison result;
    const size_t count = std::min(candidate.shots.size(),
	reference.shots.size());
    for (size_t i = 0; i < count; ++i) {
	result.candidate_invalid_rays += candidate.shots[i].valid ? 0 : 1;
	result.reference_invalid_rays += reference.shots[i].valid ? 0 : 1;
	const size_t face_differences_before = result.face_differences;
	if (!ray_audit_shots_equivalent(samples[i], candidate.shots[i],
		reference.shots[i], tol, result.maximum_distance_delta,
		result.face_differences)) {
	    result.differing_rays++;
	    ray_audit_store_example(result.differing_ray_indices, i);
	}
	if (result.face_differences != face_differences_before)
	    ray_audit_store_example(result.face_difference_ray_indices, i);
	if (candidate.shots[i].prepared_selected !=
		reference.shots[i].prepared_selected ||
		candidate.shots[i].fallback != reference.shots[i].fallback)
	    result.disposition_differences++;
    }
    if (candidate.shots.size() != reference.shots.size() ||
	    count != samples.size())
	result.differing_rays++;
    for (size_t i = 0; i + 1 < count; i += 2) {
	if (!ray_audit_reversal_equivalent(samples[i], candidate.shots[i],
		candidate.shots[i + 1], tol)) {
	    result.candidate_reversal_mismatches++;
	    ray_audit_store_example(result.candidate_reversal_indices,
		samples[i].pair_index);
	}
	if (!ray_audit_reversal_equivalent(samples[i], reference.shots[i],
		reference.shots[i + 1], tol)) {
	    result.reference_reversal_mismatches++;
	    ray_audit_store_example(result.reference_reversal_indices,
		samples[i].pair_index);
	}
    }
    return result;
}

static void
print_ray_audit_prep(const ray_audit_prep &prep)
{
    std::cout << "{\"status\":" << json_quote(
	prep.ret == BRLCAD_OK ? "ok" : "failed")
	<< ",\"requested_adaptive_depth\":" << prep.requested_depth
	<< ",\"recorded_adaptive_depth\":"
	<< prep.stats.surface_tree_depth_limit
	<< ",\"physical_maximum_depth\":"
	<< prep.stats.surface_tree_maximum_depth
	<< ",\"surface_tree_nodes\":" << prep.stats.surface_tree_nodes
	<< ",\"surface_tree_leaves\":" << prep.stats.surface_tree_leaves
	<< ",\"curve_trees\":" << prep.stats.curve_trees
	<< ",\"curve_tree_leaves\":" << prep.stats.curve_tree_leaves
	<< ",\"reported_build_seconds\":"
	<< prep.stats.surface_tree_build_microseconds / 1000000.0
	<< ",\"wall_seconds\":" << prep.wall_seconds << "}";
}

static void
print_ray_audit_telemetry(const ray_audit_telemetry &total)
{
    std::cout << "{\"rays\":" << total.rays
	<< ",\"hit_rays\":" << total.hit_rays
	<< ",\"miss_rays\":" << total.miss_rays
	<< ",\"invalid_rays\":" << total.invalid_rays
	<< ",\"segments\":" << total.segments
	<< ",\"orientation_anomalies\":" << total.orientation_anomalies
	<< ",\"prepared_eligible\":" << total.prepared_eligible
	<< ",\"prepared_selected\":" << total.prepared_selected
	<< ",\"fallbacks\":{";
    for (int fallback = 0; fallback < RT_BREP_PREPARED_FALLBACK_COUNT;
	    ++fallback) {
	if (fallback)
	    std::cout << ",";
	std::cout << json_quote(ray_audit_fallback_name(fallback)) << ":"
	    << total.fallback[fallback];
    }
    std::cout << "},\"solver_calls\":" << total.solver_calls
	<< ",\"candidate_surface_spans\":"
	<< total.candidate_surface_spans
	<< ",\"surface_subdivision_boxes\":"
	<< total.surface_subdivision_boxes
	<< ",\"surface_isolated_boxes\":" << total.surface_isolated_boxes
	<< ",\"surface_fold_attempts\":" << total.surface_fold_attempts
	<< ",\"surface_fold_certified\":" << total.surface_fold_certified
	<< ",\"workspace\":{\"surface_exhausted\":"
	<< total.surface_workspace_exhausted
	<< ",\"surface_box_overflow\":" << total.surface_box_overflow
	<< ",\"local_root_overflow\":" << total.local_root_overflow
	<< ",\"physical_event_overflow\":"
	<< total.physical_event_overflow
	<< "},\"physical_events\":{\"complete\":"
	<< total.physical_complete << ",\"unresolved\":"
	<< total.physical_unresolved << ",\"state_failures\":"
	<< total.physical_state_failures << "},\"certificates\":{\"seam\":"
	<< total.seam_certified << ",\"singular\":"
	<< total.singular_certified << ",\"regular_pair\":"
	<< total.regular_pair_certified << ",\"regular_stream\":"
	<< total.regular_stream_certified << ",\"edge\":"
	<< total.edge_certified << ",\"vertex\":"
	<< total.vertex_certified
	<< "},\"fixed_equivalence_mismatch_rays\":"
	<< total.fixed_equivalence_mismatch_rays
	<< ",\"local_legacy_mismatch_rays\":"
	<< total.local_legacy_mismatch_rays << "}";
}

static void
print_ray_audit_indices(const std::vector<size_t> &indices)
{
    std::cout << "[";
    for (size_t i = 0; i < indices.size(); ++i) {
	if (i)
	    std::cout << ",";
	std::cout << indices[i];
    }
    std::cout << "]";
}

static void
print_ray_audit_shot(const ray_audit_shot &shot,
    const std::vector<int> &face_components)
{
    std::cout << "{\"valid\":" << (shot.valid ? "true" : "false")
	<< ",\"hit_count\":" << shot.hit_count
	<< ",\"prepared_selected\":"
	<< (shot.prepared_selected ? "true" : "false")
	<< ",\"fallback\":" << json_quote(
	    ray_audit_fallback_name(shot.fallback))
	<< ",\"intersected_leaves\":" << shot.intersected_leaves
	<< ",\"solver_calls\":" << shot.solver_calls
	<< ",\"cleanup\":{\"raw\":" << shot.raw_hits
	<< ",\"after_near_miss\":" << shot.after_near_miss
	<< ",\"after_near_hit\":" << shot.after_near_hit
	<< ",\"after_grazing\":" << shot.after_grazing
	<< ",\"after_duplicates\":" << shot.after_duplicates
	<< ",\"after_direction\":" << shot.after_direction_cleanup
	<< ",\"final\":" << shot.final_hits << "}"
	<< ",\"fixed_cleanup\":{\"after_near_miss\":"
	<< shot.fixed_after_near_miss
	<< ",\"after_near_hit\":" << shot.fixed_after_near_hit
	<< ",\"after_grazing\":" << shot.fixed_after_grazing
	<< ",\"after_duplicates\":" << shot.fixed_after_duplicates
	<< ",\"after_direction\":" << shot.fixed_after_direction_cleanup
	<< ",\"mismatches\":" << shot.fixed_cleanup_mismatches << "}"
	<< ",\"trim_equivalence\":{\"leaf_allocating\":"
	<< shot.trim_equivalence_mismatches
	<< ",\"face_status\":" << shot.face_trim_status_mismatches
	<< ",\"face_class\":" << shot.face_trim_hit_class_mismatches
	<< ",\"face_adjacency\":"
	<< shot.face_trim_adjacency_mismatches
	<< ",\"face_total\":" << shot.face_trim_equivalence_mismatches
	<< "}"
	<< ",\"root_overflow\":" << shot.root_overflow
	<< ",\"roots\":[";
    for (size_t i = 0; i < shot.roots.size(); ++i) {
	const struct rt_brep_trace_root &root = shot.roots[i];
	if (i)
	    std::cout << ",";
	std::cout << "{\"dist\":" << root.dist
	    << ",\"face\":" << root.face_index
	    << ",\"component\":" << (root.face_index >= 0 &&
		(size_t)root.face_index < face_components.size() ?
		face_components[root.face_index] : -1)
	    << ",\"uv\":[" << root.uv[0] << "," << root.uv[1] << "]"
	    << ",\"normal_dot\":" << root.normal_dot
	    << ",\"trim_status\":" << root.trim_status
	    << ",\"trim_distance\":" << root.trim_distance
	    << ",\"trim_candidates\":" << root.trim_candidates
	    << ",\"hit_class\":" << root.hit_class
	    << ",\"face_trim_status\":" << root.face_trim_status
	    << ",\"face_trim_distance\":" << root.face_trim_distance
	    << ",\"face_trim_candidates\":" << root.face_trim_candidates
	    << ",\"face_hit_class\":" << root.face_hit_class
	    << ",\"direction\":" << root.direction
	    << ",\"adjacent_face\":" << root.adjacent_face_index << "}";
    }
    std::cout << "],\"stored_roots\":" << shot.stored_roots
	<< ",\"intervals\":[";
    for (size_t i = 0; i < shot.intervals.size(); ++i) {
	if (i)
	    std::cout << ",";
	const ray_audit_interval &interval = shot.intervals[i];
	std::cout << "{\"in\":" << interval.in_dist
	    << ",\"out\":" << interval.out_dist
	    << ",\"in_face\":" << interval.in_face
	    << ",\"out_face\":" << interval.out_face
	    << ",\"in_normal_dot\":" << interval.in_normal_dot
	    << ",\"out_normal_dot\":" << interval.out_normal_dot << "}";
    }
    std::cout << "]}";
}

static void
print_ray_audit_failure_records(
    const std::vector<ray_audit_sample> &samples,
    const ray_audit_depth_run &candidate,
    const ray_audit_depth_run &reference,
    const ray_audit_comparison &comparison,
    const std::vector<int> &face_components)
{
    std::vector<size_t> pairs;
    for (size_t i = 0; i < comparison.differing_ray_indices.size(); ++i) {
	const size_t ray_index = comparison.differing_ray_indices[i];
	if (ray_index < samples.size())
	    pairs.push_back(samples[ray_index].pair_index);
    }
    for (size_t i = 0; i < comparison.face_difference_ray_indices.size();
	    ++i) {
	const size_t ray_index = comparison.face_difference_ray_indices[i];
	if (ray_index < samples.size())
	    pairs.push_back(samples[ray_index].pair_index);
    }
    pairs.insert(pairs.end(), comparison.candidate_reversal_indices.begin(),
	comparison.candidate_reversal_indices.end());
    pairs.insert(pairs.end(), comparison.reference_reversal_indices.begin(),
	comparison.reference_reversal_indices.end());
    std::sort(pairs.begin(), pairs.end());
    pairs.erase(std::unique(pairs.begin(), pairs.end()), pairs.end());
    if (pairs.size() > 16)
	pairs.resize(16);
    std::cout << "[";
    for (size_t record_index = 0; record_index < pairs.size();
	    ++record_index) {
	const size_t pair = pairs[record_index];
	size_t forward = SIZE_MAX;
	size_t reverse = SIZE_MAX;
	for (size_t sample_index = 0; sample_index < samples.size();
		sample_index++) {
	    if (samples[sample_index].pair_index != pair)
		continue;
	    if (samples[sample_index].reverse)
		reverse = sample_index;
	    else
		forward = sample_index;
	}
	if (forward == SIZE_MAX || reverse == SIZE_MAX ||
		forward >= samples.size() ||
		forward >= candidate.shots.size() ||
		reverse >= candidate.shots.size() ||
		forward >= reference.shots.size() ||
		reverse >= reference.shots.size())
	    continue;
	if (record_index)
	    std::cout << ",";
	std::cout << "{\"pair_index\":" << pair
	    << ",\"sample_kind\":"
	    << json_quote(ray_audit_sample_kind_name(samples[forward].kind))
	    << ",\"target_face\":" << samples[forward].face_index
	    << ",\"target_edge\":" << samples[forward].edge_index
	    << ",\"forward_ray_index\":" << forward
	    << ",\"reverse_ray_index\":" << reverse
	    << ",\"origin\":[" << samples[forward].origin[X] << ","
	    << samples[forward].origin[Y] << ","
	    << samples[forward].origin[Z] << "]"
	    << ",\"direction\":[" << samples[forward].direction[X] << ","
	    << samples[forward].direction[Y] << ","
	    << samples[forward].direction[Z] << "]"
	    << ",\"chord_length\":" << samples[forward].chord_length
	    << ",\"candidate_forward\":";
	print_ray_audit_shot(candidate.shots[forward], face_components);
	std::cout << ",\"candidate_reverse\":";
	print_ray_audit_shot(candidate.shots[reverse], face_components);
	std::cout << ",\"reference_forward\":";
	print_ray_audit_shot(reference.shots[forward], face_components);
	std::cout << ",\"reference_reverse\":";
	print_ray_audit_shot(reference.shots[reverse], face_components);
	std::cout << "}";
    }
    std::cout << "]";
}

static std::vector<int>
ray_audit_face_components(const ON_Brep &brep, size_t &component_count)
{
    const size_t face_count = (size_t)brep.m_F.Count();
    std::vector<std::vector<int> > adjacency(face_count);
    for (int edge_index = 0; edge_index < brep.m_E.Count(); ++edge_index) {
	const ON_BrepEdge &edge = brep.m_E[edge_index];
	std::vector<int> faces;
	for (int trim_slot = 0; trim_slot < edge.m_ti.Count(); ++trim_slot) {
	    const int trim_index = edge.m_ti[trim_slot];
	    if (trim_index < 0 || trim_index >= brep.m_T.Count())
		continue;
	    const int face_index = brep.m_T[trim_index].FaceIndexOf();
	    if (face_index >= 0 && (size_t)face_index < face_count &&
		    std::find(faces.begin(), faces.end(), face_index) ==
			faces.end())
		faces.push_back(face_index);
	}
	for (size_t first = 0; first < faces.size(); ++first)
	    for (size_t second = first + 1; second < faces.size(); ++second) {
		adjacency[faces[first]].push_back(faces[second]);
		adjacency[faces[second]].push_back(faces[first]);
	    }
    }
    std::vector<int> components(face_count, -1);
    component_count = 0;
    for (size_t seed = 0; seed < face_count; ++seed) {
	if (components[seed] >= 0)
	    continue;
	std::vector<size_t> pending(1, seed);
	components[seed] = (int)component_count;
	for (size_t next = 0; next < pending.size(); ++next) {
	    const size_t face = pending[next];
	    for (size_t i = 0; i < adjacency[face].size(); ++i) {
		const size_t neighbor = (size_t)adjacency[face][i];
		if (components[neighbor] >= 0)
		    continue;
		components[neighbor] = (int)component_count;
		pending.push_back(neighbor);
	    }
	}
	component_count++;
    }
    return components;
}

static int
ray_depth_compare(struct db_i *dbip, const char *db_path,
    struct directory *dp, int candidate_depth, int reference_depth,
    size_t random_chords, size_t feature_chords, long selected_pair,
    bool legacy_only)
{
    struct rt_db_internal intern;
    bool solid = false;
    int face_count = 0;
    point_t bmin = VINIT_ZERO;
    point_t bmax = VINIT_ZERO;
    bool have_bbox = false;
    size_t component_count = 0;
    std::vector<int> face_components;
    struct bn_tol tol = BN_TOL_INIT_TOL;
    std::vector<ray_audit_sample> samples;
    const size_t generated_random_chords = selected_pair >= 3 &&
	!feature_chords ? std::max(random_chords,
	    (size_t)selected_pair - 2) : random_chords;
    if (load_brep(dbip, dp, &intern) == BRLCAD_OK) {
	struct rt_brep_internal *bi =
	    (struct rt_brep_internal *)intern.idb_ptr;
	RT_BREP_CK_MAGIC(bi);
	face_count = bi->brep->m_F.Count();
	solid = bi->brep->IsSolid();
	face_components = ray_audit_face_components(*bi->brep,
	    component_count);
	const ON_BoundingBox bbox = bi->brep->BoundingBox();
	if (bbox.IsValid()) {
	    VMOVE(bmin, bbox.m_min);
	    VMOVE(bmax, bbox.m_max);
	    have_bbox = true;
	}
	if (have_bbox && solid) {
	    samples = ray_audit_samples(bmin, bmax,
		generated_random_chords,
		std::max(100.0 * tol.dist, 1.0e-6));
	    (void)ray_audit_add_feature_samples(samples, *bi->brep, bmin,
		bmax, feature_chords,
		std::max(100.0 * tol.dist, 1.0e-6));
	}
	rt_db_free_internal(&intern);
    }
    if (selected_pair >= 0)
	samples.erase(std::remove_if(samples.begin(), samples.end(),
	    [selected_pair](const ray_audit_sample &sample) {
		return sample.pair_index != (size_t)selected_pair;
	    }), samples.end());
    size_t sample_kind_pairs[5] = {};
    for (size_t i = 0; i < samples.size(); ++i)
	if (!samples[i].reverse && samples[i].kind >= RAY_AUDIT_AXIS_CHORD &&
		samples[i].kind <= RAY_AUDIT_EDGE_GRAZE_CHORD)
	    sample_kind_pairs[samples[i].kind]++;
    ray_audit_depth_run reference;
    ray_audit_depth_run candidate;
    if (!samples.empty()) {
	/* Direct primitive prep is deliberate: the persistent prep-cache key
	 * does not currently encode an explicitly requested adaptive depth. */
	reference = ray_audit_run_depth(dbip, dp, reference_depth, &tol,
	    samples, legacy_only);
	if (reference.prep.ret == BRLCAD_OK)
	    candidate = ray_audit_run_depth(dbip, dp, candidate_depth, &tol,
		samples, legacy_only);
    }
    const ray_audit_comparison comparison =
	(candidate.prep.ret == BRLCAD_OK && reference.prep.ret == BRLCAD_OK) ?
	ray_audit_compare(samples, candidate, reference, &tol) :
	ray_audit_comparison();
    const bool pass = solid && have_bbox && face_count > 0 &&
	!samples.empty() && candidate.prep.ret == BRLCAD_OK &&
	reference.prep.ret == BRLCAD_OK && !comparison.differing_rays &&
	!comparison.candidate_invalid_rays &&
	!comparison.reference_invalid_rays &&
	!comparison.candidate_reversal_mismatches &&
	!comparison.reference_reversal_mismatches;
    const char *status = pass ? "pass" : !face_count ? "empty_brep" :
	!solid ? "non_solid" : !have_bbox ? "invalid_bbox" :
	"comparison_failed";

    std::cout << std::setprecision(17)
	<< "{\"format\":\"brlcad-brep-ray-depth-comparison-v1\""
	<< ",\"database\":" << json_quote(db_path)
	<< ",\"object\":" << json_quote(dp->d_namep)
	<< ",\"status\":" << json_quote(status)
	<< ",\"solid\":" << (solid ? "true" : "false")
	<< ",\"faces\":" << face_count
	<< ",\"face_components\":" << component_count
	<< ",\"tolerance\":{\"distance\":" << tol.dist
	<< ",\"distance_squared\":" << tol.dist_sq
	<< ",\"perpendicular\":" << tol.perp
	<< ",\"parallel\":" << tol.para << "}"
	<< ",\"trace_mode\":" << json_quote(legacy_only ?
	    "legacy_only" : "prepared_and_legacy")
	<< ",\"sampling\":{\"seed\":\"0x9e3779b97f4a7c15\""
	<< ",\"selected_pair_index\":" << selected_pair
	<< ",\"requested_random_chord_pairs\":" << random_chords
	<< ",\"requested_feature_chord_pairs\":" << feature_chords
	<< ",\"axis_chord_pairs\":"
	<< sample_kind_pairs[RAY_AUDIT_AXIS_CHORD]
	<< ",\"random_chord_pairs\":"
	<< sample_kind_pairs[RAY_AUDIT_RANDOM_CHORD]
	<< ",\"face_normal_chord_pairs\":"
	<< sample_kind_pairs[RAY_AUDIT_FACE_NORMAL_CHORD]
	<< ",\"edge_normal_chord_pairs\":"
	<< sample_kind_pairs[RAY_AUDIT_EDGE_NORMAL_CHORD]
	<< ",\"edge_graze_chord_pairs\":"
	<< sample_kind_pairs[RAY_AUDIT_EDGE_GRAZE_CHORD]
	<< ",\"feature_chord_pairs\":"
	<< (sample_kind_pairs[RAY_AUDIT_FACE_NORMAL_CHORD] +
	    sample_kind_pairs[RAY_AUDIT_EDGE_NORMAL_CHORD] +
	    sample_kind_pairs[RAY_AUDIT_EDGE_GRAZE_CHORD])
	<< ",\"total_chord_pairs\":"
	<< samples.size() / 2 << ",\"rays\":" << samples.size() << "}"
	<< ",\"reference\":{\"prep\":";
    print_ray_audit_prep(reference.prep);
    std::cout << ",\"trace\":";
    print_ray_audit_telemetry(reference.telemetry);
    std::cout << "},\"candidate\":{\"prep\":";
    print_ray_audit_prep(candidate.prep);
    std::cout << ",\"trace\":";
    print_ray_audit_telemetry(candidate.telemetry);
    std::cout << "},\"comparison\":{\"differing_rays\":"
	<< comparison.differing_rays
	<< ",\"candidate_invalid_rays\":"
	<< comparison.candidate_invalid_rays
	<< ",\"reference_invalid_rays\":"
	<< comparison.reference_invalid_rays
	<< ",\"candidate_reversal_mismatches\":"
	<< comparison.candidate_reversal_mismatches
	<< ",\"reference_reversal_mismatches\":"
	<< comparison.reference_reversal_mismatches
	<< ",\"disposition_differences\":"
	<< comparison.disposition_differences
	<< ",\"face_differences\":" << comparison.face_differences
	<< ",\"face_difference_ray_indices\":";
    print_ray_audit_indices(comparison.face_difference_ray_indices);
    std::cout
	<< ",\"maximum_distance_delta\":"
	<< comparison.maximum_distance_delta
	<< ",\"differing_ray_indices\":";
    print_ray_audit_indices(comparison.differing_ray_indices);
    std::cout << ",\"candidate_reversal_pair_indices\":";
    print_ray_audit_indices(comparison.candidate_reversal_indices);
    std::cout << ",\"reference_reversal_pair_indices\":";
    print_ray_audit_indices(comparison.reference_reversal_indices);
    std::cout << ",\"failure_records\":";
    print_ray_audit_failure_records(samples, candidate, reference,
	comparison, face_components);
    std::cout << "},\"selected_pair_records\":";
    ray_audit_comparison selected_record;
    if (selected_pair >= 0)
	selected_record.candidate_reversal_indices.push_back(
	    (size_t)selected_pair);
    print_ray_audit_failure_records(samples, candidate, reference,
	selected_record, face_components);
    std::cout << ",\"process_peak_rss_bytes\":" << peak_rss_bytes()
	<< "}\n";
    return pass ? 0 : 1;
}

static geom_result
wireframe_result(struct db_i *dbip, struct directory *dp,
	const struct bg_tess_tol *ttol, const struct bn_tol *tol)
{
    geom_result result;
    struct rt_db_internal intern;
    if (load_brep(dbip, dp, &intern) != BRLCAD_OK) {
	result.issues.push_back("database_internal_load_failed");
	return result;
    }

    struct bu_list vhead;
    BU_LIST_INIT(&vhead);
    int64_t start = bu_gettime();
    result.ret = rt_brep_plot_poly(&vhead, dp, &intern, ttol, tol, NULL);
    result.seconds = (bu_gettime() - start) / 1000000.0;
    result.peak_rss_bytes = peak_rss_bytes();

    struct bv_vlist *vp;
    for (BU_LIST_FOR(vp, bv_vlist, &vhead)) {
	for (size_t i = 0; i < vp->nused; i++) {
	    result.commands++;
	    int cmd = vp->cmd[i];
	    if (!drawable_vlist_cmd(cmd))
		continue;
	    const point_t &p = vp->pt[i];
	    if (!std::isfinite(p[X]) || !std::isfinite(p[Y]) || !std::isfinite(p[Z])) {
		result.finite = false;
		continue;
	    }
	    result.vertices++;
	    bbox_add(result.bmin, result.bmax, &result.have_bbox, p);
	    if (cmd == BV_VLIST_LINE_DRAW || cmd == BV_VLIST_POLY_DRAW ||
		    cmd == BV_VLIST_POLY_END || cmd == BV_VLIST_TRI_DRAW ||
		    cmd == BV_VLIST_TRI_END || cmd == BV_VLIST_POINT_DRAW)
		result.primitives++;
	}
    }

    if (result.ret != BRLCAD_OK)
	result.issues.push_back("generation_failed");
    if (!result.vertices || !result.primitives)
	result.issues.push_back("empty_geometry");
    if (!result.finite)
	result.issues.push_back("non_finite_coordinates");
    if (!result.have_bbox)
	result.issues.push_back("missing_bbox");

    BV_FREE_VLIST(&rt_vlfree, &vhead);
    rt_db_free_internal(&intern);
    return result;
}

static geom_result
shaded_result(struct db_i *dbip, struct directory *dp,
	const struct bg_tess_tol *ttol, const struct bn_tol *tol)
{
    geom_result result;
    struct rt_db_internal intern;
    if (load_brep(dbip, dp, &intern) != BRLCAD_OK) {
	result.issues.push_back("database_internal_load_failed");
	return result;
    }
    struct rt_brep_internal *bi = (struct rt_brep_internal *)intern.idb_ptr;
    RT_BREP_CK_MAGIC(bi);

    int *faces = NULL;
    int face_cnt = 0;
    vect_t *normals = NULL;
    point_t *points = NULL;
    int point_cnt = 0;
    int64_t start = bu_gettime();
    result.ret = brep_cdt_fast(&faces, &face_cnt, &normals, &points,
	    &point_cnt, bi->brep, -1, ttol, tol);
    result.seconds = (bu_gettime() - start) / 1000000.0;
    result.peak_rss_bytes = peak_rss_bytes();

    if (face_cnt > 0)
	result.primitives = (size_t)face_cnt;
    if (point_cnt > 0)
	result.vertices = (size_t)point_cnt;
    for (int i = 0; points && i < point_cnt; i++) {
	if (!std::isfinite(points[i][X]) || !std::isfinite(points[i][Y]) ||
		!std::isfinite(points[i][Z])) {
	    result.finite = false;
	    continue;
	}
	bbox_add(result.bmin, result.bmax, &result.have_bbox, points[i]);
    }
    for (int i = 0; faces && i < face_cnt * 3; i++) {
	if (faces[i] < 0 || faces[i] >= point_cnt)
	    result.invalid_indices++;
    }
    for (int i = 0; normals && i < face_cnt * 3; i++) {
	if (!std::isfinite(normals[i][X]) || !std::isfinite(normals[i][Y]) ||
		!std::isfinite(normals[i][Z]))
	    result.finite = false;
    }

    if (result.ret != BRLCAD_OK)
	result.issues.push_back("generation_failed");
    if (!result.vertices || !result.primitives)
	result.issues.push_back("empty_geometry");
    if (result.invalid_indices)
	result.issues.push_back("invalid_face_indices");
    if (!result.finite)
	result.issues.push_back("non_finite_coordinates_or_normals");
    if (!result.have_bbox)
	result.issues.push_back("missing_bbox");

    bu_free(faces, "brep audit faces");
    bu_free(normals, "brep audit normals");
    bu_free(points, "brep audit points");
    rt_db_free_internal(&intern);
    return result;
}

static void
dims(vect_t d, const point_t bmin, const point_t bmax)
{
    VSUB2(d, bmax, bmin);
    d[X] = fabs(d[X]);
    d[Y] = fabs(d[Y]);
    d[Z] = fabs(d[Z]);
}

static void
check_dimensions(geom_result *result, const vect_t ref_dims, double ref_diag,
	double ratio_min, double ratio_max, const char *prefix)
{
    if (!result->have_bbox)
	return;
    vect_t gdims;
    dims(gdims, result->bmin, result->bmax);
    double active_tol = std::max(BN_TOL_DIST, ref_diag * 1.0e-9);
    for (int axis = 0; axis < 3; axis++) {
	if (ref_dims[axis] <= active_tol)
	    continue;
	double ratio = gdims[axis] / ref_dims[axis];
	if (!std::isfinite(ratio) || ratio < ratio_min || ratio > ratio_max) {
	    std::ostringstream issue;
	    issue << prefix << "_bbox_axis_" << "xyz"[axis] << "_ratio_out_of_range";
	    result->issues.push_back(issue.str());
	}
    }
    double gdiag = MAGNITUDE(gdims);
    if (ref_diag > active_tol) {
	double ratio = gdiag / ref_diag;
	if (!std::isfinite(ratio) || ratio < ratio_min || ratio > ratio_max) {
	    std::ostringstream issue;
	    issue << prefix << "_bbox_diagonal_ratio_out_of_range";
	    result->issues.push_back(issue.str());
	}
    }
}

static void
print_num(double value)
{
    if (std::isfinite(value))
	std::cout << std::setprecision(17) << value;
    else
	std::cout << "null";
}

static void
print_vec(const fastf_t *v)
{
    std::cout << "[";
    print_num(v[X]);
    std::cout << ",";
    print_num(v[Y]);
    std::cout << ",";
    print_num(v[Z]);
    std::cout << "]";
}

static void
print_issues(const std::vector<std::string> &issues)
{
    std::cout << "[";
    for (size_t i = 0; i < issues.size(); i++) {
	if (i)
	    std::cout << ",";
	std::cout << json_quote(issues[i].c_str());
    }
    std::cout << "]";
}

static void
print_result(const geom_result &result, const vect_t ref_dims)
{
    vect_t gdims = VINIT_ZERO;
    if (result.have_bbox)
	dims(gdims, result.bmin, result.bmax);
    std::cout << "{\"return_code\":" << result.ret
	<< ",\"seconds\":";
    print_num(result.seconds);
    std::cout << ",\"vertices\":" << result.vertices
	<< ",\"primitives\":" << result.primitives
	<< ",\"commands\":" << result.commands
	<< ",\"peak_rss_bytes\":" << result.peak_rss_bytes
	<< ",\"invalid_indices\":" << result.invalid_indices
	<< ",\"finite\":" << (result.finite ? "true" : "false")
	<< ",\"bbox_valid\":" << (result.have_bbox ? "true" : "false")
	<< ",\"bbox_min\":";
    if (result.have_bbox) print_vec(result.bmin); else std::cout << "null";
    std::cout << ",\"bbox_max\":";
    if (result.have_bbox) print_vec(result.bmax); else std::cout << "null";
    std::cout << ",\"dimensions\":";
    if (result.have_bbox) print_vec(gdims); else std::cout << "null";
    std::cout << ",\"dimension_ratios\":[";
    for (int axis = 0; axis < 3; axis++) {
	if (axis)
	    std::cout << ",";
	if (result.have_bbox && ref_dims[axis] > BN_TOL_DIST)
	    print_num(gdims[axis] / ref_dims[axis]);
	else
	    std::cout << "null";
    }
    std::cout << "],\"issues\":";
    print_issues(result.issues);
    std::cout << "}";
}

static void
print_interval(const ON_Interval &interval)
{
    std::cout << "[";
    print_num(interval.Min());
    std::cout << ",";
    print_num(interval.Max());
    std::cout << "]";
}

static void
print_prep_result(const char *db_path, const char *object,
	const prep_result &result)
{
    std::cout << "{\"format\":\"brlcad-brep-ray-prep-audit-v1\""
	<< ",\"database\":" << json_quote(db_path)
	<< ",\"object\":" << json_quote(object)
	<< ",\"status\":" << json_quote(result.ret == BRLCAD_OK ?
	    "ok" : "fail")
	<< ",\"return_code\":" << result.ret
	<< ",\"failure_reason\":" << json_quote(result.failure_reason)
	<< ",\"seconds\":";
    print_num(result.seconds);
    std::cout << ",\"peak_rss_bytes\":" << result.peak_rss_bytes
	<< ",\"face_count\":" << result.face_count
	<< ",\"bbox_valid\":" << (result.have_bbox ? "true" : "false")
	<< ",\"bbox_min\":";
    if (result.have_bbox) print_vec(result.bmin); else std::cout << "null";
    std::cout << ",\"bbox_max\":";
    if (result.have_bbox) print_vec(result.bmax); else std::cout << "null";
    std::cout << ",\"surface_tree_failures\":[";
    for (size_t i = 0; i < result.failures.size(); ++i) {
	if (i)
	    std::cout << ",";
	const prep_face_failure &failure = result.failures[i];
	std::cout << "{\"face_index\":" << failure.face_index
	    << ",\"reason\":" << json_quote(
		surface_tree_failure_name(failure.reason))
	    << ",\"depth\":" << failure.depth << ",\"u\":";
	print_interval(failure.u);
	std::cout << ",\"v\":";
	print_interval(failure.v);
	std::cout << "}";
    }
    std::cout << "]}\n";
}

static surface_tree_totals
surface_tree_collect_totals(const surface_tree_result &result)
{
    surface_tree_totals totals;
    totals.objects = 1;
    totals.successful_objects = result.ret == BRLCAD_OK ? 1 : 0;
    totals.failed_objects = result.ret == BRLCAD_OK ? 0 : 1;
    totals.faces = result.faces.size();
    totals.seconds = result.seconds;
    for (size_t i = 0; i < result.faces.size(); ++i) {
	const surface_tree_profile &profile = result.faces[i];
	if (profile.failure.reason != brlcad::SurfaceTree::FAILURE_NONE)
	    totals.failed_faces++;
	totals.loops += (size_t)std::max(0, profile.loop_count);
	totals.trims += (size_t)std::max(0, profile.trim_count);
	totals.nodes += profile.nodes;
	totals.leaves += profile.leaves;
	totals.curve_leaves += profile.curve_leaves;
	totals.native_nurbs_faces += profile.native_nurbs ? 1 : 0;
	totals.rational_faces += profile.rational ||
	    profile.nurb_form_rational ? 1 : 0;
	if (profile.nurb_form_status <= 0)
	    totals.nurb_form_unavailable++;
	else if (profile.nurb_form_status == 1)
	    totals.nurb_form_status_one++;
	else if (profile.nurb_form_status == 2)
	    totals.nurb_form_status_two++;
	else
	    totals.nurb_form_status_other++;
	totals.maximum_depth = std::max(totals.maximum_depth,
	    profile.maximum_depth);
	if (profile.leaves > totals.maximum_leaves) {
	    totals.maximum_leaves = profile.leaves;
	    totals.maximum_leaves_face = profile.failure.face_index;
	}
	if (profile.seconds > totals.maximum_seconds) {
	    totals.maximum_seconds = profile.seconds;
	    totals.maximum_seconds_face = profile.failure.face_index;
	}
	for (int direction = 0; direction < 2; ++direction) {
	    totals.maximum_order[direction] = std::max(
		totals.maximum_order[direction], profile.order[direction]);
	    totals.maximum_cv_count[direction] = std::max(
		totals.maximum_cv_count[direction], profile.cv_count[direction]);
	    totals.maximum_span_count[direction] = std::max(
		totals.maximum_span_count[direction],
		profile.span_count[direction]);
	    totals.maximum_nurb_form_order[direction] = std::max(
		totals.maximum_nurb_form_order[direction],
		profile.nurb_form_order[direction]);
	    totals.maximum_nurb_form_cv_count[direction] = std::max(
		totals.maximum_nurb_form_cv_count[direction],
		profile.nurb_form_cv_count[direction]);
	    totals.maximum_nurb_form_span_count[direction] = std::max(
		totals.maximum_nurb_form_span_count[direction],
		profile.nurb_form_span_count[direction]);
	}
    }
    return totals;
}

static void
surface_tree_add_totals(surface_tree_totals &aggregate,
    const surface_tree_totals &object)
{
    aggregate.objects += object.objects;
    aggregate.successful_objects += object.successful_objects;
    aggregate.failed_objects += object.failed_objects;
    aggregate.faces += object.faces;
    aggregate.failed_faces += object.failed_faces;
    aggregate.loops += object.loops;
    aggregate.trims += object.trims;
    aggregate.nodes += object.nodes;
    aggregate.leaves += object.leaves;
    aggregate.curve_leaves += object.curve_leaves;
    aggregate.native_nurbs_faces += object.native_nurbs_faces;
    aggregate.rational_faces += object.rational_faces;
    aggregate.nurb_form_unavailable += object.nurb_form_unavailable;
    aggregate.nurb_form_status_one += object.nurb_form_status_one;
    aggregate.nurb_form_status_two += object.nurb_form_status_two;
    aggregate.nurb_form_status_other += object.nurb_form_status_other;
    aggregate.maximum_depth = std::max(aggregate.maximum_depth,
	object.maximum_depth);
    aggregate.maximum_leaves = std::max(aggregate.maximum_leaves,
	object.maximum_leaves);
    aggregate.maximum_seconds = std::max(aggregate.maximum_seconds,
	object.maximum_seconds);
    aggregate.seconds += object.seconds;
    for (int direction = 0; direction < 2; ++direction) {
	aggregate.maximum_order[direction] = std::max(
	    aggregate.maximum_order[direction], object.maximum_order[direction]);
	aggregate.maximum_cv_count[direction] = std::max(
	    aggregate.maximum_cv_count[direction],
	    object.maximum_cv_count[direction]);
	aggregate.maximum_span_count[direction] = std::max(
	    aggregate.maximum_span_count[direction],
	    object.maximum_span_count[direction]);
	aggregate.maximum_nurb_form_order[direction] = std::max(
	    aggregate.maximum_nurb_form_order[direction],
	    object.maximum_nurb_form_order[direction]);
	aggregate.maximum_nurb_form_cv_count[direction] = std::max(
	    aggregate.maximum_nurb_form_cv_count[direction],
	    object.maximum_nurb_form_cv_count[direction]);
	aggregate.maximum_nurb_form_span_count[direction] = std::max(
	    aggregate.maximum_nurb_form_span_count[direction],
	    object.maximum_nurb_form_span_count[direction]);
    }
}

static void
print_surface_tree_totals(const surface_tree_totals &totals)
{
    std::cout << "{\"objects\":" << totals.objects
	<< ",\"successful_objects\":" << totals.successful_objects
	<< ",\"failed_objects\":" << totals.failed_objects
	<< ",\"faces\":" << totals.faces
	<< ",\"failed_faces\":" << totals.failed_faces
	<< ",\"loops\":" << totals.loops
	<< ",\"trims\":" << totals.trims
	<< ",\"nodes\":" << totals.nodes
	<< ",\"leaves\":" << totals.leaves
	<< ",\"curve_leaves\":" << totals.curve_leaves
	<< ",\"native_nurbs_faces\":" << totals.native_nurbs_faces
	<< ",\"rational_faces\":" << totals.rational_faces
	<< ",\"nurb_form_status_counts\":{\"unavailable\":"
	<< totals.nurb_form_unavailable << ",\"exact\":"
	<< totals.nurb_form_status_one << ",\"reparameterized\":"
	<< totals.nurb_form_status_two << ",\"other\":"
	<< totals.nurb_form_status_other << "}"
	<< ",\"maximum_depth\":" << totals.maximum_depth
	<< ",\"maximum_leaves\":" << totals.maximum_leaves
	<< ",\"maximum_face_seconds\":";
    print_num(totals.maximum_seconds);
    std::cout << ",\"maximum_order\":[" << totals.maximum_order[0]
	<< "," << totals.maximum_order[1] << "]"
	<< ",\"maximum_cv_count\":[" << totals.maximum_cv_count[0]
	<< "," << totals.maximum_cv_count[1] << "]"
	<< ",\"maximum_span_count\":[" << totals.maximum_span_count[0]
	<< "," << totals.maximum_span_count[1] << "]"
	<< ",\"maximum_nurb_form_order\":["
	<< totals.maximum_nurb_form_order[0] << ","
	<< totals.maximum_nurb_form_order[1] << "]"
	<< ",\"maximum_nurb_form_cv_count\":["
	<< totals.maximum_nurb_form_cv_count[0] << ","
	<< totals.maximum_nurb_form_cv_count[1] << "]"
	<< ",\"maximum_nurb_form_span_count\":["
	<< totals.maximum_nurb_form_span_count[0] << ","
	<< totals.maximum_nurb_form_span_count[1] << "]"
	<< ",\"seconds\":";
    print_num(totals.seconds);
    std::cout << "}";
}

static void
print_surface_tree_summary(const char *object,
    const surface_tree_result &result)
{
    const surface_tree_totals totals = surface_tree_collect_totals(result);
    vect_t dimensions = VINIT_ZERO;
    if (result.have_bbox)
	dims(dimensions, result.bmin, result.bmax);
    std::cout << "{\"object\":" << json_quote(object)
	<< ",\"status\":" << json_quote(result.ret == BRLCAD_OK ?
	    "ok" : "fail")
	<< ",\"return_code\":" << result.ret
	<< ",\"failure_reason\":" << json_quote(result.failure_reason)
	<< ",\"adaptive_depth_limit\":" << result.depth_limit
	<< ",\"process_peak_rss_bytes\":" << result.peak_rss_bytes
	<< ",\"bbox_valid\":" << (result.have_bbox ? "true" : "false")
	<< ",\"bbox_min\":";
    if (result.have_bbox) print_vec(result.bmin); else std::cout << "null";
    std::cout << ",\"bbox_max\":";
    if (result.have_bbox) print_vec(result.bmax); else std::cout << "null";
    std::cout << ",\"dimensions\":";
    if (result.have_bbox) print_vec(dimensions); else std::cout << "null";
    std::cout << ",\"diagonal\":";
    if (result.have_bbox) print_num(MAGNITUDE(dimensions));
    else std::cout << "null";
    std::cout << ",\"maximum_leaves_face\":"
	<< totals.maximum_leaves_face
	<< ",\"maximum_seconds_face\":"
	<< totals.maximum_seconds_face
	<< ",\"face_failures\":[";
    bool first_failure = true;
    for (size_t face_index = 0; face_index < result.faces.size();
	    ++face_index) {
	const prep_face_failure &failure = result.faces[face_index].failure;
	if (failure.reason == brlcad::SurfaceTree::FAILURE_NONE)
	    continue;
	if (!first_failure)
	    std::cout << ",";
	first_failure = false;
	std::cout << "{\"face_index\":" << failure.face_index
	    << ",\"reason\":" << json_quote(
		surface_tree_failure_name(failure.reason))
	    << ",\"depth\":" << failure.depth << ",\"u\":";
	print_interval(failure.u);
	std::cout << ",\"v\":";
	print_interval(failure.v);
	std::cout << "}";
    }
    std::cout << "]";
    std::cout << ",\"totals\":";
    print_surface_tree_totals(totals);
    std::cout << "}";
}

static void
print_surface_tree_result(const char *db_path, const char *object,
	const surface_tree_result &result)
{
    size_t total_nodes = 0;
    size_t total_leaves = 0;
    size_t total_curve_leaves = 0;
    int maximum_depth = 0;
    size_t maximum_leaves = 0;
    int maximum_leaves_face = -1;
    double maximum_seconds = 0.0;
    int maximum_seconds_face = -1;
    size_t nurb_form_unavailable = 0;
    size_t nurb_form_status_one = 0;
    size_t nurb_form_status_two = 0;
    size_t nurb_form_status_other = 0;
    for (size_t i = 0; i < result.faces.size(); ++i) {
	const surface_tree_profile &profile = result.faces[i];
	if (profile.nurb_form_status <= 0)
	    nurb_form_unavailable++;
	else if (profile.nurb_form_status == 1)
	    nurb_form_status_one++;
	else if (profile.nurb_form_status == 2)
	    nurb_form_status_two++;
	else
	    nurb_form_status_other++;
	total_nodes += profile.nodes;
	total_leaves += profile.leaves;
	total_curve_leaves += profile.curve_leaves;
	maximum_depth = std::max(maximum_depth, profile.maximum_depth);
	if (profile.leaves > maximum_leaves) {
	    maximum_leaves = profile.leaves;
	    maximum_leaves_face = profile.failure.face_index;
	}
	if (profile.seconds > maximum_seconds) {
	    maximum_seconds = profile.seconds;
	    maximum_seconds_face = profile.failure.face_index;
	}
    }
    vect_t dimensions = VINIT_ZERO;
    if (result.have_bbox)
	dims(dimensions, result.bmin, result.bmax);
    std::cout << "{\"format\":\"brlcad-brep-surface-tree-audit-v2\""
	<< ",\"database\":" << json_quote(db_path)
	<< ",\"object\":" << json_quote(object)
	<< ",\"status\":" << json_quote(result.ret == BRLCAD_OK ?
	    "ok" : "fail")
	<< ",\"return_code\":" << result.ret
	<< ",\"failure_reason\":" << json_quote(result.failure_reason)
	<< ",\"seconds\":";
    print_num(result.seconds);
    std::cout << ",\"peak_rss_bytes\":" << result.peak_rss_bytes
	<< ",\"face_count\":" << result.face_count
	<< ",\"depth_limit\":" << result.depth_limit
	<< ",\"bbox_valid\":" << (result.have_bbox ? "true" : "false")
	<< ",\"bbox_min\":";
    if (result.have_bbox) print_vec(result.bmin); else std::cout << "null";
    std::cout << ",\"bbox_max\":";
    if (result.have_bbox) print_vec(result.bmax); else std::cout << "null";
    std::cout << ",\"dimensions\":";
    if (result.have_bbox) print_vec(dimensions); else std::cout << "null";
    std::cout << ",\"diagonal\":";
    if (result.have_bbox) print_num(MAGNITUDE(dimensions));
    else std::cout << "null";
    std::cout
	<< ",\"maximum_depth\":" << maximum_depth
	<< ",\"total_nodes\":" << total_nodes
	<< ",\"total_leaves\":" << total_leaves
	<< ",\"total_curve_leaves\":" << total_curve_leaves
	<< ",\"maximum_leaves\":" << maximum_leaves
	<< ",\"maximum_leaves_face\":" << maximum_leaves_face
	<< ",\"nurb_form_status_counts\":{\"unavailable\":"
	<< nurb_form_unavailable << ",\"exact\":" << nurb_form_status_one
	<< ",\"reparameterized\":" << nurb_form_status_two
	<< ",\"other\":" << nurb_form_status_other << "}"
	<< ",\"maximum_face_seconds\":";
    print_num(maximum_seconds);
    std::cout << ",\"maximum_seconds_face\":" << maximum_seconds_face
	<< ",\"faces\":[";
    for (size_t i = 0; i < result.faces.size(); ++i) {
	if (i)
	    std::cout << ",";
	const surface_tree_profile &profile = result.faces[i];
	std::cout << "{\"face_index\":" << profile.failure.face_index
	    << ",\"surface_index\":" << profile.surface_index
	    << ",\"loop_count\":" << profile.loop_count
	    << ",\"trim_count\":" << profile.trim_count
	    << ",\"status\":" << json_quote(
		profile.failure.reason == brlcad::SurfaceTree::FAILURE_NONE ?
		"ok" : "fail")
	    << ",\"seconds\":";
	print_num(profile.seconds);
	std::cout << ",\"nodes\":" << profile.nodes
	    << ",\"leaves\":" << profile.leaves
	    << ",\"curve_leaves\":" << profile.curve_leaves
	    << ",\"maximum_depth\":" << profile.maximum_depth
	    << ",\"surface_type\":" << json_quote(profile.surface_type)
	    << ",\"native_nurbs\":" <<
		(profile.native_nurbs ? "true" : "false")
	    << ",\"rational\":" << (profile.rational ? "true" : "false")
	    << ",\"order\":[" << profile.order[0] << ","
	    << profile.order[1] << "]"
	    << ",\"cv_count\":[" << profile.cv_count[0] << ","
	    << profile.cv_count[1] << "]"
	    << ",\"span_count\":[" << profile.span_count[0] << ","
	    << profile.span_count[1] << "]"
	    << ",\"nurb_form_status\":" << profile.nurb_form_status
	    << ",\"nurb_form_available\":" <<
		(profile.nurb_form_available ? "true" : "false")
	    << ",\"nurb_form_rational\":" <<
		(profile.nurb_form_rational ? "true" : "false")
	    << ",\"nurb_form_order\":[" << profile.nurb_form_order[0]
	    << "," << profile.nurb_form_order[1] << "]"
	    << ",\"nurb_form_cv_count\":[" <<
		profile.nurb_form_cv_count[0] << "," <<
		profile.nurb_form_cv_count[1] << "]"
	    << ",\"nurb_form_span_count\":[" <<
		profile.nurb_form_span_count[0] << "," <<
		profile.nurb_form_span_count[1] << "]"
	    << ",\"domain\":[";
	print_interval(profile.domain[0]);
	std::cout << ",";
	print_interval(profile.domain[1]);
	std::cout << "]";
	if (profile.failure.reason != brlcad::SurfaceTree::FAILURE_NONE) {
	    std::cout << ",\"failure_reason\":" << json_quote(
		surface_tree_failure_name(profile.failure.reason))
		<< ",\"failure_depth\":" << profile.failure.depth
		<< ",\"failure_u\":";
	    print_interval(profile.failure.u);
	    std::cout << ",\"failure_v\":";
	    print_interval(profile.failure.v);
	}
	std::cout << "}";
    }
    std::cout << "]}\n";
}

static int
list_breps(const char *db_path)
{
    struct db_i *dbip = open_db(db_path);
    if (dbip == DBI_NULL)
	return 2;
    std::vector<std::string> names;
    struct directory *dp;
    FOR_ALL_DIRECTORY_START(dp, dbip) {
	if (dp->d_minor_type == DB5_MINORTYPE_BRLCAD_BREP)
	    names.push_back(dp->d_namep);
    } FOR_ALL_DIRECTORY_END;
    std::sort(names.begin(), names.end());
    for (const auto &name : names)
	std::cout << name << "\n";
    db_close(dbip);
    return 0;
}

static int
profile_all_surface_trees(struct db_i *dbip, const char *db_path,
    int depth_limit)
{
    if (dbip == DBI_NULL || !db_path)
	return 2;
    std::vector<std::string> names;
    struct directory *dp;
    FOR_ALL_DIRECTORY_START(dp, dbip) {
	if (dp->d_minor_type == DB5_MINORTYPE_BRLCAD_BREP)
	    names.push_back(dp->d_namep);
    } FOR_ALL_DIRECTORY_END;
    std::sort(names.begin(), names.end());

    surface_tree_totals aggregate;
    size_t failed_objects = 0;
    std::cout << "{\"format\":"
	<< "\"brlcad-brep-surface-tree-database-summary-v1\""
	<< ",\"database\":" << json_quote(db_path)
	<< ",\"database_file_bytes\":" << bu_file_size(db_path)
	<< ",\"adaptive_depth_limit\":" << depth_limit
	<< ",\"objects\":[";
    for (size_t object_index = 0; object_index < names.size();
	    ++object_index) {
	if (object_index)
	    std::cout << ",";
	dp = db_lookup(dbip, names[object_index].c_str(), LOOKUP_QUIET);
	surface_tree_result result;
	if (dp != RT_DIR_NULL)
	    result = surface_tree_profile_result(dbip, dp, depth_limit);
	print_surface_tree_summary(names[object_index].c_str(), result);
	const surface_tree_totals totals = surface_tree_collect_totals(result);
	surface_tree_add_totals(aggregate, totals);
	failed_objects += result.ret == BRLCAD_OK ? 0 : 1;
    }
    std::cout << "],\"totals\":";
    print_surface_tree_totals(aggregate);
    std::cout << ",\"process_peak_rss_bytes\":" << peak_rss_bytes()
	<< "}\n";
    if (names.empty())
	return 1;
    return failed_objects ? 1 : 0;
}

int
main(int argc, const char **argv)
{
    bu_setprogname(argv[0]);
    argc--; argv++;

    int print_help = 0;
    int list_only = 0;
    int prep_only = 0;
    int surface_trees_only = 0;
    int surface_trees_all = 0;
    int ray_depth_compare_only = 0;
    double ratio_min = 0.5;
    double ratio_max = 2.0;
    double tess_abs = 0.0;
    double tess_rel = 0.01;
    double tess_norm = 0.0;
    long memory_limit_mib = 0;
    long surface_tree_depth = BREP_MAX_FT_DEPTH;
    long candidate_depth = 3;
    long reference_depth = RT_BREP_DEFAULT_SURFACE_TREE_DEPTH;
    long ray_count = 64;
    long feature_ray_count = 0;
    long ray_pair = -1;
    int legacy_only = 0;
    const char *trim_query_arg = NULL;
    struct bu_opt_desc d[21];
    BU_OPT(d[0], "h", "help", "", NULL, &print_help, "Print help and exit");
    BU_OPT(d[1], "l", "list", "", NULL, &list_only, "List BRep primitive names");
    BU_OPT(d[2], "", "ratio-min", "#", &bu_opt_fastf_t, &ratio_min, "Minimum acceptable generated/reference dimension ratio");
    BU_OPT(d[3], "", "ratio-max", "#", &bu_opt_fastf_t, &ratio_max, "Maximum acceptable generated/reference dimension ratio");
    BU_OPT(d[4], "", "tess-abs", "#", &bu_opt_fastf_t, &tess_abs, "Absolute shaded tessellation tolerance");
    BU_OPT(d[5], "", "tess-rel", "#", &bu_opt_fastf_t, &tess_rel, "Relative shaded tessellation tolerance");
    BU_OPT(d[6], "", "tess-norm", "#", &bu_opt_fastf_t, &tess_norm, "Normal shaded tessellation tolerance");
    BU_OPT(d[7], "", "memory-limit-mib", "#", &bu_opt_long, &memory_limit_mib, "Process address-space limit in MiB (zero disables)");
    BU_OPT(d[8], "", "prep-only", "", NULL, &prep_only, "Audit raytrace preparation only");
    BU_OPT(d[9], "", "surface-trees-only", "", NULL, &surface_trees_only, "Profile raytrace SurfaceTrees serially");
    BU_OPT(d[10], "", "surface-trees-all", "", NULL, &surface_trees_all, "Summarize SurfaceTrees for every BREP in a database");
    BU_OPT(d[11], "", "surface-tree-depth", "#", &bu_opt_long, &surface_tree_depth, "Adaptive SurfaceTree depth after structural splits");
    BU_OPT(d[12], "", "ray-depth-compare", "", NULL, &ray_depth_compare_only, "Compare uncached BREP partitions at two SurfaceTree depths");
    BU_OPT(d[13], "", "candidate-depth", "#", &bu_opt_long, &candidate_depth, "Candidate adaptive SurfaceTree depth");
    BU_OPT(d[14], "", "reference-depth", "#", &bu_opt_long, &reference_depth, "Reference adaptive SurfaceTree depth");
    BU_OPT(d[15], "", "ray-count", "#", &bu_opt_long, &ray_count, "Deterministic random chord pairs (three axis pairs are added)");
    BU_OPT(d[16], "", "ray-pair", "#", &bu_opt_long, &ray_pair, "Shoot only one deterministic chord pair by global index");
    BU_OPT(d[17], "", "legacy-only", "", NULL, &legacy_only, "Skip prepared solving while tracing the legacy SurfaceTree path");
    BU_OPT(d[18], "", "trim-query", "face,u,v", &bu_opt_str, &trim_query_arg, "Compare production and sampled-polygon trim classification");
    BU_OPT(d[19], "", "feature-ray-count", "#", &bu_opt_long, &feature_ray_count, "Deterministic face-normal, edge-normal, and edge-grazing chord pairs");
    BU_OPT_NULL(d[20]);
    int ac = bu_opt_parse(NULL, argc, argv, d);
    trim_query_spec parsed_trim_query;
    const bool trim_query_only = trim_query_arg != NULL;
    const bool trim_query_valid = !trim_query_only ||
	parse_trim_query(trim_query_arg, parsed_trim_query);
    const char *usage = "Usage: brep-audit [options] "
	"[--list|--prep-only|--surface-trees-only|--surface-trees-all|"
	"--ray-depth-compare|--trim-query face,u,v] "
	"file.g [brep]\n";
    const bool database_only = list_only || surface_trees_all;
    if (print_help || (database_only && ac != 1) ||
	    (!database_only && ac != 2) ||
	    ((list_only ? 1 : 0) + (prep_only ? 1 : 0) +
	     (surface_trees_only ? 1 : 0) +
	     (surface_trees_all ? 1 : 0) +
	     (ray_depth_compare_only ? 1 : 0) +
	     (trim_query_only ? 1 : 0) > 1) || !trim_query_valid ||
	    ratio_min <= 0.0 || ratio_max < ratio_min || tess_abs < 0.0 ||
	    tess_rel < 0.0 || tess_norm < 0.0 || memory_limit_mib < 0 ||
	    surface_tree_depth < 0 ||
	    surface_tree_depth > BREP_MAX_FT_DEPTH ||
	    (!surface_trees_only && !surface_trees_all &&
	     surface_tree_depth != BREP_MAX_FT_DEPTH) ||
	    candidate_depth < 0 || candidate_depth > BREP_MAX_FT_DEPTH ||
	    reference_depth < 0 || reference_depth > BREP_MAX_FT_DEPTH ||
	    ray_count < 0 || ray_count > 1000000 ||
	    feature_ray_count < 0 || feature_ray_count > 1000000 ||
	    ray_pair < -1 || ray_pair > 1000002 ||
	    (!ray_depth_compare_only &&
	     (candidate_depth != 3 ||
	      reference_depth != RT_BREP_DEFAULT_SURFACE_TREE_DEPTH ||
	      ray_count != 64 || feature_ray_count != 0 ||
	      ray_pair != -1 || legacy_only))) {
	std::cerr << usage;
	return print_help ? 0 : 2;
    }
    if (!set_memory_limit(memory_limit_mib)) {
	std::cerr << "Unable to set memory limit to " << memory_limit_mib << " MiB\n";
	return 2;
    }
    if (!bu_file_exists(argv[0], NULL)) {
	std::cerr << "Database does not exist: " << argv[0] << "\n";
	return 2;
    }
    if (list_only)
	return list_breps(argv[0]);

    struct db_i *dbip = open_db(argv[0]);
    if (dbip == DBI_NULL) {
	std::cerr << "Unable to open database: " << argv[0] << "\n";
	return 2;
    }
    if (surface_trees_all) {
	const int ret = profile_all_surface_trees(dbip, argv[0],
	    (int)surface_tree_depth);
	db_close(dbip);
	return ret;
    }
    struct directory *dp = db_lookup(dbip, argv[1], LOOKUP_QUIET);
    if (dp == RT_DIR_NULL || dp->d_minor_type != DB5_MINORTYPE_BRLCAD_BREP) {
	std::cerr << "Not a BRep primitive: " << argv[1] << "\n";
	db_close(dbip);
	return 2;
    }

    if (prep_only) {
	prep_result prep = raytrace_prep_result(dbip, dp);
	print_prep_result(argv[0], argv[1], prep);
	db_close(dbip);
	return prep.ret == BRLCAD_OK ? 0 : 1;
    }

    if (surface_trees_only) {
	surface_tree_result trees = surface_tree_profile_result(dbip, dp,
	    (int)surface_tree_depth);
	print_surface_tree_result(argv[0], argv[1], trees);
	db_close(dbip);
	return trees.ret == BRLCAD_OK ? 0 : 1;
    }

    if (ray_depth_compare_only) {
	const int ret = ray_depth_compare(dbip, argv[0], dp,
	    (int)candidate_depth, (int)reference_depth, (size_t)ray_count,
	    (size_t)feature_ray_count, ray_pair, legacy_only != 0);
	db_close(dbip);
	return ret;
    }

    if (trim_query_only) {
	const int ret = trim_query(dbip, argv[0], dp, parsed_trim_query);
	db_close(dbip);
	return ret;
    }

    struct bn_tol tol = BN_TOL_INIT_TOL;
    struct bg_tess_tol ttol = BG_TESS_TOL_INIT_TOL;
    ttol.abs = tess_abs;
    ttol.rel = tess_rel;
    ttol.norm = tess_norm;
    std::cerr << "brep-audit: phase=reference" << std::endl;
    point_t ref_min = VINIT_ZERO;
    point_t ref_max = VINIT_ZERO;
    bool ref_valid = false;
    int ref_faces = 0;
    int ref_face_failures = 0;
    std::vector<std::string> top_issues;
    struct rt_db_internal intern;
    if (load_brep(dbip, dp, &intern) == BRLCAD_OK) {
	struct rt_brep_internal *bi = (struct rt_brep_internal *)intern.idb_ptr;
	ON_BoundingBox bbox = ON_BoundingBox::EmptyBoundingBox;
	ref_faces = bi->brep->m_F.Count();
	for (int i = 0; i < ref_faces; i++) {
	    ON_BoundingBox face_bbox = ON_BoundingBox::EmptyBoundingBox;
	    if (!face_GetBoundingBox(bi->brep->m_F[i], face_bbox, false) ||
		    !face_bbox.IsValid()) {
		ref_face_failures++;
		continue;
	    }
	    if (bbox.IsValid())
		bbox.Union(face_bbox);
	    else
		bbox = face_bbox;
	}
	if (bbox.IsValid()) {
	    VMOVE(ref_min, bbox.m_min);
	    VMOVE(ref_max, bbox.m_max);
	    ref_valid = true;
	} else {
	    top_issues.push_back("trimmed_bbox_failed");
	}
	if (ref_face_failures)
	    top_issues.push_back("trimmed_bbox_face_failures");
	rt_db_free_internal(&intern);
    } else {
	top_issues.push_back("database_internal_load_failed");
    }

    std::cerr << "brep-audit: phase=wireframe" << std::endl;
    geom_result wire = wireframe_result(dbip, dp, &ttol, &tol);
    std::cerr << "brep-audit: phase=shaded" << std::endl;
    geom_result shaded = shaded_result(dbip, dp, &ttol, &tol);
    vect_t ref_dims = VINIT_ZERO;
    double ref_diag = 0.0;
    if (ref_valid) {
	dims(ref_dims, ref_min, ref_max);
	ref_diag = MAGNITUDE(ref_dims);
	check_dimensions(&wire, ref_dims, ref_diag, ratio_min, ratio_max, "wireframe");
	check_dimensions(&shaded, ref_dims, ref_diag, ratio_min, ratio_max, "shaded");
    }
    bool okay = ref_valid && top_issues.empty() && wire.issues.empty() && shaded.issues.empty();

    std::cerr << "brep-audit: phase=report" << std::endl;
    std::cout << "{\"format\":\"brlcad-brep-realization-audit-v1\",\"database\":"
	<< json_quote(argv[0]) << ",\"object\":" << json_quote(argv[1])
	<< ",\"status\":" << json_quote(okay ? "ok" : "fail")
	<< ",\"ratio_limits\":[" << std::setprecision(17) << ratio_min << "," << ratio_max << "]"
	<< ",\"tessellation_tolerance\":{\"abs\":" << tess_abs
	<< ",\"rel\":" << tess_rel << ",\"norm\":" << tess_norm << "}"
	<< ",\"memory_limit_mib\":" << memory_limit_mib
	<< ",\"generators\":{\"wireframe\":\"rt_brep_plot_poly\""
	<< ",\"shaded\":\"brep_cdt_fast\"}"
	<< ",\"reference\":{\"method\":\"face_GetBoundingBox_trim_parameter_envelopes\""
	<< ",\"face_count\":" << ref_faces
	<< ",\"failed_faces\":" << ref_face_failures
	<< ",\"bbox_valid\":" << (ref_valid ? "true" : "false")
	<< ",\"bbox_min\":";
    if (ref_valid) print_vec(ref_min); else std::cout << "null";
    std::cout << ",\"bbox_max\":";
    if (ref_valid) print_vec(ref_max); else std::cout << "null";
    std::cout << ",\"dimensions\":";
    if (ref_valid) print_vec(ref_dims); else std::cout << "null";
    std::cout << ",\"diagonal\":";
    if (ref_valid) print_num(ref_diag); else std::cout << "null";
    std::cout << "},\"wireframe\":";
    print_result(wire, ref_dims);
    std::cout << ",\"shaded\":";
    print_result(shaded, ref_dims);
    std::cout << ",\"issues\":";
    print_issues(top_issues);
    std::cout << "}\n";

    db_close(dbip);
    return okay ? 0 : 1;
}
