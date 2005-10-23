/*                        S E R V E R . C
 * BRL-CAD
 *
 * Copyright (C) 1998-2005 United States Government as represented by
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

/** \addtogroup libfb */
/*@{*/

/** @file server.c
 *  Remote libfb server event handlers (originally rfbd).
 *
 *  Authors -
 *	Phillip Dykstra
 *	Michael John Muuss
 *
 *  Source -
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5068  USA
 */
/*@}*/


#ifndef lint
static const char RCSid[] = "@(#)$Header$ (ARL)";
#endif

#include "common.h"

#include <stdlib.h>
#include <stdio.h>
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#if defined (HAVE_SYS_SELECT_H)
#  include <sys/select.h>
#else
#  if defined(HAVE_SYS_TYPES_H)
#    include <sys/types.h>
#  endif
#  if defined(HAVE_SYS_TIME_H)
#    include <sys/time.h>
#  endif
#  if defined(HAVE_UNISTD_H)
#    include <unistd.h>
#  else
#    if defined(HAVE_SYS_UNISTD_H)
#      include <sys/unistd.h>
#    endif
#  endif
#endif
#include "machine.h"
#include "fb.h"
#include "pkg.h"

#include "./pkgtypes.h"

#define NET_LONG_LEN	4	/* # bytes to network long */

/*
 * Package Handlers defined in this file.
 */
void	fb_server_got_unknown(struct pkg_conn *pcp, char *buf);	/* foobar message handler */
void	fb_server_fb_open(struct pkg_conn *pcp, char *buf), fb_server_fb_close(struct pkg_conn *pcp, char *buf), fb_server_fb_clear(struct pkg_conn *pcp, char *buf), fb_server_fb_read(struct pkg_conn *pcp, char *buf), fb_server_fb_write(struct pkg_conn *pcp,
 char *buf);
void	fb_server_fb_cursor(struct pkg_conn *pcp, char *buf), fb_server_fb_getcursor(struct pkg_conn *pcp, char *buf);
void	fb_server_fb_rmap(struct pkg_conn *pcp, char *buf), fb_server_fb_wmap(struct pkg_conn *pcp, char *buf);
void	fb_server_fb_help(struct pkg_conn *pcp, char *buf);
void	fb_server_fb_readrect(struct pkg_conn *pcp, char *buf), fb_server_fb_writerect(struct pkg_conn *pcp, char *buf);
void	fb_server_fb_bwreadrect(struct pkg_conn *pcp, char *buf), fb_server_fb_bwwriterect(struct pkg_conn *pcp, char *buf);
void	fb_server_fb_poll(struct pkg_conn *pcp, char *buf), fb_server_fb_flush(struct pkg_conn *pcp, char *buf), fb_server_fb_free(struct pkg_conn *pcp, char *buf);
void	fb_server_fb_view(struct pkg_conn *pcp, char *buf), fb_server_fb_getview(struct pkg_conn *pcp, char *buf);
void	fb_server_fb_setcursor(struct pkg_conn *pcp, char *buf);
/* Old Routines */
void	fb_server_fb_scursor(struct pkg_conn *pcp, char *buf), fb_server_fb_window(struct pkg_conn *pcp, char *buf), fb_server_fb_zoom(struct pkg_conn *pcp, char *buf);

/*
 *  These are the only symbols intended for export to LIBFB users.
 */

FBIO	*fb_server_fbp = FBIO_NULL;
fd_set	*fb_server_select_list;
int	*fb_server_max_fd = (int *)NULL;
int	fb_server_got_fb_free = 0;	/* !0 => we have received an fb_free */
int	fb_server_refuse_fb_free = 0;	/* !0 => don't accept fb_free() */
int	fb_server_retain_on_close = 0;	/* !0 => we are holding a reusable FB open */

