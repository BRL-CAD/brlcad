/*                         M I M E _ L E G A C Y . C P P
 * BRL-CAD
 *
 * Copyright (c) 2004-2026 United States Government as represented by
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

#include "common.h"
#include "bu/mime.h"
#include "bu/str.h"

struct mime_legacy_mapping {
    const char *name;
    int legacy_type;
    bu_mime_context_t context;
};

static const mime_legacy_mapping mime_legacy_mappings[] = {
#include "mime_legacy.inc"
};

extern "C" const char *
bu_mime_legacy_name(int type, bu_mime_context_t context)
{
    for (const auto &builtin : mime_legacy_mappings)
	if (builtin.context == context && builtin.legacy_type == type)
	    return builtin.name;
    return NULL;
}

extern "C" int
bu_mime_legacy_type(const char *name, bu_mime_context_t context)
{
    if (!name)
	return -1;
    for (const auto &builtin : mime_legacy_mappings)
	if (builtin.name && builtin.context == context && BU_STR_EQUIV(name, builtin.name))
	    return builtin.legacy_type;
    return -1;
}
