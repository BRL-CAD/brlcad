/*                     O B J _ U T I L . C P P
 * BRL-CAD
 *
 * Copyright (c) 2010-2022 United States Government as represented by
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

#include "obj_util.h"

extern "C" char *wfobj_strdup(const char *cp) {
    char *base = NULL;
    if (cp) {
	size_t len = strlen(cp)+1;
	base = (char *)malloc(len);
	memcpy(base, cp, len);
    }
    return base;
}

extern "C" size_t wfobj_strlcpy(char *dst, const char *src, size_t size) {
    size_t srcsize;
    if (!dst || !src || size <= 0) {
	return 0;
    }
    srcsize = strlen(src);
    (void)strncpy(dst, src, size - 1);
    if (srcsize < size - 1) {
	dst[srcsize] = '\0';
    } else {
	dst[size-1] = '\0'; /* sanity */
    }

    return strlen(dst);
}

/*
 * Local Variables:
 * mode: C++
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
