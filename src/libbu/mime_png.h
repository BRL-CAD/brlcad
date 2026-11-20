/* M I M E _ P N G . H
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
#ifndef BU_MIME_PNG_H
#define BU_MIME_PNG_H
#include "common.h"
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <string>
#include "bu/mime_runtime.h"
#include "mime_binary.h"

/* Observe only the header preceding IDAT. CRCs and compressed data remain
 * the decoder's responsibility. Feeding decoder reads avoids buffering or
 * seeking through potentially large ancillary chunks a second time. */
namespace mime_png {
enum color_type {grayscale = 0, truecolor = 2, indexed = 3, grayscale_alpha = 4, truecolor_alpha = 6};

inline uint32_t u32(const unsigned char *data)
{
    return mime_binary::big_u32(data);
}

struct profile {
    bool header_known = false, animated = false, animation_known = false;
    bool default_known = false, default_is_frame = false;
    uint32_t width = 0, height = 0, frames = 0, plays = 0;
    unsigned depth = 0, color = 0, compression = 0, filter = 0, interlace = 0;

    bool standard_header() const
    {
	bool valid_depth = depth == 8 || depth == 16;
	if (color == grayscale || color == indexed)
	    valid_depth = valid_depth || depth == 1 || depth == 2 || depth == 4;
	const bool valid_color = color == grayscale || color == truecolor || color == indexed ||
	    color == grayscale_alpha || color == truecolor_alpha;
	return header_known && width && height && width <= INT32_MAX && height <= INT32_MAX &&
	    valid_depth && valid_color && !(color == indexed && depth == 16) &&
	    !compression && !filter && interlace <= 1;
    }

    bool append_traits(bu_mime_traits &traits) const
    {
	if (!header_known) return false;
	if (!bu_mime_traits_add(&traits, "encoding", "binary") ||
	    !bu_mime_traits_add(&traits, "bit-depth", std::to_string(depth).c_str()) ||
	    !bu_mime_traits_add(&traits, "png.color-type", std::to_string(color).c_str()) ||
	    !bu_mime_traits_add(&traits, "png.compression-method", std::to_string(compression).c_str()) ||
	    !bu_mime_traits_add(&traits, "png.filter-method", std::to_string(filter).c_str()) ||
	    !bu_mime_traits_add(&traits, "png.interlace-method", std::to_string(interlace).c_str())) return false;
	if (animation_known && !bu_mime_traits_add(&traits, "png.animated", animated ? "true" : "false")) return false;
	if (animated && (!bu_mime_traits_add(&traits, "apng.frames", std::to_string(frames).c_str()) ||
	    !bu_mime_traits_add(&traits, "apng.plays", std::to_string(plays).c_str()))) return false;
	return !animated || !default_known || bu_mime_traits_add(&traits, "apng.default-image", default_is_frame ? "frame" : "fallback");
    }
    bool matches(const bu_mime_traits *requested, const bu_mime_traits *supplemental = NULL) const
    {
	bu_mime_traits observed = BU_MIME_TRAITS_INIT_ZERO;
	const bool valid = append_traits(observed) && bu_mime_traits_match(&observed, supplemental, requested);
	bu_mime_traits_free(&observed);
	return valid;
    }
};

class inspector {
    enum class phase { signature, chunk_header, chunk_body, stopped } state = phase::signature;
    static constexpr size_t chunk_header_size = 8;
    static constexpr size_t checksum_size = 4;
    static constexpr size_t image_header_size = 13;
    static constexpr size_t animation_control_size = 8;
    static constexpr size_t frame_control_size = 26;
    unsigned char saved[frame_control_size + checksum_size] = {};
    char type[4] = {};
    size_t used = 0;
    uint32_t length = 0;
    uint64_t remaining = 0;
    bool first = true;

    void finish_chunk()
    {
	if (first) {
	    facts.width = u32(saved);
	    facts.height = u32(saved + sizeof(uint32_t));
	    /* IHDR's two dimensions are followed by five one-byte fields. */
	    facts.depth = saved[8];
	    facts.color = saved[9];
	    facts.compression = saved[10];
	    facts.filter = saved[11];
	    facts.interlace = saved[12];
	    facts.header_known = true;
	} else if (!memcmp(type, "acTL", sizeof(type))) {
	    if (length != animation_control_size || facts.animated || !u32(saved)) {state = phase::stopped; return;}
	    facts.frames = u32(saved);
	    facts.plays = u32(saved + sizeof(uint32_t));
	    facts.animated = facts.animation_known = true;
	} else if (!memcmp(type, "fcTL", sizeof(type))) {
	    if (length != frame_control_size || !facts.animated) {state = phase::stopped; return;}
	    facts.default_is_frame = true;
	}
	first = false;
	used = 0;
	state = phase::chunk_header;
    }
public:
    profile facts;
    bool finished() const {return state == phase::stopped;}

    void feed(const unsigned char *data, size_t size)
    {
	static const unsigned char signature[] = {137, 'P', 'N', 'G', 13, 10, 26, 10};
	while (size && !finished()) {
	    if (state == phase::chunk_body) {
		const size_t take = static_cast<size_t>(std::min<uint64_t>(remaining, size));
		const size_t keep = std::min(take, sizeof(saved) - used);
		if (keep) memcpy(saved + used, data, keep);
		used += keep;
		data += take;
		size -= take;
		remaining -= take;
		if (!remaining) finish_chunk();
		continue;
	    }
	    const size_t needed = state == phase::signature ? sizeof(signature) : chunk_header_size;
	    const size_t take = std::min(size, needed - used);
	    memcpy(saved + used, data, take);
	    used += take;
	    data += take;
	    size -= take;
	    if (used != needed) continue;
	    if (state == phase::signature) {
		state = memcmp(saved, signature, sizeof(signature)) ? phase::stopped : phase::chunk_header;
	    } else {
		length = u32(saved);
		memcpy(type, saved + sizeof(uint32_t), sizeof(type));
		if (first && (length != image_header_size || memcmp(type, "IHDR", sizeof(type)))) state = phase::stopped;
		else if (!memcmp(type, "IDAT", sizeof(type))) {
		    facts.animation_known = facts.default_known = true;
		    state = phase::stopped;
		} else if (!memcmp(type, "IEND", sizeof(type))) state = phase::stopped;
		else {remaining = uint64_t(length) + checksum_size; state = phase::chunk_body;}
	    }
	    used = 0;
	}
    }
};
}
#endif
