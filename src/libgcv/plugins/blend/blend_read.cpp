/*                     B L E N D _ R E A D . C P P
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
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
#include <array>
#include <cctype>
#include <climits>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <limits>
#include <map>
#include <memory>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <zlib.h>

#include "bg/polygon.h"
#include "brep/defines.h"
#include "bu/path.h"
#include "gcv/api.h"
#include "raytrace.h"
#include "rt/geom.h"
#include "rt/primitives/annot.h"
#include "vmath.h"
#include "wdb.h"

namespace {

constexpr size_t BLEND_HEADER_SIZE = 12;
constexpr size_t READ_CHUNK_SIZE = 64 * 1024;
constexpr size_t MAX_BLEND_SIZE = static_cast<size_t>(2) * 1024 * 1024 * 1024;
constexpr std::array<unsigned char, 4> ZSTD_MAGIC = {0x28, 0xb5, 0x2f, 0xfd};
constexpr int64_t SPLINE_TYPE_MASK = 7;
constexpr int64_t SPLINE_TYPE_POLY = 0;
constexpr int64_t SPLINE_TYPE_BEZIER = 1;
constexpr int64_t SPLINE_TYPE_NURBS = 4;
constexpr int64_t SPLINE_CYCLIC = 1;
constexpr int64_t SPLINE_BEZIER_KNOTS = 4;
constexpr int64_t META_ELEMENT_BALL = 0;
constexpr int64_t META_ELEMENT_NEGATIVE = 2;
constexpr fastf_t METABALL_THRESHOLD = 1.0;
constexpr double OBJECT_SCALE_TOLERANCE = 1.0e-8;
constexpr fastf_t DEFAULT_POINT_RADIUS = 0.01;

class ParseError : public std::runtime_error {
public:
    explicit ParseError(const std::string &message) : std::runtime_error(message) {}
};

class ByteReader {
public:
    ByteReader(const unsigned char *data, size_t size, bool little_endian) :
	data_(data), size_(size), little_endian_(little_endian) {}

    size_t position() const { return position_; }
    size_t remaining() const { return size_ - position_; }

    void seek(size_t position)
    {
	if (position > size_)
	    throw ParseError("seek extends beyond available data");
	position_ = position;
    }

    void skip(size_t count)
    {
	if (count > remaining())
	    throw ParseError("read extends beyond available data");
	position_ += count;
    }

    uint16_t u16()
    {
	const unsigned char *bytes = take(sizeof(uint16_t));
	if (little_endian_)
	    return static_cast<uint16_t>(bytes[0]) |
		(static_cast<uint16_t>(bytes[1]) << 8);
	return (static_cast<uint16_t>(bytes[0]) << 8) |
	    static_cast<uint16_t>(bytes[1]);
    }

    uint32_t u32()
    {
	const unsigned char *bytes = take(sizeof(uint32_t));
	uint32_t value = 0;
	for (size_t i = 0; i < sizeof(uint32_t); ++i) {
	    size_t index = little_endian_ ? sizeof(uint32_t) - i - 1 : i;
	    value = (value << 8) | bytes[index];
	}
	return value;
    }

    uint64_t u64()
    {
	const unsigned char *bytes = take(sizeof(uint64_t));
	uint64_t value = 0;
	for (size_t i = 0; i < sizeof(uint64_t); ++i) {
	    size_t index = little_endian_ ? sizeof(uint64_t) - i - 1 : i;
	    value = (value << 8) | bytes[index];
	}
	return value;
    }

    std::string fixed_string(size_t count)
    {
	const unsigned char *bytes = take(count);
	return std::string(reinterpret_cast<const char *>(bytes), count);
    }

    std::string c_string()
    {
	size_t start = position_;
	while (position_ < size_ && data_[position_] != '\0')
	    ++position_;
	if (position_ == size_)
	    throw ParseError("unterminated SDNA string");
	std::string result(reinterpret_cast<const char *>(data_ + start), position_ - start);
	++position_;
	return result;
    }

    const unsigned char *take(size_t count)
    {
	if (count > remaining())
	    throw ParseError("read extends beyond available data");
	const unsigned char *result = data_ + position_;
	position_ += count;
	return result;
    }

private:
    const unsigned char *data_ = nullptr;
    size_t size_ = 0;
    size_t position_ = 0;
    bool little_endian_ = true;
};

struct DnaField {
    std::string type;
    std::string name;
    size_t offset = 0;
    size_t size = 0;
    size_t array_count = 1;
    bool pointer = false;
};

struct DnaStruct {
    std::string name;
    size_t size = 0;
    std::vector<DnaField> fields;

    const DnaField *field(const std::string &field_name) const
    {
	for (const DnaField &candidate : fields) {
	    if (candidate.name == field_name)
		return &candidate;
	}
	return nullptr;
    }
};

struct BlendBlock {
    std::string code;
    size_t data_offset = 0;
    size_t size = 0;
    uint64_t old_address = 0;
    uint32_t sdna_index = 0;
    uint32_t count = 0;
};

struct DataView {
    const unsigned char *data = nullptr;
    size_t size = 0;
    const DnaStruct *structure = nullptr;

    explicit operator bool() const { return data != nullptr && structure != nullptr; }
};

struct BlendFile {
    std::vector<unsigned char> bytes;
    std::vector<BlendBlock> blocks;
    std::vector<DnaStruct> structures;
    std::map<std::string, size_t> structure_indices;
    size_t pointer_size = 0;
    bool little_endian = true;
    int version = 0;

    const DnaStruct *structure(const std::string &name) const
    {
	auto found = structure_indices.find(name);
	return found == structure_indices.end() ? nullptr : &structures[found->second];
    }
};

static std::vector<unsigned char>
read_uncompressed(const char *path)
{
    std::ifstream input(path, std::ios::binary);
    if (!input)
	throw ParseError(std::string("cannot open input file: ") + path);
    input.seekg(0, std::ios::end);
    std::streamoff length = input.tellg();
    if (length < 0 || static_cast<uint64_t>(length) > MAX_BLEND_SIZE)
	throw ParseError("input file is too large");
    input.seekg(0, std::ios::beg);
    std::vector<unsigned char> bytes(static_cast<size_t>(length));
    if (!bytes.empty() && !input.read(reinterpret_cast<char *>(bytes.data()), length))
	throw ParseError("could not read the complete input file");
    return bytes;
}

static std::vector<unsigned char>
read_gzip(const char *path)
{
    gzFile input = gzopen(path, "rb");
    if (!input)
	throw ParseError(std::string("cannot open compressed input file: ") + path);

    std::vector<unsigned char> bytes;
    std::array<unsigned char, READ_CHUNK_SIZE> chunk;
    int count = 0;
    while ((count = gzread(input, chunk.data(), static_cast<unsigned int>(chunk.size()))) > 0) {
	if (static_cast<size_t>(count) > MAX_BLEND_SIZE - bytes.size()) {
	    gzclose(input);
	    throw ParseError("decompressed input file is too large");
	}
	bytes.insert(bytes.end(), chunk.begin(), chunk.begin() + count);
    }
    if (count < 0) {
	int error_number = Z_OK;
	const char *message = gzerror(input, &error_number);
	std::string error = message ? message : "unknown gzip error";
	gzclose(input);
	throw ParseError("gzip decompression failed: " + error);
    }
    if (gzclose(input) != Z_OK)
	throw ParseError("gzip stream did not close cleanly");
    return bytes;
}

static std::vector<unsigned char>
read_blend_bytes(const char *path)
{
    std::vector<unsigned char> bytes = read_uncompressed(path);
    if (bytes.size() >= 2 && bytes[0] == 0x1f && bytes[1] == 0x8b)
	return read_gzip(path);
    if (bytes.size() >= ZSTD_MAGIC.size() &&
	std::equal(ZSTD_MAGIC.begin(), ZSTD_MAGIC.end(), bytes.begin()))
	throw ParseError("Zstandard-compressed Blender files are not supported by this build");
    return bytes;
}

static size_t
array_element_count(const std::string &declaration)
{
    size_t count = 1;
    size_t left = declaration.find('[');
    while (left != std::string::npos) {
	size_t right = declaration.find(']', left + 1);
	if (right == std::string::npos)
	    throw ParseError("invalid array declaration in SDNA: " + declaration);
	std::string dimension = declaration.substr(left + 1, right - left - 1);
	if (dimension.empty() || !std::all_of(dimension.begin(), dimension.end(),
		[](unsigned char character) { return std::isdigit(character) != 0; }))
	    throw ParseError("invalid array dimension in SDNA: " + declaration);
	size_t value = static_cast<size_t>(std::stoull(dimension));
	if (value == 0 || count > std::numeric_limits<size_t>::max() / value)
	    throw ParseError("invalid array size in SDNA: " + declaration);
	count *= value;
	left = declaration.find('[', right + 1);
    }
    return count;
}

static std::string
field_identifier(const std::string &declaration)
{
    size_t end = declaration.find('[');
    std::string name = declaration.substr(0, end);
    name.erase(std::remove_if(name.begin(), name.end(),
	[](char character) { return character == '*' || character == '(' || character == ')'; }), name.end());
    return name;
}

static void
expect_tag(ByteReader &reader, const char *expected)
{
    std::string actual = reader.fixed_string(4);
    if (actual != expected)
	throw ParseError("expected SDNA tag " + std::string(expected) + ", found " + actual);
}

static void
align_four(ByteReader &reader)
{
    size_t padding = (4 - reader.position() % 4) % 4;
    reader.skip(padding);
}

static void
parse_sdna(BlendFile &file, const BlendBlock &dna_block)
{
    ByteReader reader(file.bytes.data() + dna_block.data_offset, dna_block.size,
	file.little_endian);
    expect_tag(reader, "SDNA");
    expect_tag(reader, "NAME");

    uint32_t name_count = reader.u32();
    std::vector<std::string> names;
    names.reserve(name_count);
    for (uint32_t i = 0; i < name_count; ++i)
	names.push_back(reader.c_string());
    align_four(reader);

    expect_tag(reader, "TYPE");
    uint32_t type_count = reader.u32();
    std::vector<std::string> types;
    types.reserve(type_count);
    for (uint32_t i = 0; i < type_count; ++i)
	types.push_back(reader.c_string());
    align_four(reader);

    expect_tag(reader, "TLEN");
    std::vector<uint16_t> type_lengths;
    type_lengths.reserve(type_count);
    for (uint32_t i = 0; i < type_count; ++i)
	type_lengths.push_back(reader.u16());
    align_four(reader);

    expect_tag(reader, "STRC");
    uint32_t structure_count = reader.u32();
    file.structures.reserve(structure_count);
    for (uint32_t i = 0; i < structure_count; ++i) {
	uint16_t type_index = reader.u16();
	uint16_t field_count = reader.u16();
	if (type_index >= types.size())
	    throw ParseError("SDNA structure has an invalid type index");
	DnaStruct structure;
	structure.name = types[type_index];
	structure.size = type_lengths[type_index];
	structure.fields.reserve(field_count);
	size_t offset = 0;
	for (uint16_t field_index = 0; field_index < field_count; ++field_index) {
	    uint16_t field_type = reader.u16();
	    uint16_t field_name = reader.u16();
	    if (field_type >= types.size() || field_name >= names.size())
		throw ParseError("SDNA field has an invalid dictionary index");
	    const std::string &declaration = names[field_name];
	    DnaField field;
	    field.type = types[field_type];
	    field.name = field_identifier(declaration);
	    field.pointer = declaration.find('*') != std::string::npos;
	    field.array_count = array_element_count(declaration);
	    field.size = (field.pointer ? file.pointer_size : type_lengths[field_type]) * field.array_count;
	    field.offset = offset;
	    if (field.size > std::numeric_limits<size_t>::max() - offset)
		throw ParseError("SDNA structure size overflows host limits");
	    offset += field.size;
	    structure.fields.push_back(std::move(field));
	}
	if (offset > structure.size)
	    throw ParseError("SDNA fields exceed declared structure size for " + structure.name);
	file.structure_indices[structure.name] = file.structures.size();
	file.structures.push_back(std::move(structure));
    }
}

static BlendFile
parse_blend_file(const char *path)
{
    BlendFile file;
    file.bytes = read_blend_bytes(path);
    if (file.bytes.size() < BLEND_HEADER_SIZE)
	throw ParseError("file is shorter than the Blender header");
    if (std::memcmp(file.bytes.data(), "BLENDER", 7) != 0)
	throw ParseError("file does not begin with the BLENDER signature");

    if (std::isdigit(file.bytes[7]))
	throw ParseError("extended Blender file headers are not supported");
    if (file.bytes[7] == '_')
	file.pointer_size = 4;
    else if (file.bytes[7] == '-')
	file.pointer_size = 8;
    else
	throw ParseError("header specifies an unknown pointer size");

    if (file.bytes[8] == 'v')
	file.little_endian = true;
    else if (file.bytes[8] == 'V')
	file.little_endian = false;
    else
	throw ParseError("header specifies an unknown byte order");

    file.version = 0;
    for (size_t i = 9; i < BLEND_HEADER_SIZE; ++i) {
	if (!std::isdigit(file.bytes[i]))
	    throw ParseError("header contains an invalid Blender version");
	file.version = file.version * 10 + (file.bytes[i] - '0');
    }

    ByteReader reader(file.bytes.data(), file.bytes.size(), file.little_endian);
    reader.seek(BLEND_HEADER_SIZE);
    size_t dna_index = std::numeric_limits<size_t>::max();
    bool found_end = false;
    while (reader.remaining() > 0) {
	size_t header_size = 4 + sizeof(uint32_t) * 3 + file.pointer_size;
	if (reader.remaining() < header_size)
	    throw ParseError("truncated file block header");
	BlendBlock block;
	block.code = reader.fixed_string(4);
	while (!block.code.empty() && block.code.back() == '\0')
	    block.code.pop_back();
	block.size = reader.u32();
	block.old_address = file.pointer_size == 8 ? reader.u64() : reader.u32();
	block.sdna_index = reader.u32();
	block.count = reader.u32();
	block.data_offset = reader.position();
	if (block.size > reader.remaining())
	    throw ParseError("file block " + block.code + " extends beyond end of file");
	file.blocks.push_back(block);
	if (block.code == "DNA1")
	    dna_index = file.blocks.size() - 1;
	reader.skip(block.size);
	if (block.code == "ENDB") {
	    found_end = true;
	    break;
	}
    }
    if (!found_end)
	throw ParseError("file has no ENDB terminator");
    if (dna_index == std::numeric_limits<size_t>::max())
	throw ParseError("file has no DNA1 schema block");

    parse_sdna(file, file.blocks[dna_index]);
    for (const BlendBlock &block : file.blocks) {
	if (block.code != "DNA1" && block.code != "ENDB" &&
	    block.count > 0 && block.sdna_index >= file.structures.size())
	    throw ParseError("file block " + block.code + " has an invalid SDNA index");
    }
    return file;
}

static const BlendBlock *
find_block(const BlendFile &file, uint64_t address)
{
    if (address == 0)
	return nullptr;
    for (const BlendBlock &block : file.blocks) {
	if (address >= block.old_address && address - block.old_address < block.size)
	    return &block;
    }
    return nullptr;
}

static DataView
block_view(const BlendFile &file, const BlendBlock &block)
{
    if (block.sdna_index >= file.structures.size())
	return {};
    return {file.bytes.data() + block.data_offset, block.size,
	&file.structures[block.sdna_index]};
}

static DataView
address_view(const BlendFile &file, uint64_t address)
{
    const BlendBlock *block = find_block(file, address);
    if (!block)
	return {};
    size_t offset = static_cast<size_t>(address - block->old_address);
    DataView view = block_view(file, *block);
    view.data += offset;
    view.size -= offset;
    return view;
}

static const DnaField *
require_field(const DataView &view, const std::string &name)
{
    if (!view.structure)
	throw ParseError("attempted field access without an SDNA structure");
    const DnaField *field = view.structure->field(name);
    if (!field)
	throw ParseError("structure " + view.structure->name + " has no field " + name);
    if (field->offset > view.size || field->size > view.size - field->offset)
	throw ParseError("field " + view.structure->name + "." + name + " exceeds its file block");
    return field;
}

static DataView
embedded_view(const BlendFile &file, const DataView &view, const std::string &name)
{
    const DnaField *field = require_field(view, name);
    if (field->pointer)
	throw ParseError("field " + view.structure->name + "." + name + " is not embedded");
    const DnaStruct *structure = file.structure(field->type);
    if (!structure)
	throw ParseError("field " + view.structure->name + "." + name + " has unknown type " + field->type);
    return {view.data + field->offset, field->size, structure};
}

static uint64_t
unsigned_value(const BlendFile &file, const unsigned char *data, size_t size)
{
    ByteReader reader(data, size, file.little_endian);
    switch (size) {
	case 1: return reader.take(1)[0];
	case 2: return reader.u16();
	case 4: return reader.u32();
	case 8: return reader.u64();
	default: throw ParseError("unsupported integer width in SDNA field");
    }
}

static uint64_t
pointer_value(const BlendFile &file, const DataView &view, const std::string &name)
{
    const DnaField *field = require_field(view, name);
    if (!field->pointer || field->array_count != 1)
	throw ParseError("field " + view.structure->name + "." + name + " is not a pointer");
    return unsigned_value(file, view.data + field->offset, file.pointer_size);
}

static int64_t
integer_value(const BlendFile &file, const DataView &view, const std::string &name)
{
    const DnaField *field = require_field(view, name);
    if (field->pointer || field->array_count != 1)
	throw ParseError("field " + view.structure->name + "." + name + " is not a scalar integer");
    uint64_t value = unsigned_value(file, view.data + field->offset, field->size);
    if (field->size < sizeof(uint64_t) && (value & (uint64_t(1) << (field->size * 8 - 1))))
	value |= ~uint64_t(0) << (field->size * 8);
    return static_cast<int64_t>(value);
}

static float
float_value(const BlendFile &file, const unsigned char *data)
{
    uint32_t bits = static_cast<uint32_t>(unsigned_value(file, data, sizeof(uint32_t)));
    float value;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

static double
float_field(const BlendFile &file, const DataView &view, const std::string &name)
{
    const DnaField *field = require_field(view, name);
    if (field->pointer || field->array_count != 1 || field->size != sizeof(float))
	throw ParseError("field " + view.structure->name + "." + name +
	    " is not a scalar float");
    double value = float_value(file, view.data + field->offset);
    if (!std::isfinite(value))
	throw ParseError("field " + view.structure->name + "." + name +
	    " is not finite");
    return value;
}

static std::string
string_value(const DataView &view, const std::string &name)
{
    const DnaField *field = require_field(view, name);
    if (field->pointer || field->type != "char")
	throw ParseError("field " + view.structure->name + "." + name + " is not a character array");
    const char *characters = reinterpret_cast<const char *>(view.data + field->offset);
    size_t length = 0;
    while (length < field->size && characters[length] != '\0')
	++length;
    return std::string(characters, length);
}

struct MeshGeometry {
    std::vector<fastf_t> vertices;
    std::vector<int> faces;
};

static DataView
array_view(const BlendFile &file, uint64_t address, const std::string &type,
    size_t count)
{
    const DnaStruct *structure = file.structure(type);
    if (!structure)
	throw ParseError("file schema does not define " + type);
    DataView view = address_view(file, address);
    if (!view)
	throw ParseError("pointer to " + type + " does not reference a file block");
    if (count > 0 && structure->size > view.size / count)
	throw ParseError("array of " + type + " extends beyond its file block");
    view.structure = structure;
    view.size = structure->size * count;
    return view;
}

static DataView
array_element(const DataView &array, size_t index)
{
    if (!array.structure || array.structure->size == 0 ||
	index >= array.size / array.structure->size)
	throw ParseError("array element index is out of range");
    return {array.data + index * array.structure->size,
	array.structure->size, array.structure};
}

static size_t
checked_count(int64_t value, const std::string &label)
{
    if (value < 0 || static_cast<uint64_t>(value) > std::numeric_limits<size_t>::max())
	throw ParseError(label + " has an invalid count");
    return static_cast<size_t>(value);
}

static size_t
checked_product(size_t first, size_t second, const std::string &label)
{
    if (second > 0 && first > std::numeric_limits<size_t>::max() / second)
	throw ParseError(label + " is too large");
    return first * second;
}

static std::array<double, 16>
object_matrix(const BlendFile &file, const DataView &object)
{
    std::array<double, 16> matrix = {1.0, 0.0, 0.0, 0.0,
	0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0,
	0.0, 0.0, 0.0, 1.0};
    const DnaField *field = object.structure->field("obmat");
    if (!field || field->pointer || field->size != sizeof(float) * 16)
	return matrix;
    for (size_t row = 0; row < 4; ++row) {
	for (size_t column = 0; column < 4; ++column) {
	    matrix[row * 4 + column] = float_value(file,
		object.data + field->offset + (column * 4 + row) * sizeof(float));
	}
    }
    return matrix;
}

static void
read_vertices(const BlendFile &file, const DataView &mesh, MeshGeometry &geometry)
{
    size_t vertex_count = checked_count(integer_value(file, mesh, "totvert"), "mesh vertex count");
    uint64_t vertex_address = pointer_value(file, mesh, "mvert");
    DataView vertices = array_view(file, vertex_address, "MVert", vertex_count);
    if (vertex_count > geometry.vertices.max_size() / 3)
	throw ParseError("mesh has too many vertices");
    geometry.vertices.resize(vertex_count * 3);
    for (size_t vertex_index = 0; vertex_index < vertex_count; ++vertex_index) {
	DataView vertex = array_element(vertices, vertex_index);
	const DnaField *coordinate = require_field(vertex, "co");
	if (coordinate->pointer || coordinate->size != sizeof(float) * 3)
	    throw ParseError("MVert.co is not a three-component float array");
	for (size_t axis = 0; axis < 3; ++axis)
	    geometry.vertices[vertex_index * 3 + axis] =
		static_cast<fastf_t>(float_value(file,
		    vertex.data + coordinate->offset + axis * sizeof(float)));
    }
}

static uint64_t
custom_layer_address(const BlendFile &file, const DataView &mesh,
    const std::string &custom_data_name, const std::string &layer_name)
{
    DataView custom_data = embedded_view(file, mesh, custom_data_name);
    size_t layer_count = checked_count(integer_value(file, custom_data, "totlayer"),
	custom_data_name + " layer count");
    if (layer_count == 0)
	return 0;
    DataView layers = array_view(file, pointer_value(file, custom_data, "layers"),
	"CustomDataLayer", layer_count);
    for (size_t index = 0; index < layer_count; ++index) {
	DataView layer = array_element(layers, index);
	if (string_value(layer, "name") == layer_name)
	    return pointer_value(file, layer, "data");
    }
    return 0;
}

static DataView
raw_array_view(const BlendFile &file, uint64_t address, size_t element_size,
    size_t count, const std::string &description)
{
    DataView view = address_view(file, address);
    if (!view)
	throw ParseError(description + " does not reference a file block");
    if (count > 0 && element_size > view.size / count)
	throw ParseError(description + " extends beyond its file block");
    view.size = element_size * count;
    return view;
}

static std::string
pointed_string_value(const BlendFile &file, const DataView &view,
    const std::string &pointer_name, const std::string &length_name)
{
    size_t length = checked_count(integer_value(file, view, length_name),
	view.structure->name + " string length");
    DataView characters = raw_array_view(file, pointer_value(file, view,
	pointer_name), sizeof(char), length + 1, view.structure->name + " string");
    const char *text = reinterpret_cast<const char *>(characters.data);
    size_t actual_length = 0;
    while (actual_length < length && text[actual_length] != '\0')
	++actual_length;
    return std::string(text, actual_length);
}

static void
read_attribute_vertices(const BlendFile &file, const DataView &mesh,
    MeshGeometry &geometry)
{
    size_t vertex_count = checked_count(integer_value(file, mesh, "totvert"), "mesh vertex count");
    uint64_t position_address = custom_layer_address(file, mesh, "vdata", "position");
    if (position_address == 0)
	throw ParseError("mesh vertex data has no position layer");
    DataView positions = raw_array_view(file, position_address, sizeof(float) * 3,
	vertex_count, "mesh position layer");
    if (vertex_count > geometry.vertices.max_size() / 3)
	throw ParseError("mesh has too many vertices");
    geometry.vertices.resize(vertex_count * 3);
    for (size_t component = 0; component < vertex_count * 3; ++component)
	geometry.vertices[component] = static_cast<fastf_t>(
	    float_value(file, positions.data + component * sizeof(float)));
}

static void
add_triangle(MeshGeometry &geometry, size_t vertex_count,
    int64_t first, int64_t second, int64_t third)
{
    if (first < 0 || second < 0 || third < 0 ||
	static_cast<uint64_t>(first) >= vertex_count ||
	static_cast<uint64_t>(second) >= vertex_count ||
	static_cast<uint64_t>(third) >= vertex_count)
	throw ParseError("mesh face references a vertex outside the vertex array");
    if (first > INT_MAX || second > INT_MAX || third > INT_MAX)
	throw ParseError("mesh vertex index exceeds BRL-CAD limits");
    geometry.faces.push_back(static_cast<int>(first));
    geometry.faces.push_back(static_cast<int>(second));
    geometry.faces.push_back(static_cast<int>(third));
}

static void
triangulate_polygon(MeshGeometry &geometry, const std::vector<int64_t> &polygon)
{
    const size_t vertex_count = geometry.vertices.size() / 3;
    for (int64_t vertex : polygon) {
	if (vertex < 0 || static_cast<uint64_t>(vertex) >= vertex_count)
	    throw ParseError("mesh polygon references a vertex outside the vertex array");
    }
    if (polygon.size() == 3) {
	add_triangle(geometry, vertex_count, polygon[0], polygon[1], polygon[2]);
	return;
    }

    std::array<double, 3> normal = {0.0, 0.0, 0.0};
    for (size_t index = 0; index < polygon.size(); ++index) {
	size_t current = static_cast<size_t>(polygon[index]) * 3;
	size_t next = static_cast<size_t>(polygon[(index + 1) % polygon.size()]) * 3;
	normal[0] += (geometry.vertices[current + 1] - geometry.vertices[next + 1]) *
	    (geometry.vertices[current + 2] + geometry.vertices[next + 2]);
	normal[1] += (geometry.vertices[current + 2] - geometry.vertices[next + 2]) *
	    (geometry.vertices[current] + geometry.vertices[next]);
	normal[2] += (geometry.vertices[current] - geometry.vertices[next]) *
	    (geometry.vertices[current + 1] + geometry.vertices[next + 1]);
    }
    size_t drop_axis = static_cast<size_t>(std::max_element(normal.begin(), normal.end(),
	[](double left, double right) { return std::abs(left) < std::abs(right); }) - normal.begin());
    if (std::abs(normal[drop_axis]) < SMALL_FASTF)
	throw ParseError("mesh contains a degenerate polygon");

    std::vector<fastf_t> projected;
    projected.reserve(polygon.size() * 2);
    for (int64_t vertex : polygon) {
	size_t offset = static_cast<size_t>(vertex) * 3;
	projected.push_back(geometry.vertices[offset + (drop_axis + 1) % 3]);
	projected.push_back(geometry.vertices[offset + (drop_axis + 2) % 3]);
    }
    double signed_area = 0.0;
    for (size_t index = 0; index < polygon.size(); ++index) {
	size_t next = (index + 1) % polygon.size();
	signed_area += projected[index * 2] * projected[next * 2 + 1] -
	    projected[next * 2] * projected[index * 2 + 1];
    }

    std::vector<int64_t> ordered = polygon;
    if (signed_area < 0.0) {
	std::reverse(ordered.begin(), ordered.end());
	projected.clear();
	for (int64_t vertex : ordered) {
	    size_t offset = static_cast<size_t>(vertex) * 3;
	    projected.push_back(geometry.vertices[offset + (drop_axis + 1) % 3]);
	    projected.push_back(geometry.vertices[offset + (drop_axis + 2) % 3]);
	}
    }

    int *local_faces = nullptr;
    int face_count = 0;
    int result = bg_poly_triangulate(&local_faces, &face_count, nullptr, nullptr,
	nullptr, 0, reinterpret_cast<const point2d_t *>(projected.data()),
	projected.size() / 2, TRI_EAR_CLIPPING);
    if (result != 0 || !local_faces || face_count <= 0) {
	if (local_faces)
	    bu_free(local_faces, "BLEND polygon triangulation");
	throw ParseError("could not triangulate a mesh polygon");
    }
    for (int face = 0; face < face_count; ++face) {
	add_triangle(geometry, vertex_count,
	    ordered[static_cast<size_t>(local_faces[face * 3])],
	    ordered[static_cast<size_t>(local_faces[face * 3 + 1])],
	    ordered[static_cast<size_t>(local_faces[face * 3 + 2])]);
    }
    bu_free(local_faces, "BLEND polygon triangulation");
}

static void
read_polygon_faces(const BlendFile &file, const DataView &mesh, MeshGeometry &geometry)
{
    size_t loop_count = checked_count(integer_value(file, mesh, "totloop"), "mesh loop count");
    size_t polygon_count = checked_count(integer_value(file, mesh, "totpoly"), "mesh polygon count");
    DataView loops = array_view(file, pointer_value(file, mesh, "mloop"), "MLoop", loop_count);
    DataView polygons = array_view(file, pointer_value(file, mesh, "mpoly"), "MPoly", polygon_count);

    for (size_t polygon_index = 0; polygon_index < polygon_count; ++polygon_index) {
	DataView polygon_view = array_element(polygons, polygon_index);
	size_t first_loop = checked_count(integer_value(file, polygon_view, "loopstart"), "polygon loop start");
	size_t polygon_loops = checked_count(integer_value(file, polygon_view, "totloop"), "polygon loop count");
	if (polygon_loops < 3) {
	    bu_log("BLEND Reader: skipping polygon %zu with fewer than three corners\n", polygon_index);
	    continue;
	}
	if (first_loop > loop_count || polygon_loops > loop_count - first_loop)
	    throw ParseError("polygon loop range exceeds the mesh loop array");
	std::vector<int64_t> polygon_vertices;
	polygon_vertices.reserve(polygon_loops);
	for (size_t corner = 0; corner < polygon_loops; ++corner)
	    polygon_vertices.push_back(integer_value(file,
		array_element(loops, first_loop + corner), "v"));
	triangulate_polygon(geometry, polygon_vertices);
    }
}

static void
read_legacy_faces(const BlendFile &file, const DataView &mesh, MeshGeometry &geometry)
{
    size_t face_count = checked_count(integer_value(file, mesh, "totface"), "mesh face count");
    DataView faces = array_view(file, pointer_value(file, mesh, "mface"), "MFace", face_count);
    for (size_t face_index = 0; face_index < face_count; ++face_index) {
	DataView face = array_element(faces, face_index);
	int64_t first = integer_value(file, face, "v1");
	int64_t second = integer_value(file, face, "v2");
	int64_t third = integer_value(file, face, "v3");
	int64_t fourth = integer_value(file, face, "v4");
	std::vector<int64_t> polygon = {first, second, third};
	if (fourth != 0)
	    polygon.push_back(fourth);
	triangulate_polygon(geometry, polygon);
    }
}

static void
read_attribute_faces(const BlendFile &file, const DataView &mesh, MeshGeometry &geometry)
{
    size_t loop_count = checked_count(integer_value(file, mesh, "totloop"), "mesh loop count");
    size_t polygon_count = checked_count(integer_value(file, mesh, "totpoly"), "mesh polygon count");
    uint64_t corner_address = custom_layer_address(file, mesh, "ldata", ".corner_vert");
    if (corner_address == 0)
	throw ParseError("mesh corner data has no .corner_vert layer");
    DataView corners = raw_array_view(file, corner_address, sizeof(uint32_t), loop_count,
	"mesh corner vertex layer");
    DataView offsets = raw_array_view(file, pointer_value(file, mesh, "poly_offset_indices"),
	sizeof(uint32_t), polygon_count + 1, "mesh polygon offsets");

    for (size_t polygon_index = 0; polygon_index < polygon_count; ++polygon_index) {
	size_t first_corner = static_cast<size_t>(unsigned_value(file,
	    offsets.data + polygon_index * sizeof(uint32_t), sizeof(uint32_t)));
	size_t next_corner = static_cast<size_t>(unsigned_value(file,
	    offsets.data + (polygon_index + 1) * sizeof(uint32_t), sizeof(uint32_t)));
	if (first_corner > next_corner || next_corner > loop_count)
	    throw ParseError("mesh polygon offsets are invalid");
	if (next_corner - first_corner < 3) {
	    bu_log("BLEND Reader: skipping polygon %zu with fewer than three corners\n", polygon_index);
	    continue;
	}
	std::vector<int64_t> polygon;
	polygon.reserve(next_corner - first_corner);
	for (size_t corner = first_corner; corner < next_corner; ++corner)
	    polygon.push_back(static_cast<int64_t>(unsigned_value(file,
		corners.data + corner * sizeof(uint32_t), sizeof(uint32_t))));
	triangulate_polygon(geometry, polygon);
    }
}

static MeshGeometry
read_legacy_mesh(const BlendFile &file, const DataView &mesh)
{
    MeshGeometry geometry;
    bool has_legacy_vertices = mesh.structure->field("mvert") &&
	pointer_value(file, mesh, "mvert") != 0;
    if (has_legacy_vertices)
	read_vertices(file, mesh, geometry);
    else
	read_attribute_vertices(file, mesh, geometry);

    bool has_legacy_polygons = mesh.structure->field("mpoly") &&
	mesh.structure->field("mloop") && pointer_value(file, mesh, "mpoly") != 0 &&
	pointer_value(file, mesh, "mloop") != 0;
    if (has_legacy_polygons && integer_value(file, mesh, "totpoly") > 0)
	read_polygon_faces(file, mesh, geometry);
    else if (mesh.structure->field("poly_offset_indices") &&
	pointer_value(file, mesh, "poly_offset_indices") != 0)
	read_attribute_faces(file, mesh, geometry);
    else if (mesh.structure->field("mface") && pointer_value(file, mesh, "mface") != 0 &&
	integer_value(file, mesh, "totface") > 0)
	read_legacy_faces(file, mesh, geometry);
    else
	throw ParseError("mesh has no supported face topology");
    if (geometry.vertices.empty() || geometry.faces.empty())
	throw ParseError("mesh contains no convertible surface geometry");
    return geometry;
}

static std::string
datablock_name(const BlendFile &file, const DataView &view)
{
    const DnaField *id_field = view.structure->field("id");
    if (!id_field)
	return {};
    DataView id = embedded_view(file, view, "id");
    std::string name = string_value(id, "name");
    return name.size() > 2 ? name.substr(2) : name;
}

static std::string
safe_name(const std::string &source, const std::string &fallback,
    const char *extension, std::set<std::string> &used_names)
{
    std::string name = source.empty() ? fallback : source;
    for (char &character : name) {
	unsigned char byte = static_cast<unsigned char>(character);
	if (!std::isalnum(byte) && character != '_' && character != '-' && character != '.')
	    character = '_';
    }
    if (name.empty())
	name = fallback;
    std::string candidate = name + extension;
    for (size_t suffix = 1; !used_names.insert(candidate).second; ++suffix)
	candidate = name + "_" + std::to_string(suffix) + extension;
    return candidate;
}

static bool
object_selected(const struct gcv_opts *options, const std::string &name)
{
    if (!options || options->num_objects == 0)
	return true;
    for (size_t i = 0; i < options->num_objects; ++i) {
	if (options->object_names[i] && name == options->object_names[i])
	    return true;
    }
    return false;
}

static void
transform_point(double point[3], const std::array<double, 16> &matrix,
    fastf_t scale)
{
    double transformed[3] = {
	matrix[0] * point[0] + matrix[1] * point[1] + matrix[2] * point[2] + matrix[3],
	matrix[4] * point[0] + matrix[5] * point[1] + matrix[6] * point[2] + matrix[7],
	matrix[8] * point[0] + matrix[9] * point[1] + matrix[10] * point[2] + matrix[11]
    };
    for (size_t axis = 0; axis < 3; ++axis) {
	transformed[axis] *= scale;
	if (!std::isfinite(transformed[axis]))
	    throw ParseError("object transform produced a non-finite coordinate");
	point[axis] = transformed[axis];
    }
}

static double
metaball_radius_scale(const std::array<double, 16> &matrix, fastf_t scale,
    bool &non_uniform)
{
    std::array<double, 3> axis_scales;
    for (size_t axis = 0; axis < axis_scales.size(); ++axis) {
	double squared = 0.0;
	for (size_t component = 0; component < 3; ++component) {
	    double value = matrix[component * 4 + axis];
	    squared += value * value;
	}
	axis_scales[axis] = std::sqrt(squared);
	if (!std::isfinite(axis_scales[axis]) || axis_scales[axis] <= 0.0)
	    throw ParseError("metaball object transform has an invalid scale");
    }
    double radius_scale = *std::max_element(axis_scales.begin(), axis_scales.end());
    non_uniform = std::any_of(axis_scales.begin(), axis_scales.end(),
	[radius_scale](double axis_scale) {
	    return std::abs(axis_scale - radius_scale) >
		radius_scale * OBJECT_SCALE_TOLERANCE;
	});
    radius_scale *= std::abs(static_cast<double>(scale));
    if (!std::isfinite(radius_scale) || radius_scale <= 0.0)
	throw ParseError("metaball scale factor is invalid");
    return radius_scale;
}

static void
transform_vertices(MeshGeometry &geometry, const std::array<double, 16> &matrix,
    fastf_t scale)
{
    for (size_t vertex = 0; vertex < geometry.vertices.size() / 3; ++vertex) {
	double point[3] = {
	    geometry.vertices[vertex * 3],
	    geometry.vertices[vertex * 3 + 1],
	    geometry.vertices[vertex * 3 + 2]
	};
	transform_point(point, matrix, scale);
	for (size_t axis = 0; axis < 3; ++axis) {
	    geometry.vertices[vertex * 3 + axis] = static_cast<fastf_t>(point[axis]);
	}
    }
}

struct PointCloudGeometry {
    std::vector<fastf_t> vertices;
    std::vector<fastf_t> radii;
};

static PointCloudGeometry
read_point_cloud(const BlendFile &file, const DataView &point_cloud,
    const std::array<double, 16> &matrix, fastf_t scale)
{
    size_t point_count = checked_count(integer_value(file, point_cloud,
	"totpoint"), "point-cloud point count");
    if (point_count == 0)
	throw ParseError("point cloud has no points");
    uint64_t position_address = custom_layer_address(file, point_cloud,
	"pdata", "position");
    if (position_address == 0)
	throw ParseError("point cloud has no position layer");
    DataView positions = raw_array_view(file, position_address,
	sizeof(float) * 3, point_count, "point-cloud position layer");
    PointCloudGeometry geometry;
    geometry.vertices.resize(point_count * 3);
    for (size_t point = 0; point < point_count; ++point) {
	for (size_t axis = 0; axis < 3; ++axis) {
	    geometry.vertices[point * 3 + axis] = static_cast<fastf_t>(
		float_value(file, positions.data +
		(point * 3 + axis) * sizeof(float)));
	}
    }
    MeshGeometry transformed;
    transformed.vertices.swap(geometry.vertices);
    transform_vertices(transformed, matrix, scale);
    transformed.vertices.swap(geometry.vertices);

    bool non_uniform = false;
    double radius_scale = metaball_radius_scale(matrix, scale, non_uniform);
    if (non_uniform)
	bu_log("BLEND Reader: using the largest object-axis scale for point radii\n");
    uint64_t radius_address = custom_layer_address(file, point_cloud,
	"pdata", "radius");
    geometry.radii.resize(point_count, DEFAULT_POINT_RADIUS * radius_scale);
    if (radius_address != 0) {
	DataView radii = raw_array_view(file, radius_address, sizeof(float),
	    point_count, "point-cloud radius layer");
	for (size_t point = 0; point < point_count; ++point) {
	    double radius = float_value(file, radii.data + point * sizeof(float));
	    radius *= radius_scale;
	    if (!std::isfinite(radius) || radius <= 0.0)
		throw ParseError("point cloud contains an invalid radius");
	    geometry.radii[point] = static_cast<fastf_t>(radius);
	}
    }
    return geometry;
}

struct SplinePoint {
    ON_3dPoint point;
    double weight = 1.0;
};

struct BezierPoint {
    ON_3dPoint left;
    ON_3dPoint point;
    ON_3dPoint right;
};

using MetaballControl = std::array<fastf_t, 5>;

static std::vector<MetaballControl>
read_metaball_controls(const BlendFile &file, const DataView &metaball,
    const std::array<double, 16> &matrix, fastf_t scale, size_t &unsupported)
{
    DataView elements = embedded_view(file, metaball, "elems");
    uint64_t address = pointer_value(file, elements, "first");
    std::set<uint64_t> visited;
    bool non_uniform = false;
    double radius_scale = metaball_radius_scale(matrix, scale, non_uniform);
    if (non_uniform)
	bu_log("BLEND Reader: using the largest object-axis scale for spherical metaball elements\n");

    std::vector<MetaballControl> controls;
    unsupported = 0;
    while (address != 0) {
	if (!visited.insert(address).second)
	    throw ParseError("metaball element list contains a cycle");
	DataView element = address_view(file, address);
	if (!element || element.structure->name != "MetaElem")
	    throw ParseError("metaball element list contains an invalid pointer");
	uint64_t next = pointer_value(file, element, "next");
	int64_t element_type = integer_value(file, element, "type");
	if (element_type != META_ELEMENT_BALL) {
	    const DnaField *extent_x = element.structure->field("expx");
	    const DnaField *extent_y = element.structure->field("expy");
	    const DnaField *extent_z = element.structure->field("expz");
	    if (extent_x && extent_y && extent_z) {
		bu_log("BLEND Reader: metaball element type %lld with extents %g, %g, %g is not supported\n",
		    static_cast<long long>(element_type), float_field(file, element, "expx"),
		    float_field(file, element, "expy"), float_field(file, element, "expz"));
	    } else {
		bu_log("BLEND Reader: metaball element type %lld is not supported\n",
		    static_cast<long long>(element_type));
	    }
	    ++unsupported;
	    address = next;
	    continue;
	}
	double point[3] = {
	    float_field(file, element, "x"),
	    float_field(file, element, "y"),
	    float_field(file, element, "z")
	};
	transform_point(point, matrix, scale);
	double radius = float_field(file, element, "rad") * radius_scale;
	if (radius <= 0.0)
	    throw ParseError("metaball element has a non-positive radius");
	double stiffness = float_field(file, element, "s");
	if (stiffness <= 0.0)
	    throw ParseError("metaball element has a non-positive stiffness");
	bool negative = (integer_value(file, element, "flag") &
	    META_ELEMENT_NEGATIVE) != 0;
	controls.push_back({static_cast<fastf_t>(point[0]),
	    static_cast<fastf_t>(point[1]), static_cast<fastf_t>(point[2]),
	    static_cast<fastf_t>(negative ? -radius : radius),
	    static_cast<fastf_t>(stiffness)});
	address = next;
    }
    return controls;
}

static std::vector<SplinePoint>
read_spline_points(const BlendFile &file, const DataView &spline,
    const std::array<double, 16> &matrix, fastf_t scale,
    bool require_positive_weights = true)
{
    size_t point_count_u = checked_count(integer_value(file, spline, "pntsu"),
	"spline U control-point count");
    size_t point_count_v = checked_count(integer_value(file, spline, "pntsv"),
	"spline V control-point count");
    if (point_count_u == 0 || point_count_v == 0)
	throw ParseError("spline has no control points");
    size_t point_count = checked_product(point_count_u, point_count_v,
	"spline control-point count");
    DataView points = array_view(file, pointer_value(file, spline, "bp"),
	"BPoint", point_count);

    std::vector<SplinePoint> result;
    result.reserve(point_count);
    for (size_t index = 0; index < point_count; ++index) {
	DataView point_view = array_element(points, index);
	const DnaField *coordinates = require_field(point_view, "vec");
	if (coordinates->pointer || coordinates->size != sizeof(float) * 4)
	    throw ParseError("BPoint.vec is not a four-component float array");
	double point[3];
	for (size_t axis = 0; axis < 3; ++axis)
	    point[axis] = float_value(file, point_view.data + coordinates->offset +
		axis * sizeof(float));
	transform_point(point, matrix, scale);
	double weight = float_value(file, point_view.data + coordinates->offset +
	    3 * sizeof(float));
	if (!std::isfinite(weight) || (require_positive_weights && weight <= 0.0))
	    throw ParseError("spline control point has an invalid weight");
	result.push_back({ON_3dPoint(point[0], point[1], point[2]), weight});
    }
    return result;
}

static std::vector<BezierPoint>
read_bezier_points(const BlendFile &file, const DataView &spline,
    const std::array<double, 16> &matrix, fastf_t scale)
{
    size_t point_count = checked_count(integer_value(file, spline, "pntsu"),
	"Bezier curve control-point count");
    if (point_count < 2)
	throw ParseError("Bezier curve has fewer than two control points");
    DataView points = array_view(file, pointer_value(file, spline, "bezt"),
	"BezTriple", point_count);

    std::vector<BezierPoint> result;
    result.reserve(point_count);
    for (size_t index = 0; index < point_count; ++index) {
	DataView point_view = array_element(points, index);
	const DnaField *coordinates = require_field(point_view, "vec");
	if (coordinates->pointer || coordinates->size != sizeof(float) * 9)
	    throw ParseError("BezTriple.vec is not a three-by-three float array");
	std::array<ON_3dPoint, 3> transformed;
	for (size_t control = 0; control < transformed.size(); ++control) {
	    double point[3];
	    for (size_t axis = 0; axis < 3; ++axis) {
		point[axis] = float_value(file, point_view.data + coordinates->offset +
		    (control * 3 + axis) * sizeof(float));
	    }
	    transform_point(point, matrix, scale);
	    transformed[control] = ON_3dPoint(point[0], point[1], point[2]);
	}
	result.push_back({transformed[0], transformed[1], transformed[2]});
    }
    return result;
}

static std::unique_ptr<ON_NurbsCurve>
make_clamped_curve(const std::vector<ON_3dPoint> &points)
{
    if (points.size() < 2 || points.size() > static_cast<size_t>(INT_MAX))
	throw ParseError("spline segment has an invalid control-point count");
    int order = static_cast<int>(points.size());
    std::unique_ptr<ON_NurbsCurve> curve(
	ON_NurbsCurve::New(3, false, order, order));
    if (!curve || !curve->MakeClampedUniformKnotVector(1.0))
	throw ParseError("could not construct a spline segment");
    for (int index = 0; index < order; ++index) {
	if (!curve->SetCV(index, points[static_cast<size_t>(index)]))
	    throw ParseError("could not set a spline segment control point");
    }
    if (!curve->IsValid())
	throw ParseError("constructed spline segment is invalid");
    return curve;
}

static void
set_curve_knots(const BlendFile &file, const DataView &spline,
    ON_NurbsCurve &curve, int64_t flags)
{
    uint64_t address = pointer_value(file, spline, "knotsu");
    if (address == 0) {
	bool made = (flags & SPLINE_CYCLIC) ? curve.MakePeriodicUniformKnotVector(1.0) :
	    curve.MakeClampedUniformKnotVector(1.0);
	if (!made)
	    throw ParseError("could not construct a default NURBS curve knot vector");
	return;
    }
    size_t knot_count = static_cast<size_t>(curve.KnotCount());
    DataView knots = raw_array_view(file, address, sizeof(float), knot_count + 2,
	"NURBS curve knot vector");
    size_t source_offset = (!(flags & SPLINE_CYCLIC) &&
	(flags & SPLINE_BEZIER_KNOTS)) ? 2 : 1;
    for (size_t index = 0; index < knot_count; ++index) {
	double knot = float_value(file, knots.data + (index + source_offset) * sizeof(float));
	if (!std::isfinite(knot) || !curve.SetKnot(static_cast<int>(index), knot))
	    throw ParseError("NURBS curve has an invalid knot vector");
    }
}

static void
set_surface_knots(const BlendFile &file, const DataView &spline,
    ON_NurbsSurface &surface, int direction, const char *field_name, bool cyclic)
{
    uint64_t address = pointer_value(file, spline, field_name);
    if (address == 0) {
	bool made = cyclic ? surface.MakePeriodicUniformKnotVector(direction, 1.0) :
	    surface.MakeClampedUniformKnotVector(direction, 1.0);
	if (!made)
	    throw ParseError("could not construct a default NURBS surface knot vector");
	return;
    }
    size_t knot_count = static_cast<size_t>(surface.KnotCount(direction));
    DataView knots = raw_array_view(file, address, sizeof(float), knot_count + 2,
	"NURBS surface knot vector");
    for (size_t index = 0; index < knot_count; ++index) {
	double knot = float_value(file, knots.data + (index + 1) * sizeof(float));
	if (!std::isfinite(knot) ||
	    !surface.SetKnot(direction, static_cast<int>(index), knot))
	    throw ParseError("NURBS surface has an invalid knot vector");
    }
}

static void
set_control_point(ON_NurbsCurve &curve, int index, const SplinePoint &point,
    bool rational)
{
    bool set = rational ? curve.SetCV(index, ON_4dPoint(point.point.x * point.weight,
	point.point.y * point.weight, point.point.z * point.weight, point.weight)) :
	curve.SetCV(index, point.point);
    if (!set)
	throw ParseError("could not set a NURBS curve control point");
}

static void
set_control_point(ON_NurbsSurface &surface, int u, int v,
    const SplinePoint &point, bool rational)
{
    bool set = rational ? surface.SetCV(u, v, ON_4dPoint(point.point.x * point.weight,
	point.point.y * point.weight, point.point.z * point.weight, point.weight)) :
	surface.SetCV(u, v, point.point);
    if (!set)
	throw ParseError("could not set a NURBS surface control point");
}

static std::unique_ptr<ON_NurbsCurve>
make_nurbs_curve(const BlendFile &file, const DataView &spline,
    const std::array<double, 16> &matrix, fastf_t scale)
{
    size_t source_count = checked_count(integer_value(file, spline, "pntsu"),
	"NURBS curve control-point count");
    size_t order = checked_count(integer_value(file, spline, "orderu"),
	"NURBS curve order");
    int64_t flags = integer_value(file, spline, "flagu");
    bool cyclic = (flags & SPLINE_CYCLIC) != 0;
    if (source_count < 2 || order < 2 || order > source_count)
	throw ParseError("NURBS curve has invalid control-point count or order");
    bool bezier_knots = !cyclic && (flags & SPLINE_BEZIER_KNOTS);
    /* Blender's Bezier knot mode can leave incomplete control-point groups
     * outside the active degree-sized knot spans. */
    size_t inactive_count = bezier_knots ? (source_count - 1) % (order - 1) : 0;
    bool trim_inactive_start = inactive_count > 0;
    size_t active_count = source_count;
    active_count -= inactive_count;
    if (active_count < order)
	throw ParseError("NURBS curve has invalid control-point count or order");
    size_t extra_count = cyclic ? order - 1 : 0;
    if (active_count > static_cast<size_t>(INT_MAX) - extra_count)
	throw ParseError("NURBS curve has too many control points");

    std::vector<SplinePoint> points = read_spline_points(file, spline, matrix, scale);
    bool rational = std::any_of(points.begin(), points.end(),
	[](const SplinePoint &point) { return !EQUAL(point.weight, 1.0); });
    int control_count = static_cast<int>(active_count + extra_count);
    std::unique_ptr<ON_NurbsCurve> curve(
	ON_NurbsCurve::New(3, rational, static_cast<int>(order), control_count));
    if (!curve)
	throw ParseError("could not allocate a NURBS curve");
    if (bezier_knots && source_count == order) {
	if (!curve->MakeClampedUniformKnotVector(1.0))
	    throw ParseError("could not construct a Bezier-style NURBS knot vector");
    } else {
	set_curve_knots(file, spline, *curve, flags);
    }
    size_t source_offset = trim_inactive_start ? 1 : 0;
    for (int index = 0; index < control_count; ++index) {
	size_t source = static_cast<size_t>(index) % active_count + source_offset;
	set_control_point(*curve, index, points[source], rational);
    }
    ON_wString validation_messages;
    ON_TextLog validation_log(validation_messages);
    if (!curve->IsValid(&validation_log)) {
	ON_String validation_text(validation_messages);
	std::string knots;
	for (int index = 0; index < curve->KnotCount(); ++index) {
	    if (!knots.empty())
		knots += ",";
	    knots += std::to_string(curve->Knot(index));
	}
	throw ParseError("constructed NURBS curve (flags " +
	    std::to_string(integer_value(file, spline, "flagu")) +
	    ", knots " + knots + ") is invalid: " +
	    static_cast<const char *>(validation_text));
    }
    return curve;
}

