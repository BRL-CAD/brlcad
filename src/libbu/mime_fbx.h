/* M I M E _ F B X . H
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
#ifndef BU_MIME_FBX_H
#define BU_MIME_FBX_H
#include "common.h"
#include <algorithm>
#include <cstring>
#include <istream>
#include <string>
#include "bu/mime.h"
#include "mime_binary.h"
#include "mime_unicode.h"

namespace mime_fbx {
inline constexpr const char *type = "application/vnd.autodesk.fbx";
inline constexpr char binary_magic[] = "Kaydara FBX Binary  \0\x1a";
inline constexpr char binary_discriminator[] = "Kaydara FBX Binary";
constexpr size_t binary_version_at = sizeof(binary_magic);
constexpr size_t binary_header_size = binary_version_at + sizeof(uint32_t);

inline bool sdk_binary(const char *data, size_t size)
{
    return size >= sizeof(binary_discriminator) - 1 &&
	!memcmp(data, binary_discriminator, sizeof(binary_discriminator) - 1);
}

/* Preserve unknown codes. Autodesk's special historical codes do not
 * all follow simple digit arithmetic, so normalize known revisions only. */
inline const char *version(const std::string &code)
{
    const struct {const char *code; const char *version;} known[] = {
	{"7100", "7.1.0"}, {"7200", "7.2.0"}, {"7300", "7.3.0"},
	{"7400", "7.4.0"}, {"7500", "7.5.0"}, {"7600", "7.6.0"}, {"7700", "7.7.0"}
    };
    for (const auto &entry : known) if (code == entry.code) return entry.version;
    return NULL;
}

struct profile {
    bool identified = false, incomplete = false, bom = false;
    std::string serialization, encoding, width, binary_version;
    void observe_text(mime_unicode::text_source &source, bool whole, bool inspect_encoding)
    {
	serialization = "text";
	width = source.width();
	bom = source.marked();
	if (source.marked()) encoding = source.encoding();
	if (whole && inspect_encoding) encoding = source.finish_encoding();
    }
    bool append_traits(bu_mime_traits &traits) const
    {
	if (!identified || !bu_mime_traits_add(&traits, "fbx.serialization", serialization.c_str())) return false;
	if (serialization == "binary" && !bu_mime_traits_add(&traits, "byte-order", "little-endian")) return false;
	if (serialization == "text" && !bu_mime_traits_add(&traits, "fbx.text-bom", bom ? "true" : "false")) return false;
	return (encoding.empty() || bu_mime_traits_add(&traits, "encoding", encoding.c_str())) &&
	    (width.empty() || bu_mime_traits_add(&traits, "text.code-unit-width", width.c_str())) &&
	    (binary_version.empty() || bu_mime_traits_add(&traits, "fbx.binary-header-version-code", binary_version.c_str()));
    }
};

inline profile inspect(std::istream &stream, bool whole, bool inspect_encoding = true)
{
    profile result;
    if (stream.peek() == binary_magic[0]) {
	char header[binary_header_size];
	stream.read(header, sizeof(header));
	const size_t size = static_cast<size_t>(stream.gcount());
	const size_t available = std::min(size, binary_version_at);
	if (memcmp(header, binary_magic, available)) return result;
	result.incomplete = !whole && size < binary_header_size;
	if (size < binary_version_at) return result;
	result.identified = true;
	result.serialization = "binary";
	result.encoding = "binary";
	if (size == binary_header_size)
	    result.binary_version = std::to_string(mime_binary::little_u32(reinterpret_cast<const unsigned char *>(header) + binary_version_at));
	return result;
    }
    mime_unicode::text_source source(stream);
    const auto spaces = [&]() {
	for (;;) {
	    source.spaces();
	    if (source.peek() != ';') break;
	    while (source.peek() >= 0 && source.peek() != '\n' && source.peek() != '\r') source.get();
	}
    };
    spaces();
    /* Identify a leading dictionary, not a version in a producer comment
     * or a quoted value. The SDK supplies the parsed document version. */
    constexpr char dictionary[] = "FBXHeaderExtension";
    for (const char *c = dictionary; *c; ++c) {
	const int actual = source.get();
	if (actual < 0) {result.incomplete = !whole; return result;}
	if (actual != *c) return result;
    }
    spaces();
    if (source.get() != ':') {result.incomplete = !whole && stream.eof(); return result;}
    spaces();
    if (source.get() != '{') {result.incomplete = !whole && stream.eof(); return result;}
    result.identified = true;
    result.observe_text(source, whole, inspect_encoding);
    return result;
}
}
#endif
