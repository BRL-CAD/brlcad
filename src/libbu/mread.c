/*                       P N G - I P U . C
 * BRL-CAD
 *
 * Copyright (c) 1992-2006 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this file; see the file named COPYING for more
 * information.
 *
 */
/** @file mread.c
 *
 * Provide a general means to a read some count of items from a file
 * descriptor reading multiple times until the quantity desired is
 * obtained.  This is useful for pipes.
 *
 * If a read error occurs, a negative value will be returns and errno
 * should be set (by read()).
 */

#include "common.h"

#ifdef HAVE_SYS_TYPES_H
#  include <sys/types.h>
#endif
#ifdef HAVE_UNISTD_H
#  include <unistd.h>
#endif


/** "Multiple try" read.  Read multiple times until quantity is
 *  obtained or an error occurs.  This is useful for pipes.
 */
int bu_mread(int fd, char *bufp, int n)
{
    register int count = 0;
    register int nread;

    while (count < n) {
	nread = read(fd, bufp, n-count);
	if (nread < 0)  {
	    return nread;
	}
	if (nread == 0) {
	    return count;
	}
	count += nread;
	bufp += nread;
    }
    return count;
}

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
