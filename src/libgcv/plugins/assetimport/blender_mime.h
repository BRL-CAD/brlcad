/* B L E N D E R _ M I M E . H
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
#ifndef ASSETIMPORT_BLENDER_MIME_H
#define ASSETIMPORT_BLENDER_MIME_H
#include "common.h"
#include <algorithm>
#include <exception>
#include <fstream>
#include <sstream>
#include <assimp/BaseImporter.h>
#include <assimp/Importer.hpp>
#include <assimp/MemoryIOWrapper.h>
#include "mime_blender.h"
#include "../inflate_mime.h"

namespace assetimport_blender {
inline bool gzip_input()
{
    /* Compression can be disabled independently in the SDK build. Its
     * CanRead checks a gzip member containing BLENDER_v250 without
     * converting a scene. */
    static const bool available = []() {
	constexpr unsigned char header[] = {
	    0x1f, 0x8b, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0xff, 0x73, 0xf2, 0x71, 0xf5, 0x73, 0x71,
	    0x0d, 0x8a, 0x2f, 0x33, 0x32, 0x35, 0x00, 0x00, 0xd2, 0x2b, 0xae, 0xb8, 0x0c, 0x00, 0x00, 0x00
	};
	Assimp::Importer importer;
	auto *reader = importer.GetImporter("blend");
	Assimp::MemoryIOSystem source(header, sizeof(header), NULL);
	try {return reader && reader->CanRead(AI_MEMORYIO_MAGIC_FILENAME, &source, true);}
	catch (const std::exception &) {return false;}
    }();
    return available;
}

struct observation {
    mime_blender::profile header;
    bool compressed = false, error = false;
    bool append(bu_mime_traits &traits) const
    {
	return header.append_traits(traits, compressed ? "gzip" : "none");
    }
};

inline observation inspect(std::istream &source, bool complete)
{
    observation result;
    const auto prefix = gcv_compression::gzip_prefix(source, mime_blender::modern_header_size, complete);
    const bu_mime_probe_input bytes = {prefix.data.data(), prefix.data.size(), complete, 0, 0};
    result.header = mime_blender::inspect(bytes);
    result.compressed = prefix.compressed;
    result.error = prefix.error;
    /* A mismatching decoded prefix cannot become Blender with more input.
     * An exhausted gzip header may have produced no payload bytes yet. */
    if (prefix.data.empty() && prefix.incomplete) result.header.incomplete = true;
    return result;
}

inline int file_traits(const std::string &filename, bu_mime_traits &traits)
{
    std::ifstream file(filename, std::ios::binary);
    if (!file.is_open()) return 0;
    const auto result = inspect(file, true);
    return !file.bad() && !result.error && result.header.valid && result.append(traits);
}

inline observation prefix(const bu_mime_probe_input &input)
{
    const size_t size = std::min(input.size, size_t(BU_MIME_DEFAULT_SCAN_LIMIT));
    std::istringstream source(std::string(reinterpret_cast<const char *>(input.data), size));
    return inspect(source, input.complete && size == input.size);
}
}
#endif
