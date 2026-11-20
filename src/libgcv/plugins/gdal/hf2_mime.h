/* H F 2 _ M I M E . H
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
#ifndef GCV_GDAL_HF2_MIME_H
#define GCV_GDAL_HF2_MIME_H
#include "common.h"
#include <algorithm>
#include <istream>
#include <sstream>
#include <string>
#include "../../../libbu/mime_hf2.h"
#include "../inflate_mime.h"

namespace gcv_hf2 {
struct observation {
    mime_hf2::profile header;
    bool compressed = false, error = false;
    bool append_traits(bu_mime_traits &traits) const
    {
	return header.append_traits(traits, compressed ? "gzip" : "none");
    }
};
inline observation inspect(std::istream &source, bool complete)
{
    observation result;
    const auto prefix = gcv_compression::gzip_prefix(source, mime_hf2::header_size, complete);
    result.header = mime_hf2::inspect(prefix.data.data(), prefix.data.size(), complete);
    result.compressed = prefix.compressed;
    result.error = prefix.error;
    if (prefix.data.empty() && prefix.incomplete) result.header.incomplete = true;
    return result;
}
inline observation inspect(const bu_mime_probe_input &input)
{
    const size_t size = std::min(input.size, size_t(BU_MIME_DEFAULT_SCAN_LIMIT));
    if (!size || input.data[0] != 0x1f) {
	observation result;
	result.header = mime_hf2::inspect(input.data, size, input.complete && size == input.size);
	return result;
    }
    std::istringstream source(std::string(reinterpret_cast<const char *>(input.data), size));
    return inspect(source, input.complete && size == input.size);
}
}
#endif
