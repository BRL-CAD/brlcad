/*                 A P 2 1 4 A D A P T E R . C P P
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

#include "AP214Adapter.h"
#include "STEPString.h"

#include <algorithm>
#include <cctype>
#include <fstream>
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
    std::string out;
    bool comment = false;
    bool quoted = false;
    for (size_t i = 0; i < text.size();) {
	if (comment) {
	    if (i + 1 < text.size() && text[i] == '*' && text[i + 1] == '/') {
		comment = false;
		i += 2;
	    } else {
		if (text[i] == '\n' || text[i] == '\r') out.push_back(text[i]);
		++i;
	    }
	    continue;
	}
	if (!quoted && i + 1 < text.size() && text[i] == '/' && text[i + 1] == '*') {
	    comment = true;
	    i += 2;
	    continue;
	}
	out.push_back(text[i]);
	if (text[i] == '\'') {
	    if (quoted && i + 1 < text.size() && text[i + 1] == '\'') {
		out.push_back(text[i + 1]);
		i += 2;
		continue;
	    }
	    quoted = !quoted;
	}
	++i;
    }
    return out;
}

std::string
suggest_converter(const std::vector<std::string> &identifiers)
{
    for (const std::string &id : identifiers) {
	std::string value = upper_ascii(id);
	if (value.find("CONFIG_CONTROL_DESIGN") != std::string::npos || value.find("AP203") != std::string::npos)
	    return "step-g";
	if (value.find("AP242") != std::string::npos || value.find("MANAGED_MODEL_BASED_3D_ENGINEERING") != std::string::npos)
	    return "ap242-g";
	if (value.find("IFC") != std::string::npos)
	    return "ifc-g";
    }
    return std::string();
}

} // namespace

brlcad::step::AP214SchemaInfo
brlcad::step::AP214Adapter::inspect_file(const std::string &path)
{
    AP214SchemaInfo result;
    std::ifstream input(path.c_str(), std::ios::binary);
    if (!input) {
	result.error = "unable to read STEP input";
	return result;
    }

    // A normal Part 21 header is tiny.  The generous bound prevents a corrupt
    // file from turning schema inspection into another whole-file load.
    const size_t max_header = 16U * 1024U * 1024U;
    std::string header;
    header.reserve(64U * 1024U);
    char buffer[8192];
    while (input && header.size() < max_header) {
	input.read(buffer, sizeof(buffer));
	std::streamsize count = input.gcount();
	if (count > 0) header.append(buffer, static_cast<size_t>(count));
	std::string normalized = upper_ascii(without_comments(header));
	size_t header_pos = normalized.find("HEADER;");
	if (header_pos != std::string::npos && normalized.find("ENDSEC;", header_pos) != std::string::npos)
	    return inspect_header(header);
    }
    result.error = "missing or over-large ISO 10303-21 HEADER section";
    return result;
}

brlcad::step::AP214SchemaInfo
brlcad::step::AP214Adapter::inspect_header(const std::string &input)
{
    AP214SchemaInfo result;
    const std::string header = without_comments(input);
    const std::string upper = upper_ascii(header);
    size_t key = upper.find("FILE_SCHEMA");
    if (key == std::string::npos) {
	result.error = "STEP header has no FILE_SCHEMA declaration";
	return result;
    }
    size_t open = header.find('(', key);
    if (open == std::string::npos) {
	result.error = "malformed FILE_SCHEMA declaration";
	return result;
    }

    bool quoted = false;
    std::string token;
    for (size_t i = open + 1; i < header.size(); ++i) {
	char c = header[i];
	if (!quoted) {
	    if (c == '\'') {
		quoted = true;
		token.assign(1, '\'');
	    } else if (c == ')') {
		break;
	    }
	    continue;
	}
	token.push_back(c);
	if (c == '\'') {
	    if (i + 1 < header.size() && header[i + 1] == '\'') {
		token.push_back(header[++i]);
		continue;
	    }
	    quoted = false;
	    result.identifiers.push_back(trim(decode_string(token)));
	    token.clear();
	}
    }

    if (result.identifiers.empty()) {
	result.error = "FILE_SCHEMA does not contain a schema identifier";
	return result;
    }

    for (const std::string &identifier : result.identifiers) {
	const std::string value = upper_ascii(trim(identifier));
	if (value == "AUTOMOTIVE_DESIGN_CC2") {
	    result.accepted = true;
	    result.legacy_cc2 = true;
	    continue;
	}
	const std::string schema = "AUTOMOTIVE_DESIGN";
	if (value.compare(0, schema.size(), schema) == 0 &&
	    (value.size() == schema.size() || std::isspace(static_cast<unsigned char>(value[schema.size()])) || value[schema.size()] == '{')) {
	    result.accepted = true;
	}
    }

    if (!result.accepted) {
	result.suggested_converter = suggest_converter(result.identifiers);
	std::ostringstream message;
	message << "unsupported STEP schema '" << result.identifiers.front() << "'";
	if (!result.suggested_converter.empty()) message << "; use " << result.suggested_converter;
	result.error = message.str();
    }
    return result;
}