static std::unique_ptr<ON_NurbsSurface>
make_nurbs_surface(const BlendFile &file, const DataView &spline,
    const std::array<double, 16> &matrix, fastf_t scale)
{
    size_t source_count_u = checked_count(integer_value(file, spline, "pntsu"),
	"NURBS surface U control-point count");
    size_t source_count_v = checked_count(integer_value(file, spline, "pntsv"),
	"NURBS surface V control-point count");
    size_t order_u = checked_count(integer_value(file, spline, "orderu"),
	"NURBS surface U order");
    size_t order_v = checked_count(integer_value(file, spline, "orderv"),
	"NURBS surface V order");
    bool cyclic_u = (integer_value(file, spline, "flagu") & SPLINE_CYCLIC) != 0;
    bool cyclic_v = (integer_value(file, spline, "flagv") & SPLINE_CYCLIC) != 0;
    if (source_count_u < 2 || source_count_v < 2 || order_u < 2 || order_v < 2 ||
	order_u > source_count_u || order_v > source_count_v)
	throw ParseError("NURBS surface has invalid dimensions or orders");
    size_t count_u = source_count_u + (cyclic_u ? order_u - 1 : 0);
    size_t count_v = source_count_v + (cyclic_v ? order_v - 1 : 0);
    if (count_u > INT_MAX || count_v > INT_MAX)
	throw ParseError("NURBS surface has too many control points");

    std::vector<SplinePoint> points = read_spline_points(file, spline, matrix, scale);
    bool rational = std::any_of(points.begin(), points.end(),
	[](const SplinePoint &point) { return !EQUAL(point.weight, 1.0); });
    std::unique_ptr<ON_NurbsSurface> surface(ON_NurbsSurface::New(3, rational,
	static_cast<int>(order_u), static_cast<int>(order_v),
	static_cast<int>(count_u), static_cast<int>(count_v)));
    if (!surface)
	throw ParseError("could not allocate a NURBS surface");
    set_surface_knots(file, spline, *surface, 0, "knotsu", cyclic_u);
    set_surface_knots(file, spline, *surface, 1, "knotsv", cyclic_v);
    for (size_t v = 0; v < count_v; ++v) {
	for (size_t u = 0; u < count_u; ++u) {
	    size_t source = (v % source_count_v) * source_count_u +
		(u % source_count_u);
	    set_control_point(*surface, static_cast<int>(u), static_cast<int>(v),
		points[source], rational);
	}
    }
    ON_wString validation_messages;
    ON_TextLog validation_log(validation_messages);
    if (!surface->IsValid(&validation_log)) {
	ON_String validation_text(validation_messages);
	throw ParseError(std::string("constructed NURBS surface is invalid: ") +
	    static_cast<const char *>(validation_text));
    }
    return surface;
}

