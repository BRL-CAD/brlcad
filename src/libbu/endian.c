/*                        E N D I A N . C
 * BRL-CAD
 *
 * Copyright (c) 2008-2011 United States Government as represented by
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

#include "bu.h"


inline bu_endian_t
bu_byteorder()
{
    const union bob {
	unsigned long i;
	unsigned char c[sizeof(unsigned long)];
    } b = {1};

/* give run-time test preference to compile-time endian, tested much
 * faster than stashing in a static.
 */
#ifdef WORDS_BIGENDIAN
    if (LIKELY(b.c[sizeof(unsigned long)-1]))
	return BU_BIG_ENDIAN;
    if (UNLIKELY(b.c[0]))
	return BU_LITTLE_ENDIAN;
#else
    if (LIKELY(b.c[0]))
	return BU_LITTLE_ENDIAN;
    if (UNLIKELY(b.c[sizeof(unsigned long)-1]))
	return BU_BIG_ENDIAN;
#endif
    if (UNLIKELY(b.c[1]))
	return BU_PDP_ENDIAN;

    return (bu_endian_t)0;
}


/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
