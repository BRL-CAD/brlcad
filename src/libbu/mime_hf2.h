/* M I M E _ H F 2 . H
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
#ifndef BU_MIME_HF2_H
#define BU_MIME_HF2_H
#include "common.h"
#include <algorithm>
#include <cstring>
#include <string>
#include "bu/mime_runtime.h"
#include "mime_binary.h"

namespace mime_hf2 {
constexpr const char *type = "image/vnd.bundysoft.hf2";
constexpr unsigned char signature[] = {'H', 'F', '2', 0};
constexpr size_t version_offset = sizeof(signature), header_size = 28;

struct profile {
    bool identified = false, incomplete = false, revision_known = false;
    unsigned int revision = 0;
    bool append_traits(bu_mime_traits &traits, const char *compression = "none") const
    {
	return identified && bu_mime_traits_add(&traits, "encoding", "binary") &&
	    bu_mime_traits_add(&traits, "compression", compression) &&
	    (!revision_known || bu_mime_traits_add(&traits, "version", std::to_string(revision).c_str())) &&
	    (!revision_known || revision != 0 ||
		(bu_mime_traits_add(&traits, "byte-order", "little-endian") &&
		 bu_mime_traits_add(&traits, "hf2.sample-encoding", "scaled-integer-deltas")));
    }
};

inline profile inspect(const unsigned char *data, size_t size, bool complete)
{
    profile out;
    if (!data || !size || memcmp(data, signature, std::min(size, sizeof(signature)))) return out;
    if (size < sizeof(signature)) { out.incomplete = !complete; return out; }
    out.identified = true;
    if (size < version_offset + sizeof(uint16_t)) { out.incomplete = !complete; return out; }
    out.revision_known = true;
    out.revision = mime_binary::little_u16(data + version_offset);
    return out;
}
}
#endif
