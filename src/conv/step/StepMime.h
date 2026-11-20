/*                     S T E P M I M E . H
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 */

#ifndef CONV_STEP_STEPMIME_H
#define CONV_STEP_STEPMIME_H
#include "common.h"
#include <cstring>
#include <string>
#include "bu/mime_runtime.h"
#include "bu/str.h"
#include "../../libbu/mime_p21.h"

namespace brlcad {
namespace step {

struct MimeTraits {
    bu_mime_traits value = BU_MIME_TRAITS_INIT_ZERO;
    MimeTraits() = default;
    MimeTraits(const MimeTraits &) = delete;
    MimeTraits &operator=(const MimeTraits &) = delete;
    ~MimeTraits() {bu_mime_traits_free(&value);}
};

struct OutputSchemaProfile {
    const char *schema;
    const char *file_schema;
};

inline const OutputSchemaProfile *output_schema_profile(const std::string &schema)
{
    /* The writer and MIME preflight use the same STEP string literal. The
     * literal OID remains separate from its coarser application edition. */
    static const OutputSchemaProfile profiles[] = {
	{"ap203", "'CONFIG_CONTROL_DESIGN'"},
	{"ap203e2", "'AP203_CONFIGURATION_CONTROLLED_3D_DESIGN_OF_MECHANICAL_PARTS_AND_ASSEMBLIES_MIM_LF'"},
	{"ap214", "'AUTOMOTIVE_DESIGN {1 0 10303 214 3 1 1}'"},
	{"ap242e1", "'AP242_MANAGED_MODEL_BASED_3D_ENGINEERING_MIM_LF {1 0 10303 442 1 1 4}'"},
	{"ap242e2", "'AP242_MANAGED_MODEL_BASED_3D_ENGINEERING_MIM_LF {1 0 10303 442 3 1 4}'"},
	{"ap242e3", "'AP242_MANAGED_MODEL_BASED_3D_ENGINEERING_MIM_LF {1 0 10303 442 4 1 4}'"},
	{"ap242e4", "'AP242_MANAGED_MODEL_BASED_3D_ENGINEERING_MIM_LF {1 0 10303 442 5 1 4}'"}
    };
    for (const auto &profile : profiles)
	if (schema == profile.schema) return &profile;
    return NULL;
}

inline std::string step_mime_identity(const char *format, bool product_model)
{
    const char *specific = product_model ? "model/step" : "application/p21";
    if (!format) return specific;
    bu_mime_registry *registry = bu_mime_registry_create(0);
    bu_mime_result *names = NULL;
    bool specific_allowed = false, generic_allowed = false;
    if (registry && bu_mime_resolve_candidates(registry, format, &names) != BU_MIME_ERROR && names)
	for (size_t i = 0; i < names->count; ++i) {
	    specific_allowed = specific_allowed || BU_STR_EQUAL(names->candidates[i].name, specific);
	    generic_allowed = generic_allowed || BU_STR_EQUAL(names->candidates[i].name, "application/p21");
	}
    bu_mime_result_free(names);
    bu_mime_registry_destroy(registry);
    return specific_allowed ? specific : generic_allowed ? "application/p21" : "";
}

inline bool append_output_schema_traits(const std::string &schema, const std::string &type, bu_mime_traits &traits)
{
    const auto *profile = output_schema_profile(schema);
    if (!profile) return false;
    mime_p21::profile declared;
    /* All table entries are quoted ASCII identifiers without quote escapes. */
    declared.identifiers.emplace_back(profile->file_schema + 1, std::strlen(profile->file_schema) - 2);
    declared.complete = true;
    return type == "model/step" ? declared.append_step_traits(traits) :
	type == "application/p21" && declared.append_traits(traits);
}

} // namespace step
} // namespace brlcad
#endif
