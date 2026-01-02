/*                         R L E . H P P
 * BRL-CAD
 *
 * Published in 2025 by the United States Government.
 * This work is in the public domain.
 *
 */
/** @file rle.hpp
 * Header-only implementation of the Utah Raster Toolkit RLE image
 * format.
 *
 * Primary Specification:
 *   Spencer W. Thomas, "Design of the Utah RLE Format"
 *   https://sarnold.github.io/urt/docs/rle.pdf
 *
 * Historical Behavior Clarifications:
 *   - All 16-bit words stored little-endian (VAX order).
 *   - BYTE_DATA and RUN_DATA operands encode (length - 1).
 *   - Run pixel value stored as 16-bit word; low 8 bits are color.
 *   - Filler byte appears ONLY after BYTE_DATA when decoded length is odd.
 *   - SkipLines / SkipPixels operands are direct counts (no +1).
 *   - SetColor operand 255 selects alpha channel (logical -1).
 *
 * Note:
 *   - RLE uses bottom-up coordinates: y=0 is at BOTTOM of image
 *   - Encoders and decoders work directly in RLE's bottom-up coordinate system
 *   - Image buffers are expected to be in bottom-up order (buffer[0] = bottom row)
 *   - Conversion tools must handle order conversion for their formats if needed
 *
 * CRITICAL COORDINATE SYSTEM DETAIL (not explicit in original spec):
 *   - Encoders must emit SkipLines(1) after each scanline to advance scan_y
 *   - Without explicit SkipLines, all channel data writes to same y coordinate
 *   - This behavior matches ImageMagick and Utah RLE reference implementations
 *
 * HARDENING:
 *   - Dimension, pixel count, colormap, comment length, and allocation caps.
 *   - Overflow-safe multiplication and allocation checks.
 *   - Clamped decoding of PixelData & RunData (discard excess safely).
 *   - Per-row opcode count guard to prevent DoS opcode floods.
 *   - Endian auto-detection unless STRICT_RLE_ENDIAN is defined.
 *   - Robust error codes (enum Error) instead of silent failures.
 *   - std::chrono civil date timestamp.
 *
 * CONFIGURATION (define before including to override defaults):
 *   RLE_CFG_MAX_DIM                (default 32768)
 *   RLE_CFG_MAX_COMMENT_LEN        (default 8192)
 *   RLE_CFG_MAX_COLORMAP_ENTRIES   (default 3 * 256)
 *   RLE_CFG_MAX_ALLOC_BYTES        (default 1ULL << 30)  // 1 GiB cap
 *   RLE_CFG_MAX_OPS_PER_ROW_FACTOR (default 10)
 *   RLE_TIMESTAMP_ENABLED          (default 1)
 *   STRICT_RLE_ENDIAN              (force little-endian only)
 *   RLE_NO_EXCEPTIONS              (return bool instead of throw)
 */

#ifndef BRLCAD_RLE_HPP
#define BRLCAD_RLE_HPP

#include "common.h"

#include <cstdint>
#include <cstdio>
#include <vector>
#include <string>
#include <cstring>
#include <stdexcept>
#include <chrono>
#include <limits>

#include "icv/defines.h"

#ifndef RLE_CFG_MAX_DIM
#define RLE_CFG_MAX_DIM 32768u
#endif
#ifndef RLE_CFG_MAX_COMMENT_LEN
#define RLE_CFG_MAX_COMMENT_LEN 8192u
#endif
#ifndef RLE_CFG_MAX_COLORMAP_ENTRIES
#define RLE_CFG_MAX_COLORMAP_ENTRIES (3u * 256u)
#endif
#ifndef RLE_CFG_MAX_ALLOC_BYTES
#define RLE_CFG_MAX_ALLOC_BYTES (1ULL << 30) /* 1 GiB */
#endif
#ifndef RLE_CFG_MAX_OPS_PER_ROW_FACTOR
#define RLE_CFG_MAX_OPS_PER_ROW_FACTOR 10u
#endif
#ifndef RLE_TIMESTAMP_ENABLED
#define RLE_TIMESTAMP_ENABLED 1
#endif

#ifndef RLE_NO_EXCEPTIONS
#define RLE_THROW(MSG) throw std::runtime_error(MSG)
#else
#define RLE_THROW(MSG) do { return false; } while(0)
#endif

