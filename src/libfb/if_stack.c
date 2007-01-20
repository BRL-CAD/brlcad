/*                      I F _ S T A C K . C
 * BRL-CAD
 *
 * Copyright (c) 1986-2007 United States Government as represented by
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

/** \addtogroup if */
/*@{*/
/** @file if_stack.c
 *  Allows multiple frame buffers to be ganged together.
 *
 *  Authors -
 *	Phillip Dykstra
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
#include <string.h>
#else
#include <strings.h>
#endif
#include "machine.h"
#include "fb.h"
#include "./fblocal.h"

HIDDEN int	stk_open(FBIO *ifp, char *file, int width, int height),
		stk_close(FBIO *ifp),
		stk_clear(FBIO *ifp, unsigned char *pp),
		stk_read(FBIO *ifp, int x, int y, unsigned char *pixelp, int count),
		stk_write(FBIO *ifp, int x, int y, const unsigned char *pixelp, int count),
		stk_rmap(FBIO *ifp, ColorMap *cmp),
		stk_wmap(FBIO *ifp, const ColorMap *cmp),
		stk_view(FBIO *ifp, int xcenter, int ycenter, int xzoom, int yzoom),
		stk_getview(FBIO *ifp, int *xcenter, int *ycenter, int *xzoom, int *yzoom),
		stk_setcursor(FBIO *ifp, const unsigned char *bits, int xbits, int ybits, int xorig, int yorig),
		stk_cursor(FBIO *ifp, int mode, int x, int y),
		stk_getcursor(FBIO *ifp, int *mode, int *x, int *y),
		stk_readrect(FBIO *ifp, int xmin, int ymin, int width, int height, unsigned char *pp),
		stk_writerect(FBIO *ifp, int xmin, int ymin, int width, int height, const unsigned char *pp),
		stk_bwreadrect(FBIO *ifp, int xmin, int ymin, int width, int height, unsigned char *pp),
		stk_bwwriterect(FBIO *ifp, int xmin, int ymin, int width, int height, const unsigned char *pp),
		stk_poll(FBIO *ifp),
		stk_flush(FBIO *ifp),
		stk_free(FBIO *ifp),
		stk_help(FBIO *ifp);



/* This is the ONLY thing that we normally "export" */
FBIO stk_interface =  {
	0,
	stk_open,		/* device_open		*/
	stk_close,		/* device_close		*/
	stk_clear,		/* device_clear		*/
	stk_read,		/* buffer_read		*/
	stk_write,		/* buffer_write		*/
	stk_rmap,		/* colormap_read	*/
	stk_wmap,		/* colormap_write	*/
	stk_view,		/* set view		*/
	stk_getview,		/* get view		*/
	stk_setcursor,		/* define cursor	*/
	stk_cursor,		/* set cursor		*/
	stk_getcursor,		/* get cursor		*/
	stk_readrect,		/* read rectangle	*/
	stk_writerect,		/* write rectangle	*/
	stk_bwreadrect,		/* read bw rectangle	*/
	stk_bwwriterect,	/* write bw rectangle	*/
	stk_poll,		/* handle events	*/
	stk_flush,		/* flush output		*/
	stk_free,		/* free resources	*/
	stk_help,		/* help function	*/
	"Multiple Device Stacker", /* device description */
	32*1024,		/* max width		*/
	32*1024,		/* max height		*/
	"/dev/stack",		/* short device name	*/
	4,			/* default/current width  */
	4,			/* default/current height */
	-1,			/* select fd		*/
	-1,			/* file descriptor	*/
	1, 1,			/* zoom			*/
	2, 2,			/* window center	*/
	0, 0, 0,		/* cursor		*/
	PIXEL_NULL,		/* page_base		*/
	PIXEL_NULL,		/* page_curp		*/
	PIXEL_NULL,		/* page_endp		*/
	-1,			/* page_no		*/
	0,			/* page_dirty		*/
	0L,			/* page_curpos		*/
	0L,			/* page_pixels		*/
	0			/* debug		*/
};

/* List of interface struct pointers, one per dev */
#define	MAXIF	32
struct	stkinfo {
	FBIO	*if_list[MAXIF];
};
#define	SI(ptr) ((struct stkinfo *)((ptr)->u1.p))
#define	SIL(ptr) ((ptr)->u1.p)		/* left hand side version */

