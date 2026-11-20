/* M I M E _ T A R _ F I X T U R E . H
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
#ifndef MIME_TAR_FIXTURE_H
#define MIME_TAR_FIXTURE_H
#include "common.h"
#include <cstdio>
#include <string>

namespace mime_tar_fixture {
constexpr size_t block_size = 512;
enum class format {posix, gnu, empty_prefix_padding};

inline void checksum(std::string &header)
{
    constexpr size_t offset = 148, width = 8;
    header.replace(offset, width, width, ' ');
    unsigned int sum = 0;
    for (size_t at = 0; at < block_size; ++at) sum += static_cast<unsigned char>(header[at]);
    char field[width];
    snprintf(field, sizeof(field), "%06o", sum);
    header.replace(offset, width - 1, field, width - 1);
}

inline std::string entry(const std::string &name, const std::string &content, format kind = format::posix)
{
    std::string header(block_size, '\0');
    if (name.empty() || name.size() > 100) return {};
    header.replace(0, name.size(), name);
    header.replace(100, 8, "0000644\0", 8);
    header.replace(108, 8, "0000000\0", 8);
    header.replace(116, 8, "0000000\0", 8);
    char length[12];
    snprintf(length, sizeof(length), "%011llo", static_cast<unsigned long long>(content.size()));
    header.replace(124, sizeof(length), length, sizeof(length));
    header.replace(136, 12, "00000000000\0", 12);
    header[156] = '0';
    header.replace(257, 8, kind == format::gnu ? "ustar  \0" : "ustar\0" "00", 8);
    if (kind == format::empty_prefix_padding) {
	/* The prefix is a C string; bytes after its first NUL are padding. */
	constexpr size_t prefix_offset = 345;
	header[prefix_offset + 1] = 'p';
    }
    checksum(header);
    const size_t padding = (block_size - content.size() % block_size) % block_size;
    return header + content + std::string(padding, '\0');
}
} // namespace mime_tar_fixture
#endif