namespace rle {

constexpr uint32_t MAX_DIM                = RLE_CFG_MAX_DIM;
constexpr uint64_t MAX_PIXELS             = uint64_t(MAX_DIM) * uint64_t(MAX_DIM);
constexpr uint32_t MAX_COMMENT_LEN        = RLE_CFG_MAX_COMMENT_LEN;
constexpr uint32_t MAX_COLORMAP_ENTRIES   = RLE_CFG_MAX_COLORMAP_ENTRIES;
constexpr uint64_t MAX_ALLOC_BYTES        = RLE_CFG_MAX_ALLOC_BYTES;
constexpr uint32_t MAX_OPS_PER_ROW_FACTOR = RLE_CFG_MAX_OPS_PER_ROW_FACTOR;

/* Opcodes/flags (LONG_BIT renamed to avoid system macro conflict) */
static constexpr uint8_t  OPC_LONG_FLAG   = 0x40;
static constexpr uint8_t  OPC_SKIP_LINES  = 1;
static constexpr uint8_t  OPC_SET_COLOR   = 2;
static constexpr uint8_t  OPC_SKIP_PIXELS = 3;
static constexpr uint8_t  OPC_BYTE_DATA   = 5;
static constexpr uint8_t  OPC_RUN_DATA    = 6;
static constexpr uint8_t  OPC_EOF         = 7;

static constexpr uint8_t  FLAG_CLEAR_FIRST   = 0x01;
static constexpr uint8_t  FLAG_NO_BACKGROUND = 0x02;
static constexpr uint8_t  FLAG_ALPHA         = 0x04;
static constexpr uint8_t  FLAG_COMMENT       = 0x08;

static constexpr uint16_t RLE_MAGIC = 0xCC52;

enum class Error {
    OK = 0,
    BAD_MAGIC,
    HEADER_TRUNCATED,
    UNSUPPORTED_ENDIAN,
    DIM_TOO_LARGE,
    PIXELS_TOO_LARGE,
    ALLOC_TOO_LARGE,
    COLORMAP_TOO_LARGE,
    COMMENT_TOO_LARGE, /* may be used by writers; reader now skips oversized */
    INVALID_NCOLORS,
    INVALID_PIXELBITS,
    INVALID_BG_BLOCK,
    OPCODE_OVERFLOW,
    OPCODE_UNKNOWN,
    TRUNCATED_OPCODE,
    OP_COUNT_EXCEEDED,
    INTERNAL_ERROR
};

inline const char* error_string(Error e) {
    switch (e) {
	case Error::OK: return "OK";
	case Error::BAD_MAGIC: return "Bad magic";
	case Error::HEADER_TRUNCATED: return "Header truncated";
	case Error::UNSUPPORTED_ENDIAN: return "Unsupported endian";
	case Error::DIM_TOO_LARGE: return "Dimensions exceed max";
	case Error::PIXELS_TOO_LARGE: return "Pixel count exceeds max";
	case Error::ALLOC_TOO_LARGE: return "Allocation exceeds cap";
	case Error::COLORMAP_TOO_LARGE: return "Colormap exceeds cap";
	case Error::COMMENT_TOO_LARGE: return "Comment block too large";
	case Error::INVALID_NCOLORS: return "Invalid ncolors";
	case Error::INVALID_PIXELBITS: return "Invalid pixelbits";
	case Error::INVALID_BG_BLOCK: return "Invalid background block";
	case Error::OPCODE_OVERFLOW: return "Opcode operand overflow";
	case Error::OPCODE_UNKNOWN: return "Unknown opcode";
	case Error::TRUNCATED_OPCODE: return "Truncated opcode data";
	case Error::OP_COUNT_EXCEEDED: return "Opcode count per row exceeded";
	case Error::INTERNAL_ERROR: return "Internal error";
	default: return "Unknown";
    }
}

enum class Endian { Little, Big };

inline bool read_u16(FILE* f, Endian e, uint16_t& v) {
    uint8_t b0, b1;
    int c0 = std::fgetc(f); if (c0 == EOF) return false;
    int c1 = std::fgetc(f); if (c1 == EOF) return false;
    b0 = uint8_t(c0); b1 = uint8_t(c1);
    if (e == Endian::Little) v = uint16_t(b0) | (uint16_t(b1) << 8);
    else                     v = uint16_t(b1) | (uint16_t(b0) << 8);
    return true;
}
inline bool write_u16_le(FILE* f, uint16_t v) {
    return std::fputc(int(v & 0xFF), f) != EOF &&
	std::fputc(int((v >> 8) & 0xFF), f) != EOF;
}
inline bool read_u8(FILE* f, uint8_t& v) {
    int c = std::fgetc(f); if (c == EOF) return false;
    v = uint8_t(c); return true;
}
inline bool write_u8(FILE* f, uint8_t v) { return std::fputc(int(v), f) != EOF; }

inline bool safe_mul_u64(uint64_t a, uint64_t b, uint64_t limit, uint64_t& out) {
    if (a == 0 || b == 0) { out = 0; return true; }
    if (a > limit / b) return false;
    out = a * b;
    return true;
}

/* Small helper to discard N bytes from a stream without seeking */
inline bool discard_bytes(FILE* f, uint64_t n) {
    static const size_t CHUNK = 4096;
    uint8_t buf[CHUNK];
    while (n > 0) {
	size_t want = (n > CHUNK) ? CHUNK : size_t(n);
	size_t got = std::fread(buf, 1, want, f);
	if (got != want) return false;
	n -= got;
    }
    return true;
}

struct Header {
    uint16_t xpos     = 0;
    uint16_t ypos     = 0;
    uint16_t xlen     = 0;
    uint16_t ylen     = 0;
    uint8_t  flags    = 0;
    uint8_t  ncolors  = 3;
    uint8_t  pixelbits= 8;
    uint8_t  ncmap    = 0;
    uint8_t  cmaplen  = 0;

