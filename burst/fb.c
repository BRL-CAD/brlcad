/*
	Author:		Gary S. Moss
			U. S. Army Ballistic Research Laboratory
			Aberdeen Proving Ground
			Maryland 21005-5066
			(301)278-6651 or AV-298-6651
*/
#ifndef lint
static char RCSid[] = "@(#)$Header$ (BRL)";
#endif
#include <stdio.h>
#include <string.h>
#include <memory.h>
#include "fb.h"
#include "./burst.h"
#include "./ascii.h"
#include "./extern.h"

bool
imageInit()
	{
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
	if( fbiop == FBIO_NULL )
		{
		if( ! openFbDevice( fbfile ) )
			return	false;
		paintGridFb();
		}
	else
	if( fb_getwidth( fbiop ) != devwid )
		{
		prntScr( "IRIS can't change window size." );
		return	false;
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
	if(	pixgrid == NULL
	    && (pixgrid = (RGBpixel *) malloc( sizeof(RGBpixel)*devwid ))
		== (RGBpixel *) NULL )
		{
		prntScr( "Memory allocation of %d bytes failed.",
			sizeof(RGBpixel)*devwid );
		ret = false;
		goto	safe_exit;
		}
	(void) memset( (char *) pixgrid, NUL, sizeof(RGBpixel)*devwid );
	if(	fbiop == FBIO_NULL
	   &&  ((fbiop = fb_open( devname, devwid, devhgt )) == FBIO_NULL
	    ||	fb_clear( fbiop, pixblack ) == -1
	    ||	notify( "Zooming", NOTIFY_APPEND ),
			fb_zoom( fbiop, 1, 1 ) == -1
	    ||	notify( "Windowing", NOTIFY_DELETE ),
			fb_window( fbiop, devwid/2, devhgt/2 ) == -1
	       )
		)
		{
		ret = false;
		goto	safe_exit;
		}
	else
	if(	strncmp( devname, "/dev/sgi", 8 ) != 0
	    &&	(fbiop = fb_open( devname, devwid, devhgt )) == FBIO_NULL
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
	if( strncmp( fbiop->if_name, "/dev/sgi", 8 ) != 0 )
		{
	    	if( fb_close( fbiop ) == -1 )
			ret = false;
		else
			ret = true;
		}
	else
		ret = false;
	notify( NULL, NOTIFY_DELETE );
	return	ret;
	}
