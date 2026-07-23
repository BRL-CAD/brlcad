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

#include "bu/app.h"

#include <zlib.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <cstdio>
#include <functional>
#include <string>
#include <vector>

namespace {

void append_u32(jt::ByteBuffer &data, uint32_t value, bool little = true)
{
    for (unsigned i = 0; i < 4; ++i) {
	const unsigned shift = (little ? i : 3 - i) * 8;
	data.push_back(static_cast<unsigned char>(value >> shift));
    }
}

void append_u64(jt::ByteBuffer &data, uint64_t value, bool little)
{
    for (unsigned i = 0; i < 8; ++i) {
	const unsigned shift = (little ? i : 7 - i) * 8;
	data.push_back(static_cast<unsigned char>(value >> shift));
    }
}

void write_u32(jt::ByteBuffer &data, size_t offset, uint32_t value)
{
    for (unsigned i = 0; i < 4; ++i) data[offset + i] = static_cast<unsigned char>(value >> (i * 8));
}

void append_u16(jt::ByteBuffer &data, uint16_t value)
{
    data.push_back(static_cast<unsigned char>(value));
    data.push_back(static_cast<unsigned char>(value >> 8));
}

void append_f32(jt::ByteBuffer &data, float value)
{
    uint32_t bits = 0;
    std::memcpy(&bits, &value, sizeof(bits));
    append_u32(data, bits, true);
}

jt::ByteBuffer sample_file(unsigned major, unsigned minor, bool little)
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

