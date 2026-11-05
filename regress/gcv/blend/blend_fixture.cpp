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
constexpr uint64_t BEZIER_NURB_ADDRESS = 0xe000;
constexpr uint64_t BEZIER_POINT_ADDRESS = 0xf000;
constexpr uint64_t POLY_NURB_ADDRESS = 0x10000;
constexpr uint64_t POLY_POINT_ADDRESS = 0x11000;
constexpr uint64_t METABALL_OBJECT_ADDRESS = 0x12000;
constexpr uint64_t METABALL_ADDRESS = 0x13000;
constexpr uint64_t META_ELEMENT_ADDRESS = 0x14000;
constexpr uint64_t NEGATIVE_META_ELEMENT_ADDRESS = 0x15000;
constexpr uint64_t TEXT_OBJECT_ADDRESS = 0x16000;
constexpr uint64_t TEXT_CURVE_ADDRESS = 0x17000;
constexpr uint64_t TEXT_STRING_ADDRESS = 0x18000;
constexpr uint64_t POINT_CLOUD_OBJECT_ADDRESS = 0x19000;
constexpr uint64_t POINT_CLOUD_ADDRESS = 0x1a000;
constexpr uint64_t POINT_CLOUD_LAYER_ADDRESS = 0x1b000;
constexpr uint64_t POINT_POSITION_ADDRESS = 0x1c000;
constexpr uint64_t POINT_RADIUS_ADDRESS = 0x1d000;
constexpr size_t ID_NAME_SIZE = 66;
constexpr uint32_t TETRAHEDRON_VERTEX_COUNT = 4;
constexpr uint32_t TETRAHEDRON_FACE_COUNT = 4;
constexpr uint32_t SURFACE_POINT_COUNT_U = 4;
constexpr uint32_t SURFACE_POINT_COUNT_V = 4;
constexpr uint32_t SURFACE_POINT_COUNT = SURFACE_POINT_COUNT_U * SURFACE_POINT_COUNT_V;
constexpr uint32_t CURVE_POINT_COUNT = 4;
constexpr uint32_t BEZIER_POINT_COUNT = 3;
constexpr uint32_t POLY_POINT_COUNT = 4;
constexpr uint16_t SPLINE_TYPE_POLY = 0;
constexpr uint16_t SPLINE_TYPE_BEZIER = 1;
constexpr uint16_t SPLINE_TYPE_NURBS = 4;
constexpr uint16_t SPLINE_CYCLIC = 1;
constexpr uint32_t META_ELEMENT_BALL = 0;
constexpr uint32_t META_ELEMENT_NEGATIVE = 2;
constexpr char TEXT_CONTENT[] = "Blend Text!";
constexpr uint32_t POINT_CLOUD_COUNT = 3;

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
    NAME_NURB_LIST,
    NAME_BEZIER_POINTS,
    NAME_BEZIER_COORDINATES,
    NAME_META_ELEMENTS,
    NAME_META_THRESHOLD,
    NAME_META_X,
    NAME_META_Y,
    NAME_META_Z,
    NAME_META_RADIUS,
    NAME_META_STIFFNESS,
    NAME_META_FLAG,
    NAME_TEXT_STRING,
    NAME_TEXT_LENGTH,
    NAME_TEXT_SIZE,
    NAME_TEXT_EXTRUDE,
    NAME_TEXT_BEVEL,
    NAME_TOTAL_POINTS,
    NAME_POINT_DATA,
    NAME_TOTAL_LAYERS,
    NAME_LAYERS,
    NAME_LAYER_NAME
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
    TYPE_BPOINT,
    TYPE_BEZIER_TRIPLE,
    TYPE_METABALL,
    TYPE_META_ELEMENT,
    TYPE_CUSTOM_DATA,
    TYPE_CUSTOM_DATA_LAYER,
    TYPE_POINT_CLOUD
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
    STRUCTURE_BEZIER_TRIPLE,
    STRUCTURE_METABALL,
    STRUCTURE_META_ELEMENT,
    STRUCTURE_CUSTOM_DATA,
    STRUCTURE_CUSTOM_DATA_LAYER,
    STRUCTURE_POINT_CLOUD,
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
    const std::array<std::string, 54> names = {
	"name[66]", "_pad[2]", "id", "*data", "obmat[4][4]",
	"totvert", "totface", "totpoly", "totloop", "*mvert",
	"*mface", "*mpoly", "*mloop", "co[3]", "v1", "v2", "v3", "v4",
	"*first", "*last", "*next", "*prev", "type", "pntsu", "pntsv",
	"orderu", "orderv", "flagu", "flagv", "*knotsu", "*knotsv",
	"*bp", "vec[4]", "nurb", "*bezt", "vec[3][3]", "elems",
	"thresh", "x", "y", "z", "rad", "s", "flag", "*str", "len",
	"fsize", "extrude", "bevel_depth", "totpoint", "pdata",
	"totlayer", "*layers", "name[68]"
    };
    const std::array<std::string, 22> types = {
	"char", "float", "int", "void", "ID", "Object", "Mesh",
	"MVert", "MFace", "MLoop", "MPoly", "short", "ListBase",
	"Curve", "Nurb", "BPoint", "BezTriple", "MetaBall", "MetaElem",
	"CustomData", "CustomDataLayer", "PointCloud"
    };
    const std::array<uint16_t, 22> lengths = {
	1, 4, 4, 0, 68, 140, 116, 12, 16, 8, 12, 2, 16, 108, 66, 16,
	36, 88, 44, 12, 76, 84
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
    append_u16(dna, 7);
    append_sdna_field(dna, TYPE_ID, NAME_ID);
    append_sdna_field(dna, TYPE_LIST_BASE, NAME_NURB_LIST);
    append_sdna_field(dna, TYPE_CHARACTER, NAME_TEXT_STRING);
    append_sdna_field(dna, TYPE_INTEGER, NAME_TEXT_LENGTH);
    append_sdna_field(dna, TYPE_FLOAT, NAME_TEXT_SIZE);
    append_sdna_field(dna, TYPE_FLOAT, NAME_TEXT_EXTRUDE);
    append_sdna_field(dna, TYPE_FLOAT, NAME_TEXT_BEVEL);

    append_u16(dna, TYPE_NURB);
    append_u16(dna, 13);
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
    append_sdna_field(dna, TYPE_BEZIER_TRIPLE, NAME_BEZIER_POINTS);

    append_u16(dna, TYPE_BPOINT);
    append_u16(dna, 1);
    append_sdna_field(dna, TYPE_FLOAT, NAME_HOMOGENEOUS_COORDINATE);

    append_u16(dna, TYPE_BEZIER_TRIPLE);
    append_u16(dna, 1);
    append_sdna_field(dna, TYPE_FLOAT, NAME_BEZIER_COORDINATES);

    append_u16(dna, TYPE_METABALL);
    append_u16(dna, 3);
    append_sdna_field(dna, TYPE_ID, NAME_ID);
    append_sdna_field(dna, TYPE_LIST_BASE, NAME_META_ELEMENTS);
    append_sdna_field(dna, TYPE_FLOAT, NAME_META_THRESHOLD);

    append_u16(dna, TYPE_META_ELEMENT);
    append_u16(dna, 9);
    append_sdna_field(dna, TYPE_META_ELEMENT, NAME_NEXT);
    append_sdna_field(dna, TYPE_META_ELEMENT, NAME_PREVIOUS);
    append_sdna_field(dna, TYPE_INTEGER, NAME_TYPE);
    append_sdna_field(dna, TYPE_INTEGER, NAME_META_FLAG);
    append_sdna_field(dna, TYPE_FLOAT, NAME_META_X);
    append_sdna_field(dna, TYPE_FLOAT, NAME_META_Y);
    append_sdna_field(dna, TYPE_FLOAT, NAME_META_Z);
    append_sdna_field(dna, TYPE_FLOAT, NAME_META_RADIUS);
    append_sdna_field(dna, TYPE_FLOAT, NAME_META_STIFFNESS);

    append_u16(dna, TYPE_CUSTOM_DATA);
    append_u16(dna, 2);
    append_sdna_field(dna, TYPE_INTEGER, NAME_TOTAL_LAYERS);
    append_sdna_field(dna, TYPE_CUSTOM_DATA_LAYER, NAME_LAYERS);

    append_u16(dna, TYPE_CUSTOM_DATA_LAYER);
    append_u16(dna, 2);
    append_sdna_field(dna, TYPE_CHARACTER, NAME_LAYER_NAME);
    append_sdna_field(dna, TYPE_VOID, NAME_DATA);

    append_u16(dna, TYPE_POINT_CLOUD);
    append_u16(dna, 3);
    append_sdna_field(dna, TYPE_ID, NAME_ID);
    append_sdna_field(dna, TYPE_INTEGER, NAME_TOTAL_POINTS);
    append_sdna_field(dna, TYPE_CUSTOM_DATA, NAME_POINT_DATA);
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
    append_u64(curve, POLY_NURB_ADDRESS);
    append_u64(curve, 0);
    append_u32(curve, 0);
    append_float(curve, 1.0f);
    append_float(curve, 0.0f);
    append_float(curve, 0.0f);
    return curve;
}

