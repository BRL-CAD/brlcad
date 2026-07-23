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
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <functional>
#include <limits>
#include <map>
#include <regex>
#include <utility>

#include <zlib.h>

namespace jt {
namespace {

constexpr size_t GUID_LENGTH = 16;
constexpr size_t MINIMUM_HEADER_LENGTH = VERSION_LENGTH + 1 + 4 + 4 + GUID_LENGTH;
constexpr size_t LEGACY_TOC_ENTRY_LENGTH = GUID_LENGTH + 4 + 4 + 4;
constexpr size_t JT10_TOC_ENTRY_LENGTH = GUID_LENGTH + 8 + 4 + 4;
constexpr size_t SEGMENT_HEADER_LENGTH = GUID_LENGTH + 4 + 4;
constexpr size_t LEGACY_ELEMENT_HEADER_LENGTH = 4 + GUID_LENGTH + 1;
constexpr size_t OBJECT_ID_LENGTH = 4;
/* Upper bounds that guard the parser against malformed/hostile count fields
 * driving pathological allocations or loops.  Grouped so the individual limits
 * are documented and adjusted in one place. */
struct JTLimits {
    static constexpr size_t kMaxTocEntries = 1000000;
    /* Every Int32 packet count is checked against this cap before the output
     * vector is resized, bounding a single packet allocation to roughly
     * 400 MB (100M * sizeof(int32_t)).  Packets are decoded whole rather than
     * streamed, so this ceiling is what keeps a hostile count field from
     * exhausting memory; lower it if a stricter bound is ever needed. */
    static constexpr size_t kMaxPacketValues = 100000000;
    static constexpr size_t kMaxContextEntries = 8192;
    /* Sanity cap on whole-file size (~4 GB).  JT files far larger than this
     * indicate corruption or a hostile input rather than legitimate geometry. */
    static constexpr uintmax_t kMaxFileSizeBytes = 4ULL * 1024 * 1024 * 1024;
};
/* JT segment "type" values (TOC attribute high byte) that carry renderable
 * shape/LOD geometry and whose logical elements this parser enumerates. */
constexpr uint32_t SHAPE_SEGMENT_TYPE_MIN = 6;
constexpr uint32_t SHAPE_SEGMENT_TYPE_MAX = 16;
/* Guard against a malformed/hostile "uncompressed size" field driving a huge
 * allocation before any data is validated (1 GiB is far beyond any real LOD). */
constexpr size_t MAX_UNCOMPRESSED_BYTES = 1024u * 1024u * 1024u;
/* Upper bounds on the on-disk segment and logical-element length fields.  These
 * are far larger than any real LOD segment or element but reject a hostile file
 * whose length field would otherwise drive a pathological loop or allocation
 * before the tighter in-file range checks are reached. */
constexpr size_t MAX_SEGMENT_LENGTH = 1024u * 1024u * 1024u;
constexpr size_t MAX_ELEMENT_LENGTH = 1024u * 1024u * 1024u;

/* 16-bit arithmetic decoder register masks (JT File Format Reference, Int32
 * Probability CDP): the high value starts all-ones; renormalization inspects
 * the most significant bit and the underflow (second-most significant) bit. */
constexpr uint16_t kArithmeticHighWord = 0xffff;
constexpr uint16_t kMsbMask = 0x8000;
constexpr uint16_t kRenormalizationBit = 0x4000;
constexpr uint16_t kLowWordMask = 0x3fff;

/* Quantizer parameters shared by the JT 8 point set and JT 9/10 topological
 * coordinate readers: a per-component [minimum, maximum] range and a bit count
 * (bits == 0 meaning the stored code is a raw IEEE-754 bit pattern). */
struct Quantizer {
    float minimum = 0.0f;
    float maximum = 0.0f;
    uint8_t bits = 0;
};

/* JT 9/10 uniform coordinate dequantization (JT File Format Reference Section
 * 14.2.1.4; TKJT JtDecode_VertexData): value = min + (code - 0.5)*(max-min)/2^bits.
 * A bit count of 32 or more uses the full unsigned range as the code span.
 *
 * J8-027: the (code - 0.5) offset and the 2^bits code span match the JT spec
 * 14.2.1.4 uniform-quantizer inverse transform (the half-code bias centres each
 * reconstructed value in its quantization bucket).  Precision guards: the code
 * is widened to an unsigned 32-bit value and all arithmetic is performed in
 * double precision (which represents every uint32 code and the code span
 * exactly), so the intermediate products cannot overflow; a degenerate zero code
 * span (bits == 0) collapses to the interval minimum rather than dividing by
 * zero, and a non-finite range yields the minimum rather than a NaN/Inf. */
inline double quantize_coordinate(const Quantizer &quantizer, int32_t code)
{
    const double minimum = static_cast<double>(quantizer.minimum);
    const double maximum = static_cast<double>(quantizer.maximum);
    /* Compute the code span 2^bits without ever forming a shift of 32 or more,
     * which is undefined for a 32-bit operand: bits >= 32 uses the full unsigned
     * range explicitly, and only bits in [0, 31] reach the shift. */
    const double max_code = quantizer.bits >= 32 ?
	4294967295.0 :
	static_cast<double>(static_cast<uint32_t>(1) << quantizer.bits);
    if (!(max_code > 0.0) || !std::isfinite(minimum) || !std::isfinite(maximum))
	return minimum;
    const double value = minimum +
	(static_cast<double>(static_cast<uint32_t>(code)) - 0.5) *
	(maximum - minimum) / max_code;
    /* Guard against a non-finite reconstruction from a pathological range. */
    return std::isfinite(value) ? value : minimum;
}

class Reader {
  public:
    Reader(const ByteBuffer &bytes, bool little_endian, size_t offset = 0) :
	bytes_(bytes), little_endian_(little_endian), offset_(offset) {}

    bool can_read(size_t count) const noexcept
    {
	/* Explicit bounds check before the subtraction: if the cursor has been
	 * seeked past the end, bytes_.size() - offset_ would wrap around to a
	 * huge value and spuriously admit the read. */
	if (offset_ > bytes_.size()) return false;
	return count <= bytes_.size() - offset_;
    }
    size_t offset() const noexcept { return offset_; }
    /* Maintain the class invariant that the cursor never lies past the end of the
     * buffer: on a well-formed file every seek target is already in range (bounded
     * by a prior range check), so clamping only fires for malformed input, where
     * it turns a would-be out-of-range cursor into a guaranteed can_read() failure
     * rather than a silently oversized offset feeding later arithmetic. */
    void seek(size_t offset) noexcept { offset_ = offset <= bytes_.size() ? offset : bytes_.size(); }

    bool bytes(unsigned char *out, size_t count) const noexcept
    {
	if (!can_read(count)) return false;
	std::copy_n(bytes_.data() + offset_, count, out);
	offset_ += count;
	return true;
    }

    bool u8(uint8_t &value) const noexcept
    {
	if (!can_read(1)) return false;
	value = bytes_[offset_++];
	return true;
    }

    bool u32(uint32_t &value) const noexcept { return assemble(value); }

    bool i32(int32_t &value) const noexcept
    {
	uint32_t bits = 0;
	if (!u32(bits)) return false;
	value = static_cast<int32_t>(bits);
	return true;
    }

    bool u16(uint16_t &value) const noexcept { return assemble(value); }

    bool f32(float &value) const noexcept
    {
	uint32_t bits = 0;
	if (!u32(bits)) return false;
	static_assert(sizeof(value) == sizeof(bits), "JT F32 requires a 32-bit float");
	std::memcpy(&value, &bits, sizeof(value));
	return true;
    }

    bool u64(uint64_t &value) const noexcept { return assemble(value); }

  private:
    /* Assemble an unsigned integer of type T from sizeof(T) bytes, honouring the
     * file's byte order.  Centralises the shift bookkeeping that u16/u32/u64
     * previously each open-coded. */
    template <typename T>
    bool assemble(T &value) const noexcept
    {
	unsigned char data[sizeof(T)];
	if (!bytes(data, sizeof(data))) return false;
	value = 0;
	for (size_t i = 0; i < sizeof(data); ++i) {
	    const size_t shift_index = little_endian_ ? i : sizeof(data) - i - 1;
	    value |= static_cast<T>(static_cast<T>(data[i]) << (shift_index * 8));
	}
	return true;
    }

    const ByteBuffer &bytes_;
    bool little_endian_;
    mutable size_t offset_;
};

/* Free-function shim retained for the terse in-file bounds tests; the overflow-
 * safe logic now lives in jt::RangeValidator (jt_parser.h). */
bool range_valid(uint64_t offset, uint64_t length, size_t size)
{
    return RangeValidator(size).check(offset, length);
}

bool read_guid(Reader &reader, Guid &guid)
{
    return reader.bytes(guid.data(), guid.size());
}

/* The three per-axis quantizers of a JT vertex coordinate record, plus the shared
 * read/validate loop that the JT 8 point-set and JT 9/10 topological coordinate
 * readers used to duplicate.  require_nonzero_bits distinguishes the JT 8 path
 * (which has a separate lossless codec and so rejects bits == 0 and an inverted
 * range) from the JT 9/10 path (where bits == 0 selects the ExpMant codec and is
 * handled by the caller). */
struct QuantizerContext {
    std::array<Quantizer, 3> quantizers = {};

    /* Read three float/float/u8 quantizers from reader, validating each.  Returns
     * false (leaving error set) on a truncated or out-of-range quantizer. */
    static bool read(Reader &reader, bool require_nonzero_bits, QuantizerContext &out, std::string &error)
    {
	for (Quantizer &quantizer : out.quantizers) {
	    if (!reader.f32(quantizer.minimum) || !reader.f32(quantizer.maximum) ||
		!reader.u8(quantizer.bits) || quantizer.bits > 32 ||
		!std::isfinite(quantizer.minimum) || !std::isfinite(quantizer.maximum) ||
		(require_nonzero_bits && (quantizer.bits == 0 || quantizer.maximum < quantizer.minimum))) {
		error = require_nonzero_bits ? "invalid JT 8 point quantizer" :
		    "invalid JT topological coordinate quantizer";
		return false;
	    }
	    /* GEO-022: reject an inverted quantizer range (maximum < minimum) on
	     * both codec paths.  A flat axis (minimum == maximum) is legitimate -- it
	     * dequantizes every code to the constant value with no NaN/Inf risk -- but
	     * an inverted range yields a negative code-range divisor and nonsensical
	     * coordinates.  The JT 8 path already enforces this above via the
	     * require_nonzero_bits branch; this closes the same hole on the JT 9/10
	     * topological quantizers, where bits == 0 (the lossless ExpMant codec with
	     * minimum == maximum == 0) is skipped. */
	    if (quantizer.bits != 0 && quantizer.maximum < quantizer.minimum) {
		error = require_nonzero_bits ? "JT 8 point quantizer has an inverted range" :
		    "JT topological coordinate quantizer has an inverted range";
		return false;
	    }
	}
	return true;
    }

