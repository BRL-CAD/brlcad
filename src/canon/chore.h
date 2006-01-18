/*                         C H O R E . H
 * BRL-CAD
 *
 * Copyright (c) 2004-2006 United States Government as represented by
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
/** @file chore.h
 *
 *  Header file for multi-threaded scanner support.
 *
 *  Author -
 *	Michael John Muuss
 *
 *  Source -
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5068  USA
 *
 *
 *  $Header$
 */

struct chore {
    int		todo;
    int		buflen;
    int		pix_y;
    int		canon_y;
    unsigned char	*cbuf;			/* ptr to canon buffer */
    unsigned char	obuf[255*1024];
};


#define GET(item, queue)	{\
	while( queue == NULL )  sginap(1); \
	item = queue; \
	queue = NULL; }

#define PUT(queue, item)	{ \
	while( queue != NULL )  sginap(1); \
	queue = item; \
	item = NULL;  }

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
