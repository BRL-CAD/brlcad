/*                      V E C T F O N T . H
 * BRL-CAD
 *
 * Copyright (C) 2004-2005 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2.1 of
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
/** @file vectfont.h
 *
 *  Vector font definitions, for TIG-PACK fonts.
 *  Used by LIBPLOT3 and LIBRT for simple vector fonts.
 *
 *  Author -
 *	Michael John Muuss
 *	Douglas A. Gwyn
 *
 *  Source -
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5068  USA
 *
 *  $Header$
 */

/*
 *	Motion encoding macros
 *
 * All characters reference absolute points within a 10 x 10 square
 */
#define	brt(x,y)	(11*x+y)
#define drk(x,y)	-(11*x+y)
#define	LAST		-128		/* 0200 Marks end of stroke list */
#define	NEGY		-127		/* 0201 Denotes negative y stroke */
#define bneg(x,y)	NEGY, brt(x,y)
#define dneg(x,y)	NEGY, drk(x,y)

#if defined(CRAY1) || defined(CRAY2) || defined(mips)
#define TINY	int
#else
#define TINY	char		/* must be signed */
#endif

extern TINY	*tp_cindex[256];	/* index to stroke tokens */
extern TINY	tp_ctable[];		/* table of strokes */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
