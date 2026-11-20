/* M I M E _ O P E N G E X . H
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
#ifndef BU_MIME_OPENGEX_H
#define BU_MIME_OPENGEX_H
#include "common.h"
#include <istream>
#include <string>
#include "bu/mime.h"
#include "mime_text.h"
#include "mime_unicode.h"

namespace mime_opengex {
inline constexpr const char *type = "model/vnd.opengex";

struct profile {
    bool identified = false, incomplete = false, bom = false;
    bool repertoire_known = false, ascii_only = false;
    std::string encoding, width;
    void observe_text(mime_unicode::text_source &source, bool whole, bool inspect_encoding)
    {
	width = source.width();
	bom = source.marked();
	if (bom) encoding = source.encoding();
	if (whole && inspect_encoding) {
	    encoding = source.finish_encoding();
	    repertoire_known = !encoding.empty();
	    ascii_only = source.ascii;
	    /* The registration specifies UTF-8, including its ASCII subset.
	     * Keep character repertoire separate from source encoding. */
	    if (encoding == "ascii") encoding = "utf-8";
	}
    }
    bool append_traits(bu_mime_traits &traits) const
    {
	return identified && bu_mime_traits_add(&traits, "opengex.serialization", "text") &&
	    bu_mime_traits_add(&traits, "text.code-unit-width", width.c_str()) &&
	    bu_mime_traits_add(&traits, "opengex.text-bom", bom ? "true" : "false") &&
	    (encoding.empty() || bu_mime_traits_add(&traits, "encoding", encoding.c_str())) &&
	    (!repertoire_known || bu_mime_traits_add(&traits, "text.ascii-only", ascii_only ? "true" : "false"));
    }
};

inline bool structure(const std::string &word, bool prefix = false)
{
    for (const char *name : {"GeometryNode", "GeometryObject", "CameraNode", "CameraObject", "LightNode", "LightObject", "BoneNode"})
	if (word == name || (prefix && std::string(name).compare(0, word.size(), word) == 0)) return true;
    return false;
}

/* OpenGEX has no magic number. Require an unquoted top-level structure
 * header; generic Metric data alone does not establish this identity. */
inline profile inspect(std::istream &stream, bool whole, bool inspect_encoding = true)
{
    profile result;
    mime_unicode::text_source source(stream);
    mime_detail::unquoted_line unquoted;
    size_t depth = 0, parentheses = 0;
    bool want_type = true, candidate = false, invalid = false;
    const bool byte_text = source.encoding() == "utf-8";
    std::string word, line;
    const auto feed = [&](char c) {
	if (depth) {
	    if (c == '{') ++depth;
	    else if (c == '}' && !--depth) {want_type = true; word.clear();}
	    return false;
	}
	if (want_type) {
	    if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '_' ||
		(!word.empty() && c >= '0' && c <= '9')) {
		/* Only fixed structure keywords need storage. */
		if (word.size() < sizeof("GeometryObject")) word += c;
		return false;
	    }
	    if (word.empty()) {
		if (!mime_unicode::text_source::space(static_cast<unsigned char>(c))) invalid = true;
		return false;
	    }
	    candidate = structure(word);
	    want_type = false;
	}
	if (c == '(') ++parentheses;
	else if (c == ')' && parentheses) --parentheses;
	else if (c == '{' && !parentheses) {
	    if (candidate) return true;
	    depth = 1;
	}
	return false;
    };
    while (source.peek() >= 0) {
	line.clear();
	while (source.peek() >= 0) {
	    const int c = source.get();
	    if (c == '\r' || c == '\n') break;
	    if (byte_text) line += static_cast<char>(c);
	    else mime_unicode::characters::append_scalar(line, static_cast<uint32_t>(c));
	}
	const std::string text = unquoted(line) + '\n';
	for (char c : text) {
	    const bool matched = feed(c);
	    if (invalid) return result;
	    if (!matched) continue;
	    result.identified = true;
	    result.observe_text(source, whole, inspect_encoding);
	    return result;
	}
    }
    result.incomplete = !whole && ((!word.empty() && structure(word, true)) || word == "Metric" || !unquoted.complete());
    return result;
}
}
#endif
