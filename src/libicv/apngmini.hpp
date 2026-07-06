//                    A P N G M I N I . H P P
// BRL-CAD
//
// Published in 2026 by the United States Government.
// This work is in the public domain.
//
// Single-header C++17 library for reading/writing APNG images
//
// ==============================================================================
//
// INTRODUCTION
// ------------
// `apngmini` is a robust, clean, and portable C++17 single-header library for
// reading, writing, and compositing APNG (Animated Portable Network Graphics)
// images. It relies on standard PNG backends (like libpng) for decoding and
// encoding the actual frame pixels, but fully handles the complex APNG chunk
// manipulation, sequence numbering, and frame compositing logic internally.
//
// To use this library, do this in *one* C++ source file:
//     #define APNGMINI_IMPLEMENTATION
//     #include "apngmini.hpp"
//
// HOW TO USE
// ----------
//
// 1. Reading an APNG
//
//     apngmini::Animation anim = apngmini::read_file("animation.png");
//     if (anim.frames.empty()) {
//         // Handle error
//     }
//
//     // You can access individual raw frames directly:
//     // anim.frames[0].pixels (RGBA8 buffer)
//     // anim.frames[0].width, etc.
//
// 2. Compositing
//
// APNG frames typically only contain the "differences" between frames (with x/y
// offsets) to save space. To get the full 32-bit RGBA canvas for playback:
//
//     // This handles all `dispose_op` and `blend_op` logic:
//     apngmini::vector<apngmini::vector<uint8_t>> full_frames = anim.compose();
//
// 3. Writing an APNG
//
//     apngmini::Animation anim;
//     anim.canvas_width = 800;
//     anim.canvas_height = 600;
//     anim.num_plays = 0; // Infinite loop
//
//     apngmini::Frame frame;
//     frame.width = 800;
//     frame.height = 600;
//     frame.delay = {1, 10}; // 1/10th of a second
//     frame.pixels = apngmini::vector<uint8_t>(800 * 600 * 4, 255); // White canvas
//     anim.frames.push_back(frame);
//
//     apngmini::write_file("out.png", anim);
//
// 4. Streaming Reader and Writer
//
// If you don't want to hold the entire animation in memory, you can use the
// streaming APIs: `apngmini::Reader` and `apngmini::Writer`.
//
//     apngmini::Reader reader(data_ptr, data_size);
//     while (reader.has_next()) {
//         apngmini::Frame frame = reader.next_frame();
//     }
//
//     apngmini::Writer writer(width, height, num_frames, num_plays,
//                             [](const uint8_t* data, size_t size) {
//         // Write to network or file...
//     });
//     writer.add_frame(...);
//
// DEFINING CUSTOM BACKENDS
// ------------------------
// By default, `apngmini` uses `libpng` for encoding and decoding standard PNG
// data chunks. However, you can swap it out for something else (e.g. `libspng`
// or `lodepng`) by defining `APNGMINI_PNG_BACKEND` before including the header
// in the implementation file.
//
// Your backend struct must look exactly like this:
//
//     struct MyCustomBackend {
//         static std::optional<apngmini::detail::DecodeResult> decode_png(const uint8_t* data, size_t len);
//         static std::optional<apngmini::vector<uint8_t>> encode_png(const uint8_t* rgba, uint32_t width, uint32_t height);
//     };
//
//     #define APNGMINI_PNG_BACKEND MyCustomBackend
//     #define APNGMINI_IMPLEMENTATION
//     #include "apngmini.hpp"
//
// MACROS
// ------
// - `APNGMINI_IMPLEMENTATION`: Define in exactly one cpp file.
// - `APNGMINI_NO_EXCEPTIONS`: Define to disable C++ exceptions. Errors will
//                             print to stderr and return empty values instead.
// - `APNGMINI_PNG_BACKEND`: Define to override the default libpng backend.
//
// ==============================================================================


#ifndef APNGMINI_HPP_INCLUDED
#define APNGMINI_HPP_INCLUDED

#include <cstdint>
#include <cstring>
#include <algorithm>
#include <vector>
#include <string>
#include <functional>
#include <stdexcept>
#include <optional>

#include "bu/exit.h" // use bu_bomb instead of std::abort()

#ifndef APNGMINI_MAX_PIXELS
#define APNGMINI_MAX_PIXELS 268435456  // 256 megapixels
#endif

#ifndef APNGMINI_MALLOC
#include <cstdlib>
#define APNGMINI_MALLOC(size)        std::malloc(size)
#define APNGMINI_FREE(ptr)           std::free(ptr)
#define APNGMINI_REALLOC(ptr, size)  std::realloc(ptr, size)
#endif

// ============================================================================
// PUBLIC TYPES
// ============================================================================

namespace apngmini
{

template <typename T>
struct Allocator {
    using value_type = T;
    Allocator() = default;
    template <class U> constexpr Allocator(const Allocator<U>&) noexcept {}
    T* allocate(std::size_t n)
    {
	if (n > static_cast<std::size_t>(-1) / sizeof(T)) {
#ifndef APNGMINI_NO_EXCEPTIONS
	    throw std::bad_alloc();
#else
	    bu_bomb("apngmini allocation failure 1");
#endif
	}
	if (auto p = static_cast<T*>(APNGMINI_MALLOC(n * sizeof(T)))) return p;
#ifndef APNGMINI_NO_EXCEPTIONS
	throw std::bad_alloc();
#else
	bu_bomb("apngmini allocation failure 2");
#endif
    }
    void deallocate(T* p, std::size_t) noexcept
    {
	APNGMINI_FREE(p);
    }
};
template <class T, class U> bool operator==(const Allocator<T>&, const Allocator<U>&)
{
    return true;
}
template <class T, class U> bool operator!=(const Allocator<T>&, const Allocator<U>&)
{
    return false;
}

template <typename T>
using vector = std::vector<T, Allocator<T>>;

enum class DisposeOp : uint8_t {
    None       = 0,
    Background = 1,
    Previous   = 2
};

enum class BlendOp : uint8_t {
    Source = 0,
    Over   = 1
};

struct Delay {
    uint16_t numerator   = 1;
    uint16_t denominator = 10;

