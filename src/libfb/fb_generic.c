/*                    F B _ G E N E R I C . C
 * BRL-CAD
 *
 * Copyright (c) 1986-2006 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */

/** \defgroup fb Framebuffer
 * \ingroup libfb */
/*@{*/
/** @file fb_generic.c
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
 */
/*@}*/

#ifndef lint
static const char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "common.h"

#include <stdlib.h>
#include <stdio.h>
#ifdef HAVE_STRING_H
#  include <string.h>
#else
#  include <strings.h>
#endif

#include "machine.h"
#include "fb.h"
#include "./fblocal.h"


static int fb_totally_numeric(register char *s);

/*
 * Disk interface enable flag.  Used so the the remote daemon
 * can turn off the disk interface.
 */
int _fb_disk_enable = 1;

/*
 *		f b _ n u l l
 *
 *  Filler for FBIO function slots not used by a particular device
 */
int fb_null(FBIO *ifp)
{
	return	0;
}

/*
 *			F B _ N U L L _ S E T C U R S O R
 *
 *  Used by if_*.c routines that don't have programmable cursor patterns.
 */
int fb_null_setcursor(FBIO *ifp, const unsigned char *bits, int xbits, int ybits, int xorig, int yorig)
{
	return	0;
}

/* First element of list is default device when no name given */
static
FBIO *_if_list[] = {
#ifdef IF_TK
	&tk_interface,
#endif
#ifdef IF_ADAGE
	&adage_interface,
#endif
#ifdef IF_SUN
	&sun_interface,
#endif
#if defined(IF_SGI)
	&sgi_interface,
#endif

#ifdef IF_WGL
	&wgl_interface,
#endif
#ifdef IF_OGL
	&ogl_interface,
#endif
#ifdef IF_RAT
	&rat_interface,
#endif
#ifdef IF_UG
	&ug_interface,
#endif
#ifdef IF_X
	&X24_interface,
	&X_interface,
#endif

	&debug_interface,
/* never get any of the following by default */
#ifdef IF_AB
	&abekas_interface,
#endif
#ifdef IF_TS
	&ts_interface,
#endif
#ifdef IF_PTTY
	&ptty_interface,
#endif
	&stk_interface,
	&memory_interface,
	&null_interface,
	(FBIO *) 0
};

/*
 *			F B _ O P E N
 */
FBIO *
fb_open(char *file, int width, int height)
{
	register FBIO	*ifp;
	int	i;

	if( width < 0 || height < 0 )
		return	FBIO_NULL;

	if( (ifp = (FBIO *) calloc( sizeof(FBIO), 1 )) == FBIO_NULL ) {
		Malloc_Bomb( sizeof(FBIO) );
		return	FBIO_NULL;
	}
	if( file == NULL || *file == '\0' )  {
		/* No name given, check environment variable first.	*/
		if( (file = (char *)getenv( "FB_FILE" )) == NULL || *file == '\0' )  {
			/* None set, use first device as default */
			*ifp = *(_if_list[0]);	/* struct copy */
			file = ifp->if_name;
			goto found_interface;
		}
	}
	/*
	 *  Determine what type of hardware the device name refers to.
	 *
	 *  "file" can in general look like: hostname:/pathname/devname#
	 *  If we have a ':' assume the remote interface
	 *    (We don't check to see if it's us. Good for debugging.)
	 *  else strip out "/path/devname" and try to look it up in the
	 *    device array.  If we don't find it assume it's a file.
	 */
	i = 0;
	while( _if_list[i] != (FBIO *)NULL ) {
		if( strncmp( file, _if_list[i]->if_name,
		    strlen(_if_list[i]->if_name) ) == 0 ) {
		    	/* found it, copy its struct in */
			*ifp = *(_if_list[i]);
			goto found_interface;
		}
		i++;
	}

	/* Not in list, check special interfaces or disk files */
	/* "/dev/" protection! */
	if( strncmp(file, "/dev/", 5) == 0 ) {
		fb_log(	"fb_open: no such device \"%s\".\n", file );
		free( (void *) ifp );
		return	FBIO_NULL;
	}

#ifdef IF_REMOTE
	if( fb_totally_numeric(file) || strchr( file, ':' ) != NULL ) {
		/* We have a remote file name of the form <host>:<file>
		 * or a port number (which assumes localhost) */
		*ifp = remote_interface;
		goto found_interface;
	}
#endif /* IF_REMOTE */
	/* Assume it's a disk file */
	if( _fb_disk_enable ) {
		*ifp = disk_interface;
	} else {
		fb_log(	"fb_open: no such device \"%s\".\n", file );
		free( (void *) ifp );
		return	FBIO_NULL;
	}

found_interface:
	/* Copy over the name it was opened by. */
	if( (ifp->if_name = (char*)malloc( (unsigned) strlen( file ) + 1 ))
	    == (char *) NULL
	    )
	{
		Malloc_Bomb( strlen( file ) + 1 );
		free( (void *) ifp );
		return	FBIO_NULL;
	}
	(void) strcpy( ifp->if_name, file );

	/* Mark OK by filling in magic number */
	ifp->if_magic = FB_MAGIC;

	if( (i=(*ifp->if_open)( ifp, file, width, height )) <= -1 )  {
		fb_log(	"fb_open: can not open device \"%s\", ret=%d.\n",
			file, i );
		ifp->if_magic = 0;		/* sanity */
		free( (void *) ifp->if_name );
		free( (void *) ifp );
		return	FBIO_NULL;
	}
	return	ifp;
}

