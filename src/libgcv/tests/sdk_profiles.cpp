/* S D K _ P R O F I L E S . C P P
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

#include "common.h"

#include <algorithm>
#include <climits>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <initializer_list>
#include <iterator>
#include <limits>
#include <locale>
#include <string>
#include <utility>
#include <vector>
#include <zlib.h>

#include "bu/app.h"
#include "bu/file.h"
#include "bu/endian.h"
#include "bu/log.h"
#include "bu/malloc.h"
#include "bu/str.h"
#include "gcv/api.h"
#include "rt/db_internal.h"
#include "rt/db_io.h"
#include "rt/geom.h"
#include "wdb.h"
#include "../plugins/assetimport/gltf_draco.h"

namespace {
using bytes = std::vector<unsigned char>;
constexpr uint32_t float_one = 0x3f800000;
constexpr uint32_t float_two = 0x40000000;
constexpr uint64_t double_one = 0x3ff0000000000000ULL;
constexpr uint32_t triangle_vertices = 3;

/* Encode exchange words explicitly so fixture validity does not depend on
 * the host's byte order, floating-point representation or structure padding. */
void put_word(bytes &data, size_t offset, uint64_t value, size_t width = sizeof(uint32_t))
{
    if (data.size() < offset + width) data.resize(offset + width);
    for (size_t byte = 0; byte < width; ++byte)
	data[offset + byte] = static_cast<unsigned char>(value >> (CHAR_BIT * byte));
}

void append_words(bytes &data, std::initializer_list<uint32_t> values, size_t width = sizeof(uint32_t))
{
    for (uint32_t value : values) put_word(data, data.size(), value, width);
}

void append_be_words(bytes &data, std::initializer_list<uint32_t> values, size_t width = sizeof(uint32_t))
{
    for (uint32_t value : values) {
	put_word(data, data.size(), value, width);
	std::reverse(data.end() - width, data.end());
    }
}

void append_iff_chunk(bytes &data, const char *tag, const bytes &payload)
{
    const size_t tag_size = sizeof(uint32_t);
    data.insert(data.end(), tag, tag + tag_size);
    append_be_words(data, {static_cast<uint32_t>(payload.size())});
    data.insert(data.end(), payload.begin(), payload.end());
    if (payload.size() & 1) data.push_back(0);
}

bytes iff_triangle(const char *form, bool padded = false)
{
    const size_t tag_size = sizeof(uint32_t);
    bytes body(form, form + tag_size);
    if (padded) append_iff_chunk(body, "JUNK", bytes(BU_MIME_DEFAULT_SCAN_LIMIT + 2, 0));
    if (BU_STR_EQUAL(form, "LXOB")) {
	bytes version;
	append_be_words(version, {4, 2});
	version.insert(version.end(), {'x', 0});
	append_iff_chunk(body, "VRSN", version);
    }
    bytes points;
    append_be_words(points, {0, 0, 0, float_one, 0, 0, 0, float_one, 0});
    append_iff_chunk(body, "PNTS", points);
    bytes polygons;
    if (BU_STR_EQUAL(form, "LWOB")) append_be_words(polygons, {3, 0, 1, 2, 1}, sizeof(uint16_t));
    else {
	polygons = {'F', 'A', 'C', 'E'};
	append_be_words(polygons, {3, 0, 1, 2}, sizeof(uint16_t));
    }
    append_iff_chunk(body, "POLS", polygons);
    bytes data = {'F', 'O', 'R', 'M'};
    append_be_words(data, {static_cast<uint32_t>(body.size())});
    data.insert(data.end(), body.begin(), body.end());
    return data;
}

bytes xml_bytes(const std::string &xml, const char *encoding)
{
    std::string name = encoding;
    size_t width = name == "utf-8" ? 1 : name.find("16") != std::string::npos ? 2 : 4;
    bool little = name.find("le") != std::string::npos;
    bytes data;
    if (width == 2) data = little ? bytes{0xff, 0xfe} : bytes{0xfe, 0xff};
    if (width == 4) data = little ? bytes{0xff, 0xfe, 0, 0} : bytes{0, 0, 0xfe, 0xff};
    for (unsigned char c : xml)
	for (size_t byte = 0; byte < width; ++byte) data.push_back((little ? byte : width - byte - 1) == 0 ? c : 0);
    return data;
}

bytes zlib_bytes(const bytes &xml)
{
    uLongf size = compressBound(static_cast<uLong>(xml.size()));
    bytes compressed(size);
    int status = compress2(compressed.data(), &size, xml.data(), static_cast<uLong>(xml.size()), Z_BEST_SPEED);
    if (status != Z_OK) {
	bu_log("Cannot compress fixture: %d\n", status);
	return {};
    }
    compressed.resize(size);
    return compressed;
}

void append_short_chunk(bytes &data, uint16_t id, const bytes &payload, bool big)
{
    const size_t header_size = sizeof(uint16_t) + sizeof(uint32_t);
    if (big) {
	append_be_words(data, {id}, sizeof(uint16_t));
	append_be_words(data, {static_cast<uint32_t>(header_size + payload.size())});
    } else {
	append_words(data, {id}, sizeof(uint16_t));
	append_words(data, {static_cast<uint32_t>(header_size + payload.size())});
    }
    data.insert(data.end(), payload.begin(), payload.end());
}

bytes zip_bytes(const std::vector<std::pair<std::string, bytes>> &entries, bool compressed, bool zip64 = false)
{
    constexpr uint32_t local_signature = 0x04034b50, central_signature = 0x02014b50, end_signature = 0x06054b50;
    constexpr uint16_t zip_version = 20, stored_method = 0, deflate_method = 8;
    constexpr size_t zlib_header_size = 2, adler_size = sizeof(uint32_t);
    bytes data, directory;
    for (const auto &entry : entries) {
	bytes content = compressed ? zlib_bytes(entry.second) : entry.second;
	if (compressed) {
	    if (content.size() < zlib_header_size + adler_size) return {};
	    content = bytes(content.begin() + zlib_header_size, content.end() - adler_size);
	}
	uint32_t crc = static_cast<uint32_t>(crc32(0, entry.second.data(), static_cast<uInt>(entry.second.size())));
	uint32_t offset = static_cast<uint32_t>(data.size());
	uint16_t method = compressed ? deflate_method : stored_method;
	append_words(data, {local_signature});
	append_words(data, {zip_version, 0, method, 0, 0}, sizeof(uint16_t));
	append_words(data, {crc, static_cast<uint32_t>(content.size()), static_cast<uint32_t>(entry.second.size())});
	append_words(data, {static_cast<uint32_t>(entry.first.size()), 0}, sizeof(uint16_t));
	data.insert(data.end(), entry.first.begin(), entry.first.end());
	data.insert(data.end(), content.begin(), content.end());
	append_words(directory, {central_signature});
	append_words(directory, {zip_version, zip_version, 0, method, 0, 0}, sizeof(uint16_t));
	append_words(directory, {crc, static_cast<uint32_t>(content.size()), static_cast<uint32_t>(entry.second.size())});
	append_words(directory, {static_cast<uint32_t>(entry.first.size()), 0, 0, 0, 0}, sizeof(uint16_t));
	append_words(directory, {0, offset});
	directory.insert(directory.end(), entry.first.begin(), entry.first.end());
    }
    uint32_t directory_offset = static_cast<uint32_t>(data.size());
    data.insert(data.end(), directory.begin(), directory.end());
    if (zip64) {
	constexpr uint32_t zip64_end = 0x06064b50, zip64_locator = 0x07064b50, zip64_record_size = 44;
	constexpr uint16_t zip64_version = 45;
	uint32_t end_offset = static_cast<uint32_t>(data.size());
	append_words(data, {zip64_end, zip64_record_size, 0});
	append_words(data, {zip64_version, zip64_version}, sizeof(uint16_t));
	append_words(data, {0, 0, static_cast<uint32_t>(entries.size()), 0, static_cast<uint32_t>(entries.size()), 0,
	    static_cast<uint32_t>(directory.size()), 0, directory_offset, 0});
	append_words(data, {zip64_locator, 0, end_offset, 0, 1});
    }
    uint32_t count = static_cast<uint32_t>(entries.size());
    append_words(data, {end_signature});
    append_words(data, {0, 0, count, zip64 ? UINT16_MAX : count}, sizeof(uint16_t));
    append_words(data, {zip64 ? UINT32_MAX : static_cast<uint32_t>(directory.size()), zip64 ? UINT32_MAX : directory_offset});
    append_words(data, {0}, sizeof(uint16_t));
    return data;
}

bytes bsp_triangle()
{
    constexpr uint32_t bsp_version = 46, lump_count = 17, lump_size = 2 * sizeof(uint32_t);
    constexpr uint32_t entities_lump = 0, textures_lump = 1, vertices_lump = 10, indices_lump = 11, faces_lump = 13;
    constexpr size_t vertex_size = 44, face_size = 104, texture_name_size = 64;
    const size_t magic_size = sizeof("IBSP") - 1;
    bytes data(magic_size + sizeof(uint32_t) + lump_count * lump_size, 0);
    memcpy(data.data(), "IBSP", magic_size);
    put_word(data, magic_size, bsp_version);
    auto lump = [&](uint32_t index, const bytes &payload) {
	size_t offset = magic_size + sizeof(uint32_t) + index * lump_size;
	put_word(data, offset, static_cast<uint32_t>(data.size()));
	put_word(data, offset + sizeof(uint32_t), static_cast<uint32_t>(payload.size()));
	data.insert(data.end(), payload.begin(), payload.end());
    };
    const std::string entities = "{\n\"classname\" \"worldspawn\"\n}\n";
    bytes entity_data(entities.begin(), entities.end());
    entity_data.push_back(0);
    lump(entities_lump, entity_data);
    bytes texture(texture_name_size + 2 * sizeof(uint32_t), 0);
    const char *texture_name = "textures/fixture";
    memcpy(texture.data(), texture_name, strlen(texture_name));
    lump(textures_lump, texture);
    bytes vertices;
    for (const auto &position : std::vector<std::vector<uint32_t>>{{0, 0, 0}, {float_one, 0, 0}, {0, float_one, 0}}) {
	size_t offset = vertices.size();
	vertices.resize(offset + vertex_size, 0);
	for (size_t axis = 0; axis < position.size(); ++axis) put_word(vertices, offset + axis * sizeof(uint32_t), position[axis]);
	put_word(vertices, offset + 9 * sizeof(uint32_t), float_one); // Normal Z.
	put_word(vertices, offset + 10 * sizeof(uint32_t), UINT32_MAX); // RGBA.
    }
    lump(vertices_lump, vertices);
    bytes indices;
    append_words(indices, {0, 1, 2});
    lump(indices_lump, indices);
    bytes face(face_size, 0);
    put_word(face, sizeof(uint32_t), UINT32_MAX); // No effect.
    put_word(face, 2 * sizeof(uint32_t), 1); // Polygon.
    put_word(face, 4 * sizeof(uint32_t), triangle_vertices);
    put_word(face, 6 * sizeof(uint32_t), triangle_vertices);
    put_word(face, 7 * sizeof(uint32_t), UINT32_MAX); // No lightmap.
    put_word(face, 23 * sizeof(uint32_t), float_one); // Normal Z.
    lump(faces_lump, face);
    return data;
}

bytes ogre_triangle(const char *version, bool big = false)
{
    /* Ogre's chunk identifiers, vertex semantics and triangle-list opcode. */
    constexpr uint16_t header_id = 0x1000, mesh_id = 0x3000, submesh_id = 0x4000;
    constexpr uint16_t operation_id = 0x4010, geometry_id = 0x5000, declaration_id = 0x5100;
    constexpr uint16_t element_id = 0x5110, vertex_buffer_id = 0x5200, buffer_data_id = 0x5210, bounds_id = 0x9000;
    constexpr uint16_t float3_type = 2, position_semantic = 1, triangle_list = 4;
    auto words = [big](bytes &data, std::initializer_list<uint32_t> values, size_t width = sizeof(uint32_t)) {
	if (big) append_be_words(data, values, width);
	else append_words(data, values, width);
    };
    bytes data;
    words(data, {header_id}, sizeof(uint16_t));
    std::string tag = std::string("[MeshSerializer_v") + version + "]\n";
    data.insert(data.end(), tag.begin(), tag.end());
    bytes element;
    words(element, {0, float3_type, position_semantic, 0, 0}, sizeof(uint16_t));
    bytes declaration;
    append_short_chunk(declaration, element_id, element, big);
    bytes positions;
    words(positions, {0, 0, 0, float_one, 0, 0, 0, float_one, 0});
    bytes buffer;
    words(buffer, {0, 3 * sizeof(uint32_t)}, sizeof(uint16_t));
    append_short_chunk(buffer, buffer_data_id, positions, big);
    bytes geometry;
    words(geometry, {triangle_vertices});
    append_short_chunk(geometry, declaration_id, declaration, big);
    append_short_chunk(geometry, vertex_buffer_id, buffer, big);
    bytes mesh = {0}; // No skeleton.
    append_short_chunk(mesh, geometry_id, geometry, big);
    bytes submesh = {'\n', 1}; // Empty material reference, shared geometry.
    words(submesh, {triangle_vertices});
    submesh.push_back(0); // 16-bit indices.
    words(submesh, {0, 1, 2}, sizeof(uint16_t));
    bytes operation;
    words(operation, {triangle_list}, sizeof(uint16_t));
    append_short_chunk(submesh, operation_id, operation, big);
    append_short_chunk(mesh, submesh_id, submesh, big);
    bytes bounds;
    words(bounds, {0, 0, 0, float_one, float_one, 0, float_one});
    append_short_chunk(mesh, bounds_id, bounds, big);
    append_short_chunk(data, mesh_id, mesh, big);
    return data;
}

std::string irrlicht_buffer(const char *layout)
{
    std::string vertices;
    for (const char *position : {"0 0 0", "1 0 0", "0 1 0"}) {
	vertices += std::string(position) + " 0 0 1 ffffffff 0 0";
	if (BU_STR_EQUAL(layout, "2tcoords")) vertices += " 0 0";
	if (BU_STR_EQUAL(layout, "tangents")) vertices += " 1 0 0 0 1 0";
	vertices += '\n';
    }
    return std::string("<buffer><material><enum name='Type' value='solid'/></material><vertices type='") + layout +
	"' vertexCount='3'>\n" + vertices + "</vertices><indices indexCount='3'>0 1 2</indices></buffer>";
}

std::string collada_triangle(const char *version)
{
    const char *uri = BU_STR_EQUAL(version, "1.5.0") ? "http://www.collada.org/2008/03/COLLADASchema" : "http://www.collada.org/2005/11/COLLADASchema";
    return std::string("<?xml version='1.0'?><COLLADA version='") + version + "' xmlns='" + uri + "'>"
	"<asset><unit name='millimeter' meter='0.001'/><up_axis>Z_UP</up_axis></asset>"
	"<library_geometries><geometry id='triangle'><mesh><source id='positions'>"
	"<float_array id='positions-array' count='9'>0 0 0 1 0 0 0 1 0</float_array><technique_common>"
	"<accessor source='#positions-array' count='3' stride='3'><param name='X' type='float'/>"
	"<param name='Y' type='float'/><param name='Z' type='float'/></accessor></technique_common></source>"
	"<vertices id='vertices'><input semantic='POSITION' source='#positions'/></vertices>"
	"<triangles count='1'><input semantic='VERTEX' source='#vertices' offset='0'/><p>0 1 2</p></triangles>"
	"</mesh></geometry></library_geometries><library_visual_scenes><visual_scene id='Scene'>"
	"<node id='triangle-node'><instance_geometry url='#triangle'/></node></visual_scene></library_visual_scenes>"
	"<scene><instance_visual_scene url='#Scene'/></scene></COLLADA>";
}

bytes quick3d_triangle(char kind, char minor)
{
    bytes data = {'q', 'u', 'i', 'c', 'k', '3', 'D', static_cast<unsigned char>(kind), '3', static_cast<unsigned char>(minor)};
    append_words(data, {1, 0, 0}); // Mesh, material and texture counts.
    data.push_back('m');
    append_words(data, {3, 0, 0, 0, float_one, 0, 0, 0, float_one, 0, 1});
    append_words(data, {3}, sizeof(uint16_t));
    append_words(data, {0, 1, 2, 0, 3, 0, 0, float_one, 0, 0, float_one, 0, 0, float_one, 0});
    const size_t mesh_trailer_size = 36;
    data.resize(data.size() + mesh_trailer_size, 0);
    if (minor > '0') data.push_back(0); // Version 3.1's per-face flag.
    return data;
}

void append_pmx_text(bytes &data, const std::string &text, bool utf8)
{
    const size_t character_size = utf8 ? 1 : sizeof(uint16_t);
    append_words(data, {static_cast<uint32_t>(text.size() * character_size)});
    for (unsigned char character : text) put_word(data, data.size(), character, character_size);
}

bytes pmx_triangle(bool version21, bool utf8, unsigned int width, unsigned char uv)
{
    bytes data = {'P', 'M', 'X', ' '};
    append_words(data, {version21 ? 0x40066666u : 0x40000000u});
    data.push_back(8); // Eight standard global settings.
    data.push_back(utf8 ? 1 : 0);
    data.push_back(uv);
    data.insert(data.end(), 6, static_cast<unsigned char>(width));
    append_pmx_text(data, "triangle", utf8);
    append_pmx_text(data, "triangle", utf8);
    /* Keep a valid descriptive block large enough for the SDK's minimum
     * file-size check against its in-memory model structure. */
    const size_t comment_length = 512;
    append_pmx_text(data, std::string(comment_length, 'x'), utf8);
    append_pmx_text(data, "", utf8);
    append_words(data, {triangle_vertices});
    for (unsigned int vertex = 0; vertex < triangle_vertices; ++vertex) {
	append_words(data, {vertex == 1 ? float_one : 0, vertex == 2 ? float_one : 0, 0,
	    0, 0, float_one, 0, 0});
	for (unsigned char channel = 0; channel < uv; ++channel) append_words(data, {0, 0, 0, 0});
	data.push_back(0); // BDEF1 with full weight on the root bone.
	put_word(data, data.size(), 0, width);
	append_words(data, {float_one});
    }
    append_words(data, {3});
    append_words(data, {0, 1, 2}, width);
    append_words(data, {0, 1}); // No textures, one material.
    append_pmx_text(data, "material", utf8);
    append_pmx_text(data, "", utf8);
    append_words(data, {float_one, float_one, float_one, float_one, 0, 0, 0, 0, 0, 0, 0});
    data.push_back(0); // Material flags.
    append_words(data, {0, 0, 0, 0, 0}); // Edge color and size.
    append_words(data, {UINT32_MAX, UINT32_MAX}, width); // No diffuse/sphere textures.
    data.insert(data.end(), {0, 1, 0}); // Sphere mode, shared toon, toon index.
    append_pmx_text(data, "", utf8);
    append_words(data, {3, 1}); // Material's face indices, one bone.
    append_pmx_text(data, "root", utf8);
    append_pmx_text(data, "", utf8);
    append_words(data, {0, 0, 0});
    append_words(data, {UINT32_MAX}, width); // Root has no parent.
    append_words(data, {0}); // Transform layer.
    append_words(data, {0}, sizeof(uint16_t)); // No optional bone fields.
    append_words(data, {0, 0, 0}); // Tail offset.
    append_words(data, {0, 0, 0, 0}); // Morphs, display frames, rigid bodies, joints.
    if (version21) append_words(data, {0}); // No soft bodies.
    return data;
}

bytes quake_mdl_triangle()
{
    bytes data = {'I', 'D', 'P', 'O'};
    append_words(data, {6, float_one, float_one, float_one, 0, 0, 0, float_one,
	0, 0, 0, 0, 1, 1, triangle_vertices, 1, 1, 0, 0, 0});
    /* No skins; three seam/UV records, one triangle and a single frame. */
    append_words(data, {0, 0, 0, 0, 1, 0, 0, 0, 1});
    append_words(data, {1, 0, 1, 2});
    append_words(data, {0, 0, 0});
    const size_t frame_name_size = 16;
    data.resize(data.size() + frame_name_size);
    data.insert(data.end(), {0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0});
    /* The SDK's position checks require storage beyond the last vertex. */
    data.push_back(0);
    return data;
}

bytes hmp_heightfield(unsigned version)
{
    bytes data = {'H', 'M', 'P', static_cast<unsigned char>('0' + version)};
    /* The legacy word is deliberately different from the magic revision. */
    append_words(data, {99, 0, 0, 0, 0, 0, 0, 0, float_one, float_one, float_two,
	0, 0, 0, 4, 0, 1, 0, 0, 0});
    const size_t frame_header_size = 36;
    data.resize(data.size() + frame_header_size);
    append_words(data, {0, 0x00008000, 0x00008000, 0});
    return data;
}