    std::vector<uint8_t>  background;
    std::vector<uint16_t> colormap;
    std::vector<std::string> comments;

    bool has_alpha()     const { return (flags & FLAG_ALPHA) != 0; }
    bool has_comments()  const { return (flags & FLAG_COMMENT) != 0; }
    bool no_background() const { return (flags & FLAG_NO_BACKGROUND) != 0; }
    bool clear_first()   const { return (flags & FLAG_CLEAR_FIRST) != 0; }

    uint32_t width()  const { return xlen; }
    uint32_t height() const { return ylen; }
    uint8_t channels() const { return uint8_t(ncolors + (has_alpha() ? 1 : 0)); }

    bool validate(Error& err) const {
	if (!xlen || !ylen || xlen > MAX_DIM || ylen > MAX_DIM) { err = Error::DIM_TOO_LARGE; return false; }
	if (pixelbits != 8) { err = Error::INVALID_PIXELBITS; return false; }
	if (ncolors == 0 || ncolors > 254) { err = Error::INVALID_NCOLORS; return false; }
	if (!no_background() && background.size() != ncolors) { err = Error::INVALID_BG_BLOCK; return false; }
	if (ncmap > 0) {
	    if (ncmap > 3 || cmaplen > 8) { err = Error::COLORMAP_TOO_LARGE; return false; }
	    uint64_t entries = uint64_t(ncmap) * (uint64_t(1) << cmaplen);
	    if (entries > MAX_COLORMAP_ENTRIES) { err = Error::COLORMAP_TOO_LARGE; return false; }
	    if (colormap.size() != entries) { err = Error::COLORMAP_TOO_LARGE; return false; }
	}
	uint64_t pixTotal;
	if (!safe_mul_u64(xlen, ylen, MAX_PIXELS, pixTotal)) { err = Error::PIXELS_TOO_LARGE; return false; }
	err = Error::OK;
	return true;
    }
};

inline std::vector<uint8_t> pack_comments(const std::vector<std::string>& comments) {
    std::vector<uint8_t> out;
    for (auto& s : comments) {
	out.insert(out.end(), s.begin(), s.end());
	out.push_back(0);
    }
    return out;
}
inline void unpack_comments(const std::vector<uint8_t>& block, std::vector<std::string>& out) {
    size_t i = 0;
    while (i < block.size()) {
	std::string s;
	while (i < block.size() && block[i] != 0) s.push_back(char(block[i++]));
	if (i < block.size() && block[i] == 0) ++i;
	if (!s.empty()) out.push_back(std::move(s));
    }
}

inline bool write_header(FILE* f, const Header& h) {
    Error e;
    if (!h.validate(e)) RLE_THROW(error_string(e));
    if (!write_u16_le(f, RLE_MAGIC)) return false;
    if (!write_u16_le(f, h.xpos) || !write_u16_le(f, h.ypos) ||
	    !write_u16_le(f, h.xlen) || !write_u16_le(f, h.ylen)) return false;
    if (!write_u8(f, h.flags) || !write_u8(f, h.ncolors) || !write_u8(f, h.pixelbits) ||
	    !write_u8(f, h.ncmap) || !write_u8(f, h.cmaplen)) return false;

    if (!h.no_background()) {
	for (uint8_t v : h.background) if (!write_u8(f, v)) return false;
	/* COMPATIBILITY: Utah RLE reference implementation does NOT write padding
	 * after the background block, even when ncolors is odd. The RLE spec
	 * describes padding for comments and pixel data, but not for background. */
    } else {
	/* COMPATIBILITY: Utah RLE reference implementation (libutahrle) writes
	 * a single null byte when NO_BACKGROUND flag is set, even though the
	 * format specification suggests no background data should be present.
	 * This behavior is found in utahrle/Runput.c line ~220.
	 * We replicate this for compatibility. */
	if (!write_u8(f, 0)) return false;
    }
    if (h.ncmap > 0) {
	for (uint16_t cv : h.colormap)
	    if (!write_u16_le(f, cv)) return false;
    }
    if (h.has_comments()) {
	auto packed = pack_comments(h.comments);
	if (packed.size() > MAX_COMMENT_LEN) RLE_THROW("Comment block too large");
	uint16_t clen = uint16_t(packed.size());
	if (!write_u16_le(f, clen)) return false;
	for (uint8_t b : packed) if (!write_u8(f, b)) return false;
	if (clen & 0x01) if (!write_u8(f, 0)) return false;
    }
    return true;
}

inline bool read_header_single(FILE* f, Header& h, Endian e, Error& err) {
    long start = std::ftell(f);
    if (start == -1L) { err = Error::INTERNAL_ERROR; return false; }
    uint16_t magic;
    if (!read_u16(f, e, magic)) { err = Error::HEADER_TRUNCATED; std::fseek(f, start, SEEK_SET); return false; }
    if (magic != RLE_MAGIC) { err = Error::BAD_MAGIC; std::fseek(f, start, SEEK_SET); return false; }
    auto rd8 = [&](uint8_t& v)->bool { int c = std::fgetc(f); if (c == EOF) return false; v = uint8_t(c); return true; };
    if (!read_u16(f, e, h.xpos) || !read_u16(f, e, h.ypos) ||
	    !read_u16(f, e, h.xlen) || !read_u16(f, e, h.ylen) ||
	    !rd8(h.flags) || !rd8(h.ncolors) || !rd8(h.pixelbits) ||
	    !rd8(h.ncmap) || !rd8(h.cmaplen)) {
	err = Error::HEADER_TRUNCATED; std::fseek(f, start, SEEK_SET); return false;
    }
    if (!(h.flags & FLAG_NO_BACKGROUND)) {
	h.background.resize(h.ncolors);
	for (uint8_t &v : h.background)
	    if (!rd8(v)) { err = Error::HEADER_TRUNCATED; std::fseek(f, start, SEEK_SET); return false; }
	/* COMPATIBILITY: Utah RLE reference implementation does NOT write padding
	 * after the background block, even when ncolors is odd. The RLE spec
	 * describes padding for comments and pixel data, but not for background. */
    } else {
	/* COMPATIBILITY: Utah RLE reference implementation (libutahrle) writes
	 * a single null byte when NO_BACKGROUND flag is set, even though the
	 * format specification suggests no background data should be present.
	 * This behavior is found in utahrle/Runput.c line ~220.
	 * We must read this byte for compatibility. */
	uint8_t null_byte;
	if (!rd8(null_byte)) { err = Error::HEADER_TRUNCATED; std::fseek(f, start, SEEK_SET); return false; }
    }
    if (h.ncmap > 0) {
	if (h.ncmap > 3 || h.cmaplen > 8) { err = Error::COLORMAP_TOO_LARGE; std::fseek(f, start, SEEK_SET); return false; }
	uint64_t entries = uint64_t(h.ncmap) * (uint64_t(1) << h.cmaplen);
	if (entries > MAX_COLORMAP_ENTRIES) { err = Error::COLORMAP_TOO_LARGE; std::fseek(f, start, SEEK_SET); return false; }
	h.colormap.resize(entries);
	for (uint64_t i = 0; i < entries; ++i) {
	    uint16_t cv;
	    if (!read_u16(f, e, cv)) { err = Error::HEADER_TRUNCATED; std::fseek(f, start, SEEK_SET); return false; }
	    h.colormap[i] = cv;
	}
	uint16_t all = 0;
	for (auto v : h.colormap) all |= v;
	if ((all & 0xFF00) == 0 && (all & 0x00FF) != 0)
	    for (auto &v : h.colormap) v <<= 8;
    }
    if (h.flags & FLAG_COMMENT) {
	uint16_t clen;
	if (!read_u16(f, e, clen)) { err = Error::HEADER_TRUNCATED; std::fseek(f, start, SEEK_SET); return false; }
	/* Hardened: if comments too large, read-and-discard instead of failing */
	if (clen > MAX_COMMENT_LEN) {
	    if (!discard_bytes(f, clen)) { err = Error::HEADER_TRUNCATED; std::fseek(f, start, SEEK_SET); return false; }
	    if (clen & 0x01) { uint8_t pad; if (!read_u8(f, pad)) { err = Error::HEADER_TRUNCATED; std::fseek(f, start, SEEK_SET); return false; } }
	    /* Do not populate h.comments; proceed */
	} else if (clen > 0) {
	    std::vector<uint8_t> block(clen);
	    for (uint16_t i = 0; i < clen; ++i)
		if (!rd8(block[i])) { err = Error::HEADER_TRUNCATED; std::fseek(f, start, SEEK_SET); return false; }
	    if (clen & 0x01) { uint8_t pad; if (!rd8(pad)) { err = Error::HEADER_TRUNCATED; std::fseek(f, start, SEEK_SET); return false; } }
	    unpack_comments(block, h.comments);
	} else {
	    /* zero-length comments: still may have even padding (clen==0 => no pad) */
	}
    }
    if (!h.validate(err)) { std::fseek(f, start, SEEK_SET); return false; }
    err = Error::OK;
    return true;
}

inline bool read_header_auto(FILE* f, Header& h, Endian& chosen, Error& err) {
#ifdef STRICT_RLE_ENDIAN
    if (!read_header_single(f, h, Endian::Little, err)) return false;
    chosen = Endian::Little; return true;
#else
    long pos = std::ftell(f);
    if (pos == -1L) { err = Error::INTERNAL_ERROR; return false; }
    if (read_header_single(f, h, Endian::Little, err)) { chosen = Endian::Little; return true; }
    if (err == Error::BAD_MAGIC) {
	std::fseek(f, pos, SEEK_SET);
	if (read_header_single(f, h, Endian::Big, err)) { chosen = Endian::Big; return true; }
    }
    return false;
#endif
}

struct Image {
    Header header;
    std::vector<uint8_t> pixels;

