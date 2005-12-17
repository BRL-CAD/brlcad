/*                         S T A T . C
 * BRL-CAD
 *
 * Copyright (C) 2004-2005 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file stat.c
 *
 *  Support routine for identifying whether files and directories
 *  exist or not.
 *
 *  Author -
 *	Christopher Sean Morrison
 *
 *  Source -
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5068  USA
 *
 */
static const char RCS_stat[] = "@(#)$Header$";

#include "common.h"

#include <stdio.h>

#ifdef HAVE_UNISTD_H
#  include <unistd.h>
#endif
#ifdef HAVE_SYS_STAT_H
#  include <sys/stat.h>
#endif

#include "machine.h"
#include "bu.h"


/*
 *			B U _ F I L E _ E X I S T S
 *
 *  Returns boolean -
 *	1	The given filename exists.
 *	0	The given filename does not exist.
 */
int
bu_file_exists(const char *path)
{
    struct stat sbuf;

    if (bu_debug) {
	printf("Does %s exist? ", path);
    }

    if( path == NULL )  return 0;			/* FAIL */

    /* defined in unistd.h */
#if defined(HAVE_ACCESS) && defined(F_OK)
#  define bu_file_exists_method 1
    if( access( path, F_OK )  == 0 ) {
	if (bu_debug) {
	    printf("YES\n");
	}
	return 1;	/* OK */
    }
#endif

    /* does it exist as a filesystem entity? */
#if defined(HAVE_STAT)
#  define bu_file_exists_method 1
    if( stat( path, &sbuf ) == 0 ) {
	if (bu_debug) {
	    printf("YES\n");
	}
	return 1;	/* OK */
    }
#endif

#ifndef bu_file_exists_method
#  error "Do not know how to check whether a file exists on this system"
#endif

    if (bu_debug) {
	printf("NO\n");
    }
    return 0;					/* FAIL */
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
