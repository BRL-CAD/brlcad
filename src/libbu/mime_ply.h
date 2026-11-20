/* M I M E _ P L Y . H
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
#ifndef BU_MIME_PLY_H
#define BU_MIME_PLY_H
#include "common.h"
#include <istream>
#include <locale>
#include <sstream>
#include <string>
#include "bu/mime.h"
#include "mime_text.h"

namespace mime_ply {
inline constexpr const char *type = "application/x-ply";
inline constexpr char signature[] = "PLY";
constexpr size_t signature_size = sizeof(signature) - 1;

struct profile {
    bool identified = false, incomplete = false;
    std::string encoding, order, version;
    bool append_traits(bu_mime_traits &traits) const
    {
	if (!identified) return false;
	return (encoding.empty() || bu_mime_traits_add(&traits, "encoding", encoding.c_str())) &&
	    (order.empty() || bu_mime_traits_add(&traits, "byte-order", order.c_str())) &&
	    (version.empty() || bu_mime_traits_add(&traits, "version", version.c_str()));
    }
};

/* The format line declares the wire representation, not the charset of
 * comments or a format version inferred from the decoder's defaults. */
inline profile inspect(std::istream &stream, bool whole)
{
    profile result;
    std::string line;
    const bool first_complete = mime_detail::read_line(stream, line, whole, result.incomplete);
    const std::string magic = mime_detail::upper(line);
    const bool valid_magic = magic.compare(0, signature_size, signature) == 0 &&
	magic.find_first_not_of(" \t\r", signature_size) == std::string::npos;
    if (!first_complete) {
	result.incomplete = !whole && (valid_magic || (magic.size() < signature_size &&
	    std::string(signature).compare(0, magic.size(), magic) == 0));
	return result;
    }
    if (!valid_magic) return result;
    result.identified = true;
    if (!mime_detail::read_line(stream, line, whole, result.incomplete)) return result;
    std::istringstream fields(line);
    fields.imbue(std::locale::classic());
    std::string keyword, format, version;
    if (!(fields >> keyword >> format >> version) || keyword != "format" || version.find('\0') != std::string::npos) return result;
    result.version = version;
    if (format == "ascii") result.encoding = "ascii";
    else if (format == "binary_little_endian" || format == "binary_big_endian") {
	result.encoding = "binary";
	result.order = format == "binary_little_endian" ? "little-endian" : "big-endian";
    }
    return result;
}
}
#endif