const struct pkg_switch fb_server_pkg_switch[] = {
  { MSG_FBOPEN,			fb_server_fb_open,	"Open Framebuffer" },
  { MSG_FBCLOSE,		fb_server_fb_close,	"Close Framebuffer" },
  { MSG_FBCLEAR,		fb_server_fb_clear,	"Clear Framebuffer" },
  { MSG_FBREAD,			fb_server_fb_read,	"Read Pixels" },
  { MSG_FBWRITE,		fb_server_fb_write,	"Write Pixels" },
  { MSG_FBWRITE + MSG_NORETURN,	fb_server_fb_write,	"Asynch write" },
  { MSG_FBCURSOR,		fb_server_fb_cursor,	"Cursor" },
  { MSG_FBGETCURSOR,		fb_server_fb_getcursor,	"Get Cursor" },	   /*NEW*/
  { MSG_FBSCURSOR,		fb_server_fb_scursor,	"Screen Cursor" }, /*OLD*/
  { MSG_FBWINDOW,		fb_server_fb_window,	"Window" },	   /*OLD*/
  { MSG_FBZOOM,			fb_server_fb_zoom,	"Zoom" },	   /*OLD*/
  { MSG_FBVIEW,			fb_server_fb_view,	"View" },	   /*NEW*/
  { MSG_FBGETVIEW,		fb_server_fb_getview,	"Get View" },	   /*NEW*/
  { MSG_FBRMAP,			fb_server_fb_rmap,	"R Map" },
  { MSG_FBWMAP,			fb_server_fb_wmap,	"W Map" },
  { MSG_FBHELP,			fb_server_fb_help,	"Help Request" },
  { MSG_ERROR,			fb_server_got_unknown,	"Error Message" },
  { MSG_CLOSE,			fb_server_got_unknown,	"Close Connection" },
  { MSG_FBREADRECT, 		fb_server_fb_readrect,	"Read Rectangle" },
  { MSG_FBWRITERECT,		fb_server_fb_writerect,	"Write Rectangle" },
  { MSG_FBWRITERECT+MSG_NORETURN, fb_server_fb_writerect,"Write Rectangle" },
  { MSG_FBBWREADRECT, 		fb_server_fb_bwreadrect,"Read BW Rectangle" },
  { MSG_FBBWWRITERECT,		fb_server_fb_bwwriterect,"Write BW Rectangle" },
  { MSG_FBBWWRITERECT+MSG_NORETURN, fb_server_fb_bwwriterect,"Write BW Rectangle" },
  { MSG_FBFLUSH,		fb_server_fb_flush,	"Flush Output" },
  { MSG_FBFLUSH + MSG_NORETURN, fb_server_fb_flush,	 "Flush Output" },
  { MSG_FBFREE,			fb_server_fb_free,	"Free Resources" },
  { MSG_FBPOLL,			fb_server_fb_poll,	"Handle Events" },
  { MSG_FBSETCURSOR,		fb_server_fb_setcursor,	"Set Cursor Shape" },
  { MSG_FBSETCURSOR + MSG_NORETURN, fb_server_fb_setcursor, "Set Cursor Shape" },
  { 0,				NULL,			NULL }
};


/******** Here's where the hooks lead *********/

/*
 *			F B _ S E R V E R _ G O T _ U N K N O W N
 *
 *  This is where we go for message types we don't understand.
 *
 *  The fb_log() might go to the ordinary LIBFB routine, or
 *  the application might have provided a replacement routine
 *  which might send the message back across the wire.
 */
void
fb_server_got_unknown(struct pkg_conn *pcp, char *buf)
{
	fb_log( "fb_server_got_unknown: message type %d not part of remote LIBFB protocol, ignored.\n",
		pcp->pkc_type );
	(void)free(buf);
}

/*
 *			F B _ S E R V E R _ F B _ O P E N
 *
 *  There can only be one framebuffer (fbp) open at any one time
 *  (although that can be a stacker driving many actual windows),
 *  but framebuffers can be opened and closed and opened again in the
 *  lifetime of one server process if fb_server_retain_on_close is set.
 */