static void
add_wire_curve(ON_Brep &brep, std::unique_ptr<ON_NurbsCurve> curve)
{
    ON_3dPoint start = curve->PointAtStart();
    ON_3dPoint end = curve->PointAtEnd();
    int curve_index = brep.AddEdgeCurve(curve.release());
    int start_vertex = brep.NewVertex(start).m_vertex_index;
    int end_vertex = start_vertex;
    if (start.DistanceTo(end) > SMALL_FASTF)
	end_vertex = brep.NewVertex(end).m_vertex_index;
    brep.m_V[start_vertex].m_tolerance = 0.0;
    brep.m_V[end_vertex].m_tolerance = 0.0;
    ON_BrepEdge &edge = brep.NewEdge(brep.m_V[start_vertex],
	brep.m_V[end_vertex], curve_index);
    edge.m_tolerance = 0.0;
}

static void
add_bezier_spline(const BlendFile &file, const DataView &spline,
    const std::array<double, 16> &matrix, fastf_t scale, ON_Brep &brep)
{
    if (checked_count(integer_value(file, spline, "pntsv"),
	"Bezier curve V control-point count") > 1)
	throw ParseError("Bezier surfaces are not supported");
    std::vector<BezierPoint> points = read_bezier_points(file, spline, matrix, scale);
    bool cyclic = (integer_value(file, spline, "flagu") & SPLINE_CYCLIC) != 0;
    size_t segment_count = cyclic ? points.size() : points.size() - 1;
    for (size_t index = 0; index < segment_count; ++index) {
	const BezierPoint &start = points[index];
	const BezierPoint &end = points[(index + 1) % points.size()];
	add_wire_curve(brep, make_clamped_curve({start.point, start.right,
	    end.left, end.point}));
    }
}

