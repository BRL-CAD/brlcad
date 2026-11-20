/* M I M E _ R A S T E R _ L A B E L _ F I X T U R E . H
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
#ifndef BU_MIME_RASTER_LABEL_FIXTURE_H
#define BU_MIME_RASTER_LABEL_FIXTURE_H
#include "common.h"
#include <string>
#include "mime_heightfield_fixture.h"

namespace mime_raster_label_fixture {
constexpr size_t side = 2;
constexpr size_t cells = side * side;

/* These wire patterns are independent of the host's floating representation. */
inline std::string pixels(const std::string &layout, size_t size, bool floating, bool little, size_t bands = 2, bool logical = false)
{
    const uint64_t real32[] = {0x3f800000, 0x40000000, 0x40400000, 0x40800000,
	0x41300000, 0x41400000, 0x41500000, 0x41600000};
    const uint64_t real64[] = {UINT64_C(0x3ff0000000000000), UINT64_C(0x4000000000000000),
	UINT64_C(0x4008000000000000), UINT64_C(0x4010000000000000), UINT64_C(0x4026000000000000),
	UINT64_C(0x4028000000000000), UINT64_C(0x402a000000000000), UINT64_C(0x402c000000000000)};
    std::string bytes(cells * bands * size, 0);
    for (size_t band = 0; band < bands; ++band)
	for (size_t row = 0; row < side; ++row)
	    for (size_t col = 0; col < side; ++col) {
		const size_t cell = row * side + col;
		const size_t at = layout == "BIP" ? cell * bands + band :
		    layout == "BIL" ? (row * bands + band) * side + col : band * cells + cell;
		const uint64_t value = logical ? ((cell == 1 || cell == 2) != (band != 0)) :
		    floating ? (size == sizeof(uint32_t) ? real32 : real64)[band * cells + cell] : band * 10 + cell + 1;
		mime_heightfield_fixture::word(bytes, at * size, value, size, little);
	    }
    return bytes;
}

inline std::string rraster(const std::string &datatype = "FLT4S", const std::string &layout = "BIL", bool little = true)
{
    return "[general]\ncreator=Independent raster fixture\n[data]\ndatatype=" + datatype + "\nbyteorder=" +
	(little ? "little" : "big") + "\nnbands=2\nbandorder=" + layout +
	"\n[georeference]\nnrows=2\nncols=2\nxmin=10\nxmax=10.02\nymin=49.98\nymax=50\n"
	"projection=+proj=longlat +datum=WGS84 +no_defs\n";
}

inline std::string ehdr(const std::string &layout = "BIL", bool little = true)
{
    return std::string("BYTEORDER ") + (little ? "I" : "M") + "\nLAYOUT " + layout +
	"\nNROWS 2\nNCOLS 2\nNBANDS 2\nNBITS 32\nPIXELTYPE FLOAT\n"
	"ULXMAP 10.005\nULYMAP 49.995\nXDIM 0.01\nYDIM 0.01\n";
}

inline std::string roipac(const std::string &scale = "1", const std::string &offset = "0")
{
    return "WIDTH 2\nFILE_LENGTH 2\nPROJECTION LL\nDATUM WGS84\nX_FIRST 10\nX_STEP 0.01\nY_FIRST 50\nY_STEP -0.01\nZ_OFFSET " +
	offset + "\nZ_SCALE " + scale + "\n";
}

inline std::string property(const std::string &name, const std::string &value)
{
    return "<property name=\"" + name + "\"><value>" + value + "</value></property>\n";
}

inline std::string isce(const std::string &datatype = "FLOAT", const std::string &layout = "BIL", bool little = true)
{
    return "<imageFile>\n" + property("WIDTH", "2") + property("LENGTH", "2") + property("NUMBER_BANDS", "2") +
	property("DATA_TYPE", datatype) + property("SCHEME", layout) + property("BYTE_ORDER", little ? "l" : "b") +
	"<component name=\"Coordinate1\">" + property("startingValue", "10") + property("delta", "0.01") + "</component>\n"
	"<component name=\"Coordinate2\">" + property("startingValue", "50") + property("delta", "-0.01") + "</component>\n</imageFile>\n";
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
