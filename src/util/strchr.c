/*                        S T R C H R . C
 * BRL-CAD
 *
 * Copyright (C) 2004-2005 United States Government as represented by
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
 */
/** @file strchr.c
 *
 */

/*	@(#)strchr.c	1.2	*/
/*	3.0 SID #	1.2	*/
/*LINTLIBRARY*/
#define NULL	(char *)0
/*
 * Return the ptr in sp at which the character c appears;
 * NULL if not found
 */
char *
strchr(register char *sp, register char c)
{
	do {
		if(*sp == c)
			return(sp);
	} while(*sp++);
	return(NULL);
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
