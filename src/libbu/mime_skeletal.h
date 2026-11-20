/* M I M E _ S K E L E T A L . H
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

#ifndef BU_MIME_SKELETAL_H
#define BU_MIME_SKELETAL_H

#include "common.h"
#include "mime_text.h"
#include <istream>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

/* Shared by bounded inspection and the SDK's full-reader validation. The
 * section scan identifies exchange variants, without parsing geometry. */
namespace mime_skeletal {
struct profile {
    std::string type, version, kind;
    std::vector<std::pair<std::string, std::string>> traits() const
    {
	std::vector<std::pair<std::string, std::string>> values;
	if (type.empty()) return values;
	values.emplace_back("encoding", "ascii");
	if (!version.empty()) values.emplace_back("version", version);
	if (!kind.empty()) values.emplace_back(type == "model/vnd.valve.smd" ? "smd.kind" : "md5.kind", kind);
	return values;
    }
};

inline bool is_type(const char *type)
{
    return type && (std::string(type) == "model/vnd.valve.smd" || std::string(type) == "model/vnd.idsoftware.md5");
}

inline profile inspect(std::istream &input, bool complete)
{
    enum {mesh_kind = 1, animation_kind = 2, camera_kind = 4};
    profile result;
    bool header = false, smd = false, section = false;
    mime_detail::unquoted_line lines;
    unsigned kinds = 0;
    size_t depth = 0;
    std::string raw;
    while (std::getline(input, raw)) {
	if (input.eof() && !complete) break;
	std::string text = lines(raw);
	std::istringstream fields(text);
	std::string keyword, value, extra;
	if (!(fields >> keyword)) continue;
	if (!header) {
	    if ((keyword != "MD5Version" && keyword != "version") || !(fields >> value) || (fields >> extra)) return result;
	    result.version = mime_detail::decimal_field(value);
	    if (result.version.empty()) return result;
	    header = true;
	    smd = keyword == "version";
	    if (!smd) result.type = "model/vnd.idsoftware.md5";
	    continue;
	}
	if ((!smd && !depth && keyword == "MD5Version") || (smd && !section && keyword == "version")) {
	    if (!(fields >> value) || (fields >> extra) || mime_detail::decimal_field(value) != result.version) result.version.clear();
	    continue;
	}
	if (smd) {
	    if (section) {
		if (keyword == "end" && !(fields >> extra)) section = false;
		continue;
	    }
	    if ((keyword == "nodes" || keyword == "skeleton" || keyword == "triangles" || keyword == "vertexanimation") && !(fields >> extra)) {
		result.type = "model/vnd.valve.smd";
		section = true;
		if (keyword == "triangles") kinds |= mesh_kind;
		if (keyword == "vertexanimation") kinds |= animation_kind;
	    }
	} else {
	    if (!depth) {
		if (keyword == "numMeshes" || keyword == "mesh") kinds |= mesh_kind;
		if (keyword == "numAnimatedComponents" || keyword == "hierarchy" || keyword == "baseframe") kinds |= animation_kind;
		if (keyword == "numCuts" || keyword == "camera") kinds |= camera_kind;
	    }
	    for (char c : text) {
		if (c == '{') ++depth;
		if (c == '}') {
		    if (!depth) return result;
		    --depth;
		}
	    }
	}
    }
    /* Absence of a later vertex-animation or camera section is established
     * only at EOF. A bounded prefix must not claim a complete kind. */
    if (complete && !input.bad() && !section && !depth && lines.complete() && !result.type.empty()) {
	if (kinds == mesh_kind) result.kind = "mesh";
	else if (kinds == animation_kind) result.kind = smd ? "vertex-animation" : "animation";
	else if (kinds == camera_kind) result.kind = "camera";
	else if (kinds) result.kind = "mixed";
	else if (smd) result.kind = "skeleton";
    }
    return result;
}
}
#endif
