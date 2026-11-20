/* M I M E _ R A S T E R _ L A B E L . H
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
#ifndef BU_MIME_RASTER_LABEL_H
#define BU_MIME_RASTER_LABEL_H
#include "common.h"
#include <cmath>
#include <istream>
#include <locale>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include "bu/mime_runtime.h"
#include "mime_text.h"
#include "mime_unicode.h"

namespace mime_raster_label {
constexpr const char *rraster_label = "application/vnd.rspatial.raster";
constexpr const char *rraster_data = "image/vnd.rspatial.raster";
constexpr const char *roipac_label = "application/vnd.jpl.roi-pac";
constexpr const char *roipac_data = "image/vnd.jpl.roi-pac";
constexpr const char *isce_label = "application/vnd.nasa.isce+xml";
constexpr const char *isce_data = "image/vnd.nasa.isce";
constexpr const char *ehdr_label = "application/vnd.esri.ehdr";
constexpr const char *ehdr_data = "image/vnd.esri.ehdr";
constexpr const char *bil_data = "image/bil";
constexpr const char *photomod_label = "application/vnd.racurs.photomod.raster+xml";

enum class family {none, rraster, roipac, ehdr};

inline bool layout(const std::string &value)
{
    return value == "BIL" || value == "BIP" || value == "BSQ";
}

inline std::string integer(std::string value)
{
    if (!value.empty() && value[0] == '+') value.erase(0, 1);
    return mime_detail::decimal_field(value);
}

inline bool number_is(const std::string &word, double expected)
{
    std::istringstream input(word);
    input.imbue(std::locale::classic());
    double value;
    /* These are exact protocol identity transforms, not geometric
     * comparisons with a tolerance. Tiny nonzero offsets still differ. */
    return (input >> value) && (input >> std::ws).eof() && std::isfinite(value) && std::fpclassify(value - expected) == FP_ZERO;
}

struct profile {
    family kind = family::none;
    bool identified = false, incomplete = false, header_known = false;
    bool bom = false, repertoire_known = false, ascii = false;
    std::string width, encoding;
    std::map<std::string, std::string> fields;
    std::string value(const std::string &key, const char *implicit = "") const
    {
	auto found = fields.find(key);
	return found == fields.end() ? std::string(implicit) : found->second;
    }
    const char *type() const
    {
	return kind == family::rraster ? rraster_label : kind == family::roipac ? roipac_label : kind == family::ehdr ? ehdr_label : NULL;
    }
    std::string interleave() const
    {
	return mime_detail::upper(value(kind == family::rraster ? "DATA.BANDORDER" : "LAYOUT", kind == family::ehdr ? "BIL" : ""));
    }
    bool append_traits(bu_mime_traits &traits, bool payload = false) const
    {
	if (!identified || !bu_mime_traits_add(&traits, "raster.entrypoint", payload ? "payload" : "label") ||
	    !bu_mime_traits_add(&traits, payload ? "raster.label-code-unit-width" : "text.code-unit-width", width.c_str()) ||
	    !bu_mime_traits_add(&traits, "raster.label-bom", bom ? "true" : "false")) return false;
	if (payload && !bu_mime_traits_add(&traits, "encoding", "binary")) return false;
	if (!encoding.empty() && !bu_mime_traits_add(&traits, payload ? "raster.label-encoding" : "encoding", encoding.c_str())) return false;
	if (repertoire_known && !bu_mime_traits_add(&traits, payload ? "raster.label-ascii-only" : "text.ascii-only", ascii ? "true" : "false")) return false;
	if (!header_known) return true;
	const auto property = [&](const char *key, const char *name, const char *implicit = "", bool upper = false) {
	    std::string text = value(key, implicit);
	    if (upper) text = mime_detail::upper(text);
	    return text.empty() || bu_mime_traits_add(&traits, name, text.c_str());
	};
	const auto count = [&](const char *key, const char *name, const char *implicit = "") {
	    const std::string text = integer(value(key, implicit));
	    return text.empty() || bu_mime_traits_add(&traits, name, text.c_str());
	};
	if (kind == family::rraster)
	    return property("DATA.DATATYPE", "rraster.datatype", "", true) &&
		property("DATA.BANDORDER", "rraster.interleave", "", true) &&
		property("DATA.BYTEORDER", "rraster.byte-order", "native", true) &&
		count("DATA.NBANDS", "rraster.band-count", "1");
	if (kind == family::ehdr)
	    return bu_mime_traits_add(&traits, "ehdr.interleave", interleave().c_str()) &&
		count("NBITS", "ehdr.nbits") && count("NBANDS", "ehdr.band-count", "1") &&
		property("PIXELTYPE", "ehdr.pixeltype", "", true) && property("BYTEORDER", "ehdr.byte-order", "native", true);
	if (kind == family::roipac) {
	    const bool unit = number_is(value("Z_SCALE", "1"), 1.0) && number_is(value("Z_OFFSET", "0"), 0.0);
	    return property("PROJECTION", "roi-pac.projection") &&
		bu_mime_traits_add(&traits, "roi-pac.unit-transform", unit ? "true" : "false");
	}
	return false;
    }
};

