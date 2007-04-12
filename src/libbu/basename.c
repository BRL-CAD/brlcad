/*                     B A S E N A M E . C
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
/** @file basename.c
 *
 * @brief
 * Routines to process file names.
 *
 */

#include "common.h"
#include "bu.h"


/**
 *  B U _ B A S E N A M E
 *
 *  Given a string containing slashes such as a pathname, return a
 *  pointer to the first character after the last slash.
 *
 *	/usr/dir/file	file
 * @n	/usr/dir/	dir
 * @n	/usr/		usr
 * @n	/usr		usr
 * @n	/		/
 * @n	.		.
 * @n	..		..
 * @n	usr		usr
 * @n	a/b		b
 * @n	a/		a
 */
const char *
bu_basename(const char *str)
{
    register const char	*p = str;
    while( *p != '\0' )
	if( *p++ == '/' )
	    str = p;
    return str;
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
