/* M I M E _ X M L . H
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
#ifndef BU_MIME_XML_H
#define BU_MIME_XML_H
#include "common.h"
#include <cctype>
#include <cstdint>
#include <istream>
#include <map>
#include <string>
#include "mime_text.h"
#include "mime_unicode.h"
#include "mime_raster_label.h"
#include "mime_esric.h"

/* Read only the document element. Geometry and full XML validation remain
 * the decoder's job; the same prefix interpretation serves full readers. */
namespace mime_xml {
using mime_unicode::characters;
inline constexpr const char *kml_type = "application/vnd.google-earth.kml+xml";
inline constexpr const char *pds4_type = "application/vnd.nasa.pds4+xml";
inline constexpr const char *mrf_type = "application/vnd.nasa.mrf+xml";
inline constexpr const char *safe_type = "application/vnd.esa.safe+xml";
inline constexpr const char *sentinel2_type = "application/vnd.esa.sentinel-2+xml";

struct value {std::string text; bool known = true;};
inline bool attribute(characters &input, value &out)
{
    int quote = input.get();
    if (quote != '\'' && quote != '"') return false;
    while (input.peek() >= 0 && input.peek() != quote) {
	int c = input.get();
	if (c == '<' || !c) return false;
	if (c != '&') {input.append(out.text, c); continue;}
	std::string entity;
	while (input.peek() >= 0 && input.peek() != ';') input.append(entity, input.get());
	if (input.get() != ';') return false;
	if (entity == "amp") out.text += '&';
	else if (entity == "lt") out.text += '<';
	else if (entity == "gt") out.text += '>';
	else if (entity == "quot") out.text += '"';
	else if (entity == "apos") out.text += '\'';
	else if (!entity.empty() && entity[0] == '#') {
	    size_t at = 1;
	    unsigned int base = 10;
	    if (at < entity.size() && entity[at] == 'x') {++at; base = 16;}
	    uint32_t code = 0;
	    bool valid = at < entity.size();
	    for (; at < entity.size(); ++at) {
		char digit = entity[at];
		unsigned int n = digit >= '0' && digit <= '9' ? digit - '0' : digit >= 'a' && digit <= 'f' ? digit - 'a' + 10 : digit >= 'A' && digit <= 'F' ? digit - 'A' + 10 : base;
		if (n >= base || code > (0x10ffff - n) / base) {valid = false; break;}
		code = code * base + n;
	    }
	    if (!valid || !code || (code >= 0xd800 && code <= 0xdfff)) out.known = false;
	    else characters::append_scalar(out.text, code);
	} else out.known = false;
    }
    return input.get() == quote;
}

inline std::string collada_schema(const std::string &version)
{
    int comparison = 0;
    if (!bu_mime_version_compare(version.c_str(), version.c_str(), &comparison)) return {};
    size_t dot = version.find('.');
    return dot == std::string::npos ? std::string{} : version.substr(0, version.find('.', dot + 1));
}

struct profile {
    std::string qname, local, uri, version, encoding, code_unit_width, declared_profile, amf_unit, safe_profile;
    bool valid = false, plausible = false, namespace_known = true;
    std::string sentinel2_role() const
    {
	if (local.size() < 9 || local.compare(0, 6, "Level-") || local[8] != '_') return {};
	const std::string level = local.substr(6, 2), role = local.substr(9);
	if (level != "1B" && level != "1C" && level != "2A") return {};
	std::string schema, name;
	if (role == "User_Product") {schema = "User_Product_Level-" + level; name = "product";}
	else if (role == "Granule_ID" && level == "1B") {schema = "S2_PDI_Level-" + level + "_Granule_Metadata"; name = "granule";}
	else if (role == "Tile_ID" && level != "1B") {schema = "S2_PDI_Level-" + level + "_Tile_Metadata"; name = "tile";}
	else if (role == "DataStrip_ID") {schema = "S2_PDI_Level-" + level + "_Datastrip_Metadata"; name = "datastrip";}
	else return {};
	const size_t start = uri.compare(0, 11, "http://psd-") == 0 ? 11 : uri.compare(0, 12, "https://psd-") == 0 ? 12 : 0;
	if (!start) return {};
	const size_t end = uri.find(".sentinel2.eo.esa.int/PSD/", start);
	if (end == std::string::npos || end == start || uri.substr(start, end - start).find_first_not_of("0123456789") != std::string::npos) return {};
	return uri.substr(end) == ".sentinel2.eo.esa.int/PSD/" + schema + ".xsd" ? name : "";
    }
    const char *type() const
    {
	if (!valid) return NULL;
	if (qname == "amf") return "model/amf";
	if (qname == "VTKFile") return "application/vnd.kitware.vtk+xml";
	/* Standard X3D uses no namespace; X3DOM documents also use Web3D's
	 * named namespace. A DTD can leave namespace facts unresolved. */
	if (local == "X3D" && ((qname == "X3D" && !namespace_known) ||
	    (namespace_known && (uri.empty() || uri == "http://www.web3d.org/specifications/x3d-namespace")))) return "model/x3d+xml";
	if (!namespace_known) return NULL;
	if (local == "XFDU" && uri == "urn:ccsds:schema:xfdu:1" && !safe_profile.empty()) return safe_type;
	if (!sentinel2_role().empty()) return sentinel2_type;
	if (qname == "CacheInfo" && uri.empty()) return mime_esric::xml_type;
	if (qname == "MRF_META" && uri.empty()) return mrf_type;
	if (qname == "imageFile" && uri.empty()) return mime_raster_label::isce_label;
	if (qname == "phini" && uri.empty()) return mime_raster_label::photomod_label;
	if (local.compare(0, sizeof("Product_") - 1, "Product_") == 0 && local.size() > sizeof("Product_") - 1 &&
	    uri == "http://pds.nasa.gov/pds4/pds/v1") return pds4_type;
	if (local == "kml" && (uri == "http://www.opengis.net/kml/2.2" ||
	    uri == "http://earth.google.com/kml/2.1" || uri == "http://earth.google.com/kml/2.2")) return kml_type;
	if (local == "COLLADA" && (uri.empty() || uri == "http://www.collada.org/2005/11/COLLADASchema" || uri == "http://www.collada.org/2008/03/COLLADASchema")) return "model/vnd.collada+xml";
	if (local == "mesh" && uri == "http://irrlicht.sourceforge.net/IRRMESH_09_2007") return "model/vnd.irrlicht.mesh+xml";
	if (local == "irr_scene" && uri.empty()) return "model/vnd.irrlicht.scene+xml";
	return NULL;
    }
    bool append_traits(bu_mime_traits &traits, const char *encoding_key = "encoding") const
    {
	if (!valid) return false;
	if ((!encoding.empty() && !bu_mime_traits_add(&traits, encoding_key, encoding.c_str())) ||
	    !bu_mime_traits_add(&traits, "xml.root-qname", qname.c_str()) ||
	    !bu_mime_traits_add(&traits, "xml.root-localname", local.c_str())) return false;
	/* An embedded XML member does not describe the outer file's text width. */
	if (std::string(encoding_key) == "encoding" && !code_unit_width.empty() &&
	    !bu_mime_traits_add(&traits, "text.code-unit-width", code_unit_width.c_str())) return false;
	if (!version.empty() && !bu_mime_traits_add(&traits, "version", version.c_str())) return false;
	if (type() && std::string(type()) == safe_type && !bu_mime_traits_add(&traits, "safe.profile", safe_profile.c_str())) return false;
	const std::string role = sentinel2_role();
	if (!role.empty() && (!bu_mime_traits_add(&traits, "sentinel2.processing-level", local.substr(6, 2).c_str()) ||
	    !bu_mime_traits_add(&traits, "sentinel2.metadata-role", role.c_str()))) return false;
	if (qname == "amf") {
	    if (!bu_mime_traits_add(&traits, "container", "none")) return false;
	    if (!amf_unit.empty() && !bu_mime_traits_add(&traits, "amf.unit-declaration", amf_unit.c_str())) return false;
	}
	if (local == "X3D" && !declared_profile.empty() && !bu_mime_traits_add(&traits, "x3d.profile", declared_profile.c_str())) return false;
	if (namespace_known && !mime_detail::append_set(traits, "xml.root-namespaces", uri.empty() ? std::set<std::string>{} : std::set<std::string>{uri})) return false;
	if (local == "COLLADA" && !version.empty()) {
	    std::string family = collada_schema(version);
	    if (!family.empty() && !bu_mime_traits_add(&traits, "collada.schema-version", family.c_str())) return false;
	}
	return true;
    }
};

struct text_profile {
    std::string encoding;
    bool ascii = false;
    bool append_traits(profile root, bu_mime_traits &traits) const
    {
	root.encoding = encoding;
	return root.append_traits(traits) && (encoding.empty() ||
	    bu_mime_traits_add(&traits, "text.ascii-only", ascii ? "true" : "false"));
    }
};

/* The stream starts at the document's beginning. Root declarations identify
 * the intended charset; explicit assertions also need its complete bytes. */
inline text_profile inspect_text(std::istream &stream, const profile &root)
{
    if (!root.valid || !stream.good()) return {};
    mime_unicode::text_source source(stream);
    /* ISO-8859-1 maps every octet. It needs no UTF-8 interpretation, and
     * its declaration must not contradict a Unicode byte-order mark. */
    const bool unmarked_bytes = root.code_unit_width == "8" && !source.marked();
    const bool latin1 = root.encoding == "iso-8859-1" && unmarked_bytes;
    (void)source.finish_encoding();
    text_profile result;
    const bool ascii_declaration = unmarked_bytes && (root.encoding == "ascii" || root.encoding == "us-ascii");
    if (!stream.bad() && (latin1 || (source.valid() &&
	(root.encoding == source.encoding() || (ascii_declaration && source.ascii))))) result.encoding = root.encoding;
    result.ascii = source.ascii;
    return result;
}

inline bool is_type(const char *type)
{
    return type && (std::string(type) == "model/amf" || std::string(type) == "model/vnd.collada+xml" || std::string(type) == "model/vnd.irrlicht.mesh+xml" ||
	std::string(type) == "model/vnd.irrlicht.scene+xml" || std::string(type) == "model/x3d+xml" ||
	std::string(type) == "model/vnd.ogre.mesh+xml");
}

inline profile inspect(std::istream &stream)
{
    characters input(stream);
    profile result;
    result.code_unit_width = input.encoding.compare(0, 6, "utf-16") == 0 ? "16" :
	input.encoding.compare(0, 6, "utf-32") == 0 ? "32" : "8";
    for (;;) {
	input.space();
	if (input.get() != '<') return result;
	result.plausible = true;
	if (input.peek() == '?') {
	    input.get();
	    std::string target = input.name();
	    if (target != "xml") {if (!input.until("?>")) return result; continue;}
	    input.space();
	    while (input.peek() != '?') {
		std::string key = input.name();
		input.space();
		if (key.empty() || input.get() != '=') return result;
		input.space();
		value v;
		if (!attribute(input, v)) return result;
		if (key == "encoding" && v.known) {
		    for (char &c : v.text) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
		    if (v.text == "us-ascii") v.text = "ascii";
		    if (v.text == "latin1") v.text = "iso-8859-1";
		    if (v.text == "utf-16" || v.text == "utf-32") {
			if (input.encoding.compare(0, v.text.size(), v.text) != 0) return result;
		    } else if (v.text.compare(0, 4, "utf-") == 0) {
			if (v.text != input.encoding) return result;
		    } else result.encoding = v.text;
		}
		input.space();
	    }
	    input.get();
	    if (input.get() != '>') return result;
	    continue;
	}
	if (input.peek() != '!') break;
	input.get();
	if (input.peek() == '-') {
	    input.get();
	    if (input.get() != '-' || !input.until("-->")) return result;
	} else {
	    if (input.name() != "DOCTYPE") return result;
	    /* DTD attribute defaults may supply a namespace. Without an
	     * explicit root binding, its absence has not been established. */
	    result.namespace_known = false;
	    int depth = 0, quote = 0;
	    for (;;) {
		int c = input.get();
		if (c < 0) return result;
		if (quote) {if (c == quote) quote = 0; continue;}
		if (c == '\'' || c == '"') quote = c;
		else if (c == '[') ++depth;
		else if (c == ']') {if (!depth) return result; --depth;}
		else if (c == '<' && input.peek() == '!') {
		    input.get();
		    if (input.peek() == '-') {input.get(); if (input.get() != '-' || !input.until("-->")) return result;}
		} else if (c == '>' && !depth) break;
	    }
	}
    }
    result.qname = input.name();
    if (result.qname.empty()) return result;
    std::map<std::string, value> attributes;
    for (;;) {
	input.space();
	if (input.peek() == '>') {input.get(); break;}
	if (input.peek() == '/') {input.get(); if (input.get() != '>') return result; break;}
	std::string name = input.name();
	input.space();
	if (name.empty() || input.get() != '=') return result;
	input.space();
	value v;
	if (!attribute(input, v) || !attributes.emplace(name, v).second) return result;
    }
    size_t colon = result.qname.find(':');
    result.local = colon == std::string::npos ? result.qname : result.qname.substr(colon + 1);
    std::string binding = colon == std::string::npos ? "xmlns" : "xmlns:" + result.qname.substr(0, colon);
    auto ns = attributes.find(binding);
    if (ns != attributes.end()) {result.uri = ns->second.text; result.namespace_known = ns->second.known;}
    else if (colon != std::string::npos) result.namespace_known = false;
    auto version = attributes.find("version");
    int compared;
    if (version != attributes.end() && version->second.known && bu_mime_version_compare(version->second.text.c_str(), version->second.text.c_str(), &compared)) result.version = version->second.text;
    /* These labels do not define a wire revision through root/@version.
     * Their model identifiers and dataset history live in other fields. */
    if (version != attributes.end() && version->second.known &&
	version->second.text.compare(0, 9, "esa/safe/") == 0 && version->second.text.size() > 9) {
	bool printable = true;
	for (unsigned char c : version->second.text) printable = printable && c >= 0x21 && c <= 0x7e;
	if (printable) result.safe_profile = version->second.text;
    }
    result.valid = true;
    const char *type = result.type();
    if (type && (std::string(type) == pds4_type || std::string(type) == mrf_type || std::string(type) == safe_type || std::string(type) == sentinel2_type || std::string(type) == mime_esric::xml_type)) result.version.clear();
    auto declared_profile = attributes.find("profile");
    if (declared_profile != attributes.end() && declared_profile->second.known) result.declared_profile = declared_profile->second.text;
    auto unit = attributes.find("unit");
    if (result.qname == "amf" && unit != attributes.end() && unit->second.known) result.amf_unit = unit->second.text;
    if (result.encoding.empty()) result.encoding = input.encoding;
    return result;
}
}
#endif