    double seconds() const
    {
	return static_cast<double>(numerator) / (denominator == 0 ? 100 : denominator);
    }
};

struct Frame {
    apngmini::vector<uint8_t> pixels; // RGBA8, width*height*4 bytes
    uint32_t width    = 0;
    uint32_t height   = 0;
    uint32_t x_offset = 0;
    uint32_t y_offset = 0;
    Delay      delay;
    DisposeOp  dispose_op = DisposeOp::None;
    BlendOp    blend_op   = BlendOp::Source;
};

struct Animation {
    uint32_t  canvas_width  = 0;
    uint32_t  canvas_height = 0;
    uint32_t  num_plays     = 0;
    apngmini::vector<Frame> frames;

    apngmini::vector<apngmini::vector<uint8_t>> compose() const;
};

// Default backend uses libpng
#ifndef APNGMINI_PNG_BACKEND
namespace detail
{
struct LibpngBackend;
}
#define APNGMINI_PNG_BACKEND apngmini::detail::LibpngBackend
#endif

// Error handling
#ifndef APNGMINI_NO_EXCEPTIONS
class Error : public std::runtime_error
{
public:
    explicit Error(const std::string& what_arg) : std::runtime_error(what_arg) {}
    explicit Error(const char* what_arg) : std::runtime_error(what_arg) {}
};
#endif

// ============================================================================
// PUBLIC API
// ============================================================================

// Read an entire APNG file from memory.
Animation read(const uint8_t* data, size_t size);
Animation read(const apngmini::vector<uint8_t>& data);
Animation read_file(const char* path);
Animation read_file(const std::string& path);

// Write a complete Animation to memory.
apngmini::vector<uint8_t> write(const Animation& anim, int compression_level = -1);
bool write_file(const char* path, const Animation& anim, int compression_level = -1);
bool write_file(const std::string& path, const Animation& anim, int compression_level = -1);

// Streaming reader
class Reader
{
public:
    explicit Reader(const uint8_t* data, size_t size);

    uint32_t canvas_width()  const
    {
	return canvas_width_;
    }
    uint32_t canvas_height() const
    {
	return canvas_height_;
    }
    uint32_t num_frames()    const
    {
	return num_frames_;
    }
    uint32_t num_plays()     const
    {
	return num_plays_;
    }

    bool has_next() const;
    Frame next_frame();

    bool error() const
    {
	return error_;
    }

private:
    struct Chunk {
	uint32_t length;
	uint32_t type;
	const uint8_t* data;
    };

    const uint8_t* data_ = nullptr;
    size_t size_ = 0;
    size_t offset_ = 0;

    uint32_t canvas_width_ = 0;
    uint32_t canvas_height_ = 0;
    uint32_t num_frames_ = 0;
    uint32_t num_plays_ = 0;
    uint32_t frames_read_ = 0;
    uint32_t expected_seq_ = 0;
    bool is_apng_ = false;
    bool error_ = false;

    apngmini::vector<uint8_t> ihdr_chunk_;
    apngmini::vector<uint8_t> global_ancillary_chunks_; // e.g., PLTE, tRNS, gAMA (before IDAT)

    bool default_image_is_first_frame_ = false; // true if fcTL precedes first IDAT

    // Read next chunk without advancing
    std::optional<Chunk> peek_chunk(size_t at_offset) const;
    // Read next chunk and advance offset
    std::optional<Chunk> read_chunk();
    // Reassemble and decode a single frame
    Frame decode_frame_data(const apngmini::vector<uint8_t>& frame_data, uint32_t width, uint32_t height, const Frame& metadata);
};

// Streaming writer
class Writer
{
public:
    using OutputFunc = std::function<void(const uint8_t*, size_t)>;

    Writer(uint32_t canvas_width, uint32_t canvas_height,
	   uint32_t num_frames, uint32_t num_plays = 0, OutputFunc output = {}, int compression_level = -1);

    void add_frame(const Frame& frame);
    void add_frame(const uint8_t* rgba, uint32_t w, uint32_t h,
		   Delay delay, uint32_t x_off = 0, uint32_t y_off = 0,
		   DisposeOp disp = DisposeOp::None, BlendOp blend = BlendOp::Source);

    apngmini::vector<uint8_t> finish();

    bool error() const
    {
	return error_;
    }

private:
    uint32_t canvas_width_ = 0;
    uint32_t canvas_height_ = 0;
    uint32_t num_frames_ = 0;
    OutputFunc output_;
    apngmini::vector<uint8_t> buffer_;
    uint32_t sequence_number_ = 0;
    uint32_t frames_written_ = 0;
    bool error_ = false;
    bool default_image_written_ = false;
    int compression_level_ = -1;

    void write_data(const uint8_t* data, size_t len);
};

namespace detail
{
// Backend concept definition for decode/encode single PNG frame
struct DecodeResult {
    apngmini::vector<uint8_t> pixels; // RGBA8
    uint32_t width;
    uint32_t height;
};
} // namespace detail

} // namespace apngmini

#endif // APNGMINI_HPP_INCLUDED

// ============================================================================
// IMPLEMENTATION
// ============================================================================

#ifdef APNGMINI_IMPLEMENTATION
#ifndef APNGMINI_IMPL_GUARD
#define APNGMINI_IMPL_GUARD

#include <png.h>
#ifndef APNGMINI_NO_ZLIB_CRC
#include <zlib.h>
#endif
#include <fstream>
#include <iostream>

