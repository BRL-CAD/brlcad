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
constexpr uint64_t CURVE_OBJECT_ADDRESS = 0x5000;
constexpr uint64_t CURVE_ADDRESS = 0x6000;
constexpr uint64_t NURB_ADDRESS = 0x7000;
constexpr uint64_t CONTROL_POINT_ADDRESS = 0x8000;
constexpr uint64_t KNOT_U_ADDRESS = 0x9000;
constexpr uint64_t KNOT_V_ADDRESS = 0xa000;
constexpr uint64_t CURVE_NURB_ADDRESS = 0xb000;
constexpr uint64_t CURVE_CONTROL_POINT_ADDRESS = 0xc000;
constexpr uint64_t CURVE_KNOT_ADDRESS = 0xd000;
constexpr size_t ID_NAME_SIZE = 66;
constexpr uint32_t TETRAHEDRON_VERTEX_COUNT = 4;
constexpr uint32_t TETRAHEDRON_FACE_COUNT = 4;
constexpr uint32_t SURFACE_POINT_COUNT_U = 4;
constexpr uint32_t SURFACE_POINT_COUNT_V = 4;
constexpr uint32_t SURFACE_POINT_COUNT = SURFACE_POINT_COUNT_U * SURFACE_POINT_COUNT_V;
constexpr uint32_t CURVE_POINT_COUNT = 4;

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
    NAME_VERTEX_4,
    NAME_FIRST,
    NAME_LAST,
    NAME_NEXT,
    NAME_PREVIOUS,
    NAME_TYPE,
    NAME_POINTS_U,
    NAME_POINTS_V,
    NAME_ORDER_U,
    NAME_ORDER_V,
    NAME_FLAG_U,
    NAME_FLAG_V,
    NAME_KNOTS_U,
    NAME_KNOTS_V,
    NAME_CONTROL_POINTS,
    NAME_HOMOGENEOUS_COORDINATE,
    NAME_NURB_LIST
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
    TYPE_POLYGON,
    TYPE_SHORT,
    TYPE_LIST_BASE,
    TYPE_CURVE,
    TYPE_NURB,
    TYPE_BPOINT
};

