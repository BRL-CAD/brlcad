/*                       J T _ P A R S E R . C P P
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "common.h"

#include "jt_parser.h"
#include "jt_topology.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <functional>
#include <limits>
#include <regex>

#include <zlib.h>

namespace jt {
namespace {

constexpr size_t VERSION_LENGTH = 80;
constexpr size_t GUID_LENGTH = 16;
constexpr size_t MINIMUM_HEADER_LENGTH = VERSION_LENGTH + 1 + 4 + 4 + GUID_LENGTH;
constexpr size_t LEGACY_TOC_ENTRY_LENGTH = GUID_LENGTH + 4 + 4 + 4;
constexpr size_t JT10_TOC_ENTRY_LENGTH = GUID_LENGTH + 8 + 4 + 4;
constexpr size_t SEGMENT_HEADER_LENGTH = GUID_LENGTH + 4 + 4;
constexpr size_t LEGACY_ELEMENT_HEADER_LENGTH = 4 + GUID_LENGTH + 1;
constexpr size_t OBJECT_ID_LENGTH = 4;
constexpr size_t MAX_TOC_ENTRIES = 1000000;
constexpr size_t MAX_PACKET_VALUES = 100000000;
constexpr size_t MAX_CONTEXT_ENTRIES = 8192;

class Reader {
  public:
    Reader(const std::vector<unsigned char> &bytes, bool little_endian, size_t offset = 0) :
	bytes_(bytes), little_endian_(little_endian), offset_(offset) {}

    bool can_read(size_t count) const { return count <= bytes_.size() - std::min(offset_, bytes_.size()); }
    size_t offset() const { return offset_; }

    bool bytes(unsigned char *out, size_t count)
    {
	if (!can_read(count)) return false;
	std::copy_n(bytes_.data() + offset_, count, out);
	offset_ += count;
	return true;
    }

    bool u8(uint8_t &value)
    {
	if (!can_read(1)) return false;
	value = bytes_[offset_++];
	return true;
    }

    bool u32(uint32_t &value)
    {
	unsigned char data[4];
	if (!bytes(data, sizeof(data))) return false;
	value = 0;
	for (size_t i = 0; i < sizeof(data); ++i) {
	    const size_t shift_index = little_endian_ ? i : sizeof(data) - i - 1;
	    value |= static_cast<uint32_t>(data[i]) << (shift_index * 8);
	}
	return true;
    }

    bool i32(int32_t &value)
    {
	uint32_t bits = 0;
	if (!u32(bits)) return false;
	value = static_cast<int32_t>(bits);
	return true;
    }

    bool u16(uint16_t &value)
    {
	unsigned char data[2];
	if (!bytes(data, sizeof(data))) return false;
	value = little_endian_ ? static_cast<uint16_t>(data[0] | (data[1] << 8)) :
	    static_cast<uint16_t>((data[0] << 8) | data[1]);
	return true;
    }

    bool f32(float &value)
    {
	uint32_t bits = 0;
	if (!u32(bits)) return false;
	static_assert(sizeof(value) == sizeof(bits), "JT F32 requires a 32-bit float");
	std::memcpy(&value, &bits, sizeof(value));
	return true;
    }

    bool u64(uint64_t &value)
    {
	unsigned char data[8];
	if (!bytes(data, sizeof(data))) return false;
	value = 0;
	for (size_t i = 0; i < sizeof(data); ++i) {
	    const size_t shift_index = little_endian_ ? i : sizeof(data) - i - 1;
	    value |= static_cast<uint64_t>(data[i]) << (shift_index * 8);
	}
	return true;
    }

  private:
    const std::vector<unsigned char> &bytes_;
    bool little_endian_;
    size_t offset_;
};

bool range_valid(uint64_t offset, uint64_t length, size_t size)
{
    return offset <= size && length <= size - static_cast<size_t>(offset);
}

bool read_guid(Reader &reader, Guid &guid)
{
    return reader.bytes(guid.data(), guid.size());
}

int32_t predicted_value(const std::vector<int32_t> &values, size_t index, Predictor predictor)
{
    const int32_t v1 = values[index - 1];
    const int32_t v2 = values[index - 2];
    const int32_t v4 = values[index - 4];
    const auto add = [](int32_t a, int32_t b) {
	return static_cast<int32_t>(static_cast<uint32_t>(a) + static_cast<uint32_t>(b));
    };
    const auto subtract = [](int32_t a, int32_t b) {
	return static_cast<int32_t>(static_cast<uint32_t>(a) - static_cast<uint32_t>(b));
    };
    switch (predictor) {
	case Predictor::Lag1: case Predictor::Xor1: return v1;
	case Predictor::Lag2: case Predictor::Xor2: return v2;
	case Predictor::Stride1: return add(v1, subtract(v1, v2));
	case Predictor::Stride2: return add(v2, subtract(v2, v4));
	case Predictor::StripIndex:
	{
	    const int32_t delta = subtract(v2, v4);
	    return (delta < 8 && delta > -8) ? add(v2, delta) : add(v2, 2);
	}
	case Predictor::Ramp: return static_cast<int32_t>(index);
	case Predictor::None: return 0;
    }
    return 0;
}

void unpack_residuals(std::vector<int32_t> &values, Predictor predictor)
{
    if (predictor == Predictor::None) return;
    for (size_t i = 4; i < values.size(); ++i) {
	const int32_t predicted = predicted_value(values, i, predictor);
	if (predictor == Predictor::Xor1 || predictor == Predictor::Xor2)
	    values[i] ^= predicted;
	else
	    values[i] = static_cast<int32_t>(static_cast<uint32_t>(values[i]) + static_cast<uint32_t>(predicted));
    }
}

bool guid_matches(const Guid &guid, bool little_endian, uint32_t first, uint16_t second, uint16_t third,
    const std::array<unsigned char, 8> &tail)
{
    std::vector<unsigned char> expected;
    expected.reserve(16);
    for (size_t i = 0; i < 4; ++i) expected.push_back(static_cast<unsigned char>(first >> ((little_endian ? i : 3 - i) * 8)));
    for (size_t i = 0; i < 2; ++i) expected.push_back(static_cast<unsigned char>(second >> ((little_endian ? i : 1 - i) * 8)));
    for (size_t i = 0; i < 2; ++i) expected.push_back(static_cast<unsigned char>(third >> ((little_endian ? i : 1 - i) * 8)));
    expected.insert(expected.end(), tail.begin(), tail.end());
    return std::equal(expected.begin(), expected.end(), guid.begin());
}

bool decode_bitlength(const std::vector<uint32_t> &words, size_t bit_count, size_t value_count,
    std::vector<int32_t> &values, std::string &error)
{
    size_t bit_offset = 0;
    int field_width = 0;
    const auto read_bit = [&words, bit_count, &bit_offset](uint32_t &bit) {
	if (bit_offset >= bit_count) return false;
	bit = (words[bit_offset / 32] >> (31 - bit_offset % 32)) & 1u;
	++bit_offset;
	return true;
    };

    values.reserve(value_count);
    while (values.size() < value_count) {
	uint32_t prefix = 0;
	if (!read_bit(prefix)) {
	    error = "JT Bitlength CODEC ended before producing all values";
	    return false;
	}
	if (prefix != 0) {
	    uint32_t direction = 0;
	    if (!read_bit(direction)) {
		error = "truncated JT Bitlength CODEC width prefix";
		return false;
	    }
	    do {
		field_width += direction ? 2 : -2;
		if (field_width < 0 || field_width > 32) {
		    error = "invalid JT Bitlength CODEC field width";
		    return false;
		}
		uint32_t next = 0;
		if (!read_bit(next)) {
		    error = "truncated JT Bitlength CODEC width prefix";
		    return false;
		}
		if (next != direction) break;
	    } while (true);
	}

	uint32_t encoded = 0;
	for (int i = 0; i < field_width; ++i) {
	    uint32_t bit = 0;
	    if (!read_bit(bit)) {
		error = "truncated JT Bitlength CODEC value";
		return false;
	    }
	    encoded = (encoded << 1) | bit;
	}
	if (field_width > 0 && field_width < 32 && (encoded & (1u << (field_width - 1))))
	    encoded |= ~((1u << field_width) - 1u);
	values.push_back(static_cast<int32_t>(encoded));
    }
    return true;
}

bool decode_bitlength2(const std::vector<uint32_t> &words, size_t bit_count, size_t value_count,
    std::vector<int32_t> &values, std::string &error)
{
    size_t bit_offset = 0;
    const auto read = [&words, bit_count, &bit_offset](unsigned width, uint32_t &value) {
	if (bit_offset + width > bit_count) return false;
	value = 0;
	for (unsigned i = 0; i < width; ++i, ++bit_offset)
	    value = (value << 1) | ((words[bit_offset / 32] >> (31 - bit_offset % 32)) & 1u);
	return true;
    };
    const auto nibbler = [&read](int32_t &value) {
	uint32_t encoded = 0;
	unsigned nibbles = 0;
	uint32_t more = 0;
	do {
	    uint32_t nibble = 0;
	    if (nibbles == 8 || !read(4, nibble) || !read(1, more)) return false;
	    encoded |= nibble << (nibbles++ * 4);
	} while (more != 0);
	const unsigned width = nibbles * 4;
	if (width < 32 && (encoded & (1u << (width - 1)))) encoded |= ~((1u << width) - 1u);
	value = static_cast<int32_t>(encoded);
	return true;
    };
    uint32_t variable = 0;
    if (!read(1, variable)) return false;
    values.reserve(value_count);
    if (variable == 0) {
	int32_t minimum = 0;
	int32_t maximum = 0;
	if (!nibbler(minimum) || !nibbler(maximum) || maximum < minimum) {
	    error = "invalid JT 10 fixed-width Bitlength CODEC range";
	    return false;
	}
	const uint32_t span = static_cast<uint32_t>(maximum) - static_cast<uint32_t>(minimum);
	unsigned field_width = 0;
	for (uint32_t bound = span; bound != 0; bound >>= 1) ++field_width;
	for (size_t i = 0; i < value_count; ++i) {
	    uint32_t encoded = 0;
	    if (!read(field_width, encoded) || encoded > span) {
		error = "truncated JT 10 fixed-width Bitlength CODEC values";
		return false;
	    }
	    values.push_back(static_cast<int32_t>(static_cast<uint32_t>(minimum) + encoded));
	}
    } else {
	int32_t mean = 0;
	if (!nibbler(mean)) return false;
	int field_width = 0;
	while (values.size() < value_count) {
	    int32_t delta = 0;
	    do {
		uint32_t encoded_delta = 0;
		if (!read(4, encoded_delta)) return false;
		delta = static_cast<int32_t>(encoded_delta << 28) >> 28;
		field_width += delta;
	    } while (delta == -8 || delta == 7);
	    uint32_t run_length = 0;
	    if (field_width < 0 || field_width > 32 || !read(4, run_length) || run_length == 0 ||
		values.size() + run_length > value_count) {
		error = "invalid JT 10 variable-width Bitlength CODEC block (width " +
		    std::to_string(field_width) + ", run " + std::to_string(run_length) + ")";
		return false;
	    }
	    for (uint32_t i = 0; i < run_length; ++i) {
		uint32_t encoded = 0;
		if (!read(static_cast<unsigned>(field_width), encoded)) return false;
		if (field_width > 0 && field_width < 32 && (encoded & (1u << (field_width - 1))))
		    encoded |= ~((1u << field_width) - 1u);
		values.push_back(static_cast<int32_t>(encoded + static_cast<uint32_t>(mean)));
	    }
	}
    }
    return true;
}

} // namespace

bool
File::load(const char *path, std::string &error)
{
    std::ifstream input(path, std::ios::binary | std::ios::ate);
    if (!input) {
	error = "cannot open input file";
	return false;
    }
    const std::streamoff length = input.tellg();
    if (length < 0 || static_cast<uintmax_t>(length) > std::numeric_limits<size_t>::max()) {
	error = "input file size is invalid";
	return false;
    }
    std::vector<unsigned char> bytes(static_cast<size_t>(length));
    input.seekg(0);
    if (!bytes.empty() && !input.read(reinterpret_cast<char *>(bytes.data()), length)) {
	error = "cannot read input file";
	return false;
    }
    return load(bytes, error);
}

bool
File::load(const std::vector<unsigned char> &bytes, std::string &error)
{
    header_ = Header{};
    toc_.clear();
    bytes_.clear();
    if (bytes.size() < MINIMUM_HEADER_LENGTH) {
	error = "file is shorter than a JT header";
	return false;
    }

    header_.version_text.assign(reinterpret_cast<const char *>(bytes.data()), VERSION_LENGTH);
    const std::regex version_pattern("^Version[[:space:]]+([0-9]+)\\.([0-9]+)");
    std::smatch match;
    if (!std::regex_search(header_.version_text, match, version_pattern)) {
	error = "invalid JT version signature";
	return false;
    }
    try {
	header_.major_version = static_cast<unsigned>(std::stoul(match[1].str()));
	header_.minor_version = static_cast<unsigned>(std::stoul(match[2].str()));
    } catch (const std::exception &) {
	error = "JT version number is out of range";
	return false;
    }

    const uint8_t byte_order = bytes[VERSION_LENGTH];
    if (byte_order > 1) {
	error = "invalid JT byte order";
	return false;
    }
    header_.little_endian = byte_order == 0;
    Reader reader(bytes, header_.little_endian, VERSION_LENGTH + 1);
    uint32_t reserved = 0;
    uint32_t legacy_toc_offset = 0;
    if (!reader.u32(reserved)) {
	error = "truncated JT header";
	return false;
    }
    const bool toc_offset_read = header_.major_version >= 10 ? reader.u64(header_.toc_offset) : reader.u32(legacy_toc_offset);
    if (!toc_offset_read || !read_guid(reader, header_.lsg_segment_id)) {
	error = "truncated JT header";
	return false;
    }
    if (header_.major_version < 10) header_.toc_offset = legacy_toc_offset;
    if (reserved != 0) {
	Guid reserved_guid;
	if (!read_guid(reader, reserved_guid)) {
	    error = "truncated JT extended header";
	    return false;
	}
    }
    if (!range_valid(header_.toc_offset, 4, bytes.size())) {
	error = "JT TOC offset is outside the file";
	return false;
    }

    Reader toc_reader(bytes, header_.little_endian, static_cast<size_t>(header_.toc_offset));
    uint32_t count = 0;
    const size_t toc_entry_length = header_.major_version >= 10 ? JT10_TOC_ENTRY_LENGTH : LEGACY_TOC_ENTRY_LENGTH;
    if (!toc_reader.u32(count) || count > MAX_TOC_ENTRIES ||
	count > (bytes.size() - toc_reader.offset()) / toc_entry_length) {
	error = "invalid or truncated JT TOC";
	return false;
    }
    toc_.reserve(count);
    for (uint32_t i = 0; i < count; ++i) {
	TocEntry entry{};
	uint32_t legacy_offset = 0;
	if (!read_guid(toc_reader, entry.id) ||
	    (header_.major_version >= 10 ? !toc_reader.u64(entry.offset) : !toc_reader.u32(legacy_offset)) ||
	    !toc_reader.u32(entry.length) || !toc_reader.u32(entry.attributes)) {
	    error = "truncated JT TOC entry";
	    return false;
	}
	if (header_.major_version < 10) entry.offset = legacy_offset;
	if (entry.length < SEGMENT_HEADER_LENGTH || !range_valid(entry.offset, entry.length, bytes.size())) {
	    error = "JT TOC entry references an invalid segment range";
	    return false;
	}
	toc_.push_back(entry);
    }
    bytes_ = bytes;
    return true;
}

bool
File::segment(const TocEntry &entry, Segment &result, std::string &error) const
{
    if (!range_valid(entry.offset, entry.length, bytes_.size())) {
	error = "segment range is outside the file";
	return false;
    }
    Reader reader(bytes_, header_.little_endian, static_cast<size_t>(entry.offset));
    result = Segment{};
    result.offset = entry.offset;
    if (!read_guid(reader, result.id) || !reader.u32(result.type) || !reader.u32(result.length)) {
	error = "truncated JT segment header";
	return false;
    }
    if (result.id != entry.id || result.type != entry.type() || result.length != entry.length) {
	error = "JT segment header does not match its TOC entry";
	return false;
    }
    return true;
}

bool
File::elements(const Segment &segment, std::vector<Element> &result, std::string &error) const
{
    result.clear();
    if (segment.type < 6 || segment.type > 16) {
	error = "element enumeration currently supports only shape segments";
	return false;
    }
    if (segment.length < SEGMENT_HEADER_LENGTH || !range_valid(segment.offset, segment.length, bytes_.size())) {
	error = "segment range is outside the file";
	return false;
    }
    size_t offset = static_cast<size_t>(segment.offset) + SEGMENT_HEADER_LENGTH;
    const size_t end = static_cast<size_t>(segment.offset) + segment.length;
    const bool has_object_id = header_.major_version >= 9;
    const size_t element_header_length = LEGACY_ELEMENT_HEADER_LENGTH + (has_object_id ? OBJECT_ID_LENGTH : 0);
    while (offset < end) {
	if (end - offset < 4 + GUID_LENGTH) {
	    error = "truncated JT logical element header";
	    return false;
	}
	Reader reader(bytes_, header_.little_endian, offset);
	uint32_t element_length = 0;
	Element element{};
	uint32_t object_id = 0;
	if (!reader.u32(element_length) || !read_guid(reader, element.type_id)) {
	    error = "truncated JT logical element header";
	    return false;
	}
	const bool end_marker = element_length == GUID_LENGTH &&
	    std::all_of(element.type_id.begin(), element.type_id.end(), [](unsigned char byte) { return byte == 0xff; });
	if (end_marker) return true;
	if (end - offset < element_header_length || !reader.u8(element.base_type) ||
	    (has_object_id && !reader.u32(object_id))) {
	    error = "truncated JT logical element header";
	    return false;
	}
	const size_t element_data_header_length = element_header_length - 4;
	if (element_length < element_data_header_length || element_length > end - offset - 4) {
	    error = "invalid JT element length";
	    return false;
	}
	element.object_id = has_object_id ? static_cast<int32_t>(object_id) : -1;
	element.data_offset = reader.offset();
	element.data_length = element_length - element_data_header_length;
	result.push_back(element);
	offset += 4 + element_length;
    }
    return true;
}

bool
File::int32_packet(uint64_t offset, Predictor predictor, std::vector<int32_t> &values,
    size_t &bytes_read, std::string &error) const
{
    values.clear();
    bytes_read = 0;
    if (offset > bytes_.size()) {
	error = "JT Int32 compressed data packet offset is outside the file";
	return false;
    }
    Reader reader(bytes_, header_.little_endian, static_cast<size_t>(offset));
    uint8_t codec = 0;
    if (!reader.u8(codec)) {
	error = "truncated JT Int32 compressed data packet";
	return false;
    }
    if (codec > 1) {
	error = "unsupported JT Int32 packet CODEC type " + std::to_string(codec);
	return false;
    }

    int32_t count = 0;
    if (codec == 0) {
	if (!reader.i32(count) || count < 0 || static_cast<uint32_t>(count) > MAX_PACKET_VALUES ||
	    static_cast<size_t>(count) > (bytes_.size() - reader.offset()) / sizeof(int32_t)) {
	    error = "invalid or truncated JT Null CODEC packet";
	    return false;
	}
	values.resize(static_cast<size_t>(count));
	for (int32_t &value : values) {
	    if (!reader.i32(value)) {
		error = "truncated JT Null CODEC values";
		return false;
	    }
	}
	unpack_residuals(values, predictor);
	bytes_read = reader.offset() - static_cast<size_t>(offset);
	return true;
    }

    int32_t bit_count = 0;
    int32_t word_count = 0;
    if (!reader.i32(bit_count) || !reader.i32(count) || !reader.i32(word_count) ||
	bit_count < 0 || count < 0 || word_count < 0 || static_cast<uint32_t>(count) > MAX_PACKET_VALUES ||
	static_cast<size_t>(word_count) > (bytes_.size() - reader.offset()) / sizeof(uint32_t) ||
	static_cast<uint64_t>(bit_count) > static_cast<uint64_t>(word_count) * 32) {
	error = "invalid or truncated JT Bitlength CODEC packet";
	return false;
    }
    std::vector<uint32_t> words(static_cast<size_t>(word_count));
    for (uint32_t &word : words) {
	if (!reader.u32(word)) {
	    error = "truncated JT Bitlength CODEC code text";
	    return false;
	}
    }
    if (!decode_bitlength(words, static_cast<size_t>(bit_count), static_cast<size_t>(count), values, error)) return false;
    unpack_residuals(values, predictor);
    bytes_read = reader.offset() - static_cast<size_t>(offset);
    return true;
}

bool
File::int32_packet2(uint64_t offset, Predictor predictor, std::vector<int32_t> &values,
    size_t &bytes_read, std::string &error) const
{
    // JT 10 permits nested out-of-band/chopper packets to a depth of eight.
    constexpr unsigned max_chopper_depth = 8;
    std::function<bool(size_t, unsigned, int32_t, std::vector<int32_t> &, size_t &)> decode;
    decode = [&](size_t packet_offset, unsigned depth, int32_t inherited_count,
	std::vector<int32_t> &output, size_t &consumed) {
	output.clear();
	if (depth > max_chopper_depth) {
	    error = "JT Int32 CDP2 recursion limit exceeded";
	    return false;
	}
	if (packet_offset > bytes_.size()) {
	    error = "JT Int32 CDP2 offset is outside the file";
	    return false;
	}
	Reader reader(bytes_, header_.little_endian, packet_offset);
	int32_t count = inherited_count;
	uint8_t codec = 0;
	if ((inherited_count < 0 && !reader.i32(count)) || count < 0 ||
	    static_cast<uint32_t>(count) > MAX_PACKET_VALUES) {
	    error = "invalid or truncated JT Int32 CDP2 header";
	    return false;
	}
	if (count == 0) {
	    output.clear();
	    consumed = reader.offset() - packet_offset;
	    return true;
	}
	if (!reader.u8(codec)) {
	    error = "truncated JT Int32 CDP2 CODEC type";
	    return false;
	}
	if (codec == 0) {
	    if (static_cast<size_t>(count) > (bytes_.size() - reader.offset()) / sizeof(int32_t)) {
		error = "truncated JT Int32 CDP2 Null values";
		return false;
	    }
	    output.resize(static_cast<size_t>(count));
	    for (int32_t &value : output) {
		if (!reader.i32(value)) return false;
	    }
	} else if (codec == 1) {
	    int32_t bit_count = 0;
	    if (!reader.i32(bit_count) || bit_count < 0) {
		error = "invalid JT Int32 CDP2 Bitlength code text size";
		return false;
	    }
	    /* The Int32 CDP2 (Load2) path always uses the revised (word-aligned
	     * Bitlength2) CODEC, in both JT 9.x and 10.x. */
	    const bool revised_bitlength = true;
	    const size_t byte_count = revised_bitlength ?
		((static_cast<size_t>(bit_count) + 31) / 32) * sizeof(uint32_t) :
		(static_cast<size_t>(bit_count) + 7) / 8;
	    if (byte_count > bytes_.size() - reader.offset()) {
		error = "truncated JT Int32 CDP2 Bitlength code text";
		return false;
	    }
	    std::vector<unsigned char> code_text(byte_count);
	    if (!reader.bytes(code_text.data(), code_text.size())) return false;
	    std::vector<uint32_t> words((static_cast<size_t>(bit_count) + 31) / 32, 0);
	    for (size_t i = 0; i < code_text.size(); ++i) {
		const size_t source = revised_bitlength && header_.little_endian ?
		    (i & ~size_t(3)) + (3 - i % 4) : i;
		words[i / 4] |= static_cast<uint32_t>(code_text[source]) << (24 - (i % 4) * 8);
	    }
	    const bool decoded = revised_bitlength ?
		decode_bitlength2(words, static_cast<size_t>(bit_count), static_cast<size_t>(count), output, error) :
		decode_bitlength(words, static_cast<size_t>(bit_count), static_cast<size_t>(count), output, error);
	    if (!decoded) return false;
	} else if (codec == 3) {
	    int32_t bit_count = 0;
	    if (!reader.i32(bit_count) || bit_count < 0) {
		error = "invalid JT Int32 CDP2 Arithmetic code text size";
		return false;
	    }
	    const size_t byte_count = ((static_cast<size_t>(bit_count) + 31) / 32) * sizeof(uint32_t);
	    if (byte_count > bytes_.size() - reader.offset()) {
		error = "truncated JT Int32 CDP2 Arithmetic code text";
		return false;
	    }
	    std::vector<unsigned char> code_text(byte_count);
	    if (!reader.bytes(code_text.data(), code_text.size())) return false;

	    size_t context_bit = reader.offset() * 8;
	    auto read_bits = [&](unsigned width, uint32_t &value) {
		if (width > 32 || context_bit + width > bytes_.size() * 8) return false;
		value = 0;
		for (unsigned i = 0; i < width; ++i, ++context_bit)
		    value = (value << 1) | ((bytes_[context_bit / 8] >> (7 - context_bit % 8)) & 1u);
		return true;
	    };
	    uint32_t entry_count = 0;
	    uint32_t occurrence_bits = 0;
	    uint32_t value_bits = 0;
	    uint32_t minimum_bits = 0;
	    if (!read_bits(16, entry_count) || !read_bits(6, occurrence_bits) ||
		!read_bits(7, value_bits) || !read_bits(32, minimum_bits)) {
		error = "truncated JT Int32 CDP2 probability context";
		return false;
	    }
	    const int32_t minimum = static_cast<int32_t>(minimum_bits);
	    if (entry_count == 0 || entry_count > MAX_CONTEXT_ENTRIES || occurrence_bits == 0 ||
		occurrence_bits > 32 || value_bits > 32) {
		error = "invalid JT Int32 CDP2 probability context";
		return false;
	    }
	    struct ContextEntry { uint32_t low; uint32_t high; int32_t value; bool escape; };
	    std::vector<ContextEntry> entries;
	    entries.reserve(entry_count);
	    uint32_t total = 0;
	    for (uint32_t i = 0; i < entry_count; ++i) {
		uint32_t escape = 0;
		uint32_t occurrence = 0;
		uint32_t associated = 0;
		if (!read_bits(1, escape) || !read_bits(occurrence_bits, occurrence) ||
		    !read_bits(value_bits, associated) || occurrence == 0 || occurrence > 65535 - total) {
		    error = "invalid JT Int32 CDP2 probability entry";
		    return false;
		}
		entries.push_back({total, total + occurrence,
		    static_cast<int32_t>(associated + static_cast<uint32_t>(minimum)), escape != 0});
		total += occurrence;
	    }
	    context_bit = (context_bit + 7) & ~static_cast<size_t>(7);
	    Reader oob_reader(bytes_, header_.little_endian, context_bit / 8);
	    int32_t oob_count = 0;
	    const bool has_escape = std::any_of(entries.begin(), entries.end(),
		[](const ContextEntry &entry) { return entry.escape; });
	    if (has_escape && (!oob_reader.i32(oob_count) || oob_count < 0 || oob_count > count)) {
		error = "invalid JT Int32 CDP2 out-of-band count";
		return false;
	    }
	    std::vector<int32_t> oob_values;
	    size_t oob_size = 0;
	    if (oob_count > 0) {
		if (!decode(oob_reader.offset(), depth + 1, oob_count, oob_values, oob_size)) return false;
	    }
	    if (bit_count == 0) {
		if (oob_values.size() != static_cast<size_t>(count)) {
		    error = "JT Arithmetic CODEC with empty code text does not contain all values out-of-band";
		    return false;
		}
		output = std::move(oob_values);
		consumed = oob_reader.offset() + oob_size - packet_offset;
		return true;
	    }

	    size_t code_bit = 0;
	    bool code_text_exhausted = false;
	    const auto next_code_bit = [&]() -> uint16_t {
		if (code_bit >= static_cast<size_t>(bit_count)) {
		    code_text_exhausted = true;
		    return 0;
		}
		const size_t byte = code_bit / 8;
		const size_t source_byte = header_.little_endian ? (byte & ~size_t(3)) + (3 - byte % 4) : byte;
		const uint16_t bit = (code_text[source_byte] >> (7 - code_bit % 8)) & 1u;
		++code_bit;
		return bit;
	    };
	    uint16_t low = 0;
	    uint16_t high = 0xffff;
	    uint16_t code = 0;
	    for (unsigned i = 0; i < 16; ++i) code = static_cast<uint16_t>((code << 1) | next_code_bit());
	    size_t oob_index = 0;
	    output.reserve(static_cast<size_t>(count));
	    for (int32_t i = 0; i < count; ++i) {
		const uint32_t range = static_cast<uint32_t>(high - low) + 1;
		const uint32_t scaled = ((static_cast<uint32_t>(code - low) + 1) * total - 1) / range;
		auto found = std::find_if(entries.begin(), entries.end(),
		    [scaled](const ContextEntry &entry) { return scaled >= entry.low && scaled < entry.high; });
		if (found == entries.end()) {
		    error = "JT Arithmetic CODEC symbol is outside the probability context";
		    return false;
		}
		if (found->escape) {
		    if (oob_index >= oob_values.size()) {
			error = "JT Arithmetic CODEC exhausted out-of-band values";
			return false;
		    }
		    output.push_back(oob_values[oob_index++]);
		} else {
		    output.push_back(found->value);
		}
		high = static_cast<uint16_t>(low + (range * found->high) / total - 1);
		low = static_cast<uint16_t>(low + (range * found->low) / total);
		for (;;) {
		    if ((high & 0x8000) == (low & 0x8000)) {
		    } else if ((low >> 14) == 1 && (high >> 14) == 2) {
			code ^= 0x4000;
			low &= 0x3fff;
			high |= 0x4000;
		    } else {
			break;
		    }
		    low = static_cast<uint16_t>(low << 1);
		    high = static_cast<uint16_t>((high << 1) | 1);
		    code = static_cast<uint16_t>((code << 1) | next_code_bit());
		}
	    }
	    if (code_text_exhausted) {
		error = "JT Arithmetic CODEC exhausted its code text";
		return false;
	    }
	    if (oob_index != oob_values.size()) {
		error = "JT Arithmetic CODEC did not consume all out-of-band values";
		return false;
	    }
	    consumed = oob_reader.offset() + oob_size - packet_offset;
	    return true;
	} else if (codec == 4) {
	    if (depth >= max_chopper_depth) {
		error = "JT Int32 CDP2 Chopper recursion limit exceeded";
		return false;
	    }
	    uint8_t chop_bits = 0;
	    uint8_t span_bits = 0;
	    int32_t bias = 0;
	    const bool jt10_chopper = header_.major_version >= 10;
	    if (!reader.u8(chop_bits) ||
		(jt10_chopper ? !reader.i32(bias) : (chop_bits == 0 && !reader.i32(bias))) ||
		!reader.u8(span_bits) || span_bits > 32 || chop_bits > span_bits ||
		(jt10_chopper && chop_bits == 0)) {
		error = "invalid JT Int32 CDP2 Chopper parameters";
		return false;
	    }
	    std::vector<int32_t> most_significant;
	    std::vector<int32_t> least_significant;
	    size_t msb_size = 0;
	    size_t lsb_size = 0;
	    if (!decode(reader.offset(), depth + 1, -1, most_significant, msb_size) ||
		!decode(reader.offset() + msb_size, depth + 1, -1, least_significant, lsb_size) ||
		most_significant.size() != static_cast<size_t>(count) ||
		least_significant.size() != static_cast<size_t>(count)) {
		error = "JT Int32 CDP2 Chopper child packets have inconsistent lengths";
		return false;
	    }
	    output.resize(static_cast<size_t>(count));
	    const unsigned shift = span_bits - chop_bits;
	    for (size_t i = 0; i < output.size(); ++i) {
		const uint32_t combined = static_cast<uint32_t>(least_significant[i]) |
		    (static_cast<uint32_t>(most_significant[i]) << shift);
		output[i] = static_cast<int32_t>(combined + static_cast<uint32_t>(bias));
	    }
	    consumed = reader.offset() + msb_size + lsb_size - packet_offset;
	    return true;
	} else if (codec == 5) {
	    constexpr size_t window_capacity = 16;
	    std::vector<int32_t> window_values;
	    std::vector<int32_t> window_offsets;
	    size_t values_size = 0;
	    size_t offsets_size = 0;
	    if (!decode(reader.offset(), depth + 1, -1, window_values, values_size) ||
		!decode(reader.offset() + values_size, depth + 1, -1, window_offsets, offsets_size) ||
		window_offsets.size() != static_cast<size_t>(count)) {
		error = "invalid JT Int32 CDP2 move-to-front streams";
		return false;
	    }
	    std::vector<int32_t> window;
	    size_t value_index = 0;
	    output.reserve(static_cast<size_t>(count));
	    for (int32_t offset_value : window_offsets) {
		int32_t value = 0;
		if (offset_value < 0 || static_cast<size_t>(offset_value) >= window.size()) {
		    if (value_index >= window_values.size()) {
			error = "JT move-to-front stream exhausted its window values";
			return false;
		    }
		    value = window_values[value_index++];
		} else {
		    value = window[static_cast<size_t>(offset_value)];
		    window.erase(window.begin() + offset_value);
		}
		window.insert(window.begin(), value);
		if (window.size() > window_capacity) window.pop_back();
		output.push_back(value);
	    }
	    if (value_index != window_values.size()) {
		error = "JT move-to-front stream has unused window values";
		return false;
	    }
	    consumed = reader.offset() + values_size + offsets_size - packet_offset;
	    return true;
	} else {
	    error = "unsupported JT Int32 CDP2 CODEC type " + std::to_string(codec);
	    return false;
	}
	consumed = reader.offset() - packet_offset;
	return true;
    };

    values.clear();
    bytes_read = 0;
    if (offset > std::numeric_limits<size_t>::max() ||
	!decode(static_cast<size_t>(offset), 0, -1, values, bytes_read)) return false;
    unpack_residuals(values, predictor);
    return true;
}

