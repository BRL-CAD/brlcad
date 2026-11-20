/* M I M E _ N S I D C . H
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
#ifndef BU_MIME_NSIDC_H
#define BU_MIME_NSIDC_H
#include "common.h"
#include <algorithm>
#include <cstring>
#include <string>
#include "bu/mime_runtime.h"
#include "mime_text.h"

namespace mime_nsidc {
constexpr const char *type = "image/vnd.nsidc.sea-ice-concentration";
constexpr size_t header_size = 300, field_size = 6, information_offset = 230;

struct profile {
    bool identified = false;
    std::string rows, columns, scale, missing, channel, descriptors, instrument, hemisphere;
    bool append_traits(bu_mime_traits &traits) const
    {
	return identified && bu_mime_traits_add(&traits, "encoding", "binary") &&
	    bu_mime_traits_add(&traits, "bit-depth", "8") && bu_mime_traits_add(&traits, "sample-format", "unsigned-integer") &&
	    bu_mime_traits_add(&traits, "nsidc.layout", "header300") &&
	    bu_mime_traits_add(&traits, "nsidc.rows", rows.c_str()) && bu_mime_traits_add(&traits, "nsidc.columns", columns.c_str()) &&
	    bu_mime_traits_add(&traits, "nsidc.scaling-factor", scale.c_str()) && bu_mime_traits_add(&traits, "nsidc.missing-value", missing.c_str()) &&
	    bu_mime_traits_add(&traits, "nsidc.channel", channel.c_str()) && bu_mime_traits_add(&traits, "nsidc.instrument", instrument.c_str()) &&
	    bu_mime_traits_add(&traits, "nsidc.data-descriptors", descriptors.c_str()) &&
	    bu_mime_traits_add(&traits, "nsidc.hemisphere", hemisphere.c_str());
    }
};

inline profile inspect(const unsigned char *data, size_t size)
{
    profile out;
    if (!data || size < header_size) return out;
    const auto field = [&](size_t index) {
	const char *start = reinterpret_cast<const char *>(data + index * field_size);
	const char *end = static_cast<const char *>(memchr(start, 0, field_size));
	return mime_detail::trim(std::string(start, end ? static_cast<size_t>(end - start) : field_size));
    };
    const auto number = [&](size_t index) {return mime_detail::decimal_field(field(index));};
    out.missing = number(0);
    out.columns = number(1);
    out.rows = number(2);
    out.instrument = field(9);
    out.descriptors = field(10);
    out.channel = number(19);
    out.scale = number(20);
    const std::string year = number(17);
    const std::string information(reinterpret_cast<const char *>(data + information_offset), header_size - information_offset);
    out.hemisphere = information.compare(0, sizeof("ANTARCTIC") - 1, "ANTARCTIC") == 0 ? "south" :
	information.compare(0, sizeof("ARCTIC") - 1, "ARCTIC") == 0 ? "north" : "";
    const std::string descriptors = mime_detail::upper(out.descriptors);
    const bool concentration = out.channel == "0" && descriptors.size() >= 2 &&
	descriptors.compare(descriptors.size() - 2, 2, "CN") == 0 &&
	std::all_of(descriptors.begin(), descriptors.end(), [](unsigned char c) {return c >= ' ' && c <= '~';});
    out.identified = !out.rows.empty() && out.rows != "0" && !out.columns.empty() && out.columns != "0" &&
	!out.missing.empty() && !out.scale.empty() && out.scale != "0" && concentration && year.size() == 4 &&
	(year.compare(0, 2, "19") == 0 || year.compare(0, 2, "20") == 0) && !out.hemisphere.empty() &&
	(out.instrument == "SMMR" || out.instrument == "SSM/I" || out.instrument == "SSMIS");
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
