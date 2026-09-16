/*          S T E P C O N F I G U R A T I O N E X P O R T . H
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 */

#ifndef CONV_STEP_STEPCONFIGURATIONEXPORT_H
#define CONV_STEP_STEPCONFIGURATIONEXPORT_H

#include <cstdint>
#include <string>
#include <vector>

struct AP203_Contents;

namespace brlcad {
namespace step {

struct StepExportPlan;

struct ConfigurationExportStatistics {
    uint64_t emitted = 0;
    uint64_t omitted = 0;
};

/** Author the lossless, schema-common subset of a retained configuration
 * graph.  Source entity numbers are graph identities only: all authored
 * references are connected directly to newly allocated entities. */
ConfigurationExportStatistics EmitSTEPConfigurationRecords(
    StepExportPlan &plan, AP203_Contents *contents,
    std::vector<std::string> &diagnostics);

} // namespace step
} // namespace brlcad

#endif /* CONV_STEP_STEPCONFIGURATIONEXPORT_H */
