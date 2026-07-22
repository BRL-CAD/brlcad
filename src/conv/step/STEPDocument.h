/*                 S T E P D O C U M E N T . H
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 */

#ifndef CONV_STEP_STEPDOCUMENT_H
#define CONV_STEP_STEPDOCUMENT_H

#include "common.h"

#include <cstddef>
#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace brlcad {
namespace step {

enum class RepairMode {
    None,
    Safe
};

enum class DiagnosticSeverity {
    Information,
    Warning,
    Error,
    Fatal
};

/** Thread-safe progress state copied by the command-line telemetry reporter.
 * Totals are zero when a parser or conversion phase cannot determine them. */
struct ImportProgress {
    std::string phase;
    uint64_t completed = 0;
    uint64_t total = 0;
    uint64_t secondary_completed = 0;
    uint64_t secondary_total = 0;
    std::string secondary_label;
    std::string detail;
    int64_t current_entity_id = 0;
    /** Cumulative geometry-object progress across every conversion batch.
     * The total may grow while a legacy representation walk discovers later
     * surface-model jobs, but it never shrinks. */
    uint64_t geometry_items_processed = 0;
    uint64_t geometry_items_total = 0;
    /** Nested context for the oldest active exact-geometry job. */
    int64_t geometry_root_entity_id = 0;
    int64_t geometry_item_entity_id = 0;
    uint64_t geometry_item_completed = 0;
    uint64_t geometry_item_total = 0;
    std::string geometry_item_label;
    int64_t geometry_subentity_id = 0;
    /** Geometry-pipeline occupancy.  Capacity is zero outside the detached
     * conversion pipeline, allowing reporters to omit these fields. */
    uint64_t geometry_jobs_queued = 0;
    uint64_t geometry_workers_active = 0;
    uint64_t geometry_jobs_ready = 0;
    uint64_t geometry_jobs_spooled = 0;
    uint64_t geometry_jobs_finished = 0;
    uint64_t geometry_jobs_materializing = 0;
    uint64_t geometry_helpers_active = 0;
    uint64_t geometry_jobs_in_flight = 0;
    uint64_t geometry_runnable_capacity = 0;
    uint64_t geometry_ready_bytes = 0;
    uint64_t geometry_ready_byte_budget = 0;
};

struct ImportOptions {
    unsigned int requested_jobs = 1;
    unsigned int effective_jobs = 1;
    double absolute_tolerance_mm = 0.0;
    RepairMode repair = RepairMode::Safe;
    /** Enforce the file-declared/overridden tolerance literally.  When false,
     * a verified, bounded source curve/surface mismatch may raise the
     * corresponding OpenNURBS edge tolerance with a warning. */
    bool exact = false;
    bool strict = false;
    bool verbose = false;
    bool dry_run = false;
    /**
     * If nonempty, convert only representation-item roots with these Part 21
     * instance identifiers.  Product and assembly context and the complete
     * dependency closure of each selected root are still imported.
     */
    std::set<int64_t> selected_entity_ids;
};

struct Diagnostic {
    DiagnosticSeverity severity = DiagnosticSeverity::Information;
    int64_t entity_id = 0;
    std::string entity_type;
    uint64_t file_offset = 0;
    uint64_t line = 0;
    std::string attribute;
    std::string message;
    uint64_t repeat_count = 1;
};

/** A numeric, point, or descriptive product property retained from STEP. */
struct MetadataProperty {
    int64_t entity_id = 0;
    int64_t representation_id = 0;
    int64_t item_entity_id = 0;
    std::string category;
    std::string name;
    std::string description;
    std::string value_type;
    std::string units;
    std::string text;
    std::vector<double> values;
};

/** A direct or MAKE_FROM_USAGE_OPTION material assignment. */
struct Material {
    int64_t usage_entity_id = 0;
    int64_t definition_entity_id = 0;
    int64_t product_entity_id = 0;
    std::string identifier;
    std::string name;
    std::string description;
    std::vector<MetadataProperty> properties;
};

struct Product {
    int64_t entity_id = 0;
    std::string original_name;
    std::string output_name;
    std::string identifier;
    std::string description;
    std::string revision;
    std::string revision_description;
    std::string definition_identifier;
    std::string definition_description;
    std::vector<Material> materials;
    std::vector<MetadataProperty> validation_properties;
};

struct Representation {
    int64_t entity_id = 0;
    int64_t product_id = 0;
    std::string type;
    std::string output_name;
};

struct Occurrence {
    int64_t entity_id = 0;
    int64_t parent_product_id = 0;
    int64_t child_product_id = 0;
    double matrix[16] = {
	1.0, 0.0, 0.0, 0.0,
	0.0, 1.0, 0.0, 0.0,
	0.0, 0.0, 1.0, 0.0,
	0.0, 0.0, 0.0, 1.0
    };
};

struct Style {
    int64_t item_entity_id = 0;
    std::vector<int64_t> source_entity_ids;
    std::string name;
    bool has_rgb = false;
    double rgb[3] = {0.0, 0.0, 0.0};
    bool has_transparency = false;
    double transparency = 0.0;
    std::vector<std::string> layers;
};

struct Layer {
    int64_t entity_id = 0;
    std::string name;
    std::string description;
    std::vector<int64_t> item_entity_ids;
};

/** Schema-neutral, immutable-after-extraction import graph. */
struct Document {
    std::vector<std::string> schema_identifiers;
    std::map<int64_t, Product> products;
    std::map<int64_t, Representation> representations;
    std::map<int64_t, Occurrence> occurrences;
    std::map<int64_t, Style> styles;
    std::map<int64_t, Layer> layers;
    /** False when --entity intentionally limits coverage accounting to the
     * selected roots instead of rescanning every indexed source instance. */
    bool entity_counts_complete = true;
    std::map<std::string, uint64_t> entity_counts;
    std::map<std::string, uint64_t> unsupported_counts;
};

/** One exact representation item which could not be written.  These records
 * remain entity-specific even when the human-readable diagnostic stream is
 * aggregated, so a report can be fed directly back to --entity for a focused
 * retry. */
struct SkippedItem {
    int64_t entity_id = 0;
    std::string entity_type;
    std::string reason;
};

/** Aggregated elapsed time for a measured import stage. */
struct StageTiming {
    uint64_t calls = 0;
    uint64_t total_us = 0;
    uint64_t maximum_us = 0;
    int64_t maximum_entity_id = 0;
};

/** Bounded per-item timing retained for slow-item diagnosis. */
struct ItemTiming {
    int64_t entity_id = 0;
    std::string entity_type;
    std::string stage;
    uint64_t elapsed_us = 0;
    uint64_t faces = 0;
    uint64_t edges = 0;
    uint64_t trims = 0;
};

struct ImportStatistics {
    uint64_t input_instances = 0;
    uint64_t products = 0;
    uint64_t occurrences = 0;
    uint64_t geometry_attempted = 0;
    uint64_t geometry_written = 0;
    uint64_t geometry_skipped = 0;
    uint64_t styles_extracted = 0;
    uint64_t styles_applied = 0;
    uint64_t layers_extracted = 0;
    uint64_t materials_extracted = 0;
    uint64_t properties_extracted = 0;
    uint64_t invalid_breps = 0;
    uint64_t output_failures = 0;
    uint64_t repairs = 0;
    uint64_t pullback_closest_point_queries = 0;
    uint64_t pullback_surfaces_prepared = 0;
    uint64_t pullback_surface_cache_hits = 0;
    uint64_t pullback_span_boxes_built = 0;
    uint64_t pullback_span_boxes_tested = 0;
    uint64_t pullback_primary_search_successes = 0;
    uint64_t pullback_continuity_seed_searches = 0;
    uint64_t pullback_continuity_seed_successes = 0;
    uint64_t pullback_continuity_seed_failures = 0;
    uint64_t pullback_continuity_seed_finite_candidates = 0;
    uint64_t pullback_continuity_seed_iterations = 0;
    uint64_t pullback_continuity_seed_line_searches = 0;
    uint64_t pullback_maximum_continuity_seed_iterations = 0;
    uint64_t pullback_maximum_continuity_seed_line_searches = 0;
    uint64_t pullback_multiseed_fallbacks = 0;
    uint64_t pullback_multiseed_successes = 0;
    uint64_t pullback_multiseed_failures = 0;
    uint64_t pullback_fallback_calls_with_finite_primary = 0;
    uint64_t pullback_fallback_samples_evaluated = 0;
    uint64_t pullback_fallback_seed_refinements = 0;
    uint64_t pullback_fallback_refinement_improvements = 0;
    uint64_t pullback_fallback_late_seed_improvements = 0;
    uint64_t pullback_maximum_winning_seed_index = 0;
    uint64_t pullback_subdivision_nodes = 0;
    uint64_t pullback_maximum_subdivision_nodes = 0;
    uint64_t pullback_preparation_us = 0;
    uint64_t pullback_primary_search_us = 0;
    uint64_t pullback_continuity_seed_us = 0;
    uint64_t pullback_multiseed_us = 0;
    double pullback_fallback_primary_improvement_total = 0.0;
    double pullback_fallback_primary_improvement_maximum = 0.0;
    double pullback_fallback_refinement_improvement_total = 0.0;
    double pullback_fallback_refinement_improvement_maximum = 0.0;
    std::map<std::string, StageTiming> stage_timings;
    std::vector<ItemTiming> slow_item_timings;
    uint64_t slow_item_timings_omitted = 0;
    std::vector<SkippedItem> skipped_items;
    /** Records beyond the bounded skipped_items report budget. */
    uint64_t skipped_items_omitted = 0;
    /** Requested representation-item roots encountered by a converter. */
    std::set<int64_t> selected_entity_ids_encountered;
    uint64_t lazy_indexed_instances = 0;
    uint64_t lazy_loaded_instances = 0;
    uint64_t lazy_current_loaded_instances = 0;
    uint64_t lazy_pinned_instances = 0;
    uint64_t lazy_cache_hits = 0;
    uint64_t lazy_cache_misses = 0;
    uint64_t lazy_materializations = 0;
    uint64_t lazy_evictions = 0;
    uint64_t lazy_active_batches = 0;
    uint64_t lazy_data_sections = 0;
    /** Source-record bytes represented by currently materialized instances. */
    uint64_t lazy_cache_bytes = 0;
    /** Maximum lazy_cache_bytes observed during the import session. */
    uint64_t lazy_cache_byte_high_water = 0;
    bool lazy_cache_bytes_available = false;
    int64_t load_time_us = 0;
    int64_t convert_time_us = 0;
    double tolerance_mm = 0.0;
    size_t peak_rss_bytes = 0;
};

} // namespace step
} // namespace brlcad

#endif /* CONV_STEP_STEPDOCUMENT_H */