static std::vector<unsigned char>
make_text_curve()
{
    std::vector<unsigned char> curve;
    append_fixed_text(curve, "CUText", ID_NAME_SIZE);
    curve.insert(curve.end(), 2, 0);
    append_u64(curve, 0);
    append_u64(curve, 0);
    append_u64(curve, TEXT_STRING_ADDRESS);
    append_u32(curve, static_cast<uint32_t>(sizeof(TEXT_CONTENT) - 1));
    append_float(curve, 1.25f);
    append_float(curve, 0.0f);
    append_float(curve, 0.0f);
    return curve;
}

static std::vector<unsigned char>
make_text_content()
{
    std::vector<unsigned char> text;
    append_fixed_text(text, TEXT_CONTENT, sizeof(TEXT_CONTENT));
    return text;
}

static std::vector<unsigned char>
make_point_cloud()
{
    std::vector<unsigned char> point_cloud;
    append_fixed_text(point_cloud, "PTPoints", ID_NAME_SIZE);
    point_cloud.insert(point_cloud.end(), 2, 0);
    append_u32(point_cloud, POINT_CLOUD_COUNT);
    append_u32(point_cloud, 2);
    append_u64(point_cloud, POINT_CLOUD_LAYER_ADDRESS);
    return point_cloud;
}

