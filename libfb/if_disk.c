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
#ifdef BSD
#include <sys/file.h>
#else
#include <fcntl.h>
#endif
#include "fb.h"
#include "./fblocal.h"

#define FILE_CMAP_ADDR	((long) ifp->if_width*ifp->if_height\
			*sizeof(RGBpixel))

/* Ensure integer number of pixels per DMA */
#define	DISK_DMA_BYTES	(16*1024/sizeof(RGBpixel)*sizeof(RGBpixel))
#define	DISK_DMA_PIXELS	(DISK_DMA_BYTES/sizeof(RGBpixel))

_LOCAL_ int	dsk_dopen(),
		dsk_dclose(),
		dsk_dclear(),
		dsk_bread(),
		dsk_bwrite(),
		dsk_cmread(),
		dsk_cmwrite();

FBIO disk_interface =
		{
		dsk_dopen,
		dsk_dclose,
		fb_null,		/* dreset */
		dsk_dclear,
		dsk_bread,
		dsk_bwrite,
		dsk_cmread,
		dsk_cmwrite,
		fb_null,		/* viewport_set */
		fb_null,		/* window_set */
		fb_null,		/* zoom_set */
		fb_null,		/* curs_set */
		fb_null,		/* cmemory_addr */
		fb_null,		/* cscreen_addr */
		"Disk File Interface",
		16*1024,		/* the sky's really the limit here */
		16*1024,
		"*disk*",
		512,
		512,
		-1,
		PIXEL_NULL,
		PIXEL_NULL,
		PIXEL_NULL,
		-1,
		0,
		0L,
		0L,
		0
		};

_LOCAL_ int	disk_color_clear();

_LOCAL_ int
dsk_dopen( ifp, file, width, height )
FBIO	*ifp;
char	*file;
int	width, height;
	{
	static char zero = 0;

	/* check for default size */
	if( width == 0 )
		width = ifp->if_width;
	if( height == 0 )
		height = ifp->if_height;

	if( (ifp->if_fd = open( file, O_RDWR, 0 )) == -1
	  && (ifp->if_fd = open( file, O_RDONLY, 0 )) == -1 ) {
		if( (ifp->if_fd = open( file, O_RDWR|O_CREAT, 0664 )) > 0 ) {
			/* New file, write byte at end */
			if( lseek( ifp->if_fd, height*width*sizeof(RGBpixel)-1, 0 ) == -1 )
				{
				fb_log( "disk_device_open : can not seek to end of new file.\n" );
				return	-1;
				}
			if( write( ifp->if_fd, &zero, 1 ) < 0 )
				{
				fb_log( "disk_device_open : initial write failed.\n" );
				return	-1;
				}
		} else
			return	-1;
	}
	ifp->if_width = width;
	ifp->if_height = height;
	if( lseek( ifp->if_fd, 0L, 0 ) == -1L )
		{
		fb_log( "disk_device_open : can not seek to beginning.\n" );
		return	-1;
		}
	return	0;
	}

_LOCAL_ int
dsk_dclose( ifp )
FBIO	*ifp;
	{
	return	close( ifp->if_fd );
	}

_LOCAL_ int
dsk_dclear( ifp, bgpp )
FBIO	*ifp;
RGBpixel	*bgpp;
{
	static RGBpixel	black = { 0, 0, 0 };

	if( bgpp == (RGBpixel *)NULL )
		return disk_color_clear( ifp, black );

	return	disk_color_clear( ifp, bgpp );
}

_LOCAL_ int
dsk_bread( ifp, x, y, pixelp, count )
FBIO	*ifp;
int	x,  y;
RGBpixel	*pixelp;
int	count;
{	register long bytes = count * (long) sizeof(RGBpixel);
	register long todo;

	if( lseek(	ifp->if_fd,
			(((long) y * (long) ifp->if_width) + (long) x)
			* (long) sizeof(RGBpixel),
			0
			)
	    == -1L
		)
		{
		fb_log( "disk_buffer_read : seek to %ld failed.\n",
			(((long) y * (long) ifp->if_width) + (long) x)
			* (long) sizeof(RGBpixel)
			);
		return	-1;
		}
	while( bytes > 0 )
		{
#ifdef never
		todo = (bytes > DISK_DMA_BYTES ? DISK_DMA_BYTES : bytes);
#else
		todo = bytes;
#endif
		if( read( ifp->if_fd, (char *) pixelp, todo ) != todo )
			return	-1;
		bytes -= todo;
		pixelp += todo / sizeof(RGBpixel);
		}
	return	count;
	}

