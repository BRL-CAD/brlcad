/*
 *			I F _ D I S K . C
 *
 *  Author -
 *	Gary S. Moss
 *
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 *  
 *  Copyright Notice -
 *	This software is Copyright (C) 1986-2004 by the United States Army.
 *	All rights reserved.
 */
#ifndef lint
static const char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "conf.h"

#include <stdio.h>
#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include <sys/types.h>
#ifndef WIN32
#include <unistd.h>
#endif
       
#ifdef HAVE_SYS_FILE_H
#include <sys/file.h>
#else
#include <fcntl.h>
#endif

#include "machine.h"
#include "externs.h"		/* For malloc */
#include "fb.h"
#include "./fblocal.h"

#define FILE_CMAP_ADDR	((long) ifp->if_width*ifp->if_height\
			*sizeof(RGBpixel))

/* Ensure integer number of pixels per DMA */
#define	DISK_DMA_BYTES	(16*1024/sizeof(RGBpixel)*sizeof(RGBpixel))
#define	DISK_DMA_PIXELS	(DISK_DMA_BYTES/sizeof(RGBpixel))

_LOCAL_ int	dsk_open(),
		dsk_close(),
		dsk_clear(),
		dsk_read(),
		dsk_write(),
		dsk_rmap(),
		dsk_wmap(),
		dsk_free(),
		dsk_help();

FBIO disk_interface = {
	0,
	dsk_open,
	dsk_close,
	dsk_clear,
	dsk_read,
	dsk_write,
	dsk_rmap,
	dsk_wmap,
	fb_sim_view,		/* set view */
	fb_sim_getview,		/* get view */
	fb_null_setcursor,		/* define cursor */
	fb_sim_cursor,		/* set cursor */
	fb_sim_getcursor,	/* get cursor */
	fb_sim_readrect,
	fb_sim_writerect,
	fb_sim_bwreadrect,
	fb_sim_bwwriterect,
	fb_null,		/* poll */
	fb_null,		/* flush */
	dsk_free,
	dsk_help,
	"Disk File Interface",
	16*1024,		/* the sky's really the limit here */
	16*1024,
	"filename",		/* not in list so name is safe */
	512,
	512,
	-1,			/* select fd */
	-1,
	1, 1,			/* zoom */
	256, 256,		/* window center */
	0, 0, 0,		/* cursor */
	PIXEL_NULL,
	PIXEL_NULL,
	PIXEL_NULL,
	-1,
	0,
	0L,
	0L,
	0
};

#define if_seekpos	u5.l	/* stored seek position */

_LOCAL_ int	disk_color_clear();

_LOCAL_ int
dsk_open( ifp, file, width, height )
FBIO	*ifp;
char	*file;
int	width, height;
{
	static char zero = 0;

	FB_CK_FBIO(ifp);

	/* check for default size */
	if( width == 0 )
		width = ifp->if_width;
	if( height == 0 )
		height = ifp->if_height;

	if( strcmp( file, "-" ) == 0 )  {
		/*
		 *  It is the applications job to write ascending scanlines.
		 *  If it does not, then this can be stacked with /dev/mem,
		 *  i.e.	/dev/mem -
		 */
		ifp->if_fd = 1;		/* fileno(stdout) */
		ifp->if_width = width;
		ifp->if_height = height;
		ifp->if_seekpos = 0L;
		return 0;
	}

	if( (ifp->if_fd = open( file, O_RDWR, 0 )) == -1
	  && (ifp->if_fd = open( file, O_RDONLY, 0 )) == -1 ) {
		if( (ifp->if_fd = open( file, O_RDWR|O_CREAT, 0664 )) > 0 ) {
			/* New file, write byte at end */
			if( lseek( ifp->if_fd, (off_t)(height*width*sizeof(RGBpixel)-1), 0 ) == -1 ) {
				fb_log( "disk_device_open : can not seek to end of new file.\n" );
				return	-1;
			}
			if( write( ifp->if_fd, &zero, 1 ) < 0 ) {
				fb_log( "disk_device_open : initial write failed.\n" );
				return	-1;
			}
		} else
			return	-1;
	}
	ifp->if_width = width;
	ifp->if_height = height;
	if( lseek( ifp->if_fd, (off_t)0L, 0 ) == -1L ) {
		fb_log( "disk_device_open : can not seek to beginning.\n" );
		return	-1;
	}
	ifp->if_seekpos = 0L;
	return	0;
}

_LOCAL_ int
dsk_close( ifp )
FBIO	*ifp;
{
	return	close( ifp->if_fd );
}

_LOCAL_ int
dsk_free( ifp )
FBIO	*ifp;
{
	close( ifp->if_fd );
	return	unlink( ifp->if_name );
}

_LOCAL_ int
dsk_clear( ifp, bgpp )
FBIO	*ifp;
unsigned char	*bgpp;
{
	static RGBpixel	black = { 0, 0, 0 };

	if( bgpp == (unsigned char *)NULL )
		return disk_color_clear( ifp, black );

	return	disk_color_clear( ifp, bgpp );
}