static void
add_poly_spline(const BlendFile &file, const DataView &spline,
    const std::array<double, 16> &matrix, fastf_t scale, ON_Brep &brep)
{
    if (checked_count(integer_value(file, spline, "pntsv"),
	"poly curve V control-point count") > 1)
	throw ParseError("poly surfaces are not supported");
    std::vector<SplinePoint> points = read_spline_points(file, spline, matrix,
	scale, false);
    if (points.size() < 2)
	throw ParseError("poly curve has fewer than two control points");
    bool cyclic = (integer_value(file, spline, "flagu") & SPLINE_CYCLIC) != 0;
    size_t segment_count = cyclic ? points.size() : points.size() - 1;
    for (size_t index = 0; index < segment_count; ++index) {
	add_wire_curve(brep, make_clamped_curve({points[index].point,
	    points[(index + 1) % points.size()].point}));
    }
}

static bool
write_region(struct rt_wdb *output, struct wmember &all,
    const std::string &primitive_name)
{
    struct wmember region;
    BU_LIST_INIT(&region.l);
    if (!mk_addmember(primitive_name.c_str(), &region.l, nullptr, WMOP_UNION))
	return false;
    size_t extension = primitive_name.find_last_of('.');
    std::string region_name = primitive_name.substr(0, extension) + ".r";
    if (mk_lcomb(output, region_name.c_str(), &region, 1, nullptr, nullptr,
	nullptr, 0) < 0) {
	mk_freemembers(&region.l);
	bu_log("BLEND Reader: failed to write region %s\n", region_name.c_str());
	return false;
    }
    return mk_addmember(region_name.c_str(), &all.l, nullptr, WMOP_UNION) != nullptr;
}

