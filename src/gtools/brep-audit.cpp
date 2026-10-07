/*                     B R E P - A U D I T . C P P
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file brep-audit.cpp
 *
 * Isolated checks for BRep wireframes, display meshes, and certified meshes.
 */

#include "common.h"

#include <algorithm>
#include <cerrno>
#include <cfloat>
#include <cmath>
#include <cstring>
#include <fstream>
#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <new>
#include <queue>
#include <set>
#include <random>
#include <sstream>
#include <string>
#include <vector>

#ifdef __GLIBC__
#  include <malloc.h>
#endif

#ifdef _WIN32
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  include <windows.h>
#  include <psapi.h>
#  include "bio.h"
#else
#  include <sys/resource.h>
#endif

#include "bu/app.h"
#include "bu/env.h"
#include "bu/file.h"
#include "bu/opt.h"
#include "bu/datetime.h"
#include "bg/trimesh.h"
#include "bn/poly.h"
#include "bv/vlist.h"
#include "brep/cdt.h"
#include "brep/surfacetree.h"
#include "brep/util.h"
#include "raytrace.h"
#include "rt/geom.h"
#include "rt/primitives/brep.h"
#include "rt/vlist.h"

struct face_failure {
    int face_index = -1;
    int result = 0;
    int stage = 0;
    std::string message;
};

struct image_segment {
    point_t a;
    point_t b;
};

struct image_point {
    double p[3];
};

struct image_mesh {
    std::vector<image_point> points;
    std::vector<int> faces;
};
#include "../librt/librt_private.h"

struct geom_result {
    int ret = BRLCAD_ERROR;
    size_t vertices = 0;
    size_t primitives = 0;
    size_t commands = 0;
    size_t invalid_indices = 0;
    int diagnostic_result = 0;
    int diagnostic_stage = 0;
    int diagnostic_face = -1;
    std::string diagnostic_message;
    bool solid_checked = false;
    bool solid = false;
    bool manifold_checked = false;
    bool manifold_accepted = false;
    int degenerate_faces = 0;
    int unmatched_edges = 0;
    int excess_edges = 0;
    int misoriented_edges = 0;
    size_t unique_edges = 0;
    int connected_components = 0;
    int invalid_vertex_links = 0;
    int geometric_degenerate_faces = 0;
    bool repair_attempted = false;
    bool repair_succeeded = false;
    int repair_source_result = 0;
    int repair_source_stage = 0;
    int repair_removed_faces = 0;
    int repair_added_faces = 0;
    int repair_separated_vertices = 0;
    bool repair_component_union = false;
    bool repair_manifold_normalization = false;
    bool repair_self_intersections_allowed = false;
    bool repair_manifold_accepted = false;
    int repair_rejected_hole_faces = 0;
    int repair_geometric_degenerate_faces = 0;
    int repair_unmatched_edges = 0;
    int repair_excess_edges = 0;
    int repair_misoriented_edges = 0;
    int repair_invalid_vertex_links = 0;
    int repair_reoriented_faces = 0;
    int repair_changed_faces = 0;
    size_t repair_deviation_samples = 0;
    size_t repair_projection_failures = 0;
    size_t repair_untrimmed_samples = 0;
    size_t repair_input_mesh_samples = 0;
    unsigned int repair_resource_limits = 0;
    int repair_fast_attempted_faces = 0;
    int repair_fast_used_faces = 0;
    int repair_fast_failed_faces = 0;
    int repair_fast_triangles = 0;
    size_t repair_fast_constrained_edges = 0;
    size_t repair_fast_constrained_samples = 0;
    int repair_added_patch_components = 0;
    int repair_largest_added_patch_faces = 0;
    double repair_largest_added_patch_area = 0.0;
    bool repair_rigorous_first_attempted = false;
    int repair_rigorous_first_result = 0;
    int repair_rigorous_first_fast_faces = 0;
    size_t repair_rigorous_first_constrained_edges = 0;
    size_t repair_rigorous_first_constrained_samples = 0;
    double repair_rigorous_first_reference_area = 0.0;
    double repair_rigorous_first_output_area = 0.0;
    double repair_rigorous_first_area_change_percent = 0.0;
    int repair_approximation_tier = BREP_CDT_REPAIR_APPROX_NONE;
    bool repair_pullback_retry_attempted = false;
    bool repair_pullback_retry_applied = false;
    int repair_retained_rigorous_triangles = 0;
    int repair_best_effort_faces = 0;
    int repair_best_effort_triangles = 0;
    int repair_best_effort_folded_triangles = 0;
    size_t repair_best_effort_reference_samples = 0;
    size_t repair_best_effort_reference_failures = 0;
    double repair_best_effort_max_deviation = 0.0;
    int repair_boundary_strip_faces = 0;
    int repair_boundary_strip_triangles = 0;
    size_t repair_boundary_strip_constrained_edges = 0;
    size_t repair_boundary_strip_constrained_samples = 0;
    int repair_topological_disk_faces = 0;
    int repair_topological_disk_triangles = 0;
    size_t repair_topological_disk_constrained_edges = 0;
    size_t repair_topological_disk_constrained_samples = 0;
    int repair_missing_rigorous_triangles = 0;
    int repair_subdivided_rigorous_triangles = 0;
    int repair_replaced_rigorous_components = 0;
    int repair_largest_replaced_rigorous_triangles = 0;
    size_t repair_largest_replaced_boundary_edges = 0;
    double repair_replaced_rigorous_area = 0.0;
    double repair_largest_replaced_rigorous_area = 0.0;
    std::vector<int> repair_approximation_faces;
    std::vector<int> repair_approximation_edges;
    bool repair_full_fast_used = false;
    bool repair_relaxed_tessellation_attempted = false;
    int repair_relaxed_tessellation_completed_faces = 0;
    bool repair_bounded_edge_retry_attempted = false;
    int repair_bounded_edge_retry_completed_faces = 0;
    bool repair_adaptive_hole_retry_attempted = false;
    size_t repair_adaptive_hole_edges = 0;
    bool repair_adaptive_hole_area_retry_attempted = false;
    double repair_adaptive_hole_area_percent = 0.0;
    struct brep_cdt_healing_report repair_healing = BREP_CDT_HEALING_REPORT_INIT;
    int repair_bounded_edge_approximation_edges = 0;
    int repair_bounded_edge_approximation_faces = 0;
    double repair_max_bounded_edge_deviation = 0.0;
    double repair_max_deviation = 0.0;
    double repair_rms_deviation = 0.0;
    double repair_allowed_deviation = 0.0;
    double repair_reference_area = 0.0;
    double repair_reference_area_change_percent = 0.0;
    double repair_area_change_percent = 0.0;
    double repair_output_area = 0.0;
    double repair_output_volume = 0.0;
    size_t repair_coverage_samples = 0;
    size_t repair_coverage_failures = 0;
    double repair_max_coverage_deviation = 0.0;
    double repair_rms_coverage_deviation = 0.0;
    bool repair_relaxed_fidelity_applied = false;
    double repair_relaxed_fidelity_factor = 0.0;
    double repair_relaxed_deviation_limit = 0.0;
    double repair_relaxed_area_limit = 0.0;
    long long euler_characteristic = 0;
    double minimum_angle_degrees =
	std::numeric_limits<double>::quiet_NaN();
    double maximum_aspect_ratio =
	std::numeric_limits<double>::quiet_NaN();
    int requested_items = 0;
    int completed_items = 0;
    int approximated_items = 0;
    int failed_items = 0;
    int skipped_items = 0;
    int tolerance_skipped_items = 0;
    bool hit_time_limit = false;
    bool hit_memory_limit = false;
    bool hit_point_limit = false;
    bool refinement_time_limited = false;
    size_t triangle_budget = 0;
    int area_converged_faces = 0;
    int triangle_budget_limited_faces = 0;
    int boundary_envelope_incomplete_faces = 0;
    int refinement_passes = 0;
    int memory_approximated_surface_cues = 0;
    int time_approximated_surface_cues = 0;
    bool finite = true;
    bool have_bbox = false;
    point_t bmin = VINIT_ZERO;
    point_t bmax = VINIT_ZERO;
    double seconds = 0.0;
    size_t peak_rss_bytes = 0;
    size_t peak_working_bytes = 0;
    std::vector<int> failed_faces;
    std::vector<face_failure> face_failures;
    std::vector<int> skipped_faces;
    std::vector<int> tolerance_skipped_faces;
    std::vector<int> approximated_faces;
    std::vector<int> unprocessed_faces;
    std::vector<int> failed_edges;
    std::vector<int> unprocessed_edges;
    std::vector<int> failed_surface_cues;
    std::vector<int> approximated_surface_cues;
    std::vector<int> unprocessed_surface_cues;
    size_t omitted_failed_faces = 0;
    size_t omitted_skipped_faces = 0;
    size_t omitted_tolerance_skipped_faces = 0;
    size_t omitted_approximated_faces = 0;
    size_t omitted_unprocessed_faces = 0;
    size_t omitted_failed_edges = 0;
    size_t omitted_unprocessed_edges = 0;
    size_t omitted_failed_surface_cues = 0;
    size_t omitted_approximated_surface_cues = 0;
    size_t omitted_unprocessed_surface_cues = 0;
    std::vector<std::string> issues;
};

static void
quality_repair_provenance(int approximation_tier, const int *face_indices,
	size_t face_count, const int *edge_indices, size_t edge_count, void *data)
{
    geom_result *result = (geom_result *)data;
    if (!result)
	return;
    result->repair_approximation_tier = approximation_tier;
    if (face_indices && face_count)
	result->repair_approximation_faces.assign(face_indices,
	    face_indices + face_count);
    if (edge_indices && edge_count)
	result->repair_approximation_edges.assign(edge_indices,
	    edge_indices + edge_count);
}

static const size_t MAX_REPORTED_ITEM_INDICES = 16384;

static void
capture_index(std::vector<int> *indices, size_t *omitted, int index)
{
    if (indices->size() < MAX_REPORTED_ITEM_INDICES)
	indices->push_back(index);
    else
	(*omitted)++;
}

static void
wire_item_status(int item_type, int item_index, int status, void *data)
{
    geom_result *result = (geom_result *)data;
    if (status == RT_BREP_DRAW_ITEM_COMPLETED)
	return;
    if (item_type == RT_BREP_DRAW_EDGE) {
	if (status == RT_BREP_DRAW_ITEM_FAILED)
	    capture_index(&result->failed_edges,
		&result->omitted_failed_edges, item_index);
	else
	    capture_index(&result->unprocessed_edges,
		&result->omitted_unprocessed_edges, item_index);
	return;
    }
    if (status == RT_BREP_DRAW_ITEM_APPROXIMATED) {
	capture_index(&result->approximated_surface_cues,
	    &result->omitted_approximated_surface_cues, item_index);
	return;
    }
    if (status == RT_BREP_DRAW_ITEM_FAILED)
	capture_index(&result->failed_surface_cues,
	    &result->omitted_failed_surface_cues, item_index);
    else
	capture_index(&result->unprocessed_surface_cues,
	    &result->omitted_unprocessed_surface_cues, item_index);
}

static void
shaded_face_status(int face_index, int status, void *data)
{
    geom_result *result = (geom_result *)data;
    switch (status) {
	case BREP_CDT_FAST_FACE_FAILED:
	    capture_index(&result->failed_faces,
		&result->omitted_failed_faces, face_index);
	    break;
	case BREP_CDT_FAST_FACE_SKIPPED_DEGENERATE:
	    capture_index(&result->skipped_faces,
		&result->omitted_skipped_faces, face_index);
	    break;
	case BREP_CDT_FAST_FACE_SKIPPED_TOLERANCE:
	    capture_index(&result->tolerance_skipped_faces,
		&result->omitted_tolerance_skipped_faces, face_index);
	    break;
	case BREP_CDT_FAST_FACE_APPROXIMATED:
	    capture_index(&result->approximated_faces,
		&result->omitted_approximated_faces, face_index);
	    break;
	case BREP_CDT_FAST_FACE_NOT_PROCESSED:
	    capture_index(&result->unprocessed_faces,
		&result->omitted_unprocessed_faces, face_index);
	    break;
	default:
	    break;
    }
}


static void
shaded_face_diagnostic(int face_index, int diagnostic_result,
	int diagnostic_stage, const char *message, void *data)
{
    geom_result *result = (geom_result *)data;
    face_failure detail;
    detail.face_index = face_index;
    detail.result = diagnostic_result;
    detail.stage = diagnostic_stage;
    detail.message = message ? message : "";
    result->face_failures.push_back(detail);
}


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
    int requested_depth = -1;
    const char *failure_reason = "database_internal_load";
    double seconds = 0.0;
    double cache_serialize_seconds = 0.0;
    double cache_compress_seconds = 0.0;
    size_t peak_rss_bytes = 0;
    size_t resident_before_bytes = 0;
    size_t resident_prepared_bytes = 0;
    size_t resident_after_free_bytes = 0;
    size_t cache_payload_bytes = 0;
    size_t cache_payload_compressed_bytes = 0;
    size_t cache_variant = (size_t)-1;
    struct rt_brep_prep_stats stats = {};
    bool stats_valid = false;
    bool cache_lookup_requested = false;
    bool cache_hit = false;
    bool cache_payload_valid = false;
    bool cache_payload_compressed = false;
    bool have_bbox = false;
    point_t bmin = VINIT_ZERO;
    point_t bmax = VINIT_ZERO;
    std::vector<prep_face_failure> failures;
};

enum parameter_mapping_orientation {
    PARAMETER_MAPPING_DECREASING = -1,
    PARAMETER_MAPPING_UNKNOWN = 0,
    PARAMETER_MAPPING_INCREASING = 1,
    PARAMETER_MAPPING_CONSTANT = 2,
    PARAMETER_MAPPING_MIXED = 3
};

struct parameter_mapping_direction_profile {
    size_t attempts = 0;
    size_t successes = 0;
    size_t domain_contained = 0;
    size_t round_trip_successes = 0;
    size_t round_trip_within_tolerance = 0;
    double maximum_round_trip_error[2] = {0.0, 0.0};
    double maximum_normalized_round_trip_error[2] = {0.0, 0.0};
    bool behavior_complete = false;
    bool separable = false;
    int output_input_axis[2] = {-1, -1};
    double maximum_cross_axis_error[2] = {0.0, 0.0};
    bool monotone[2] = {false, false};
    bool strictly_monotone[2] = {false, false};
    parameter_mapping_orientation orientation[2] = {
	PARAMETER_MAPPING_UNKNOWN, PARAMETER_MAPPING_UNKNOWN
    };
};

struct surface_parameter_mapping_profile {
    bool sampled = false;
    size_t grid_size = 0;
    parameter_mapping_direction_profile forward;
    parameter_mapping_direction_profile inverse;
};

struct parameter_mapping_sample {
    bool valid = false;
    double input[2] = {0.0, 0.0};
    double output[2] = {0.0, 0.0};
};

struct polycurve_segment_profile {
    std::string type = "none";
    ON_Interval polycurve_domain;
    ON_Interval curve_domain;
    bool proxy = false;
    std::string proxy_target_type = "none";
    bool proxy_reversed = false;
    ON_Interval proxy_domain;
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
    ON_Interval nurb_form_domain[2];
    bool face_reversed = false;
    bool face_proxy_transposed = false;
    bool surface_proxy = false;
    const char *surface_proxy_target_type = "none";
    bool surface_proxy_transposed = false;
    bool closed[2] = {false, false};
    bool periodic[2] = {false, false};
    bool singular_sides[4] = {false, false, false, false};
    bool revolution = false;
    bool revolution_transposed = false;
    const char *profile_type = "none";
    ON_Interval profile_domain;
    bool profile_proxy = false;
    const char *profile_proxy_target_type = "none";
    bool profile_proxy_reversed = false;
    ON_Interval profile_proxy_domain;
    bool profile_polycurve = false;
    std::vector<polycurve_segment_profile> profile_polycurve_segments;
    surface_parameter_mapping_profile parameter_mapping;
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
    point_t in_point = VINIT_ZERO;
    point_t out_point = VINIT_ZERO;
    vect_t in_normal = VINIT_ZERO;
    vect_t out_normal = VINIT_ZERO;
    fastf_t in_uv[2] = {0.0, 0.0};
    fastf_t out_uv[2] = {0.0, 0.0};
};

struct ray_audit_shot {
    int hit_count = 0;
    bool valid = true;
    bool prepared_selected = false;
    bool seeded_verifier_complete = false;
    int fallback = RT_BREP_PREPARED_FALLBACK_UNSUPPORTED;
    int seeded_route_lane = -1;
    int seeded_verifier_failure_stage = RT_BREP_SEEDED_VERIFY_INPUT;
    unsigned int seeded_route_capability_flags = 0;
    unsigned int seeded_route_risk_flags = 0;
    size_t route_decision_attempts = 0;
    struct rt_brep_route_decision route_decision = {};
    bool route_test_available = false;
    struct rt_brep_route_test_result route_test = {};
    size_t missing_root_counterfactual_attempts = 0;
    struct rt_brep_route_decision missing_root_counterfactual = {};
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
    size_t fixed_leaf_fallback = 0;
    size_t fixed_hit_fallback = 0;
    size_t legacy_cleanup_actions[RT_BREP_LEGACY_CLEANUP_COUNT] = {};
    size_t legacy_cleanup_action_mismatches = 0;
    size_t trim_equivalence_mismatches = 0;
    size_t face_trim_status_mismatches = 0;
    size_t face_trim_hit_class_mismatches = 0;
    size_t face_trim_adjacency_mismatches = 0;
    size_t face_trim_equivalence_mismatches = 0;
    size_t prepared_production_hits = 0;
    bool prepared_require_surface_box_free = false;
    size_t prepared_selective_class_rejections = 0;
    size_t prepared_selective_candidate_surface_spans = 0;
    size_t prepared_selective_subdivision_boxes = 0;
    size_t prepared_selective_surface_boxes = 0;
    size_t physical_event_complete = 0;
    size_t physical_event_unresolved = 0;
    size_t physical_event_state_failures = 0;
    size_t physical_event_material_segments = 0;
    size_t physical_event_subminimum_contacts = 0;
    size_t physical_event_tolerance_ambiguous = 0;
    size_t stored_physical_events = 0;
    std::vector<struct rt_brep_trace_root> roots;
    std::vector<ray_audit_interval> intervals;
};

