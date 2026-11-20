/* M I M E _ T E R R A I N . H
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
#ifndef BU_MIME_TERRAIN_H
#define BU_MIME_TERRAIN_H
#include "common.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <istream>
#include <locale>
#include <set>
#include <sstream>
#include <string>
#include <vector>
#include "bu/mime.h"
#include "bu/str.h"
#include "mime_binary.h"
#include "mime_text.h"
#include "mime_unicode.h"

namespace mime_terrain {
inline constexpr const char *pcidsk_type = "application/x-pcidsk";
inline constexpr const char *dted_type = "application/dted";
inline constexpr const char *gxf_type = "application/x-gxf";
inline constexpr const char *zmap_type = "application/x-zmap";
inline constexpr const char *hfa_type = "application/x-erdas-hfa";
inline constexpr const char *bt_type = "application/x-bt";
inline constexpr const char *grass_type = "application/grass-ascii-grid";
inline constexpr const char *xyz_type = "application/x-ascii-xyz";
inline constexpr const char *surfer_type = "image/vnd.goldensoftware.surfer-grid";
inline constexpr const char *ascii_grid_type = "application/x-ascii-grid";
constexpr size_t dted_record_size = 80;
constexpr size_t dted_designator_offset = 59, dted_legacy_designator_offset = 118;
constexpr size_t dted_designator_size = 5;
constexpr size_t bt_header_size = 256;
constexpr size_t bt_version_offset = 7, bt_version_size = 3;
constexpr size_t bt_sample_size_offset = 18, bt_float_offset = 20;

struct profile {
    const char *type = NULL;
    const char *serialization_family = NULL;
    bool incomplete = false, text = false, bom = false, regular_grid = false;
    bool repertoire_known = false, ascii_only = false, gxf_inline_values = false;
    std::string encoding, order, version, width, sample_size, float_flag, bit_depth, sample_format;
    std::string layout, header_version;
    std::string dted_designator, dted_level, gxf_gtype, gxf_value_encoding;
    std::string zmap_values_per_line, zmap_field_width;
    void observe_text(mime_unicode::text_source &source, bool whole, const char *family = "esri-grid")
    {
	text = true;
	serialization_family = family;
	width = source.width();
	bom = source.marked();
	if (bom) encoding = source.encoding();
	if (whole) {
	    encoding = source.finish_encoding();
	    repertoire_known = !encoding.empty();
	    ascii_only = source.ascii;
	}
    }
    bool append_traits(bu_mime_traits &traits) const
    {
	return type && (encoding.empty() || bu_mime_traits_add(&traits, "encoding", encoding.c_str())) &&
	    (order.empty() || bu_mime_traits_add(&traits, "byte-order", order.c_str())) &&
	    (version.empty() || bu_mime_traits_add(&traits, "version", version.c_str())) &&
	    (sample_size.empty() || bu_mime_traits_add(&traits, "bt.sample-size", sample_size.c_str())) &&
	    (float_flag.empty() || bu_mime_traits_add(&traits, "bt.float-flag", float_flag.c_str())) &&
	    (bit_depth.empty() || bu_mime_traits_add(&traits, "bit-depth", bit_depth.c_str())) &&
	    (sample_format.empty() || bu_mime_traits_add(&traits, "sample-format", sample_format.c_str())) &&
	    (dted_designator.empty() || bu_mime_traits_add(&traits, "dted.designator", dted_designator.c_str())) &&
	    (dted_level.empty() || bu_mime_traits_add(&traits, "dted.level", dted_level.c_str())) &&
	    (!BU_STR_EQUAL(type, gxf_type) || bu_mime_traits_add(&traits, "gxf.inline-header-values", gxf_inline_values ? "true" : "false")) &&
	    (gxf_gtype.empty() || bu_mime_traits_add(&traits, "gxf.gtype", gxf_gtype.c_str())) &&
	    (gxf_value_encoding.empty() || bu_mime_traits_add(&traits, "gxf.value-encoding", gxf_value_encoding.c_str())) &&
	    (zmap_values_per_line.empty() || bu_mime_traits_add(&traits, "zmap.values-per-line", zmap_values_per_line.c_str())) &&
	    (zmap_field_width.empty() || bu_mime_traits_add(&traits, "zmap.field-width", zmap_field_width.c_str())) &&
	    (layout.empty() || bu_mime_traits_add(&traits, "surfer.layout", layout.c_str())) &&
	    (header_version.empty() || bu_mime_traits_add(&traits, "surfer.header-version", header_version.c_str())) &&
	    (!regular_grid || bu_mime_traits_add(&traits, "xyz.topology", "regular-grid")) &&
	    (!serialization_family || bu_mime_traits_add(&traits, (std::string(serialization_family) + ".serialization").c_str(), text ? "text" : "binary")) &&
	    (!text || (bu_mime_traits_add(&traits, "text.code-unit-width", width.c_str()) &&
		bu_mime_traits_add(&traits, (std::string(serialization_family) + ".text-bom").c_str(), bom ? "true" : "false"))) &&
	    (!repertoire_known || bu_mime_traits_add(&traits, "text.ascii-only", ascii_only ? "true" : "false"));
    }
};

inline profile binary(const bu_mime_probe_input &input)
{
    profile result;
    const auto prefix = [&](const char *magic, bool insensitive = false) {
	const size_t size = strlen(magic), available = std::min(input.size, size);
	if (!available) return false;
	for (size_t i = 0; i < available; ++i) {
	    const unsigned char c = input.data[i];
	    const unsigned char upper = insensitive && c >= 'a' && c <= 'z' ? c - ('a' - 'A') : c;
	    if (upper != static_cast<unsigned char>(magic[i])) return false;
	}
	if (available < size) {result.incomplete = !input.complete; return false;}
	return true;
    };
    if (prefix("PCIDSK  ")) {
	result.type = pcidsk_type;
	result.encoding = "binary";
	return result;
    }
    if (prefix("EHFA_HEADER_TAG", true)) {
	/* The HFA tag also occurs in auxiliary HFA containers. It does not
	 * promise an available raster layer or any particular HFA revision. */
	result.type = hfa_type;
	result.encoding = "binary";
	return result;
    }
    if (prefix("DSBB", true) || prefix("DSRB", true)) {
	result.type = surfer_type;
	result.serialization_family = "surfer";
	result.layout = input.data[2] == 'B' || input.data[2] == 'b' ? "classic" : "tagged";
	result.encoding = "binary";
	result.order = "little-endian";
	result.sample_format = "ieee754";
	result.bit_depth = result.layout == "classic" ? "32" : "64";
	if (result.layout == "tagged") {
	    constexpr size_t version_offset = 2 * sizeof(uint32_t);
	    if (input.size >= version_offset + sizeof(uint32_t)) {
		const auto header_size = mime_binary::little_u32(input.data + sizeof(uint32_t));
		if (header_size >= sizeof(uint32_t)) result.header_version = std::to_string(mime_binary::little_u32(input.data + version_offset));
	    } else result.incomplete = !input.complete;
	}
	return result;
    }
    if (!prefix("binterr")) return result;
    result.type = bt_type;
    result.encoding = "binary";
    result.order = "little-endian";
    if (input.size < bt_version_offset + bt_version_size) {result.incomplete = !input.complete; return result;}
    const auto *version = input.data + bt_version_offset;
    if (version[0] >= '0' && version[0] <= '9' && version[1] == '.' && version[2] >= '0' && version[2] <= '9')
	result.version.assign(reinterpret_cast<const char *>(version), bt_version_size);
    if (input.size < bt_float_offset + sizeof(uint16_t)) {result.incomplete = !input.complete; return result;}
    const auto size = mime_binary::little_u16(input.data + bt_sample_size_offset);
    const auto floating = mime_binary::little_u16(input.data + bt_float_offset);
    result.sample_size = std::to_string(size);
    result.float_flag = std::to_string(floating);
    /* Preserve unfamiliar declarations without guessing a layout. */
    if ((size == sizeof(int16_t) || size == sizeof(int32_t)) && !floating) {
	result.bit_depth = std::to_string(size * 8);
	result.sample_format = "signed-integer";
    } else if (size == sizeof(uint32_t) && floating == 1) {
	result.bit_depth = "32";
	result.sample_format = "ieee754";
    }
    return result;
}

