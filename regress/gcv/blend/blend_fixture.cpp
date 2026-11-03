/*                    B L E N D _ F I X T U R E . C P P
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 */

#include "common.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

#include "bu/app.h"

namespace {

constexpr uint64_t OBJECT_ADDRESS = 0x1000;
constexpr uint64_t MESH_ADDRESS = 0x2000;
constexpr uint64_t VERTEX_ADDRESS = 0x3000;
constexpr uint64_t FACE_ADDRESS = 0x4000;
constexpr size_t ID_NAME_SIZE = 66;
constexpr uint32_t TETRAHEDRON_VERTEX_COUNT = 4;
constexpr uint32_t TETRAHEDRON_FACE_COUNT = 4;

enum DnaNameIndex : uint16_t {
    NAME_ID_NAME,
    NAME_ID_PADDING,
    NAME_ID,
    NAME_DATA,
    NAME_OBJECT_MATRIX,
    NAME_TOTAL_VERTICES,
    NAME_TOTAL_FACES,
    NAME_TOTAL_POLYGONS,
    NAME_TOTAL_LOOPS,
    NAME_VERTICES,
    NAME_FACES,
    NAME_POLYGONS,
    NAME_LOOPS,
    NAME_COORDINATE,
    NAME_VERTEX_1,
    NAME_VERTEX_2,
    NAME_VERTEX_3,
    NAME_VERTEX_4
};

enum DnaTypeIndex : uint16_t {
    TYPE_CHARACTER,
    TYPE_FLOAT,
    TYPE_INTEGER,
    TYPE_VOID,
    TYPE_ID,
    TYPE_OBJECT,
    TYPE_MESH,
    TYPE_VERTEX,
    TYPE_FACE,
    TYPE_LOOP,
    TYPE_POLYGON
};

enum DnaStructureIndex : uint32_t {
    STRUCTURE_ID,
    STRUCTURE_OBJECT,
    STRUCTURE_MESH,
    STRUCTURE_VERTEX,
    STRUCTURE_FACE,
    STRUCTURE_COUNT
};

static void
append_u16(std::vector<unsigned char> &bytes, uint16_t value)
{
    bytes.push_back(static_cast<unsigned char>(value));
    bytes.push_back(static_cast<unsigned char>(value >> 8));
}

static void
append_sdna_field(std::vector<unsigned char> &bytes, DnaTypeIndex type,
    DnaNameIndex name)
{
    append_u16(bytes, static_cast<uint16_t>(type));
    append_u16(bytes, static_cast<uint16_t>(name));
}

static void
append_u32(std::vector<unsigned char> &bytes, uint32_t value)
{
    for (size_t shift = 0; shift < 32; shift += 8)
	bytes.push_back(static_cast<unsigned char>(value >> shift));
}

static void
append_u64(std::vector<unsigned char> &bytes, uint64_t value)
{
    for (size_t shift = 0; shift < 64; shift += 8)
	bytes.push_back(static_cast<unsigned char>(value >> shift));
}

static void
append_float(std::vector<unsigned char> &bytes, float value)
{
    uint32_t bits = 0;
    std::memcpy(&bits, &value, sizeof(bits));
    append_u32(bytes, bits);
}

static void
append_text(std::vector<unsigned char> &bytes, const std::string &text)
{
    bytes.insert(bytes.end(), text.begin(), text.end());
}

static void
append_fixed_text(std::vector<unsigned char> &bytes, const std::string &text,
    size_t size)
{
    size_t count = std::min(text.size(), size);
    bytes.insert(bytes.end(), text.begin(), text.begin() + count);
    bytes.insert(bytes.end(), size - count, 0);
}

static void
append_block(std::vector<unsigned char> &file, const std::string &code,
    uint64_t address, uint32_t sdna_index, uint32_t count,
    const std::vector<unsigned char> &data)
{
    append_fixed_text(file, code, 4);
    append_u32(file, static_cast<uint32_t>(data.size()));
    append_u64(file, address);
    append_u32(file, sdna_index);
    append_u32(file, count);
    file.insert(file.end(), data.begin(), data.end());
}

static void
align_four(std::vector<unsigned char> &bytes)
{
    while (bytes.size() % 4 != 0)
	bytes.push_back(0);
}

static std::vector<unsigned char>
make_sdna()
{
    const std::array<std::string, 18> names = {
	"name[66]", "_pad[2]", "id", "*data", "obmat[4][4]",
	"totvert", "totface", "totpoly", "totloop", "*mvert",
	"*mface", "*mpoly", "*mloop", "co[3]", "v1", "v2", "v3", "v4"
    };
    const std::array<std::string, 11> types = {
	"char", "float", "int", "void", "ID", "Object", "Mesh",
	"MVert", "MFace", "MLoop", "MPoly"
    };
    const std::array<uint16_t, 11> lengths = {
	1, 4, 4, 0, 68, 140, 116, 12, 16, 8, 12
    };

    std::vector<unsigned char> dna;
    append_text(dna, "SDNANAME");
    append_u32(dna, static_cast<uint32_t>(names.size()));
    for (const std::string &name : names) {
	append_text(dna, name);
	dna.push_back(0);
    }
    align_four(dna);
    append_text(dna, "TYPE");
    append_u32(dna, static_cast<uint32_t>(types.size()));
    for (const std::string &type : types) {
	append_text(dna, type);
	dna.push_back(0);
    }
    align_four(dna);
    append_text(dna, "TLEN");
    for (uint16_t length : lengths)
	append_u16(dna, length);
    align_four(dna);
    append_text(dna, "STRC");
    append_u32(dna, STRUCTURE_COUNT);

    append_u16(dna, TYPE_ID);
    append_u16(dna, 2);
    append_sdna_field(dna, TYPE_CHARACTER, NAME_ID_NAME);
    append_sdna_field(dna, TYPE_CHARACTER, NAME_ID_PADDING);

    append_u16(dna, TYPE_OBJECT);
    append_u16(dna, 3);
    append_sdna_field(dna, TYPE_ID, NAME_ID);
    append_sdna_field(dna, TYPE_MESH, NAME_DATA);
    append_sdna_field(dna, TYPE_FLOAT, NAME_OBJECT_MATRIX);

    append_u16(dna, TYPE_MESH);
    append_u16(dna, 9);
    append_sdna_field(dna, TYPE_ID, NAME_ID);
    for (uint16_t name = NAME_TOTAL_VERTICES; name <= NAME_TOTAL_LOOPS; ++name) {
	append_sdna_field(dna, TYPE_INTEGER, static_cast<DnaNameIndex>(name));
    }
    append_sdna_field(dna, TYPE_VERTEX, NAME_VERTICES);
    append_sdna_field(dna, TYPE_FACE, NAME_FACES);
    append_sdna_field(dna, TYPE_POLYGON, NAME_POLYGONS);
    append_sdna_field(dna, TYPE_LOOP, NAME_LOOPS);

    append_u16(dna, TYPE_VERTEX);
    append_u16(dna, 1);
    append_sdna_field(dna, TYPE_FLOAT, NAME_COORDINATE);

    append_u16(dna, TYPE_FACE);
    append_u16(dna, 4);
    for (uint16_t name = NAME_VERTEX_1; name <= NAME_VERTEX_4; ++name) {
	append_sdna_field(dna, TYPE_INTEGER, static_cast<DnaNameIndex>(name));
    }
    return dna;
}

static std::vector<unsigned char>
make_object()
{
    std::vector<unsigned char> object;
    append_fixed_text(object, "OBTetrahedron", ID_NAME_SIZE);
    object.insert(object.end(), 2, 0);
    append_u64(object, MESH_ADDRESS);
    const std::array<float, 16> matrix = {
	1.0f, 0.0f, 0.0f, 0.0f,
	0.0f, 1.0f, 0.0f, 0.0f,
	0.0f, 0.0f, 1.0f, 0.0f,
	0.0f, 0.0f, 0.0f, 1.0f
    };
    for (float value : matrix)
	append_float(object, value);
    return object;
}

static std::vector<unsigned char>
make_mesh()
{
    std::vector<unsigned char> mesh;
    append_fixed_text(mesh, "METetrahedronMesh", ID_NAME_SIZE);
    mesh.insert(mesh.end(), 2, 0);
    append_u32(mesh, TETRAHEDRON_VERTEX_COUNT);
    append_u32(mesh, TETRAHEDRON_FACE_COUNT);
    append_u32(mesh, 0);
    append_u32(mesh, 0);
    append_u64(mesh, VERTEX_ADDRESS);
    append_u64(mesh, FACE_ADDRESS);
    append_u64(mesh, 0);
    append_u64(mesh, 0);
    return mesh;
}

static std::vector<unsigned char>
make_vertices()
{
    const std::array<std::array<float, 3>, 4> coordinates = {{
	{{0.0f, 0.0f, 0.0f}},
	{{1.0f, 0.0f, 0.0f}},
	{{0.0f, 1.0f, 0.0f}},
	{{0.0f, 0.0f, 1.0f}}
    }};
    std::vector<unsigned char> vertices;
    for (const auto &coordinate : coordinates) {
	for (float value : coordinate)
	    append_float(vertices, value);
    }
    return vertices;
}

static std::vector<unsigned char>
make_faces()
{
    const std::array<std::array<uint32_t, 4>, 4> indices = {{
	{{0, 2, 1, 0}},
	{{0, 1, 3, 0}},
	{{1, 2, 3, 0}},
	{{2, 0, 3, 0}}
    }};
    std::vector<unsigned char> faces;
    for (const auto &face : indices) {
	for (uint32_t index : face)
	    append_u32(faces, index);
    }
    return faces;
}

static std::vector<unsigned char>
make_fixture()
{
    std::vector<unsigned char> file;
    append_text(file, "BLENDER-v300");
    append_block(file, "OB", OBJECT_ADDRESS, STRUCTURE_OBJECT, 1, make_object());
    append_block(file, "ME", MESH_ADDRESS, STRUCTURE_MESH, 1, make_mesh());
    append_block(file, "DATA", VERTEX_ADDRESS, STRUCTURE_VERTEX,
	TETRAHEDRON_VERTEX_COUNT, make_vertices());
    append_block(file, "DATA", FACE_ADDRESS, STRUCTURE_FACE,
	TETRAHEDRON_FACE_COUNT, make_faces());
    append_block(file, "DNA1", 0, STRUCTURE_ID, 1, make_sdna());
    append_block(file, "ENDB", 0, 0, 0, {});
    return file;
}

} // namespace

int
main(int argc, char **argv)
{
    bu_setprogname(argv[0]);
    if (argc != 2)
	return 1;
    std::vector<unsigned char> file = make_fixture();
    std::ofstream output(argv[1], std::ios::binary);
    if (!output)
	return 1;
    output.write(reinterpret_cast<const char *>(file.data()),
	static_cast<std::streamsize>(file.size()));
    return output ? 0 : 1;
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
