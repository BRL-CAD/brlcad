/*
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

bool
imageInit()
	{	bool needopen = false;
		static char lastfbfile[LNBUFSZ]={0}; /* last fbfile */
	devwid = 512;
	devhgt = 512;
	if( fbfile[0] == NUL )
		return	true;
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
		needopen = true; /* not currently open or size changed */
	else
	if( lastfbfile[0] != NUL && strcmp( fbfile, lastfbfile ) != 0 )
		needopen = true; /* name changed */
	(void) strcpy( lastfbfile, fbfile );

	if( needopen )
		{
		if( ! openFbDevice( fbfile ) )
			return	false;
		paintGridFb();
		}
	else
	if( !(firemode & FM_SHOT) || (firemode & FM_FILE) )
		paintGridFb();
	return	true;
	}

/*	openFbDevice( char *devname )

	Must be called after gridInit() so that gridsz is setup.
 */
bool
openFbDevice( devname )
char	*devname;
	{	bool	ret = true;
	notify( "Opening frame buffer", NOTIFY_APPEND );
	if( zoom < 1 )
		{
		prntScr( "Device is too small to display image." );
		ret = false;
		goto	safe_exit;
		}
	if(	(	(fbiop != FBIO_NULL && fb_getwidth( fbiop ) != devwid)
		||	pixgrid == NULL)
		&&	(pixgrid = (unsigned char *) calloc( devwid*3, sizeof(unsigned char) ))
				== (unsigned char *) NULL )
		{
			prntScr( "Memory allocation of %d bytes failed.",
				sizeof(unsigned char)*devwid );
			ret = false;
			goto	safe_exit;
		}
	(void) memset( (char *) pixgrid, NUL, sizeof(unsigned char)*devwid*3 );
	if( fbiop != FBIO_NULL )
		{
#if SGI_WINCLOSE_BUG
		if( strncmp( fbiop->if_name, "/dev/sgi", 8 ) == 0 )
			{
			prntScr( "IRIS can't change window size." );
			ret = false;
			goto	safe_exit;
			}
		else
#endif
		if( ! closFbDevice() )
			{
			ret = false;
			goto	safe_exit;
			}

		}
	fbiop = fb_open( devname, devwid, devhgt );
	if( fbiop == NULL )
		{
		ret = false;
		goto	safe_exit;
		}
	else if( fb_clear( fbiop, pixblack ) == -1
	    ||	(notify( "Zooming", NOTIFY_APPEND ),
		 fb_zoom( fbiop, 1, 1 ) == -1)
	    ||	(notify( "Windowing", NOTIFY_DELETE ),
		 fb_window( fbiop, devwid/2, devhgt/2 ) == -1)
		)
		{
		ret = false;
		goto	safe_exit;
		}
safe_exit : notify( NULL, NOTIFY_DELETE );
	return	ret;
	}

bool
closFbDevice()
	{	int	ret;
	notify( "Closing frame buffer", NOTIFY_APPEND );
	if( fb_close( fbiop ) == -1 )
		ret = false;
	else
		{
		ret = true;
		fbiop = FBIO_NULL;
		}
	notify( NULL, NOTIFY_DELETE );
	return	ret;
	}