    jt::ByteBuffer data(jt::VERSION_LENGTH, ' ');
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

jt::ByteBuffer sample_tristrip()
{
    jt::ByteBuffer data = sample_file(8, 1, true);
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
    /* Primitive List Indices are direct vertex indices (not float offsets): a
     * single three-vertex strip spans vertex indices [0, 3). */
    append_u32(data, 3, true);
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

jt::ByteBuffer sample_quantized_tristrip()
{
    jt::ByteBuffer data = sample_tristrip();
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

/* EXP-042: reference CRC-32 (IEEE 802.3 polynomial), matching the writer's
 * jt_crc32 so a round-trip test can re-derive an element payload's checksum and
 * compare it against the value the writer stored in its checksum footer element. */
uint32_t reference_crc32(const unsigned char *data, size_t len)
{
    uint32_t crc = 0xFFFFFFFFu;
    for (size_t i = 0; i < len; ++i) {
	crc ^= data[i];
	for (int k = 0; k < 8; ++k)
	    crc = (crc & 1) ? (0xEDB88320u ^ (crc >> 1)) : (crc >> 1);
    }
    return crc ^ 0xFFFFFFFFu;
}

/* Build a JT 8 legacy mesh element (Tri-Strip Set or Polygon Set) whose primitive
 * list indices are exactly primitive_starts and whose lossless vertex block holds
 * vertices (interleaved XYZ triples).  When zlib is false the vertex block is raw
 * (compressed_size negative); otherwise it is a zlib-deflated payload.  The three
 * length fields are rewritten from the parsed sample_file layout.  Mirrors the
 * byte layout of sample_tristrip() but parameterised for the boundary/winding/
 * lossless coverage tests. */
jt::ByteBuffer sample_legacy_mesh(bool polygon_set, const std::vector<int32_t> &primitive_starts,
    const std::vector<float> &vertices, bool zlib = false, bool corrupt_zlib = false)
{
    jt::ByteBuffer data = sample_file(8, 1, true);
    const size_t element_offset = 161;
    const std::array<unsigned char, 16> &guid = jt::jt_tristrip_guid;
    std::copy(guid.begin(), guid.end(), data.begin() + element_offset + 4);
    if (polygon_set) data[element_offset + 4] = 0x9f; /* Polygon Set GUID first-field byte. */

    append_u16(data, 1);                 /* vertex_lod_version */
    append_u32(data, 0, true);           /* bindings */
    data.insert(data.end(), 4, 0);       /* quantization (all 0 -> not quantized) */
    append_u16(data, 1);                 /* tristrip_version */
    append_u16(data, 1);                 /* representation_version */
    data.insert(data.end(), 3, 0);       /* normal/texture/color binding */
    data.insert(data.end(), 4, 0);       /* representation_quantization (0 -> not quantized) */

    /* Primitive List Indices as a CDP1 Null packet (Stride1 pass-through). */
    data.push_back(0);
    append_u32(data, static_cast<uint32_t>(primitive_starts.size()), true);
    for (int32_t start : primitive_starts) append_u32(data, static_cast<uint32_t>(start), true);

    /* Lossless vertex block: uncompressed size then compressed size. */
    jt::ByteBuffer raw;
    for (float value : vertices) append_f32(raw, value);
    append_u32(data, static_cast<uint32_t>(raw.size()), true);
    if (zlib) {
	uLongf bound = compressBound(static_cast<uLong>(raw.size()));
	jt::ByteBuffer deflated(static_cast<size_t>(bound));
	if (compress(deflated.data(), &bound, raw.data(), static_cast<uLong>(raw.size())) != Z_OK)
	    return data;
	deflated.resize(static_cast<size_t>(bound));
	if (corrupt_zlib && !deflated.empty()) deflated[deflated.size() / 2] ^= 0xff;
	append_u32(data, static_cast<uint32_t>(deflated.size()), true);
	data.insert(data.end(), deflated.begin(), deflated.end());
    } else {
	append_u32(data, static_cast<uint32_t>(-static_cast<int64_t>(raw.size())), true);
	data.insert(data.end(), raw.begin(), raw.end());
    }

    const uint32_t element_length = static_cast<uint32_t>(data.size() - element_offset - 4);
    const uint32_t segment_length = static_cast<uint32_t>(data.size() - 137);
    write_u32(data, element_offset, element_length);
    write_u32(data, 129, segment_length);
    write_u32(data, 157, segment_length);
    return data;
}

/* Decode a legacy mesh sample and return the resulting Mesh, reporting parse
 * failures through success/error out-parameters. */
bool load_legacy_mesh(const jt::ByteBuffer &data, jt::Mesh &mesh, std::string &error)
{
    jt::File file;
    if (!file.load(data, error)) return false;
    jt::Segment segment;
    if (!file.segment(file.toc()[0], segment, error)) return false;
    std::vector<jt::Element> elements;
    if (!file.elements(segment, elements, error)) return false;
    return file.legacy_mesh(elements[0], mesh, error);
}

/* Build a single-element shape segment for a JT 9.x or 10.x file whose element
 * body is exactly the supplied bytes and whose type GUID is the topological
 * (Tri-Strip Set) shape GUID.  Reuses sample_file()'s fixed layout: the segment
 * is the last thing in the file and holds one logical element, so appending the
 * body and rewriting the element/segment length fields (which sample_file left at
 * an empty-body value) yields a well-formed file that File::load() accepts and
 * File::topological_mesh() can walk.  The element data begins immediately after
 * the element header (GUID + base type + object id for JT >= 9). */
jt::ByteBuffer sample_topological(unsigned major, unsigned minor, const jt::ByteBuffer &body)
{
    jt::ByteBuffer data = sample_file(major, minor, true);
    /* Locate the element header via the parser rather than hand-computing byte
     * offsets, then splice the body in at the element's data offset. */
    jt::File probe;
    std::string error;
    if (!probe.load(data, error)) return data;
    jt::Segment segment;
    if (!probe.segment(probe.toc()[0], segment, error)) return data;
    std::vector<jt::Element> elements;
    if (!probe.elements(segment, elements, error)) return data;
    const size_t data_offset = static_cast<size_t>(elements[0].data_offset);
    /* Overwrite the element type GUID (four bytes into the element header) with the
     * topological shape GUID so is_topological_mesh() accepts it.  The element
     * header is the 4-byte length field, the 16-byte type GUID, a base-type byte,
     * and (for JT >= 9) a 4-byte object id; the length field lies that far before
     * the element data. */
    const size_t element_header_offset = data_offset - (4 + 16 + 1 + (major >= 9 ? 4 : 0));
    std::copy(jt::jt_tristrip_guid.begin(), jt::jt_tristrip_guid.end(),
	data.begin() + element_header_offset + 4);

    data.resize(data_offset);
    data.insert(data.end(), body.begin(), body.end());

    /* Rewrite the element length, the TOC segment length, and the segment-header
     * length so the appended body is inside every enclosing range.  sample_file
     * placed the segment last, so its length equals (file size - segment offset). */
    const uint32_t element_length = static_cast<uint32_t>(data.size() - (element_header_offset + 4));
    write_u32(data, element_header_offset, element_length);
    const uint32_t segment_length = static_cast<uint32_t>(data.size() - static_cast<size_t>(segment.offset));
    /* The TOC entry's length field and the segment header's length field both hold
     * the segment length; find them from the parsed offsets. */
    const size_t toc_length_offset = static_cast<size_t>(probe.header().toc_offset) + 4 +
	16 + (major >= 10 ? 8 : 4);
    write_u32(data, toc_length_offset, segment_length);
    const size_t segment_length_offset = static_cast<size_t>(segment.offset) + 16 + 4;
    write_u32(data, segment_length_offset, segment_length);
    return data;
}

/* Append a valid JT 10.x topologically compressed mesh header (u8 versions, u64
 * bindings, nested logical-element header with base type 9, and the three inner
 * version words) to body.  The individual field values follow topological_mesh()'s
 * accepted-header check (base/vertex-LOD/topomesh/topology versions all 1). */
void append_jt10_topo_header(jt::ByteBuffer &body)
{
    body.push_back(1);                 /* base_version */
    body.push_back(1);                 /* vertex_lod_version */
    append_u64(body, 0, true);         /* outer_bindings */
    append_u32(body, 0, true);         /* nested_length */
    jt::Guid nested{};
    body.insert(body.end(), nested.begin(), nested.end());  /* nested_type */
    body.push_back(9);                 /* nested_base_type */
    append_u32(body, 0, true);         /* nested_object_id */
    body.push_back(1);                 /* topomesh_version */
    append_u32(body, 0, true);         /* vertex_records_id */
    body.push_back(1);                 /* topology_version */
}

/* Append a valid JT 9.x topologically compressed mesh flat header (16-bit version
 * words, a u64 VertexBinding2, TopoMesh-LOD version, TriID, and the compressed-rep
 * version).  topological_mesh() reads every field but only range-checks the reads,
 * so any values parse; distinct sentinels make the read order observable. */
void append_jt9_topo_header(jt::ByteBuffer &body)
{
    append_u16(body, 1);               /* base_version */
    append_u16(body, 1);               /* vertex_lod_version */
    append_u64(body, 0, true);         /* VertexBinding2 */
    append_u16(body, 1);               /* topomesh_lod_version */
    append_u32(body, 0, true);         /* tri_id */
    append_u16(body, 1);               /* comp_rep_version */
}

/* Append a self-describing CDP1 (Load1) Null CODEC packet: a codec byte of 0,
 * an I32 value count, then the raw I32 values.  Mirrors the layout the codec-0
 * branch of File::int32_packet() consumes. */
void append_cdp1_null(jt::ByteBuffer &data, const std::vector<int32_t> &raw)
{
    data.push_back(0);
    append_u32(data, static_cast<uint32_t>(raw.size()), true);
    for (int32_t value : raw) append_u32(data, static_cast<uint32_t>(value), true);
}

/* Append a self-describing CDP2 (Load2) Null CODEC packet: an I32 value count,
 * a codec byte of 0, then the raw I32 values (the layout the codec-0 branch of
 * File::int32_packet2() consumes). */
void append_cdp2_null(jt::ByteBuffer &data, const std::vector<int32_t> &raw)
{
    append_u32(data, static_cast<uint32_t>(raw.size()), true);
    data.push_back(0);
    for (int32_t value : raw) append_u32(data, static_cast<uint32_t>(value), true);
}

/* MSB-first bit accumulator that emits a JT CDP2 Bitlength2 code-text byte
 * stream.  The parser assembles each 32-bit code word MSB-first and, on a
 * little-endian file, reads the four code-text bytes of a word in reverse
 * order, so a fully populated word is simply stored little-endian; a trailing
 * partial word is zero-padded to 32 bits.  bit_count() returns the exact number
 * of significant bits so callers can supply the packet's I32 bit-count field. */
class Cdp2BitWriter {
  public:
    void put(uint32_t value, unsigned width)
    {
	for (unsigned i = 0; i < width; ++i)
	    put_bit((value >> (width - 1 - i)) & 1u);
    }
    void put_signed(int32_t value, unsigned width) { put(static_cast<uint32_t>(value) & mask(width), width); }
    size_t bit_count() const { return bits_.size(); }

    /* Emit the little-endian code-text bytes (multiple of four) for this stream. */
    jt::ByteBuffer code_text() const
    {
	std::vector<uint32_t> words((bits_.size() + 31) / 32, 0);
	for (size_t i = 0; i < bits_.size(); ++i)
	    if (bits_[i]) words[i / 32] |= 1u << (31 - i % 32);
	jt::ByteBuffer out;
	for (uint32_t word : words) append_u32(out, word, true);
	return out;
    }

  private:
    static uint32_t mask(unsigned width) { return width >= 32 ? 0xffffffffu : ((1u << width) - 1u); }
    void put_bit(uint32_t bit) { bits_.push_back(bit != 0); }
    std::vector<bool> bits_;
};

/* Append a self-describing CDP2 Bitlength2 (codec 1) packet built from writer:
 * the I32 value count, the codec byte, the I32 significant-bit count, and the
 * little-endian code text. */
void append_cdp2_bitlength2(jt::ByteBuffer &data, size_t value_count, const Cdp2BitWriter &writer)
{
    append_u32(data, static_cast<uint32_t>(value_count), true);
    data.push_back(1);
    append_u32(data, static_cast<uint32_t>(writer.bit_count()), true);
    jt::ByteBuffer text = writer.code_text();
    data.insert(data.end(), text.begin(), text.end());
}

/* Independent reference implementation of the JT Int32 residual predictors,
 * used by the CDP1 Null CODEC test to cross-check File::int32_packet().  Kept
 * separate from the parser's PredictorCodec so the test verifies behaviour
 * rather than restating the same code path. */
std::vector<int32_t> apply_predictor_reference(std::vector<int32_t> v, jt::Predictor predictor)
{
    if (predictor == jt::Predictor::None) return v;
    const auto add = [](int32_t a, int32_t b) {
	return static_cast<int32_t>(static_cast<uint32_t>(a) + static_cast<uint32_t>(b));
    };
    const auto sub = [](int32_t a, int32_t b) {
	return static_cast<int32_t>(static_cast<uint32_t>(a) - static_cast<uint32_t>(b));
    };
    for (size_t i = 4; i < v.size(); ++i) {
	const int32_t v1 = v[i - 1], v2 = v[i - 2], v4 = v[i - 4];
	int32_t predicted = 0;
	switch (predictor) {
	    case jt::Predictor::Lag1: case jt::Predictor::Xor1: predicted = v1; break;
	    case jt::Predictor::Lag2: case jt::Predictor::Xor2: predicted = v2; break;
	    case jt::Predictor::Stride1: predicted = add(v1, sub(v1, v2)); break;
	    case jt::Predictor::Stride2: predicted = add(v2, sub(v2, v4)); break;
	    case jt::Predictor::StripIndex: {
		const int32_t delta = sub(v2, v4);
		predicted = (delta < 8 && delta > -8) ? add(v2, delta) : add(v2, 2);
		break;
	    }
	    case jt::Predictor::Ramp: predicted = static_cast<int32_t>(i); break;
	    case jt::Predictor::None: predicted = 0; break;
	}
	if (predictor == jt::Predictor::Xor1 || predictor == jt::Predictor::Xor2)
	    v[i] ^= predicted;
	else
	    v[i] = add(v[i], predicted);
    }
    return v;
}

} // namespace

bool check_sample(unsigned major, unsigned minor, bool little)
{
    std::string error;
    jt::File file;
    jt::ByteBuffer data = sample_file(major, minor, little);
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

/* TST-001: CDP1 Null CODEC (codec 0) exercised with every Predictor variant.
 * A Null packet stores its residuals verbatim; File::int32_packet() then applies
 * the predictor, so the decoded output must equal the independent reference. */
bool test_cdp1_null_codec()
{
    const jt::Predictor predictors[] = {
	jt::Predictor::Lag1, jt::Predictor::Lag2, jt::Predictor::Stride1, jt::Predictor::Stride2,
	jt::Predictor::StripIndex, jt::Predictor::Ramp, jt::Predictor::Xor1, jt::Predictor::Xor2,
	jt::Predictor::None};
    /* A mix of sizes, including one below the four-value predictor prime so the
     * pass-through (i < 4) path is covered, and negatives to exercise the
     * wrapping add/subtract used by the stride predictors. */
    const std::vector<std::vector<int32_t>> residual_sets = {
	{7, -3, 11, -5, 2, -8, 100, 0, -1},
	{0, 0, 0, 0, 5},
	{3, -3, 3, -3},
	{2147483647, -2147483647 - 1, 1, -1, 4, 8, 15, 16, 23, 42},
    };
    for (jt::Predictor predictor : predictors) {
	for (const std::vector<int32_t> &residuals : residual_sets) {
	    jt::ByteBuffer data = sample_file(8, 1, true);
	    const uint64_t offset = data.size();
	    append_cdp1_null(data, residuals);
	    jt::File file;
	    std::string error;
	    if (!expect(file.load(data, error), error.c_str())) return false;
	    std::vector<int32_t> values;
	    size_t bytes_read = 0;
	    if (!expect(file.int32_packet(offset, predictor, values, bytes_read, error), error.c_str())) return false;
	    if (!expect(values == apply_predictor_reference(residuals, predictor),
		"CDP1 Null CODEC predictor output mismatch")) return false;
	    if (!expect(bytes_read == 1 + 4 + residuals.size() * 4, "CDP1 Null CODEC packet length")) return false;
	}
    }
    return true;
}

/* TST-002: CDP1 Bitlength CODEC (codec 1) width-prefix and value boundaries.
 * The Bitlength CODEC starts at field_width 0 and adjusts it by +/-2 per width
 * prefix, so achievable widths are even; each value is field_width bits, sign-
 * extended.  These cases drive field_width to 0, 2, and 32 and check sign
 * extension and the full-width (no sign-extension) edge. */
bool test_cdp1_bitlength_boundaries()
{
    struct Case {
	std::function<void(Cdp2BitWriter &)> encode;
	std::vector<int32_t> expected;
	const char *name;
    };
    /* Helper cases are built with the same MSB-first bit writer used for CDP2;
     * for CDP1 the code text is a plain big-endian U32 word array, which the
     * writer's code_text() produces when the file is little-endian (the parser
     * reads the CDP1 words with Reader::u32, i.e. little-endian on an LE file). */
    std::vector<Case> cases;

    /* field_width 0: a leading 0 prefix keeps width 0, so every value decodes to
     * 0 with no value bits consumed. */
    cases.push_back({[](Cdp2BitWriter &w) {
	for (int i = 0; i < 3; ++i) w.put(0, 1);   /* three width-0 values */
    }, {0, 0, 0}, "Bitlength field_width 0"});

    /* field_width 2: prefix 1, direction 1, terminator 0 -> +2; then three 2-bit
     * signed values (01 -> 1, 11 -> -1, 10 -> -2). */
    cases.push_back({[](Cdp2BitWriter &w) {
	w.put(1, 1); w.put(1, 1); w.put(0, 1);     /* value 1 prefix: width 0 -> 2 */
	w.put(0x1, 2);
	w.put(0, 1); w.put(0x3, 2);                /* value 2 prefix 0 (keep width) */
	w.put(0, 1); w.put(0x2, 2);                /* value 3 prefix 0 (keep width) */
    }, {1, -1, -2}, "Bitlength field_width 2 sign extension"});

    /* field_width 32: sixteen "+2" steps (prefix 1, direction 1, fifteen more
     * 1s, terminating 0) then a single full-width value that must NOT be
     * sign-extended by the 1u<<32 shift.  0x80000000 stays INT32_MIN. */
    cases.push_back({[](Cdp2BitWriter &w) {
	w.put(1, 1); w.put(1, 1);                  /* prefix + direction=1 */
	for (int i = 0; i < 15; ++i) w.put(1, 1);  /* continue: +2 fifteen more times -> 32 */
	w.put(0, 1);                               /* terminator (!= direction) */
	w.put(0x80000000u, 32);
    }, {static_cast<int32_t>(0x80000000u)}, "Bitlength field_width 32 no over-extension"});

    for (const Case &c : cases) {
	Cdp2BitWriter writer;
	c.encode(writer);
	jt::ByteBuffer data = sample_file(8, 1, true);
	const uint64_t offset = data.size();
	data.push_back(1);                                   /* CODEC 1: Bitlength */
	append_u32(data, static_cast<uint32_t>(writer.bit_count()), true);
	append_u32(data, static_cast<uint32_t>(c.expected.size()), true);
	jt::ByteBuffer text = writer.code_text();
	append_u32(data, static_cast<uint32_t>(text.size() / 4), true);
	data.insert(data.end(), text.begin(), text.end());
	jt::File file;
	std::string error;
	if (!expect(file.load(data, error), error.c_str())) return false;
	std::vector<int32_t> values;
	size_t bytes_read = 0;
	if (!expect(file.int32_packet(offset, jt::Predictor::None, values, bytes_read, error), c.name)) return false;
	if (!expect(values == c.expected, c.name)) return false;
    }

    /* Truncation: a width-2 prefix that promises a value the code text does not
     * contain must be rejected rather than read past the cap. */
    {
	Cdp2BitWriter writer;
	writer.put(1, 1); writer.put(1, 1); writer.put(0, 1);   /* width -> 2 */
	writer.put(0x1, 2);                                     /* only one value present */
	jt::ByteBuffer data = sample_file(8, 1, true);
	const uint64_t offset = data.size();
	data.push_back(1);
	append_u32(data, static_cast<uint32_t>(writer.bit_count()), true);
	append_u32(data, 3, true);                               /* claim three values */
	jt::ByteBuffer text = writer.code_text();
	append_u32(data, static_cast<uint32_t>(text.size() / 4), true);
	data.insert(data.end(), text.begin(), text.end());
	jt::File file;
	std::string error;
	if (!expect(file.load(data, error), error.c_str())) return false;
	std::vector<int32_t> values;
	size_t bytes_read = 0;
	if (!expect(!file.int32_packet(offset, jt::Predictor::None, values, bytes_read, error),
	    "truncated Bitlength CODEC was accepted")) return false;
    }
    return true;
}

/* TST-003: CDP2 Null CODEC (codec 0) in File::int32_packet2().  Verifies count
 * inheritance is not used at the top level (the packet carries its own count),
 * output buffering of the raw values, and that the requested predictor is
 * applied to the decoded output. */
bool test_cdp2_null_codec()
{
    const std::vector<int32_t> raw = {5, -4, 3, -2, 1, 0, -1, 2, -3};
    struct Case { jt::Predictor predictor; const char *name; };
    const Case cases[] = {
	{jt::Predictor::None, "CDP2 Null None"},
	{jt::Predictor::Lag1, "CDP2 Null Lag1"},
	{jt::Predictor::Stride1, "CDP2 Null Stride1"},
	{jt::Predictor::Xor1, "CDP2 Null Xor1"},
    };
    for (const Case &c : cases) {
	jt::ByteBuffer data = sample_file(9, 5, true);
	const uint64_t offset = data.size();
	append_cdp2_null(data, raw);
	jt::File file;
	std::string error;
	if (!expect(file.load(data, error), error.c_str())) return false;
	std::vector<int32_t> values;
	size_t bytes_read = 0;
	if (!expect(file.int32_packet2(offset, c.predictor, values, bytes_read, error), c.name)) return false;
	if (!expect(values == apply_predictor_reference(raw, c.predictor), c.name)) return false;
	if (!expect(bytes_read == 4 + 1 + raw.size() * 4, "CDP2 Null CODEC packet length")) return false;
    }
    /* A zero-count Null packet yields no values and consumes only its count. */
    {
	jt::ByteBuffer data = sample_file(9, 5, true);
	const uint64_t offset = data.size();
	append_u32(data, 0, true);                 /* value count 0: codec byte absent */
	jt::File file;
	std::string error;
	if (!expect(file.load(data, error), error.c_str())) return false;
	std::vector<int32_t> values;
	size_t bytes_read = 0;
	if (!expect(file.int32_packet2(offset, jt::Predictor::None, values, bytes_read, error),
	    "CDP2 Null zero-count") || !expect(values.empty() && bytes_read == 4,
	    "CDP2 Null zero-count output")) return false;
    }
    return true;
}

/* TST-004: CDP2 Bitlength2 (codec 1) JT 9.x fixed-width variant.  The JT 9 fixed
 * path reads two 6-bit widths, signed min/max, then value_count fields of
 * ceil(log2(range+1)) bits added to minimum.  range == 0 (min == max) and an
 * inverted range both emit a constant block; a general range checks the field
 * width and the sign-extended min/max. */
bool test_cdp2_bitlength2_jt9_fixed()
{
    /* Case A: minimum == maximum == -3 -> constant block, no per-value bits. */
    {
	Cdp2BitWriter w;
	w.put(0, 1);                 /* variable flag = 0 (fixed width) */
	w.put(6, 6); w.put(6, 6);    /* min_bits = 6, max_bits = 6 */
	w.put_signed(-3, 6); w.put_signed(-3, 6);
	jt::ByteBuffer data = sample_file(9, 5, true);
	const uint64_t offset = data.size();
	append_cdp2_bitlength2(data, 4, w);
	jt::File file;
	std::string error;
	if (!expect(file.load(data, error), error.c_str())) return false;
	std::vector<int32_t> values;
	size_t bytes_read = 0;
	if (!expect(file.int32_packet2(offset, jt::Predictor::None, values, bytes_read, error),
	    "JT9 fixed range=0") ||
	    !expect(values == std::vector<int32_t>(4, -3), "JT9 fixed range=0 values")) return false;
    }
    /* Case B: range = 1 (min -1, max 0) -> field_width 1, values {0,-1,0,-1}. */
    {
	Cdp2BitWriter w;
	w.put(0, 1);
	w.put(6, 6); w.put(6, 6);
	w.put_signed(-1, 6); w.put_signed(0, 6);
	w.put(1, 1); w.put(0, 1); w.put(1, 1); w.put(0, 1);  /* codes 1,0,1,0 */
	jt::ByteBuffer data = sample_file(9, 5, true);
	const uint64_t offset = data.size();
	append_cdp2_bitlength2(data, 4, w);
	jt::File file;
	std::string error;
	if (!expect(file.load(data, error), error.c_str())) return false;
	std::vector<int32_t> values;
	size_t bytes_read = 0;
	if (!expect(file.int32_packet2(offset, jt::Predictor::None, values, bytes_read, error),
	    "JT9 fixed range=1") ||
	    !expect(values == std::vector<int32_t>({0, -1, 0, -1}), "JT9 fixed range=1 values")) return false;
    }
    /* Case C: inverted range (min 5 > max 0) -> constant block of minimum. */
    {
	Cdp2BitWriter w;
	w.put(0, 1);
	w.put(6, 6); w.put(6, 6);
	w.put_signed(5, 6); w.put_signed(0, 6);
	jt::ByteBuffer data = sample_file(9, 5, true);
	const uint64_t offset = data.size();
	append_cdp2_bitlength2(data, 3, w);
	jt::File file;
	std::string error;
	if (!expect(file.load(data, error), error.c_str())) return false;
	std::vector<int32_t> values;
	size_t bytes_read = 0;
	if (!expect(file.int32_packet2(offset, jt::Predictor::None, values, bytes_read, error),
	    "JT9 fixed inverted") ||
	    !expect(values == std::vector<int32_t>(3, 5), "JT9 fixed inverted values")) return false;
    }
    /* Case D: min_bits > 32 is rejected. */
    {
	Cdp2BitWriter w;
	w.put(0, 1);
	w.put(33, 6); w.put(6, 6);
	jt::ByteBuffer data = sample_file(9, 5, true);
	const uint64_t offset = data.size();
	append_cdp2_bitlength2(data, 1, w);
	jt::File file;
	std::string error;
	if (!expect(file.load(data, error), error.c_str())) return false;
	std::vector<int32_t> values;
	size_t bytes_read = 0;
	if (!expect(!file.int32_packet2(offset, jt::Predictor::None, values, bytes_read, error),
	    "JT9 fixed min_bits>32 accepted")) return false;
    }
    return true;
}

/* TST-005: CDP2 Bitlength2 (codec 1) JT 9.x variable-width variant.  The header
 * is a signed 32-bit mean, a 3-bit change-width, and a 3-bit run-length width;
 * blocks then carry signed change deltas (escaped at the min/max of the change
 * width) and a run length of field_width-bit values biased by mean. */
bool test_cdp2_bitlength2_jt9_variable()
{
    /* Case A: mean = INT32_MAX, one run of two zero-width values -> all equal to
     * mean.  chg_width 2 (min -2, max 1), run_len_bits 3.  A single delta of 0
     * (not an escape) sets field_width 0. */
    {
	Cdp2BitWriter w;
	w.put(1, 1);                     /* variable flag = 1 */
	w.put_signed(2147483647, 32);    /* mean = INT32_MAX */
	w.put(2, 3);                     /* chg_width_bits = 2 */
	w.put(3, 3);                     /* run_len_bits = 3 */
	w.put_signed(0, 2);              /* delta 0 -> field_width 0, not escape */
	w.put(2, 3);                     /* run length 2 */
	jt::ByteBuffer data = sample_file(9, 5, true);
	const uint64_t offset = data.size();
	append_cdp2_bitlength2(data, 2, w);
	jt::File file;
	std::string error;
	if (!expect(file.load(data, error), error.c_str())) return false;
	std::vector<int32_t> values;
	size_t bytes_read = 0;
	if (!expect(file.int32_packet2(offset, jt::Predictor::None, values, bytes_read, error),
	    "JT9 var mean=MAX") ||
	    !expect(values == std::vector<int32_t>(2, 2147483647), "JT9 var mean=MAX values")) return false;
    }
    /* Case B: mean = INT32_MIN, field_width 3 with signed values, run at capacity.
     * chg_width 2, first delta +1 -> width 1, then delta +1 again (chained via
     * escape at chg_max) ... simpler: use chg_width 3 (min -4, max 3) and a
     * single delta of 3 which is chg_max, so it chains; follow with delta 0 to
     * stop: field_width = 3.  Run length exactly equals value_count (3). */
    {
	Cdp2BitWriter w;
	w.put(1, 1);
	w.put_signed(-2147483647 - 1, 32);   /* mean = INT32_MIN */
	w.put(3, 3);                         /* chg_width_bits = 3 (min -4, max 3) */
	w.put(3, 3);                         /* run_len_bits = 3 */
	w.put_signed(3, 3);                  /* delta +3 == chg_max -> chain */
	w.put_signed(0, 3);                  /* delta 0 -> stop; field_width = 3 */
	w.put(3, 3);                         /* run length 3 == value_count */
	/* three signed 3-bit values: 1, -4, 3 -> biased by mean (INT32_MIN). */
	w.put_signed(1, 3); w.put_signed(-4, 3); w.put_signed(3, 3);
	jt::ByteBuffer data = sample_file(9, 5, true);
	const uint64_t offset = data.size();
	append_cdp2_bitlength2(data, 3, w);
	jt::File file;
	std::string error;
	if (!expect(file.load(data, error), error.c_str())) return false;
	std::vector<int32_t> values;
	size_t bytes_read = 0;
	if (!expect(file.int32_packet2(offset, jt::Predictor::None, values, bytes_read, error),
	    "JT9 var mean=MIN")) return false;
	const int32_t base = -2147483647 - 1;
	const std::vector<int32_t> expected = {
	    static_cast<int32_t>(static_cast<uint32_t>(base) + 1u),
	    static_cast<int32_t>(static_cast<uint32_t>(base) + static_cast<uint32_t>(-4)),
	    static_cast<int32_t>(static_cast<uint32_t>(base) + 3u)};
	if (!expect(values == expected, "JT9 var mean=MIN values")) return false;
    }
    /* Case C: a run length that overflows value_count is rejected. */
    {
	Cdp2BitWriter w;
	w.put(1, 1);
	w.put_signed(0, 32);
	w.put(2, 3);                 /* chg_width_bits = 2 */
	w.put(3, 3);                 /* run_len_bits = 3 */
	w.put_signed(0, 2);          /* field_width 0 */
	w.put(7, 3);                 /* run length 7 > value_count (2) */
	jt::ByteBuffer data = sample_file(9, 5, true);
	const uint64_t offset = data.size();
	append_cdp2_bitlength2(data, 2, w);
	jt::File file;
	std::string error;
	if (!expect(file.load(data, error), error.c_str())) return false;
	std::vector<int32_t> values;
	size_t bytes_read = 0;
	if (!expect(!file.int32_packet2(offset, jt::Predictor::None, values, bytes_read, error),
	    "JT9 var run overflow accepted")) return false;
    }
    return true;
}

/* TST-006: CDP2 Bitlength2 (codec 1) JT 10.x nibbler fixed-width variant.  The
 * JT 10 fixed path decodes min/max via the nibble scheme (4 data bits + 1 more
 * flag per nibble, sign-extended at the final width), computes field_width from
 * the span, and rejects an inverted range or an encoded value beyond the span. */
bool test_cdp2_bitlength2_jt10_nibbler_fixed()
{
    /* Nibble helper: emit value as 4-bit groups with continuation flags.  Callers
     * pass the exact number of nibbles so sign-extension width is controlled. */
    const auto put_nibbles = [](Cdp2BitWriter &w, uint32_t value, unsigned nibbles) {
	for (unsigned i = 0; i < nibbles; ++i) {
	    w.put((value >> (i * 4)) & 0xfu, 4);
	    w.put(i + 1 < nibbles ? 1u : 0u, 1);
	}
    };
    /* Case A: minimum -2, maximum 5 (span 7 -> field_width 3); values 0,7,3. */
    {
	Cdp2BitWriter w;
	w.put(0, 1);                     /* variable flag = 0 */
	put_nibbles(w, static_cast<uint32_t>(-2) & 0xfu, 1);  /* -2 in one nibble sign-extends */
	put_nibbles(w, 5u, 1);
	w.put(0, 3); w.put(7, 3); w.put(3, 3);   /* codes 0,7,3 -> -2, 5, 1 */
	jt::ByteBuffer data = sample_file(10, 3, true);
	const uint64_t offset = data.size();
	append_cdp2_bitlength2(data, 3, w);
	jt::File file;
	std::string error;
	if (!expect(file.load(data, error), error.c_str())) return false;
	std::vector<int32_t> values;
	size_t bytes_read = 0;
	if (!expect(file.int32_packet2(offset, jt::Predictor::None, values, bytes_read, error),
	    "JT10 fixed nibbler") ||
	    !expect(values == std::vector<int32_t>({-2, 5, 1}), "JT10 fixed nibbler values")) return false;
    }
    /* Case B: minimum > maximum is rejected (maximum < minimum branch). */
    {
	Cdp2BitWriter w;
	w.put(0, 1);
	put_nibbles(w, 5u, 1);                         /* minimum 5 */
	put_nibbles(w, static_cast<uint32_t>(-1) & 0xfu, 1); /* maximum -1 */
	jt::ByteBuffer data = sample_file(10, 3, true);
	const uint64_t offset = data.size();
	append_cdp2_bitlength2(data, 1, w);
	jt::File file;
	std::string error;
	if (!expect(file.load(data, error), error.c_str())) return false;
	std::vector<int32_t> values;
	size_t bytes_read = 0;
	if (!expect(!file.int32_packet2(offset, jt::Predictor::None, values, bytes_read, error),
	    "JT10 fixed min>max accepted")) return false;
    }
    /* Case C: an encoded value exceeding the span is rejected. */
    {
	Cdp2BitWriter w;
	w.put(0, 1);
	put_nibbles(w, 0u, 1);                         /* minimum 0 */
	put_nibbles(w, 2u, 1);                         /* maximum 2 -> span 2, field_width 2 */
	w.put(3, 2);                                   /* code 3 > span 2 */
	jt::ByteBuffer data = sample_file(10, 3, true);
	const uint64_t offset = data.size();
	append_cdp2_bitlength2(data, 1, w);
	jt::File file;
	std::string error;
	if (!expect(file.load(data, error), error.c_str())) return false;
	std::vector<int32_t> values;
	size_t bytes_read = 0;
	if (!expect(!file.int32_packet2(offset, jt::Predictor::None, values, bytes_read, error),
	    "JT10 fixed value>span accepted")) return false;
    }
    return true;
}

/* TST-007: CDP2 Bitlength2 (codec 1) JT 10.x nibbler variable-width variant.
 * The header is a nibble-coded mean; blocks carry 4-bit signed change deltas
 * (escaped at -8 and 7), a 4-bit run length, and field_width-bit values biased
 * by mean.  Exercises mean extremes, the delta escape boundaries, and a run
 * that overflows value_count. */
bool test_cdp2_bitlength2_jt10_variable()
{
    const auto put_nibbles = [](Cdp2BitWriter &w, uint32_t value, unsigned nibbles) {
	for (unsigned i = 0; i < nibbles; ++i) {
	    w.put((value >> (i * 4)) & 0xfu, 4);
	    w.put(i + 1 < nibbles ? 1u : 0u, 1);
	}
    };
    /* Case A: mean = INT32_MAX via eight nibbles, field_width 0, run of two. */
    {
	Cdp2BitWriter w;
	w.put(1, 1);                                   /* variable flag = 1 */
	put_nibbles(w, 0x7fffffffu, 8);                /* mean = INT32_MAX */
	w.put(0, 4);                                   /* delta 0 -> field_width 0 (not escape) */
	w.put(2, 4);                                   /* run length 2 */
	jt::ByteBuffer data = sample_file(10, 3, true);
	const uint64_t offset = data.size();
	append_cdp2_bitlength2(data, 2, w);
	jt::File file;
	std::string error;
	if (!expect(file.load(data, error), error.c_str())) return false;
	std::vector<int32_t> values;
	size_t bytes_read = 0;
	if (!expect(file.int32_packet2(offset, jt::Predictor::None, values, bytes_read, error),
	    "JT10 var mean=MAX") ||
	    !expect(values == std::vector<int32_t>(2, 2147483647), "JT10 var mean=MAX values")) return false;
    }
    /* Case B: mean = INT32_MIN; delta 7 (escape, chains) then delta 0 sets
     * field_width 7; a run of two signed 7-bit values biased by mean. */
    {
	Cdp2BitWriter w;
	w.put(1, 1);
	put_nibbles(w, 0x80000000u, 8);                /* mean = INT32_MIN */
	w.put(7, 4);                                   /* delta +7 == escape -> chain */
	w.put(0, 4);                                   /* delta 0 -> stop; field_width 7 */
	w.put(2, 4);                                   /* run length 2 */
	w.put_signed(1, 7); w.put_signed(-1, 7);       /* values 1, -1 */
	jt::ByteBuffer data = sample_file(10, 3, true);
	const uint64_t offset = data.size();
	append_cdp2_bitlength2(data, 2, w);
	jt::File file;
	std::string error;
	if (!expect(file.load(data, error), error.c_str())) return false;
	std::vector<int32_t> values;
	size_t bytes_read = 0;
	if (!expect(file.int32_packet2(offset, jt::Predictor::None, values, bytes_read, error),
	    "JT10 var mean=MIN")) return false;
	const int32_t base = -2147483647 - 1;
	const std::vector<int32_t> expected = {
	    static_cast<int32_t>(static_cast<uint32_t>(base) + 1u),
	    static_cast<int32_t>(static_cast<uint32_t>(base) + static_cast<uint32_t>(-1))};
	if (!expect(values == expected, "JT10 var mean=MIN values")) return false;
    }
    /* Case C: delta -8 (escape, chains) then delta 0 -> negative running width,
     * which the [0,32] clamp rejects. */
    {
	Cdp2BitWriter w;
	w.put(1, 1);
	put_nibbles(w, 0u, 1);                         /* mean 0 */
	w.put(static_cast<uint32_t>(-8) & 0xfu, 4);    /* delta -8 == escape -> chain */
	w.put(0, 4);                                   /* delta 0 -> field_width = -8 */
	w.put(1, 4);                                   /* run length 1 */
	jt::ByteBuffer data = sample_file(10, 3, true);
	const uint64_t offset = data.size();
	append_cdp2_bitlength2(data, 1, w);
	jt::File file;
	std::string error;
	if (!expect(file.load(data, error), error.c_str())) return false;
	std::vector<int32_t> values;
	size_t bytes_read = 0;
	if (!expect(!file.int32_packet2(offset, jt::Predictor::None, values, bytes_read, error),
	    "JT10 var negative width accepted")) return false;
    }
    /* Case D: run length that overflows value_count is rejected. */
    {
	Cdp2BitWriter w;
	w.put(1, 1);
	put_nibbles(w, 0u, 1);                         /* mean 0 */
	w.put(0, 4);                                   /* field_width 0 */
	w.put(15, 4);                                  /* run length 15 > value_count (2) */
	jt::ByteBuffer data = sample_file(10, 3, true);
	const uint64_t offset = data.size();
	append_cdp2_bitlength2(data, 2, w);
	jt::File file;
	std::string error;
	if (!expect(file.load(data, error), error.c_str())) return false;
	std::vector<int32_t> values;
	size_t bytes_read = 0;
	if (!expect(!file.int32_packet2(offset, jt::Predictor::None, values, bytes_read, error),
	    "JT10 var run overflow accepted")) return false;
    }
    return true;
}

/* TST-012: big-endian JT file parsing.  sample_file() already parameterises byte
 * order, so this drives the Reader's u16/u32/u64 byte-swapping and the GUID
 * matching over big-endian header/TOC/segment/element records for JT 8, 9, and
 * 10, and confirms a big-endian file round-trips identically to little-endian. */
bool test_big_endian_parsing()
{
    const unsigned versions[][2] = {{8, 1}, {9, 5}, {10, 3}};
    for (const auto &v : versions) {
	if (!expect(check_sample(v[0], v[1], false), "big-endian sample parse")) return false;
	if (!expect(check_sample(v[0], v[1], true), "little-endian sample parse")) return false;
    }
    /* A big-endian CDP1 Null packet must decode identically to little-endian:
     * the Reader must swap the codec header's I32 count and each I32 value. */
    {
	const std::vector<int32_t> raw = {100, -200, 300, -400, 500};
	jt::ByteBuffer data = sample_file(9, 5, false);
	const uint64_t offset = data.size();
	data.push_back(0);                                 /* CODEC 0: Null */
	append_u32(data, static_cast<uint32_t>(raw.size()), false);
	for (int32_t value : raw) append_u32(data, static_cast<uint32_t>(value), false);
	jt::File file;
	std::string error;
	if (!expect(file.load(data, error), error.c_str())) return false;
	if (!expect(!file.header().little_endian, "big-endian flag not detected")) return false;
	std::vector<int32_t> values;
	size_t bytes_read = 0;
	if (!expect(file.int32_packet(offset, jt::Predictor::None, values, bytes_read, error),
	    "big-endian CDP1 Null decode") ||
	    !expect(values == raw, "big-endian CDP1 Null values")) return false;
    }
    return true;
}

/* TST-015: JT 10.x topologically compressed mesh nested-element header
 * (topological_mesh(), JT 10 branch).  A correct header (base/vertex-LOD/topomesh/
 * topology versions all 1, nested base type 9) must be fully consumed so the
 * failure comes from the missing symbol streams -- never the header check itself.
 * Each version/base-type field is then corrupted in turn to confirm the header
 * validation rejects it with the JT 10 header error. */
bool test_jt10_topological_mesh_header()
{
    const auto load_topo = [](const jt::ByteBuffer &body, jt::File &file, jt::Element &element,
	std::string &error) {
	jt::ByteBuffer data = sample_topological(10, 3, body);
	if (!file.load(data, error)) return false;
	jt::Segment segment;
	if (!file.segment(file.toc()[0], segment, error)) return false;
	std::vector<jt::Element> elements;
	if (!file.elements(segment, elements, error)) return false;
	element = elements[0];
	return true;
    };

    /* A valid header with no following symbol data: the header itself is accepted,
     * so the reported error is not the JT 10 header error. */
    {
	jt::ByteBuffer body;
	append_jt10_topo_header(body);
	jt::File file;
	jt::Element element{};
	std::string error;
	if (!expect(load_topo(body, file, element, error), error.c_str())) return false;
	if (!expect(file.is_topological_mesh(element), "JT 10 topological element not recognised")) return false;
	jt::Mesh mesh;
	if (!expect(!file.topological_mesh(element, mesh, error), "empty JT 10 topo body was accepted")) return false;
	if (!expect(error != "invalid JT 10 topologically compressed mesh header",
	    "valid JT 10 header rejected as malformed")) return false;
    }

    /* Corrupt each guarded header field in turn (base type stays out of the version
     * set so the byte offsets are those the parser reads). */
    struct Corruption { size_t index; unsigned char value; const char *name; };
    const Corruption corruptions[] = {
	{0, 2, "JT 10 base_version mismatch"},         /* [0] base_version u8 */
	{1, 2, "JT 10 vertex_lod_version mismatch"},   /* [1] vertex_lod_version u8 */
	{30, 8, "JT 10 nested_base_type mismatch"},    /* [2..9] u64 +[10..13] u32 +[14..29] guid */
	{35, 2, "JT 10 topomesh_version mismatch"},    /* [30] base type +[31..34] object id */
	{40, 2, "JT 10 topology_version mismatch"},    /* [35] topomesh ver +[36..39] vertex records id */
    };
    for (const Corruption &c : corruptions) {
	jt::ByteBuffer body;
	append_jt10_topo_header(body);
	body[c.index] = c.value;
	jt::File file;
	jt::Element element{};
	std::string error;
	if (!expect(load_topo(body, file, element, error), error.c_str())) return false;
	jt::Mesh mesh;
	if (!expect(!file.topological_mesh(element, mesh, error), c.name)) return false;
	if (!expect(error == "invalid JT 10 topologically compressed mesh header", c.name)) return false;
    }
    return true;
}

/* TST-016: JT 9.x topologically compressed mesh flat header (topological_mesh(),
 * JT 9 branch).  The 16-bit versions, u64 VertexBinding2, TriID, and compressed-
 * rep version are all read but unused; the parser only range-checks them, so a
 * complete header advances into the symbol streams (failing there, not on the
 * header) while a truncated header fails on the header read itself. */
bool test_jt9_topological_mesh_header()
{
    const auto load_topo = [](const jt::ByteBuffer &body, jt::File &file, jt::Element &element,
	std::string &error) {
	jt::ByteBuffer data = sample_topological(9, 5, body);
	if (!file.load(data, error)) return false;
	jt::Segment segment;
	if (!file.segment(file.toc()[0], segment, error)) return false;
	std::vector<jt::Element> elements;
	if (!file.elements(segment, elements, error)) return false;
	element = elements[0];
	return true;
    };

    /* Complete header, no symbol streams: every header field is consumed, so the
     * failure is not the JT 9 header error. */
    {
	jt::ByteBuffer body;
	append_jt9_topo_header(body);
	jt::File file;
	jt::Element element{};
	std::string error;
	if (!expect(load_topo(body, file, element, error), error.c_str())) return false;
	if (!expect(file.is_topological_mesh(element), "JT 9 topological element not recognised")) return false;
	jt::Mesh mesh;
	if (!expect(!file.topological_mesh(element, mesh, error), "empty JT 9 topo body was accepted")) return false;
	if (!expect(error != "invalid JT 9 topologically compressed mesh header",
	    "valid JT 9 header rejected as malformed")) return false;
    }

    /* Truncate the header one byte short of the compressed-rep version (the flat
     * header is 20 bytes) so the final u16 read fails and the header error fires. */
    {
	jt::ByteBuffer body;
	append_jt9_topo_header(body);
	body.resize(body.size() - 1);
	jt::File file;
	jt::Element element{};
	std::string error;
	if (!expect(load_topo(body, file, element, error), error.c_str())) return false;
	jt::Mesh mesh;
	if (!expect(!file.topological_mesh(element, mesh, error), "truncated JT 9 header accepted")) return false;
	if (!expect(error == "invalid JT 9 topologically compressed mesh header",
	    "truncated JT 9 header wrong error")) return false;
    }
    return true;
}

/* TST-020: JT 8 triangle-strip primitive-list boundary parsing (legacy_mesh(),
 * the primitive_starts validation loop).  The boundaries must start at 0, stay
 * non-decreasing, and end at the vertex count; anything else is rejected.  A
 * valid three-vertex, one-strip case and a trailing boundary that already equals
 * the vertex count confirm the accepting paths. */
bool test_jt8_primitive_boundaries()
{
    /* A single strip of three vertices: boundaries {0, 3} == {0, vertex_count}. */
    const std::vector<float> tri = {0, 0, 0, 1, 0, 0, 0, 1, 0};  /* three XYZ vertices */
    {
	jt::Mesh mesh;
	std::string error;
	if (!expect(load_legacy_mesh(sample_legacy_mesh(false, {0, 3}, tri), mesh, error), error.c_str()))
	    return false;
	if (!expect(mesh.faces == std::vector<int>({0, 1, 2}), "valid strip boundaries")) return false;
    }
    /* Boundaries not starting at 0 are rejected. */
    {
	jt::Mesh mesh;
	std::string error;
	if (!expect(!load_legacy_mesh(sample_legacy_mesh(false, {1, 3}, tri), mesh, error),
	    "non-zero first boundary accepted")) return false;
    }
    /* An empty boundary list (fewer than two entries) is rejected. */
    {
	jt::Mesh mesh;
	std::string error;
	if (!expect(!load_legacy_mesh(sample_legacy_mesh(false, {}, tri), mesh, error),
	    "empty boundary list accepted")) return false;
    }
    /* Out-of-order (decreasing) boundaries are rejected. */
    {
	jt::Mesh mesh;
	std::string error;
	if (!expect(!load_legacy_mesh(sample_legacy_mesh(false, {0, 3, 2}, tri), mesh, error),
	    "decreasing boundaries accepted")) return false;
    }
    /* A boundary beyond the vertex count is rejected. */
    {
	jt::Mesh mesh;
	std::string error;
	if (!expect(!load_legacy_mesh(sample_legacy_mesh(false, {0, 4}, tri), mesh, error),
	    "boundary past vertex count accepted")) return false;
    }
    /* Duplicate boundaries (zero-length strip) are tolerated: the degenerate strip
     * yields no triangles but the extra boundary at the vertex count still closes
     * a real strip, so the mesh parses. */
    {
	jt::Mesh mesh;
	std::string error;
	if (!expect(load_legacy_mesh(sample_legacy_mesh(false, {0, 0, 3}, tri), mesh, error), error.c_str()))
	    return false;
	if (!expect(mesh.faces == std::vector<int>({0, 1, 2}), "duplicate boundary strip")) return false;
    }
    return true;
}

/* TST-021: JT 8 polygon-set vs triangle-strip winding (legacy_mesh() ->
 * fan_triangulate()).  A polygon set fans every triangle from the strip's first
 * vertex; a tri-strip walks the strip and swaps the leading pair on odd steps.
 * A four-vertex strip makes the two triangulations distinguishable. */
bool test_jt8_polygon_vs_tristrip_winding()
{
    const std::vector<float> quad = {0, 0, 0, 1, 0, 0, 0, 1, 0, 1, 1, 0};  /* four XYZ vertices */
    /* Tri-strip: vertices 0..3 -> triangle (0,1,2) then, on the odd step, the
     * swapped triangle (2,1,3) to preserve winding. */
    {
	jt::Mesh mesh;
	std::string error;
	if (!expect(load_legacy_mesh(sample_legacy_mesh(false, {0, 4}, quad), mesh, error), error.c_str()))
	    return false;
	if (!expect(mesh.faces == std::vector<int>({0, 1, 2, 2, 1, 3}), "tri-strip winding")) return false;
    }
    /* Polygon set: fan from vertex 0 -> triangles (0,1,2) and (0,2,3). */
    {
	jt::Mesh mesh;
	std::string error;
	if (!expect(load_legacy_mesh(sample_legacy_mesh(true, {0, 4}, quad), mesh, error), error.c_str()))
	    return false;
	if (!expect(mesh.faces == std::vector<int>({0, 1, 2, 0, 2, 3}), "polygon-set winding")) return false;
    }
    return true;
}

/* TST-022: JT 8 lossless uncompressed vertex data (legacy_mesh() lossless block).
 * compressed_size < 0 stores the raw floats verbatim; compressed_size > 0 is a
 * zlib-deflated payload that must inflate to exactly uncompressed_size; a corrupt
 * deflate stream or an inflated-size mismatch is rejected. */
bool test_jt8_lossless_vertex_data()
{
    const std::vector<float> tri = {0, 0, 0, 1, 0, 0, 0, 1, 0};
    const std::vector<double> expected = {0, 0, 0, 1, 0, 0, 0, 1, 0};
    /* Raw path (compressed_size negative). */
    {
	jt::Mesh mesh;
	std::string error;
	if (!expect(load_legacy_mesh(sample_legacy_mesh(false, {0, 3}, tri, false), mesh, error), error.c_str()))
	    return false;
	if (!expect(mesh.vertices == expected, "raw lossless vertices")) return false;
    }
    /* zlib path (compressed_size positive), round-tripping the same coordinates. */
    {
	jt::Mesh mesh;
	std::string error;
	if (!expect(load_legacy_mesh(sample_legacy_mesh(false, {0, 3}, tri, true), mesh, error), error.c_str()))
	    return false;
	if (!expect(mesh.vertices == expected, "zlib lossless vertices")) return false;
    }
    /* A corrupted zlib stream fails to inflate and is rejected. */
    {
	jt::Mesh mesh;
	std::string error;
	if (!expect(!load_legacy_mesh(sample_legacy_mesh(false, {0, 3}, tri, true, true), mesh, error),
	    "corrupt zlib stream accepted")) return false;
    }
    /* An uncompressed-size that disagrees with the inflated output is rejected: a
     * valid deflate of three vertices but a claimed size of one extra vertex. */
    {
	jt::ByteBuffer data = sample_legacy_mesh(false, {0, 3}, tri, true);
	/* Bump the stored uncompressed_size (the u32 immediately before the vertex
	 * block) so the inflated length no longer matches.  Locate it by parsing. */
	jt::File file;
	std::string error;
	if (!expect(file.load(data, error), error.c_str())) return false;
	jt::Segment segment;
	if (!expect(file.segment(file.toc()[0], segment, error), error.c_str())) return false;
	std::vector<jt::Element> elements;
	if (!expect(file.elements(segment, elements, error), error.c_str())) return false;
	/* Metadata (22 bytes) + primitive-list Null packet (1 + 4 + 2*4) precede the
	 * uncompressed_size field within the element body. */
	const size_t size_field = static_cast<size_t>(elements[0].data_offset) + 22 + 1 + 4 + 2 * 4;
	write_u32(data, size_field, static_cast<uint32_t>((tri.size() + 3) * sizeof(float)));
	jt::Mesh mesh;
	if (!expect(!load_legacy_mesh(data, mesh, error), "inflated-size mismatch accepted")) return false;
    }
    return true;
}

/* Reference implementation of the JT 9/10 uniform coordinate dequantization
 * (value = min + (code - 0.5)*(max-min)/max_code, max_code = 2^bits capped at the
 * full unsigned range), kept separate from quantize_coordinate() so the test
 * verifies the parser's behaviour rather than restating its code. */
double quantize_reference(float minimum, float maximum, uint8_t bits, int32_t code)
{
    const double max_code = bits >= 32 ? 4294967295.0 :
	static_cast<double>(static_cast<uint32_t>(1) << bits);
    return static_cast<double>(minimum) +
	(static_cast<double>(static_cast<uint32_t>(code)) - 0.5) *
	(static_cast<double>(maximum) - static_cast<double>(minimum)) / max_code;
}

/* Build the body of a JT 10.x single-triangle topologically compressed mesh whose
 * three coordinate records are dequantized with the given per-axis quantizer.  The
 * topology symbol streams describe one triangle (two valence-3 dual vertices, three
 * degree-2 faces); the vertex records then follow.  bits == 0 selects the lossless
 * exponent/mantissa float codec (codes are IEEE-754 bit fields); bits > 0 selects
 * uniform quantization (one code stream per axis).  All symbol/coordinate arrays
 * are size <= 3, so a Null CDP2 packet stores them verbatim regardless of the
 * predictor the parser applies (its unpack loop starts at index 4). */
jt::ByteBuffer topological_triangle_body(float minimum, float maximum, uint8_t bits,
    const std::array<int32_t, 3> &codes)
{
    jt::ByteBuffer body;
    append_jt10_topo_header(body);
    /* A zero-count CDP2 Null packet is just its 4-byte count: int32_packet2 does
     * not read a codec byte when the count is zero, so an empty stream must not
     * emit one (unlike append_cdp2_null, which serves non-empty packets). */
    const auto empty_packet = [](jt::ByteBuffer &out) { append_u32(out, 0, true); };
    /* face_degrees[0..7]: three degree-2 faces spread across contexts 0 and 1. */
    append_cdp2_null(body, {2, 2});   /* context 0 */
    append_cdp2_null(body, {2});      /* context 1 */
    for (int i = 2; i < 8; ++i) empty_packet(body);
    append_cdp2_null(body, {3, 3});   /* vertex_valences */
    append_cdp2_null(body, {7, 7});   /* vertex_groups */
    append_cdp2_null(body, {0, 1});   /* vertex_flags (second vertex is the cover) */
    append_cdp2_null(body, {0, 0, 0});/* face_attribute_masks[0] lower 32 bits */
    for (int i = 1; i < 8; ++i) empty_packet(body);
    empty_packet(body);               /* upper masks (context 7 high 32 bits) */
    append_u32(body, 0, true);        /* JT10 high-degree masks: VecU32 count 0 */
    empty_packet(body);               /* split_face_offsets */
    empty_packet(body);               /* split_face_positions */

    /* topological_mesh() skips a u32 before the vertex records. */
    append_u32(body, 0, true);
    append_u64(body, 0, true);        /* vertex bindings */
    for (int i = 0; i < 4; ++i) body.push_back(0);  /* quantization descriptor */
    append_u32(body, 3, true);        /* topological_vertex_count */
    append_u32(body, 0, true);        /* attribute_count */
    append_u32(body, 3, true);        /* coordinate_count */
    body.push_back(3);                /* components */
    for (int axis = 0; axis < 3; ++axis) {
	append_f32(body, minimum);
	append_f32(body, maximum);
	body.push_back(bits);
    }
    if (bits == 0) {
	/* Lossless codec: per axis an exponent stream then a mantissa stream, each
	 * reassembled as (exp << 23) | mantissa.  Encode the IEEE-754 field of the
	 * float 1.0f so the reconstructed coordinate is finite and known. */
	const uint32_t pattern = 0x3f800000u;   /* 1.0f */
	const int32_t exponent = static_cast<int32_t>(pattern >> 23);
	const int32_t mantissa = static_cast<int32_t>(pattern & 0x7fffffu);
	for (int axis = 0; axis < 3; ++axis) {
	    append_cdp2_null(body, {exponent, exponent, exponent});
	    append_cdp2_null(body, {mantissa, mantissa, mantissa});
	}
    } else {
	for (int axis = 0; axis < 3; ++axis)
	    append_cdp2_null(body, {codes[0], codes[1], codes[2]});
    }
    return body;
}

/* TST-018: JT 9/10 coordinate quantization edge cases (topological_mesh() ->
 * quantize_coordinate()).  bits == 0 exercises the lossless IEEE-754 codec; bits
 * == 1 the two-code uniform case; bits == 32 the full unsigned code span.  For the
 * quantized cases the decoded coordinates must equal the documented formula
 * value = min + (code - 0.5)*(max-min)/max_code to within float round-off. */
bool test_coordinate_quantization_edges()
{
    const auto decode = [](const jt::ByteBuffer &body, jt::Mesh &mesh, std::string &error) {
	jt::ByteBuffer data = sample_topological(10, 3, body);
	jt::File file;
	if (!file.load(data, error)) return false;
	jt::Segment segment;
	if (!file.segment(file.toc()[0], segment, error)) return false;
	std::vector<jt::Element> elements;
	if (!file.elements(segment, elements, error)) return false;
	return file.topological_mesh(elements[0], mesh, error);
    };

    /* bits == 0: lossless float codec.  Each of the three coordinates on every axis
     * reconstructs the float 1.0f. */
    {
	jt::Mesh mesh;
	std::string error;
	if (!expect(decode(topological_triangle_body(0.0f, 0.0f, 0, {0, 0, 0}), mesh, error), error.c_str()))
	    return false;
	if (!expect(mesh.vertices == std::vector<double>(9, 1.0), "lossless coordinate decode")) return false;
    }

    /* bits == 1 (max_code 2) and bits == 32 (full unsigned span): the decoded
     * coordinates must match the reference formula. */
    struct Case { float minimum; float maximum; uint8_t bits; std::array<int32_t, 3> codes; const char *name; };
    const Case cases[] = {
	{0.0f, 4.0f, 1, {{0, 1, 2}}, "bits=1 two-code quantization"},
	{0.0f, 2.0f, 32, {{0, static_cast<int32_t>(2147483648u), static_cast<int32_t>(4294967295u)}},
	    "bits=32 full-range quantization"},
    };
    for (const Case &c : cases) {
	jt::Mesh mesh;
	std::string error;
	if (!expect(decode(topological_triangle_body(c.minimum, c.maximum, c.bits, c.codes), mesh, error),
	    c.name)) return false;
	if (!expect(mesh.vertices.size() == 9, c.name)) return false;
	bool matched = true;
	for (int vertex = 0; vertex < 3 && matched; ++vertex)
	    for (int axis = 0; axis < 3 && matched; ++axis) {
		const double expected = quantize_reference(c.minimum, c.maximum, c.bits, c.codes[vertex]);
		const double actual = mesh.vertices[static_cast<size_t>(vertex) * 3 + axis];
		const double tolerance = std::abs(expected) * 1e-9 + 1e-9;
		if (std::abs(actual - expected) > tolerance) matched = false;
	    }
	if (!expect(matched, c.name)) return false;
    }
    return true;
}

/* TST-030: malformed TOC entry (offset/length outside the file).  File::load()
 * runs range_valid() on every TOC entry's [offset, offset+length) window; a
 * corrupted offset past end-of-file or a length that overruns the remaining
 * bytes must be rejected.  The segment()/elements() range_valid() callsites are
 * exercised too: a TOC entry that passes load() but whose segment window is then
 * shortened out from under a Segment must fail there as well. */
bool test_toc_bounds_violations()
{
    /* Locate the TOC entry's offset and length fields in a JT 9.x file (u32
     * offset for JT < 10, then a u32 length).  The TOC begins with a u32 count. */
    const size_t toc_entry_offset_field = 105 + 4 + 4 + 16;   /* header + wide? no + count + guid */
    (void)toc_entry_offset_field;

    /* Case A: entry offset past end-of-file. */
    {
	jt::ByteBuffer data = sample_file(9, 5, true);
	jt::File probe;
	std::string error;
	if (!expect(probe.load(data, error), error.c_str())) return false;
	const size_t toc = static_cast<size_t>(probe.header().toc_offset);
	const size_t offset_field = toc + 4 + 16;   /* count(4) + entry guid(16) */
	write_u32(data, offset_field, static_cast<uint32_t>(data.size() + 4096));
	jt::File file;
	if (!expect(!file.load(data, error), "TOC entry offset past EOF was accepted")) return false;
    }
    /* Case B: entry length overruns the remaining bytes. */
    {
	jt::ByteBuffer data = sample_file(9, 5, true);
	jt::File probe;
	std::string error;
	if (!expect(probe.load(data, error), error.c_str())) return false;
	const size_t toc = static_cast<size_t>(probe.header().toc_offset);
	const size_t length_field = toc + 4 + 16 + 4;   /* count + guid + offset */
	write_u32(data, length_field, 0x0fffffffu);      /* huge but under MAX_SEGMENT_LENGTH */
	jt::File file;
	if (!expect(!file.load(data, error), "TOC entry length overrun was accepted")) return false;
    }
    /* Case C: a Segment whose window is valid at load() but is later checked
     * against a buffer that no longer contains it (segment() callsite).  Build a
     * TocEntry by hand with an out-of-range offset and confirm segment() rejects
     * it without reading out of bounds. */
    {
	jt::ByteBuffer data = sample_file(9, 5, true);
	jt::File file;
	std::string error;
	if (!expect(file.load(data, error), error.c_str())) return false;
	jt::TocEntry entry = file.toc()[0];
	entry.offset = data.size() + 1;   /* past end of file */
	jt::Segment segment;
	if (!expect(!file.segment(entry, segment, error), "segment() accepted an out-of-range TOC entry")) return false;
    }
    /* Case D: elements() range_valid() callsite: a shape Segment whose declared
     * length runs past the buffer must be rejected. */
    {
	jt::ByteBuffer data = sample_file(9, 5, true);
	jt::File file;
	std::string error;
	if (!expect(file.load(data, error), error.c_str())) return false;
	jt::Segment segment{};
	segment.type = 7;
	segment.offset = 0;
	segment.length = static_cast<uint32_t>(data.size() + 4096);
	std::vector<jt::Element> elements;
	if (!expect(!file.elements(segment, elements, error), "elements() accepted an over-long segment")) return false;
    }
    return true;
}

/* TST-031: truncated logical element header in a shape segment.  elements()
 * reads a u32 element_length, a 16-byte type GUID, a base-type byte, and (for
 * JT >= 9) a 4-byte object id.  Truncating the file so each successive field is
 * missing must yield a parse failure and never read past the buffer. */
bool test_truncated_element_header()
{
    /* A JT 9.5 sample_file() carries exactly one element (base type 4, object id
     * 123).  Its element header lies at a fixed offset that the parser locates;
     * truncate the whole file just short of each field to force the read failure. */
    jt::File probe;
    std::string error;
    jt::ByteBuffer full = sample_file(9, 5, true);
    if (!expect(probe.load(full, error), error.c_str())) return false;
    jt::Segment segment;
    if (!expect(probe.segment(probe.toc()[0], segment, error), error.c_str())) return false;
    std::vector<jt::Element> elements;
    if (!expect(probe.elements(segment, elements, error), error.c_str())) return false;
    /* Element header start = data_offset - (base_type + object_id) - 16 - 4. */
    const size_t data_offset = static_cast<size_t>(elements[0].data_offset);
    const size_t header_start = data_offset - (1 + 4) - 16 - 4;   /* JT>=9 object id */

    /* For each field, shorten the segment so the element body claims to reach the
     * segment end but the field cannot be read.  Truncating the whole file is not
     * enough on its own (load() re-validates the TOC), so instead point a
     * hand-built shape Segment at a buffer truncated mid-header. */
    struct Trunc { size_t keep; const char *name; };
    const Trunc cases[] = {
	{header_start + 4 + 8,      "truncated element GUID"},        /* mid-GUID */
	{header_start + 4 + 16,     "truncated element base type"},   /* after GUID, no base type */
	{header_start + 4 + 16 + 1, "truncated element object id"},   /* base type, no object id */
    };
    for (const Trunc &c : cases) {
	jt::ByteBuffer data(full.begin(), full.begin() + static_cast<std::ptrdiff_t>(c.keep));
	jt::File file;
	std::string err;
	/* load() may reject the truncated file outright (TOC past EOF); if it
	 * parses, elements() must fail on the missing field.  Either way there
	 * must be no crash and a non-empty diagnostic. */
	if (file.load(data, err)) {
	    jt::Segment seg{};
	    seg.type = 7;
	    seg.offset = segment.offset;
	    seg.length = static_cast<uint32_t>(data.size() - static_cast<size_t>(segment.offset));
	    std::vector<jt::Element> els;
	    if (!expect(!file.elements(seg, els, err), c.name)) return false;
	} else if (!expect(!err.empty(), c.name)) {
	    return false;
	}
    }
    return true;
}

/* TST-035: Reader boundary violations.  The parser's internal Reader refuses any
 * read whose window extends past end-of-buffer (can_read()); exercised here
 * through the public File API by truncating a file just before each field a load
 * reads (u32 reserved, the u32/u64 TOC offset, the 16-byte GUID, the TOC count,
 * and a packet's u8 codec / u32 count).  Every case must fail cleanly. */
bool test_reader_boundary_violations()
{
    /* Truncating the fixed-layout header at successive byte counts starves each
     * successive Reader read (u32 reserved at 81, TOC offset at 85, LSG GUID at
     * 89..104).  A file shorter than the minimum header is rejected up front. */
    const size_t truncations[] = {80, 82, 88, 100, 104};
    for (size_t keep : truncations) {
	jt::ByteBuffer full = sample_file(9, 5, true);
	if (keep >= full.size()) continue;
	jt::ByteBuffer data(full.begin(), full.begin() + static_cast<std::ptrdiff_t>(keep));
	jt::File file;
	std::string error;
	if (!expect(!file.load(data, error), "truncated header field was accepted")) return false;
    }

    /* A well-formed file with a packet truncated before its u8 codec byte and
     * before its u32 count must make int32_packet()/int32_packet2() fail without
     * reading past the buffer. */
    {
	jt::ByteBuffer data = sample_file(9, 5, true);
	const uint64_t offset = data.size();   /* packet begins at EOF: no bytes to read */
	jt::File file;
	std::string error;
	if (!expect(file.load(data, error), error.c_str())) return false;
	std::vector<int32_t> values;
	size_t bytes_read = 0;
	if (!expect(!file.int32_packet(offset, jt::Predictor::None, values, bytes_read, error),
	    "int32_packet read past EOF was accepted")) return false;
	if (!expect(!file.int32_packet2(offset, jt::Predictor::None, values, bytes_read, error),
	    "int32_packet2 read past EOF was accepted")) return false;
    }
    /* A CDP1 packet with a codec byte but no value count (one byte only) must
     * fail on the u32 count read. */
    {
	jt::ByteBuffer data = sample_file(9, 5, true);
	const uint64_t offset = data.size();
	data.push_back(0);   /* codec 0 (Null), but no count follows */
	jt::File file;
	std::string error;
	if (!expect(file.load(data, error), error.c_str())) return false;
	std::vector<int32_t> values;
	size_t bytes_read = 0;
	if (!expect(!file.int32_packet(offset, jt::Predictor::None, values, bytes_read, error),
	    "int32_packet missing count was accepted")) return false;
    }
    return true;
}

/* TST-042: predictor codec matrix.  unpack_residuals() applies each of the nine
 * predictors to the residual stream a CDP produces, so every (CDP, predictor)
 * pairing must reproduce the independent predictor reference.  The CDPs covered
 * are CDP1 Null and CDP1 Bitlength (File::int32_packet) and CDP2 Bitlength2
 * (File::int32_packet2); a fixed value set is round-tripped through each. */
bool test_predictor_codec_matrix()
{
    const jt::Predictor predictors[] = {
	jt::Predictor::Lag1, jt::Predictor::Lag2, jt::Predictor::Stride1, jt::Predictor::Stride2,
	jt::Predictor::StripIndex, jt::Predictor::Ramp, jt::Predictor::Xor1, jt::Predictor::Xor2,
	jt::Predictor::None};
    /* Ten values inside [-128, 127] so the fixed-width encoders below stay small;
     * the leading four are untouched by any predictor (i < 4 pass-through). */
    const std::vector<int32_t> raw = {3, -2, 5, -4, 0, 7, -1, 2, -3, 6};

    /* CDP1 Bitlength encoder at a fixed even field width: the width prefix walks
     * from 0 up by +2 per step (prefix bit 1, direction bit 1, then continuation
     * 1s, terminated by a 0), after which each value is width bits, signed. */
    const auto encode_cdp1_bitlength = [](const std::vector<int32_t> &values, unsigned width) {
	Cdp2BitWriter w;
	const unsigned steps = width / 2;   /* width is even */
	bool first = true;
	for (int32_t v : values) {
	    if (first) {
		/* First value carries the width prefix that walks field_width to
		 * width in +2 steps: prefix 1, direction 1, (steps-1) continuation
		 * 1s, then a 0 terminator. */
		w.put(1, 1);
		w.put(1, 1);
		for (unsigned i = 1; i < steps; ++i) w.put(1, 1);
		w.put(0, 1);
		first = false;
	    } else {
		w.put(0, 1);   /* keep current width for every later value */
	    }
	    w.put_signed(v, width);
	}
	return w;
    };

    for (jt::Predictor predictor : predictors) {
	const std::vector<int32_t> expected = apply_predictor_reference(raw, predictor);

	/* CDP1 Null. */
	{
	    jt::ByteBuffer data = sample_file(8, 1, true);
	    const uint64_t offset = data.size();
	    append_cdp1_null(data, raw);
	    jt::File file;
	    std::string error;
	    if (!expect(file.load(data, error), error.c_str())) return false;
	    std::vector<int32_t> values;
	    size_t bytes_read = 0;
	    if (!expect(file.int32_packet(offset, predictor, values, bytes_read, error),
		"matrix CDP1 Null decode") || !expect(values == expected, "matrix CDP1 Null predictor"))
		return false;
	}
	/* CDP1 Bitlength (width 8 holds every value in [-128, 127]). */
	{
	    Cdp2BitWriter w = encode_cdp1_bitlength(raw, 8);
	    jt::ByteBuffer data = sample_file(8, 1, true);
	    const uint64_t offset = data.size();
	    data.push_back(1);                                           /* CODEC 1: Bitlength */
	    append_u32(data, static_cast<uint32_t>(w.bit_count()), true);
	    append_u32(data, static_cast<uint32_t>(raw.size()), true);   /* value count */
	    jt::ByteBuffer text = w.code_text();
	    append_u32(data, static_cast<uint32_t>(text.size() / 4), true);
	    data.insert(data.end(), text.begin(), text.end());
	    jt::File file;
	    std::string error;
	    if (!expect(file.load(data, error), error.c_str())) return false;
	    std::vector<int32_t> values;
	    size_t bytes_read = 0;
	    if (!expect(file.int32_packet(offset, predictor, values, bytes_read, error),
		"matrix CDP1 Bitlength decode") || !expect(values == expected, "matrix CDP1 Bitlength predictor"))
		return false;
	}
	/* CDP2 Bitlength2 (JT 10 nibbler fixed-width): min/max via single nibbles,
	 * field_width from the span, codes = value - minimum. */
	{
	    const int32_t minimum = -4, maximum = 7;   /* raw's actual range */
	    const uint32_t span = static_cast<uint32_t>(maximum) - static_cast<uint32_t>(minimum);
	    unsigned field_width = 0;
	    for (uint32_t bound = span; bound != 0; bound >>= 1) ++field_width;
	    Cdp2BitWriter w;
	    w.put(0, 1);                                  /* variable flag = 0 (fixed) */
	    w.put(static_cast<uint32_t>(minimum) & 0xfu, 4); w.put(0, 1);   /* minimum, one nibble */
	    w.put(static_cast<uint32_t>(maximum) & 0xfu, 4); w.put(0, 1);   /* maximum, one nibble */
	    for (int32_t v : raw)
		w.put(static_cast<uint32_t>(v - minimum), field_width);
	    jt::ByteBuffer data = sample_file(10, 3, true);
	    const uint64_t offset = data.size();
	    append_cdp2_bitlength2(data, raw.size(), w);
	    jt::File file;
	    std::string error;
	    if (!expect(file.load(data, error), error.c_str())) return false;
	    std::vector<int32_t> values;
	    size_t bytes_read = 0;
	    if (!expect(file.int32_packet2(offset, predictor, values, bytes_read, error),
		"matrix CDP2 Bitlength2 decode") || !expect(values == expected, "matrix CDP2 Bitlength2 predictor"))
		return false;
	}
    }
    return true;
}

/* TST-043: TOC/segment offset field width.  JT < 10 stores the header's TOC
 * offset and each TOC entry offset as a u32; JT >= 10 widens both to u64.  Both
 * variants must load and report the same TOC, and a JT 10 file with an offset
 * beyond the 32-bit range in its low word must still resolve correctly. */
bool test_toc_offset_width()
{
    /* Both a JT 9.5 (u32 offsets) and a JT 10.3 (u64 offsets) sample_file() must
     * load and expose exactly one shape TOC entry. */
    for (unsigned major : {9u, 10u}) {
	jt::ByteBuffer data = sample_file(major, major >= 10 ? 3 : 5, true);
	jt::File file;
	std::string error;
	if (!expect(file.load(data, error), error.c_str())) return false;
	if (!expect(file.header().major_version == major, "wrong major version")) return false;
	if (!expect(file.header().version().has_wide_offsets() == (major >= 10),
	    "wrong offset width flag")) return false;
	if (!expect(file.toc().size() == 1 && file.toc()[0].type() == 7, "wrong TOC after offset read"))
	    return false;
	/* The parsed TOC offset must fall inside the file for either width. */
	if (!expect(file.header().toc_offset < data.size(), "TOC offset outside file")) return false;
    }

    /* A JT 10 file whose header TOC offset carries a high byte set in the upper
     * 32 bits would be unreadable if the field were read as u32; here we confirm
     * the JT 9 (narrow) reader rejects an offset that only a wide read could
     * represent.  Corrupt the JT 10 header offset's high word and verify the
     * offset is honoured as 64-bit (load fails because it now points past EOF,
     * proving the upper bytes were read rather than ignored). */
    {
	jt::ByteBuffer data = sample_file(10, 3, true);
	/* Header layout: 80 version + 1 byte order + 4 reserved = offset 85 for the
	 * u64 TOC offset.  Set its high word so a u32-only read would miss it. */
	const size_t toc_offset_field = jt::VERSION_LENGTH + 1 + 4;
	data[toc_offset_field + 4] = 0x01;   /* bit 32 of the 64-bit offset */
	jt::File file;
	std::string error;
	if (!expect(!file.load(data, error),
	    "JT10 wide TOC offset high word was ignored")) return false;
    }
    return true;
}

/* EXP-047: a JT file whose version signature carries the "*INCOMPLETE*" partial
 * marker the writer stamps on a short write must be rejected with a clear
 * "incomplete JT file" error rather than a generic truncation message. */
bool test_jt_incomplete_marker()
{
    jt::ByteBuffer data = sample_tristrip();
    const std::string marker = jt::JT_INCOMPLETE_MARKER;
    std::copy(marker.begin(), marker.end(), data.begin() + jt::JT_INCOMPLETE_MARKER_OFFSET);
    jt::File file;
    std::string error;
    if (!expect(!file.load(data, error), "incomplete-marked JT file was accepted")) return false;
    if (!expect(error == "incomplete JT file", error.c_str())) return false;
    return true;
}

/* J8-040: elements() defaults to lenient termination (a segment may simply run
 * out of bytes when the writer omits the explicit all-0xFF end-of-elements
 * marker).  Strict mode requires that marker per Section 9.3.3.  sample_tristrip
 * has no explicit end marker, so it parses cleanly by default but is rejected in
 * strict mode; appending the marker makes strict mode accept it again. */
bool test_jt_strict_end_of_segment()
{
    jt::ByteBuffer data = sample_tristrip();
    jt::File file;
    std::string error;
    if (!expect(file.load(data, error), error.c_str())) return false;
    jt::Segment seg;
    if (!expect(file.segment(file.toc()[0], seg, error), error.c_str())) return false;

    std::vector<jt::Element> els;
    if (!expect(file.elements(seg, els, error), "lenient elements() rejected a valid segment")) return false;
    if (!expect(els.size() == 1, "lenient elements() found the wrong element count")) return false;

    file.set_strict_end_of_segment(true);
    if (!expect(!file.elements(seg, els, error), "strict elements() accepted a missing end marker")) return false;

    /* Append the explicit end-of-elements marker: [u32 length == 16][16 0xFF],
     * growing the two segment-length fields to cover it. */
    append_u32(data, 16, true);
    data.insert(data.end(), 16, 0xff);
    const uint32_t new_segment_length = static_cast<uint32_t>(data.size() - 137);
    write_u32(data, 129, new_segment_length);   /* TOC entry length field */
    write_u32(data, 157, new_segment_length);   /* segment header length field */

    jt::File strict;
    strict.set_strict_end_of_segment(true);
    if (!expect(strict.load(data, error), error.c_str())) return false;
    if (!expect(strict.segment(strict.toc()[0], seg, error), error.c_str())) return false;
    if (!expect(strict.elements(seg, els, error), "strict elements() rejected an explicit end marker")) return false;
    if (!expect(els.size() == 1, "strict elements() found the wrong element count")) return false;
    return true;
}

/* EXP-042: the writer appends a checksum footer element (a 4-byte CRC-32 body
 * behind an all-0xEE "checksum" element GUID) after the mesh element.  A reader
 * that does not recognise the GUID must skip it and still return the correct mesh;
 * a checking reader can re-derive the CRC-32 of the element payload and compare it
 * against the stored value.  This test appends such a footer to a valid tri-strip
 * segment and verifies both the skip behaviour and the CRC round-trip. */
bool test_jt_checksum_footer()
{
    jt::ByteBuffer data = sample_tristrip();
    jt::File probe;
    std::string error;
    if (!expect(probe.load(data, error), error.c_str())) return false;
    jt::Segment seg;
    if (!expect(probe.segment(probe.toc()[0], seg, error), error.c_str())) return false;
    std::vector<jt::Element> els;
    if (!expect(probe.elements(seg, els, error), error.c_str())) return false;

    /* CRC-32 of the mesh element's payload (its data body). */
    const size_t payload_off = static_cast<size_t>(els[0].data_offset);
    const size_t payload_len = static_cast<size_t>(els[0].data_length);
    const uint32_t crc = reference_crc32(data.data() + payload_off, payload_len);

    /* Append the checksum footer element after the mesh element: [u32 length][16
     * GUID 0xEE][u8 base_type][u32 CRC].  Grow the two segment-length fields (at
     * offsets 129 and 157 for the JT 8 sample layout) to cover it. */
    const size_t element_offset = 161;
    append_u32(data, 16 + 1 + 4, true);
    data.insert(data.end(), 16, 0xee);
    data.push_back(4);
    append_u32(data, crc, true);
    const uint32_t new_segment_length = static_cast<uint32_t>(data.size() - 137);
    write_u32(data, 129, new_segment_length);   /* TOC entry length field */
    write_u32(data, 157, new_segment_length);   /* segment header length field */

    jt::File file;
    if (!expect(file.load(data, error), error.c_str())) return false;
    if (!expect(file.segment(file.toc()[0], seg, error), error.c_str())) return false;
    std::vector<jt::Element> els2;
    if (!expect(file.elements(seg, els2, error), error.c_str())) return false;
    /* Two elements now: the mesh and the skipped checksum footer. */
    if (!expect(els2.size() == 2, "checksum footer element was not enumerated")) return false;
    /* The reader still decodes the mesh from the first element unchanged. */
    jt::Mesh mesh;
    if (!expect(file.legacy_mesh(els2[0], mesh, error), error.c_str())) return false;
    if (!expect(mesh.faces == std::vector<int>({0, 1, 2}), "mesh corrupted by checksum footer")) return false;
    /* Re-derive the CRC from the reloaded payload and confirm it matches the stored
     * footer value (the last 4 bytes of the footer element's body). */
    const uint32_t stored_crc = static_cast<uint32_t>(data[data.size() - 4]) |
	(static_cast<uint32_t>(data[data.size() - 3]) << 8) |
	(static_cast<uint32_t>(data[data.size() - 2]) << 16) |
	(static_cast<uint32_t>(data[data.size() - 1]) << 24);
    const uint32_t recomputed = reference_crc32(data.data() + static_cast<size_t>(els2[0].data_offset),
	static_cast<size_t>(els2[0].data_length));
    if (!expect(stored_crc == recomputed, "checksum footer CRC does not round-trip")) return false;
    return true;
}

int main(int argc, char **argv)
{
    (void)argc;
    bu_setprogname(argv[0]);

    if (!check_sample(8, 1, false) || !check_sample(9, 5, true) || !check_sample(10, 0, true)) return 1;

    if (!test_cdp1_null_codec() || !test_cdp1_bitlength_boundaries() || !test_cdp2_null_codec() ||
	!test_cdp2_bitlength2_jt9_fixed() || !test_cdp2_bitlength2_jt9_variable() ||
	!test_cdp2_bitlength2_jt10_nibbler_fixed() || !test_cdp2_bitlength2_jt10_variable() ||
	!test_big_endian_parsing() || !test_jt10_topological_mesh_header() ||
	!test_jt9_topological_mesh_header() || !test_jt8_primitive_boundaries() ||
	!test_jt8_polygon_vs_tristrip_winding() || !test_jt8_lossless_vertex_data() ||
	!test_coordinate_quantization_edges() || !test_toc_bounds_violations() ||
	!test_truncated_element_header() || !test_reader_boundary_violations() ||
	!test_predictor_codec_matrix() || !test_toc_offset_width() ||
	!test_jt_incomplete_marker() || !test_jt_checksum_footer() ||
	!test_jt_strict_end_of_segment()) return 1;

    std::string error;
    jt::File file;
    jt::ByteBuffer data = sample_file(9, 5, true);
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
    append_u32(data, 2, true);   /* outer value count */
    data.push_back(4);           /* CODEC 4: Chopper */
    data.push_back(1);           /* Chop Bits = 1 (non-zero) */
    append_u32(data, 0, true);   /* Value Bias (present when Chop Bits != 0) */
    data.push_back(2);           /* Value Span Bits = 2 -> shift = span - chop = 1 */
    append_u32(data, 2, true);   /* MSB child: value count */
    data.push_back(0);           /* MSB child: Null CODEC */
    append_u32(data, 0, true);
    append_u32(data, 1, true);
    append_u32(data, 2, true);   /* LSB child: value count */
    data.push_back(0);           /* LSB child: Null CODEC */
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

    /* CDP1 (Load1) golden packets captured from real JT 8 files: a Huffman
     * (CODEC 2) coordinate stream from conrod.jt and an Arithmetic (CODEC 3)
     * stream from fishing_reel_rotor.jt.  Both exercise the JT 8 two-table
     * probability context, the out-of-band packet, and the decode loop. */
    static const unsigned char cdp1_huffman_packet[] = {
	0x02, 0x01, 0x00, 0x00, 0x00, 0x02, 0x08, 0xa0, 0x00, 0x00, 0x00, 0x00,
	0x00, 0xa9, 0xa0, 0x45, 0x45, 0x00, 0x00, 0x00, 0x01, 0x05, 0x01, 0x00,
	0x00, 0x45, 0x00, 0x00, 0x00, 0x09, 0x00, 0x00, 0x00, 0x5d, 0xe5, 0x25,
	0xc9, 0x75, 0x84, 0x55, 0x5e, 0x6c, 0xc9, 0x96, 0x6c, 0x59, 0x67, 0xc9,
	0x96, 0xed, 0x8e, 0x75, 0x6e, 0x49, 0xae, 0x58, 0xe5, 0x2d, 0xd9, 0x92,
	0x2d, 0x6a, 0x3b, 0xd9, 0x92, 0x00, 0x00, 0x00, 0xc8, 0xdf, 0x02, 0x00,
	0x00, 0xdf, 0x02, 0x00, 0x00, 0x17, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0xf0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x41, 0x04,
	0x00, 0x00, 0x44, 0x10, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x04, 0x00, 0x11, 0x00, 0x51, 0x04, 0x00, 0x51, 0x14, 0x40, 0x00,
	0x00, 0x40, 0x00, 0x00, 0x01, 0x00, 0x03, 0x18, 0x14, 0x10, 0x00, 0x03,
	0x18, 0xe0, 0x04, 0x90, 0x81, 0x10, 0x00, 0x10, 0x84, 0x00, 0x40, 0x00,
	0x05, 0x40, 0x00, 0x05, 0x10, 0x04, 0x00, 0x51, 0x04, 0x20, 0x00, 0x10,
	0x51, 0x00, 0x08, 0x00, 0x04,
    };
    static const unsigned char cdp1_arithmetic_packet[] = {
	0x03, 0x01, 0x00, 0x00, 0x00, 0x02, 0x08, 0x80, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x29, 0x38, 0x80, 0xa4, 0x00, 0x00, 0x00, 0x01, 0x8f, 0x03, 0x00,
	0x00, 0xa4, 0x00, 0x00, 0x00, 0x1d, 0x00, 0x00, 0x00, 0x49, 0x80, 0x38,
	0xf3, 0x2c, 0xe2, 0x35, 0x24, 0x29, 0x8b, 0xf2, 0xdc, 0x42, 0x50, 0xdb,
	0x08, 0x9a, 0x68, 0x52, 0xda, 0x13, 0x4e, 0x38, 0x11, 0xc2, 0x89, 0xc7,
	0x8a, 0x89, 0x55, 0x9c, 0x70, 0xee, 0x84, 0x13, 0xa1, 0xbc, 0x4d, 0x54,
	0x5e, 0xe1, 0xde, 0x68, 0x41, 0xd9, 0x1d, 0x4e, 0x38, 0xc2, 0x09, 0x27,
	0xdc, 0x67, 0x49, 0x79, 0x68, 0x2d, 0x08, 0x26, 0x38, 0x89, 0xd0, 0x9e,
	0x10, 0x16, 0x44, 0x13, 0xa1, 0x42, 0x13, 0x81, 0x91, 0x23, 0x6c, 0x84,
	0x26, 0x98, 0x8d, 0xc0, 0x46, 0x37, 0xc4, 0xcd, 0xbb, 0xcf, 0xbd, 0x3b,
	0xbc, 0xc8, 0xc9, 0x6a, 0x13, 0xdb, 0x94, 0xb7, 0xca, 0x7c, 0xb9, 0xe2,
	0x5b, 0x4d, 0x84, 0xb6, 0x45, 0x6d, 0x8b, 0x13, 0x19, 0x46, 0x13, 0xa1,
	0x49, 0x00, 0x00, 0xb6, 0xda, 0x40, 0x01, 0x00, 0x00, 0x2c, 0x01, 0x00,
	0x00, 0x0a, 0x00, 0x00, 0x00, 0xd7, 0x14, 0x98, 0x9e, 0xd5, 0x74, 0x40,
	0x72, 0xea, 0x6e, 0x0a, 0x84, 0xf8, 0x02, 0x00, 0x80, 0xff, 0xff, 0xff,
	0x9f, 0xad, 0xff, 0xff, 0xff, 0xfe, 0xff, 0x2f, 0x31, 0xf8, 0xff, 0x03,
	0x5c, 0xc7, 0xaa, 0xff, 0x54, 0x00, 0x00, 0x20, 0x33,
    };
    struct Cdp1Case {
	const unsigned char *packet;
	size_t size;
	size_t count;
	int64_t weighted_sum;
	const char *name;
    };
    const Cdp1Case cdp1_cases[] = {
	{cdp1_huffman_packet, sizeof(cdp1_huffman_packet), 735, 358, "CDP1 Huffman"},
	{cdp1_arithmetic_packet, sizeof(cdp1_arithmetic_packet), 300, -729, "CDP1 Arithmetic"},
    };
    for (const Cdp1Case &c : cdp1_cases) {
	data = sample_file(8, 1, true);
	const uint64_t cdp1_offset = data.size();
	data.insert(data.end(), c.packet, c.packet + c.size);
	if (!expect(file.load(data, error), error.c_str())) return 1;
	if (!expect(file.int32_packet(cdp1_offset, jt::Predictor::None, values, bytes_read, error),
	    error.c_str())) return 1;
	int64_t sum = 0;
	for (size_t i = 0; i < values.size(); ++i) sum += static_cast<int64_t>(i + 1) * values[i];
	if (!expect(values.size() == c.count && bytes_read == c.size && sum == c.weighted_sum, c.name)) return 1;
    }
    return 0;
}
