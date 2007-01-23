/*                         A S C I I . H
 * BRL-CAD
 *
 * Copyright (c) 2004-2007 United States Government as represented by
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
/** @file ascii.h
	SCCS id:	@(#) ascii.h	2.1
	Modified: 	12/9/86 at 15:55:31
	Retrieved: 	12/26/86 at 21:53:36
	SCCS archive:	/vld/moss/src/fbed/s.ascii.h

	Author:		Gary S. Moss
			U. S. Army Ballistic Research Laboratory
			Aberdeen Proving Ground
			Maryland 21005-5066
			(301)278-6651 or DSN 298-6651
*/
#define NUL		'\000'
#define SOH		'\001'
#define	STX		'\002'
#define INTR		'\003'
#define	EOT		'\004'
#define ACK		'\006'
#define BEL		'\007'
#define	BS		'\010'
#define HT		'\011'
#define LF		'\012'
#define FF		'\014'
#define	CR		'\015'
#define DLE		'\020'
#define	DC1		'\021'
#define	DC2		'\022'
#define	DC3		'\023'
#define	DC4		'\024'
#define	KILL		'\025'
#define	CAN		'\030'
#define	ESC		'\033'
#define	GS		'\035'
#define	RS		'\036'
#define	US		'\037'
#define SP		'\040'
#define DEL		'\177'

#define Ctrl(chr)	((int)chr&037)

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
