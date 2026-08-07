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
#include "STEPUnit.h"

#include <cstddef>
#include <cstdint>
#include <array>
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

/** Policy for completed B-rep candidates which fail validation. */
enum class InvalidBrepPolicy {
    Preserve,
    Reject
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
    /** Requested machine-speed multiplier.  Zero selects the deterministic
     * startup calibration; positive values bypass calibration. */
    double budget_scale = 0.0;
    /** Effective multiplier and derived limits are populated by the import
     * session before the first exact-geometry job starts. */
    double effective_budget_scale = 1.0;
    uint64_t item_budget_milliseconds = 0;
    uint64_t effective_item_budget_milliseconds = 60000;
    uint64_t stall_timeout_milliseconds = 0;
    uint64_t effective_stall_timeout_milliseconds = 60000;
    /** Disable CPU-work per-item deadlines while retaining the independent
     * no-progress watchdog. */
    bool disable_item_budgets = false;
    RepairMode repair = RepairMode::Safe;
    /** The command-line policy and the policy after stronger modes (notably
     * --strict) have been applied. */
    InvalidBrepPolicy requested_invalid_brep_policy = InvalidBrepPolicy::Preserve;
    InvalidBrepPolicy effective_invalid_brep_policy = InvalidBrepPolicy::Preserve;
    /** Enforce the file-declared/overridden tolerance literally.  When false,
     * a verified, bounded source curve/surface mismatch may raise the
     * corresponding OpenNURBS edge tolerance with a warning. */
    bool exact = false;
    bool strict = false;
    bool verbose = false;
    bool dry_run = false;
    /** Do not schedule OPEN_SHELL boundaries owned by shell-based surface
     * models.  CLOSED_SHELL boundaries, including those used by solid B-rep
     * entities, remain eligible for conversion. */
    bool skip_open_shells = false;
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

/** Geometry deliberately inferred from contradictory or incomplete source
 * data.  Unlike a bounded safe repair, this records a modeling choice made
 * only by the permissive importer.  The completed B-rep must still pass the
 * ordinary structural and solidness validation before it is written as a
 * region. */
struct InferredCurve {
    int64_t edge_entity_id = 0;
    std::string kind;
    double discrepancy_mm = 0.0;
    double safe_limit_mm = 0.0;
    double inference_limit_mm = 0.0;
    double declared_tolerance_mm = 0.0;
    std::string detail;
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
    /** Structured unit graph when the source supplied one.  This is exact
     * unless the safe importer has recorded and applied a narrowly defined
     * repair to schema-invalid source data.  units remains a convenient
     * normalized display/edit label. */
    UnitStructure unit_structure;
    std::string text;
    std::vector<double> values;
    bool has_dimensions = false;
    std::array<double, 7> dimension_exponents = {{0.0, 0.0, 0.0, 0.0,
	0.0, 0.0, 0.0}};
    /** False when the source value/unit pair violates a schema constraint.
     * Invalid source data remains in the report for diagnosis but is not
     * retained as exportable BRL-CAD attributes. */
    bool valid = true;
    std::string error;
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
    /** Source formation/definition identities are provenance for remapping
     * retained configuration assignments.  More than one source identity is
     * deliberately preserved as an ambiguity rather than collapsed. */
    std::vector<int64_t> formation_entity_ids;
    std::vector<int64_t> definition_entity_ids;
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
    uint64_t invalid_geometry_count = 0;
};

/** Conversion disposition for product-bound representation geometry. */
enum class RepresentationCoverageStatus {
    Unclassified,
    Handled,
    PreservedInvalid,
    Filtered,
    IntentionallyNonGeometric,
    Skipped,
    Malformed,
    Unsupported
};

inline const char *
RepresentationCoverageStatusName(RepresentationCoverageStatus status)
{
    switch (status) {
	case RepresentationCoverageStatus::Handled: return "handled";
	case RepresentationCoverageStatus::PreservedInvalid:
	    return "preserved_invalid";
	case RepresentationCoverageStatus::Filtered: return "filtered";
	case RepresentationCoverageStatus::IntentionallyNonGeometric:
	    return "intentionally_non_geometric";
	case RepresentationCoverageStatus::Skipped: return "skipped";
	case RepresentationCoverageStatus::Malformed: return "malformed";
	case RepresentationCoverageStatus::Unsupported: return "unsupported";
	default: return "unclassified";
    }
}

struct RepresentationItemCoverage {
    int64_t entity_id = 0;
    std::string type;
    RepresentationCoverageStatus status =
	RepresentationCoverageStatus::Unclassified;
    std::string reason;
};

/** Exhaustive coverage record for one product-bound STEP representation.
 * This is deliberately separate from Representation, whose map also records
 * converted representation items for naming and product membership. */
struct RepresentationCoverage {
    int64_t entity_id = 0;
    int64_t product_id = 0;
    std::string type;
    RepresentationCoverageStatus status =
	RepresentationCoverageStatus::Unclassified;
    std::string reason;
    std::vector<RepresentationItemCoverage> items;
    uint64_t handled_items = 0;
    uint64_t preserved_invalid_items = 0;
    uint64_t filtered_items = 0;
    uint64_t intentionally_non_geometric_items = 0;
    uint64_t omitted_items = 0;
};

struct Representation {
    int64_t entity_id = 0;
    int64_t product_id = 0;
    std::string type;
    std::string output_name;
};

/** Physical context retained from one source representation.  These values
 * are provenance only: export never silently reuses them after the BRL-CAD
 * model may have been edited. */
struct RepresentationContextProvenance {
    int64_t representation_id = 0;
    int64_t product_id = 0;
    double length_unit_mm = 0.0;
    double plane_angle_unit_radians = 0.0;
    double solid_angle_unit_steradians = 0.0;
    bool has_uncertainty = false;
    double uncertainty_mm = 0.0;
};

struct Occurrence {
    int64_t entity_id = 0;
    int64_t usage_entity_id = 0;
    int64_t parent_product_id = 0;
    int64_t child_product_id = 0;
    /** AP203 assembly-shape strategy used to realize this occurrence. */
    std::string shape_method;
    double matrix[16] = {
	1.0, 0.0, 0.0, 0.0,
	0.0, 1.0, 0.0, 0.0,
	0.0, 0.0, 1.0, 0.0,
	0.0, 0.0, 0.0, 1.0
    };
};

/** Product-structure semantics retained independently of whether a usage has
 * an explicit geometric occurrence.  AP203 SHUO and promissory usages, for
 * example, carry useful parts-list/path information without necessarily
 * defining another BRL-CAD combination member. */
struct AssemblyUsage {
    int64_t entity_id = 0;
    std::string type;
    std::string identifier;
    std::string name;
    std::string description;
    std::string reference_designator;
    int64_t parent_product_id = 0;
    int64_t child_product_id = 0;
    int64_t upper_usage_id = 0;
    int64_t next_usage_id = 0;
    bool quantified = false;
    bool promissory = false;
    double quantity = 0.0;
    std::string quantity_unit;
    std::string source_record;
};

struct ProductAlternative {
    int64_t entity_id = 0;
    int64_t base_product_id = 0;
    int64_t alternate_product_id = 0;
    std::string basis;
    std::string source_record;
};

struct UsageSubstitute {
    int64_t entity_id = 0;
    int64_t base_usage_id = 0;
    int64_t substitute_usage_id = 0;
    std::string source_record;
};

/** A schema configuration-management node retained without claiming that its
 * application semantics can already be re-authored.  The exact Part 21
 * right-hand side preserves ordered/select/aggregate details; references make
 * the retained records a traversable graph rather than an opaque key/value
 * collection. */
struct ConfigurationRecord {
    int64_t entity_id = 0;
    std::string type;
    /** Source-ordered entity keywords for a Part 21 complex instance. */
    std::vector<std::string> component_types;
    std::string value;
    std::vector<int64_t> references;
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

/** One retained AP242 product-manufacturing-information graph node.  The
 * exact Part 21 right-hand side and ordered references are authoritative;
 * category and product_id are derived indexing aids.  native_object is set
 * only when the importer resolved enough geometry to create an equivalent
 * BRL-CAD datum or annotation object. */
struct PMIRecord {
    int64_t entity_id = 0;
    std::string type;
    std::vector<std::string> component_types;
    std::string category;
    std::string value;
    std::vector<int64_t> references;
    int64_t product_id = 0;
    std::string native_object;
    std::string native_kind;
    std::string native_status;
};

/** Schema-neutral, immutable-after-extraction import graph. */
struct Document {
    std::vector<std::string> schema_identifiers;
    std::map<int64_t, Product> products;
    std::map<int64_t, Representation> representations;
    std::map<int64_t, RepresentationContextProvenance>
	representation_contexts;
    /** Every representation attached to a product, whether or not its
     * geometry is supported by the active importer. */
    std::map<int64_t, RepresentationCoverage> representation_coverage;
    std::map<int64_t, Occurrence> occurrences;
    std::map<int64_t, AssemblyUsage> assembly_usages;
    std::map<int64_t, ProductAlternative> product_alternatives;
    std::map<int64_t, UsageSubstitute> usage_substitutes;
    std::map<int64_t, ConfigurationRecord> configuration_records;
    std::map<int64_t, Style> styles;
    std::map<int64_t, Layer> layers;
    /** Semantic, presentation, association, and supporting AP242 PMI nodes.
     * Semantic-only and graphical-only records remain distinct categories. */
    std::map<int64_t, PMIRecord> pmi_records;
    /** Legacy namespaced _GLOBAL view retained for compatibility.  New code
     * should consume configuration_records or the structured CONFIGURATION
     * attribute namespace rather than infer a graph from these keys. */
    std::map<std::string, std::string> global_attributes;
    /** True when entity_counts inventories the complete Part 21 source. */
    bool entity_counts_complete = true;
    /** Part 21 entity-keyword counts.  A complex instance contributes once
     * to every explicitly encoded component entity, matching the historical
     * STEP converter inventory rather than appearing as COMPLEX_ENTITY. */
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
    /** Geometry roots excluded by an explicit import filter, not by a
     * conversion failure. */
    uint64_t geometry_filtered = 0;
    /** complete, partial, empty, or failed after coverage finalization. */
    std::string outcome;
    uint64_t styles_extracted = 0;
    uint64_t styles_applied = 0;
    uint64_t layers_extracted = 0;
    uint64_t pmi_semantic_records = 0;
    uint64_t pmi_presentation_records = 0;
    uint64_t pmi_association_records = 0;
    uint64_t pmi_dependency_records = 0;
    uint64_t pmi_native_datums = 0;
    uint64_t pmi_native_annotations = 0;
    uint64_t pmi_invalid_records = 0;
    uint64_t materials_extracted = 0;
    uint64_t properties_extracted = 0;
    uint64_t properties_invalid = 0;
    uint64_t invalid_breps = 0;
    uint64_t invalid_breps_written = 0;
    uint64_t invalid_breps_rejected = 0;
    uint64_t output_failures = 0;
    uint64_t repairs = 0;
    uint64_t inferred_curves = 0;
    bool budget_calibration_ran = false;
    bool budget_calibration_valid = false;
    uint64_t budget_calibration_queries = 0;
    uint64_t budget_calibration_microseconds = 0;
    unsigned int budget_calibration_parallel_workers = 0;
    double budget_calibration_scalar_queries_per_second = 0.0;
    double budget_calibration_parallel_queries_per_second = 0.0;
    double budget_calibration_parallel_cpu_queries_per_second = 0.0;
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
    /** Portion of load_time_us spent inventorying indexed entity types. */
    int64_t inventory_time_us = 0;
    int64_t convert_time_us = 0;
    double tolerance_mm = 0.0;
    size_t peak_rss_bytes = 0;
};

} // namespace step
} // namespace brlcad

#endif /* CONV_STEP_STEPDOCUMENT_H */
