/*                  S T E P _ M E T A D A T A . C P P
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 */

#include "STEPMetadata.h"

#include <cstdlib>
#include <iostream>
#include <map>
#include <string>
#include <vector>

#include "bu/app.h"

namespace {

void
require(bool condition, const char *message)
{
    if (condition) return;
    std::cerr << "step_metadata_test: " << message << std::endl;
    std::exit(EXIT_FAILURE);
}

} // namespace

int
main(int, char **argv)
{
    bu_setprogname(argv[0]);
    std::map<std::string, std::string> source;
    source["STEP::AP203::FILE_SCHEMA"] = "CONFIG_CONTROL_DESIGN";
    source["STEP::AP203::CONFIGURATION::#7::VALUE"] =
	"PERSON('P-1','O''Brien','Anne',$,$,$)";
    source[std::string("binary\0key", 10)] = std::string("value\0tail", 10);

    std::vector<unsigned char> encoded;
    std::string error;
    require(brlcad::step::EncodeSTEPMetadata(source, encoded, error),
	"encoding failed");
    require(!encoded.empty() && error.empty(),
	"successful encoding returned no data or an error");

    std::map<std::string, std::string> decoded;
    require(brlcad::step::DecodeSTEPMetadata(encoded.data(), encoded.size(),
	decoded, error), "decoding failed");
    require(decoded == source, "round trip changed retained metadata");

    std::map<std::string, std::string> unchanged;
    unchanged["sentinel"] = "preserved";
    const std::map<std::string, std::string> expected = unchanged;
    std::vector<unsigned char> malformed = encoded;
    malformed.push_back(0);
    require(!brlcad::step::DecodeSTEPMetadata(malformed.data(),
	malformed.size(), unchanged, error), "trailing data was accepted");
    require(unchanged == expected, "failed decode changed its destination");

    malformed = encoded;
    malformed[0] ^= 0xff;
    require(!brlcad::step::DecodeSTEPMetadata(malformed.data(),
	malformed.size(), unchanged, error), "incorrect magic was accepted");

    malformed = encoded;
    malformed[19] = 2;
    require(!brlcad::step::DecodeSTEPMetadata(malformed.data(),
	malformed.size(), unchanged, error), "unsupported version was accepted");

    require(!brlcad::step::DecodeSTEPMetadata(encoded.data(),
	encoded.size() - 1, unchanged, error), "truncated record was accepted");
    return EXIT_SUCCESS;
}
