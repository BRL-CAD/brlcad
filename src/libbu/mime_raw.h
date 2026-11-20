/* M I M E _ R A W . H
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
#ifndef BU_MIME_RAW_H
#define BU_MIME_RAW_H
#include "common.h"
#include <istream>
#include <set>
#include <sstream>
#include <string>
#include <vector>
#include "mime_text.h"

/* RAW has no magic identifier. Use this grammar only with a filename hint
 * or an explicit type; a nine-column numeric table is not proof of a mesh. */
namespace mime_raw {
inline bool is_type(const char *type)
{
    return type && (std::string(type) == "model/vnd.povray.raw" || std::string(type) == "model/vnd.iabg.raw");
}

struct profile {
    std::string type;
    std::set<std::string> features;
    bool features_known = false;
    bool shared_named_subset = false;
    bool append_traits(bu_mime_traits &traits) const
    {
	return type.empty() || (bu_mime_traits_add(&traits, "encoding", "ascii") &&
	    (!features_known || mime_detail::append_set(traits, "raw.features", features)));
    }
};

inline profile inspect(std::istream &input, bool complete)
{
    constexpr size_t triangle_coordinates = 9, colored_triangle_fields = 12;
    profile result;
    bool triangle = false, plate = false, first = true, unnamed = false;
    std::string group = "<default>";
    std::set<std::string> populated_groups;
    std::string raw;
    while (std::getline(input, raw)) {
	if (input.eof() && !complete) break;
	if (first && raw.compare(0, 3, "\xef\xbb\xbf") == 0) raw.erase(0, 3);
	first = false;
	std::istringstream line(raw);
	std::vector<std::string> fields;
	std::string field;
	while (line >> field) fields.push_back(field);
	if (fields.empty()) continue;
	if (raw.find('\0') != std::string::npos) {
	    result.features.insert("unknown-records");
	    continue;
	}
	if (!mime_detail::finite_number(fields.front())) {
	    /* The SDK treats any nonnumeric first token as a group name.
	     * Additional header metadata must not disappear unnoticed. */
	    result.features.insert("groups");
	    group = fields.front();
	    for (size_t i = 1; i < fields.size(); ++i) {
		if (fields[i].compare(0, sizeof("thickf=") - 1, "thickf=") == 0) {
		    plate = true;
		    result.features.insert("plate-thickness");
		} else result.features.insert("group-metadata");
	    }
	    continue;
	}
	size_t numbers = 0;
	while (numbers < fields.size() && mime_detail::finite_number(fields[numbers])) ++numbers;
	if ((numbers != triangle_coordinates && numbers != colored_triangle_fields) || fields.size() > numbers + 1) {
	    result.features.insert("unknown-records");
	    continue;
	}
	triangle = true;
	unnamed = unnamed || !result.features.count("groups");
	populated_groups.insert(group);
	if (populated_groups.size() > 1) result.features.insert("multiple-groups");
	if (numbers == colored_triangle_fields) result.features.insert("face-colors");
	if (fields.size() > numbers) {
	    result.features.insert("textures");
	    if (numbers == colored_triangle_fields) result.features.insert("colored-textures");
	}
    }
    if (triangle) {
	result.type = plate ? "model/vnd.iabg.raw" : "model/vnd.povray.raw";
	result.features_known = complete && input.eof() && !input.bad();
	result.shared_named_subset = !plate && !unnamed && result.features.count("groups") &&
	    (result.features.size() == 1 || (result.features.size() == 2 && result.features.count("multiple-groups")));
    }
    return result;
}
}
#endif
