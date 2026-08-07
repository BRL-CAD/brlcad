/*            A P 2 0 3 M A N A G E M E N T E X P O R T . H
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 */

#ifndef CONV_STEP_AP203_AP203MANAGEMENTEXPORT_H
#define CONV_STEP_AP203_AP203MANAGEMENTEXPORT_H

#include <string>
#include <vector>

struct AP203_Contents;

namespace brlcad {
namespace step {

/** Complete the mandatory AP203 edition 1 administrative graph.
 *
 * AP203's global rules require substantially more than a PRODUCT and shape:
 * formations, products, definitions, approvals, dates, responsible parties,
 * classifications, categories, usages, and the application context all have
 * exact association requirements.  This schema-specific finalizer preserves
 * compatible retained associations already authored by the common metadata
 * pass and creates explicit neutral defaults only for missing mandatory
 * records.  Conflicting or over-assigned input is rejected rather than
 * hidden by another generated assignment.
 */
bool FinalizeAP203ManagementGraph(AP203_Contents *contents,
    std::vector<std::string> &diagnostics);

} // namespace step
} // namespace brlcad

#endif /* CONV_STEP_AP203_AP203MANAGEMENTEXPORT_H */