enum DnaStructureIndex : uint32_t {
    STRUCTURE_ID,
    STRUCTURE_OBJECT,
    STRUCTURE_MESH,
    STRUCTURE_VERTEX,
    STRUCTURE_FACE,
    STRUCTURE_LIST_BASE,
    STRUCTURE_CURVE,
    STRUCTURE_NURB,
    STRUCTURE_BPOINT,
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
    const std::array<std::string, 34> names = {
	"name[66]", "_pad[2]", "id", "*data", "obmat[4][4]",
	"totvert", "totface", "totpoly", "totloop", "*mvert",
	"*mface", "*mpoly", "*mloop", "co[3]", "v1", "v2", "v3", "v4",
	"*first", "*last", "*next", "*prev", "type", "pntsu", "pntsv",
	"orderu", "orderv", "flagu", "flagv", "*knotsu", "*knotsv",
	"*bp", "vec[4]", "nurb"
    };
    const std::array<std::string, 16> types = {
	"char", "float", "int", "void", "ID", "Object", "Mesh",
	"MVert", "MFace", "MLoop", "MPoly", "short", "ListBase",
	"Curve", "Nurb", "BPoint"
    };
    const std::array<uint16_t, 16> lengths = {
	1, 4, 4, 0, 68, 140, 116, 12, 16, 8, 12, 2, 16, 84, 58, 16
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

    append_u16(dna, TYPE_LIST_BASE);
    append_u16(dna, 2);
    append_sdna_field(dna, TYPE_NURB, NAME_FIRST);
    append_sdna_field(dna, TYPE_NURB, NAME_LAST);

    append_u16(dna, TYPE_CURVE);
    append_u16(dna, 2);
    append_sdna_field(dna, TYPE_ID, NAME_ID);
    append_sdna_field(dna, TYPE_LIST_BASE, NAME_NURB_LIST);

    append_u16(dna, TYPE_NURB);
    append_u16(dna, 12);
    append_sdna_field(dna, TYPE_NURB, NAME_NEXT);
    append_sdna_field(dna, TYPE_NURB, NAME_PREVIOUS);
    append_sdna_field(dna, TYPE_SHORT, NAME_TYPE);
    append_sdna_field(dna, TYPE_INTEGER, NAME_POINTS_U);
    append_sdna_field(dna, TYPE_INTEGER, NAME_POINTS_V);
    append_sdna_field(dna, TYPE_SHORT, NAME_ORDER_U);
    append_sdna_field(dna, TYPE_SHORT, NAME_ORDER_V);
    append_sdna_field(dna, TYPE_SHORT, NAME_FLAG_U);
    append_sdna_field(dna, TYPE_SHORT, NAME_FLAG_V);
    append_sdna_field(dna, TYPE_FLOAT, NAME_KNOTS_U);
    append_sdna_field(dna, TYPE_FLOAT, NAME_KNOTS_V);
    append_sdna_field(dna, TYPE_BPOINT, NAME_CONTROL_POINTS);

    append_u16(dna, TYPE_BPOINT);
    append_u16(dna, 1);
    append_sdna_field(dna, TYPE_FLOAT, NAME_HOMOGENEOUS_COORDINATE);
    return dna;
}

static std::vector<unsigned char>
make_object(const std::string &name, uint64_t data_address)
{
    std::vector<unsigned char> object;
    append_fixed_text(object, "OB" + name, ID_NAME_SIZE);
    object.insert(object.end(), 2, 0);
    append_u64(object, data_address);
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
make_curve()
{
    std::vector<unsigned char> curve;
    append_fixed_text(curve, "CUSurface", ID_NAME_SIZE);
    curve.insert(curve.end(), 2, 0);
    append_u64(curve, NURB_ADDRESS);
    append_u64(curve, CURVE_NURB_ADDRESS);
    return curve;
}

static std::vector<unsigned char>
make_nurb(uint64_t next, uint64_t previous, uint32_t points_u,
    uint32_t points_v, uint16_t order_u, uint16_t order_v,
    uint64_t knots_u, uint64_t knots_v, uint64_t control_points)
{
    std::vector<unsigned char> nurb;
    append_u64(nurb, next);
    append_u64(nurb, previous);
    append_u16(nurb, 4);
    append_u32(nurb, points_u);
    append_u32(nurb, points_v);
    append_u16(nurb, order_u);
    append_u16(nurb, order_v);
    append_u16(nurb, 0);
    append_u16(nurb, 0);
    append_u64(nurb, knots_u);
    append_u64(nurb, knots_v);
    append_u64(nurb, control_points);
    return nurb;
}

static std::vector<unsigned char>
make_curve_control_points()
{
    const std::array<std::array<float, 4>, CURVE_POINT_COUNT> points = {{
	{{0.0f, 0.0f, 2.0f, 1.0f}},
	{{1.0f, 0.0f, 3.0f, 0.75f}},
	{{2.0f, 1.0f, 3.0f, 1.0f}},
	{{3.0f, 1.0f, 2.0f, 1.0f}}
    }};
    std::vector<unsigned char> result;
    for (const auto &point : points) {
	for (float value : point)
	    append_float(result, value);
    }
    return result;
}

static std::vector<unsigned char>
make_control_points()
{
    std::vector<unsigned char> points;
    for (uint32_t v = 0; v < SURFACE_POINT_COUNT_V; ++v) {
	for (uint32_t u = 0; u < SURFACE_POINT_COUNT_U; ++u) {
	    append_float(points, static_cast<float>(u));
	    append_float(points, static_cast<float>(v));
	    append_float(points, (u == 1 || u == 2) && (v == 1 || v == 2) ? 1.0f : 0.0f);
	    append_float(points, u == 1 && v == 1 ? 0.5f : 1.0f);
	}
    }
    return points;
}

static std::vector<unsigned char>
make_knots()
{
    const std::array<float, 7> values = {{0.0f, 0.0f, 0.0f, 0.5f,
	1.0f, 1.0f, 1.0f}};
    std::vector<unsigned char> knots;
    for (float value : values)
	append_float(knots, value);
    return knots;
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
    append_block(file, "OB", OBJECT_ADDRESS, STRUCTURE_OBJECT, 1,
	make_object("Tetrahedron", MESH_ADDRESS));
    append_block(file, "ME", MESH_ADDRESS, STRUCTURE_MESH, 1, make_mesh());
    append_block(file, "DATA", VERTEX_ADDRESS, STRUCTURE_VERTEX,
	TETRAHEDRON_VERTEX_COUNT, make_vertices());
    append_block(file, "DATA", FACE_ADDRESS, STRUCTURE_FACE,
	TETRAHEDRON_FACE_COUNT, make_faces());
    append_block(file, "OB", CURVE_OBJECT_ADDRESS, STRUCTURE_OBJECT, 1,
	make_object("NurbsSurface", CURVE_ADDRESS));
    append_block(file, "CU", CURVE_ADDRESS, STRUCTURE_CURVE, 1, make_curve());
    append_block(file, "DATA", NURB_ADDRESS, STRUCTURE_NURB, 1,
	make_nurb(CURVE_NURB_ADDRESS, 0, SURFACE_POINT_COUNT_U,
	    SURFACE_POINT_COUNT_V, 3, 3, KNOT_U_ADDRESS, KNOT_V_ADDRESS,
	    CONTROL_POINT_ADDRESS));
    append_block(file, "DATA", CURVE_NURB_ADDRESS, STRUCTURE_NURB, 1,
	make_nurb(0, NURB_ADDRESS, CURVE_POINT_COUNT, 1, 3, 0,
	    CURVE_KNOT_ADDRESS, 0, CURVE_CONTROL_POINT_ADDRESS));
    append_block(file, "DATA", CONTROL_POINT_ADDRESS, STRUCTURE_BPOINT,
	SURFACE_POINT_COUNT, make_control_points());
    append_block(file, "DATA", KNOT_U_ADDRESS, STRUCTURE_ID, 7, make_knots());
    append_block(file, "DATA", KNOT_V_ADDRESS, STRUCTURE_ID, 7, make_knots());
    append_block(file, "DATA", CURVE_CONTROL_POINT_ADDRESS, STRUCTURE_BPOINT,
	CURVE_POINT_COUNT, make_curve_control_points());
    append_block(file, "DATA", CURVE_KNOT_ADDRESS, STRUCTURE_ID, 7, make_knots());
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
