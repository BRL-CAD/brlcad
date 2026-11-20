/* M I M E _ J S O N . H
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
#ifndef BU_MIME_JSON_H
#define BU_MIME_JSON_H
#include "common.h"
#include <array>
#include <set>
#include <string>
#include "json.hpp"

namespace mime_json {
using json = nlohmann::json;
inline const unsigned char *object_begin(const unsigned char *data, size_t size)
{
    size_t at = 0;
    if (size >= 3 && data[0] == 0xef && data[1] == 0xbb && data[2] == 0xbf) at = 3;
    while (at < size && (data[at] == ' ' || data[at] == '\t' || data[at] == '\r' || data[at] == '\n')) ++at;
    return at < size && data[at] == '{' ? data + at : nullptr;
}

/* Track only declarations owned by an inspector. Opaque user attributes
 * are not format declarations, even when they use similar key names. */
template<size_t Depth> class declarations {
    std::array<std::string, Depth + 1> fields;
    std::array<std::set<std::string>, Depth + 1> seen;
    bool unique_fields = true;
public:
    bool unique() const {return unique_fields;}
    template<class Owns> json::parser_callback_t callback(Owns owns)
    {
	return [this, owns](int depth, json::parse_event_t event, json &value) {
	    if (event == json::parse_event_t::object_start && depth >= 0 && static_cast<size_t>(depth) < Depth)
		seen[depth + 1].clear();
	    if (event != json::parse_event_t::key || depth < 1 || static_cast<size_t>(depth) > Depth) return true;
	    const std::string key = value.get<std::string>();
	    if (owns(depth, fields, key) && !seen[depth].insert(key).second) unique_fields = false;
	    fields[depth] = key;
	    return true;
	};
    }
};
}
#endif