bytes md2_triangle()
{
    const uint32_t header = 17 * sizeof(uint32_t);
    const uint32_t coordinates = triangle_vertices * 2 * sizeof(uint16_t);
    const uint32_t triangle = triangle_vertices * 2 * sizeof(uint16_t);
    const uint32_t frame_name = 16;
    const uint32_t frame = 6 * sizeof(uint32_t) + frame_name + triangle_vertices * 4;
    const uint32_t triangles_at = header + coordinates;
    const uint32_t frame_at = triangles_at + triangle;
    const uint32_t commands_at = frame_at + frame;
    /* The SDK expects trailing storage after the last frame. */
    const uint32_t file_size = commands_at + 1;
    bytes data = {'I', 'D', 'P', '2'};
    append_words(data, {8, 1, 1, frame, 0, triangle_vertices, triangle_vertices, 1, 0, 1,
	header, header, triangles_at, frame_at, commands_at, file_size});
    append_words(data, {0, 0, 1, 0, 0, 1}, sizeof(uint16_t));
    append_words(data, {0, 1, 2, 0, 1, 2}, sizeof(uint16_t));
    append_words(data, {float_one, float_one, float_one, 0, 0, 0});
    data.resize(data.size() + frame_name);
    data.insert(data.end(), {0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0});
    data.resize(file_size);
    return data;
}

bytes md3_triangle()
{
    const uint32_t name_size = 64;
    const uint32_t frame_name = 16;
    const uint32_t header = 2 * sizeof(uint32_t) + name_size + 9 * sizeof(uint32_t);
    const uint32_t frame = 10 * sizeof(uint32_t) + frame_name;
    const uint32_t surface_at = header + frame;
    const uint32_t surface_header = sizeof(uint32_t) + name_size + 10 * sizeof(uint32_t);
    const uint32_t coordinates_at = surface_header + triangle_vertices * sizeof(uint32_t);
    const uint32_t vertices_at = coordinates_at + triangle_vertices * 2 * sizeof(uint32_t);
    const uint32_t surface_size = vertices_at + triangle_vertices * 4 * sizeof(uint16_t);
    const uint32_t coordinate_one = 64; /* MD3 stores coordinates in 1/64 unit increments. */
    bytes data = {'I', 'D', 'P', '3'};
    append_words(data, {15});
    data.resize(data.size() + name_size);
    append_words(data, {0, 1, 0, 1, 0, header, surface_at, surface_at, surface_at + surface_size});
    append_words(data, {0, 0, 0, float_one, float_one, float_one, 0, 0, 0, float_one});
    data.resize(data.size() + frame_name);
    data.insert(data.end(), {'I', 'D', 'P', '3'});
    data.resize(data.size() + name_size);
    append_words(data, {0, 1, 0, triangle_vertices, 1, surface_header,
	coordinates_at, coordinates_at, vertices_at, surface_size});
    append_words(data, {0, 1, 2});
    append_words(data, {0, 0, float_one, 0, 0, float_one});
    append_words(data, {0, 0, 0, 0, coordinate_one, 0, 0, 0, 0, coordinate_one, 0, 0}, sizeof(uint16_t));
    return data;
}

bytes iqm_triangle()
{
    const unsigned char magic[] = "INTERQUAKEMODEL";
    const unsigned char names[] = "triangle\0material";
    const uint32_t header = sizeof(magic) + 27 * sizeof(uint32_t);
    const uint32_t padded_names = ((sizeof(names) + sizeof(uint32_t) - 1) / sizeof(uint32_t)) * sizeof(uint32_t);
    const uint32_t mesh_at = header + padded_names;
    const uint32_t arrays_at = mesh_at + 6 * sizeof(uint32_t);
    const uint32_t triangles_at = arrays_at + 5 * sizeof(uint32_t);
    const uint32_t positions_at = triangles_at + triangle_vertices * sizeof(uint32_t);
    const uint32_t file_size = positions_at + triangle_vertices * 3 * sizeof(uint32_t);
    const uint32_t position_array = 0;
    const uint32_t float_format = 7;
    bytes data(magic, magic + sizeof(magic));
    append_words(data, {2, file_size, 0, sizeof(names), header, 1, mesh_at, 1, triangle_vertices,
	arrays_at, 1, triangles_at, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0});
    data.insert(data.end(), names, names + sizeof(names));
    data.resize(mesh_at);
    append_words(data, {0, sizeof("triangle"), 0, triangle_vertices, 0, 1});
    append_words(data, {position_array, 0, float_format, 3, positions_at});
    append_words(data, {0, 1, 2});
    append_words(data, {0, 0, 0, float_one, 0, 0, 0, float_one, 0});
    return data;
}

bytes ms3d_triangle()
{
    const unsigned char magic[] = "MS3D000000";
    const unsigned char no_reference = 0xff;
    const unsigned char name[] = "triangle";
    const size_t group_name_size = 32;
    const uint32_t positions[][3] = {{0, 0, 0}, {float_one, 0, 0}, {0, float_one, 0}};
    bytes data(magic, magic + sizeof(magic) - 1);
    append_words(data, {4});
    append_words(data, {triangle_vertices}, sizeof(uint16_t));
    for (const auto &position : positions) {
	data.push_back(0);
	append_words(data, {position[0], position[1], position[2]});
	data.insert(data.end(), {no_reference, 1});
    }
    append_words(data, {1, 0, 0, 1, 2}, sizeof(uint16_t));
    for (uint32_t vertex = 0; vertex < triangle_vertices; ++vertex)
	append_words(data, {0, 0, float_one});
    append_words(data, {0, float_one, 0, 0, 0, float_one});
    data.insert(data.end(), {1, 0});
    append_words(data, {1}, sizeof(uint16_t));
    data.push_back(0);
    data.insert(data.end(), name, name + sizeof(name));
    data.resize(data.size() + group_name_size - sizeof(name));
    append_words(data, {1, 0}, sizeof(uint16_t));
    data.push_back(no_reference);
    append_words(data, {0}, sizeof(uint16_t));
    append_words(data, {float_one, 0, 1});
    append_words(data, {0}, sizeof(uint16_t));
    return data;
}

bytes nendo_triangle(unsigned minor)
{
    std::string signature = "nendo 1." + std::to_string(minor);
    const size_t index_size = minor == 2 ? sizeof(uint32_t) : sizeof(uint16_t);
    const size_t object_attributes_size = 76;
    const uint32_t positions[][3] = {{0, 0, 0}, {float_one, 0, 0}, {0, float_one, 0}};
    bytes data(signature.begin(), signature.end());
    data.resize(data.size() + 2);
    if (minor == 2) data.resize(data.size() + 2);
    data.insert(data.end(), {1, 1});
    const unsigned char name[] = "triangle";
    append_be_words(data, {sizeof(name) - 1}, index_size);
    data.insert(data.end(), name, name + sizeof(name) - 1);
    data.resize(data.size() + object_attributes_size);
    append_be_words(data, {triangle_vertices}, index_size);
    for (uint32_t edge = 0; edge < triangle_vertices; ++edge) {
	append_be_words(data, {edge, (edge + 1) % triangle_vertices, 0, 1,
	    (edge + 1) % triangle_vertices, (edge + 2) % triangle_vertices, 0, 0}, index_size);
	if (minor) data.push_back(0);
	data.resize(data.size() + 8);
    }
    append_be_words(data, {2, 0, 0, triangle_vertices}, index_size);
    for (uint32_t vertex = 0; vertex < triangle_vertices; ++vertex) {
	append_be_words(data, {vertex}, index_size);
	append_be_words(data, {positions[vertex][0], positions[vertex][1], positions[vertex][2]});
    }
    append_be_words(data, {0, 0}, index_size);
    data.push_back(0);
    return data;
}

void append_le_chunk(bytes &data, const char (&tag)[5], const bytes &payload)
{
    data.insert(data.end(), tag, tag + sizeof(tag) - 1);
    append_words(data, {static_cast<uint32_t>(payload.size())});
    data.insert(data.end(), payload.begin(), payload.end());
}

bytes silo_triangle(uint32_t version, bool late_header)
{
    bytes points;
    append_words(points, {0, 0, 0, float_one, 0, 0, 0, float_one, 0});
    bytes face;
    append_words(face, {triangle_vertices, 0, 1, 2});
    bytes shape;
    append_le_chunk(shape, "VRTS", points);
    append_le_chunk(shape, "FACS", face);
    bytes data;
    /* An ignorable chunk can place HEAD beyond bounded recognition. */
    if (late_header) append_le_chunk(data, "TEST", bytes(BU_MIME_DEFAULT_SCAN_LIMIT + 1));
    bytes header;
    append_words(header, {version});
    append_le_chunk(data, "HEAD", header);
    append_le_chunk(data, "SHAP", shape);
    return data;
}

bytes cob_triangle(bool binary)
{
    const char text[] = "Caligari V00.01ALH             \n"
	"PolH V0.04 Id 1 Parent 0 Size -1\nName triangle,1\n"
	"center 0 0 0\nx axis 1 0 0\ny axis 0 1 0\nz axis 0 0 1\n"
	"Transform\n1 0 0 0\n0 1 0 0\n0 0 1 0\n0 0 0 1\n"
	"World Vertices 3\n0 0 0\n1 0 0\n0 1 0\n"
	"Texture Vertices 3\n0 0\n1 0\n0 1\n"
	"Faces 1\nFace verts 3 flags 0 mat 0\n<0,0> <1,1> <2,2>\n"
	"END V1.00 Id 0 Parent 0 Size 0\n";
    if (!binary) return bytes(text, text + sizeof(text) - 1);
    const char header[] = "Caligari V00.01BLH             \n";
    const unsigned char name[] = "triangle";
    bytes body;
    append_words(body, {1, sizeof(name) - 1}, sizeof(uint16_t));
    body.insert(body.end(), name, name + sizeof(name) - 1);
    append_words(body, {0, 0, 0, float_one, 0, 0, 0, float_one, 0, 0, 0, float_one});
    append_words(body, {float_one, 0, 0, 0, 0, float_one, 0, 0, 0, 0, float_one, 0});
    append_words(body, {triangle_vertices, 0, 0, 0, float_one, 0, 0, 0, float_one, 0});
    append_words(body, {triangle_vertices, 0, 0, float_one, 0, 0, float_one, 1});
    body.push_back(0);
    append_words(body, {triangle_vertices, 0}, sizeof(uint16_t));
    append_words(body, {0, 0, 1, 1, 2, 2});
    bytes data(header, header + sizeof(header) - 1);
    data.insert(data.end(), {'P', 'o', 'l', 'H'});
    append_words(data, {0, 4}, sizeof(uint16_t));
    append_words(data, {1, 0, static_cast<uint32_t>(body.size())});
    data.insert(data.end(), body.begin(), body.end());
    data.insert(data.end(), {'E', 'N', 'D', ' '});
    append_words(data, {1, 0}, sizeof(uint16_t));
    append_words(data, {0, 0, 0});
    return data;
}

bytes terragen_heightfield()
{
    const unsigned char magic[] = "TERRAGENTERRAIN ";
    const uint32_t height_scale = 256;
    bytes data(magic, magic + sizeof(magic) - 1);
    data.insert(data.end(), {'S', 'I', 'Z', 'E'});
    append_words(data, {1, 0}, sizeof(uint16_t));
    data.insert(data.end(), {'S', 'C', 'A', 'L'});
    append_words(data, {float_one, float_one, float_one});
    data.insert(data.end(), {'A', 'L', 'T', 'W'});
    append_words(data, {height_scale, 0, 0, 1, 0, 0}, sizeof(uint16_t));
    data.insert(data.end(), {'E', 'O', 'F', ' '});
    return data;
}

#define PROFILE_CHECK(condition) do { \
    if (!(condition)) { \
	bu_log("SDK profile check failed for %s at line %d: %s\n", type, __LINE__, #condition); \
	goto done; \
    } \
} while (0)

struct profile_case {
    const char *type;
    const char *key;
    std::string value;
    std::string unsupported;
    bytes data;
    bytes invalid_data;
    const char *extension = NULL;
    size_t faces = 1;
    size_t vertices = triangle_vertices;
    bool explicit_traits = true;
    const char *invalid_companion_extension = NULL;
    bool request_selectable = false;
    const char *set_key = NULL;
    std::vector<const char *> set_members = {};
    bool native_suffix_required = false;
    bool untyped_may_accept = false;
    std::vector<std::pair<std::string, bytes>> companions = {};
    bu_mime_trait unestablished = BU_MIME_TRAIT_STRING("fixture.unestablished", "assertion");
};

int fixture_test(const profile_case &sample)
{
    const char *type = sample.type;
    struct gcv_context context;
    struct gcv_conversion_result result = GCV_CONVERSION_RESULT_INIT_ZERO;
    struct rt_db_internal object;
    struct directory **objects = NULL;
    char temporary[MAXPATHLEN] = {0};
    std::string filename;
    std::string companion;
    std::vector<std::string> companion_files;
    FILE *file = NULL;
    int status = 1;
    gcv_context_init(&context);
    RT_DB_INTERNAL_INIT(&object);
    {
	std::vector<bu_mime_trait> values = {BU_MIME_TRAIT_STRING(sample.key, sample.value.c_str())};
	if (sample.set_key) values.push_back(BU_MIME_TRAIT_SET(sample.set_key, sample.set_members.data(), sample.set_members.size()));
	const struct bu_mime_traits traits = {values.data(), values.size()};
	const struct gcv_filter *reader = gcv_select_filter(&context, GCV_FILTER_READ, type, "Assetimport Reader", NULL);
	PROFILE_CHECK(reader && gcv_select_variant(&context, GCV_FILTER_READ, type, &traits, "Assetimport Reader", NULL));
	values[0].value = sample.unsupported.c_str();
	PROFILE_CHECK((gcv_select_variant(&context, GCV_FILTER_READ, type, &traits, "Assetimport Reader", NULL) != NULL) == sample.request_selectable);
	values[0].value = sample.value.c_str();
	PROFILE_CHECK(!mk_id_units(wdb_dbopen(context.dbip, RT_WDB_TYPE_DB_INMEM), "Before SDK input validation", "cm"));
	const fastf_t original_units = context.dbip->dbi_local2base;
	const auto metadata_unchanged = [&]() {
	    return BU_STR_EQUAL(context.dbip->dbi_title, "Before SDK input validation") &&
		EQUAL(context.dbip->dbi_local2base, original_units);
	};
	const bytes &initial = sample.invalid_data.empty() ? sample.data : sample.invalid_data;
	file = bu_temp_file(temporary, sizeof(temporary));
	filename = temporary;
	PROFILE_CHECK(file && fwrite(initial.data(), 1, initial.size(), file) == initial.size());
	int close_status = fclose(file);
	file = NULL;
	PROFILE_CHECK(!close_status);
	if (sample.extension) {
	    if (sample.native_suffix_required) {
		PROFILE_CHECK(!gcv_execute(&context, reader, NULL, 0, NULL, filename.c_str()));
		PROFILE_CHECK(!gcv_execute_variant(&context, reader, type, &traits, NULL, 0, NULL, filename.c_str()));
		PROFILE_CHECK(metadata_unchanged());
		PROFILE_CHECK(!db_ls(context.dbip, DB_LS_PRIM | DB_LS_COMB, NULL, NULL));
	    }
	    std::string extended = filename + sample.extension;
	    PROFILE_CHECK(!rename(filename.c_str(), extended.c_str()));
	    filename = extended;
	}
	for (const auto &entry : sample.companions) {
	    std::string path = std::string(temporary) + entry.first;
	    companion_files.push_back(path);
	    file = fopen(path.c_str(), "wb");
	    PROFILE_CHECK(file && fwrite(entry.second.data(), 1, entry.second.size(), file) == entry.second.size());
	    close_status = fclose(file);
	    file = NULL;
	    PROFILE_CHECK(!close_status);
	}
	/* Assert the selected type directly to exercise full-reader checks.
	 * Compatibility execution may accept another source identity or leave
	 * an unrequested property to the native reader's existing policy. */
	if (!sample.invalid_data.empty()) {
	    PROFILE_CHECK(!gcv_execute_variant(&context, reader, type, &traits, NULL, 0, NULL, filename.c_str()));
	    if (!sample.untyped_may_accept) PROFILE_CHECK(!gcv_execute(&context, reader, NULL, 0, NULL, filename.c_str()));
	}
	PROFILE_CHECK(metadata_unchanged());
	PROFILE_CHECK(!db_ls(context.dbip, DB_LS_PRIM, NULL, &objects));
	bu_free(objects, "database primitives");
	objects = NULL;
	file = fopen(filename.c_str(), "wb");
	PROFILE_CHECK(file && fwrite(sample.data.data(), 1, sample.data.size(), file) == sample.data.size());
	close_status = fclose(file);
	file = NULL;
	PROFILE_CHECK(!close_status);
	if (sample.request_selectable) {
	    values[0].value = sample.unsupported.c_str();
	    PROFILE_CHECK(!gcv_execute_variant(&context, reader, type, &traits, NULL, 0, NULL, filename.c_str()));
	    values[0].value = sample.value.c_str();
	    PROFILE_CHECK(metadata_unchanged());
	    PROFILE_CHECK(!db_ls(context.dbip, DB_LS_PRIM | DB_LS_COMB, NULL, NULL));
	}
	if (sample.invalid_companion_extension) {
	    companion = filename.substr(0, filename.find_last_of('.')) + sample.invalid_companion_extension;
	    file = fopen(companion.c_str(), "wb");
	    PROFILE_CHECK(file && fputs("invalid companion input\n", file) >= 0);
	    close_status = fclose(file);
	    file = NULL;
	    PROFILE_CHECK(!close_status);
	}
	if (sample.set_key && !sample.set_members.empty()) {
	    /* An exact file assertion differs from the capability's supported
	     * superset. A nonempty source cannot satisfy a known-empty request. */
	    values.back().members = NULL;
	    values.back().member_count = 0;
	    PROFILE_CHECK(!gcv_execute_variant(&context, reader, type, &traits, NULL, 0, NULL, filename.c_str()));
	    PROFILE_CHECK(metadata_unchanged());
	    PROFILE_CHECK(!db_ls(context.dbip, DB_LS_PRIM | DB_LS_COMB, NULL, NULL));
	    values.back().members = sample.set_members.data();
	    values.back().member_count = sample.set_members.size();
	}
	{
	    const bu_mime_traits request = {&sample.unestablished, 1};
	    PROFILE_CHECK(!gcv_execute_variant(&context, reader, type, &request, NULL, 0, NULL, filename.c_str()));
	    PROFILE_CHECK(metadata_unchanged());
	    PROFILE_CHECK(!db_ls(context.dbip, DB_LS_PRIM | DB_LS_COMB, NULL, NULL));
	}
	PROFILE_CHECK(gcv_execute_result(&context, reader, type, sample.explicit_traits ? &traits : NULL, NULL, 0, NULL,
	    filename.c_str(), &result) == GCV_CONVERSION_COMPLETE);
	PROFILE_CHECK(db_ls(context.dbip, DB_LS_PRIM, NULL, &objects) == 1);
	PROFILE_CHECK(rt_db_get_internal(&object, objects[0], context.dbip, NULL) == ID_BOT);
	const auto *bot = static_cast<const rt_bot_internal *>(object.idb_ptr);
	if (bot->num_faces != sample.faces || bot->num_vertices != sample.vertices)
	    bu_log("SDK fixture expected %zu faces/%zu vertices, got %zu/%zu\n",
		sample.faces, sample.vertices, bot->num_faces, bot->num_vertices);
	PROFILE_CHECK(bot->num_faces == sample.faces && bot->num_vertices == sample.vertices);
	status = 0;
    }
done:
    if (file) fclose(file);
    if (!companion.empty()) bu_file_delete(companion.c_str());
    for (const auto &path : companion_files) bu_file_delete(path.c_str());
    if (!filename.empty()) bu_file_delete(filename.c_str());
    bu_free(objects, "database primitives");
    if (object.idb_ptr) rt_db_free_internal(&object);
    gcv_conversion_result_free(&result);
    gcv_context_destroy(&context);
    return status;
}

int profile_test(const char *type, uint32_t version, size_t version_at, const bytes &data,
    size_t faces = 1, size_t vertices = triangle_vertices)
{
    /* Zero denotes a format with no version word, not a version-0 file. */
    profile_case sample{type, version ? "version" : "encoding", version ? std::to_string(version) : "binary",
	version ? std::to_string(version + 1) : "ascii", data, {}};
    sample.faces = faces;
    sample.vertices = vertices;
    if (version) {
	sample.invalid_data = data;
	put_word(sample.invalid_data, version_at, version + 1);
    }
    return fixture_test(sample);
}

