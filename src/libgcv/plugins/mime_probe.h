/* M I M E _ P R O B E . H
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
#ifndef GCV_MIME_PROBE_H
#define GCV_MIME_PROBE_H

#include "common.h"
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "bu/malloc.h"
#include "gcv/api.h"
#include "../mime_validate.h"

static inline uint32_t
gcv_probe_u32be(const unsigned char *data)
{
    return ((uint32_t)data[0] << 24) | ((uint32_t)data[1] << 16) |
	((uint32_t)data[2] << 8) | (uint32_t)data[3];
}

static inline uint32_t
gcv_probe_u32le(const unsigned char *data)
{
    return (uint32_t)data[0] | ((uint32_t)data[1] << 8) |
	((uint32_t)data[2] << 16) | ((uint32_t)data[3] << 24);
}

/* Prefixes are not necessarily NUL terminated. The VLS string-copy
 * functions inspect their source with strlen even when given a limit. */
static inline void
gcv_probe_text(struct bu_vls *text, const unsigned char *data, size_t size)
{
    bu_vls_trunc(text, 0);
    bu_vls_extend(text, size + 1);
    if (size)
	memcpy(bu_vls_addr(text), data, size);
    bu_vls_addr(text)[size] = '\0';
    bu_vls_setlen(text, size);
}

static inline enum bu_mime_probe_status
gcv_probe_prefix(const struct bu_mime_probe_input *input, const char *signature)
{
    size_t length = strlen(signature);
    size_t available = input->size < length ? input->size : length;
    if (available && memcmp(input->data, signature, available))
	return BU_MIME_PROBE_NO_MATCH;
    if (input->size < length)
	return input->complete ? BU_MIME_PROBE_NO_MATCH : BU_MIME_PROBE_INCOMPLETE;
    return BU_MIME_PROBE_SIGNATURE;
}

/* Legacy callbacks share the bounded recognizer while their ABI remains. */
static inline int
gcv_probe_file(const char *filename,
    enum bu_mime_probe_status (*probe)(const struct bu_mime_probe_input *, const char *), const char *type)
{
    FILE *fp = filename ? fopen(filename, "rb") : NULL;
    if (!fp)
	return 0;
    unsigned char *prefix = (unsigned char *)bu_malloc(BU_MIME_DEFAULT_SCAN_LIMIT, "recognition prefix");
    size_t size = fread(prefix, 1, BU_MIME_DEFAULT_SCAN_LIMIT, fp);
    struct bu_mime_probe_input input = {prefix, size, feof(fp), 0, 0};
    int error = ferror(fp);
    if (fclose(fp))
	error = 1;
    enum bu_mime_probe_status status = error ? BU_MIME_PROBE_ERROR : probe(&input, type);
    bu_free(prefix, "recognition prefix");
    return status == BU_MIME_PROBE_HEURISTIC || status == BU_MIME_PROBE_SIGNATURE;
}

#endif