inline profile inspect(std::istream &stream, bool complete)
{
    profile out;
    mime_unicode::text_source source(stream);
    out.width = source.width();
    out.bom = source.marked();
    if (out.bom) out.encoding = source.encoding();
    std::string section;
    bool ini = false, duplicate = false;
    std::set<std::string> seen;
    static const std::set<std::string> profile_keys = {"DATATYPE", "BANDORDER", "BYTEORDER", "NBANDS", "NROWS", "NCOLS",
	"LAYOUT", "NBITS", "PIXELTYPE", "WIDTH", "FILE_LENGTH", "PROJECTION", "Z_SCALE", "Z_OFFSET"};
    for (std::string line;;) {
	line.clear();
	int c;
	while ((c = source.get()) >= 0 && c != '\r' && c != '\n') {
	    if (!c) return out;
	    line.push_back(c < 0x80 ? static_cast<char>(c) : '?');
	}
	if (c < 0 && line.empty()) break;
	if (c < 0 && !complete) break;
	line = mime_detail::trim(line);
	if (line.empty() || line[0] == '#' || line[0] == ';') continue;
	if (line.front() == '[' && line.back() == ']') {
	    ini = true;
	    section = mime_detail::upper(mime_detail::trim(line.substr(1, line.size() - 2)));
	    continue;
	}
	std::string key, value;
	const size_t equal = line.find('=');
	if (ini && equal != std::string::npos) {
	    key = mime_detail::upper(mime_detail::trim(line.substr(0, equal)));
	    value = mime_detail::trim(line.substr(equal + 1));
	} else {
	    std::istringstream tokens(line);
	    tokens >> key >> value;
	    key = mime_detail::upper(key);
	}
	if (key.empty() || value.empty()) continue;
	if (!profile_keys.count(key)) continue;
	if (!seen.insert(key).second) duplicate = true;
	out.fields.emplace(ini ? section + "." + key : key, value);
    }
    const auto positive = [&](const char *key) {
	const std::string value = integer(out.value(key));
	return !value.empty() && value != "0";
    };
    if (ini && positive("GEOREFERENCE.NROWS") && positive("GEOREFERENCE.NCOLS") && !out.value("DATA.DATATYPE").empty())
	out.kind = family::rraster;
    else if (!ini && positive("NROWS") && positive("NCOLS") &&
	(!out.value("LAYOUT").empty() || !out.value("NBITS").empty() || !out.value("BYTEORDER").empty() || !out.value("PIXELTYPE").empty()))
	out.kind = family::ehdr;
    else if (!ini && positive("WIDTH") && positive("FILE_LENGTH")) out.kind = family::roipac;
    out.identified = out.kind != family::none;
    out.header_known = out.identified && complete && !duplicate;
    out.incomplete = out.identified && !complete;
    if (complete) {
	out.encoding = source.finish_encoding();
	out.repertoire_known = !out.encoding.empty();
	out.ascii = source.ascii;
    }
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