HIDDEN int
stk_open(FBIO *ifp, char *file, int width, int height)
{
	int	i;
	char	*cp;
	char	devbuf[80];

	FB_CK_FBIO(ifp);

	/* Check for /dev/stack */
	if( strncmp(file,ifp->if_name,strlen("/dev/stack")) != 0 ) {
		fb_log( "stack_dopen: Bad device %s\n", file );
		return(-1);
	}

	if( (SIL(ifp) = (char *)calloc( 1, sizeof(struct stkinfo) )) == NULL )  {
		fb_log("stack_dopen:  stkinfo malloc failed\n");
		return(-1);
	}

	cp = &file[strlen("/dev/stack")];
	while( *cp != '\0' && *cp != ' ' && *cp != '\t' )
		cp++;	/* skip suffix */

	/* special check for a possibly user confusing case */
	if( *cp == '\0' ) {
		fb_log("stack_dopen: No devices specified\n");
		fb_log("Usage: /dev/stack device_one; device_two; [etc]\n");
		return(-1);
	}

	ifp->if_width = ifp->if_max_width;
	ifp->if_height = ifp->if_max_height;
	i = 0;
	while( i < MAXIF && *cp != '\0' ) {
		register char	*dp;
		register FBIO	*fbp;

		while( *cp != '\0' && (*cp == ' ' || *cp == '\t' || *cp == ';') )
			cp++;	/* skip blanks and separators */
		if( *cp == '\0' )
			break;
		dp = devbuf;
		while( *cp != '\0' && *cp != ';' )
			*dp++ = *cp++;
		*dp = '\0';
		if( (fbp = fb_open(devbuf, width, height)) != FBIO_NULL )  {
			FB_CK_FBIO(fbp);
			/* Track the minimum of all the actual sizes */
			if( fbp->if_width < ifp->if_width )
				ifp->if_width = fbp->if_width;
			if( fbp->if_height < ifp->if_height )
				ifp->if_height = fbp->if_height;
			if( fbp->if_max_width < ifp->if_max_width )
				ifp->if_max_width = fbp->if_max_width;
			if( fbp->if_max_height < ifp->if_max_height )
				ifp->if_max_height = fbp->if_max_height;
			SI(ifp)->if_list[i++] = fbp;
		}
	}
	if( i > 0 )
		return(0);
	else
		return(-1);
}

HIDDEN int
stk_close(FBIO *ifp)
{
	register FBIO **ip = SI(ifp)->if_list;

	FB_CK_FBIO(ifp);
	while( *ip != (FBIO *)NULL ) {
		FB_CK_FBIO( (*ip) );
		fb_close( (*ip) );
		ip++;
	}

	return(0);
}

HIDDEN int
stk_clear(FBIO *ifp, unsigned char *pp)
{
	register FBIO **ip = SI(ifp)->if_list;

	while( *ip != (FBIO *)NULL ) {
		fb_clear( (*ip), pp );
		ip++;
	}

	return(0);
}

HIDDEN int
stk_read(FBIO *ifp, int x, int y, unsigned char *pixelp, int count)
{
	register FBIO **ip = SI(ifp)->if_list;

	if( *ip != (FBIO *)NULL ) {
		fb_read( (*ip), x, y, pixelp, count );
	}

	return(count);
}

HIDDEN int
stk_write(FBIO *ifp, int x, int y, const unsigned char *pixelp, int count)
{
	register FBIO **ip = SI(ifp)->if_list;

	while( *ip != (FBIO *)NULL ) {
		fb_write( (*ip), x, y, pixelp, count );
		ip++;
	}

	return(count);
}

/*
 *			S T K _ R E A D R E C T
 *
 *  Read only from the first source on the stack.
 */
HIDDEN int
stk_readrect(FBIO *ifp, int xmin, int ymin, int width, int height, unsigned char *pp)
{
	register FBIO **ip = SI(ifp)->if_list;

	if( *ip != (FBIO *)NULL ) {
		(void)fb_readrect( (*ip), xmin, ymin, width, height, pp );
	}

	return( width*height );
}

/*
 *			S T K _ W R I T E R E C T
 *
 *  Write to all destinations on the stack
 */
HIDDEN int
stk_writerect(FBIO *ifp, int xmin, int ymin, int width, int height, const unsigned char *pp)
{
	register FBIO **ip = SI(ifp)->if_list;

	while( *ip != (FBIO *)NULL ) {
		(void)fb_writerect( (*ip), xmin, ymin, width, height, pp );
		ip++;
	}

	return( width*height );
}

