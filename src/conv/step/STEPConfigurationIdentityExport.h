/*   S T E P C O N F I G U R A T I O N I D E N T I T Y E X P O R T . H
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 */

#ifndef CONV_STEP_STEPCONFIGURATIONIDENTITYEXPORT_H
#define CONV_STEP_STEPCONFIGURATIONIDENTITYEXPORT_H

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

struct ConfigurationIdentityEntities {
    typedef std::pair<std::string, int64_t> SourceKey;

    std::map<SourceKey, STEPentity *> organizations;
    std::map<SourceKey, STEPentity *> people;
    std::map<SourceKey, STEPentity *> person_organizations;
    std::map<SourceKey, STEPentity *> classifications;
    std::map<SourceKey, STEPentity *> documents;
};

/** Author the schema-common person, organization, security-classification,
 * and document identity records and their supported assignments in a retained
 * configuration graph.  The implementation applies stricter AP203 edition 1
 * vocabulary, uniqueness, mandatory-attribute, dependent-instance, role/item,
 * and assignment-SELECT rules when that is the target schema. */
ConfigurationIdentityEntities EmitSTEPConfigurationIdentities(
    StepExportPlan &plan,
    AP203_Contents *contents,
    const std::map<int64_t, STEPentity *> &products,
    const std::map<int64_t, STEPentity *> &formations,
    const std::map<int64_t, STEPentity *> &definitions,
    const std::map<int64_t, STEPentity *> &usages,
    ConfigurationExportStatistics &statistics);

} // namespace step
} // namespace brlcad

#endif /* CONV_STEP_STEPCONFIGURATIONIDENTITYEXPORT_H */
