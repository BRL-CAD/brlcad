/*       S T E P S C H E M A R U N T I M E G E N E R A T E D . C P P
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * Schema-bound initializer adapter, compiled once into each schema module.
 */

#include "StepSchemaRuntime.h"

#include <schema.h>

#if defined(AP203e2) || defined(AP214e3) || defined(AP242)
#  include "STEPNativeCSGImport.h"
#  include "STEPPresentation.h"
#  include "STEPSweptSolid.h"
#  include "STEPWrapper.h"
#endif

#if defined(AP203e2) || defined(AP214e3) || defined(AP242)
#  include "STEPMaterialImport.h"
#endif

#ifdef AP214e3
#  include "AP214Presentation.h"
#endif

#ifdef AP242
#  include "AP242Tessellated.h"
#  include "AP242PMI.h"
#endif

#ifndef STEP_PLUGIN_SCHEMA_DISPLAY
#  error STEP_PLUGIN_SCHEMA_DISPLAY must name the schema runtime
#endif

#ifdef AP214e3
namespace {

void
mechanical_preprocess(STEPWrapper &wrapper)
{
    wrapper.SetProgress("extracting STEP presentation metadata");
    if (!ExtractSTEPPresentation(wrapper))
	ExtractAP214Presentation(wrapper);
    if (wrapper.HasLazyIndex() &&
	(!wrapper.LazyInstancesByType("DESIGN_CONTEXT").empty() ||
	 !wrapper.LazyInstancesByType("MECHANICAL_CONTEXT").empty())) {
	wrapper.RecordDiagnostic(brlcad::step::DiagnosticSeverity::Warning, 0,
	    "FILE_SCHEMA", std::string(),
	    "AP214 file uses legacy AP203 DESIGN_CONTEXT/MECHANICAL_CONTEXT names; "
	    "imported as their attribute-compatible AP214 parent contexts");
    }
}

void
ap214_post_index(STEPWrapper &wrapper, BRLCADWrapper &database,
    const std::vector<uint64_t> &handled_sdrs)
{
    wrapper.SetProgress("extracting STEP material metadata");
    ExtractSTEPMaterialMetadata(wrapper);
    wrapper.SetProgress("converting AP214 CSG solids");
    ImportSTEPNativeCSG(wrapper, database, handled_sdrs);
    wrapper.SetProgress("converting AP214 swept solids");
    ConvertSTEPSweptSolids(wrapper, database);
}

} // namespace
#elif defined(AP203e2) || defined(AP242)
namespace {

void
mechanical_preprocess(STEPWrapper &wrapper)
{
    wrapper.SetProgress("extracting STEP presentation metadata");
    ExtractSTEPPresentation(wrapper);
}

} // namespace
#endif

#ifdef AP203e2
namespace {

void
ap203e2_post_index(STEPWrapper &wrapper, BRLCADWrapper &database,
    const std::vector<uint64_t> &handled_sdrs)
{
    wrapper.SetProgress("extracting STEP material metadata");
    ExtractSTEPMaterialMetadata(wrapper);
    wrapper.SetProgress("converting AP203e2 CSG solids");
    ImportSTEPNativeCSG(wrapper, database, handled_sdrs);
    wrapper.SetProgress("converting AP203e2 swept solids");
    ConvertSTEPSweptSolids(wrapper, database);
}

} // namespace
#endif

#ifdef AP242
namespace {

void
ap242_post_index(STEPWrapper &wrapper, BRLCADWrapper &database,
    const std::vector<uint64_t> &handled_sdrs)
{
    wrapper.SetProgress("extracting STEP material metadata");
    ExtractSTEPMaterialMetadata(wrapper);
    wrapper.SetProgress("retaining AP242 PMI");
    ImportAP242PMI(wrapper, database);
    wrapper.SetProgress("converting AP242 tessellated geometry");
    /* The generic representation walk records tessellated associations as
     * visited even though it has no tessellated factory mapping.  Do not use
     * that visited set as an exclusion list for the AP242-specific decoder. */
    ImportAP242Tessellated(wrapper, database);
    wrapper.SetProgress("converting AP242 CSG solids");
    ImportSTEPNativeCSG(wrapper, database, handled_sdrs);
    wrapper.SetProgress("converting AP242 swept solids");
    ConvertSTEPSweptSolids(wrapper, database);
}

} // namespace
#endif

const brlcad::step::StepSchemaRuntime &
brlcad::step::CurrentStepSchemaRuntime()
{
#ifdef AP214e3
    static const std::vector<StepSchemaRuntime::Alias> aliases = {
	{"DESIGN_CONTEXT", "PRODUCT_DEFINITION_CONTEXT"},
	{"MECHANICAL_CONTEXT", "PRODUCT_CONTEXT"}
    };
    static const StepSchemaRuntime runtime(STEP_PLUGIN_SCHEMA_DISPLAY, SchemaInit, aliases,
	mechanical_preprocess, ap214_post_index);
#elif defined(AP242)
    static const StepSchemaRuntime runtime(STEP_PLUGIN_SCHEMA_DISPLAY, SchemaInit,
	std::vector<StepSchemaRuntime::Alias>(), mechanical_preprocess, ap242_post_index);
#elif defined(AP203e2)
    static const StepSchemaRuntime runtime(STEP_PLUGIN_SCHEMA_DISPLAY, SchemaInit,
	std::vector<StepSchemaRuntime::Alias>(), mechanical_preprocess, ap203e2_post_index);
#else
    static const StepSchemaRuntime runtime(STEP_PLUGIN_SCHEMA_DISPLAY, SchemaInit);
#endif
    return runtime;
}
