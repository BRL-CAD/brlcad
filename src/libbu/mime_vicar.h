/* M I M E _ V I C A R . H
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
#ifndef BU_MIME_VICAR_H
#define BU_MIME_VICAR_H
#include "common.h"
#include <cstdint>
#include <cstring>
#include <istream>
#include <limits>
#include <sstream>
#include <string>
#include "bu/mime_runtime.h"

namespace mime_vicar {
constexpr const char *type = "application/vnd.nasa.vicar";
constexpr char label_keyword[] = "LBLSIZE";
struct header {
    bool identified = false;
    bool incomplete = false;
    bool append_traits(bu_mime_traits &traits) const
    {
	return identified && bu_mime_traits_add(&traits, "vicar.header-layout", "lblsize");
    }
};
inline bool space(int c)
{
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}
inline header inspect(std::istream &stream, bool whole)
{
    header out;
    uint64_t consumed = 0;
    const auto get = [&]() {const int c = stream.get(); if (c >= 0) ++consumed; return c;};
    const auto ended = [&]() {out.incomplete = !whole; return out;};
    for (size_t i = 0; i < sizeof(label_keyword) - 1; ++i) {
	const int c = get();
	if (c < 0) return ended();
	if (c != label_keyword[i]) return out;
    }
    while (space(stream.peek())) get();
    const int equals = get();
    if (equals < 0) return ended();
    if (equals != '=') return out;
    while (space(stream.peek())) get();
    if (stream.peek() == '+') get();
    uint64_t size = 0;
    bool digits = false;
    while (stream.peek() >= '0' && stream.peek() <= '9') {
	const unsigned digit = static_cast<unsigned>(get() - '0');
	if (size > (std::numeric_limits<uint64_t>::max() - digit) / 10) return out;
	size = size * 10 + digit;
	digits = true;
    }
    const int end = stream.peek();
    if (end < 0 && !whole) return ended();
    /* A prefix ending inside the integer cannot establish its value. The
     * label area must at least contain its own leading declaration. */
    out.identified = digits && size >= consumed && (end < 0 || !end || space(end));
    return out;
}
inline header inspect(const unsigned char *data, size_t size, bool whole)
{
    if (!size) return {false, !whole};
    const size_t prefix_size = size < sizeof(label_keyword) - 1 ? size : sizeof(label_keyword) - 1;
    if (prefix_size && (!data || memcmp(data, label_keyword, prefix_size))) return {};
    std::istringstream stream(std::string(reinterpret_cast<const char *>(data), size));
    return inspect(stream, whole);
}
} // namespace mime_vicar
#endif
