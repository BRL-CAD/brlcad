/* I N F L A T E _ M I M E . H
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
#ifndef GCV_INFLATE_MIME_H
#define GCV_INFLATE_MIME_H
#include "common.h"
#include <algorithm>
#include <array>
#include <istream>
#include <limits>
#include <streambuf>
#include <vector>
#include <zlib.h>

namespace gcv_compression {
/* zlib reserves the added 16 bits to select gzip framing. */
enum class framing {zlib = MAX_WBITS, gzip = MAX_WBITS + 16};

/* Decode only the inspected prefix. Callers control the output limit and
 * stop reading when the format header is complete. */
class inflate_buffer : public std::streambuf {
    static constexpr size_t block_size = 4096;
    std::istream &source;
    z_stream decoder = {};
    std::array<unsigned char, block_size> input = {}, output = {};
    size_t remaining;
    bool initialized, ended = false, exhausted = false;
    int status;
protected:
    int_type underflow() override
    {
	if (gptr() && gptr() < egptr()) return traits_type::to_int_type(*gptr());
	if (!initialized || ended || failed() || !remaining) return traits_type::eof();
	for (;;) {
	    if (!decoder.avail_in) {
		source.read(reinterpret_cast<char *>(input.data()), input.size());
		if (source.bad()) {status = Z_DATA_ERROR; return traits_type::eof();}
		if (!source.gcount()) {exhausted = true; return traits_type::eof();}
		decoder.next_in = input.data();
		decoder.avail_in = static_cast<uInt>(source.gcount());
	    }
	    size_t capacity = std::min(remaining, output.size());
	    decoder.next_out = output.data();
	    decoder.avail_out = static_cast<uInt>(capacity);
	    status = inflate(&decoder, Z_NO_FLUSH);
	    if (failed()) return traits_type::eof();
	    ended = status == Z_STREAM_END;
	    size_t count = capacity - decoder.avail_out;
	    if (count) {
		remaining -= count;
		char *begin = reinterpret_cast<char *>(output.data());
		setg(begin, begin, begin + count);
		return traits_type::to_int_type(*gptr());
	    }
	    if (ended) return traits_type::eof();
	    if (status == Z_BUF_ERROR && decoder.avail_in) {status = Z_DATA_ERROR; return traits_type::eof();}
	}
    }
public:
    explicit inflate_buffer(std::istream &stream, framing format, size_t limit = std::numeric_limits<size_t>::max()) :
	source(stream), remaining(limit), initialized(false), status(inflateInit2(&decoder, static_cast<int>(format)))
    {
	initialized = status == Z_OK;
    }
    ~inflate_buffer() override {if (initialized) inflateEnd(&decoder);}
    inflate_buffer(const inflate_buffer &) = delete;
    inflate_buffer &operator=(const inflate_buffer &) = delete;
    bool failed() const {return status != Z_OK && status != Z_STREAM_END && status != Z_BUF_ERROR;}
    bool incomplete() const {return !ended && (!remaining || exhausted);}
    bool internal_error() const {return status == Z_MEM_ERROR || status == Z_VERSION_ERROR || status == Z_STREAM_ERROR;}
};

struct decoded_prefix {
    std::vector<unsigned char> data;
    bool compressed = false, error = false, incomplete = false;
};

/* Both raw and gzip representations share the same bounded format-header
 * inspectors. A gzip wrapper alone never establishes the payload type. */
inline decoded_prefix gzip_prefix(std::istream &source, size_t limit, bool complete)
{
    decoded_prefix result;
    result.data.resize(limit);
    const auto read = [&](std::istream &input) {
	input.read(reinterpret_cast<char *>(result.data.data()), result.data.size());
	result.data.resize(static_cast<size_t>(input.gcount()));
    };
    result.compressed = source.peek() == 0x1f;
    if (!result.compressed) {read(source); return result;}
    inflate_buffer buffer(source, framing::gzip, limit);
    std::istream decoded(&buffer);
    read(decoded);
    result.error = buffer.internal_error();
    result.incomplete = !complete && buffer.incomplete();
    if (buffer.failed()) result.data.clear();
    return result;
}

}
#endif
