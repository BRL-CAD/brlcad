/*          M I M E _ P 2 1 . H
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */

#ifndef BU_MIME_P21_H
#define BU_MIME_P21_H
#include "common.h"
#include <cctype>
#include <cstring>
#include <istream>
#include <set>
#include <sstream>
#include <string>
#include <utility>
#include <vector>
#include "mime_text.h"

namespace mime_p21 {
inline constexpr char signature[] = "ISO-10303-21;";
inline std::string schema_name(const std::string &raw)
{
    return mime_detail::upper(mime_detail::trim(raw.substr(0, raw.find('{'))));
}
inline std::string schema_oid(const std::string &raw)
{
    std::string identifier = mime_detail::trim(raw);
    size_t brace = identifier.find('{');
    if (brace == std::string::npos || identifier.back() != '}') return "";
    std::istringstream numbers(identifier.substr(brace + 1, identifier.size() - brace - 2));
    std::string number, normalized;
    while (numbers >> number) {
	if (!normalized.empty()) normalized += '.';
	normalized += number;
    }
    return normalized;
}


inline bool step_companion(const std::string &name)
{
    return name == "GEOMETRIC_VALIDATION_PROPERTIES_MIM" ||
	name == "GEOMETRIC_VALIDATION_PROPERTY_REPRESENTATION_MIM" || name == "SHAPE_APPEARANCE_LAYER_MIM" ||
	name == "SHAPE_APPEARANCE_LAYERS_MIM";
}

inline std::string step_application(const std::string &schema)
{
    if (schema == "CONFIG_CONTROL_DESIGN" || schema == "CONFIGURATION_CONTROL_3D_DESIGN_MIM_LF" ||
	schema == "CCD_CLA_GVP_AST_ASD" || schema == "CCD_CLA_GVP_AST")
	return "203";
    if (schema == "AUTOMOTIVE_DESIGN" || schema == "AUTOMOTIVE_DESIGN_CC2")
	return "214";
    if (schema == "MANAGED_MODEL_BASED_3D_ENGINEERING")
	return "242";
    const size_t ap_digits_end = 5;
    if (schema.size() > ap_digits_end && schema.compare(0, 2, "AP") == 0 && schema[ap_digits_end] == '_' &&
	schema[2] >= '0' && schema[2] <= '9' && schema[3] >= '0' && schema[3] <= '9' &&
	schema[4] >= '0' && schema[4] <= '9')
	return schema.substr(2, 3);
    return "";
}

inline std::string step_edition(const std::string &raw, bool companion)
{
    const std::string identifier = mime_detail::trim(raw);
    const size_t brace = identifier.find('{');
    const std::string schema = schema_name(identifier);
    const std::string ap = step_application(schema);
    std::string edition;
    if (schema == "CONFIG_CONTROL_DESIGN")
	edition = companion ? "2" : "1";
    else if (schema == "AP203_CONFIGURATION_CONTROLLED_3D_DESIGN_OF_MECHANICAL_PARTS_AND_ASSEMBLIES_MIM_LF" ||
	    schema == "CONFIGURATION_CONTROL_3D_DESIGN_MIM_LF" || schema == "CCD_CLA_GVP_AST_ASD" || schema == "CCD_CLA_GVP_AST")
	edition = "2";
    if (brace != std::string::npos && identifier.back() == '}') {
	std::string normalized = schema_oid(raw);
	/* Only established edition identifiers establish a profile. STEP Tools
	 * and the MBx schema index document distinct AP242e4/AP214e3 OIDs;
	 * preserve the literal OID even when both identify the same AP edition.
	 * See the STEP profile references in the MIME article. */
	if (ap == "242") {
	    if (normalized == "1.0.10303.442.1.1.4") edition = "1";
	    else if (normalized == "1.0.10303.442.3.1.4") edition = "2";
	    else if (normalized == "1.0.10303.442.4.1.4") edition = "3";
	    else if (normalized == "1.0.10303.442.5.1.4" || normalized == "1.0.10303.442.7.1.4") edition = "4";
	} else if (ap == "214") {
	    if (normalized == "1.0.10303.214.1.1.1") edition = "1";
	    else if (normalized == "1.0.10303.214.2.1.1") edition = "2";
	    else if (normalized == "1.0.10303.214.3.1.1" || normalized == "1.0.10303.214.1.1.1.1") edition = "3";
	}
    }
    return edition;
}

/* Quoted values and comments cannot introduce a FILE_SCHEMA declaration.
 * A stream lets full readers stop at the header instead of buffering a
 * potentially large model, while prefix inspection uses the same tokens. */
class tokens {
    std::istream &input;
public:
    bool quoted = false, ended = false;
    explicit tokens(std::istream &stream) : input(stream) {}
    bool consume(const char *expected)
    {
	std::string value = next();
	return !quoted && !ended && value == expected;
    }
    std::string next()
    {
	quoted = false;
	for (;;) {
	    int c = input.peek();
	    while (c >= 0 && std::isspace(static_cast<unsigned char>(c))) {input.get(); c = input.peek();}
	    if (c != '/') break;
	    input.get();
	    if (input.peek() != '*') return "/";
	    input.get();
	    int previous = 0;
	    for (c = input.get(); c >= 0 && !(previous == '*' && c == '/'); c = input.get()) previous = c;
	    if (c < 0) {ended = true; return {};}
	}
	int c = input.get();
	if (c < 0) {ended = true; return {};}
	if (c == '\'') {
	    quoted = true;
	    std::string value;
	    for (c = input.get(); c >= 0; c = input.get()) {
		if (c == '\'') {
		    if (input.peek() != '\'') return value;
		    input.get();
		}
		value += static_cast<char>(c);
	    }
	    ended = true;
	    return {};
	}
	auto identifier = [](int value) {return value >= 0 && (std::isalnum(static_cast<unsigned char>(value)) || value == '_');};
	std::string value(1, static_cast<char>(c));
	if (identifier(c)) {
	    while (identifier(input.peek())) value += static_cast<char>(input.get());
	    return mime_detail::upper(value);
	}
	return value;
    }
};

struct profile {
    std::vector<std::string> identifiers;
    bool complete = false, incomplete = false;
    bool step_model() const
    {
	if (!complete) return false;
	bool product = false;
	for (const auto &identifier : identifiers) {
	    const std::string name = schema_name(identifier);
	    const bool application = !step_application(name).empty();
	    if (!application && !step_companion(name)) return false;
	    product = product || application;
	}
	return product;
    }

