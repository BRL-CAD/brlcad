/* E S R I C _ M I M E . H
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
#ifndef GCV_GDAL_ESRIC_MIME_H
#define GCV_GDAL_ESRIC_MIME_H
#include "common.h"
#include <fstream>
#include <locale>
#include <sstream>
#include <string>
#include "bu/str.h"
#include "raster_mime.h"
#include "../../../libbu/mime_esric.h"
#include "../../../libbu/mime_zip.h"
#include <cpl_conv.h>
#include <cpl_minixml.h>
#include <cpl_vsi.h>

namespace gcv_esric {
inline bool supported(const mime_esric::profile &profile, bool xml = false)
{
    constexpr unsigned int packet_size = 128, default_tile_size = 256, maximum_tile_size = 8192;
    if (!profile.identified || !profile.consistent || !BU_STR_EQUIV(profile.storage.c_str(), mime_esric::compact_v2)) return false;
    if ((!xml || !profile.packet.empty()) && profile.packet != std::to_string(packet_size)) return false;
    const std::string effective_rows = xml && profile.rows.empty() ? std::to_string(default_tile_size) : profile.rows;
    const std::string effective_cols = xml && profile.columns.empty() ? std::to_string(default_tile_size) : profile.columns;
    if (effective_rows.empty() || effective_rows != effective_cols || effective_rows.find_first_not_of("0123456789") != std::string::npos) return false;
    unsigned int size = 0;
    std::istringstream dimension(effective_rows);
    dimension.imbue(std::locale::classic());
    if (!(dimension >> size) || !size || size > maximum_tile_size) return false;
    int comparison = 0;
    if (profile.version_declared && (!bu_mime_version_compare(profile.package_version.c_str(), "1.0", &comparison) || comparison)) return false;
    const std::string format = mime_detail::upper(profile.tile_format.empty() && xml ? "JPEG" : profile.tile_format);
    return format == "JPEG" || format == "PNG" || format == "PNG8" || format == "PNG24" || format == "PNG32" || format == "MIXED";
}
inline std::string member_path(const char *filename)
{
    return std::string("/vsizip/{") + filename + "}/root.json";
}
inline const CPLXMLNode *child(const CPLXMLNode *parent, const char *name, bool &valid)
{
    const CPLXMLNode *found = NULL;
    for (const CPLXMLNode *node = parent ? parent->psChild : NULL; node; node = node->psNext) {
	if ((node->eType != CXT_Element && node->eType != CXT_Attribute) || !BU_STR_EQUIV(node->pszValue, name)) continue;
	if (found || !gcv_gdal_raster::unqualified(node, name)) valid = false;
	found = node;
    }
    return found;
}
inline std::string text(const CPLXMLNode *parent, const char *name, bool &valid)
{
    const auto *node = child(parent, name, valid);
    std::string value;
    for (const auto *part = node ? node->psChild : NULL; part; part = part->psNext) {
	if (part->eType == CXT_Text) value += part->pszValue;
	else if (part->eType == CXT_Element) valid = false;
    }
    return mime_detail::trim(value);
}
inline mime_esric::profile xml(const char *filename)
{
    mime_esric::profile out;
    CPLXMLNode *document = CPLParseXMLFile(filename);
    if (!document) return out;
    const CPLXMLNode *root = NULL;
    bool valid = true;
    for (const auto *node = document; node; node = node->psNext) {
	if (node->eType != CXT_Element || node->pszValue[0] == '?') continue;
	if (root || !gcv_gdal_raster::unqualified(node, "CacheInfo")) valid = false;
	root = node;
    }
    const auto *storage = child(root, "CacheStorageInfo", valid);
    const auto *tiles = child(root, "TileCacheInfo", valid);
    const auto *image = child(root, "TileImageInfo", valid);
    out.storage = text(storage, "StorageFormat", valid);
    const auto number = [&](const CPLXMLNode *parent, const char *name) {
	const std::string value = text(parent, name, valid);
	const std::string parsed = mime_raster_label::integer(value);
	/* An omitted declaration permits a native default, but a malformed
	 * declaration must not become an omitted observation. */
	if (child(parent, name, valid) && parsed.empty()) valid = false;
	return parsed;
    };
    out.packet = number(storage, "PacketSize");
    out.rows = number(tiles, "TileRows");
    out.columns = number(tiles, "TileCols");
    out.tile_format = text(image, "CacheTileFormat", valid);
    if (child(image, "CacheTileFormat", valid) && out.tile_format.empty()) valid = false;
    out.identified = root && storage && tiles && !out.storage.empty();
    out.consistent = valid;
    CPLDestroyXMLNode(document);
    return out;
}
inline mime_esric::profile archive(const char *filename)
{
    GByte *data = NULL;
    vsi_l_offset size = 0;
    if (!VSIIngestFile(NULL, member_path(filename).c_str(), &data, &size, -1)) return {};
    /* Inspect the original member bytes. Re-serializing the SDK's parsed
     * JSON object would discard repeated declarations. */
    const auto out = mime_esric::inspect(data, static_cast<size_t>(size), true);
    CPLFree(data);
    return out;
}
inline std::string identity(const char *filename)
{
    if (mime_zip::end_record(filename)) return mime_esric::zip_type;
    std::ifstream input(filename, std::ios::binary);
    const auto root = mime_xml::inspect(input);
    if (root.type() && BU_STR_EQUAL(root.type(), mime_esric::xml_type)) return mime_esric::xml_type;
    input.clear();
    input.seekg(0);
    return input && mime_esric::inspect(input).identified ? mime_esric::json_type : "";
}
inline bool inspect(const char *filename, const char *type, bu_mime_traits &observed, const bu_mime_traits *requested)
{
    if (identity(filename) != type) return false;
    if (BU_STR_EQUAL(type, mime_esric::zip_type)) {
	const auto profile = archive(filename);
	return supported(profile) && profile.append_traits(observed, true);
    }
    std::ifstream input(filename, std::ios::binary);
    if (!input) return false;
    if (BU_STR_EQUAL(type, mime_esric::json_type)) {
	const auto profile = mime_esric::inspect(input);
	return !input.bad() && supported(profile) && profile.append_traits(observed);
    }
    auto root = mime_xml::inspect(input);
    input.clear();
    input.seekg(0);
    const auto text = mime_xml::inspect_text(input, root);
    const bool charset = bu_mime_traits_get(requested, "encoding") || bu_mime_traits_get(requested, "text.ascii-only");
    if (charset && text.encoding.empty()) return false;
    const auto profile = xml(filename);
    return !input.bad() && supported(profile, true) && text.append_traits(root, observed) &&
	profile.append_fields(observed) && bu_mime_traits_add(&observed, "container", "none");
}
}
#endif

/*
 * Local Variables:
 * mode: C++
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
