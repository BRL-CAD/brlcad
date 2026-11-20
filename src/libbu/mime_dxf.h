/* M I M E _ D X F . H
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
#ifndef BU_MIME_DXF_H
#define BU_MIME_DXF_H
#include "common.h"
#include <cctype>
#include <cstring>
#include <istream>
#include <set>
#include <string>
#include "mime_text.h"

namespace mime_dxf {
inline constexpr const char *type = "image/vnd.dxf";
inline constexpr char binary_signature[] = "AutoCAD Binary DXF\r\n\x1a";

struct profile {
    bool identified = false, binary = false, utf8_bom = false;
    bool header_seen = false, header_complete = false, incomplete = false;
    std::set<std::string> acad_versions, codepages;
    bool append_traits(bu_mime_traits &traits) const
    {
	if (!identified || !bu_mime_traits_add(&traits, "dxf.serialization", binary ? "binary" : "text")) return false;
	if (binary) return bu_mime_traits_add(&traits, "encoding", "binary") &&
	    bu_mime_traits_add(&traits, "byte-order", "little-endian");
	if (utf8_bom && !bu_mime_traits_add(&traits, "encoding", "utf-8")) return false;
	for (const auto &value : acad_versions)
	    if (!bu_mime_traits_add(&traits, "dxf.acad-version", value.c_str())) return false;
	for (const auto &value : codepages)
	    if (!bu_mime_traits_add(&traits, "dxf.codepage", value.c_str())) return false;
	return true;
    }
};

class pairs {
    std::istream &input;
    bool whole, first = true;
    bool line(std::string &value)
    {
	return mime_detail::read_line(input, value, whole, incomplete);
    }
public:
    bool incomplete = false, utf8_bom = false;
    pairs(std::istream &stream, bool complete) : input(stream), whole(complete) {}
    bool next(std::string &code, std::string &value)
    {
	std::string raw;
	do {
	    if (!line(raw)) return false;
	    if (first && raw.compare(0, 3, "\xef\xbb\xbf") == 0) {
		raw.erase(0, 3);
		utf8_bom = true;
	    }
	    first = false;
	    raw = mime_detail::trim(raw);
	} while (raw.empty());
	code = mime_detail::decimal_field(raw);
	if (code.empty() || !line(value) || value.find('\0') != std::string::npos) return false;
	value = mime_detail::trim(value);
	return true;
    }
};

inline profile inspect(std::istream &input, bool complete)
{
    profile out;
    const int first = input.peek();
    if (first == binary_signature[0]) {
	char signature[sizeof(binary_signature)];
	input.read(signature, sizeof(signature));
	const size_t size = static_cast<size_t>(input.gcount());
	if (memcmp(signature, binary_signature, size)) return out;
	out.identified = out.binary = size == sizeof(signature);
	out.incomplete = !complete && !out.identified;
	return out;
    }
    if (first < 0 || (!(first >= '0' && first <= '9') && first != 0xef &&
	!std::isspace(static_cast<unsigned char>(first)))) return out;
    pairs records(input, complete);
    std::string code, value, variable;
    bool section_name = false;
    while (records.next(code, value)) {
	if (code == "999") continue;
	if (section_name) {
	    if (code != "2" || value.empty()) break;
	    out.identified = true;
	    out.utf8_bom = records.utf8_bom;
	    /* HEADER precedes drawing data. Stop before any model body rather
	     * than treating a later entity's text as a header declaration. */
	    if (value != "HEADER") return out;
	    out.header_seen = true;
	    section_name = false;
	    continue;
	}
	if (code == "0" && value == "SECTION") {
	    if (out.header_seen) break;
	    section_name = true;
	    continue;
	}
	if (!out.header_seen) break;
	if (code == "0" && value == "ENDSEC") {
	    out.header_complete = true;
	    return out;
	}
	if (code == "0") break;
	if (code == "9") variable = value;
	else if (code == "1" && variable == "$ACADVER" && !value.empty()) out.acad_versions.insert(value);
	else if (code == "3" && variable == "$DWGCODEPAGE" && !value.empty()) out.codepages.insert(value);
    }
    out.incomplete = records.incomplete;
    return out;
}
} // namespace mime_dxf
#endif

/*
 * Local Variables:
 * mode: C++
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8 cino=N-s
 */