/*
 *			S T K _ B W R E A D R E C T
 *
 *  Read only from the first source on the stack.
 */
HIDDEN int
stk_bwreadrect(FBIO *ifp, int xmin, int ymin, int width, int height, unsigned char *pp)
{
	register FBIO **ip = SI(ifp)->if_list;

	if( *ip != (FBIO *)NULL ) {
		(void)fb_bwreadrect( (*ip), xmin, ymin, width, height, pp );
	}

	return width*height;
}

/*
 *			S T K _ B W W R I T E R E C T
 *
 *  Write to all destinations on the stack
 */
HIDDEN int
stk_bwwriterect(FBIO *ifp, int xmin, int ymin, int width, int height, const unsigned char *pp)
{
	register FBIO **ip = SI(ifp)->if_list;

	while( *ip != (FBIO *)NULL ) {
		(void)fb_bwwriterect( (*ip), xmin, ymin, width, height, pp );
		ip++;
	}

	return width*height;
}

HIDDEN int
stk_rmap(FBIO *ifp, ColorMap *cmp)
{
	register FBIO **ip = SI(ifp)->if_list;

	if( *ip != (FBIO *)NULL ) {
		fb_rmap( (*ip), cmp );
	}

	return(0);
}

HIDDEN int
stk_wmap(FBIO *ifp, const ColorMap *cmp)
{
	register FBIO **ip = SI(ifp)->if_list;

	while( *ip != (FBIO *)NULL ) {
		fb_wmap( (*ip), cmp );
		ip++;
	}

	return(0);
}

HIDDEN int
stk_view(FBIO *ifp, int xcenter, int ycenter, int xzoom, int yzoom)
{
	register FBIO **ip = SI(ifp)->if_list;

	while( *ip != (FBIO *)NULL ) {
		fb_view( (*ip), xcenter, ycenter, xzoom, yzoom );
		ip++;
	}

	return(0);
}

HIDDEN int
stk_getview(FBIO *ifp, int *xcenter, int *ycenter, int *xzoom, int *yzoom)
{
	register FBIO **ip = SI(ifp)->if_list;

	if( *ip != (FBIO *)NULL ) {
		fb_getview( (*ip), xcenter, ycenter, xzoom, yzoom );
	}

	return(0);
}

HIDDEN int
stk_setcursor(FBIO *ifp, const unsigned char *bits, int xbits, int ybits, int xorig, int yorig)
{
	register FBIO **ip = SI(ifp)->if_list;

	while( *ip != (FBIO *)NULL ) {
		fb_setcursor( (*ip), bits, xbits, ybits, xorig, yorig );
		ip++;
	}

	return(0);
}

HIDDEN int
stk_cursor(FBIO *ifp, int mode, int x, int y)
{
	register FBIO **ip = SI(ifp)->if_list;

	while( *ip != (FBIO *)NULL ) {
		fb_cursor( (*ip), mode, x, y );
		ip++;
	}

	return(0);
}

HIDDEN int
stk_getcursor(FBIO *ifp, int *mode, int *x, int *y)
{
	register FBIO **ip = SI(ifp)->if_list;

	if( *ip != (FBIO *)NULL ) {
		fb_getcursor( (*ip), mode, x, y );
	}

	return(0);
}

HIDDEN int
stk_poll(FBIO *ifp)
{
	register FBIO **ip = SI(ifp)->if_list;

	while( *ip != (FBIO *)NULL ) {
		fb_poll( (*ip) );
		ip++;
	}

	return(0);
}

HIDDEN int
stk_flush(FBIO *ifp)
{
	register FBIO **ip = SI(ifp)->if_list;

	while( *ip != (FBIO *)NULL ) {
		fb_flush( (*ip) );
		ip++;
	}

	return(0);
}

HIDDEN int
stk_free(FBIO *ifp)
{
	register FBIO **ip = SI(ifp)->if_list;

	while( *ip != (FBIO *)NULL ) {
		fb_free( (*ip) );
		ip++;
	}

	return(0);
}

HIDDEN int
stk_help(FBIO *ifp)
{
	register FBIO **ip = SI(ifp)->if_list;
	int	i;

	fb_log("Device: /dev/stack\n");
	fb_log("Usage: /dev/stack device_one; device_two; [etc]\n");

	i = 0;
	while( *ip != (FBIO *)NULL ) {
		fb_log("=== Current stack device #%d ===\n", i++);
		fb_help( (*ip) );
		ip++;
	}

	return(0);
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
