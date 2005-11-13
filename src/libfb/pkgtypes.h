/*                      P K G T Y P E S . H
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

/** \addtogroup libfb */
/*@{*/

/** @file pkgtypes.h
 *  Types of packages used for the remote frame buffer
 *  communication between libfb/if_remote.c and rfbd/rfbd.c.
 *  This file is shared by those two modules.
 *
 *  Authors -
 *	Phil Dykstra
 *	Michael John Muuss
 *
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005
 *
 *  $Header$
 */
/*@}*/

#define	MSG_FBOPEN	1
#define	MSG_FBCLOSE	2
#define	MSG_FBCLEAR	3
#define	MSG_FBREAD	4
#define	MSG_FBWRITE	5
#define	MSG_FBCURSOR	6		/* fb_cursor() */
#define	MSG_FBWINDOW	7		/* OLD */
#define	MSG_FBZOOM	8		/* OLD */
#define	MSG_FBSCURSOR	9		/* OLD */
#define	MSG_FBVIEW	10		/* NEW */
#define	MSG_FBGETVIEW	11		/* NEW */
#define	MSG_FBRMAP	12
#define	MSG_FBWMAP	13
#define	MSG_FBHELP	14
#define	MSG_FBREADRECT	15
#define	MSG_FBWRITERECT	16
#define	MSG_FBFLUSH	17
#define	MSG_FBFREE	18
#define	MSG_FBGETCURSOR	19		/* NEW */
#define	MSG_FBPOLL	30		/* NEW */
#define MSG_FBSETCURSOR	31		/* NEW in Release 4.4 */
#define	MSG_FBBWREADRECT 32		/* NEW in Release 4.6 */
#define	MSG_FBBWWRITERECT 33		/* NEW in Release 4.6 */

#define	MSG_DATA	20
#define	MSG_RETURN	21
#define	MSG_CLOSE	22
#define	MSG_ERROR	23

#define	MSG_NORETURN	100

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
