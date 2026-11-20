/* M I M E _ U N R E A L . H
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
#ifndef BU_MIME_UNREAL_H
#define BU_MIME_UNREAL_H
#include "common.h"
#include <cstdint>
#include <istream>
#include <initializer_list>
#include <set>
#include <sstream>
#include <string>
#include <utility>
#include "bu/str.h"
#include "mime_text.h"
#include "mime_unicode.h"

namespace mime_unreal {
inline constexpr const char *mesh_type = "model/vnd.epicgames.unreal.mesh";
inline constexpr const char *script_type = "text/vnd.epicgames.unrealscript";
inline constexpr size_t data_header_size = 48, triangle_size = 16;
inline constexpr size_t animation_header_size = 2 * sizeof(uint16_t), packed_vertex_size = sizeof(uint32_t);

inline bool is_type(const char *type)
{
    return type && (BU_STR_EQUAL(type, mesh_type) || BU_STR_EQUAL(type, script_type));
}

struct files {
    std::string base, part;
    explicit files(const std::string &filename)
    {
	/* Match the files the SDK actually opens, including its case-sensitive
	 * companion names, rather than accepting an arbitrary _x shortcut. */
	for (const auto &entry : {std::pair<const char *, const char *>{"_d.3d", "data"}, {"_a.3d", "animation"}, {".uc", "script"}}) {
	    std::string suffix = entry.first;
	    if (filename.size() >= suffix.size() && filename.compare(filename.size() - suffix.size(), suffix.size(), suffix) == 0) {
		base = filename.substr(0, filename.size() - suffix.size());
		part = entry.second;
		break;
	    }
	}
    }
    bool valid() const {return !part.empty();}
};

struct binary {
    std::string part;
    uint32_t triangles = 0, vertices = 0, frames = 0, frame_size = 0;
    bool valid = false;
    bool append_counts(bu_mime_traits &traits) const
    {
	if (part == "data")
	    return bu_mime_traits_add(&traits, "unreal.triangles", std::to_string(triangles).c_str()) &&
		bu_mime_traits_add(&traits, "unreal.vertices", std::to_string(vertices).c_str());
	return bu_mime_traits_add(&traits, "unreal.frames", std::to_string(frames).c_str()) &&
	    bu_mime_traits_add(&traits, "unreal.frame-size", std::to_string(frame_size).c_str());
    }
    bool append(bu_mime_traits &traits) const
    {
	if (!valid || !bu_mime_traits_add(&traits, "encoding", "binary") ||
	    !bu_mime_traits_add(&traits, "byte-order", "little-endian") || !bu_mime_traits_add(&traits, "unreal.part", part.c_str())) return false;
	return append_counts(traits);
    }
};

inline binary inspect_binary(const std::string &part, const bu_mime_probe_input &input)
{
    binary out;
    out.part = part;
    if (input.size < animation_header_size) return out;
    auto word = [&](size_t offset) {return uint32_t(input.data[offset]) | (uint32_t(input.data[offset + 1]) << 8);};
    uint64_t required = 0;
    if (part == "data") {
	if (input.size < data_header_size) return out;
	out.triangles = word(0);
	out.vertices = word(sizeof(uint16_t));
	if (!out.triangles || out.vertices < 3) return out;
	required = data_header_size + uint64_t(out.triangles) * triangle_size;
    } else if (part == "animation") {
	out.frames = word(0);
	out.frame_size = word(sizeof(uint16_t));
	if (!out.frames || !out.frame_size) return out;
	required = animation_header_size + uint64_t(out.frames) * out.frame_size;
    } else return out;
    out.valid = !input.file_size_known || input.file_size >= required;
    return out;
}

struct script {
    std::string encoding;
    std::set<std::string> commands;
    bool recognized = false, complete = false, textual = true;
    bool append(bu_mime_traits &traits) const
    {
	return textual && bu_mime_traits_add(&traits, "encoding", encoding.c_str()) &&
	    (!complete || mime_detail::append_set(traits, "unrealscript.exec-commands", commands));
    }
};

inline script inspect_script(std::istream &stream, bool complete)
{
    mime_unicode::characters input(stream);
    mime_detail::unquoted_line visible;
    script out;
    out.encoding = input.encoding;
    bool commands_complete = true;
    std::string declaration;
    auto lower = [](std::string text) {
	for (char &c : text) if (c >= 'A' && c <= 'Z') c += 'a' - 'A';
	return text;
    };
    auto line = [&](const std::string &raw) {
	std::string text = visible(raw);
	std::istringstream words(text);
	std::string first, name, relation, parent;
	if (!(words >> first)) return;
	first = lower(first);
	if (first == "class" || !declaration.empty()) {
	    declaration += ' ' + text;
	    size_t end = declaration.find(';'), brace = declaration.find('{');
	    if (brace != std::string::npos && (end == std::string::npos || brace < end)) declaration.clear();
	    else if (end != std::string::npos) {
		std::istringstream header(declaration);
		if (header >> first >> name >> relation >> parent) {
		    relation = lower(relation);
		    if (relation == "extends" || relation == "expands") out.recognized = true;
		}
		declaration.clear();
	    }
	}
	if (first == "#exec") {
	    if (!(words >> name >> relation)) {commands_complete = false; return;}
	    name = lower(name);
	    relation = lower(relation);
	    out.commands.insert(name + "." + relation);
	    if (name == "mesh" || name == "meshmap" || (name == "texture" && relation == "import")) {
		out.recognized = true;
	    }
	}
    };
    std::string text;
    for (int c = input.get(); c >= 0; c = input.get()) {
	if (c < ' ' && c != '\t' && c != '\n' && c != '\r' && c != '\f') {out.textual = false; break;}
	if (c == '\n' || c == '\r') {line(text); text.clear();}
	else input.append(text, c);
    }
    if (complete) line(text);
    out.textual = out.textual && !stream.bad() && input.valid();
    out.complete = complete && commands_complete && visible.complete() && out.textual;
    return out;
}
}
#endif