void
fb_server_fb_open(struct pkg_conn *pcp, char *buf)
{
	int	height, width;
	char	rbuf[5*NET_LONG_LEN+1];
	int	want;

	width = pkg_glong( &buf[0*NET_LONG_LEN] );
	height = pkg_glong( &buf[1*NET_LONG_LEN] );

	if( fb_server_fbp == FBIO_NULL ) {
		/* Attempt to open new framebuffer */
		if( strlen(&buf[8]) == 0 )
			fb_server_fbp = fb_open( NULL, width, height );
		else
			fb_server_fbp = fb_open( &buf[8], width, height );
	}  else  {
		/* Use existing framebuffer */
	}

	if( fb_server_fbp == FBIO_NULL )  {
		(void)pkg_plong( &rbuf[0*NET_LONG_LEN], -1 );	/* ret */
		(void)pkg_plong( &rbuf[1*NET_LONG_LEN], 0 );
		(void)pkg_plong( &rbuf[2*NET_LONG_LEN], 0 );
		(void)pkg_plong( &rbuf[3*NET_LONG_LEN], 0 );
		(void)pkg_plong( &rbuf[4*NET_LONG_LEN], 0 );
	} else {
		(void)pkg_plong( &rbuf[0*NET_LONG_LEN], 0 );	/* ret */
		(void)pkg_plong( &rbuf[1*NET_LONG_LEN], fb_server_fbp->if_max_width );
		(void)pkg_plong( &rbuf[2*NET_LONG_LEN], fb_server_fbp->if_max_height );
		(void)pkg_plong( &rbuf[3*NET_LONG_LEN], fb_server_fbp->if_width );
		(void)pkg_plong( &rbuf[4*NET_LONG_LEN], fb_server_fbp->if_height );
		if(fb_server_fbp->if_selfd > 0 && fb_server_select_list )  {
			FD_SET(fb_server_fbp->if_selfd, fb_server_select_list);
			if( fb_server_max_fd != NULL &&
			    fb_server_fbp->if_selfd > *fb_server_max_fd )
				*fb_server_max_fd = fb_server_fbp->if_selfd;
		}
	}

	want = 5*NET_LONG_LEN;
	if( pkg_send( MSG_RETURN, rbuf, want, pcp ) != want )
		fprintf(stderr, "pkg_send fb_open reply\n");
	if( buf ) (void)free(buf);
}


/*
 *			F B _ S E R V E R _ F B _ C L O S E
 *
 */
void
fb_server_fb_close(struct pkg_conn *pcp, char *buf)
{
	char	rbuf[NET_LONG_LEN+1];

	if( fb_server_retain_on_close ) {
		/*
		 * We are playing FB server so we don't really close the
		 * frame buffer.  We should flush output however.
		 */
		(void)fb_flush( fb_server_fbp );
		(void)pkg_plong( &rbuf[0], 0 );		/* return success */
	} else {
		if(fb_server_fbp->if_selfd > 0 && fb_server_select_list)  {
			FD_CLR(fb_server_fbp->if_selfd, fb_server_select_list);
		}
		(void)pkg_plong( &rbuf[0], fb_close( fb_server_fbp ) );
		fb_server_fbp = FBIO_NULL;
	}
	/* Don't check for errors, SGI linger mode or other events
	 * may have already closed down all the file descriptors.
	 * If communication has broken, other end will know we are gone.
	 */
	(void)pkg_send( MSG_RETURN, rbuf, NET_LONG_LEN, pcp );
	if( buf ) (void)free(buf);
}


/*
 *			F B _ S E R V E R _ F B _ F R E E
 *
 *  The fb_free() call is more potent than fb_close(), which it
 *  must preceed; it causes shared memory to be returned & stuff like that.
 *  The current FBSERV program exits after getting one.
 *
 *  We set fb_server_got_fb_free as an indicator to calling application.
 *  It doesn't prevent another framebuffer from being opened,
 *  although application might want to reset the flag after noticing,
 *  if it isn't going to exit at that point.
 */