static bool
write_mesh(struct rt_wdb *output, struct wmember &all, MeshGeometry &geometry,
    const std::string &name)
{
    size_t vertex_count = geometry.vertices.size() / 3;
    size_t face_count = geometry.faces.size() / 3;
    if (vertex_count > INT_MAX || face_count > INT_MAX) {
	bu_log("BLEND Reader: mesh %s exceeds BRL-CAD BOT limits\n", name.c_str());
	return false;
    }
    if (mk_bot(output, name.c_str(), RT_BOT_SOLID, RT_BOT_UNORIENTED, 0,
	static_cast<int>(vertex_count), static_cast<int>(face_count),
	geometry.vertices.data(), geometry.faces.data(), nullptr, nullptr) < 0) {
	bu_log("BLEND Reader: failed to write BOT %s\n", name.c_str());
	return false;
    }
    return write_region(output, all, name);
}

static bool
write_point_cloud(struct rt_wdb *output, struct wmember &all,
    const PointCloudGeometry &geometry, const std::string &name)
{
    size_t point_count = geometry.vertices.size() / 3;
    if (mk_pnts(output, name.c_str(), RT_PNT_TYPE_SCA, DEFAULT_POINT_RADIUS,
	point_count, geometry.vertices.data(), nullptr, geometry.radii.data(),
	nullptr) < 0) {
	bu_log("BLEND Reader: failed to write point cloud %s\n", name.c_str());
	return false;
    }
    return write_region(output, all, name);
}

