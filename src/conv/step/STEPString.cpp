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

const char *
latin_ascii(uint32_t cp)
{
    switch (cp) {
	case 0x00c0: case 0x00c1: case 0x00c2: case 0x00c3: case 0x00c4: case 0x00c5: return "A";
	case 0x00c6: return "AE";
	case 0x00c7: return "C";
	case 0x00c8: case 0x00c9: case 0x00ca: case 0x00cb: return "E";
	case 0x00cc: case 0x00cd: case 0x00ce: case 0x00cf: return "I";
	case 0x00d0: return "D";
	case 0x00d1: return "N";
	case 0x00d2: case 0x00d3: case 0x00d4: case 0x00d5: case 0x00d6: case 0x00d8: return "O";
	case 0x00d9: case 0x00da: case 0x00db: case 0x00dc: return "U";
	case 0x00dd: return "Y";
	case 0x00de: return "TH";
	case 0x00df: return "ss";
	case 0x00e0: case 0x00e1: case 0x00e2: case 0x00e3: case 0x00e4: case 0x00e5: return "a";
	case 0x00e6: return "ae";
	case 0x00e7: return "c";
	case 0x00e8: case 0x00e9: case 0x00ea: case 0x00eb: return "e";
	case 0x00ec: case 0x00ed: case 0x00ee: case 0x00ef: return "i";
	case 0x00f0: return "d";
	case 0x00f1: return "n";
	case 0x00f2: case 0x00f3: case 0x00f4: case 0x00f5: case 0x00f6: case 0x00f8: return "o";
	case 0x00f9: case 0x00fa: case 0x00fb: case 0x00fc: return "u";
	case 0x00fd: case 0x00ff: return "y";
	case 0x00fe: return "th";
	default: return NULL;
    }
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
brlcad::step::sanitize_name(const std::string &input)
{
    const std::string decoded = decode_string(input);
    std::string out;
    bool separator = false;
    for (size_t i = 0; i < decoded.size();) {
	uint32_t cp = next_utf8(decoded, i);
	if (cp < 0x80 && std::isalnum(static_cast<unsigned char>(cp))) {
	    if (separator && !out.empty() && out.back() != '_') out.push_back('_');
	    separator = false;
	    out.push_back(static_cast<char>(cp));
	    continue;
	}
	if (cp == '_') {
	    separator = !out.empty();
	    continue;
	}
	const char *ascii = latin_ascii(cp);
	if (ascii) {
	    if (separator && !out.empty() && out.back() != '_') out.push_back('_');
	    separator = false;
	    out.append(ascii);
	    continue;
	}
	if (cp > 0x7f) {
	    if (separator && !out.empty() && out.back() != '_') out.push_back('_');
	    separator = false;
	    std::ostringstream code;
	    code << "u" << std::uppercase << std::hex << cp;
	    if (!out.empty() && out.back() != '_') out.push_back('_');
	    out.append(code.str());
	    continue;
	}
	separator = !out.empty();
    }
    while (!out.empty() && out.back() == '_') out.pop_back();
    return out.empty() ? std::string("step") : out;
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