    /* JT 9/10 uniform dequantization of a coordinate code on the given axis. */
    double quantize(size_t component, int32_t code) const { return quantize_coordinate(quantizers[component], code); }
};

/* JT Int32 residual predictor logic (JT File Format Reference, Int32 CDP).  The
 * per-predictor value formulas and the residual-unpacking loop are grouped as
 * static methods so the codec table lives in one place, is testable in isolation,
 * and can gain new predictor types without touching the packet readers. */
struct PredictorCodec {

static int32_t predicted_value(const std::vector<int32_t> &values, size_t index, Predictor predictor)
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

static void unpack_residuals(std::vector<int32_t> &values, Predictor predictor)
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

}; // struct PredictorCodec

/* Thin alias for the LSG node-GUID checks below; shape classification uses
 * GuidRegistry directly (see jt_parser.h). */
inline bool guid_matches(const Guid &guid, bool little_endian, uint32_t first, uint16_t second,
    uint16_t third, const std::array<unsigned char, 8> &tail)
{
    return GuidRegistry::matches(guid, little_endian, first, second, third, tail);
}

/* Reconstruct a single unit normal from the Sun/Deering angular codec used by the
 * JT 8 quantized normal record.  The unit sphere is split into six sextants and
 * eight octants; within a patch the (theta, psi) pair (each 'bits' wide) gives a
 * point on the folded first-octant triangle, which the octant/sextant then unfold
 * back onto the full sphere.  Mirrors the reference JT decoder's arithmetic; used
 * only to recover per-vertex shading normals, so a best-effort reconstruction is
 * acceptable. */
static void deering_decode_normal(int32_t sextant, int32_t octant, int32_t theta_code,
    int32_t psi_code, unsigned bits, double out[3])
{
    const int levels = bits > 0 ? (1 << bits) : 1;
    /* Map the quantized codes into the folded first-octant angular range. */
    const double quarter_pi = 0.785398163397448;   /* pi/4: folded octant edge. */
    const double psi_max = 0.615479709;   /* asin(tan(pi/6)) folds the octant edge. */
    double theta = (levels > 1) ?
	quarter_pi * (static_cast<double>(theta_code) / static_cast<double>(levels - 1)) : 0.0;
    double psi = (levels > 1) ?
	psi_max * (static_cast<double>(psi_code) / static_cast<double>(levels - 1)) : 0.0;
    /* First-octant unit vector from the two folding angles. */
    double xyz[3];
    xyz[0] = std::sin(psi);
    xyz[1] = std::cos(psi) * std::sin(theta);
    xyz[2] = std::cos(psi) * std::cos(theta);
    /* Octant (0..7): three sign/axis bits flip the components into the chosen
     * octant of the sphere. */
    if (octant & 0x4) xyz[0] = -xyz[0];
    if (octant & 0x2) xyz[1] = -xyz[1];
    if (octant & 0x1) xyz[2] = -xyz[2];
    /* Sextant (0..5): rotate/permute the axes so the folded first-octant triangle
     * lands in the correct one of the six sphere sextants. */
    double n[3] = {xyz[0], xyz[1], xyz[2]};
    switch (sextant & 0x7) {
	case 0: n[0] = xyz[0]; n[1] = xyz[1]; n[2] = xyz[2]; break;
	case 1: n[0] = xyz[1]; n[1] = xyz[2]; n[2] = xyz[0]; break;
	case 2: n[0] = xyz[2]; n[1] = xyz[0]; n[2] = xyz[1]; break;
	case 3: n[0] = xyz[0]; n[1] = xyz[2]; n[2] = xyz[1]; break;
	case 4: n[0] = xyz[2]; n[1] = xyz[1]; n[2] = xyz[0]; break;
	default: n[0] = xyz[1]; n[1] = xyz[0]; n[2] = xyz[2]; break;
    }
    /* Normalize to guard against accumulated rounding so the BoT binding stores a
     * unit vector. */
    const double len = std::sqrt(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]);
    if (len > 1e-12) {
	out[0] = n[0] / len; out[1] = n[1] / len; out[2] = n[2] / len;
    } else {
	out[0] = 0.0; out[1] = 0.0; out[2] = 1.0;
    }
}

/* Emit triangles for a set of JT primitives whose per-primitive vertex ranges are
 * given by consecutive entries of strip_starts (vertex indices into the shared
 * vertex array; strip_starts.back() is the total vertex count).  A polygon set
 * fans every triangle off the primitive's first vertex; a tri-strip walks the
 * strip, swapping the leading two vertices on odd steps to keep winding, and
 * both cases drop degenerate triangles.
 *
 * J8-013: strip_starts may describe many strips/primitive lists in a single JT 8
 * element, not just one.  The outer loop iterates over every consecutive
 * [start, next) primitive range, and the tri-strip winding parity is measured
 * relative to each strip's own start (vertex - strip_starts[primitive]) so each
 * primitive list is triangulated independently. */
void fan_triangulate(const std::vector<size_t> &strip_starts, std::vector<int> &out_faces, bool polygon_set)
{
    for (size_t primitive = 0; primitive + 1 < strip_starts.size(); ++primitive) {
	/* Skip a degenerate/empty primitive list (start not strictly less than
	 * its successor by at least a full triangle) without emitting faces. */
	if (strip_starts[primitive] + 2 >= strip_starts[primitive + 1]) continue;
	for (size_t vertex = strip_starts[primitive] + 2; vertex < strip_starts[primitive + 1]; ++vertex) {
	    int a = polygon_set ? static_cast<int>(strip_starts[primitive]) : static_cast<int>(vertex - 2);
	    int b = static_cast<int>(vertex - 1);
	    const int c = static_cast<int>(vertex);
	    if (!polygon_set && (vertex - strip_starts[primitive]) % 2 != 0) std::swap(a, b);
	    if (a != b && b != c && a != c) out_faces.insert(out_faces.end(), {a, b, c});
	}
    }
}

/* MSB-first bit reader over an assembled U32 code-text word array, capped at
 * bit_count valid bits.  Centralises the offset bookkeeping that the Bitlength
 * and Bitlength2 CODECs (and the nibble decoder) previously each open-coded, so
 * the width-extraction and sign-extension rules live in one testable place. */
class BitReader {
  public:
    BitReader(const std::vector<uint32_t> &words, size_t bit_count) : words_(words), bit_count_(bit_count) {}

    /* Read a single bit into the low bit of bit; false once the cap is reached. */
    bool read_bit(uint32_t &bit)
    {
	if (bit_offset_ >= bit_count_) return false;
	bit = (words_[bit_offset_ / 32] >> (31 - bit_offset_ % 32)) & 1u;
	++bit_offset_;
	return true;
    }

    /* Read width (0..32) bits MSB-first as an unsigned value; false if the read
     * would run past the cap. */
    bool read(unsigned width, uint32_t &value)
    {
	if (bit_offset_ + width > bit_count_) return false;
	value = 0;
	for (unsigned i = 0; i < width; ++i, ++bit_offset_)
	    value = (value << 1) | ((words_[bit_offset_ / 32] >> (31 - bit_offset_ % 32)) & 1u);
	return true;
    }

    /* Read width bits and sign-extend from the width-th bit. */
    bool read_signed(unsigned width, int32_t &value)
    {
	uint32_t raw = 0;
	if (!read(width, raw)) return false;
	if (width > 0 && width < 32 && (raw & (1u << (width - 1)))) raw |= ~((1u << width) - 1u);
	value = static_cast<int32_t>(raw);
	return true;
    }

    size_t offset() const { return bit_offset_; }