bool
File::is_legacy_mesh(const Element &element) const
{
    static const std::array<unsigned char, 8> jt_tail = {{0x9b, 0x6b, 0x00, 0x80, 0xc7, 0xbb, 0x59, 0x97}};
    return header_.major_version == 8 &&
	(guid_matches(element.type_id, header_.little_endian, 0x10dd10ab, 0x2ac8, 0x11d1, jt_tail) ||
	 guid_matches(element.type_id, header_.little_endian, 0x10dd109f, 0x2ac8, 0x11d1, jt_tail));
}

bool
File::is_topological_mesh(const Element &element) const
{
    static const std::array<unsigned char, 8> jt_tail = {{0x9b, 0x6b, 0x00, 0x80, 0xc7, 0xbb, 0x59, 0x97}};
    return header_.major_version >= 9 &&
	(guid_matches(element.type_id, header_.little_endian, 0x10dd10ab, 0x2ac8, 0x11d1, jt_tail) ||
	 guid_matches(element.type_id, header_.little_endian, 0x10dd109f, 0x2ac8, 0x11d1, jt_tail));
}

bool
File::topological_mesh(const Element &element, Mesh &mesh, std::string &error) const
{
    mesh = Mesh{};
    if (!is_topological_mesh(element) || !range_valid(element.data_offset, element.data_length, bytes_.size())) {
	error = "element is not a supported JT topological mesh Shape LOD";
	return false;
    }
    Reader reader(bytes_, header_.little_endian, static_cast<size_t>(element.data_offset));
    if (header_.major_version >= 10) {
	/* JT 10.x wraps the Topologically Compressed Rep in a nested logical
	 * element (length, GUID, base type, object id) with u8 version words. */
	uint8_t base_version = 0;
	uint8_t vertex_lod_version = 0;
	uint64_t outer_bindings = 0;
	uint32_t nested_length = 0;
	Guid nested_type{};
	uint8_t nested_base_type = 0;
	int32_t nested_object_id = 0;
	uint8_t topomesh_version = 0;
	int32_t vertex_records_id = 0;
	uint8_t topology_version = 0;
	if (!reader.u8(base_version) || !reader.u8(vertex_lod_version) || !reader.u64(outer_bindings) ||
	    !reader.u32(nested_length) || !read_guid(reader, nested_type) || !reader.u8(nested_base_type) ||
	    !reader.i32(nested_object_id) || !reader.u8(topomesh_version) || !reader.i32(vertex_records_id) ||
	    !reader.u8(topology_version) || base_version != 1 || vertex_lod_version != 1 || nested_base_type != 9 ||
	    topomesh_version != 1 || topology_version != 1) {
	    error = "invalid JT 10 topologically compressed mesh header";
	    return false;
	}
	(void)outer_bindings;
	(void)nested_length;
	(void)nested_object_id;
	(void)vertex_records_id;
    } else {
	/* JT 9.x uses a flat layout with 16-bit version words and no nested
	 * element: base ver, vertex-LOD ver, VertexBinding2, TopoMesh-LOD ver,
	 * TriID, and the Compressed-Rep-Data version, then the symbol streams. */
	uint16_t base_version = 0;
	uint16_t vertex_lod_version = 0;
	uint64_t bindings = 0;
	uint16_t topomesh_lod_version = 0;
	int32_t tri_id = 0;
	uint16_t comp_rep_version = 0;
	if (!reader.u16(base_version) || !reader.u16(vertex_lod_version) || !reader.u64(bindings) ||
	    !reader.u16(topomesh_lod_version) || !reader.i32(tri_id) || !reader.u16(comp_rep_version)) {
	    error = "invalid JT 9 topologically compressed mesh header";
	    return false;
	}
	(void)base_version;
	(void)vertex_lod_version;
	(void)bindings;
	(void)topomesh_lod_version;
	(void)tri_id;
	(void)comp_rep_version;
    }

    TopologySymbols symbols;
    size_t offset = reader.offset();
    const auto packet = [&](Predictor predictor, std::vector<int32_t> &values) {
	size_t size = 0;
	if (!int32_packet2(offset, predictor, values, size, error)) return false;
	offset += size;
	return true;
    };
    for (std::vector<int32_t> &degrees : symbols.face_degrees)
	if (!packet(Predictor::None, degrees)) return false;
    if (!packet(Predictor::None, symbols.vertex_valences) ||
	!packet(Predictor::None, symbols.vertex_groups) ||
	!packet(Predictor::Lag1, symbols.vertex_flags)) return false;
    for (std::vector<uint64_t> &masks : symbols.face_attribute_masks) {
	std::vector<int32_t> lower;
	if (!packet(Predictor::None, lower)) return false;
	masks.reserve(lower.size());
	for (int32_t value : lower) masks.push_back(static_cast<uint32_t>(value));
    }
    std::vector<int32_t> upper_masks;
    if (!packet(Predictor::None, upper_masks) ||
	upper_masks.size() != symbols.face_attribute_masks[7].size()) {
	error = "JT topological face attribute mask streams have inconsistent lengths";
	return false;
    }
    for (size_t i = 0; i < upper_masks.size(); ++i)
	symbols.face_attribute_masks[7][i] |= static_cast<uint64_t>(static_cast<uint32_t>(upper_masks[i])) << 32;
    /* Unlike the surrounding fields, the high-degree masks are an ordinary
     * VecU32, not an Int32 CDP.  Omitting its count and payload shifts both
     * split streams and makes every genuine split appear to lack symbols. */
    Reader high_mask_reader(bytes_, header_.little_endian, offset);
    int32_t high_mask_count = 0;
    if (!high_mask_reader.i32(high_mask_count) || high_mask_count < 0 ||
	static_cast<size_t>(high_mask_count) > MAX_PACKET_VALUES) {
	error = "invalid JT high-degree face attribute mask count";
	return false;
    }
    symbols.high_degree_attribute_masks.reserve(static_cast<size_t>(high_mask_count));
    for (int32_t i = 0; i < high_mask_count; ++i) {
	uint32_t mask = 0;
	if (!high_mask_reader.u32(mask)) {
	    error = "truncated JT high-degree face attribute masks";
	    return false;
	}
	symbols.high_degree_attribute_masks.push_back(mask);
    }
    offset = high_mask_reader.offset();
    if (!packet(Predictor::Lag1, symbols.split_face_offsets) ||
	!packet(Predictor::None, symbols.split_face_positions)) return false;

    Reader vertex_reader(bytes_, header_.little_endian, offset + sizeof(uint32_t));
    uint64_t bindings = 0;
    uint8_t quantization[4] = {};
    int32_t topological_vertex_count = 0;
    int32_t attribute_count = 0;
    int32_t coordinate_count = 0;
    uint8_t components = 0;
    if (!vertex_reader.u64(bindings)) return false;
    for (uint8_t &value : quantization) if (!vertex_reader.u8(value)) return false;
    if (!vertex_reader.i32(topological_vertex_count) || !vertex_reader.i32(attribute_count) ||
	!vertex_reader.i32(coordinate_count) || !vertex_reader.u8(components) || components != 3 ||
	topological_vertex_count < 0 || coordinate_count != topological_vertex_count) {
	error = "invalid JT topological vertex record counts at " + std::to_string(vertex_reader.offset()) +
	    " (topological " + std::to_string(topological_vertex_count) + ", attributes " +
	    std::to_string(attribute_count) + ", coordinates " + std::to_string(coordinate_count) + ")";
	return false;
    }
    struct Quantizer { float minimum; float maximum; uint8_t bits; } quantizers[3] = {};
    for (Quantizer &quantizer : quantizers) {
	if (!vertex_reader.f32(quantizer.minimum) || !vertex_reader.f32(quantizer.maximum) ||
	    !vertex_reader.u8(quantizer.bits) || quantizer.bits > 32) {
	    error = "invalid JT topological coordinate quantizer";
	    return false;
	}
    }
    offset = vertex_reader.offset();
    std::vector<int32_t> coordinates[3];
    for (std::vector<int32_t> &component : coordinates) {
	if (!packet(Predictor::Lag1, component) || component.size() != static_cast<size_t>(coordinate_count)) {
	    error = "JT topological coordinate streams have inconsistent lengths";
	    return false;
	}
    }
    /* Coordinates are either lossless (bits==0, the integer is the raw IEEE-754
     * bit pattern) or uniformly quantized: value = min + (code - 0.5)*(max-min)/2^bits. */
    mesh.vertices.reserve(static_cast<size_t>(coordinate_count) * 3);
    for (size_t i = 0; i < static_cast<size_t>(coordinate_count); ++i) {
	for (size_t c = 0; c < 3; ++c) {
	    const Quantizer &quantizer = quantizers[c];
	    const uint32_t code = static_cast<uint32_t>(coordinates[c][i]);
	    double coordinate;
	    if (quantizer.bits != 0) {
		const double max_code = quantizer.bits >= 32 ? 4294967295.0 :
		    static_cast<double>(static_cast<uint32_t>(1) << quantizer.bits);
		coordinate = static_cast<double>(quantizer.minimum) +
		    (static_cast<double>(code) - 0.5) *
		    (static_cast<double>(quantizer.maximum) - static_cast<double>(quantizer.minimum)) / max_code;
	    } else {
		float value = 0.0f;
		std::memcpy(&value, &code, sizeof(value));
		coordinate = value;
	    }
	    if (!std::isfinite(coordinate)) {
		error = "JT topological coordinate is not finite";
		return false;
	    }
	    mesh.vertices.push_back(coordinate);
	}
    }
    TopologyMesh topology;
    if (!decode_topology(symbols, topology, error)) return false;
    mesh.faces.assign(topology.triangles.begin(), topology.triangles.end());
    for (int index : mesh.faces) {
	if (index < 0 || index >= coordinate_count) {
	    error = "JT topology references a coordinate outside the vertex array";
	    return false;
	}
    }
    if (mesh.faces.empty()) {
	error = "JT topological mesh contains no triangles";
	return false;
    }
    (void)bindings;
    (void)attribute_count;
    return true;
}