    bool append_schema_traits(bu_mime_traits &traits, const char *prefix) const
    {
	if (!complete) return false;
	std::set<std::string> names, raw(identifiers.begin(), identifiers.end()), oids;
	for (const auto &identifier : identifiers) {
	    names.insert(schema_name(identifier));
	    std::string oid = schema_oid(identifier);
	    if (!oid.empty()) oids.insert(oid);
	}
	const std::pair<const char *, const std::set<std::string> *> sets[] = {
	    {"schemas", &names}, {"schema-identifiers", &raw}, {"oids", &oids}
	};
	for (const auto &item : sets)
	    if (!mime_detail::append_set(traits, (std::string(prefix) + "." + item.first).c_str(), *item.second)) return false;
	const std::pair<const char *, const std::set<std::string> *> scalars[] = {
	    {"schema", &names}, {"schema-identifier", &raw}, {"oid", &oids}
	};
	for (const auto &item : scalars)
	    if (item.second->size() == 1 && !bu_mime_traits_add(&traits,
		(std::string(prefix) + "." + item.first).c_str(), item.second->begin()->c_str())) return false;
	return true;
    }

    bool append_traits(bu_mime_traits &traits) const
    {
	return append_schema_traits(traits, "p21");
    }

    bool append_step_traits(bu_mime_traits &traits) const
    {
	if (!step_model() || !append_schema_traits(traits, "step")) return false;
	bool companion = false;
	for (const auto &identifier : identifiers)
	    companion = companion || step_companion(schema_name(identifier));
	std::set<std::string> protocols, profiles, editions;
	bool all_editions_known = true;
	for (const auto &identifier : identifiers) {
	    const std::string ap = step_application(schema_name(identifier));
	    if (ap.empty()) continue;
	    protocols.insert(ap);
	    const std::string edition = step_edition(identifier, companion);
	    all_editions_known = all_editions_known && !edition.empty();
	    if (!edition.empty()) {
		profiles.insert("ap" + ap + "e" + edition);
		editions.insert(edition);
	    }
	}
	return mime_detail::append_set(traits, "step.application-protocols", protocols) &&
	    (!all_editions_known || mime_detail::append_set(traits, "step.profiles", profiles)) &&
	    (protocols.size() != 1 || bu_mime_traits_add(&traits, "step.ap", protocols.begin()->c_str())) &&
	    (!all_editions_known || editions.size() != 1 || bu_mime_traits_add(&traits, "step.edition", editions.begin()->c_str()));
    }
};

inline profile inspect(std::istream &stream, bool complete)
{
    profile out;
    char prefix[sizeof(signature) - 1];
    stream.read(prefix, sizeof(prefix));
    if (stream.gcount() != static_cast<std::streamsize>(sizeof(prefix)) || memcmp(prefix, signature, sizeof(prefix))) {
	out.incomplete = !complete;
	return out;
    }
    tokens input(stream);
    bool header = false;
    for (std::string value = input.next(); !input.ended; value = input.next()) {
	if (input.quoted) continue;
	if (value == "HEADER") header = true;
	if (value == "ENDSEC" || value == "DATA") return out;
	if (!header || value != "FILE_SCHEMA") continue;
	if (!input.consume("(") || !input.consume("(")) {out.incomplete = !complete; return out;}
	for (;;) {
	    value = input.next();
	    if (!input.quoted || input.ended || schema_name(value).empty() || value.find('\0') != std::string::npos) {
		out.incomplete = !complete;
		return out;
	    }
	    out.identifiers.push_back(value);
	    value = input.next();
	    if (input.quoted || value != ",") break;
	}
	out.complete = value == ")" && !input.quoted && input.consume(")") && input.consume(";") && !stream.bad();
	out.incomplete = !out.complete && !complete;
	return out;
    }
    out.incomplete = !complete;
    return out;
}
}
#endif