void
fb_server_fb_free(struct pkg_conn *pcp, char *buf)
{
	char	rbuf[NET_LONG_LEN+1];

	if( fb_server_refuse_fb_free )  {
		(void)pkg_plong( &rbuf[0], -1 );
	} else {
		(void)pkg_plong( &rbuf[0], fb_free( fb_server_fbp ) );
		fb_server_fbp = FBIO_NULL;
	}

	if( pkg_send( MSG_RETURN, rbuf, NET_LONG_LEN, pcp ) != NET_LONG_LEN )
		fprintf(stderr, "pkg_send fb_free reply\n");
	if( buf ) (void)free(buf);

	fb_server_got_fb_free = 1;
}


/*
 *			F B _ S E R V E R _ F B _ C L E A R
 *
 */
void
fb_server_fb_clear(struct pkg_conn *pcp, char *buf)
{
	RGBpixel bg;
	char	rbuf[NET_LONG_LEN+1];

	bg[RED] = buf[0];
	bg[GRN] = buf[1];
	bg[BLU] = buf[2];

	(void)pkg_plong( rbuf, fb_clear( fb_server_fbp, bg ) );
	pkg_send( MSG_RETURN, rbuf, NET_LONG_LEN, pcp );
	if( buf ) (void)free(buf);
}


/*
 *			F B _ S E R V E R _ F B _ R E A D
 *
 */
void
fb_server_fb_read(struct pkg_conn *pcp, char *buf)
{
	int	x, y, num;
	int	ret;
	static unsigned char	*scanbuf = NULL;
	static int	buflen = 0;

	x = pkg_glong( &buf[0*NET_LONG_LEN] );
	y = pkg_glong( &buf[1*NET_LONG_LEN] );
	num = pkg_glong( &buf[2*NET_LONG_LEN] );

	if( num*sizeof(RGBpixel) > buflen ) {
		if( scanbuf != NULL )
			free( (char *)scanbuf );
		buflen = num*sizeof(RGBpixel);
		if( buflen < 1024*sizeof(RGBpixel) )
			buflen = 1024*sizeof(RGBpixel);
		if( (scanbuf = (unsigned char *)malloc( buflen )) == NULL ) {
			fb_log("fb_read: malloc failed!");
			if( buf ) (void)free(buf);
			buflen = 0;
			return;
		}
	}

	ret = fb_read( fb_server_fbp, x, y, scanbuf, num );
	if( ret < 0 )  ret = 0;		/* map error indications */
	/* sending a 0-length package indicates error */
	pkg_send( MSG_RETURN, scanbuf, ret*sizeof(RGBpixel), pcp );
	if( buf ) (void)free(buf);
}


/*
 *			F B _ S E R V E R _ F B _ W R I T E
 *
 *  Client can ask for PKG-level acknowledgements (with error code) or not,
 *  based upon whether type is MSG_FBWRITE or MSG_FBWRITE+MSG_NORETURN.
 */
void
fb_server_fb_write(struct pkg_conn *pcp, char *buf)
{
	int	x, y, num;
	char	rbuf[NET_LONG_LEN+1];
	int	ret;
	int	type;

	x = pkg_glong( &buf[0*NET_LONG_LEN] );
	y = pkg_glong( &buf[1*NET_LONG_LEN] );
	num = pkg_glong( &buf[2*NET_LONG_LEN] );
	type = pcp->pkc_type;
	ret = fb_write( fb_server_fbp, x, y, (unsigned char *)&buf[3*NET_LONG_LEN], num );

	if( type < MSG_NORETURN ) {
		(void)pkg_plong( &rbuf[0*NET_LONG_LEN], ret );
		pkg_send( MSG_RETURN, rbuf, NET_LONG_LEN, pcp );
	}
	if( buf ) (void)free(buf);
}


/*
 *			F B _ S E R V E R _ F B _ R E A D R E C T
 */
