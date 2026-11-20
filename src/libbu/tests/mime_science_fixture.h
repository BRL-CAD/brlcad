/* M I M E _ S C I E N C E _ F I X T U R E . H
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
#ifndef BU_MIME_SCIENCE_FIXTURE_H
#define BU_MIME_SCIENCE_FIXTURE_H
#include "common.h"
#include <cstdint>
#include <string>
#include "mime_heightfield_fixture.h"

namespace mime_science_fixture {
inline std::string noaa(int kind = 1, bool little = false)
{
    const size_t sample_size = kind == 2 || kind == -1 ? 2 : 4, record_size = 2 * sample_size;
    std::string bytes(52 + 2 * (record_size + 8), 0);
    const auto word = [&](size_t at, uint64_t value, size_t size) {mime_heightfield_fixture::word(bytes, at, value, size, little);};
    word(0, 44, 4);
    word(4, UINT64_C(0x4049000000000000), 8); // Southwest latitude 50.
    word(12, UINT64_C(0x4024000000000000), 8); // Southwest longitude 10.
    word(20, UINT64_C(0x3f847ae147ae147b), 8); // Grid spacing 0.01 degree.
    word(28, UINT64_C(0x3f847ae147ae147b), 8);
    word(36, 2, 4);
    word(40, 2, 4);
    word(44, static_cast<uint32_t>(kind), 4);
    word(48, 44, 4);
    const uint32_t real32[] = {0x3f800000, 0x40000000, 0x40400000, 0x40800000};
    for (size_t row = 0; row < 2; ++row) {
	const size_t at = 52 + row * (record_size + 8);
	word(at, record_size, 4);
	for (size_t col = 0; col < 2; ++col) {
	    const size_t index = (1 - row) * 2 + col;
	    word(at + 4 + col * sample_size, kind == 1 ? real32[index] : index + 1, sample_size);
	}
	word(at + 4 + record_size, record_size, 4);
    }
    return bytes;
}

inline std::string nsidc(bool south = true, const std::string &scale = "250")
{
    const size_t columns = south ? 316 : 304, rows = south ? 332 : 448;
    std::string bytes(300 + columns * rows, 0);
    const auto field = [&](size_t index, const std::string &value) {
	bytes.replace(index * 6, 5, std::string(5 - value.size(), ' ') + value);
    };
    field(0, "255"); field(1, std::to_string(columns)); field(2, std::to_string(rows));
    field(9, "SSMIS"); field(10, "18 cn"); field(17, "2022"); field(18, "99"); field(19, "0"); field(20, scale);
    const std::string information = south ? "ANTARCTIC concentration fixture" : "ARCTIC concentration fixture";
    bytes.replace(230, information.size(), information);
    for (size_t i = 300; i < bytes.size(); ++i) bytes[i] = static_cast<char>((i - 300) % 256);
    return bytes;
}

inline std::string iris(unsigned int product = 1, unsigned int quantity = 2, size_t bands = 1, bool little = true)
{
    const size_t width = quantity == 31 ? 4 : quantity == 8 || quantity == 9 || quantity == 33 || quantity == 37 ? 2 : 1;
    std::string bytes(640 + 4 * bands * width, 0);
    const auto word = [&](size_t at, uint64_t value, size_t size) {mime_heightfield_fixture::word(bytes, at, value, size, little);};
    word(0, 27, 2); word(2, 8, 2); word(4, bytes.size(), 4);
    word(12, 26, 2); word(14, 6, 2); word(16, 320, 4);
    word(24, product, 2);
    word(38, 2022, 2); word(40, 1, 2); word(42, 1, 2);
    word(100, 1000, 4); word(104, 1000, 4); // Ten-meter Cartesian pixels.
    word(112, 2, 4); word(116, 2, 4); word(120, bands, 4);
    word(142, quantity, 2);
    word(158, 1, 1); // Mercator projection.
    word(440, 0x238e38e3, 4); word(444, 0x071c71c7, 4); // Binary angles, near 50 N, 10 E.
    word(452, 5080, 4); word(480, 1000, 4); // Nyquist velocity 127 m/s.
    const uint32_t real32[] = {0x3f800000, 0x40000000, 0x40400000, 0x40800000};
    for (size_t band = 0; band < bands; ++band)
	for (size_t i = 0; i < 4; ++i) {
	    const unsigned int value = static_cast<unsigned int>(band * 10 + (i < 2 ? i + 3 : i - 1));
	    const unsigned int encoded = quantity == 1 || quantity == 2 ? 64 + value * 2 :
		quantity == 3 ? 128 + value : quantity == 8 || quantity == 9 ? 32768 + value * 100 :
		quantity == 32 ? 1 + value * 10 : quantity == 33 ? 1 + value * 1000 :
		quantity == 31 ? real32[i < 2 ? i + 2 : i - 2] :
		quantity == 35 ? 128 + value * 5 : quantity == 37 ? value * 1000 : value;
	    word(640 + (band * 4 + i) * width, encoded, width);
	}
    return bytes;
}

inline std::string scalar(const std::string &name, const std::string &value)
{
    return "<s n=\"" + name + "\" v=\"" + value + "\"/>\n";
}

inline std::string photomod(bool dem = false, const std::string &channel = "U", const std::string &bytes = "1", const std::string &offset = "0")
{
    std::string text = "<phini>\n" + scalar("Width", "2") + scalar("Height", "2") + scalar("QChans", "1") +
	"<l n=\"ChannelsInfo\">" + scalar("type", channel) + scalar("bytes_ps", bytes) + "</l>\n";
    if (dem) text += scalar("XR_0", "10") + scalar("XR_1", "10.01") + scalar("YR_0", "49.99") + scalar("YR_1", "50") +
	"<l n=\"DemShift\">" + scalar("x", "0") + scalar("y", "0") + scalar("z", offset) + "</l>\n";
    else text += "<l n=\"GeoRef\">" + scalar("A_0", "10") + scalar("A_1", "0.01") + scalar("A_2", "0") +
	scalar("B_0", "50") + scalar("B_1", "0") + scalar("B_2", "-0.01") + "</l>\n";
    return text + "<l n=\"tile\">" + scalar("Width", "2") + scalar("Height", "2") + scalar("DispX", "0") +
	scalar("DispY", "0") + scalar("Scale", "0") + "</l>\n</phini>\n";
}
}
#endif

/*
 * Local Variables:
 * mode: C++
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8 cino=N-s
 */
