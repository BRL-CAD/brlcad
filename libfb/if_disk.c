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
			*sizeof(Pixel))

_LOCAL_ int	disk_dopen(),
		disk_dclose(),
		disk_dclear(),
		disk_bread(),
		disk_bwrite(),
		disk_cmread(),
		disk_cmwrite();

FBIO disk_interface =
		{
		disk_dopen,
		disk_dclose,
		fb_null,		/* dreset */
		disk_dclear,
		disk_bread,
		disk_bwrite,
		disk_cmread,
		disk_cmwrite,
		fb_null,		/* viewport_set */
		fb_null,		/* window_set */
		fb_null,		/* zoom_set */
		fb_null,		/* cinit_bitmap */
		fb_null,		/* cmemory_addr */
		fb_null,		/* cscreen_addr */
		"Disk File Interface",
		8*1024,		/* the sky's really the limit here... */
		8*1024,
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
		0L
		};

_LOCAL_ int
disk_dopen( ifp, file, width, height )
FBIO	*ifp;
char	*file;
int	width, height;
	{
	static unsigned char zero = 0;

	if( (ifp->if_fd = open( file, O_RDWR, 0 )) == -1
	  && (ifp->if_fd = open( file, O_RDONLY, 0 )) == -1 ) {
		if( (ifp->if_fd = open( file, O_RDWR|O_CREAT, 0664 )) > 0 ) {
			/* New file, write byte at end */
			if( lseek( ifp->if_fd, height*width*4-1, 0 ) == -1 )
				{
				fb_log( "disk_device_open : can not seek to end of new file.\n" );
				return	-1;
				}
			if( write( ifp->if_fd, zero, 1 ) < 0 )
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
disk_dclose( ifp )
FBIO	*ifp;
	{
	return	close( ifp->if_fd );
	}

_LOCAL_ int
disk_dclear( ifp, bgpp )
FBIO	*ifp;
Pixel	*bgpp;
	{
	return	fb_fast_dma_bg( ifp, bgpp );
	}

_LOCAL_ int
disk_bread( ifp, x, y, pixelp, count )
FBIO	*ifp;
int		x,  y;
Pixel		*pixelp;
int		count;
	{	register long bytes = count * (long) sizeof(Pixel);
		register long todo;
	if( lseek(	ifp->if_fd,
			(((long) y * (long) ifp->if_width) + (long) x)
			* (long) sizeof(Pixel),
			0
			)
	    == -1L
		)
		{
		fb_log( "disk_buffer_read : seek to %ld failed.\n",
			(((long) y * (long) ifp->if_width) + (long) x)
			* (long) sizeof(Pixel)
			);
		return	-1;
		}
	while( bytes > 0 )
		{
		todo = (bytes > MAX_BYTES_DMA ? MAX_BYTES_DMA : bytes);
		if( read( ifp->if_fd, (char *) pixelp, todo ) != todo )
			return	-1;
		bytes -= todo;
		pixelp += todo / sizeof(Pixel);
		}
	return	0;
	}

_LOCAL_ int
disk_bwrite( ifp, x, y, pixelp, count )
FBIO	*ifp;
int	x, y;
Pixel	*pixelp;
long	count;
	{	register long	bytes = count * (long) sizeof(Pixel);
		register int	todo;
	if( lseek(	ifp->if_fd,
			((long) y * (long) ifp->if_width + (long) x)
			* (long) sizeof(Pixel),
			0
			)
	    == -1L
		)
		{
		fb_log( "disk_buffer_write : seek to %ld failed.\n",
			(((long) y * (long) ifp->if_width) + (long) x)
			* (long) sizeof(Pixel)
			);
		return	-1;
		}
	while( bytes > 0 )
		{
		todo = (bytes > MAX_BYTES_DMA ? MAX_BYTES_DMA : bytes);
		if( write( ifp->if_fd, (char *) pixelp, todo ) != todo )
			return	-1;
		bytes -= todo;
		pixelp += todo / sizeof(Pixel);
		}
	return	0;
	}

_LOCAL_ int
disk_cmread( ifp, cmap )
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
disk_cmwrite( ifp, cmap )
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