bytes x_triangle(bool binary, bool wide, bool compressed)
{
    const std::string text = "\nMesh triangle {\n3;\n0;0;0;,1;0;0;,0;1;0;;\n1;\n3;0,1,2;;\n}\n";
    bytes body(text.begin(), text.end());
    if (binary) {
	body.clear();
	const auto name = [&](const char *value) {
	    append_words(body, {1}, sizeof(uint16_t));
	    append_words(body, {static_cast<uint32_t>(strlen(value))});
	    body.insert(body.end(), value, value + strlen(value));
	};
	name("Mesh"); name("triangle");
	append_words(body, {10, 3}, sizeof(uint16_t));
	append_words(body, {3});
	append_words(body, {7}, sizeof(uint16_t));
	append_words(body, {9});
	const uint64_t one = wide ? double_one : float_one;
	const size_t width = wide ? sizeof(uint64_t) : sizeof(uint32_t);
	for (unsigned coordinate : {0, 0, 0, 1, 0, 0, 0, 1, 0})
	    put_word(body, body.size(), coordinate ? one : 0, width);
	append_words(body, {6}, sizeof(uint16_t));
	append_words(body, {5, 1, 3, 0, 1, 2});
	append_words(body, {11}, sizeof(uint16_t));
    }
    std::string header = "xof 0303";
    if (binary) header += compressed ? "bzip" : "bin ";
    else header += compressed ? "tzip" : "txt ";
    header += wide ? "0064" : "0032";
    bytes data(header.begin(), header.end());
    if (compressed) {
	const bytes encoded = zlib_bytes(body);
	constexpr size_t zlib_header = 2, adler_size = sizeof(uint32_t);
	if (encoded.size() < zlib_header + adler_size) return {};
	const size_t size = encoded.size() - zlib_header - adler_size;
	/* The SDK skips the six-byte MSZIP master header. Each following
	 * block carries a little-endian compressed length and CK marker. */
	append_words(data, {static_cast<uint32_t>(body.size())});
	append_words(data, {0, static_cast<uint32_t>(size)}, sizeof(uint16_t));
	data.insert(data.end(), {'C', 'K'});
	data.insert(data.end(), encoded.begin() + zlib_header, encoded.end() - adler_size);
    } else data.insert(data.end(), body.begin(), body.end());
    return data;
}

int x_profiles_test()
{
    const char *type = "model/vnd.microsoft.x";
    for (bool binary : {false, true})
	for (bool wide : {false, true})
	    for (bool compressed : {false, true}) {
		gcv_context context;
		gcv_context_init(&context);
		const bu_mime_trait values[] = {BU_MIME_TRAIT_STRING("x.serialization", binary ? "binary" : "text"),
		    BU_MIME_TRAIT_STRING("x.float-width", wide ? "64" : "32"), BU_MIME_TRAIT_STRING("compression", compressed ? "mszip" : "none")};
		const bu_mime_traits request = {values, sizeof(values) / sizeof(values[0])};
		const bool available = gcv_select_variant(&context, GCV_FILTER_READ, type, &request, "Assetimport Reader", NULL) != NULL;
		gcv_context_destroy(&context);
		if (!available) {
		    if (!binary && !wide && !compressed) {bu_log("SDK lost the basic X text profile\n"); return 1;}
		    continue;
		}
		const bytes data = x_triangle(binary, wide, compressed);
		const struct {const char *key; const char *value; const char *other; bool selectable;} fields[] = {
		    {"x.serialization", binary ? "binary" : "text", "unknown", false},
		    {"x.float-width", wide ? "64" : "32", "80", false},
		    {"compression", compressed ? "mszip" : "none", "gzip", false},
		    {"x.header-code-unit-width", "8", "16", false},
		    {"version", "3.3", "9.9", true}, {"x.version-code", "0303", "9909", true},
		    {"encoding", binary || compressed ? "binary" : "ascii", "unestablished", true}
		};
		for (const auto &field : fields) {
		    profile_case sample{type, field.key, field.value, field.other, data, {}};
		    sample.request_selectable = field.selectable;
		    if (fixture_test(sample)) return 1;
		}
		profile_case bom{type, "x.float-width", wide ? "64" : "32", "80", data, {}};
		bom.data.insert(bom.data.begin(), {0xef, 0xbb, 0xbf});
		bom.extension = ".x";
		bom.native_suffix_required = true;
		if (fixture_test(bom)) return 1;
	    }
    profile_case wide_header{type, "x.serialization", "text", "unknown", x_triangle(false, false, false), {}};
    const std::string text(wide_header.data.begin(), wide_header.data.end());
    for (const char *encoding : {"utf-16le", "utf-16be", "utf-32le", "utf-32be"}) {
	wide_header.invalid_data = xml_bytes(text, encoding);
	if (fixture_test(wide_header)) return 1;
    }
    return 0;
}

bytes assbin_triangle(bool wide, bool compressed, bool big = false)
{
    const auto words = [big](bytes &data, std::initializer_list<uint32_t> values, size_t width = sizeof(uint32_t)) {
        if (big) append_be_words(data, values, width);
        else append_words(data, values, width);
    };
    const auto put = [big](bytes &data, size_t at, uint64_t value, size_t width = sizeof(uint32_t)) {
        put_word(data, at, value, width);
        if (big) std::reverse(data.begin() + at, data.begin() + at + width);
    };
    constexpr uint32_t scene_chunk = 0x1239, node_chunk = 0x123c, mesh_chunk = 0x1237, material_chunk = 0x123d;
    const uint64_t one = wide ? double_one : float_one;
    const size_t real_size = wide ? sizeof(uint64_t) : sizeof(uint32_t);
    const auto chunk = [&](uint32_t code, const bytes &body) {
	bytes data;
	words(data, {code, static_cast<uint32_t>(body.size())});
	data.insert(data.end(), body.begin(), body.end());
	return data;
    };
    bytes node;
    const char name[] = "triangle";
    words(node, {sizeof(name) - 1});
    node.insert(node.end(), name, name + sizeof(name) - 1);
    constexpr size_t matrix_side = 4;
    for (size_t at = 0; at < matrix_side * matrix_side; ++at)
	put(node, node.size(), at % (matrix_side + 1) == 0 ? one : 0, real_size);
    words(node, {0, 1, 0, 0}); // No children/metadata; one mesh reference.
    bytes mesh;
    constexpr uint32_t triangles = 4, positions = 1;
    words(mesh, {triangles, 3, 1, 0, 0, positions});
    for (unsigned coordinate : {0, 0, 0, 1, 0, 0, 0, 1, 0})
	put(mesh, mesh.size(), coordinate ? one : 0, real_size);
    words(mesh, {3, 0, 1, 2}, sizeof(uint16_t));
    bytes material;
    words(material, {0});
    bytes scene;
    words(scene, {0, 1, 1, 0, 0, 0, 0});
    for (const auto &data : {chunk(node_chunk, node), chunk(mesh_chunk, mesh), chunk(material_chunk, material)})
	scene.insert(scene.end(), data.begin(), data.end());
    const bytes body = chunk(scene_chunk, scene);
    const char signature[] = "ASSIMP.binary-dump.";
    bytes data(signature, signature + sizeof(signature) - 1);
    constexpr size_t header_size = 512, version_at = 44, flags_at = 56, compression_at = 62;
    constexpr uint32_t double_support = 0x20;
    data.resize(header_size);
    put(data, version_at, 1);
    put(data, flags_at, wide ? double_support : 0);
    put(data, compression_at, compressed ? 1 : 0, sizeof(uint16_t));
    if (compressed) {
	words(data, {static_cast<uint32_t>(body.size())});
	const bytes encoded = zlib_bytes(body);
	data.insert(data.end(), encoded.begin(), encoded.end());
    } else data.insert(data.end(), body.begin(), body.end());
    return data;
}

int assbin_profiles_test()
{
    const char *type = "model/vnd.assimp.binary";
    bool tested = false;
    const bool big = bu_byteorder() == BU_BIG_ENDIAN;
    for (bool wide : {false, true}) {
	gcv_context context;
	gcv_context_init(&context);
	const bu_mime_trait width_trait = BU_MIME_TRAIT_STRING("assimp.real-width", wide ? "64" : "32");
	const bu_mime_traits request = {&width_trait, 1};
	const bool available = gcv_select_variant(&context, GCV_FILTER_READ, type, &request, "Assetimport Reader", NULL) != NULL;
	gcv_context_destroy(&context);
	if (!available) continue;
	tested = true;
	for (bool compressed : {false, true}) {
	    const bytes data = assbin_triangle(wide, compressed, big);
	    const struct {const char *key; const char *value; const char *other; bool selectable;} fields[] = {
		{"assimp.real-width", wide ? "64" : "32", wide ? "32" : "64", false},
		{"compression", compressed ? "zlib" : "none", "gzip", false}, {"version", "1.0", "2.0", false},
		{"assimp.shortened", "false", "true", false}, {"byte-order", big ? "big-endian" : "little-endian", big ? "little-endian" : "big-endian", false},
		{"assimp.producer-revision", "0", "12345", true}, {"assimp.compression-flag", compressed ? "1" : "0", "2", true}
	    };
	    for (const auto &field : fields) {
		profile_case sample{type, field.key, field.value, field.other, data, {}};
		sample.request_selectable = field.selectable;
		if (fixture_test(sample)) return 1;
	    }
	    profile_case different_width{type, "assimp.real-width", wide ? "64" : "32", wide ? "32" : "64",
		data, assbin_triangle(!wide, compressed, big)};
	    if (fixture_test(different_width)) return 1;
	    profile_case different_order{type, "encoding", "binary", "ascii", data, assbin_triangle(wide, compressed, !big)};
	    if (fixture_test(different_order)) return 1;
	    for (size_t offset : {size_t(44), size_t(60)}) {
		profile_case unsupported{type, "encoding", "binary", "ascii", data, data};
		const size_t width = offset == 44 ? sizeof(uint32_t) : sizeof(uint16_t);
		put_word(unsupported.invalid_data, offset, 2, width);
		if (big) std::reverse(unsupported.invalid_data.begin() + offset, unsupported.invalid_data.begin() + offset + width);
		if (fixture_test(unsupported)) return 1;
	    }
	    if (compressed) {
		profile_case boolean_flag{type, "assimp.compression-flag", "2", "0", data, {}};
		put_word(boolean_flag.data, 62, 2, sizeof(uint16_t));
		if (big) std::reverse(boolean_flag.data.begin() + 62, boolean_flag.data.begin() + 64);
		boolean_flag.request_selectable = true;
		if (fixture_test(boolean_flag)) return 1;
	    }
	}
    }
    if (!tested && (bu_byteorder() == BU_LITTLE_ENDIAN || bu_byteorder() == BU_BIG_ENDIAN) && sizeof(unsigned int) == sizeof(uint32_t) &&
	sizeof(float) == sizeof(uint32_t) && sizeof(double) == sizeof(uint64_t) &&
	std::numeric_limits<float>::is_iec559 && std::numeric_limits<double>::is_iec559) {
	bu_log("SDK lost the native Assimp binary layout\n"); return 1;
    }
    return 0;
}

int scene_text_profiles_test()
{
    const std::string ac3d = "AC3Db\nOBJECT poly\nname \"triangle\"\nnumvert 3\n0 0 0\n1 0 0\n0 1 0\n"
	"numsurf 1\nSURF 0x0\nrefs 3\n0 0 0\n1 1 0\n2 0 1\nkids 0\n";
    const std::string ase = "*3DSMAX_ASCIIEXPORT 200\n"
	"*MATERIAL_LIST {\n *MATERIAL_COUNT 1\n *MATERIAL 0 {\n *MATERIAL_NAME \"default\"\n *MATERIAL_DIFFUSE 1 1 1\n }\n}\n"
	"*GEOMOBJECT {\n *NODE_NAME \"triangle\"\n *NODE_TM {\n *TM_ROW0 1 0 0\n *TM_ROW1 0 1 0\n *TM_ROW2 0 0 1\n *TM_ROW3 0 0 0\n }\n"
	"*MESH {\n *TIMEVALUE 0\n *MESH_NUMVERTEX 3\n *MESH_NUMFACES 1\n *MESH_VERTEX_LIST {\n"
	" *MESH_VERTEX 0 0 0 0\n *MESH_VERTEX 1 1 0 0\n *MESH_VERTEX 2 0 1 0\n }\n *MESH_FACE_LIST {\n"
	" *MESH_FACE 0: A: 0 B: 1 C: 2 AB: 1 BC: 1 CA: 1 *MESH_SMOOTHING 1 *MESH_MTLID 0\n }\n }\n *MATERIAL_REF 0\n}\n";
    const struct {const char *type; const char *serialization; const char *version; const char *header_version; const std::string &data;} formats[] = {
	{"model/vnd.inivis.ac3d", "ac3d.serialization", "11", "b", ac3d},
	{"model/vnd.autodesk.ase", "ase.serialization", "200", "200", ase}
    };
    for (const auto &format : formats) {
	const bytes data(format.data.begin(), format.data.end());
	profile_case sample{format.type, format.serialization, "text", "binary", data, {}};
	if (fixture_test(sample)) return 1;
	sample.key = "text.code-unit-width"; sample.value = "8"; sample.unsupported = "16";
	for (const char *encoding : {"utf-16le", "utf-16be", "utf-32le", "utf-32be"}) {
	    sample.invalid_data = xml_bytes(format.data, encoding);
	    if (fixture_test(sample)) return 1;
	}
	sample.invalid_data.clear();
	sample.key = "version"; sample.value = format.version; sample.unsupported = "999";
	sample.request_selectable = true;
	if (fixture_test(sample)) return 1;
	sample.key = "encoding"; sample.value = "ascii"; sample.unsupported = "binary";
	if (fixture_test(sample)) return 1;
	sample.data.insert(sample.data.begin(), {0xef, 0xbb, 0xbf});
	sample.value = "utf-8"; sample.unsupported = "ascii";
	if (BU_STR_EQUAL(format.type, "model/vnd.inivis.ac3d")) {
	    sample.extension = ".ac";
	    sample.native_suffix_required = true;
	}
	if (fixture_test(sample)) return 1;
	sample.extension = NULL;
	sample.native_suffix_required = false;
	std::string unicode = format.data;
	const size_t name_at = unicode.find("triangle");
	unicode.replace(name_at, strlen("triangle"), "tri\xc3\xa9");
	sample.data.assign(unicode.begin(), unicode.end());
	if (fixture_test(sample)) return 1;
	/* An ASCII declaration cannot establish the body's encoding. The full
	 * reader must reach non-ASCII text beyond the prefix before accepting it. */
	std::string padded = format.data;
	const size_t padded_at = padded.find("triangle");
	padded.replace(padded_at, strlen("triangle"), std::string(BU_MIME_DEFAULT_SCAN_LIMIT + 1, 'p') + "\xc3\xa9");
	sample.data.assign(padded.begin(), padded.end());
	if (fixture_test(sample)) return 1;
	std::string missing = format.data;
	const size_t version_at = missing.find(std::string(format.header_version) + '\n');
	missing.erase(version_at, strlen(format.header_version));
	sample.key = format.serialization; sample.value = "text"; sample.unsupported = "binary";
	sample.data.assign(missing.begin(), missing.end());
	sample.request_selectable = false;
	sample.unestablished = BU_MIME_TRAIT_STRING("version", format.version);
	if (fixture_test(sample)) return 1;
    }
    return 0;
}

bytes b3d_triangle(uint32_t version)
{
    bytes vertices;
    append_words(vertices, {0, 0, 0, 0, 0, 0, float_one, 0, 0, 0, float_one, 0});
    bytes triangles;
    constexpr uint32_t default_material = UINT32_MAX;
    append_words(triangles, {default_material, 0, 1, 2});
    bytes mesh;
    append_words(mesh, {default_material});
    append_le_chunk(mesh, "VRTS", vertices);
    append_le_chunk(mesh, "TRIS", triangles);
    const unsigned char name[] = "triangle";
    bytes node(name, name + sizeof(name));
    append_words(node, {0, 0, 0, float_one, float_one, float_one, float_one, 0, 0, 0});
    append_le_chunk(node, "MESH", mesh);
    bytes body;
    append_words(body, {version});
    append_le_chunk(body, "NODE", node);
    bytes data;
    append_le_chunk(data, "BB3D", body);
    return data;
}

int b3d_profiles_test()
{
    const char *type = "model/vnd.blitzresearch.b3d";
    const bool native = bu_byteorder() == BU_LITTLE_ENDIAN && sizeof(float) == sizeof(uint32_t) &&
	sizeof(int) == sizeof(uint32_t) && std::numeric_limits<float>::is_iec559;
    gcv_context context;
    gcv_context_init(&context);
    const bool advertised = gcv_select_filter(&context, GCV_FILTER_READ, type, "Assetimport Reader", NULL) != NULL;
    gcv_context_destroy(&context);
    if (advertised != native) {bu_log("B3D capability does not match the native SDK layout\n"); return 1;}
    if (!native) return 0;
    const struct {uint32_t code; const char *version;} versions[] = {{1, "0.01"}, {100, "1.00"}};
    for (const auto &version : versions) {
	const auto data = b3d_triangle(version.code);
	profile_case sample{type, "encoding", "binary", "ascii", data, {}};
	sample.extension = version.code == 1 ? ".b3d" : ".B3D";
	sample.native_suffix_required = true;
	if (fixture_test(sample)) return 1;
	sample.key = "byte-order"; sample.value = "little-endian"; sample.unsupported = "big-endian";
	if (fixture_test(sample)) return 1;
	sample.key = "version"; sample.value = version.version; sample.unsupported = "9.99";
	sample.request_selectable = true;
	if (fixture_test(sample)) return 1;
	sample.key = "b3d.version-code"; sample.value = std::to_string(version.code); sample.unsupported = std::to_string(version.code + 1);
	if (fixture_test(sample)) return 1;
	sample.key = "b3d.payload-size"; sample.value = std::to_string(data.size() - 2 * sizeof(uint32_t)); sample.unsupported = "0";
	if (fixture_test(sample)) return 1;
    }
    return 0;
}

