/*                    S T E P M A T E R I A L E X P O R T . H
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 */

#ifndef CONV_STEP_STEP_MATERIAL_EXPORT_H
#define CONV_STEP_STEP_MATERIAL_EXPORT_H

#include "sdai.h"

#include <cstdint>
#include <string>
#include <vector>

struct AP203_Contents;

namespace brlcad {
namespace step {

struct ExportPropertyPlan;

struct ExportMaterialPlan;

struct MaterialExportResult {
    bool material_emitted = false;
    uint64_t properties_emitted = 0;
    uint64_t properties_omitted = 0;
    std::vector<std::string> diagnostics;
};

/** Emit the shared mechanical-AP product-structure form for one material
 * identity and its explicitly modeled properties.
 *
 * The material is a separate PRODUCT_DEFINITION related to the shape product
 * by MAKE_FROM_USAGE_OPTION.  Descriptive, numeric measure, and Cartesian
 * properties use PROPERTY_DEFINITION_REPRESENTATION graphs and retain an
 * explicit measure type, unit label, and dimensional exponents.  Known SI,
 * exact conversion, and derived expressions receive standardized unit graphs;
 * an opaque simple label uses CONTEXT_DEPENDENT_UNIT.
 */
MaterialExportResult EmitSTEPMaterialAssignment(
    const ExportMaterialPlan &material, STEPentity *shape_product_definition,
    AP203_Contents *contents);

/** Author one typed property directly on a product definition.  This shares
 * the material-property value and unit implementation without creating a
 * material PRODUCT or MAKE_FROM_USAGE_OPTION association. */
bool EmitSTEPProductProperty(const ExportPropertyPlan &property,
    STEPentity *shape_product_definition, AP203_Contents *contents,
    std::string &error);

} // namespace step
} // namespace brlcad

#endif /* CONV_STEP_STEP_MATERIAL_EXPORT_H */