void
fb_server_fb_readrect(struct pkg_conn *pcp, char *buf)
{
	int	xmin, ymin;
	int	width, height;
	int	num;
	int	ret;
	static unsigned char	*scanbuf = NULL;
	static int	buflen = 0;

	xmin = pkg_glong( &buf[0*NET_LONG_LEN] );
	ymin = pkg_glong( &buf[1*NET_LONG_LEN] );
	width = pkg_glong( &buf[2*NET_LONG_LEN] );
	height = pkg_glong( &buf[3*NET_LONG_LEN] );
	num = width * height;

	if( num*sizeof(RGBpixel) > buflen ) {
		if( scanbuf != NULL )
			free( (char *)scanbuf );
		buflen = num*sizeof(RGBpixel);
		if( buflen < 1024*sizeof(RGBpixel) )
			buflen = 1024*sizeof(RGBpixel);
		if( (scanbuf = (unsigned char *)malloc( buflen )) == NULL ) {
			fb_log("fb_read: malloc failed!");
			if( buf ) (void)free(buf);
			buflen = 0;
			return;
		}
	}

	ret = fb_readrect( fb_server_fbp, xmin, ymin, width, height, scanbuf );
	if( ret < 0 )  ret = 0;		/* map error indications */
	/* sending a 0-length package indicates error */
	pkg_send( MSG_RETURN, scanbuf, ret*sizeof(RGBpixel), pcp );
	if( buf ) (void)free(buf);
}


/*
 *			F B _ S E R V E R _ F B _ W R I T E R E C T
 *
 *  A whole rectangle of pixels at once, probably large.
 */
void
fb_server_fb_writerect(struct pkg_conn *pcp, char *buf)
{
	int	x, y;
	int	width, height;
	char	rbuf[NET_LONG_LEN+1];
	int	ret;
	int	type;

	x = pkg_glong( &buf[0*NET_LONG_LEN] );
	y = pkg_glong( &buf[1*NET_LONG_LEN] );
	width = pkg_glong( &buf[2*NET_LONG_LEN] );
	height = pkg_glong( &buf[3*NET_LONG_LEN] );

	type = pcp->pkc_type;
	ret = fb_writerect( fb_server_fbp, x, y, width, height,
		(unsigned char *)&buf[4*NET_LONG_LEN] );

	if( type < MSG_NORETURN ) {
		(void)pkg_plong( &rbuf[0*NET_LONG_LEN], ret );
		pkg_send( MSG_RETURN, rbuf, NET_LONG_LEN, pcp );
	}
	if( buf ) (void)free(buf);
}

/*
 *			F B _ S E R V E R _ F B _ B W R E A D R E C T
 */
void
fb_server_fb_bwreadrect(struct pkg_conn *pcp, char *buf)
{
	int	xmin, ymin;
	int	width, height;
	int	num;
	int	ret;
	static unsigned char	*scanbuf = NULL;
	static int	buflen = 0;

	xmin = pkg_glong( &buf[0*NET_LONG_LEN] );
	ymin = pkg_glong( &buf[1*NET_LONG_LEN] );
	width = pkg_glong( &buf[2*NET_LONG_LEN] );
	height = pkg_glong( &buf[3*NET_LONG_LEN] );
	num = width * height;

	if( num > buflen ) {
		if( scanbuf != NULL )
			free( (char *)scanbuf );
		buflen = num;
		if( buflen < 1024 )
			buflen = 1024;
		if( (scanbuf = (unsigned char *)malloc( buflen )) == NULL ) {
			fb_log("fb_bwreadrect: malloc failed!");
			if( buf ) (void)free(buf);
			buflen = 0;
			return;
		}
	}

	ret = fb_bwreadrect( fb_server_fbp, xmin, ymin, width, height, scanbuf );
	if( ret < 0 )  ret = 0;		/* map error indications */
	/* sending a 0-length package indicates error */
	pkg_send( MSG_RETURN, scanbuf, ret, pcp );
	if( buf ) (void)free(buf);
}


