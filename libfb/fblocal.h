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
#ifndef INCL_FBLOCAL
#define INCL_FBLOCAL

#define Malloc_Bomb( _bytes_ ) \
		fb_log( "\"%s\"(%d) : allocation of %d bytes failed.\n", \
				__FILE__, __LINE__, _bytes_ )

#define MAX_BYTES_DMA	(64*1024L)		/* Max # of bytes/dma.	*/
#define MAX_PIXELS_DMA	(MAX_BYTES_DMA/sizeof(Pixel))

/* System calls and run-time C library functions.			*/
extern char	*malloc();
extern char	*getenv();
extern long	lseek();
extern unsigned	sleep();
extern void	free();

#ifndef _LOCAL_
/* Useful for ADB when debugging, set to nothing */
#define _LOCAL_ static
#endif

#ifdef BSD
#define strchr	index
#define	strrchr	rindex
#endif

#endif INCL_FBLOCAL
