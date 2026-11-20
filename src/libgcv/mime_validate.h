/* M I M E _ V A L I D A T E . H
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
#ifndef GCV_MIME_VALIDATE_H
#define GCV_MIME_VALIDATE_H

#include "common.h"
#include "bu/str.h"
#include "gcv/runtime.h"

/* Full-reader assertions must be established by actual data, including keys
 * that were unavailable during bounded selection. */
static inline int
gcv_probe_traits_match(const char *type, const struct bu_mime_traits *observed, const struct bu_mime_traits *requested)
{
    return type && type[0] && bu_mime_traits_match(observed, NULL, requested);
}

/* A reader's complete source facts remain authoritative. Runtime inspectors
 * can establish additional requested keys, independently of selection defaults
 * and the caller's assertions. Readers reject failed full-source validation
 * before supplementing unestablished keys. Preserve conflicting observations. */
static inline int
gcv_probe_file_traits_match(struct gcv_context *context, const char *filename, const char *type,
    const struct bu_mime_traits *observed, const struct bu_mime_traits *requested)
{
    struct bu_mime_result *result = NULL;
    const struct bu_mime_candidate *candidate = NULL;
    int valid;

    if (gcv_probe_traits_match(type, observed, requested))
	return 1;
    if (!type || !type[0] || !requested || !requested->count || !bu_mime_traits_valid(requested, 1))
	return 0;
    valid = gcv_detect_file(context, filename, &result, NULL) != BU_MIME_ERROR;
    for (size_t i = 0; valid && result && i < result->count; ++i)
	if (BU_STR_EQUAL(result->candidates[i].name, type))
	    candidate = &result->candidates[i];
    valid = valid && bu_mime_traits_match(observed, candidate ? &candidate->traits : NULL, requested);
    bu_mime_result_free(result);
    return valid;
}

#endif
