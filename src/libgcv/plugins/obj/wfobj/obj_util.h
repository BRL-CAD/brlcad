/*                       O B J _ U T I L . H
 * BRL-CAD
 *
 * Copyright (c) 2010-2016 United States Government as represented by
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

#ifndef WFOBJ_OBJ_UTIL_H
#define WFOBJ_OBJ_UTIL_H

#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>

#define WFOBJ_GET(_ptr, _type) _ptr = (_type *)calloc(1, sizeof(_type))
#define WFOBJ_PUT(_ptr, _type) do { *(uint8_t *)(_type *)(_ptr) = /*zap*/ 0; *((uint32_t *)_ptr) = 0xFFFFFFFF;  free(_ptr); _ptr = NULL; } while (0)

extern "C" {
    char *wfobj_strdup(const char *cp);
    size_t wfobj_strlcpy(char *dst, const char *src, size_t size);
}

#endif

/*
 * Local Variables:
 * mode: C++
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