    bool allocate(Error& err) {
	Error hv;
	if (!header.validate(hv)) { err = hv; return false; }
	uint64_t total;
	if (!safe_mul_u64(header.width(), header.height(), MAX_PIXELS, total)) { err = Error::PIXELS_TOO_LARGE; return false; }
	uint64_t bytes;
	if (!safe_mul_u64(total, header.channels(), MAX_ALLOC_BYTES, bytes)) { err = Error::ALLOC_TOO_LARGE; return false; }
	try {
	    pixels.assign(size_t(header.width()) * size_t(header.height()) * header.channels(), 0);

	    // Initialize pixels with background color if specified
	    if (!header.no_background() && !header.background.empty()) {
		size_t npix = size_t(header.width()) * header.height();
		for (size_t i = 0; i < npix; ++i) {
		    for (size_t c = 0; c < header.ncolors && c < header.background.size(); ++c) {
			pixels[i * header.channels() + c] = header.background[c];
		    }
		}
	    }

	    // Initialize alpha channel to 255 (fully opaque) by default
	    if (header.has_alpha()) {
		size_t npix = size_t(header.width()) * header.height();
		for (size_t i = 0; i < npix; ++i) {
		    pixels[i * header.channels() + header.ncolors] = 255;
		}
	    }
	} catch (...) { err = Error::ALLOC_TOO_LARGE; return false; }
	err = Error::OK; return true;
    }

