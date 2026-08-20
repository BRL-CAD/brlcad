/*               S T E P R E P O R T . C P P
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 */

#include "common.h"

#include "STEPDocument.h"
#include "STEPReport.h"
#include "STEPString.h"

#include <iomanip>
#include <ostream>

namespace brlcad {
namespace step {

void
write_assembly_report_json(std::ostream &out, const Document &document)
{
    out << "\"assembly_usages\":[";
    bool first = true;
    for (std::map<int64_t, AssemblyUsage>::const_iterator usage =
	    document.assembly_usages.begin();
	 usage != document.assembly_usages.end(); ++usage) {
	if (!first) out << ',';
	first = false;
	out << "{\"entity_id\":" << usage->second.entity_id
	    << ",\"type\":\"" << json_escape(usage->second.type)
	    << "\",\"parent_product_id\":" << usage->second.parent_product_id
	    << ",\"child_product_id\":" << usage->second.child_product_id
	    << ",\"identifier\":\"" << json_escape(usage->second.identifier)
	    << "\",\"reference_designator\":\""
	    << json_escape(usage->second.reference_designator)
	    << "\",\"upper_usage_id\":" << usage->second.upper_usage_id
	    << ",\"next_usage_id\":" << usage->second.next_usage_id
	    << ",\"quantified\":" << (usage->second.quantified ? "true" : "false")
	    << ",\"promissory\":" << (usage->second.promissory ? "true" : "false")
	    << ",\"quantity\":" << usage->second.quantity
	    << ",\"quantity_unit\":\"" << json_escape(usage->second.quantity_unit)
	    << "\"}";
    }

    out << "],\n  \"occurrence_details\":[";
    first = true;
    for (std::map<int64_t, Occurrence>::const_iterator occurrence =
	    document.occurrences.begin(); occurrence != document.occurrences.end();
	 ++occurrence) {
	if (!first) out << ',';
	first = false;
	out << "{\"entity_id\":" << occurrence->second.entity_id
	    << ",\"usage_entity_id\":" << occurrence->second.usage_entity_id
	    << ",\"parent_product_id\":" << occurrence->second.parent_product_id
	    << ",\"child_product_id\":" << occurrence->second.child_product_id
	    << ",\"shape_method\":\"" << json_escape(occurrence->second.shape_method)
	    << "\"}";
    }

    out << "],\n  \"product_alternatives\":[";
    first = true;
    for (std::map<int64_t, ProductAlternative>::const_iterator alternate =
	    document.product_alternatives.begin();
	 alternate != document.product_alternatives.end(); ++alternate) {
	if (!first) out << ',';
	first = false;
	out << "{\"entity_id\":" << alternate->second.entity_id
	    << ",\"base_product_id\":" << alternate->second.base_product_id
	    << ",\"alternate_product_id\":" << alternate->second.alternate_product_id
	    << ",\"basis\":\"" << json_escape(alternate->second.basis) << "\"}";
    }

    out << "],\n  \"usage_substitutes\":[";
    first = true;
    for (std::map<int64_t, UsageSubstitute>::const_iterator substitute =
	    document.usage_substitutes.begin();
	 substitute != document.usage_substitutes.end(); ++substitute) {
	if (!first) out << ',';
	first = false;
	out << "{\"entity_id\":" << substitute->second.entity_id
	    << ",\"base_usage_id\":" << substitute->second.base_usage_id
	    << ",\"substitute_usage_id\":" << substitute->second.substitute_usage_id
	    << "}";
    }
    out << ']';
}

void
write_configuration_report_json(std::ostream &out, const Document &document)
{
    out << "\"configuration_records\":[";
    bool first = true;
    for (std::map<int64_t, ConfigurationRecord>::const_iterator entry =
	    document.configuration_records.begin();
	 entry != document.configuration_records.end(); ++entry) {
	if (!first) out << ',';
	first = false;
	out << "{\"entity_id\":" << entry->second.entity_id
	    << ",\"type\":\"" << json_escape(entry->second.type)
	    << "\",\"component_types\":[";
	for (size_t i = 0; i < entry->second.component_types.size(); ++i) {
	    if (i) out << ',';
	    out << '"' << json_escape(entry->second.component_types[i]) << '"';
	}
	out << "],\"value\":\"" << json_escape(entry->second.value)
	    << "\",\"references\":[";
	for (size_t i = 0; i < entry->second.references.size(); ++i) {
	    if (i) out << ',';
	    out << entry->second.references[i];
	}
	out << "]}";
    }

    out << "],\n  \"configuration_metadata\":{";
    first = true;
    for (std::map<std::string, std::string>::const_iterator attribute =
	    document.global_attributes.begin();
	 attribute != document.global_attributes.end(); ++attribute) {
	if (!first) out << ',';
	first = false;
	out << '\"' << json_escape(attribute->first) << "\":\""
	    << json_escape(attribute->second) << '\"';
    }
    out << '}';
}

namespace {

void
write_property_json(std::ostream &out, const MetadataProperty &property)
{
    out << "{\"entity_id\":" << property.entity_id
	<< ",\"representation_id\":" << property.representation_id
	<< ",\"item_entity_id\":" << property.item_entity_id
	<< ",\"category\":\"" << json_escape(property.category)
	<< "\",\"name\":\"" << json_escape(property.name)
	<< "\",\"description\":\"" << json_escape(property.description)
	<< "\",\"value_type\":\"" << json_escape(property.value_type)
	<< "\",\"units\":\"" << json_escape(property.units)
	<< "\",\"text\":\"" << json_escape(property.text)
	<< "\",\"valid\":" << (property.valid ? "true" : "false")
	<< ",\"error\":\"" << json_escape(property.error)
	<< "\",\"values\":[";
    for (size_t i = 0; i < property.values.size(); ++i) {
	if (i) out << ',';
	out << std::setprecision(17) << property.values[i];
    }
    out << "]}";
}

} // namespace

void
write_product_metadata_report_json(std::ostream &out,
    const Document &document)
{
    out << "\"product_metadata\":[";
    bool first_product = true;
    for (const auto &entry : document.products) {
	const Product &product = entry.second;
	if (product.materials.empty() && product.validation_properties.empty())
	    continue;
	if (!first_product) out << ',';
	first_product = false;
	out << "{\"entity_id\":" << product.entity_id
	    << ",\"materials\":[";
	for (size_t i = 0; i < product.materials.size(); ++i) {
	    if (i) out << ',';
	    const Material &material = product.materials[i];
	    out << "{\"usage_entity_id\":" << material.usage_entity_id
		<< ",\"definition_entity_id\":"
		<< material.definition_entity_id
		<< ",\"product_entity_id\":" << material.product_entity_id
		<< ",\"identifier\":\"" << json_escape(material.identifier)
		<< "\",\"name\":\"" << json_escape(material.name)
		<< "\",\"description\":\"" << json_escape(material.description)
		<< "\",\"properties\":[";
	    for (size_t j = 0; j < material.properties.size(); ++j) {
		if (j) out << ',';
		write_property_json(out, material.properties[j]);
	    }
	    out << "]}";
	}
	out << "],\"validation_properties\":[";
	for (size_t i = 0; i < product.validation_properties.size(); ++i) {
	    if (i) out << ',';
	    write_property_json(out, product.validation_properties[i]);
	}
	out << "]}";
    }
    out << ']';
}

void
write_pmi_report_json(std::ostream &out, const Document &document)
{
    out << "\"pmi\":{\"records\":[";
    bool first = true;
    for (const auto &entry : document.pmi_records) {
	const PMIRecord &record = entry.second;
	if (!first) out << ',';
	first = false;
	out << "{\"entity_id\":" << record.entity_id
	    << ",\"type\":\"" << json_escape(record.type)
	    << "\",\"component_types\":[";
	for (size_t i = 0; i < record.component_types.size(); ++i) {
	    if (i) out << ',';
	    out << '"' << json_escape(record.component_types[i]) << '"';
	}
	out << "],\"category\":\"" << json_escape(record.category)
	    << "\",\"value\":\"" << json_escape(record.value)
	    << "\",\"references\":[";
	for (size_t i = 0; i < record.references.size(); ++i) {
	    if (i) out << ',';
	    out << record.references[i];
	}
	out << "],\"product_id\":" << record.product_id
	    << ",\"native_object\":\"" << json_escape(record.native_object)
	    << "\",\"native_kind\":\"" << json_escape(record.native_kind)
	    << "\",\"native_status\":\"" << json_escape(record.native_status)
	    << "\"}";
    }
    out << "]}";
}


void
write_inferred_curve_report_json(std::ostream &out,
    const std::vector<InferredCurve> &curves)
{
    out << "\"inferred_curves\":[";
    for (size_t i = 0; i < curves.size(); ++i) {
	if (i) out << ',';
	const InferredCurve &curve = curves[i];
	out << "{\"edge_entity_id\":" << curve.edge_entity_id
	    << ",\"kind\":\"" << json_escape(curve.kind)
	    << "\",\"discrepancy_mm\":" << curve.discrepancy_mm
	    << ",\"safe_limit_mm\":" << curve.safe_limit_mm
	    << ",\"inference_limit_mm\":" << curve.inference_limit_mm
	    << ",\"declared_tolerance_mm\":"
	    << curve.declared_tolerance_mm << ",\"detail\":\""
	    << json_escape(curve.detail) << "\"}";
    }
    out << ']';
}

} // namespace step
} // namespace brlcad