bytes blender_triangle(size_t pointer_size, bool big)
{
    /* A self-describing mesh fixture exercises both pointer layouts and
     * byte orders without depending on a Blender installation or host ABI. */
    enum type_index {character, short_integer, integer, real, opaque, material, id, list,
	scene, base, object, mesh, vertex, face};
    enum structure_index {id_structure, list_structure, scene_structure, base_structure,
	object_structure, mesh_structure, vertex_structure, face_structure};
    struct field {uint16_t type; const char *name;};
    struct structure {uint16_t type; std::vector<field> fields;};
    const std::vector<structure> structures = {
	{id, {{character, "name[64]"}}},
	{list, {{opaque, "*first"}, {opaque, "*last"}}},
	{scene, {{id, "id"}, {list, "base"}}},
	{base, {{base, "*next"}, {base, "*prev"}, {object, "*object"}}},
	{object, {{id, "id"}, {integer, "type"}, {real, "obmat[4][4]"}, {integer, "_pad"}, {opaque, "*data"}}},
	{mesh, {{id, "id"}, {integer, "totface"}, {integer, "totedge"}, {integer, "totvert"},
	    {integer, "_pad"}, {face, "*mface"}, {vertex, "*mvert"}, {material, "**mat"}}},
	{vertex, {{real, "co[3]"}, {short_integer, "no[3]"}, {short_integer, "_pad"}}},
	{face, {{integer, "v1"}, {integer, "v2"}, {integer, "v3"}, {integer, "v4"},
	    {short_integer, "mat_nr"}, {short_integer, "_pad"}}}
    };
    const std::vector<const char *> types = {"char", "short", "int", "float", "void", "Material",
	"ID", "ListBase", "Scene", "Base", "Object", "Mesh", "MVert", "MFace"};
    constexpr size_t name_size = 64, matrix_side = 4, matrix_words = matrix_side * matrix_side;
    constexpr size_t vertex_size = 3 * sizeof(uint32_t) + 4 * sizeof(uint16_t);
    constexpr size_t face_size = 4 * sizeof(uint32_t) + 2 * sizeof(uint16_t);
    const std::vector<size_t> sizes = {1, 2, 4, 4, 0, 0, name_size, 2 * pointer_size,
	name_size + 2 * pointer_size, 3 * pointer_size, name_size + (matrix_words + 2) * sizeof(uint32_t) + pointer_size,
	name_size + 4 * sizeof(uint32_t) + 3 * pointer_size, vertex_size, face_size};
    const auto word = [big](bytes &data, uint64_t value, size_t width = sizeof(uint32_t)) {
	for (size_t byte = 0; byte < width; ++byte)
	    data.push_back(static_cast<unsigned char>(value >> (CHAR_BIT * (big ? width - byte - 1 : byte))));
    };
    const auto tag = [](bytes &data, const char *value) {data.insert(data.end(), value, value + sizeof(uint32_t));};
    const auto align = [](bytes &data) {while (data.size() % sizeof(uint32_t)) data.push_back(0);};
    std::vector<std::string> names;
    for (const auto &definition : structures)
	for (const auto &field : definition.fields)
	    if (std::find(names.begin(), names.end(), field.name) == names.end()) names.emplace_back(field.name);
    bytes dna;
    tag(dna, "SDNA"); tag(dna, "NAME"); word(dna, names.size());
    for (const auto &name : names) {dna.insert(dna.end(), name.begin(), name.end()); dna.push_back(0);}
    align(dna);
    tag(dna, "TYPE"); word(dna, types.size());
    for (const char *type : types) dna.insert(dna.end(), type, type + strlen(type) + 1);
    align(dna);
    tag(dna, "TLEN");
    for (size_t size : sizes) word(dna, size, sizeof(uint16_t));
    align(dna);
    tag(dna, "STRC"); word(dna, structures.size());
    for (const auto &definition : structures) {
	word(dna, definition.type, sizeof(uint16_t)); word(dna, definition.fields.size(), sizeof(uint16_t));
	for (const auto &field : definition.fields) {
	    word(dna, field.type, sizeof(uint16_t));
	    word(dna, static_cast<size_t>(std::find(names.begin(), names.end(), field.name) - names.begin()), sizeof(uint16_t));
	}
    }
    const auto identifier = [](const char *name) {
	bytes value(name, name + strlen(name));
	value.resize(name_size);
	return value;
    };
    bytes data = {'B', 'L', 'E', 'N', 'D', 'E', 'R', static_cast<unsigned char>(pointer_size == 8 ? '-' : '_'),
	static_cast<unsigned char>(big ? 'V' : 'v'), '2', '5', '0'};
    const auto block = [&](const char *code, uint32_t address, structure_index index, uint32_t count, const bytes &body) {
	tag(data, code); word(data, body.size()); word(data, address, pointer_size); word(data, index); word(data, count);
	data.insert(data.end(), body.begin(), body.end());
    };
    enum address {scene_address = 0x1000, base_address = 0x2000, object_address = 0x3000,
	mesh_address = 0x4000, face_address = 0x5000, vertex_address = 0x6000};
    bytes body = identifier("SCtest");
    word(body, base_address, pointer_size); word(body, base_address, pointer_size);
    block("SC\0\0", scene_address, scene_structure, 1, body);
    body.clear();
    word(body, 0, pointer_size); word(body, 0, pointer_size); word(body, object_address, pointer_size);
    block("DATA", base_address, base_structure, 1, body);
    body = identifier("OBtriangle");
    constexpr uint32_t mesh_object = 1;
    word(body, mesh_object);
    for (size_t value = 0; value < matrix_words; ++value) word(body, value % (matrix_side + 1) == 0 ? float_one : 0);
    word(body, 0); word(body, mesh_address, pointer_size);
    block("OB\0\0", object_address, object_structure, 1, body);
    body = identifier("MEtriangle");
    for (uint32_t value : {1u, 0u, triangle_vertices, 0u}) word(body, value);
    word(body, face_address, pointer_size); word(body, vertex_address, pointer_size); word(body, 0, pointer_size);
    block("ME\0\0", mesh_address, mesh_structure, 1, body);
    body.clear();
    for (uint32_t value : {0u, 1u, 2u, 0u}) word(body, value);
    word(body, 0, sizeof(uint16_t)); word(body, 0, sizeof(uint16_t));
    block("DATA", face_address, face_structure, 1, body);
    body.clear();
    constexpr uint16_t normal_one = 32767;
    for (uint32_t value = 0; value < triangle_vertices; ++value) {
	word(body, value == 1 ? float_one : 0); word(body, value == 2 ? float_one : 0); word(body, 0);
	for (uint16_t normal : {uint16_t(0), uint16_t(0), normal_one, uint16_t(0)}) word(body, normal, sizeof(uint16_t));
    }
    block("DATA", vertex_address, vertex_structure, triangle_vertices, body);
    block("DNA1", 0, id_structure, 1, dna);
    block("ENDB", 0, id_structure, 0, {});
    return data;
}

bytes gzip_bytes(const bytes &data)
{
    z_stream stream = {};
    constexpr int gzip_window = MAX_WBITS + 16;
    const int initialized = deflateInit2(&stream, Z_BEST_SPEED, Z_DEFLATED, gzip_window, MAX_MEM_LEVEL, Z_DEFAULT_STRATEGY);
    if (initialized != Z_OK) {bu_log("Cannot initialize gzip fixture: %d\n", initialized); return {};}
    bytes compressed(deflateBound(&stream, static_cast<uLong>(data.size())));
    stream.next_in = const_cast<Bytef *>(data.data());
    stream.avail_in = static_cast<uInt>(data.size());
    stream.next_out = compressed.data();
    stream.avail_out = static_cast<uInt>(compressed.size());
    const int status = deflate(&stream, Z_FINISH);
    compressed.resize(stream.total_out);
    deflateEnd(&stream);
    if (status != Z_STREAM_END) {bu_log("Cannot gzip fixture: %d\n", status); return {};}
    return compressed;
}

int blender_profiles_test()
{
    const char *type = "application/x-blender";
    for (size_t pointer_size : {size_t(4), size_t(8)})
	for (bool big : {false, true})
	    for (bool compressed : {false, true}) {
		const bytes raw = blender_triangle(pointer_size, big);
		const bytes data = compressed ? gzip_bytes(raw) : raw;
		const std::string width = std::to_string(pointer_size * CHAR_BIT);
		gcv_context context;
		gcv_context_init(&context);
		const bu_mime_trait trait = BU_MIME_TRAIT_STRING("compression", "gzip");
		const bu_mime_traits request = {&trait, 1};
		const bool gzip_available = gcv_select_variant(&context, GCV_FILTER_READ, type, &request, "Assetimport Reader", NULL) != NULL;
		bu_mime_result *result = NULL;
		const bu_mime_probe_input input = {data.data(), data.size(), 1, data.size(), 1};
		bool valid = gcv_detect(&context, NULL, &input, &result, NULL) == BU_MIME_MATCH && result->count &&
		    BU_STR_EQUAL(result->candidates[0].name, type);
		if (valid) {
		    const auto &traits = result->candidates[0].traits;
		    valid = BU_STR_EQUAL(bu_mime_traits_get(&traits, "blender.pointer-width"), width.c_str()) &&
			BU_STR_EQUAL(bu_mime_traits_get(&traits, "compression"), compressed ? "gzip" : "none") &&
			BU_STR_EQUAL(bu_mime_traits_get(&traits, "byte-order"), big ? "big-endian" : "little-endian");
		}
		bu_mime_result_free(result);
		gcv_context_destroy(&context);
		if (!valid) {bu_log("Blender observations do not match the actual header\n"); return 1;}
		if (compressed && !gzip_available) continue;
		profile_case sample{type, "blender.pointer-width", width, "16", data, {}};
		if (fixture_test(sample)) return 1;
		const struct {const char *key; const char *value; const char *other; bool selectable;} fields[] = {
		    {"blender.pointer-width", width.c_str(), pointer_size == 4 ? "64" : "32", true},
		    {"byte-order", big ? "big-endian" : "little-endian", big ? "little-endian" : "big-endian", true},
		    {"compression", compressed ? "gzip" : "none", "zstd", false},
		    {"blender.format-version", "0", "1", false}, {"version", "2.50", "9.99", true},
		    {"blender.file-version", "250", "999", true}
		};
		for (const auto &field : fields) {
		    sample.key = field.key; sample.value = field.value; sample.unsupported = field.other;
		    sample.request_selectable = field.selectable;
		    if (fixture_test(sample)) return 1;
		}
	    }
    bytes modern = blender_triangle(8, false);
    constexpr size_t legacy_header_size = 12;
    const std::string header = "BLENDER17-01v0500";
    modern.erase(modern.begin(), modern.begin() + legacy_header_size);
    modern.insert(modern.begin(), header.begin(), header.end());
    profile_case unsupported{type, "blender.format-version", "0", "1", blender_triangle(8, false), modern};
    if (fixture_test(unsupported)) return 1;
    unsupported.invalid_data = gzip_bytes(modern);
    if (fixture_test(unsupported)) return 1;
    gcv_context context;
    gcv_context_init(&context);
    const bu_mime_trait compression = BU_MIME_TRAIT_STRING("compression", "gzip");
    const bu_mime_traits request = {&compression, 1};
    const bool gzip_available = gcv_select_variant(&context, GCV_FILTER_READ, type, &request, "Assetimport Reader", NULL) != NULL;
    const auto other = gzip_bytes(bytes(128, 'x'));
    bu_mime_result *result = NULL;
    bool valid = true;
    for (int complete : {0, 1}) {
	result = NULL;
	const bu_mime_probe_input input = {other.data(), other.size(), complete, other.size(), 1};
	/* Other formats can require later offsets in a partial input. */
	valid = valid && gcv_detect(&context, NULL, &input, &result, NULL) == BU_MIME_UNKNOWN_RESULT && result &&
	    (!complete || !result->incomplete);
	bu_mime_result_free(result);
    }
    bytes padded = gzip_bytes(blender_triangle(8, false));
    /* RFC 1952 permits a comment before the compressed data. The full
     * reader can reach a header that bounded recognition cannot inspect. */
    constexpr size_t flags_at = 3, gzip_header_size = 10;
    constexpr unsigned char comment_flag = 0x10;
    padded[flags_at] |= comment_flag;
    padded.insert(padded.begin() + gzip_header_size, BU_MIME_DEFAULT_SCAN_LIMIT + 1, 'p');
    padded.insert(padded.begin() + gzip_header_size + BU_MIME_DEFAULT_SCAN_LIMIT + 1, 0);
    result = NULL;
    const bu_mime_probe_input long_input = {padded.data(), padded.size(), 1, padded.size(), 1};
    valid = valid && gcv_detect(&context, NULL, &long_input, &result, NULL) == BU_MIME_UNKNOWN_RESULT && result && result->incomplete;
    bu_mime_result_free(result);
    gcv_context_destroy(&context);
    if (!valid) {bu_log("Blender compressed-prefix recognition crossed its inspection boundary\n"); return 1;}
    if (!gzip_available) return 0;
    profile_case full_header{type, "compression", "gzip", "zstd", padded, other};
    full_header.extension = ".blend";
    return fixture_test(full_header);
}

std::string amf_mesh()
{
    return "<object id='0'><mesh><vertices>"
	"<vertex><coordinates><x>0</x><y>0</y><z>0</z></coordinates></vertex>"
	"<vertex><coordinates><x>1</x><y>0</y><z>0</z></coordinates></vertex>"
	"<vertex><coordinates><x>0</x><y>1</y><z>0</z></coordinates></vertex>"
	"</vertices><volume><triangle><v1>0</v1><v2>1</v2><v3>2</v3></triangle>"
	"</volume></mesh></object>";
}

int amf_profiles_test()
{
    const char *type = "model/amf";
    const std::string mesh = amf_mesh();
    const std::string declaration = "<?xml version='1.0'?>";
    const std::string xml = declaration + "<amf version='1.1' unit='millimeter'>" + mesh + "</amf>";
    for (const char *encoding : {"utf-8", "utf-16le", "utf-16be", "utf-32le", "utf-32be"}) {
	profile_case sample{type, "encoding", encoding, "binary", xml_bytes(xml, encoding), {}};
	sample.extension = ".amf";
	if (fixture_test(sample)) return 1;
    }
    const struct {const char *key; const char *value; const char *other;} fields[] = {
	{"container", "none", "zip"}, {"version", "1.1", "9.9"},
	{"amf.unit-declaration", "millimeter", "inch"}
    };
    for (const auto &field : fields) {
	profile_case sample{type, field.key, field.value, field.other, xml_bytes(xml, "utf-8"), {}};
	sample.request_selectable = !BU_STR_EQUAL(field.key, "container");
	sample.extension = ".amf";
	if (fixture_test(sample)) return 1;
    }
    const std::string missing = declaration + "<amf>" + mesh + "</amf>";
    profile_case no_declarations{type, "container", "none", "zip", xml_bytes(missing, "utf-8"), {}};
    no_declarations.unestablished = BU_MIME_TRAIT_STRING("version", "1.0");
    if (fixture_test(no_declarations)) return 1;
    no_declarations.unestablished = BU_MIME_TRAIT_STRING("amf.unit-declaration", "millimeter");
    if (fixture_test(no_declarations)) return 1;
    const std::string padded = declaration + "<!--" + std::string(BU_MIME_DEFAULT_SCAN_LIMIT + 1, 'p') + "-->" +
	"<amf version='1.1' unit='millimeter'>" + mesh + "</amf>";
    const std::string wrapped = declaration + "<wrapper><amf>" + mesh + "</amf></wrapper>";
    profile_case full_header{type, "amf.unit-declaration", "millimeter", "inch", xml_bytes(padded, "utf-8"), xml_bytes(wrapped, "utf-8")};
    full_header.extension = ".amf";
    full_header.request_selectable = true;
    if (fixture_test(full_header)) return 1;
    const bytes data = xml_bytes(xml, "utf-8");
    for (bool compressed : {false, true}) {
	profile_case archive{type, "container", "none", "zip", data, zip_bytes({{"model.amf", data}}, compressed)};
	archive.extension = ".amf";
	if (fixture_test(archive)) return 1;
    }
    return 0;
}

int dxf_profiles_test()
{
    const char *type = "image/vnd.dxf";
    const std::string entities = "0\nSECTION\n2\nENTITIES\n0\n3DFACE\n8\n0\n"
	"10\n0\n20\n0\n30\n0\n11\n1\n21\n0\n31\n0\n12\n0\n22\n1\n32\n0\n"
	"13\n0\n23\n1\n33\n0\n0\nENDSEC\n0\nEOF\n";
    const auto document = [&](const std::string &fields) {
	return "0\nSECTION\n2\nHEADER\n" + fields + "0\nENDSEC\n" + entities;
    };
    const std::string header = "9\n$ACADVER\n1\nAC1027\n9\n$DWGCODEPAGE\n3\nANSI_1252\n";
    const std::string text = document(header);
    const char binary[] = "AutoCAD Binary DXF\r\n\x1a";
    profile_case base{type, "dxf.serialization", "text", "binary", bytes(text.begin(), text.end()),
	bytes(binary, binary + sizeof(binary))};
    if (fixture_test(base)) return 1;
    const struct {const char *key; const char *value; const char *other;} assertions[] = {
	{"dxf.acad-version", "AC1027", "AC9999"}, {"dxf.codepage", "ANSI_1252", "ANSI_932"}
    };
    for (const auto &field : assertions) {
	profile_case sample{type, field.key, field.value, field.other, base.data, {}};
	sample.request_selectable = true;
	sample.unestablished = BU_MIME_TRAIT_STRING("version", "2013");
	const std::string missing = document("");
	sample.invalid_data.assign(missing.begin(), missing.end());
	sample.untyped_may_accept = true;
	if (fixture_test(sample)) return 1;
    }
    const std::string conflict = document(header + "9\n$ACADVER\n1\nAC9999\n");
    profile_case revision{type, "dxf.acad-version", "AC1027", "AC9999", base.data, bytes(conflict.begin(), conflict.end())};
    revision.request_selectable = true;
    revision.untyped_may_accept = true;
    if (fixture_test(revision)) return 1;
    const std::string empty_header = document("999\nProducer comment without a revision\n");
    base.invalid_data.clear();
    base.data.assign(empty_header.begin(), empty_header.end());
    base.unestablished = BU_MIME_TRAIT_STRING("dxf.acad-version", "AC1027");
    if (fixture_test(base)) return 1;
    base.data.assign(entities.begin(), entities.end());
    if (fixture_test(base)) return 1;
    const std::string padded = "999\n" + std::string(BU_MIME_DEFAULT_SCAN_LIMIT + 1, 'p') + "\n" + text;
    base.data.assign(padded.begin(), padded.end());
    base.unestablished = BU_MIME_TRAIT_STRING("version", "2013");
    base.extension = ".dxf";
    if (fixture_test(base)) return 1;
    return 0;
}

int off_profiles_test()
{
    const char *type = "model/vnd.geomview.off";
    const std::string body = "3 1 3\n0 0 0\n1 0 0\n0 1 0\n3 0 1 2\n";
    const std::string plain = "OFF\n" + body;
    const std::string binary = "OFF BINARY\n";
    profile_case base{type, "encoding", "ascii", "binary", bytes(plain.begin(), plain.end()), bytes(binary.begin(), binary.end())};
    if (fixture_test(base)) return 1;
    const std::string commented_headers[] = {"# comment\n" + plain, "OFF # comment\n" + body,
	"OFF\n3 # vertices\n1 3\n0 0 0\n1 0 0\n0 1 0\n3 0 1 2\n"};
    for (const auto &commented : commented_headers) {
	base.invalid_data.assign(commented.begin(), commented.end());
	if (fixture_test(base)) return 1;
    }
    base.invalid_data.clear();
    base.extension = ".off";
    base.native_suffix_required = true;
    base.data.assign(body.begin(), body.end());
    if (fixture_test(base)) return 1;
    for (const auto &text : {plain, body}) {
	const std::string encoded = "\xef\xbb\xbf" + text;
	profile_case bom{type, "encoding", "utf-8", "ascii", bytes(encoded.begin(), encoded.end()), {}};
	bom.extension = ".off";
	bom.native_suffix_required = true;
	bom.request_selectable = true;
	if (fixture_test(bom)) return 1;
    }
    const std::string padded = std::string(BU_MIME_DEFAULT_SCAN_LIMIT + 1, ' ') + plain;
    base.data.assign(padded.begin(), padded.end());
    const std::string late_comment = "OFF\n" + std::string(BU_MIME_DEFAULT_SCAN_LIMIT + 1, ' ') + "# comment\n" + body;
    base.invalid_data.assign(late_comment.begin(), late_comment.end());
    if (fixture_test(base)) return 1;
    for (const char *dimension : {"1", "2", "3"}) {
	const std::string fields = BU_STR_EQUAL(dimension, "1") ? "0\n1\n2\n" :
	    BU_STR_EQUAL(dimension, "2") ? "0 0\n1 0\n0 1\n" : "0 0 0\n1 0 0\n0 1 0\n";
	const std::string text = "nOFF\n" + std::string(dimension) + "\n3 1 3\n" + fields + "3 0 1 2\n";
	const std::string invalid = "nOFF\n4\n" + body;
	profile_case sample{type, "off.dimension", dimension, "4", bytes(text.begin(), text.end()), bytes(invalid.begin(), invalid.end())};
	sample.extension = ".off";
	sample.native_suffix_required = true;
	if (fixture_test(sample)) return 1;
    }
    const std::string attributes = "STCN4OFF\n3 1 3\n"
	"0 0 0 1 0 0 1 1 0 0 1 0 0\n"
	"1 0 0 1 0 0 1 0 1 0 1 1 0\n"
	"0 1 0 1 0 0 1 0 0 1 1 0 1\n3 0 1 2\n";
    profile_case attributed{type, "encoding", "ascii", "binary", bytes(attributes.begin(), attributes.end()), {}};
    attributed.set_key = "off.vertex-attributes";
    attributed.set_members = {"colors", "homogeneous-coordinates", "normals", "texture-coordinates"};
    attributed.extension = ".off";
    attributed.native_suffix_required = true;
    if (fixture_test(attributed)) return 1;
    return 0;
}

bytes ply_triangle(const char *format, const char *version = "1.0", const char *newline = "\n", bool uppercase = false, bool padded = false)
{
    std::string text = uppercase ? "PLY\n" : "ply\n";
    if (padded) text += std::string(BU_MIME_DEFAULT_SCAN_LIMIT + 1, ' ');
    text += std::string("format ") + format + ' ' + version +
	"\nelement vertex 3\nproperty float x\nproperty float y\nproperty float z\n"
	"element face 1\nproperty list uchar int vertex_indices\nend_header\n";
    const bool ascii = BU_STR_EQUAL(format, "ascii");
    if (ascii) text += "0 0 0\n1 0 0\n0 1 0\n3 0 1 2\n";
    bytes data;
    for (char c : text) {
	if (c == '\n') data.insert(data.end(), newline, newline + strlen(newline));
	else data.push_back(c);
    }
    if (!ascii) {
	const bool big = BU_STR_EQUAL(format, "binary_big_endian");
	if (big) append_be_words(data, {0, 0, 0, float_one, 0, 0, 0, float_one, 0});
	else append_words(data, {0, 0, 0, float_one, 0, 0, 0, float_one, 0});
	data.push_back(triangle_vertices);
	if (big) append_be_words(data, {0, 1, 2});
	else append_words(data, {0, 1, 2});
    }
    return data;
}

