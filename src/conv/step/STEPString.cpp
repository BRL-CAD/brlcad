/*               S T E P S T R I N G . C P P
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

#include "STEPString.h"

#include "rt/db_io.h"

#include <cctype>
#include <climits>
#include <cstdint>
#include <iomanip>
#include <sstream>

namespace {

void
append_utf8(std::string &out, uint32_t cp)
{
    if (cp > 0x10ffff || (cp >= 0xd800 && cp <= 0xdfff))
	cp = 0xfffd;

    if (cp <= 0x7f) {
	out.push_back(static_cast<char>(cp));
    } else if (cp <= 0x7ff) {
	out.push_back(static_cast<char>(0xc0 | (cp >> 6)));
	out.push_back(static_cast<char>(0x80 | (cp & 0x3f)));
    } else if (cp <= 0xffff) {
	out.push_back(static_cast<char>(0xe0 | (cp >> 12)));
	out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3f)));
	out.push_back(static_cast<char>(0x80 | (cp & 0x3f)));
    } else {
	out.push_back(static_cast<char>(0xf0 | (cp >> 18)));
	out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3f)));
	out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3f)));
	out.push_back(static_cast<char>(0x80 | (cp & 0x3f)));
    }
}

int
hex_value(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

bool
parse_hex(const std::string &s, size_t pos, size_t digits, uint32_t &value)
{
    if (pos + digits > s.size()) return false;
    value = 0;
    for (size_t i = 0; i < digits; ++i) {
	int h = hex_value(s[pos + i]);
	if (h < 0) return false;
	value = (value << 4) | static_cast<uint32_t>(h);
    }
    return true;
}

bool
escape_at(const std::string &s, size_t pos, const char *tag)
{
    size_t i = 0;
    while (tag[i]) {
	if (pos + i >= s.size()) return false;
	unsigned char a = static_cast<unsigned char>(s[pos + i]);
	unsigned char b = static_cast<unsigned char>(tag[i]);
	if (std::toupper(a) != std::toupper(b)) return false;
	++i;
    }
    return true;
}

uint32_t
next_utf8(const std::string &s, size_t &pos)
{
    unsigned char c = static_cast<unsigned char>(s[pos++]);
    if (c < 0x80) return c;

    int count = 0;
    uint32_t cp = 0;
    if ((c & 0xe0) == 0xc0) {
	count = 1;
	cp = c & 0x1f;
    } else if ((c & 0xf0) == 0xe0) {
	count = 2;
	cp = c & 0x0f;
    } else if ((c & 0xf8) == 0xf0) {
	count = 3;
	cp = c & 0x07;
    } else {
	return 0xfffd;
    }
    while (count--) {
	if (pos >= s.size()) return 0xfffd;
	unsigned char n = static_cast<unsigned char>(s[pos]);
	if ((n & 0xc0) != 0x80) return 0xfffd;
	++pos;
	cp = (cp << 6) | (n & 0x3f);
    }
    return cp;
}


} // namespace

std::string
brlcad::step::decode_string(const std::string &input)
{
    size_t begin = 0;
    size_t end = input.size();
    if (end >= 2 && input.front() == '\'' && input.back() == '\'') {
	begin = 1;
	--end;
    }

    std::string out;
    for (size_t i = begin; i < end;) {
	if (input[i] == '\'' && i + 1 < end && input[i + 1] == '\'') {
	    out.push_back('\'');
	    i += 2;
	    continue;
	}
	if (input[i] != '\\') {
	    out.push_back(input[i++]);
	    continue;
	}

	if (i + 1 < end && input[i + 1] == '\\') {
	    out.push_back('\\');
	    i += 2;
	    continue;
	}
	if (escape_at(input, i, "\\N\\")) {
	    out.push_back('\n');
	    i += 3;
	    continue;
	}
	if (escape_at(input, i, "\\T\\")) {
	    out.push_back('\t');
	    i += 3;
	    continue;
	}
	if (i + 3 < end && input[i + 1] == 'P' && input[i + 3] == '\\') {
	    i += 4; // ISO-8859 page selection; page A is represented by UTF-8 below.
	    continue;
	}
	if (escape_at(input, i, "\\S\\") && i + 3 < end) {
	    append_utf8(out, static_cast<unsigned char>(input[i + 3]) + 128U);
	    i += 4;
	    continue;
	}
	if (escape_at(input, i, "\\X\\")) {
	    uint32_t cp = 0;
	    if (parse_hex(input, i + 3, 2, cp)) {
		append_utf8(out, cp);
		i += 5;
		continue;
	    }
	}

	size_t digits = 0;
	if (escape_at(input, i, "\\X2\\")) digits = 4;
	if (escape_at(input, i, "\\X4\\")) digits = 8;
	if (digits) {
	    size_t p = i + 4;
	    while (p < end && !escape_at(input, p, "\\X0\\")) {
		uint32_t cp = 0;
		if (!parse_hex(input, p, digits, cp)) {
		    append_utf8(out, 0xfffd);
		    while (p < end && !escape_at(input, p, "\\X0\\")) ++p;
		    break;
		}
		p += digits;
		if (digits == 4 && cp >= 0xd800 && cp <= 0xdbff) {
		    uint32_t low = 0;
		    if (parse_hex(input, p, 4, low) && low >= 0xdc00 && low <= 0xdfff) {
			p += 4;
			cp = 0x10000 + ((cp - 0xd800) << 10) + (low - 0xdc00);
		    }
		}
		append_utf8(out, cp);
	    }
	    i = escape_at(input, p, "\\X0\\") ? p + 4 : p;
	    continue;
	}

	// Preserve an unknown escape literally so metadata is not silently lost.
	out.push_back(input[i++]);
    }
    return out;
}

std::string
brlcad::step::encode_string(const std::string &input)
{
    std::ostringstream out;
    out << '\'';
    bool unicode = false;
    const auto end_unicode = [&out, &unicode]() {
	if (!unicode) return;
	out << "\\X0\\";
	unicode = false;
    };
    for (size_t position = 0; position < input.size();) {
	const uint32_t cp = next_utf8(input, position);
	if (cp >= 0x20 && cp <= 0x7e && cp != '\\') {
	    end_unicode();
	    if (cp == '\'') out << "''";
	    else out << static_cast<char>(cp);
	    continue;
	}
	if (!unicode) {
	    out << "\\X2\\";
	    unicode = true;
	}
	out << std::uppercase << std::hex << std::setfill('0');
	if (cp <= 0xffff) {
	    out << std::setw(4) << cp;
	} else {
	    const uint32_t value = cp - 0x10000;
	    out << std::setw(4) << (0xd800 + (value >> 10));
	    out << std::setw(4) << (0xdc00 + (value & 0x3ff));
	}
	out << std::dec;
    }
    end_unicode();
    out << '\'';
    return out.str();
}

std::string
brlcad::step::sanitize_name(const std::string &input)
{
    const std::string decoded = decode_string(input);
    struct bu_vls output = BU_VLS_INIT_ZERO;

    db_sanitize_name(&output, decoded.c_str());
    const std::string result = bu_vls_cstr(&output);
    bu_vls_free(&output);
    return result.empty() ? std::string("step") : result;
}

std::string
brlcad::step::json_escape(const std::string &input)
{
    std::ostringstream out;
    for (unsigned char c : input) {
	switch (c) {
	    case '"': out << "\\\""; break;
	    case '\\': out << "\\\\"; break;
	    case '\b': out << "\\b"; break;
	    case '\f': out << "\\f"; break;
	    case '\n': out << "\\n"; break;
	    case '\r': out << "\\r"; break;
	    case '\t': out << "\\t"; break;
	    default:
		if (c < 0x20) {
		    out << "\\u" << std::hex << std::setw(4) << std::setfill('0') << static_cast<unsigned int>(c) << std::dec;
		} else {
		    out << static_cast<char>(c);
		}
	}
    }
    return out.str();
}

bool
brlcad::step::parse_entity_id_list(const std::string &input,
	std::set<int64_t> &entity_ids, std::string *error)
{
    std::set<int64_t> parsed;
    size_t begin = 0;
    while (begin <= input.size()) {
	size_t end = input.find(',', begin);
	if (end == std::string::npos) end = input.size();
	size_t first = begin;
	while (first < end && std::isspace(static_cast<unsigned char>(input[first]))) ++first;
	size_t last = end;
	while (last > first && std::isspace(static_cast<unsigned char>(input[last - 1]))) --last;
	if (first < last && input[first] == '#') ++first;
	if (first == last) {
	    if (error) *error = "empty entity identifier in '" + input + "'";
	    return false;
	}

	uint64_t value = 0;
	for (size_t position = first; position < last; ++position) {
	    const unsigned char character = static_cast<unsigned char>(input[position]);
	    if (!std::isdigit(character)) {
		if (error) *error = "invalid entity identifier '" +
		    input.substr(first, last - first) + "'";
		return false;
	    }
	    const uint64_t digit = static_cast<uint64_t>(character - '0');
	    if (value > (static_cast<uint64_t>(INT64_MAX) - digit) / 10U) {
		if (error) *error = "entity identifier is out of range";
		return false;
	    }
	    value = value * 10U + digit;
	}
	if (!value) {
	    if (error) *error = "entity identifiers must be positive";
	    return false;
	}
	parsed.insert(static_cast<int64_t>(value));
	if (end == input.size()) break;
	begin = end + 1;
    }

    entity_ids.insert(parsed.begin(), parsed.end());
    if (error) error->clear();
    return true;
}
