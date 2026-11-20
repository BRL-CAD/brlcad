/* M I M E _ I S G . H
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 * See the file named COPYING for more information.
 */
#ifndef BU_MIME_ISG_H
#define BU_MIME_ISG_H
#include "common.h"
#include <cstring>
#include <istream>
#include <map>
#include <string>
#include "bu/mime_runtime.h"
#include "mime_text.h"
#include "mime_unicode.h"

namespace mime_isg {
constexpr const char *type = "application/vnd.isgeoid.isg";

struct profile {
    bool identified = false, incomplete = false, header_known = false, bom = false;
    bool repertoire_known = false, ascii = false;
    std::string width, encoding;
    std::map<std::string, std::string> fields;
    std::string value(const char *key) const
    {
	auto found = fields.find(key);
	return found == fields.end() ? std::string() : found->second;
    }
    bool append_traits(bu_mime_traits &traits) const
    {
	if (!identified || !bu_mime_traits_add(&traits, "isg.serialization", "text") ||
	    !bu_mime_traits_add(&traits, "text.code-unit-width", width.c_str()) ||
	    !bu_mime_traits_add(&traits, "isg.text-bom", bom ? "true" : "false") ||
	    (!encoding.empty() && !bu_mime_traits_add(&traits, "encoding", encoding.c_str())) ||
	    (repertoire_known && !bu_mime_traits_add(&traits, "text.ascii-only", ascii ? "true" : "false"))) return false;
	if (!header_known) return true;
	const auto property = [&](const char *key, const char *name, const char *implicit = "") {
	    std::string text = value(key);
	    if (text.empty()) text = implicit;
	    return text.empty() || bu_mime_traits_add(&traits, name, text.c_str());
	};
	const bool legacy = value("ISG format") == "1.0" || value("ISG format") == "1.01";
	return property("ISG format", "isg.format-version") &&
	    property("data format", "isg.data-format", legacy ? "grid" : "") &&
	    property("data ordering", "isg.data-ordering", legacy ? "N-to-S, W-to-E" : "") &&
	    property("coord type", "isg.coordinate-type", legacy ? "geodetic" : "") &&
	    property("coord units", "isg.coordinate-units", legacy ? "deg" : "") &&
	    property(legacy ? "units" : "data units", "isg.data-units");
    }
};

inline profile inspect(std::istream &stream, bool complete, bool repertoire)
{
    profile out;
    mime_unicode::text_source source(stream);
    out.width = source.width();
    out.bom = source.marked();
    if (out.bom) out.encoding = source.encoding();
    bool begun = false, ended = false, duplicate = false;
    for (std::string line; !ended;) {
	line.clear();
	int c;
	/* Profile labels and values are ASCII. text_source separately
	 * observes the encoding of comments and numeric coordinates. */
	while ((c = source.get()) >= 0 && c != '\n' && c != '\r') {
	    if (!c) return out;
	    line.push_back(c < 0x80 ? static_cast<char>(c) : '?');
	}
	if (c < 0 && line.empty()) break;
	if (c < 0 && !complete) break;
	line = mime_detail::trim(line);
	const auto marker = [&](const char *word) {
	    const size_t length = strlen(word);
	    return line.compare(0, length, word) == 0 &&
		(line.size() == length || line[length] == '=' || mime_unicode::text_source::space(line[length]));
	};
	if (!begun) {begun = marker("begin_of_head"); continue;}
	if (marker("end_of_head")) {ended = true; break;}
	const size_t separator = line.find_first_of(":=");
	if (separator == std::string::npos) continue;
	const std::string key = mime_detail::trim(line.substr(0, separator));
	const std::string value = mime_detail::trim(line.substr(separator + 1));
	if (key == "model name") {out.fields.emplace(key, value); continue;}
	/* Extra metadata does not affect these profile observations. */
	constexpr const char *keys[] = {"ISG format", "data format", "data ordering", "coord type", "coord units", "units", "data units"};
	for (const char *name : keys)
	    if (key == name && !out.fields.emplace(key, value).second) duplicate = true;
    }
    out.identified = begun && out.fields.count("model name") && out.fields.count("ISG format");
    out.header_known = out.identified && ended && !duplicate;
    out.incomplete = begun && !ended && !complete;
    if (repertoire) {
	out.encoding = source.finish_encoding();
	out.repertoire_known = !out.encoding.empty();
	out.ascii = source.ascii;
    }
    return out;
}
}
#endif