int ply_profiles_test()
{
    const char *type = "application/x-ply";
    for (const char *format : {"ascii", "binary_little_endian", "binary_big_endian"}) {
	const bytes data = ply_triangle(format);
	const bool ascii = BU_STR_EQUAL(format, "ascii"), big = BU_STR_EQUAL(format, "binary_big_endian");
	profile_case encoding{type, "encoding", ascii ? "ascii" : "binary", "unknown", data, ply_triangle("unsupported")};
	if (ascii) encoding.unestablished = BU_MIME_TRAIT_STRING("byte-order", "little-endian");
	if (fixture_test(encoding)) return 1;
	profile_case version{type, "version", "1.0", "9.9", data, {}};
	version.request_selectable = true;
	if (fixture_test(version)) return 1;
	if (!ascii) {
	    profile_case order{type, "byte-order", big ? "big-endian" : "little-endian", big ? "little-endian" : "big-endian", data, {}};
	    order.request_selectable = true;
	    if (fixture_test(order)) return 1;
	}
	for (const char *newline : {"\r", "\r\n"}) {
	    version.data = ply_triangle(format, "1.0", newline);
	    if (fixture_test(version)) return 1;
	}
    }
    profile_case full_header{type, "version", "9.9", "1.0", ply_triangle("ascii", "9.9", "\n", false, true), {}};
    full_header.request_selectable = true;
    if (fixture_test(full_header)) return 1;
    profile_case uppercase{type, "encoding", "ascii", "binary", ply_triangle("ascii", "1.0", "\n", true), {}};
    uppercase.request_selectable = true;
    return fixture_test(uppercase);
}

bytes stl_triangle(bool binary, bool bom = false)
{
    if (!binary) {
	const std::string text = std::string(bom ? "\xef\xbb\xbf" : "") +
	    "solid triangle\nfacet normal 0 0 1\nouter loop\nvertex 0 0 0\nvertex 1 0 0\nvertex 0 1 0\nendloop\nendfacet\nendsolid triangle\n";
	return bytes(text.begin(), text.end());
    }
    constexpr size_t header_size = 80;
    bytes data(header_size, 0);
    const char header[] = "solid binary STL";
    std::copy_n(header, sizeof(header) - 1, data.begin());
    append_words(data, {1, 0, 0, float_one, 0, 0, 0, float_one, 0, 0, 0, float_one, 0});
    append_words(data, {0}, sizeof(uint16_t));
    return data;
}

int stl_profiles_test()
{
    const char *type = "model/stl";
    for (size_t mode = 0; mode < 3; ++mode) {
	const bool binary = mode == 2, bom = mode == 1;
	gcv_context context;
	gcv_context_init(&context);
	const bu_mime_trait serialization = BU_MIME_TRAIT_STRING("stl.serialization", binary ? "binary" : "text");
	const bu_mime_traits request = {&serialization, 1};
	const bool available = gcv_select_variant(&context, GCV_FILTER_READ, type, &request, "Assetimport Reader", NULL) != NULL;
	gcv_context_destroy(&context);
	if (!available) {if (!binary) return 1; continue;}
	const bytes data = stl_triangle(binary, bom);
	profile_case layout{type, "stl.serialization", binary ? "binary" : "text", "unknown", data, {}};
	if (binary) layout.unestablished = BU_MIME_TRAIT_STRING("stl.utf8-bom", "false");
	if (fixture_test(layout)) return 1;
	profile_case encoding{type, "encoding", binary ? "binary" : bom ? "utf-8" : "ascii", "unestablished", data, {}};
	encoding.request_selectable = true;
	if (fixture_test(encoding)) return 1;
	if (!binary) {
	    profile_case marker{type, "stl.utf8-bom", bom ? "true" : "false", bom ? "false" : "true", data, {}};
	    marker.request_selectable = true;
	    if (fixture_test(marker)) return 1;
	} else {
	    profile_case order{type, "byte-order", "little-endian", "big-endian", data, {}};
	    order.request_selectable = true;
	    if (fixture_test(order)) return 1;
	}
    }
    bytes padded = stl_triangle(false);
    padded.insert(padded.begin(), BU_MIME_DEFAULT_SCAN_LIMIT + 1, ' ');
    profile_case full_header{type, "encoding", "ascii", "binary", padded, {}};
    full_header.request_selectable = true;
    full_header.extension = ".stl";
    full_header.native_suffix_required = true;
    if (fixture_test(full_header)) return 1;
    profile_case invalid_ascii{type, "encoding", "ascii", "binary", stl_triangle(false), {}};
    invalid_ascii.request_selectable = true;
    invalid_ascii.invalid_data = invalid_ascii.data;
    const char keyword[] = "triangle";
    const auto at = std::search(invalid_ascii.invalid_data.begin(), invalid_ascii.invalid_data.end(), keyword, keyword + strlen(keyword));
    invalid_ascii.invalid_data.insert(at, {0xc3, 0xa9});
    invalid_ascii.untyped_may_accept = true;
    return fixture_test(invalid_ascii);
}

bytes three_ds_triangle(bool project = false)
{
    constexpr uint16_t editor_id = 0x3d3d, object_id = 0x4000, mesh_id = 0x4100;
    constexpr uint16_t points_id = 0x4110, faces_id = 0x4120, main_id = 0x4d4d, project_id = 0xc23d;
    const auto chunk = [](uint16_t id, const bytes &payload) {
	bytes data;
	append_short_chunk(data, id, payload, false);
	return data;
    };
    bytes points;
    append_words(points, {triangle_vertices}, sizeof(uint16_t));
    append_words(points, {0, 0, 0, float_one, 0, 0, 0, float_one, 0});
    bytes faces;
    append_words(faces, {1, 0, 1, 2, 0}, sizeof(uint16_t));
    bytes mesh = chunk(points_id, points);
    const auto triangles = chunk(faces_id, faces);
    mesh.insert(mesh.end(), triangles.begin(), triangles.end());
    const unsigned char name[] = "triangle";
    bytes object(name, name + sizeof(name));
    const auto geometry = chunk(mesh_id, mesh);
    object.insert(object.end(), geometry.begin(), geometry.end());
    const auto data = chunk(main_id, chunk(editor_id, chunk(object_id, object)));
    if (!project) return data;
    bytes result = chunk(project_id, {});
    result.insert(result.end(), data.begin(), data.end());
    return result;
}

std::string opengex_triangle()
{
    return "// OpenGEX producer version 3.0 is not a file revision\n"
	"Metric (key = \"distance\") {float {1}}\n"
	"GeometryNode $node {Name {string {\"triangle\"}} ObjectRef {ref {$geometry}}}\n"
	"GeometryObject $geometry {Mesh (primitive = \"triangles\") {"
	"VertexArray (attrib = \"position\") {float[3] {{0,0,0},{1,0,0},{0,1,0}}} "
	"IndexArray {unsigned_int32[3] {{0,1,2}}}}}\n";
}

int opengex_profiles_test()
{
    const char *type = "model/vnd.opengex";
    const auto text = opengex_triangle();
    const bytes ascii(text.begin(), text.end());
    for (bool bom : {false, true}) {
	bytes data = ascii;
	if (bom) data.insert(data.begin(), {0xef, 0xbb, 0xbf});
	profile_case sample{type, "encoding", "utf-8", "ascii", data, {}};
	sample.request_selectable = true;
	sample.unestablished = BU_MIME_TRAIT_STRING("version", "3.0");
	if (fixture_test(sample)) return 1;
	sample.key = "opengex.text-bom"; sample.value = bom ? "true" : "false"; sample.unsupported = bom ? "false" : "true";
	if (fixture_test(sample)) return 1;
	sample.key = "text.code-unit-width"; sample.value = "8"; sample.unsupported = "16";
	sample.request_selectable = false;
	if (fixture_test(sample)) return 1;
	sample.explicit_traits = false;
	if (fixture_test(sample)) return 1;
    }
    const auto unicode = text + "// a late Unicode comment: caf\xc3\xa9\n";
    const bytes unicode_data(unicode.begin(), unicode.end());
    for (bool repertoire : {false, true}) {
	profile_case sample{type, "text.ascii-only", repertoire ? "true" : "false", repertoire ? "false" : "true",
	    repertoire ? ascii : unicode_data, repertoire ? unicode_data : ascii};
	sample.request_selectable = true;
	sample.untyped_may_accept = true;
	if (fixture_test(sample)) return 1;
    }
    const auto padded = "/*" + std::string(BU_MIME_DEFAULT_SCAN_LIMIT + 1, 'p') + "*/\n" + unicode;
    profile_case full{type, "encoding", "utf-8", "binary", bytes(padded.begin(), padded.end()), {}};
    full.extension = ".ogex";
    full.native_suffix_required = true;
    full.request_selectable = true;
    full.untyped_may_accept = true;
    const auto invalid = padded + "// invalid UTF-8 byte: \xff\n";
    full.invalid_data.assign(invalid.begin(), invalid.end());
    if (fixture_test(full)) return 1;
    for (const char *encoding : {"utf-16le", "utf-16be", "utf-32le", "utf-32be"}) {
	profile_case wide{type, "text.code-unit-width", "8", "16", ascii, xml_bytes(padded, encoding)};
	wide.extension = ".ogex";
	if (fixture_test(wide)) return 1;
    }
    return 0;
}

struct fbx_node {
    std::string name;
    std::vector<bytes> properties;
    std::vector<fbx_node> children;
};

bytes fbx_record(const fbx_node &node, bool wide, size_t offset)
{
    const size_t word_size = wide ? sizeof(uint64_t) : sizeof(uint32_t);
    const size_t header_size = 3 * word_size + sizeof(uint8_t);
    bytes data(header_size, 0);
    data.back() = static_cast<unsigned char>(node.name.size());
    data.insert(data.end(), node.name.begin(), node.name.end());
    size_t property_size = 0;
    for (const auto &property : node.properties) {
	data.insert(data.end(), property.begin(), property.end());
	property_size += property.size();
    }
    for (const auto &child : node.children) {
	const auto record = fbx_record(child, wide, offset + data.size());
	data.insert(data.end(), record.begin(), record.end());
    }
    if (!node.children.empty()) data.resize(data.size() + header_size, 0);
    put_word(data, 0, offset + data.size(), word_size);
    put_word(data, word_size, node.properties.size(), word_size);
    put_word(data, 2 * word_size, property_size, word_size);
    return data;
}

bytes fbx_binary_triangle(uint32_t header_version, uint32_t document_version)
{
    const auto integer = [](uint32_t value, bool wide = false) {
	bytes data{static_cast<unsigned char>(wide ? 'L' : 'I')};
	put_word(data, data.size(), value, wide ? sizeof(uint64_t) : sizeof(uint32_t));
	return data;
    };
    const auto string = [](const char *value) {
	bytes data{'S'};
	append_words(data, {static_cast<uint32_t>(strlen(value))});
	data.insert(data.end(), value, value + strlen(value));
	return data;
    };
    const auto array = [](char kind, const bytes &payload, size_t count) {
	bytes data{static_cast<unsigned char>(kind)};
	append_words(data, {static_cast<uint32_t>(count), 0, static_cast<uint32_t>(payload.size())});
	data.insert(data.end(), payload.begin(), payload.end());
	return data;
    };
    bytes positions;
    for (uint64_t word : std::initializer_list<uint64_t>{0, 0, 0, double_one, 0, 0, 0, double_one, 0})
	put_word(positions, positions.size(), word, sizeof(uint64_t));
    bytes indices;
    append_words(indices, {0, 1, static_cast<uint32_t>(-3)});
    const std::vector<fbx_node> nodes = {
	{"FBXHeaderExtension", {}, {{"FBXHeaderVersion", {integer(1003)}, {}}, {"FBXVersion", {integer(document_version)}, {}}}},
	{"Objects", {}, {
	    {"Geometry", {integer(1, true), string("Geometry::triangle"), string("Mesh")}, {
		{"Vertices", {array('d', positions, 9)}, {}}, {"PolygonVertexIndex", {array('i', indices, 3)}, {}}}},
	    {"Model", {integer(2, true), string("Model::triangle"), string("Mesh")}, {{"Version", {integer(232)}, {}}}}
	}},
	{"Connections", {}, {
	    {"C", {string("OO"), integer(1, true), integer(2, true)}, {}},
	    {"C", {string("OO"), integer(2, true), integer(0, true)}, {}}
	}}
    };
    const unsigned char magic[] = "Kaydara FBX Binary  \0\x1a";
    bytes data(magic, magic + sizeof(magic));
    append_words(data, {header_version});
    const bool wide = header_version >= 7500;
    for (const auto &node : nodes) {
	const auto record = fbx_record(node, wide, data.size());
	data.insert(data.end(), record.begin(), record.end());
    }
    data.resize(data.size() + 3 * (wide ? sizeof(uint64_t) : sizeof(uint32_t)) + sizeof(uint8_t), 0);
    return data;
}

std::string fbx_text_triangle(uint32_t version)
{
    return "; FBX 1.0.0 producer comment, not the document version\nFBXHeaderExtension: {\n"
	" FBXHeaderVersion: 1003\n FBXVersion: " + std::to_string(version) + "\n}\n"
	"Objects: {\n Geometry: 1, \"Geometry::triangle\", \"Mesh\" {\n"
	"  Vertices: *9 {\n   a: 0,0,0,1,0,0,0,1,0\n  }\n"
	"  PolygonVertexIndex: *3 {\n   a: 0,1,-3\n  }\n }\n"
	" Model: 2, \"Model::triangle\", \"Mesh\" {\n  Version: 232\n }\n}\n"
	"Connections: {\n C: \"OO\",1,2\n C: \"OO\",2,0\n}\n";
}

class fbx_non_ascii_punctuation : public std::numpunct<char> {
    char do_decimal_point() const override {return static_cast<char>(0xe9);}
};

int fbx_locale_test()
{
    const char *type = "application/vnd.autodesk.fbx";
    const std::locale original;
    gcv_context context;
    gcv_context_init(&context);
    char filename[MAXPATHLEN] = {0};
    FILE *file = bu_temp_file(filename, sizeof(filename));
    int status = 1;
    {
	PROFILE_CHECK(file);
	const int closed = fclose(file);
	file = NULL;
	PROFILE_CHECK(!closed);
	const point_t low = {0, 0, 0}, high = {1, 1, 1};
	const char *objects[] = {"plain.r"};
	gcv_opts options;
	gcv_opts_default(&options);
	options.num_objects = 1;
	options.object_names = objects;
	auto *database = wdb_dbopen(context.dbip, RT_WDB_TYPE_DB_INMEM);
	PROFILE_CHECK(database && !mk_rpp(database, "plain.s", low, high) && !mk_comb1(database, "plain.r", "plain.s", 1));
	const bu_mime_trait value = BU_MIME_TRAIT_STRING("encoding", "ascii");
	const bu_mime_traits request = {&value, 1};
	const auto *writer = gcv_select_variant(&context, GCV_FILTER_WRITE, type, &request, "Assetimport Writer", NULL);
	PROFILE_CHECK(writer && gcv_execute_variant(&context, writer, type, &request, &options, 0, NULL, filename));
	const char marker[] = "Preserve this destination on a charset conflict.\n";
	file = fopen(filename, "wb");
	PROFILE_CHECK(file && fwrite(marker, 1, sizeof(marker) - 1, file) == sizeof(marker) - 1);
	const int saved = fclose(file);
	file = NULL;
	PROFILE_CHECK(!saved);
	std::locale::global(std::locale(std::locale::classic(), new fbx_non_ascii_punctuation));
	const int converted = gcv_execute_variant(&context, writer, type, &request, &options, 0, NULL, filename);
	std::locale::global(original);
	PROFILE_CHECK(!converted);
	file = fopen(filename, "rb");
	char contents[sizeof(marker)];
	PROFILE_CHECK(file && fread(contents, 1, sizeof(contents), file) == sizeof(marker) - 1 &&
	    !memcmp(contents, marker, sizeof(marker) - 1) && feof(file));
	status = 0;
    }
done:
    std::locale::global(original);
    if (file) fclose(file);
    if (*filename) bu_file_delete(filename);
    gcv_context_destroy(&context);
    return status;
}

int obj_output_profiles_test()
{
    gcv_context context;
    gcv_context_init(&context);
    bool valid = true;
    for (const char *materials : {"mtl", "none"}) {
	const bu_mime_trait value = BU_MIME_TRAIT_STRING("obj.materials", materials);
	const bu_mime_traits request = {&value, 1};
	bu_mime_traits resolved = BU_MIME_TRAITS_INIT_ZERO;
	valid = valid && gcv_resolve_variant(&context, GCV_FILTER_WRITE, NULL, "model/obj", &request,
	    "Assetimport Writer", NULL, &resolved, NULL) && !bu_mime_traits_get(&resolved, "encoding") &&
	    BU_STR_EQUAL(bu_mime_traits_get(&resolved, "obj.serialization"), "text") &&
	    BU_STR_EQUAL(bu_mime_traits_get(&resolved, "obj.materials"), materials);
	bu_mime_traits_free(&resolved);
    }
    gcv_context_destroy(&context);
    if (!valid) bu_log("SDK OBJ default text output makes an unverified charset claim\n");
    return valid ? 0 : 1;
}

int fbx_profiles_test()
{
    const char *type = "application/vnd.autodesk.fbx";
    gcv_context discovery;
    gcv_context_init(&discovery);
    bu_mime_traits resolved = BU_MIME_TRAITS_INIT_ZERO;
    const bu_mime_trait text_value = BU_MIME_TRAIT_STRING("fbx.serialization", "text");
    const bu_mime_traits text_request = {&text_value, 1};
    const bool default_text = gcv_resolve_variant(&discovery, GCV_FILTER_WRITE, NULL, type, &text_request,
	"Assetimport Writer", NULL, &resolved, NULL) && !bu_mime_traits_get(&resolved, "encoding") &&
	BU_STR_EQUAL(bu_mime_traits_get(&resolved, "fbx.serialization"), "text");
    bu_mime_traits_free(&resolved);
    gcv_context_destroy(&discovery);
    if (!default_text) {bu_log("SDK FBX default text output makes an unverified charset claim\n"); return 1;}
    if (fbx_locale_test()) return 1;
    const struct {uint32_t code; const char *version;} versions[] = {
	{7100, "7.1.0"}, {7200, "7.2.0"}, {7300, "7.3.0"}, {7400, "7.4.0"}, {7500, "7.5.0"}, {7700, "7.7.0"}
    };
    for (const auto &version : versions)
	for (bool binary : {false, true}) {
	    const auto text = fbx_text_triangle(version.code);
	    profile_case sample{type, "version", version.version, "6.1.0",
		binary ? fbx_binary_triangle(version.code, version.code) : bytes(text.begin(), text.end()), {}};
	    sample.extension = ".fbx";
	    sample.native_suffix_required = true;
	    sample.request_selectable = true;
	    if (version.code == 7100) {
		const auto old = fbx_text_triangle(6100);
		sample.invalid_data = binary ? fbx_binary_triangle(7100, 6100) : bytes(old.begin(), old.end());
	    }
	    if (fixture_test(sample)) return 1;
	}
    for (bool binary : {false, true}) {
	const auto text = fbx_text_triangle(7500);
	profile_case sample{type, "fbx.serialization", binary ? "binary" : "text", binary ? "text" : "binary",
	    binary ? fbx_binary_triangle(7500, 7500) : bytes(text.begin(), text.end()), {}};
	sample.extension = ".fbx";
	sample.request_selectable = true;
	if (binary) sample.data[sizeof("Kaydara FBX Binary  ") - 1] = 'x';
	if (fixture_test(sample)) return 1;
    }
    for (uint32_t header : {7400u, 7500u}) {
	profile_case sample{type, "fbx.binary-header-version-code", std::to_string(header), "7100", fbx_binary_triangle(header, 7500), {}};
	sample.extension = ".fbx";
	sample.native_suffix_required = true;
	sample.request_selectable = true;
	if (fixture_test(sample)) return 1;
	sample.key = "fbx.document-version-code"; sample.value = "7500"; sample.unsupported = "7400";
	if (fixture_test(sample)) return 1;
	sample.key = "version"; sample.value = "7.5.0"; sample.unsupported = "7.4.0";
	if (fixture_test(sample)) return 1;
    }
    for (bool binary : {false, true}) {
	const std::string text = std::string(BU_MIME_DEFAULT_SCAN_LIMIT + 1, ' ') + fbx_text_triangle(9900);
	profile_case sample{type, "fbx.document-version-code", "9900", "7500",
	    binary ? fbx_binary_triangle(9900, 9900) : bytes(text.begin(), text.end()), {}};
	sample.extension = ".fbx";
	sample.request_selectable = true;
	sample.unestablished = BU_MIME_TRAIT_STRING("version", "9.9.0");
	if (fixture_test(sample)) return 1;
	sample.explicit_traits = false;
	if (fixture_test(sample)) return 1;
    }
    const auto text = fbx_text_triangle(7500);
    for (const char *encoding : {"utf-8", "utf-16le", "utf-16be", "utf-32le", "utf-32be"}) {
	profile_case marked{type, "fbx.text-bom", "false", "true", bytes(text.begin(), text.end()), xml_bytes(text, encoding)};
	if (BU_STR_EQUAL(encoding, "utf-8")) marked.invalid_data.insert(marked.invalid_data.begin(), {0xef, 0xbb, 0xbf});
	marked.extension = ".fbx";
	marked.request_selectable = true;
	if (fixture_test(marked)) return 1;
    }
    const std::string padded = std::string(BU_MIME_DEFAULT_SCAN_LIMIT + 1, ' ') + text;
    profile_case ascii{type, "encoding", "ascii", "binary", bytes(padded.begin(), padded.end()), {}};
    ascii.extension = ".fbx";
    ascii.request_selectable = true;
    ascii.untyped_may_accept = true;
    const auto unicode = padded + "; a late UTF-8 comment: caf\xc3\xa9\n";
    ascii.invalid_data.assign(unicode.begin(), unicode.end());
    if (fixture_test(ascii)) return 1;
    ascii.value = "utf-8"; ascii.unsupported = "ascii";
    ascii.data = ascii.invalid_data; ascii.invalid_data.clear();
    return fixture_test(ascii);
}

