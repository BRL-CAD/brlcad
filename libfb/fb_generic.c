/*
  Author -
	Gary S. Moss

  Source -
	SECAD/VLD Computing Consortium, Bldg 394
	The U. S. Army Ballistic Research Laboratory
	Aberdeen Proving Ground, Maryland  21005-5066
  
  Copyright Notice -
	This software is Copyright (C) 1986 by the United States Army.
	All rights reserved.

	$Header$ (BRL)
 */
#ifndef lint
static char RCSid[] = "@(#)$Header$ (BRL)";
#endif
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "fb.h"
#include "./fblocal.h"

/*
 *		f b _ n u l l
 *
 *  Filler for FBIO function slots not used by a particular device
 */
int fb_null( ifp )
FBIO *ifp;
{
	return	0;
}

static
FBIO *_if_list[] = {
#ifdef IF_ADAGE
	&adage_interface,
#endif
#ifdef IF_SUN		/* XXX - perhaps the default should be the first one */
	&sun_interface,
#endif
#ifdef IF_SGI
	&sgi_interface,
#endif
#ifdef IF_RAT
	&rat_interface,
#endif
#ifdef	IF_DEBUG
	&debug_interface,
#endif
	(FBIO *) 0
};

/* Default device name.	 Very BRL specific!	*/
#if defined( vax )
#define FB_DEFAULT_NAME	"/dev/ik0l"
#else
#if defined( gould ) || defined( sel )
#define FB_DEFAULT_NAME	"vgr:/dev/ik0l"
#endif
#endif

#define	DEVNAMLEN	80

FBIO *
fb_open( file, width, height )
char	*file;
int	width, height;
{	
	register FBIO	*ifp;
	int	i;
	char	*cp;
	char	devnambuf[DEVNAMLEN+1];

	if( (ifp = (FBIO *) malloc( sizeof(FBIO) )) == FBIO_NULL )  {
		Malloc_Bomb( sizeof(FBIO) );
		return	FBIO_NULL;
	}
	if( file == NULL )  {
		/* No name given, check environment variable first.	*/
		if( (file = getenv( "FB_FILE" )) == NULL )
			/* None set, use this host's default device.	*/
			file = FB_DEFAULT_NAME;
	}
	/*
	 * Determine what type of hardware the device name refers to.
	 *
	 *  "file" can in general look like: hostname:/pathname/devname#
	 *  If we have a ':' assume the remote interface
	 *    (should we check to see if it's us? XXX)
	 *  else strip out "devname" and try to look it up in the
	 *    device array.  If we don't find it assume it's a file?
	 *
	 * XXX - devname can match a magic cookie regardless of pathname.
	 */
#ifdef IF_REMOTE
	if( (cp = strchr( file, ':' )) != NULL ) {
		/* We have a remote file name of the form <host>:<file>.*/
		file = cp+1;
		*ifp = remote_interface;
		goto found_interface;
	}
#endif IF_REMOTE
	/* NOTE: we are assuming no host names at this point		*/
	if( (cp = strrchr( file, '/' )) == NULL )
		cp = file;
	else
		cp++;	/* start just past the slash			*/
	i = 0;
	while( *cp != NULL && !isdigit(*cp) && i < DEVNAMLEN ) {
		devnambuf[ i++ ] = *cp++;
	}
	devnambuf[i] = 0;

#ifdef IF_PTTY
	if( strncmp( devnambuf, "tty", 3 ) == 0 && devnambuf[3] != '\0' ) {
		/* We have a UNIX pseudo-tty presumably (tty[pqr]*).	*/
		*ifp = ptty_interface;
		goto found_interface;
	}
#endif IF_PTTY

	i = 0;
	while( _if_list[i] != (FBIO *)NULL ) {
		if( strcmp( &devnambuf[0], _if_list[i]->if_name ) == 0 )
			break;
		i++;
	}
	if( _if_list[i] != (FBIO *)NULL )
		*ifp = *(_if_list[i]);
	else {
#ifdef IF_DISK
		*ifp = disk_interface;
#else
		fb_log( "fb_open : device type \"%s\" wrong.\n", file );
		free( (void *) ifp );
		return	FBIO_NULL;
#endif IF_DISK
	}

found_interface:
	if( (ifp->if_name = malloc( (unsigned) strlen( file ) + 1 ))
	    == (char *) NULL
	    )
	{
		Malloc_Bomb( strlen( file ) + 1 );
		free( (void *) ifp );
		return	FBIO_NULL;
	}
	(void) strcpy( ifp->if_name, file );

	if( (*ifp->if_dopen)( ifp, file, width, height ) == -1 )  {
		fb_log(	"fb_open : can not open device \"%s\".\n",
		file
		    );
		free( (void *) ifp->if_name );
		free( (void *) ifp );
		return	FBIO_NULL;
	}
	return	ifp;
}

int
fb_close( ifp )
FBIO	*ifp;
{
	if( (*ifp->if_dclose)( ifp ) == -1 )  {
		fb_log(	"Can not close device \"%s\".\n", ifp->if_name );
		return	-1;
	}
	if( ifp->if_pbase != PIXEL_NULL )
		free( (void *) ifp->if_pbase );
	free( (void *) ifp->if_name );
	free( (void *) ifp );
	return	0;
}

/*	f a s t _ d m a _ b g
	Clear the frame buffer to the background color.
 */
int
fb_fast_dma_bg( ifp, bpp )
register FBIO	*ifp;
register Pixel	*bpp;
{	
	static Pixel	*pix_buf = NULL;
	register int	i, y, scans_per_dma;
	register Pixel	*pix_to;
	if( pix_buf == NULL )
		if( (pix_buf = (Pixel *) malloc(MAX_BYTES_DMA)) == PIXEL_NULL )
		{
			Malloc_Bomb(MAX_BYTES_DMA);
			return	-1;
		}
	/* Fill buffer with background color.				*/
	for( i = 0, pix_to = pix_buf; i < MAX_PIXELS_DMA; ++i )
		*pix_to++ = *bpp;
	/* Send until frame buffer is full.				*/
	scans_per_dma = MAX_PIXELS_DMA / ifp->if_width;
	for( y = 0; y < ifp->if_height; y += scans_per_dma )
		if( fb_write( ifp, 0, y, pix_buf, (long) MAX_PIXELS_DMA	) == -1 )
			return	-1;
	return	0;
}