    inline uint8_t* pixel(uint32_t x, uint32_t y) {
	return pixels.data() + (size_t(y) * header.width() + x) * header.channels();
    }
    inline const uint8_t* pixel(uint32_t x, uint32_t y) const {
	return pixels.data() + (size_t(y) * header.width() + x) * header.channels();
    }
};

inline bool pixel_is_background(const Image& img, uint32_t x, uint32_t y) {
    const uint8_t* p = img.pixel(x, y);
    for (uint8_t c = 0; c < img.header.ncolors; ++c) {
	if (img.header.background.empty() || p[c] != img.header.background[c]) return false;
    }
    return true;
}
inline bool row_is_background(const Image& img, uint32_t y) {
    const uint32_t W = img.header.width();
    const uint8_t chans = img.header.channels();
    for (uint32_t x = 0; x < W; ++x) {
	const uint8_t* p = img.pixel(x, y);
	for (uint8_t c = 0; c < img.header.ncolors; ++c) {
	    if (img.header.background.empty() || p[c] != img.header.background[c]) return false;
	}
	if (img.header.has_alpha() && p[chans - 1] != 0) return false;
    }
    return true;
}

class Encoder {
    public:
	enum BackgroundMode { BG_SAVE_ALL = 0, BG_OVERLAY = 1, BG_CLEAR = 2 };

