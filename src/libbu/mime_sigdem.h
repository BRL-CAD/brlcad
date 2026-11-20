/* M I M E _ S I G D E M . H
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
#ifndef BU_MIME_SIGDEM_H
#define BU_MIME_SIGDEM_H
#include "common.h"
#include <algorithm>
#include <cstring>
#include <string>
#include "bu/mime_runtime.h"
#include "mime_binary.h"

namespace mime_sigdem {
constexpr const char *type = "image/x-revolsys-sigdem";
constexpr char signature[] = "SIGDEM";
constexpr size_t version_offset = sizeof(signature) - 1, header_size = 132;

struct profile {
    bool identified = false, incomplete = false, revision_known = false;
    unsigned int revision = 0;
    bool append_traits(bu_mime_traits &traits) const
    {
	return identified && bu_mime_traits_add(&traits, "encoding", "binary") &&
	    bu_mime_traits_add(&traits, "compression", "none") &&
	    (!revision_known || bu_mime_traits_add(&traits, "version", std::to_string(revision).c_str())) &&
	    (!revision_known || revision != 1 ||
		(bu_mime_traits_add(&traits, "byte-order", "big-endian") &&
		 bu_mime_traits_add(&traits, "bit-depth", "32") &&
		 bu_mime_traits_add(&traits, "sample-format", "signed-integer") &&
		 bu_mime_traits_add(&traits, "sigdem.sample-encoding", "offset-scaled-integer")));
    }
};

inline profile inspect(const unsigned char *data, size_t size, bool complete)
{
    profile out;
    if (!data || !size || memcmp(data, signature, std::min(size, version_offset))) return out;
    if (size < version_offset) { out.incomplete = !complete; return out; }
    out.identified = true;
    if (size < version_offset + sizeof(uint16_t)) { out.incomplete = !complete; return out; }
    out.revision_known = true;
    out.revision = mime_binary::big_u16(data + version_offset);
    return out;
}
}
#endif