bool
File::legacy_mesh(const Element &element, Mesh &mesh, std::string &error) const
{
    static const std::array<unsigned char, 8> jt_tail = {{0x9b, 0x6b, 0x00, 0x80, 0xc7, 0xbb, 0x59, 0x97}};
    mesh = Mesh{};
    if (!is_legacy_mesh(element)) {
	error = "element is not a supported JT 8 mesh Shape LOD";
	return false;
    }
    const bool polygon_set = guid_matches(element.type_id, header_.little_endian,
	0x10dd109f, 0x2ac8, 0x11d1, jt_tail);
    if (!range_valid(element.data_offset, element.data_length, bytes_.size())) {
	error = "JT Tri-Strip element data is outside the file";
	return false;
    }

    Reader reader(bytes_, header_.little_endian, static_cast<size_t>(element.data_offset));
    const size_t element_end = static_cast<size_t>(element.data_offset) + element.data_length;
    uint16_t vertex_lod_version = 0;
    uint32_t bindings = 0;
    unsigned char quantization[4];
    uint16_t tristrip_version = 0;
    uint16_t representation_version = 0;
    uint8_t normal_binding = 0;
    uint8_t texture_binding = 0;
    uint8_t color_binding = 0;
    unsigned char representation_quantization[4];
    if (!reader.u16(vertex_lod_version) || !reader.u32(bindings) ||
	!reader.bytes(quantization, sizeof(quantization)) || !reader.u16(tristrip_version) ||
	!reader.u16(representation_version) || !reader.u8(normal_binding) ||
	!reader.u8(texture_binding) || !reader.u8(color_binding) ||
	!reader.bytes(representation_quantization, sizeof(representation_quantization))) {
	error = "truncated JT 8 Tri-Strip element metadata";
	return false;
    }
    if (vertex_lod_version != 1 || tristrip_version != 1 || representation_version != 1) {
	error = "unsupported JT 8 Tri-Strip element version";
	return false;
    }
    const bool quantized = representation_quantization[0] != 0;
    if ((quantization[0] == 0) != !quantized) {
	error = "inconsistent JT 8 vertex quantization parameters";
	return false;
    }
    if (normal_binding > 1 || texture_binding > 1 || color_binding > 1) {
	error = "JT 8 per-facet or per-primitive vertex attributes are not yet supported";
	return false;
    }
    (void)bindings;

    std::vector<int32_t> primitive_starts;
    size_t packet_size = 0;
    if (!int32_packet(reader.offset(), Predictor::Stride1, primitive_starts, packet_size, error)) return false;
    const size_t vertex_data_offset = reader.offset() + packet_size;
    size_t floats_per_vertex = 3;
    size_t vertex_count = 0;

    if (quantized) {
	if (normal_binding != 0 || texture_binding != 0 || color_binding != 0) {
	    error = "quantized JT 8 vertex attributes are not yet supported";
	    return false;
	}
	Reader quantized_reader(bytes_, header_.little_endian, vertex_data_offset);
	struct Quantizer { float minimum; float maximum; uint8_t bits; } quantizers[3] = {};
	for (Quantizer &quantizer : quantizers) {
	    if (!quantized_reader.f32(quantizer.minimum) || !quantized_reader.f32(quantizer.maximum) ||
		!quantized_reader.u8(quantizer.bits) || quantizer.bits == 0 || quantizer.bits > 32 ||
		quantizer.maximum < quantizer.minimum) {
		error = "invalid JT 8 point quantizer";
		return false;
	    }
	}
	int32_t unique_count = 0;
	if (!quantized_reader.i32(unique_count) || unique_count < 0) {
	    error = "invalid JT 8 quantized vertex count";
	    return false;
	}
	size_t quantized_offset = quantized_reader.offset();
	std::vector<int32_t> codes[3];
	for (std::vector<int32_t> &component_codes : codes) {
	    size_t component_size = 0;
	    if (!int32_packet(quantized_offset, Predictor::Lag1, component_codes, component_size, error)) return false;
	    if (component_codes.size() != static_cast<size_t>(unique_count)) {
		error = "JT 8 quantized coordinate arrays have inconsistent lengths";
		return false;
	    }
	    quantized_offset += component_size;
	}
	std::vector<int32_t> vertex_indices;
	size_t index_size = 0;
	if (!int32_packet(quantized_offset, Predictor::StripIndex, vertex_indices, index_size, error)) return false;
	(void)index_size;
	mesh.vertices.reserve(vertex_indices.size() * 3);
	for (int32_t index : vertex_indices) {
	    if (index < 0 || index >= unique_count) {
		error = "JT 8 quantized vertex index is outside the coordinate arrays";
		return false;
	    }
	    for (size_t component = 0; component < 3; ++component) {
		const Quantizer &quantizer = quantizers[component];
		const uint64_t maximum_code = quantizer.bits == 32 ? 0xffffffffULL : (1ULL << quantizer.bits) - 1;
		const uint32_t code = static_cast<uint32_t>(codes[component][static_cast<size_t>(index)]);
		if (code > maximum_code) {
		    error = "JT 8 quantized coordinate code exceeds its bit range";
		    return false;
		}
		const double scale = (quantizer.maximum - quantizer.minimum) / static_cast<double>(maximum_code);
		mesh.vertices.push_back(quantizer.minimum + code * scale);
	    }
	}
	vertex_count = vertex_indices.size();
    } else {
    Reader vertex_reader(bytes_, header_.little_endian, reader.offset() + packet_size);
    int32_t uncompressed_size = 0;
    int32_t compressed_size = 0;
    if (!vertex_reader.i32(uncompressed_size) || !vertex_reader.i32(compressed_size) || uncompressed_size < 0 ||
	compressed_size == 0 || vertex_reader.offset() > element_end) {
	error = "invalid JT 8 lossless vertex data header";
	return false;
    }
    const uint64_t payload_size = compressed_size < 0 ? -static_cast<int64_t>(compressed_size) : compressed_size;
    if (payload_size > element_end - vertex_reader.offset()) {
	error = "truncated JT 8 lossless vertex data";
	return false;
    }
    std::vector<unsigned char> raw(static_cast<size_t>(uncompressed_size));
    if (compressed_size < 0) {
	if (payload_size != raw.size() || !vertex_reader.bytes(raw.data(), raw.size())) {
	    error = "invalid JT 8 uncompressed vertex data size";
	    return false;
	}
    } else {
	uLongf output_size = static_cast<uLongf>(raw.size());
	const int zresult = uncompress(raw.data(), &output_size, bytes_.data() + vertex_reader.offset(),
	    static_cast<uLong>(payload_size));
	if (zresult != Z_OK || output_size != raw.size()) {
	    error = "cannot inflate JT 8 lossless vertex data";
	    return false;
	}
    }

    const size_t texture_components = texture_binding ? 2 : 0;
    const size_t color_components = color_binding ? 3 : 0;
    const size_t normal_components = normal_binding ? 3 : 0;
    floats_per_vertex = texture_components + color_components + normal_components + 3;
    const size_t bytes_per_vertex = floats_per_vertex * sizeof(float);
    if (raw.empty() || raw.size() % bytes_per_vertex != 0) {
	error = "JT 8 vertex data does not match its attribute bindings";
	return false;
    }
    vertex_count = raw.size() / bytes_per_vertex;
    Reader raw_reader(raw, header_.little_endian);
    mesh.vertices.reserve(vertex_count * 3);
    for (size_t i = 0; i < vertex_count; ++i) {
	float ignored = 0.0f;
	for (size_t j = 0; j < floats_per_vertex - 3; ++j)
	    if (!raw_reader.f32(ignored)) return false;
	for (size_t j = 0; j < 3; ++j) {
	    float coordinate = 0.0f;
	    if (!raw_reader.f32(coordinate)) return false;
	    mesh.vertices.push_back(coordinate);
	}
    }
    }

    if (primitive_starts.size() < 2 || primitive_starts.front() != 0) {
	error = "JT 8 triangle strip index list has no valid boundaries";
	return false;
    }
    /* The JT 8 Primitive List Indices are direct vertex indices into the
     * (per-vertex) raw vertex array -- not byte or float offsets.  The last
     * entry equals the total vertex count and each entry marks a strip start. */
    std::vector<size_t> starts;
    starts.reserve(primitive_starts.size());
    for (int32_t start : primitive_starts) {
	if (start < 0) {
	    error = "invalid JT 8 triangle strip boundary";
	    return false;
	}
	const size_t vertex = static_cast<size_t>(start);
	if (vertex > vertex_count || (!starts.empty() && vertex < starts.back())) {
	    error = "JT 8 triangle strip boundary is outside the vertex data";
	    return false;
	}
	starts.push_back(vertex);
    }
    if (starts.back() != vertex_count) starts.push_back(vertex_count);

    for (size_t primitive = 0; primitive + 1 < starts.size(); ++primitive) {
	for (size_t vertex = starts[primitive] + 2; vertex < starts[primitive + 1]; ++vertex) {
	    int a = polygon_set ? static_cast<int>(starts[primitive]) : static_cast<int>(vertex - 2);
	    int b = static_cast<int>(vertex - 1);
	    const int c = static_cast<int>(vertex);
	    if (!polygon_set && (vertex - starts[primitive]) % 2 != 0) std::swap(a, b);
	    if (a != b && b != c && a != c) mesh.faces.insert(mesh.faces.end(), {a, b, c});
	}
    }
    if (mesh.faces.empty()) {
	error = "JT 8 mesh primitives contain no non-degenerate triangles";
	return false;
    }
    return true;
}

bool
has_jt_signature(const char *path)
{
    if (!path) return false;
    std::ifstream input(path, std::ios::binary);
    char signature[8] = {};
    return input.read(signature, sizeof(signature)) && std::memcmp(signature, "Version ", sizeof(signature)) == 0;
}

} // namespace jt
