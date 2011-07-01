/*                        M E M S E T . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2011 United States Government as represented by
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
/** @addtogroup libsysv */
/** @{ */
/** @file libsysv/memset.c
 *
 */
/** @} */

#include "common.h"

/* quell empty-compilation unit warnings */
static const int unused = 0;

/*
 * defined for the folks that don't seem to have a system memset()
 */
#ifndef HAVE_MEMSET
#include "sysv.h"

void *
memset(void *s, register int c, register size_t n)
{
    register unsigned char *p=(char *)s;

    if (p) {
	while (n-- > 0) {
	    *p++ = (unsigned char)c;
	}
    }

    return s;
}

#endif

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