_LOCAL_ int
dsk_bwrite( ifp, x, y, pixelp, count )
FBIO	*ifp;
int	x, y;
RGBpixel	*pixelp;
long	count;
	{	register long	bytes = count * (long) sizeof(RGBpixel);
		register int	todo;
	if( lseek(	ifp->if_fd,
			((long) y * (long) ifp->if_width + (long) x)
			* (long) sizeof(RGBpixel),
			0
			)
	    == -1L
		)
		{
		fb_log( "disk_buffer_write : seek to %ld failed.\n",
			(((long) y * (long) ifp->if_width) + (long) x)
			* (long) sizeof(RGBpixel)
			);
		return	-1;
		}
	while( bytes > 0 )
		{
#ifdef never
		todo = (bytes > DISK_DMA_BYTES ? DISK_DMA_BYTES : bytes);
#else
		todo = bytes;
#endif
		if( write( ifp->if_fd, (char *) pixelp, todo ) != todo )
			return	-1;
		bytes -= todo;
		pixelp += todo / sizeof(RGBpixel);
		}
	return	count;
	}

_LOCAL_ int
dsk_cmread( ifp, cmap )
FBIO	*ifp;
ColorMap	*cmap;
	{
	if( lseek( ifp->if_fd, FILE_CMAP_ADDR, 0 ) == -1 )
		{
		fb_log(	"disk_colormap_read : seek to %ld failed.\n",
				FILE_CMAP_ADDR
				);
	   	return	-1;
		}
	if( read( ifp->if_fd, (char *) cmap, sizeof(ColorMap) )
		!= sizeof(ColorMap)
		)
		{ /* Not necessarily an error.  It is not required
			that a color map be saved and the standard 
			map is not generally saved.
		   */
		return	-1;
		}
	return	0;
	}

_LOCAL_ int
dsk_cmwrite( ifp, cmap )
FBIO	*ifp;
ColorMap	*cmap;
	{
	if( cmap == (ColorMap *) NULL )
		/* Do not write default map to file.			*/
		return	0;
	if( lseek( ifp->if_fd, FILE_CMAP_ADDR, 0 ) == -1 )
		{
		fb_log(	"disk_colormap_write : seek to %ld failed.\n",
				FILE_CMAP_ADDR
				);
		return	-1;
		}
	if( write( ifp->if_fd, (char *) cmap, sizeof(ColorMap) )
		!= sizeof(ColorMap)
		)
		{
	    	fb_log( "disk_colormap_write : write failed.\n" );
		return	-1;
		}
	return	0;
	}

/*
 *		D I S K _ C O L O R _ C L E A R
 *
 *  Clear the disk file to the given color.
 */
_LOCAL_ int
disk_color_clear( ifp, bpp )
FBIO	*ifp;
register RGBpixel	*bpp;
{
	static RGBpixel	*pix_buf = NULL;
	register RGBpixel *pix_to;
	register long	i;
	int	fd, pixelstodo;

	if( pix_buf == NULL )
		if( (pix_buf = (RGBpixel *) malloc(DISK_DMA_BYTES)) == PIXEL_NULL ) {
			Malloc_Bomb(DISK_DMA_BYTES);
			return	-1;
		}

	/* Fill buffer with background color. */
	for( i = DISK_DMA_PIXELS, pix_to = pix_buf; i > 0; i-- ) {
		COPYRGB( *pix_to, *bpp );
		pix_to++;
	}

	/* Set start of framebuffer */
	fd = ifp->if_fd;
	if( lseek( fd, 0L, 0 ) == -1 ) {
		fb_log( "disk_color_clear : seek failed.\n" );
		return	-1;
	}

	/* Send until frame buffer is full. */
	pixelstodo = ifp->if_height * ifp->if_width;
	while( pixelstodo > 0 ) {
		i = pixelstodo > DISK_DMA_PIXELS ? DISK_DMA_PIXELS : pixelstodo;
		if( write( fd, pix_buf, i * sizeof(RGBpixel) ) == -1 )
			return	-1;
		pixelstodo -= i;
	}

	return	0;
}
