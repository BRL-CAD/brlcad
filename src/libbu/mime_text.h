/* M I M E _ T E X T . H
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
#ifndef BU_MIME_TEXT_H
#define BU_MIME_TEXT_H
#include "common.h"
#include <cmath>
#include <istream>
#include <locale>
#include <set>
#include <sstream>
#include <string>
#include <vector>
#include "bu/mime_runtime.h"

namespace mime_detail {
inline bool read_line(std::istream &input, std::string &value, bool whole, bool &incomplete)
{
    value.clear();
    for (int c = input.get(); c >= 0; c = input.get()) {
	if (c == '\r' || c == '\n') {
	    if (c == '\r' && input.peek() == '\n') input.get();
	    return true;
	}
	value += static_cast<char>(c);
    }
    incomplete = !whole;
    return whole && !value.empty() && !input.bad();
}

inline std::string trim(const std::string &text)
{
    size_t first = text.find_first_not_of(" \t\r\n");
    if (first == std::string::npos)
	return "";
    return text.substr(first, text.find_last_not_of(" \t\r\n") - first + 1);
}

inline std::string upper(std::string text)
{
    for (char &c : text)
	if (c >= 'a' && c <= 'z')
	    c += 'A' - 'a';
    return text;
}

inline bool finite_number(const std::string &word)
{
    std::istringstream number(word);
    number.imbue(std::locale::classic());
    double value;
    return (number >> value) && (number >> std::ws).eof() && std::isfinite(value);
}

inline bool append_set(bu_mime_traits &traits, const char *key, const std::set<std::string> &values)
{
    std::vector<const char *> members;
    for (const auto &value : values) members.push_back(value.c_str());
    return bu_mime_traits_add_set(&traits, key, members.data(), members.size()) != 0;
}

inline std::string decimal_field(const std::string &value)
{
    if (value.empty() || value.find_first_not_of("0123456789") != std::string::npos) return "";
    size_t first = value.find_first_not_of('0');
    return first == std::string::npos ? "0" : value.substr(first);
}

/* Section names inside comments and quoted data are not declarations. */
class unquoted_line {
    bool block = false, quoted = false, escaped = false;
    bool hash_comments;
public:
    explicit unquoted_line(bool hash = false) : hash_comments(hash) {}
    bool complete() const {return !block && !quoted;}
    std::string operator()(const std::string &raw)
    {
	std::string text;
	for (size_t at = 0; at < raw.size(); ++at) {
	    char c = raw[at];
	    char next = at + 1 < raw.size() ? raw[at + 1] : '\0';
	    if (block) {
		if (c == '*' && next == '/') {block = false; ++at;}
	    } else if (quoted) {
		if (escaped) escaped = false;
		else if (c == '\\') escaped = true;
		else if (c == '"') quoted = false;
	    } else if (c == '"') {
		quoted = true;
		text += ' ';
	    } else if ((c == '/' && next == '/') || (hash_comments && c == '#')) break;
	    else if (c == '/' && next == '*') {
		block = true;
		text += ' ';
		++at;
	    } else text += c;
	}
	return text;
    }
};
}
#endif