	static bool write(FILE* f, const Image& img, BackgroundMode bg_mode, Error& err) {
	    if (!f) { err = Error::INTERNAL_ERROR; return false; }

	    Header h = img.header;
	    if (bg_mode == BG_CLEAR) h.flags |= FLAG_CLEAR_FIRST;
	    if (img.header.has_alpha()) h.flags |= FLAG_ALPHA;
	    if (!img.header.comments.empty()) h.flags |= FLAG_COMMENT;
	    if (h.background.empty()) h.flags |= FLAG_NO_BACKGROUND;

	    if (!write_header(f, h)) { err = Error::INTERNAL_ERROR; return false; }

	    const uint32_t W = h.width();
	    const uint32_t H = h.height();
	    const uint8_t chans = h.channels();

	    /* Encoder coordinate system: RLE uses bottom-up coordinates (y=0 at bottom).
	     * Image buffer is in bottom-up order (buffer[0] = bottom row).
	     * After writing all channels for a scanline, SkipLines(1) advances to next row.
	     * Without SkipLines, decoder would write next scanline to same y coordinate. */
	    uint32_t rle_y = 0;
	    while (rle_y < H) {
		uint32_t buffer_y = rle_y;

		if (bg_mode != BG_SAVE_ALL && !h.no_background() && row_is_background(img, buffer_y)) {
		    uint32_t start = rle_y;
		    ++rle_y;  // Start counting from next scanline
		    buffer_y = rle_y;
		    while (rle_y < H && row_is_background(img, buffer_y) && (rle_y - start) < 65535) {
			++rle_y;
			buffer_y = rle_y;
		    }
		    uint32_t skipCount = rle_y - start;
		    if (skipCount <= 255) {
			if (!write_u8(f, OPC_SKIP_LINES) || !write_u8(f, uint8_t(skipCount))) { err = Error::INTERNAL_ERROR; return false; }
		    } else {
			if (!write_u8(f, OPC_SKIP_LINES | OPC_LONG_FLAG) || !write_u8(f, 0) || !write_u16_le(f, uint16_t(skipCount))) { err = Error::INTERNAL_ERROR; return false; }
		    }
		    continue;
		}

		for (uint8_t c = 0; c < chans; ++c) {
		    uint16_t operand = (c == h.ncolors && h.has_alpha()) ? 255 : c;
		    if (!write_u8(f, OPC_SET_COLOR) || !write_u8(f, uint8_t(operand))) { err = Error::INTERNAL_ERROR; return false; }

		    uint32_t x = 0;
		    uint64_t opsThisRow = 0;
		    while (x < W) {
			if (++opsThisRow > uint64_t(MAX_OPS_PER_ROW_FACTOR) * W) { err = Error::OP_COUNT_EXCEEDED; return false; }

			if (bg_mode != BG_SAVE_ALL && c < h.ncolors && pixel_is_background(img, x, buffer_y)) {
			    uint32_t start = x;
			    while (x < W && pixel_is_background(img, x, buffer_y) && (x - start) < 65535) ++x;
			    uint32_t span = x - start;
			    if (span >= 2) {
				if (span <= 255) {
				    if (!write_u8(f, OPC_SKIP_PIXELS) || !write_u8(f, uint8_t(span))) { err = Error::INTERNAL_ERROR; return false; }
				} else {
				    if (!write_u8(f, OPC_SKIP_PIXELS | OPC_LONG_FLAG) || !write_u8(f, 0) || !write_u16_le(f, uint16_t(span))) { err = Error::INTERNAL_ERROR; return false; }
				}
				continue;
			    } else {
				x = start;
			    }
			}

			uint8_t v = img.pixel(x, buffer_y)[c];
			uint32_t run_len = 1;
			while (x + run_len < W && img.pixel(x + run_len, buffer_y)[c] == v && run_len < 65535) ++run_len;
			if (run_len >= 3) {
			    uint32_t encoded = run_len - 1;
			    if (encoded <= 255) {
				if (!write_u8(f, OPC_RUN_DATA) || !write_u8(f, uint8_t(encoded))) { err = Error::INTERNAL_ERROR; return false; }
			    } else {
				if (!write_u8(f, OPC_RUN_DATA | OPC_LONG_FLAG) || !write_u8(f, 0) || !write_u16_le(f, uint16_t(encoded))) { err = Error::INTERNAL_ERROR; return false; }
			    }
			    if (!write_u16_le(f, uint16_t(v))) { err = Error::INTERNAL_ERROR; return false; }
			    x += run_len;
			    continue;
			}

			std::vector<uint8_t> lit;
			lit.reserve(256);
			while (x < W) {
			    uint8_t pv = img.pixel(x, buffer_y)[c];
			    uint32_t look = 1;
			    while (x + look < W && img.pixel(x + look, buffer_y)[c] == pv && look < 3) ++look;
			    if (look >= 3) break;
			    lit.push_back(pv);
			    ++x;
			    if (lit.size() == 256) break;
			}
			if (lit.empty()) continue;
			uint32_t count = uint32_t(lit.size());
			uint32_t encoded = count - 1;
			if (encoded <= 255) {
			    if (!write_u8(f, OPC_BYTE_DATA) || !write_u8(f, uint8_t(encoded))) { err = Error::INTERNAL_ERROR; return false; }
			} else {
			    if (!write_u8(f, OPC_BYTE_DATA | OPC_LONG_FLAG) || !write_u8(f, 0) || !write_u16_le(f, uint16_t(encoded))) { err = Error::INTERNAL_ERROR; return false; }
			}
			for (uint8_t pv : lit)
			    if (!write_u8(f, pv)) { err = Error::INTERNAL_ERROR; return false; }
			if (count & 1)
			    if (!write_u8(f, 0)) { err = Error::INTERNAL_ERROR; return false; }
		    }
		}
		++rle_y;

		// After completing a scanline, write SkipLines(1) to advance to the next scanline
		// (unless this was the last scanline, or we already skipped lines due to background)
		if (rle_y < H) {
		    if (!write_u8(f, OPC_SKIP_LINES) || !write_u8(f, 1)) { err = Error::INTERNAL_ERROR; return false; }
		}
	    }

	    if (!write_u8(f, OPC_EOF) || !write_u8(f, 0)) { err = Error::INTERNAL_ERROR; return false; }
	    err = Error::OK; return true;
	}
};

struct DecoderResult {
    bool   ok = false;
    Error  error = Error::OK;
    Endian endian = Endian::Little;
};

class Decoder {
    public:
	static DecoderResult read(FILE* f, Image& img) {
	    DecoderResult res;
	    if (!f) { res.error = Error::INTERNAL_ERROR; return res; }
	    Header h; Endian e; Error herr;
	    if (!read_header_auto(f, h, e, herr)) { res.error = herr; return res; }
	    img.header = h;
	    Error aerr;
	    if (!img.allocate(aerr)) { res.error = aerr; return res; }

	    const uint32_t W = h.width();
	    const uint32_t H = h.height();
	    const uint32_t xmin = h.xpos;
	    const uint32_t ymin = h.ypos;
	    const uint8_t  chans = h.channels();

	    /* Decoder coordinate system: RLE uses bottom-up coordinates (y=0 at bottom).
	     * Decoder writes to buffer in bottom-up order (buffer[0] = bottom row).
	     * scan_y advances only via SkipLines opcodes, not implicitly after channels. */
	    uint32_t scan_y = ymin;
	    int current_channel = -1;
	    uint32_t scan_x = xmin;

	    while (scan_y < ymin + H) {
		uint8_t op0, op1;
		if (!read_u8(f, op0)) break;
		if (!read_u8(f, op1)) { res.error = Error::TRUNCATED_OPCODE; return res; }
		uint8_t base = op0 & ~OPC_LONG_FLAG;
		bool longForm = (op0 & OPC_LONG_FLAG) != 0;

		switch (base) {
		    case OPC_SKIP_LINES: {
					     uint16_t lines;
					     if (longForm) { if (!read_u16(f, e, lines)) { res.error = Error::TRUNCATED_OPCODE; return res; } }
					     else lines = op1;
					     scan_y += lines;
					     scan_x = xmin;
					     current_channel = -1;
					     continue;
					 }
		    case OPC_SET_COLOR: {
					    if (longForm) { res.error = Error::OPCODE_UNKNOWN; return res; }
					    uint16_t ch = op1;
					    int new_channel = (ch == 255 && h.has_alpha()) ? h.ncolors : int(ch);
					    current_channel = new_channel;
					    scan_x = xmin;
					} break;
		    case OPC_SKIP_PIXELS: {
					      uint16_t skip;
					      if (longForm) { if (!read_u16(f, e, skip)) { res.error = Error::TRUNCATED_OPCODE; return res; } }
					      else skip = op1;
					      scan_x += skip;
					  } break;
		    case OPC_BYTE_DATA: {
					    uint16_t enc;
					    if (longForm) { if (!read_u16(f, e, enc)) { res.error = Error::TRUNCATED_OPCODE; return res; } }
					    else enc = op1;
					    uint32_t count = uint32_t(enc) + 1;
					    uint32_t remaining = (xmin + W > scan_x) ? (xmin + W - scan_x) : 0;
					    uint32_t to_write = (count < remaining) ? count : remaining;
					    uint32_t to_discard = count - to_write;
					    for (uint32_t i = 0; i < to_write; ++i) {
						uint8_t pv;
						if (!read_u8(f, pv)) { res.error = Error::TRUNCATED_OPCODE; return res; }
						if (current_channel >= 0 && current_channel < int(chans)) {
						    uint32_t buffer_y = scan_y - ymin;
						    img.pixel(scan_x - xmin, buffer_y)[current_channel] = pv;
						}
						++scan_x;
					    }
					    for (uint32_t i = 0; i < to_discard; ++i) {
						uint8_t tmp;
						if (!read_u8(f, tmp)) { res.error = Error::TRUNCATED_OPCODE; return res; }
						++scan_x;
					    }
					    if (count & 1) { uint8_t filler; if (!read_u8(f, filler)) { res.error = Error::TRUNCATED_OPCODE; return res; } }
					} break;
		    case OPC_RUN_DATA: {
					   uint16_t enc;
					   if (longForm) { if (!read_u16(f, e, enc)) { res.error = Error::TRUNCATED_OPCODE; return res; } }
					   else enc = op1;
					   uint32_t run_len = uint32_t(enc) + 1;
					   uint16_t word;
					   if (!read_u16(f, e, word)) { res.error = Error::TRUNCATED_OPCODE; return res; }
					   uint8_t pv = uint8_t(word & 0xFF);
					   uint32_t remaining = (xmin + W > scan_x) ? (xmin + W - scan_x) : 0;
					   uint32_t to_write = (run_len < remaining) ? run_len : remaining;
					   uint32_t to_skip = run_len - to_write;
					   for (uint32_t i = 0; i < to_write; ++i) {
					       if (current_channel >= 0 && current_channel < int(chans)) {
						   uint32_t buffer_y = scan_y - ymin;
						   img.pixel(scan_x - xmin, buffer_y)[current_channel] = pv;
					       }
					       ++scan_x;
					   }
					   scan_x += to_skip;
				       } break;
		    case OPC_EOF:
				       // Apply colormap if present
				       if (h.ncmap > 0 && !h.colormap.empty()) {
					   apply_colormap(img, h);
				       }
				       res.ok = true; res.error = Error::OK; res.endian = e; return res;
		    default:
				       res.error = Error::OPCODE_UNKNOWN; return res;
		}
	    }
	    // Apply colormap if present (in case EOF wasn't reached normally)
	    if (h.ncmap > 0 && !h.colormap.empty()) {
		apply_colormap(img, h);
	    }
	    res.ok = true; res.error = Error::OK; res.endian = e; return res;
	}

