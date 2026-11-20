/* M I M E _ T A R . H
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
#ifndef BU_MIME_TAR_H
#define BU_MIME_TAR_H
#include "common.h"
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <limits>
#include <string>
#include <sstream>
#include "bu/mime_runtime.h"
#include "mime_xml.h"

namespace mime_tar {
constexpr const char *type = "application/x-tar";
constexpr size_t block_size = 512;
constexpr size_t name_size = 100;
constexpr size_t size_offset = 124, size_width = 12;
constexpr size_t checksum_offset = 148, checksum_width = 8;
constexpr size_t kind_offset = 156, magic_offset = 257;
constexpr size_t prefix_offset = 345;

inline bool octal(const unsigned char *data, size_t size, uint64_t &value)
{
    value = 0;
    size_t at = 0;
    while (at < size && data[at] == ' ') ++at;
    const size_t begin = at;
    while (at < size && data[at] >= '0' && data[at] <= '7') {
	constexpr unsigned int radix = 8;
	const unsigned int digit = data[at++] - '0';
	if (value > (std::numeric_limits<uint64_t>::max() - digit) / radix) return false;
	value = value * radix + digit;
    }
    if (at == begin) return false;
    while (at < size && (data[at] == ' ' || data[at] == 0)) ++at;
    return at == size;
}

struct profile {
    bool identified = false;
    bool incomplete = false;
    std::string member_name, member_type;

    bool append_traits(bu_mime_traits &traits) const
    {
	if (!identified) return false;
	return bu_mime_traits_add(&traits, "container", "tar") &&
	    bu_mime_traits_add(&traits, "encoding", "binary") &&
	    (member_type.empty() || bu_mime_traits_add(&traits, "tar.first-member-type", member_type.c_str()));
    }
};

/* Inspect only the initial ustar/GNU header and its direct regular member.
 * Extended names, preceding directory entries and later members require an
 * archive reader. They cannot establish this reader's MRF entrypoint. */
inline profile inspect(const unsigned char *data, size_t size, bool complete)
{
    profile out;
    if (!data || size < block_size) return out;
    const bool posix = !memcmp(data + magic_offset, "ustar\0" "00", 8);
    const bool gnu = !memcmp(data + magic_offset, "ustar  \0", 8);
    if (!posix && !gnu) return out;
    uint64_t expected = 0, checksum = 0, member_size = 0;
    if (!octal(data + checksum_offset, checksum_width, expected) ||
	!octal(data + size_offset, size_width, member_size)) return out;
    for (size_t at = 0; at < block_size; ++at)
	checksum += at >= checksum_offset && at < checksum_offset + checksum_width ? ' ' : data[at];
    if (checksum != expected) return out;
    out.identified = true;
    out.member_name.assign(reinterpret_cast<const char *>(data), name_size);
    const size_t end = out.member_name.find('\0');
    if (end != std::string::npos) out.member_name.resize(end);
    const bool prefix = posix && data[prefix_offset] != 0;
    if (prefix || out.member_name.empty() || out.member_name.find('/') != std::string::npos ||
	(data[kind_offset] != 0 && data[kind_offset] != '0')) return out;
    const size_t available = size - block_size;
    if (member_size > available && complete) return out;
    const size_t member_bytes = static_cast<size_t>(std::min<uint64_t>(member_size, available));
    std::istringstream member(std::string(reinterpret_cast<const char *>(data + block_size), member_bytes));
    const auto xml = mime_xml::inspect(member);
    if (xml.type()) out.member_type = xml.type();
    out.incomplete = xml.plausible && !xml.type() && member_size > available && !complete;
    return out;
}
} // namespace mime_tar
#endif
