/*         A P 2 0 3 C O N F I G U R A T I O N E X P O R T . H
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 */

#ifndef CONV_STEP_AP203_AP203CONFIGURATIONEXPORT_H
#define CONV_STEP_AP203_AP203CONFIGURATIONEXPORT_H

#include <cstdint>
#include <map>
#include <string>
#include <utility>

struct AP203_Contents;
class SDAI_Application_instance;
typedef SDAI_Application_instance STEPentity;

namespace brlcad {
namespace step {

struct ConfigurationExportStatistics;
struct StepExportPlan;

/** Author retained AP203 edition 1 complex configuration effectivities and
 * their configuration-item dependency chains. */
void EmitAP203ComplexConfiguration(StepExportPlan &plan,
    AP203_Contents *contents,
    const std::map<int64_t, STEPentity *> &formations,
    const std::map<int64_t, STEPentity *> &usages,
    const std::map<std::pair<std::string, int64_t>, STEPentity *> &date_times,
    const std::map<std::pair<std::string, int64_t>, STEPentity *> &measures,
    std::map<std::pair<std::string, int64_t>, STEPentity *> &effectivities,
    std::map<std::pair<std::string, int64_t>, std::string> &effectivity_types,
    ConfigurationExportStatistics &statistics);

} // namespace step
} // namespace brlcad

#endif /* CONV_STEP_AP203_AP203CONFIGURATIONEXPORT_H */