/*
 *			F B _ S E R V E R _ F B _ B W W R I T E R E C T
 *
 *  A whole rectangle of monochrome pixels at once, probably large.
 */
void
fb_server_fb_bwwriterect(struct pkg_conn *pcp, char *buf)
{
	int	x, y;
	int	width, height;
	char	rbuf[NET_LONG_LEN+1];
	int	ret;
	int	type;

	x = pkg_glong( &buf[0*NET_LONG_LEN] );
	y = pkg_glong( &buf[1*NET_LONG_LEN] );
	width = pkg_glong( &buf[2*NET_LONG_LEN] );
	height = pkg_glong( &buf[3*NET_LONG_LEN] );

	type = pcp->pkc_type;
	ret = fb_bwwriterect( fb_server_fbp, x, y, width, height,
		(unsigned char *)&buf[4*NET_LONG_LEN] );

	if( type < MSG_NORETURN ) {
		(void)pkg_plong( &rbuf[0*NET_LONG_LEN], ret );
		pkg_send( MSG_RETURN, rbuf, NET_LONG_LEN, pcp );
	} else {
		/* No formal return code.  Note errors locally */
		if(ret < 0) fb_log("fb_server_fb_bwwriterect(%d,%d, %d,%d) error %d\n",
			x, y, width, height, ret);
	}
	if( buf ) (void)free(buf);
}


/*
 *			F B _ S E R V E R _ F B _ C U R S O R
 *
 */
void
fb_server_fb_cursor(struct pkg_conn *pcp, char *buf)
{
	int	mode, x, y;
	char	rbuf[NET_LONG_LEN+1];

	mode = pkg_glong( &buf[0*NET_LONG_LEN] );
	x = pkg_glong( &buf[1*NET_LONG_LEN] );
	y = pkg_glong( &buf[2*NET_LONG_LEN] );

	(void)pkg_plong( &rbuf[0], fb_cursor( fb_server_fbp, mode, x, y ) );
	pkg_send( MSG_RETURN, rbuf, NET_LONG_LEN, pcp );
	if( buf ) (void)free(buf);
}


/*
 *			F B _ S E R V E R _ F B _ G E T _ C U R S O R
 *
 */
void
fb_server_fb_getcursor(struct pkg_conn *pcp, char *buf)
{
	int	ret;
	int	mode, x, y;
	char	rbuf[4*NET_LONG_LEN+1];

	ret = fb_getcursor( fb_server_fbp, &mode, &x, &y );
	(void)pkg_plong( &rbuf[0*NET_LONG_LEN], ret );
	(void)pkg_plong( &rbuf[1*NET_LONG_LEN], mode );
	(void)pkg_plong( &rbuf[2*NET_LONG_LEN], x );
	(void)pkg_plong( &rbuf[3*NET_LONG_LEN], y );
	pkg_send( MSG_RETURN, rbuf, 4*NET_LONG_LEN, pcp );
	if( buf ) (void)free(buf);
}


/*
 *			F B _ S E R V E R _ F B _ S E T C U R S O R
 *
 */
void
fb_server_fb_setcursor(struct pkg_conn *pcp, char *buf)
{
	char	rbuf[NET_LONG_LEN+1];
	int	ret;
	int	xbits, ybits;
	int	xorig, yorig;

	xbits = pkg_glong( &buf[0*NET_LONG_LEN] );
	ybits = pkg_glong( &buf[1*NET_LONG_LEN] );
	xorig = pkg_glong( &buf[2*NET_LONG_LEN] );
	yorig = pkg_glong( &buf[3*NET_LONG_LEN] );

	ret = fb_setcursor( fb_server_fbp, (unsigned char *)&buf[4*NET_LONG_LEN],
		xbits, ybits, xorig, yorig );

	if( pcp->pkc_type < MSG_NORETURN ) {
		(void)pkg_plong( &rbuf[0*NET_LONG_LEN], ret );
		pkg_send( MSG_RETURN, rbuf, NET_LONG_LEN, pcp );
	}
	if( buf ) (void)free(buf);
}