int three_ds_profiles_test()
{
    for (bool project : {false, true}) {
	profile_case sample{"image/x-3ds", "encoding", "binary", "ascii", three_ds_triangle(project), {}};
	sample.extension = project ? ".prj" : NULL;
	sample.unestablished = BU_MIME_TRAIT_STRING("version", "3");
	sample.invalid_data = sample.data;
	put_word(sample.invalid_data, sizeof(uint16_t), sizeof(uint16_t));
	if (fixture_test(sample)) return 1;
	sample.key = "byte-order"; sample.value = "little-endian"; sample.unsupported = "big-endian";
	if (fixture_test(sample)) return 1;
	sample.explicit_traits = false;
	if (fixture_test(sample)) return 1;
    }
    return 0;
}

int obj_input_profiles_test()
{
    const std::string text = "v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3\n";
    const bytes ascii(text.begin(), text.end());
    for (bool bom : {false, true}) {
	bytes data = ascii;
	if (bom) data.insert(data.begin(), {0xef, 0xbb, 0xbf});
	profile_case sample{"model/obj", "encoding", bom ? "utf-8" : "ascii", "binary", data, {}};
	sample.request_selectable = true;
	sample.unestablished = BU_MIME_TRAIT_STRING("version", "1.0");
	if (fixture_test(sample)) return 1;
	sample.key = "obj.text-bom"; sample.value = bom ? "true" : "false"; sample.unsupported = bom ? "false" : "true";
	if (fixture_test(sample)) return 1;
	sample.explicit_traits = false;
	if (fixture_test(sample)) return 1;
    }
    const auto preamble = "#" + std::string(BU_MIME_DEFAULT_SCAN_LIMIT, ' ') + "\n" + text;
    profile_case padded{"model/obj", "encoding", "ascii", "utf-8", bytes(preamble.begin(), preamble.end()), {}};
    padded.request_selectable = true;
    padded.extension = ".obj";
    padded.native_suffix_required = true;
    if (fixture_test(padded)) return 1;
    const auto late_unicode = text + "#" + std::string(BU_MIME_DEFAULT_SCAN_LIMIT, ' ') + "caf\xc3\xa9\n";
    profile_case unicode{"model/obj", "encoding", "utf-8", "ascii", bytes(late_unicode.begin(), late_unicode.end()), {}};
    unicode.request_selectable = true;
    if (fixture_test(unicode)) return 1;
    unicode.key = "text.ascii-only"; unicode.value = "false"; unicode.unsupported = "true";
    if (fixture_test(unicode)) return 1;
    const auto late_invalid = text + "#" + std::string(BU_MIME_DEFAULT_SCAN_LIMIT, ' ') + "bad \xff\n";
    profile_case invalid{"model/obj", "encoding", "ascii", "binary", ascii, bytes(late_invalid.begin(), late_invalid.end())};
    invalid.untyped_may_accept = true;
    invalid.request_selectable = true;
    if (fixture_test(invalid)) return 1;
    profile_case marked_invalid = invalid;
    marked_invalid.value = "utf-8";
    marked_invalid.data.insert(marked_invalid.data.begin(), {0xef, 0xbb, 0xbf});
    marked_invalid.invalid_data.insert(marked_invalid.invalid_data.begin(), {0xef, 0xbb, 0xbf});
    if (fixture_test(marked_invalid)) return 1;
    invalid.invalid_data.clear(); invalid.data.assign(late_invalid.begin(), late_invalid.end());
    invalid.explicit_traits = false;
    if (fixture_test(invalid)) return 1;
    const auto material = text + "#" + std::string(BU_MIME_DEFAULT_SCAN_LIMIT, ' ') + "\nmtllib missing-sidecar.mtl\n";
    profile_case reference{"model/obj", "obj.material-library-reference", "true", "false", bytes(material.begin(), material.end()), {}};
    reference.request_selectable = true;
    if (fixture_test(reference)) return 1;
    reference.value = "false"; reference.unsupported = "true"; reference.data = ascii;
    if (fixture_test(reference)) return 1;
    for (const char *encoding : {"utf-16le", "utf-16be", "utf-32le", "utf-32be"}) {
	profile_case wide{"model/obj", "encoding", "ascii", encoding, ascii, xml_bytes(text, encoding)};
	wide.request_selectable = true;
	if (fixture_test(wide)) return 1;
    }
    return 0;
}

int xml_profiles_test()
{
    const std::string open_mesh = "<?xml version='1.0'?><mesh xmlns='http://irrlicht.sourceforge.net/IRRMESH_09_2007' version='1.0'>";
    const std::string cube = "<node type='cube'><attributes><string name='Name' value='cube'/><float name='Size' value='1'/></attributes>"
	"<materials><attributes><enum name='Type' value='solid'/></attributes></materials></node>";
    for (const char *encoding : {"utf-8", "utf-16le", "utf-16be", "utf-32le", "utf-32be"}) {
	std::string padding;
	if (BU_STR_EQUAL(encoding, "utf-8")) padding = "<!--" + std::string(BU_MIME_DEFAULT_SCAN_LIMIT, ' ') + "-->";
	for (const char *layout : {"standard", "2tcoords", "tangents"}) {
	    std::string xml = open_mesh + irrlicht_buffer(layout) + "</mesh>";
	    std::string invalid = open_mesh + irrlicht_buffer(layout) + padding + irrlicht_buffer("unsupported") + "</mesh>";
	    profile_case sample{"model/vnd.irrlicht.mesh+xml", "encoding", encoding, "binary", xml_bytes(xml, encoding), xml_bytes(invalid, encoding)};
	    sample.extension = ".irrmesh";
	    sample.set_key = "irrlicht.vertex-layouts";
	    sample.set_members = {layout};
	    if (fixture_test(sample)) return 1;
	}
	std::string scene = "<?xml version='1.0'?><irr_scene>" + cube + "</irr_scene>";
	std::string invalid = "<?xml version='1.0'?><irr_scene>" + cube + padding + "<node type='terrain'/></irr_scene>";
	profile_case sample{"model/vnd.irrlicht.scene+xml", "encoding", encoding, "binary", xml_bytes(scene, encoding), xml_bytes(invalid, encoding)};
	sample.extension = ".irr";
	sample.set_key = "irrlicht.node-types";
	sample.set_members = {"cube"};
	sample.faces = 12;
	sample.vertices = 8;
	if (fixture_test(sample)) return 1;
	for (const char *version : {"1.4.1", "1.5.0"}) {
	    std::string xml = collada_triangle(version);
	    std::string future = xml;
	    size_t declared = future.find(std::string("version='") + version);
	    future.replace(declared + strlen("version='"), strlen(version), "2.0.0");
	    profile_case collada{"model/vnd.collada+xml", "encoding", encoding, "binary", xml_bytes(xml, encoding), xml_bytes(future, encoding)};
	    collada.extension = ".dae";
	    if (fixture_test(collada)) return 1;
	}
    }
    const std::string native = open_mesh + irrlicht_buffer("standard") + "</mesh>";
    const std::string scene = "<irr_scene>" + cube + "</irr_scene>";
    std::string padded = native;
    padded.insert(padded.find("<mesh"), "<!--" + std::string(BU_MIME_DEFAULT_SCAN_LIMIT, ' ') + "-->");
    profile_case large_header{"model/vnd.irrlicht.mesh+xml", "encoding", "utf-8", "binary", xml_bytes(padded, "utf-8"), {}};
    large_header.extension = ".irrmesh";
    if (fixture_test(large_header)) return 1;
    std::string prefixed = collada_triangle("1.4.1");
    prefixed.insert(prefixed.find("<COLLADA") + 1, "c:");
    prefixed.insert(prefixed.find("</COLLADA") + 2, "c:");
    prefixed.insert(prefixed.find(" version='1.4.1'"), " xmlns:c='http://www.collada.org/2005/11/COLLADASchema'");
    profile_case qualified{"model/vnd.collada+xml", "encoding", "utf-8", "binary", xml_bytes(collada_triangle("1.4.1"), "utf-8"), xml_bytes(prefixed, "utf-8")};
    qualified.extension = ".dae";
    if (fixture_test(qualified)) return 1;
    profile_case missing_material{"model/vnd.irrlicht.scene+xml", "encoding", "utf-8", "binary", xml_bytes(scene, "utf-8"),
	xml_bytes("<irr_scene><node type='cube'/></irr_scene>", "utf-8")};
    missing_material.extension = ".irr";
    missing_material.faces = 12;
    missing_material.vertices = 8;
    if (fixture_test(missing_material)) return 1;
    for (const auto &sample : std::vector<profile_case>{
	{"model/vnd.irrlicht.mesh+xml", "version", "1.0", "2.0", xml_bytes(native, "utf-8"), {}},
	{"model/vnd.irrlicht.scene+xml", "encoding", "utf-8", "binary", xml_bytes(scene, "utf-8"), {}},
	{"model/vnd.collada+xml", "version", "1.4.1", "1.5.0", xml_bytes(collada_triangle("1.4.1"), "utf-8"), {}}}) {
	profile_case generic = sample;
	generic.extension = ".xml";
	generic.request_selectable = !BU_STR_EQUAL(generic.key, "encoding");
	if (BU_STR_EQUAL(generic.type, "model/vnd.irrlicht.scene+xml")) {generic.faces = 12; generic.vertices = 8;}
	if (fixture_test(generic)) return 1;
    }
    return 0;
}

int x3d_profiles_test()
{
    const std::string body = "<Scene><Shape><IndexedFaceSet coordIndex='0 1 2 -1'>"
	"<Coordinate point='0 0 0 1 0 0 0 1 0'/></IndexedFaceSet></Shape></Scene>";
    const std::string declaration = "<?xml version='1.0'?>";
    const std::string root = "<X3D version='3.3' profile='Interchange'>";
    const std::string xml = declaration + root + body + "</X3D>";
    for (const char *encoding : {"utf-8", "utf-16le", "utf-16be", "utf-32le", "utf-32be"}) {
	profile_case sample{"model/x3d+xml", "encoding", encoding, "binary", xml_bytes(xml, encoding),
	    xml_bytes(declaration + "<X3D xmlns='unrelated'>" + body + "</X3D>", encoding)};
	sample.extension = ".x3d";
	if (fixture_test(sample)) return 1;
    }
    /* The SDK does not branch on the declared scene version or profile.
     * These are still exact file assertions, checked before conversion. */
    for (const char *version : {"3.0", "3.1", "3.2", "3.3", "4.0"}) {
	std::string versioned = declaration + "<X3D version='" + version + "' profile='Interchange'>" + body + "</X3D>";
	profile_case sample{"model/x3d+xml", "version", version, "5.0", xml_bytes(versioned, "utf-8"), {}};
	sample.extension = ".x3d";
	sample.request_selectable = true;
	if (fixture_test(sample)) return 1;
    }
    profile_case named_profile{"model/x3d+xml", "x3d.profile", "Interchange", "Full", xml_bytes(xml, "utf-8"), {}};
    named_profile.extension = ".x3d";
    named_profile.request_selectable = true;
    if (fixture_test(named_profile)) return 1;
    profile_case namespaced{"model/x3d+xml", "version", "3.3", "4.0",
	xml_bytes(declaration + "<X3D xmlns='http://www.web3d.org/specifications/x3d-namespace' version='3.3'>" + body + "</X3D>", "utf-8"), {}};
    namespaced.extension = ".x3d";
    namespaced.request_selectable = true;
    if (fixture_test(namespaced)) return 1;
    std::string prolog = declaration + "<!DOCTYPE X3D PUBLIC 'ISO//Web3D//DTD X3D 3.3//EN' 'http://www.web3d.org/specifications/x3d-3.3.dtd'>";
    prolog += "<!--" + std::string(BU_MIME_DEFAULT_SCAN_LIMIT, ' ') + "-->";
    profile_case full_header{"model/x3d+xml", "version", "3.3", "4.0", xml_bytes(prolog + root + body + "</X3D>", "utf-8"),
	xml_bytes(declaration + "<wrapper>" + root + body + "</X3D></wrapper>", "utf-8")};
    full_header.extension = ".x3d";
    full_header.request_selectable = true;
    return fixture_test(full_header);
}

std::string xgl_environment()
{
    return "<BACKGROUND><BACKCOLOR>1,1,1</BACKCOLOR></BACKGROUND><LIGHTING><AMBIENT>1,1,1</AMBIENT></LIGHTING>";
}

std::string xgl_mesh()
{
    return "<MESH><MAT ID='0'><DIFF>1,0,0</DIFF></MAT>"
	"<P ID='0'>0,0,0</P><P ID='1'>1,0,0</P><P ID='2'>0,1,0</P>"
	"<F><MATREF>0</MATREF><FV1><PREF>0</PREF></FV1><FV2><PREF>1</PREF></FV2><FV3><PREF>2</PREF></FV3></F></MESH>";
}

int xgl_profiles_test()
{
    const std::string declaration = "<?xml version='1.0'?>";
    const std::string environment = xgl_environment();
    const std::string mesh = xgl_mesh();
    const std::string xml = declaration + "<WORLD>" + environment + mesh + "</WORLD>";
    for (bool compressed : {false, true}) {
	const char *type = compressed ? "model/vnd.realitywave.zgl" : "model/vnd.realitywave.xgl+xml";
	for (const char *encoding : {"utf-8", "utf-16le", "utf-16be", "utf-32le", "utf-32be"}) {
	    bytes data = xml_bytes(xml, encoding);
	    bytes invalid = xml_bytes(declaration + "<wrapper><WORLD>" + environment + mesh + "</WORLD></wrapper>", encoding);
	    if (compressed) {data = zlib_bytes(data); invalid = zlib_bytes(invalid);}
	    profile_case sample{type, compressed ? "xml.encoding" : "encoding", encoding, "unsupported", data, invalid};
	    sample.extension = compressed ? ".zgl" : ".xgl";
	    sample.native_suffix_required = compressed;
	    if (fixture_test(sample)) return 1;
	    gcv_context context;
	    gcv_context_init(&context);
	    bu_mime_result *result = NULL;
	    bu_mime_probe_input input = {data.data(), data.size(), 1, data.size(), 1};
	    bool valid = gcv_detect(&context, NULL, &input, &result, NULL) == BU_MIME_MATCH &&
		result->count && BU_STR_EQUAL(result->candidates[0].name, type);
	    if (valid) {
		const auto &observed = result->candidates[0].traits;
		const char *charset = bu_mime_traits_get(&observed, compressed ? "xml.encoding" : "encoding");
		const char *compression = bu_mime_traits_get(&observed, "compression");
		valid = charset && BU_STR_EQUAL(charset, encoding) && compression &&
		    BU_STR_EQUAL(compression, compressed ? "zlib" : "none") && !bu_mime_traits_get(&observed, "version");
	    }
	    bu_mime_result_free(result);
	    gcv_context_destroy(&context);
	    if (!valid) {bu_log("XGL observation check failed for %s, %s\n", type, encoding); return 1;}
	}
	std::string long_prolog = declaration + "<!--" + std::string(BU_MIME_DEFAULT_SCAN_LIMIT, ' ') + "--><WORLD>" + environment + mesh + "</WORLD>";
	bytes data = xml_bytes(long_prolog, "utf-8");
	if (compressed) data = zlib_bytes(data);
	profile_case full_header{type, "compression", compressed ? "zlib" : "none", "gzip", data, {}};
	full_header.extension = compressed ? ".zgl" : ".xgl";
	full_header.native_suffix_required = compressed;
	if (fixture_test(full_header)) return 1;
	gcv_context context;
	gcv_context_init(&context);
	bu_mime_result *result = NULL;
	bu_mime_probe_input input = {data.data(), data.size(), 1, data.size(), 1};
	bool limited = gcv_detect(&context, NULL, &input, &result, NULL) == BU_MIME_UNKNOWN_RESULT && result && result->incomplete;
	bu_mime_result_free(result);
	gcv_context_destroy(&context);
	if (!limited) {bu_log("XGL prefix exceeded its inspection window\n"); return 1;}
    }
    gcv_context context;
    gcv_context_init(&context);
    bool valid = true;
    for (const std::string &other : {std::string("<WORLD/>"), std::string("<WORLD><!--<BACKGROUND><BACKCOLOR/></BACKGROUND><LIGHTING/>--></WORLD>"), std::string("<other/>")})
	for (bool compressed : {false, true}) {
	    bytes data = xml_bytes(other, "utf-8");
	    if (compressed) data = zlib_bytes(data);
	    bu_mime_result *result = NULL;
	    bu_mime_probe_input input = {data.data(), data.size(), 1, data.size(), 1};
	    valid = valid && gcv_detect(&context, NULL, &input, &result, NULL) == BU_MIME_UNKNOWN_RESULT;
	    if (result) valid = valid && !result->incomplete;
	    bu_mime_result_free(result);
	}
    gcv_context_destroy(&context);
    if (!valid) bu_log("Generic XML/zlib was mistaken for XGL\n");
    return valid ? 0 : 1;
}

int ogre_profiles_test()
{
    const char *type = "model/vnd.ogre.mesh";
    profile_case binary{type, "version", "1.8", "1.100", ogre_triangle("1.8"), ogre_triangle("1.100")};
    binary.extension = ".mesh";
    binary.native_suffix_required = true;
    if (fixture_test(binary)) return 1;
    profile_case endian{type, "byte-order", "little-endian", "big-endian", ogre_triangle("1.8"), ogre_triangle("1.8", true)};
    endian.extension = ".mesh";
    if (fixture_test(endian)) return 1;
    const std::string geometry = "<sharedgeometry vertexcount='3'><vertexbuffer positions='true'>"
	"<vertex><position x='0' y='0' z='0'/></vertex><vertex><position x='1' y='0' z='0'/></vertex>"
	"<vertex><position x='0' y='1' z='0'/></vertex></vertexbuffer></sharedgeometry>";
    const std::string submeshes = "<submeshes><submesh usesharedvertices='true'><faces count='1'>"
	"<face v1='0' v2='1' v3='2'/></faces></submesh></submeshes>";
    const std::string xml = "<?xml version='1.0'?><mesh>" + geometry + submeshes + "</mesh>";
    for (const char *encoding : {"utf-8", "utf-16le", "utf-16be", "utf-32le", "utf-32be"}) {
	profile_case sample{"model/vnd.ogre.mesh+xml", "encoding", encoding, "binary", xml_bytes(xml, encoding),
	    xml_bytes("<wrapper><mesh>" + geometry + submeshes + "</mesh></wrapper>", encoding)};
	sample.extension = ".mesh.xml";
	sample.native_suffix_required = true;
	if (fixture_test(sample)) return 1;
    }
    gcv_context context;
    gcv_context_init(&context);
    bool valid = true;
    for (const std::string &source : {xml, std::string("<mesh/>"), std::string("<mesh><!--<submeshes><submesh/></submeshes>--></mesh>")}) {
	bu_mime_result *result = NULL;
	bu_mime_probe_input input = {reinterpret_cast<const unsigned char *>(source.data()), source.size(), 1, source.size(), 1};
	bool positive = source == xml;
	bu_mime_status status = gcv_detect(&context, NULL, &input, &result, NULL);
	valid = valid && status == (positive ? BU_MIME_MATCH : BU_MIME_UNKNOWN_RESULT);
	if (positive && result && result->count)
	    valid = valid && BU_STR_EQUAL(result->candidates[0].name, "model/vnd.ogre.mesh+xml") &&
		!bu_mime_traits_get(&result->candidates[0].traits, "version");
	bu_mime_result_free(result);
    }
    gcv_context_destroy(&context);
    if (!valid) bu_log("Ogre XML structural observations failed\n");
    return valid ? 0 : 1;
}

