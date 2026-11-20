/* X G L _ M I M E . H
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
#ifndef ASSETIMPORT_XGL_MIME_H
#define ASSETIMPORT_XGL_MIME_H

#include "common.h"
#include <algorithm>
#include <fstream>
#include <iterator>
#include <sstream>
#include <string>
#include <pugixml.hpp>
#include <zlib.h>
#include "bu/log.h"
#include "bu/path.h"
#include "bu/str.h"
#include "mime_xml.h"
#include "../inflate_mime.h"

namespace assetimport_xgl {
inline constexpr const char *xml_type = "model/vnd.realitywave.xgl+xml";
inline constexpr const char *compressed_type = "model/vnd.realitywave.zgl";

inline bool is_type(const char *type)
{
    return type && (BU_STR_EQUAL(type, xml_type) || BU_STR_EQUAL(type, compressed_type));
}


struct observation {
    mime_xml::profile root;
    bool compressed = false, matched = false, incomplete = false, error = false;
    bool valid() const
    {
	return root.valid && root.qname == "WORLD" && (!root.namespace_known || root.uri.empty());
    }
    bool append(bu_mime_traits &traits) const
    {
	if (!valid()) return true;
	/* XGL defines no format revision here; XML 1.0 is syntax metadata. */
	auto facts = root;
	facts.version.clear();
	return facts.append_traits(traits, compressed ? "xml.encoding" : "encoding") &&
	    bu_mime_traits_add(&traits, "compression", compressed ? "zlib" : "none") &&
	    (!compressed || bu_mime_traits_add(&traits, "encoding", "binary"));
    }
};

inline bool compressed_filename(const std::string &filename)
{
    bu_vls extension = BU_VLS_INIT_ZERO;
    bu_path_component(&extension, filename.c_str(), BU_PATH_EXT);
    bool compressed = !bu_strcasecmp(bu_vls_cstr(&extension), "zgl");
    bu_vls_free(&extension);
    return compressed;
}

inline int file_traits(const std::string &filename, const char *type, const bu_mime_traits *requested, bu_mime_traits &traits)
{
    observation facts;
    facts.compressed = BU_STR_EQUAL(type, compressed_type);
    /* The SDK chooses decompression from the suffix before parsing XML. */
    if (facts.compressed != compressed_filename(filename)) {
	bu_log("SDK %s input requires %s\n", facts.compressed ? "ZGL" : "XGL",
	    facts.compressed ? "a .zgl filename suffix" : "a filename without the .zgl suffix");
	return 0;
    }
    std::ifstream file(filename, std::ios::binary);
    if (!file.is_open()) return 0;
    const bool charset = bu_mime_traits_get(requested, facts.compressed ? "xml.encoding" : "encoding") ||
	(!facts.compressed && bu_mime_traits_get(requested, "text.ascii-only"));
    mime_xml::text_profile text;
    if (facts.compressed) {
	gcv_compression::inflate_buffer buffer(file, gcv_compression::framing::zlib);
	std::istream decoded(&buffer);
	facts.root = mime_xml::inspect(decoded);
	if (buffer.failed()) return 0;
    } else facts.root = mime_xml::inspect(file);
    if (charset) {
	file.clear();
	file.seekg(0);
	if (!file) return 0;
	if (facts.compressed) {
	    gcv_compression::inflate_buffer buffer(file, gcv_compression::framing::zlib);
	    std::istream decoded(&buffer);
	    text = mime_xml::inspect_text(decoded, facts.root);
	    if (buffer.failed()) return 0;
	} else text = mime_xml::inspect_text(file, facts.root);
	if (text.encoding.empty()) return 0;
	facts.root.encoding = text.encoding;
    }
    return !file.bad() && facts.valid() && facts.append(traits) &&
	(!charset || facts.compressed || bu_mime_traits_add(&traits, "text.ascii-only", text.ascii ? "true" : "false"));
}

inline observation prefix(const bu_mime_probe_input &input, const char *type)
{
    observation facts;
    facts.compressed = BU_STR_EQUAL(type, compressed_type);
    size_t size = std::min(input.size, size_t(BU_MIME_DEFAULT_SCAN_LIMIT));
    facts.incomplete = !input.complete || size != input.size;
    if (facts.compressed && size) {
	/* RFC 1950's CMF/FLG check avoids initializing a decompressor for
	 * unrelated formats. It does not establish a ZGL identity. */
	constexpr unsigned int method_mask = 0x0f, window_shift = 4;
	constexpr unsigned int maximum_window_code = 7, header_modulus = 31;
	unsigned int header = input.data[0];
	bool valid = (header & method_mask) == Z_DEFLATED && (header >> window_shift) <= maximum_window_code;
	if (size > 1) valid = valid && ((header << 8) | input.data[1]) % header_modulus == 0;
	if (!valid) {facts.incomplete = false; return facts;}
    }
    std::string xml;
    if (size) xml.assign(reinterpret_cast<const char *>(input.data), size);
    if (facts.compressed) {
	std::istringstream source(xml);
	gcv_compression::inflate_buffer buffer(source, gcv_compression::framing::zlib, BU_MIME_DEFAULT_SCAN_LIMIT);
	std::istream decoded(&buffer);
	xml.assign(std::istreambuf_iterator<char>(decoded), std::istreambuf_iterator<char>());
	facts.incomplete = facts.incomplete || buffer.incomplete();
	facts.error = buffer.internal_error();
	if (buffer.failed()) {facts.incomplete = false; return facts;}
    }
    std::istringstream document_root(xml);
    facts.root = mime_xml::inspect(document_root);
    if (!facts.valid()) {
	if (facts.root.valid) facts.incomplete = false;
	return facts;
    }

    /* WORLD alone is generic. Require XGL scene structure, including on
     * partial XML trees when the bounded prefix ends inside the body. */
    pugi::xml_document document;
    auto parsed = document.load_buffer(xml.data(), xml.size());
    if (parsed.status == pugi::status_out_of_memory) {facts.error = true; return facts;}
    auto world = document.document_element();
    auto mesh = world.child("MESH");
    facts.matched = (world.child("BACKGROUND").child("BACKCOLOR") && world.child("LIGHTING")) ||
	(mesh.child("F").child("FV1") && mesh.child("P"));
    return facts;
}

inline bu_mime_probe_status probe(const bu_mime_probe_input *input, const char *type)
{
    if (!is_type(type)) return BU_MIME_PROBE_NO_MATCH;
    auto facts = prefix(*input, type);
    if (facts.error) return BU_MIME_PROBE_ERROR;
    if (facts.matched) return BU_MIME_PROBE_HEURISTIC;
    return facts.incomplete && (facts.root.plausible || facts.compressed) ? BU_MIME_PROBE_INCOMPLETE : BU_MIME_PROBE_NO_MATCH;
}

inline int inspect(const bu_mime_probe_input *input, const char *type, bu_mime_traits *traits)
{
    if (!is_type(type)) return 1;
    auto facts = prefix(*input, type);
    return !facts.error && facts.append(*traits);
}
}
#endif
