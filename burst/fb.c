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

void
imageInit()
	{
	if( fbfile[0] == NUL )
		return;
	do
		{
		zoom = devwid / gridsz - 1;
		if( zoom * gridsz == devwid )
			zoom--;
		if( zoom < 3 )
			{
			devwid *= 2;
			devhgt *= 2;
			}
		}
	while( zoom < 3  && devwid < MAXDEVWID );
	if( fbiop == FBIO_NULL )
		{
		if( ! openFbDevice( fbfile ) )
			return;
		paintGridFb();
		}
	else
	if( !(firemode & FM_SHOT) || (firemode & FM_FILE) )
		paintGridFb();
	return;
	}

/*	openFbDevice( char *devname )

	Must be called after gridInit() so that gridsz is setup.
 */
bool
openFbDevice( devname )
char	*devname;
	{
	notify( "Opening frame buffer...", NOTIFY_APPEND );
	if( zoom < 1 )
		{
		rt_log( "Device is too small to display image.\n" );
		return	false;
		}
	if(	pixgrid == NULL
	    && (pixgrid = (RGBpixel *) malloc( sizeof(RGBpixel)*devwid ))
		== (RGBpixel *) NULL )
		{
		rt_log( "Memory allocation of %d bytes failed.\n",
			sizeof(RGBpixel)*devwid );
		return	false;
		}
	(void) memset( (char *) pixgrid, NUL, sizeof(RGBpixel)*devwid );
	if(	fbiop == FBIO_NULL
	   &&  ((fbiop = fb_open( devname, devwid, devhgt )) == FBIO_NULL
	    ||	fb_clear( fbiop, pixblack ) == -1
	    ||	notify( "Zooming...", NOTIFY_APPEND ),
			fb_zoom( fbiop, 1, 1 ) == -1
	    ||	notify( "Windowing...", NOTIFY_DELETE ),
			fb_window( fbiop, devwid/2, devhgt/2 ) == -1
	       )
		)
		return	false;
	else
	if(	strncmp( devname, "/dev/sgi", 8 ) != 0
	    &&	(fbiop = fb_open( devname, devwid, devhgt )) == FBIO_NULL
		)
		return	false;
	notify( NULL, NOTIFY_DELETE );
	return	true;
	}

bool
closFbDevice()
	{	int	ret;
	notify( "Closing frame buffer...", NOTIFY_APPEND );
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