    private:
	static void apply_colormap(Image& img, const Header& h) {
	    // Apply colormap transformation to pixel data
	    // Colormap layout: [channel0_entries...][channel1_entries...][channel2_entries...]
	    // where each channel has (1 << h.cmaplen) entries

	    if (h.ncmap == 0 || h.colormap.empty()) return;

	    const size_t map_length = size_t(1) << h.cmaplen;
	    const size_t num_pixels = size_t(h.width()) * h.height();
	    const uint8_t num_channels = h.channels();

	    // For each pixel, apply colormap to color channels (not alpha)
	    for (size_t i = 0; i < num_pixels; ++i) {
		uint8_t* pixel = img.pixels.data() + i * num_channels;

		if (h.ncmap == 1) {
		    // Single colormap for grayscale: apply to first channel only
		    uint8_t index = pixel[0];
		    if (index < map_length && index < h.colormap.size()) {
			pixel[0] = uint8_t(h.colormap[index] >> 8);
		    }
		} else if (h.ncmap >= 3 && h.ncolors >= 3) {
		    // Separate colormaps for RGB channels
		    for (uint8_t c = 0; c < 3 && c < h.ncolors; ++c) {
			uint8_t index = pixel[c];
			size_t cmap_offset = c * map_length;
			if (index < map_length && (cmap_offset + index) < h.colormap.size()) {
			    pixel[c] = uint8_t(h.colormap[cmap_offset + index] >> 8);
			}
		    }
		}
		// Alpha channel is never affected by colormap
	    }
	}
};

/* ----- Convenience RGB helpers ----- */
inline bool write_rgb(FILE* f,
	const uint8_t* interleaved,
	uint32_t width,
	uint32_t height,
	const std::vector<std::string>& comments,
	const std::vector<uint8_t>& background,
	bool include_alpha,
	Encoder::BackgroundMode bg_mode,
	Error& err) {
    Header h;
    h.xpos = 0; h.ypos = 0;
    h.xlen = width; h.ylen = height;
    h.ncolors = 3;
    h.pixelbits = 8;
    h.ncmap = 0; h.cmaplen = 0;
    if (!background.empty()) h.background = background;
    else h.flags |= FLAG_NO_BACKGROUND;
    if (!comments.empty()) h.flags |= FLAG_COMMENT;
    h.comments = comments;
    if (include_alpha) h.flags |= FLAG_ALPHA;

    Error hv;
    if (!h.validate(hv)) { err = hv; return false; }

    Image img;
    img.header = h;
    Error aerr;
    if (!img.allocate(aerr)) { err = aerr; return false; }

    size_t pixCount = size_t(width) * height;
    if (!include_alpha) {
	std::memcpy(img.pixels.data(), interleaved, pixCount * 3);
    } else {
	for (size_t i = 0; i < pixCount; ++i) {
	    img.pixels[4*i+0] = interleaved[4*i+0];
	    img.pixels[4*i+1] = interleaved[4*i+1];
	    img.pixels[4*i+2] = interleaved[4*i+2];
	    img.pixels[4*i+3] = interleaved[4*i+3];
	}
    }
    return Encoder::write(f, img, bg_mode, err);
}

inline bool read_rgb(FILE* f,
	std::vector<uint8_t>& interleaved,
	uint32_t& width,
	uint32_t& height,
	bool* has_alpha_out,
	std::vector<std::string>* comments_out,
	Error& err) {
    Image img;
    DecoderResult dr = Decoder::read(f, img);
    if (!dr.ok) { err = dr.error; return false; }
    width  = img.header.width();
    height = img.header.height();
    if (comments_out) *comments_out = img.header.comments;
    if (has_alpha_out) *has_alpha_out = img.header.has_alpha();

    size_t pixCount = size_t(width) * height;
    if (!img.header.has_alpha()) {
	interleaved.resize(pixCount * 3);
	std::memcpy(interleaved.data(), img.pixels.data(), pixCount * 3);
    } else {
	interleaved.resize(pixCount * 4);
	std::memcpy(interleaved.data(), img.pixels.data(), pixCount * 4);
    }
    err = Error::OK;
    return true;
}

/* ----- UTC timestamp (std::chrono based; no platform ifdefs) ----- */
#if RLE_TIMESTAMP_ENABLED
inline std::string rle_utc_timestamp() {
    using namespace std::chrono;
    auto now  = system_clock::now();
    auto secs = time_point_cast<seconds>(now);
    int64_t z = secs.time_since_epoch().count();      // seconds since epoch
    int64_t days = z / 86400;
    int64_t rem  = z % 86400;
    if (rem < 0) { rem += 86400; --days; }
    int hour = int(rem / 3600); rem %= 3600;
    int minute = int(rem / 60);
    int second = int(rem % 60);

    /* Civil date algorithm (proleptic Gregorian) */
    constexpr int64_t DAYS_OFFSET = 719468; // from 0000-03-01 to 1970-01-01
    int64_t civil_days = days + DAYS_OFFSET;
    int64_t era = (civil_days >= 0 ? civil_days : civil_days - 146096) / 146097;
    int64_t doe = civil_days - era * 146097;
    int64_t yoe = (doe - doe/1460 + doe/36524 - doe/146096) / 365;
    int year = int(yoe) + int(era) * 400;
    int64_t doy = doe - (365*yoe + yoe/4 - yoe/100);
    int64_t mp = (5*doy + 2)/153;
    int day = int(doy - (153*mp + 2)/5 + 1);
    int month = int(mp < 10 ? mp + 3 : mp - 9);
    year += (month <= 2);

    char buf[32];
    std::snprintf(buf, sizeof(buf),
	    "%04d-%02d-%02dT%02d:%02d:%02dZ",
	    year, month, day, hour, minute, second);
    return std::string(buf);
}
#endif /* RLE_TIMESTAMP_ENABLED */

} /* namespace rle */

__BEGIN_DECLS

ICV_EXPORT int
rle_write(icv_image_t *bif, FILE *fp);

ICV_EXPORT icv_image_t*
rle_read(FILE *fp);

__END_DECLS

#endif /* BRLCAD_RLE_HPP */

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8 cino=N-s