static bool
write_brep(struct rt_wdb *output, struct wmember &all, ON_Brep &brep,
    const std::string &name)
{
    brep.Compact();

    brep.SetTolerancesBoxesAndFlags(false, false, false, false,
	true, true, true, true);
    ON_wString validation_messages;
    ON_TextLog validation_log(validation_messages);
    if (!brep.IsValid(&validation_log)) {
	ON_String validation_text(validation_messages);
	bu_log("BLEND Reader: BREP %s is invalid: %s\n", name.c_str(),
	    static_cast<const char *>(validation_text));
	return false;
    }
    if (mk_brep(output, name.c_str(), &brep) < 0) {
	bu_log("BLEND Reader: failed to write BREP %s\n", name.c_str());
	return false;
    }
    return write_region(output, all, name);
}

static bool
write_metaball(struct rt_wdb *output, struct wmember &all,
    const std::vector<MetaballControl> &controls, const std::string &name)
{
    std::vector<const fastf_t *> control_pointers;
    control_pointers.reserve(controls.size());
    for (const MetaballControl &control : controls)
	control_pointers.push_back(control.data());
    if (mk_metaball(output, name.c_str(), controls.size(), METABALL_ISOPOTENTIAL,
	METABALL_THRESHOLD, control_pointers.data()) < 0) {
	bu_log("BLEND Reader: failed to write metaball %s\n", name.c_str());
	return false;
    }
    return write_region(output, all, name);
}