static std::vector<unsigned char>
make_point_cloud_layers()
{
    std::vector<unsigned char> layers;
    append_fixed_text(layers, "position", 68);
    append_u64(layers, POINT_POSITION_ADDRESS);
    append_fixed_text(layers, "radius", 68);
    append_u64(layers, POINT_RADIUS_ADDRESS);
    return layers;
}

static std::vector<unsigned char>
make_point_positions()
{
    const std::array<float, POINT_CLOUD_COUNT * 3> positions = {{
	-2.0f, 3.0f, 1.0f, 0.0f, 3.0f, 2.0f, 2.0f, 3.0f, 1.0f
    }};
    std::vector<unsigned char> result;
    for (float coordinate : positions)
	append_float(result, coordinate);
    return result;
}

static std::vector<unsigned char>
make_point_radii()
{
    const std::array<float, POINT_CLOUD_COUNT> radii = {{0.25f, 0.5f, 0.75f}};
    std::vector<unsigned char> result;
    for (float radius : radii)
	append_float(result, radius);
    return result;
}

static std::vector<unsigned char>
make_metaball()
{
    std::vector<unsigned char> metaball;
    append_fixed_text(metaball, "MBTwoElements", ID_NAME_SIZE);
    metaball.insert(metaball.end(), 2, 0);
    append_u64(metaball, META_ELEMENT_ADDRESS);
    append_u64(metaball, NEGATIVE_META_ELEMENT_ADDRESS);
    append_float(metaball, 0.6f);
    return metaball;
}

static std::vector<unsigned char>
make_meta_element(uint64_t next, uint64_t previous, uint32_t flags,
    float x, float y, float z, float radius)
{
    std::vector<unsigned char> element;
    append_u64(element, next);
    append_u64(element, previous);
    append_u32(element, META_ELEMENT_BALL);
    append_u32(element, flags);
    append_float(element, x);
    append_float(element, y);
    append_float(element, z);
    append_float(element, radius);
    append_float(element, 2.0f);
    return element;
}

