/* M I M E _ X . H
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
#ifndef BU_MIME_X_H
#define BU_MIME_X_H
#include "common.h"
#include <algorithm>
#include <istream>
#include <string>
#include <utility>
#include "bu/mime.h"
#include "mime_unicode.h"

namespace mime_x {
inline constexpr const char *type = "model/vnd.microsoft.x";
inline constexpr char signature[] = "xof ";
constexpr size_t header_size = 16, version_end = 8, format_end = 12;

struct profile {
    bool identified = false, complete = false, incomplete = false, bom = false;
    std::string version, version_code, serialization, compression, float_width, encoding, width;
    bool append_traits(bu_mime_traits &traits) const
    {
	if (!identified || !bu_mime_traits_add(&traits, "x.header-code-unit-width", width.c_str())) return false;
	const std::pair<const char *, const std::string *> fields[] = {
	    {"version", &version}, {"x.version-code", &version_code}, {"x.serialization", &serialization},
	    {"compression", &compression}, {"x.float-width", &float_width}, {"encoding", &encoding}
	};
	for (const auto &field : fields)
	    if (!field.second->empty() && !bu_mime_traits_add(&traits, field.first, field.second->c_str())) return false;
	return serialization != "binary" || bu_mime_traits_add(&traits, "byte-order", "little-endian");
    }
};

inline bool digits(const std::string &text)
{
    return std::all_of(text.begin(), text.end(), [](unsigned char c) {return c >= '0' && c <= '9';});
}

inline profile inspect(std::istream &stream, bool whole, bool inspect_encoding = true)
{
    profile result;
    mime_unicode::text_source source(stream);
    std::string header;
    for (size_t at = 0; at < header_size; ++at) {
	const int c = source.get();
	if (c < 0) {result.incomplete = !whole; break;}
	if (at < sizeof(signature) - 1 && c != signature[at]) return {};
	if (c >= 0x80) break;
	header += static_cast<char>(c);
    }
    if (header.size() < sizeof(signature) - 1) return result;
    result.identified = true;
    result.width = source.width();
    result.bom = source.marked();
    if (header.size() >= version_end && digits(header.substr(sizeof(signature) - 1, 4))) {
	result.version_code = header.substr(sizeof(signature) - 1, 4);
	const unsigned major = static_cast<unsigned>((header[4] - '0') * 10 + header[5] - '0');
	const unsigned minor = static_cast<unsigned>((header[6] - '0') * 10 + header[7] - '0');
	result.version = std::to_string(major) + '.' + std::to_string(minor);
    }
    if (header.size() >= format_end) {
	const std::string format = header.substr(version_end, format_end - version_end);
	if (format == "txt " || format == "tzip") result.serialization = "text";
	if (format == "bin " || format == "bzip") result.serialization = "binary";
	if (!result.serialization.empty()) result.compression = format == "tzip" || format == "bzip" ? "mszip" : "none";
    }
    if (header.size() == header_size && digits(header.substr(format_end))) {
	const std::string word = header.substr(format_end);
	const size_t first = word.find_first_not_of('0');
	result.float_width = first == std::string::npos ? "0" : word.substr(first);
    }
    result.complete = header.size() == header_size && !result.version.empty() &&
	!result.serialization.empty() && !result.float_width.empty();
    if (result.serialization == "binary" || result.compression == "mszip") result.encoding = "binary";
    else if (result.serialization == "text") {
	if (source.marked()) result.encoding = source.encoding();
	if (inspect_encoding && whole) result.encoding = source.finish_encoding();
    }
    return result;
}
}
#endif