inline profile dted(std::istream &stream, bool whole)
{
    profile result;
    bool label_seen = false;
    for (;;) {
	char record[dted_record_size];
	stream.read(record, sizeof(record));
	const size_t size = static_cast<size_t>(stream.gcount());
	if (size < 3) {
	    if (!whole && (size >= 2 || label_seen)) {
		const auto prefix = mime_detail::upper(std::string(record, size));
		for (const char *tag : {"UHL", "VOL", "HDR"})
		    if (std::string(tag).compare(0, size, prefix) == 0) result.incomplete = true;
	    }
	    return result;
	}
	const auto tag = mime_detail::upper(std::string(record, 3));
	if (tag != "UHL" && tag != "VOL" && tag != "HDR") return result;
	if (size < sizeof(record)) {result.incomplete = !whole; return result;}
	if (tag != "UHL") {label_seen = true; continue;}
	/* Historical DTED headers use a shifted DSI designator field. */
	const size_t offset = record[4] == ' ' ? dted_legacy_designator_offset : dted_designator_offset;
	char description[dted_legacy_designator_offset + dted_designator_size];
	const size_t required = offset + dted_designator_size;
	stream.read(description, required);
	const size_t available = static_cast<size_t>(stream.gcount());
	if (available < 3 || mime_detail::upper(std::string(description, 3)) != "DSI") {
	    result.incomplete = available < 3 && !whole &&
		std::string("DSI").compare(0, available, mime_detail::upper(std::string(description, available))) == 0;
	    return result;
	}
	result.type = dted_type;
	result.encoding = "binary";
	result.order = "big-endian";
	result.bit_depth = "16";
	result.sample_format = "signed-integer";
	if (available < required) {result.incomplete = !whole; return result;}
	const auto designator = mime_detail::trim(std::string(description + offset, dted_designator_size));
	if (designator.find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789 -") == std::string::npos)
	    result.dted_designator = designator;
	if (designator.size() == dted_designator_size && mime_detail::upper(designator.substr(0, 4)) == "DTED" &&
	    designator.back() >= '0' && designator.back() <= '9') result.dted_level = designator.back();
	return result;
    }
}

/* Only numeric and keyword fields are interpreted below. Non-ASCII names
 * and comments remain available to the independent charset observer. */
inline bool header_line(mime_unicode::text_source &source, std::string &line)
{
    line.clear();
    for (int c = source.get(); c >= 0; c = source.get()) {
	if (c == '\r' || c == '\n') {
	    if (c == '\r' && source.peek() == '\n') source.get();
	    return true;
	}
	line += c < 0x80 ? static_cast<char>(c) : '?';
    }
    return false;
}

inline profile gxf(std::istream &stream, bool whole, bool charset = true)
{
    profile result;
    mime_unicode::text_source source(stream);
    bool points = false, rows = false, explicit_gtype = false;
    std::string key, line;
    for (;;) {
	const bool ended = header_line(source, line);
	if (line.find('\0') != std::string::npos) return {};
	line = mime_detail::trim(line);
	if (!ended && !whole) {
	    result.incomplete = points || rows || line.compare(0, 2, "#P") == 0 || line.compare(0, 2, "#R") == 0;
	    return result;
	}
	if (!line.empty() && line[0] == '#') {
	    const auto split = line.find_first_of(" \t");
	    key = mime_detail::upper(line.substr(0, split));
	    line = split == std::string::npos ? "" : mime_detail::trim(line.substr(split));
	    if (key.compare(0, 6, "#GTYPE") == 0) {explicit_gtype = true; result.gxf_gtype.clear();}
	    if (key == "#GRID") {
		if (!points || !rows) return {};
		result.type = gxf_type;
		if (!explicit_gtype) result.gxf_gtype = "0";
		const auto gtype = mime_detail::decimal_field(result.gxf_gtype);
		if (gtype == "0") result.gxf_value_encoding = "decimal";
		else if (!gtype.empty() && (gtype.size() == 1 || (gtype.size() == 2 && gtype <= "20")))
		    result.gxf_value_encoding = "base90";
		result.observe_text(source, whole && charset, "gxf");
		return result;
	    }
	    if (!line.empty()) result.gxf_inline_values = true;
	}
	if (!line.empty()) {
	    const auto integer = mime_detail::decimal_field(line);
	    if (key.compare(0, 5, "#POIN") == 0) points = !integer.empty() && integer != "0";
	    else if (key.compare(0, 5, "#ROWS") == 0) rows = !integer.empty() && integer != "0";
	    else if (key.compare(0, 6, "#GTYPE") == 0) {
		result.gxf_gtype = integer.empty() ? line : integer;
	    }
	    key.clear();
	}
	if (!ended) return {};
    }
}

inline profile zmap(std::istream &stream, bool whole, bool charset = true)
{
    profile result;
    mime_unicode::text_source source(stream);
    std::string line;
    bool ended;
    do {
	ended = header_line(source, line);
	if (line.find('\0') != std::string::npos) return {};
	if (!ended && !whole) {result.incomplete = !line.empty() && (line[0] == '@' || line[0] == '!'); return result;}
    } while (ended && !line.empty() && line[0] == '!');
    if (line.empty() || line[0] != '@') return result;
    const auto fields = [](const std::string &record) {
	std::vector<std::string> values;
	std::istringstream input(record);
	std::string value;
	while (std::getline(input, value, ',')) values.push_back(mime_detail::trim(value));
	return values;
    };
    const auto first = fields(line);
    if (first.size() != 3 || mime_detail::upper(first[1]) != "GRID") return result;
    result.zmap_values_per_line = mime_detail::decimal_field(first[2]);
    if (result.zmap_values_per_line.empty() || result.zmap_values_per_line == "0") return {};
    result.type = zmap_type;
    ended = header_line(source, line);
    if (line.find('\0') != std::string::npos) return {};
    if (ended || whole) {
	const auto second = fields(line);
	if (second.size() == 5) result.zmap_field_width = mime_detail::decimal_field(second[0]);
    } else result.incomplete = true;
    result.observe_text(source, whole && charset, "zmap");
    return result;
}

inline profile surfer_text(std::istream &stream, bool whole, bool charset)
{
    profile result;
    mime_unicode::text_source source(stream);
    constexpr char magic[] = "DSAA";
    for (size_t i = 0; i < sizeof(magic) - 1; ++i) {
	const int c = source.get();
	if (c < 0) {result.incomplete = !whole; return result;}
	if ((c >= 'a' && c <= 'z' ? c - ('a' - 'A') : c) != magic[i]) return {};
    }
    result.type = surfer_type;
    result.layout = "classic";
    result.observe_text(source, whole && charset, "surfer");
    return result;
}

inline profile grass(std::istream &stream, bool whole)
{
    profile result;
    mime_unicode::text_source source(stream);
    std::set<std::string> fields;
    for (;;) {
	std::string line;
	bool ended = false;
	for (int c = source.get(); c >= 0; c = source.get()) {
	    if (c == '\r' || c == '\n') {
		if (c == '\r' && source.peek() == '\n') source.get();
		ended = true;
		break;
	    }
	    if (!c || c >= 0x80) return {};
	    line += static_cast<char>(c);
	}
	const auto colon = line.find(':');
	const auto key = mime_detail::upper(mime_detail::trim(line.substr(0, colon)));
	bool known = false;
	for (const char *name : {"NORTH", "SOUTH", "EAST", "WEST", "ROWS", "COLS", "TYPE", "NULL"})
	    if (key == name) {known = true; break;}
	if (!ended && !whole) {result.incomplete = known; return result;}
	if (colon == std::string::npos || !known || !fields.insert(key).second) return {};
	const auto value = mime_detail::trim(line.substr(colon + 1));
	if (key == "ROWS" || key == "COLS") {
	    const auto count = mime_detail::decimal_field(value);
	    if (count.empty() || count == "0") return {};
	} else if (key != "TYPE" && key != "NULL" && !mime_detail::finite_number(value)) return {};
	if (fields.count("NORTH") && fields.count("SOUTH") && fields.count("EAST") && fields.count("WEST") && fields.count("ROWS") && fields.count("COLS")) {
	    result.type = grass_type;
	    result.observe_text(source, whole, "grass-grid");
	    return result;
	}
	if (source.peek() < 0) {result.incomplete = !whole; return result;}
    }
}

inline bool grid_field(const std::string &key, bool prefix = false)
{
    for (const char *name : {"NCOLS", "NROWS", "XLLCORNER", "YLLCORNER", "XLLCENTER", "YLLCENTER", "CELLSIZE", "DX", "DY", "NODATA_VALUE"})
	if (key == name || (prefix && std::string(name).compare(0, key.size(), key) == 0)) return true;
    return false;
}

inline profile ascii_grid(std::istream &stream, bool whole)
{
    profile result;
    mime_unicode::text_source source(stream);
    std::set<std::string> fields;
    for (;;) {
	std::string line;
	bool ended = false;
	for (int c = source.get(); c >= 0; c = source.get()) {
	    if (c == '\r' || c == '\n') {ended = true; break;}
	    if (!c || c >= 0x80) return {};
	    line += static_cast<char>(c);
	}
	if (!ended && !whole) {
	    std::istringstream partial(line);
	    partial.imbue(std::locale::classic());
	    std::string word;
	    partial >> word;
	    word = mime_detail::upper(word);
	    result.incomplete = (word.size() > 1 || !fields.empty()) && grid_field(word, true);
	    return result;
	}
	std::istringstream record(line);
	record.imbue(std::locale::classic());
	std::string key, value, extra;
	if (!(record >> key)) {if (source.peek() >= 0) continue; return result;}
	if (!(record >> value) || record >> extra) return {};
	key = mime_detail::upper(key);
	if (!grid_field(key) || !fields.insert(key).second) return {};
	if (key != "NODATA_VALUE") {
	    std::istringstream number(value);
	    number.imbue(std::locale::classic());
	    double scalar;
	    if (!(number >> scalar) || !number.eof() || !std::isfinite(scalar)) return {};
	    if (key == "NCOLS" || key == "NROWS") {
		const auto count = mime_detail::decimal_field(value);
		if (count.empty() || count == "0") return {};
	    }
	    if ((key == "CELLSIZE" || key == "DX" || key == "DY") && scalar <= 0) return {};
	}
	const bool corner = fields.count("XLLCORNER") && fields.count("YLLCORNER");
	const bool center = fields.count("XLLCENTER") && fields.count("YLLCENTER");
	const bool cell = fields.count("CELLSIZE") || (fields.count("DX") && fields.count("DY"));
	if (fields.count("NCOLS") && fields.count("NROWS") && (corner || center) && cell) {
	    result.type = ascii_grid_type;
	    result.observe_text(source, whole);
	    return result;
	}
	if (source.peek() < 0) {result.incomplete = !whole; return result;}
    }
}
}
#endif
