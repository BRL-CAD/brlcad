/*                      B A D M A G I C . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2020 United States Government as represented by
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

#include "bio.h"

#include "bu/magic.h"
#include "bu/exit.h"


void
bu_badmagic(const uint32_t *ptr, uint32_t magic, const char *str, const char *file, int line)
{
#define MAGICBUFSIZ 512
    char buf[MAGICBUFSIZ] = {'\0'};

    if (UNLIKELY(!(ptr))) {
	snprintf(buf, MAGICBUFSIZ, "ERROR: NULL %s pointer, file %s, line %d\n",
		 str, file, line);
    } else if (UNLIKELY((uintptr_t)(ptr) & (sizeof(uintptr_t)-1))) {
	snprintf(buf, MAGICBUFSIZ, "ERROR: %p mis-aligned %s pointer, file %s, line %d\n",
		 (void *)ptr, str, file, line);
    } else if (UNLIKELY(*(ptr) != (uint32_t)(magic))) {
	snprintf(buf, MAGICBUFSIZ, "ERROR: bad pointer %p: s/b %s(x%lx), was %s(x%lx), file %s, line %d\n",
		 (void *)ptr,
		 str, (unsigned long)magic,
		 bu_identify_magic(*(ptr)), (unsigned long)*(ptr),
		 file, line);
    } else {
	snprintf(buf, MAGICBUFSIZ, "ERROR: bad %s pointer %p @ file %s:%d\n", str, (void *)ptr, file, line);
    }

    bu_bomb(buf);
}

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
