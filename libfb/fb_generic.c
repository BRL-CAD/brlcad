/*
 *			F B _ G E N E R I C
 *
 *  Authors -
 *	Phil Dykstra
 *	Gary S. Moss
 *	Michael John Muuss
 *
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 *  
 *  Copyright Notice -
 *	This software is Copyright (C) 1986 by the United States Army.
 *	All rights reserved.
 */
#ifndef lint
static char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include <stdio.h>
#include <ctype.h>
#ifdef BSD
#include <strings.h>
#else
#include <string.h>
#endif

#include "fb.h"
#include "./fblocal.h"

extern char *getenv();

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

#ifdef IF_REMOTE
extern FBIO remote_interface;	/* not in list[] */
#endif
#ifdef IF_PTTY
extern FBIO ptty_interface;	/* not in list[] */
#endif

#ifdef IF_ADAGE
extern FBIO adage_interface;
#endif
#ifdef IF_SUN
extern FBIO sun_interface;
#endif
#ifdef IF_SGI
extern FBIO sgi_interface;
#endif
#ifdef IF_RAT
extern FBIO rat_interface;
#endif

extern FBIO debug_interface, disk_interface;	/* Always included */

/* First element of list is default device when no name given */
static
FBIO *_if_list[] = {
#ifdef IF_ADAGE
	&adage_interface,
#endif
#ifdef IF_SUN
	&sun_interface,
#endif
#ifdef IF_SGI
	&sgi_interface,
#endif
#ifdef IF_RAT
	&rat_interface,
#endif
	&debug_interface,
	(FBIO *) 0
};

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
		if( (file = getenv( "FB_FILE" )) == NULL )  {
			/* None set, use first device as default */
			*ifp = *(_if_list[0]);	/* struct copy */
			file = ifp->if_name;
			goto found_interface;
		}
	}
	/*
	 * Determine what type of hardware the device name refers to.
	 *
	 *  "file" can in general look like: hostname:/pathname/devname#
	 *  If we have a ':' assume the remote interface
	 *    (should we check to see if it's us? XXX)
	 *  else strip out "devname" and try to look it up in the
	 *    device array.  If we don't find it assume it's a file.
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
	cp = file;
	i = 0;
	while( *cp != NULL && !isdigit(*cp) && i < DEVNAMLEN ) {
		devnambuf[ i++ ] = *cp++;
	}
	devnambuf[i] = 0;

#ifdef IF_PTTY
	if( strncmp( devnambuf, "/dev/tty", 8 ) == 0 && devnambuf[8] != '\0' ) {
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
		*ifp = disk_interface;
	}

found_interface:
	/* Copy over the name it was opened by. */
	if( (ifp->if_name = malloc( (unsigned) strlen( file ) + 1 ))
	    == (char *) NULL
	    )
	{
		Malloc_Bomb( strlen( file ) + 1 );
		free( (void *) ifp );
		return	FBIO_NULL;
	}
	(void) strcpy( ifp->if_name, file );

	/* set default width/height */
	if( width == 0 )
		width = ifp->if_width;
	if( height == 0 )
		height = ifp->if_height;

	if( (*ifp->if_dopen)( ifp, file, width, height ) == -1 )  {
		fb_log(	"fb_open : can not open device \"%s\".\n", file );
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
	for( y = ifp->if_height-1; y > 0; y -= scans_per_dma )
		if( fb_write( ifp, 0, y, pix_buf, (long) MAX_PIXELS_DMA	) == -1 )
			return	-1;
	return	0;
}
