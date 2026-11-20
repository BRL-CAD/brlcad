/* M I M E _ S T A C . H
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
#ifndef BU_MIME_STAC_H
#define BU_MIME_STAC_H
#include "common.h"
#include <istream>
#include <set>
#include <string>
#include "bu/mime_runtime.h"
#include "mime_json.h"
#include "mime_text.h"

namespace mime_stac {
using mime_json::json;
inline constexpr const char *type = "application/geo+json";
struct profile {
    std::string object_type, stac_kind;
    std::set<std::string> versions, extensions;
    bool identified = false, consistent = true, incomplete = false, asset_templates = false;
    bool items_have_assets = true;
    bool append_traits(bu_mime_traits &traits) const
    {
	if (!identified) return false;
	if (!consistent) return true;
	if (!bu_mime_traits_add(&traits, "encoding", "utf-8") ||
	    !bu_mime_traits_add(&traits, "geojson.object-type", object_type.c_str())) return false;
	if (stac_kind.empty()) return true;
	return bu_mime_traits_add(&traits, "stac.object-kind", stac_kind.c_str()) &&
	    bu_mime_traits_add(&traits, "stac.items-have-assets", items_have_assets ? "true" : "false") &&
	    mime_detail::append_set(traits, "stac.versions", versions) &&
	    mime_detail::append_set(traits, "stac.declared-extensions", extensions) &&
	    (stac_kind != "item" || bu_mime_traits_add(&traits, "stac.asset-templates", asset_templates ? "present" : "absent"));
    }
};
inline std::string string(const json &object, const char *key)
{
    const auto field = object.find(key);
    if (field == object.end() || !field->is_string()) return {};
    const auto value = field->get<std::string>();
    return value.find('\0') == std::string::npos ? value : "";
}
inline bool feature(const json &object)
{
    if (!object.is_object() || string(object, "type") != "Feature") return false;
    const auto geometry = object.find("geometry"), properties = object.find("properties");
    return geometry != object.end() && (geometry->is_null() || geometry->is_object()) &&
	properties != object.end() && (properties->is_null() || properties->is_object());
}
inline bool item(const json &object, profile &out)
{
    if (!feature(object)) return false;
    const auto version = string(object, "stac_version");
    if (version.empty()) return false;
    out.versions.insert(version);
    const auto assets = object.find("assets");
    if (assets != object.end() && !assets->is_object()) {out.consistent = false; return false;}
    out.items_have_assets = out.items_have_assets && assets != object.end() && !assets->empty();
    const auto extensions = object.find("stac_extensions");
    if (extensions != object.end()) {
	if (!extensions->is_array()) {out.consistent = false; return false;}
	for (const auto &entry : *extensions) {
	    if (!entry.is_string()) {out.consistent = false; return false;}
	    const auto name = entry.get<std::string>();
	    if (name.empty() || name.find('\0') != std::string::npos) {out.consistent = false; return false;}
	    out.extensions.insert(name);
	}
    }
    return true;
}
inline profile inspect(const json &object)
{
    profile out;
    if (!object.is_object()) return out;
    out.object_type = string(object, "type");
    if (out.object_type == "Feature") {
	if (!feature(object)) return out;
	out.identified = true;
	if (item(object, out)) {
	    out.stac_kind = "item";
	    const auto templates = object.find("asset_templates");
	    if (templates != object.end() && !templates->is_object()) out.consistent = false;
	    out.asset_templates = templates != object.end() && templates->is_object() && !templates->empty();
	}
    } else if (out.object_type == "FeatureCollection") {
	const auto features = object.find("features");
	if (features == object.end() || !features->is_array()) return out;
	out.identified = true;
	if (features->empty()) return out;
	for (const auto &feature : *features) if (!item(feature, out)) return out;
	out.stac_kind = "item-collection";
    }
    return out;
}
class declarations {
    mime_json::declarations<3> fields;
public:
    json::parser_callback_t callback()
    {
	return fields.callback([](int depth, const auto &parents, const std::string &key) {
	    if (depth != 1 && !(depth == 3 && parents[1] == "features")) return false;
	    return key == "type" || key == "stac_version" || key == "stac_extensions" ||
		key == "features" || key == "assets" || key == "asset_templates" || key == "geometry" || key == "properties";
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
}
#endif
