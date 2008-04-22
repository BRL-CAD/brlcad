/*                          M R E A D . C
 * BRL-CAD
 *
 * Copyright (c) 1992-2008 United States Government as represented by
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
/** @addtogroup bu_log */
/** @{ */
/** @file mread.c
 *
 * @brief multiple-read to fill a buffer
 *
 * Provide a general means to a read some count of items from a file
 * descriptor reading multiple times until the quantity desired is
 * obtained.  This is useful for pipes and network connections that
 * don't necessarily deliver data with the same grouping as it is
 * written with.
 *
 * If a read error occurs, a negative value will be returns and errno
 * should be set (by read()).
 *
 */

#include "common.h"

#ifdef HAVE_SYS_TYPES_H
#  include <sys/types.h>
#endif
#include "bio.h"

#include "bu.h"


/**
 * "Multiple try" read.  Read multiple times until quantity is
 *  obtained or an error occurs.  This is useful for pipes.
 */
long int
bu_mread(int fd, void *bufp, long int n)
{
    register long int count = 0;
    register long int nread;
    char *cbufp = (char *)bufp;

    while (count < n) {
	nread = read(fd, cbufp, (size_t)n-count);
	if (nread < 0)  {
	    return nread;
	}
	if (nread == 0) {
	    return count;
	}
	count += nread;
	cbufp += nread;
    }
    return count;
}
/** @} */

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