static bool
write_text_annotation(struct rt_wdb *output, struct wmember &all,
    const std::string &name, const std::string &text, fastf_t text_size,
    const std::array<double, 16> &matrix, fastf_t scale)
{
    double origin[3] = {0.0, 0.0, 0.0};
    double u_point[3] = {1.0, 0.0, 0.0};
    double v_point[3] = {0.0, 1.0, 0.0};
    transform_point(origin, matrix, scale);
    transform_point(u_point, matrix, scale);
    transform_point(v_point, matrix, scale);

    point2d_t reference = {0.0, 0.0};
    int reverse = 0;
    struct txt_seg text_segment = {};
    text_segment.magic = ANN_TSEG_MAGIC;
    text_segment.ref_pt = 0;
    text_segment.rel_pos = RT_TXT_POS_BL;
    text_segment.txt_size = text_size;
    bu_vls_init(&text_segment.label);
    bu_vls_strcpy(&text_segment.label, text.c_str());
    void *segment = &text_segment;

    struct rt_annot_seg_style style = {};
    style.role = RT_ANNOT_ROLE_TEXT;
    style.line_pattern = RT_ANNOT_LINE_CONTINUOUS;
    struct rt_annot_internal annotation = {};
    annotation.magic = RT_ANNOT_INTERNAL_MAGIC;
    VSET(annotation.V, origin[0], origin[1], origin[2]);
    VSET(annotation.u_vec, u_point[0] - origin[0], u_point[1] - origin[1],
	u_point[2] - origin[2]);
    VSET(annotation.v_vec, v_point[0] - origin[0], v_point[1] - origin[1],
	v_point[2] - origin[2]);
    annotation.flags = RT_ANNOT_MODEL_SPACE;
    annotation.vert_count = 1;
    annotation.verts = &reference;
    annotation.ant.count = 1;
    annotation.ant.reverse = &reverse;
    annotation.ant.segments = &segment;
    annotation.styles = &style;

    bool written = !rt_annot_validate(&annotation, nullptr) &&
	mk_annot(output, name.c_str(), &annotation) >= 0 &&
	mk_addmember(name.c_str(), &all.l, nullptr, WMOP_UNION) != nullptr;
    bu_vls_free(&text_segment.label);
    if (!written)
	bu_log("BLEND Reader: failed to write text annotation %s\n", name.c_str());
    return written;
}

static bool
convert_mesh_object(const BlendFile &file, const DataView &object,
    const DataView &mesh, const struct gcv_opts *options, struct rt_wdb *output,
    struct wmember &all, std::set<std::string> &used_names)
{
    std::string object_name = datablock_name(file, object);
    if (!object_selected(options, object_name))
	return false;
    MeshGeometry geometry = read_legacy_mesh(file, mesh);
    transform_vertices(geometry, object_matrix(file, object), options->scale_factor);
    std::string name = safe_name(object_name, datablock_name(file, mesh), ".bot", used_names);
    return write_mesh(output, all, geometry, name);
}

static bool
convert_point_cloud_object(const BlendFile &file, const DataView &object,
    const DataView &point_cloud, const struct gcv_opts *options,
    struct rt_wdb *output, struct wmember &all,
    std::set<std::string> &used_names)
{
    std::string object_name = datablock_name(file, object);
    if (!object_selected(options, object_name))
	return false;
    PointCloudGeometry geometry = read_point_cloud(file, point_cloud,
	object_matrix(file, object), options->scale_factor);
    std::string name = safe_name(object_name, datablock_name(file, point_cloud),
	".pnts", used_names);
    return write_point_cloud(output, all, geometry, name);
}

