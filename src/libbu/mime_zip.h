/*          M I M E _ Z I P . H
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

#ifndef BU_MIME_ZIP_H
#define BU_MIME_ZIP_H
#include "common.h"
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <limits>
#include <string>
#include <vector>

namespace mime_zip {
/* Use only with a successful archive-capable decoder, not as a ZIP
 * validator. The bounded footer check also handles archive preambles,
 * ZIP64 sentinels and trailing bytes accepted by the SDK. It establishes
 * the outer container without interpreting its selected XML member. */
inline bool end_record(const std::string &filename)
{
    constexpr size_t end_size = 22;
    constexpr unsigned char signature[] = {'P', 'K', 5, 6};
    constexpr size_t disks = sizeof(signature);
    constexpr size_t comment_length = end_size - sizeof(uint16_t);
    std::ifstream stream(filename, std::ios::binary);
    if (!stream.is_open()) return false;
    stream.seekg(0, std::ios::end);
    std::streamoff length = stream.tellg();
    if (length < static_cast<std::streamoff>(end_size)) return false;
    size_t size = static_cast<size_t>(std::min<std::streamoff>(length, end_size + std::numeric_limits<uint16_t>::max()));
    std::vector<unsigned char> tail(size);
    stream.seekg(length - static_cast<std::streamoff>(size));
    stream.read(reinterpret_cast<char *>(tail.data()), static_cast<std::streamsize>(size));
    if (!stream) return false;
    for (size_t index = tail.size() - end_size + 1; index;) {
	size_t at = --index;
	if (memcmp(tail.data() + at, signature, sizeof(signature))) continue;
	auto word = [&](size_t offset) {return uint16_t(tail[at + offset]) | (uint16_t(tail[at + offset + 1]) << 8);};
	if (!word(disks) && !word(disks + sizeof(uint16_t)) &&
	    static_cast<size_t>(word(comment_length)) <= tail.size() - at - end_size) return true;
    }
    return false;
}
}
#endif