int pk3_profiles_test()
{
    const char *type = "application/vnd.idsoftware.pk3+zip";
    gcv_context context;
    gcv_context_init(&context);
    bool valid = !gcv_select_filter(&context, GCV_FILTER_READ, "model/vnd.idsoftware.bsp", "Assetimport Reader", NULL);
    bool native = bu_byteorder() == BU_LITTLE_ENDIAN && sizeof(float) == sizeof(uint32_t) &&
	sizeof(int) == sizeof(uint32_t) && std::numeric_limits<float>::is_iec559;
    if (!native) valid = valid && !gcv_select_filter(&context, GCV_FILTER_READ, type, "Assetimport Reader", NULL);
    gcv_context_destroy(&context);
    if (!valid) {bu_log("Incorrect raw BSP/native PK3 capability\n"); return 1;}
    if (!native) return 0;
    /* Supply the referenced texture: this SDK leaves a non-null empty
     * texture array when every texture is external, failing validation. */
    const bytes texture = {0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 24, 0, 0, 0, 255}; // 1x1 RGB TGA.
    for (bool compressed : {false, true})
	for (const char *extension : {".pk3", ".bsp"}) {
	    const bytes readme = {'n', 'o', 't', 'e', 's'};
	    bytes data = zip_bytes({{"readme.txt", readme}, {"maps/triangle.bsp", bsp_triangle()}, {"textures/fixture.tga", texture}}, compressed);
	    bytes invalid = zip_bytes({{"readme.txt", readme}}, compressed);
	    profile_case sample{type, "container", "zip", "rar", data, invalid};
	    sample.extension = extension;
	    sample.native_suffix_required = true;
	    if (fixture_test(sample)) return 1;
	    const std::string preamble = "archive preamble\n";
	    data.insert(data.begin(), preamble.begin(), preamble.end());
	    sample.data = data;
	    if (fixture_test(sample)) return 1;
	}
    return 0;
}

int ifc_profiles_test(const char *filename)
{
    std::ifstream stream(filename, std::ios::binary);
    if (!stream.is_open()) {bu_log("Cannot open IFC fixture\n"); return 1;}
    const std::string source{std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
    if (stream.bad()) return 1;
    const std::string declaration = "FILE_SCHEMA(('IFC2X3'));";
    const size_t schema = source.find(declaration);
    if (schema == std::string::npos) return 1;
    const bytes data(source.begin(), source.end());
    auto changed_schema = [&](const std::string &replacement) {
	std::string changed = source;
	changed.replace(schema, declaration.size(), replacement);
	return bytes(changed.begin(), changed.end());
    };
    profile_case sample{"application/p21", "p21.schema", "IFC2X3", "IFC4", data, {}};
    sample.faces = 4;
    sample.vertices = 4;
    sample.request_selectable = true;
    /* A schema declaration does not establish a body character encoding. */
    sample.unestablished = BU_MIME_TRAIT_STRING("encoding", "ascii");
    for (const char *extension : {static_cast<const char *>(NULL), ".ifc", ".step", ".stp"}) {
	auto named = sample;
	named.extension = extension;
	named.invalid_data = changed_schema("FILE_SCHEMA(('IFC4'));");
	named.set_key = "p21.schemas";
	named.set_members = {"IFC2X3"};
	if (fixture_test(named)) return 1;
    }
    for (const char *declaration_text : {"FILE_SCHEMA(('IFC2X3','IFC4'));",
	"FILE_SCHEMA(('IFC2X3','CONFIG_CONTROL_DESIGN'));", "FILE_SCHEMA(('IFC2X2'));"}) {
	/* Header substitutions test rejection, not support for another schema. */
	sample.invalid_data = changed_schema(declaration_text);
	if (fixture_test(sample)) return 1;
    }
    std::string padding = "/* FILE_SCHEMA(('IFC4')); " + std::string(BU_MIME_DEFAULT_SCAN_LIMIT + 1, ' ') + "*/\n";
    sample.data = changed_schema(padding + declaration);
    sample.invalid_data = changed_schema(padding + "FILE_SCHEMA(('IFC4'));");
    return fixture_test(sample);
}

bytes three_mf_package(const char *encoding, bool compressed, const char *root = "3D/3dmodel.model", bool zip64 = false)
{
    const std::string model = "<model xmlns='http://schemas.microsoft.com/3dmanufacturing/core/2015/02' unit='millimeter'>"
	"<resources><object id='1' type='model'><mesh><vertices><vertex x='0' y='0' z='0'/>"
	"<vertex x='1' y='0' z='0'/><vertex x='0' y='1' z='0'/></vertices>"
	"<triangles><triangle v1='0' v2='1' v3='2'/></triangles></mesh></object></resources>"
	"<build><item objectid='1'/></build></model>";
    const std::string relationships = "<Relationships xmlns='http://schemas.openxmlformats.org/package/2006/relationships'>"
	"<Relationship Target='/" + std::string(root) + "' Id='model' "
	"Type='http://schemas.microsoft.com/3dmanufacturing/2013/01/3dmodel'/></Relationships>";
    const std::string types = "<Types xmlns='http://schemas.openxmlformats.org/package/2006/content-types'>"
	"<Default Extension='rels' ContentType='application/vnd.openxmlformats-package.relationships+xml'/>"
	"<Override PartName='/" + std::string(root) + "' ContentType='application/vnd.ms-package.3dmanufacturing-3dmodel+xml'/></Types>";
    const std::string readme = "A leading non-model entry does not declare the package's inner XML encoding or revision.";
    return zip_bytes({{"readme.txt", bytes(readme.begin(), readme.end())},
	{"_rels/.rels", bytes(relationships.begin(), relationships.end())},
	{"[Content_Types].xml", bytes(types.begin(), types.end())}, {root, xml_bytes(model, encoding)}}, compressed, zip64);
}

int three_mf_profiles_test()
{
    for (bool compressed : {false, true})
	for (const char *encoding : {"utf-8", "utf-16le", "utf-16be", "utf-32le", "utf-32be"}) {
	    profile_case sample{"model/3mf", "container", "zip", "gzip", three_mf_package(encoding, compressed), {}};
	    sample.unestablished = BU_MIME_TRAIT_STRING("3mf.model-encoding", encoding);
	    if (fixture_test(sample)) return 1;
	}
    for (bool zip64 : {false, true}) {
	profile_case sample{"model/3mf", "encoding", "binary", "utf-8", three_mf_package("utf-8", true, "3D/3dmodel.model", zip64), {}};
	if (!zip64) {
	    const std::string preamble = "<?xml version='1.0'?><model/>" + std::string(BU_MIME_DEFAULT_SCAN_LIMIT + 1, ' ');
	    sample.data.insert(sample.data.begin(), preamble.begin(), preamble.end());
	    const char trailer[] = "SDK-compatible archive trailer";
	    sample.data.insert(sample.data.end(), trailer, trailer + sizeof(trailer) - 1);
	}
	sample.unestablished = BU_MIME_TRAIT_STRING("version", "1.0");
	if (fixture_test(sample)) return 1;
    }
    profile_case custom{"model/3mf", "container", "zip", "none", three_mf_package("utf-8", true, "models/triangle.model"), {}};
    custom.extension = ".3mf";
    custom.native_suffix_required = true;
    if (fixture_test(custom)) return 1;
    custom.explicit_traits = false;
    if (fixture_test(custom)) return 1;
    const std::string plain = "<model xmlns='http://schemas.microsoft.com/3dmanufacturing/core/2015/02'/>";
    profile_case invalid{"model/3mf", "encoding", "binary", "ascii", three_mf_package("utf-8", false), bytes(plain.begin(), plain.end())};
    invalid.extension = ".3mf";
    return fixture_test(invalid);
}

int zae_profiles_test()
{
    const std::string manifest = "<manifest><dae_root>models/mesh%20entry.dae</dae_root></manifest>";
    const bytes manifest_bytes(manifest.begin(), manifest.end());
    const std::string readme = "A leading entry is not the root COLLADA document.";
    const std::vector<std::pair<std::string, bytes>> empty = {{"readme.txt", bytes(readme.begin(), readme.end())}};
    auto package = [&](const char *version, const char *encoding, bool compressed, bool zip64 = false) {
	return zip_bytes({empty[0], {"models/mesh entry.dae", xml_bytes(collada_triangle(version), encoding)},
	    {"manifest.xml", manifest_bytes}}, compressed, zip64);
    };
    for (const char *version : {"1.4.1", "1.5.0"})
	for (bool compressed : {false, true})
	    for (const char *encoding : {"utf-8", "utf-16le", "utf-16be", "utf-32le", "utf-32be"}) {
		profile_case sample{"model/vnd.collada+xml+zip", "collada.document-version", version,
		    BU_STR_EQUAL(version, "1.4.1") ? "1.5.0" : "1.4.1", package(version, encoding, compressed), zip_bytes(empty, compressed)};
		sample.extension = ".zae";
		sample.request_selectable = true;
		sample.native_suffix_required = true;
		if (fixture_test(sample)) return 1;
	    }
    for (bool zip64 : {false, true}) {
	profile_case sample{"model/vnd.collada+xml+zip", "container", "zip", "rar", package("1.4.1", "utf-8", true, zip64), {}};
	sample.extension = ".zae";
	sample.native_suffix_required = true;
	if (!zip64) {
	    const std::string preamble = "<?xml version='1.0'?><COLLADA version='1.5.0'/>";
	    sample.data.insert(sample.data.begin(), preamble.begin(), preamble.end());
	    const std::string trailing = "trailing SDK-compatible bytes";
	    sample.data.insert(sample.data.end(), trailing.begin(), trailing.end());
	}
	if (fixture_test(sample)) return 1;
    }
    const std::string xml = collada_triangle("1.4.1");
    profile_case plain{"model/vnd.collada+xml+zip", "container", "zip", "rar", package("1.4.1", "utf-8", false), xml_bytes(xml, "utf-8")};
    plain.extension = ".zae";
    plain.untyped_may_accept = true;
    if (fixture_test(plain)) return 1;
    profile_case future{"model/vnd.collada+xml+zip", "collada.schema-version", "1.4", "2.0", package("1.4.1", "utf-8", false), package("2.0.0", "utf-8", false)};
    future.extension = ".zae";
    if (fixture_test(future)) return 1;
    const bytes no_manifest = zip_bytes({{"only.dae", xml_bytes(xml, "utf-8")}}, true);
    profile_case legacy{"model/vnd.collada+xml+zip", "encoding", "binary", "utf-8", no_manifest, {}};
    legacy.extension = ".zae";
    /* An inner document revision must not become a package revision. */
    legacy.unestablished = BU_MIME_TRAIT_STRING("version", "1.4.1");
    return fixture_test(legacy);
}

bytes gltf_triangle(bool modern, bool binary, const std::string &fields = "")
{
    const std::string uri = binary ? "" : R"(,"uri":"data:application/octet-stream;base64,AAAAAAAAAAAAAAAAAACAPwAAAAAAAAAAAAAAAAAAgD8AAAAAAAABAAIA")";
    std::string json = "{" + fields;
    if (modern) {
	json += R"("asset":{"version":"2.0"},"buffers":[{"byteLength":42)" + uri + R"(}],
"bufferViews":[{"buffer":0,"byteLength":36},{"buffer":0,"byteOffset":36,"byteLength":6}],
"accessors":[{"bufferView":0,"componentType":5126,"count":3,"type":"VEC3","min":[0,0,0],"max":[1,1,0]},
{"bufferView":1,"componentType":5123,"count":3,"type":"SCALAR"}],
"meshes":[{"primitives":[{"attributes":{"POSITION":0},"indices":1,"mode":4}]}],
"nodes":[{"mesh":0}],"scenes":[{"nodes":[0]}],"scene":0})";
    } else {
	if (binary) json += R"("extensionsUsed":["KHR_binary_glTF"],)";
	json += R"("asset":{"version":"1.0"},"buffers":{"binary_glTF":{"byteLength":42,"type":"arraybuffer")" + uri + R"(}},
"bufferViews":{"positions":{"buffer":"binary_glTF","byteLength":36},"indices":{"buffer":"binary_glTF","byteOffset":36,"byteLength":6}},
"accessors":{"positions":{"bufferView":"positions","componentType":5126,"count":3,"type":"VEC3","min":[0,0,0],"max":[1,1,0]},
"indices":{"bufferView":"indices","componentType":5123,"count":3,"type":"SCALAR"}},
"meshes":{"mesh":{"primitives":[{"attributes":{"POSITION":"positions"},"indices":"indices","mode":4}]}},
"nodes":{"node":{"meshes":["mesh"]}},"scenes":{"scene":{"nodes":["node"]}},"scene":"scene"})";
    }
    if (!binary) return bytes(json.begin(), json.end());
    constexpr size_t alignment = sizeof(uint32_t);
    json.append((alignment - json.size() % alignment) % alignment, ' ');
    bytes body;
    append_words(body, {0, 0, 0, float_one, 0, 0, 0, float_one, 0});
    append_words(body, {0, 1, 2}, sizeof(uint16_t));
    body.resize((body.size() + alignment - 1) / alignment * alignment, 0);
    constexpr uint32_t magic = 0x46546c67, json_chunk = 0x4e4f534a, bin_chunk = 0x004e4942;
    const uint32_t header_size = modern ? 28 : 20;
    bytes data;
    append_words(data, {magic, modern ? 2u : 1u, header_size + static_cast<uint32_t>(json.size() + body.size()),
	static_cast<uint32_t>(json.size()), modern ? json_chunk : 0u});
    data.insert(data.end(), json.begin(), json.end());
    if (modern) append_words(data, {static_cast<uint32_t>(body.size()), bin_chunk});
    data.insert(data.end(), body.begin(), body.end());
    return data;
}

int gltf_profiles_test()
{
    if (bu_byteorder() != BU_LITTLE_ENDIAN || sizeof(float) != sizeof(uint32_t) || !std::numeric_limits<float>::is_iec559) {
	gcv_context context;
	gcv_context_init(&context);
	bool absent = !gcv_select_filter(&context, GCV_FILTER_READ, "model/gltf+json", "Assetimport Reader", nullptr) &&
	    !gcv_select_filter(&context, GCV_FILTER_WRITE, "model/gltf-binary", "Assetimport Writer", nullptr);
	gcv_context_destroy(&context);
	return absent ? 0 : 1;
    }
    for (bool modern : {false, true})
	for (bool binary : {false, true}) {
	    profile_case sample{binary ? "model/gltf-binary" : "model/gltf+json", "version", modern ? "2.0" : "1.0",
		modern ? "1.0" : "2.0", gltf_triangle(modern, binary), {}};
	    sample.request_selectable = true;
	    if (fixture_test(sample)) return 1;
	}
    const std::string unknown = R"("extensionsUsed":["EXT_unimplemented"],"extensionsRequired":["EXT_unimplemented"],)";
    const std::string unlit = R"("extensionsUsed":["KHR_materials_unlit"],"extensionsRequired":["KHR_materials_unlit"],)";
    profile_case required{"model/gltf+json", "version", "2.0", "3.0", gltf_triangle(true, false, unlit), gltf_triangle(true, false, unknown)};
    required.set_key = "gltf.required-extensions";
    required.set_members = {"KHR_materials_unlit"};
    if (fixture_test(required)) return 1;
    profile_case avatar{"model/gltf-binary", "vrm.version", "1.0", "99",
	gltf_triangle(true, true, R"("extensionsUsed":["VRMC_vrm"],"extensions":{"VRMC_vrm":{"specVersion":"1.0"}},)"), {}};
    avatar.request_selectable = true;
    if (fixture_test(avatar)) return 1;
    gcv_context context;
    gcv_context_init(&context);
    const char *extension[] = {"KHR_draco_mesh_compression"};
    const bu_mime_trait value = BU_MIME_TRAIT_SET("gltf.required-extensions", extension, 1);
    const bu_mime_traits traits = {&value, 1};
    bool draco = gcv_select_variant(&context, GCV_FILTER_READ, "model/gltf+json", &traits, "Assetimport Reader", nullptr) != nullptr;
    const bu_mime_trait old_glb = BU_MIME_TRAIT_STRING("version", "1.0");
    const bu_mime_traits old_glb_traits = {&old_glb, 1};
    bool broken_writer = gcv_select_variant(&context, GCV_FILTER_WRITE, "model/gltf-binary", &old_glb_traits, "Assetimport Writer", nullptr) != nullptr;
    gcv_context_destroy(&context);
    if (broken_writer) return 1;
    if (draco) {
	const char *begin = assetimport_draco_triangle;
	profile_case compressed{"model/gltf+json", "version", "2.0", "3.0", bytes(begin, begin + sizeof(assetimport_draco_triangle) - 1), {}};
	compressed.set_key = "gltf.required-extensions";
	compressed.set_members = {"KHR_draco_mesh_compression"};
	if (fixture_test(compressed)) return 1;
    }
    return 0;
}

int unreal_profiles_test()
{
    gcv_context context;
    gcv_context_init(&context);
    bool native = bu_byteorder() == BU_LITTLE_ENDIAN;
    bool absent = !gcv_select_filter(&context, GCV_FILTER_READ, "model/vnd.epicgames.unreal.mesh", "Assetimport Reader", NULL);
    gcv_context_destroy(&context);
    if (!native) return absent ? 0 : 1;
    /* Closed tetrahedron avoids the SDK's unrelated triangle-count check
     * on vertex indices. Wire words do not depend on C++ bitfield layout. */
    constexpr uint16_t vertices = 4, triangles = 4;
    constexpr size_t data_header_size = 48, triangle_size = 16;
    bytes data(data_header_size + triangles * triangle_size, 0);
    put_word(data, 0, triangles, sizeof(uint16_t));
    put_word(data, sizeof(uint16_t), vertices, sizeof(uint16_t));
    const uint16_t faces[triangles][3] = {{0, 1, 2}, {0, 3, 1}, {0, 2, 3}, {1, 3, 2}};
    for (size_t face = 0; face < triangles; ++face)
	for (size_t corner = 0; corner < 3; ++corner)
	    put_word(data, data_header_size + face * triangle_size + corner * sizeof(uint16_t), faces[face][corner], sizeof(uint16_t));
    auto frame = [](uint16_t stride) {
	bytes result;
	append_words(result, {1, uint32_t(vertices) * stride}, sizeof(uint16_t));
	const uint32_t packed[] = {0, 1, 1u << 11, 1u << 22};
	for (uint32_t value : packed) {
	    append_words(result, {value});
	    if (stride > sizeof(uint32_t)) result.resize(result.size() + stride - sizeof(uint32_t), 0);
	}
	return result;
    };
    const bytes animation = frame(sizeof(uint32_t));
    profile_case topology{"model/vnd.epicgames.unreal.mesh", "unreal.part", "data", "animation", data, {}};
    topology.extension = "_d.3d";
    topology.faces = triangles;
    topology.vertices = vertices;
    topology.request_selectable = true;
    topology.native_suffix_required = true;
    topology.companions = {{"_a.3d", animation}};
    if (fixture_test(topology)) return 1;
    profile_case aniv{"model/vnd.epicgames.unreal.mesh", "unreal.vertex-stride", "4", "8", animation, frame(8)};
    aniv.extension = "_a.3d";
    aniv.faces = triangles;
    aniv.vertices = vertices;
    aniv.native_suffix_required = true;
    aniv.companions = {{"_d.3d", data}};
    if (fixture_test(aniv)) return 1;
    const std::string script = "class fixture expands Actor;\n#exec MESHMAP SCALE MESHMAP=fixture X=1 Y=1 Z=1\n";
    for (const char *encoding : {"utf-8", "utf-16le", "utf-16be", "utf-32le", "utf-32be"}) {
	profile_case source{"text/vnd.epicgames.unrealscript", "encoding", encoding, "binary", xml_bytes(script, encoding), data};
	source.extension = ".uc";
	source.faces = triangles;
	source.vertices = vertices;
	source.set_key = "unrealscript.exec-commands";
	source.set_members = {"meshmap.scale"};
	source.native_suffix_required = true;
	source.companions = {{"_d.3d", data}, {"_a.3d", animation}};
	if (fixture_test(source)) return 1;
    }
    return 0;
}

