/* M I M E _ B I N A R Y . H
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
#ifndef BU_MIME_BINARY_H
#define BU_MIME_BINARY_H
#include "common.h"
#include <cstddef>
#include <cstdint>
namespace mime_binary {
inline uint16_t little_u16(const unsigned char *p)
{
    return uint16_t(p[0]) | (uint16_t(p[1]) << 8);
}
inline uint32_t little_u32(const unsigned char *p)
{
    return uint32_t(p[0]) | (uint32_t(p[1]) << 8) | (uint32_t(p[2]) << 16) | (uint32_t(p[3]) << 24);
}
inline uint16_t big_u16(const unsigned char *p)
{
    return (uint16_t(p[0]) << 8) | uint16_t(p[1]);
}
inline uint32_t big_u32(const unsigned char *p)
{
    return (uint32_t(p[0]) << 24) | (uint32_t(p[1]) << 16) | (uint32_t(p[2]) << 8) | uint32_t(p[3]);
}
inline uint64_t big_u64(const unsigned char *p)
{
    uint64_t value = 0;
    for (size_t i = 0; i < sizeof(value); ++i) value = (value << 8) | p[i];
    return value;
}
inline uint64_t little_u64(const unsigned char *p)
{
    uint64_t value = 0;
    for (size_t i = 0; i < sizeof(value); ++i) value |= uint64_t(p[i]) << (8 * i);
    return value;
}
} // namespace mime_binary
#endif
