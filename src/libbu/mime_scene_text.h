/* M I M E _ S C E N E _ T E X T . H
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
#ifndef BU_MIME_SCENE_TEXT_H
#define BU_MIME_SCENE_TEXT_H
#include "common.h"
#include <cctype>
#include <cstdint>
#include <istream>
#include <limits>
#include <string>
#include "bu/str.h"
#include "mime_text.h"
#include "mime_unicode.h"

namespace mime_scene_text {
inline constexpr const char *ac3d_type = "model/vnd.inivis.ac3d";
inline constexpr const char *ase_type = "model/vnd.autodesk.ase";

inline bool is_type(const char *type)
{
    return type && (BU_STR_EQUAL(type, ac3d_type) || BU_STR_EQUAL(type, ase_type));
}

using mime_unicode::text_source;

struct profile {
    std::string type, version, encoding, width;
    bool incomplete = false, bom = false;
    bool append_traits(bu_mime_traits &traits) const
    {
	return !type.empty() && bu_mime_traits_add(&traits,
	    type == ac3d_type ? "ac3d.serialization" : "ase.serialization", "text") &&
	    bu_mime_traits_add(&traits, "text.code-unit-width", width.c_str()) &&
	    (encoding.empty() || bu_mime_traits_add(&traits, "encoding", encoding.c_str())) &&
	    (version.empty() || bu_mime_traits_add(&traits, "version", version.c_str()));
    }
};

inline profile inspect(std::istream &stream, bool complete, bool inspect_encoding = true)
{
    profile result;
    text_source source(stream);
    const auto match = [&](const char *word) {
	for (const char *c = word; *c; ++c) {
	    const int actual = source.get();
	    if (actual < 0) {result.incomplete = !complete; return false;}
	    if (actual != *c) return false;
	}
	return true;
    };
    if (source.peek() == 'A') {
	if (!match("AC3D")) return result;
	result.type = ac3d_type;
	uint32_t value = 0;
	size_t digits = 0;
	bool overflow = false;
	for (int c = source.peek(); c >= 0 && c < 0x80 && std::isxdigit(static_cast<unsigned char>(c)); c = source.peek()) {
	    const unsigned digit = c <= '9' ? static_cast<unsigned>(c - '0') :
		static_cast<unsigned>(std::tolower(static_cast<unsigned char>(c)) - 'a' + 10);
	    if (value > (std::numeric_limits<uint32_t>::max() - digit) / 16) overflow = true;
	    if (!overflow) value = value * 16 + digit;
	    source.get(); ++digits;
	}
	if (source.peek() < 0 && !complete) result.incomplete = true;
	else if (digits && !overflow && (source.peek() < 0 || text_source::space(source.peek()))) result.version = std::to_string(value);
    } else {
	source.spaces();
	for (;;) {
	    if (source.get() != '*') return result;
	    /* Export declarations and COMMENT are short fixed tokens. */
	    constexpr size_t keyword_limit = sizeof("3DSMAX_ASCIIEXPORT") - 1;
	    std::string word;
	    while (source.peek() >= 0 && !text_source::space(source.peek())) {
		if (word.size() == keyword_limit || source.peek() >= 0x80) return result;
		word += static_cast<char>(source.get());
	    }
	    if (word == "COMMENT") {
		source.spaces();
		if (!source.quoted()) {result.incomplete = !complete && source.peek() < 0; return result;}
		source.spaces();
		continue;
	    }
	    if (word != "3DSMAX_ASCIIEXPORT") {
		result.incomplete = !complete && source.peek() < 0 && std::string("3DSMAX_ASCIIEXPORT").compare(0, word.size(), word) == 0;
		return result;
	    }
	    if (source.peek() < 0 && !complete) {result.incomplete = true; return result;}
	    result.type = ase_type;
	    while (source.peek() == ' ' || source.peek() == '\t') source.get();
	    std::string version;
	    constexpr size_t version_limit = std::numeric_limits<uint32_t>::digits10 + 1;
	    bool overflow = false;
	    while (source.peek() >= '0' && source.peek() <= '9') {
		if (version.size() == version_limit) overflow = true;
		if (!overflow) version += static_cast<char>(source.peek());
		source.get();
	    }
	    if (source.peek() < 0 && !complete) result.incomplete = true;
	    else if (!overflow && (source.peek() < 0 || text_source::space(source.peek()))) result.version = mime_detail::decimal_field(version);
	    break;
	}
    }
    result.width = source.width();
    result.bom = source.marked();
    if (source.marked()) result.encoding = source.encoding();
    if (inspect_encoding && complete) {
	result.encoding = source.finish_encoding();
    }
    return result;
}
}
#endif
