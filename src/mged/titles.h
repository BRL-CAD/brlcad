/*                        T I T L E S . H
 * BRL-CAD
 *
 * Copyright (c) 1986-2007 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file titles.h
 *
 *  Constants that describe the layout of the faceplate.
 *
 *  Author -
 *	Michael John Muuss
 *
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 *
 *  $Header$
 */
#define XMIN		(-2048)
#define XMAX		(2047)
#define YMIN		(-2048)
#define YMAX		(2047)
#define	MENUXLIM	(-1250)		/* Value to set X lim to for menu */
#define	MENUX		(-2048+115)	/* pixel position for menu, X */
#define	MENUY		1780		/* pixel position for menu, Y */
#define	SCROLLY		(2047)		/* starting Y pos for scroll area */
#define	MENU_DY		(-104)		/* Distance between menu items */
#define SCROLL_DY	(-100)		/* Distance between scrollers */

#define TITLE_XBASE	(-2048)		/* pixel X of title line start pos */
#define TITLE_YBASE	(-1920)		/* pixel pos of last title line */
#define SOLID_XBASE	MENUXLIM	/* X to start display text */
#define SOLID_YBASE	( 1920)		/* pixel pos of first solid line */
#define TEXT0_DY	(  -60)		/* #pixels per line, Size 0 */
#define TEXT1_DY	(  -90)		/* #pixels per line, Size 1 */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
