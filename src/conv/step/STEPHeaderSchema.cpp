/*                  S T E P H E A D E R S C H E M A . C P P
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 */

#include "STEPHeaderSchema.h"
#include "STEPString.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <set>
#include <sstream>

namespace {

std::string
upper_ascii(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
	return static_cast<char>(std::toupper(c));
    });
    return value;
}

std::string
trim(const std::string &value)
{
    size_t first = 0;
    while (first < value.size() && std::isspace(static_cast<unsigned char>(value[first]))) ++first;
    size_t last = value.size();
    while (last > first && std::isspace(static_cast<unsigned char>(value[last - 1]))) --last;
    return value.substr(first, last - first);
}

std::string
without_comments(const std::string &text)
{
    std::string output;
    bool comment = false;
    bool quoted = false;
    for (size_t i = 0; i < text.size();) {
	if (comment) {
	    if (i + 1 < text.size() && text[i] == '*' && text[i + 1] == '/') {
		comment = false;
		i += 2;
	    } else {
		if (text[i] == '\n' || text[i] == '\r') output.push_back(text[i]);
		++i;
	    }
	    continue;
	}
	if (!quoted && i + 1 < text.size() && text[i] == '/' && text[i + 1] == '*') {
	    comment = true;
	    i += 2;
	    continue;
	}
	output.push_back(text[i]);
	if (text[i] == '\'') {
	    if (quoted && i + 1 < text.size() && text[i + 1] == '\'') {
		output.push_back(text[i + 1]);
		i += 2;
		continue;
	    }
	    quoted = !quoted;
	}
	++i;
    }
    return output;
}

bool
schema_prefix(const std::string &value, const std::string &prefix)
{
    return value.compare(0, prefix.size(), prefix) == 0 &&
	(value.size() == prefix.size() ||
	 std::isspace(static_cast<unsigned char>(value[prefix.size()])) ||
	 value[prefix.size()] == '{');
}

brlcad::step::HeaderSchema
classify(const std::string &identifier, bool &legacy)
{
    const std::string value = upper_ascii(trim(identifier));
    if (schema_prefix(value,
	    "AP203_CONFIGURATION_CONTROLLED_3D_DESIGN_OF_MECHANICAL_PARTS_AND_ASSEMBLIES_MIM_LF"))
	return brlcad::step::HeaderSchema::AP203e2;
    if (schema_prefix(value, "CONFIGURATION_CONTROL_3D_DESIGN_MIM_LF") ||
	schema_prefix(value, "CCD_CLA_GVP_AST_ASD") ||
	schema_prefix(value, "CCD_CLA_GVP_AST")) {
	legacy = true;
	return brlcad::step::HeaderSchema::AP203e2;
    }
    if (schema_prefix(value, "CONFIG_CONTROL_DESIGN"))
	return brlcad::step::HeaderSchema::AP203;
    if (schema_prefix(value, "AUTOMOTIVE_DESIGN_CC2")) {
	legacy = true;
	return brlcad::step::HeaderSchema::AP214;
    }
    if (schema_prefix(value, "AUTOMOTIVE_DESIGN"))
	return brlcad::step::HeaderSchema::AP214;
    if (schema_prefix(value, "AP242_MANAGED_MODEL_BASED_3D_ENGINEERING_MIM_LF") ||
	schema_prefix(value, "MANAGED_MODEL_BASED_3D_ENGINEERING"))
	return brlcad::step::HeaderSchema::AP242;
    if (value.compare(0, 3, "IFC") == 0)
	return brlcad::step::HeaderSchema::IFC;
    return brlcad::step::HeaderSchema::Unknown;
}

bool
ap203_interim_companion(const std::string &identifier)
{
    const std::string value = upper_ascii(trim(identifier));
    return value == "GEOMETRIC_VALIDATION_PROPERTIES_MIM" ||
	value == "GEOMETRIC_VALIDATION_PROPERTY_REPRESENTATION_MIM" ||
	value == "SHAPE_APPEARANCE_LAYER_MIM" ||
	value == "SHAPE_APPEARANCE_LAYERS_MIM";
}

} // namespace

const char *
brlcad::step::STEPHeaderSchema::key(HeaderSchema schema)
{
    switch (schema) {
	case HeaderSchema::AP203: return "ap203";
	case HeaderSchema::AP203e2: return "ap203e2";
	case HeaderSchema::AP214: return "ap214";
	case HeaderSchema::AP242: return "ap242";
	case HeaderSchema::IFC: return "ifc";
	default: return "unknown";
    }
}

