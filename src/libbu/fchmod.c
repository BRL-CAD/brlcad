/*                        F C H M O D . C
 * BRL-CAD
 *
 * Copyright (c) 2007-2011 United States Government as represented by
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

#ifdef HAVE_SYS_TYPES_H
#  include <sys/types.h>
#endif
#ifdef HAVE_SYS_STAT_H
#  include <sys/stat.h>
#endif

#include "bu.h"


/* c99 doesn't declare these */
#ifndef  fileno
extern int fileno(FILE*);
#endif

#ifdef HAVE_FCHMOD
extern int fchmod(int, mode_t);
#endif


int
bu_fchmod(FILE *fp,
	  unsigned long pmode)
{
    if (UNLIKELY(!fp)) {
	return 0;
    }

#ifdef HAVE_FCHMOD
    return fchmod(fileno(fp), (mode_t)pmode);
#else
    pmode = pmode; /* quellage */
    return -1; /* probably Windows, fchmod unavailable and chmod insecure */
#endif
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