_LOCAL_ int
dsk_read( ifp, x, y, pixelp, count )
FBIO	*ifp;
int	x,  y;
unsigned char	*pixelp;
int	count;
{
	register long bytes = count * (long) sizeof(RGBpixel);
	register long todo;
	long		got;
	long		dest;
	long		bytes_read = 0;
	int		fd = ifp->if_fd;

	/* Reads on stdout make no sense.  Take reads from stdin. */
	if( fd == 1 )  fd = 0;

	dest = (((long) y * (long) ifp->if_width) + (long) x)
	     * (long) sizeof(RGBpixel);
	if( ifp->if_seekpos != dest && lseek(fd, (off_t)dest, 0) == -1L ) {
		fb_log( "disk_buffer_read : seek to %ld failed.\n", dest );
		return	-1;
	}
	ifp->if_seekpos = dest;
	while( bytes > 0 ) {
		todo = bytes;
		if( (got = read( fd, (char *) pixelp, todo )) != todo )  {
			if( got == 0 )  {
				if( bytes_read <= 0 )
					return -1;	/* error */
				/* early EOF -- indicate what we got */
				return bytes_read/sizeof(RGBpixel);
			}
			if( fd != 0 )  {
				/* This happens all the time reading from pipes */
				fb_log("disk_buffer_read(fd=%d): y=%d read of %d got %d bytes\n",
					fd, y, todo, got);
			}
		}
		bytes -= got;
		pixelp += got;
		ifp->if_seekpos += got;
		bytes_read += got;
	}
	return	bytes_read/sizeof(RGBpixel);
}

_LOCAL_ int
dsk_write( ifp, x, y, pixelp, count )
FBIO	*ifp;
int	x, y;
const unsigned char	*pixelp;
long	count;
{
	register long	bytes = count * (long) sizeof(RGBpixel);
	register long	todo;
	long		dest;

	dest = ((long) y * (long) ifp->if_width + (long) x)
	     * (long) sizeof(RGBpixel);
	if( dest != ifp->if_seekpos )  {
		if( lseek(ifp->if_fd, (off_t)dest, 0) == -1L ) {
			fb_log( "disk_buffer_write : seek to %ld failed.\n", dest );
			return	-1;
		}
		ifp->if_seekpos = dest;
	}
	while( bytes > 0 ) {
		todo = bytes;
		if( write( ifp->if_fd, (char *) pixelp, todo ) != todo )  {
			fb_log( "disk_buffer_write: write failed\n" );
			return	-1;
		}
		bytes -= todo;
		pixelp += todo / sizeof(RGBpixel);
		ifp->if_seekpos += todo;
	}
	return	count;
}

_LOCAL_ int
dsk_rmap( ifp, cmap )
FBIO	*ifp;
ColorMap	*cmap;
{
	int		fd = ifp->if_fd;

	/* Reads on stdout make no sense.  Take reads from stdin. */
	if( fd == 1 )  fd = 0;

	if( ifp->if_seekpos != FILE_CMAP_ADDR &&
	    lseek( fd, (off_t)FILE_CMAP_ADDR, 0 ) == -1 ) {
		fb_log(	"disk_colormap_read : seek to %ld failed.\n",
				FILE_CMAP_ADDR );
	   	return	-1;
	}
	if( read( fd, (char *) cmap, sizeof(ColorMap) )
		!= sizeof(ColorMap) ) {
		/* Not necessarily an error.  It is not required
		 * that a color map be saved and the standard 
		 * map is not generally saved.
		 */
		return	-1;
	}
	return	0;
}

_LOCAL_ int
dsk_wmap( ifp, cmap )
FBIO	*ifp;
const ColorMap	*cmap;
{
	if( cmap == (ColorMap *) NULL )
		/* Do not write default map to file. */
		return	0;
	if( fb_is_linear_cmap( cmap ) )
		return  0;
	if( lseek( ifp->if_fd, (off_t)FILE_CMAP_ADDR, 0 ) == -1 ) {
		fb_log(	"disk_colormap_write : seek to %ld failed.\n",
				FILE_CMAP_ADDR );
		return	-1;
	}
	if( write( ifp->if_fd, (char *) cmap, sizeof(ColorMap) )
	    != sizeof(ColorMap) ) {
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
register unsigned char	*bpp;
{
	static unsigned char	*pix_buf = NULL;
	register unsigned char *pix_to;
	register long	i;
	int	fd, pixelstodo;

	if( pix_buf == NULL )
		if( (pix_buf = (unsigned char *) malloc(DISK_DMA_BYTES)) == PIXEL_NULL ) {
			Malloc_Bomb(DISK_DMA_BYTES);
			return	-1;
		}

	/* Fill buffer with background color. */
	for( i = DISK_DMA_PIXELS, pix_to = pix_buf; i > 0; i-- ) {
		COPYRGB( pix_to, bpp );
		pix_to += sizeof(RGBpixel);
	}

	/* Set start of framebuffer */
	fd = ifp->if_fd;
	if( ifp->if_seekpos != 0L && lseek( fd, (off_t)0L, 0 ) == -1 ) {
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
		ifp->if_seekpos += i * sizeof(RGBpixel);
	}

	return	0;
}

_LOCAL_ int
dsk_help( ifp )
FBIO	*ifp;
{
	fb_log( "Description: %s\n", disk_interface.if_type );
	fb_log( "Device: %s\n", ifp->if_name );
	fb_log( "Max width/height: %d %d\n",
		disk_interface.if_max_width,
		disk_interface.if_max_height );
	fb_log( "Default width/height: %d %d\n",
		disk_interface.if_width,
		disk_interface.if_height );
	if( ifp->if_fd == 1 )  {
		fb_log("File \"-\" reads from stdin, writes to stdout\n");
	} else {
		fb_log( "Note: you may have just created a disk file\n" );
		fb_log( "called \"%s\" by running this.\n", ifp->if_name );
	}

	return(0);
}