namespace apngmini
{
namespace detail
{

// Error helper
inline void throw_error(const char* msg)
{
#ifndef APNGMINI_NO_EXCEPTIONS
    throw Error(msg);
#else
    std::cerr << "apngmini error: " << msg << std::endl;
#endif
}

// Endianness helpers
inline uint16_t read_be16(const uint8_t* p)
{
    return static_cast<uint16_t>((static_cast<unsigned>(p[0]) << 8) | p[1]);
}

inline uint32_t read_be32(const uint8_t* p)
{
    return (static_cast<uint32_t>(p[0]) << 24) |
	   (static_cast<uint32_t>(p[1]) << 16) |
	   (static_cast<uint32_t>(p[2]) << 8) |
	   static_cast<uint32_t>(p[3]);
}

inline void write_be16(uint8_t* p, uint16_t val)
{
    p[0] = static_cast<uint8_t>(val >> 8);
    p[1] = static_cast<uint8_t>(val);
}

inline void write_be32(uint8_t* p, uint32_t val)
{
    p[0] = static_cast<uint8_t>(val >> 24);
    p[1] = static_cast<uint8_t>(val >> 16);
    p[2] = static_cast<uint8_t>(val >> 8);
    p[3] = static_cast<uint8_t>(val);
}

// CRC32 helper
inline uint32_t crc32_data(const uint8_t* data, size_t len, uint32_t crc = 0)
{
#ifndef APNGMINI_NO_ZLIB_CRC
    return static_cast<uint32_t>(::crc32(crc, data, static_cast<uInt>(len)));
#else
    static const uint32_t crc_table[256] = {
	0x00000000, 0x77073096, 0xee0e612c, 0x990951ba, 0x076dc419, 0x706af48f, 0xe963a535, 0x9e6495a3,
	0x0edb8832, 0x79dcb8a4, 0xe0d5e91e, 0x97d2d988, 0x09b64c2b, 0x7eb17cbd, 0xe7b82d07, 0x90bf1d91,
	0x1db71064, 0x6ab020f2, 0xf3b97148, 0x84be41de, 0x1adad47d, 0x6ddde4eb, 0xf4d4b551, 0x83d385c7,
	0x136c9856, 0x646ba8c0, 0xfd62f97a, 0x8a65c9ec, 0x14015c4f, 0x63066cd9, 0xfa0f3d63, 0x8d080df5,
	0x3b6e20c8, 0x4c69105e, 0xd56041e4, 0xa2677172, 0x3c03e4d1, 0x4b04d447, 0xd20d85fd, 0xa50ab56b,
	0x35b5a8fa, 0x42b2986c, 0xdbbbc9d6, 0xacbcf940, 0x32d86ce3, 0x45df5c75, 0xdcd60dcf, 0xabd13d59,
	0x26d930ac, 0x51de003a, 0xc8d75180, 0xbfd06116, 0x21b4f4b5, 0x56b3c423, 0xcfba9599, 0xb8bda50f,
	0x2802b89e, 0x5f058808, 0xc60cd9b2, 0xb10be924, 0x2f6f7c87, 0x58684c11, 0xc1611dab, 0xb6662d3d,
	0x76dc4190, 0x01db7106, 0x98d220bc, 0xefd5102a, 0x71b18589, 0x06b6b51f, 0x9fbfe4a5, 0xe8b8d433,
	0x7807c9a2, 0x0f00f934, 0x9609a88e, 0xe10e9818, 0x7f6a0dbb, 0x086d3d2d, 0x91646c97, 0xe6635c01,
	0x6b6b51f4, 0x1c6c6162, 0x856530d8, 0xf262004e, 0x6c0695ed, 0x1b01a57b, 0x8208f4c1, 0xf50fc457,
	0x65b0d9c6, 0x12b7e950, 0x8bbeb8ea, 0xfcb9887c, 0x62dd1ddf, 0x15da2d49, 0x8cd37cf3, 0xfbd44c65,
	0x4db26158, 0x3ab551ce, 0xa3bc0074, 0xd4bb30e2, 0x4adfa541, 0x3dd895d7, 0xa4d1c46d, 0xd3d6f4fb,
	0x4369e96a, 0x346ed9fc, 0xad678846, 0xda60b8d0, 0x44042d73, 0x33031de5, 0xaa0a4c5f, 0xdd0d7cc9,
	0x5005713c, 0x270241aa, 0xbe0b1010, 0xc90c2086, 0x5768b525, 0x206f85b3, 0xb966d409, 0xce61e49f,
	0x5edef90e, 0x29d9c998, 0xb0d09822, 0xc7d7a8b4, 0x59b33d17, 0x2eb40d81, 0xb7bd5c3b, 0xc0ba6cad,
	0xedb88320, 0x9abfb3b6, 0x03b6e20c, 0x74b1d29a, 0xead54739, 0x9dd277af, 0x04db2615, 0x73dc1683,
	0xe3630b12, 0x94643b84, 0x0d6d6a3e, 0x7a6a5aa8, 0xe40ecf0b, 0x9309ff9d, 0x0a00ae27, 0x7d079eb1,
	0xf00f9344, 0x8708a3d2, 0x1e01f268, 0x6906c2fe, 0xf762575d, 0x806567cb, 0x196c3671, 0x6e6b06e7,
	0xfed41b76, 0x89d32be0, 0x10da7a5a, 0x67dd4acc, 0xf9b9df6f, 0x8ebeeff9, 0x17b7be43, 0x60b08ed5,
	0xd6d6a3e8, 0xa1d1937e, 0x38d8c2c4, 0x4fdff252, 0xd1bb67f1, 0xa6bc5767, 0x3fb506dd, 0x48b2364b,
	0xd80d2bda, 0xaf0a1b4c, 0x36034af6, 0x41047a60, 0xdf60efc3, 0xa867df55, 0x316e8eef, 0x4669be79,
	0xcb61b38c, 0xbc66831a, 0x256fd2a0, 0x5268e236, 0xcc0c7795, 0xbb0b4703, 0x220216b9, 0x5505262f,
	0xc5ba3bbe, 0xb2bd0b28, 0x2bb45a92, 0x5cb36a04, 0xc2d7ffa7, 0xb5d0cf31, 0x2cd99e8b, 0x5bdeae1d,
	0x9b64c2b0, 0xec63f226, 0x756aa39c, 0x026d930a, 0x9c0906a9, 0xeb0e363f, 0x72076785, 0x05005713,
	0x95bf4a82, 0xe2b87a14, 0x7bb12bae, 0x0cb61b38, 0x92d28e9b, 0xe5d5be0d, 0x7cdcefb7, 0x0bdbdf21,
	0x86d3d2d4, 0xf1d4e242, 0x68ddb3f8, 0x1fda836e, 0x81be16cd, 0xf6b9265b, 0x6fb077e1, 0x18b74777,
	0x88085ae6, 0xff0f6a70, 0x66063bca, 0x11010b5c, 0x8f659eff, 0xf862ae69, 0x616bffd3, 0x166ccf45,
	0xa00ae278, 0xd70dd2ee, 0x4e048354, 0x3903b3c2, 0xa7672661, 0xd06016f7, 0x4969474d, 0x3e6e77db,
	0xaed16a4a, 0xd9d65adc, 0x40df0b66, 0x37d83bf0, 0xa9bcae53, 0xdebb9ec5, 0x47b2cf7f, 0x30b5ffe9,
	0xbdbdf21c, 0xcabac28a, 0x53b39330, 0x24b4a3a6, 0xbad03605, 0xcdd70693, 0x54de5729, 0x23d967bf,
	0xb3667a2e, 0xc4614ab8, 0x5d681b02, 0x2a6f2b94, 0xb40bbe37, 0xc30c8ea1, 0x5a05df1b, 0x2d02ef8d
    };
    uint32_t c = crc ^ 0xFFFFFFFF;
    for (size_t i = 0; i < len; ++i) {
	c = crc_table[(c ^ data[i]) & 0xFF] ^ (c >> 8);
    }
    return c ^ 0xFFFFFFFF;
#endif
}



// LibpngBackend
struct LibpngBackend {
    struct ReadState {
	const uint8_t* data;
	size_t size;
	size_t offset;
    };

