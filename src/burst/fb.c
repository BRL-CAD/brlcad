/*                            F B . C
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
 *
 */
/** @file fb.c
	Author:		Gary S. Moss
			U. S. Army Ballistic Research Laboratory
			Aberdeen Proving Ground
			Maryland 21005-5066
*/
#ifndef lint
static const char RCSid[] = "@(#)$Header$ (BRL)";
#endif
#include <stdio.h>
#include <string.h>
#include <memory.h>
#include <signal.h>
#include "fb.h"
#include "./burst.h"
#include "./ascii.h"
#include "./extern.h"

boolean
imageInit()
	{	boolean needopen = 0;
		static char lastfbfile[LNBUFSZ]={0}; /* last fbfile */
	devwid = 512;
	devhgt = 512;
	if( fbfile[0] == NUL )
		return	1;
	zoom = devwid / gridsz - 1;
	while( zoom < 3  && devwid < MAXDEVWID )
		{
		devwid *= 2;
		devhgt *= 2;
		zoom = devwid / gridsz;
		}
	if( zoom * gridsz == devwid )
		zoom--;
	if( zoom < 1 )
		zoom = 1;

	/* Determine whether it is necessary to open fbfile. */
	if( fbiop == FBIO_NULL || fb_getwidth( fbiop ) != devwid )
		needopen = 1; /* not currently open or size changed */
	else
	if( lastfbfile[0] != NUL && strcmp( fbfile, lastfbfile ) != 0 )
		needopen = 1; /* name changed */
	(void) strcpy( lastfbfile, fbfile );

	if( needopen )
		{
		if( ! openFbDevice( fbfile ) )
			return	0;
		paintGridFb();
		}
	else
	if( !(firemode & FM_SHOT) || (firemode & FM_FILE) )
		paintGridFb();
	return	1;
	}

/*	openFbDevice( char *devname )

	Must be called after gridInit() so that gridsz is setup.
 */
boolean
openFbDevice( devname )
char	*devname;
	{	boolean	ret = 1;
	notify( "Opening frame buffer", NOTIFY_APPEND );
	if( zoom < 1 )
		{
		prntScr( "Device is too small to display image." );
		ret = 0;
		goto	safe_exit;
		}
	if(	(	(fbiop != FBIO_NULL && fb_getwidth( fbiop ) != devwid)
		||	pixgrid == NULL)
		&&	(pixgrid = (unsigned char *) calloc( devwid*3, sizeof(unsigned char) ))
				== (unsigned char *) NULL )
		{
			prntScr( "Memory allocation of %d bytes failed.",
				sizeof(unsigned char)*devwid );
			ret = 0;
			goto	safe_exit;
		}
	(void) memset( (char *) pixgrid, NUL, sizeof(unsigned char)*devwid*3 );
	if( fbiop != FBIO_NULL )
		{
#if SGI_WINCLOSE_BUG
		if( strncmp( fbiop->if_name, "/dev/sgi", 8 ) == 0 )
			{
			prntScr( "IRIS can't change window size." );
			ret = 0;
			goto	safe_exit;
			}
		else
#endif
		if( ! closFbDevice() )
			{
			ret = 0;
			goto	safe_exit;
			}

		}
	fbiop = fb_open( devname, devwid, devhgt );
	if( fbiop == NULL )
		{
		ret = 0;
		goto	safe_exit;
		}
	else if( fb_clear( fbiop, pixblack ) == -1
	    ||	(notify( "Zooming", NOTIFY_APPEND ),
		 fb_zoom( fbiop, 1, 1 ) == -1)
	    ||	(notify( "Windowing", NOTIFY_DELETE ),
		 fb_window( fbiop, devwid/2, devhgt/2 ) == -1)
		)
		{
		ret = 0;
		goto	safe_exit;
		}
safe_exit : notify( NULL, NOTIFY_DELETE );
	return	ret;
	}

boolean
closFbDevice()
	{	int	ret;
	notify( "Closing frame buffer", NOTIFY_APPEND );
	if( fb_close( fbiop ) == -1 )
		ret = 0;
	else
		{
		ret = 1;
		fbiop = FBIO_NULL;
		}
	notify( NULL, NOTIFY_DELETE );
	return	ret;
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
