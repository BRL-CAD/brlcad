/*                       S T E P E X P O R T P L A N . H
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 */

#ifndef CONV_STEP_STEPEXPORTPLAN_H
#define CONV_STEP_STEPEXPORTPLAN_H

#include "common.h"
#include "STEPUnit.h"

#include <array>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace brlcad {
namespace step {

/** A typed STEP product property.  Material properties and properties attached
 * directly to the represented product use the same schema-neutral value and
 * unit model. */
struct ExportPropertyPlan {
    size_t ordinal = 0;
    std::string category;
    std::string name;
    std::string description;
    std::string value_type;
    std::string units;
    std::string text;
    std::string values;
    std::string dimensions;
    UnitStructure unit_structure;
    bool unit_structure_requested = false;
    bool unit_structure_malformed = false;
    std::string unit_structure_error;
};

struct ExportMaterialPlan {
    size_t ordinal = 0;
    std::string identifier;
    std::string name;
    std::string description;
    std::vector<ExportPropertyPlan> properties;
};

/** Schema-neutral product and presentation values resolved from BRL-CAD
 * object state.  Imported STEP provenance is deliberately represented by
 * named fields rather than being confused with opaque Part 21 records. */
struct ExportMetadataPlan {
    std::string product_name;
    std::string product_identifier;
    std::string product_description;
    std::string revision;
    std::string revision_description;
    std::string definition_identifier;
    std::string definition_description;
    std::string style_name;
    bool has_rgb = false;
    std::array<double, 3> rgb = {{0.0, 0.0, 0.0}};
    bool has_transparency = false;
    double transparency = 0.0;
    std::vector<std::string> layers;
    std::vector<ExportMaterialPlan> materials;
    std::vector<ExportPropertyPlan> properties;
    bool material_requested = false;
    bool property_requested = false;
    bool has_step_provenance = false;
    bool presentation_requested = false;
    bool presentation_malformed = false;
    std::string presentation_error;
};

struct ExportObjectPlan {
    std::string name;
    int primitive_type = 0;
    bool combination = false;
    /** BRL-CAD region intent distinguishes a Boolean solid from a product
     * assembly even when both trees happen to contain only unions. */
    bool region = false;
    std::map<std::string, std::string> attributes;
    ExportMetadataPlan metadata;
    size_t brep_vertices = 0;
    size_t brep_edges = 0;
    size_t brep_faces = 0;
    size_t nurbs_curves = 0;
    size_t nurbs_surfaces = 0;
};

struct ExportOccurrencePlan {
    size_t parent = 0;
    size_t child = 0;
    size_t ordinal = 0;
    int boolean_operation = 0;
    /* Exact root-to-leaf boolean-tree coordinates.  boolean_operations and
     * branch_path have equal length; each branch character is 'L' or 'R'. */
    std::vector<int> boolean_operations;
    std::string branch_path;
    std::array<double, 16> transform;
    /** Retained source usage identity for remapping configuration graph
     * edges.  It is never reused as an output Part 21 instance number. */
    int64_t source_entity_id = 0;
    /** True when an imported STEP product combination contains one of its
     * representation items.  This is shape layering, not a product usage. */
    bool representation_membership = false;
    bool metadata_requested = false;
    bool metadata_valid = true;
    std::string identifier;
    std::string name;
    std::string description;
    std::string reference_designator;
    std::string metadata_error;
};

/** A structured configuration record recovered from retained STEP metadata.
 * It is inventoried and reported until a schema-aware authoring path can
 * preserve its application semantics. */
struct ExportConfigurationRecordPlan {
    std::string schema;
    int64_t entity_id = 0;
    std::string type;
    std::vector<std::string> component_types;
    std::string value;
    std::vector<int64_t> references;
    bool valid = true;
    std::string error;
    /** Exhaustive disposition assigned by the schema-bound authoring pass.
     * pending is used only before that pass runs. */
    std::string export_status = "pending";
    std::string export_reason;
};

struct StepExportPlan {
    std::string input_path;
    std::vector<size_t> roots;
    std::vector<ExportObjectPlan> objects;
    std::vector<ExportOccurrencePlan> occurrences;
    /** Retained STEP values are loaded from the versioned metadata object and
     * overlaid with legacy _GLOBAL values for reporting/provenance decisions.
     * They are never treated as authored entity graphs merely because their
     * names begin with STEP::. */
    std::map<std::string, std::string> global_attributes;
    std::vector<ExportConfigurationRecordPlan> configuration_records;
    std::vector<std::string> diagnostics;
};

bool BuildStepExportPlan(StepExportPlan &plan,
                         const std::string &input_path,
                         const std::vector<std::string> &requested_objects,
                         std::string &error);

} // namespace step
} // namespace brlcad

#endif /* CONV_STEP_STEPEXPORTPLAN_H */
