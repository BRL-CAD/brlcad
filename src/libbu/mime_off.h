/* M I M E _ O F F . H
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
#ifndef BU_MIME_OFF_H
#define BU_MIME_OFF_H
#include "common.h"
#include <cctype>
#include <istream>
#include <limits>
#include <set>
#include <string>
#include "mime_binary.h"
#include "mime_text.h"

namespace mime_off {
inline constexpr const char *type = "model/vnd.geomview.off";
constexpr uint32_t default_dimension = 3;
constexpr size_t count_fields = 3;

struct profile {
    bool identified = false, interpreted = false, header_complete = false, incomplete = false;
    bool header_comments = false;
    std::string encoding, dimension;
    std::set<std::string> attributes;
    bool append_traits(bu_mime_traits &traits) const
    {
	return interpreted && bu_mime_traits_add(&traits, "off.header", identified ? "present" : "absent") &&
	    mime_detail::append_set(traits, "off.vertex-attributes", attributes) &&
	    (encoding.empty() || bu_mime_traits_add(&traits, "encoding", encoding.c_str())) &&
	    (encoding != "binary" || bu_mime_traits_add(&traits, "byte-order", "big-endian")) &&
	    (dimension.empty() || bu_mime_traits_add(&traits, "off.dimension", dimension.c_str())) &&
	    (!(header_comments || header_complete) ||
		bu_mime_traits_add(&traits, "off.header-comments", header_comments ? "true" : "false"));
    }
};

inline bool unsigned_word(const std::string &word, uint32_t &value)
{
    if (word.empty()) return false;
    value = 0;
    for (unsigned char c : word) {
	if (c < '0' || c > '9') return false;
	const unsigned digit = c - '0';
	if (value > (std::numeric_limits<uint32_t>::max() - digit) / 10) return false;
	value = value * 10 + digit;
    }
    return true;
}

class header_tokens {
    std::istream &input;
    bool complete;
    bool valid = true;
public:
    bool comments = false, incomplete = false;
    bool utf8_bom = false;
    header_tokens(std::istream &source, bool whole) : input(source), complete(whole)
    {
	if (input.peek() != 0xef) return;
	constexpr unsigned char bom[] = {0xef, 0xbb, 0xbf};
	for (unsigned char byte : bom)
	    if (input.get() != byte) {valid = false; incomplete = !complete && input.eof(); return;}
	utf8_bom = true;
    }
    bool skip()
    {
	if (!valid) return false;
	for (;;) {
	    const int c = input.peek();
	    if (c < 0) {incomplete = !complete; return false;}
	    if (std::isspace(static_cast<unsigned char>(c))) {input.get(); continue;}
	    if (c != '#') return true;
	    comments = true;
	    for (int byte = input.get(); byte >= 0 && byte != '\n' && byte != '\r'; byte = input.get()) {}
	}
    }
    bool word(std::string &value)
    {
	value.clear();
	if (!skip()) return false;
	/* Header keywords and unsigned 32-bit fields fit within ten bytes.
	 * A malformed token must not make full-file inspection buffer a body. */
	constexpr size_t token_size = std::numeric_limits<uint32_t>::digits10 + 1;
	for (int c = input.peek(); c >= 0 && c != '#' && !std::isspace(static_cast<unsigned char>(c)); c = input.peek()) {
	    if (value.size() == token_size) return false;
	    value += static_cast<char>(input.get());
	}
	if (input.eof() && !complete) {incomplete = true; return false;}
	return !value.empty();
    }
};

inline profile inspect(std::istream &input, bool complete, bool allow_headerless = false)
{
    profile out;
    header_tokens tokens(input, complete);
    std::string word;
    if (!tokens.word(word)) {out.incomplete = tokens.incomplete; return out;}
    size_t at = 0;
    const auto prefix = [&](const char *tag, const char *attribute) {
	const size_t size = std::char_traits<char>::length(tag);
	if (word.compare(at, size, tag)) return false;
	at += size;
	if (attribute) out.attributes.insert(attribute);
	return true;
    };
    prefix("ST", "texture-coordinates");
    prefix("C", "colors");
    prefix("N", "normals");
    prefix("4", "homogeneous-coordinates");
    const bool dimensional = prefix("n", NULL);
    out.identified = word.substr(at) == "OFF";
    uint32_t number = 0;
    if (!out.identified) {
	if (!allow_headerless || !unsigned_word(word, number)) return {};
	out.attributes.clear();
    }
    out.interpreted = true;
    if (!dimensional || !out.identified) out.dimension = std::to_string(default_dimension);
    const char *text_encoding = tokens.utf8_bom ? "utf-8" : "ascii";
    if (!out.identified) out.encoding = text_encoding;
    else if (!tokens.word(word)) {
	out.header_comments = tokens.comments;
	out.incomplete = tokens.incomplete;
	return out;
    } else out.encoding = word == "BINARY" ? "binary" : text_encoding;
    if (out.encoding == "binary") {
	/* Binary payload starts after the first newline following BINARY.
	 * Whitespace or a comment may precede that newline. */
	bool newline = false;
	for (int c = input.get(); c >= 0; c = input.get()) {
	    if (c == '\n') {newline = true; break;}
	    if (c == '#') tokens.comments = true;
	}
	if (newline) {
	    unsigned char dimension[sizeof(uint32_t)];
	    if (dimensional && input.read(reinterpret_cast<char *>(dimension), sizeof(dimension)))
		out.dimension = std::to_string(mime_binary::big_u32(dimension));
	    unsigned char counts[count_fields * sizeof(uint32_t)];
	    out.header_complete = (!dimensional || !out.dimension.empty()) &&
		static_cast<bool>(input.read(reinterpret_cast<char *>(counts), sizeof(counts)));
	}
    } else {
	bool valid = true;
	if (dimensional && out.identified) {
	    valid = unsigned_word(word, number);
	    if (valid) out.dimension = std::to_string(number);
	    valid = valid && tokens.word(word);
	}
	for (size_t field = 0; valid && field < count_fields; ++field) {
	    valid = unsigned_word(word, number);
	    if (field + 1 < count_fields) valid = valid && tokens.word(word);
	}
	if (valid) out.header_complete = tokens.skip() || (complete && input.eof());
    }
    out.header_comments = tokens.comments;
    out.incomplete = !out.header_complete && !complete && input.eof();
    return out;
}
} // namespace mime_off
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
