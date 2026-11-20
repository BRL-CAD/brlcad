/* R A S T E R _ M I M E . H
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
#ifndef GCV_GDAL_RASTER_MIME_H
#define GCV_GDAL_RASTER_MIME_H
#include "common.h"
#include <fstream>
#include <map>
#include <set>
#include <string>
#include "bu/str.h"
#include "../../../libbu/mime_raster_label.h"
#include "../../../libbu/mime_xml.h"
#include <gdal.h>
#include <cpl_conv.h>
#include <cpl_string.h>
#include <cpl_minixml.h>

namespace gcv_gdal_raster {
inline std::string companion(GDALDatasetH dataset, const char *source, const char *extension)
{
    char **files = GDALGetFileList(dataset);
    std::string found;
    bool multiple = false;
    for (size_t i = 0; files && files[i]; ++i) {
	const std::string file = files[i];
	const size_t dot = file.find_last_of('.');
	if (file == source || dot == std::string::npos || mime_detail::upper(file.substr(dot + 1)) != extension) continue;
	const std::string upper = mime_detail::upper(file);
	constexpr char pam_suffix[] = ".AUX.XML";
	if (upper.size() >= sizeof(pam_suffix) - 1 &&
	    upper.compare(upper.size() - (sizeof(pam_suffix) - 1), sizeof(pam_suffix) - 1, pam_suffix) == 0) continue;
	if (!found.empty() && found != file) multiple = true;
	found = file;
    }
    CSLDestroy(files);
    return multiple ? std::string() : found;
}

inline mime_raster_label::profile label(GDALDatasetH dataset, const char *source)
{
    GDALDriverH driver = GDALGetDatasetDriver(dataset);
    if (!driver) return {};
    const char *name = GDALGetDriverShortName(driver);
    const std::string path = BU_STR_EQUAL(name, "RRASTER") ? std::string(source) :
	BU_STR_EQUAL(name, "EHdr") ? companion(dataset, source, "HDR") :
	BU_STR_EQUAL(name, "ROI_PAC") ? companion(dataset, source, "RSC") : std::string();
    if (path.empty()) return {};
    std::ifstream input(path, std::ios::binary);
    auto result = mime_raster_label::inspect(input, true);
    if (!input.is_open() || input.bad()) return {};
    return result;
}

/* ISCE uses unqualified XML names. A default namespace on a property or
 * value cannot silently turn extension metadata into an owned declaration. */
inline bool unqualified(const CPLXMLNode *node, const char *name)
{
    if (!node || node->eType != CXT_Element || !BU_STR_EQUAL(node->pszValue, name)) return false;
    for (const CPLXMLNode *attribute = node->psChild; attribute; attribute = attribute->psNext)
	if (attribute->eType == CXT_Attribute && BU_STR_EQUAL(attribute->pszValue, "xmlns") &&
	    (!attribute->psChild || attribute->psChild->eType != CXT_Text || attribute->psChild->pszValue[0])) return false;
    return true;
}

struct isce_profile {
    bool known = false;
    std::string width;
    std::map<std::string, std::string> fields;
    std::string value(const char *name) const
    {
	auto found = fields.find(name);
	return found == fields.end() ? std::string() : found->second;
    }
    bool append_traits(bu_mime_traits &traits) const
    {
	if (!known || !bu_mime_traits_add(&traits, "encoding", "binary") ||
	    !bu_mime_traits_add(&traits, "raster.entrypoint", "payload") ||
	    !bu_mime_traits_add(&traits, "raster.label-code-unit-width", width.c_str())) return false;
	const std::string datatype = mime_detail::upper(value("DATA_TYPE"));
	const std::string scheme = mime_detail::upper(value("SCHEME"));
	const std::string order = mime_detail::upper(value("BYTE_ORDER"));
	return bu_mime_traits_add(&traits, "isce.datatype", datatype.c_str()) &&
	    bu_mime_traits_add(&traits, "isce.scheme", scheme.c_str()) &&
	    bu_mime_traits_add(&traits, "isce.byte-order", order.empty() ? "NATIVE" : order.c_str());
    }
};

inline isce_profile isce(GDALDatasetH dataset, const char *source)
{
    isce_profile out;
    const std::string path = companion(dataset, source, "XML");
    if (path.empty()) return out;
    std::ifstream input(path, std::ios::binary);
    const auto root = mime_xml::inspect(input);
    if (!input.is_open() || input.bad() || !root.valid || !root.namespace_known ||
	root.qname != "imageFile" || !root.uri.empty()) return out;
    out.width = root.code_unit_width;
    CPLXMLNode *document = CPLParseXMLFile(path.c_str());
    if (!document) return out;
    const std::set<std::string> keys = {"WIDTH", "LENGTH", "NUMBER_BANDS", "DATA_TYPE", "SCHEME", "BYTE_ORDER"};
    bool valid = true, found = false;
    for (const CPLXMLNode *element = document; valid && element; element = element->psNext) {
	if (!unqualified(element, "imageFile")) continue;
	if (found) {valid = false; break;}
	found = true;
	for (const CPLXMLNode *property = element->psChild; valid && property; property = property->psNext) {
	    if (property->eType != CXT_Element || !BU_STR_EQUIV(property->pszValue, "property")) continue;
	    const char *native_name = CPLGetXMLValue(property, "name", NULL);
	    if (!native_name || !keys.count(mime_detail::upper(native_name))) continue;
	    const std::string name = mime_detail::upper(native_name);
	    size_t names = 0;
	    bool canonical = true;
	    for (const CPLXMLNode *attribute = property->psChild; attribute; attribute = attribute->psNext)
		if (attribute->eType == CXT_Attribute) {
		    if (BU_STR_EQUIV(attribute->pszValue, "name")) {
			canonical = canonical && BU_STR_EQUAL(attribute->pszValue, "name") &&
			    attribute->psChild && attribute->psChild->eType == CXT_Text;
			++names;
		    }
		    /* The native accessor also accepts value attributes. They must
		     * not override the schema's value element unnoticed. */
		    if (BU_STR_EQUIV(attribute->pszValue, "value")) canonical = false;
		}
	    if (names != 1 || !canonical || !unqualified(property, "property")) {valid = false; break;}
	    std::string text;
	    size_t values = 0;
	    for (const CPLXMLNode *value = property->psChild; valid && value; value = value->psNext) {
		if (value->eType != CXT_Element || !BU_STR_EQUIV(value->pszValue, "value")) continue;
		if (!unqualified(value, "value")) {valid = false; break;}
		++values;
		for (const CPLXMLNode *part = value->psChild; part; part = part->psNext) {
		    if (part->eType == CXT_Element) {valid = false; break;}
		    if (part->eType == CXT_Text) text += part->pszValue;
		}
	    }
	    text = mime_detail::trim(text);
	    if (values != 1 || text.empty() || !out.fields.emplace(name, text).second) valid = false;
	}
    }
    CPLDestroyXMLNode(document);
    out.known = valid && found && !out.value("WIDTH").empty() && !out.value("LENGTH").empty() &&
	!out.value("NUMBER_BANDS").empty() && !out.value("DATA_TYPE").empty() && !out.value("SCHEME").empty();
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
