/* M I M E _ Z A R R . H
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
#ifndef BU_MIME_ZARR_H
#define BU_MIME_ZARR_H
#include "common.h"
#include <cstdint>
#include <istream>
#include <set>
#include <string>
#include "bu/mime_runtime.h"
#include "mime_json.h"

namespace mime_zarr {
constexpr const char *type = "application/vnd.zarr";
using mime_json::json;
struct profile {
    std::string version, node_type, consolidated_format;
    bool identified = false, consistent = true, incomplete = false;
    bool append_traits(bu_mime_traits &traits, const char *entrypoint) const
    {
	if (!identified) return false;
	if (!consistent) return true;
	return bu_mime_traits_add(&traits, "zarr.entrypoint", entrypoint) &&
	    (version.empty() || bu_mime_traits_add(&traits, "version", version.c_str())) &&
	    (node_type.empty() || bu_mime_traits_add(&traits, "zarr.node-type", node_type.c_str())) &&
	    (consolidated_format.empty() || bu_mime_traits_add(&traits, "zarr.consolidated-format", consolidated_format.c_str()));
    }
};
inline std::string integer(const json &object, const char *key)
{
    const auto field = object.find(key);
    if (field == object.end() || !field->is_number_integer()) return {};
    const bool positive = field->is_number_unsigned() ? field->get<uint64_t>() > 0 : field->get<int64_t>() > 0;
    return positive ? field->dump() : "";
}
inline profile node(const json &object)
{
    profile out;
    if (!object.is_object()) return out;
    out.version = integer(object, "zarr_format");
    out.identified = !out.version.empty();
    /* V2 node roles come from the metadata key (.zarray or .zgroup),
     * rather than the absence of array fields in a byte prefix. */
    if (!out.identified || out.version == "2") return out;
    const auto kind = object.find("node_type");
    if (kind != object.end() && kind->is_string()) {
	const std::string value = kind->get<std::string>();
	if (value.find('\0') == std::string::npos) out.node_type = value;
    }
    return out;
}
inline profile inspect(const json &object)
{
    profile out = node(object);
    if (out.identified || !object.is_object()) return out;
    out.consolidated_format = integer(object, "zarr_consolidated_format");
    if (out.consolidated_format.empty()) return out;
    out.identified = true;
    const auto metadata = object.find("metadata");
    if (metadata == object.end() || !metadata->is_object()) {out.consistent = false; return out;}
    if (out.consolidated_format != "1") return out;
    const bool array = metadata->contains(".zarray"), group = metadata->contains(".zgroup");
    if (array && group) {out.consistent = false; return out;}
    if (array || group) {
	const auto root = node((*metadata)[array ? ".zarray" : ".zgroup"]);
	if (!root.identified || root.version != "2") {out.consistent = false; return out;}
    }
    /* Consolidated format 1 uses V2 metadata. Without a root record,
     * retain the version and leave the node role unobserved. */
    out.version = "2";
    if (array || group) out.node_type = array ? "array" : "group";
    return out;
}
class declarations {
    /* The deepest owned declaration is metadata[".zarray"].zarr_format. */
    mime_json::declarations<3> fields;
public:
    json::parser_callback_t callback()
    {
	return fields.callback([](int depth, const auto &parents, const std::string &key) {
	    const bool root = depth == 1 && (key == "zarr_format" || key == "node_type" ||
		key == "zarr_consolidated_format" || key == "metadata");
	    const bool metadata = depth == 2 && parents[1] == "metadata" && (key == ".zarray" || key == ".zgroup");
	    const bool node_version = depth == 3 && parents[1] == "metadata" &&
		(parents[2] == ".zarray" || parents[2] == ".zgroup") && key == "zarr_format";
	    return root || metadata || node_version;
	});
    }
    profile finish(const json &object) const
    {
	profile out = inspect(object);
	out.consistent = out.consistent && fields.unique();
	return out;
    }
};
inline profile inspect(std::istream &stream)
{
    declarations fields;
    return fields.finish(json::parse(stream, fields.callback(), false));
}
inline profile inspect(const unsigned char *data, size_t size, bool whole)
{
    const auto *begin = mime_json::object_begin(data, size);
    if (!begin) return {};
    declarations fields;
    const auto object = json::parse(begin, data + size, fields.callback(), false);
    if (object.is_discarded()) {profile out; out.incomplete = !whole; return out;}
    return fields.finish(object);
}
} // namespace mime_zarr
#endif
