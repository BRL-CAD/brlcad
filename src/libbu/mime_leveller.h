/* M I M E _ L E V E L L E R . H
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
#ifndef BU_MIME_LEVELLER_H
#define BU_MIME_LEVELLER_H
#include "common.h"
#include <string>
#include "bu/mime_runtime.h"

namespace mime_leveller {
constexpr const char *type = "application/leveller";
constexpr size_t signature_size = 4, header_size = 5;
constexpr unsigned int first_revision = 4, first_float_revision = 6, last_revision = 12;

struct profile {
    bool identified = false, incomplete = false, revision_known = false;
    unsigned int revision = 0;

    bool append_traits(bu_mime_traits &traits) const
    {
	if (!identified) return false;
	if (!bu_mime_traits_add(&traits, "encoding", "binary")) return false;
	if (!revision_known) return true;
	if (!bu_mime_traits_add(&traits, "version", std::to_string(revision).c_str())) return false;
	if (revision < first_revision || revision > last_revision) return true;
	const bool fixed = revision < first_float_revision;
	return bu_mime_traits_add(&traits, "byte-order", "little-endian") &&
	    bu_mime_traits_add(&traits, "bit-depth", "32") &&
	    bu_mime_traits_add(&traits, "sample-format", fixed ? "signed-fixed-point" : "ieee754") &&
	    (!fixed || bu_mime_traits_add(&traits, "leveller.fractional-bits", "16"));
    }
};

inline profile inspect(const unsigned char *data, size_t size, bool complete)
{
    profile out;
    if (!data) return out;
    constexpr char signature[] = "trrn";
    for (size_t at = 0; at < size && at < signature_size; ++at)
	if (data[at] != static_cast<unsigned char>(signature[at]) && data[at] != static_cast<unsigned char>(signature[at] - 'a' + 'A')) return out;
    if (size < signature_size) { out.incomplete = !complete; return out; }
    out.identified = true;
    if (size < header_size) { out.incomplete = !complete; return out; }
    out.revision_known = true;
    out.revision = data[signature_size];
    return out;
}
} // namespace mime_leveller
#endif
