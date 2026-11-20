/* M I M E _ I R I S . H
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
#ifndef BU_MIME_IRIS_H
#define BU_MIME_IRIS_H
#include "common.h"
#include <cstdint>
#include <limits>
#include <string>
#include "bu/mime_runtime.h"
#include "mime_binary.h"

namespace mime_iris {
constexpr const char *type = "application/vnd.vaisala.iris.product";
constexpr const char *ppi_type = "application/x-ppi", *cappi_type = "application/x-cappi";
constexpr size_t header_size = 640, structure_size = 12, configuration_size = 320;
constexpr size_t product_offset = 24, columns_offset = 112, rows_offset = 116, bands_offset = 120, quantity_offset = 142;
/* Prefer a published product-specific view during automatic selection,
 * while retaining an independently usable family candidate. */
constexpr unsigned int product_view_priority = BU_MIME_DEFAULT_PRIORITY + 1;
constexpr unsigned int float32_quantity = 31;

struct quantity_info {unsigned int code; size_t bytes; const char *name; const char *encoding;};
inline const quantity_info *quantity(unsigned int code)
{
    static const quantity_info values[] = {
	{1, 1, "DBT", "biased-integer"}, {2, 1, "DBZ", "biased-integer"}, {3, 1, "VEL", "nyquist-scaled-integer"},
	{8, 2, "DBT2", "biased-integer"}, {9, 2, "DBZ2", "biased-integer"}, {float32_quantity, 4, "FLOAT32", "ieee754"},
	{32, 1, "HEIGHT", "scaled-integer"},
	{33, 2, "VIL2", "scaled-integer"}, {35, 1, "SHEAR", "biased-integer"}, {37, 2, "FLIQUID2", "packed-exponent"}
    };
    for (const auto &value : values) if (value.code == code) return &value;
    return NULL;
}

inline bool cartesian(unsigned int product)
{
    return product == 1 || product == 3 || product == 5 || product == 7 || product == 8 || product == 10 || product == 16;
}

struct profile {
    bool identified = false, incomplete = false, little = false, configuration_known = false, fields_known = false, plain = false;
    int32_t revision = 0, configuration_revision = 0;
    unsigned int product = 0, quantity_code = 0;
    uint32_t rows = 0, columns = 0, bands = 0;
    const char *product_view() const
    {
	return fields_known ? product == 1 ? ppi_type : product == 3 ? cappi_type : NULL : NULL;
    }
    bool append_traits(bu_mime_traits &traits) const
    {
	if (!identified || !bu_mime_traits_add(&traits, "encoding", "binary") ||
	    !bu_mime_traits_add(&traits, "byte-order", little ? "little-endian" : "big-endian") ||
	    !bu_mime_traits_add(&traits, "iris.product-header-version", std::to_string(revision).c_str())) return false;
	if (configuration_known && !bu_mime_traits_add(&traits, "iris.configuration-version", std::to_string(configuration_revision).c_str())) return false;
	if (!fields_known) return true;
	if (!bu_mime_traits_add(&traits, "iris.product-code", std::to_string(product).c_str()) ||
	    !bu_mime_traits_add(&traits, "iris.quantity-code", std::to_string(quantity_code).c_str()) ||
	    !bu_mime_traits_add(&traits, "iris.band-count", std::to_string(bands).c_str())) return false;
	const auto *value = quantity(quantity_code);
	if (value && (!bu_mime_traits_add(&traits, "iris.quantity", value->name) ||
	    !bu_mime_traits_add(&traits, "iris.sample-encoding", value->encoding) ||
	    !bu_mime_traits_add(&traits, "bit-depth", std::to_string(value->bytes * 8).c_str()) ||
	    !bu_mime_traits_add(&traits, "sample-format", value->code == float32_quantity ? "ieee754" : "unsigned-integer"))) return false;
	return !plain || (bu_mime_traits_add(&traits, "iris.payload-layout", "plain-cartesian") &&
	    bu_mime_traits_add(&traits, "compression", "none"));
    }
};

inline profile inspect(const bu_mime_probe_input &input)
{
    profile out;
    if (!input.data || input.size < 2 * structure_size) return out;
    if (mime_binary::little_u16(input.data) == 27) out.little = true;
    else if (mime_binary::big_u16(input.data) != 27) return out;
    const auto word = [&](size_t at) {return out.little ? mime_binary::little_u16(input.data + at) : mime_binary::big_u16(input.data + at);};
    const auto dword = [&](size_t at) {return out.little ? mime_binary::little_u32(input.data + at) : mime_binary::big_u32(input.data + at);};
    const auto version = [&](size_t at) {
	const uint16_t value = word(at);
	return value <= std::numeric_limits<int16_t>::max() ? int32_t(value) : int32_t(value) - (INT32_C(1) << 16);
    };
    const uint32_t declared_size = dword(4);
    if (declared_size < header_size || declared_size > std::numeric_limits<int32_t>::max() ||
	word(structure_size) != 26 || dword(structure_size + 4) < structure_size ||
	dword(structure_size + 4) > declared_size - structure_size) return out;
    out.identified = true;
    out.revision = version(2);
    /* The compound prefix corroborates the family identity. A future
     * revision does not establish the current configuration's fields. */
    if (out.revision != 8) return out;
    if (input.size < header_size) {out.incomplete = !input.complete; return out;}
    out.configuration_known = true;
    out.configuration_revision = version(structure_size + 2);
    if (out.configuration_revision != 6 || dword(structure_size + 4) != configuration_size) return out;
    out.fields_known = true;
    out.product = word(product_offset);
    out.quantity_code = word(quantity_offset);
    out.columns = dword(columns_offset);
    out.rows = dword(rows_offset);
    out.bands = dword(bands_offset);
    const auto *value = quantity(out.quantity_code);
    if (!value || !cartesian(out.product) || !out.rows || !out.columns || !out.bands ||
	out.rows > std::numeric_limits<int32_t>::max() || out.columns > std::numeric_limits<int32_t>::max() ||
	out.bands > std::numeric_limits<int32_t>::max()) return out;
    uint64_t expected = uint64_t(out.rows) * out.columns;
    if (expected > (std::numeric_limits<uint64_t>::max() - header_size) / out.bands / value->bytes) return out;
    expected = header_size + expected * out.bands * value->bytes;
    const bool extent_known = input.file_size_known || input.complete;
    const uint64_t actual = input.file_size_known ? input.file_size : input.size;
    /* Plain Cartesian data start immediately after the fixed header.
     * Optional setup/extension records require a different reader profile. */
    out.plain = extent_known && actual == expected && declared_size == expected;
    return out;
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
