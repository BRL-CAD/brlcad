/*                        O B J D I R . H
 * BRL-CAD
 *
 * Copyright (C) 1985-2005 United States Government as represented by
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
/** @file objdir.h
 *			D I R . H
 *
 * The in-core object directory
 *
 *  Author -
 *	Michael John Muuss
 *  
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005
 *
 *  $Header$
 */

struct directory  {
	char		*d_namep;	/* pointer to name string */
	long		d_addr;		/* disk address in obj file */
	short		d_flags;	/* flags */
	short		d_len;		/* # of db granules used by obj */
	short		d_nref;		/* # times referenced by COMBs */
	struct directory *d_forw;	/* forward link */
};
#define DIR_NULL	((struct directory *) NULL)

#define DIR_SOLID	0x1		/* this name is a solid */
#define DIR_COMB	0x2		/* combination */
#define DIR_REGION	0x4		/* region */
#define DIR_BRANCH	0x8		/* branch name */

#define LOOKUP_QUIET	0
#define LOOKUP_NOISY	1

#ifndef NAMESIZE
#define NAMESIZE		16
#endif

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
