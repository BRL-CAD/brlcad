/*          Z I P _ M I M E . H
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

#ifndef ASSETIMPORT_ZIP_MIME_H
#define ASSETIMPORT_ZIP_MIME_H
#include "common.h"
#include <cstring>
#include <string>
#include "../../../libbu/mime_zip.h"
#include "bu/str.h"

namespace assetimport_zip {
inline constexpr const char *zae_type = "model/vnd.collada+xml+zip";
inline constexpr const char *three_mf_type = "model/3mf";
inline bool is_zae(const char *type) {return type && BU_STR_EQUAL(type, zae_type);}
inline bool is_3mf(const char *type) {return type && BU_STR_EQUAL(type, three_mf_type);}
inline bool suffix(const std::string &filename, const char *ending)
{
    size_t size = strlen(ending);
    return filename.size() >= size && !bu_strcasecmp(filename.c_str() + filename.size() - size, ending);
}

using mime_zip::end_record;
}
#endif
