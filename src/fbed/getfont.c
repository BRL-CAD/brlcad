/*                       G E T F O N T . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2007 United States Government as represented by
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
/** @file getfont.c
	SCCS id:	@(#) getfont.c	2.1
	Modified: 	12/9/86 at 15:54:45
	Retrieved: 	12/26/86 at 21:54:19
	SCCS archive:	/vld/moss/src/fbed/s.getfont.c

	Authors:	Paul R. Stay	(original author)
			Gary S. Moss	(port to big-endian machine)

			U. S. Army Ballistic Research Laboratory
			Aberdeen Proving Ground
			Maryland 21005-5066
			(301)278-6651 or DSN 298-6651
*/
#if ! defined( lint )
static const char RCSid[] = "@(#) getfont.c 2.1, modified 12/9/86 at 15:54:45, archive /vld/moss/src/fbed/s.getfont.c";
#endif
/*
	getfont.c - Load a new font by reading in the header and directory.
 */
#include "common.h"


#include <stdio.h>

#ifdef HAVE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif

#include "./font.h"

/* Variables controlling the font itself */
FILE		*ffdes;		/* File pointer for current font. */
int offset;		/* Current offset to character data. */
struct header	hdr;		/* Header for font file. */
struct dispatch	dir[256];	/* Directory for character font. */
int width = 0,	/* Size of current character. */
		height = 0;

extern void fb_log(char *fmt, ...);

int
get_Font(char *fontname)
{	FILE		*newff;
		struct header	lochdr;
		static char fname[FONTNAMESZ];
	if( fontname == NULL )
		fontname = FONTNAME;
	if( fontname[0] != '/' )		/* absolute path */
		(void) sprintf( fname, "%s/%s", FONTDIR, fontname );
	else
		(void) strncpy( fname, fontname, FONTNAMESZ );

	/* Open the file and read in the header information. */
	if( (newff = fopen( fname, "r" )) == NULL )
		{
		fb_log( "Error opening font file '%s'\n", fname );
		ffdes = NULL;
		return 0;
    		}
	if( ffdes != NULL )
		(void) fclose(ffdes);
	ffdes = newff;
	if( fread( (char *) &lochdr, (int) sizeof(struct header), 1, ffdes ) != 1 )
		{
		fb_log( "get_Font() read failed!\n" );
		ffdes = NULL;
		return 0;
		}
	SWAB( lochdr.magic );
	SWAB( lochdr.size );
	SWAB( lochdr.maxx );
	SWAB( lochdr.maxy );
	SWAB( lochdr.xtend );

	if( lochdr.magic != 0436 )
    		{
		fb_log( "Not a font file \"%s\": magic=0%o\n",
			fname, (int) lochdr.magic
			);
		ffdes = NULL;
		return 0;
		}
	hdr = lochdr;

	/* Read in the directory for the font. */
	if( fread( (char *) dir, (int) sizeof(struct dispatch), 256, ffdes ) != 256 )
		{
		fb_log( "get_Font() read failed!\n" );
		ffdes = NULL;
		return 0;
		}
	/* Addresses of characters in the file are relative to
		point in the file after the directory, so grab the
		current position.
	 */
 	offset = ftell( ffdes );
	return 1;
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
