/* M I M E _ A V I . H
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
#ifndef BU_MIME_AVI_H
#define BU_MIME_AVI_H
#include "common.h"
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <istream>
#include <string>
#include "bu/mime_runtime.h"
#include "mime_binary.h"

/* Inspect the RIFF/AVI header list only. A container signature does not
 * identify its video handler; stream order matters to readers using 00dc. */
namespace mime_avi {
inline uint32_t u32(const unsigned char *data)
{
    return mime_binary::little_u32(data);
}
inline bool read(std::istream &input, void *data, size_t size)
{
    input.read(static_cast<char *>(data), static_cast<std::streamsize>(size));
    return static_cast<size_t>(input.gcount()) == size;
}
inline bool skip(std::istream &input, uint64_t size)
{
    input.ignore(static_cast<std::streamsize>(size));
    return static_cast<uint64_t>(input.gcount()) == size;
}

struct profile {
    bool identified = false, complete = false;
    size_t streams = 0, video_streams = 0, video_index = 0;
    uint32_t handler_id = 0;
    std::string handler;
    bool append_traits(bu_mime_traits &traits) const
    {
	if (!identified || !bu_mime_traits_add(&traits, "encoding", "binary") ||
	    !bu_mime_traits_add(&traits, "container", "avi") ||
	    !bu_mime_traits_add(&traits, "byte-order", "little-endian")) return false;
	if (!complete) return true;
	if (!bu_mime_traits_add(&traits, "avi.streams", std::to_string(streams).c_str()) ||
	    !bu_mime_traits_add(&traits, "avi.video-streams", std::to_string(video_streams).c_str())) return false;
	return video_streams != 1 || (bu_mime_traits_add(&traits, "avi.video-stream-index", std::to_string(video_index).c_str()) &&
	    bu_mime_traits_add(&traits, "avi.video-handler-id", std::to_string(handler_id).c_str()) &&
	    (handler.empty() || bu_mime_traits_add(&traits, "avi.video-handler", handler.c_str())));
    }
};

struct chunk {
    char tag[sizeof(uint32_t)] = {};
    uint32_t size = 0;
    uint64_t padded = 0;
    bool next(std::istream &input, uint64_t &remaining)
    {
	unsigned char header[2 * sizeof(uint32_t)];
	if (remaining < sizeof(header) || !read(input, header, sizeof(header))) return false;
	memcpy(tag, header, sizeof(tag));
	size = u32(header + sizeof(uint32_t));
	padded = uint64_t(size) + (size & 1);
	remaining -= sizeof(header);
	if (padded > remaining) return false;
	remaining -= padded;
	return true;
    }
    bool is(const char *type) const {return !memcmp(tag, type, sizeof(tag));}
};

inline bool stream_header(std::istream &input, uint64_t remaining, profile &facts)
{
    bool found = false;
    while (remaining) {
	chunk item;
	if (!item.next(input, remaining)) return false;
	if (item.is("strh")) {
	    unsigned char stream[2 * sizeof(uint32_t)];
	    if (found || item.size < sizeof(stream) || !read(input, stream, sizeof(stream))) return false;
	    found = true;
	    if (!memcmp(stream, "vids", sizeof(uint32_t))) {
		++facts.video_streams;
		facts.video_index = facts.streams;
		facts.handler_id = u32(stream + sizeof(uint32_t));
		const char *handler = reinterpret_cast<const char *>(stream + sizeof(uint32_t));
		if (std::all_of(handler, handler + sizeof(uint32_t), [](unsigned char byte) {return byte >= ' ' && byte <= '~';}))
		    facts.handler.assign(handler, sizeof(uint32_t));
		else facts.handler.clear();
	    }
	    if (!skip(input, item.padded - sizeof(stream))) return false;
	} else if (!skip(input, item.padded)) return false;
    }
    return found;
}

inline bool header_list(std::istream &input, uint64_t remaining, profile &facts)
{
    while (remaining) {
	chunk item;
	if (!item.next(input, remaining)) return false;
	if (item.is("LIST")) {
	    char type[sizeof(uint32_t)];
	    if (item.size < sizeof(type) || !read(input, type, sizeof(type))) return false;
	    if (!memcmp(type, "strl", sizeof(type))) {
		if (!stream_header(input, item.size - sizeof(type), facts) || !skip(input, item.size & 1)) return false;
		++facts.streams;
	    } else if (!skip(input, item.padded - sizeof(type))) return false;
	} else if (!skip(input, item.padded)) return false;
    }
    return true;
}

inline profile inspect(std::istream &input)
{
    constexpr size_t form_header_size = 3 * sizeof(uint32_t);
    unsigned char header[form_header_size];
    profile facts;
    if (!read(input, header, sizeof(header)) || memcmp(header, "RIFF", sizeof(uint32_t)) ||
	memcmp(header + 2 * sizeof(uint32_t), "AVI ", sizeof(uint32_t))) return facts;
    const uint32_t size = u32(header + sizeof(uint32_t));
    if (size < sizeof(uint32_t)) return facts;
    facts.identified = true;
    uint64_t remaining = size - sizeof(uint32_t);
    while (remaining) {
	chunk item;
	if (!item.next(input, remaining)) break;
	if (item.is("LIST")) {
	    char type[sizeof(uint32_t)];
	    if (item.size < sizeof(type) || !read(input, type, sizeof(type))) break;
	    if (!memcmp(type, "hdrl", sizeof(type))) {
		facts.complete = header_list(input, item.size - sizeof(type), facts) && skip(input, item.size & 1);
		return facts;
	    }
	    if (!skip(input, item.padded - sizeof(type))) break;
	} else if (!skip(input, item.padded)) break;
    }
    return facts;
}
}
#endif