brlcad::step::HeaderSchemaInfo
brlcad::step::STEPHeaderSchema::inspect_file(const std::string &path)
{
    HeaderSchemaInfo result;
    std::ifstream input(path.c_str(), std::ios::binary);
    if (!input) {
	result.error = "unable to read STEP input";
	return result;
    }

    const size_t max_header = 16U * 1024U * 1024U;
    std::string header;
    header.reserve(64U * 1024U);
    char buffer[8192];
    while (input && header.size() < max_header) {
	input.read(buffer, sizeof(buffer));
	const std::streamsize count = input.gcount();
	if (count > 0) header.append(buffer, static_cast<size_t>(count));
	const std::string normalized = upper_ascii(without_comments(header));
	const size_t header_pos = normalized.find("HEADER;");
	if (header_pos != std::string::npos &&
	    normalized.find("ENDSEC;", header_pos) != std::string::npos)
	    return inspect_header(header);
    }
    result.error = "missing or over-large ISO 10303-21 HEADER section";
    return result;
}

brlcad::step::HeaderSchemaInfo
brlcad::step::STEPHeaderSchema::inspect_header(const std::string &input)
{
    HeaderSchemaInfo result;
    const std::string header = without_comments(input);
    const std::string upper = upper_ascii(header);
    const size_t header_start = upper.find("HEADER;");
    const size_t header_end = header_start == std::string::npos ?
	std::string::npos : upper.find("ENDSEC;", header_start);
    if (header_start == std::string::npos || header_end == std::string::npos) {
	result.error = "malformed STEP HEADER section";
	return result;
    }
    const size_t key_pos = upper.find("FILE_SCHEMA", header_start);
    if (key_pos == std::string::npos || key_pos > header_end) {
	result.error = "STEP header has no FILE_SCHEMA declaration";
	return result;
    }
    const size_t open = header.find('(', key_pos);
    if (open == std::string::npos || open > header_end) {
	result.error = "malformed FILE_SCHEMA declaration";
	return result;
    }

    bool quoted = false;
    std::string token;
    for (size_t i = open + 1; i < header_end; ++i) {
	const char c = header[i];
	if (!quoted) {
	    if (c == '\'') {
		quoted = true;
		token.assign(1, '\'');
	    }
	    continue;
	}
	token.push_back(c);
	if (c == '\'') {
	    if (i + 1 < header_end && header[i + 1] == '\'') {
		token.push_back(header[++i]);
		continue;
	    }
	    quoted = false;
	    result.identifiers.push_back(trim(decode_string(token)));
	    token.clear();
	}
    }
    if (quoted) {
	result.error = "unterminated FILE_SCHEMA identifier";
	return result;
    }
    if (result.identifiers.empty()) {
	result.error = "FILE_SCHEMA does not contain a schema identifier";
	return result;
    }

    std::set<HeaderSchema> schemas;
    for (const std::string &identifier : result.identifiers) {
	const HeaderSchema schema = classify(identifier, result.legacy_identifier);
	if (schema == HeaderSchema::Unknown)
	    result.unrecognized_identifiers.push_back(identifier);
	else
	    schemas.insert(schema);
    }
    /* Early modular extensions were published as a FILE_SCHEMA list whose
     * first entry was the edition-1 CONFIG_CONTROL_DESIGN schema.  Treating
     * that list as ordinary AP203 loses the added unit and presentation
     * entities.  The AP203e2 binding contains the compatible geometry and
     * deprecated AP203 constructs; schema-independent metadata retention
     * preserves legacy records which were subsequently retired. */
    if (schemas.size() == 1 && *schemas.begin() == HeaderSchema::AP203) {
	bool interim_profile = false;
	std::vector<std::string> remaining;
	for (const std::string &identifier : result.unrecognized_identifiers) {
	    if (ap203_interim_companion(identifier))
		interim_profile = true;
	    else
		remaining.push_back(identifier);
	}
	if (interim_profile) {
	    schemas.clear();
	    schemas.insert(HeaderSchema::AP203e2);
	    result.legacy_identifier = true;
	    result.unrecognized_identifiers.swap(remaining);
	}
    }
    /* Part 21 permits a FILE_SCHEMA list.  Producers use that facility for
     * an integrated AP schema plus companion MIM schemas (for example,
     * CONFIG_CONTROL_DESIGN with validation-property and appearance-layer
     * MIMs).  An identifier for which we have no plugin does not create a
     * choice when exactly one supported AP is present: select that registry
     * and let normal entity diagnostics account for any unsupported content.
     * Two supported schema families really are ambiguous and must not be
     * guessed. */
    if (schemas.size() > 1) {
	result.ambiguous = true;
	result.error = "ambiguous mixed FILE_SCHEMA declaration";
	return result;
    }
    if (schemas.empty()) {
	std::ostringstream message;
	message << "unknown STEP schema '" << result.identifiers.front() << "'";
	result.error = message.str();
	return result;
    }
    result.schema = *schemas.begin();
    result.recognized = true;
    return result;
}