int quick3d_pmx_profiles_test()
{
    for (char kind : {'o', 's'})
	for (char minor : {'0', '1'}) {
	    profile_case sample{"model/vnd.quick3d", "version", std::string("3.") + minor, "3.2",
		quick3d_triangle(kind, minor), quick3d_triangle(kind, '2')};
	    if (fixture_test(sample)) return 1;
	}
    profile_case kind{"model/vnd.quick3d", "quick3d.kind", "object", "scene", quick3d_triangle('o', '0'), {}};
    kind.request_selectable = true;
    if (fixture_test(kind)) return 1;
    if (bu_byteorder() != BU_LITTLE_ENDIAN || sizeof(float) != sizeof(uint32_t) ||
	sizeof(int) != sizeof(uint32_t) || !std::numeric_limits<float>::is_iec559) return 0;
    for (bool version21 : {false, true})
	for (bool utf8 : {false, true})
	    for (unsigned int width : {1u, 2u, 4u}) {
		profile_case sample{"model/vnd.mikumikudance.pmx", "version", version21 ? "2.1" : "2.0", "3.0",
		    pmx_triangle(version21, utf8, width, 0), {}};
		sample.set_key = "pmx.index-widths";
		sample.set_members = {width == 1 ? "1" : width == 2 ? "2" : "4"};
		sample.invalid_data = sample.data;
		const size_t globals_offset = 9;
		if (width == 1) sample.invalid_data[globals_offset + 2] = 5;
		else if (width == 2) sample.invalid_data[globals_offset + 1] = 5;
		else sample.invalid_data[globals_offset - 1] = 7;
		if (fixture_test(sample)) return 1;
	    }
    for (unsigned char uv : {1, 2, 3, 4}) {
	profile_case sample{"model/vnd.mikumikudance.pmx", "pmx.additional-uv", "0", std::to_string(uv),
	    pmx_triangle(true, true, 1, 0), pmx_triangle(true, true, 1, uv)};
	if (fixture_test(sample)) return 1;
    }
    return 0;
}

int container_profiles_test()
{
    const char future_nendo[] = "nendo 2.0";
    for (unsigned minor = 0; minor <= 2; ++minor) {
	profile_case sample{"model/vnd.izware.nendo", "version", "1." + std::to_string(minor), "2.0",
	    nendo_triangle(minor), {}};
	sample.invalid_data = sample.data;
	std::copy(future_nendo, future_nendo + sizeof(future_nendo) - 1, sample.invalid_data.begin());
	sample.faces = 2;
	if (fixture_test(sample)) return 1;
    }
    for (uint32_t version = 1; version <= 2; ++version) {
	profile_case sample{"model/vnd.nevercenter.sib", "version", std::to_string(version), "3",
	    silo_triangle(version, version == 2), silo_triangle(3, false)};
	sample.extension = ".sib";
	sample.explicit_traits = version != 2;
	if (fixture_test(sample)) return 1;
    }
    for (bool binary : {false, true}) {
	profile_case sample{"model/vnd.caligari.cob", "version", "00.01", "00.02", cob_triangle(binary), {}};
	sample.invalid_data = sample.data;
	const size_t minor_digit = sizeof("Caligari V00.0") - 1;
	sample.invalid_data[minor_digit] = '2';
	sample.extension = ".cob";
	if (fixture_test(sample)) return 1;
    }
    return 0;
}

int mdl_profiles_test()
{
    if (profile_test("model/vnd.idsoftware.mdl", 6, sizeof("IDPO") - 1, quake_mdl_triangle())) return 1;
    profile_case game{"model/vnd.conitec.mdl", "version", "2", "6", quake_mdl_triangle(), {}};
    const char magic[] = "MDL2";
    std::copy(magic, magic + sizeof(magic) - 1, game.data.begin());
    if (fixture_test(game)) return 1;
    for (unsigned version : {5, 7}) {
	profile_case terrain{"image/vnd.conitec.hmp", "version", std::to_string(version), "4",
	    hmp_heightfield(version), hmp_heightfield(4)};
	terrain.faces = 2;
	terrain.vertices = 4;
	if (fixture_test(terrain)) return 1;
    }
    gcv_context context;
    gcv_context_init(&context);
    const bu_mime_trait values[] = {BU_MIME_TRAIT_STRING("version", "10"), BU_MIME_TRAIT_STRING("version", "48"),
	BU_MIME_TRAIT_STRING("valve.mdl-kind", "sequence-group")};
    bool valid = true;
    for (size_t i = 0; i < sizeof(values) / sizeof(values[0]); ++i) {
	const bu_mime_traits request = {&values[i], 1};
	bool selected = gcv_select_variant(&context, GCV_FILTER_READ, "model/vnd.valve.mdl", &request, "Assetimport Reader", NULL);
	valid = valid && selected == (i == 0);
    }
    gcv_context_destroy(&context);
    if (!valid) {
	bu_log("Valve MDL declarations do not match the implemented model reader\n");
	return 1;
    }
    return 0;
}

int iff_profiles_test()
{
    for (const char *form : {"LWOB", "LWO2", "LXOB"}) {
	profile_case sample{BU_STR_EQUAL(form, "LXOB") ? "model/vnd.foundry.lxo" : "image/x-lwo", "iff.form", form,
	    "LWO3", iff_triangle(form), iff_triangle("LWO3")};
	if (fixture_test(sample)) return 1;
    }
    profile_case revision{"model/vnd.foundry.lxo", "version", "4.2", "9.0", iff_triangle("LXOB", true), {}};
    revision.request_selectable = true;
    return fixture_test(revision);
}

int skeletal_profiles_test()
{
    const std::string smd_header = "version 1\nnodes\n0 \"origin\" -1\nend\nskeleton\ntime 0\n0 0 0 0 0 0 0\nend\n";
    const std::string smd_faces = "triangles\nmaterial\n0 0 0 0 0 0 1 0 0\n0 1 0 0 0 0 1 1 0\n0 0 1 0 0 0 1 0 1\nend\n";
    /* The SDK groups adjacent global records unless blank lines separate
     * them. Use the conventional separated layout for this mesh fixture. */
    const std::string md5 = "MD5Version 10\ncommandline \"\"\n\nnumJoints 1\n\nnumMeshes 1\n\n"
	"joints {\n\"origin\" -1 ( 0 0 0 ) ( 0 0 0 )\n}\n\nmesh {\nshader \"\"\nnumverts 3\n"
	"vert 0 ( 0 0 ) 0 1\nvert 1 ( 1 0 ) 1 1\nvert 2 ( 0 1 ) 2 1\nnumtris 1\ntri 0 0 1 2\n"
	"numweights 3\nweight 0 0 1 ( 0 0 0 )\nweight 1 0 1 ( 1 0 0 )\nweight 2 0 1 ( 0 1 0 )\n}\n";
    std::string padding;
    const std::string comment = "// profile padding\n";
    while (padding.size() <= BU_MIME_DEFAULT_SCAN_LIMIT) padding += comment;
    for (bool padded : {false, true}) {
	std::string smd = smd_header + (padded ? padding : "") + smd_faces;
	std::string future = smd;
	future[sizeof("version ") - 1] = '2';
	profile_case sample{"model/vnd.valve.smd", "version", "1", "2", bytes(smd.begin(), smd.end()), bytes(future.begin(), future.end())};
	sample.extension = ".smd";
	sample.explicit_traits = !padded;
	if (fixture_test(sample)) return 1;
    }
    std::string future_md5 = md5;
    future_md5[sizeof("MD5Version 1") - 1] = '1';
    profile_case sample{"model/vnd.idsoftware.md5", "version", "10", "11", bytes(md5.begin(), md5.end()), bytes(future_md5.begin(), future_md5.end())};
    sample.extension = ".md5mesh";
    sample.invalid_companion_extension = ".md5anim";
    if (fixture_test(sample)) return 1;
    gcv_context context;
    gcv_context_init(&context);
    bool valid = true;
    const struct {const char *type; const char *key; const char *kind;} unsupported_kinds[] = {
	{"model/vnd.valve.smd", "smd.kind", "vertex-animation"}, {"model/vnd.valve.smd", "smd.kind", "skeleton"},
	{"model/vnd.idsoftware.md5", "md5.kind", "animation"}, {"model/vnd.idsoftware.md5", "md5.kind", "camera"}};
    for (const auto &entry : unsupported_kinds) {
	const bu_mime_trait unsupported = BU_MIME_TRAIT_STRING(entry.key, entry.kind);
	const bu_mime_traits request = {&unsupported, 1};
	valid = valid && !gcv_select_variant(&context, GCV_FILTER_READ, entry.type, &request, "Assetimport Reader", NULL);
    }
    gcv_context_destroy(&context);
    return valid ? 0 : 1;
}

int raw_profiles_test()
{
    const std::string triangle = "0 0 0 1 0 0 0 1 0\n";
    const struct {std::string text; std::vector<const char *> features;} samples[] = {
	{triangle, {}}, {"part\n" + triangle, {"groups"}},
	{"1 0 0 " + triangle, {"face-colors"}},
	{"0 0 0 1 0 0 0 1 0 texture.png\n", {"textures"}}};
    for (const auto &entry : samples) {
	std::string padding;
	while (padding.size() <= BU_MIME_DEFAULT_SCAN_LIMIT) padding += "\n";
	std::string unsupported = entry.text + padding + "part thickf=0.5\n" + triangle;
	profile_case sample{"model/vnd.povray.raw", "encoding", "ascii", "binary", bytes(entry.text.begin(), entry.text.end()),
	    bytes(unsupported.begin(), unsupported.end())};
	sample.extension = ".raw";
	sample.set_key = "raw.features";
	sample.set_members = entry.features;
	sample.native_suffix_required = true;
	if (fixture_test(sample)) return 1;
    }
    /* A colored row's trailing texture is not consumed by the configured
     * SDK parser. Exclude that combination until its parser is corrected. */
    const std::string colored_texture = "1 0 0 0 0 0 1 0 0 0 1 0 texture.png\n";
    profile_case combined{"model/vnd.povray.raw", "encoding", "ascii", "binary", bytes(triangle.begin(), triangle.end()),
	bytes(colored_texture.begin(), colored_texture.end())};
    combined.extension = ".raw";
    if (fixture_test(combined)) return 1;
    std::string multiple = "first\n" + triangle;
    while (multiple.size() <= BU_MIME_DEFAULT_SCAN_LIMIT) multiple += "\n";
    multiple += "second\n" + triangle;
    combined.invalid_data.assign(multiple.begin(), multiple.end());
    return fixture_test(combined);
}

int motion_profiles_test()
{
    const char *type = "image/x-lws";
    gcv_context context;
    gcv_context_init(&context);
    char temporary[MAXPATHLEN] = {0};
    FILE *file = bu_temp_file(temporary, sizeof(temporary));
    int closed = file ? fclose(file) : EOF;
    std::string filename = temporary;
    std::string objectfile = filename + ".lwo";
    int status = 1;
    {
	auto write = [](const std::string &path, const bytes &data) {
	    std::ofstream stream(path, std::ios::binary);
	    stream.write(reinterpret_cast<const char *>(data.data()), data.size());
	    stream.close();
	    return !stream.fail();
	};
	PROFILE_CHECK(file && !closed);
	PROFILE_CHECK(write(objectfile, iff_triangle("LWO2")));
	const gcv_filter *reader = gcv_select_filter(&context, GCV_FILTER_READ, type, "Assetimport Reader", NULL);
	PROFILE_CHECK(reader);
	PROFILE_CHECK(!mk_id_units(wdb_dbopen(context.dbip, RT_WDB_TYPE_DB_INMEM), "Before motion profile validation", "cm"));
	const std::string scene_body = "\n1\nLoadObject " + objectfile + "\n";
	const struct {const char *mime; std::string data;} motion[] = {
	    {"model/vnd.newtek.motion", "LWMO" + scene_body},
	    {"model/vnd.biovision.bvh", "HIERARCHY\nROOT Hips\n{\nOFFSET 0 0 0\nCHANNELS 0\n}\nMOTION\nFrames: 1\nFrame Time: 0.04\n"},
	    {"model/vnd.autodesk.csm", "$Filename sample.csm\n$FirstFrame 1\n$LastFrame 1\n$Rate 30\n$Order\nmarker\n$Points\n1 0 0 0\n"}};
	for (const auto &entry : motion) {
	    PROFILE_CHECK(!gcv_select_filter(&context, GCV_FILTER_READ, entry.mime, "Assetimport Reader", NULL));
	    PROFILE_CHECK(write(filename, bytes(entry.data.begin(), entry.data.end())));
	    PROFILE_CHECK(!gcv_execute(&context, reader, NULL, 0, NULL, filename.c_str()));
	    PROFILE_CHECK(!gcv_execute_variant(&context, reader, type, NULL, NULL, 0, NULL, filename.c_str()));
	    PROFILE_CHECK(BU_STR_EQUAL(context.dbip->dbi_title, "Before motion profile validation"));
	    PROFILE_CHECK(!db_ls(context.dbip, DB_LS_PRIM | DB_LS_COMB, NULL, NULL));
	}
	std::string scene = "LWSC" + scene_body;
	PROFILE_CHECK(write(filename, bytes(scene.begin(), scene.end())));
	const bu_mime_trait version = BU_MIME_TRAIT_STRING("version", "1");
	const bu_mime_traits traits = {&version, 1};
	PROFILE_CHECK(gcv_execute_variant(&context, reader, type, &traits, NULL, 0, NULL, filename.c_str()));
	PROFILE_CHECK(db_ls(context.dbip, DB_LS_PRIM, NULL, NULL) == 1);
	status = 0;
    }
done:
    if (!filename.empty()) bu_file_delete(filename.c_str());
    if (!objectfile.empty()) bu_file_delete(objectfile.c_str());
    gcv_context_destroy(&context);
    return status;
}

int nff_profiles_test()
{
    const std::string polygon = "p 3\n0 0 0\n1 0 0\n0 1 0\n";
    const std::string sense8 = "nff\nversion 2.0\ntriangle\n3\n0 0 0\n1 0 0\n0 1 0\n1\n3 0 1 2\n";
    std::string padding;
    while (padding.size() <= BU_MIME_DEFAULT_SCAN_LIMIT) padding += "# NFF profile padding\n";
    for (const char *suffix : {".nff", ".enff"}) {
	std::string haines = polygon + padding;
	std::string unsupported = haines + "pb 4\n";
	profile_case standard{"model/prs.haines.nff", "encoding", "ascii", "binary",
	    bytes(haines.begin(), haines.end()), bytes(unsupported.begin(), unsupported.end())};
	standard.extension = suffix;
	standard.native_suffix_required = true;
	standard.set_key = "nff.extensions";
	if (fixture_test(standard)) return 1;
	profile_case indexed{"model/vnd.sense8.nff", "version", "2.0", "9.0",
	    bytes(sense8.begin(), sense8.end()), bytes(polygon.begin(), polygon.end())};
	indexed.extension = suffix;
	indexed.native_suffix_required = true;
	indexed.request_selectable = true;
	indexed.untyped_may_accept = true;
	if (fixture_test(indexed)) return 1;
    }
    const std::string patch = "tpp 3\n0 0 0 0 0 1 0 0\n1 0 0 0 0 1 1 0\n0 1 0 0 0 1 0 1\n";
    profile_case textured{"model/prs.haines.nff", "encoding", "ascii", "binary", bytes(patch.begin(), patch.end()), {}};
    textured.extension = ".nff";
    textured.set_key = "nff.extensions";
    textured.set_members = {"tpp"};
    if (fixture_test(textured)) return 1;
    std::string mixed = " " + sense8 + polygon;
    profile_case branch{"model/vnd.sense8.nff", "encoding", "ascii", "binary", bytes(sense8.begin(), sense8.end()),
	bytes(mixed.begin(), mixed.end())};
    branch.extension = ".nff";
    if (fixture_test(branch)) return 1;
    profile_case type_only{"model/prs.haines.nff", "encoding", "ascii", "binary",
	bytes(polygon.begin(), polygon.end()), {}};
    type_only.extension = ".nff";
    type_only.explicit_traits = false;
    return fixture_test(type_only);
}

int xml_charset_profiles_test()
{
    const std::string amf = "<?xml version='1.0'?><amf unit='millimeter'>" + amf_mesh() + "</amf>";
    const std::string xgl = "<?xml version='1.0'?><WORLD>" + xgl_environment() + xgl_mesh() + "</WORLD>";
    const struct {const char *type; const char *extension; std::string xml; bool compressed;} cases[] = {
	{"model/amf", ".amf", amf, false},
	{"model/vnd.collada+xml", ".dae", collada_triangle("1.4.1"), false},
	{"model/vnd.realitywave.xgl+xml", ".xgl", xgl, false},
	{"model/vnd.realitywave.zgl", ".zgl", xgl, true}
    };
    for (const auto &entry : cases) {
	for (const char *declaration : {"US-ASCII", "ISO-8859-1", "latin1"}) {
	    const bool ascii = BU_STR_EQUAL(declaration, "US-ASCII");
	    const std::string document = std::string("<?xml version='1.0' encoding='") + declaration + "'?>" +
		entry.xml.substr(entry.xml.find("?>") + 2) + (ascii ? "<!--plain-->" : "<!--caf\xe9-->");
	    bytes data(document.begin(), document.end());
	    if (entry.compressed) data = zlib_bytes(data);
	    profile_case declared{entry.type, entry.compressed ? "xml.encoding" : "encoding", ascii ? "ascii" : "iso-8859-1",
		"binary", data, {}};
	    declared.extension = entry.extension;
	    declared.native_suffix_required = entry.compressed;
	    if (fixture_test(declared)) return 1;
	}
	const std::string padded = entry.xml + "<!--" + std::string(BU_MIME_DEFAULT_SCAN_LIMIT + 1, ' ') + "-->";
	bytes valid(padded.begin(), padded.end()), invalid = valid;
	/* An overlong UTF-8 leading octet, outside the bounded prefix. */
	invalid[invalid.size() - sizeof("-->")] = 0xc0;
	if (entry.compressed) {valid = zlib_bytes(valid); invalid = zlib_bytes(invalid);}
	for (const char *key : {entry.compressed ? "xml.encoding" : "encoding", entry.compressed ? nullptr : "text.ascii-only"}) {
	    if (!key) continue;
	    const bool repertoire = BU_STR_EQUAL(key, "text.ascii-only");
	    profile_case sample{entry.type, key, repertoire ? "true" : "utf-8", repertoire ? "false" : "binary", valid, invalid};
	    sample.extension = entry.extension;
	    sample.request_selectable = repertoire;
	    sample.native_suffix_required = entry.compressed;
	    sample.untyped_may_accept = true;
	    if (fixture_test(sample)) return 1;
	    /* Declining an unverified charset must not add a new blanket
	     * rejection to the underlying reader's unrequested path. */
	    sample.data = invalid;
	    sample.invalid_data.clear();
	    sample.explicit_traits = false;
	    if (fixture_test(sample)) return 1;
	}
    }
    return 0;
}
}

extern "C" int
gcv_sdk_ifc_profiles(const char *ifc_fixture)
{
    return ifc_profiles_test(ifc_fixture);


}

extern "C" int
gcv_sdk_xml_charset_profiles(void)
{
    return xml_charset_profiles_test();
}

extern "C" int
gcv_sdk_assertion_profiles(void)
{
    return obj_input_profiles_test() || obj_output_profiles_test() || opengex_profiles_test() || fbx_profiles_test() || three_mf_profiles_test() || three_ds_profiles_test() || stl_profiles_test() || ply_profiles_test() || x_profiles_test() || assbin_profiles_test() || scene_text_profiles_test() || b3d_profiles_test() || off_profiles_test() || dxf_profiles_test() || amf_profiles_test() || blender_profiles_test() || xgl_profiles_test();
}

extern "C" int
gcv_sdk_profiles(const char *ifc_fixture)
{
    return obj_input_profiles_test() || obj_output_profiles_test() || opengex_profiles_test() || fbx_profiles_test() || three_mf_profiles_test() || three_ds_profiles_test() || stl_profiles_test() || ply_profiles_test() || x_profiles_test() || assbin_profiles_test() || scene_text_profiles_test() || b3d_profiles_test() || off_profiles_test() || dxf_profiles_test() || amf_profiles_test() || blender_profiles_test() || ifc_profiles_test(ifc_fixture) || container_profiles_test() || mdl_profiles_test() || skeletal_profiles_test() || iff_profiles_test() || nff_profiles_test() || raw_profiles_test() || motion_profiles_test() || quick3d_pmx_profiles_test() || xml_profiles_test() || x3d_profiles_test() || xgl_profiles_test() || ogre_profiles_test() || pk3_profiles_test() || zae_profiles_test() || unreal_profiles_test() || gltf_profiles_test() || profile_test("model/vnd.idsoftware.md2", 8, sizeof("IDP2") - 1, md2_triangle()) ||
	profile_test("model/vnd.idsoftware.md3", 15, sizeof("IDP3") - 1, md3_triangle()) ||
	profile_test("model/vnd.interquake.iqm", 2, sizeof("INTERQUAKEMODEL"), iqm_triangle()) ||
	profile_test("model/vnd.chumbalumsoft.ms3d", 4, sizeof("MS3D000000") - 1, ms3d_triangle()) ||
	profile_test("application/terragen", 0, 0, terragen_heightfield(), 2, 4);
}

/*
 * Local Variables:
 * mode: C++
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