static std::unique_ptr<ON_Brep>
read_curve_brep(const BlendFile &file, const DataView &curve,
    const std::array<double, 16> &matrix, fastf_t scale)
{
    DataView splines = embedded_view(file, curve, "nurb");
    uint64_t address = pointer_value(file, splines, "first");
    std::set<uint64_t> visited;
    std::unique_ptr<ON_Brep> brep(ON_Brep::New());
    size_t imported = 0;
    size_t unsupported = 0;

    while (address != 0) {
	if (!visited.insert(address).second)
	    throw ParseError("Curve spline list contains a cycle");
	DataView spline = address_view(file, address);
	if (!spline || spline.structure->name != "Nurb")
	    throw ParseError("Curve spline list contains an invalid pointer");
	uint64_t next = pointer_value(file, spline, "next");
	int64_t type = integer_value(file, spline, "type") & SPLINE_TYPE_MASK;
	if (type == SPLINE_TYPE_NURBS) {
	    size_t point_count_v = checked_count(integer_value(file, spline, "pntsv"),
		"NURBS V control-point count");
	    if (point_count_v > 1) {
		std::unique_ptr<ON_NurbsSurface> surface =
		    make_nurbs_surface(file, spline, matrix, scale);
		if (!brep->NewFace(*surface))
		    throw ParseError("could not add a NURBS surface to a BREP");
	    } else {
		add_wire_curve(*brep, make_nurbs_curve(file, spline, matrix, scale));
	    }
	} else if (type == SPLINE_TYPE_BEZIER) {
	    add_bezier_spline(file, spline, matrix, scale, *brep);
	} else if (type == SPLINE_TYPE_POLY) {
	    add_poly_spline(file, spline, matrix, scale, *brep);
	} else {
	    ++unsupported;
	    address = next;
	    continue;
	}
	++imported;
	address = next;
    }
    if (unsupported > 0)
	bu_log("BLEND Reader: skipped %zu unsupported spline%s in a Curve object\n",
	    unsupported, unsupported == 1 ? "" : "s");
    if (imported == 0)
	throw ParseError("Curve object contains no supported splines");
    return brep;
}

static bool
convert_text_object(const BlendFile &file, const DataView &object,
    const DataView &curve, const struct gcv_opts *options, struct rt_wdb *output,
    struct wmember &all, std::set<std::string> &used_names)
{
    std::string object_name = datablock_name(file, object);
    if (!object_selected(options, object_name))
	return false;
    std::string text = pointed_string_value(file, curve, "str", "len");
    if (text.empty())
	throw ParseError("font object has no text");
    double text_size = float_field(file, curve, "fsize");
    if (text_size <= 0.0)
	throw ParseError("font object has a non-positive text size");
    const DnaField *extrude = curve.structure->field("extrude");
    const DnaField *bevel = curve.structure->field("bevel_depth");
    if ((extrude && !ZERO(float_field(file, curve, "extrude"))) ||
	(bevel && !ZERO(float_field(file, curve, "bevel_depth"))))
	bu_log("BLEND Reader: text object %s imported as planar annotation; extrusion and bevel are not represented\n",
	    object_name.c_str());
    std::string name = safe_name(object_name, datablock_name(file, curve),
	".annot", used_names);
    return write_text_annotation(output, all, name, text,
	static_cast<fastf_t>(text_size), object_matrix(file, object),
	options->scale_factor);
}

static bool
convert_curve_object(const BlendFile &file, const DataView &object,
    const DataView &curve, const struct gcv_opts *options, struct rt_wdb *output,
    struct wmember &all, std::set<std::string> &used_names)
{
    const DnaField *text = curve.structure->field("str");
    if (text && text->pointer && pointer_value(file, curve, "str") != 0)
	return convert_text_object(file, object, curve, options, output, all, used_names);
    std::string object_name = datablock_name(file, object);
    if (!object_selected(options, object_name))
	return false;
    std::unique_ptr<ON_Brep> brep = read_curve_brep(file, curve,
	object_matrix(file, object), options->scale_factor);
    std::string name = safe_name(object_name, datablock_name(file, curve),
	".brep", used_names);
    return write_brep(output, all, *brep, name);
}

static bool
convert_metaball_object(const BlendFile &file, const DataView &object,
    const DataView &metaball, const struct gcv_opts *options,
    struct rt_wdb *output, struct wmember &all,
    std::set<std::string> &used_names)
{
    std::string object_name = datablock_name(file, object);
    if (!object_selected(options, object_name))
	return false;
    double threshold = float_field(file, metaball, "thresh");
    if (threshold <= 0.0)
	throw ParseError("metaball has a non-positive threshold");
    size_t unsupported = 0;
    std::vector<MetaballControl> controls = read_metaball_controls(file,
	metaball, object_matrix(file, object), options->scale_factor, unsupported);
    if (unsupported > 0)
	bu_log("BLEND Reader: skipped %zu non-spherical metaball element%s in object %s\n",
	    unsupported, unsupported == 1 ? "" : "s", object_name.c_str());
    if (controls.empty())
	throw ParseError("metaball contains no supported spherical elements");
    std::string name = safe_name(object_name, datablock_name(file, metaball),
	".metaball", used_names);
    return write_metaball(output, all, controls, name);
}

static size_t
convert_objects(const BlendFile &file, const struct gcv_opts *options,
    struct rt_wdb *output, struct wmember &all)
{
    size_t converted = 0;
    std::set<std::string> used_names;
    std::set<std::string> reported_types;
    for (const BlendBlock &block : file.blocks) {
	if (block.sdna_index >= file.structures.size() ||
	    file.structures[block.sdna_index].name != "Object")
	    continue;
	DataView objects = block_view(file, block);
	const DnaStruct *object_structure = file.structure("Object");
	if (!object_structure || object_structure->size == 0)
	    throw ParseError("Object SDNA structure has an invalid size");
	size_t available_count = objects.size / object_structure->size;
	if (block.count > available_count)
	    throw ParseError("Object block count exceeds its data size");
	objects.structure = object_structure;
	for (size_t index = 0; index < block.count; ++index) {
	    DataView object = array_element(objects, index);
	    uint64_t data_address = pointer_value(file, object, "data");
	    if (data_address == 0)
		continue;
	    DataView data = address_view(file, data_address);
	    if (!data) {
		bu_log("BLEND Reader: object %s has an unresolved data pointer; skipping\n",
		    datablock_name(file, object).c_str());
		continue;
	    }
	    if (data.structure->name != "Mesh" && data.structure->name != "Curve" &&
		data.structure->name != "MetaBall" &&
		data.structure->name != "PointCloud") {
		if (reported_types.insert(data.structure->name).second)
		    bu_log("BLEND Reader: object geometry type %s is not supported; skipping instances\n",
			data.structure->name.c_str());
		continue;
	    }
	    try {
		bool converted_object = false;
		if (data.structure->name == "Mesh") {
		    converted_object = convert_mesh_object(file, object, data, options,
			output, all, used_names);
		} else if (data.structure->name == "Curve") {
		    converted_object = convert_curve_object(file, object, data, options,
			output, all, used_names);
		} else if (data.structure->name == "MetaBall") {
		    converted_object = convert_metaball_object(file, object, data, options,
			output, all, used_names);
		} else {
		    converted_object = convert_point_cloud_object(file, object, data,
			options, output, all, used_names);
		}
		if (converted_object)
		    ++converted;
	    } catch (const ParseError &error) {
		bu_log("BLEND Reader: cannot import object %s: %s\n",
		    datablock_name(file, object).c_str(), error.what());
	    }
	}
    }
    return converted;
}

static int
blend_can_read(const char *path)
{
    if (!path)
	return 0;
    try {
	std::vector<unsigned char> bytes = read_blend_bytes(path);
	return bytes.size() >= BLEND_HEADER_SIZE &&
	    std::memcmp(bytes.data(), "BLENDER", 7) == 0;
    } catch (const std::exception &) {
	return 0;
    }
}

static int
blend_read(struct gcv_context *context, const struct gcv_opts *options,
    const void *UNUSED(options_data), const char *source_path)
{
    if (!context || !context->dbip || !options || !source_path) {
	bu_log("BLEND Reader: invalid conversion arguments\n");
	return 0;
    }

    try {
	BlendFile file = parse_blend_file(source_path);
	bu_log("BLEND Reader: Blender %d.%02d, %zu-bit pointers, %s-endian\n",
	    file.version / 100, file.version % 100, file.pointer_size * 8,
	    file.little_endian ? "little" : "big");

	struct rt_wdb *output = wdb_dbopen(context->dbip, RT_WDB_TYPE_DB_DEFAULT);
	if (!output)
	    throw ParseError("could not open the output database");
	std::string title = std::string("Blender conversion from ") + source_path;
	if (mk_id_units(output, title.c_str(), "mm") < 0)
	    throw ParseError("could not initialize the output database");

	struct wmember all;
	BU_LIST_INIT(&all.l);
	size_t converted = convert_objects(file, options, output, all);
	if (converted == 0) {
	    mk_freemembers(&all.l);
	    bu_log("BLEND Reader: no supported geometry was converted\n");
	    return 0;
	}
	if (mk_lcomb(output, "all", &all, 0, nullptr, nullptr, nullptr, 0) < 0) {
	    mk_freemembers(&all.l);
	    throw ParseError("could not create the top-level all combination");
	}
	bu_log("BLEND Reader: converted %zu geometry object%s\n", converted,
	    converted == 1 ? "" : "s");
	return 1;
    } catch (const std::exception &error) {
	bu_log("BLEND Reader: %s\n", error.what());
	return 0;
    }
}

static const struct gcv_filter gcv_conv_blend_read = {
    "Blender Reader", GCV_FILTER_READ, BU_MIME_MODEL_VND_BLENDER,
    blend_can_read, nullptr, nullptr, blend_read
};

static const struct gcv_filter * const filters[] = {
    &gcv_conv_blend_read,
    nullptr
};

const struct gcv_plugin gcv_plugin_info_s = {filters};

} // namespace

extern "C" COMPILER_DLLEXPORT const struct gcv_plugin *
gcv_plugin_info(void)
{
    return &gcv_plugin_info_s;
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
