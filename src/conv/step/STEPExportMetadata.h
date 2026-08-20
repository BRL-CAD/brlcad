/*                S T E P E X P O R T M E T A D A T A . H
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 */

#ifndef CONV_STEP_STEPEXPORTMETADATA_H
#define CONV_STEP_STEPEXPORTMETADATA_H

#include <cstdint>
#include <string>
#include <vector>

struct AP203_Contents;

namespace brlcad {
namespace step {

struct StepExportPlan;

struct ExportMetadataStatistics {
    uint64_t products_updated = 0;
    uint64_t occurrences_updated = 0;
    uint64_t occurrences_omitted = 0;
    uint64_t styled_items_emitted = 0;
    uint64_t layers_emitted = 0;
    uint64_t presentation_omitted = 0;
    uint64_t materials_emitted = 0;
    uint64_t materials_omitted = 0;
    uint64_t material_properties_emitted = 0;
    uint64_t material_properties_omitted = 0;
    uint64_t product_properties_emitted = 0;
    uint64_t product_properties_omitted = 0;
    uint64_t configuration_records_seen = 0;
    uint64_t configuration_records_emitted = 0;
    uint64_t configuration_records_omitted = 0;
    uint64_t opaque_global_attributes_seen = 0;
};

/** Apply the schema-neutral metadata plan to products and their geometric
 * representation items.  Product identity is common to every supported AP;
 * presentation emission is compiled only where its entities are legal. */
bool ApplySTEPExportMetadata(StepExportPlan &plan,
    AP203_Contents *contents, std::vector<std::string> &diagnostics,
    ExportMetadataStatistics &statistics);

} // namespace step
} // namespace brlcad

#endif /* CONV_STEP_STEPEXPORTMETADATA_H */
