/* P H O T O M O D _ M I M E . H
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 * See the file named COPYING for more information.
 */
#ifndef GCV_GDAL_PHOTOMOD_MIME_H
#define GCV_GDAL_PHOTOMOD_MIME_H
#include "common.h"
#include <cstring>
#include <fstream>
#include <map>
#include <set>
#include <string>
#include "bu/path.h"
#include "raster_mime.h"

namespace gcv_photomod {
struct field {const CPLXMLNode *node; std::string value;};
using fields = std::map<std::string, field>;

inline bool collect(const CPLXMLNode *parent, const std::set<std::string> &keys, fields &out)
{
    for (const CPLXMLNode *node = parent->psChild; node; node = node->psNext) {
	if (node->eType != CXT_Element) continue;
	std::string name, value;
	size_t names = 0, values = 0;
	bool canonical = true;
	for (const CPLXMLNode *attribute = node->psChild; attribute; attribute = attribute->psNext) {
	    if (attribute->eType != CXT_Attribute || !attribute->psChild || attribute->psChild->eType != CXT_Text) continue;
	    if (BU_STR_EQUIV(attribute->pszValue, "n")) {
		name = mime_detail::upper(attribute->psChild->pszValue);
		++names;
		canonical = canonical && BU_STR_EQUAL(attribute->pszValue, "n");
	    } else if (BU_STR_EQUIV(attribute->pszValue, "v")) {
		value = mime_detail::trim(attribute->psChild->pszValue);
		++values;
		canonical = canonical && BU_STR_EQUAL(attribute->pszValue, "v");
	    }
	}
	if (!keys.count(name)) continue;
	/* The native reader consumes named fields regardless of element tag.
	 * Foreign elements must not override owned profile declarations. */
	if (!canonical || names != 1 || values > 1 || strchr(node->pszValue, ':') ||
	    !gcv_gdal_raster::unqualified(node, node->pszValue) || !out.emplace(name, field{node, value}).second) return false;
    }
    return true;
}

inline std::string value(const fields &source, const char *name)
{
    const auto found = source.find(name);
    return found == source.end() ? std::string() : found->second.value;
}

struct profile {
    bool known = false, repertoire_known = false, ascii = false, unit_offset = true;
    std::string width, encoding, kind, channel_type, channel_bytes, channel_count, rows, columns, parts;
    bool append_traits(bu_mime_traits &traits) const
    {
	if (!known || !bu_mime_traits_add(&traits, "raster.entrypoint", "label") ||
	    !bu_mime_traits_add(&traits, "text.code-unit-width", width.c_str()) ||
	    !bu_mime_traits_add(&traits, "photomod.filename-kind", kind.c_str()) ||
	    !bu_mime_traits_add(&traits, "photomod.channel-type", channel_type.c_str()) ||
	    !bu_mime_traits_add(&traits, "photomod.channel-bytes", channel_bytes.c_str()) ||
	    !bu_mime_traits_add(&traits, "photomod.channel-count", channel_count.c_str()) ||
	    !bu_mime_traits_add(&traits, "photomod.rows", rows.c_str()) ||
	    !bu_mime_traits_add(&traits, "photomod.columns", columns.c_str()) ||
	    !bu_mime_traits_add(&traits, "photomod.parts-extension", parts.c_str()) ||
	    !bu_mime_traits_add(&traits, "photomod.unit-offset", unit_offset ? "true" : "false")) return false;
	return !repertoire_known || (bu_mime_traits_add(&traits, "encoding", encoding.c_str()) &&
	    bu_mime_traits_add(&traits, "text.ascii-only", ascii ? "true" : "false"));
    }
};

inline profile inspect(const char *filename)
{
    profile out;
    bu_vls extension = BU_VLS_INIT_ZERO;
    bu_path_component(&extension, filename, BU_PATH_EXT);
    const std::string suffix = mime_detail::upper(bu_vls_cstr(&extension));
    bu_vls_free(&extension);
    if (suffix != "PRF" && suffix != "X-DEM") return out;
    out.kind = suffix == "PRF" ? "prf" : "x-dem";
    std::ifstream input(filename, std::ios::binary);
    const auto root = mime_xml::inspect(input);
    if (!input.is_open() || input.bad() || !root.valid || !root.namespace_known || root.qname != "phini" || !root.uri.empty()) return out;
    out.width = root.code_unit_width;
    input.clear();
    input.seekg(0);
    if (!input) return out;
    const auto text = mime_xml::inspect_text(input, root);
    out.repertoire_known = !text.encoding.empty() && !input.bad();
    out.encoding = text.encoding;
    out.ascii = text.ascii;
    CPLXMLNode *document = CPLParseXMLFile(filename);
    if (!document) return out;
    bool valid = false;
    const CPLXMLNode *label = NULL;
    for (const CPLXMLNode *node = document; node; node = node->psNext)
	if (gcv_gdal_raster::unqualified(node, "phini")) {
	    if (label) {label = NULL; break;}
	    label = node;
	}
    fields header;
    if (label && collect(label, {"CHANNELSINFO", "PARTS_EXT", "DEMSHIFT", "WIDTH", "HEIGHT", "QCHANS"}, header) && header.count("CHANNELSINFO")) {
	fields channels;
	valid = collect(header.at("CHANNELSINFO").node, {"TYPE", "BYTES_PS"}, channels);
	out.channel_type = value(channels, "TYPE");
	out.channel_bytes = mime_raster_label::integer(value(channels, "BYTES_PS"));
	out.channel_count = mime_raster_label::integer(value(header, "QCHANS"));
	out.rows = mime_raster_label::integer(value(header, "HEIGHT"));
	out.columns = mime_raster_label::integer(value(header, "WIDTH"));
	out.parts = value(header, "PARTS_EXT");
	if (header.count("PARTS_EXT")) valid = valid && BU_STR_EQUAL(header.at("PARTS_EXT").node->pszValue, "s");
	if (out.parts.empty()) out.parts = out.kind == "prf" ? "tif" : "demtif";
	const std::string part_kind = mime_detail::upper(out.parts);
	valid = valid && (part_kind == "TIF" || part_kind == "JP2" || part_kind == "DEMTIF");
	if (header.count("DEMSHIFT")) {
	    fields shift;
	    valid = valid && collect(header.at("DEMSHIFT").node, {"X", "Y", "Z"}, shift) && shift.size() == 3;
	    out.unit_offset = mime_raster_label::number_is(value(shift, "Z"), 0.0);
	}
    }
    CPLDestroyXMLNode(document);
    out.known = valid && !out.channel_type.empty() && !out.channel_bytes.empty() &&
	!out.channel_count.empty() && out.channel_count != "0" && !out.rows.empty() && out.rows != "0" &&
	!out.columns.empty() && out.columns != "0";
    return out;
}
}
#endif

/*
 * Local Variables:
 * mode: C++
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8 cino=N-s
 */
