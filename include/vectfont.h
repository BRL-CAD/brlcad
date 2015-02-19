/*                      V E C T F O N T . H
 * BRL-CAD
 *
 * Copyright (c) 2004-2014 United States Government as represented by
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
/** @addtogroup plot */
/** @{ */
/** @file vectfont.h
 *
 *  Vector font definitions, for TIG-PACK fonts.
 *  Used by LIBPLOT3 and LIBRT for simple vector fonts.
 *
 */

/*
 *	Motion encoding macros
 *
 * All characters reference absolute points within a 10 x 10 square
 */
#ifndef VECTFONT_H
#define VECTFONT_H

#define	brt(x, y)	(11*x+y)
#define drk(x, y)	-(11*x+y)
#define	LAST		-128		/**< @brief  0200 Marks end of stroke list */
#define	NEGY		-127		/**< @brief  0201 Denotes negative y stroke */
#define bneg(x, y)	NEGY, brt(x, y)
#define dneg(x, y)	NEGY, drk(x, y)

int *tp_getchar(const unsigned char *c);

#endif /* VECTFONT_H */

/** @} */
/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