    static void read_callback(png_structp png_ptr, png_bytep data, png_size_t length)
    {
	auto* state = static_cast<ReadState*>(png_get_io_ptr(png_ptr));
	if (state->offset + length > state->size) {
	    png_error(png_ptr, "Read Error");
	}
	std::memcpy(data, state->data + state->offset, length);
	state->offset += length;
    }

    struct LibpngReadGuard {
	png_structp png = nullptr;
	png_infop info = nullptr;
	~LibpngReadGuard()
	{
	    if (png) png_destroy_read_struct(&png, info ? &info : nullptr, nullptr);
	}
    };

    static std::optional<DecodeResult> decode_png(const uint8_t* data, size_t len)
    {
	if (len < 8 || png_sig_cmp(data, 0, 8)) {
	    return std::nullopt;
	}

	png_structp png = png_create_read_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
	if (!png) return std::nullopt;

	LibpngReadGuard guard{png, nullptr};

	png_infop info = png_create_info_struct(png);
	if (!info) return std::nullopt;
	guard.info = info;

	DecodeResult result;
	apngmini::vector<png_bytep> row_pointers;
	ReadState state{data, len, 0};

	if (setjmp(png_jmpbuf(png))) return std::nullopt;

	png_set_read_fn(png, &state, read_callback);

	png_read_info(png, info);

	png_byte color_type = png_get_color_type(png, info);
	png_byte bit_depth  = png_get_bit_depth(png, info);

	if (bit_depth == 16) png_set_strip_16(png);
	if (color_type == PNG_COLOR_TYPE_PALETTE) png_set_palette_to_rgb(png);
	if (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8) png_set_expand_gray_1_2_4_to_8(png);
	if (png_get_valid(png, info, PNG_INFO_tRNS)) png_set_tRNS_to_alpha(png);
	if (color_type == PNG_COLOR_TYPE_RGB || color_type == PNG_COLOR_TYPE_GRAY || color_type == PNG_COLOR_TYPE_PALETTE) {
	    png_set_filler(png, 0xFF, PNG_FILLER_AFTER);
	}
	if (color_type == PNG_COLOR_TYPE_GRAY || color_type == PNG_COLOR_TYPE_GRAY_ALPHA) {
	    png_set_gray_to_rgb(png);
	}

	png_read_update_info(png, info);

	uint32_t width = png_get_image_width(png, info);
	uint32_t height = png_get_image_height(png, info);

	result.width = width;
	result.height = height;
	size_t rowbytes = png_get_rowbytes(png, info);
	result.pixels.resize(rowbytes * height);
	row_pointers.resize(height);

	for (uint32_t y = 0; y < height; ++y) {
	    row_pointers[y] = result.pixels.data() + y * rowbytes;
	}

	png_read_image(png, row_pointers.data());
	png_read_end(png, nullptr);

	return result;
    }

    struct WriteState {
	apngmini::vector<uint8_t>& data;
    };

    static void write_callback(png_structp png_ptr, png_bytep data, png_size_t length)
    {
	auto* state = static_cast<WriteState*>(png_get_io_ptr(png_ptr));
	state->data.insert(state->data.end(), data, data + length);
    }

    static void flush_callback(png_structp /*png_ptr*/) {}

    struct LibpngWriteGuard {
	png_structp png = nullptr;
	png_infop info = nullptr;
	~LibpngWriteGuard()
	{
	    if (png) png_destroy_write_struct(&png, info ? &info : nullptr);
	}
    };

