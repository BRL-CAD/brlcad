/* M I M E _ H E I G H T F I E L D _ F I X T U R E . H
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
#ifndef BU_MIME_HEIGHTFIELD_FIXTURE_H
#define BU_MIME_HEIGHTFIELD_FIXTURE_H
#include "common.h"
#include <cstdint>
#include <string>
#include "../mime_byn.h"

namespace mime_heightfield_fixture {
inline void word(std::string &bytes, size_t at, uint64_t value, size_t size, bool little = true)
{
    for (size_t i = 0; i < size; ++i)
	bytes[at + i] = static_cast<char>(value >> (8 * (little ? i : size - i - 1)));
}

inline std::string byn(size_t sample_size, bool little, bool unit_scale = true)
{
    constexpr size_t samples = 4;
    std::string bytes(mime_byn::header_size + samples * sample_size, 0);
    /* Four geographic nodes, spaced by 36 arcseconds near 50 N, 10 E. */
    word(bytes, 0, 179964, sizeof(uint32_t));
    word(bytes, 4, 180000, sizeof(uint32_t));
    word(bytes, 8, 36000, sizeof(uint32_t));
    word(bytes, 12, 36036, sizeof(uint32_t));
    word(bytes, 16, 36, sizeof(uint16_t));
    word(bytes, 18, 36, sizeof(uint16_t));
    word(bytes, 22, 5, sizeof(uint16_t)); // DEM data kind.
    /* Little-endian IEEE factor 1 or 2; payload byte order is independent. */
    bytes[30] = unit_scale ? static_cast<char>(0xf0) : 0;
    bytes[31] = unit_scale ? 0x3f : 0x40;
    word(bytes, mime_byn::sample_size_offset, static_cast<uint32_t>(sample_size), sizeof(uint16_t));
    word(bytes, 46, 1, sizeof(uint16_t)); // WGS84 ellipsoid.
    word(bytes, mime_byn::byte_order_offset, little ? 1 : 0, sizeof(uint16_t));
    for (size_t i = 0; i < samples; ++i)
	word(bytes, mime_byn::header_size + i * sample_size, static_cast<uint32_t>(i + 1), sample_size, little);
    return bytes;
}

inline std::string isg(const std::string &version, bool dms = false)
{
    const bool legacy = version == "1.0" || version == "1.01";
    std::string text = "Independent four-node geoid fixture\nbegin_of_head ====\nmodel name : fixture\nmodel type : gravimetric\n";
    text += legacy ? "units : meters\nreference : WGS84\n" :
	"data type : geoid\ndata units : meters\ndata format : grid\ndata ordering : N-to-S, W-to-E\nref ellipsoid : WGS84\ncoord type : geodetic\ncoord units : ";
    if (!legacy) text += dms ? "dms\n" : "deg\n";
    if (dms) {
	text += "lat min = 50\xc2\xb0" "00'00\"\nlat max = 50\xc2\xb0" "00'36\"\n";
	text += "lon min = 10\xc2\xb0" "00'00\"\nlon max = 10\xc2\xb0" "00'36\"\n";
	text += "delta lat = 0\xc2\xb0" "00'36\"\ndelta lon = 0\xc2\xb0" "00'36\"\n";
    } else {
	text += std::string("lat min = 50.0\nlat max = ") + (legacy ? "50.02" : "50.01") + "\nlon min = 10.0\nlon max = " +
	    (legacy ? "10.02" : "10.01") + "\ndelta lat = 0.01\ndelta lon = 0.01\n";
    }
    return text + "nrows = 2\nncols = 2\nnodata = -9999\nISG format = " + version + "\nend_of_head ====\n1 2\n3 4\n";
}
}
#endif