static std::vector<unsigned char>
make_nurb(uint64_t next, uint64_t previous, uint16_t type, uint32_t points_u,
    uint32_t points_v, uint16_t order_u, uint16_t order_v,
    uint16_t flags_u, uint64_t knots_u, uint64_t knots_v,
    uint64_t control_points, uint64_t bezier_points)
{
    std::vector<unsigned char> nurb;
    append_u64(nurb, next);
    append_u64(nurb, previous);
    append_u16(nurb, type);
    append_u32(nurb, points_u);
    append_u32(nurb, points_v);
    append_u16(nurb, order_u);
    append_u16(nurb, order_v);
    append_u16(nurb, flags_u);
    append_u16(nurb, 0);
    append_u64(nurb, knots_u);
    append_u64(nurb, knots_v);
    append_u64(nurb, control_points);
    append_u64(nurb, bezier_points);
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
make_bezier_points()
{
    const std::array<float, BEZIER_POINT_COUNT * 9> coordinates = {{
	-0.5f, -2.0f, 2.0f, 0.0f, -2.0f, 2.0f, 0.5f, -2.0f, 3.0f,
	1.5f, -2.0f, 3.5f, 2.0f, -2.0f, 3.0f, 2.5f, -2.0f, 2.5f,
	3.5f, -2.0f, 1.5f, 4.0f, -2.0f, 2.0f, 4.5f, -2.0f, 2.0f
    }};
    std::vector<unsigned char> result;
    for (float coordinate : coordinates)
	append_float(result, coordinate);
    return result;
}

static std::vector<unsigned char>
make_poly_points()
{
    const std::array<std::array<float, 4>, POLY_POINT_COUNT> points = {{
	{{0.0f, -4.0f, 1.0f, 1.0f}},
	{{2.0f, -4.0f, 1.0f, 1.0f}},
	{{2.0f, -4.0f, 3.0f, 1.0f}},
	{{0.0f, -4.0f, 3.0f, 1.0f}}
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
    append_block(file, "OB", METABALL_OBJECT_ADDRESS, STRUCTURE_OBJECT, 1,
	make_object("Metaball", METABALL_ADDRESS));
    append_block(file, "OB", TEXT_OBJECT_ADDRESS, STRUCTURE_OBJECT, 1,
	make_object("Text", TEXT_CURVE_ADDRESS));
    append_block(file, "CU", CURVE_ADDRESS, STRUCTURE_CURVE, 1, make_curve());
    append_block(file, "CU", TEXT_CURVE_ADDRESS, STRUCTURE_CURVE, 1,
	make_text_curve());
    append_block(file, "DATA", NURB_ADDRESS, STRUCTURE_NURB, 1,
	make_nurb(CURVE_NURB_ADDRESS, 0, SPLINE_TYPE_NURBS,
	    SURFACE_POINT_COUNT_U, SURFACE_POINT_COUNT_V, 3, 3, 0,
	    KNOT_U_ADDRESS, KNOT_V_ADDRESS, CONTROL_POINT_ADDRESS, 0));
    append_block(file, "DATA", CURVE_NURB_ADDRESS, STRUCTURE_NURB, 1,
	make_nurb(BEZIER_NURB_ADDRESS, NURB_ADDRESS, SPLINE_TYPE_NURBS,
	    CURVE_POINT_COUNT, 1, 3, 0, 0, CURVE_KNOT_ADDRESS, 0,
	    CURVE_CONTROL_POINT_ADDRESS, 0));
    append_block(file, "DATA", BEZIER_NURB_ADDRESS, STRUCTURE_NURB, 1,
	make_nurb(POLY_NURB_ADDRESS, CURVE_NURB_ADDRESS, SPLINE_TYPE_BEZIER,
	    BEZIER_POINT_COUNT, 1, 0, 0, 0, 0, 0, 0,
	    BEZIER_POINT_ADDRESS));
    append_block(file, "DATA", POLY_NURB_ADDRESS, STRUCTURE_NURB, 1,
	make_nurb(0, BEZIER_NURB_ADDRESS, SPLINE_TYPE_POLY, POLY_POINT_COUNT,
	    1, 0, 0, SPLINE_CYCLIC, 0, 0, POLY_POINT_ADDRESS, 0));
    append_block(file, "DATA", CONTROL_POINT_ADDRESS, STRUCTURE_BPOINT,
	SURFACE_POINT_COUNT, make_control_points());
    append_block(file, "DATA", KNOT_U_ADDRESS, STRUCTURE_ID, 7, make_knots());
    append_block(file, "DATA", KNOT_V_ADDRESS, STRUCTURE_ID, 7, make_knots());
    append_block(file, "DATA", CURVE_CONTROL_POINT_ADDRESS, STRUCTURE_BPOINT,
	CURVE_POINT_COUNT, make_curve_control_points());
    append_block(file, "DATA", CURVE_KNOT_ADDRESS, STRUCTURE_ID, 7, make_knots());
    append_block(file, "DATA", BEZIER_POINT_ADDRESS, STRUCTURE_BEZIER_TRIPLE,
	BEZIER_POINT_COUNT, make_bezier_points());
    append_block(file, "DATA", POLY_POINT_ADDRESS, STRUCTURE_BPOINT,
	POLY_POINT_COUNT, make_poly_points());
    append_block(file, "MB", METABALL_ADDRESS, STRUCTURE_METABALL, 1,
	make_metaball());
    append_block(file, "DATA", META_ELEMENT_ADDRESS, STRUCTURE_META_ELEMENT, 1,
	make_meta_element(NEGATIVE_META_ELEMENT_ADDRESS, 0, 0, 6.0f, 0.0f,
	    1.5f, 1.5f));
    append_block(file, "DATA", NEGATIVE_META_ELEMENT_ADDRESS,
	STRUCTURE_META_ELEMENT, 1, make_meta_element(0, META_ELEMENT_ADDRESS,
	    META_ELEMENT_NEGATIVE, 6.5f, 0.0f, 1.5f, 0.5f));
    append_block(file, "DATA", TEXT_STRING_ADDRESS, STRUCTURE_ID,
	static_cast<uint32_t>(sizeof(TEXT_CONTENT)), make_text_content());
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