/*
 *			F B _ S E R V E R _ F B _ S C U R S O R
 *
 * An OLD iterface.  Retained so old clients can still be served.
 */
void
fb_server_fb_scursor(struct pkg_conn *pcp, char *buf)
{
	int	mode, x, y;
	char	rbuf[NET_LONG_LEN+1];

	mode = pkg_glong( &buf[0*NET_LONG_LEN] );
	x = pkg_glong( &buf[1*NET_LONG_LEN] );
	y = pkg_glong( &buf[2*NET_LONG_LEN] );

	(void)pkg_plong( &rbuf[0], fb_scursor( fb_server_fbp, mode, x, y ) );
	pkg_send( MSG_RETURN, rbuf, NET_LONG_LEN, pcp );
	if( buf ) (void)free(buf);
}


/*
 *			F B _ S E R V E R _ F B _ W I N D O W
 *
 * An OLD iterface.  Retained so old clients can still be served.
 */
void
fb_server_fb_window(struct pkg_conn *pcp, char *buf)
{
	int	x, y;
	char	rbuf[NET_LONG_LEN+1];

	x = pkg_glong( &buf[0*NET_LONG_LEN] );
	y = pkg_glong( &buf[1*NET_LONG_LEN] );

	(void)pkg_plong( &rbuf[0], fb_window( fb_server_fbp, x, y ) );
	pkg_send( MSG_RETURN, rbuf, NET_LONG_LEN, pcp );
	if( buf ) (void)free(buf);
}


/*
 *			F B _ S E R V E R _ F B _ Z O O M
 *
 * An OLD iterface.  Retained so old clients can still be served.
 */
void
fb_server_fb_zoom(struct pkg_conn *pcp, char *buf)
{
	int	x, y;
	char	rbuf[NET_LONG_LEN+1];

	x = pkg_glong( &buf[0*NET_LONG_LEN] );
	y = pkg_glong( &buf[1*NET_LONG_LEN] );

	(void)pkg_plong( &rbuf[0], fb_zoom( fb_server_fbp, x, y ) );
	pkg_send( MSG_RETURN, rbuf, NET_LONG_LEN, pcp );
	if( buf ) (void)free(buf);
}


/*
 *			F B _ S E R V E R _ F B _ V I E W
 *
 */
void
fb_server_fb_view(struct pkg_conn *pcp, char *buf)
{
	int	ret;
	int	xcenter, ycenter, xzoom, yzoom;
	char	rbuf[NET_LONG_LEN+1];

	xcenter = pkg_glong( &buf[0*NET_LONG_LEN] );
	ycenter = pkg_glong( &buf[1*NET_LONG_LEN] );
	xzoom = pkg_glong( &buf[2*NET_LONG_LEN] );
	yzoom = pkg_glong( &buf[3*NET_LONG_LEN] );

	ret = fb_view( fb_server_fbp, xcenter, ycenter, xzoom, yzoom );
	(void)pkg_plong( &rbuf[0], ret );
	pkg_send( MSG_RETURN, rbuf, NET_LONG_LEN, pcp );
	if( buf ) (void)free(buf);
}


/*
 *			F B _ S E R V E R _ F B _ G E T V I E W
 *
 */
void
fb_server_fb_getview(struct pkg_conn *pcp, char *buf)
{
	int	ret;
	int	xcenter, ycenter, xzoom, yzoom;
	char	rbuf[5*NET_LONG_LEN+1];

	ret = fb_getview( fb_server_fbp, &xcenter, &ycenter, &xzoom, &yzoom );
	(void)pkg_plong( &rbuf[0*NET_LONG_LEN], ret );
	(void)pkg_plong( &rbuf[1*NET_LONG_LEN], xcenter );
	(void)pkg_plong( &rbuf[2*NET_LONG_LEN], ycenter );
	(void)pkg_plong( &rbuf[3*NET_LONG_LEN], xzoom );
	(void)pkg_plong( &rbuf[4*NET_LONG_LEN], yzoom );
	pkg_send( MSG_RETURN, rbuf, 5*NET_LONG_LEN, pcp );
	if( buf ) (void)free(buf);
}