    static std::optional<apngmini::vector<uint8_t>> encode_png(const uint8_t* rgba, uint32_t width, uint32_t height, int compression_level = -1)
    {
	png_structp png = png_create_write_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
	if (!png) return std::nullopt;

	LibpngWriteGuard guard{png, nullptr};

	png_infop info = png_create_info_struct(png);
	if (!info) return std::nullopt;
	guard.info = info;

	apngmini::vector<uint8_t> out_data;
	WriteState state{out_data};
	apngmini::vector<png_bytep> row_pointers(height);

	if (setjmp(png_jmpbuf(png))) return std::nullopt;

	png_set_write_fn(png, &state, write_callback, flush_callback);

	png_set_IHDR(png, info, width, height, 8,
		     PNG_COLOR_TYPE_RGBA, PNG_INTERLACE_NONE,
		     PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
	if (compression_level != -1) {
	    png_set_compression_level(png, compression_level);
	}

	png_write_info(png, info);

	for (uint32_t y = 0; y < height; ++y) {
	    row_pointers[y] = const_cast<png_bytep>(rgba + y * width * 4);
	}

	png_write_image(png, row_pointers.data());
	png_write_end(png, nullptr);

	return out_data;
    }
};



// ----------------------------------------------------------------------------
// Chunk Types
// ----------------------------------------------------------------------------
constexpr uint32_t CHUNK_IHDR = 0x49484452;
constexpr uint32_t CHUNK_acTL = 0x6163544C;
constexpr uint32_t CHUNK_fcTL = 0x6663544C;
constexpr uint32_t CHUNK_IDAT = 0x49444154;
constexpr uint32_t CHUNK_fdAT = 0x66644154;
constexpr uint32_t CHUNK_IEND = 0x49454E44;

inline void write_chunk(apngmini::vector<uint8_t>& out, uint32_t type, const uint8_t* data, uint32_t len)
{
    size_t start = out.size();
    out.resize(start + 12 + len);
    write_be32(&out[start], len);
    write_be32(&out[start + 4], type);
    if (len > 0 && data) {
	std::memcpy(&out[start + 8], data, len);
    }
    uint32_t crc = crc32_data(&out[start + 4], 4 + len);
    write_be32(&out[start + 8 + len], crc);
}

} // namespace detail

// ----------------------------------------------------------------------------
// Writer Implementation
// ----------------------------------------------------------------------------

void Writer::write_data(const uint8_t* data, size_t len)
{
    if (output_) {
	output_(data, len);
    } else {
	buffer_.insert(buffer_.end(), data, data + len);
    }
}

Writer::Writer(uint32_t canvas_width, uint32_t canvas_height,
	       uint32_t num_frames, uint32_t num_plays, OutputFunc output, int compression_level)
    : canvas_width_(canvas_width), canvas_height_(canvas_height),
      num_frames_(num_frames), output_(std::move(output)), compression_level_(compression_level)
{

    if (canvas_width == 0 || canvas_height == 0 || num_frames == 0) {
	error_ = true;
	detail::throw_error("Invalid animation parameters");
	return;
    }

    // Write PNG signature
    write_data(reinterpret_cast<const uint8_t*>("\x89PNG\r\n\x1a\n"), 8);

    // Write IHDR
    apngmini::vector<uint8_t> tmp;
    uint8_t ihdr_data[13] = {0};
    detail::write_be32(ihdr_data, canvas_width);
    detail::write_be32(ihdr_data + 4, canvas_height);
    ihdr_data[8] = 8; // bit depth
    ihdr_data[9] = 6; // color type RGBA
    ihdr_data[10] = 0; // compression
    ihdr_data[11] = 0; // filter
    ihdr_data[12] = 0; // interlace
    detail::write_chunk(tmp, detail::CHUNK_IHDR, ihdr_data, 13);

    // Write acTL
    uint8_t actl_data[8] = {0};
    detail::write_be32(actl_data, num_frames);
    detail::write_be32(actl_data + 4, num_plays);
    detail::write_chunk(tmp, detail::CHUNK_acTL, actl_data, 8);

    write_data(tmp.data(), tmp.size());
}

void Writer::add_frame(const Frame& frame)
{
    add_frame(frame.pixels.data(), frame.width, frame.height, frame.delay,
	      frame.x_offset, frame.y_offset, frame.dispose_op, frame.blend_op);
}

void Writer::add_frame(const uint8_t* rgba, uint32_t w, uint32_t h,
		       Delay delay, uint32_t x_off, uint32_t y_off,
		       DisposeOp disp, BlendOp blend)
{
    if (error_ || frames_written_ >= num_frames_) return;

    if (static_cast<uint64_t>(x_off) + w > canvas_width_ || static_cast<uint64_t>(y_off) + h > canvas_height_ || w == 0 || h == 0) {
	error_ = true;
	detail::throw_error("Invalid frame dimensions or offset");
	return;
    }

    // Determine if first frame qualifies as the default image
    bool is_default_candidate = (frames_written_ == 0 && w == canvas_width_ &&
				 h == canvas_height_ && x_off == 0 && y_off == 0 && blend == BlendOp::Source);

    // If first frame is NOT a default image candidate, write a transparent
    // full-canvas IDAT as the default image (required by PNG spec).
    if (frames_written_ == 0 && !is_default_candidate && !default_image_written_) {
	apngmini::vector<uint8_t> transparent(static_cast<size_t>(canvas_width_) * canvas_height_ * 4, 0);
	auto def_enc = APNGMINI_PNG_BACKEND::encode_png(transparent.data(), canvas_width_, canvas_height_, compression_level_);
	if (!def_enc) {
	    error_ = true;
	    detail::throw_error("Failed to encode default image");
	    return;
	}
	size_t def_off = 8;
	while (def_off + 8 <= def_enc->size()) {
	    uint32_t def_len = detail::read_be32(def_enc->data() + def_off);
	    if (def_off + 12 + def_len > def_enc->size()) break;
	    uint32_t def_type = detail::read_be32(def_enc->data() + def_off + 4);
	    if (def_type == detail::CHUNK_IDAT) {
		write_data(def_enc->data() + def_off, 12 + def_len);
	    }
	    def_off += 12 + def_len;
	}
	default_image_written_ = true;
    }

    // Write fcTL
    apngmini::vector<uint8_t> tmp;
    uint8_t fctl_data[26] = {0};
    detail::write_be32(fctl_data, sequence_number_++);
    detail::write_be32(fctl_data + 4, w);
    detail::write_be32(fctl_data + 8, h);
    detail::write_be32(fctl_data + 12, x_off);
    detail::write_be32(fctl_data + 16, y_off);
    detail::write_be16(fctl_data + 20, delay.numerator);
    detail::write_be16(fctl_data + 22, delay.denominator);
    fctl_data[24] = static_cast<uint8_t>(disp);
    fctl_data[25] = static_cast<uint8_t>(blend);
    detail::write_chunk(tmp, detail::CHUNK_fcTL, fctl_data, 26);
    write_data(tmp.data(), tmp.size());

    // Encode frame pixels
    auto encoded = APNGMINI_PNG_BACKEND::encode_png(rgba, w, h, compression_level_);
    if (!encoded) {
	error_ = true;
	detail::throw_error("Failed to encode frame");
	return;
    }



    // Extract IDAT from encoded PNG and write it as IDAT or fdAT
    size_t offset = 8; // skip sig
    while (offset + 8 <= encoded->size()) {
	uint32_t len = detail::read_be32(encoded->data() + offset);
	if (offset + 12 + len > encoded->size()) break;
	uint32_t type = detail::read_be32(encoded->data() + offset + 4);

	if (type == detail::CHUNK_IDAT) {
	    if (is_default_candidate && !default_image_written_) {
		// Write as IDAT (default image = first frame)
		write_data(encoded->data() + offset, 12 + len);
	    } else {
		// Write as fdAT
		tmp.clear();
		apngmini::vector<uint8_t> fdat_data(4 + len);
		detail::write_be32(fdat_data.data(), sequence_number_++);
		std::memcpy(fdat_data.data() + 4, encoded->data() + offset + 8, len);
		detail::write_chunk(tmp, detail::CHUNK_fdAT, fdat_data.data(), 4 + len);
		write_data(tmp.data(), tmp.size());
	    }
	}
	offset += 12 + len;
    }

    if (is_default_candidate) default_image_written_ = true;
    frames_written_++;
}

apngmini::vector<uint8_t> Writer::finish()
{
    if (!error_ && frames_written_ < num_frames_) {
	error_ = true;
	detail::throw_error("Not all frames were written");
    }

    if (!error_) {
	apngmini::vector<uint8_t> tmp;
	detail::write_chunk(tmp, detail::CHUNK_IEND, nullptr, 0);
	write_data(tmp.data(), tmp.size());
    }

    return std::move(buffer_);
}

apngmini::vector<uint8_t> write(const Animation& anim, int compression_level)
{
    if (anim.frames.empty() || anim.canvas_width == 0 || anim.canvas_height == 0) {
	return {};
    }

    Writer writer(anim.canvas_width, anim.canvas_height,
		  static_cast<uint32_t>(anim.frames.size()), anim.num_plays, {}, compression_level);

    for (const auto& frame : anim.frames) {
	writer.add_frame(frame);
    }

    return writer.finish();
}

bool write_file(const char* path, const Animation& anim, int compression_level)
{
    auto data = write(anim, compression_level);
    if (data.empty()) return false;

    std::ofstream file(path, std::ios::binary);
    if (!file) return false;

    file.write(reinterpret_cast<const char*>(data.data()), data.size());
    return file.good();
}

bool write_file(const std::string& path, const Animation& anim, int compression_level)
{
    return write_file(path.c_str(), anim, compression_level);
}

apngmini::vector<apngmini::vector<uint8_t>> Animation::compose() const
{
    apngmini::vector<apngmini::vector<uint8_t>> composed_frames;
    if (frames.empty() || canvas_width == 0 || canvas_height == 0) return composed_frames;

    uint64_t total_pixels = static_cast<uint64_t>(canvas_width) * canvas_height;
    if (total_pixels > APNGMINI_MAX_PIXELS) return composed_frames;

    apngmini::vector<uint8_t> output_buffer(static_cast<size_t>(canvas_width) * canvas_height * 4, 0);
    apngmini::vector<uint8_t> previous_buffer;

    for (size_t i = 0; i < frames.size(); ++i) {
	const auto& frame = frames[i];

	// Per spec: first frame dispose_op PREVIOUS is treated as BACKGROUND
	DisposeOp effective_dispose = frame.dispose_op;
	if (i == 0 && effective_dispose == DisposeOp::Previous) {
	    effective_dispose = DisposeOp::Background;
	}

	BlendOp effective_blend = frame.blend_op;
	if (i == 0) effective_blend = BlendOp::Source;

	if (effective_dispose == DisposeOp::Previous) {
	    previous_buffer = output_buffer;
	}

	// Render frame onto output_buffer
	for (uint32_t y = 0; y < frame.height; ++y) {
	    uint32_t out_y = frame.y_offset + y;
	    if (out_y >= canvas_height) continue;

	    for (uint32_t x = 0; x < frame.width; ++x) {
		uint32_t out_x = frame.x_offset + x;
		if (out_x >= canvas_width) continue;

		size_t out_idx = (out_y * canvas_width + out_x) * 4;
		size_t src_idx = (y * frame.width + x) * 4;

		if (effective_blend == BlendOp::Source) {
		    std::memcpy(&output_buffer[out_idx], &frame.pixels[src_idx], 4);
		} else {
		    // BlendOp::Over
		    uint8_t src_r = frame.pixels[src_idx + 0];
		    uint8_t src_g = frame.pixels[src_idx + 1];
		    uint8_t src_b = frame.pixels[src_idx + 2];
		    uint8_t src_a = frame.pixels[src_idx + 3];

		    if (src_a == 255) {
			std::memcpy(&output_buffer[out_idx], &frame.pixels[src_idx], 4);
		    } else if (src_a > 0) {
			uint8_t dst_r = output_buffer[out_idx + 0];
			uint8_t dst_g = output_buffer[out_idx + 1];
			uint8_t dst_b = output_buffer[out_idx + 2];
			uint8_t dst_a = output_buffer[out_idx + 3];

			if (dst_a == 0) {
			    std::memcpy(&output_buffer[out_idx], &frame.pixels[src_idx], 4);
			} else {
			    // alpha compositing
			    uint32_t u = src_a * 255;
			    uint32_t v = (255 - src_a) * dst_a;
			    uint32_t al = u + v;
			    output_buffer[out_idx + 0] = static_cast<uint8_t>((src_r * u + dst_r * v) / al);
			    output_buffer[out_idx + 1] = static_cast<uint8_t>((src_g * u + dst_g * v) / al);
			    output_buffer[out_idx + 2] = static_cast<uint8_t>((src_b * u + dst_b * v) / al);
			    output_buffer[out_idx + 3] = static_cast<uint8_t>(al / 255);
			}
		    }
		}
	    }
	}

	composed_frames.push_back(output_buffer);

	// Apply dispose op for next frame
	if (effective_dispose == DisposeOp::Background) {
	    for (uint32_t y = 0; y < frame.height; ++y) {
		uint32_t out_y = frame.y_offset + y;
		if (out_y >= canvas_height) continue;

		size_t out_idx = (out_y * canvas_width + frame.x_offset) * 4;
		uint32_t w = std::min(frame.width, canvas_width - frame.x_offset);
		std::memset(&output_buffer[out_idx], 0, w * 4);
	    }
	} else if (effective_dispose == DisposeOp::Previous) {
	    output_buffer = previous_buffer;
	}
    }

    return composed_frames;
}

// ----------------------------------------------------------------------------
// Reader Implementation
// ----------------------------------------------------------------------------

std::optional<Reader::Chunk> Reader::peek_chunk(size_t at_offset) const
{
    if (at_offset + 8 > size_) return std::nullopt;
    uint32_t len = detail::read_be32(data_ + at_offset);
    if (at_offset + 12 + len > size_) return std::nullopt;
    uint32_t type = detail::read_be32(data_ + at_offset + 4);
    uint32_t expected_crc = detail::read_be32(data_ + at_offset + 8 + len);
    uint32_t actual_crc = detail::crc32_data(data_ + at_offset + 4, 4 + len);
    if (expected_crc != actual_crc) return std::nullopt;
    return Chunk{len, type, data_ + at_offset + 8};
}

std::optional<Reader::Chunk> Reader::read_chunk()
{
    auto chunk = peek_chunk(offset_);
    if (chunk) {
	offset_ += 12 + chunk->length;
    }
    return chunk;
}

Reader::Reader(const uint8_t* data, size_t size) : data_(data), size_(size)
{
    if (size < 8 || std::memcmp(data, "\x89PNG\r\n\x1a\n", 8) != 0) {
	error_ = true;
	detail::throw_error("Not a PNG file");
	return;
    }
    offset_ = 8;

    // Read IHDR
    auto ihdr = read_chunk();
    if (!ihdr || ihdr->type != detail::CHUNK_IHDR || ihdr->length != 13) {
	error_ = true;
	detail::throw_error("Missing or invalid IHDR chunk");
	return;
    }
    canvas_width_ = detail::read_be32(ihdr->data);
    canvas_height_ = detail::read_be32(ihdr->data + 4);

    // Validate canvas dimensions
    uint64_t total_pixels = static_cast<uint64_t>(canvas_width_) * canvas_height_;
    if (total_pixels > APNGMINI_MAX_PIXELS || canvas_width_ == 0 || canvas_height_ == 0) {
	error_ = true;
	detail::throw_error("Canvas dimensions too large or zero");
	return;
    }

    // Save IHDR for reassembling frames later
    ihdr_chunk_.resize(12 + ihdr->length);
    std::memcpy(ihdr_chunk_.data(), data_ + 8, 12 + ihdr->length);

    // Read until first IDAT or fcTL to parse acTL and global chunks
    bool seen_fcTL = false;
    while (auto chunk = peek_chunk(offset_)) {
	if (chunk->type == detail::CHUNK_acTL) {
	    if (chunk->length == 8) {
		is_apng_ = true;
		num_frames_ = detail::read_be32(chunk->data);
		num_plays_ = detail::read_be32(chunk->data + 4);
	    }
	    offset_ += 12 + chunk->length;
	} else if (chunk->type == detail::CHUNK_fcTL) {
	    seen_fcTL = true;
	    break; // Stop parsing headers, leave offset at first fcTL
	} else if (chunk->type == detail::CHUNK_IDAT) {
	    break; // Stop parsing headers, leave offset at IDAT
	} else {
	    // It's some ancillary chunk (PLTE, tRNS, etc.)
	    // Save it so we can prepend it to individual frames.
	    size_t start = global_ancillary_chunks_.size();
	    global_ancillary_chunks_.resize(start + 12 + chunk->length);
	    std::memcpy(global_ancillary_chunks_.data() + start, data_ + offset_, 12 + chunk->length);
	    offset_ += 12 + chunk->length;
	}
    }

    if (!is_apng_) {
	// Plain PNG, treat as 1-frame animation
	num_frames_ = 1;
	num_plays_ = 1;
	default_image_is_first_frame_ = true;
    } else {
	default_image_is_first_frame_ = seen_fcTL;
    }
}

bool Reader::has_next() const
{
    return !error_ && frames_read_ < num_frames_;
}

Frame Reader::next_frame()
{
    if (error_ || frames_read_ >= num_frames_) {
	return {};
    }

    Frame meta;
    apngmini::vector<uint8_t> frame_compressed_data;

    if (!is_apng_) {
	// Read all IDAT chunks for the plain PNG
	meta.width = canvas_width_;
	meta.height = canvas_height_;
	while (auto chunk = read_chunk()) {
	    if (chunk->type == detail::CHUNK_IDAT) {
		frame_compressed_data.insert(frame_compressed_data.end(), chunk->data, chunk->data + chunk->length);
	    } else if (chunk->type == detail::CHUNK_IEND) {
		break;
	    }
	}
    } else {
	// Read APNG frame
	if (frames_read_ == 0 && !default_image_is_first_frame_) {
	    // The default image is not part of the animation, skip its IDATs
	    while (auto chunk = peek_chunk(offset_)) {
		if (chunk->type == detail::CHUNK_IDAT) {
		    offset_ += 12 + chunk->length;
		} else {
		    break;
		}
	    }
	}

	// We expect an fcTL chunk
	auto fctl_chunk = read_chunk();
	if (!fctl_chunk || fctl_chunk->type != detail::CHUNK_fcTL || fctl_chunk->length != 26) {
	    error_ = true;
	    detail::throw_error("Expected fcTL chunk");
	    return {};
	}

	// Parse fcTL
	uint32_t seq = detail::read_be32(fctl_chunk->data);
	if (seq != expected_seq_++) {
	    error_ = true;
	    detail::throw_error("Invalid fcTL sequence number");
	    return {};
	}
	meta.width = detail::read_be32(fctl_chunk->data + 4);
	meta.height = detail::read_be32(fctl_chunk->data + 8);
	meta.x_offset = detail::read_be32(fctl_chunk->data + 12);
	meta.y_offset = detail::read_be32(fctl_chunk->data + 16);

	if (meta.width == 0 || meta.height == 0 ||
	    static_cast<uint64_t>(meta.x_offset) + meta.width > canvas_width_ ||
	    static_cast<uint64_t>(meta.y_offset) + meta.height > canvas_height_) {
	    error_ = true;
	    detail::throw_error("Frame dimensions out of bounds");
	    return {};
	}
	meta.delay.numerator = detail::read_be16(fctl_chunk->data + 20);
	meta.delay.denominator = detail::read_be16(fctl_chunk->data + 22);
	uint8_t raw_dispose = fctl_chunk->data[24];
	uint8_t raw_blend = fctl_chunk->data[25];
	meta.dispose_op = (raw_dispose <= 2) ? static_cast<DisposeOp>(raw_dispose) : DisposeOp::None;
	meta.blend_op = (raw_blend <= 1) ? static_cast<BlendOp>(raw_blend) : BlendOp::Source;

	if (frames_read_ == 0 && meta.dispose_op == DisposeOp::Previous) {
	    meta.dispose_op = DisposeOp::Background; // Per spec
	}

	// Read image data chunks
	while (auto chunk = peek_chunk(offset_)) {
	    if (chunk->type == detail::CHUNK_IDAT) {
		if (frames_read_ == 0 && default_image_is_first_frame_) {
		    frame_compressed_data.insert(frame_compressed_data.end(), chunk->data, chunk->data + chunk->length);
		    offset_ += 12 + chunk->length;
		} else {
		    break; // IDAT belongs to something else or is unexpected
		}
	    } else if (chunk->type == detail::CHUNK_fdAT) {
		if (chunk->length >= 4) { // Strip 4-byte sequence number
		    uint32_t fdat_seq = detail::read_be32(chunk->data);
		    if (fdat_seq != expected_seq_++) {
			error_ = true;
			detail::throw_error("Invalid fdAT sequence number");
			return {};
		    }
		    frame_compressed_data.insert(frame_compressed_data.end(), chunk->data + 4, chunk->data + chunk->length);
		}
		offset_ += 12 + chunk->length;
	    } else if (chunk->type == detail::CHUNK_fcTL || chunk->type == detail::CHUNK_IEND) {
		break; // Start of next frame or end of file
	    } else {
		// Skip unknown chunk
		offset_ += 12 + chunk->length;
	    }
	}
    }

    frames_read_++;
    return decode_frame_data(frame_compressed_data, meta.width, meta.height, meta);
}

Frame Reader::decode_frame_data(const apngmini::vector<uint8_t>& frame_data, uint32_t width, uint32_t height, const Frame& metadata)
{
    if (frame_data.empty()) {
	error_ = true;
	detail::throw_error("Empty frame data");
	return metadata;
    }

    // Build a complete PNG stream in memory
    apngmini::vector<uint8_t> png_stream;
    png_stream.reserve(8 + ihdr_chunk_.size() + global_ancillary_chunks_.size() + 12 + frame_data.size() + 12);

    // Signature
    png_stream.insert(png_stream.end(), {137, 80, 78, 71, 13, 10, 26, 10});

    // IHDR (patched with frame dimensions)
    size_t ihdr_start = png_stream.size();
    png_stream.insert(png_stream.end(), ihdr_chunk_.begin(), ihdr_chunk_.end());
    detail::write_be32(&png_stream[ihdr_start + 8], width);
    detail::write_be32(&png_stream[ihdr_start + 12], height);
    uint32_t new_crc = detail::crc32_data(&png_stream[ihdr_start + 4], 17);
    detail::write_be32(&png_stream[ihdr_start + 21], new_crc);

    // Global ancillary chunks
    png_stream.insert(png_stream.end(), global_ancillary_chunks_.begin(), global_ancillary_chunks_.end());

    // IDAT
    detail::write_chunk(png_stream, detail::CHUNK_IDAT, frame_data.data(), static_cast<uint32_t>(frame_data.size()));

    // IEND
    detail::write_chunk(png_stream, detail::CHUNK_IEND, nullptr, 0);

    auto decoded = APNGMINI_PNG_BACKEND::decode_png(png_stream.data(), png_stream.size());
    Frame frame = metadata;
    if (decoded) {
	frame.pixels = std::move(decoded->pixels);
    } else {
	error_ = true;
	detail::throw_error("Failed to decode frame PNG data");
    }
    return frame;
}

Animation read(const uint8_t* data, size_t size)
{
    Reader reader(data, size);
    Animation anim;
    if (reader.error()) return anim;
    anim.canvas_width = reader.canvas_width();
    anim.canvas_height = reader.canvas_height();
    anim.num_plays = reader.num_plays();

    while (reader.has_next()) {
	anim.frames.push_back(reader.next_frame());
    }

    if (reader.error()) {
	anim.frames.clear();
    }
    return anim;
}

Animation read(const apngmini::vector<uint8_t>& data)
{
    return read(data.data(), data.size());
}

Animation read_file(const char* path)
{
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) {
	detail::throw_error("Could not open file for reading");
	return {};
    }
    auto fsize = file.tellg();
    if (fsize < 0) {
	detail::throw_error("Could not determine file size");
	return {};
    }
    auto size = static_cast<size_t>(fsize);
    file.seekg(0, std::ios::beg);
    apngmini::vector<uint8_t> buffer(size);
    if (!file.read(reinterpret_cast<char*>(buffer.data()), static_cast<std::streamsize>(size))) {
	detail::throw_error("Failed to read file contents");
	return {};
    }
    return read(buffer);
}

Animation read_file(const std::string& path)
{
    return read_file(path.c_str());
}

} // namespace apngmini

#endif // APNGMINI_IMPL_GUARD
#endif // APNGMINI_IMPLEMENTATION

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