enum ray_audit_sample_kind {
    RAY_AUDIT_AXIS_CHORD = 0,
    RAY_AUDIT_RANDOM_CHORD,
    RAY_AUDIT_FACE_NORMAL_CHORD,
    RAY_AUDIT_EDGE_NORMAL_CHORD,
    RAY_AUDIT_EDGE_GRAZE_CHORD,
    RAY_AUDIT_EXACT_CHORD,
    RAY_AUDIT_SAMPLE_KIND_COUNT
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

struct ray_audit_exact_ray {
    point_t origin = VINIT_ZERO;
    vect_t direction = VINIT_ZERO;
};

struct mesh_ray_hit {
    double distance = 0.0;
    double barycentric[2] = {0.0, 0.0};
    double normal_dot = 0.0;
    int triangle = -1;
};

struct torus_ray_root {
    double distance = 0.0;
    double imaginary = 0.0;
    double residual = 0.0;
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

struct ray_audit_route_certificate_telemetry {
    size_t associations = 0;
    size_t state[RT_BREP_ROUTE_CERT_STATE_COUNT] = {};
    size_t regular_root_count = 0;
    size_t observed_roots = 0;
    size_t deficit_cells = 0;
    size_t deficit_roots = 0;
    size_t cached_bbox_exclusions = 0;
    size_t hull_exclusions = 0;
    size_t subdivision_attempts = 0;
    size_t subdivision_boxes = 0;
    size_t subdivision_hull_exclusions = 0;
    size_t subdivision_exclusions = 0;
    size_t degree_attempts = 0;
    size_t degree_complete = 0;
    size_t failure_reason[RT_BREP_ROUTE_CERT_FAILURE_COUNT] = {};
    size_t degree_failure_detail[RT_BREP_ROOT_DEGREE_DETAIL_COUNT] = {};
    size_t boundary_pieces = 0;
    size_t boundary_splits = 0;
    size_t boundary_edges = 0;
    size_t expansion_high_water = 0;
    size_t nanoseconds = 0;
    size_t complete_rays = 0;
    size_t root_free_rays = 0;
    size_t root_positive_rays = 0;
    size_t incomplete_rays = 0;
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
    size_t legacy_capacity_fallback[
	RT_BREP_LEGACY_CAPACITY_FALLBACK_COUNT] = {};
    size_t legacy_cleanup_action_rays[RT_BREP_LEGACY_CLEANUP_COUNT] = {};
    size_t legacy_cleanup_action_events[RT_BREP_LEGACY_CLEANUP_COUNT] = {};
    size_t legacy_cleanup_action_mismatch_rays = 0;
    size_t solver_calls = 0;
    size_t manifold_edges = 0;
    size_t prepared_edge_spans = 0;
    size_t candidate_edge_spans = 0;
    size_t candidate_surface_spans = 0;
    size_t surface_subdivision_boxes = 0;
    size_t surface_isolated_boxes = 0;
    size_t surface_fold_attempts = 0;
    size_t surface_fold_certified = 0;
    size_t surface_fold_candidates = 0;
    size_t surface_fold_krawczyk_attempts = 0;
    size_t surface_fold_krawczyk_available = 0;
    size_t surface_fold_krawczyk_certified = 0;
    size_t surface_fold_expansion_attempts = 0;
    size_t surface_fold_expansion_available = 0;
    size_t surface_fold_expansion_certified = 0;
    size_t surface_fold_expansion_contraction_attempts = 0;
    size_t surface_fold_expansion_contracted = 0;
    size_t surface_fold_corridor_attempts = 0;
    size_t surface_fold_corridor_available = 0;
    size_t surface_fold_corridor_unique = 0;
    size_t surface_fold_corridor_graph_attempts = 0;
    size_t surface_fold_corridor_graph_certified = 0;
    size_t surface_fold_corridor_graph_boxes = 0;
    size_t surface_fold_corridor_graph_interval_attempts = 0;
    size_t surface_fold_corridor_graph_interval_signed = 0;
    size_t surface_fold_corridor_graph_interval_excluded = 0;
    size_t surface_fold_corridor_graph_exact_fallbacks = 0;
    size_t surface_fold_corridor_graph_parent_restrictions = 0;
    size_t surface_fold_strip_build_attempts = 0;
    size_t surface_fold_strip_build_avoided = 0;
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
    size_t seeded_verify_attempts = 0;
    size_t seeded_verify_complete = 0;
    size_t seeded_verify_complete_selected = 0;
    size_t seeded_verify_failed_selected = 0;
    size_t seeded_verify_complete_mismatch = 0;
    size_t seeded_verify_legacy_groups = 0;
    size_t seeded_verify_candidate_faces = 0;
    size_t seeded_verify_candidate_spans = 0;
    size_t seeded_verify_hull_excluded_spans = 0;
    size_t seeded_verify_mapped_roots = 0;
    size_t seeded_verify_local_attempts = 0;
    size_t seeded_verify_local_certified = 0;
    size_t seeded_verify_degree_attempts = 0;
    size_t seeded_verify_degree_complete = 0;
    size_t seeded_verify_degree_determinant_failures = 0;
    size_t seeded_verify_degree_boundary_failures = 0;
    size_t seeded_verify_degree_orientation_failures = 0;
    size_t seeded_verify_degree_count_mismatches = 0;
    size_t seeded_verify_degree_sign_mismatches = 0;
    size_t seeded_verify_degree_count_low = 0;
    size_t seeded_verify_degree_count_high = 0;
    size_t seeded_verify_degree_mismatch_local_exact = 0;
    size_t seeded_verify_degree_mismatch_local_inexact = 0;
    size_t seeded_verify_degree_extra_local_roots = 0;
    size_t seeded_verify_degree_extra_local_trimmed = 0;
    size_t seeded_verify_degree_extra_local_untrimmed = 0;
    size_t seeded_verify_degree_surplus_empty = 0;
    size_t seeded_verify_degree_surplus_seeded = 0;
    size_t seeded_verify_degree_surplus_corrector_attempts = 0;
    size_t seeded_verify_degree_surplus_corrector_converged = 0;
    size_t seeded_verify_degree_surplus_local_attempts = 0;
    size_t seeded_verify_degree_surplus_local_certified = 0;
    size_t seeded_verify_degree_surplus_trimmed_certified = 0;
    size_t seeded_verify_degree_surplus_untrimmed = 0;
    size_t seeded_verify_degree_surplus_nanoseconds = 0;
    size_t seeded_verify_degree_root_count = 0;
    size_t seeded_verify_degree_seed_count = 0;
    size_t seeded_verify_degree_boundary_pieces = 0;
    size_t seeded_verify_degree_boundary_splits = 0;
    size_t seeded_verify_degree_boundary_edges = 0;
    size_t seeded_verify_degree_expansion_high_water = 0;
    size_t seeded_verify_degree_nanoseconds = 0;
    size_t seeded_verify_degree_complement_agreements = 0;
    size_t seeded_verify_degree_complement_disagreements = 0;
    size_t seeded_verify_degree_failure_detail[
	RT_BREP_ROOT_DEGREE_DETAIL_COUNT] = {};
    size_t seeded_verify_complement_visited = 0;
    size_t seeded_verify_complement_high_water = 0;
    size_t seeded_verify_partition_events = 0;
    size_t seeded_verify_nanoseconds = 0;
    size_t seeded_verify_stage_nanoseconds[RT_BREP_SEEDED_VERIFY_STAGE_COUNT] = {};
    size_t seeded_verify_failure_stage[RT_BREP_SEEDED_VERIFY_STAGE_COUNT] = {};
    size_t seeded_verify_selected_failure_stage[
	RT_BREP_SEEDED_VERIFY_STAGE_COUNT] = {};
    size_t seeded_route_rays[RT_BREP_ROUTE_LANE_COUNT] = {};
    size_t seeded_route_verifier_complete[RT_BREP_ROUTE_LANE_COUNT] = {};
    size_t seeded_route_prepared_selected[RT_BREP_ROUTE_LANE_COUNT] = {};
    size_t seeded_route_root_mismatch[RT_BREP_ROUTE_LANE_COUNT] = {};
    size_t seeded_route_capability_rays[RT_BREP_ROUTE_CAP_COUNT] = {};
    size_t seeded_route_risk_rays[RT_BREP_ROUTE_RISK_COUNT] = {};
    size_t seeded_route_qualification_rays[RT_BREP_ROUTE_QUAL_COUNT] = {};
    unsigned int seeded_route_capability_mask = 0;
    unsigned int seeded_route_risk_mask = 0;
    size_t route_decision_nanoseconds = 0;
    size_t route_candidate_cells = 0;
    size_t route_hazard_roots = 0;
    size_t route_contact_roots = 0;
    size_t route_legacy_transaction_incomplete = 0;
    size_t route_selective_refinement_attempted = 0;
    size_t route_selective_refinement_complete = 0;
    size_t route_selective_refinement_max_level = 0;
    size_t route_selective_refinement_hits = 0;
    size_t route_selective_refinement_reversal_mismatches = 0;
    size_t route_selective_refinement_nanoseconds = 0;
    size_t route_cell_lookup_failures = 0;
    size_t route_cell_lookup_failure_reasons[
	RT_BREP_ROUTE_LOOKUP_REASON_COUNT] = {};
    size_t route_roots_unmapped_after_leaf_overflow = 0;
    size_t route_empty_correction_cells = 0;
    size_t route_missing_root_candidate_spans = 0;
    size_t route_missing_root_retained_spans = 0;
    size_t route_missing_root_restricted_spans = 0;
    size_t route_missing_root_inconclusive_spans = 0;
    size_t route_missing_root_workspace_overflow = 0;
    size_t route_missing_root_signal_rays = 0;
    size_t route_missing_root_prepared_candidate_faces = 0;
    size_t route_missing_root_prepared_candidate_spans = 0;
    size_t route_missing_root_prepared_retained_spans = 0;
    size_t route_missing_root_prepared_only_spans = 0;
    size_t route_missing_root_prepared_only_retained_spans = 0;
    size_t route_missing_root_prepared_inconclusive_spans = 0;
    size_t route_missing_root_prepared_candidate_failures = 0;
    size_t route_missing_root_prepared_signal_rays = 0;
    ray_audit_route_certificate_telemetry route_missing_root_certificate;
    size_t route_missing_root_prepared_independent_hit_rays = 0;
    size_t route_missing_root_prepared_independent_empty_rays = 0;
    size_t route_missing_root_prepared_unresolved_rays = 0;
    size_t route_missing_root_independent_hit_rays = 0;
    size_t route_missing_root_independent_empty_rays = 0;
    size_t route_missing_root_unresolved_rays = 0;
    size_t route_missing_root_counterfactual_attempts = 0;
    size_t route_missing_root_counterfactual_signal_rays = 0;
    size_t route_missing_root_counterfactual_unsignaled_rays = 0;
    size_t route_missing_root_counterfactual_true_root_controls = 0;
    size_t route_missing_root_counterfactual_true_root_signals = 0;
    size_t route_missing_root_counterfactual_true_root_missed = 0;
    size_t route_missing_root_counterfactual_empty_controls = 0;
    size_t route_missing_root_counterfactual_empty_signals = 0;
    size_t route_missing_root_counterfactual_empty_rejected = 0;
    size_t route_missing_root_counterfactual_unresolved_controls = 0;
    size_t route_missing_root_counterfactual_candidate_spans = 0;
    size_t route_missing_root_counterfactual_retained_spans = 0;
    size_t route_missing_root_counterfactual_restricted_spans = 0;
    size_t route_missing_root_counterfactual_inconclusive_spans = 0;
    size_t route_missing_root_counterfactual_workspace_overflow = 0;
    size_t route_missing_root_counterfactual_nanoseconds = 0;
    size_t route_missing_root_counterfactual_prepared_candidate_faces = 0;
    size_t route_missing_root_counterfactual_prepared_candidate_spans = 0;
    size_t route_missing_root_counterfactual_prepared_retained_spans = 0;
    size_t route_missing_root_counterfactual_prepared_only_spans = 0;
    size_t route_missing_root_counterfactual_prepared_only_retained_spans = 0;
    size_t route_missing_root_counterfactual_prepared_inconclusive_spans = 0;
    size_t route_missing_root_counterfactual_prepared_candidate_failures = 0;
    size_t route_missing_root_counterfactual_prepared_signal_rays = 0;
    size_t route_missing_root_counterfactual_prepared_unsignaled_rays = 0;
    size_t route_missing_root_counterfactual_true_root_prepared_signals = 0;
    size_t route_missing_root_counterfactual_true_root_prepared_missed = 0;
    size_t route_missing_root_counterfactual_true_root_prepared_recovered = 0;
    size_t route_missing_root_counterfactual_empty_prepared_signals = 0;
    size_t route_missing_root_counterfactual_empty_prepared_rejected = 0;
    ray_audit_route_certificate_telemetry
	route_missing_root_counterfactual_certificate;
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

struct ray_audit_publication {
    size_t rays = 0;
    size_t invalid_rays = 0;
    size_t optional_attempts = 0;
    size_t optional_complete = 0;
    size_t eligible_publications = 0;
    size_t prepared_matches = 0;
    size_t legacy_fallback_matches = 0;
    size_t published_correction_rays = 0;
    size_t mismatches = 0;
    std::vector<size_t> mismatch_ray_indices;
};

struct ray_audit_missing_root_injection {
    size_t rays = 0;
    size_t fault_injected = 0;
    size_t suppressed_hit_rays = 0;
    size_t suppressed_raw_hits = 0;
    size_t suppressed_route_roots = 0;
    size_t direct_empty_results = 0;
    size_t fault_exposed_rays = 0;
    size_t triggered_faults = 0;
    size_t triggered = 0;
    size_t optional_attempts = 0;
    size_t general_prepared_attempts = 0;
    size_t optional_complete = 0;
    size_t optional_incomplete = 0;
    size_t published = 0;
    size_t complete_oracle_agreements = 0;
    size_t recoverable_faults = 0;
    size_t recovered_faults = 0;
    size_t published_prepared_matches = 0;
    size_t published_injected_fallback_matches = 0;
    size_t decision_mismatches = 0;
    size_t transaction_mismatches = 0;
    std::vector<size_t> mismatch_ray_indices;
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
    const std::size_t child_count = node->childCount();
    if (!child_count) {
	leaves++;
	return;
    }
    for (std::size_t child_index = 0; child_index < child_count;
	    ++child_index)
	surface_tree_node_counts(node->child(child_index), depth + 1,
	    nodes, leaves, maximum_depth);
}

static size_t
peak_rss_bytes()
{
    const ssize_t peak = bu_mem(BU_MEM_PROCESS_PEAK_RESIDENT, NULL);
    return peak < 0 ? 0 : (size_t)peak;
}

static size_t
resident_rss_bytes()
{
    size_t result = 0;
    return bu_mem(BU_MEM_PROCESS_RESIDENT, &result) < 0 ? 0 : result;
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
	const unsigned long long bytes =
	    (unsigned long long)memory_limit_mib * 1024ULL * 1024ULL;
	if (bytes > (unsigned long long)std::numeric_limits<SIZE_T>::max())
	    return false;
	static HANDLE memory_job = NULL;
	if (memory_job)
	    return true;
	memory_job = CreateJobObject(NULL, NULL);
	if (!memory_job)
	    return false;
	JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits = {};
	limits.BasicLimitInformation.LimitFlags =
	    JOB_OBJECT_LIMIT_PROCESS_MEMORY;
	limits.ProcessMemoryLimit = (SIZE_T)bytes;
	if (!SetInformationJobObject(memory_job,
		JobObjectExtendedLimitInformation, &limits, sizeof(limits)) ||
		!AssignProcessToJobObject(memory_job, GetCurrentProcess())) {
	    CloseHandle(memory_job);
	    memory_job = NULL;
	    return false;
	}
	return true;
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
face_boundary_bbox(const ON_BrepFace &face, ON_BoundingBox &bbox)
{
    bbox = ON_BoundingBox::EmptyBoundingBox;
    const auto include_point = [&](const ON_3dPoint &point) {
	if (!point.IsValid())
	    return;
	if (bbox.IsValid())
	    bbox.Set(point, true);
	else
	    bbox = ON_BoundingBox(point, point);
    };
    for (int loop_index = 0; loop_index < face.LoopCount(); ++loop_index) {
	const ON_BrepLoop *loop = face.Loop(loop_index);
	if (!loop)
	    continue;
	for (int trim_index = 0; trim_index < loop->TrimCount(); ++trim_index) {
	    const ON_BrepTrim *trim = loop->Trim(trim_index);
	    if (!trim)
		continue;
	    const ON_BrepVertex *first = trim->Vertex(0);
	    const ON_BrepVertex *second = trim->Vertex(1);
	    if (first)
		include_point(first->Point());
	    if (second)
		include_point(second->Point());
	    const ON_BrepEdge *edge = trim->Edge();
	    if (!edge)
		continue;
	    /* GetTightBoundingBox may conservatively retain distant NURBS
	     * control-hull extents.  The boundary box is the audit's lower
	     * extent reference, so sample each actual proxy-curve span while
	     * retaining the independent face box as the upper guard. */
	    int span_count = std::max(1, edge->SpanCount());
	    std::vector<double> spans((size_t)span_count + 1);
	    if (!edge->GetSpanVector(spans.data())) {
		const ON_Interval domain = edge->Domain();
		span_count = 1;
		spans.resize(2);
		spans[0] = domain.Min();
		spans[1] = domain.Max();
	    }
	    const int samples_per_span = std::max(1,
		std::min(32, 4096 / span_count));
	    for (int span = 0; span < span_count; ++span) {
		const ON_Interval interval(spans[(size_t)span],
		    spans[(size_t)span + 1]);
		for (int sample = 0; sample <= samples_per_span; ++sample) {
		    include_point(edge->PointAt(interval.ParameterAt(
			(double)sample / (double)samples_per_span)));
		}
	    }
	}
    }
    return bbox.IsValid();
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
load_brep(struct db_i *dbip, struct directory *dp, struct rt_db_internal *intern,
    const mat_t transform = NULL)
{
    RT_DB_INTERNAL_INIT(intern);
    if (rt_db_get_internal(intern, dp, dbip, transform) < 0)
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

static bool
parse_exact_ray(const char *arg, ray_audit_exact_ray &ray)
{
    if (!arg || !arg[0])
	return false;
    double value[6] = {};
    const char *start = arg;
    for (size_t i = 0; i < 6; ++i) {
	char *end = NULL;
	errno = 0;
	value[i] = strtod(start, &end);
	if (errno || end == start || !std::isfinite(value[i]) ||
		(i < 5 ? *end != ',' : *end != '\0'))
	    return false;
	start = end + 1;
    }
    VSET(ray.origin, value[0], value[1], value[2]);
    VSET(ray.direction, value[3], value[4], value[5]);
    const double magnitude = MAGNITUDE(ray.direction);
    if (!(magnitude > SMALL_FASTF) || !std::isfinite(magnitude))
	return false;
    VSCALE(ray.direction, ray.direction, 1.0 / magnitude);
    return true;
}

static bool
parse_transform(const char *arg, mat_t transform)
{
    if (!arg || !arg[0])
	return false;
    const char *start = arg;
    for (size_t i = 0; i < ELEMENTS_PER_MAT; ++i) {
	char *end = NULL;
	errno = 0;
	const double value = strtod(start, &end);
	if (errno || end == start || !std::isfinite(value) ||
		(i + 1 < ELEMENTS_PER_MAT ? *end != ',' : *end != '\0'))
	    return false;
	transform[i] = value;
	start = end + 1;
    }
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
raytrace_prep_result(struct db_i *dbip, struct directory *dp, int depth)
{
    prep_result result;
    result.requested_depth = depth;
    result.resident_before_bytes = resident_rss_bytes();
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
    stp->st_dp = dp;
    stp->st_id = ID_BREP;
    stp->st_meth = &OBJ[ID_BREP];

    if (rtip && !_rt_brep_set_surface_tree_depth(rtip, depth)) {
	rt_i_destroy(rtip);
	bu_free(stp, "brep audit prep soltab");
	rt_db_free_internal(&intern);
	result.failure_reason = "invalid_depth";
	return result;
    }
    int64_t start = bu_gettime();
    result.ret = rtip ? OBJ[ID_BREP].ft_prep(stp, &intern, rtip) :
	BRLCAD_ERROR;
    result.seconds = (bu_gettime() - start) / 1000000.0;
    result.resident_prepared_bytes = resident_rss_bytes();
    result.peak_rss_bytes = peak_rss_bytes();
    if (result.ret == BRLCAD_OK) {
	VMOVE(result.bmin, stp->st_min);
	VMOVE(result.bmax, stp->st_max);
	result.have_bbox = true;
	result.stats_valid = _rt_brep_prep_stats(stp, &result.stats) != 0;
	struct bu_external payload = BU_EXTERNAL_INIT_ZERO;
	start = bu_gettime();
	if (!rt_obj_prep_serialize(stp, &intern, &payload,
		&result.cache_variant)) {
	    result.cache_payload_valid = true;
	    result.cache_payload_bytes = payload.ext_nbytes;
	}
	result.cache_serialize_seconds =
	    (bu_gettime() - start) / 1000000.0;
	if (result.cache_payload_valid) {
	    start = bu_gettime();
	    if (_rt_cache_compress_external(&payload)) {
		result.cache_payload_compressed = true;
		result.cache_payload_compressed_bytes = payload.ext_nbytes;
	    }
	    result.cache_compress_seconds =
		(bu_gettime() - start) / 1000000.0;
	}
	bu_free_external(&payload);
    }
    if (stp->st_specific && stp->st_meth && stp->st_meth->ft_free)
	stp->st_meth->ft_free(stp);
    bu_free(stp, "brep audit prep soltab");
    if (rtip)
	rt_i_destroy(rtip);
    rt_db_free_internal(&intern);
    result.resident_after_free_bytes = resident_rss_bytes();

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

static const char *
on_object_class_name(const ON_Object *object)
{
    if (!object)
	return "none";
    const ON_ClassId *class_id = object->ClassId();
    return class_id ? class_id->ClassName() : "unknown";
}

static double
parameter_domain_scale(const ON_Interval &domain)
{
    double scale = 1.0;
    if (std::isfinite(domain.Min()))
	scale = std::max(scale, std::fabs(domain.Min()));
    if (std::isfinite(domain.Max()))
	scale = std::max(scale, std::fabs(domain.Max()));
    if (std::isfinite(domain.Length()))
	scale = std::max(scale, std::fabs(domain.Length()));
    return scale;
}

static double
parameter_domain_tolerance(const ON_Interval &domain)
{
    return 4096.0 * std::numeric_limits<double>::epsilon() *
	parameter_domain_scale(domain);
}

static double
normalized_parameter_error(double error, const ON_Interval &domain)
{
    const double length = std::fabs(domain.Length());
    const double divisor = std::isfinite(length) && length > 0.0 ?
	length : parameter_domain_scale(domain);
    return error / divisor;
}

static bool
finite_parameter_pair(const double parameter[2])
{
    return std::isfinite(parameter[0]) && std::isfinite(parameter[1]);
}

static bool
parameter_pair_in_domain(const double parameter[2],
    const ON_Interval domain[2])
{
    for (int direction = 0; direction < 2; ++direction) {
	const double minimum = domain[direction].Min();
	const double maximum = domain[direction].Max();
	if (!std::isfinite(parameter[direction]) ||
		!std::isfinite(minimum) || !std::isfinite(maximum) ||
		parameter[direction] < minimum ||
		parameter[direction] > maximum)
	    return false;
    }
    return true;
}

static void
record_round_trip_error(parameter_mapping_direction_profile &profile,
    const double expected[2], const double actual[2],
    const ON_Interval domain[2])
{
    bool within_tolerance = true;
    for (int direction = 0; direction < 2; ++direction) {
	const double error = std::fabs(actual[direction] - expected[direction]);
	profile.maximum_round_trip_error[direction] = std::max(
	    profile.maximum_round_trip_error[direction], error);
	profile.maximum_normalized_round_trip_error[direction] = std::max(
	    profile.maximum_normalized_round_trip_error[direction],
	    normalized_parameter_error(error, domain[direction]));
	if (error > parameter_domain_tolerance(domain[direction]))
	    within_tolerance = false;
    }
    if (within_tolerance)
	profile.round_trip_within_tolerance++;
}

static void
analyze_parameter_mapping_behavior(
    const std::vector<parameter_mapping_sample> &samples, size_t grid_size,
    const ON_Interval output_domain[2],
    parameter_mapping_direction_profile &profile)
{
    profile.behavior_complete = grid_size >= 2 &&
	samples.size() == grid_size * grid_size &&
	profile.successes == profile.attempts;
    if (!profile.behavior_complete)
	return;

    double diagonal_cross_error[2] = {0.0, 0.0};
    double transposed_cross_error[2] = {0.0, 0.0};
    const auto sample = [&samples, grid_size](size_t first, size_t second) ->
	const parameter_mapping_sample & {
	return samples[first * grid_size + second];
    };
    for (size_t first = 0; first < grid_size; ++first) {
	for (size_t second = 0; second < grid_size; ++second) {
	    diagonal_cross_error[0] = std::max(diagonal_cross_error[0],
		std::fabs(sample(first, second).output[0] -
		    sample(first, 0).output[0]));
	    diagonal_cross_error[1] = std::max(diagonal_cross_error[1],
		std::fabs(sample(first, second).output[1] -
		    sample(0, second).output[1]));
	    transposed_cross_error[0] = std::max(transposed_cross_error[0],
		std::fabs(sample(first, second).output[0] -
		    sample(0, second).output[0]));
	    transposed_cross_error[1] = std::max(transposed_cross_error[1],
		std::fabs(sample(first, second).output[1] -
		    sample(first, 0).output[1]));
	}
    }

    const bool diagonal =
	diagonal_cross_error[0] <= parameter_domain_tolerance(output_domain[0]) &&
	diagonal_cross_error[1] <= parameter_domain_tolerance(output_domain[1]);
    const bool transposed =
	transposed_cross_error[0] <= parameter_domain_tolerance(output_domain[0]) &&
	transposed_cross_error[1] <= parameter_domain_tolerance(output_domain[1]);
    profile.separable = diagonal || transposed;
    if (diagonal) {
	profile.output_input_axis[0] = 0;
	profile.output_input_axis[1] = 1;
	profile.maximum_cross_axis_error[0] = diagonal_cross_error[0];
	profile.maximum_cross_axis_error[1] = diagonal_cross_error[1];
    } else if (transposed) {
	profile.output_input_axis[0] = 1;
	profile.output_input_axis[1] = 0;
	profile.maximum_cross_axis_error[0] = transposed_cross_error[0];
	profile.maximum_cross_axis_error[1] = transposed_cross_error[1];
    } else {
	profile.maximum_cross_axis_error[0] = std::min(diagonal_cross_error[0],
	    transposed_cross_error[0]);
	profile.maximum_cross_axis_error[1] = std::min(diagonal_cross_error[1],
	    transposed_cross_error[1]);
	return;
    }

    for (int output = 0; output < 2; ++output) {
	const int input = profile.output_input_axis[output];
	const double tolerance = parameter_domain_tolerance(output_domain[output]);
	bool increasing = false;
	bool decreasing = false;
	bool constant = false;
	for (size_t fixed = 0; fixed < grid_size; ++fixed) {
	    for (size_t step = 1; step < grid_size; ++step) {
		const parameter_mapping_sample &previous = input == 0 ?
		    sample(step - 1, fixed) : sample(fixed, step - 1);
		const parameter_mapping_sample &current = input == 0 ?
		    sample(step, fixed) : sample(fixed, step);
		const double delta = current.output[output] -
		    previous.output[output];
		if (delta > tolerance)
		    increasing = true;
		else if (delta < -tolerance)
		    decreasing = true;
		else
		    constant = true;
	    }
	}
	if (increasing && decreasing) {
	    profile.orientation[output] = PARAMETER_MAPPING_MIXED;
	    continue;
	}
	profile.monotone[output] = true;
	profile.strictly_monotone[output] = !constant &&
	    (increasing || decreasing);
	if (increasing)
	    profile.orientation[output] = PARAMETER_MAPPING_INCREASING;
	else if (decreasing)
	    profile.orientation[output] = PARAMETER_MAPPING_DECREASING;
	else
	    profile.orientation[output] = PARAMETER_MAPPING_CONSTANT;
    }
}

static void
profile_surface_parameter_mapping(const ON_Surface *surface,
    const ON_NurbsSurface &nurb_form,
    surface_parameter_mapping_profile &mapping)
{
    if (!surface)
	return;
    const size_t grid_size = 9;
    const ON_Interval surface_domain[2] = {
	surface->Domain(0), surface->Domain(1)
    };
    const ON_Interval nurb_domain[2] = {
	nurb_form.Domain(0), nurb_form.Domain(1)
    };
    mapping.sampled = true;
    mapping.grid_size = grid_size;
    std::vector<parameter_mapping_sample> forward_samples(
	grid_size * grid_size);
    std::vector<parameter_mapping_sample> inverse_samples(
	grid_size * grid_size);

    for (size_t first = 0; first < grid_size; ++first) {
	for (size_t second = 0; second < grid_size; ++second) {
	    const size_t index = first * grid_size + second;
	    parameter_mapping_sample &sample = forward_samples[index];
	    sample.input[0] = surface_domain[0].ParameterAt(
		(double)first / (double)(grid_size - 1));
	    sample.input[1] = surface_domain[1].ParameterAt(
		(double)second / (double)(grid_size - 1));
	    mapping.forward.attempts++;
	    sample.valid = surface->GetNurbFormParameterFromSurfaceParameter(
		sample.input[0], sample.input[1], &sample.output[0],
		&sample.output[1]) && finite_parameter_pair(sample.output);
	    if (sample.valid) {
		mapping.forward.successes++;
		if (parameter_pair_in_domain(sample.output, nurb_domain))
		    mapping.forward.domain_contained++;
		double round_trip[2] = {0.0, 0.0};
		if (surface->GetSurfaceParameterFromNurbFormParameter(
			sample.output[0], sample.output[1], &round_trip[0],
			&round_trip[1]) && finite_parameter_pair(round_trip)) {
		    mapping.forward.round_trip_successes++;
		    record_round_trip_error(mapping.forward, sample.input,
			round_trip, surface_domain);
		}
	    }

	    parameter_mapping_sample &inverse = inverse_samples[index];
	    inverse.input[0] = nurb_domain[0].ParameterAt(
		(double)first / (double)(grid_size - 1));
	    inverse.input[1] = nurb_domain[1].ParameterAt(
		(double)second / (double)(grid_size - 1));
	    mapping.inverse.attempts++;
	    inverse.valid = surface->GetSurfaceParameterFromNurbFormParameter(
		inverse.input[0], inverse.input[1], &inverse.output[0],
		&inverse.output[1]) && finite_parameter_pair(inverse.output);
	    if (inverse.valid) {
		mapping.inverse.successes++;
		if (parameter_pair_in_domain(inverse.output, surface_domain))
		    mapping.inverse.domain_contained++;
		double inverse_round_trip[2] = {0.0, 0.0};
		if (surface->GetNurbFormParameterFromSurfaceParameter(
			inverse.output[0], inverse.output[1],
			&inverse_round_trip[0], &inverse_round_trip[1]) &&
			finite_parameter_pair(inverse_round_trip)) {
		    mapping.inverse.round_trip_successes++;
		    record_round_trip_error(mapping.inverse, inverse.input,
			inverse_round_trip, nurb_domain);
		}
	    }
	}
    }
    analyze_parameter_mapping_behavior(forward_samples, grid_size,
	nurb_domain, mapping.forward);
    analyze_parameter_mapping_behavior(inverse_samples, grid_size,
	surface_domain, mapping.inverse);
}

static void
profile_surface_representation(const ON_BrepFace &face,
    const ON_Surface *surface, surface_tree_profile &profile)
{
    profile.face_reversed = face.m_bRev;
    profile.face_proxy_transposed = face.ProxySurfaceIsTransposed();
    if (!surface)
	return;
    for (int direction = 0; direction < 2; ++direction) {
	profile.closed[direction] = surface->IsClosed(direction);
	profile.periodic[direction] = surface->IsPeriodic(direction);
    }
    for (int side = 0; side < 4; ++side)
	profile.singular_sides[side] = surface->IsSingular(side);

    const ON_Surface *resolved_surface = surface;
    const ON_SurfaceProxy *surface_proxy = ON_SurfaceProxy::Cast(surface);
    if (surface_proxy) {
	profile.surface_proxy = true;
	profile.surface_proxy_transposed =
	    surface_proxy->ProxySurfaceIsTransposed();
	const ON_Surface *target = surface_proxy->ProxySurface();
	profile.surface_proxy_target_type = on_object_class_name(target);
	if (target)
	    resolved_surface = target;
    }

    const ON_RevSurface *revolution = ON_RevSurface::Cast(resolved_surface);
    if (!revolution)
	return;
    profile.revolution = true;
    profile.revolution_transposed = revolution->m_bTransposed;
    const ON_Curve *curve = revolution->m_curve;
    profile.profile_type = on_object_class_name(curve);
    if (!curve)
	return;
    profile.profile_domain = curve->Domain();

    const ON_Curve *resolved_curve = curve;
    const ON_CurveProxy *curve_proxy = ON_CurveProxy::Cast(curve);
    if (curve_proxy) {
	profile.profile_proxy = true;
	profile.profile_proxy_reversed = curve_proxy->ProxyCurveIsReversed();
	profile.profile_proxy_domain = curve_proxy->ProxyCurveDomain();
	const ON_Curve *target = curve_proxy->ProxyCurve();
	profile.profile_proxy_target_type = on_object_class_name(target);
	if (target)
	    resolved_curve = target;
    }

    const ON_PolyCurve *polycurve = ON_PolyCurve::Cast(resolved_curve);
    if (!polycurve)
	return;
    profile.profile_polycurve = true;
    profile.profile_polycurve_segments.reserve((size_t)std::max(0,
	polycurve->Count()));
    for (int segment_index = 0; segment_index < polycurve->Count();
	    ++segment_index) {
	polycurve_segment_profile segment;
	const ON_Curve *segment_curve = polycurve->SegmentCurve(segment_index);
	segment.type = on_object_class_name(segment_curve);
	segment.polycurve_domain = polycurve->SegmentDomain(segment_index);
	if (segment_curve)
	    segment.curve_domain = segment_curve->Domain();
	const ON_CurveProxy *segment_proxy =
	    ON_CurveProxy::Cast(segment_curve);
	if (segment_proxy) {
	    segment.proxy = true;
	    segment.proxy_reversed = segment_proxy->ProxyCurveIsReversed();
	    segment.proxy_domain = segment_proxy->ProxyCurveDomain();
	    segment.proxy_target_type = on_object_class_name(
		segment_proxy->ProxyCurve());
	}
	profile.profile_polycurve_segments.push_back(segment);
    }
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
	profile_surface_representation(face, surface, profile);
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
		    profile.nurb_form_domain[direction] =
			nurb_form.Domain(direction);
		}
		if (profile.nurb_form_status == 2)
		    profile_surface_parameter_mapping(surface, nurb_form,
			profile.parameter_mapping);
	    }
	}
	int64_t start = bu_gettime();
	brlcad::SurfaceTree tree(&face, true, depth_limit);
	profile.seconds = (bu_gettime() - start) / 1000000.0;
	if (tree.Valid()) {
	    surface_tree_node_counts(tree.getRootNode(), 0, profile.nodes,
		profile.leaves, profile.maximum_depth);
	    if (tree.m_ctree)
		profile.curve_leaves = tree.m_ctree->preparedLeafCount();
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

static const char *
ray_audit_legacy_capacity_fallback_name(int reason)
{
    static const char *names[RT_BREP_LEGACY_CAPACITY_FALLBACK_COUNT] = {
	"leaf_workspace", "hit_workspace"
    };
    return reason >= 0 && reason < RT_BREP_LEGACY_CAPACITY_FALLBACK_COUNT ?
	names[reason] : "invalid";
}


static const char *
ray_audit_legacy_cleanup_action_name(int action)
{
    static const char *names[RT_BREP_LEGACY_CLEANUP_COUNT] = {
	"coincident_grazing", "near_miss_shadow", "near_miss_crack_merge",
	"near_miss_crack_cancel", "near_miss_nonadjacent_drop",
	"clean_overlap", "near_hit_shadow", "near_hit_crack_merge",
	"trimmed_invalid", "parameter_oob", "isolated_grazing",
	"duplicate_cancel", "duplicate_collapse", "solid_coincident",
	"solid_unmatched_enter", "solid_unmatched_leave",
	"solid_interior_winding", "open_same_direction",
	"open_odd_endpoint"
    };
    return action >= 0 && action < RT_BREP_LEGACY_CLEANUP_COUNT ?
	names[action] : "invalid";
}


static const char *
ray_audit_route_name(int lane)
{
    switch (lane) {
	case RT_BREP_ROUTE_LEGACY_DIRECT: return "none";
	case RT_BREP_ROUTE_SEEDED_ORDINARY_VERIFY:
	    return "seeded_ordinary";
	case RT_BREP_ROUTE_EXISTING_COMPLEX_PREPARED:
	    return "complex_prepared";
    }
    return "unavailable";
}

enum ray_audit_route_stratum {
    RAY_AUDIT_ROUTE_EMPTY = 0,
    RAY_AUDIT_ROUTE_CLEAN_ORDINARY,
    RAY_AUDIT_ROUTE_MIXED_HAZARD,
    RAY_AUDIT_ROUTE_ALL_HAZARD,
    RAY_AUDIT_ROUTE_UNSUPPORTED,
    RAY_AUDIT_ROUTE_UNCLASSIFIED,
    RAY_AUDIT_ROUTE_STRATUM_COUNT
};

enum ray_audit_correction_shape {
    RAY_AUDIT_CORRECTION_PREPARED_MISS_LEGACY_HIT = 0,
    RAY_AUDIT_CORRECTION_PREPARED_HIT_LEGACY_MISS,
    RAY_AUDIT_CORRECTION_BOTH_NONEMPTY_CHANGED,
    RAY_AUDIT_CORRECTION_OTHER,
    RAY_AUDIT_CORRECTION_SHAPE_COUNT
};

static ray_audit_route_stratum
ray_audit_route_stratum_for(const ray_audit_shot &shot)
{
    const unsigned int qualification = shot.route_decision.qualification_flags;
    if (qualification & RT_BREP_ROUTE_QUAL_UNSUPPORTED)
	return RAY_AUDIT_ROUTE_UNSUPPORTED;
    if (qualification & RT_BREP_ROUTE_QUAL_EMPTY)
	return RAY_AUDIT_ROUTE_EMPTY;
    if (qualification & RT_BREP_ROUTE_QUAL_MIXED_HAZARD)
	return RAY_AUDIT_ROUTE_MIXED_HAZARD;
    if (qualification & RT_BREP_ROUTE_QUAL_CLEAN_ORDINARY)
	return RAY_AUDIT_ROUTE_CLEAN_ORDINARY;
    /* Observation strata describe the legacy roots, independently of the
     * optional work chosen by the router.  In particular, an all-hazard ray
     * remains all-hazard when a narrower contact selector correctly leaves it
     * on the retained legacy transaction. */
    if (qualification & RT_BREP_ROUTE_QUAL_CORRECTION_RISK)
	return RAY_AUDIT_ROUTE_ALL_HAZARD;
    return RAY_AUDIT_ROUTE_UNCLASSIFIED;
}

static const char *
ray_audit_route_stratum_name(ray_audit_route_stratum stratum)
{
    static const char *names[RAY_AUDIT_ROUTE_STRATUM_COUNT] = {
	"empty", "clean_ordinary", "mixed_hazard", "all_hazard",
	"unsupported", "unclassified"
    };
    return stratum >= 0 && stratum < RAY_AUDIT_ROUTE_STRATUM_COUNT ?
	names[stratum] : "invalid";
}

static ray_audit_correction_shape
ray_audit_correction_shape_for(const ray_audit_shot &prepared,
	const ray_audit_shot &legacy)
{
    const bool prepared_empty = prepared.intervals.empty();
    const bool legacy_empty = legacy.intervals.empty();
    if (prepared_empty && !legacy_empty)
	return RAY_AUDIT_CORRECTION_PREPARED_MISS_LEGACY_HIT;
    if (!prepared_empty && legacy_empty)
	return RAY_AUDIT_CORRECTION_PREPARED_HIT_LEGACY_MISS;
    if (!prepared_empty && !legacy_empty)
	return RAY_AUDIT_CORRECTION_BOTH_NONEMPTY_CHANGED;
    return RAY_AUDIT_CORRECTION_OTHER;
}

static const char *
ray_audit_correction_shape_name(ray_audit_correction_shape shape)
{
    static const char *names[RAY_AUDIT_CORRECTION_SHAPE_COUNT] = {
	"prepared_miss_legacy_hit", "prepared_hit_legacy_miss",
	"both_nonempty_changed", "other"
    };
    return shape >= 0 && shape < RAY_AUDIT_CORRECTION_SHAPE_COUNT ?
	names[shape] : "invalid";
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
    ray_audit_prep &result, const mat_t transform = NULL)
{
    result.requested_depth = depth;
    struct rt_db_internal intern;
    if (!tol || load_brep(dbip, dp, &intern, transform) != BRLCAD_OK)
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

static std::vector<ray_audit_sample>
ray_audit_exact_samples(const ray_audit_exact_ray &ray, const point_t bmin,
    const point_t bmax, double minimum_length)
{
    vect_t diagonal;
    VSUB2(diagonal, bmax, bmin);
    const double margin = std::max(MAGNITUDE(diagonal), minimum_length);
    double maximum_projection = -std::numeric_limits<double>::infinity();
    for (int corner = 0; corner < 8; ++corner) {
	point_t point;
	VSET(point, corner & 1 ? bmax[X] : bmin[X],
	    corner & 2 ? bmax[Y] : bmin[Y],
	    corner & 4 ? bmax[Z] : bmin[Z]);
	vect_t offset;
	VSUB2(offset, point, ray.origin);
	maximum_projection = std::max(maximum_projection,
	    (double)VDOT(offset, ray.direction));
    }
    const double chord_length = std::max(margin,
	maximum_projection + margin);
    std::vector<ray_audit_sample> samples;
    if (!(chord_length > SMALL_FASTF) || !std::isfinite(chord_length))
	return samples;
    samples.reserve(2);
    ray_audit_sample forward;
    VMOVE(forward.origin, ray.origin);
    VMOVE(forward.direction, ray.direction);
    forward.chord_length = chord_length;
    forward.kind = RAY_AUDIT_EXACT_CHORD;
    samples.push_back(forward);
    ray_audit_sample reverse;
    VJOIN1(reverse.origin, ray.origin, chord_length, ray.direction);
    VREVERSE(reverse.direction, ray.direction);
    reverse.chord_length = chord_length;
    reverse.reverse = true;
    reverse.kind = RAY_AUDIT_EXACT_CHORD;
    samples.push_back(reverse);
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
	case RAY_AUDIT_EXACT_CHORD:
	    return "exact";
	case RAY_AUDIT_SAMPLE_KIND_COUNT:
	    break;
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
    result.seeded_verifier_complete = trace->seeded_verify_complete == 1;
    result.seeded_route_lane = trace->route_decision_attempts ?
	trace->seeded_route_lane : -1;
    result.seeded_verifier_failure_stage =
	trace->seeded_verify_failure_stage;
    result.seeded_route_capability_flags =
	trace->seeded_route_capability_flags;
    result.seeded_route_risk_flags = trace->seeded_route_risk_flags;
    result.route_decision_attempts = trace->route_decision_attempts;
    result.route_decision = trace->route_decision;
    result.missing_root_counterfactual_attempts =
	trace->missing_root_counterfactual_attempts;
    result.missing_root_counterfactual =
	trace->missing_root_counterfactual;
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
    result.fixed_leaf_fallback = trace->fixed_leaf_fallback;
    result.fixed_hit_fallback = trace->fixed_hit_fallback;
    for (int action = 0; action < RT_BREP_LEGACY_CLEANUP_COUNT; ++action)
	result.legacy_cleanup_actions[action] =
	    trace->legacy_cleanup_actions[action];
    result.legacy_cleanup_action_mismatches =
	trace->legacy_cleanup_action_mismatches;
    result.trim_equivalence_mismatches = trace->trim_equivalence_mismatches;
    result.face_trim_status_mismatches =
	trace->face_trim_status_mismatches;
    result.face_trim_hit_class_mismatches =
	trace->face_trim_hit_class_mismatches;
    result.face_trim_adjacency_mismatches =
	trace->face_trim_adjacency_mismatches;
    result.face_trim_equivalence_mismatches =
	trace->face_trim_equivalence_mismatches;
    result.prepared_production_hits = trace->prepared_production_hits;
    result.prepared_require_surface_box_free =
	trace->prepared_require_surface_box_free != 0;
    result.prepared_selective_class_rejections =
	trace->prepared_selective_class_rejections;
    result.prepared_selective_candidate_surface_spans =
	trace->prepared_selective_candidate_surface_spans;
    result.prepared_selective_subdivision_boxes =
	trace->prepared_selective_subdivision_boxes;
    result.prepared_selective_surface_boxes =
	trace->prepared_selective_surface_boxes;
    result.physical_event_complete = trace->physical_event_complete;
    result.physical_event_unresolved = trace->physical_event_unresolved;
    result.physical_event_state_failures =
	trace->physical_event_state_failures;
    result.physical_event_material_segments =
	trace->physical_event_material_segments;
    result.physical_event_subminimum_contacts =
	trace->physical_event_subminimum_contacts;
    result.physical_event_tolerance_ambiguous =
	trace->physical_event_tolerance_ambiguous;
    result.stored_physical_events = trace->stored_physical_events;
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
	VMOVE(interval.in_point, segp->seg_in.hit_point);
	VMOVE(interval.out_point, segp->seg_out.hit_point);
	VMOVE(interval.in_normal, segp->seg_in.hit_normal);
	VMOVE(interval.out_normal, segp->seg_out.hit_normal);
	interval.in_uv[0] = segp->seg_in.hit_vpriv[0];
	interval.in_uv[1] = segp->seg_in.hit_vpriv[1];
	interval.out_uv[0] = segp->seg_out.hit_vpriv[0];
	interval.out_uv[1] = segp->seg_out.hit_vpriv[1];
	if (segp->seg_stp != prepared.stp)
	    result.valid = false;
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

/* Exercise the same cache-aware object-prep path used by normal raytracing.
 * Unlike raytrace_prep_result(), the measured interval includes tree loading
 * and the final rt_prep() pass, but excludes opening the database.  A BREP
 * cache load is observable without exposing cache internals: deserialization
 * reconstructs all derived routing data but records zero SurfaceTree build
 * time. */
static prep_result
raytrace_cached_prep_result(struct db_i *dbip, struct directory *dp,
	const char *db_path, const char *object, int depth)
{
    prep_result result;
    result.requested_depth = depth;
    result.cache_lookup_requested = true;

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
    rt_db_free_internal(&intern);

    struct rt_i *rtip = rt_dirbuild(db_path, NULL, 0);
    if (!rtip) {
	result.failure_reason = "database_raytrace_load";
	return result;
    }
    if (!_rt_brep_set_surface_tree_depth(rtip, depth)) {
	rt_i_destroy(rtip);
	result.failure_reason = "invalid_depth";
	return result;
    }

    result.resident_before_bytes = resident_rss_bytes();
    const int64_t start = bu_gettime();
    const int tree_result = rt_gettree(rtip, object);
    if (tree_result >= 0)
	rt_prep(rtip);
    result.seconds = (bu_gettime() - start) / 1000000.0;
    result.resident_prepared_bytes = resident_rss_bytes();
    result.peak_rss_bytes = peak_rss_bytes();

    struct soltab *stp = tree_result >= 0 ? rt_find_solid(rtip, object) : NULL;
    result.ret = stp ? BRLCAD_OK : BRLCAD_ERROR;
    if (stp) {
	VMOVE(result.bmin, stp->st_min);
	VMOVE(result.bmax, stp->st_max);
	result.have_bbox = true;
	result.stats_valid = _rt_brep_prep_stats(stp, &result.stats) != 0;
	result.cache_hit = result.stats_valid &&
	    result.stats.surface_tree_build_microseconds == 0;
    }

    rt_clean(rtip);
    rt_i_destroy(rtip);
    result.resident_after_free_bytes = resident_rss_bytes();
    if (result.ret != BRLCAD_OK) {
	result.failure_reason = "cache_prep";
	diagnose_surface_tree_failures(dbip, dp, &result);
    }
    return result;
}


/* Exercise the production selective transaction without trace-mode side
 * effects.  This is deliberately separate from ray_audit_shoot: trace mode
 * constructs both prepared and legacy records, while the direct transaction
 * must prove that it publishes exactly one complete result. */
static ray_audit_shot
ray_audit_shoot_routed(audit_prepared_brep &prepared,
    const ray_audit_sample &sample, const struct bn_tol *tol,
    bool publish_optional, bool inject_legacy_root_fault = false)
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
    if (inject_legacy_root_fault) {
	result.route_test_available = true;
	result.hit_count = _rt_brep_shot_route_missing_root_test(prepared.stp,
	    &ray, &ap, &seghead, publish_optional ? 1 : 0,
	    &result.route_test);
	result.route_decision = result.route_test.decision;
	result.route_decision_attempts = 1;
	result.seeded_route_lane = result.route_test.decision.optional_work;
	result.prepared_selected = result.route_test.optional_complete != 0;
	result.prepared_require_surface_box_free =
	    result.route_test.require_empty_partition == 1;
	result.fallback = result.route_test.prepared_fallback;
    } else {
	result.hit_count = _rt_brep_shot_route(prepared.stp, &ray, &ap,
	    &seghead, publish_optional ? 1 : 0);
    }

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
	VMOVE(interval.in_point, segp->seg_in.hit_point);
	VMOVE(interval.out_point, segp->seg_out.hit_point);
	VMOVE(interval.in_normal, segp->seg_in.hit_normal);
	VMOVE(interval.out_normal, segp->seg_out.hit_normal);
	interval.in_uv[0] = segp->seg_in.hit_vpriv[0];
	interval.in_uv[1] = segp->seg_in.hit_vpriv[1];
	interval.out_uv[0] = segp->seg_out.hit_vpriv[0];
	interval.out_uv[1] = segp->seg_out.hit_vpriv[1];
	if (segp->seg_stp != prepared.stp)
	    result.valid = false;
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
	    (size_t)result.hit_count != 2 * result.intervals.size())
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

/* A completed prepared physical-event stream is independent of the legacy
 * cleanup that published the comparison interval.  If every raw legacy root
 * is either grazing or on a trim boundary, that reduced stream proves the
 * disagreement belongs to the boundary/contact stratum rather than the clean
 * ordinary stratum. */
static bool
ray_audit_certified_boundary_contact_miss(const ray_audit_shot &prepared,
    const ray_audit_shot &legacy, const struct bn_tol *tol)
{
    if (!tol || !prepared.valid || !legacy.valid ||
	!prepared.prepared_selected || prepared.hit_count ||
	prepared.physical_event_complete != 1 ||
	prepared.physical_event_unresolved ||
	prepared.physical_event_state_failures ||
	prepared.physical_event_material_segments ||
	prepared.physical_event_subminimum_contacts ||
	prepared.physical_event_tolerance_ambiguous ||
	prepared.stored_physical_events || legacy.hit_count <= 0 ||
	legacy.intervals.empty() || prepared.roots.empty())
	return false;

    bool have_trim_boundary = false;
    for (size_t root_index = 0; root_index < prepared.roots.size();
	    ++root_index) {
	const struct rt_brep_trace_root &root = prepared.roots[root_index];
	const bool grazing = std::isfinite(root.normal_dot) &&
	    fabs(root.normal_dot) <= tol->perp;
	const bool trim_boundary = std::isfinite(root.trim_distance) &&
	    root.trim_distance >= 0.0 && root.trim_distance <= tol->dist;
	if (!grazing && !trim_boundary)
	    return false;
	have_trim_boundary = have_trim_boundary || trim_boundary;
    }
    return have_trim_boundary;
}


/* A completed selective replay is a separate publication certificate from
 * the boundary/contact oracle above.  It rebuilds the complete ray
 * transaction from a finer fixed grid and accepts it only after an
 * independently repeated reverse-direction solve agrees. */
static bool
ray_audit_certified_selective_replay(const ray_audit_shot &prepared)
{

    if (!prepared.valid ||
	!prepared.prepared_selected ||
	!prepared.route_decision.legacy_transaction_incomplete ||
	!prepared.route_decision.missing_root_certificate_triggered ||
	!prepared.route_decision.selective_refinement_attempted ||
	!prepared.route_decision.selective_refinement_complete ||
	!prepared.route_decision.selective_refinement_level ||
	!prepared.route_decision.selective_refinement_hits ||
	prepared.route_decision.selective_refinement_reversal_mismatches)
	return false;
    return true;
}

static void
ray_audit_accumulate_route_certificate(
    ray_audit_route_certificate_telemetry &total,
    const struct rt_brep_route_decision &decision)
{
    const size_t associations =
	decision.missing_root_certificate_associations;
    total.associations += associations;
    for (int state = 0; state < RT_BREP_ROUTE_CERT_STATE_COUNT; ++state)
	total.state[state] += decision.missing_root_certificate_state[state];
    total.regular_root_count +=
	decision.missing_root_certificate_regular_root_count;
    total.observed_roots +=
	decision.missing_root_certificate_observed_roots;
    total.deficit_cells +=
	decision.missing_root_certificate_deficit_cells;
    total.deficit_roots +=
	decision.missing_root_certificate_deficit_roots;
    total.cached_bbox_exclusions +=
	decision.missing_root_certificate_cached_bbox_exclusions;
    total.hull_exclusions +=
	decision.missing_root_certificate_hull_exclusions;
    total.subdivision_attempts +=
	decision.missing_root_certificate_subdivision_attempts;
    total.subdivision_boxes +=
	decision.missing_root_certificate_subdivision_boxes;
    total.subdivision_hull_exclusions +=
	decision.missing_root_certificate_subdivision_hull_exclusions;
    total.subdivision_exclusions +=
	decision.missing_root_certificate_subdivision_exclusions;
    total.degree_attempts += decision.missing_root_certificate_degree_attempts;
    total.degree_complete += decision.missing_root_certificate_degree_complete;
    for (int reason = 0; reason < RT_BREP_ROUTE_CERT_FAILURE_COUNT; ++reason)
	total.failure_reason[reason] +=
	    decision.missing_root_certificate_failure_reason[reason];
    for (int detail = 0; detail < RT_BREP_ROOT_DEGREE_DETAIL_COUNT; ++detail)
	total.degree_failure_detail[detail] +=
	    decision.missing_root_certificate_degree_failure_detail[detail];
    total.boundary_pieces +=
	decision.missing_root_certificate_boundary_pieces;
    total.boundary_splits +=
	decision.missing_root_certificate_boundary_splits;
    total.boundary_edges += decision.missing_root_certificate_boundary_edges;
    total.expansion_high_water = std::max(total.expansion_high_water,
	decision.missing_root_certificate_expansion_high_water);
    total.nanoseconds += decision.missing_root_certificate_nanoseconds;

    if (!associations)
	return;
    const size_t incomplete =
	decision.missing_root_certificate_state[RT_BREP_ROUTE_CERT_INCOMPLETE];
    const size_t root_free =
	decision.missing_root_certificate_state[RT_BREP_ROUTE_CERT_ROOT_FREE];
    const size_t root_positive =
	decision.missing_root_certificate_state[RT_BREP_ROUTE_CERT_ROOT_POSITIVE];
    if (incomplete)
	total.incomplete_rays++;
    else if (root_free + root_positive == associations)
	total.complete_rays++;
    if (root_positive)
	total.root_positive_rays++;
    else if (!incomplete && root_free == associations)
	total.root_free_rays++;
}

static bool
ray_audit_route_certificate_accounting_valid(
    const ray_audit_route_certificate_telemetry &certificate)
{
    size_t state_total = 0;
    for (int state = 0; state < RT_BREP_ROUTE_CERT_STATE_COUNT; ++state)
	state_total += certificate.state[state];
    size_t failure_total = 0;
    for (int reason = 0; reason < RT_BREP_ROUTE_CERT_FAILURE_COUNT; ++reason)
	failure_total += certificate.failure_reason[reason];
    size_t detail_total = 0;
    for (int detail = 0; detail < RT_BREP_ROOT_DEGREE_DETAIL_COUNT; ++detail)
	detail_total += certificate.degree_failure_detail[detail];

    const size_t incomplete =
	certificate.state[RT_BREP_ROUTE_CERT_INCOMPLETE];
    const size_t root_free =
	certificate.state[RT_BREP_ROUTE_CERT_ROOT_FREE];
    const size_t root_positive =
	certificate.state[RT_BREP_ROUTE_CERT_ROOT_POSITIVE];
    if (state_total != certificate.associations || failure_total != incomplete ||
	    certificate.degree_complete > certificate.degree_attempts ||
	    detail_total != certificate.degree_attempts - certificate.degree_complete ||
	    certificate.cached_bbox_exclusions > root_free ||
	    certificate.hull_exclusions >
		root_free - certificate.cached_bbox_exclusions ||
	    certificate.subdivision_exclusions >
		root_free - certificate.cached_bbox_exclusions -
		    certificate.hull_exclusions ||
	    certificate.subdivision_hull_exclusions <
		certificate.subdivision_exclusions ||
	    certificate.subdivision_exclusions >
		certificate.subdivision_attempts ||
	    certificate.subdivision_attempts !=
		certificate.subdivision_exclusions +
		    certificate.degree_attempts ||
	    root_free - certificate.cached_bbox_exclusions -
		certificate.hull_exclusions - certificate.subdivision_exclusions +
		root_positive !=
		certificate.degree_complete ||
	    certificate.regular_root_count < root_positive ||
	    certificate.deficit_roots > certificate.regular_root_count)
	return false;

    /* Each ray population is a lower-cardinality projection of association
     * state.  A root-positive ray may also be incomplete because another
     * association on that ray failed, so it is intentionally not required to
     * be a subset of complete_rays. */
    return certificate.complete_rays + certificate.incomplete_rays <=
	    certificate.associations &&
	certificate.root_free_rays <= certificate.complete_rays &&
	certificate.root_free_rays <= root_free &&
	certificate.root_positive_rays <= root_positive &&
	certificate.incomplete_rays <= incomplete;
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
    total.legacy_capacity_fallback[
	RT_BREP_LEGACY_CAPACITY_FALLBACK_LEAF_WORKSPACE] +=
	trace.fixed_leaf_fallback ? 1 : 0;
    total.legacy_capacity_fallback[
	RT_BREP_LEGACY_CAPACITY_FALLBACK_HIT_WORKSPACE] +=
	trace.fixed_hit_fallback ? 1 : 0;
    for (int action = 0; action < RT_BREP_LEGACY_CLEANUP_COUNT; ++action) {
	total.legacy_cleanup_action_events[action] +=
	    trace.legacy_cleanup_actions[action];
	if (trace.legacy_cleanup_actions[action])
	    total.legacy_cleanup_action_rays[action]++;
    }
    if (trace.legacy_cleanup_action_mismatches)
	total.legacy_cleanup_action_mismatch_rays++;
    total.solver_calls += trace.solver_calls;
    total.manifold_edges += trace.manifold_edges;
    total.prepared_edge_spans += trace.prepared_edge_spans;
    total.candidate_edge_spans += trace.candidate_edge_spans;
    total.candidate_surface_spans += trace.candidate_surface_spans;
    total.surface_subdivision_boxes += trace.surface_subdivision_boxes;
    total.surface_isolated_boxes += trace.surface_isolated_boxes;
    total.surface_fold_attempts += trace.surface_fold_attempts;
    total.surface_fold_certified += trace.surface_fold_complete;
    total.surface_fold_candidates += trace.surface_fold_candidates;
    total.surface_fold_krawczyk_attempts +=
	trace.surface_fold_krawczyk_attempts;
    total.surface_fold_krawczyk_available +=
	trace.surface_fold_krawczyk_available;
    total.surface_fold_krawczyk_certified +=
	trace.surface_fold_krawczyk_certified;
    total.surface_fold_expansion_attempts +=
	trace.surface_fold_expansion_attempts;
    total.surface_fold_expansion_available +=
	trace.surface_fold_expansion_available;
    total.surface_fold_expansion_certified +=
	trace.surface_fold_expansion_certified;
    total.surface_fold_expansion_contraction_attempts +=
	trace.surface_fold_expansion_contraction_attempts;
    total.surface_fold_expansion_contracted +=
	trace.surface_fold_expansion_contracted;
    total.surface_fold_corridor_attempts +=
	trace.surface_fold_corridor_attempts;
    total.surface_fold_corridor_available +=
	trace.surface_fold_corridor_available;
    total.surface_fold_corridor_unique += trace.surface_fold_corridor_unique;
    total.surface_fold_corridor_graph_attempts +=
	trace.surface_fold_corridor_graph_attempts;
    total.surface_fold_corridor_graph_certified +=
	trace.surface_fold_corridor_graph_certified;
    total.surface_fold_corridor_graph_boxes +=
	trace.surface_fold_corridor_graph_boxes;
    total.surface_fold_corridor_graph_interval_attempts +=
	trace.surface_fold_corridor_graph_interval_attempts;
    total.surface_fold_corridor_graph_interval_signed +=
	trace.surface_fold_corridor_graph_interval_signed;
    total.surface_fold_corridor_graph_interval_excluded +=
	trace.surface_fold_corridor_graph_interval_excluded;
    total.surface_fold_corridor_graph_exact_fallbacks +=
	trace.surface_fold_corridor_graph_exact_fallbacks;
    total.surface_fold_corridor_graph_parent_restrictions +=
	trace.surface_fold_corridor_graph_parent_restrictions;
    total.surface_fold_strip_build_attempts +=
	trace.surface_fold_strip_build_attempts;
    total.surface_fold_strip_build_avoided +=
	trace.surface_fold_strip_build_avoided;
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
    total.seeded_verify_attempts += trace.seeded_verify_attempts;
    total.seeded_verify_complete += trace.seeded_verify_complete;
    total.seeded_verify_complete_selected +=
	trace.seeded_verify_complete && trace.prepared_production_selected ? 1 : 0;
    total.seeded_verify_failed_selected +=
	trace.seeded_verify_attempts && !trace.seeded_verify_complete &&
	trace.prepared_production_selected ? 1 : 0;
    total.seeded_verify_complete_mismatch +=
	trace.seeded_verify_complete && trace.root_event_mismatches ? 1 : 0;
    total.seeded_verify_legacy_groups += trace.seeded_verify_legacy_groups;
    total.seeded_verify_candidate_faces +=
	trace.seeded_verify_candidate_faces;
    total.seeded_verify_candidate_spans +=
	trace.seeded_verify_candidate_spans;
    total.seeded_verify_hull_excluded_spans +=
	trace.seeded_verify_hull_excluded_spans;
    total.seeded_verify_mapped_roots += trace.seeded_verify_mapped_roots;
    total.seeded_verify_local_attempts += trace.seeded_verify_local_attempts;
    total.seeded_verify_local_certified += trace.seeded_verify_local_certified;
    total.seeded_verify_degree_attempts += trace.seeded_verify_degree_attempts;
    total.seeded_verify_degree_complete += trace.seeded_verify_degree_complete;
    total.seeded_verify_degree_determinant_failures +=
	trace.seeded_verify_degree_determinant_failures;
    total.seeded_verify_degree_boundary_failures +=
	trace.seeded_verify_degree_boundary_failures;
    total.seeded_verify_degree_orientation_failures +=
	trace.seeded_verify_degree_orientation_failures;
    total.seeded_verify_degree_count_mismatches +=
	trace.seeded_verify_degree_count_mismatches;
    total.seeded_verify_degree_sign_mismatches +=
	trace.seeded_verify_degree_sign_mismatches;
    total.seeded_verify_degree_count_low +=
	trace.seeded_verify_degree_count_low;
    total.seeded_verify_degree_count_high +=
	trace.seeded_verify_degree_count_high;
    total.seeded_verify_degree_mismatch_local_exact +=
	trace.seeded_verify_degree_mismatch_local_exact;
    total.seeded_verify_degree_mismatch_local_inexact +=
	trace.seeded_verify_degree_mismatch_local_inexact;
    total.seeded_verify_degree_extra_local_roots +=
	trace.seeded_verify_degree_extra_local_roots;
    total.seeded_verify_degree_extra_local_trimmed +=
	trace.seeded_verify_degree_extra_local_trimmed;
    total.seeded_verify_degree_extra_local_untrimmed +=
	trace.seeded_verify_degree_extra_local_untrimmed;
    total.seeded_verify_degree_surplus_empty +=
	trace.seeded_verify_degree_surplus_empty;
    total.seeded_verify_degree_surplus_seeded +=
	trace.seeded_verify_degree_surplus_seeded;
    total.seeded_verify_degree_surplus_corrector_attempts +=
	trace.seeded_verify_degree_surplus_corrector_attempts;
    total.seeded_verify_degree_surplus_corrector_converged +=
	trace.seeded_verify_degree_surplus_corrector_converged;
    total.seeded_verify_degree_surplus_local_attempts +=
	trace.seeded_verify_degree_surplus_local_attempts;
    total.seeded_verify_degree_surplus_local_certified +=
	trace.seeded_verify_degree_surplus_local_certified;
    total.seeded_verify_degree_surplus_trimmed_certified +=
	trace.seeded_verify_degree_surplus_trimmed_certified;
    total.seeded_verify_degree_surplus_untrimmed +=
	trace.seeded_verify_degree_surplus_untrimmed;
    total.seeded_verify_degree_surplus_nanoseconds +=
	trace.seeded_verify_degree_surplus_nanoseconds;
    total.seeded_verify_degree_root_count +=
	trace.seeded_verify_degree_root_count;
    total.seeded_verify_degree_seed_count +=
	trace.seeded_verify_degree_seed_count;
    total.seeded_verify_degree_boundary_pieces +=
	trace.seeded_verify_degree_boundary_pieces;
    total.seeded_verify_degree_boundary_splits +=
	trace.seeded_verify_degree_boundary_splits;
    total.seeded_verify_degree_boundary_edges +=
	trace.seeded_verify_degree_boundary_edges;
    total.seeded_verify_degree_expansion_high_water = std::max(
	total.seeded_verify_degree_expansion_high_water,
	trace.seeded_verify_degree_expansion_high_water);
    total.seeded_verify_degree_nanoseconds +=
	trace.seeded_verify_degree_nanoseconds;
    total.seeded_verify_degree_complement_agreements +=
	trace.seeded_verify_degree_complement_agreements;
    total.seeded_verify_degree_complement_disagreements +=
	trace.seeded_verify_degree_complement_disagreements;
    for (int detail = 0; detail < RT_BREP_ROOT_DEGREE_DETAIL_COUNT; ++detail)
	total.seeded_verify_degree_failure_detail[detail] +=
	    trace.seeded_verify_degree_failure_detail[detail];
    total.seeded_verify_complement_visited +=
	trace.seeded_verify_complement_visited;
    total.seeded_verify_complement_high_water = std::max(
	total.seeded_verify_complement_high_water,
	trace.seeded_verify_complement_high_water);
    total.seeded_verify_partition_events +=
	trace.seeded_verify_partition_events;
    total.seeded_verify_nanoseconds += trace.seeded_verify_nanoseconds;
    for (int stage = 0; stage < RT_BREP_SEEDED_VERIFY_STAGE_COUNT; ++stage)
	total.seeded_verify_stage_nanoseconds[stage] +=
	    trace.seeded_verify_stage_nanoseconds[stage];
    if (trace.seeded_verify_attempts && trace.seeded_verify_failure_stage >= 0 &&
	trace.seeded_verify_failure_stage < RT_BREP_SEEDED_VERIFY_STAGE_COUNT) {
	total.seeded_verify_failure_stage[trace.seeded_verify_failure_stage]++;
	if (trace.prepared_production_selected)
	    total.seeded_verify_selected_failure_stage[
		trace.seeded_verify_failure_stage]++;
    }
    if (trace.route_decision_attempts && trace.seeded_route_lane >= 0 &&
	trace.seeded_route_lane < RT_BREP_ROUTE_LANE_COUNT) {
	const int lane = trace.seeded_route_lane;
	total.seeded_route_rays[lane]++;
	total.seeded_route_verifier_complete[lane] +=
	    trace.seeded_verify_complete ? 1 : 0;
	total.seeded_route_prepared_selected[lane] +=
	    trace.prepared_production_selected ? 1 : 0;
	total.seeded_route_root_mismatch[lane] +=
	    trace.root_event_mismatches ? 1 : 0;
	total.seeded_route_capability_mask |=
	    trace.seeded_route_capability_flags;
	total.seeded_route_risk_mask |= trace.seeded_route_risk_flags;
	for (int bit = 0; bit < RT_BREP_ROUTE_CAP_COUNT; ++bit)
	    if (trace.seeded_route_capability_flags & (1u << bit))
		total.seeded_route_capability_rays[bit]++;
	for (int bit = 0; bit < RT_BREP_ROUTE_RISK_COUNT; ++bit)
	    if (trace.seeded_route_risk_flags & (1u << bit))
		total.seeded_route_risk_rays[bit]++;
	for (int bit = 0; bit < RT_BREP_ROUTE_QUAL_COUNT; ++bit)
	    if (trace.route_decision.qualification_flags & (1u << bit))
		total.seeded_route_qualification_rays[bit]++;
	total.route_decision_nanoseconds +=
	    trace.route_decision.decision_nanoseconds;
	total.route_candidate_cells += trace.route_decision.candidate_cells;
	total.route_hazard_roots += trace.route_decision.hazard_roots;
	total.route_contact_roots += trace.route_decision.contact_roots;
	total.route_legacy_transaction_incomplete +=
	    trace.route_decision.legacy_transaction_incomplete;
	total.route_selective_refinement_attempted +=
	    trace.route_decision.selective_refinement_attempted;
	total.route_selective_refinement_complete +=
	    trace.route_decision.selective_refinement_complete;
	total.route_selective_refinement_max_level = std::max(
	    total.route_selective_refinement_max_level,
	    trace.route_decision.selective_refinement_level);
	total.route_selective_refinement_hits +=
	    trace.route_decision.selective_refinement_hits;
	total.route_selective_refinement_reversal_mismatches +=
	    trace.route_decision.selective_refinement_reversal_mismatches;
	total.route_selective_refinement_nanoseconds +=
	    trace.route_decision.selective_refinement_nanoseconds;
	total.route_cell_lookup_failures +=
	    trace.route_decision.cell_lookup_failures;
	for (size_t reason = 0; reason < RT_BREP_ROUTE_LOOKUP_REASON_COUNT;
		reason++)
	    total.route_cell_lookup_failure_reasons[reason] +=
		trace.route_decision.cell_lookup_failure_reasons[reason];
	total.route_roots_unmapped_after_leaf_overflow +=
	    trace.route_decision.roots_unmapped_after_leaf_overflow;
	total.route_empty_correction_cells +=
	    trace.route_decision.empty_correction_cells;
	total.route_missing_root_candidate_spans +=
	    trace.route_decision.missing_root_candidate_spans;
	total.route_missing_root_retained_spans +=
	    trace.route_decision.missing_root_retained_spans;
	total.route_missing_root_restricted_spans +=
	    trace.route_decision.missing_root_restricted_spans;
	total.route_missing_root_inconclusive_spans +=
	    trace.route_decision.missing_root_inconclusive_spans;
	total.route_missing_root_workspace_overflow +=
	    trace.route_decision.missing_root_workspace_overflow;
	total.route_missing_root_signal_rays +=
	    trace.route_decision.missing_root_retained_spans ? 1 : 0;
	total.route_missing_root_prepared_candidate_faces +=
	    trace.route_decision.missing_root_prepared_candidate_faces;
	total.route_missing_root_prepared_candidate_spans +=
	    trace.route_decision.missing_root_prepared_candidate_spans;
	total.route_missing_root_prepared_retained_spans +=
	    trace.route_decision.missing_root_prepared_retained_spans;
	total.route_missing_root_prepared_only_spans +=
	    trace.route_decision.missing_root_prepared_only_spans;
	total.route_missing_root_prepared_only_retained_spans +=
	    trace.route_decision.missing_root_prepared_only_retained_spans;
	total.route_missing_root_prepared_inconclusive_spans +=
	    trace.route_decision.missing_root_prepared_inconclusive_spans;
	total.route_missing_root_prepared_candidate_failures +=
	    trace.route_decision.missing_root_prepared_candidate_failures;
	total.route_missing_root_prepared_signal_rays +=
	    trace.route_decision.missing_root_prepared_retained_spans ? 1 : 0;
	ray_audit_accumulate_route_certificate(
	    total.route_missing_root_certificate, trace.route_decision);
	if (trace.route_decision.missing_root_retained_spans) {
	    if (!trace.prepared_production_selected)
		total.route_missing_root_unresolved_rays++;
	    else if (trace.prepared_production_hits)
		total.route_missing_root_independent_hit_rays++;
	    else
		    total.route_missing_root_independent_empty_rays++;
	}
	if (trace.route_decision.missing_root_prepared_retained_spans) {
	    if (!trace.prepared_production_selected)
		total.route_missing_root_prepared_unresolved_rays++;
	    else if (trace.prepared_production_hits)
		total.route_missing_root_prepared_independent_hit_rays++;
	    else
		total.route_missing_root_prepared_independent_empty_rays++;
	}
	total.route_missing_root_counterfactual_attempts +=
	    trace.missing_root_counterfactual_attempts;
	if (trace.missing_root_counterfactual_attempts) {
	    const struct rt_brep_route_decision &counterfactual =
		trace.missing_root_counterfactual;
	    total.route_missing_root_counterfactual_signal_rays +=
		counterfactual.missing_root_retained_spans ? 1 : 0;
	    total.route_missing_root_counterfactual_unsignaled_rays +=
		counterfactual.missing_root_retained_spans ? 0 : 1;
	    const bool signaled =
		counterfactual.missing_root_retained_spans != 0;
	    const bool prepared_signaled =
		counterfactual.missing_root_prepared_retained_spans != 0;
	    total.route_missing_root_counterfactual_prepared_signal_rays +=
		prepared_signaled ? 1 : 0;
	    total.route_missing_root_counterfactual_prepared_unsignaled_rays +=
		prepared_signaled ? 0 : 1;
	    if (!trace.prepared_production_selected) {
		total.route_missing_root_counterfactual_unresolved_controls++;
	    } else if (trace.prepared_production_hits) {
		total.route_missing_root_counterfactual_true_root_controls++;
		if (signaled)
		    total.route_missing_root_counterfactual_true_root_signals++;
		else
		    total.route_missing_root_counterfactual_true_root_missed++;
		if (prepared_signaled) {
		    total.route_missing_root_counterfactual_true_root_prepared_signals++;
		    if (!signaled)
			total.route_missing_root_counterfactual_true_root_prepared_recovered++;
		} else {
		    total.route_missing_root_counterfactual_true_root_prepared_missed++;
		}
	    } else {
		total.route_missing_root_counterfactual_empty_controls++;
		if (signaled)
		    total.route_missing_root_counterfactual_empty_signals++;
		else
		    total.route_missing_root_counterfactual_empty_rejected++;
		if (prepared_signaled)
		    total.route_missing_root_counterfactual_empty_prepared_signals++;
		else
		    total.route_missing_root_counterfactual_empty_prepared_rejected++;
	    }
	    total.route_missing_root_counterfactual_candidate_spans +=
		counterfactual.missing_root_candidate_spans;
	    total.route_missing_root_counterfactual_retained_spans +=
		counterfactual.missing_root_retained_spans;
	    total.route_missing_root_counterfactual_restricted_spans +=
		counterfactual.missing_root_restricted_spans;
	    total.route_missing_root_counterfactual_inconclusive_spans +=
		counterfactual.missing_root_inconclusive_spans;
	    total.route_missing_root_counterfactual_workspace_overflow +=
		counterfactual.missing_root_workspace_overflow;
	    total.route_missing_root_counterfactual_prepared_candidate_faces +=
		counterfactual.missing_root_prepared_candidate_faces;
	    total.route_missing_root_counterfactual_prepared_candidate_spans +=
		counterfactual.missing_root_prepared_candidate_spans;
	    total.route_missing_root_counterfactual_prepared_retained_spans +=
		counterfactual.missing_root_prepared_retained_spans;
	    total.route_missing_root_counterfactual_prepared_only_spans +=
		counterfactual.missing_root_prepared_only_spans;
	    total.route_missing_root_counterfactual_prepared_only_retained_spans +=
		counterfactual.missing_root_prepared_only_retained_spans;
	    total.route_missing_root_counterfactual_prepared_inconclusive_spans +=
		counterfactual.missing_root_prepared_inconclusive_spans;
	    total.route_missing_root_counterfactual_prepared_candidate_failures +=
		counterfactual.missing_root_prepared_candidate_failures;
	    ray_audit_accumulate_route_certificate(
		total.route_missing_root_counterfactual_certificate,
		counterfactual);
	    total.route_missing_root_counterfactual_nanoseconds +=
		counterfactual.decision_nanoseconds;
	}
    }
    if (trace.fixed_leaf_mismatches || trace.fixed_hit_mismatches ||
	    trace.fixed_cleanup_mismatches ||
	    trace.legacy_cleanup_action_mismatches ||
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
    bool legacy_only, const mat_t transform = NULL)
{
    ray_audit_depth_run run;
    audit_prepared_brep prepared;
    if (!ray_audit_prepare(dbip, dp, depth, tol, prepared, run.prep,
	    transform))
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


static ray_audit_depth_run
ray_audit_run_routed(struct db_i *dbip, struct directory *dp, int depth,
    const struct bn_tol *tol, const std::vector<ray_audit_sample> &samples,
    bool publish_optional, bool inject_legacy_root_fault = false,
    const mat_t transform = NULL)
{
    ray_audit_depth_run run;
    audit_prepared_brep prepared;
    if (!ray_audit_prepare(dbip, dp, depth, tol, prepared, run.prep,
	    transform))
	return run;
    run.shots.reserve(samples.size());
    for (size_t i = 0; i < samples.size(); ++i)
	run.shots.push_back(ray_audit_shoot_routed(prepared, samples[i], tol,
	    publish_optional, inject_legacy_root_fault));
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
ray_audit_publication_equivalent(const ray_audit_sample &sample,
    const ray_audit_shot &published, const ray_audit_shot &expected,
    const struct bn_tol *tol)
{
    double maximum_delta = 0.0;
    size_t face_differences = 0;
    if (!ray_audit_shots_equivalent(sample, published, expected, tol,
	    maximum_delta, face_differences) || face_differences)
	return false;
    const double distance_tolerance = ray_audit_distance_tolerance(sample,
	&published, &expected, tol);
    const double normal_tolerance = std::max(tol ? (double)tol->perp : 0.0,
	65536.0 * DBL_EPSILON);
    for (size_t i = 0; i < published.intervals.size(); ++i) {
	const ray_audit_interval &actual = published.intervals[i];
	const ray_audit_interval &wanted = expected.intervals[i];
	if (fabs(actual.in_normal_dot - wanted.in_normal_dot) >
		normal_tolerance ||
		fabs(actual.out_normal_dot - wanted.out_normal_dot) >
		normal_tolerance)
	    return false;
	for (int coordinate = 0; coordinate < 3; ++coordinate) {
	    if (!std::isfinite(actual.in_point[coordinate]) ||
		    !std::isfinite(wanted.in_point[coordinate]) ||
		    !std::isfinite(actual.out_point[coordinate]) ||
		    !std::isfinite(wanted.out_point[coordinate]) ||
		    !std::isfinite(actual.in_normal[coordinate]) ||
		    !std::isfinite(wanted.in_normal[coordinate]) ||
		    !std::isfinite(actual.out_normal[coordinate]) ||
		    !std::isfinite(wanted.out_normal[coordinate]) ||
		    fabs(actual.in_point[coordinate] -
		    wanted.in_point[coordinate]) > distance_tolerance ||
		    fabs(actual.out_point[coordinate] -
		    wanted.out_point[coordinate]) > distance_tolerance ||
		    fabs(actual.in_normal[coordinate] -
		    wanted.in_normal[coordinate]) > normal_tolerance ||
		    fabs(actual.out_normal[coordinate] -
		    wanted.out_normal[coordinate]) > normal_tolerance)
		return false;
	}
	for (int coordinate = 0; coordinate < 2; ++coordinate) {
	    if (!std::isfinite(actual.in_uv[coordinate]) ||
		    !std::isfinite(wanted.in_uv[coordinate]) ||
		    !std::isfinite(actual.out_uv[coordinate]) ||
		    !std::isfinite(wanted.out_uv[coordinate]))
		return false;
	    const double in_scale = std::max(1.0, std::max(
		fabs(actual.in_uv[coordinate]),
		fabs(wanted.in_uv[coordinate])));
	    const double out_scale = std::max(1.0, std::max(
		fabs(actual.out_uv[coordinate]),
		fabs(wanted.out_uv[coordinate])));
	    if (fabs(actual.in_uv[coordinate] - wanted.in_uv[coordinate]) >
		    65536.0 * DBL_EPSILON * in_scale ||
		    fabs(actual.out_uv[coordinate] - wanted.out_uv[coordinate]) >
		    65536.0 * DBL_EPSILON * out_scale)
		return false;
	}
    }
    return true;
}


static void
ray_audit_store_example(std::vector<size_t> &examples, size_t index);


static ray_audit_publication
ray_audit_check_publication(const std::vector<ray_audit_sample> &samples,
    const ray_audit_depth_run &prepared, const ray_audit_depth_run &legacy,
    const ray_audit_depth_run &published, const struct bn_tol *tol)
{
    ray_audit_publication result;
    const size_t count = std::min(samples.size(), std::min(prepared.shots.size(),
	std::min(legacy.shots.size(), published.shots.size())));
    result.rays = count;
    for (size_t i = 0; i < count; ++i) {
	const ray_audit_shot &prepared_shot = prepared.shots[i];
	const ray_audit_shot &legacy_shot = legacy.shots[i];
	const ray_audit_shot &published_shot = published.shots[i];
	if (!published_shot.valid)
	    result.invalid_rays++;
	const bool attempted = prepared_shot.route_decision.optional_work ==
	    RT_BREP_ROUTE_WORK_COMPLEX_PREPARED;
	const bool complete = attempted && prepared_shot.prepared_selected;
	const bool eligible = complete &&
	    !(prepared_shot.route_decision.qualification_flags &
		RT_BREP_ROUTE_QUAL_UNSUPPORTED) &&
	    (prepared_shot.route_decision.qualification_flags &
		RT_BREP_ROUTE_QUAL_CORRECTION_RISK);
	result.optional_attempts += attempted ? 1 : 0;
	result.optional_complete += complete ? 1 : 0;
	result.eligible_publications += eligible ? 1 : 0;
	const ray_audit_shot &expected = eligible ? prepared_shot : legacy_shot;
	if (!ray_audit_publication_equivalent(samples[i], published_shot,
		expected, tol)) {
	    result.mismatches++;
	    ray_audit_store_example(result.mismatch_ray_indices, i);
	    continue;
	}
	if (eligible) {
	    result.prepared_matches++;
	    double ignored_delta = 0.0;
	    size_t ignored_faces = 0;
	    if (!ray_audit_shots_equivalent(samples[i], prepared_shot,
		    legacy_shot, tol, ignored_delta, ignored_faces))
		result.published_correction_rays++;
	} else {
	    result.legacy_fallback_matches++;
	}
    }
    if (count != samples.size() || count != prepared.shots.size() ||
	    count != legacy.shots.size() || count != published.shots.size())
	result.mismatches++;
    return result;
}


static ray_audit_missing_root_injection
ray_audit_check_missing_root_injection(
    const std::vector<ray_audit_sample> &samples,
    const ray_audit_depth_run &prepared, const ray_audit_depth_run &legacy,
    const ray_audit_depth_run &direct,
    const ray_audit_depth_run &published, const struct bn_tol *tol)
{
    ray_audit_missing_root_injection result;
    const size_t count = std::min(samples.size(), std::min(
	prepared.shots.size(), std::min(legacy.shots.size(),
	    std::min(direct.shots.size(), published.shots.size()))));
    result.rays = count;
    for (size_t i = 0; i < count; ++i) {
	const ray_audit_shot &prepared_shot = prepared.shots[i];
	const ray_audit_shot &legacy_shot = legacy.shots[i];
	const ray_audit_shot &direct_shot = direct.shots[i];
	const ray_audit_shot &published_shot = published.shots[i];
	bool decision_valid = direct_shot.route_test_available &&
	    published_shot.route_test_available;
	const struct rt_brep_route_test_result &direct_test =
	    direct_shot.route_test;
	const struct rt_brep_route_test_result &published_test =
	    published_shot.route_test;
	const bool fault_injected =
	    direct_test.legacy_root_fault_injected &&
	    published_test.legacy_root_fault_injected;
	const bool matching_fault = fault_injected &&
	    direct_test.suppressed_legacy_raw_hits ==
		published_test.suppressed_legacy_raw_hits &&
	    direct_test.suppressed_legacy_route_roots ==
		published_test.suppressed_legacy_route_roots &&
	    direct_test.suppressed_legacy_hit_overflow ==
		published_test.suppressed_legacy_hit_overflow &&
	    direct_test.suppressed_legacy_root_overflow ==
		published_test.suppressed_legacy_root_overflow &&
	    !direct_test.suppressed_legacy_hit_overflow &&
	    !direct_test.suppressed_legacy_root_overflow;
	if (fault_injected)
	    result.fault_injected++;
	if (direct_test.suppressed_legacy_raw_hits) {
	    result.suppressed_hit_rays++;
	    result.suppressed_raw_hits +=
		direct_test.suppressed_legacy_raw_hits;
	}
	result.suppressed_route_roots +=
	    direct_test.suppressed_legacy_route_roots;
	const bool direct_empty = direct_shot.valid && !direct_shot.hit_count &&
	    direct_shot.intervals.empty();
	if (direct_empty)
	    result.direct_empty_results++;
	const bool fault_exposed = !ray_audit_publication_equivalent(samples[i],
	    direct_shot, legacy_shot, tol);
	if (fault_exposed)
	    result.fault_exposed_rays++;
	const bool triggered =
	    direct_test.decision.missing_root_certificate_triggered != 0;
	decision_valid = decision_valid && matching_fault && direct_empty &&
	    triggered ==
		(published_test.decision.
		 missing_root_certificate_triggered != 0) &&
	    direct_test.decision.missing_root_certificate_associations ==
		published_test.decision.missing_root_certificate_associations &&
	    direct_test.decision.optional_work ==
		published_test.decision.optional_work;

	/* The ordinary trace computes the same root-suppressed decision as
	 * evidence.  When it exists, use it as an independent check that the
	 * production-path injection selected the same certificate result. */
	if (prepared_shot.missing_root_counterfactual_attempts) {
	    const struct rt_brep_route_decision &expected =
		prepared_shot.missing_root_counterfactual;
	    decision_valid = decision_valid &&
		direct_test.decision.missing_root_certificate_triggered ==
		    expected.missing_root_certificate_triggered &&
		direct_test.decision.missing_root_certificate_associations ==
		    expected.missing_root_certificate_associations &&
		direct_test.decision.optional_work == expected.optional_work;
	}

	if (triggered) {
	    result.triggered++;
	    if (fault_exposed)
		result.triggered_faults++;
	    result.optional_attempts += direct_test.optional_attempted ? 1 : 0;
	    result.general_prepared_attempts +=
		direct_test.general_prepared_attempted ? 1 : 0;
	    result.optional_complete += direct_test.optional_complete ? 1 : 0;
	    result.optional_incomplete += direct_test.optional_complete ? 0 : 1;
	    result.published += published_test.optional_published ? 1 : 0;
	    decision_valid = decision_valid &&
		direct_test.decision.optional_work ==
		    RT_BREP_ROUTE_WORK_COMPLEX_PREPARED &&
		direct_test.optional_attempted &&
		direct_test.require_empty_partition == 0 &&
		!direct_test.optional_published &&
		published_test.optional_attempted &&
		published_test.require_empty_partition == 0 &&
		direct_test.optional_complete ==
		    published_test.optional_complete &&
		published_test.optional_published ==
		    published_test.optional_complete;
	} else {
	    decision_valid = decision_valid &&
		!direct_test.optional_attempted &&
		!published_test.optional_attempted &&
		!direct_test.optional_published &&
		!published_test.optional_published;
	}

	if (!decision_valid)
	    result.decision_mismatches++;
	bool transaction_valid = direct_shot.valid && published_shot.valid;
	const bool complete_oracle_agreement =
	    published_test.optional_complete &&
	    ray_audit_publication_equivalent(samples[i], prepared_shot,
		legacy_shot, tol);
	if (complete_oracle_agreement)
	    result.complete_oracle_agreements++;
	if (fault_exposed && complete_oracle_agreement)
	    result.recoverable_faults++;
	const ray_audit_shot &expected_publication =
	    published_test.optional_complete ? prepared_shot : direct_shot;
	if (ray_audit_publication_equivalent(samples[i], published_shot,
		expected_publication, tol)) {
	    if (published_test.optional_complete) {
		result.published_prepared_matches++;
		if (fault_exposed && complete_oracle_agreement)
		    result.recovered_faults++;
	    } else {
		result.published_injected_fallback_matches++;
	    }
	} else {
	    transaction_valid = false;
	}
	if (!transaction_valid || !decision_valid) {
	    result.transaction_mismatches += transaction_valid ? 0 : 1;
	    ray_audit_store_example(result.mismatch_ray_indices, i);
	}
    }
    if (count != samples.size() || count != prepared.shots.size() ||
	    count != legacy.shots.size() || count != direct.shots.size() ||
	    count != published.shots.size())
	result.transaction_mismatches++;
    return result;
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
	<< ",\"bezier_spans\":" << prep.stats.bezier_spans
	<< ",\"packed_surface_controls\":"
	<< prep.stats.packed_surface_controls
	<< ",\"packed_surface_control_values\":"
	<< prep.stats.packed_surface_control_values
	<< ",\"routing_metadata\":{\"capability_mask\":"
	<< prep.stats.routing_capability_flags
	<< ",\"risk_mask\":" << prep.stats.routing_risk_flags
	<< ",\"seeded_faces\":" << prep.stats.routing_seeded_faces
	<< ",\"seeded_spans\":" << prep.stats.routing_seeded_spans
	<< ",\"unqualified_faces\":"
	<< prep.stats.routing_unqualified_faces
	<< ",\"unsupported_faces\":"
	<< prep.stats.routing_unsupported_faces
	<< ",\"reparameterized_faces\":"
	<< prep.stats.routing_reparameterized_faces
	<< ",\"singular_spans\":" << prep.stats.routing_singular_spans
	<< ",\"periodic_faces\":" << prep.stats.routing_periodic_faces
	<< ",\"inner_trim_faces\":"
	<< prep.stats.routing_inner_trim_faces
	<< ",\"boundary_faces\":" << prep.stats.routing_boundary_faces
	<< ",\"nonmanifold_faces\":"
	<< prep.stats.routing_nonmanifold_faces
	<< ",\"seam_faces\":" << prep.stats.routing_seam_faces << "}"
	<< ",\"routing_cells\":{\"total\":"
	<< prep.stats.routing_feature_cells
	<< ",\"seeded\":" << prep.stats.routing_seeded_cells
	<< ",\"unsupported\":" << prep.stats.routing_unsupported_cells
	<< ",\"trim_boundary\":"
	<< prep.stats.routing_trim_boundary_cells
	<< ",\"domain_boundary\":"
	<< prep.stats.routing_domain_boundary_cells
	<< ",\"correction\":"
	<< prep.stats.routing_correction_cells
	<< ",\"span_bindings\":"
	<< prep.stats.routing_feature_cell_span_bindings
	<< ",\"certificate_packs\":"
	<< prep.stats.routing_cell_span_packs
	<< ",\"complete_certificate_packs\":"
	<< prep.stats.routing_complete_cell_span_packs
	<< ",\"certificate_control_intervals\":"
	<< prep.stats.routing_cell_span_control_intervals
	<< ",\"certificate_subpatch_boxes\":"
	<< prep.stats.routing_cell_span_subpatch_boxes
	<< ",\"certificate_pack_budget_bytes\":"
	<< prep.stats.routing_cell_span_pack_budget_bytes
	<< ",\"certificate_pack_used_bytes\":"
	<< prep.stats.routing_cell_span_pack_used_bytes
	<< ",\"direction_cache_bytes\":"
	<< prep.stats.routing_direction_cache_bytes
	<< ",\"direction_cache_capacity\":"
	<< prep.stats.routing_direction_cache_capacity
	<< ",\"direction_cache_ready_entries\":"
	<< prep.stats.routing_direction_cache_ready_entries
	<< ",\"correction_bvh_nodes\":"
	<< prep.stats.routing_correction_bvh_nodes << "}"
	<< ",\"reported_build_seconds\":"
	<< prep.stats.surface_tree_build_microseconds / 1000000.0
	<< ",\"wall_seconds\":" << prep.wall_seconds << "}";
}

static void
print_ray_audit_route_certificate(
    const ray_audit_route_certificate_telemetry &certificate)
{
    static const char *failure_names[RT_BREP_ROUTE_CERT_FAILURE_COUNT] = {
	"none", "input", "ray_frame", "status2_mapping", "singular",
	"overlap", "coefficients", "reparameterization", "determinant",
	"boundary", "orientation", "resource"
    };
    static const char *degree_detail_names[RT_BREP_ROOT_DEGREE_DETAIL_COUNT] = {
	"none", "input", "boundary_piece", "boundary_subdivision",
	"cross_arithmetic", "cross_zero", "orientation"
    };
    std::cout << "{\"associations\":" << certificate.associations
	<< ",\"incomplete\":"
	<< certificate.state[RT_BREP_ROUTE_CERT_INCOMPLETE]
	<< ",\"root_free\":"
	<< certificate.state[RT_BREP_ROUTE_CERT_ROOT_FREE]
	<< ",\"root_positive\":"
	<< certificate.state[RT_BREP_ROUTE_CERT_ROOT_POSITIVE]
	<< ",\"association_regular_root_count\":"
	<< certificate.regular_root_count
	<< ",\"observed_roots\":" << certificate.observed_roots
	<< ",\"deficit_cells\":" << certificate.deficit_cells
	<< ",\"deficit_roots\":" << certificate.deficit_roots
	<< ",\"globally_unique\":false,\"publication_authority\":false"
	<< ",\"accounting_valid\":"
	<< (ray_audit_route_certificate_accounting_valid(certificate) ?
	    "true" : "false")
	<< ",\"cached_bbox_exclusions\":"
	<< certificate.cached_bbox_exclusions
	<< ",\"hull_exclusions\":" << certificate.hull_exclusions
	<< ",\"subdivision_attempts\":" << certificate.subdivision_attempts
	<< ",\"subdivision_boxes\":" << certificate.subdivision_boxes
	<< ",\"subdivision_hull_exclusions\":"
	<< certificate.subdivision_hull_exclusions
	<< ",\"subdivision_exclusions\":"
	<< certificate.subdivision_exclusions
	<< ",\"degree_attempts\":" << certificate.degree_attempts
	<< ",\"degree_complete\":" << certificate.degree_complete
	<< ",\"failure_reasons\":[";
    for (int reason = 0; reason < RT_BREP_ROUTE_CERT_FAILURE_COUNT; ++reason) {
	if (reason)
	    std::cout << ",";
	std::cout << "{\"name\":" << json_quote(failure_names[reason])
	    << ",\"associations\":" << certificate.failure_reason[reason]
	    << "}";
    }
    std::cout << "],\"degree_failure_details\":[";
    for (int detail = 0; detail < RT_BREP_ROOT_DEGREE_DETAIL_COUNT; ++detail) {
	if (detail)
	    std::cout << ",";
	std::cout << "{\"name\":" << json_quote(degree_detail_names[detail])
	    << ",\"associations\":"
	    << certificate.degree_failure_detail[detail] << "}";
    }
    std::cout << "],\"boundary_pieces\":" << certificate.boundary_pieces
	<< ",\"boundary_splits\":" << certificate.boundary_splits
	<< ",\"boundary_edges\":" << certificate.boundary_edges
	<< ",\"expansion_high_water\":"
	<< certificate.expansion_high_water
	<< ",\"complete_rays\":" << certificate.complete_rays
	<< ",\"root_free_rays\":" << certificate.root_free_rays
	<< ",\"root_positive_rays\":" << certificate.root_positive_rays
	<< ",\"incomplete_rays\":" << certificate.incomplete_rays
	<< ",\"elapsed_ns\":" << certificate.nanoseconds
	<< ",\"elapsed_ms\":" << certificate.nanoseconds / 1000000.0
	<< "}";
}


static ray_audit_route_certificate_telemetry
ray_audit_route_certificate_for_decision(
    const struct rt_brep_route_decision &decision)
{
    ray_audit_route_certificate_telemetry certificate;
    ray_audit_accumulate_route_certificate(certificate, decision);
    return certificate;
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
    std::cout << "},\"legacy_capacity_fallbacks\":{";
    for (int reason = 0; reason < RT_BREP_LEGACY_CAPACITY_FALLBACK_COUNT;
	    ++reason) {
	if (reason)
	    std::cout << ",";
	std::cout << json_quote(ray_audit_legacy_capacity_fallback_name(reason))
	    << ":" << total.legacy_capacity_fallback[reason];
    }
    std::cout << "},\"legacy_cleanup\":{";
    for (int action = 0; action < RT_BREP_LEGACY_CLEANUP_COUNT; ++action) {
	if (action)
	    std::cout << ",";
	std::cout << json_quote(ray_audit_legacy_cleanup_action_name(action))
	    << ":{\"rays\":" << total.legacy_cleanup_action_rays[action]
	    << ",\"events\":" << total.legacy_cleanup_action_events[action]
	    << "}";
    }
    std::cout << "},\"legacy_cleanup_action_mismatch_rays\":"
	<< total.legacy_cleanup_action_mismatch_rays
	<< ",\"solver_calls\":" << total.solver_calls
	<< ",\"edges\":{\"manifold\":" << total.manifold_edges
	<< ",\"prepared_spans\":" << total.prepared_edge_spans
	<< ",\"candidate_spans\":" << total.candidate_edge_spans
	<< "}"
	<< ",\"candidate_surface_spans\":"
	<< total.candidate_surface_spans
	<< ",\"surface_subdivision_boxes\":"
	<< total.surface_subdivision_boxes
	<< ",\"surface_isolated_boxes\":" << total.surface_isolated_boxes
	<< ",\"surface_fold_attempts\":" << total.surface_fold_attempts
	<< ",\"surface_fold_certified\":" << total.surface_fold_certified
	<< ",\"fold\":{\"candidates\":" << total.surface_fold_candidates
	<< ",\"binary_krawczyk\":{\"attempts\":"
	<< total.surface_fold_krawczyk_attempts << ",\"available\":"
	<< total.surface_fold_krawczyk_available << ",\"certified\":"
	<< total.surface_fold_krawczyk_certified << "}"
	<< ",\"expansion_krawczyk\":{\"attempts\":"
	<< total.surface_fold_expansion_attempts << ",\"available\":"
	<< total.surface_fold_expansion_available << ",\"certified\":"
	<< total.surface_fold_expansion_certified << ",\"contraction_attempts\":"
	<< total.surface_fold_expansion_contraction_attempts
	<< ",\"contracted\":" << total.surface_fold_expansion_contracted
	<< "}"
	<< ",\"corridors\":{\"attempts\":"
	<< total.surface_fold_corridor_attempts << ",\"available\":"
	<< total.surface_fold_corridor_available << ",\"unique\":"
	<< total.surface_fold_corridor_unique << "}"
	<< ",\"graph\":{\"attempts\":"
	<< total.surface_fold_corridor_graph_attempts << ",\"certified\":"
	<< total.surface_fold_corridor_graph_certified << ",\"boxes\":"
	<< total.surface_fold_corridor_graph_boxes
	<< ",\"interval_attempts\":"
	<< total.surface_fold_corridor_graph_interval_attempts
	<< ",\"interval_signed\":"
	<< total.surface_fold_corridor_graph_interval_signed
	<< ",\"interval_excluded\":"
	<< total.surface_fold_corridor_graph_interval_excluded
	<< ",\"exact_fallbacks\":"
	<< total.surface_fold_corridor_graph_exact_fallbacks
	<< ",\"parent_restrictions\":"
	<< total.surface_fold_corridor_graph_parent_restrictions << "}"
	<< ",\"strip_build\":{\"attempts\":"
	<< total.surface_fold_strip_build_attempts << ",\"avoided\":"
	<< total.surface_fold_strip_build_avoided << "}}"
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
	<< "},\"seeded_verifier\":{\"attempts\":"
	<< total.seeded_verify_attempts << ",\"complete\":"
	<< total.seeded_verify_complete << ",\"complete_selected\":"
	<< total.seeded_verify_complete_selected << ",\"failed_selected\":"
	<< total.seeded_verify_failed_selected << ",\"complete_mismatch\":"
	<< total.seeded_verify_complete_mismatch << ",\"legacy_groups\":"
	<< total.seeded_verify_legacy_groups << ",\"candidate_faces\":"
	<< total.seeded_verify_candidate_faces << ",\"candidate_spans\":"
	<< total.seeded_verify_candidate_spans
	<< ",\"hull_excluded_spans\":"
	<< total.seeded_verify_hull_excluded_spans << ",\"mapped_roots\":"
	<< total.seeded_verify_mapped_roots << ",\"local_attempts\":"
	<< total.seeded_verify_local_attempts << ",\"local_certified\":"
	<< total.seeded_verify_local_certified
	<< ",\"degree\":{\"attempts\":"
	<< total.seeded_verify_degree_attempts << ",\"complete\":"
	<< total.seeded_verify_degree_complete
	<< ",\"determinant_failures\":"
	<< total.seeded_verify_degree_determinant_failures
	<< ",\"boundary_failures\":"
	<< total.seeded_verify_degree_boundary_failures
	<< ",\"orientation_failures\":"
	<< total.seeded_verify_degree_orientation_failures
	<< ",\"count_mismatches\":"
	<< total.seeded_verify_degree_count_mismatches
	<< ",\"sign_mismatches\":"
	<< total.seeded_verify_degree_sign_mismatches << ",\"count_low\":"
	<< total.seeded_verify_degree_count_low << ",\"count_high\":"
	<< total.seeded_verify_degree_count_high
	<< ",\"mismatch_local_exact\":"
	<< total.seeded_verify_degree_mismatch_local_exact
	<< ",\"mismatch_local_inexact\":"
	<< total.seeded_verify_degree_mismatch_local_inexact
	<< ",\"extra_local_roots\":"
	<< total.seeded_verify_degree_extra_local_roots
	<< ",\"extra_local_trimmed\":"
	<< total.seeded_verify_degree_extra_local_trimmed
	<< ",\"extra_local_untrimmed\":"
	<< total.seeded_verify_degree_extra_local_untrimmed
	<< ",\"surplus_empty\":"
	<< total.seeded_verify_degree_surplus_empty << ",\"surplus_seeded\":"
	<< total.seeded_verify_degree_surplus_seeded
	<< ",\"surplus_corrector_attempts\":"
	<< total.seeded_verify_degree_surplus_corrector_attempts
	<< ",\"surplus_corrector_converged\":"
	<< total.seeded_verify_degree_surplus_corrector_converged
	<< ",\"surplus_local_attempts\":"
	<< total.seeded_verify_degree_surplus_local_attempts
	<< ",\"surplus_local_certified\":"
	<< total.seeded_verify_degree_surplus_local_certified
	<< ",\"surplus_trimmed_certified\":"
	<< total.seeded_verify_degree_surplus_trimmed_certified
	<< ",\"surplus_untrimmed\":"
	<< total.seeded_verify_degree_surplus_untrimmed
	<< ",\"surplus_elapsed_ms\":"
	<< total.seeded_verify_degree_surplus_nanoseconds / 1000000.0
	<< ",\"root_count\":"
	<< total.seeded_verify_degree_root_count << ",\"seed_count\":"
	<< total.seeded_verify_degree_seed_count << ",\"boundary_pieces\":"
	<< total.seeded_verify_degree_boundary_pieces << ",\"boundary_splits\":"
	<< total.seeded_verify_degree_boundary_splits << ",\"boundary_edges\":"
	<< total.seeded_verify_degree_boundary_edges
	<< ",\"expansion_high_water\":"
	<< total.seeded_verify_degree_expansion_high_water << ",\"elapsed_ms\":"
	<< total.seeded_verify_degree_nanoseconds / 1000000.0
	<< ",\"complement_agreements\":"
	<< total.seeded_verify_degree_complement_agreements
	<< ",\"complement_disagreements\":"
	<< total.seeded_verify_degree_complement_disagreements
	<< ",\"failure_details\":[";
    for (int detail = 0; detail < RT_BREP_ROOT_DEGREE_DETAIL_COUNT; ++detail) {
	if (detail)
	    std::cout << ",";
	std::cout << total.seeded_verify_degree_failure_detail[detail];
    }
    std::cout << "]}"
	<< ",\"complement_visited\":"
	<< total.seeded_verify_complement_visited
	<< ",\"complement_high_water\":"
	<< total.seeded_verify_complement_high_water
	<< ",\"partition_events\":"
	<< total.seeded_verify_partition_events << ",\"elapsed_ms\":"
	<< total.seeded_verify_nanoseconds / 1000000.0
	<< ",\"stage_ms\":[";
    for (int stage = 0; stage < RT_BREP_SEEDED_VERIFY_STAGE_COUNT; ++stage) {
	if (stage)
	    std::cout << ",";
	std::cout << total.seeded_verify_stage_nanoseconds[stage] / 1000000.0;
    }
    std::cout << "]"
	<< ",\"failure_stages\":[";
    for (int stage = 0; stage < RT_BREP_SEEDED_VERIFY_STAGE_COUNT; ++stage) {
	if (stage)
	    std::cout << ",";
	std::cout << total.seeded_verify_failure_stage[stage];
    }
    std::cout << "]"
	<< ",\"selected_failure_stages\":[";
    for (int stage = 0; stage < RT_BREP_SEEDED_VERIFY_STAGE_COUNT; ++stage) {
	if (stage)
	    std::cout << ",";
	std::cout << total.seeded_verify_selected_failure_stage[stage];
    }
    std::cout << "]"
	<< "},\"routing_shadow\":{\"lanes\":[";
    static const char *lane_name[RT_BREP_ROUTE_LANE_COUNT] = {
	"none", "seeded_ordinary", "complex_prepared"
    };
    for (int lane = 0; lane < RT_BREP_ROUTE_LANE_COUNT; ++lane) {
	if (lane)
	    std::cout << ",";
	std::cout << "{\"name\":" << json_quote(lane_name[lane])
	    << ",\"rays\":" << total.seeded_route_rays[lane]
	    << ",\"verifier_complete\":"
	    << total.seeded_route_verifier_complete[lane]
	    << ",\"prepared_selected\":"
	    << total.seeded_route_prepared_selected[lane]
	    << ",\"root_mismatch\":"
	    << total.seeded_route_root_mismatch[lane] << "}";
    }
    static const char *capability_name[RT_BREP_ROUTE_CAP_COUNT] = {
	"packed_controls", "degree_theorem", "seeded_ordinary"
    };
    static const char *risk_name[RT_BREP_ROUTE_RISK_COUNT] = {
	"unqualified", "unsupported", "reparameterized", "singular",
	"periodic", "inner_trim", "topology_boundary", "nonmanifold", "seam",
	"boundary_contact_correction", "directional_grazing"
    };
    std::cout << "],\"capability_mask\":"
	<< total.seeded_route_capability_mask
	<< ",\"risk_mask\":" << total.seeded_route_risk_mask
	<< ",\"capabilities\":[";
    for (int bit = 0; bit < RT_BREP_ROUTE_CAP_COUNT; ++bit) {
	if (bit)
	    std::cout << ",";
	std::cout << "{\"name\":" << json_quote(capability_name[bit])
	    << ",\"rays\":" << total.seeded_route_capability_rays[bit]
	    << "}";
    }
    std::cout << "],\"risks\":[";
    for (int bit = 0; bit < RT_BREP_ROUTE_RISK_COUNT; ++bit) {
	if (bit)
	    std::cout << ",";
	std::cout << "{\"name\":" << json_quote(risk_name[bit])
	    << ",\"rays\":" << total.seeded_route_risk_rays[bit] << "}";
    }
    static const char *qualification_name[RT_BREP_ROUTE_QUAL_COUNT] = {
	"unqualified", "empty", "clean_ordinary", "correction_risk",
	"unsupported", "mixed_hazard"
    };
    static const char *lookup_reason_name[
	RT_BREP_ROUTE_LOOKUP_REASON_COUNT] = {
	"metadata_unavailable", "null_leaf", "nonleaf", "face_index",
	"leaf_ordinal", "record_range", "cell_identity", "unmatched_root"
    };
    std::cout << "],\"decision\":{\"base\":\"legacy\","
	"\"publication_authority\":\"legacy\",\"candidate_cells\":"
	<< total.route_candidate_cells << ",\"hazard_roots\":"
	<< total.route_hazard_roots << ",\"contact_roots\":"
	<< total.route_contact_roots
	<< ",\"legacy_transaction_incomplete\":"
	<< total.route_legacy_transaction_incomplete
	<< ",\"selective_refinement\":{\"attempted\":"
	<< total.route_selective_refinement_attempted
	<< ",\"complete\":" << total.route_selective_refinement_complete
	<< ",\"max_level\":" << total.route_selective_refinement_max_level
	<< ",\"hits\":" << total.route_selective_refinement_hits
	<< ",\"reversal_mismatches\":"
	<< total.route_selective_refinement_reversal_mismatches
	<< ",\"elapsed_ns\":"
	<< total.route_selective_refinement_nanoseconds << "}"
	<< ",\"cell_lookup_failures\":"
	<< total.route_cell_lookup_failures
	<< ",\"cell_lookup_failure_reasons\":[";
    for (size_t reason = 0; reason < RT_BREP_ROUTE_LOOKUP_REASON_COUNT;
	    ++reason) {
	if (reason)
	    std::cout << ",";
	std::cout << "{\"name\":" << json_quote(lookup_reason_name[reason])
	    << ",\"failures\":"
	    << total.route_cell_lookup_failure_reasons[reason] << "}";
    }
    std::cout << "],\"roots_unmapped_after_leaf_overflow\":"
	<< total.route_roots_unmapped_after_leaf_overflow
	<< ",\"missing_root_probe\":{\"empty_correction_cells\":"
	<< total.route_empty_correction_cells
	<< ",\"candidate_spans\":"
	<< total.route_missing_root_candidate_spans
	<< ",\"retained_spans\":"
	<< total.route_missing_root_retained_spans
	<< ",\"restricted_spans\":"
	<< total.route_missing_root_restricted_spans
	<< ",\"inconclusive_spans\":"
	<< total.route_missing_root_inconclusive_spans
	<< ",\"workspace_overflow\":"
	<< total.route_missing_root_workspace_overflow
	<< ",\"signal_rays\":"
	<< total.route_missing_root_signal_rays
	<< ",\"independent_hit_rays\":"
	<< total.route_missing_root_independent_hit_rays
	<< ",\"independent_empty_rays\":"
	<< total.route_missing_root_independent_empty_rays
	<< ",\"unresolved_rays\":"
	<< total.route_missing_root_unresolved_rays
	<< ",\"prepared_candidates\":{\"faces\":"
	<< total.route_missing_root_prepared_candidate_faces
	<< ",\"spans\":"
	<< total.route_missing_root_prepared_candidate_spans
	<< ",\"retained_spans\":"
	<< total.route_missing_root_prepared_retained_spans
	<< ",\"prepared_only_spans\":"
	<< total.route_missing_root_prepared_only_spans
	<< ",\"prepared_only_retained_spans\":"
	<< total.route_missing_root_prepared_only_retained_spans
	<< ",\"inconclusive_spans\":"
	<< total.route_missing_root_prepared_inconclusive_spans
	<< ",\"candidate_failures\":"
	<< total.route_missing_root_prepared_candidate_failures
	<< ",\"signal_rays\":"
	<< total.route_missing_root_prepared_signal_rays
	<< ",\"independent_hit_rays\":"
	<< total.route_missing_root_prepared_independent_hit_rays
	<< ",\"independent_empty_rays\":"
	<< total.route_missing_root_prepared_independent_empty_rays
	<< ",\"unresolved_rays\":"
	<< total.route_missing_root_prepared_unresolved_rays << "}"
	<< ",\"counterfactual\":{\"attempts\":"
	<< total.route_missing_root_counterfactual_attempts
	<< ",\"signal_rays\":"
	<< total.route_missing_root_counterfactual_signal_rays
	<< ",\"unsignaled_rays\":"
	<< total.route_missing_root_counterfactual_unsignaled_rays
	<< ",\"true_root_controls\":"
	<< total.route_missing_root_counterfactual_true_root_controls
	<< ",\"true_root_signals\":"
	<< total.route_missing_root_counterfactual_true_root_signals
	<< ",\"true_root_missed\":"
	<< total.route_missing_root_counterfactual_true_root_missed
	<< ",\"true_root_prepared_signals\":"
	<< total.route_missing_root_counterfactual_true_root_prepared_signals
	<< ",\"true_root_prepared_missed\":"
	<< total.route_missing_root_counterfactual_true_root_prepared_missed
	<< ",\"true_root_prepared_recovered\":"
	<< total.route_missing_root_counterfactual_true_root_prepared_recovered
	<< ",\"empty_controls\":"
	<< total.route_missing_root_counterfactual_empty_controls
	<< ",\"empty_signals\":"
	<< total.route_missing_root_counterfactual_empty_signals
	<< ",\"empty_rejected\":"
	<< total.route_missing_root_counterfactual_empty_rejected
	<< ",\"empty_prepared_signals\":"
	<< total.route_missing_root_counterfactual_empty_prepared_signals
	<< ",\"empty_prepared_rejected\":"
	<< total.route_missing_root_counterfactual_empty_prepared_rejected
	<< ",\"unresolved_controls\":"
	<< total.route_missing_root_counterfactual_unresolved_controls
	<< ",\"candidate_spans\":"
	<< total.route_missing_root_counterfactual_candidate_spans
	<< ",\"retained_spans\":"
	<< total.route_missing_root_counterfactual_retained_spans
	<< ",\"restricted_spans\":"
	<< total.route_missing_root_counterfactual_restricted_spans
	<< ",\"inconclusive_spans\":"
	<< total.route_missing_root_counterfactual_inconclusive_spans
	<< ",\"workspace_overflow\":"
	<< total.route_missing_root_counterfactual_workspace_overflow
	<< ",\"prepared_candidates\":{\"faces\":"
	<< total.route_missing_root_counterfactual_prepared_candidate_faces
	<< ",\"spans\":"
	<< total.route_missing_root_counterfactual_prepared_candidate_spans
	<< ",\"retained_spans\":"
	<< total.route_missing_root_counterfactual_prepared_retained_spans
	<< ",\"prepared_only_spans\":"
	<< total.route_missing_root_counterfactual_prepared_only_spans
	<< ",\"prepared_only_retained_spans\":"
	<< total.route_missing_root_counterfactual_prepared_only_retained_spans
	<< ",\"inconclusive_spans\":"
	<< total.route_missing_root_counterfactual_prepared_inconclusive_spans
	<< ",\"candidate_failures\":"
	<< total.route_missing_root_counterfactual_prepared_candidate_failures
	<< ",\"signal_rays\":"
	<< total.route_missing_root_counterfactual_prepared_signal_rays
	<< ",\"unsignaled_rays\":"
	<< total.route_missing_root_counterfactual_prepared_unsignaled_rays
	<< "},\"certificate\":";
    print_ray_audit_route_certificate(
	total.route_missing_root_counterfactual_certificate);
    std::cout << ",\"elapsed_ms\":"
	<< total.route_missing_root_counterfactual_nanoseconds / 1000000.0
	<< "},\"certificate\":";
    print_ray_audit_route_certificate(total.route_missing_root_certificate);
    std::cout << "}"
	<< ",\"elapsed_ms\":"
	<< total.route_decision_nanoseconds / 1000000.0
	<< ",\"qualifications\":[";
    for (int bit = 0; bit < RT_BREP_ROUTE_QUAL_COUNT; ++bit) {
	if (bit)
	    std::cout << ",";
	std::cout << "{\"name\":" << json_quote(qualification_name[bit])
	    << ",\"rays\":"
	    << total.seeded_route_qualification_rays[bit] << "}";
    }
    std::cout << "]},\"actual_verify_attempts\":"
	<< total.seeded_verify_attempts
	<< ",\"predicted_verify_attempts\":"
	<< total.seeded_route_rays[RT_BREP_ROUTE_SEEDED_ORDINARY_VERIFY]
	<< ",\"predicted_verify_complete\":"
	<< total.seeded_route_verifier_complete[
	    RT_BREP_ROUTE_SEEDED_ORDINARY_VERIFY]
	<< ",\"predicted_verify_failed\":"
	<< total.seeded_route_rays[RT_BREP_ROUTE_SEEDED_ORDINARY_VERIFY] -
	    total.seeded_route_verifier_complete[
		RT_BREP_ROUTE_SEEDED_ORDINARY_VERIFY]
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
	<< ",\"routing_shadow\":{\"lane\":"
	<< json_quote(ray_audit_route_name(shot.seeded_route_lane))
	<< ",\"observation_stratum\":" << json_quote(
	    ray_audit_route_stratum_name(ray_audit_route_stratum_for(shot)))
	<< ",\"verifier_complete\":"
	<< (shot.seeded_verifier_complete ? "true" : "false")
	<< ",\"failure_stage\":" << shot.seeded_verifier_failure_stage
	<< ",\"capability_mask\":" << shot.seeded_route_capability_flags
	<< ",\"risk_mask\":" << shot.seeded_route_risk_flags
	<< ",\"decision\":{\"base\":\"legacy\",\"optional_work\":"
	<< json_quote(ray_audit_route_name(
	    shot.route_decision.optional_work))
	<< ",\"publication_authority\":\"legacy\","
	   "\"qualification_mask\":"
	<< shot.route_decision.qualification_flags
	<< ",\"candidate_cells\":" << shot.route_decision.candidate_cells
	<< ",\"hazard_roots\":" << shot.route_decision.hazard_roots
	<< ",\"contact_roots\":" << shot.route_decision.contact_roots
	<< ",\"legacy_transaction_incomplete\":"
	<< shot.route_decision.legacy_transaction_incomplete
	<< ",\"selective_refinement\":{\"attempted\":"
	<< shot.route_decision.selective_refinement_attempted
	<< ",\"complete\":"
	<< shot.route_decision.selective_refinement_complete
	<< ",\"level\":" << shot.route_decision.selective_refinement_level
	<< ",\"hits\":" << shot.route_decision.selective_refinement_hits
	<< ",\"reversal_mismatches\":"
	<< shot.route_decision.selective_refinement_reversal_mismatches
	<< ",\"elapsed_ns\":"
	<< shot.route_decision.selective_refinement_nanoseconds << "}"
	<< ",\"cell_lookup_failures\":"
	<< shot.route_decision.cell_lookup_failures
	<< ",\"cell_lookup_failure_reasons\":[";
    static const char *lookup_reason_name[
	RT_BREP_ROUTE_LOOKUP_REASON_COUNT] = {
	"metadata_unavailable", "null_leaf", "nonleaf", "face_index",
	"leaf_ordinal", "record_range", "cell_identity", "unmatched_root"
    };
    for (size_t reason = 0; reason < RT_BREP_ROUTE_LOOKUP_REASON_COUNT;
	    ++reason) {
	if (reason)
	    std::cout << ",";
	std::cout << "{\"name\":" << json_quote(lookup_reason_name[reason])
	    << ",\"failures\":"
	    << shot.route_decision.cell_lookup_failure_reasons[reason] << "}";
    }
    std::cout << "],\"roots_unmapped_after_leaf_overflow\":"
	<< shot.route_decision.roots_unmapped_after_leaf_overflow
	<< ",\"directional_grazing\":{\"attempts\":"
	<< shot.route_decision.directional_probe_attempts
	<< ",\"refined_boxes\":"
	<< shot.route_decision.directional_refined_boxes
	<< ",\"continuations\":"
	<< shot.route_decision.directional_continuation_attempts
	<< ",\"continuation_boxes\":"
	<< shot.route_decision.directional_continuation_boxes
	<< ",\"continuation_resolved_spans\":"
	<< shot.route_decision.directional_continuation_resolved_spans
	<< ",\"ambiguous_spans\":"
	<< shot.route_decision.directional_ambiguous_spans
	<< ",\"workspace_overflow\":"
	<< shot.route_decision.directional_workspace_overflow
	<< ",\"triggered\":"
	<< shot.route_decision.directional_grazing_triggered << "}"
	<< ",\"missing_root_probe\":{\"empty_correction_cells\":"
	<< shot.route_decision.empty_correction_cells
	<< ",\"candidate_spans\":"
	<< shot.route_decision.missing_root_candidate_spans
	<< ",\"retained_spans\":"
	<< shot.route_decision.missing_root_retained_spans
	<< ",\"restricted_spans\":"
	<< shot.route_decision.missing_root_restricted_spans
	<< ",\"inconclusive_spans\":"
	<< shot.route_decision.missing_root_inconclusive_spans
	<< ",\"workspace_overflow\":"
	<< shot.route_decision.missing_root_workspace_overflow
	<< ",\"independent_result\":" << json_quote(
	    !shot.route_decision.missing_root_retained_spans ? "not_signaled" :
	    !shot.prepared_selected ? "unresolved" :
	    shot.prepared_production_hits ? "hit" : "empty")
	<< ",\"prepared_candidates\":{\"faces\":"
	<< shot.route_decision.missing_root_prepared_candidate_faces
	<< ",\"spans\":"
	<< shot.route_decision.missing_root_prepared_candidate_spans
	<< ",\"retained_spans\":"
	<< shot.route_decision.missing_root_prepared_retained_spans
	<< ",\"prepared_only_spans\":"
	<< shot.route_decision.missing_root_prepared_only_spans
	<< ",\"prepared_only_retained_spans\":"
	<< shot.route_decision.missing_root_prepared_only_retained_spans
	<< ",\"inconclusive_spans\":"
	<< shot.route_decision.missing_root_prepared_inconclusive_spans
	<< ",\"candidate_failures\":"
	<< shot.route_decision.missing_root_prepared_candidate_failures
	<< ",\"independent_result\":" << json_quote(
	    !shot.route_decision.missing_root_prepared_retained_spans ?
		"not_signaled" : !shot.prepared_selected ? "unresolved" :
		shot.prepared_production_hits ? "hit" : "empty") << "}"
	<< ",\"counterfactual\":{\"attempted\":"
	<< (shot.missing_root_counterfactual_attempts ? "true" : "false")
	<< ",\"candidate_spans\":"
	<< shot.missing_root_counterfactual.missing_root_candidate_spans
	<< ",\"retained_spans\":"
	<< shot.missing_root_counterfactual.missing_root_retained_spans
	<< ",\"restricted_spans\":"
	<< shot.missing_root_counterfactual.missing_root_restricted_spans
	<< ",\"inconclusive_spans\":"
	<< shot.missing_root_counterfactual.missing_root_inconclusive_spans
	<< ",\"workspace_overflow\":"
	<< shot.missing_root_counterfactual.missing_root_workspace_overflow
	<< ",\"prepared_candidates\":{\"faces\":"
	<< shot.missing_root_counterfactual.missing_root_prepared_candidate_faces
	<< ",\"spans\":"
	<< shot.missing_root_counterfactual.missing_root_prepared_candidate_spans
	<< ",\"retained_spans\":"
	<< shot.missing_root_counterfactual.missing_root_prepared_retained_spans
	<< ",\"prepared_only_spans\":"
	<< shot.missing_root_counterfactual.missing_root_prepared_only_spans
	<< ",\"prepared_only_retained_spans\":"
	<< shot.missing_root_counterfactual.
	    missing_root_prepared_only_retained_spans
	<< ",\"inconclusive_spans\":"
	<< shot.missing_root_counterfactual.
	    missing_root_prepared_inconclusive_spans
	<< ",\"candidate_failures\":"
	<< shot.missing_root_counterfactual.
	    missing_root_prepared_candidate_failures << "}"
	<< ",\"independent_result\":" << json_quote(
	    !shot.missing_root_counterfactual_attempts ? "not_attempted" :
	    !shot.prepared_selected ? "unresolved" :
	    shot.prepared_production_hits ? "hit" : "empty")
	<< ",\"certificate\":";
    print_ray_audit_route_certificate(ray_audit_route_certificate_for_decision(
	shot.missing_root_counterfactual));
    std::cout << "},\"certificate\":";
    print_ray_audit_route_certificate(ray_audit_route_certificate_for_decision(
	shot.route_decision));
    std::cout << "}"
	<< ",\"elapsed_ns\":"
	<< shot.route_decision.decision_nanoseconds
	<< "}"
	<< "}"
	<< ",\"fallback\":" << json_quote(
	    ray_audit_fallback_name(shot.fallback))
	<< ",\"prepared_certificate\":{\"hits\":"
	<< shot.prepared_production_hits
	<< ",\"physical_complete\":" << shot.physical_event_complete
	<< ",\"physical_unresolved\":" << shot.physical_event_unresolved
	<< ",\"state_failures\":" << shot.physical_event_state_failures
	<< ",\"material_segments\":"
	<< shot.physical_event_material_segments
	<< ",\"subminimum_contacts\":"
	<< shot.physical_event_subminimum_contacts
	<< ",\"tolerance_ambiguous\":"
	<< shot.physical_event_tolerance_ambiguous
	<< ",\"events\":" << shot.stored_physical_events
	<< ",\"surface_box_free_required\":"
	<< (shot.prepared_require_surface_box_free ? "true" : "false")
	<< ",\"selective_class_rejections\":"
	<< shot.prepared_selective_class_rejections
	<< ",\"selective_candidate_surface_spans\":"
	<< shot.prepared_selective_candidate_surface_spans
	<< ",\"selective_subdivision_boxes\":"
	<< shot.prepared_selective_subdivision_boxes
	<< ",\"selective_surface_boxes\":"
	<< shot.prepared_selective_surface_boxes << "}"
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
	<< ",\"mismatches\":" << shot.fixed_cleanup_mismatches
	<< ",\"capacity_fallbacks\":{\"leaf_workspace\":"
	<< shot.fixed_leaf_fallback << ",\"hit_workspace\":"
	<< shot.fixed_hit_fallback << "},\"actions\":{";
    for (int action = 0; action < RT_BREP_LEGACY_CLEANUP_COUNT; ++action) {
	if (action)
	    std::cout << ",";
	std::cout << json_quote(ray_audit_legacy_cleanup_action_name(action))
	    << ":" << shot.legacy_cleanup_actions[action];
    }
    std::cout << "},\"action_mismatches\":"
	<< shot.legacy_cleanup_action_mismatches << "}"
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
	    << ",\"out_normal_dot\":" << interval.out_normal_dot
	    << ",\"in_point\":[" << interval.in_point[X] << ","
	    << interval.in_point[Y] << "," << interval.in_point[Z] << "]"
	    << ",\"out_point\":[" << interval.out_point[X] << ","
	    << interval.out_point[Y] << "," << interval.out_point[Z] << "]"
	    << ",\"in_normal\":[" << interval.in_normal[X] << ","
	    << interval.in_normal[Y] << "," << interval.in_normal[Z] << "]"
	    << ",\"out_normal\":[" << interval.out_normal[X] << ","
	    << interval.out_normal[Y] << "," << interval.out_normal[Z] << "]"
	    << ",\"in_uv\":[" << interval.in_uv[0] << ","
	    << interval.in_uv[1] << "]"
	    << ",\"out_uv\":[" << interval.out_uv[0] << ","
	    << interval.out_uv[1] << "]}";
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
    bool legacy_only, bool routing_truth, bool routing_publication,
    bool routing_missing_root_injection,
    const ray_audit_exact_ray *exact_ray, const mat_t transform)
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
    if (exact_ray)
	selected_pair = 0;
    const size_t generated_random_chords = selected_pair >= 3 &&
	!feature_chords ? std::max(random_chords,
	    (size_t)selected_pair - 2) : random_chords;
    if (load_brep(dbip, dp, &intern, transform) == BRLCAD_OK) {
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
	if (have_bbox && exact_ray) {
	    samples = ray_audit_exact_samples(*exact_ray, bmin, bmax,
		std::max(100.0 * tol.dist, 1.0e-6));
	} else if (have_bbox && solid) {
	    samples = ray_audit_samples(bmin, bmax, generated_random_chords,
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
    size_t sample_kind_pairs[RAY_AUDIT_SAMPLE_KIND_COUNT] = {};
    for (size_t i = 0; i < samples.size(); ++i)
	if (!samples[i].reverse && samples[i].kind >= RAY_AUDIT_AXIS_CHORD &&
		samples[i].kind < RAY_AUDIT_SAMPLE_KIND_COUNT)
	    sample_kind_pairs[samples[i].kind]++;
    ray_audit_depth_run reference;
    ray_audit_depth_run candidate;
    ray_audit_depth_run routing_legacy;
    ray_audit_depth_run routing_published;
    ray_audit_depth_run routing_injected_direct;
    ray_audit_depth_run routing_injected_published;
    if (!samples.empty()) {
	/* Direct primitive prep keeps the two requested depth variants
	 * independently constructed for this comparison; persistent cache
	 * coexistence is covered by the dedicated BRP6 regression. */
	reference = ray_audit_run_depth(dbip, dp, reference_depth, &tol,
	    samples, legacy_only, transform);
	if (reference.prep.ret == BRLCAD_OK)
	    candidate = ray_audit_run_depth(dbip, dp, candidate_depth, &tol,
		samples, legacy_only, transform);
	if (routing_truth && reference.prep.ret == BRLCAD_OK)
	    /* Exercise the actual direct-routing/no-publication lane for the
	     * retained baseline.  A traced legacy shot also runs audit-only
	     * selective replay when route decisions are enabled, so it is not a
	     * valid legacy oracle for naturally incomplete transactions. */
	    routing_legacy = ray_audit_run_routed(dbip, dp, reference_depth,
		&tol, samples, false, false, transform);
	if (routing_publication && routing_legacy.prep.ret == BRLCAD_OK)
	    routing_published = ray_audit_run_routed(dbip, dp, reference_depth,
		&tol, samples, true, false, transform);
	if (routing_missing_root_injection &&
		routing_legacy.prep.ret == BRLCAD_OK) {
	    routing_injected_direct = ray_audit_run_routed(dbip, dp,
		reference_depth, &tol, samples, false, true, transform);
	    routing_injected_published = ray_audit_run_routed(dbip, dp,
		reference_depth, &tol, samples, true, true, transform);
	}
    }
    const ray_audit_comparison comparison =
	(candidate.prep.ret == BRLCAD_OK && reference.prep.ret == BRLCAD_OK) ?
	ray_audit_compare(samples, candidate, reference, &tol) :
	ray_audit_comparison();
    const ray_audit_comparison routing_comparison =
	(routing_truth && routing_legacy.prep.ret == BRLCAD_OK &&
	 reference.prep.ret == BRLCAD_OK) ?
	ray_audit_compare(samples, reference, routing_legacy, &tol) :
	ray_audit_comparison();
    ray_audit_comparison routing_authoritative_comparison;
    std::vector<size_t> routing_difference_pairs;
    std::vector<size_t> routing_authoritative_pairs;
    std::vector<size_t> routing_boundary_contact_oracle_pairs;
    std::vector<size_t> routing_selective_replay_oracle_pairs;
    size_t routing_lane_differences[RT_BREP_ROUTE_LANE_COUNT] = {};
    size_t routing_observation_strata[RAY_AUDIT_ROUTE_STRATUM_COUNT] = {};
    size_t routing_correction_strata[RAY_AUDIT_ROUTE_STRATUM_COUNT] = {};
    size_t routing_correction_shapes[RAY_AUDIT_CORRECTION_SHAPE_COUNT] = {};
    size_t routing_complete_differences = 0;
    size_t routing_boundary_contact_oracle_rays = 0;
    size_t routing_selective_replay_oracle_rays = 0;
    for (size_t ray_index = 0; ray_index < reference.shots.size();
	    ++ray_index)
	routing_observation_strata[ray_audit_route_stratum_for(
	    reference.shots[ray_index])]++;
    const size_t routing_ray_count = std::min(samples.size(),
	std::min(reference.shots.size(), routing_legacy.shots.size()));
    for (size_t ray_index = 0; ray_index < routing_ray_count; ++ray_index) {
	double ignored_maximum_delta = 0.0;
	size_t ignored_face_differences = 0;
	if (ray_audit_shots_equivalent(samples[ray_index],
		reference.shots[ray_index], routing_legacy.shots[ray_index],
		&tol, ignored_maximum_delta, ignored_face_differences))
	    continue;
	routing_difference_pairs.push_back(samples[ray_index].pair_index);
	const ray_audit_shot &shot = reference.shots[ray_index];
	if (!shot.prepared_selected)
	    continue;
	if (shot.seeded_route_lane >= 0 &&
		shot.seeded_route_lane < RT_BREP_ROUTE_LANE_COUNT)
	    routing_lane_differences[shot.seeded_route_lane]++;
	if (shot.seeded_verifier_complete)
	    routing_complete_differences++;
	routing_correction_strata[ray_audit_route_stratum_for(shot)]++;
	routing_correction_shapes[ray_audit_correction_shape_for(shot,
	    routing_legacy.shots[ray_index])]++;
	routing_authoritative_comparison.differing_rays++;
	ray_audit_store_example(
	    routing_authoritative_comparison.differing_ray_indices, ray_index);
	routing_authoritative_pairs.push_back(samples[ray_index].pair_index);
	if (ray_audit_certified_boundary_contact_miss(shot,
		    routing_legacy.shots[ray_index], &tol)) {
	    routing_boundary_contact_oracle_rays++;
	    routing_boundary_contact_oracle_pairs.push_back(
		samples[ray_index].pair_index);
	} else if (ray_audit_certified_selective_replay(shot)) {
	    routing_selective_replay_oracle_rays++;
	    routing_selective_replay_oracle_pairs.push_back(
		samples[ray_index].pair_index);
	}
    }
    std::sort(routing_difference_pairs.begin(),
	routing_difference_pairs.end());
    routing_difference_pairs.erase(std::unique(routing_difference_pairs.begin(),
	routing_difference_pairs.end()), routing_difference_pairs.end());
    std::sort(routing_authoritative_pairs.begin(),
	routing_authoritative_pairs.end());
    routing_authoritative_pairs.erase(std::unique(
	routing_authoritative_pairs.begin(), routing_authoritative_pairs.end()),
	routing_authoritative_pairs.end());
    std::sort(routing_boundary_contact_oracle_pairs.begin(),
	routing_boundary_contact_oracle_pairs.end());
    routing_boundary_contact_oracle_pairs.erase(std::unique(
	routing_boundary_contact_oracle_pairs.begin(),
	    routing_boundary_contact_oracle_pairs.end()),
	routing_boundary_contact_oracle_pairs.end());
    std::sort(routing_selective_replay_oracle_pairs.begin(),
	routing_selective_replay_oracle_pairs.end());
    routing_selective_replay_oracle_pairs.erase(std::unique(
	routing_selective_replay_oracle_pairs.begin(),
	    routing_selective_replay_oracle_pairs.end()),
	routing_selective_replay_oracle_pairs.end());
    std::vector<size_t> routing_oracle_pairs =
	routing_boundary_contact_oracle_pairs;
    routing_oracle_pairs.insert(routing_oracle_pairs.end(),
	routing_selective_replay_oracle_pairs.begin(),
	routing_selective_replay_oracle_pairs.end());
    std::sort(routing_oracle_pairs.begin(), routing_oracle_pairs.end());
    routing_oracle_pairs.erase(std::unique(routing_oracle_pairs.begin(),
	routing_oracle_pairs.end()), routing_oracle_pairs.end());
    const size_t routing_oracle_rays =
	routing_boundary_contact_oracle_rays +
	routing_selective_replay_oracle_rays;
    const ray_audit_publication publication =
	routing_publication && routing_published.prep.ret == BRLCAD_OK ?
	ray_audit_check_publication(samples, reference, routing_legacy,
	    routing_published, &tol) : ray_audit_publication();
    const bool publication_valid = !routing_publication ||
	(routing_published.prep.ret == BRLCAD_OK && !publication.invalid_rays &&
	 !publication.mismatches &&
	 publication.published_correction_rays ==
	    routing_authoritative_comparison.differing_rays &&
	 routing_oracle_rays ==
	    routing_authoritative_comparison.differing_rays);
    const ray_audit_missing_root_injection missing_root_injection =
	routing_missing_root_injection &&
	    routing_injected_direct.prep.ret == BRLCAD_OK &&
	    routing_injected_published.prep.ret == BRLCAD_OK ?
	    ray_audit_check_missing_root_injection(samples, reference,
		routing_legacy, routing_injected_direct,
		routing_injected_published, &tol) :
	    ray_audit_missing_root_injection();
    const bool missing_root_injection_valid =
	!routing_missing_root_injection ||
	(routing_injected_direct.prep.ret == BRLCAD_OK &&
	 routing_injected_published.prep.ret == BRLCAD_OK &&
	 missing_root_injection.rays == samples.size() &&
	 missing_root_injection.fault_injected == samples.size() &&
	 missing_root_injection.suppressed_hit_rays > 0 &&
	 missing_root_injection.suppressed_raw_hits > 0 &&
	 missing_root_injection.suppressed_route_roots > 0 &&
	 missing_root_injection.direct_empty_results == samples.size() &&
	 missing_root_injection.fault_exposed_rays > 0 &&
	 missing_root_injection.triggered > 0 &&
	 missing_root_injection.triggered_faults > 0 &&
	 missing_root_injection.optional_attempts ==
	    missing_root_injection.triggered &&
	 missing_root_injection.general_prepared_attempts ==
	    missing_root_injection.triggered &&
	 missing_root_injection.optional_complete > 0 &&
	 missing_root_injection.published ==
	    missing_root_injection.optional_complete &&
	 missing_root_injection.complete_oracle_agreements > 0 &&
	 missing_root_injection.recoverable_faults > 0 &&
	 missing_root_injection.recovered_faults ==
	    missing_root_injection.recoverable_faults &&
	 missing_root_injection.published_prepared_matches ==
	    missing_root_injection.optional_complete &&
	 missing_root_injection.published_injected_fallback_matches ==
	    samples.size() - missing_root_injection.optional_complete &&
	 !missing_root_injection.decision_mismatches &&
	 !missing_root_injection.transaction_mismatches);
    const bool routing_valid = !routing_truth ||
	(routing_legacy.prep.ret == BRLCAD_OK &&
	 !routing_comparison.candidate_invalid_rays &&
	 !routing_comparison.reference_invalid_rays &&
	 !routing_comparison.candidate_reversal_mismatches &&
	 !routing_comparison.reference_reversal_mismatches && publication_valid &&
	 missing_root_injection_valid);
    const bool pass = solid && have_bbox && face_count > 0 &&
	!samples.empty() && candidate.prep.ret == BRLCAD_OK &&
	reference.prep.ret == BRLCAD_OK && !comparison.differing_rays &&
	!comparison.candidate_invalid_rays &&
	!comparison.reference_invalid_rays &&
	!comparison.candidate_reversal_mismatches &&
	!comparison.reference_reversal_mismatches && routing_valid;
    const char *status = pass ? "pass" : !face_count ? "empty_brep" :
	!solid ? "non_solid" : !have_bbox ? "invalid_bbox" :
	"comparison_failed";

    std::cout << std::setprecision(17)
	<< "{\"format\":\"brlcad-brep-ray-depth-comparison-v1\""
	<< ",\"database\":" << json_quote(db_path)
	<< ",\"object\":" << json_quote(dp->d_namep)
	<< ",\"transform\":";
    if (transform) {
	std::cout << "[";
	for (size_t i = 0; i < ELEMENTS_PER_MAT; ++i) {
	    if (i)
		std::cout << ",";
	    std::cout << transform[i];
	}
	std::cout << "]";
    } else {
	std::cout << "null";
    }
    std::cout
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
	<< ",\"requested_random_chord_pairs\":"
	<< (exact_ray ? 0 : random_chords)
	<< ",\"requested_feature_chord_pairs\":"
	<< (exact_ray ? 0 : feature_chords)
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
	<< ",\"exact_ray_pairs\":"
	<< sample_kind_pairs[RAY_AUDIT_EXACT_CHORD]
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
    if (routing_truth) {
	std::cout << ",\"routing_truth\":{\"valid\":"
	    << (routing_valid ? "true" : "false")
	    << ",\"record_labels\":{\"candidate\":\"prepared\","
	       "\"reference\":\"legacy\"}"
	    << ",\"legacy\":{\"prep\":";
	print_ray_audit_prep(routing_legacy.prep);
	std::cout << ",\"trace\":";
	print_ray_audit_telemetry(routing_legacy.telemetry);
	std::cout << "}"
	    << ",\"differing_rays\":" << routing_comparison.differing_rays
	    << ",\"differing_pairs\":" << routing_difference_pairs.size()
	    << ",\"prepared_invalid_rays\":"
	    << routing_comparison.candidate_invalid_rays
	    << ",\"legacy_invalid_rays\":"
	    << routing_comparison.reference_invalid_rays
	    << ",\"prepared_reversal_mismatches\":"
	    << routing_comparison.candidate_reversal_mismatches
	    << ",\"legacy_reversal_mismatches\":"
	    << routing_comparison.reference_reversal_mismatches
	    << ",\"prepared_reversal_pair_indices\":";
	print_ray_audit_indices(routing_comparison.candidate_reversal_indices);
	std::cout << ",\"legacy_reversal_pair_indices\":";
	print_ray_audit_indices(routing_comparison.reference_reversal_indices);
	std::cout
	    << ",\"authoritative_correction_rays\":"
	    << routing_authoritative_comparison.differing_rays
	    << ",\"authoritative_correction_pairs\":"
	    << routing_authoritative_pairs.size()
	    << ",\"trace_only_differing_rays\":"
	    << routing_comparison.differing_rays -
		routing_authoritative_comparison.differing_rays
	    << ",\"verifier_complete_correction_rays\":"
	    << routing_complete_differences
	    << ",\"oracle\":{\"class\":"
	    << json_quote(routing_selective_replay_oracle_rays ?
		(routing_boundary_contact_oracle_rays ?
		 "certified_mixed" : "certified_selective_replay") :
		"certified_boundary_contact_miss")
	    << ",\"completed_rays\":"
	    << routing_oracle_rays
	    << ",\"completed_pairs\":"
	    << routing_oracle_pairs.size()
	    << ",\"unresolved_correction_rays\":"
	    << routing_authoritative_comparison.differing_rays -
		routing_oracle_rays << "}"
	    << ",\"boundary_contact_oracle_rays\":"
	    << routing_boundary_contact_oracle_rays
	    << ",\"selective_replay_oracle_rays\":"
	    << routing_selective_replay_oracle_rays
	    << ",\"difference_pair_indices\":";
	print_ray_audit_indices(routing_difference_pairs);
	std::cout << ",\"correction_pair_indices\":";
	print_ray_audit_indices(routing_authoritative_pairs);
	std::cout << ",\"oracle_pair_indices\":";
	print_ray_audit_indices(routing_oracle_pairs);
	std::cout << ",\"correction_shapes\":[";
	for (int shape = 0; shape < RAY_AUDIT_CORRECTION_SHAPE_COUNT;
		++shape) {
	    if (shape)
		std::cout << ",";
	    std::cout << "{\"name\":" << json_quote(
		ray_audit_correction_shape_name(
		    (ray_audit_correction_shape)shape))
		<< ",\"rays\":" << routing_correction_shapes[shape] << "}";
	}
	std::cout << "],\"observation_strata\":[";
	for (int stratum = 0; stratum < RAY_AUDIT_ROUTE_STRATUM_COUNT;
		++stratum) {
	    if (stratum)
		std::cout << ",";
	    std::cout << "{\"name\":" << json_quote(
		ray_audit_route_stratum_name(
		    (ray_audit_route_stratum)stratum))
		<< ",\"rays\":" << routing_observation_strata[stratum] << "}";
	}
	std::cout << "],\"correction_strata\":[";
	for (int stratum = 0; stratum < RAY_AUDIT_ROUTE_STRATUM_COUNT;
		++stratum) {
	    if (stratum)
		std::cout << ",";
	    std::cout << "{\"name\":" << json_quote(
		ray_audit_route_stratum_name(
		    (ray_audit_route_stratum)stratum))
		<< ",\"rays\":" << routing_correction_strata[stratum] << "}";
	}
	std::cout << "]"
	    << ",\"missed_correction_rays\":"
	    << routing_lane_differences[RT_BREP_ROUTE_LEGACY_DIRECT]
	    << ",\"lanes\":[";
	for (int lane = 0; lane < RT_BREP_ROUTE_LANE_COUNT; ++lane) {
	    if (lane)
		std::cout << ",";
	    std::cout << "{\"name\":"
		<< json_quote(ray_audit_route_name(lane))
		<< ",\"correction_rays\":"
		<< routing_lane_differences[lane] << "}";
	}
	std::cout << "]";
	if (routing_publication) {
	    std::cout << ",\"publication\":{\"valid\":"
		<< (publication_valid ? "true" : "false")
		<< ",\"rays\":" << publication.rays
		<< ",\"invalid_rays\":" << publication.invalid_rays
		<< ",\"optional_attempts\":" << publication.optional_attempts
		<< ",\"optional_complete\":" << publication.optional_complete
		<< ",\"eligible_publications\":"
		<< publication.eligible_publications
		<< ",\"prepared_matches\":" << publication.prepared_matches
		<< ",\"legacy_fallback_matches\":"
		<< publication.legacy_fallback_matches
		<< ",\"published_correction_rays\":"
		<< publication.published_correction_rays
		<< ",\"mismatches\":" << publication.mismatches
		<< ",\"mismatch_ray_indices\":";
	    print_ray_audit_indices(publication.mismatch_ray_indices);
	    std::cout << "}";
	}
	if (routing_missing_root_injection) {
	    std::cout << ",\"missing_root_injection\":{\"valid\":"
		<< (missing_root_injection_valid ? "true" : "false")
		<< ",\"rays\":" << missing_root_injection.rays
		<< ",\"fault_injected\":"
		<< missing_root_injection.fault_injected
		<< ",\"suppressed_hit_rays\":"
		<< missing_root_injection.suppressed_hit_rays
		<< ",\"suppressed_raw_hits\":"
		<< missing_root_injection.suppressed_raw_hits
		<< ",\"suppressed_route_roots\":"
		<< missing_root_injection.suppressed_route_roots
		<< ",\"direct_empty_results\":"
		<< missing_root_injection.direct_empty_results
		<< ",\"fault_exposed_rays\":"
		<< missing_root_injection.fault_exposed_rays
		<< ",\"triggered_faults\":"
		<< missing_root_injection.triggered_faults
		<< ",\"triggered\":" << missing_root_injection.triggered
		<< ",\"optional_attempts\":"
		<< missing_root_injection.optional_attempts
		<< ",\"general_prepared_attempts\":"
		<< missing_root_injection.general_prepared_attempts
		<< ",\"optional_complete\":"
		<< missing_root_injection.optional_complete
		<< ",\"optional_incomplete\":"
		<< missing_root_injection.optional_incomplete
		<< ",\"published\":" << missing_root_injection.published
		<< ",\"complete_oracle_agreements\":"
		<< missing_root_injection.complete_oracle_agreements
		<< ",\"recoverable_faults\":"
		<< missing_root_injection.recoverable_faults
		<< ",\"recovered_faults\":"
		<< missing_root_injection.recovered_faults
		<< ",\"published_prepared_matches\":"
		<< missing_root_injection.published_prepared_matches
		<< ",\"published_injected_fallback_matches\":"
		<< missing_root_injection.published_injected_fallback_matches
		<< ",\"decision_mismatches\":"
		<< missing_root_injection.decision_mismatches
		<< ",\"transaction_mismatches\":"
		<< missing_root_injection.transaction_mismatches
		<< ",\"mismatch_ray_indices\":";
	    print_ray_audit_indices(
		missing_root_injection.mismatch_ray_indices);
	    std::cout << "}";
	}
	std::cout << ",\"difference_records\":";
	print_ray_audit_failure_records(samples, reference, routing_legacy,
	    routing_comparison, face_components);
	std::cout << ",\"correction_records\":";
	print_ray_audit_failure_records(samples, reference, routing_legacy,
	    routing_authoritative_comparison, face_components);
	std::cout << ",\"selected_pair_records\":";
	ray_audit_comparison selected_routing_record;
	if (selected_pair >= 0)
	    selected_routing_record.candidate_reversal_indices.push_back(
		(size_t)selected_pair);
	print_ray_audit_failure_records(samples, reference, routing_legacy,
	    selected_routing_record, face_components);
	if (routing_publication) {
	    std::cout << ",\"published_selected_pair_records\":";
	    print_ray_audit_failure_records(samples, routing_published,
		routing_legacy, selected_routing_record, face_components);
	}
	std::cout << "}";
    }
    std::cout << ",\"process_peak_rss_bytes\":" << peak_rss_bytes()
	<< "}\n";
    return pass ? 0 : 1;
}

static geom_result
wireframe_result(struct db_i *dbip, struct directory *dp,
	const struct bg_tess_tol *ttol, const struct bn_tol *tol,
	const struct rt_brep_draw_options *options,
	std::vector<image_segment> *segments)
{
    geom_result result;
    struct rt_db_internal intern;
    if (load_brep(dbip, dp, &intern) != BRLCAD_OK) {
	result.issues.push_back("database_internal_load_failed");
	return result;
    }

    struct bu_list vhead;
    BU_LIST_INIT(&vhead);
    struct rt_brep_draw_options active_options = *options;
    active_options.item_status = wire_item_status;
    active_options.item_status_data = &result;
    int64_t start = bu_gettime();
    struct rt_brep_draw_report report = {};
    result.ret = rt_brep_plot_ex(&vhead, &intern, ttol, tol, NULL,
	&active_options, &report);
    result.seconds = (bu_gettime() - start) / 1000000.0;
    result.peak_rss_bytes = peak_rss_bytes();
    result.requested_items = report.requested_edges +
	report.requested_surface_cues;
    result.completed_items = report.completed_edges +
	report.completed_surface_cues + report.approximated_surface_cues;
    result.approximated_items = report.approximated_surface_cues;
    result.memory_approximated_surface_cues =
	report.memory_approximated_surface_cues;
    result.time_approximated_surface_cues =
	report.time_approximated_surface_cues;
    result.failed_items = report.failed_edges +
	report.requested_surface_cues - report.completed_surface_cues -
	report.approximated_surface_cues;
    result.hit_time_limit = report.hit_time_limit;
    result.hit_memory_limit = report.hit_memory_limit;
    result.hit_point_limit = report.hit_point_limit;

    struct bv_vlist *vp;
    for (BU_LIST_FOR(vp, bv_vlist, &vhead)) {
	point_t previous = VINIT_ZERO;
	bool have_previous = false;
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
	    if (segments && cmd == BV_VLIST_LINE_MOVE) {
		VMOVE(previous, p);
		have_previous = true;
	    } else if (segments && cmd == BV_VLIST_LINE_DRAW && have_previous) {
		image_segment segment;
		VMOVE(segment.a, previous);
		VMOVE(segment.b, p);
		segments->push_back(segment);
		VMOVE(previous, p);
	    }
	    if (cmd == BV_VLIST_LINE_DRAW || cmd == BV_VLIST_POLY_DRAW ||
		    cmd == BV_VLIST_POLY_END || cmd == BV_VLIST_TRI_DRAW ||
		    cmd == BV_VLIST_TRI_END || cmd == BV_VLIST_POINT_DRAW)
		result.primitives++;
	}
    }

    if (result.ret == RT_BREP_DRAW_PARTIAL) {
	if (result.failed_items > 0)
	    result.issues.push_back("partial_geometry");
    } else if (result.ret == RT_BREP_DRAW_LIMIT) {
	result.issues.push_back("resource_limit");
    } else if (result.ret != RT_BREP_DRAW_OK) {
	result.issues.push_back("generation_failed");
    }
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
	const struct bg_tess_tol *ttol, const struct bn_tol *tol,
	const struct brep_cdt_fast_options *options, int face_index,
	image_mesh *mesh)
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
    struct brep_cdt_fast_options active_options = *options;
    active_options.face_status = shaded_face_status;
    active_options.face_status_data = &result;
    active_options.face_diagnostic = shaded_face_diagnostic;
    active_options.face_diagnostic_data = &result;
    int64_t start = bu_gettime();
    struct brep_cdt_fast_report report = {};
    result.ret = brep_cdt_fast_ex(&faces, &face_cnt, &normals, &points,
	    &point_cnt, bi->brep, face_index, ttol, tol, &active_options,
	    &report);
    result.seconds = (bu_gettime() - start) / 1000000.0;
    result.peak_rss_bytes = peak_rss_bytes();
    result.requested_items = report.requested_faces;
    result.completed_items = report.completed_faces;
    result.approximated_items = report.approximated_faces;
    result.failed_items = report.failed_faces;
    result.skipped_items = report.skipped_degenerate_faces;
    result.tolerance_skipped_items = report.skipped_tolerance_faces;
    result.hit_time_limit = report.hit_time_limit;
    result.hit_memory_limit = report.hit_memory_limit;
    result.hit_point_limit = report.hit_point_limit;
    result.peak_working_bytes = report.peak_working_bytes;
    result.triangle_budget = report.triangle_budget;
    result.area_converged_faces = report.area_converged_faces;
    result.triangle_budget_limited_faces =
	report.triangle_budget_limited_faces;
    result.boundary_envelope_incomplete_faces =
	report.boundary_envelope_incomplete_faces;
    result.refinement_passes = report.refinement_passes;
    result.refinement_time_limited =
	report.refinement_time_limited != 0;

    if (face_cnt > 0)
	result.primitives = (size_t)face_cnt;
    if (point_cnt > 0)
	result.vertices = (size_t)point_cnt;
    if (mesh && point_cnt > 0 && face_cnt > 0) {
	mesh->points.resize((size_t)point_cnt);
	for (int i = 0; i < point_cnt; ++i)
	    VMOVE(mesh->points[(size_t)i].p, points[i]);
	mesh->faces.assign(faces, faces + (size_t)face_cnt * 3);
    }
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

    if (result.ret == BREP_CDT_FAST_PARTIAL)
	result.issues.push_back("partial_geometry");
    else if (result.ret == BREP_CDT_FAST_LIMIT)
	result.issues.push_back("resource_limit");
    else if (result.ret != BREP_CDT_FAST_OK)
	result.issues.push_back("generation_failed");
    const bool expected_empty = result.ret == BREP_CDT_FAST_OK &&
	result.failed_items == 0 &&
	result.completed_items == result.requested_items &&
	result.completed_items == result.skipped_items +
	result.tolerance_skipped_items;
    if ((!result.vertices || !result.primitives) && !expected_empty)
	result.issues.push_back("empty_geometry");
    if (result.invalid_indices)
	result.issues.push_back("invalid_face_indices");
    if (!result.finite)
	result.issues.push_back("non_finite_coordinates_or_normals");
    if (!result.have_bbox && !expected_empty)
	result.issues.push_back("missing_bbox");

    bu_free(faces, "brep audit faces");
    bu_free(normals, "brep audit normals");
    bu_free(points, "brep audit points");
    rt_db_free_internal(&intern);
    return result;
}

static void
mesh_quality_metrics(geom_result *result, int vertex_count, int face_count,
	const fastf_t *vertices, const int *faces, bool require_closed_links)
{
    if (!result || vertex_count <= 0 || face_count <= 0 || !vertices ||
	    !faces)
	return;
    typedef std::pair<int, int> mesh_edge;
    std::map<mesh_edge, std::vector<int>> edge_faces;
    std::vector<std::vector<int>> vertex_faces((size_t)vertex_count);
    std::vector<std::vector<int>> face_neighbors((size_t)face_count);
    double minimum_angle = std::numeric_limits<double>::infinity();
    double maximum_aspect = 0.0;
    for (int fi = 0; fi < face_count; ++fi) {
	const int *triangle = &faces[3 * fi];
	point_t p[3];
	for (int corner = 0; corner < 3; ++corner) {
	    const int vertex = triangle[corner];
	    if (vertex < 0 || vertex >= vertex_count)
		return;
	    VSET(p[corner], vertices[3 * vertex],
		vertices[3 * vertex + 1], vertices[3 * vertex + 2]);
	    vertex_faces[(size_t)vertex].push_back(fi);
	    const int other = triangle[(corner + 1) % 3];
	    edge_faces[vertex < other ? mesh_edge(vertex, other) :
		mesh_edge(other, vertex)].push_back(fi);
	}
	vect_t ab;
	vect_t ac;
	VSUB2(ab, p[1], p[0]);
	VSUB2(ac, p[2], p[0]);
	vect_t cross;
	VCROSS(cross, ab, ac);
	const double doubled_area = MAGNITUDE(cross);
	double lengths[3] = {
	    DIST_PNT_PNT(p[0], p[1]),
	    DIST_PNT_PNT(p[1], p[2]),
	    DIST_PNT_PNT(p[2], p[0])
	};
	const double longest = std::max(lengths[0],
	    std::max(lengths[1], lengths[2]));
	if (!(longest > 0.0) || doubled_area <=
		64.0 * std::numeric_limits<double>::epsilon() *
		longest * longest) {
	    result->geometric_degenerate_faces++;
	    continue;
	}
	maximum_aspect = std::max(maximum_aspect,
	    longest * longest / doubled_area);
	for (int corner = 0; corner < 3; ++corner) {
	    const double adjacent_a = lengths[corner];
	    const double adjacent_b = lengths[(corner + 2) % 3];
	    const double opposite = lengths[(corner + 1) % 3];
	    double cosine = (adjacent_a * adjacent_a +
		adjacent_b * adjacent_b - opposite * opposite) /
		(2.0 * adjacent_a * adjacent_b);
	    cosine = std::max(-1.0, std::min(1.0, cosine));
	    minimum_angle = std::min(minimum_angle, acos(cosine) *
		180.0 / M_PI);
	}
    }
    result->unique_edges = edge_faces.size();
    result->euler_characteristic = (long long)vertex_count -
	(long long)result->unique_edges + (long long)face_count;
    if (std::isfinite(minimum_angle))
	result->minimum_angle_degrees = minimum_angle;
    if (maximum_aspect > 0.0)
	result->maximum_aspect_ratio = maximum_aspect;

    for (const auto &entry : edge_faces) {
	const std::vector<int> &incident = entry.second;
	for (size_t i = 0; i < incident.size(); ++i) {
	    for (size_t j = i + 1; j < incident.size(); ++j) {
		face_neighbors[(size_t)incident[i]].push_back(incident[j]);
		face_neighbors[(size_t)incident[j]].push_back(incident[i]);
	    }
	}
    }
    std::vector<bool> visited_faces((size_t)face_count, false);
    for (int seed = 0; seed < face_count; ++seed) {
	if (visited_faces[(size_t)seed])
	    continue;
	result->connected_components++;
	std::queue<int> work;
	work.push(seed);
	visited_faces[(size_t)seed] = true;
	while (!work.empty()) {
	    const int current = work.front();
	    work.pop();
	    for (int neighbor : face_neighbors[(size_t)current]) {
		if (!visited_faces[(size_t)neighbor]) {
		    visited_faces[(size_t)neighbor] = true;
		    work.push(neighbor);
		}
	    }
	}
    }

    if (!require_closed_links)
	return;
    for (int vertex = 0; vertex < vertex_count; ++vertex) {
	const std::vector<int> &incident = vertex_faces[(size_t)vertex];
	if (incident.empty())
	    continue;
	const std::set<int> incident_set(incident.begin(), incident.end());
	bool valid_link = true;
	std::set<int> reached;
	std::queue<int> work;
	work.push(incident[0]);
	reached.insert(incident[0]);
	while (!work.empty()) {
	    const int current = work.front();
	    work.pop();
	    const int *triangle = &faces[3 * current];
	    int degree = 0;
	    for (int corner = 0; corner < 3; ++corner) {
		if (triangle[corner] != vertex)
		    continue;
		for (int offset : {1, 2}) {
		    const int other = triangle[(corner + offset) % 3];
		    const mesh_edge edge = vertex < other ?
			mesh_edge(vertex, other) : mesh_edge(other, vertex);
		    const auto edge_entry = edge_faces.find(edge);
		    if (edge_entry == edge_faces.end() ||
			    edge_entry->second.size() != 2)
			continue;
		    const int neighbor = edge_entry->second[0] == current ?
			edge_entry->second[1] : edge_entry->second[0];
		    if (incident_set.find(neighbor) == incident_set.end())
			continue;
		    ++degree;
		    if (reached.insert(neighbor).second)
			work.push(neighbor);
		}
		break;
	    }
	    valid_link = valid_link && degree == 2;
	}
	if (!valid_link || reached.size() != incident_set.size())
	    result->invalid_vertex_links++;
    }
}

static void
quality_failed_faces(struct ON_Brep_CDT_State *state, geom_result *result)
{
    if (!result)
	return;
    const int failed_face_count = ON_Brep_CDT_Failed_Faces(NULL, 0, state);
    if (failed_face_count <= 0)
	return;
    std::vector<int> failed_face_indices((size_t)failed_face_count);
    ON_Brep_CDT_Failed_Faces(failed_face_indices.data(),
	failed_face_count, state);
    result->failed_faces.clear();
    result->face_failures.clear();
    result->omitted_failed_faces = 0;
    for (int failed_face : failed_face_indices)
	capture_index(&result->failed_faces, &result->omitted_failed_faces,
	    failed_face);
    for (int failed_face : failed_face_indices) {
	struct brep_cdt_diagnostic face_diagnostic = {};
	if (ON_Brep_CDT_Face_Diagnostic(&face_diagnostic, failed_face,
		state) != 0)
	    continue;
	face_failure detail;
	detail.face_index = failed_face;
	detail.result = face_diagnostic.result;
	detail.stage = face_diagnostic.stage;
	detail.message = face_diagnostic.message;
	result->face_failures.push_back(detail);
    }
}

static geom_result
quality_result(struct db_i *dbip, struct directory *dp,
	const struct bg_tess_tol *ttol, int face_index,
	long max_face_time_ms,
	const struct brep_cdt_repair_settings *repair_settings)
{
    geom_result result;
    struct rt_db_internal intern;
    if (load_brep(dbip, dp, &intern) != BRLCAD_OK) {
	result.issues.push_back("database_internal_load_failed");
	return result;
    }
    struct rt_brep_internal *bi = (struct rt_brep_internal *)intern.idb_ptr;
    RT_BREP_CK_MAGIC(bi);
    result.requested_items = face_index >= 0 ? 1 : bi->brep->m_F.Count();
    if (face_index >= bi->brep->m_F.Count()) {
	result.issues.push_back("face_index_out_of_range");
	rt_db_free_internal(&intern);
	return result;
    }

    struct ON_Brep_CDT_State *state = ON_Brep_CDT_Create(bi->brep,
	dp->d_namep);
    if (!state) {
	result.issues.push_back("state_creation_failed");
	rt_db_free_internal(&intern);
	return result;
    }
    ON_Brep_CDT_Tol_Set(state, ttol);
    ON_Brep_CDT_Face_Time_Limit_Set(state, max_face_time_ms);
    int selected_face = face_index;
    const int selected_count = face_index >= 0 ? 1 : 0;
    int64_t start = bu_gettime();
    bool memory_exhausted = false;
    try {
	result.ret = ON_Brep_CDT_Tessellate(state, selected_count,
	    selected_count ? &selected_face : NULL);
    } catch (const std::bad_alloc &) {
	/* A pathological face can exhaust the allocator before the library's
	 * point/triangle limits are reached.  Do not let that terminate a
	 * corpus worker; retain a bounded, actionable quality failure instead. */
	memory_exhausted = true;
	result.hit_memory_limit = true;
	result.ret = BREP_CDT_RESULT_REFINEMENT_LIMIT;
	result.diagnostic_result = BREP_CDT_RESULT_REFINEMENT_LIMIT;
	result.diagnostic_stage = BREP_CDT_STAGE_FACE_TRIANGULATION;
	result.diagnostic_face = -1;
	result.diagnostic_message =
	    "rigorous tessellation exhausted available memory";
	result.issues.push_back("resource_limit");
    }

    struct brep_cdt_diagnostic diagnostic = {};
    if (!memory_exhausted && ON_Brep_CDT_Diagnostic(&diagnostic, state) == 0) {
	result.diagnostic_result = diagnostic.result;
	result.diagnostic_stage = diagnostic.stage;
	result.diagnostic_face = diagnostic.face_index;
	result.diagnostic_message = diagnostic.message;
	result.completed_items = diagnostic.completed_faces;
	result.failed_items = diagnostic.failed_faces;
	if (diagnostic.face_index >= 0 && diagnostic.failed_faces > 0)
	    capture_index(&result.failed_faces,
		&result.omitted_failed_faces, diagnostic.face_index);
    }
    if (!memory_exhausted)
	quality_failed_faces(state, &result);

    bool repair_needed = result.ret != 0;
    if (!memory_exhausted && repair_settings && face_index < 0 &&
	    result.ret == 0 && repair_settings->mesh.require_manifold) {
	int *native_faces = NULL;
	int native_face_count = 0;
	fastf_t *native_vertices = NULL;
	int native_vertex_count = 0;
	if (ON_Brep_CDT_Mesh(&native_faces, &native_face_count,
		&native_vertices, &native_vertex_count, NULL, NULL, NULL, NULL,
		state, 0, NULL) == 0) {
	    result.manifold_checked = true;
	    result.manifold_accepted = bg_trimesh_manifold_accepted(
		native_vertex_count, native_face_count, native_vertices,
		native_faces) != 0;
	    repair_needed = !result.manifold_accepted;
	} else {
	    repair_needed = true;
	}
	if (native_faces)
	    bu_free(native_faces, "native Manifold audit faces");
	if (native_vertices)
	    bu_free(native_vertices, "native Manifold audit vertices");
    }

    if (!memory_exhausted && repair_settings && face_index < 0 &&
	    repair_needed) {
	result.repair_attempted = true;
	struct brep_cdt_repair_report repair_report =
	    BREP_CDT_REPAIR_REPORT_INIT;
	struct brep_cdt_repair_settings active_repair_settings =
	    *repair_settings;
	active_repair_settings.provenance = quality_repair_provenance;
	active_repair_settings.provenance_data = &result;
	int repair_result = BREP_CDT_RESULT_REPAIR_FAILED;
	bool repair_memory_exhausted = false;
	try {
	    repair_result = ON_Brep_CDT_Repair(state,
		&active_repair_settings,
		&repair_report);
	} catch (const std::bad_alloc &) {
	    repair_memory_exhausted = true;
	    result.hit_memory_limit = true;
	    result.issues.push_back("resource_limit");
	    result.diagnostic_result = BREP_CDT_RESULT_REFINEMENT_LIMIT;
	    result.diagnostic_stage = BREP_CDT_STAGE_MESH_REPAIR;
	    result.diagnostic_face = -1;
	    result.diagnostic_message =
		"mesh repair exhausted available memory";
	}
	result.repair_succeeded = repair_result == 0;
	result.repair_source_result = repair_report.source_diagnostic.result;
	result.repair_resource_limits = repair_report.resource_limits;
	result.repair_source_stage = repair_report.source_diagnostic.stage;
	result.repair_removed_faces = repair_report.mesh.removed_faces;
	result.repair_added_faces = repair_report.mesh.added_faces;
	result.repair_separated_vertices =
	    repair_report.mesh.separated_vertices;
	result.repair_component_union =
	    repair_report.mesh.component_union_applied != 0;
	result.repair_manifold_normalization =
	    repair_report.mesh.manifold_normalization_applied != 0;
	result.repair_self_intersections_allowed =
	    repair_report.mesh.self_intersections_allowed != 0;
	result.repair_manifold_accepted =
	    repair_report.mesh.manifold_accepted != 0;
	if (repair_settings->mesh.require_manifold) {
	    result.manifold_checked = true;
	    result.manifold_accepted = result.repair_manifold_accepted;
	}
	result.repair_rejected_hole_faces =
	    repair_report.mesh.rejected_hole_faces;
	result.repair_geometric_degenerate_faces =
	    repair_report.mesh.geometric_degenerate_faces;
	result.repair_unmatched_edges = repair_report.mesh.unmatched_edges;
	result.repair_excess_edges = repair_report.mesh.excess_edges;
	result.repair_misoriented_edges = repair_report.mesh.misoriented_edges;
	result.repair_invalid_vertex_links =
	    repair_report.mesh.invalid_vertex_links;
	result.repair_reoriented_faces = repair_report.mesh.reoriented_faces;
	result.repair_changed_faces = repair_report.changed_faces;
	result.repair_deviation_samples = repair_report.deviation_samples;
	result.repair_projection_failures =
	    repair_report.deviation_projection_failures;
	result.repair_untrimmed_samples =
	    repair_report.untrimmed_surface_samples;
	result.repair_input_mesh_samples =
	    repair_report.input_mesh_surface_samples;
	result.repair_fast_attempted_faces =
	    repair_report.fast_fallback_attempted_faces;
	result.repair_fast_used_faces = repair_report.fast_fallback_used_faces;
	result.repair_fast_failed_faces =
	    repair_report.fast_fallback_failed_faces;
	result.repair_fast_triangles = repair_report.fast_fallback_triangles;
	result.repair_fast_constrained_edges =
	    repair_report.fast_fallback_constrained_edges;
	result.repair_fast_constrained_samples =
	    repair_report.fast_fallback_constrained_samples;
	result.repair_added_patch_components =
	    repair_report.added_patch_components;
	result.repair_largest_added_patch_faces =
	    repair_report.largest_added_patch_faces;
	result.repair_largest_added_patch_area =
	    repair_report.largest_added_patch_area;
	result.repair_rigorous_first_attempted =
	    repair_report.rigorous_first_attempted != 0;
	result.repair_rigorous_first_result =
	    repair_report.rigorous_first_result;
	result.repair_rigorous_first_fast_faces =
	    repair_report.rigorous_first_fast_faces;
	result.repair_rigorous_first_constrained_edges =
	    repair_report.rigorous_first_constrained_edges;
	result.repair_rigorous_first_constrained_samples =
	    repair_report.rigorous_first_constrained_samples;
	result.repair_rigorous_first_reference_area =
	    repair_report.rigorous_first_reference_area;
	result.repair_rigorous_first_output_area =
	    repair_report.rigorous_first_output_area;
	result.repair_rigorous_first_area_change_percent =
	    repair_report.rigorous_first_area_change_percent;
	result.repair_approximation_tier =
	    repair_report.approximation_tier;
	result.repair_pullback_retry_attempted =
	    repair_report.pullback_retry_attempted != 0;
	result.repair_pullback_retry_applied =
	    repair_report.pullback_retry_applied != 0;
	result.repair_retained_rigorous_triangles =
	    repair_report.retained_rigorous_triangles;
	result.repair_best_effort_faces = repair_report.best_effort_faces;
	result.repair_best_effort_triangles =
	    repair_report.best_effort_triangles;
	result.repair_best_effort_folded_triangles =
	    repair_report.best_effort_folded_triangles;
	result.repair_best_effort_reference_samples =
	    repair_report.best_effort_reference_samples;
	result.repair_best_effort_reference_failures =
	    repair_report.best_effort_reference_failures;
	result.repair_best_effort_max_deviation =
	    repair_report.max_best_effort_surface_deviation;
	result.repair_boundary_strip_faces =
	    repair_report.boundary_strip_faces;
	result.repair_boundary_strip_triangles =
	    repair_report.boundary_strip_triangles;
	result.repair_boundary_strip_constrained_edges =
	    repair_report.boundary_strip_constrained_edges;
	result.repair_boundary_strip_constrained_samples =
	    repair_report.boundary_strip_constrained_samples;
	result.repair_topological_disk_faces =
	    repair_report.topological_disk_faces;
	result.repair_topological_disk_triangles =
	    repair_report.topological_disk_triangles;
	result.repair_topological_disk_constrained_edges =
	    repair_report.topological_disk_constrained_edges;
	result.repair_topological_disk_constrained_samples =
	    repair_report.topological_disk_constrained_samples;
	result.repair_missing_rigorous_triangles =
	    repair_report.missing_rigorous_triangles;
	result.repair_subdivided_rigorous_triangles =
	    repair_report.subdivided_rigorous_triangles;
	result.repair_replaced_rigorous_components =
	    repair_report.replaced_rigorous_components;
	result.repair_largest_replaced_rigorous_triangles =
	    repair_report.largest_replaced_rigorous_triangles;
	result.repair_largest_replaced_boundary_edges =
	    repair_report.largest_replaced_boundary_edges;
	result.repair_replaced_rigorous_area =
	    repair_report.replaced_rigorous_area;
	result.repair_largest_replaced_rigorous_area =
	    repair_report.largest_replaced_rigorous_area;
	result.repair_full_fast_used =
	    repair_report.full_fast_fallback_used != 0;
	result.repair_relaxed_tessellation_attempted =
	    repair_report.relaxed_tessellation_attempted != 0;
	result.repair_relaxed_tessellation_completed_faces =
	    repair_report.relaxed_tessellation_completed_faces;
	result.repair_bounded_edge_retry_attempted =
	    repair_report.bounded_edge_retry_attempted != 0;
	result.repair_bounded_edge_retry_completed_faces =
	    repair_report.bounded_edge_retry_completed_faces;
	result.repair_adaptive_hole_retry_attempted =
	    repair_report.adaptive_hole_retry_attempted != 0;
	result.repair_adaptive_hole_edges =
	    repair_report.adaptive_hole_edges;
	result.repair_adaptive_hole_area_retry_attempted =
	    repair_report.adaptive_hole_area_retry_attempted != 0;
	result.repair_adaptive_hole_area_percent =
	    repair_report.adaptive_hole_area_percent;
	result.repair_healing = repair_report.healing;
	result.repair_bounded_edge_approximation_edges =
	    repair_report.bounded_edge_approximation_edges;
	result.repair_bounded_edge_approximation_faces =
	    repair_report.bounded_edge_approximation_faces;
	result.repair_max_bounded_edge_deviation =
	    repair_report.max_bounded_edge_deviation;
	result.repair_max_deviation = repair_report.max_surface_deviation;
	result.repair_rms_deviation = repair_report.rms_surface_deviation;
	result.repair_allowed_deviation =
	    repair_report.allowed_surface_deviation;
	result.repair_reference_area = repair_report.reference_area;
	result.repair_reference_area_change_percent =
	    repair_report.reference_area_change_percent;
	result.repair_area_change_percent =
	    repair_report.area_change_percent;
	result.repair_output_area = repair_report.mesh.output_area;
	result.repair_output_volume = repair_report.mesh.output_volume;
	result.repair_coverage_samples = repair_report.coverage_samples;
	result.repair_coverage_failures = repair_report.coverage_failures;
	result.repair_max_coverage_deviation =
	    repair_report.max_coverage_deviation;
	result.repair_rms_coverage_deviation =
	    repair_report.rms_coverage_deviation;
	result.repair_relaxed_fidelity_applied =
	    repair_report.relaxed_fidelity_applied != 0;
	result.repair_relaxed_fidelity_factor =
	    repair_report.relaxed_fidelity_factor;
	result.repair_relaxed_deviation_limit =
	    repair_report.relaxed_surface_deviation_limit;
	result.repair_relaxed_area_limit =
	    repair_report.relaxed_area_change_percent_limit;
	if (repair_result == 0)
	    result.ret = 0;
	else if (result.ret == 0)
	    result.ret = BREP_CDT_RESULT_REPAIR_FAILED;
	if (!repair_memory_exhausted &&
	    ON_Brep_CDT_Diagnostic(&diagnostic, state) == 0) {
	    result.diagnostic_result = diagnostic.result;
	    result.diagnostic_stage = diagnostic.stage;
	    result.diagnostic_face = diagnostic.face_index;
	    result.diagnostic_message = diagnostic.message;
	}
	quality_failed_faces(state, &result);
    }

    const bool tessellated = face_index >= 0 ? result.ret == 1 :
	result.ret == 0;
    int *faces = NULL;
    int face_count = 0;
    fastf_t *vertices = NULL;
    int vertex_count = 0;
    if (tessellated && ON_Brep_CDT_Mesh(&faces, &face_count, &vertices,
	    &vertex_count, NULL, NULL, NULL, NULL, state, selected_count,
	    selected_count ? &selected_face : NULL) < 0) {
	result.issues.push_back("mesh_export_failed");
    }
    result.seconds = (bu_gettime() - start) / 1000000.0;
    result.peak_rss_bytes = peak_rss_bytes();
    result.primitives = face_count > 0 ? (size_t)face_count : 0;
    result.vertices = vertex_count > 0 ? (size_t)vertex_count : 0;

    /* The exported arrays own all data needed by the remaining checks.  Drop
     * the much larger face-local CDT state before allocating independent
     * topology metrics for million-triangle corpus meshes. */
    ON_Brep_CDT_Destroy(state);
    state = NULL;

    for (int i = 0; vertices && i < vertex_count; ++i) {
	point_t point;
	VSET(point, vertices[3 * i], vertices[3 * i + 1],
	    vertices[3 * i + 2]);
	if (!std::isfinite(point[X]) || !std::isfinite(point[Y]) ||
		!std::isfinite(point[Z])) {
	    result.finite = false;
	    continue;
	}
	bbox_add(result.bmin, result.bmax, &result.have_bbox, point);
    }
    for (int i = 0; faces && i < face_count * 3; ++i) {
	if (faces[i] < 0 || faces[i] >= vertex_count)
	    result.invalid_indices++;
    }

    if (!result.invalid_indices && result.finite && faces && vertices)
	mesh_quality_metrics(&result, vertex_count, face_count, vertices,
	    faces, face_index < 0);

    if (face_index < 0 && tessellated && !result.invalid_indices &&
	    result.finite && faces && vertices) {
	struct bg_trimesh_solid_errors errors =
	    BG_TRIMESH_SOLID_ERRORS_INIT_NULL;
	result.solid_checked = true;
	result.solid = bg_trimesh_solid2(vertex_count, face_count, vertices,
	    faces, &errors) == 0;
	result.degenerate_faces = errors.degenerate.count;
	result.unmatched_edges = errors.unmatched.count;
	result.excess_edges = errors.excess.count;
	result.misoriented_edges = errors.misoriented.count;
	bg_free_trimesh_solid_errors(&errors);
    }

    if (!tessellated)
	result.issues.push_back("generation_failed");
    if (!result.vertices || !result.primitives)
	result.issues.push_back("empty_geometry");
    if (result.invalid_indices)
	result.issues.push_back("invalid_face_indices");
    if (!result.finite)
	result.issues.push_back("non_finite_coordinates");
    if (!result.have_bbox)
	result.issues.push_back("missing_bbox");
    if (result.solid_checked && !result.solid)
	result.issues.push_back("non_solid_mesh");
    if (result.manifold_checked && !result.manifold_accepted)
	result.issues.push_back("manifold_rejected");
    if (result.solid_checked && result.invalid_vertex_links)
	result.issues.push_back("invalid_vertex_links");
    if (result.geometric_degenerate_faces)
	result.issues.push_back("geometric_degenerate_faces");

    bu_free(faces, "brep quality audit faces");
    bu_free(vertices, "brep quality audit vertices");
    rt_db_free_internal(&intern);
    return result;
}

static void print_num(double value);
static void print_vec(const fastf_t *v);

static bool
mesh_ray_triangle(const ray_audit_exact_ray &ray, const point_t first,
	const point_t second, const point_t third, mesh_ray_hit &hit)
{
    vect_t edge1, edge2, pvec, tvec, qvec, normal;
    VSUB2(edge1, second, first);
    VSUB2(edge2, third, first);
    VCROSS(pvec, ray.direction, edge2);
    const double determinant = VDOT(edge1, pvec);
    const double determinant_scale = std::max(1.0,
	MAGNITUDE(edge1) * MAGNITUDE(edge2));
    if (!std::isfinite(determinant) ||
	fabs(determinant) <= 128.0 * DBL_EPSILON * determinant_scale)
	return false;

    const double inverse = 1.0 / determinant;
    VSUB2(tvec, ray.origin, first);
    const double first_coordinate = VDOT(tvec, pvec) * inverse;
    const double barycentric_tolerance = 1024.0 * DBL_EPSILON;
    if (!std::isfinite(first_coordinate) ||
	first_coordinate < -barycentric_tolerance ||
	first_coordinate > 1.0 + barycentric_tolerance)
	return false;
    VCROSS(qvec, tvec, edge1);
    const double second_coordinate = VDOT(ray.direction, qvec) * inverse;
    if (!std::isfinite(second_coordinate) ||
	second_coordinate < -barycentric_tolerance ||
	first_coordinate + second_coordinate > 1.0 + barycentric_tolerance)
	return false;
    const double distance = VDOT(edge2, qvec) * inverse;
    if (!std::isfinite(distance) || distance < 0.0)
	return false;

    VCROSS(normal, edge1, edge2);
    const double normal_length = MAGNITUDE(normal);
    if (!(normal_length > SMALL_FASTF) || !std::isfinite(normal_length))
	return false;
    VSCALE(normal, normal, 1.0 / normal_length);
    hit.distance = distance;
    hit.barycentric[0] = first_coordinate;
    hit.barycentric[1] = second_coordinate;
    hit.normal_dot = VDOT(normal, ray.direction);
    return true;
}

static double
torus_implicit_value(const ON_Torus &torus, const ON_3dPoint &point)
{
    const ON_3dVector offset = point - torus.Center();
    ON_3dVector axis = torus.Axis();
    if (!axis.Unitize())
	return std::numeric_limits<double>::quiet_NaN();
    const double axial = offset * axis;
    const double squared = offset * offset;
    const double radial_squared = std::max(0.0,
	squared - axial * axial);
    const double major_squared = torus.MajorRadius() * torus.MajorRadius();
    const double minor_squared = torus.MinorRadius() * torus.MinorRadius();
    const double sum = squared + major_squared - minor_squared;
    return sum * sum - 4.0 * major_squared * radial_squared;
}

static bool
torus_ray_intersections(const ON_Torus &torus,
	const ray_audit_exact_ray &ray, std::vector<torus_ray_root> &roots)
{
    ON_3dVector axis = torus.Axis();
    if (!torus.IsValid() || !axis.Unitize())
	return false;
    vect_t offset, centered;
    VSUB2(offset, ray.origin, torus.Center());
    const double projection = VDOT(offset, ray.direction);
    VJOIN1(centered, offset, -projection, ray.direction);

    const double direction_axis = VDOT(ray.direction, axis);
    const double centered_axis = VDOT(centered, axis);
    const double major_squared = torus.MajorRadius() * torus.MajorRadius();
    const double minor_squared = torus.MinorRadius() * torus.MinorRadius();
    const double q2 = VDOT(ray.direction, ray.direction);
    const double q1 = 2.0 * VDOT(centered, ray.direction);
    const double q0 = VDOT(centered, centered) + major_squared -
	minor_squared;
    const double radial2 = q2 - direction_axis * direction_axis;
    const double radial1 = q1 - 2.0 * centered_axis * direction_axis;
    const double radial0 = VDOT(centered, centered) -
	centered_axis * centered_axis;

    bn_poly_t equation = BN_POLY_INIT_ZERO;
    equation.dgr = 4;
    equation.cf[0] = q2 * q2;
    equation.cf[1] = 2.0 * q2 * q1;
    equation.cf[2] = q1 * q1 + 2.0 * q2 * q0 -
	4.0 * major_squared * radial2;
    equation.cf[3] = 2.0 * q1 * q0 -
	4.0 * major_squared * radial1;
    equation.cf[4] = q0 * q0 - 4.0 * major_squared * radial0;

    bn_complex_t values[4] = {};
    if (!bn_poly_quartic_roots(values, &equation))
	return false;
    for (size_t i = 0; i < 4; ++i) {
	torus_ray_root root;
	root.distance = values[i].re - projection;
	root.imaginary = values[i].im;
	const ON_3dPoint point(ray.origin[X] +
	    root.distance * ray.direction[X], ray.origin[Y] +
	    root.distance * ray.direction[Y], ray.origin[Z] +
	    root.distance * ray.direction[Z]);
	root.residual = torus_implicit_value(torus, point);
	roots.push_back(root);
    }
    std::sort(roots.begin(), roots.end(),
	[](const torus_ray_root &first, const torus_ray_root &second) {
	    return first.distance < second.distance;
	});
    return true;
}

/* A deliberately independent accuracy oracle for difficult grazing rays.
 * The fast display tessellator samples one requested trimmed face without
 * using the raytrace SurfaceTree, prepared solver, or selective replay.  A
 * sequence of decreasing tessellation tolerances can therefore demonstrate
 * convergence of triangle/ray crossings around an exact B-Rep result. */
static int
mesh_ray_query(struct db_i *dbip, const char *db_path, struct directory *dp,
	int face_index, const ray_audit_exact_ray &ray,
	const struct bg_tess_tol *ttol, const struct bn_tol *tol,
	const mat_t transform = NULL)
{
    struct rt_db_internal intern;
    if (!ttol || !tol ||
	load_brep(dbip, dp, &intern, transform) != BRLCAD_OK)
	return 2;
    struct rt_brep_internal *bi =
	(struct rt_brep_internal *)intern.idb_ptr;
    RT_BREP_CK_MAGIC(bi);
    if (face_index < 0 || face_index >= bi->brep->m_F.Count()) {
	std::cerr << "Face index is out of range: " << face_index << "\n";
	rt_db_free_internal(&intern);
	return 2;
    }

    int *faces = NULL;
    int triangle_count = 0;
    vect_t *normals = NULL;
    point_t *points = NULL;
    int point_count = 0;
    const int64_t start = bu_gettime();
    const int tessellation_result = brep_cdt_fast(&faces, &triangle_count,
	&normals, &points, &point_count, bi->brep, face_index, ttol, tol);
    const double seconds = (bu_gettime() - start) / 1000000.0;

    std::vector<mesh_ray_hit> raw_hits;
    const ON_Surface *surface = bi->brep->m_F[face_index].SurfaceOf();
    ON_Torus torus;
    const bool torus_available = surface &&
	surface->IsTorus(&torus, ON_ZERO_TOLERANCE);
    std::vector<torus_ray_root> torus_roots;
    const bool torus_roots_valid = torus_available &&
	torus_ray_intersections(torus, ray, torus_roots);
    bool valid_mesh = tessellation_result == BRLCAD_OK &&
	triangle_count > 0 && point_count > 0 && faces && points;
    for (int triangle = 0; valid_mesh && triangle < triangle_count;
	    ++triangle) {
	const int first = faces[triangle * 3];
	const int second = faces[triangle * 3 + 1];
	const int third = faces[triangle * 3 + 2];
	if (first < 0 || second < 0 || third < 0 ||
		first >= point_count || second >= point_count ||
		third >= point_count) {
	    valid_mesh = false;
	    break;
	}
	mesh_ray_hit hit;
	hit.triangle = triangle;
	if (mesh_ray_triangle(ray, points[first], points[second],
		points[third], hit))
	    raw_hits.push_back(hit);
    }
    std::sort(raw_hits.begin(), raw_hits.end(),
	[](const mesh_ray_hit &first, const mesh_ray_hit &second) {
	    return first.distance < second.distance;
	});
    std::vector<mesh_ray_hit> unique_hits;
    for (size_t i = 0; i < raw_hits.size(); ++i) {
	const double duplicate_tolerance = std::max(1.0e-9,
	    512.0 * DBL_EPSILON * std::max(1.0,
		fabs(raw_hits[i].distance)));
	if (!unique_hits.empty() &&
		fabs(raw_hits[i].distance - unique_hits.back().distance) <=
		duplicate_tolerance)
	    continue;
	unique_hits.push_back(raw_hits[i]);
    }

    std::cout << "{\"format\":\"brlcad-brep-mesh-ray-query-v1\""
	<< ",\"database\":" << json_quote(db_path)
	<< ",\"object\":" << json_quote(dp->d_namep)
	<< ",\"face\":" << face_index
	<< ",\"status\":" << json_quote(valid_mesh ? "ok" : "fail")
	<< ",\"tessellation\":{\"absolute\":";
    print_num(ttol->abs);
    std::cout << ",\"relative\":";
    print_num(ttol->rel);
    std::cout << ",\"normal\":";
    print_num(ttol->norm);
    std::cout << ",\"return_code\":" << tessellation_result
	<< ",\"seconds\":";
    print_num(seconds);
    std::cout << ",\"triangles\":" << std::max(0, triangle_count)
	<< ",\"vertices\":" << std::max(0, point_count) << "}"
	<< ",\"ray\":{\"origin\":";
    print_vec(ray.origin);
    std::cout << ",\"direction\":";
    print_vec(ray.direction);
    std::cout << "},\"analytic_torus\":{\"available\":"
	<< (torus_available ? "true" : "false");
    if (torus_available) {
	const ON_3dPoint center = torus.Center();
	const ON_3dVector axis = torus.Axis();
	point_t center_values;
	vect_t axis_values;
	VSET(center_values, center.x, center.y, center.z);
	VSET(axis_values, axis.x, axis.y, axis.z);
	std::cout << ",\"center\":";
	print_vec(center_values);
	std::cout << ",\"axis\":";
	print_vec(axis_values);
	std::cout << ",\"major_radius\":";
	print_num(torus.MajorRadius());
	std::cout << ",\"minor_radius\":";
	print_num(torus.MinorRadius());
    }
    std::cout << ",\"root_solver_valid\":"
	<< (torus_roots_valid ? "true" : "false") << ",\"roots\":[";
    for (size_t i = 0; i < torus_roots.size(); ++i) {
	if (i)
	    std::cout << ",";
	std::cout << "{\"distance\":";
	print_num(torus_roots[i].distance);
	std::cout << ",\"imaginary\":";
	print_num(torus_roots[i].imaginary);
	std::cout << ",\"residual\":";
	print_num(torus_roots[i].residual);
	std::cout << "}";
    }
    std::cout << "]},\"raw_intersections\":" << raw_hits.size()
	<< ",\"unique_intersections\":[";
    for (size_t i = 0; i < unique_hits.size(); ++i) {
	if (i)
	    std::cout << ",";
	const mesh_ray_hit &hit = unique_hits[i];
	std::cout << "{\"distance\":";
	print_num(hit.distance);
	std::cout << ",\"triangle\":" << hit.triangle
	    << ",\"barycentric\":[";
	print_num(hit.barycentric[0]);
	std::cout << ",";
	print_num(hit.barycentric[1]);
	std::cout << "],\"normal_dot\":";
	print_num(hit.normal_dot);
	std::cout << "}";
    }
    std::cout << "],\"process_peak_rss_bytes\":" << peak_rss_bytes()
	<< "}\n";

    bu_free(faces, "brep audit mesh ray faces");
    bu_free(normals, "brep audit mesh ray normals");
    bu_free(points, "brep audit mesh ray points");
    rt_db_free_internal(&intern);
    return valid_mesh ? 0 : 1;
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
	double ratio_min, double ratio_max, const char *prefix,
	const fastf_t *ref_min = NULL, const fastf_t *ref_max = NULL,
	double extent_tolerance = 0.0)
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
	bool bounded_shortfall = false;
	if (std::isfinite(ratio) && ratio < ratio_min && ref_min && ref_max &&
		extent_tolerance > 0.0 && std::isfinite(extent_tolerance)) {
	    const double lower = std::max(0.0,
		result->bmin[axis] - ref_min[axis]);
	    const double upper = std::max(0.0,
		ref_max[axis] - result->bmax[axis]);
	    bounded_shortfall = lower <= extent_tolerance &&
		upper <= extent_tolerance;
	}
	if (!std::isfinite(ratio) ||
		(ratio < ratio_min && !bounded_shortfall) ||
		ratio > ratio_max) {
	    std::ostringstream issue;
	    issue << prefix << "_bbox_axis_" << "xyz"[axis] << "_ratio_out_of_range";
	    result->issues.push_back(issue.str());
	}
    }
    double gdiag = MAGNITUDE(gdims);
    if (ref_diag > active_tol) {
	double ratio = gdiag / ref_diag;
	bool bounded_shortfall = false;
	if (std::isfinite(ratio) && ratio < ratio_min && ref_min && ref_max &&
		extent_tolerance > 0.0 && std::isfinite(extent_tolerance)) {
	    bounded_shortfall = true;
	    for (int axis = 0; axis < 3; ++axis) {
		const double lower = std::max(0.0,
		    result->bmin[axis] - ref_min[axis]);
		const double upper = std::max(0.0,
		    ref_max[axis] - result->bmax[axis]);
		bounded_shortfall = bounded_shortfall &&
		    lower <= extent_tolerance && upper <= extent_tolerance;
	    }
	}
	if (!std::isfinite(ratio) ||
		(ratio < ratio_min && !bounded_shortfall) ||
		ratio > ratio_max) {
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
print_indices(const std::vector<int> &indices)
{
    std::cout << "[";
    for (size_t i = 0; i < indices.size(); i++) {
	if (i)
	    std::cout << ",";
	std::cout << indices[i];
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
	<< ",\"peak_working_bytes\":" << result.peak_working_bytes
	<< ",\"invalid_indices\":" << result.invalid_indices
	<< ",\"diagnostic\":{\"result\":" << result.diagnostic_result
	<< ",\"stage\":" << result.diagnostic_stage
	<< ",\"face_index\":" << result.diagnostic_face
	<< ",\"message\":"
	<< json_quote(result.diagnostic_message.c_str()) << "}"
	<< ",\"solid_validation\":{\"checked\":"
	<< (result.solid_checked ? "true" : "false")
	<< ",\"solid\":" << (result.solid ? "true" : "false")
	<< ",\"manifold_checked\":"
	<< (result.manifold_checked ? "true" : "false")
	<< ",\"manifold_accepted\":"
	<< (result.manifold_accepted ? "true" : "false")
	<< ",\"degenerate_faces\":" << result.degenerate_faces
	<< ",\"unmatched_edges\":" << result.unmatched_edges
	<< ",\"excess_edges\":" << result.excess_edges
	<< ",\"misoriented_edges\":" << result.misoriented_edges << "}"
	<< ",\"mesh_metrics\":{\"unique_edges\":" << result.unique_edges
	<< ",\"connected_components\":" << result.connected_components
	<< ",\"invalid_vertex_links\":" << result.invalid_vertex_links
	<< ",\"geometric_degenerate_faces\":"
	<< result.geometric_degenerate_faces
	<< ",\"euler_characteristic\":" << result.euler_characteristic
	<< ",\"minimum_angle_degrees\":";
    print_num(result.minimum_angle_degrees);
    std::cout << ",\"maximum_aspect_ratio\":";
    print_num(result.maximum_aspect_ratio);
    std::cout << "},\"repair\":{\"attempted\":"
	<< (result.repair_attempted ? "true" : "false")
	<< ",\"succeeded\":"
	<< (result.repair_succeeded ? "true" : "false")
	<< ",\"source_result\":" << result.repair_source_result
	<< ",\"source_stage\":" << result.repair_source_stage
	<< ",\"removed_faces\":" << result.repair_removed_faces
	<< ",\"added_faces\":" << result.repair_added_faces
	<< ",\"separated_vertices\":"
	<< result.repair_separated_vertices
	<< ",\"component_union_applied\":"
	<< (result.repair_component_union ? "true" : "false")
	<< ",\"manifold_normalization_applied\":"
	<< (result.repair_manifold_normalization ? "true" : "false")
	<< ",\"self_intersections_allowed\":"
	<< (result.repair_self_intersections_allowed ? "true" : "false")
	<< ",\"manifold_accepted\":"
	<< (result.repair_manifold_accepted ? "true" : "false")
	<< ",\"rejected_hole_faces\":"
	<< result.repair_rejected_hole_faces
	<< ",\"geometric_degenerate_faces\":"
	<< result.repair_geometric_degenerate_faces
	<< ",\"unmatched_edges\":" << result.repair_unmatched_edges
	<< ",\"excess_edges\":" << result.repair_excess_edges
	<< ",\"misoriented_edges\":" << result.repair_misoriented_edges
	<< ",\"invalid_vertex_links\":"
	<< result.repair_invalid_vertex_links
	<< ",\"reoriented_faces\":" << result.repair_reoriented_faces
	<< ",\"changed_faces\":" << result.repair_changed_faces
	<< ",\"deviation_samples\":" << result.repair_deviation_samples
	<< ",\"projection_failures\":"
	<< result.repair_projection_failures
	<< ",\"untrimmed_surface_samples\":"
	<< result.repair_untrimmed_samples
	<< ",\"input_mesh_surface_samples\":"
	<< result.repair_input_mesh_samples
	<< ",\"fast_fallback_attempted_faces\":"
	<< result.repair_fast_attempted_faces
	<< ",\"fast_fallback_used_faces\":"
	<< result.repair_fast_used_faces
	<< ",\"fast_fallback_failed_faces\":"
	<< result.repair_fast_failed_faces
	<< ",\"fast_fallback_triangles\":"
	<< result.repair_fast_triangles
	<< ",\"fast_fallback_constrained_edges\":"
	<< result.repair_fast_constrained_edges
	<< ",\"fast_fallback_constrained_samples\":"
	<< result.repair_fast_constrained_samples
	<< ",\"added_patch_components\":"
	<< result.repair_added_patch_components
	<< ",\"largest_added_patch_faces\":"
	<< result.repair_largest_added_patch_faces
	<< ",\"largest_added_patch_area\":"
	<< result.repair_largest_added_patch_area
	<< ",\"rigorous_first_attempted\":"
	<< (result.repair_rigorous_first_attempted ? "true" : "false")
	<< ",\"rigorous_first_result\":"
	<< result.repair_rigorous_first_result
	<< ",\"rigorous_first_fast_faces\":"
	<< result.repair_rigorous_first_fast_faces
	<< ",\"rigorous_first_constrained_edges\":"
	<< result.repair_rigorous_first_constrained_edges
	<< ",\"rigorous_first_constrained_samples\":"
	<< result.repair_rigorous_first_constrained_samples
	<< ",\"rigorous_first_reference_area\":"
	<< result.repair_rigorous_first_reference_area
	<< ",\"rigorous_first_output_area\":"
	<< result.repair_rigorous_first_output_area
	<< ",\"rigorous_first_area_change_percent\":"
	<< result.repair_rigorous_first_area_change_percent
	<< ",\"approximation_tier\":"
	<< result.repair_approximation_tier
	<< ",\"retained_rigorous_triangles\":"
	<< result.repair_retained_rigorous_triangles
	<< ",\"best_effort_faces\":"
	<< result.repair_best_effort_faces
	<< ",\"best_effort_triangles\":"
	<< result.repair_best_effort_triangles
	<< ",\"best_effort_folded_triangles\":"
	<< result.repair_best_effort_folded_triangles
	<< ",\"best_effort_reference_samples\":"
	<< result.repair_best_effort_reference_samples
	<< ",\"best_effort_reference_failures\":"
	<< result.repair_best_effort_reference_failures
	<< ",\"best_effort_max_surface_deviation\":"
	<< result.repair_best_effort_max_deviation
	<< ",\"boundary_strip_faces\":"
	<< result.repair_boundary_strip_faces
	<< ",\"boundary_strip_triangles\":"
	<< result.repair_boundary_strip_triangles
	<< ",\"boundary_strip_constrained_edges\":"
	<< result.repair_boundary_strip_constrained_edges
	<< ",\"boundary_strip_constrained_samples\":"
	<< result.repair_boundary_strip_constrained_samples
	<< ",\"topological_disk_faces\":"
	<< result.repair_topological_disk_faces
	<< ",\"topological_disk_triangles\":"
	<< result.repair_topological_disk_triangles
	<< ",\"topological_disk_constrained_edges\":"
	<< result.repair_topological_disk_constrained_edges
	<< ",\"topological_disk_constrained_samples\":"
	<< result.repair_topological_disk_constrained_samples
	<< ",\"missing_rigorous_triangles\":"
	<< result.repair_missing_rigorous_triangles
	<< ",\"subdivided_rigorous_triangles\":"
	<< result.repair_subdivided_rigorous_triangles
	<< ",\"replaced_rigorous_components\":"
	<< result.repair_replaced_rigorous_components
	<< ",\"largest_replaced_rigorous_triangles\":"
	<< result.repair_largest_replaced_rigorous_triangles
	<< ",\"largest_replaced_boundary_edges\":"
	<< result.repair_largest_replaced_boundary_edges
	<< ",\"replaced_rigorous_area\":"
	<< result.repair_replaced_rigorous_area
	<< ",\"largest_replaced_rigorous_area\":"
	<< result.repair_largest_replaced_rigorous_area
	<< ",\"approximation_faces\":";
    print_indices(result.repair_approximation_faces);
    std::cout << ",\"approximation_edges\":";
    print_indices(result.repair_approximation_edges);
    std::cout
	<< ",\"full_fast_fallback_used\":"
	<< (result.repair_full_fast_used ? "true" : "false")
	<< ",\"relaxed_tessellation_attempted\":"
	<< (result.repair_relaxed_tessellation_attempted ? "true" : "false")
	<< ",\"relaxed_tessellation_completed_faces\":"
	<< result.repair_relaxed_tessellation_completed_faces
	<< ",\"bounded_edge_retry_attempted\":"
	<< (result.repair_bounded_edge_retry_attempted ? "true" : "false")
	<< ",\"resource_limits\":" << result.repair_resource_limits
	<< ",\"bounded_edge_retry_completed_faces\":"
	<< result.repair_bounded_edge_retry_completed_faces
	<< ",\"adaptive_hole_retry_attempted\":"
	<< (result.repair_adaptive_hole_retry_attempted ? "true" : "false")
	<< ",\"adaptive_hole_edges\":"
	<< result.repair_adaptive_hole_edges
	<< ",\"adaptive_hole_area_retry_attempted\":"
	<< (result.repair_adaptive_hole_area_retry_attempted ? "true" :
	    "false")
	<< ",\"adaptive_hole_area_percent\":"
	<< result.repair_adaptive_hole_area_percent
	<< ",\"pullback_retry_attempted\":"
	<< (result.repair_pullback_retry_attempted ? "true" : "false")
	<< ",\"pullback_retry_applied\":"
	<< (result.repair_pullback_retry_applied ? "true" : "false")
	<< ",\"topology_healing\":{\"attempted\":"
	<< (result.repair_healing.attempted ? "true" : "false")
	<< ",\"applied\":" << (result.repair_healing.applied ? "true" : "false")
	<< ",\"limited\":" << (result.repair_healing.limited ? "true" : "false")
	<< ",\"restored_outer_loops\":" << result.repair_healing.restored_outer_loops
	<< ",\"corrected_loop_roles\":" << result.repair_healing.corrected_loop_roles
	<< ",\"reoriented_faces\":" << result.repair_healing.reoriented_faces
	<< ",\"removed_unused_edges\":" << result.repair_healing.removed_unused_edges
	<< ",\"capped_loops\":" << result.repair_healing.capped_loops
	<< ",\"cap_area_bound\":";
    print_num(result.repair_healing.cap_area_bound);
    std::cout << ",\"cap_area_percent\":";
    print_num(result.repair_healing.cap_area_percent);
    std::cout << ",\"max_edge_deviation\":" << result.repair_healing.max_edge_deviation << "}"
	<< ",\"bounded_edge_approximation_edges\":"
	<< result.repair_bounded_edge_approximation_edges
	<< ",\"bounded_edge_approximation_faces\":"
	<< result.repair_bounded_edge_approximation_faces
	<< ",\"max_bounded_edge_deviation\":"
	<< result.repair_max_bounded_edge_deviation
	<< ",\"max_surface_deviation\":";
    print_num(result.repair_max_deviation);
    std::cout << ",\"rms_surface_deviation\":";
    print_num(result.repair_rms_deviation);
    std::cout << ",\"allowed_surface_deviation\":";
    print_num(result.repair_allowed_deviation);
    std::cout << ",\"reference_area\":";
    print_num(result.repair_reference_area);
    std::cout << ",\"reference_area_change_percent\":";
    print_num(result.repair_reference_area_change_percent);
    std::cout << ",\"area_change_percent\":";
    print_num(result.repair_area_change_percent);
    std::cout << ",\"output_area\":";
    print_num(result.repair_output_area);
    std::cout << ",\"output_volume\":";
    print_num(result.repair_output_volume);
    std::cout << ",\"coverage_samples\":"
	<< result.repair_coverage_samples
	<< ",\"coverage_failures\":" << result.repair_coverage_failures
	<< ",\"max_coverage_deviation\":";
    print_num(result.repair_max_coverage_deviation);
    std::cout << ",\"rms_coverage_deviation\":";
    print_num(result.repair_rms_coverage_deviation);
    std::cout << ",\"relaxed_fidelity_applied\":"
	<< (result.repair_relaxed_fidelity_applied ? "true" : "false")
	<< ",\"relaxed_fidelity_factor\":";
    print_num(result.repair_relaxed_fidelity_factor);
    std::cout << ",\"relaxed_surface_deviation_limit\":";
    print_num(result.repair_relaxed_deviation_limit);
    std::cout << ",\"relaxed_area_change_percent_limit\":";
    print_num(result.repair_relaxed_area_limit);
    std::cout << "}"
	<< ",\"requested_items\":" << result.requested_items
	<< ",\"completed_items\":" << result.completed_items
	<< ",\"approximated_items\":" << result.approximated_items
	<< ",\"failed_items\":" << result.failed_items
	<< ",\"skipped_items\":" << result.skipped_items
	<< ",\"tolerance_skipped_items\":"
	<< result.tolerance_skipped_items
	<< ",\"adaptive_quality\":{\"triangle_budget\":"
	<< result.triangle_budget
	<< ",\"area_converged_faces\":"
	<< result.area_converged_faces
	<< ",\"triangle_budget_limited_faces\":"
	<< result.triangle_budget_limited_faces
	<< ",\"boundary_envelope_incomplete_faces\":"
	<< result.boundary_envelope_incomplete_faces
	<< ",\"refinement_passes\":" << result.refinement_passes
	<< ",\"refinement_time_limited\":"
	<< (result.refinement_time_limited ? "true" : "false") << "}"
	<< ",\"wire_approximation\":{\"memory_surface_cues\":"
	<< result.memory_approximated_surface_cues
	<< ",\"time_surface_cues\":"
	<< result.time_approximated_surface_cues << "}"
	<< ",\"failed_faces\":";
    print_indices(result.failed_faces);
    std::cout << ",\"failed_faces_omitted\":"
	<< result.omitted_failed_faces << ",\"failure_details\":[";
    for (size_t i = 0; i < result.face_failures.size(); ++i) {
	if (i)
	    std::cout << ",";
	const face_failure &failure = result.face_failures[i];
	std::cout << "{\"face_index\":" << failure.face_index
	    << ",\"result\":" << failure.result
	    << ",\"stage\":" << failure.stage
	    << ",\"message\":" << json_quote(failure.message.c_str())
	    << "}";
    }
    std::cout << "],\"skipped_faces\":";
    print_indices(result.skipped_faces);
    std::cout << ",\"skipped_faces_omitted\":"
	<< result.omitted_skipped_faces
	<< ",\"tolerance_skipped_faces\":";
    print_indices(result.tolerance_skipped_faces);
    std::cout << ",\"tolerance_skipped_faces_omitted\":"
	<< result.omitted_tolerance_skipped_faces
	<< ",\"approximated_faces\":";
    print_indices(result.approximated_faces);
    std::cout << ",\"approximated_faces_omitted\":"
	<< result.omitted_approximated_faces << ",\"unprocessed_faces\":";
    print_indices(result.unprocessed_faces);
    std::cout << ",\"unprocessed_faces_omitted\":"
	<< result.omitted_unprocessed_faces << ",\"failed_edges\":";
    print_indices(result.failed_edges);
    std::cout << ",\"failed_edges_omitted\":"
	<< result.omitted_failed_edges << ",\"unprocessed_edges\":";
    print_indices(result.unprocessed_edges);
    std::cout << ",\"unprocessed_edges_omitted\":"
	<< result.omitted_unprocessed_edges
	<< ",\"failed_surface_cues\":";
    print_indices(result.failed_surface_cues);
    std::cout << ",\"failed_surface_cues_omitted\":"
	<< result.omitted_failed_surface_cues
	<< ",\"approximated_surface_cues\":";
    print_indices(result.approximated_surface_cues);
    std::cout << ",\"approximated_surface_cues_omitted\":"
	<< result.omitted_approximated_surface_cues
	<< ",\"unprocessed_surface_cues\":";
    print_indices(result.unprocessed_surface_cues);
    std::cout << ",\"unprocessed_surface_cues_omitted\":"
	<< result.omitted_unprocessed_surface_cues
	<< ",\"limits\":{\"time\":" << (result.hit_time_limit ? "true" : "false")
	<< ",\"memory\":" << (result.hit_memory_limit ? "true" : "false")
	<< ",\"points\":" << (result.hit_point_limit ? "true" : "false") << "}"
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

static const char *
parameter_mapping_orientation_name(parameter_mapping_orientation orientation)
{
    switch (orientation) {
	case PARAMETER_MAPPING_DECREASING:
	    return "decreasing";
	case PARAMETER_MAPPING_INCREASING:
	    return "increasing";
	case PARAMETER_MAPPING_CONSTANT:
	    return "constant";
	case PARAMETER_MAPPING_MIXED:
	    return "mixed";
	default:
	    return "unknown";
    }
}

static void
print_parameter_mapping_direction(
    const parameter_mapping_direction_profile &direction)
{
    std::cout << "{\"attempts\":" << direction.attempts
	<< ",\"successes\":" << direction.successes
	<< ",\"domain_contained\":" << direction.domain_contained
	<< ",\"round_trip_successes\":"
	<< direction.round_trip_successes
	<< ",\"round_trip_within_tolerance\":"
	<< direction.round_trip_within_tolerance
	<< ",\"maximum_round_trip_error\":[";
    print_num(direction.maximum_round_trip_error[0]);
    std::cout << ",";
    print_num(direction.maximum_round_trip_error[1]);
    std::cout << "],\"maximum_normalized_round_trip_error\":[";
    print_num(direction.maximum_normalized_round_trip_error[0]);
    std::cout << ",";
    print_num(direction.maximum_normalized_round_trip_error[1]);
    std::cout << "]"
	<< ",\"behavior_complete\":"
	<< (direction.behavior_complete ? "true" : "false")
	<< ",\"separable\":" << (direction.separable ? "true" : "false")
	<< ",\"output_input_axis\":[" << direction.output_input_axis[0]
	<< "," << direction.output_input_axis[1] << "]"
	<< ",\"maximum_cross_axis_error\":[";
    print_num(direction.maximum_cross_axis_error[0]);
    std::cout << ",";
    print_num(direction.maximum_cross_axis_error[1]);
    std::cout << "]"
	<< ",\"monotone\":["
	<< (direction.monotone[0] ? "true" : "false") << ","
	<< (direction.monotone[1] ? "true" : "false") << "]"
	<< ",\"strictly_monotone\":["
	<< (direction.strictly_monotone[0] ? "true" : "false") << ","
	<< (direction.strictly_monotone[1] ? "true" : "false") << "]"
	<< ",\"orientation\":["
	<< json_quote(parameter_mapping_orientation_name(direction.orientation[0]))
	<< ","
	<< json_quote(parameter_mapping_orientation_name(direction.orientation[1]))
	<< "]}";
}

static void
print_surface_representation(const surface_tree_profile &profile)
{
    std::cout << "{\"surface_type\":" << json_quote(profile.surface_type)
	<< ",\"face_reversed\":"
	<< (profile.face_reversed ? "true" : "false")
	<< ",\"face_proxy_transposed\":"
	<< (profile.face_proxy_transposed ? "true" : "false")
	<< ",\"surface_proxy\":{\"present\":"
	<< (profile.surface_proxy ? "true" : "false")
	<< ",\"target_type\":"
	<< json_quote(profile.surface_proxy_target_type)
	<< ",\"transposed\":"
	<< (profile.surface_proxy_transposed ? "true" : "false") << "}"
	<< ",\"closed\":[" << (profile.closed[0] ? "true" : "false")
	<< "," << (profile.closed[1] ? "true" : "false") << "]"
	<< ",\"periodic\":["
	<< (profile.periodic[0] ? "true" : "false") << ","
	<< (profile.periodic[1] ? "true" : "false") << "]"
	<< ",\"singular_sides\":[";
    for (int side = 0; side < 4; ++side) {
	if (side)
	    std::cout << ",";
	std::cout << (profile.singular_sides[side] ? "true" : "false");
    }
    std::cout << "]"
	<< ",\"revolution\":{\"present\":"
	<< (profile.revolution ? "true" : "false")
	<< ",\"transposed\":"
	<< (profile.revolution_transposed ? "true" : "false")
	<< ",\"profile\":{\"type\":" << json_quote(profile.profile_type)
	<< ",\"domain\":";
    if (profile.revolution && !BU_STR_EQUAL(profile.profile_type, "none"))
	print_interval(profile.profile_domain);
    else
	std::cout << "null";
    std::cout << ",\"proxy\":{\"present\":"
	<< (profile.profile_proxy ? "true" : "false")
	<< ",\"target_type\":"
	<< json_quote(profile.profile_proxy_target_type)
	<< ",\"reversed\":"
	<< (profile.profile_proxy_reversed ? "true" : "false")
	<< ",\"target_domain\":";
    if (profile.profile_proxy)
	print_interval(profile.profile_proxy_domain);
    else
	std::cout << "null";
    std::cout << "},\"polycurve\":{\"present\":"
	<< (profile.profile_polycurve ? "true" : "false")
	<< ",\"segment_count\":"
	<< profile.profile_polycurve_segments.size()
	<< ",\"segments\":[";
    for (size_t segment_index = 0;
	    segment_index < profile.profile_polycurve_segments.size();
	    ++segment_index) {
	if (segment_index)
	    std::cout << ",";
	const polycurve_segment_profile &segment =
	    profile.profile_polycurve_segments[segment_index];
	std::cout << "{\"index\":" << segment_index
	    << ",\"type\":" << json_quote(segment.type.c_str())
	    << ",\"polycurve_domain\":";
	print_interval(segment.polycurve_domain);
	std::cout << ",\"curve_domain\":";
	print_interval(segment.curve_domain);
	std::cout << ",\"proxy\":{\"present\":"
	    << (segment.proxy ? "true" : "false")
	    << ",\"target_type\":"
	    << json_quote(segment.proxy_target_type.c_str())
	    << ",\"reversed\":"
	    << (segment.proxy_reversed ? "true" : "false")
	    << ",\"target_domain\":";
	if (segment.proxy)
	    print_interval(segment.proxy_domain);
	else
	    std::cout << "null";
	std::cout << "}}";
    }
    std::cout << "]}}}}";
}

static void
print_surface_parameter_mapping(const surface_tree_profile &profile)
{
    const surface_parameter_mapping_profile &mapping =
	profile.parameter_mapping;
    std::cout << "{\"sampled\":" << (mapping.sampled ? "true" : "false")
	<< ",\"evidence_only\":true"
	<< ",\"grid_size\":" << mapping.grid_size
	<< ",\"surface_domain\":[";
    print_interval(profile.domain[0]);
    std::cout << ",";
    print_interval(profile.domain[1]);
    std::cout << "],\"nurb_form_domain\":";
    if (profile.nurb_form_available) {
	std::cout << "[";
	print_interval(profile.nurb_form_domain[0]);
	std::cout << ",";
	print_interval(profile.nurb_form_domain[1]);
	std::cout << "]";
    } else {
	std::cout << "null";
    }
    std::cout << ",\"forward\":";
    print_parameter_mapping_direction(mapping.forward);
    std::cout << ",\"inverse\":";
    print_parameter_mapping_direction(mapping.inverse);
    std::cout << "}";
}

static void
print_status2_inventory_entry(const surface_tree_profile &profile)
{
    std::cout << "{\"face_index\":" << profile.failure.face_index
	<< ",\"surface_index\":" << profile.surface_index
	<< ",\"nurb_form_status\":" << profile.nurb_form_status
	<< ",\"representation\":";
    print_surface_representation(profile);
    std::cout << ",\"parameter_mapping\":";
    print_surface_parameter_mapping(profile);
    std::cout << "}";
}

static void
print_prep_result(const char *db_path, const char *object,
	const prep_result &result, size_t repeat_index = 0,
	size_t repeat_count = 1)
{
    std::cout << "{\"format\":\"brlcad-brep-ray-prep-audit-v1\""
	<< ",\"database\":" << json_quote(db_path)
	<< ",\"object\":" << json_quote(object)
	<< ",\"status\":" << json_quote(result.ret == BRLCAD_OK ?
	    "ok" : "fail")
	<< ",\"return_code\":" << result.ret
	<< ",\"failure_reason\":" << json_quote(result.failure_reason)
	<< ",\"repeat_index\":" << repeat_index
	<< ",\"repeat_count\":" << repeat_count
	<< ",\"cache_lookup_requested\":"
	<< (result.cache_lookup_requested ? "true" : "false")
	<< ",\"cache_hit\":" << (result.cache_hit ? "true" : "false")
	<< ",\"seconds\":";
    print_num(result.seconds);
    std::cout << ",\"peak_rss_bytes\":" << result.peak_rss_bytes
	<< ",\"resident_before_bytes\":" << result.resident_before_bytes
	<< ",\"resident_prepared_bytes\":"
	<< result.resident_prepared_bytes
	<< ",\"resident_after_free_bytes\":"
	<< result.resident_after_free_bytes
	<< ",\"requested_adaptive_depth\":" << result.requested_depth
	<< ",\"recorded_adaptive_depth\":"
	<< (result.stats_valid ? result.stats.surface_tree_depth_limit : -1)
	<< ",\"surface_tree_build_microseconds\":"
	<< (result.stats_valid ?
	    result.stats.surface_tree_build_microseconds : 0)
	<< ",\"surface_tree_nodes\":"
	<< (result.stats_valid ? result.stats.surface_tree_nodes : 0)
	<< ",\"surface_tree_leaves\":"
	<< (result.stats_valid ? result.stats.surface_tree_leaves : 0)
	<< ",\"routing_feature_cells\":"
	<< (result.stats_valid ? result.stats.routing_feature_cells : 0)
	<< ",\"routing_unsupported_cells\":"
	<< (result.stats_valid ? result.stats.routing_unsupported_cells : 0)
	<< ",\"routing_cell_span_packs\":"
	<< (result.stats_valid ? result.stats.routing_cell_span_packs : 0)
	<< ",\"routing_cell_span_control_intervals\":"
	<< (result.stats_valid ?
	    result.stats.routing_cell_span_control_intervals : 0)
	<< ",\"routing_cell_span_subpatch_boxes\":"
	<< (result.stats_valid ?
	    result.stats.routing_cell_span_subpatch_boxes : 0)
	<< ",\"routing_cell_span_pack_budget_bytes\":"
	<< (result.stats_valid ?
	    result.stats.routing_cell_span_pack_budget_bytes : 0)
	<< ",\"routing_cell_span_pack_used_bytes\":"
	<< (result.stats_valid ?
	    result.stats.routing_cell_span_pack_used_bytes : 0)
	<< ",\"routing_direction_cache_bytes\":"
	<< (result.stats_valid ?
	    result.stats.routing_direction_cache_bytes : 0)
	<< ",\"routing_direction_cache_capacity\":"
	<< (result.stats_valid ?
	    result.stats.routing_direction_cache_capacity : 0)
	<< ",\"routing_direction_cache_ready_entries\":"
	<< (result.stats_valid ?
	    result.stats.routing_direction_cache_ready_entries : 0)
	<< ",\"cache_payload_serialized\":"
	<< (result.cache_payload_valid ? "true" : "false")
	<< ",\"cache_variant\":";
    if (result.cache_payload_valid)
	std::cout << result.cache_variant;
    else
	std::cout << "null";
    std::cout << ",\"cache_payload_bytes\":" << result.cache_payload_bytes
	<< ",\"cache_serialize_seconds\":";
    print_num(result.cache_serialize_seconds);
    std::cout << ",\"cache_payload_compressed\":"
	<< (result.cache_payload_compressed ? "true" : "false")
	<< ",\"cache_payload_compressed_bytes\":"
	<< result.cache_payload_compressed_bytes
	<< ",\"cache_compress_seconds\":";
    print_num(result.cache_compress_seconds);
    std::cout << ",\"retained_bytes\":"
	<< (result.stats_valid ? result.stats.retained_bytes : 0)
	<< ",\"retained_used_bytes\":"
	<< (result.stats_valid ? result.stats.retained_used_bytes : 0)
	<< ",\"retained_unused_capacity_bytes\":"
	<< (result.stats_valid ?
	    result.stats.retained_unused_capacity_bytes : 0)
	<< ",\"retained_core_bytes\":"
	<< (result.stats_valid ? result.stats.retained_core_bytes : 0)
	<< ",\"retained_brep_bytes\":"
	<< (result.stats_valid ? result.stats.retained_brep_bytes : 0)
	<< ",\"retained_surface_tree_bytes\":"
	<< (result.stats_valid ? result.stats.retained_surface_tree_bytes : 0)
	<< ",\"retained_curve_tree_bytes\":"
	<< (result.stats_valid ? result.stats.retained_curve_tree_bytes : 0)
	<< ",\"retained_edge_bytes\":"
	<< (result.stats_valid ? result.stats.retained_edge_bytes : 0)
	<< ",\"retained_surface_bytes\":"
	<< (result.stats_valid ? result.stats.retained_surface_bytes : 0)
	<< ",\"retained_hierarchy_bytes\":"
	<< (result.stats_valid ? result.stats.retained_hierarchy_bytes : 0)
	<< ",\"retained_routing_bytes\":"
	<< (result.stats_valid ? result.stats.retained_routing_bytes : 0)
	<< ",\"retained_vertex_bytes\":"
	<< (result.stats_valid ? result.stats.retained_vertex_bytes : 0)
	<< ",\"retained_surface_tree_unused_capacity_bytes\":"
	<< (result.stats_valid ?
	    result.stats.retained_surface_tree_unused_capacity_bytes : 0)
	<< ",\"retained_curve_tree_unused_capacity_bytes\":"
	<< (result.stats_valid ?
	    result.stats.retained_curve_tree_unused_capacity_bytes : 0)
	<< ",\"retained_edge_unused_capacity_bytes\":"
	<< (result.stats_valid ?
	    result.stats.retained_edge_unused_capacity_bytes : 0)
	<< ",\"retained_surface_unused_capacity_bytes\":"
	<< (result.stats_valid ?
	    result.stats.retained_surface_unused_capacity_bytes : 0)
	<< ",\"retained_hierarchy_unused_capacity_bytes\":"
	<< (result.stats_valid ?
	    result.stats.retained_hierarchy_unused_capacity_bytes : 0)
	<< ",\"retained_routing_unused_capacity_bytes\":"
	<< (result.stats_valid ?
	    result.stats.retained_routing_unused_capacity_bytes : 0)
	<< ",\"retained_vertex_unused_capacity_bytes\":"
	<< (result.stats_valid ?
	    result.stats.retained_vertex_unused_capacity_bytes : 0)
	<< ",\"retained_routing_feature_cell_unused_capacity_bytes\":"
	<< (result.stats_valid ? result.stats.
	    retained_routing_feature_cell_unused_capacity_bytes : 0)
	<< ",\"retained_routing_binding_unused_capacity_bytes\":"
	<< (result.stats_valid ? result.stats.
	    retained_routing_binding_unused_capacity_bytes : 0)
	<< ",\"retained_routing_pack_unused_capacity_bytes\":"
	<< (result.stats_valid ? result.stats.
	    retained_routing_pack_unused_capacity_bytes : 0)
	<< ",\"retained_routing_control_unused_capacity_bytes\":"
	<< (result.stats_valid ? result.stats.
	    retained_routing_control_unused_capacity_bytes : 0)
	<< ",\"retained_routing_subpatch_unused_capacity_bytes\":"
	<< (result.stats_valid ? result.stats.
	    retained_routing_subpatch_unused_capacity_bytes : 0)
	<< ",\"retained_routing_bvh_unused_capacity_bytes\":"
	<< (result.stats_valid ? result.stats.
	    retained_routing_bvh_unused_capacity_bytes : 0);
    std::cout
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
    std::cout << "],\"status2_inventory\":[";
    bool first_status2 = true;
    for (size_t face_index = 0; face_index < result.faces.size();
	    ++face_index) {
	const surface_tree_profile &profile = result.faces[face_index];
	if (profile.nurb_form_status != 2)
	    continue;
	if (!first_status2)
	    std::cout << ",";
	first_status2 = false;
	print_status2_inventory_entry(profile);
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
	    << ",\"nurb_form_domain\":";
	if (profile.nurb_form_available) {
	    std::cout << "[";
	    print_interval(profile.nurb_form_domain[0]);
	    std::cout << ",";
	    print_interval(profile.nurb_form_domain[1]);
	    std::cout << "]";
	} else {
	    std::cout << "null";
	}
	std::cout
	    << ",\"domain\":[";
	print_interval(profile.domain[0]);
	std::cout << ",";
	print_interval(profile.domain[1]);
	std::cout << "],\"representation\":";
	print_surface_representation(profile);
	std::cout << ",\"parameter_mapping\":";
	print_surface_parameter_mapping(profile);
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

struct audit_config {
    double ratio_min;
    double ratio_max;
    double tess_abs;
    double tess_rel;
    double tess_norm;
    long memory_limit_mib;
    long jobs;
    long max_time_ms;
    long quality_face_time_ms;
    long max_result_mib;
    long max_working_mib;
    long max_points;
    long max_triangles;
    double display_coarse_rel;
    double display_area_change;
    long face_index;
    bool valid_solids_only;
    bool display_only;
    bool quality_repair;
    double repair_hole_area_percent;
    long repair_hole_edges;
    long repair_adaptive_hole_edges;
    double repair_adaptive_hole_area_percent;
    double repair_planar_cap_area_percent;
    double repair_area_change_percent;
    double repair_max_deviation;
    double repair_max_deviation_rel;
    long repair_deviation_samples;
    bool repair_allow_untrimmed;
    bool repair_full_fast;
    bool repair_full_fast_if_needed;
    bool repair_try_invalid;
    bool repair_union_components;
    bool repair_allow_self_intersections;
    bool repair_require_manifold;
    bool repair_no_fast;
    double repair_relaxed_fidelity_factor;
    const char *image_dir;
    long image_size;
};

struct image_view {
    vect_t right;
    vect_t up;
    vect_t view;
    point_t center;
    double scale;
};

static std::string
image_name(const char *directory, const char *database, const char *object,
	const char *mode)
{
    std::string name = directory ? directory : "";
    if (!name.empty() && name.back() != '/') name += "/";
    const char *base = database ? std::strrchr(database, '/') : NULL;
    uint64_t path_hash = 1469598103934665603ULL;
    for (const unsigned char *p = (const unsigned char *)database;
	    p && *p; ++p) {
	path_hash ^= *p;
	path_hash *= 1099511628211ULL;
    }
    name += base ? base + 1 : (database ? database : "database");
    name += "_";
    std::ostringstream hash_text;
    hash_text << std::hex << path_hash;
    name += hash_text.str();
    name += "_";
    for (const char *p = object; p && *p; ++p)
	name += (*p == '/' || *p == '\\') ? '_' : *p;
    name += "_";
    name += mode;
    name += ".ppm";
    return name;
}

static bool
image_view_setup(const point_t minp, const point_t maxp, image_view *view)
{
    if (!view) return false;
    point_t center;
    VADD2(center, minp, maxp);
    VSCALE(view->center, center, 0.5);
    VSET(view->view, 1.0, 1.0, 1.0);
    VUNITIZE(view->view);
    vect_t up_reference = {0.0, 0.0, 1.0};
    VCROSS(view->right, view->view, up_reference);
    VUNITIZE(view->right);
    VCROSS(view->up, view->right, view->view);
    VUNITIZE(view->up);
    vect_t dimensions;
    VSUB2(dimensions, maxp, minp);
    view->scale = std::max(MAGNITUDE(dimensions) * 0.58, SMALL_FASTF);
    return true;
}

static void
image_project(const image_view *view, const point_t p, long size,
	double *x, double *y, double *depth)
{
    vect_t offset;
    VSUB2(offset, p, view->center);
    *x = 0.5 * size + VDOT(offset, view->right) * size / view->scale;
    *y = 0.5 * size - VDOT(offset, view->up) * size / view->scale;
    *depth = VDOT(offset, view->view);
}

static void
image_pixel(std::vector<unsigned char> *pixels, long size, long x, long y,
	unsigned char r, unsigned char g, unsigned char b)
{
    if (x < 0 || y < 0 || x >= size || y >= size) return;
    size_t offset = ((size_t)y * (size_t)size + (size_t)x) * 3;
    (*pixels)[offset] = r;
    (*pixels)[offset + 1] = g;
    (*pixels)[offset + 2] = b;
}

static void
image_line(std::vector<unsigned char> *pixels, long size, long x0, long y0,
	long x1, long y1)
{
    long dx = std::labs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    long dy = -std::labs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    long error = dx + dy;
    for (;;) {
	image_pixel(pixels, size, x0, y0, 245, 245, 245);
	if (x0 == x1 && y0 == y1) break;
	long e2 = 2 * error;
	if (e2 >= dy) { error += dy; x0 += sx; }
	if (e2 <= dx) { error += dx; y0 += sy; }
    }
}

static bool
write_image(const std::string &path, long size,
	const std::vector<image_segment> *segments, const image_mesh *mesh,
	const point_t minp, const point_t maxp)
{
    if (size <= 0 || (!segments && !mesh)) return false;
    image_view view;
    if (!image_view_setup(minp, maxp, &view)) return false;
    std::vector<unsigned char> pixels((size_t)size * (size_t)size * 3, 18);
    if (segments) {
	for (const image_segment &segment : *segments) {
	    double x0, y0, z0, x1, y1, z1;
	    image_project(&view, segment.a, size, &x0, &y0, &z0);
	    image_project(&view, segment.b, size, &x1, &y1, &z1);
	    image_line(&pixels, size, (long)x0, (long)y0, (long)x1, (long)y1);
	}
    }
    if (mesh && mesh->faces.size() >= 3) {
	std::vector<double> depth((size_t)size * (size_t)size,
	    -std::numeric_limits<double>::infinity());
	for (size_t i = 0; i + 2 < mesh->faces.size(); i += 3) {
	    int ia = mesh->faces[i], ib = mesh->faces[i + 1], ic = mesh->faces[i + 2];
	    if (ia < 0 || ib < 0 || ic < 0 || (size_t)ia >= mesh->points.size() ||
		(size_t)ib >= mesh->points.size() || (size_t)ic >= mesh->points.size()) continue;
	    double ax, ay, az, bx, by, bz, cx, cy, cz;
	    image_project(&view, mesh->points[(size_t)ia].p, size, &ax, &ay, &az);
	    image_project(&view, mesh->points[(size_t)ib].p, size, &bx, &by, &bz);
	    image_project(&view, mesh->points[(size_t)ic].p, size, &cx, &cy, &cz);
	    long xmin = std::max(0L, (long)std::floor(std::min(ax, std::min(bx, cx))));
	    long xmax = std::min(size - 1, (long)std::ceil(std::max(ax, std::max(bx, cx))));
	    long ymin = std::max(0L, (long)std::floor(std::min(ay, std::min(by, cy))));
	    long ymax = std::min(size - 1, (long)std::ceil(std::max(ay, std::max(by, cy))));
	    double area = (bx - ax) * (cy - ay) - (by - ay) * (cx - ax);
	    if (std::fabs(area) < SMALL_FASTF) continue;
	    for (long y = ymin; y <= ymax; ++y) for (long x = xmin; x <= xmax; ++x) {
		double w0 = ((bx - ax) * (y - ay) - (by - ay) * (x - ax)) / area;
		double w1 = ((cx - bx) * (y - by) - (cy - by) * (x - bx)) / area;
		double w2 = 1.0 - w0 - w1;
		if (w0 < 0.0 || w1 < 0.0 || w2 < 0.0) continue;
		double z = w0 * cz + w1 * az + w2 * bz;
		size_t pi = (size_t)y * (size_t)size + (size_t)x;
		if (z <= depth[pi]) continue;
		depth[pi] = z;
		vect_t ab, ac, normal;
		VSUB2(ab, mesh->points[(size_t)ib].p,
		    mesh->points[(size_t)ia].p);
		VSUB2(ac, mesh->points[(size_t)ic].p,
		    mesh->points[(size_t)ia].p);
		VCROSS(normal, ab, ac);
		if (MAGNITUDE(normal) > SMALL_FASTF) VUNITIZE(normal);
		vect_t light = {0.35, -0.45, 0.82};
		VUNITIZE(light);
		double diffuse = std::max(0.0, VDOT(normal, light));
		double fill = 0.22;
		double intensity = std::min(1.0, fill + 0.78 * diffuse);
		image_pixel(&pixels, size, x, y,
		    (unsigned char)(145.0 * intensity),
		    (unsigned char)(175.0 * intensity),
		    (unsigned char)(220.0 * intensity));
	    }
	}
    }
    std::ofstream output(path, std::ios::binary);
    if (!output) return false;
    output << "P6\n" << size << " " << size << "\n255\n";
    output.write((const char *)pixels.data(), (std::streamsize)pixels.size());
    return output.good();
}

static int
audit_brep(struct db_i *dbip, struct directory *dp, const char *db_path,
	const char *mode_name, const audit_config &config, long task_index)
{
    struct bn_tol tol = BN_TOL_INIT_TOL;
    struct bg_tess_tol ttol = BG_TESS_TOL_INIT_TOL;
    ttol.abs = config.tess_abs;
    ttol.rel = config.tess_rel;
    ttol.norm = config.tess_norm;
    struct brep_cdt_fast_options fast_options;
    brep_cdt_fast_options_default(&fast_options);
    struct rt_brep_draw_options draw_options;
    rt_brep_draw_options_default(&draw_options);
    if (config.jobs > 0)
	fast_options.max_workers = draw_options.max_workers =
	    (size_t)config.jobs;
    if (config.max_time_ms > 0) {
	fast_options.max_time_ms = config.max_time_ms;
	draw_options.max_time_ms = config.max_time_ms;
    }
    if (config.max_result_mib > 0) {
	fast_options.max_result_bytes =
	    (size_t)config.max_result_mib * 1024 * 1024;
	draw_options.max_result_bytes =
	    (size_t)config.max_result_mib * 1024 * 1024;
	draw_options.max_working_bytes = std::max((size_t)1,
	    draw_options.max_result_bytes / 4);
    }
    if (config.max_working_mib > 0)
	fast_options.max_working_bytes = draw_options.max_working_bytes =
	    (size_t)config.max_working_mib * 1024 * 1024;
    if (config.max_points > 0) {
	fast_options.max_points = (size_t)config.max_points;
	draw_options.max_points = (size_t)config.max_points;
    }
    if (config.max_triangles > 0)
	fast_options.max_triangles = (size_t)config.max_triangles;
    if (config.display_coarse_rel > 0.0)
	fast_options.coarse_relative_tolerance =
	    config.display_coarse_rel;
    if (config.display_area_change > 0.0)
	fast_options.area_change_tolerance =
	    config.display_area_change;

    point_t ref_min = VINIT_ZERO;
    point_t ref_max = VINIT_ZERO;
    bool ref_valid = false;
    point_t boundary_min = VINIT_ZERO;
    point_t boundary_max = VINIT_ZERO;
    bool boundary_valid = false;
    int ref_faces = 0;
    int ref_face_failures = 0;
    std::vector<int> ref_failed_faces;
    std::vector<std::string> top_issues;
    bool input_loaded = false;
    bool input_valid = false;
    bool input_manifold = false;
    bool input_oriented = false;
    bool input_has_boundary = true;
    bool input_solid = false;
    bool input_two_trim_edges = false;
    bool quality_eligible = false;
    bool wireframe_only = false;
    /* Visualization must not require the expensive source-validation and
     * trimmed-bounding-box reference pass.  The display generators have
     * their own bounded geometry/resource diagnostics. */
    if (!config.display_only) {
	std::cerr << "brep-audit: phase=reference" << std::endl;
	struct rt_db_internal intern;
	if (load_brep(dbip, dp, &intern) == BRLCAD_OK) {
	    struct rt_brep_internal *bi =
		(struct rt_brep_internal *)intern.idb_ptr;
	    input_loaded = true;
	    wireframe_only = bi->brep->m_F.Count() == 0;
	    ON_wString validity_log;
	    ON_TextLog validity_output(validity_log);
	    input_valid = bi->brep->IsValid(&validity_output);
	    input_manifold = bi->brep->IsManifold(&input_oriented,
		&input_has_boundary);
	    input_solid = bi->brep->IsSolid();
	    input_two_trim_edges = true;
	    for (int edge = 0; edge < bi->brep->m_E.Count(); ++edge) {
		if (bi->brep->m_E[edge].TrimCount() != 2) {
		    input_two_trim_edges = false;
		    break;
		}
	    }
	    quality_eligible = input_valid && input_manifold && input_oriented &&
		!input_has_boundary && input_solid && input_two_trim_edges;
	    ON_BoundingBox bbox = ON_BoundingBox::EmptyBoundingBox;
	    ON_BoundingBox boundary_bbox = ON_BoundingBox::EmptyBoundingBox;
	    const int brep_faces = bi->brep->m_F.Count();
	    int first_ref_face = 0;
	    int end_ref_face = brep_faces;
	    if (config.face_index >= 0) {
		if (config.face_index >= brep_faces) {
		    top_issues.push_back("face_index_out_of_range");
		    end_ref_face = 0;
		} else {
		    first_ref_face = (int)config.face_index;
		    end_ref_face = first_ref_face + 1;
		}
	    }
	    ref_faces = end_ref_face - first_ref_face;
	    const bool excluded = config.valid_solids_only && !quality_eligible;
	    for (int i = first_ref_face; !excluded && i < end_ref_face; i++) {
		ON_BoundingBox face_bbox = ON_BoundingBox::EmptyBoundingBox;
		if (!face_GetBoundingBox(bi->brep->m_F[i], face_bbox, false) ||
			!face_bbox.IsValid()) {
		    ref_face_failures++;
		    ref_failed_faces.push_back(i);
		    continue;
		}
		if (bbox.IsValid())
		    bbox.Union(face_bbox);
		else
		    bbox = face_bbox;
		ON_BoundingBox face_boundary = ON_BoundingBox::EmptyBoundingBox;
		if (face_boundary_bbox(bi->brep->m_F[i], face_boundary)) {
		    if (boundary_bbox.IsValid())
			boundary_bbox.Union(face_boundary);
		    else
			boundary_bbox = face_boundary;
		}
	    }
	    if (bbox.IsValid()) {
		VMOVE(ref_min, bbox.m_min);
		VMOVE(ref_max, bbox.m_max);
		ref_valid = true;
	    } else if (!excluded && !wireframe_only) {
		top_issues.push_back("trimmed_bbox_failed");
	    }
	    if (boundary_bbox.IsValid()) {
		VMOVE(boundary_min, boundary_bbox.m_min);
		VMOVE(boundary_max, boundary_bbox.m_max);
		boundary_valid = true;
	    }
	    if (ref_face_failures)
		top_issues.push_back("trimmed_bbox_face_failures");
	    rt_db_free_internal(&intern);
	} else {
	    top_issues.push_back("database_internal_load_failed");
	}
    }

    const bool run_wireframe = BU_STR_EQUAL(mode_name, "wireframe") ||
	BU_STR_EQUAL(mode_name, "both") || BU_STR_EQUAL(mode_name, "all");
    const bool run_shaded = BU_STR_EQUAL(mode_name, "shaded") ||
	BU_STR_EQUAL(mode_name, "both") || BU_STR_EQUAL(mode_name, "all");
    const bool run_quality = BU_STR_EQUAL(mode_name, "quality") ||
	BU_STR_EQUAL(mode_name, "all");
    const bool quality_not_applicable = run_quality && wireframe_only;
    const bool excluded = config.valid_solids_only && input_loaded &&
	!quality_eligible;
    geom_result wire;
    geom_result shaded;
    std::vector<image_segment> wire_segments;
    image_mesh shaded_mesh;
    geom_result quality;
    struct brep_cdt_repair_settings repair_settings =
	BREP_CDT_REPAIR_SETTINGS_INIT;
    repair_settings.mesh.fill_holes = config.quality_repair ? 1 : 0;
    repair_settings.mesh.max_hole_area_percent =
	config.repair_hole_area_percent;
    repair_settings.mesh.max_hole_edges =
	(size_t)config.repair_hole_edges;
    repair_settings.max_adaptive_hole_edges =
	(size_t)config.repair_adaptive_hole_edges;
    repair_settings.max_adaptive_hole_area_percent =
	config.repair_adaptive_hole_area_percent;
    repair_settings.max_planar_cap_area_percent =
	config.repair_planar_cap_area_percent;
    repair_settings.max_area_change_percent =
	config.repair_area_change_percent;
    repair_settings.max_surface_deviation = config.repair_max_deviation;
    if (config.repair_max_deviation_rel > 0.0) {
	vect_t repair_dimensions;
	if (boundary_valid)
	    VSUB2(repair_dimensions, boundary_max, boundary_min);
	else
	    VSUB2(repair_dimensions, ref_max, ref_min);
	repair_settings.max_surface_deviation =
	    config.repair_max_deviation_rel * MAGNITUDE(repair_dimensions);
    }
    repair_settings.max_deviation_samples =
	(size_t)config.repair_deviation_samples;
    repair_settings.allow_untrimmed_surface_match =
	config.repair_allow_untrimmed ? 1 : 0;
    repair_settings.use_full_fast_fallback =
	config.repair_full_fast ? 1 : 0;
    repair_settings.use_full_fast_fallback_if_needed =
	config.repair_full_fast_if_needed ? 1 : 0;
    repair_settings.try_invalid_brep = config.repair_try_invalid ? 1 : 0;
    repair_settings.mesh.union_components =
	config.repair_union_components ? 1 : 0;
    repair_settings.mesh.allow_self_intersections =
	config.repair_allow_self_intersections ? 1 : 0;
    repair_settings.mesh.require_manifold =
	config.repair_require_manifold ? 1 : 0;
    repair_settings.use_fast_face_fallback =
	config.repair_no_fast ? 0 : 1;
    repair_settings.relaxed_fidelity_factor =
	config.repair_relaxed_fidelity_factor;
    if (config.max_points > 0)
	repair_settings.max_fast_points = (size_t)config.max_points;
    if (config.max_result_mib > 0)
	repair_settings.max_fast_result_bytes =
	    (size_t)config.max_result_mib * 1024 * 1024;
    if (config.max_time_ms > 0)
	repair_settings.max_fast_time_ms = config.max_time_ms;
    if (run_wireframe && !excluded) {
	std::cerr << "brep-audit: phase=wireframe" << std::endl;
	try {
	    wire = wireframe_result(dbip, dp, &ttol, &tol, &draw_options,
		config.image_dir ? &wire_segments : NULL);
	} catch (const std::bad_alloc &) {
	    wire.ret = RT_BREP_DRAW_LIMIT;
	    wire.hit_memory_limit = true;
	    wire.issues.push_back("resource_limit");
	}
    }
    if (run_shaded && !excluded) {
	std::cerr << "brep-audit: phase=shaded" << std::endl;
	try {
	    shaded = shaded_result(dbip, dp, &ttol, &tol, &fast_options,
		(int)config.face_index, config.image_dir ? &shaded_mesh : NULL);
	} catch (const std::bad_alloc &) {
	    shaded.ret = BREP_CDT_FAST_LIMIT;
	    shaded.hit_memory_limit = true;
	    shaded.issues.push_back("resource_limit");
	}
    }
    if (config.display_only) {
	input_loaded = true;
	if (run_wireframe && std::find(wire.issues.begin(), wire.issues.end(),
		"database_internal_load_failed") != wire.issues.end())
	    input_loaded = false;
	if (run_shaded && std::find(shaded.issues.begin(), shaded.issues.end(),
		"database_internal_load_failed") != shaded.issues.end())
	    input_loaded = false;
    }
    if (run_quality && !excluded && !quality_not_applicable) {
	std::cerr << "brep-audit: phase=quality" << std::endl;
	quality = quality_result(dbip, dp, &ttol,
	    (int)config.face_index, config.quality_face_time_ms,
	    config.quality_repair ? &repair_settings : NULL);
    }
    /* Invalid B-Reps can contain faces whose trimmed bounding boxes cannot be
     * evaluated even though the remaining faces and all edge bounds establish
     * usable aggregate references.  Once mesh repair independently certifies
     * a solid and its bidirectional fidelity, retain the failed face indices
     * as diagnostics but do not let that expected source defect override the
     * repaired result. */
    if (run_quality && quality.repair_succeeded && ref_valid &&
	    boundary_valid) {
	const auto issue = std::find(top_issues.begin(), top_issues.end(),
	    "trimmed_bbox_face_failures");
	if (issue != top_issues.end())
	    top_issues.erase(issue);
    }
    vect_t ref_dims = VINIT_ZERO;
    vect_t boundary_dims = VINIT_ZERO;
    double ref_diag = 0.0;
    double boundary_diag = 0.0;
    if (boundary_valid) {
	dims(boundary_dims, boundary_min, boundary_max);
	boundary_diag = MAGNITUDE(boundary_dims);
    }
    if (ref_valid) {
	dims(ref_dims, ref_min, ref_max);
	ref_diag = MAGNITUDE(ref_dims);
	if (run_wireframe)
	    check_dimensions(&wire, ref_dims, ref_diag, 0.0,
		config.ratio_max, "wireframe");
	if (run_shaded) {
	    check_dimensions(&shaded, ref_dims, ref_diag, 0.0,
		config.ratio_max, "shaded");
	    if (boundary_valid)
		check_dimensions(&shaded, boundary_dims, boundary_diag,
		    config.ratio_min, DBL_MAX, "shaded_boundary");
	}
	if (run_quality) {
	    check_dimensions(&quality, ref_dims, ref_diag, 0.0,
		config.ratio_max, "quality");
	    if (boundary_valid) {
		double extent_tolerance = 0.0;
		if (quality.repair_succeeded) {
		    extent_tolerance = quality.repair_relaxed_fidelity_applied ?
			quality.repair_relaxed_deviation_limit :
			quality.repair_allowed_deviation;
		    if (!(extent_tolerance > 0.0) ||
			    !std::isfinite(extent_tolerance))
			extent_tolerance = 0.0;
		}
		check_dimensions(&quality, boundary_dims, boundary_diag,
		    config.ratio_min, DBL_MAX, "quality_boundary",
		    boundary_min, boundary_max, extent_tolerance);
	    }
	}
    }

    bool okay = !excluded && (config.display_only || ref_valid || wireframe_only) &&
	top_issues.empty() &&
	(!run_wireframe || wire.issues.empty()) &&
	(!run_shaded || shaded.issues.empty()) &&
	(!run_quality || quality.issues.empty());

    std::string wire_image;
    std::string shaded_image;
    if (config.image_dir && config.image_size > 0) {
	bu_mkdir(config.image_dir);
	if (run_wireframe && !excluded && wire.issues.empty() &&
		wire.have_bbox && !wire_segments.empty()) {
	    wire_image = image_name(config.image_dir, db_path, dp->d_namep,
		"wireframe");
	    if (!write_image(wire_image, config.image_size, &wire_segments, NULL,
		wire.bmin, wire.bmax)) wire_image.clear();
	}
	if (run_shaded && !excluded && shaded.issues.empty() &&
		shaded.have_bbox && !shaded_mesh.faces.empty()) {
	    shaded_image = image_name(config.image_dir, db_path, dp->d_namep,
		"shaded");
	    if (!write_image(shaded_image, config.image_size, NULL, &shaded_mesh,
		shaded.bmin, shaded.bmax)) shaded_image.clear();
	}
    }

    std::cerr << "brep-audit: phase=report" << std::endl;
    std::cout << "{\"format\":\"brlcad-brep-realization-audit-v1\",\"database\":"
	<< json_quote(db_path) << ",\"object\":" << json_quote(dp->d_namep)
	<< ",\"task_index\":" << task_index
	<< ",\"status\":" << json_quote((excluded || (quality_not_applicable &&
	    !run_wireframe && !run_shaded && top_issues.empty())) ? "excluded" :
	    (okay ? "ok" : "fail"))
	<< ",\"quality_exclusion_reason\":" <<
	    (quality_not_applicable ? "\"wireframe_only\"" : "null")
	<< ",\"ratio_limits\":[" << std::setprecision(17)
	<< config.ratio_min << "," << config.ratio_max << "]"
	<< ",\"tessellation_tolerance\":{\"abs\":" << config.tess_abs
	<< ",\"rel\":" << config.tess_rel << ",\"norm\":"
	<< config.tess_norm << "}"
	<< ",\"memory_limit_mib\":" << config.memory_limit_mib
	<< ",\"quality_options\":{\"max_face_time_ms\":"
	<< config.quality_face_time_ms << "}"
	<< ",\"mode\":" << json_quote(mode_name)
	<< ",\"face_index\":" << config.face_index
	<< ",\"input\":{\"loaded\":" << (input_loaded ? "true" : "false")
	<< ",\"valid\":" << (input_valid ? "true" : "false")
	<< ",\"manifold\":" << (input_manifold ? "true" : "false")
	<< ",\"oriented\":" << (input_oriented ? "true" : "false")
	<< ",\"has_boundary\":" << (input_has_boundary ? "true" : "false")
	<< ",\"solid\":" << (input_solid ? "true" : "false")
	<< ",\"two_trim_edges\":" <<
	    (input_two_trim_edges ? "true" : "false")
	<< ",\"quality_eligible\":" <<
	    (quality_eligible ? "true" : "false")
	<< ",\"wireframe_only\":" << (wireframe_only ? "true" : "false") << "}"
	<< ",\"fast_options\":{\"jobs\":" << fast_options.max_workers
	<< ",\"max_time_ms\":" << fast_options.max_time_ms
	<< ",\"max_result_bytes\":" << fast_options.max_result_bytes
	<< ",\"max_working_bytes\":" << fast_options.max_working_bytes
	<< ",\"max_points\":" << fast_options.max_points
	<< ",\"max_triangles\":" << fast_options.max_triangles
	<< ",\"adaptive_quality\":"
	<< (fast_options.adaptive_quality ? "true" : "false")
	<< ",\"coarse_relative_tolerance\":"
	<< fast_options.coarse_relative_tolerance
	<< ",\"area_change_tolerance\":"
	<< fast_options.area_change_tolerance << "}"
	<< ",\"wire_options\":{\"jobs\":" << draw_options.max_workers
	<< ",\"max_time_ms\":" << draw_options.max_time_ms
	<< ",\"max_result_bytes\":" << draw_options.max_result_bytes
	<< ",\"max_working_bytes\":" << draw_options.max_working_bytes
	<< ",\"max_points\":" << draw_options.max_points << "}"
	<< ",\"generators\":{\"wireframe\":\"rt_brep_plot\""
	<< ",\"shaded\":\"brep_cdt_fast\""
	<< ",\"quality\":\"ON_Brep_CDT_Tessellate\"}"
	<< ",\"reference\":{\"method\":\"face_GetBoundingBox_trim_parameter_envelope\""
	<< ",\"face_count\":" << ref_faces
	<< ",\"failed_faces\":" << ref_face_failures
	<< ",\"failed_face_indices\":";
    print_indices(ref_failed_faces);
    std::cout << ",\"bbox_valid\":" << (ref_valid ? "true" : "false")
	<< ",\"bbox_min\":";
    if (ref_valid) print_vec(ref_min); else std::cout << "null";
    std::cout << ",\"bbox_max\":";
    if (ref_valid) print_vec(ref_max); else std::cout << "null";
    std::cout << ",\"dimensions\":";
    if (ref_valid) print_vec(ref_dims); else std::cout << "null";
    std::cout << ",\"diagonal\":";
    if (ref_valid) print_num(ref_diag); else std::cout << "null";
    std::cout << ",\"role\":\"upper_extent_guard\""
	<< ",\"boundary_method\":\"brep_edge_parameter_samples\""
	<< ",\"boundary_bbox_valid\":"
	<< (boundary_valid ? "true" : "false")
	<< ",\"boundary_bbox_min\":";
    if (boundary_valid) print_vec(boundary_min); else std::cout << "null";
    std::cout << ",\"boundary_bbox_max\":";
    if (boundary_valid) print_vec(boundary_max); else std::cout << "null";
    std::cout << ",\"boundary_dimensions\":";
    if (boundary_valid) print_vec(boundary_dims); else std::cout << "null";
    std::cout << ",\"boundary_diagonal\":";
    if (boundary_valid) print_num(boundary_diag); else std::cout << "null";
    std::cout << "},\"wireframe\":";
    if (run_wireframe && !excluded) print_result(wire, ref_dims);
    else std::cout << "null";
    std::cout << ",\"shaded\":";
    if (run_shaded && !excluded) print_result(shaded, ref_dims);
    else std::cout << "null";
	std::cout << ",\"quality\":";
    if (run_quality && !excluded && !quality_not_applicable)
	print_result(quality, ref_dims);
    else std::cout << "null";
    std::cout << ",\"images\":{\"wireframe\":"
	<< (wire_image.empty() ? "null" : json_quote(wire_image.c_str()))
	<< ",\"shaded\":"
	<< (shaded_image.empty() ? "null" : json_quote(shaded_image.c_str())) << "}";
    std::cout << ",\"issues\":";
    print_issues(top_issues);
    std::cout << "}" << std::endl;
    rt_vlist_cleanup();
#ifdef __GLIBC__
    malloc_trim(0);
#endif
    return excluded || okay ? 0 : 1;
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
    int batch = 0;
    int prep_only = 0;
    int cached_prep_only = 0;
    int surface_trees_only = 0;
    int surface_trees_all = 0;
    int ray_depth_compare_only = 0;
    double ratio_min = 0.5;
    double ratio_max = 2.0;
    double tess_abs = 0.0;
    double tess_rel = 0.01;
    double tess_norm = 0.0;
    long memory_limit_mib = 0;
    long jobs = 0;
    long max_time_ms = 0;
    long quality_face_time_ms = 0;
    long max_result_mib = 0;
    long max_working_mib = 0;
    long max_points = 0;
    long max_triangles = 0;
    double display_coarse_rel = 0.0;
    double display_area_change = 0.0;
    long batch_start = 0;
    long face_index = -1;
    int valid_solids_only = 0;
    int display_only = 0;
    int quality_repair = 0;
    double repair_hole_area_percent = 1.0;
    long repair_hole_edges = 256;
    long repair_adaptive_hole_edges = 4096;
    double repair_adaptive_hole_area_percent = 0.0;
    double repair_planar_cap_area_percent = 0.0;
    double repair_area_change_percent = 1.0;
    double repair_max_deviation = 0.0;
    double repair_max_deviation_rel = 0.0;
    long repair_deviation_samples = 4096;
    int repair_allow_untrimmed = 0;
    int repair_full_fast = 0;
    int repair_full_fast_if_needed = 0;
    int repair_try_invalid = 0;
    int repair_union_components = 0;
    int repair_allow_self_intersections = 0;
    int repair_require_manifold = 0;
    int repair_no_fast = 0;
    double repair_relaxed_fidelity_factor = 0.0;
    const char *image_dir = NULL;
    long image_size = 1024;
    const char *batch_object_file = NULL;
    const char *mode_name = "both";
    struct bu_opt_desc d[67];
    long surface_tree_depth = BREP_MAX_FT_DEPTH;
    long candidate_depth = 3;
    long reference_depth = RT_BREP_DEFAULT_SURFACE_TREE_DEPTH;
    long ray_count = 64;
    long feature_ray_count = 0;
    long ray_pair = -1;
    long prep_repeat = 1;
    long mesh_ray_face = -1;
    int legacy_only = 0;
    int seeded_verify = 0;
    int routing_truth = 0;
    int routing_publication = 0;
    int routing_missing_root_injection = 0;
    const char *trim_query_arg = NULL;
    const char *exact_ray_arg = NULL;
    const char *transform_arg = NULL;
    BU_OPT(d[0], "h", "help", "", NULL, &print_help, "Print help and exit");
    BU_OPT(d[1], "l", "list", "", NULL, &list_only, "List BRep primitive names");
    BU_OPT(d[2], "", "ratio-min", "#", &bu_opt_fastf_t, &ratio_min, "Minimum acceptable generated/reference dimension ratio");
    BU_OPT(d[3], "", "ratio-max", "#", &bu_opt_fastf_t, &ratio_max, "Maximum acceptable generated/reference dimension ratio");
    BU_OPT(d[4], "", "tess-abs", "#", &bu_opt_fastf_t, &tess_abs, "Absolute shaded tessellation tolerance");
    BU_OPT(d[5], "", "tess-rel", "#", &bu_opt_fastf_t, &tess_rel, "Relative shaded tessellation tolerance");
    BU_OPT(d[6], "", "tess-norm", "#", &bu_opt_fastf_t, &tess_norm, "Normal shaded tessellation tolerance");
    BU_OPT(d[7], "", "memory-limit-mib", "#", &bu_opt_long, &memory_limit_mib, "Process address-space limit in MiB (zero disables)");
    BU_OPT(d[8], "m", "mode", "wireframe|shaded|quality|both|all", &bu_opt_str, &mode_name, "Select generators; quality runs the conversion CDT");
    BU_OPT(d[9], "j", "jobs", "#", &bu_opt_long, &jobs, "Maximum shaded face workers (zero selects the default)");
    BU_OPT(d[10], "", "max-time-ms", "#", &bu_opt_long, &max_time_ms, "Shaded generation deadline checked between faces");
    BU_OPT(d[11], "", "max-result-mib", "#", &bu_opt_long, &max_result_mib, "Maximum retained shaded result size");
    BU_OPT(d[12], "", "max-points", "#", &bu_opt_long, &max_points, "Maximum retained shaded points");
    BU_OPT(d[13], "", "batch", "", NULL, &batch, "Audit all BReps in one database process");
    BU_OPT(d[14], "", "batch-start", "#", &bu_opt_long, &batch_start, "First flattened batch task index");
    BU_OPT(d[15], "", "face-index", "#", &bu_opt_long, &face_index, "Shade only one BRep face (non-batch mode)");
    BU_OPT(d[16], "", "valid-solids-only", "", NULL,
	&valid_solids_only,
	"Exclude inputs outside the rigorous CDT topology contract");
    BU_OPT(d[17], "", "quality-repair", "", NULL, &quality_repair,
	"Attempt explicitly bounded mesh repair after quality failure");
    BU_OPT(d[18], "", "repair-hole-area-percent", "#",
	&bu_opt_fastf_t, &repair_hole_area_percent,
	"Maximum area of each repair hole as input mesh percentage");
    BU_OPT(d[19], "", "repair-hole-edges", "#", &bu_opt_long,
	&repair_hole_edges, "Maximum boundary edges in each repair hole");
    BU_OPT(d[20], "", "repair-area-change-percent", "#",
	&bu_opt_fastf_t, &repair_area_change_percent,
	"Maximum aggregate repaired mesh area change percentage");
    BU_OPT(d[21], "", "repair-max-deviation", "#", &bu_opt_fastf_t,
	&repair_max_deviation,
	"Maximum sampled repair deviation; zero uses tessellation tolerance");
    BU_OPT(d[22], "", "repair-deviation-samples", "#", &bu_opt_long,
	&repair_deviation_samples,
	"Maximum changed-triangle deviation samples");
    BU_OPT(d[23], "", "repair-allow-untrimmed", "", NULL,
	&repair_allow_untrimmed,
	"Permit and report deviation matches to underlying untrimmed surfaces");
    BU_OPT(d[24], "", "repair-full-fast", "", NULL,
	&repair_full_fast,
	"Use one whole-B-Rep display mesh as the repair input");
    BU_OPT(d[25], "", "repair-union-components", "", NULL,
	&repair_union_components,
	"Regularize closed repair components with a Manifold union");
    BU_OPT(d[26], "", "repair-no-fast", "", NULL, &repair_no_fast,
	"Repair only the rigorous face meshes, without display fallback faces");
    BU_OPT(d[27], "", "batch-object-file", "file", &bu_opt_str,
	&batch_object_file,
	"In batch mode, audit only object names listed one per line");
    BU_OPT(d[28], "", "repair-max-deviation-rel", "fraction",
	&bu_opt_fastf_t, &repair_max_deviation_rel,
	"Maximum repair deviation as a boundary bbox diagonal fraction");
    BU_OPT(d[29], "", "repair-allow-self-intersections", "", NULL,
	&repair_allow_self_intersections,
	"Permit repaired manifold topology to contain self-intersections");
    BU_OPT(d[30], "", "repair-require-manifold", "", NULL,
	&repair_require_manifold,
	"Require the bundled Manifold library to import the repaired mesh");
    BU_OPT(d[31], "", "repair-full-fast-if-needed", "", NULL,
	&repair_full_fast_if_needed,
	"Retry whole-B-Rep display repair only after rigorous repair fails");
    BU_OPT(d[32], "", "repair-try-invalid", "", NULL,
	&repair_try_invalid,
	"Try rigorous faces of structurally safe invalid B-Reps before repair");
    BU_OPT(d[33], "", "repair-relaxed-fidelity-factor", "factor",
	&bu_opt_fastf_t, &repair_relaxed_fidelity_factor,
	"Accept and tag Manifold repair within 1 through 4 times strict fidelity");
    BU_OPT(d[34], "", "repair-adaptive-hole-edges", "#", &bu_opt_long,
	&repair_adaptive_hole_edges,
	"Second-pass edge ceiling when only bounded open holes remain");
    BU_OPT(d[35], "", "quality-face-time-ms", "#", &bu_opt_long,
	&quality_face_time_ms,
	"Wall-clock limit for each rigorous quality face (zero disables)");
    BU_OPT(d[36], "", "repair-adaptive-hole-area-percent", "#",
	&bu_opt_fastf_t, &repair_adaptive_hole_area_percent,
	"Final-only hole candidate area ceiling for bounded open-edge repair");
    BU_OPT(d[37], "", "max-working-mib", "#", &bu_opt_long,
	&max_working_mib, "Maximum shared temporary generator memory");
    BU_OPT(d[38], "", "max-triangles", "#", &bu_opt_long,
	&max_triangles, "Maximum adaptive shaded triangle target");
    BU_OPT(d[39], "", "display-coarse-rel", "fraction",
	&bu_opt_fastf_t, &display_coarse_rel,
	"Initial relative tolerance for adaptive shaded display");
    BU_OPT(d[40], "", "display-area-change", "fraction",
	&bu_opt_fastf_t, &display_area_change,
	"Per-face unsigned area convergence threshold");
    BU_OPT(d[41], "", "display-only", "", NULL, &display_only,
	"Run only bounded visualization generators; skip source reference checks");
	BU_OPT(d[42], "", "image-dir", "directory", &bu_opt_str, &image_dir,
	"Write successful display geometry images to this directory");
	BU_OPT(d[43], "", "image-size", "pixels", &bu_opt_long, &image_size,
	"Square output image size (default 1024)");
    BU_OPT(d[44], "", "repair-planar-cap-area-percent", "#",
	&bu_opt_fastf_t, &repair_planar_cap_area_percent,
	"Opt-in aggregate planar cap area as a percentage of remaining surface area");
    BU_OPT(d[45], "", "prep-only", "", NULL, &prep_only, "Audit raytrace preparation only");
    BU_OPT(d[46], "", "surface-trees-only", "", NULL, &surface_trees_only, "Profile raytrace SurfaceTrees serially");
    BU_OPT(d[47], "", "surface-trees-all", "", NULL, &surface_trees_all,
	"Summarize SurfaceTrees for every BREP in a database");
    BU_OPT(d[48], "", "surface-tree-depth", "#", &bu_opt_long,
	&surface_tree_depth, "Adaptive SurfaceTree depth after structural splits");
    BU_OPT(d[49], "", "ray-depth-compare", "", NULL,
	&ray_depth_compare_only,
	"Compare uncached BREP partitions at two SurfaceTree depths");
    BU_OPT(d[50], "", "candidate-depth", "#", &bu_opt_long,
	&candidate_depth, "Candidate adaptive SurfaceTree depth");
    BU_OPT(d[51], "", "reference-depth", "#", &bu_opt_long,
	&reference_depth, "Reference adaptive SurfaceTree depth");
    BU_OPT(d[52], "", "ray-count", "#", &bu_opt_long, &ray_count,
	"Deterministic random chord pairs (three axis pairs are added)");
    BU_OPT(d[53], "", "ray-pair", "#", &bu_opt_long, &ray_pair,
	"Shoot only one deterministic chord pair by global index");
    BU_OPT(d[54], "", "legacy-only", "", NULL, &legacy_only,
	"Skip prepared solving while tracing the legacy SurfaceTree path");
    BU_OPT(d[55], "", "trim-query", "face,u,v", &bu_opt_str,
	&trim_query_arg,
	"Compare production and sampled-polygon trim classification");
    BU_OPT(d[56], "", "feature-ray-count", "#", &bu_opt_long,
	&feature_ray_count,
	"Deterministic face-normal, edge-normal, and edge-grazing chord pairs");
    BU_OPT(d[57], "", "prep-repeat", "#", &bu_opt_long, &prep_repeat,
	"Prepare and fully unload the same BREP repeatedly");
    BU_OPT(d[58], "", "seeded-verify", "", NULL, &seeded_verify,
	"Run the audit-only legacy-seeded independent partition verifier");
    BU_OPT(d[59], "", "routing-truth", "", NULL, &routing_truth,
	"Compare prepared and legacy partitions on identical rays");
    BU_OPT(d[60], "", "routing-publication", "", NULL,
	&routing_publication, "Emit publication-oriented routing evidence");
    BU_OPT(d[61], "", "routing-missing-root-injection", "", NULL,
	&routing_missing_root_injection,
	"Inject an audit-only missing-root routing failure");
    BU_OPT(d[62], "", "cached-prep-only", "", NULL, &cached_prep_only,
	"Audit cache-aware raytrace preparation only");
    BU_OPT(d[63], "", "ray", "ox,oy,oz,dx,dy,dz", &bu_opt_str,
	&exact_ray_arg, "Shoot one exact ray and a bounded reverse partner");
    BU_OPT(d[64], "", "transform", "m00,...,m33", &bu_opt_str,
	&transform_arg,
	"Apply a 4x4 solid-to-model transform before an exact-ray comparison");
    BU_OPT(d[65], "", "mesh-ray-face", "#", &bu_opt_long,
	&mesh_ray_face,
	"Intersect an exact ray with an independent fast tessellation of one face");
    BU_OPT_NULL(d[66]);
    int ac = bu_opt_parse(NULL, argc, argv, d);
    trim_query_spec parsed_trim_query;
    const bool trim_query_only = trim_query_arg != NULL;
    const bool trim_query_valid = !trim_query_only ||
	parse_trim_query(trim_query_arg, parsed_trim_query);
    ray_audit_exact_ray parsed_exact_ray;
    const bool exact_ray_valid = exact_ray_arg == NULL ||
	parse_exact_ray(exact_ray_arg, parsed_exact_ray);
    mat_t parsed_transform = MAT_INIT_IDN;
    const bool transform_valid = transform_arg == NULL ||
	parse_transform(transform_arg, parsed_transform);
    const bool mesh_ray_only = mesh_ray_face >= 0;
    const char *usage =
	"Usage: brep-audit [options] "
	"[--list|--batch|--prep-only|--cached-prep-only|--surface-trees-only|"
	"--surface-trees-all|--ray-depth-compare|"
	"--trim-query face,u,v|--mesh-ray-face #] "
	"[--routing-truth] [--routing-publication] "
	"[--routing-missing-root-injection] "
	"[--ray ox,oy,oz,dx,dy,dz] [--transform m00,...,m33] "
	"file.g [brep]\n";
    const int exclusive_modes = (list_only ? 1 : 0) + (batch ? 1 : 0) +
	(prep_only ? 1 : 0) + (cached_prep_only ? 1 : 0) +
	(surface_trees_only ? 1 : 0) +
	(surface_trees_all ? 1 : 0) + (ray_depth_compare_only ? 1 : 0) +
	(trim_query_only ? 1 : 0) + (mesh_ray_only ? 1 : 0);
    if (print_help || exclusive_modes > 1 || !trim_query_valid ||
	    !exact_ray_valid || !transform_valid ||
	    ((list_only || batch || surface_trees_all) && ac != 1) ||
	    (!list_only && !batch && !surface_trees_all && ac != 2) ||
	    ratio_min <= 0.0 || ratio_max < ratio_min || tess_abs < 0.0 ||
	    tess_rel < 0.0 || tess_norm < 0.0 || memory_limit_mib < 0 ||
	    quality_face_time_ms < 0 ||
	    surface_tree_depth < 0 ||
	    surface_tree_depth > BREP_MAX_FT_DEPTH ||
	    (!prep_only && !cached_prep_only && !surface_trees_only &&
	     !surface_trees_all &&
	     surface_tree_depth != BREP_MAX_FT_DEPTH) ||
	    candidate_depth < 0 || candidate_depth > BREP_MAX_FT_DEPTH ||
	    reference_depth < 0 || reference_depth > BREP_MAX_FT_DEPTH ||
	    ray_count < 0 || ray_count > 1000000 ||
	    feature_ray_count < 0 || feature_ray_count > 1000000 ||
	    prep_repeat < 1 || prep_repeat > 1000 ||
	    mesh_ray_face < -1 || mesh_ray_face > INT_MAX ||
	    ray_pair < -1 || ray_pair > 1000002 ||
	    (exact_ray_arg && ray_pair != -1) ||
	    (mesh_ray_only && !exact_ray_arg) ||
	    (transform_arg && !ray_depth_compare_only && !mesh_ray_only) ||
	    (!prep_only && !cached_prep_only && prep_repeat != 1) ||
	    (routing_truth && (!ray_depth_compare_only || legacy_only)) ||
	    (routing_publication && !routing_truth) ||
	    (routing_missing_root_injection && !routing_truth) ||
	    (!ray_depth_compare_only && !mesh_ray_only &&
	     (candidate_depth != 3 ||
	      reference_depth != RT_BREP_DEFAULT_SURFACE_TREE_DEPTH ||
	      ray_count != 64 || feature_ray_count != 0 ||
	      ray_pair != -1 || exact_ray_arg || legacy_only ||
	      seeded_verify || routing_truth || routing_publication ||
	      routing_missing_root_injection)) ||
	    jobs < 0 || max_time_ms < 0 || max_result_mib < 0 ||
	    max_working_mib < 0 ||
	    max_points < 0 || max_triangles < 0 ||
	    !std::isfinite(display_coarse_rel) ||
	    display_coarse_rel < 0.0 || display_coarse_rel > 0.5 ||
	    !std::isfinite(display_area_change) ||
	    display_area_change < 0.0 || display_area_change > 1.0 ||
	    image_size <= 0 || image_size > 8192 ||
	    batch_start < 0 || face_index < -1 ||
	    repair_hole_area_percent <= 0.0 || repair_hole_edges < 3 ||
	    repair_adaptive_hole_edges < 0 ||
	    (repair_adaptive_hole_edges > 0 &&
	    repair_adaptive_hole_edges < repair_hole_edges) ||
	    repair_adaptive_hole_area_percent < 0.0 ||
	    !std::isfinite(repair_planar_cap_area_percent) ||
	    repair_planar_cap_area_percent < 0.0 ||
	    (repair_planar_cap_area_percent > 0.0 &&
		(!quality_repair || !repair_try_invalid)) ||
	    (repair_adaptive_hole_area_percent > 0.0 &&
	    repair_adaptive_hole_area_percent < repair_hole_area_percent) ||
	    repair_area_change_percent < 0.0 || repair_max_deviation < 0.0 ||
	    repair_max_deviation_rel < 0.0 ||
	    (repair_max_deviation > 0.0 && repair_max_deviation_rel > 0.0) ||
	    !std::isfinite(repair_relaxed_fidelity_factor) ||
	    repair_relaxed_fidelity_factor < 0.0 ||
	    (repair_relaxed_fidelity_factor > 0.0 &&
	    (repair_relaxed_fidelity_factor < 1.0 ||
	    repair_relaxed_fidelity_factor > 4.0)) ||
	    repair_deviation_samples <= 0 ||
	    (repair_no_fast && (repair_full_fast ||
	    repair_full_fast_if_needed)) ||
	    (repair_full_fast_if_needed &&
	    (repair_full_fast)) ||
	    (repair_try_invalid && (repair_full_fast)) ||
	    (batch_object_file && !batch) ||
	    (batch && face_index != -1) ||
	    (display_only && valid_solids_only) ||
	    (display_only && (BU_STR_EQUAL(mode_name, "quality") ||
	    BU_STR_EQUAL(mode_name, "all"))) ||
	    (quality_repair && face_index != -1) ||
	    (face_index != -1 && BU_STR_EQUAL(mode_name, "wireframe")) ||
	    (!BU_STR_EQUAL(mode_name, "wireframe") &&
	     !BU_STR_EQUAL(mode_name, "shaded") &&
	     !BU_STR_EQUAL(mode_name, "quality") &&
	     !BU_STR_EQUAL(mode_name, "both") &&
	     !BU_STR_EQUAL(mode_name, "all"))) {
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
    audit_config config = {
	ratio_min, ratio_max, tess_abs, tess_rel, tess_norm,
	memory_limit_mib, jobs, max_time_ms, quality_face_time_ms,
	max_result_mib, max_working_mib, max_points, max_triangles,
	display_coarse_rel, display_area_change,
	face_index, valid_solids_only != 0, display_only != 0,
	quality_repair != 0,
	repair_hole_area_percent, repair_hole_edges,
	repair_adaptive_hole_edges,
	repair_adaptive_hole_area_percent,
	repair_planar_cap_area_percent,
	repair_area_change_percent, repair_max_deviation,
	repair_max_deviation_rel,
	repair_deviation_samples, repair_allow_untrimmed != 0,
	repair_full_fast != 0, repair_full_fast_if_needed != 0,
	repair_try_invalid != 0,
	repair_union_components != 0,
	repair_allow_self_intersections != 0,
	repair_require_manifold != 0,
	repair_no_fast != 0,
	repair_relaxed_fidelity_factor, image_dir, image_size
    };

    if (batch) {
	std::set<std::string> selected_objects;
	if (batch_object_file) {
	    std::ifstream selection(batch_object_file);
	    if (!selection) {
		std::cerr << "Unable to open batch object file: "
		    << batch_object_file << "\n";
		db_close(dbip);
		return 2;
	    }
	    std::string name;
	    while (std::getline(selection, name)) {
		if (!name.empty() && name.back() == '\r')
		    name.pop_back();
		if (!name.empty())
		    selected_objects.insert(name);
	    }
	}
	std::vector<struct directory *> breps;
	struct directory *entry;
	FOR_ALL_DIRECTORY_START(entry, dbip) {
	    if (entry->d_minor_type == DB5_MINORTYPE_BRLCAD_BREP &&
		    (!batch_object_file || selected_objects.find(
		    entry->d_namep) != selected_objects.end()))
		breps.push_back(entry);
	} FOR_ALL_DIRECTORY_END;
	std::sort(breps.begin(), breps.end(),
	    [](const struct directory *a, const struct directory *b) {
		return std::string(a->d_namep) < std::string(b->d_namep);
	    });
	std::vector<const char *> modes;
	if (BU_STR_EQUAL(mode_name, "wireframe") ||
		BU_STR_EQUAL(mode_name, "both") ||
		BU_STR_EQUAL(mode_name, "all"))
	    modes.push_back("wireframe");
	if (BU_STR_EQUAL(mode_name, "shaded") ||
		BU_STR_EQUAL(mode_name, "both") ||
		BU_STR_EQUAL(mode_name, "all"))
	    modes.push_back("shaded");
	if (BU_STR_EQUAL(mode_name, "quality") ||
		BU_STR_EQUAL(mode_name, "all"))
	    modes.push_back("quality");
	long task_index = 0;
	for (struct directory *brep : breps) {
	    for (const char *task_mode : modes) {
		if (task_index++ < batch_start)
		    continue;
		const long active_index = task_index - 1;
		std::cout
		    << "{\"format\":\"brlcad-brep-audit-progress-v1\""
		    << ",\"database\":" << json_quote(argv[0])
		    << ",\"object\":" << json_quote(brep->d_namep)
		    << ",\"mode\":" << json_quote(task_mode)
		    << ",\"task_index\":" << active_index
		    << ",\"status\":\"started\"}" << std::endl;
		audit_brep(dbip, brep, argv[0], task_mode, config,
		    active_index);
	    }
	}
	std::cout
	    << "{\"format\":\"brlcad-brep-audit-complete-v1\""
	    << ",\"database\":" << json_quote(argv[0])
	    << ",\"task_count\":" << task_index
	    << "}" << std::endl;
	db_close(dbip);
	return 0;
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
	int ret = 0;
	for (long repeat = 0; repeat < prep_repeat; ++repeat) {
	    prep_result prep = raytrace_prep_result(dbip, dp,
		(int)surface_tree_depth);
	    print_prep_result(argv[0], argv[1], prep, (size_t)repeat,
		(size_t)prep_repeat);
	    if (prep.ret != BRLCAD_OK)
		ret = 1;
	}
	db_close(dbip);
	return ret;
    }

    if (cached_prep_only) {
	int ret = 0;
	for (long repeat = 0; repeat < prep_repeat; ++repeat) {
	    prep_result prep = raytrace_cached_prep_result(dbip, dp, argv[0],
		argv[1], (int)surface_tree_depth);
	    print_prep_result(argv[0], argv[1], prep, (size_t)repeat,
		(size_t)prep_repeat);
	    if (prep.ret != BRLCAD_OK)
		ret = 1;
	}
	db_close(dbip);
	return ret;
    }

    if (surface_trees_only) {
	surface_tree_result trees = surface_tree_profile_result(dbip, dp,
	    (int)surface_tree_depth);
	print_surface_tree_result(argv[0], argv[1], trees);
	db_close(dbip);
	return trees.ret == BRLCAD_OK ? 0 : 1;
    }

    if (mesh_ray_only) {
	struct bn_tol mesh_tol = BN_TOL_INIT_TOL;
	struct bg_tess_tol mesh_tess_tol = BG_TESS_TOL_INIT_TOL;
	mesh_tess_tol.abs = tess_abs;
	mesh_tess_tol.rel = tess_rel;
	mesh_tess_tol.norm = tess_norm;
	const int ret = mesh_ray_query(dbip, argv[0], dp,
	    (int)mesh_ray_face, parsed_exact_ray, &mesh_tess_tol, &mesh_tol,
	    transform_arg ? parsed_transform : NULL);
	db_close(dbip);
	return ret;
    }

    if (ray_depth_compare_only) {
	if (seeded_verify &&
		bu_setenv("LIBRT_BREP_SEEDED_VERIFY", "1", 1) != 0) {
	    std::cerr << "Unable to enable the seeded verifier\n";
	    db_close(dbip);
	    return 2;
	}
	if (routing_truth &&
		bu_setenv("LIBRT_BREP_ROUTE_DECISION", "1", 1) != 0) {
	    std::cerr << "Unable to enable the routing decision\n";
	    db_close(dbip);
	    return 2;
	}
	const int ret = ray_depth_compare(dbip, argv[0], dp,
	    (int)candidate_depth, (int)reference_depth, (size_t)ray_count,
	    (size_t)feature_ray_count, ray_pair, legacy_only != 0,
	    routing_truth != 0, routing_publication != 0,
	    routing_missing_root_injection != 0,
	    exact_ray_arg ? &parsed_exact_ray : NULL,
	    transform_arg ? parsed_transform : NULL);
	db_close(dbip);
	return ret;
    }

    if (trim_query_only) {
	const int ret = trim_query(dbip, argv[0], dp, parsed_trim_query);
	db_close(dbip);
	return ret;
    }

    int ret = audit_brep(dbip, dp, argv[0], mode_name, config, -1);
    db_close(dbip);
    return ret;
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