/*
 *			F B _ S E R V E R _ F B _ R M A P
 *
 */
void
fb_server_fb_rmap(struct pkg_conn *pcp, char *buf)
{
	register int	i;
	char	rbuf[NET_LONG_LEN+1];
	ColorMap map;
	unsigned char	cm[256*2*3];

	(void)pkg_plong( &rbuf[0*NET_LONG_LEN], fb_rmap( fb_server_fbp, &map ) );
	for( i = 0; i < 256; i++ ) {
		(void)pkg_pshort( cm+2*(0+i), map.cm_red[i] );
		(void)pkg_pshort( cm+2*(256+i), map.cm_green[i] );
		(void)pkg_pshort( cm+2*(512+i), map.cm_blue[i] );
	}
	pkg_send( MSG_DATA, cm, sizeof(cm), pcp );
	pkg_send( MSG_RETURN, rbuf, NET_LONG_LEN, pcp );
	if( buf ) (void)free(buf);
}


/*
 *			F B _ S E R V E R _ F B _ W M A P
 *
 *  Accept a color map sent by the client, and write it to the framebuffer.
 *  Network format is to send each entry as a network (IBM) order 2-byte
 *  short, 256 red shorts, followed by 256 green and 256 blue, for a total
 *  of 3*256*2 bytes.
 */
void
fb_server_fb_wmap(struct pkg_conn *pcp, char *buf)
{
	int	i;
	char	rbuf[NET_LONG_LEN+1];
	long	ret;
	ColorMap map;

	if( pcp->pkc_len == 0 )
		ret = fb_wmap( fb_server_fbp, COLORMAP_NULL );
	else {
		for( i = 0; i < 256; i++ ) {
			map.cm_red[i] = pkg_gshort( buf+2*(0+i) );
			map.cm_green[i] = pkg_gshort( buf+2*(256+i) );
			map.cm_blue[i] = pkg_gshort( buf+2*(512+i) );
		}
		ret = fb_wmap( fb_server_fbp, &map );
	}
	(void)pkg_plong( &rbuf[0], ret );
	pkg_send( MSG_RETURN, rbuf, NET_LONG_LEN, pcp );
	if( buf ) (void)free(buf);
}


/*
 *			F B _ S E R V E R _ F B _ F L U S H
 *
 */
void
fb_server_fb_flush(struct pkg_conn *pcp, char *buf)
{
	int	ret;
	char	rbuf[NET_LONG_LEN+1];

	ret = fb_flush( fb_server_fbp );

	if( pcp->pkc_type < MSG_NORETURN ) {
		(void)pkg_plong( rbuf, ret );
		pkg_send( MSG_RETURN, rbuf, NET_LONG_LEN, pcp );
	}
	if( buf ) (void)free(buf);
}


/*
 *			F B _ S E R V E R _ F B _ P O L L
 *
 */
void
fb_server_fb_poll(struct pkg_conn *pcp, char *buf)
{
	(void)fb_poll( fb_server_fbp );
	if( buf ) (void)free(buf);
}


/*
 *			F B _ S E R V E R _ F B _ H E L P
 *
 *  At one time at least we couldn't send a zero length PKG
 *  message back and forth, so we receive a dummy long here.
 */
void
fb_server_fb_help(struct pkg_conn *pcp, char *buf)
{
	long	ret;
	char	rbuf[NET_LONG_LEN+1];

	(void)pkg_glong( &buf[0*NET_LONG_LEN] );

	ret = fb_help(fb_server_fbp);
	(void)pkg_plong( &rbuf[0], ret );
	pkg_send( MSG_RETURN, rbuf, NET_LONG_LEN, pcp );
	if( buf ) (void)free(buf);
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