int
fb_close(FBIO *ifp)
{
	int	i;

	FB_CK_FBIO(ifp);
	_fb_pgflush( ifp );
	if( (i=(*ifp->if_close)( ifp )) <= -1 )  {
		fb_log(	"fb_close: can not close device \"%s\", ret=%d.\n",
			ifp->if_name, i );
		return	-1;
	}
	if( ifp->if_pbase != PIXEL_NULL )
		free( (void *) ifp->if_pbase );
	free( (void *) ifp->if_name );
	free( (void *) ifp );
	return	0;
}

/*
 *  Generic Help.
 *  Print out the list of available frame buffers.
 */
int
fb_genhelp(void)
{
	int	i;

	i = 0;
	while( _if_list[i] != (FBIO *)NULL ) {
		fb_log( "%-12s  %s\n",
			_if_list[i]->if_name,
			_if_list[i]->if_type );
		i++;
	}

	/* Print the ones not in the device list */
#ifdef IF_REMOTE
	fb_log( "%-12s  %s\n",
		remote_interface.if_name,
		remote_interface.if_type );
#endif
	if( _fb_disk_enable ) {
		fb_log( "%-12s  %s\n",
			disk_interface.if_name,
			disk_interface.if_type );
	}

	return	0;
}

/* True if the non-null string s is all digits */
static int
fb_totally_numeric(register char *s)
{
	if( s == (char *)0 || *s == 0 )
		return	0;

	while( *s ) {
		if( *s < '0' || *s > '9' )
			return 0;
		s++;
	}

	return 1;
}

/*
 *			F B _ I S _ L I N E A R _ C M A P
 *
 *  Check for a color map being linear in the upper 8 bits of
 *  R, G, and B.
 *  Returns 1 for linear map, 0 for non-linear map
 *  (ie, non-identity map).
 */
int
fb_is_linear_cmap(register const ColorMap *cmap)
{
	register int i;

	for( i=0; i<256; i++ )  {
		if( cmap->cm_red[i]>>8 != i )  return(0);
		if( cmap->cm_green[i]>>8 != i )  return(0);
		if( cmap->cm_blue[i]>>8 != i )  return(0);
	}
	return(1);
}

/*
 *			F B _ M A K E _ L I N E A R _ C M A P
 */
void
fb_make_linear_cmap(register ColorMap *cmap)
{
	register int i;

	for( i=0; i<256; i++ )  {
		cmap->cm_red[i] = i<<8;
		cmap->cm_green[i] = i<<8;
		cmap->cm_blue[i] = i<<8;
	}
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
