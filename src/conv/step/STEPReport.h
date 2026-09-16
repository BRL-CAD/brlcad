/*                 S T E P R E P O R T . H
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 */

#ifndef CONV_STEP_STEPREPORT_H
#define CONV_STEP_STEPREPORT_H

#include "common.h"

#include <iosfwd>
#include <vector>

namespace brlcad {
namespace step {

struct Document;
struct InferredCurve;

/** Write the schema-neutral assembly and product-relationship report members.
 *
 * The function writes four complete JSON object members without a leading or
 * trailing comma: assembly_usages, occurrence_details, product_alternatives,
 * and usage_substitutes.  Keeping this serialization shared ensures every
 * schema plugin exposes the same evidence for hierarchy qualification.
 */
void write_assembly_report_json(std::ostream &out, const Document &document);

/** Write configuration_records and the legacy configuration_metadata view as
 * two complete JSON object members without a leading or trailing comma. */
void write_configuration_report_json(std::ostream &out,
    const Document &document);

/** Write the schema-neutral per-product material and validation-property
 * metadata as one complete product_metadata JSON object member without a
 * leading or trailing comma. */
void write_product_metadata_report_json(std::ostream &out,
    const Document &document);

/** Write the retained AP242 PMI association graph as one complete pmi JSON
 * object member without a leading or trailing comma. */
void write_pmi_report_json(std::ostream &out, const Document &document);

/** Write the top-level permissive curve-inference provenance array. */
void write_inferred_curve_report_json(std::ostream &out,
    const std::vector<InferredCurve> &curves);

} // namespace step
} // namespace brlcad

#endif /* CONV_STEP_STEPREPORT_H */
