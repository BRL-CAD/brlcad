/* M I M E _ 3 D S . H
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 * See the file named COPYING for more information.
 */
#ifndef BU_MIME_3DS_H
#define BU_MIME_3DS_H
#include "common.h"
#include "bu/mime.h"
#include "mime_binary.h"

namespace mime_3ds {
inline constexpr const char *type = "image/x-3ds";
constexpr uint16_t main_chunk = 0x4d4d, project_chunk = 0xc23d;
constexpr size_t header_size = sizeof(uint16_t) + sizeof(uint32_t);

struct profile {
    bool identified = false, incomplete = false;
    bool append_traits(bu_mime_traits &traits) const
    {
	return identified && bu_mime_traits_add(&traits, "encoding", "binary") &&
	    bu_mime_traits_add(&traits, "byte-order", "little-endian");
    }
};

/* A root chunk establishes the binary representation. Its ID is not a
 * format revision, and optional version chunks need separate evidence. */
inline profile inspect(const bu_mime_probe_input &input)
{
    profile result;
    if (!input.size) return result;
    if (input.size < sizeof(uint16_t)) {
	result.incomplete = !input.complete && (input.data[0] == (main_chunk & 0xff) ||
	    input.data[0] == (project_chunk & 0xff));
	return result;
    }
    const uint16_t id = mime_binary::little_u16(input.data);
    if (id != main_chunk && id != project_chunk) return result;
    if (input.size < header_size) {
	result.incomplete = !input.complete;
	return result;
    }
    const uint32_t length = mime_binary::little_u32(input.data + sizeof(uint16_t));
    if (length < header_size || (input.file_size_known && length > input.file_size) ||
	(input.complete && length > input.size)) return result;
    result.identified = true;
    return result;
}
}
#endif
