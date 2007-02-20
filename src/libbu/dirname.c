/*                      D I R N A M E . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2007 United States Government as represented by
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
/** @addtogroup filesystem */
/** @{ */
/** @file dirname.c
 *
 * @brief
 * Routines to process path names.
 *
 * @author      Christopher Sean Morrison
 */

#ifndef lint
static const char RCSmalloc[] = "@(#)$Header$ (ARL)";
#endif

#include "common.h"

#ifdef HAVE_STRING_H
#  include <string.h>
#else
#  include <strings.h>
#endif

#include "machine.h"
#include "bu.h"


/**
 *			B U _ D I R N A M E
 *
 *  Given a filesystem pathname, return a pointer to a dynamic string
 *  which is the parent directory of that file/directory.
 *
 *	/usr/dir/file	/usr/dir
 * @n	/usr/dir/	/usr
 * @n	/usr/file	/usr
 * @n	/usr/		/
 * @n	/usr		/
 * @n	/		/
 * @n	.		.
 * @n	..		.
 * @n	usr		.
 * @n	a/b		a
 * @n	a/		.
 * @n	../a/b		../a
 */
char *
bu_dirname(const char *cp)
{
	char	*ret;
	char	*slash;
	int	len;

	/* Special cases */
	if( cp == NULL )  return bu_strdup(".");
	if( strcmp( cp, "/" ) == 0 )
		return bu_strdup("/");
	if( strcmp( cp, "." ) == 0 ||
	    strcmp( cp, ".." ) == 0 ||
	    strrchr(cp, '/') == NULL )
		return bu_strdup(".");

	/* Make a duplicate copy of the string, and shorten it in place */
	ret = bu_strdup(cp);

	/* A trailing slash doesn't count */
	len = strlen(ret);
	if( ret[len-1] == '/' )  ret[len-1] = '\0';

	/* If no slashes remain, return "." */
	if( (slash = strrchr(ret, '/')) == NULL )  {
		bu_free( ret, "bu_dirname" );
		return bu_strdup(".");
	}

	/* Remove trailing slash, unless it's at front */
	if( slash == ret )
		ret[1] = '\0';		/* ret == "/" */
	else
		*slash = '\0';

	return ret;
}

/** @} */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
