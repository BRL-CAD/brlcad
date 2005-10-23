/*                         D M D F B . H
 * BRL-CAD
 *
 * Copyright (C) 1986-2005 United States Government as represented by
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

/** \addtogroup libfb */
/*@{*/

/** @file dmdfb.h
 *
 *  Author -
 *	Gary S. Moss
 *
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 *
 *
 *  $Header$
 */
/*@}*/

#define BLACK		0
#define WHITE		1
#define CLR_BIT( word, bit )	word &= ~(bit)
#define SET_BIT( word, bit )	word |= (bit)
#define TST_BIT( word, bit )	((word)&(bit))

#define PT_ANIMATE	'a'
#define PT_CLEAR	'e'
#define PT_CURSOR	'c'
#define PT_EOL		'\r'
#define PT_QUIT		'q'
#define PT_READ		'r'
#define PT_SEEK		's'
#define PT_SETSIZE	'S'
#define PT_VIEWPORT	'v'
#define PT_WINDOW	'w'
#define PT_WRITE	'p'
#define PT_ZOOM		'z'

#define PR_NOT_PENDING	-1
#define PR_ERROR	0
#define PR_END_OF_RUN	1
#define PR_END_OF_LINE	2
#define PR_SUCCESS	3

#define LAYER_BORDER	4
#define SMALL_LAYER_SZ	51
#define MIN_LAYER_SZ	80

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