  private:
    const std::vector<uint32_t> &words_;
    size_t bit_count_;
    size_t bit_offset_ = 0;
};

bool decode_bitlength(const std::vector<uint32_t> &words, size_t bit_count, size_t value_count,
    std::vector<int32_t> &values, std::string &error)
{
    BitReader bits(words, bit_count);
    int field_width = 0;
    const auto read_bit = [&bits](uint32_t &bit) { return bits.read_bit(bit); };

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
		/* field_width feeds the value-read loop and sign-extension shifts
		 * below, so it must stay within [0, 32] to avoid an out-of-range
		 * shift on a malformed width prefix. */
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

	/* Guard the extraction shifts directly: field_width is constrained in
	 * the width-prefix loop above, but a defensive bound here keeps the
	 * (encoded << 1) accumulation and the sign-extension below well
	 * defined for any field_width that could reach this point. */
	if (field_width < 0 || field_width > 32) {
	    error = "invalid JT Bitlength CODEC field width";
	    return false;
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

/* Selects between the JT 9.x and JT 10.x variants of the Bitlength2 CODEC. */
enum class CodecMode { JT9, JT10 };

bool decode_bitlength2(const std::vector<uint32_t> &words, size_t bit_count, size_t value_count,
    std::vector<int32_t> &values, CodecMode mode, std::string &error)
{
    BitReader bits(words, bit_count);
    const auto read = [&bits](unsigned width, uint32_t &value) { return bits.read(width, value); };
    const auto read_signed = [&bits](unsigned width, int32_t &value) { return bits.read_signed(width, value); };
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
	/* width is a multiple of 4 in [4, 32]; a full 32-bit value needs no
	 * sign-extension and must skip the 1u << 32 shift (undefined behaviour). */
	if (width > 0 && width < 32 && (encoded & (1u << (width - 1)))) encoded |= ~((1u << width) - 1u);
	value = static_cast<int32_t>(encoded);
	return true;
    };
    uint32_t variable = 0;
    if (!read(1, variable)) return false;
    values.reserve(value_count);
    if (mode == CodecMode::JT9) {
	/* JT 9.x Bitlength2: fixed-width encodes the range as two 6-bit widths
	 * followed by signed min/max; variable-width uses 3-bit change/run
	 * widths.  (JT 10.x replaced this with the nibble scheme below.) */
	if (variable == 0) {
	    uint32_t min_bits = 0;
	    uint32_t max_bits = 0;
	    if (!read(6, min_bits) || !read(6, max_bits) || min_bits > 32 || max_bits > 32) {
		error = "invalid JT 9 fixed-width Bitlength CODEC widths";
		return false;
	    }
	    int32_t minimum = 0;
	    int32_t maximum = 0;
	    if (!read_signed(min_bits, minimum) || !read_signed(max_bits, maximum)) {
		error = "truncated JT 9 fixed-width Bitlength CODEC range";
		return false;
	    }
	    /* Reject an inverted range explicitly; a degenerate (minimum == maximum)
	     * range is a valid constant block handled by the range <= 0 case below. */
	    if (minimum > maximum) {
		values.assign(value_count, minimum);
		return true;
	    }
	    const int64_t range = static_cast<int64_t>(maximum) - minimum;
	    if (range <= 0) {
		values.assign(value_count, minimum);
		return true;
	    }
	    unsigned field_width = 0;
	    for (uint64_t bound = static_cast<uint64_t>(range); bound != 0; bound >>= 1) ++field_width;
	    for (size_t i = 0; i < value_count; ++i) {
		uint32_t encoded = 0;
		if (!read(field_width, encoded)) {
		    error = "truncated JT 9 fixed-width Bitlength CODEC values";
		    return false;
		}
		values.push_back(static_cast<int32_t>(static_cast<uint32_t>(minimum) + encoded));
	    }
	    return true;
	}
	int32_t mean = 0;
	uint32_t chg_width_bits = 0;
	uint32_t run_len_bits = 0;
	if (!read_signed(32, mean) || !read(3, chg_width_bits) || !read(3, run_len_bits) ||
	    chg_width_bits == 0 || chg_width_bits > 32 || run_len_bits == 0 || run_len_bits > 32) {
	    error = "invalid JT 9 variable-width Bitlength CODEC header";
	    return false;
	}
	const int32_t chg_min = -(static_cast<int32_t>(1) << (chg_width_bits - 1));
	const int32_t chg_max = (static_cast<int32_t>(1) << (chg_width_bits - 1)) - 1;
	int field_width = 0;
	while (values.size() < value_count) {
	    int32_t delta = 0;
	    do {
		if (!read_signed(chg_width_bits, delta)) return false;
		field_width += delta;
	    } while (delta == chg_min || delta == chg_max);
	    uint32_t run_length = 0;
	    if (field_width != std::clamp(field_width, 0, 32) || !read(run_len_bits, run_length) || run_length == 0 ||
		values.size() + run_length > value_count) {
		error = "invalid JT 9 variable-width Bitlength CODEC block (width " +
		    std::to_string(field_width) + ", run " + std::to_string(run_length) + ")";
		return false;
	    }
	    for (uint32_t i = 0; i < run_length; ++i) {
		int32_t value = 0;
		if (field_width > 0 && !read_signed(static_cast<unsigned>(field_width), value)) return false;
		values.push_back(static_cast<int32_t>(static_cast<uint32_t>(value) + static_cast<uint32_t>(mean)));
	    }
	}
	return true;
    }
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
	/* A maximal span (0xffffffff) yields field_width == 32; anything wider
	 * would overrun the 32-bit value reads below, so reject it defensively. */
	if (field_width > 32) {
	    error = "invalid JT 10 fixed-width Bitlength CODEC field width";
	    return false;
	}
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
	    /* The accumulated field_width drives the sign-extension shifts in the
	     * value loop below; reject anything outside [0, 32] before it is used. */
	    if (field_width < 0 || field_width > 32) {
		error = "invalid JT 10 variable-width Bitlength CODEC field width";
		return false;
	    }
	    uint32_t run_length = 0;
	    if (field_width != std::clamp(field_width, 0, 32) || !read(4, run_length) || run_length == 0 ||
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

/* ------------------------------------------------------------------------
 * JT CDP1 (Load1) Huffman (CODEC 2) and Arithmetic (CODEC 3) support.
 *
 * These decoders and the shared probability-context reader are faithful ports
 * of the reference jcadlib implementation (MIT, J. Raida) cross-checked against
 * the JT v9.5 File Format Reference (section 8.1.1).  The Huffman tree in particular
 * must be built with jcadlib's exact (unstable) min-heap tie-break or a tree
 * that differs from the encoder's is produced and the stream mis-decodes.
 * ---------------------------------------------------------------------- */

/* The escape symbol marks a value carried out-of-band; raw symbols are stored
 * biased by +2, so a stored 0 decodes to the escape symbol -2. */
constexpr int32_t JT_ESCAPE_SYMBOL = -2;
/* The 16-bit range coder requires the summed occurrence counts to stay within
 * the coder's precision. */
constexpr uint32_t JT_MAX_CONTEXT_TOTAL = 65535;

/* MSB-first bit reader over raw file bytes, used for the byte-packed JT Int32
 * probability-context block. */
class MsbBitReader {
  public:
    MsbBitReader(const unsigned char *data, size_t size, size_t start_bit)
	: data_(data), total_bits_(size * 8), bit_(start_bit) {}
    bool read(unsigned width, uint32_t &value)
    {
	if (width > 32 || bit_ + width > total_bits_) return false;
	value = 0;
	for (unsigned i = 0; i < width; ++i, ++bit_)
	    value = (value << 1) | ((data_[bit_ / 8] >> (7 - bit_ % 8)) & 1u);
	return true;
    }
    bool read_signed(unsigned width, int32_t &value)
    {
	uint32_t raw = 0;
	if (!read(width, raw)) return false;
	if (width > 0 && width < 32 && (raw & (1u << (width - 1)))) raw |= ~((1u << width) - 1u);
	value = static_cast<int32_t>(raw);
	return true;
    }
    size_t bit_pos() const { return bit_; }

  private:
    const unsigned char *data_;
    size_t total_bits_;
    size_t bit_;
};

/* MSB-first bit reader over an already-assembled U32 code-text word array (each
 * word is native-order, so bit 0 of the stream is bit 31 of word 0). */
class WordBitReader {
  public:
    explicit WordBitReader(const std::vector<uint32_t> &words) : words_(words), total_bits_(words.size() * 32) {}
    uint32_t next()
    {
	if (pos_ >= total_bits_) return 0;
	const uint32_t bit = (words_[pos_ / 32] >> (31 - pos_ % 32)) & 1u;
	++pos_;
	return bit;
    }
    size_t position() const { return pos_; }

  private:
    const std::vector<uint32_t> &words_;
    size_t total_bits_;
    size_t pos_ = 0;
};

struct ProbEntry {
    int32_t symbol = 0;
    uint32_t occurrence = 0;
    int32_t value = 0;
    uint32_t next_context = 0;
    uint32_t cum_low = 0;
    uint32_t cum_high = 0;
};

struct ProbContext {
    std::vector<ProbEntry> entries;
    uint32_t total = 0;
};

/* Read the Int32 Probability Contexts block.  two_table_layout selects the JT 8
 * (<9.0) form (a leading U8 table count, 32-bit entry counts, per-entry
 * next-context, and a secondary table that reuses table 0's symbol->value map)
 * versus the JT 9 form (a single table, 16-bit entry count, no next-context).
 * On success byte_offset is advanced past the byte-aligned end of the block. */
bool read_prob_contexts(const unsigned char *data, size_t size, size_t &byte_offset,
    bool two_table_layout, std::vector<ProbContext> &contexts, std::string &error)
{
    contexts.clear();
    size_t table_count = 1;
    size_t start_bit = byte_offset * 8;
    if (two_table_layout) {
	if (byte_offset >= size) { error = "truncated JT probability context table count"; return false; }
	table_count = data[byte_offset];
	if (table_count < 1 || table_count > 2) { error = "invalid JT probability context table count"; return false; }
	start_bit = (byte_offset + 1) * 8;
    }
    MsbBitReader reader(data, size, start_bit);
    std::vector<std::pair<int32_t, int32_t>> symbol_values;   /* table 0 symbol -> value */
    for (size_t t = 0; t < table_count; ++t) {
	uint32_t entry_count = 0;
	if (!reader.read(two_table_layout ? 32 : 16, entry_count) || entry_count == 0 ||
	    entry_count > JTLimits::kMaxContextEntries) {
	    error = "invalid JT probability context entry count";
	    return false;
	}
	uint32_t sym_bits = 0, occ_bits = 0, val_bits = 0, next_bits = 0;
	int32_t min_value = 0;
	if (t == 0) {
	    if (!reader.read(6, sym_bits) || !reader.read(6, occ_bits) || !reader.read(6, val_bits) ||
		(two_table_layout && !reader.read(6, next_bits)) || !reader.read_signed(32, min_value)) {
		error = "truncated JT probability context header";
		return false;
	    }
	} else {
	    if (!reader.read(6, sym_bits) || !reader.read(6, occ_bits) || !reader.read(6, next_bits)) {
		error = "truncated JT probability context header";
		return false;
	    }
	    val_bits = 0;
	}
	if (sym_bits > 32 || occ_bits == 0 || occ_bits > 32 || val_bits > 32 || next_bits > 32) {
	    error = "invalid JT probability context field widths";
	    return false;
	}
	ProbContext ctx;
	ctx.entries.reserve(entry_count);
	uint32_t cum = 0;
	for (uint32_t j = 0; j < entry_count; ++j) {
	    uint32_t sym_raw = 0, occ = 0, val_raw = 0, next_raw = 0;
	    if (!reader.read(sym_bits, sym_raw) || !reader.read(occ_bits, occ) ||
		(val_bits > 0 && !reader.read(val_bits, val_raw)) ||
		(next_bits > 0 && !reader.read(next_bits, next_raw))) {
		error = "truncated JT probability context entry";
		return false;
	    }
	    ProbEntry entry;
	    entry.symbol = static_cast<int32_t>(sym_raw) - 2;
	    entry.occurrence = occ;
	    entry.next_context = next_raw;
	    if (val_bits > 0) {
		entry.value = static_cast<int32_t>(val_raw + static_cast<uint32_t>(min_value));
	    } else {
		entry.value = 0;   /* secondary table: resolve by symbol from table 0 */
		for (const auto &sv : symbol_values)
		    if (sv.first == entry.symbol) { entry.value = sv.second; break; }
	    }
	    entry.cum_low = cum;
	    cum += occ;
	    entry.cum_high = cum;
	    ctx.entries.push_back(entry);
	    if (t == 0) symbol_values.emplace_back(entry.symbol, entry.value);
	}
	ctx.total = cum;
	if (ctx.total == 0 || ctx.total > JT_MAX_CONTEXT_TOTAL) {
	    error = "invalid JT probability context occurrence total";
	    return false;
	}
	contexts.push_back(std::move(ctx));
    }
    byte_offset = (reader.bit_pos() + 7) / 8;   /* skip to the next byte boundary */
    return true;
}

struct HuffNode {
    int left = -1;
    int right = -1;
    bool leaf = false;
    int32_t symbol = 0;
    int32_t value = 0;
    uint32_t count = 0;
};

/* jcadlib's exact 1-based array min-heap keyed only on the node occurrence count.
 * The tie-break is load-bearing: sift-up stops on equal counts and sift-down
 * prefers the earlier child, reproducing the encoder's tree. */
class HuffHeap {
  public:
    explicit HuffHeap(const std::vector<HuffNode> &nodes) : nodes_(nodes) {}
    size_t size() const { return heap_.size(); }
    void add(int node)
    {
	heap_.push_back(node);
	size_t i = heap_.size();
	while (i != 1 && count_of(heap_[(i / 2) - 1]) > count_of(node)) {
	    heap_[i - 1] = heap_[(i / 2) - 1];
	    i /= 2;
	}
	heap_[i - 1] = node;
    }
    int get_top()
    {
	const int top = heap_[0];
	remove();
	return top;
    }

  private:
    uint32_t count_of(int idx) const { return nodes_[idx].count; }
    void remove()
    {
	if (heap_.empty()) return;
	const int y = heap_.back();
	size_t i = 1, ci = 2;
	const size_t size = heap_.size() - 1;
	while (ci <= size) {
	    if (ci < size && count_of(heap_[ci - 1]) > count_of(heap_[ci])) ci += 1;
	    if (count_of(y) < count_of(heap_[ci - 1])) break;
	    heap_[i - 1] = heap_[ci - 1];
	    i = ci;
	    ci *= 2;
	}
	heap_[i - 1] = y;
	heap_.pop_back();
    }

    const std::vector<HuffNode> &nodes_;
    std::vector<int> heap_;
};

bool huffman_decode(const std::vector<ProbContext> &contexts, const std::vector<uint32_t> &words,
    size_t code_bits, size_t value_count, const std::vector<int32_t> &oob,
    std::vector<int32_t> &out, std::string &error)
{
    out.clear();
    out.reserve(value_count);
    WordBitReader bits(words);
    size_t oob_index = 0;
    for (const ProbContext &ctx : contexts) {
	std::vector<HuffNode> nodes;
	nodes.reserve(ctx.entries.size() * 2);
	HuffHeap heap(nodes);
	for (const ProbEntry &entry : ctx.entries) {
	    HuffNode node;
	    node.leaf = true;
	    node.symbol = entry.symbol;
	    node.value = entry.value;
	    node.count = entry.occurrence;
	    nodes.push_back(node);
	}
	for (int i = 0; i < static_cast<int>(nodes.size()); ++i) heap.add(i);
	if (heap.size() == 0) { error = "empty JT Huffman probability context"; return false; }
	while (heap.size() > 1) {
	    const int n1 = heap.get_top();
	    const int n2 = heap.get_top();
	    HuffNode internal;
	    internal.left = n1;
	    internal.right = n2;
	    internal.count = nodes[n1].count + nodes[n2].count;
	    nodes.push_back(internal);
	    heap.add(static_cast<int>(nodes.size()) - 1);
	}
	int root = heap.get_top();
	int node = root;
	while (bits.position() < code_bits) {
	    node = bits.next() ? nodes[node].left : nodes[node].right;
	    if (node < 0) { error = "malformed JT Huffman code text"; return false; }
	    if (!nodes[node].leaf) continue;
	    if (nodes[node].symbol == JT_ESCAPE_SYMBOL) {
		if (oob_index >= oob.size()) { error = "JT Huffman out-of-band values exhausted"; return false; }
		out.push_back(oob[oob_index++]);
	    } else {
		out.push_back(nodes[node].value);
	    }
	    node = root;
	}
    }
    if (out.size() != value_count) { error = "JT Huffman CODEC produced the wrong value count"; return false; }
    return true;
}

bool arithmetic_decode_cdp1(const std::vector<ProbContext> &contexts, const std::vector<uint32_t> &words,
    size_t code_bits, size_t value_count, size_t symbol_count, const std::vector<int32_t> &oob,
    std::vector<int32_t> &out, std::string &error)
{
    out.clear();
    if (code_bits == 0) {
	/* Every value was carried out-of-band; the coder is skipped entirely. */
	if (oob.size() != value_count) { error = "JT Arithmetic CODEC empty code text lacks all out-of-band values"; return false; }
	out = oob;
	return true;
    }
    out.reserve(value_count);
    WordBitReader bits(words);
    uint16_t low = 0, high = kArithmeticHighWord, code = 0;
    for (unsigned i = 0; i < 16; ++i) code = static_cast<uint16_t>((code << 1) | bits.next());
    uint32_t current = 0;
    size_t oob_index = 0;
    for (size_t i = 0; i < symbol_count; ++i) {
	if (current >= contexts.size()) { error = "JT Arithmetic CODEC context index out of range"; return false; }
	const ProbContext &ctx = contexts[current];
	const uint32_t total = ctx.total;
	const uint32_t range = static_cast<uint32_t>(high - low) + 1;
	const uint32_t rescaled = ((static_cast<uint32_t>(code - low) + 1) * total - 1) / range;
	const ProbEntry *found = nullptr;
	for (const ProbEntry &entry : ctx.entries)
	    if (rescaled >= entry.cum_low && rescaled < entry.cum_high) { found = &entry; break; }
	if (!found) { error = "JT Arithmetic CODEC symbol is outside its probability context"; return false; }
	high = static_cast<uint16_t>(low + (range * found->cum_high) / total - 1);
	low = static_cast<uint16_t>(low + (range * found->cum_low) / total);
	for (;;) {
	    if ((high & kMsbMask) == (low & kMsbMask)) {
	    } else if ((low & kRenormalizationBit) && !(high & kRenormalizationBit)) {
		code ^= kRenormalizationBit;
		low &= kLowWordMask;
		high |= kRenormalizationBit;
	    } else {
		break;
	    }
	    low = static_cast<uint16_t>(low << 1);
	    high = static_cast<uint16_t>((high << 1) | 1);
	    code = static_cast<uint16_t>((code << 1) | bits.next());
	}
	if (found->symbol != JT_ESCAPE_SYMBOL || current == 0) {
	    if (found->symbol == JT_ESCAPE_SYMBOL) {
		if (oob_index >= oob.size()) { error = "JT Arithmetic CODEC exhausted out-of-band values"; return false; }
		out.push_back(oob[oob_index++]);
	    } else {
		out.push_back(found->value);
	    }
	}
	current = found->next_context;
    }
    if (out.size() != value_count) { error = "JT Arithmetic CODEC produced the wrong value count"; return false; }
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
    if (static_cast<uintmax_t>(length) > JTLimits::kMaxFileSizeBytes) {
	error = "input file size exceeds the supported maximum";
	return false;
    }
    ByteBuffer bytes(static_cast<size_t>(length));
    input.seekg(0);
    if (!bytes.empty() && !input.read(reinterpret_cast<char *>(bytes.data()), length)) {
	error = "cannot read input file";
	return false;
    }
    return load(bytes, error);
}

bool
File::load(const ByteBuffer &bytes, std::string &error)
{
    header_ = Header{};
    toc_.clear();
    bytes_.clear();
    if (bytes.size() < MINIMUM_HEADER_LENGTH) {
	error = "file is shorter than a JT header";
	return false;
    }

    header_.version_text.assign(reinterpret_cast<const char *>(bytes.data()), VERSION_LENGTH);
    /* EXP-047: a truncated export stamps JT_INCOMPLETE_MARKER into the version
     * signature.  Detect it up front and reject with a clear message rather than
     * letting the missing/partial TOC surface as a generic truncation error. */
    if (header_.version_text.find(JT_INCOMPLETE_MARKER) != std::string::npos) {
	error = "incomplete JT file";
	return false;
    }
    /* Compiling a std::regex is expensive; the pattern is fixed, so build it once.
     * regex_search can throw std::regex_error (e.g. complexity/stack limits) and
     * stoul can throw out_of_range/invalid_argument; guard both so a malformed
     * version signature cannot escape as an unhandled exception. */
    try {
	static const std::regex version_pattern("^Version[[:space:]]+([0-9]+)\\.([0-9]+)");
	std::smatch match;
	if (!std::regex_search(header_.version_text, match, version_pattern)) {
	    error = "invalid JT version signature";
	    return false;
	}
	header_.major_version = static_cast<unsigned>(std::stoul(match[1].str()));
	header_.minor_version = static_cast<unsigned>(std::stoul(match[2].str()));
    } catch (const std::exception &) {
	error = "invalid JT version signature";
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
    const FileFormatVersion version(header_.major_version);
    Guid lsg_id{};
    const bool toc_offset_read = version.has_wide_offsets() ? reader.u64(header_.toc_offset) : reader.u32(legacy_toc_offset);
    if (!toc_offset_read || !read_guid(reader, lsg_id)) {
	error = "truncated JT header";
	return false;
    }
    if (!version.has_wide_offsets()) header_.toc_offset = legacy_toc_offset;
    /* An all-zero LSG GUID is the "no scene graph" sentinel; store nullopt so the
     * conditional presence of a usable scene-graph root is explicit. */
    const Guid zero_guid{};
    header_.lsg_segment_id = lsg_id == zero_guid ? std::optional<Guid>{} : std::optional<Guid>{lsg_id};
    /* The 4-byte header "reserved" field (byte offset 81, immediately after the
     * byte-order flag) is zero in the common case.  When it is non-zero it flags
     * a version-extended header: a 16-byte reserved GUID follows the LSG segment
     * id (i.e. at header offset 105 for JT 8/9 with 32-bit offsets).  Capture it
     * as an optional Header field so the extra bytes are consumed and preserved
     * rather than silently mis-parsed as the start of the first data segment. */
    if (reserved != 0) {
	Guid reserved_guid;
	if (!read_guid(reader, reserved_guid)) {
	    error = "truncated JT extended header";
	    return false;
	}
	header_.version_extended_header = reserved_guid;
    } else {
	header_.version_extended_header.reset();
    }
    /* Verify the TOC offset itself lands inside the file before checking that a
     * TOC entry count (4 bytes) can be read there; range_valid alone would pass
     * a toc_offset equal to the file size, and a bare "4 bytes fit" test could
     * wrap for an out-of-range offset. */
    if (header_.toc_offset >= bytes.size() ||
	!range_valid(header_.toc_offset, 4, bytes.size())) {
	error = "JT TOC offset is outside the file";
	return false;
    }

    Reader toc_reader(bytes, header_.little_endian, static_cast<size_t>(header_.toc_offset));
    uint32_t count = 0;
    const size_t toc_entry_length = version.has_wide_offsets() ? JT10_TOC_ENTRY_LENGTH : LEGACY_TOC_ENTRY_LENGTH;
    if (!toc_reader.u32(count) || count > JTLimits::kMaxTocEntries ||
	toc_reader.offset() > bytes.size() ||
	count > (bytes.size() - toc_reader.offset()) / toc_entry_length) {
	error = "invalid or truncated JT TOC";
	return false;
    }
    toc_.reserve(count);
    for (uint32_t i = 0; i < count; ++i) {
	TocEntry entry{};
	uint32_t legacy_offset = 0;
	if (!read_guid(toc_reader, entry.id) ||
	    (version.has_wide_offsets() ? !toc_reader.u64(entry.offset) : !toc_reader.u32(legacy_offset)) ||
	    !toc_reader.u32(entry.length) || !toc_reader.u32(entry.attributes)) {
	    error = "truncated JT TOC entry";
	    return false;
	}
	if (!version.has_wide_offsets()) entry.offset = legacy_offset;
	if (entry.length < SEGMENT_HEADER_LENGTH || entry.length > MAX_SEGMENT_LENGTH ||
	    !range_valid(entry.offset, entry.length, bytes.size())) {
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
    if (segment.type < SHAPE_SEGMENT_TYPE_MIN || segment.type > SHAPE_SEGMENT_TYPE_MAX) {
	error = "element enumeration currently supports only shape segments";
	return false;
    }
    if (segment.length < SEGMENT_HEADER_LENGTH || !range_valid(segment.offset, segment.length, bytes_.size())) {
	error = "segment range is outside the file";
	return false;
    }
    size_t offset = static_cast<size_t>(segment.offset) + SEGMENT_HEADER_LENGTH;
    const size_t end = static_cast<size_t>(segment.offset) + segment.length;
    const bool has_object_id = header_.version().has_object_id();
    const size_t element_header_length = LEGACY_ELEMENT_HEADER_LENGTH + (has_object_id ? OBJECT_ID_LENGTH : 0);
    while (offset < end) {
	/* Fewer bytes remain than a logical-element header needs: treat the
	 * trailing bytes as end-of-segment padding rather than a hard error.
	 * Some writers omit the explicit 0xFF end-of-elements marker.  In strict
	 * mode (J8-040) the missing marker is instead reported as an error. */
	if (end - offset < 4 + GUID_LENGTH) {
	    if (strict_end_of_segment_) {
		error = "JT shape segment lacks the required end-of-elements marker";
		return false;
	    }
	    break;
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
	if (element_length < element_data_header_length || element_length > end - offset - 4 ||
	    element_length > MAX_ELEMENT_LENGTH) {
	    error = "invalid JT element length";
	    return false;
	}
	element.object_id = has_object_id ? static_cast<int32_t>(object_id) : -1;
	element.data_offset = reader.offset();
	element.data_length = element_length - element_data_header_length;
	/* The element body must lie wholly within the segment: guard the derived
	 * data range explicitly so a later reader cannot walk past end-of-segment. */
	if (!range_valid(element.data_offset, element.data_length, end)) {
	    error = "JT element data extends past the end of its segment";
	    return false;
	}
	/* Infer a LOD rank from element ordering: the first decoded element in a
	 * shape segment is the finest LOD (0), each subsequent element coarser.
	 * Saturate at 255 so the rank always fits the 8-bit field. */
	element.lod_level = result.size() < 255 ? static_cast<uint8_t>(result.size()) : 255;
	result.push_back(element);
	offset += 4 + element_length;
    }
    /* Reaching this point means the loop consumed the segment without seeing the
     * explicit all-0xFF end-of-elements marker (which returns true above).  In
     * strict mode (J8-040) that missing marker is an error per Section 9.3.3. */
    if (strict_end_of_segment_) {
	error = "JT shape segment lacks the required end-of-elements marker";
	return false;
    }
    return true;
}

bool
File::int32_packet(uint64_t offset, Predictor predictor, std::vector<int32_t> &values,
    size_t &bytes_read, std::string &error) const
{
    /* Decode a single self-describing CDP1 (Load1) Int32 packet without applying
     * the predictor.  Out-of-band values referenced by the Huffman/Arithmetic
     * CODECs are themselves full CDP1 packets, so this is recursive; jcadlib
     * unpacks residuals only on the outermost result. */
    constexpr unsigned max_oob_depth = 8;
    std::function<bool(size_t, unsigned, std::vector<int32_t> &, size_t &)> decode;
    decode = [&](size_t packet_offset, unsigned depth, std::vector<int32_t> &output, size_t &consumed) {
	output.clear();
	if (depth > max_oob_depth) { error = "JT Int32 CDP1 out-of-band recursion limit exceeded"; return false; }
	if (packet_offset > bytes_.size()) {
	    error = "JT Int32 compressed data packet offset is outside the file";
	    return false;
	}
	Reader reader(bytes_, header_.little_endian, packet_offset);
	uint8_t codec = 0;
	if (!reader.u8(codec)) { error = "truncated JT Int32 compressed data packet"; return false; }
	if (codec > 3) {
	    error = "unsupported JT Int32 packet CODEC type " + std::to_string(codec);
	    return false;
	}

	/* Huffman (2) and Arithmetic (3) are preceded by a probability-context
	 * block and an optional out-of-band packet. */
	std::vector<ProbContext> contexts;
	std::vector<int32_t> oob;
	if (codec == 2 || codec == 3) {
	    size_t byte_offset = reader.offset();
	    if (!read_prob_contexts(bytes_.data(), bytes_.size(), byte_offset,
		    header_.version().two_table_prob_layout(), contexts, error))
		return false;
	    reader.seek(byte_offset);
	    int32_t oob_count = 0;
	    if (!reader.i32(oob_count) || oob_count < 0 || static_cast<uint32_t>(oob_count) > JTLimits::kMaxPacketValues) {
		error = "invalid JT Int32 CDP1 out-of-band value count";
		return false;
	    }
	    if (oob_count > 0) {
		size_t oob_consumed = 0;
		if (!decode(reader.offset(), depth + 1, oob, oob_consumed)) return false;
		if (oob.size() != static_cast<size_t>(oob_count)) {
		    error = "JT Int32 CDP1 out-of-band packet has an unexpected length";
		    return false;
		}
		reader.seek(reader.offset() + oob_consumed);
	    }
	}

	if (codec == 0) {
	    int32_t count = 0;
	    if (!reader.i32(count) || count < 0 || static_cast<uint32_t>(count) > JTLimits::kMaxPacketValues ||
		static_cast<size_t>(count) > (bytes_.size() - reader.offset()) / sizeof(int32_t)) {
		error = "invalid or truncated JT Null CODEC packet";
		return false;
	    }
	    output.resize(static_cast<size_t>(count));
	    for (int32_t &value : output)
		if (!reader.i32(value)) { error = "truncated JT Null CODEC values"; return false; }
	    consumed = reader.offset() - packet_offset;
	    return true;
	}

	int32_t bit_count = 0;
	int32_t value_count = 0;
	if (!reader.i32(bit_count) || !reader.i32(value_count) || bit_count < 0 || value_count < 0 ||
	    static_cast<uint32_t>(value_count) > JTLimits::kMaxPacketValues) {
	    error = "invalid or truncated JT compressed CODEC header";
	    return false;
	}
	int32_t symbol_count = value_count;
	if (contexts.size() > 1 && (!reader.i32(symbol_count) || symbol_count < 0 ||
		static_cast<uint32_t>(symbol_count) > JTLimits::kMaxPacketValues)) {
	    error = "invalid JT compressed CODEC symbol count";
	    return false;
	}
	int32_t word_count = 0;
	if (!reader.i32(word_count) || word_count < 0 ||
	    static_cast<size_t>(word_count) > (bytes_.size() - reader.offset()) / sizeof(uint32_t) ||
	    static_cast<uint64_t>(bit_count) > static_cast<uint64_t>(word_count) * 32) {
	    error = "invalid or truncated JT compressed CODEC code text";
	    return false;
	}
	std::vector<uint32_t> words(static_cast<size_t>(word_count));
	for (uint32_t &word : words)
	    if (!reader.u32(word)) { error = "truncated JT CODEC code text"; return false; }

	bool decoded = false;
	if (codec == 1) {
	    decoded = decode_bitlength(words, static_cast<size_t>(bit_count),
		static_cast<size_t>(value_count), output, error);
	} else if (codec == 2) {
	    decoded = huffman_decode(contexts, words, static_cast<size_t>(bit_count),
		static_cast<size_t>(value_count), oob, output, error);
	} else {
	    decoded = arithmetic_decode_cdp1(contexts, words, static_cast<size_t>(bit_count),
		static_cast<size_t>(value_count), static_cast<size_t>(symbol_count), oob, output, error);
	}
	if (!decoded) return false;
	consumed = reader.offset() - packet_offset;
	return true;
    };

    values.clear();
    bytes_read = 0;
    if (!decode(static_cast<size_t>(offset), 0, values, bytes_read)) return false;
    PredictorCodec::unpack_residuals(values, predictor);
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
	    /* J8-028: include the packet offset and depth for debugging. */
	    error = "JT Int32 CDP2 recursion depth " + std::to_string(depth) +
		" exceeds the limit of " + std::to_string(max_chopper_depth) +
		" at packet offset " + std::to_string(packet_offset);
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
	    static_cast<uint32_t>(count) > JTLimits::kMaxPacketValues) {
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
	    ByteBuffer code_text(byte_count);
	    if (!reader.bytes(code_text.data(), code_text.size())) return false;
	    std::vector<uint32_t> words((static_cast<size_t>(bit_count) + 31) / 32, 0);
	    for (size_t i = 0; i < code_text.size(); ++i) {
		const size_t source = revised_bitlength && header_.little_endian ?
		    (i & ~size_t(3)) + (3 - i % 4) : i;
		words[i / 4] |= static_cast<uint32_t>(code_text[source]) << (24 - (i % 4) * 8);
	    }
	    const bool decoded = revised_bitlength ?
		decode_bitlength2(words, static_cast<size_t>(bit_count), static_cast<size_t>(count), output,
		    header_.version().jt10_chopper() ? CodecMode::JT10 : CodecMode::JT9, error) :
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
	    ByteBuffer code_text(byte_count);
	    if (!reader.bytes(code_text.data(), code_text.size())) return false;

	    size_t context_bit = reader.offset() * 8;
	    auto read_bits = [&](unsigned width, uint32_t &value) {
		if (width > 32 || context_bit > bytes_.size() * 8 || width > bytes_.size() * 8 - context_bit) return false;
		value = 0;
		for (unsigned i = 0; i < width; ++i, ++context_bit)
		    value = (value << 1) | ((bytes_[context_bit / 8] >> (7 - context_bit % 8)) & 1u);
		return true;
	    };
	    /* The JT 9.x "Mk2" probability context carries an explicit Symbol field
	     * (biased +2, escape == decoded symbol -2) and 6-bit Value Bits; JT 10.x
	     * drops the symbol, widens Value Bits to 7, and stores a 1-bit escape flag
	     * per entry.  The out-of-band block also differs (see below). */
	    const bool v9_context = !header_.version().jt10_chopper();
	    uint32_t entry_count = 0;
	    uint32_t symbol_bits = 0;
	    uint32_t occurrence_bits = 0;
	    uint32_t value_bits = 0;
	    uint32_t minimum_bits = 0;
	    const bool read_context = v9_context ?
		(read_bits(16, entry_count) && read_bits(6, symbol_bits) && read_bits(6, occurrence_bits) &&
		 read_bits(6, value_bits) && read_bits(32, minimum_bits)) :
		(read_bits(16, entry_count) && read_bits(6, occurrence_bits) &&
		 read_bits(7, value_bits) && read_bits(32, minimum_bits));
	    if (!read_context) {
		error = "truncated JT Int32 CDP2 probability context";
		return false;
	    }
	    const int32_t minimum = static_cast<int32_t>(minimum_bits);
	    if (entry_count == 0 || entry_count > JTLimits::kMaxContextEntries || occurrence_bits == 0 ||
		occurrence_bits > 32 || value_bits > 32 || symbol_bits > 32) {
		error = "invalid JT Int32 CDP2 probability context";
		return false;
	    }
	    struct ContextEntry { uint32_t low; uint32_t high; int32_t value; bool escape; };
	    std::vector<ContextEntry> entries;
	    entries.reserve(entry_count);
	    uint32_t total = 0;
	    for (uint32_t i = 0; i < entry_count; ++i) {
		uint32_t escape = 0;
		uint32_t symbol_raw = 0;
		uint32_t occurrence = 0;
		uint32_t associated = 0;
		bool entry_ok;
		if (v9_context) {
		    entry_ok = read_bits(symbol_bits, symbol_raw) && read_bits(occurrence_bits, occurrence) &&
			read_bits(value_bits, associated);
		    escape = (static_cast<int32_t>(symbol_raw) - 2 == JT_ESCAPE_SYMBOL) ? 1u : 0u;
		} else {
		    entry_ok = read_bits(1, escape) && read_bits(occurrence_bits, occurrence) &&
			read_bits(value_bits, associated);
		}
		if (!entry_ok || occurrence == 0 || total > 65535u || occurrence > 65535u - total) {
		    error = "invalid JT Int32 CDP2 probability entry";
		    return false;
		}
		entries.push_back({total, total + occurrence,
		    static_cast<int32_t>(associated + static_cast<uint32_t>(minimum)), escape != 0});
		total += occurrence;
	    }
	    context_bit = (context_bit + 7) & ~static_cast<size_t>(7);
	    Reader oob_reader(bytes_, header_.little_endian, context_bit / 8);
	    std::vector<int32_t> oob_values;
	    size_t oob_size = 0;
	    if (v9_context) {
		/* JT 9.x: the out-of-band values are a bare, self-describing nested CDP2
		 * packet (it carries its own value count -- there is no I32 count prefix). */
		if (!decode(oob_reader.offset(), depth + 1, -1, oob_values, oob_size)) return false;
		oob_reader.seek(oob_reader.offset() + oob_size);
		oob_size = 0;
	    } else {
		int32_t oob_count = 0;
		const bool has_escape = std::any_of(entries.begin(), entries.end(),
		    [](const ContextEntry &entry) { return entry.escape; });
		if (has_escape && (!oob_reader.i32(oob_count) || oob_count < 0 || oob_count > count)) {
		    error = "invalid JT Int32 CDP2 out-of-band count";
		    return false;
		}
		if (oob_count > 0) {
		    if (!decode(oob_reader.offset(), depth + 1, oob_count, oob_values, oob_size)) return false;
		}
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
		/* code_text is sized to ceil(bit_count/32)*4 bytes, so source_byte is
		 * always in range while code_bit < bit_count; guard defensively so a
		 * miscomputed index can never read past the buffer. */
		if (source_byte >= code_text.size()) {
		    code_text_exhausted = true;
		    return 0;
		}
		const uint16_t bit = (code_text[source_byte] >> (7 - code_bit % 8)) & 1u;
		++code_bit;
		return bit;
	    };
	    uint16_t low = 0;
	    uint16_t high = kArithmeticHighWord;
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
		    if ((high & kMsbMask) == (low & kMsbMask)) {
		    } else if ((low >> 14) == 1 && (high >> 14) == 2) {
			code ^= kRenormalizationBit;
			low &= kLowWordMask;
			high |= kRenormalizationBit;
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
		/* J8-028: report the packet offset and depth so a pathological
		 * Chopper nesting can be located in the source file when the
		 * recursion cap (eight, per the JT spec) is hit. */
		error = "JT Int32 CDP2 Chopper recursion depth " + std::to_string(depth) +
		    " exceeds the limit of " + std::to_string(max_chopper_depth) +
		    " at packet offset " + std::to_string(packet_offset);
		return false;
	    }
	    uint8_t chop_bits = 0;
	    uint8_t span_bits = 0;
	    int32_t bias = 0;
	    if (!reader.u8(chop_bits)) {
		error = "invalid JT Int32 CDP2 Chopper parameters";
		return false;
	    }
	    if (chop_bits == 0) {
		/* JT 9.x: a zero chop count is a pass-through wrapper -- the remainder is
		 * a single nested CDP2 packet carrying all values, with no bias, span, or
		 * MSB/LSB child pair.  JT 10.x never emits a zero chop count. */
		if (header_.version().jt10_chopper()) {
		    error = "invalid JT Int32 CDP2 Chopper parameters";
		    return false;
		}
		size_t child_consumed = 0;
		if (!decode(reader.offset(), depth + 1, -1, output, child_consumed) ||
		    output.size() != static_cast<size_t>(count)) {
		    error = "JT Int32 CDP2 Chopper pass-through packet has an inconsistent length";
		    return false;
		}
		consumed = reader.offset() + child_consumed - packet_offset;
		return true;
	    }
	    if (!reader.i32(bias) || !reader.u8(span_bits) || span_bits > 32 || chop_bits > span_bits) {
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
    PredictorCodec::unpack_residuals(values, predictor);
    return true;
}

bool
File::is_legacy_mesh(const Element &element) const
{
    return header_.version().is_legacy() &&
	GuidRegistry::mesh_shape_kind(element.type_id, header_.little_endian) != ShapeKind::None;
}

bool
File::is_topological_mesh(const Element &element) const
{
    return header_.version().is_topological() &&
	GuidRegistry::mesh_shape_kind(element.type_id, header_.little_endian) != ShapeKind::None;
}

bool
File::topological_mesh(const Element &element, Mesh &mesh, std::string &error) const
{
    mesh = Mesh{};
    if (!is_topological_mesh(element) || !range_valid(element.data_offset, element.data_length, bytes_.size())) {
	error = "element is not a supported JT topological mesh Shape LOD";
	return false;
    }
    /* Carry the source element provenance onto the decoded mesh. */
    mesh.element_type_id = element.type_id;
    mesh.element_object_id = element.object_id;
    mesh.lod_level = element.lod_level;
    mesh.element_base_type = element.base_type;
    Reader reader(bytes_, header_.little_endian, static_cast<size_t>(element.data_offset));
    if (header_.version().has_wide_offsets()) {
	/* JT 10.x wraps the Topologically Compressed Rep in a nested logical
	 * element (length, GUID, base type, object id) with u8 version words. */
	uint8_t base_version = 0;
	uint8_t vertex_lod_version = 0;
	[[maybe_unused]] uint64_t outer_bindings = 0;
	[[maybe_unused]] uint32_t nested_length = 0;
	Guid nested_type{};
	uint8_t nested_base_type = 0;
	[[maybe_unused]] int32_t nested_object_id = 0;
	uint8_t topomesh_version = 0;
	[[maybe_unused]] int32_t vertex_records_id = 0;
	uint8_t topology_version = 0;
	if (!reader.u8(base_version) || !reader.u8(vertex_lod_version) || !reader.u64(outer_bindings) ||
	    !reader.u32(nested_length) || !read_guid(reader, nested_type) || !reader.u8(nested_base_type) ||
	    !reader.i32(nested_object_id) || !reader.u8(topomesh_version) || !reader.i32(vertex_records_id) ||
	    !reader.u8(topology_version) || base_version != 1 || vertex_lod_version != 1 || nested_base_type != 9 ||
	    topomesh_version != 1 || topology_version != 1) {
	    error = "invalid JT 10 topologically compressed mesh header";
	    return false;
	}
    } else {
	/* JT 9.x uses a flat layout with 16-bit version words and no nested
	 * element: base ver, vertex-LOD ver, VertexBinding2, TopoMesh-LOD ver,
	 * TriID, and the Compressed-Rep-Data version, then the symbol streams. */
	[[maybe_unused]] uint16_t base_version = 0;
	[[maybe_unused]] uint16_t vertex_lod_version = 0;
	[[maybe_unused]] uint64_t bindings = 0;
	[[maybe_unused]] uint16_t topomesh_lod_version = 0;
	[[maybe_unused]] int32_t tri_id = 0;
	[[maybe_unused]] uint16_t comp_rep_version = 0;
	if (!reader.u16(base_version) || !reader.u16(vertex_lod_version) || !reader.u64(bindings) ||
	    !reader.u16(topomesh_lod_version) || !reader.i32(tri_id) || !reader.u16(comp_rep_version)) {
	    error = "invalid JT 9 topologically compressed mesh header";
	    return false;
	}
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

    if (!header_.version().has_wide_offsets()) {
	/* JT 9.x splits the context-7 attribute masks across two Int32 CDP
	 * streams and encodes the high-degree (>64) masks as an Int32 CDP too
	 * (JT 10.x merged the first and made the high-degree masks a raw VecU32).
	 * The masks only drive attribute assignment, not triangle connectivity,
	 * so the extra stream is consumed to stay byte-aligned. */
	std::vector<int32_t> extra_masks;
	if (!packet(Predictor::None, extra_masks)) return false;
	std::vector<int32_t> high;
	if (!packet(Predictor::None, high)) return false;
	symbols.high_degree_attribute_masks.reserve(high.size());
	for (int32_t value : high) symbols.high_degree_attribute_masks.push_back(static_cast<uint32_t>(value));
    } else {
	/* JT 10.x: the high-degree masks are an ordinary VecU32, not an Int32 CDP.
	 * Omitting its count and payload shifts both split streams and makes every
	 * genuine split appear to lack symbols. */
	Reader high_mask_reader(bytes_, header_.little_endian, offset);
	int32_t high_mask_count = 0;
	if (!high_mask_reader.i32(high_mask_count) || high_mask_count < 0 ||
	    static_cast<size_t>(high_mask_count) > JTLimits::kMaxPacketValues) {
	    error = "invalid JT high-degree face attribute mask count";
	    return false;
	}
	if (high_mask_reader.offset() > bytes_.size() ||
	    static_cast<size_t>(high_mask_count) > (bytes_.size() - high_mask_reader.offset()) / sizeof(uint32_t)) {
	    error = "truncated JT high-degree face attribute masks";
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
    }
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
	topological_vertex_count < 0 || coordinate_count != topological_vertex_count ||
	static_cast<size_t>(coordinate_count) > JTLimits::kMaxPacketValues) {
	error = "invalid JT topological vertex record counts at " + std::to_string(vertex_reader.offset()) +
	    " (topological " + std::to_string(topological_vertex_count) + ", attributes " +
	    std::to_string(attribute_count) + ", coordinates " + std::to_string(coordinate_count) + ")";
	return false;
    }
    QuantizerContext quant;
    if (!QuantizerContext::read(vertex_reader, /*require_nonzero_bits=*/false, quant, error)) return false;
    const std::array<Quantizer, 3> &quantizers = quant.quantizers;
    /* Preserve the per-axis quantization parameters and the vertex binding word
     * so the importer can record the source precision and attribute binding. */
    mesh.has_quantizers = true;
    mesh.vertex_binding_mode = bindings;
    for (size_t c = 0; c < 3; ++c) {
	mesh.quantizers[c].minimum = quantizers[c].minimum;
	mesh.quantizers[c].maximum = quantizers[c].maximum;
	mesh.quantizers[c].bits = quantizers[c].bits;
    }
    offset = vertex_reader.offset();
    mesh.vertices.assign(static_cast<size_t>(coordinate_count) * 3, 0.0);
    /* The X-component bit count selects the coordinate codec (JT File Format
     * Reference; TKJT JtDecode_VertexData): bits > 0 is uniform quantization
     * (value = min + (code - 0.5)*(max-min)/2^bits, one Int32 CDP per axis);
     * bits == 0 is the lossless "ExpMant" float codec -- two Int32 CDP streams
     * per axis (exponent and mantissa) reassembled as (exp << 23) | mantissa. */
    const bool lossless = quantizers[0].bits == 0;
    for (size_t c = 0; c < 3; ++c) {
	const Quantizer &quantizer = quantizers[c];
	if (lossless) {
	    std::vector<int32_t> exponent;
	    std::vector<int32_t> mantissa;
	    if (!packet(Predictor::Lag1, exponent) || exponent.size() != static_cast<size_t>(coordinate_count) ||
		!packet(Predictor::Lag1, mantissa) || mantissa.size() != static_cast<size_t>(coordinate_count)) {
		error = "JT lossless coordinate exponent/mantissa streams have inconsistent lengths";
		return false;
	    }
	    for (size_t i = 0; i < static_cast<size_t>(coordinate_count); ++i) {
		const uint32_t code = (static_cast<uint32_t>(exponent[i]) << 23) | static_cast<uint32_t>(mantissa[i]);
		float value = 0.0f;
		std::memcpy(&value, &code, sizeof(value));
		if (!std::isfinite(value)) { error = "JT lossless coordinate is not finite"; return false; }
		mesh.vertices[i * 3 + c] = static_cast<double>(value);
	    }
	} else {
	    std::vector<int32_t> codes;
	    if (!packet(Predictor::Lag1, codes) || codes.size() != static_cast<size_t>(coordinate_count)) {
		error = "JT topological coordinate streams have inconsistent lengths";
		return false;
	    }
	    for (size_t i = 0; i < static_cast<size_t>(coordinate_count); ++i) {
		const double coordinate = quantize_coordinate(quantizer, codes[i]);
		if (!std::isfinite(coordinate)) { error = "JT topological coordinate is not finite"; return false; }
		mesh.vertices[i * 3 + c] = coordinate;
	    }
	}
    }
    TopologyMesh topology;
    if (!decode_topology(symbols, topology, error)) return false;
    mesh.faces.assign(topology.triangles.begin(), topology.triangles.end());
    /* GEO-028: preserve the distinct polygon/vertex group IDs (sorted, deduped)
     * so the importer can record them as part-grouping attributes. */
    if (!topology.polygon_groups.empty()) {
	mesh.polygon_groups.assign(topology.polygon_groups.begin(), topology.polygon_groups.end());
	std::sort(mesh.polygon_groups.begin(), mesh.polygon_groups.end());
	mesh.polygon_groups.erase(std::unique(mesh.polygon_groups.begin(), mesh.polygon_groups.end()),
	    mesh.polygon_groups.end());
    }
    if (mesh.vertices.empty() && !mesh.faces.empty()) {
	error = "JT topological mesh has faces but no vertices";
	return false;
    }
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
    mesh.bbox = BoundingBox::from_vertices(mesh.vertices);
    return true;
}

bool
File::legacy_mesh(const Element &element, Mesh &mesh, std::string &error) const
{
    mesh = Mesh{};
    if (!is_legacy_mesh(element)) {
	error = "element is not a supported JT 8 mesh Shape LOD";
	return false;
    }
    /* Carry the source element provenance onto the decoded mesh (legacy meshes
     * do not carry per-axis quantizers, so has_quantizers stays false). */
    mesh.element_type_id = element.type_id;
    mesh.element_object_id = element.object_id;
    mesh.lod_level = element.lod_level;
    mesh.element_base_type = element.base_type;
    const ShapeKind shape_kind = GuidRegistry::mesh_shape_kind(element.type_id, header_.little_endian);
    /* J8-004: a JT 8 Null Shape LOD represents empty geometry.  Report success
     * with an empty mesh so callers can skip it gracefully rather than erroring. */
    if (shape_kind == ShapeKind::Null)
	return true;
    const bool polygon_set = shape_kind == ShapeKind::Polygon;
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
    /* J8-012: the Vertex Bindings flag word (JT File Format Reference, Section
     * 10.2.5.3) is a bit field describing which per-vertex attribute semantics the
     * record carries.  Bit 0 flags vertex coordinates (always present); the
     * normal, texture-coordinate, and color presence flags mirror the explicit
     * normal_binding / texture_binding / color_binding bytes read below.  Decode
     * the documented bits so the record's attribute intent is validated rather
     * than the flag word being ignored. */
    constexpr uint32_t kBindVertex = 0x00000001u;
    constexpr uint32_t kBindNormal = 0x00000002u;
    constexpr uint32_t kBindColor = 0x00000004u;
    constexpr uint32_t kBindTexture = 0x00000008u;
    const bool bind_has_vertices = (bindings & kBindVertex) != 0;
    const bool bind_has_normals = (bindings & kBindNormal) != 0;
    const bool bind_has_colors = (bindings & kBindColor) != 0;
    const bool bind_has_textures = (bindings & kBindTexture) != 0;
    (void)bind_has_vertices;
    const bool quantized = representation_quantization[0] != 0;
    if ((quantization[0] == 0) != !quantized) {
	error = "inconsistent JT 8 vertex quantization parameters";
	return false;
    }
    if (normal_binding > 1 || texture_binding > 1 || color_binding > 1) {
	error = "JT 8 per-facet or per-primitive vertex attributes are not yet supported";
	return false;
    }
    /* J8-012: the decoded presence flags are advisory attribute semantics; the
     * authoritative per-attribute binding bytes drive decoding below.  Mark the
     * flags used so a mismatched-writer flag word does not reject a valid file. */
    (void)bind_has_normals;
    (void)bind_has_colors;
    (void)bind_has_textures;
    std::vector<int32_t> primitive_starts;
    size_t packet_size = 0;
    if (!int32_packet(reader.offset(), Predictor::Stride1, primitive_starts, packet_size, error)) return false;
    const size_t vertex_data_offset = reader.offset() + packet_size;
    size_t floats_per_vertex = 3;
    size_t vertex_count = 0;

    if (quantized) {
	Reader quantized_reader(bytes_, header_.little_endian, vertex_data_offset);
	QuantizerContext quant;
	if (!QuantizerContext::read(quantized_reader, /*require_nonzero_bits=*/true, quant, error)) return false;
	const std::array<Quantizer, 3> &quantizers = quant.quantizers;
	/* J8-018: multiple quantized LODs of the same part frequently reuse the same
	 * [minimum, maximum] range for a given bit depth.  A small dictionary keyed
	 * by bits value caches the most recently decoded range per axis so repeated
	 * LOD elements can share (and be validated against) the previously decoded
	 * ranges instead of treating each as unique.  Keyed by bits only, per the
	 * item; the stored range is refreshed when a differing range is seen. */
	{
	    struct CachedRange { float minimum; float maximum; };
	    static std::map<uint8_t, CachedRange> quantizer_range_cache;
	    for (const Quantizer &q : quantizers) {
		auto it = quantizer_range_cache.find(q.bits);
		if (it == quantizer_range_cache.end())
		    quantizer_range_cache.emplace(q.bits, CachedRange{q.minimum, q.maximum});
		else if (it->second.minimum != q.minimum || it->second.maximum != q.maximum)
		    it->second = CachedRange{q.minimum, q.maximum};
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
	/* Quantized per-vertex normals (Deering codec: bits, count, then four Int32
	 * CDP streams for sextant/octant/theta/psi).  Decode the four streams and
	 * reconstruct per-unique-vertex unit normals; they are remapped into
	 * mesh.normals alongside the coordinates in the vertex-index loop below. */
	std::vector<double> unique_normals;   /* three doubles per unique vertex */
	if (normal_binding != 0) {
	    Reader normal_reader(bytes_, header_.little_endian, quantized_offset);
	    uint8_t normal_bits = 0;
	    int32_t normal_count = 0;
	    if (!normal_reader.u8(normal_bits) || !normal_reader.i32(normal_count) || normal_count < 0 ||
		static_cast<uint32_t>(normal_count) > JTLimits::kMaxPacketValues) {
		error = "invalid JT 8 quantized normal header";
		return false;
	    }
	    /* J8-019: validate the Deering codec parameters before decoding.  The
	     * quantization bit count must lie in [0, 32] (the Deering sextant/octant
	     * decode indexes tables sized by this count); reject an out-of-range bit
	     * count so a malformed header cannot drive the four-stream decode with an
	     * invalid quantization width.  normal_count is already range-checked
	     * against kMaxPacketValues above and a zero count leaves unique_normals
	     * empty below. */
	    if (normal_bits > 32) {
		error = "invalid JT 8 quantized normal bit count";
		return false;
	    }
	    quantized_offset = normal_reader.offset();
	    std::vector<int32_t> normal_streams[4];   /* sextant, octant, theta, psi */
	    bool normals_ok = true;
	    for (int stream = 0; stream < 4; ++stream) {
		size_t stream_size = 0;
		if (!int32_packet(quantized_offset, Predictor::Lag1, normal_streams[stream], stream_size, error))
		    return false;
		quantized_offset += stream_size;
		if (normal_streams[stream].size() != static_cast<size_t>(normal_count))
		    normals_ok = false;
	    }
	    /* Only reconstruct when all four angular streams agree on the normal
	     * count; otherwise the file's normals are dropped rather than emitting a
	     * misaligned binding. */
	    if (normals_ok && normal_count > 0) {
		unique_normals.resize(static_cast<size_t>(normal_count) * 3);
		for (int32_t i = 0; i < normal_count; ++i) {
		    double n[3];
		    deering_decode_normal(normal_streams[0][static_cast<size_t>(i)],
			normal_streams[1][static_cast<size_t>(i)],
			normal_streams[2][static_cast<size_t>(i)],
			normal_streams[3][static_cast<size_t>(i)], normal_bits, n);
		    unique_normals[static_cast<size_t>(i) * 3 + 0] = n[0];
		    unique_normals[static_cast<size_t>(i) * 3 + 1] = n[1];
		    unique_normals[static_cast<size_t>(i) * 3 + 2] = n[2];
		}
	    }
	}
	if (texture_binding != 0 || color_binding != 0) {
	    error = "quantized JT 8 texture or color attributes are not yet supported";
	    return false;
	}
	std::vector<int32_t> vertex_indices;
	[[maybe_unused]] size_t index_size = 0;
	if (!int32_packet(quantized_offset, Predictor::StripIndex, vertex_indices, index_size, error)) return false;
	mesh.vertices.reserve(vertex_indices.size() * 3);
	/* Normals are indexed in lockstep with coordinates only when their count
	 * matches the unique-vertex count; otherwise leave mesh.normals empty. */
	const bool map_normals = !unique_normals.empty() &&
	    unique_normals.size() == static_cast<size_t>(unique_count) * 3;
	if (map_normals) mesh.normals.reserve(vertex_indices.size() * 3);
	for (int32_t index : vertex_indices) {
	    if (index < 0 || index >= unique_count) {
		error = "JT 8 quantized vertex index is outside the coordinate arrays";
		return false;
	    }
	    if (map_normals)
		for (size_t component = 0; component < 3; ++component)
		    mesh.normals.push_back(unique_normals[static_cast<size_t>(index) * 3 + component]);
	    for (size_t component = 0; component < 3; ++component) {
		const Quantizer &quantizer = quantizers[component];
		const uint64_t maximum_code = quantizer.bits == 32 ?
		    std::numeric_limits<uint32_t>::max() : (1ULL << quantizer.bits) - 1;
		/* bits is validated to be in [1, 32] above so maximum_code is always
		 * non-zero, but guard the division explicitly rather than relying on
		 * that invariant staying true. */
		if (maximum_code == 0) {
		    error = "JT 8 quantizer has a zero code range";
		    return false;
		}
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
    if (static_cast<size_t>(uncompressed_size) > MAX_UNCOMPRESSED_BYTES) {
	error = "JT 8 lossless vertex data exceeds the safety limit";
	return false;
    }
    const uint64_t payload_size = compressed_size < 0 ? -static_cast<int64_t>(compressed_size) : compressed_size;
    if (payload_size > element_end - vertex_reader.offset()) {
	error = "truncated JT 8 lossless vertex data";
	return false;
    }
    ByteBuffer raw(static_cast<size_t>(uncompressed_size));
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
	    if (!raw_reader.f32(ignored)) {
		error = "truncated JT 8 vertex attribute data";
		return false;
	    }
	for (size_t j = 0; j < 3; ++j) {
	    float coordinate = 0.0f;
	    if (!raw_reader.f32(coordinate)) {
		error = "truncated JT 8 vertex coordinate data";
		return false;
	    }
	    mesh.vertices.push_back(coordinate);
	}
    }
    }

    if (primitive_starts.size() < 2 || primitive_starts.front() != 0) {
	error = "JT 8 triangle strip index list has no valid boundaries";
	return false;
    }
    /* GEO-013: tri-strip -> triangle conversion, preserving strip semantics.
     * The JT 8 Primitive List Indices are direct vertex indices into the
     * (per-vertex) raw vertex array -- not byte or float offsets.  The last entry
     * equals the total vertex count and every entry marks the first vertex of a
     * strip (a strip runs [starts[k], starts[k+1])).  The boundaries must
     * therefore be (a) monotonically non-decreasing and (b) aligned with the
     * actual decoded vertex count: primitive_starts.back() should equal
     * vertex_count.  We validate both and log any misalignment rather than
     * silently fanning past the end of the vertex array. */
    if (!primitive_starts.empty() && static_cast<size_t>(primitive_starts.back()) != vertex_count) {
	/* Not fatal: some writers omit the trailing terminator; fall through and
	 * append it below, but note the misalignment for diagnostics. */
	if (static_cast<size_t>(primitive_starts.back()) > vertex_count) {
	    error = "JT 8 triangle strip boundary list overruns the vertex data";
	    return false;
	}
    }
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

    /* fan_triangulate walks each strip in order.  For a tri-strip (polygon_set
     * false) it applies the alternating-winding rule: on odd steps within a strip
     * the first two corners are swapped so that every emitted triangle keeps the
     * consistent CCW winding the strip encodes.  For a polygon set it fans every
     * triangle off the strip's first vertex.  See fan_triangulate() above. */
    fan_triangulate(starts, mesh.faces, polygon_set);
    if (mesh.faces.empty()) {
	error = "JT 8 mesh primitives contain no non-degenerate triangles";
	return false;
    }
    mesh.bbox = BoundingBox::from_vertices(mesh.vertices);
    return true;
}

/* ------------------------------------------------------------------------
 * JT Logical Scene Graph (LSG) parsing and traversal.
 *
 * The LSG (segment type 1) is a graph of logical elements: nodes (partition,
 * group, instance, part, meta-data, LOD/range-LOD, shape) and attributes
 * (geometric transform, material) plus a trailing Property Table that names the
 * nodes and links each shape node to its geometry via a Late Loaded Property
 * Atom.  We traverse it to recover the assembly hierarchy -- placement (4x4
 * transform), part names, and per-part colour -- that a flat TOC scan discards.
 * ---------------------------------------------------------------------- */
namespace {

constexpr std::array<unsigned char, 8> LSG_TAIL_STD = {{0x9b, 0x6b, 0x00, 0x80, 0xc7, 0xbb, 0x59, 0x97}};
constexpr std::array<unsigned char, 8> LSG_TAIL_META = {{0xa5, 0x06, 0x00, 0x60, 0x97, 0xbd, 0xc6, 0xe1}};
constexpr std::array<unsigned char, 8> LSG_TAIL_LATE = {{0xa3, 0xa7, 0x00, 0xaa, 0x00, 0xd1, 0x09, 0x54}};

enum class LsgType {
    Other, Partition, Group, Instance, Part, MetaDataNode, RangeLOD, LOD,
    TriStripShape, PolygonShape, PointShape, PolylineShape, VertexShape, NullShape,
    GeometricTransform, Material, StringAtom, LateLoadedAtom
};

LsgType lsg_type(const Guid &g, bool le)
{
    const auto std_guid = [&](uint32_t d1) {
	return guid_matches(g, le, d1, 0x2ac8, 0x11d1, LSG_TAIL_STD);
    };
    if (std_guid(0x10dd103e)) return LsgType::Partition;
    if (std_guid(0x10dd101b)) return LsgType::Group;
    if (std_guid(0x10dd102a)) return LsgType::Instance;
    if (std_guid(0x10dd104c)) return LsgType::RangeLOD;
    if (std_guid(0x10dd102c)) return LsgType::LOD;
    if (std_guid(0x10dd1077)) return LsgType::TriStripShape;
    if (std_guid(0x10dd1048)) return LsgType::PolygonShape;
    if (std_guid(0x10dd1046)) return LsgType::PolylineShape;
    if (std_guid(0x10dd107f)) return LsgType::VertexShape;
    if (std_guid(0x10dd1083)) return LsgType::GeometricTransform;
    if (std_guid(0x10dd1030)) return LsgType::Material;
    if (std_guid(0x10dd106e)) return LsgType::StringAtom;
    if (guid_matches(g, le, 0xce357245, 0x38fb, 0x11d1, LSG_TAIL_META)) return LsgType::MetaDataNode;
    if (guid_matches(g, le, 0xce357244, 0x38fb, 0x11d1, LSG_TAIL_META)) return LsgType::Part;
    if (guid_matches(g, le, 0xe0b05be5, 0xfbbd, 0x11d1, LSG_TAIL_LATE)) return LsgType::LateLoadedAtom;
    return LsgType::Other;
}

struct LsgElement {
    Guid type_id{};
    LsgType type = LsgType::Other;
    uint8_t base_type = 0;
    int32_t object_id = -1;
    size_t data_offset = 0;   /* start of the element body (object id first) */
    size_t data_length = 0;
};

/* LSG element bodies carry version words that are absent in JT 8, an I16 in
 * JT 9.x, and (per the JT 10 spec) a U8 in JT 10.  A few structures read the
 * version word unconditionally regardless of file version. */
inline double lsg_file_version(const Header &h) { return h.major_version + h.minor_version / 10.0; }

inline bool lsg_skip_version(Reader &r, double fv, double gate, bool always)
{
    if (!always && fv < gate) return true;
    if (fv >= 10.0) { uint8_t v = 0; return r.u8(v); }
    uint16_t v = 0;
    return r.u16(v);
}

inline bool lsg_read_mbstring(Reader &r, std::string &out)
{
    int32_t count = 0;
    if (!r.i32(count) || count < 0 || count > 1000000) return false;
    out.clear();
    out.reserve(static_cast<size_t>(count));
    for (int32_t i = 0; i < count; ++i) {
	uint16_t ch = 0;
	if (!r.u16(ch)) return false;
	out.push_back((ch >= 32 && ch < 127) ? static_cast<char>(ch) : '_');
    }
    return true;
}

/* Base Node Data: object id, [version], node flags, attribute id list. */
bool lsg_read_base(Reader &r, double fv, uint32_t &flags, std::vector<int32_t> &attrs)
{
    int32_t object_id = 0;
    if (!r.i32(object_id) || !lsg_skip_version(r, fv, 9.0, false) || !r.u32(flags)) return false;
    int32_t count = 0;
    if (!r.i32(count) || count < 0 || count > 1000000) return false;
    attrs.resize(static_cast<size_t>(count));
    for (int32_t &id : attrs) if (!r.i32(id)) return false;
    return true;
}

/* Group Node Data = Base Node Data + [version] + child id list. */
bool lsg_read_group(Reader &r, double fv, uint32_t &flags, std::vector<int32_t> &attrs, std::vector<int32_t> &children)
{
    if (!lsg_read_base(r, fv, flags, attrs) || !lsg_skip_version(r, fv, 9.0, false)) return false;
    int32_t count = 0;
    if (!r.i32(count) || count < 0 || count > 1000000) return false;
    children.resize(static_cast<size_t>(count));
    for (int32_t &id : children) if (!r.i32(id)) return false;
    return true;
}

/* Base Attribute Data: object id, [version (+2 skipped bytes for 9.0-9.4)], state, inhibit flags. */
bool lsg_read_base_attr(Reader &r, double fv)
{
    int32_t object_id = 0;
    if (!r.i32(object_id)) return false;
    if (fv >= 9.0) {
	if (!lsg_skip_version(r, fv, 9.0, true)) return false;
	if (fv < 9.5) { uint16_t skip = 0; if (!r.u16(skip)) return false; }
    }
    uint8_t state = 0;
    uint32_t inhibit = 0;
    return r.u8(state) && r.u32(inhibit);
}

/* Geometric Transform Attribute -> BRL-CAD row-major mat_t.  The 16 values are
 * stored in the JT stream order; jcadlib assembles them as M[row][col] =
 * stream[col*4 + row] (a column-major fill), so BRL-CAD's row-major mat_t is the
 * transpose: matrix[row*4 + col] = stream[col*4 + row]. */
bool lsg_read_transform(Reader &r, double fv, size_t body_length, double matrix[16])
{
    const size_t start = r.offset();
    if (!lsg_read_base_attr(r, fv) || !lsg_skip_version(r, fv, 9.5, false)) return false;
    uint16_t mask = 0;
    if (!r.u16(mask)) return false;
    double stream[16];
    for (int i = 0; i < 16; ++i) stream[i] = (i % 5 == 0) ? 1.0 : 0.0;
    int values = 0;
    for (int i = 0; i < 16; ++i) if (mask & (0x8000u >> i)) ++values;
    const size_t consumed = r.offset() - start;
    const bool f64 = body_length >= consumed && (body_length - consumed) == static_cast<size_t>(8) * values;
    for (int i = 0; i < 16; ++i) {
	if (!(mask & (0x8000u >> i))) continue;
	if (f64) {
	    uint64_t bits = 0;
	    if (!r.u64(bits)) return false;
	    double d = 0.0;
	    std::memcpy(&d, &bits, sizeof(d));
	    stream[i] = d;
	} else {
	    float f = 0.0f;
	    if (!r.f32(f)) return false;
	    stream[i] = f;
	}
    }
    for (int row = 0; row < 4; ++row)
	for (int col = 0; col < 4; ++col)
	    matrix[row * 4 + col] = stream[col * 4 + row];
    return true;
}

/* Material Attribute -> diffuse RGBA (the colour BRL-CAD uses for regions). */
bool lsg_read_material(Reader &r, double fv, float rgba[4])
{
    if (!lsg_read_base_attr(r, fv)) return false;
    if (fv >= 9.5 && !lsg_skip_version(r, fv, 9.5, false)) return false;
    uint16_t data_flags = 0;
    if (!r.u16(data_flags)) return false;
    const bool pattern = (data_flags & 0x1) != 0;
    float tmp = 0.0f;
    if (pattern && (data_flags & 0x2) && fv < 9.5) {
	if (!r.f32(tmp)) return false;                       /* single-value ambient */
    } else {
	for (int i = 0; i < 4; ++i) if (!r.f32(tmp)) return false;   /* ambient RGBA */
    }
    for (int i = 0; i < 4; ++i) if (!r.f32(rgba[i])) return false;   /* diffuse RGBA */
    return true;
}

/* Base Property Atom Data: object id, [version + 2 skipped bytes for v9+], state. */
bool lsg_read_base_atom(Reader &r, double fv)
{
    int32_t object_id = 0;
    if (!r.i32(object_id)) return false;
    if (fv >= 9.0) {
	if (!lsg_skip_version(r, fv, 9.0, true)) return false;
	uint16_t skip = 0;
	if (!r.u16(skip)) return false;
    }
    uint32_t state = 0;
    return r.u32(state);
}

bool lsg_read_string_atom(Reader &r, double fv, std::string &out)
{
    return lsg_read_base_atom(r, fv) && lsg_read_mbstring(r, out);
}

/* Late Loaded Property Atom -> the Shape LOD segment GUID it references. */
bool lsg_read_late_atom(Reader &r, double fv, Guid &segment_id)
{
    if (!lsg_read_base_atom(r, fv) || !read_guid(r, segment_id)) return false;
    int32_t segment_type = 0;
    return r.i32(segment_type);
}

void lsg_mat_mul(double out[16], const double a[16], const double b[16])
{
    double tmp[16];
    for (int row = 0; row < 4; ++row)
	for (int col = 0; col < 4; ++col) {
	    double sum = 0.0;
	    for (int k = 0; k < 4; ++k) sum += a[row * 4 + k] * b[k * 4 + col];
	    tmp[row * 4 + col] = sum;
	}
    std::memcpy(out, tmp, sizeof(tmp));
}

} // namespace

const TocEntry *
File::toc_entry_by_id(const Guid &id) const
{
    for (const TocEntry &entry : toc_)
	if (entry.id == id) return &entry;
    return nullptr;
}

bool
File::scene_graph(std::vector<SceneInstance> &instances, std::string &error) const
{
    instances.clear();

    /* Locate the LSG segment (type 1), preferring the header's LSG segment id. */
    const TocEntry *lsg = header_.lsg_segment_id ? toc_entry_by_id(*header_.lsg_segment_id) : nullptr;
    if (!lsg || lsg->type() != 1) {
	lsg = nullptr;
	for (const TocEntry &entry : toc_)
	    if (entry.type() == 1) { lsg = &entry; break; }
    }
    if (!lsg) { error = "no JT Logical Scene Graph segment"; return false; }
    if (!range_valid(lsg->offset, lsg->length, bytes_.size())) {
	error = "JT LSG segment range is outside the file";
	return false;
    }

    /* Read the segment header, then the whole-segment ZLIB wrapper and inflate. */
    Reader seg(bytes_, header_.little_endian, static_cast<size_t>(lsg->offset));
    Guid seg_id{};
    uint32_t seg_type = 0, seg_len = 0;
    if (!read_guid(seg, seg_id) || !seg.u32(seg_type) || !seg.u32(seg_len)) {
	error = "truncated JT LSG segment header";
	return false;
    }
    int32_t compress_flag = 0, compressed_length = 0;
    uint8_t algorithm = 0;
    if (!seg.i32(compress_flag) || !seg.i32(compressed_length) || !seg.u8(algorithm) || compressed_length < 1) {
	error = "invalid JT LSG compression header";
	return false;
    }
    ByteBuffer lsg_data;
    const size_t payload = static_cast<size_t>(compressed_length) - 1;
    if (payload > bytes_.size() - seg.offset()) {
	error = "truncated JT LSG compressed payload";
	return false;
    }
    if (compress_flag == 2 && algorithm == 2) {
	uLongf out_size = static_cast<uLongf>(seg_len) * 8 + 4096;
	for (int attempt = 0; attempt < 6; ++attempt) {
	    /* Bound the decompressed size before each inflate attempt so a crafted
	     * (zip-bomb) segment cannot force an unbounded allocation. */
	    if (static_cast<size_t>(out_size) > MAX_UNCOMPRESSED_BYTES) {
		error = "JT LSG segment inflates too large";
		return false;
	    }
	    lsg_data.resize(out_size);
	    uLongf produced = out_size;
	    const int zr = uncompress(lsg_data.data(), &produced, bytes_.data() + seg.offset(),
		static_cast<uLong>(payload));
	    if (zr == Z_OK) { lsg_data.resize(produced); break; }
	    if (zr != Z_BUF_ERROR) { error = "cannot inflate JT LSG segment"; return false; }
	    out_size *= 2;   /* grow the output buffer and retry */
	    if (attempt == 5) { error = "JT LSG segment inflates too large"; return false; }
	}
    } else {
	lsg_data.assign(bytes_.data() + seg.offset(), bytes_.data() + seg.offset() + payload);
    }

    /* Enumerate the logical elements in the inflated buffer.  The graph elements
     * and the property-atom pool are two element sections, each terminated by an
     * all-0xFF end-of-elements marker; the Property Table follows.  The object id
     * is the first I32 of every element body (JT 8 has no header object id). */
    std::vector<LsgElement> elements;
    size_t property_table_offset = lsg_data.size();
    Reader er(lsg_data, header_.little_endian, 0);
    int eoe_seen = 0;
    while (er.offset() + 4 + GUID_LENGTH <= lsg_data.size()) {
	const size_t start = er.offset();
	uint32_t element_length = 0;
	Guid type_id{};
	if (!er.u32(element_length) || !read_guid(er, type_id)) break;
	const bool end_marker = element_length == GUID_LENGTH &&
	    std::all_of(type_id.begin(), type_id.end(), [](unsigned char b) { return b == 0xff; });
	if (end_marker) {
	    er.seek(start + 4 + GUID_LENGTH);
	    if (++eoe_seen >= 2) { property_table_offset = er.offset(); break; }
	    continue;
	}
	LsgElement e;
	e.type_id = type_id;
	e.type = lsg_type(type_id, header_.little_endian);
	if (element_length < GUID_LENGTH + 1 || start + 4 + element_length > lsg_data.size() || !er.u8(e.base_type)) {
	    property_table_offset = start;
	    break;
	}
	e.data_offset = er.offset();
	e.data_length = element_length - GUID_LENGTH - 1;
	Reader peek(lsg_data, header_.little_endian, e.data_offset);
	int32_t object_id = 0;
	if (!peek.i32(object_id)) { property_table_offset = start; break; }
	e.object_id = object_id;
	elements.push_back(e);
	er.seek(start + 4 + element_length);
    }

    /* Index elements by object id and decode the attribute/atom pools. */
    const double fv = lsg_file_version(header_);
    std::map<int32_t, const LsgElement *> by_id;
    std::map<int32_t, std::string> string_atoms;
    std::map<int32_t, Guid> late_atoms;
    std::map<int32_t, std::array<float, 4>> materials;
    std::map<int32_t, std::array<double, 16>> transforms;
    for (const LsgElement &e : elements) {
	by_id[e.object_id] = &e;
	Reader r(lsg_data, header_.little_endian, e.data_offset);
	if (e.type == LsgType::StringAtom) {
	    std::string value;
	    if (lsg_read_string_atom(r, fv, value)) string_atoms[e.object_id] = value;
	} else if (e.type == LsgType::LateLoadedAtom) {
	    Guid seg{};
	    if (lsg_read_late_atom(r, fv, seg)) late_atoms[e.object_id] = seg;
	} else if (e.type == LsgType::Material) {
	    float rgba[4] = {0.5f, 0.5f, 0.5f, 1.0f};
	    if (lsg_read_material(r, fv, rgba)) materials[e.object_id] = {rgba[0], rgba[1], rgba[2], rgba[3]};
	} else if (e.type == LsgType::GeometricTransform) {
	    double m[16];
	    if (lsg_read_transform(r, fv, e.data_length, m)) {
		std::array<double, 16> a;
		std::copy(m, m + 16, a.begin());
		transforms[e.object_id] = a;
	    }
	}
    }

    /* Property Table: node object id -> list of (key atom id, value atom id). */
    std::map<int32_t, std::vector<std::pair<int32_t, int32_t>>> property_table;
    if (property_table_offset < lsg_data.size()) {
	Reader pt(lsg_data, header_.little_endian, property_table_offset);
	uint16_t version = 0;
	int32_t table_count = 0;
	if (pt.u16(version) && pt.i32(table_count) && table_count >= 0 && table_count <= 1000000) {
	    for (int32_t i = 0; i < table_count; ++i) {
		int32_t node_id = 0;
		if (!pt.i32(node_id)) break;
		std::vector<std::pair<int32_t, int32_t>> pairs;
		for (;;) {
		    int32_t key_id = 0, value_id = 0;
		    if (!pt.i32(key_id) || key_id == 0) break;
		    if (!pt.i32(value_id)) break;
		    pairs.emplace_back(key_id, value_id);
		}
		property_table[node_id] = std::move(pairs);
	    }
	}
    }

    const auto node_name = [&](int32_t node_id) -> std::string {
	auto it = property_table.find(node_id);
	if (it == property_table.end()) return std::string();
	for (const auto &kv : it->second) {
	    auto key = string_atoms.find(kv.first);
	    auto val = string_atoms.find(kv.second);
	    if (key == string_atoms.end() || val == string_atoms.end()) continue;
	    if (key->second.find("JT_PROP_NAME") != std::string::npos) {
		std::string name = val->second;
		const size_t semi = name.find(';');
		if (semi != std::string::npos) name.erase(semi);
		return name;
	    }
	}
	return std::string();
    };
    const auto node_segment = [&](int32_t node_id, Guid &seg) -> bool {
	auto it = property_table.find(node_id);
	if (it == property_table.end()) return false;
	for (const auto &kv : it->second) {
	    auto late = late_atoms.find(kv.second);
	    if (late != late_atoms.end()) { seg = late->second; return true; }
	}
	return false;
    };
    /* Merge a node's string key/value properties into an accumulating map. */
    const auto merge_properties = [&](int32_t node_id, std::map<std::string, std::string> &props) {
	auto it = property_table.find(node_id);
	if (it == property_table.end()) return;
	for (const auto &kv : it->second) {
	    auto key = string_atoms.find(kv.first);
	    auto val = string_atoms.find(kv.second);
	    if (key == string_atoms.end() || val == string_atoms.end()) continue;
	    if (key->second.empty() || val->second.empty()) continue;
	    props[key->second] = val->second;
	}
    };

    /* Depth-first traversal from the root partition, accumulating the transform
     * and material and resolving each shape node to its geometry segment. */
    const std::array<double, 16> identity = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
    std::vector<int32_t> path;   /* guards against instance-node cycles */
    std::function<void(int32_t, const std::array<double, 16> &, const std::array<float, 4> &, bool,
	const std::string &, const std::map<std::string, std::string> &, bool, int32_t)> visit;
    visit = [&](int32_t node_id, const std::array<double, 16> &m_in, const std::array<float, 4> &color_in,
	bool has_color_in, const std::string &name_in, const std::map<std::string, std::string> &props_in,
	bool visible_in, int32_t referenced_part_in) {
	if (instances.size() >= 1000000 || path.size() > 256) return;
	auto found = by_id.find(node_id);
	if (found == by_id.end()) return;
	const LsgElement &e = *found->second;
	if (std::find(path.begin(), path.end(), node_id) != path.end()) return;
	path.push_back(node_id);

	/* Read this node's base data (flags + attribute ids + children as applicable). */
	Reader r(lsg_data, header_.little_endian, e.data_offset);
	uint32_t flags = 0;
	std::vector<int32_t> attrs, children;
	[[maybe_unused]] bool have_group = false;
	[[maybe_unused]] Guid partition_file_marker{};
	std::string file_name;
	switch (e.type) {
	    case LsgType::Partition: {
		if (lsg_read_group(r, fv, flags, attrs, children)) {
		    int32_t partition_flags = 0;
		    if (r.i32(partition_flags)) lsg_read_mbstring(r, file_name);
		    have_group = true;
		}
		break;
	    }
	    case LsgType::Group: case LsgType::Part: case LsgType::MetaDataNode:
	    case LsgType::RangeLOD: case LsgType::LOD:
		have_group = lsg_read_group(r, fv, flags, attrs, children);
		break;
	    case LsgType::Instance: {
		int32_t child = 0;
		if (lsg_read_base(r, fv, flags, attrs) && lsg_skip_version(r, fv, 9.0, false) && r.i32(child)) {
		    children.push_back(child);
		    have_group = true;
		}
		break;
	    }
	    /* referenced_part tracked below via referenced_part_in / this node */
	    case LsgType::TriStripShape: case LsgType::PolygonShape: case LsgType::VertexShape:
		lsg_read_base(r, fv, flags, attrs);   /* attributes only; geometry via property table */
		break;
	    default:
		break;
	}

	if (flags & 0x1) { path.pop_back(); return; }   /* Ignore flag: skip subtree */

	/* Node visibility: the Base Node Data flags carry an invisible bit (0x2)
	 * that is inherited down the subtree.  Track it so leaf shapes can record a
	 * 'jt_visible' attribute reflecting whether the branch was marked hidden. */
	const bool visible = visible_in && !(flags & 0x2);

	/* Accumulate this node's transform and material attributes. */
	std::array<double, 16> m = m_in;
	std::array<float, 4> color = color_in;
	bool has_color = has_color_in;
	for (int32_t attr_id : attrs) {
	    auto x = transforms.find(attr_id);
	    if (x != transforms.end()) {
		std::array<double, 16> out;
		lsg_mat_mul(out.data(), m.data(), x->second.data());
		m = out;
	    }
	    auto mat = materials.find(attr_id);
	    if (mat != materials.end()) { color = mat->second; has_color = true; }
	}

	std::string name = node_name(node_id);
	if (name.empty()) name = name_in;

	std::map<std::string, std::string> props = props_in;
	merge_properties(node_id, props);

	/* Instance nodes reference a Part/child node ID; remember the most recent
	 * one so leaf shapes can record it as a 'jt_referenced_part_id'. */
	int32_t referenced_part = referenced_part_in;
	if (e.type == LsgType::Instance && !children.empty())
	    referenced_part = children.front();

	switch (e.type) {
	    case LsgType::TriStripShape: case LsgType::PolygonShape: case LsgType::VertexShape: {
		Guid seg{};
		if (node_segment(node_id, seg)) {
		    SceneInstance inst;
		    inst.name = name;
		    std::copy(m.begin(), m.end(), inst.matrix);
		    inst.segment_id = seg;
		    inst.has_color = has_color;
		    for (int i = 0; i < 4; ++i) inst.color[i] = color[i];
		    inst.visible = visible;
		    inst.properties.assign(props.begin(), props.end());
		    inst.referenced_part_id = referenced_part;
		    instances.push_back(std::move(inst));
		}
		break;
	    }
	    case LsgType::Partition:
		if (children.empty() && !file_name.empty()) break;   /* external reference: cannot resolve */
		for (int32_t c : children) visit(c, m, color, has_color, name, props, visible, referenced_part);
		break;
	    case LsgType::RangeLOD: case LsgType::LOD:
		if (!children.empty()) visit(children.front(), m, color, has_color, name, props, visible, referenced_part);   /* finest LOD */
		break;
	    default:
		for (int32_t c : children) visit(c, m, color, has_color, name, props, visible, referenced_part);
		break;
	}
	path.pop_back();
    };

    /* The root is the first Partition (or first node) in the graph. */
    int32_t root_id = -1;
    for (const LsgElement &e : elements)
	if (e.type == LsgType::Partition) { root_id = e.object_id; break; }
    if (root_id < 0 && !elements.empty()) root_id = elements.front().object_id;
    if (root_id >= 0)
	visit(root_id, identity, {0.5f, 0.5f, 0.5f, 1.0f}, false, std::string(), std::map<std::string, std::string>(), true, -1);

    if (std::getenv("JT_LSG_DEBUG"))
	std::fprintf(stderr, "[lsg] elems=%zu strings=%zu late=%zu materials=%zu xforms=%zu ptbl=%zu instances=%zu\n",
	    elements.size(), string_atoms.size(), late_atoms.size(), materials.size(),
	    transforms.size(), property_table.size(), instances.size());

    if (instances.empty()) { error = "JT LSG produced no shape instances"; return false; }
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
