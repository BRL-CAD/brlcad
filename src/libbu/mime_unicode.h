/* M I M E _ U N I C O D E . H
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
#ifndef BU_MIME_UNICODE_H
#define BU_MIME_UNICODE_H
#include "common.h"
#include <cctype>
#include <cstdint>
#include <istream>
#include <string>

namespace mime_unicode {
class characters {
    std::istream &input;
    unsigned char prefix[4] = {};
    size_t count, at = 0, width = 1;
    bool little = false;
    bool invalid = false;
    bool marked = false;
    int lookahead = -2;
    int byte() {return at < count ? prefix[at++] : input.get();}
    int unit()
    {
	uint32_t value = 0;
	for (size_t i = 0; i < width; ++i) {
	    int b = byte();
	    if (b < 0) {if (i) invalid = true; return -1;}
	    value |= uint32_t(b) << (8 * (little ? i : width - i - 1));
	}
	if (value > 0x10ffff) {invalid = true; return -1;}
	return static_cast<int>(value);
    }
    int read()
    {
	int c = unit();
	if (width == 2 && c >= 0xd800 && c <= 0xdbff) {
	    int low = unit();
	    if (low < 0xdc00 || low > 0xdfff) {invalid = true; return -1;}
	    return 0x10000 + ((c - 0xd800) << 10) + low - 0xdc00;
	}
	if (width > 1 && c >= 0xd800 && c <= 0xdfff) {invalid = true; return -1;}
	return c;
    }
public:
    std::string encoding = "utf-8";
    explicit characters(std::istream &stream) : input(stream)
    {
	input.read(reinterpret_cast<char *>(prefix), sizeof(prefix));
	count = static_cast<size_t>(input.gcount());
	if (count >= 4 && prefix[0] == 0 && prefix[1] == 0 && prefix[2] == 0xfe && prefix[3] == 0xff) {width = 4; at = 4;}
	else if (count >= 4 && prefix[0] == 0xff && prefix[1] == 0xfe && prefix[2] == 0 && prefix[3] == 0) {width = 4; little = true; at = 4;}
	else if (count >= 2 && prefix[0] == 0xfe && prefix[1] == 0xff) {width = 2; at = 2;}
	else if (count >= 2 && prefix[0] == 0xff && prefix[1] == 0xfe) {width = 2; little = true; at = 2;}
	else if (count >= 3 && prefix[0] == 0xef && prefix[1] == 0xbb && prefix[2] == 0xbf) at = 3;
	else if (count == 4 && prefix[0] == 0 && prefix[1] == 0 && prefix[2] == 0 && prefix[3] == '<') width = 4;
	else if (count == 4 && prefix[0] == '<' && prefix[1] == 0 && prefix[2] == 0 && prefix[3] == 0) {width = 4; little = true;}
	else if (count == 4 && prefix[0] == 0 && prefix[1] == '<' && prefix[2] == 0) width = 2;
	else if (count == 4 && prefix[0] == '<' && prefix[1] == 0 && prefix[3] == 0) {width = 2; little = true;}
	if (width > 1) encoding = std::string(width == 2 ? "utf-16" : "utf-32") + (little ? "le" : "be");
	marked = at != 0;
    }
    bool has_bom() const {return marked;}
    int peek() {if (lookahead == -2) lookahead = read(); return lookahead;}
    int get() {int c = peek(); lookahead = -2; return c;}
    bool valid() const {return !invalid;}
    void append(std::string &out, int c) const
    {
	if (width == 1 || c < 0x80) out += static_cast<char>(c);
	else append_scalar(out, static_cast<uint32_t>(c));
    }
    static void append_scalar(std::string &out, uint32_t c)
    {
	if (c < 0x80) out += static_cast<char>(c);
	else if (c < 0x800) {out += static_cast<char>(0xc0 | (c >> 6)); out += static_cast<char>(0x80 | (c & 0x3f));}
	else if (c < 0x10000) {out += static_cast<char>(0xe0 | (c >> 12)); out += static_cast<char>(0x80 | ((c >> 6) & 0x3f)); out += static_cast<char>(0x80 | (c & 0x3f));}
	else {out += static_cast<char>(0xf0 | (c >> 18)); out += static_cast<char>(0x80 | ((c >> 12) & 0x3f)); out += static_cast<char>(0x80 | ((c >> 6) & 0x3f)); out += static_cast<char>(0x80 | (c & 0x3f));}
    }
    void space() {while (peek() == ' ' || peek() == '\t' || peek() == '\r' || peek() == '\n') get();}
    std::string name()
    {
	std::string value;
	while (peek() >= 0 && (peek() >= 128 || std::isalnum(static_cast<unsigned char>(peek())) ||
	    peek() == '_' || peek() == ':' || peek() == '-' || peek() == '.')) append(value, get());
	return value;
    }
    bool until(const std::string &end)
    {
	std::string tail;
	while (peek() >= 0) {
	    int c = get();
	    tail += c < 128 ? static_cast<char>(c) : '\0';
	    if (tail.size() > end.size()) tail.erase(0, 1);
	    if (tail == end) return true;
	}
	return false;
    }
};

/* Character encoding needs evidence beyond an ASCII-looking header.
 * Track bytes without retaining a scene when a caller requests that fact. */
class text_source {
    characters input;
    unsigned remaining = 0, low = 0x80, high = 0xbf;
    bool utf8 = true;
public:
    bool ascii = true;
    explicit text_source(std::istream &stream) : input(stream) {}
    bool marked() const {return input.has_bom();}
    const std::string &encoding() const {return input.encoding;}
    const char *width() const
    {
	return encoding().compare(0, 6, "utf-16") == 0 ? "16" : encoding().compare(0, 6, "utf-32") == 0 ? "32" : "8";
    }
    int peek() {return input.peek();}
    int get()
    {
	const int c = input.get();
	if (c < 0) return c;
	if (c >= 0x80) ascii = false;
	if (encoding() != "utf-8" || !utf8) return c;
	const unsigned byte = static_cast<unsigned>(c);
	/* RFC 3629 excludes overlong sequences, surrogates and values beyond
	 * Unicode's maximum from otherwise well-shaped continuation bytes. */
	if (remaining) {
	    if (byte < low || byte > high) utf8 = false;
	    --remaining; low = 0x80; high = 0xbf;
	} else if (byte < 0x80) {
	    return c;
	} else if (byte >= 0xc2 && byte <= 0xdf) remaining = 1;
	else if (byte >= 0xe0 && byte <= 0xef) {
	    remaining = 2;
	    if (byte == 0xe0) low = 0xa0;
	    if (byte == 0xed) high = 0x9f;
	} else if (byte >= 0xf0 && byte <= 0xf4) {
	    remaining = 3;
	    if (byte == 0xf0) low = 0x90;
	    if (byte == 0xf4) high = 0x8f;
	} else utf8 = false;
	return c;
    }
    bool valid() const {return input.valid() && utf8 && !remaining;}
    std::string finish_encoding()
    {
        while (get() >= 0) {}
        if (!valid()) return {};
        if (marked()) return encoding();
        return ascii ? "ascii" : "utf-8";
    }
    static bool space(int c) {return c >= 0 && c < 0x80 && std::isspace(static_cast<unsigned char>(c));}
    void spaces() {while (space(peek())) get();}
    bool quoted()
    {
	if (get() != '"') return false;
	bool escaped = false;
	for (int c = get(); c >= 0; c = get()) {
	    if (escaped) escaped = false;
	    else if (c == '\\') escaped = true;
	    else if (c == '"') return true;
	}
	return false;
    }
};


}
#endif
