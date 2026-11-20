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
#include "StepMime.h"
#include "../../libbu/mime_p21.h"
#include "bu/str.h"

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
decode_identifier(const std::string &raw)
{
    /* The shared lexer already unescaped apostrophes. Restore their STEP
     * quoting before applying the native reader's character decoding. */
    std::string quoted = "'";
    for (char c : raw) {
	quoted += c;
	if (c == '\'') quoted += c;
    }
    quoted += '\'';
    return trim(brlcad::step::decode_string(quoted));
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
    return mime_p21::step_companion(mime_p21::schema_name(identifier));
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

    return inspect_stream(input);
}

brlcad::step::HeaderSchemaInfo
brlcad::step::STEPHeaderSchema::inspect_header(const std::string &input)
{
    std::istringstream stream(input);
    return inspect_stream(stream);
}

brlcad::step::HeaderSchemaInfo
brlcad::step::STEPHeaderSchema::inspect_stream(std::istream &input)
{
    HeaderSchemaInfo result;
    const auto observed = mime_p21::inspect(input, true);
    if (!observed.complete) {
	result.error = "malformed or missing ISO 10303-21 FILE_SCHEMA declaration";
	return result;
    }
    bool header_complete = false;
    mime_p21::tokens tail(input);
    for (std::string value = tail.next(); !tail.ended; value = tail.next()) {
	if (tail.quoted) continue;
	if (value == "ENDSEC") {header_complete = tail.consume(";"); break;}
	if (value == "DATA") break;
    }
    if (!header_complete || input.bad()) {
	result.error = "malformed or incomplete STEP HEADER section";
	return result;
    }
    result.source_identifiers = observed.identifiers;
    for (const auto &identifier : observed.identifiers)
	result.identifiers.push_back(decode_identifier(identifier));
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
    bool companion = false;
    for (const auto &identifier : result.identifiers)
	companion = companion || mime_p21::step_companion(mime_p21::schema_name(identifier));
    std::set<std::string> editions;
    bool all_editions_known = true;
    for (const auto &identifier : result.identifiers) {
	if (mime_p21::step_application(mime_p21::schema_name(identifier)).empty()) continue;
	const std::string edition = mime_p21::step_edition(identifier, companion);
	all_editions_known = all_editions_known && !edition.empty();
	if (!edition.empty()) editions.insert(edition);
    }
    if (editions.size() > 1) {
	result.ambiguous = true;
	result.error = "ambiguous mixed STEP editions";
	return result;
    }
    if (all_editions_known && editions.size() == 1) result.edition = *editions.begin();
    result.schema = *schemas.begin();
    result.recognized = true;
    return result;
}

bool
brlcad::step::STEPHeaderSchema::matches_mime(const HeaderSchemaInfo &header, const char *format,
    const bu_mime_traits *requested, std::string &error)
{
    error.clear();
    if (header.source_identifiers.empty() || !bu_mime_traits_valid(requested, 1)) {
	error = "missing STEP source declarations or invalid MIME assertions";
	return false;
    }
    mime_p21::profile source;
    source.identifiers = header.source_identifiers;
    source.complete = true;
    const std::string type = step_mime_identity(format, source.step_model());
    if (type.empty()) {
	error = "requested MIME format does not match the STEP source";
	return false;
    }
    MimeTraits observed;
    const bool valid = (type == "model/step" ? source.append_step_traits(observed.value) : source.append_traits(observed.value)) &&
	bu_mime_traits_match(&observed.value, NULL, requested);
    if (!valid) {
	bu_vls detail = BU_VLS_INIT_ZERO;
	bu_vls_printf(&detail, "STEP source does not establish requested MIME traits:");
	bu_mime_traits_describe(requested, &detail);
	error = bu_vls_cstr(&detail);
	bu_vls_free(&detail);
    }
    return valid;
}
