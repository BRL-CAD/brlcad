/*                      T E S T _ J T _ P A R S E R . C P P
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "common.h"

#include "jt_parser.h"

#include <algorithm>
#include <cstring>
#include <cstdio>
#include <string>
#include <vector>

namespace {

void append_u32(std::vector<unsigned char> &data, uint32_t value, bool little = true)
{
    for (unsigned i = 0; i < 4; ++i) {
	const unsigned shift = (little ? i : 3 - i) * 8;
	data.push_back(static_cast<unsigned char>(value >> shift));
    }
}

void append_u64(std::vector<unsigned char> &data, uint64_t value, bool little)
{
    for (unsigned i = 0; i < 8; ++i) {
	const unsigned shift = (little ? i : 7 - i) * 8;
	data.push_back(static_cast<unsigned char>(value >> shift));
    }
}

void write_u32(std::vector<unsigned char> &data, size_t offset, uint32_t value)
{
    for (unsigned i = 0; i < 4; ++i) data[offset + i] = static_cast<unsigned char>(value >> (i * 8));
}

void append_u16(std::vector<unsigned char> &data, uint16_t value)
{
    data.push_back(static_cast<unsigned char>(value));
    data.push_back(static_cast<unsigned char>(value >> 8));
}

void append_f32(std::vector<unsigned char> &data, float value)
{
    uint32_t bits = 0;
    std::memcpy(&bits, &value, sizeof(bits));
    append_u32(data, bits, true);
}

std::vector<unsigned char> sample_file(unsigned major, unsigned minor, bool little)
{
    const bool wide_offsets = major >= 10;
    const bool has_object_id = major >= 9;
    const uint32_t header_size = wide_offsets ? 109 : 105;
    const uint32_t toc_size = wide_offsets ? 36 : 32;
    const uint32_t element_length = 17 + (has_object_id ? 4 : 0);
    const uint32_t segment_size = 24 + 4 + element_length;
    jt::Guid id{};
    id[0] = 42;
    jt::Guid element_id{};
    element_id[0] = 99;

    std::vector<unsigned char> data(80, ' ');
    const std::string version = "Version " + std::to_string(major) + "." + std::to_string(minor) + " JT";
    std::copy(version.begin(), version.end(), data.begin());
    data.push_back(little ? 0 : 1);
    append_u32(data, 0, little);
    if (wide_offsets) {
	append_u64(data, header_size, little);
    } else {
	append_u32(data, header_size, little);
    }
    data.insert(data.end(), id.begin(), id.end());
    append_u32(data, 1, little);
    data.insert(data.end(), id.begin(), id.end());
    if (wide_offsets) {
	append_u64(data, header_size + toc_size, little);
    } else {
	append_u32(data, header_size + toc_size, little);
    }
    append_u32(data, segment_size, little);
    append_u32(data, 7u << 24, little);
    data.insert(data.end(), id.begin(), id.end());
    append_u32(data, 7, little);
    append_u32(data, segment_size, little);
    append_u32(data, element_length, little);
    data.insert(data.end(), element_id.begin(), element_id.end());
    data.push_back(4);
    if (has_object_id) append_u32(data, 123, little);
    return data;
}

std::vector<unsigned char> sample_tristrip()
{
    std::vector<unsigned char> data = sample_file(8, 1, true);
    const size_t element_offset = 161;
    const unsigned char guid[] = {0xab, 0x10, 0xdd, 0x10, 0xc8, 0x2a, 0xd1, 0x11,
	0x9b, 0x6b, 0x00, 0x80, 0xc7, 0xbb, 0x59, 0x97};
    std::copy(guid, guid + sizeof(guid), data.begin() + element_offset + 4);

    append_u16(data, 1);
    append_u32(data, 0, true);
    data.insert(data.end(), 4, 0);
    append_u16(data, 1);
    append_u16(data, 1);
    data.insert(data.end(), 3, 0);
    data.insert(data.end(), 4, 0);
    data.push_back(0);
    append_u32(data, 2, true);
    append_u32(data, 0, true);
    append_u32(data, 9, true);
    append_u32(data, 36, true);
    append_u32(data, static_cast<uint32_t>(-36), true);
    for (float value : {0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f}) append_f32(data, value);

    const uint32_t element_length = static_cast<uint32_t>(data.size() - element_offset - 4);
    const uint32_t segment_length = static_cast<uint32_t>(data.size() - 137);
    write_u32(data, element_offset, element_length);
    write_u32(data, 129, segment_length);
    write_u32(data, 157, segment_length);
    return data;
}

std::vector<unsigned char> sample_quantized_tristrip()
{
    std::vector<unsigned char> data = sample_tristrip();
    data.resize(216);
    data[188] = 8;
    data[199] = 8;
    for (size_t component = 0; component < 3; ++component) {
	append_f32(data, 0.0f);
	append_f32(data, component < 2 ? 1.0f : 0.0f);
	data.push_back(8);
    }
    append_u32(data, 3, true);
    const uint32_t codes[3][3] = {{0, 255, 0}, {0, 0, 255}, {0, 0, 0}};
    for (const auto &component : codes) {
	data.push_back(0);
	append_u32(data, 3, true);
	for (uint32_t code : component) append_u32(data, code, true);
    }
    data.push_back(0);
    append_u32(data, 3, true);
    for (uint32_t index : {0u, 1u, 2u}) append_u32(data, index, true);

    const uint32_t element_length = static_cast<uint32_t>(data.size() - 161 - 4);
    const uint32_t segment_length = static_cast<uint32_t>(data.size() - 137);
    write_u32(data, 161, element_length);
    write_u32(data, 129, segment_length);
    write_u32(data, 157, segment_length);
    return data;
}

bool expect(bool condition, const char *message)
{
    if (!condition) std::fprintf(stderr, "%s\n", message);
    return condition;
}

} // namespace

bool check_sample(unsigned major, unsigned minor, bool little)
{
    std::string error;
    jt::File file;
    std::vector<unsigned char> data = sample_file(major, minor, little);
    if (!expect(file.load(data, error), error.c_str())) return false;
    if (!expect(file.header().major_version == major && file.header().minor_version == minor, "wrong version")) return false;
    if (!expect(file.header().little_endian == little, "wrong byte order")) return false;
    if (!expect(file.toc().size() == 1 && file.toc()[0].type() == 7, "wrong TOC")) return false;

    jt::Segment segment;
    if (!expect(file.segment(file.toc()[0], segment, error), error.c_str())) return false;
    std::vector<jt::Element> elements;
    if (!expect(file.elements(segment, elements, error), error.c_str())) return false;
    const int32_t expected_object_id = major >= 9 ? 123 : -1;
    return expect(elements.size() == 1 && elements[0].object_id == expected_object_id && elements[0].data_length == 0,
	"wrong element");
}

int main()
{
    if (!check_sample(8, 1, false) || !check_sample(9, 5, true) || !check_sample(10, 0, true)) return 1;

    std::string error;
    jt::File file;
    std::vector<unsigned char> data = sample_file(9, 5, true);
    data.resize(110);
    if (!expect(!file.load(data, error), "truncated file was accepted")) return 1;

    data = sample_file(9, 5, true);
    if (!expect(file.load(data, error), error.c_str())) return 1;
    jt::Segment invalid_segment{};
    invalid_segment.type = 7;
    invalid_segment.offset = data.size();
    invalid_segment.length = 24;
    std::vector<jt::Element> elements;
    if (!expect(!file.elements(invalid_segment, elements, error), "invalid segment range was accepted")) return 1;

    data = sample_file(9, 5, true);
    const uint64_t packet_offset = data.size();
    data.push_back(0);
    append_u32(data, 6, true);
    for (uint32_t value : {10u, 20u, 30u, 40u, 1u, 1u}) append_u32(data, value, true);
    if (!expect(file.load(data, error), error.c_str())) return 1;
    std::vector<int32_t> values;
    size_t bytes_read = 0;
    if (!expect(file.int32_packet(packet_offset, jt::Predictor::Lag1, values, bytes_read, error), error.c_str())) return 1;
    if (!expect(values == std::vector<int32_t>({10, 20, 30, 40, 41, 42}), "wrong predictor output")) return 1;
    if (!expect(bytes_read == 29, "wrong packet length")) return 1;

    data[packet_offset] = 3;
    if (!expect(file.load(data, error), error.c_str())) return 1;
    if (!expect(!file.int32_packet(packet_offset, jt::Predictor::None, values, bytes_read, error),
	"unsupported codec was accepted")) return 1;

    data = sample_file(9, 5, true);
    const uint64_t bitlength_offset = data.size();
    data.push_back(1);
    append_u32(data, 8, true);
    append_u32(data, 2, true);
    append_u32(data, 1, true);
    append_u32(data, 0xcb000000u, true);
    if (!expect(file.load(data, error), error.c_str())) return 1;
    if (!expect(file.int32_packet(bitlength_offset, jt::Predictor::None, values, bytes_read, error), error.c_str())) return 1;
    if (!expect(values == std::vector<int32_t>({1, -1}), "wrong Bitlength CODEC output")) return 1;

    data = sample_tristrip();
    if (!expect(file.load(data, error), error.c_str())) return 1;
    jt::Segment mesh_segment;
    if (!expect(file.segment(file.toc()[0], mesh_segment, error), error.c_str())) return 1;
    if (!expect(file.elements(mesh_segment, elements, error), error.c_str())) return 1;
    jt::Mesh mesh;
    if (!expect(file.legacy_mesh(elements[0], mesh, error), error.c_str())) return 1;
    if (!expect(mesh.vertices.size() == 9 && mesh.faces == std::vector<int>({0, 1, 2}),
	"wrong JT 8 triangle strip mesh")) return 1;

    data[165] = 0x9f;
    if (!expect(file.load(data, error), error.c_str())) return 1;
    if (!expect(file.segment(file.toc()[0], mesh_segment, error), error.c_str())) return 1;
    if (!expect(file.elements(mesh_segment, elements, error), error.c_str())) return 1;
    if (!expect(file.legacy_mesh(elements[0], mesh, error), error.c_str())) return 1;
    if (!expect(mesh.faces == std::vector<int>({0, 1, 2}), "wrong JT 8 polygon mesh")) return 1;

    data = sample_quantized_tristrip();
    if (!expect(file.load(data, error), error.c_str())) return 1;
    if (!expect(file.segment(file.toc()[0], mesh_segment, error), error.c_str())) return 1;
    if (!expect(file.elements(mesh_segment, elements, error), error.c_str())) return 1;
    if (!expect(file.legacy_mesh(elements[0], mesh, error), error.c_str())) return 1;
    if (!expect(mesh.vertices == std::vector<double>({0, 0, 0, 1, 0, 0, 0, 1, 0}),
	"wrong JT 8 quantized coordinates")) return 1;

    data = sample_file(9, 5, true);
    const uint64_t packet2_offset = data.size();
    append_u32(data, 2, true);
    data.push_back(4);
    data.push_back(1);
    data.push_back(2);
    append_u32(data, 2, true);
    data.push_back(0);
    append_u32(data, 0, true);
    append_u32(data, 1, true);
    append_u32(data, 2, true);
    data.push_back(0);
    append_u32(data, 1, true);
    append_u32(data, 0, true);
    if (!expect(file.load(data, error), error.c_str())) return 1;
    if (!expect(file.int32_packet2(packet2_offset, jt::Predictor::None, values, bytes_read, error), error.c_str())) return 1;
    if (!expect(values == std::vector<int32_t>({1, 2}), "wrong JT Int32 CDP2 Chopper output")) return 1;

    data = sample_file(10, 3, true);
    const uint64_t arithmetic_offset = data.size();
    const unsigned char table_arithmetic_packet[] = {
	0x90, 0x00, 0x00, 0x00, 0x03, 0x18, 0x01, 0x00, 0x00, 0x2c, 0xe2, 0xed, 0x80,
	0x4e, 0xa8, 0xa0, 0xdc, 0xa1, 0xd9, 0x28, 0xd4, 0x0b, 0x67, 0x81, 0x0f, 0x7e,
	0xf9, 0xbb, 0x5a, 0x23, 0x02, 0x8d, 0x92, 0x34, 0x99, 0x76, 0xae, 0xf2, 0x3a,
	0x98, 0x53, 0x00, 0x00, 0x00, 0x06, 0x00, 0x05, 0x1c, 0x18, 0x00, 0x00, 0x00,
	0x04, 0x20, 0x0f, 0x02, 0x15, 0x2b, 0x11, 0xf0, 0x04, 0x00, 0x00, 0x00, 0x01,
	0x2d, 0x00, 0x00, 0x00, 0x14, 0x30, 0x01, 0x44, 0x00, 0x00, 0x40, 0x01
    };
    data.insert(data.end(), std::begin(table_arithmetic_packet), std::end(table_arithmetic_packet));
    if (!expect(file.load(data, error), error.c_str())) return 1;
    if (!expect(file.int32_packet2(arithmetic_offset, jt::Predictor::None,
	values, bytes_read, error), error.c_str())) return 1;
    int64_t weighted_sum = 0;
    for (size_t i = 0; i < values.size(); ++i) weighted_sum += static_cast<int64_t>(i + 1) * values[i];
    if (!expect(values.size() == 144 && bytes_read == sizeof(table_arithmetic_packet) &&
	weighted_sum == 66179, "wrong table.jt Arithmetic CODEC output")) return 1;
    return 0;
}
