/*                          M R E A D . C
 * BRL-CAD
 *
 * Copyright (c) 1992-2011 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */

#include "common.h"

#ifdef HAVE_SYS_TYPES_H
#  include <sys/types.h>
#endif
#include "bio.h"

#include "bu.h"


long int
bu_mread(int fd, void *bufp, long int n)
{
    register long int count = 0;
    register long int nread;
    char *cbufp = (char *)bufp;

    while (count < n) {
	nread = read(fd, cbufp, (size_t)n-count);
	if (UNLIKELY(nread < 0)) {
	    return nread;
	}
	if (UNLIKELY(nread == 0)) {
	    return count;
	}
	count += nread;
	cbufp += nread;
    }
    return count;
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
