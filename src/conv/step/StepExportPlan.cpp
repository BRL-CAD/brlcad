/*                     S T E P E X P O R T P L A N . C P P
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 */

#include "StepExportPlan.h"

#include "BRLCADWrapper.h"
#include "STEPMetadata.h"

#include <algorithm>
#include <cerrno>
#include <cctype>
#include <climits>
#include <cmath>
#include <cstdlib>
#include <map>
#include <sstream>
#include <set>

#include "bu/avs.h"
#include "bu/malloc.h"
#include "raytrace.h"
#include "rt/geom.h"
#include "vmath.h"

namespace {

using brlcad::step::ExportObjectPlan;
using brlcad::step::ExportOccurrencePlan;
using brlcad::step::ExportConfigurationRecordPlan;
using brlcad::step::StepExportPlan;

static std::string
trim(const std::string &value)
{
    size_t begin = 0;
    while (begin < value.size() &&
	std::isspace(static_cast<unsigned char>(value[begin]))) ++begin;
    size_t end = value.size();
    while (end > begin &&
	std::isspace(static_cast<unsigned char>(value[end - 1]))) --end;
    return value.substr(begin, end - begin);
}

static const std::string *
attribute(const std::map<std::string, std::string> &attributes,
    const char *name)
{
    const auto found = attributes.find(name);
    return found == attributes.end() ? NULL : &found->second;
}

static bool
parse_numbers(std::string value, double *numbers, size_t count)
{
    std::replace(value.begin(), value.end(), '/', ' ');
    std::replace(value.begin(), value.end(), ',', ' ');
    std::istringstream input(value);
    for (size_t i = 0; i < count; ++i)
	if (!(input >> numbers[i])) return false;
    std::string extra;
    return !(input >> extra);
}

static bool
parse_positive_id(const std::string &text, int64_t &value)
{
    errno = 0;
    char *end = NULL;
    const unsigned long long parsed = std::strtoull(text.c_str(), &end, 10);
    if (errno || !end || end == text.c_str() || *end || !parsed ||
	    parsed > static_cast<unsigned long long>(INT64_MAX)) return false;
    value = static_cast<int64_t>(parsed);
    return true;
}

static std::vector<int64_t>
part21_references(const std::string &value)
{
    std::vector<int64_t> references;
    bool quoted = false;
    for (size_t i = 0; i < value.size(); ++i) {
	if (value[i] == '\'') {
	    if (quoted && i + 1 < value.size() && value[i + 1] == '\'') {
		++i;
		continue;
	    }
	    quoted = !quoted;
	    continue;
	}
	if (quoted || value[i] != '#') continue;
	size_t end = i + 1;
	while (end < value.size() &&
		std::isdigit(static_cast<unsigned char>(value[end]))) ++end;
	int64_t reference = 0;
	if (end > i + 1 && parse_positive_id(
		value.substr(i + 1, end - i - 1), reference))
	    references.push_back(reference);
	i = end ? end - 1 : end;
    }
    return references;
}

static void
resolve_configuration_records(StepExportPlan &plan)
{
    struct PendingRecord {
	ExportConfigurationRecordPlan record;
	bool has_type = false;
	bool has_component_types = false;
	bool has_value = false;
	bool has_references = false;
    };
    std::map<std::pair<std::string, int64_t>, PendingRecord> pending;
    const std::string prefix = "STEP::";
    const std::string marker = "::CONFIGURATION::#";
    for (const auto &attribute : plan.global_attributes) {
	if (attribute.first.compare(0, prefix.size(), prefix) != 0) continue;
	const size_t marker_position = attribute.first.find(marker, prefix.size());
	if (marker_position == std::string::npos || marker_position == prefix.size())
	    continue;
	const size_t id_begin = marker_position + marker.size();
	const size_t field_begin = attribute.first.find("::", id_begin);
	if (field_begin == std::string::npos) {
	    plan.diagnostics.push_back("malformed retained STEP configuration key '" +
		attribute.first + "'");
	    continue;
	}
	int64_t entity_id = 0;
	if (!parse_positive_id(attribute.first.substr(id_begin,
		field_begin - id_begin), entity_id)) {
	    plan.diagnostics.push_back("invalid entity identifier in retained STEP "
		"configuration key '" + attribute.first + "'");
	    continue;
	}
	const std::string schema = attribute.first.substr(prefix.size(),
	    marker_position - prefix.size());
	const std::string field = attribute.first.substr(field_begin + 2);
	PendingRecord &item = pending[std::make_pair(schema, entity_id)];
	item.record.schema = schema;
	item.record.entity_id = entity_id;
	if (field == "TYPE") {
	    item.record.type = attribute.second;
	    item.has_type = true;
	} else if (field == "COMPONENT_TYPES") {
	    item.has_component_types = true;
	    std::istringstream components(attribute.second);
	    std::string component;
	    while (components >> component) {
		bool valid_component = !component.empty() &&
		    (std::isalpha(static_cast<unsigned char>(component[0])) ||
		     component[0] == '!');
		for (size_t i = 1; valid_component && i < component.size(); ++i)
		    valid_component = std::isalnum(
			static_cast<unsigned char>(component[i])) ||
			component[i] == '_' || component[i] == '-';
		if (!valid_component || std::find(item.record.component_types.begin(),
			item.record.component_types.end(), component) !=
			item.record.component_types.end()) {
		    item.record.valid = false;
		    item.record.error =
			"complex component types are malformed or duplicated";
		    break;
		}
		item.record.component_types.push_back(component);
	    }
	} else if (field == "VALUE") {
	    item.record.value = attribute.second;
	    item.has_value = true;
	} else if (field == "REFERENCES") {
	    item.has_references = true;
	    std::istringstream references(attribute.second);
	    std::string reference;
	    while (references >> reference) {
		int64_t reference_id = 0;
		if (!parse_positive_id(reference, reference_id)) {
		    item.record.valid = false;
		    item.record.error = "configuration references contain an invalid "
			"entity identifier";
		    break;
		}
		item.record.references.push_back(reference_id);
	    }
	} else {
	    item.record.valid = false;
	    item.record.error = "unknown configuration field '" + field + "'";
	}
    }
    /* Databases imported before the structured namespace was introduced have
     * STEP::<schema>::<type>::#<id> compatibility attributes.  Recover the
     * same graph without treating arbitrary STEP-prefixed keys as records. */
    for (const auto &attribute : plan.global_attributes) {
	if (attribute.first.compare(0, prefix.size(), prefix) != 0) continue;
	const size_t schema_end = attribute.first.find("::", prefix.size());
	if (schema_end == std::string::npos || schema_end == prefix.size()) continue;
	const size_t type_begin = schema_end + 2;
	const size_t type_end = attribute.first.find("::#", type_begin);
	if (type_end == std::string::npos || type_end == type_begin) continue;
	const size_t id_begin = type_end + 3;
	if (id_begin >= attribute.first.size() ||
		attribute.first.find("::", id_begin) != std::string::npos) continue;
	int64_t entity_id = 0;
	if (!parse_positive_id(attribute.first.substr(id_begin), entity_id))
	    continue;
	const std::string schema = attribute.first.substr(prefix.size(),
	    schema_end - prefix.size());
	const std::string type = attribute.first.substr(type_begin,
	    type_end - type_begin);
	PendingRecord &item = pending[std::make_pair(schema, entity_id)];
	item.record.schema = schema;
	item.record.entity_id = entity_id;
	if (!item.has_type) {
	    item.record.type = type;
	    item.has_type = true;
	} else if (item.record.type != type) {
	    item.record.valid = false;
	    item.record.error = "structured and compatibility types disagree";
	}
	if (!item.has_value) {
	    item.record.value = attribute.second;
	    item.has_value = true;
	} else if (item.record.value != attribute.second) {
	    item.record.valid = false;
	    item.record.error = "structured and compatibility values disagree";
	}
	if (!item.has_references)
	    item.record.references = part21_references(attribute.second);
    }
    for (auto &entry : pending) {
	PendingRecord &item = entry.second;
	if (!item.has_type || item.record.type.empty()) {
	    item.record.valid = false;
	    item.record.error = "configuration record has no type";
	} else if (!item.has_value) {
	    item.record.valid = false;
	    item.record.error = "configuration record has no Part 21 value";
	} else if (item.record.type == "COMPLEX" &&
		(!item.has_component_types || item.record.component_types.empty())) {
	    item.record.valid = false;
	    item.record.error = "complex configuration record has no component types";
	} else if (item.record.type != "COMPLEX" &&
		!item.record.component_types.empty()) {
	    item.record.valid = false;
	    item.record.error =
		"ordinary configuration record declares complex component types";
	}
	if (!item.record.valid)
	    plan.diagnostics.push_back("malformed retained STEP configuration #" +
		std::to_string(item.record.entity_id) + ": " + item.record.error);
	plan.configuration_records.push_back(item.record);
    }
}

static bool
parse_rgb(const std::map<std::string, std::string> &attributes,
    brlcad::step::ExportMetadataPlan &metadata)
{
    const std::string *value = attribute(attributes, "color");
    double rgb[3] = {0.0, 0.0, 0.0};
    if (value && parse_numbers(*value, rgb, 3)) {
	for (size_t i = 0; i < 3; ++i) {
	    if (rgb[i] < 0.0 || rgb[i] > 255.0) return false;
	    metadata.rgb[i] = rgb[i] / 255.0;
	}
	metadata.has_rgb = true;
	return true;
    }
    value = attribute(attributes, "step:color_rgb");
    if (!value || !parse_numbers(*value, rgb, 3)) return false;
    for (size_t i = 0; i < 3; ++i) {
	if (rgb[i] < 0.0 || rgb[i] > 1.0) return false;
	metadata.rgb[i] = rgb[i];
    }
    metadata.has_rgb = true;
    return true;
}

static bool
parse_transparency_value(const std::string &value, double &transparency)
{
    char *end = NULL;
    const double parsed = std::strtod(value.c_str(), &end);
    while (end && *end && std::isspace(static_cast<unsigned char>(*end))) ++end;
    if (!end || end == value.c_str() || *end || parsed < 0.0 || parsed > 1.0)
	return false;
    transparency = parsed;
    return true;
}

static bool
parse_shader_transparency(const std::string &shader, double &transparency)
{
    /* BRL-CAD shader strings commonly spell this as "tr value", "tr=value"
     * or inside a brace-delimited argument list.  Match a token, not an
     * arbitrary occurrence inside another word. */
    for (size_t i = 0; i + 2 <= shader.size(); ++i) {
	if (std::tolower(static_cast<unsigned char>(shader[i])) != 't' ||
		std::tolower(static_cast<unsigned char>(shader[i + 1])) != 'r')
	    continue;
	if (i && (std::isalnum(static_cast<unsigned char>(shader[i - 1])) ||
		shader[i - 1] == '_')) continue;
	size_t value = i + 2;
	if (value < shader.size() &&
		(std::isalnum(static_cast<unsigned char>(shader[value])) ||
		 shader[value] == '_')) continue;
	while (value < shader.size() &&
		(std::isspace(static_cast<unsigned char>(shader[value])) ||
		 shader[value] == '=' || shader[value] == '{')) ++value;
	char *end = NULL;
	const double parsed = std::strtod(shader.c_str() + value, &end);
	if (end != shader.c_str() + value && parsed >= 0.0 && parsed <= 1.0) {
	    transparency = parsed;
	    return true;
	}
    }
    return false;
}

static bool
parse_unit_real(const std::string &text, double &value)
{
    char *end = NULL;
    value = std::strtod(text.c_str(), &end);
    while (end && *end && std::isspace(static_cast<unsigned char>(*end))) ++end;
    return end && end != text.c_str() && !*end && std::isfinite(value);
}

static bool
parse_unit_structure_node(const std::map<std::string, std::string> &fields,
    const std::string &prefix, brlcad::step::UnitStructure &unit,
    std::set<std::string> &consumed, std::string &error, size_t depth)
{
    if (depth > 16) {
	error = "the retained STEP unit graph exceeds 16 nested levels";
	return false;
    }
    unit = brlcad::step::UnitStructure();
    const auto field = [&fields, &prefix, &consumed](const char *name) ->
	const std::string * {
	const std::string key = prefix + name;
	const auto found = fields.find(key);
	if (found == fields.end()) return NULL;
	consumed.insert(key);
	return &found->second;
    };
    const std::string *kind = field("kind");
    if (!kind) {
	error = "the retained STEP unit graph has no kind at '" + prefix + "kind'";
	return false;
    }
    unit.kind = trim(*kind);
    std::transform(unit.kind.begin(), unit.kind.end(), unit.kind.begin(),
	[](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    if (unit.kind != "si" && unit.kind != "context" &&
	    unit.kind != "conversion" && unit.kind != "derived" &&
	    unit.kind != "named") {
	error = "unknown retained STEP unit kind '" + unit.kind + "'";
	return false;
    }
    const std::string *value = field("subtype");
    if (value) unit.subtype = trim(*value);
    value = field("name");
    if (value) unit.name = *value;
    value = field("prefix");
    if (value) unit.prefix = trim(*value);
    value = field("dimensions");
    if (value) {
	double dimensions[7];
	if (!parse_numbers(*value, dimensions, 7)) {
	    error = "the retained STEP unit dimensions are not seven numbers";
	    return false;
	}
	for (size_t i = 0; i < 7; ++i) {
	    if (!std::isfinite(dimensions[i])) {
		error = "the retained STEP unit dimensions contain a non-finite value";
		return false;
	    }
	    unit.dimension_exponents[i] = dimensions[i];
	}
	unit.has_dimensions = true;
    }
    value = field("conversion_value_type");
    if (value) unit.conversion_value_type = trim(*value);
    value = field("conversion_value");
    if (value) {
	if (!parse_unit_real(*value, unit.conversion_value)) {
	    error = "the retained STEP conversion value is not a finite number";
	    return false;
	}
	unit.has_conversion_value = true;
    }

    std::set<size_t> component_ordinals;
    const std::string component_prefix = prefix + "component:";
    for (const auto &entry : fields) {
	if (entry.first.compare(0, component_prefix.size(), component_prefix) != 0)
	    continue;
	const size_t end = entry.first.find(':', component_prefix.size());
	if (end == std::string::npos) continue;
	const std::string ordinal_text = entry.first.substr(component_prefix.size(),
	    end - component_prefix.size());
	char *ordinal_end = NULL;
	const unsigned long ordinal = std::strtoul(ordinal_text.c_str(),
	    &ordinal_end, 10);
	if (ordinal_end && !*ordinal_end && ordinal) component_ordinals.insert(ordinal);
    }
    size_t expected = 1;
    for (size_t ordinal : component_ordinals) {
	if (ordinal != expected++ || ordinal > 64) {
	    error = "retained STEP unit components must be contiguous from 1 and limited to 64";
	    return false;
	}
	const std::string child_prefix = component_prefix +
	    std::to_string(ordinal) + ':';
	const std::string exponent_key = child_prefix + "exponent";
	const auto exponent = fields.find(exponent_key);
	if (exponent != fields.end()) {
	    double parsed = 0.0;
	    consumed.insert(exponent_key);
	    if (!parse_unit_real(exponent->second, parsed)) {
		error = "a retained STEP derived-unit exponent is not finite";
		return false;
	    }
	    unit.exponents.push_back(parsed);
	} else {
	    unit.exponents.push_back(1.0);
	}
	brlcad::step::UnitStructure child;
	if (!parse_unit_structure_node(fields, child_prefix, child, consumed,
		error, depth + 1)) return false;
	unit.components.push_back(child);
    }

    if (unit.kind == "si") {
	if (unit.name.empty() || !unit.components.empty() ||
		unit.has_conversion_value) {
	    error = "an SI unit needs a name and cannot have components or a conversion value";
	    return false;
	}
    } else if (unit.kind == "context" || unit.kind == "named") {
	if (unit.kind == "context" && unit.name.empty()) {
	    error = "a context-dependent unit needs a name";
	    return false;
	}
	if (!unit.components.empty() || unit.has_conversion_value) {
	    error = "a context/named unit cannot have components or a conversion value";
	    return false;
	}
    } else if (unit.kind == "conversion") {
	if (unit.name.empty() || !unit.has_conversion_value ||
		unit.conversion_value_type.empty() || unit.components.size() != 1) {
	    error = "a conversion unit needs a name, typed value, and one factor unit";
	    return false;
	}
    } else if (unit.components.empty()) {
	error = "a derived unit needs at least one component";
	return false;
    }
    if (unit.kind != "derived" && !unit.exponents.empty()) {
	for (double exponent : unit.exponents) {
	    if (std::fabs(exponent - 1.0) > 1.0e-12) {
		error = "only a derived unit can assign component exponents";
		return false;
	    }
	}
    }
    return true;
}

static bool
parse_unit_structure(const std::map<std::string, std::string> &fields,
    brlcad::step::UnitStructure &unit, bool &requested, std::string &error)
{
    requested = false;
    for (const auto &entry : fields)
	if (entry.first.compare(0, 5, "unit:") == 0) requested = true;
    if (!requested) return true;
    std::set<std::string> consumed;
    if (!parse_unit_structure_node(fields, "unit:", unit, consumed, error, 0))
	return false;
    for (const auto &entry : fields) {
	if (entry.first.compare(0, 5, "unit:") == 0 && !consumed.count(entry.first)) {
	    error = "unknown retained STEP unit field '" + entry.first + "'";
	    return false;
	}
    }
    return true;
}

static std::vector<brlcad::step::ExportPropertyPlan>
parse_product_properties(const std::map<std::string, std::string> &attributes,
    bool &requested)
{
    const std::string prefix = "step:property:";
    std::map<size_t, brlcad::step::ExportPropertyPlan> properties;
    std::map<size_t, std::map<std::string, std::string> > fields;
    requested = false;
    for (const auto &entry : attributes) {
	if (entry.first.compare(0, prefix.size(), prefix) != 0) continue;
	requested = true;
	const size_t ordinal_end = entry.first.find(':', prefix.size());
	if (ordinal_end == std::string::npos) continue;
	const std::string number = entry.first.substr(prefix.size(),
	    ordinal_end - prefix.size());
	char *end = NULL;
	const unsigned long parsed = std::strtoul(number.c_str(), &end, 10);
	if (!end || *end || !parsed) continue;
	const std::string field = entry.first.substr(ordinal_end + 1);
	fields[parsed][field] = entry.second;
	brlcad::step::ExportPropertyPlan &property = properties[parsed];
	property.ordinal = parsed;
	if (field == "category") property.category = entry.second;
	else if (field == "name") property.name = entry.second;
	else if (field == "description") property.description = entry.second;
	else if (field == "value_type") property.value_type = entry.second;
	else if (field == "units") property.units = entry.second;
	else if (field == "text") property.text = entry.second;
	else if (field == "values") property.values = entry.second;
	else if (field == "dimensions") property.dimensions = entry.second;
    }

    std::vector<brlcad::step::ExportPropertyPlan> result;
    for (auto &entry : properties) {
	brlcad::step::ExportPropertyPlan property = entry.second;
	const auto property_fields = fields.find(entry.first);
	if (property_fields != fields.end()) {
	    property.unit_structure_malformed = !parse_unit_structure(
		property_fields->second, property.unit_structure,
		property.unit_structure_requested,
		property.unit_structure_error);
	}
	result.push_back(property);
    }
    return result;
}

static void
resolve_metadata(ExportObjectPlan &object)
{
    brlcad::step::ExportMetadataPlan &metadata = object.metadata;
    const auto copy = [&object, &metadata](const char *key, std::string &field) {
	const std::string *value = attribute(object.attributes, key);
	if (!value) return;
	field = *value;
	metadata.has_step_provenance = true;
    };
    copy("step:original_name", metadata.product_name);
    if (metadata.product_name.empty()) metadata.product_name = object.name;
    copy("step:product_id", metadata.product_identifier);
    if (metadata.product_identifier.empty())
	metadata.product_identifier = metadata.product_name;
    copy("step:description", metadata.product_description);
    copy("step:revision", metadata.revision);
    copy("step:revision_description", metadata.revision_description);
    copy("step:definition_id", metadata.definition_identifier);
    copy("step:definition_description", metadata.definition_description);
    copy("step:style_name", metadata.style_name);
    std::map<size_t, brlcad::step::ExportMaterialPlan> materials;
    std::map<size_t, std::map<size_t,
	brlcad::step::ExportPropertyPlan> > material_properties;
    std::map<size_t, std::map<size_t,
	std::map<std::string, std::string> > > material_property_fields;
    const std::string material_prefix = "step:material:";
    for (const auto &entry : object.attributes) {
	if (entry.first.compare(0, material_prefix.size(), material_prefix) != 0)
	    continue;
	const size_t ordinal_end = entry.first.find(':', material_prefix.size());
	if (ordinal_end == std::string::npos) continue;
	const std::string number = entry.first.substr(material_prefix.size(),
	    ordinal_end - material_prefix.size());
	char *end = NULL;
	const unsigned long parsed = std::strtoul(number.c_str(), &end, 10);
	if (!end || *end || !parsed) continue;
	const std::string field = entry.first.substr(ordinal_end + 1);
	brlcad::step::ExportMaterialPlan &material = materials[parsed];
	material.ordinal = parsed;
	if (field == "id") material.identifier = entry.second;
	else if (field == "name") material.name = entry.second;
	else if (field == "description") material.description = entry.second;
	else if (field.compare(0, 9, "property:") == 0) {
	    const size_t property_begin = 9;
	    const size_t property_end = field.find(':', property_begin);
	    if (property_end == std::string::npos) continue;
	    const std::string property_number = field.substr(property_begin,
		property_end - property_begin);
	    char *property_number_end = NULL;
	    const unsigned long property_ordinal = std::strtoul(
		property_number.c_str(), &property_number_end, 10);
	    if (!property_number_end || *property_number_end ||
		!property_ordinal) continue;
	    const std::string property_field = field.substr(property_end + 1);
	    material_property_fields[parsed][property_ordinal][property_field] =
		entry.second;
	    brlcad::step::ExportPropertyPlan &property =
		material_properties[parsed][property_ordinal];
	    property.ordinal = property_ordinal;
	    if (property_field == "category") property.category = entry.second;
	    else if (property_field == "name") property.name = entry.second;
	    else if (property_field == "description")
		property.description = entry.second;
	    else if (property_field == "value_type")
		property.value_type = entry.second;
	    else if (property_field == "units") property.units = entry.second;
	    else if (property_field == "text") property.text = entry.second;
	    else if (property_field == "values") property.values = entry.second;
	    else if (property_field == "dimensions")
		property.dimensions = entry.second;
	    else continue;
	} else continue;
	metadata.has_step_provenance = true;
    }
    const std::string *material_id = attribute(object.attributes,
	"step:material_id");
    const std::string *material_name = attribute(object.attributes,
	"step:material_name");
    const std::string *material_description = attribute(object.attributes,
	"step:material_description");
    if (material_id || material_name || material_description) {
	brlcad::step::ExportMaterialPlan &material = materials[1];
	material.ordinal = 1;
	if (material_id) material.identifier = *material_id;
	if (material_name) material.name = *material_name;
	if (material_description) material.description = *material_description;
	metadata.has_step_provenance = true;
    }
    for (auto &entry : materials) {
	const auto properties = material_properties.find(entry.first);
	if (properties != material_properties.end()) {
	    for (const auto &property : properties->second) {
		brlcad::step::ExportPropertyPlan planned = property.second;
		const auto material_fields = material_property_fields.find(entry.first);
		if (material_fields != material_property_fields.end()) {
		    const auto property_fields = material_fields->second.find(property.first);
		    if (property_fields != material_fields->second.end()) {
			planned.unit_structure_malformed = !parse_unit_structure(
			    property_fields->second, planned.unit_structure,
			    planned.unit_structure_requested,
			    planned.unit_structure_error);
		    }
		}
		entry.second.properties.push_back(planned);
	    }
	}
	metadata.materials.push_back(entry.second);
    }
    metadata.material_requested = !metadata.materials.empty();
    metadata.properties = parse_product_properties(object.attributes,
	metadata.property_requested);
    if (metadata.property_requested) metadata.has_step_provenance = true;

    const bool rgb_requested = attribute(object.attributes, "color") ||
	attribute(object.attributes, "step:color_rgb");
    metadata.presentation_requested = rgb_requested;
    if (rgb_requested && !parse_rgb(object.attributes, metadata)) {
	metadata.presentation_malformed = true;
	metadata.presentation_error = "RGB colour is not three values in its legal range";
    }
    if (attribute(object.attributes, "step:color_rgb"))
	metadata.has_step_provenance = true;

    const std::string *transparent = attribute(object.attributes,
	"step:transparency");
    if (transparent) {
	metadata.presentation_requested = true;
	metadata.has_step_provenance = true;
	if (parse_transparency_value(*transparent, metadata.transparency)) {
	    metadata.has_transparency = true;
	} else {
	    metadata.presentation_malformed = true;
	    if (!metadata.presentation_error.empty())
		metadata.presentation_error += "; ";
	    metadata.presentation_error +=
		"transparency is not a number from zero through one";
	}
    } else {
	const std::string *shader = attribute(object.attributes, "shader");
	if (shader && parse_shader_transparency(*shader, metadata.transparency)) {
	    metadata.has_transparency = true;
	    metadata.presentation_requested = true;
	}
    }

    const std::string *layers = attribute(object.attributes, "step:layers");
    if (layers) {
	metadata.presentation_requested = true;
	metadata.has_step_provenance = true;
	size_t begin = 0;
	while (begin <= layers->size()) {
	    size_t end = layers->find(';', begin);
	    if (end == std::string::npos) end = layers->size();
	    const std::string layer = trim(layers->substr(begin, end - begin));
	    if (!layer.empty() && std::find(metadata.layers.begin(),
		metadata.layers.end(), layer) == metadata.layers.end())
		metadata.layers.push_back(layer);
	    if (end == layers->size()) break;
	    begin = end + 1;
	}
    }
}

struct Planner {
    StepExportPlan &plan;
    struct db_i *database;
    std::map<std::string, size_t> object_indices;
    std::set<std::string> active_combinations;

    size_t object(struct directory *entry)
    {
	const std::string name = entry->d_namep;
	auto found = object_indices.find(name);
	if (found != object_indices.end()) return found->second;

	ExportObjectPlan object_plan;
	object_plan.name = name;
	object_plan.combination = (entry->d_flags & RT_DIR_COMB) != 0;
	struct bu_attribute_value_set attributes = BU_AVS_INIT_ZERO;
	if (db5_get_attributes(database, &attributes, entry) == 0) {
	    struct bu_attribute_value_pair *pair = NULL;
	    for (BU_AVS_FOR(pair, &attributes)) {
		if (pair->name && pair->value)
		    object_plan.attributes[pair->name] = pair->value;
	    }
	}
	bu_avs_free(&attributes);
	resolve_metadata(object_plan);

	struct rt_db_internal internal;
	RT_DB_INTERNAL_INIT(&internal);
	if (rt_db_get_internal(&internal, entry, database, bn_mat_identity) >= 0) {
	    object_plan.primitive_type = internal.idb_type;
	    if (internal.idb_minor_type == DB5_MINORTYPE_BRLCAD_COMBINATION &&
		    internal.idb_ptr) {
		const struct rt_comb_internal *combination =
		    static_cast<const struct rt_comb_internal *>(internal.idb_ptr);
		RT_CK_COMB(combination);
		object_plan.region = combination->region_flag != 0;
	    }
	    if (internal.idb_type == ID_BREP && internal.idb_ptr) {
		struct rt_brep_internal *brep_internal =
		    static_cast<struct rt_brep_internal *>(internal.idb_ptr);
		RT_BREP_CK_MAGIC(brep_internal);
		if (brep_internal->brep) {
		    object_plan.brep_vertices = brep_internal->brep->m_V.Count();
		    object_plan.brep_edges = brep_internal->brep->m_E.Count();
		    object_plan.brep_faces = brep_internal->brep->m_F.Count();
		    object_plan.nurbs_curves = brep_internal->brep->m_C2.Count() +
			brep_internal->brep->m_C3.Count();
		    object_plan.nurbs_surfaces = brep_internal->brep->m_S.Count();
		}
	    }
	    rt_db_free_internal(&internal);
	}

	const size_t index = plan.objects.size();
	plan.objects.push_back(object_plan);
	object_indices[name] = index;
	return index;
    }

    void tree(size_t parent, const union tree *node,
	std::vector<int> &boolean_operations, std::string &branch_path,
	size_t &occurrence_ordinal)
    {
	if (!node) return;
	switch (node->tr_op) {
	    case OP_DB_LEAF: {
		struct directory *child_entry =
		    db_lookup(database, node->tr_l.tl_name, LOOKUP_QUIET);
		if (child_entry == RT_DIR_NULL) {
		    plan.diagnostics.push_back(std::string("missing combination member '") +
			node->tr_l.tl_name + "'");
		    return;
		}
		const size_t child = object(child_entry);
		ExportOccurrencePlan occurrence;
		occurrence.parent = parent;
		occurrence.child = child;
		occurrence.ordinal = ++occurrence_ordinal;
		occurrence.boolean_operation = boolean_operations.empty() ?
		    OP_DB_LEAF : boolean_operations.back();
		occurrence.boolean_operations = boolean_operations;
		occurrence.branch_path = branch_path;
		if (node->tr_l.tl_mat)
		    MAT_COPY(occurrence.transform.data(), node->tr_l.tl_mat);
		else
		    MAT_COPY(occurrence.transform.data(), bn_mat_identity);
		const std::string prefix = "step:occurrence:" +
		    std::to_string(occurrence.ordinal) + ':';
		const auto occurrence_value = [this, parent, &prefix](const char *suffix,
			std::string &value) {
		    const std::string *source = attribute(
			plan.objects[parent].attributes, (prefix + suffix).c_str());
		    if (source) value = *source;
		    return source != NULL;
		};
		std::string expected_child;
		const bool has_child = occurrence_value("child", expected_child);
		std::string source_entity_id;
		if (occurrence_value("source_id", source_entity_id)) {
		    int64_t parsed = 0;
		    if (parse_positive_id(source_entity_id, parsed))
			occurrence.source_entity_id = parsed;
		}
		const std::string *parent_role = attribute(
		    plan.objects[parent].attributes, "step:object_role");
		const std::string *child_role = attribute(
		    plan.objects[child].attributes, "step:object_role");
		occurrence.representation_membership =
		    occurrence.source_entity_id == 0 && parent_role && child_role &&
		    *parent_role == "product" &&
		    *child_role == "representation_item";
		occurrence.metadata_requested =
		    occurrence_value("id", occurrence.identifier) |
		    occurrence_value("name", occurrence.name) |
		    occurrence_value("description", occurrence.description) |
		    occurrence_value("reference_designator",
			occurrence.reference_designator);
		if (occurrence.metadata_requested &&
			(!has_child || expected_child != plan.objects[child].name)) {
		    occurrence.metadata_valid = false;
		    occurrence.metadata_error = has_child ?
			"stored child '" + expected_child + "' no longer matches '" +
			plan.objects[child].name + "'" :
			"stored occurrence metadata has no child identity";
		}
		plan.occurrences.push_back(occurrence);
		combination(child_entry, child);
		break;
	    }
	    case OP_UNION:
	    case OP_INTERSECT:
	    case OP_SUBTRACT:
	    case OP_XOR: {
		boolean_operations.push_back(node->tr_op);
		branch_path.push_back('L');
		tree(parent, node->tr_b.tb_left, boolean_operations, branch_path,
		    occurrence_ordinal);
		branch_path.back() = 'R';
		tree(parent, node->tr_b.tb_right, boolean_operations, branch_path,
		    occurrence_ordinal);
		branch_path.pop_back();
		boolean_operations.pop_back();
		break;
	    }
	    default:
		plan.diagnostics.push_back("unsupported boolean node in combination tree");
		break;
	}
    }

    void combination(struct directory *entry, size_t index)
    {
	if (!(entry->d_flags & RT_DIR_COMB)) return;
	const std::string name = entry->d_namep;
	if (!active_combinations.insert(name).second) {
	    plan.diagnostics.push_back("combination cycle at '" + name + "'");
	    return;
	}
	struct rt_db_internal internal;
	RT_DB_INTERNAL_INIT(&internal);
	if (rt_db_get_internal(&internal, entry, database, bn_mat_identity) >= 0 &&
	    internal.idb_minor_type == DB5_MINORTYPE_BRLCAD_COMBINATION) {
	    const struct rt_comb_internal *combination =
		static_cast<const struct rt_comb_internal *>(internal.idb_ptr);
	    std::vector<int> boolean_operations;
	    std::string branch_path;
	    size_t occurrence_ordinal = 0;
	    tree(index, combination->tree, boolean_operations, branch_path,
		occurrence_ordinal);
	    rt_db_free_internal(&internal);
	}
	active_combinations.erase(name);
    }
};

} // namespace

bool
brlcad::step::BuildStepExportPlan(StepExportPlan &plan,
                                  const std::string &input_path,
                                  const std::vector<std::string> &requested_objects,
                                  std::string &error)
{
    plan = StepExportPlan();
    plan.input_path = input_path;
    BRLCADWrapper input;
    std::string mutable_input_path = input_path;
    if (!input.load(mutable_input_path)) {
	error = "unable to open BRL-CAD input file";
	return false;
    }
    struct db_i *database = input.GetDBIP();

    bool binary_metadata_found = false;
    if (!brlcad::step::ReadSTEPMetadata(database, plan.global_attributes,
	    binary_metadata_found, error))
	return false;

    struct directory *global = db_lookup(database, DB5_GLOBAL_OBJECT_NAME,
	LOOKUP_QUIET);
    if (global != RT_DIR_NULL) {
	struct bu_attribute_value_set attributes = BU_AVS_INIT_ZERO;
	if (db5_get_attributes(database, &attributes, global) == 0) {
	    struct bu_attribute_value_pair *pair = NULL;
	    for (BU_AVS_FOR(pair, &attributes))
		if (pair->name && pair->value &&
			!brlcad::step::IsSTEPMetadataLocatorAttribute(pair->name))
		    plan.global_attributes[pair->name] = pair->value;
	}
	bu_avs_free(&attributes);
    }
    resolve_configuration_records(plan);
    Planner planner = {plan, database, {}, {}};

    std::vector<struct directory *> roots;
    if (requested_objects.empty()) {
	db_update_nref(database);
	struct directory **entries = NULL;
	const int count = db_ls(database, DB_LS_TOPS, NULL, &entries);
	for (int i = 0; i < count; ++i) {
	    if (binary_metadata_found && entries[i]->d_namep &&
		    std::string(entries[i]->d_namep) ==
		    brlcad::step::STEP_METADATA_OBJECT)
		continue;
	    roots.push_back(entries[i]);
	}
	if (entries) bu_free(entries, "STEP export top-level directory list");
    } else {
	for (const std::string &name : requested_objects) {
	    if (binary_metadata_found &&
		    name == brlcad::step::STEP_METADATA_OBJECT) {
		error = "retained STEP metadata object is not export geometry";
		return false;
	    }
	    struct directory *entry = db_lookup(database, name.c_str(), LOOKUP_QUIET);
	    if (entry == RT_DIR_NULL) {
		error = "cannot find requested BRL-CAD object '" + name + "'";
		return false;
	    }
	    roots.push_back(entry);
	}
    }
    if (roots.empty()) {
	error = "BRL-CAD input contains no selected objects";
	return false;
    }
    for (struct directory *entry : roots) {
	const size_t root = planner.object(entry);
	plan.roots.push_back(root);
	planner.combination(entry, root);
    }
    return true;
}
