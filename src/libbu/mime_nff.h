/* M I M E _ N F F . H
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
#ifndef BU_MIME_NFF_H
#define BU_MIME_NFF_H
#include "common.h"
#include <istream>
#include <set>
#include <sstream>
#include <string>
#include "bu/mime_runtime.h"
#include "mime_text.h"

namespace mime_nff {
inline bool is_type(const char *type)
{
    return type && (std::string(type) == "model/prs.haines.nff" || std::string(type) == "model/vnd.sense8.nff");
}

inline bool numbers(const std::string &text, size_t minimum, size_t maximum)
{
    std::istringstream fields(text);
    std::string word;
    size_t count = 0;
    while (fields >> word) {
	if (!mime_detail::finite_number(word) || ++count > maximum) return false;
    }
    return count >= minimum;
}

struct profile {
    std::string type, version;
    std::set<std::string> extensions;
    bool extensions_known = false;
    bool append_traits(bu_mime_traits &traits) const
    {
	if (type.empty()) return true;
	if (!bu_mime_traits_add(&traits, "encoding", "ascii")) return false;
	if (!version.empty() && !bu_mime_traits_add(&traits, "version", version.c_str())) return false;
	return !extensions_known || mime_detail::append_set(traits, "nff.extensions", extensions);
    }
};

inline profile inspect(std::istream &input, bool complete)
{
    profile result;
    mime_detail::unquoted_line lines(true);
    bool first = true, sense8 = false, version_seen = false;
    size_t pending_numbers = 0;
    std::string pending_keyword, raw;
    static const std::set<std::string> standard = {"v", "from", "at", "up", "angle", "hither", "resolution", "b", "l", "f", "c", "s", "p", "pp"};
    static const std::set<std::string> known_extensions = {"dod", "hex", "oct", "shader", "tess", "tet", "tpp"};
    while (std::getline(input, raw)) {
	if (input.eof() && !complete) break;
	if (first && raw.compare(0, 3, "\xef\xbb\xbf") == 0) raw.erase(0, 3);
	std::string text = lines(raw), keyword, rest;
	std::istringstream fields(text);
	if (!(fields >> keyword)) continue;
	std::getline(fields, rest);
	if (first) {
	    first = false;
	    if (keyword == "nff" && rest.find_first_not_of(" \t\r") == std::string::npos) {
		result.type = "model/vnd.sense8.nff";
		sense8 = true;
		continue;
	    }
	}
	if (sense8) {
	    if (keyword == "version") {
		std::istringstream declaration(rest);
		std::string value, extra;
		int comparison;
		if (!(declaration >> value) || (declaration >> extra) || !bu_mime_version_compare(value.c_str(), value.c_str(), &comparison))
		    value.clear();
		if (!version_seen) result.version = value;
		else if (!bu_mime_version_compare(result.version.c_str(), value.c_str(), &comparison) || comparison) result.version.clear();
		version_seen = true;
		continue;
	    }
	    if (keyword == "viewpos" || keyword == "viewdir") continue;
	    break;
	}
	if (result.type.empty()) {
	    if (pending_numbers) {
		if ((pending_keyword.empty() && numbers(text, pending_numbers, pending_numbers)) ||
		    (keyword == pending_keyword && numbers(rest, pending_numbers, pending_numbers)))
		    result.type = "model/prs.haines.nff";
		else return result;
		pending_numbers = 0;
	    } else if (keyword == "s" && numbers(rest, 4, 6)) result.type = "model/prs.haines.nff";
	    else if ((keyword == "tet" || keyword == "hex" || keyword == "oct" || keyword == "dod") && numbers(rest, 4, 6))
		result.type = "model/prs.haines.nff";
	    else if (keyword == "v" && rest.find_first_not_of(" \t\r") == std::string::npos) {
		pending_keyword = "from";
		pending_numbers = 3;
	    } else if (keyword == "c" && rest.find_first_not_of(" \t\r") == std::string::npos) {
		pending_keyword.clear();
		pending_numbers = 4;
	    } else if (keyword == "p" || keyword == "pp" || keyword == "tpp") {
		std::istringstream count(rest);
		std::string value, extra;
		int comparison;
		if (!(count >> value) || (count >> extra) || mime_detail::decimal_field(value).empty() ||
		    !bu_mime_version_compare(value.c_str(), "3", &comparison) || comparison < 0) return result;
		pending_keyword.clear();
		pending_numbers = keyword == "p" ? 3 : keyword == "pp" ? 6 : 8;
	    } else if (!standard.count(keyword) && !known_extensions.count(keyword)) return result;
	}
	if (known_extensions.count(keyword)) result.extensions.insert(keyword);
	else if (!standard.count(keyword) && keyword.find_first_not_of("0123456789+-.eE") != std::string::npos)
	    result.extensions.insert(keyword);
    }
    if (result.type == "model/prs.haines.nff" && complete && !input.bad() && lines.complete()) result.extensions_known = true;
    return result;
}
}
#endif
