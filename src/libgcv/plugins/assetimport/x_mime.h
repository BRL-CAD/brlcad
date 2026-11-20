/* X _ M I M E . H
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
#ifndef ASSETIMPORT_X_MIME_H
#define ASSETIMPORT_X_MIME_H
#include "common.h"
#include <fstream>
#include <limits>
#include <string>
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <assimp/version.h>
#include "bu/endian.h"
#include "bu/log.h"
#include "bu/str.h"
#include "mime_x.h"
#include "zip_mime.h"

namespace assetimport_x {
inline bool binary_width(const char *width)
{
    if (bu_byteorder() != BU_LITTLE_ENDIAN) return false;
    /* Binary32 copies four bytes directly into ai_real in this SDK. */
    if (BU_STR_EQUAL(width, "32")) return sizeof(float) == sizeof(uint32_t) &&
	std::numeric_limits<float>::is_iec559 && !(aiGetCompileFlags() & ASSIMP_CFLAGS_DOUBLE_SUPPORT);
    return BU_STR_EQUAL(width, "64") && sizeof(double) == sizeof(uint64_t) && std::numeric_limits<double>::is_iec559;
}

inline bool compressed_input()
{
    /* The descriptor still lists compressed input when that feature was
     * disabled in the SDK build. Exercise a small MSZIP text triangle. */
    static const bool available = []() {
	constexpr unsigned char triangle[] = {
	    0x78,0x6f,0x66,0x20,0x30,0x33,0x30,0x33,0x74,0x7a,0x69,0x70,0x30,0x30,0x33,0x32,
	    0x39,0x00,0x00,0x00,0x00,0x00,0x2f,0x00,0x43,0x4b,0xe3,0xf2,0x4d,0x2d,0xce,0x50,
	    0x28,0x29,0xca,0x4c,0xcc,0x4b,0xcf,0x49,0x55,0xa8,0xe6,0x32,0xb6,0xe6,0x32,0xb0,
	    0x06,0x41,0x1d,0x43,0x08,0x65,0x60,0x0d,0x62,0x58,0x73,0x19,0x5a,0x03,0xe5,0x0c,
	    0x74,0x0c,0x75,0x8c,0x80,0x9c,0x5a,0x2e,0x00
	};
	Assimp::Importer importer;
	const auto *scene = importer.ReadFileFromMemory(triangle, sizeof(triangle), aiProcess_ValidateDataStructure, "x");
	return scene && scene->mNumMeshes == 1 && scene->mMeshes[0]->mNumVertices == 3 && scene->mMeshes[0]->mNumFaces == 1;
    }();
    return available;
}

inline int file_traits(const std::string &filename, const bu_mime_traits *requested, bu_mime_traits &traits)
{
    std::ifstream file(filename, std::ios::binary);
    if (!file.is_open()) return 0;
    const bool inspect_encoding = requested && bu_mime_traits_get(requested, "encoding");
    const auto profile = mime_x::inspect(file, true, inspect_encoding);
    if (file.bad() || !profile.complete) return 0;
    if (profile.bom && profile.width == "8" && !assetimport_zip::suffix(filename, ".x")) {
	bu_log("SDK X input with a UTF-8 BOM requires a .x filename suffix\n");
	return 0;
    }
    return profile.append_traits(traits);
}
}
#endif
