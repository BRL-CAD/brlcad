/*                     I F _ R E M O T E . C
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

/** \addtogroup if */
/*@{*/
/** @file if_remote.c
 *  Remote libfb interface.
 *
 *  Duplicates the functions in libfb via communication
 *  with a remote server (fbserv).
 *
 *  Note that internal errors are returned as -2 and below, because
 *  most remote errors (unpacked by fbgetlong) will be -1
 *  (although they could be anything).
 *
 *  Authors -
 *	Phillip Dykstra
 *	Gary S. Moss
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

#include <stdio.h>
#ifdef HAVE_WRITEV
#  include <sys/uio.h>		/* for struct iovec */
#endif

#ifdef HAVE_NETINET_IN_H
#  include <netinet/in.h>		/* for htons(), etc */
#else
#endif

#ifdef HAVE_SYS_SOCKET_H
#  include <sys/socket.h>
#endif

#ifdef HAVE_STRING_H
#  include <string.h>
#else
#  include <strings.h>
#endif

#include "machine.h"
#include "pkg.h"

#include "fb.h"
#include "./fblocal.h"

#include "fbmsg.h"

#define NET_LONG_LEN	4	/* # bytes to network long */

#define MAX_HOSTNAME	128
#define	PCP(ptr)	((struct pkg_conn *)((ptr)->u1.p))
#define	PCPL(ptr)	((ptr)->u1.p)	/* left hand side version */

/* Package Handlers. */
static void	pkgerror(struct pkg_conn *pcpp, char *buf);	/* error message handler */
static struct pkg_switch pkgswitch[] = {
	{ MSG_ERROR, pkgerror, "Error Message" },
	{ 0, NULL, NULL }
};

_LOCAL_ int	rem_open(register FBIO *ifp, register char *file, int width, int height),
		rem_close(FBIO *ifp),
		rem_clear(FBIO *ifp, unsigned char *bgpp),
		rem_read(register FBIO *ifp, int x, int y, unsigned char *pixelp, int num),
		rem_write(register FBIO *ifp, int x, int y, const unsigned char *pixelp, int num),
		rem_rmap(register FBIO *ifp, register ColorMap *cmap),
		rem_wmap(register FBIO *ifp, const ColorMap *cmap),
		rem_view(FBIO *ifp, int xcenter, int ycenter, int xzoom, int yzoom),
		rem_getview(FBIO *ifp, int *xcenter, int *ycenter, int *xzoom, int *yzoom),
		rem_cursor(FBIO *ifp, int mode, int x, int y),
		rem_getcursor(FBIO *ifp, int *mode, int *x, int *y),
		rem_setcursor(FBIO *ifp, const unsigned char *bits, int xbits, int ybits, int xorig, int yorig),
		rem_readrect(FBIO *ifp, int xmin, int ymin, int width, int height, unsigned char *pp),
		rem_writerect(FBIO *ifp, int xmin, int ymin, int width, int height, const unsigned char *pp),
		rem_bwreadrect(FBIO *ifp, int xmin, int ymin, int width, int height, unsigned char *pp),
		rem_bwwriterect(FBIO *ifp, int xmin, int ymin, int width, int height, const unsigned char *pp),
		rem_poll(FBIO *ifp),
		rem_flush(FBIO *ifp),
		rem_free(FBIO *ifp),
		rem_help(FBIO *ifp);

FBIO remote_interface = {
	0,
	rem_open,
	rem_close,
	rem_clear,
	rem_read,
	rem_write,
	rem_rmap,
	rem_wmap,
	rem_view,
	rem_getview,
	rem_setcursor,
	rem_cursor,
	rem_getcursor,
	rem_readrect,
	rem_writerect,
	rem_bwreadrect,
	rem_bwwriterect,
	rem_poll,
	rem_flush,
	rem_free,
	rem_help,
	"Remote Device Interface",	/* should be filled in	*/
	1024,				/* " */
	1024,				/* " */
	"host:[dev]",
	512,
	512,
	-1,				/* select fd */
	-1,
	1, 1,				/* zoom */
	256, 256,			/* window center */
	0, 0, 0,			/* cursor */
	PIXEL_NULL,
	PIXEL_NULL,
	PIXEL_NULL,
	-1,
	0,
	0L,
	0L,
	0
};

void	pkg_queue(), flush_queue();

/* from getput.c */
extern unsigned short fbgetshort(char *msgp);
extern unsigned long fbgetlong(char *msgp);
extern char *fbputshort(register short unsigned int s, register char *msgp), *fbputlong(register long unsigned int l, register char *msgp);

/* True if the non-null string s is all digits */
static int
numeric(register char *s)
{
	if( s == (char *)0 || *s == 0 )
		return	0;

	while( *s ) {
		if( *s < '0' || *s > '9' )
			return	0;
		s++;
	}

	return	1;
}

/*
 *  Break up a file specification into its component parts.
 *  We try to be infinitely flexible here which makes this complicated.
 *  Handle any of the following:
 *
 *	File			Host		Port		Dev
 *	0			localhost	0		NULL
 *	0:[dev]			localhost	0		dev
 *	:0			localhost	0		NULL
 *	host:[dev]		host		remotefb	dev
 *	host:0			host		0		NULL
 *	host:0:[dev]		host		0		dev
 *
 *  Return -1 on error, else 0.
 */
int
parse_file(char *file, char *host, int *portp, char *device)
           	/* input file spec */
           	/* host part */
            	/* port number */
             	/* device part */
{
	int	port;
	char	prefix[256];
	char	*rest;
	char	*dev;
	char	*colon;

	if( numeric(file) ) {
		/* 0 */
		port = atoi(file);
		strcpy( host, "localhost" );
		dev = "";
		goto done;
	}
	if( (colon = strchr(file, ':')) != NULL ) {
		strncpy( prefix, file, colon-file );
		prefix[colon-file] = '\0';
		rest = colon+1;
		if( numeric(prefix) ) {
			/* 0:[dev] */
			port = atoi(prefix);
			strcpy( host, "localhost" );
			dev = rest;
			goto done;
		} else {
			/* :[dev] or host:[dev] */
			strcpy( host, prefix );
			if( numeric(rest) ) {
				/* :0 or host:0 */
				port = atoi(rest);
				dev = "";
				goto done;
			} else {
				/* check for [host]:0:[dev] */
				if( (colon = strchr(rest, ':')) != NULL ) {
					strncpy( prefix, rest, colon-rest );
					prefix[colon-rest] = '\0';
					if( numeric(prefix) ) {
						port = atoi(prefix);
						dev = colon+1;
						goto done;
					} else {
						/* No port given! */
						dev = rest;
						port = 5558;	/*XXX*/
						goto done;
					}
				} else {
					/* No port given */
					dev = rest;
					port = 5558;		/*XXX*/
					goto done;
				}
			}
		}
	}
	/* bad file spec */
	return	-1;

done:
	/* Default hostname */
	if( strlen(host) == 0 ) {
		strcpy( host, "localhost" );
	}
	/* Magic port number mapping */
	if( port < 0 )
		return	-1;
	if( port < 1024 )
		port += 5559;
	/*
	 * In the spirit of X, let "unix" be an alias for the "localhost".
	 * Eventually this may invoke UNIX Domain PKG (if we can figure
	 * out what to do about socket pathnames).
	 */
	if( strcmp(host,"unix") == 0 )
		strcpy( host, "localhost" );

	/* copy out port and device */
	*portp = port;
	strcpy( device, dev );

	return( 0 );
}

/*
 * Open a connection to the remotefb.
 * We send NET_LONG_LEN bytes of mode, NET_LONG_LEN bytes of size, then the
 *  devname (or NULL if default).
 */
_LOCAL_ int
rem_open(register FBIO *ifp, register char *file, int width, int height)
{
	register int	i;
	struct pkg_conn *pc;
	char	buf[128];
	char	hostname[MAX_HOSTNAME];
	char	portname[MAX_HOSTNAME];
	char	device[MAX_HOSTNAME];
	int	port;

	FB_CK_FBIO(ifp);

	hostname[0] = '\0';
	portname[0] = '\0';
	port = 0;

	if( file == NULL || parse_file(file, hostname, &port, device) < 0 ) {
		/* too wild for our tastes */
		fb_log( "rem_open: bad device name \"%s\"\n",
			file == NULL ? "(null)" : file );
		return	-2;
	}
	/*printf("hostname = \"%s\", port = %d, device = \"%s\"\n", hostname, port, device );*/

	if( port != 5558 ) {
		sprintf(portname, "%d", port);
		if( (pc = pkg_open( hostname, portname, 0, 0, 0, pkgswitch, fb_log )) == PKC_ERROR ) {
			fb_log(	"rem_open: can't connect to fb server on host \"%s\", port \"%s\".\n",
				hostname, portname );
			return	-3;
		}
	} else
	if( (pc = pkg_open( hostname, "remotefb", 0, 0, 0, pkgswitch, fb_log )) == PKC_ERROR &&
	    (pc = pkg_open( hostname, "5558", 0, 0, 0, pkgswitch, fb_log )) == PKC_ERROR ) {
		fb_log(	"rem_open: can't connect to remotefb server on host \"%s\".\n",
			hostname );
		return	-4;
	}
	PCPL(ifp) = (char *)pc;			/* stash in u1 */
	ifp->if_fd = pc->pkc_fd;		/* unused */

#ifdef HAVE_SYS_SOCKET_H
	{
		int	n;
		int	val;
		val = 32767;
		n = setsockopt( pc->pkc_fd, SOL_SOCKET,
			SO_SNDBUF, (char *)&val, sizeof(val) );
		if( n < 0 )  perror("setsockopt: SO_SNDBUF");

		val = 32767;
		n = setsockopt( pc->pkc_fd, SOL_SOCKET,
			SO_RCVBUF, (char *)&val, sizeof(val) );
		if( n < 0 )  perror("setsockopt: SO_RCVBUF");
	}
#endif

	(void)fbputlong( width, &buf[0*NET_LONG_LEN] );
	(void)fbputlong( height, &buf[1*NET_LONG_LEN] );
	(void) strcpy( &buf[2*NET_LONG_LEN], device );
	i = strlen(device)+2*NET_LONG_LEN;
	if( pkg_send( MSG_FBOPEN, buf, i, pc ) != i )
		return	-5;

	/* return code, max_width, max_height, width, height as longs */
	if( pkg_waitfor( MSG_RETURN, buf, sizeof(buf), pc ) < 5*NET_LONG_LEN )
		return	-6;
	ifp->if_max_width = fbgetlong( &buf[1*NET_LONG_LEN] );
	ifp->if_max_height = fbgetlong( &buf[2*NET_LONG_LEN] );
	ifp->if_width = fbgetlong( &buf[3*NET_LONG_LEN] );
	ifp->if_height = fbgetlong( &buf[4*NET_LONG_LEN] );
	if( fbgetlong( &buf[0*NET_LONG_LEN] ) != 0 )
		return	-7;		/* fail */
	return( 0 );			/* OK */
}

_LOCAL_ int
rem_close(FBIO *ifp)
{
	char	buf[NET_LONG_LEN+1];

	/* send a close package to remote */
	if( pkg_send( MSG_FBCLOSE, (char *)0, 0, PCP(ifp) ) < 0 )
		return	-2;
	/*
	 *  When if_4d with a "linger mode" window gets it's fb_close()
	 *  call here, it closes down the network file descriptor,
	 *  and so the PKG connection is terminated at this point.
	 *  If there was no transmission error noted in the pkg_send() above,
	 *  but the pkg_waitfor() here gets an error, clean up and
	 *  declare this a successful close() operation.
	 */
	if( pkg_waitfor( MSG_RETURN, buf, NET_LONG_LEN, PCP(ifp) ) < 1*NET_LONG_LEN )  {
		pkg_close( PCP(ifp) );
		return	0;
	}
	pkg_close( PCP(ifp) );
	return( fbgetlong( &buf[0*NET_LONG_LEN] ) );
}

_LOCAL_ int
rem_free(FBIO *ifp)
{
	char	buf[NET_LONG_LEN+1];

	/* send a free package to remote */
	if( pkg_send( MSG_FBFREE, (char *)0, 0, PCP(ifp) ) < 0 )
		return	-2;
	if( pkg_waitfor( MSG_RETURN, buf, NET_LONG_LEN, PCP(ifp) ) < 1*NET_LONG_LEN )
		return	-3;
	pkg_close( PCP(ifp) );
	return( fbgetlong( &buf[0*NET_LONG_LEN] ) );
}

_LOCAL_ int
rem_clear(FBIO *ifp, unsigned char *bgpp)
{
	char	buf[NET_LONG_LEN+1];

	/* send a clear package to remote */
	if( bgpp == PIXEL_NULL )  {
		buf[0] = buf[1] = buf[2] = 0;	/* black */
	} else {
		buf[0] = (bgpp)[RED];
		buf[1] = (bgpp)[GRN];
		buf[2] = (bgpp)[BLU];
	}
	if( pkg_send( MSG_FBCLEAR, buf, 3, PCP(ifp) ) < 3 )
		return	-2;
	if( pkg_waitfor( MSG_RETURN, buf, NET_LONG_LEN, PCP(ifp) ) < 1*NET_LONG_LEN )
		return	-3;
	return( fbgetlong( buf ) );
}

/*
 *  Send as longs:  x, y, num
 */
_LOCAL_ int
rem_read(register FBIO *ifp, int x, int y, unsigned char *pixelp, int num)
{
	int	ret;
	char	buf[3*NET_LONG_LEN+1];

	if( num <= 0 )
		return(0);
	/* Send Read Command */
	(void)fbputlong( x, &buf[0*NET_LONG_LEN] );
	(void)fbputlong( y, &buf[1*NET_LONG_LEN] );
	(void)fbputlong( num, &buf[2*NET_LONG_LEN] );
	if( pkg_send( MSG_FBREAD, buf, 3*NET_LONG_LEN, PCP(ifp) ) < 3*NET_LONG_LEN )
		return	-2;

	/* Get response;  0 len means failure */
	ret = pkg_waitfor( MSG_RETURN, (char *)pixelp,
		num*sizeof(RGBpixel), PCP(ifp) );
	if( ret <= 0 )  {
		fb_log( "rem_read: read %d at <%d,%d> failed, ret=%d.\n",
			num, x, y, ret );
		return	-3;
	}
	return( ret/sizeof(RGBpixel) );
}

/*
 * As longs, x, y, num
 */
_LOCAL_ int
rem_write(register FBIO *ifp, int x, int y, const unsigned char *pixelp, int num)
{
	int	ret;
	char	buf[3*NET_LONG_LEN+1];

	if( num <= 0 )  return	num;

	/* Send Write Command */
	(void)fbputlong( x, &buf[0*NET_LONG_LEN] );
	(void)fbputlong( y, &buf[1*NET_LONG_LEN] );
	(void)fbputlong( num, &buf[2*NET_LONG_LEN] );
	ret = pkg_2send( MSG_FBWRITE+MSG_NORETURN,
		buf, 3*NET_LONG_LEN,
		(char *)pixelp, num*sizeof(RGBpixel),
		PCP(ifp) );
	ret -= 3*NET_LONG_LEN;
	if( ret < 0 )
		return	-1;	/* Error from libpkg */
	return	ret/sizeof(RGBpixel);
	/* No reading an error return package, sacrificed for speed. */
}

/*
 *			R E M _ R E A D R E C T
 */
_LOCAL_ int
rem_readrect(FBIO *ifp, int xmin, int ymin, int width, int height, unsigned char *pp)
{
	int	num;
	int	ret;
	char	buf[4*NET_LONG_LEN+1];

	num = width*height;
	if( num <= 0 )
		return(0);
	/* Send Read Command */
	(void)fbputlong( xmin, &buf[0*NET_LONG_LEN] );
	(void)fbputlong( ymin, &buf[1*NET_LONG_LEN] );
	(void)fbputlong( width, &buf[2*NET_LONG_LEN] );
	(void)fbputlong( height, &buf[3*NET_LONG_LEN] );
	if( pkg_send( MSG_FBREADRECT, buf, 4*NET_LONG_LEN, PCP(ifp) ) < 4*NET_LONG_LEN )
		return	-2;

	/* Get response;  0 len means failure */
	ret = pkg_waitfor( MSG_RETURN, (char *)pp,
			num*sizeof(RGBpixel), PCP(ifp) );
	if( ret <= 0 )  {
		fb_log( "rem_rectread: read %d at <%d,%d> failed, ret=%d.\n",
			num, xmin, ymin, ret );
		return	-3;
	}
	return( ret/sizeof(RGBpixel) );
}

/*
 *			R E M _ W R I T E R E C T
 */
_LOCAL_ int
rem_writerect(FBIO *ifp, int xmin, int ymin, int width, int height, const unsigned char *pp)
{
	int	num;
	int	ret;
	char	buf[4*NET_LONG_LEN+1];

	num = width*height;
	if( num <= 0 )
		return(0);

	/* Send Write Command */
	(void)fbputlong( xmin, &buf[0*NET_LONG_LEN] );
	(void)fbputlong( ymin, &buf[1*NET_LONG_LEN] );
	(void)fbputlong( width, &buf[2*NET_LONG_LEN] );
	(void)fbputlong( height, &buf[3*NET_LONG_LEN] );
	ret = pkg_2send( MSG_FBWRITERECT+MSG_NORETURN,
		buf, 4*NET_LONG_LEN,
		(char *)pp, num*sizeof(RGBpixel),
		PCP(ifp) );
	ret -= 4*NET_LONG_LEN;
	if( ret < 0 )
		return	-4;	/* Error from libpkg */
	return	ret/sizeof(RGBpixel);
	/* No reading an error return package, sacrificed for speed. */
}

/*
 *			R E M _ B W R E A D R E C T
 *
 *  Issue:  Determining if other end has support for this yet.
 */
_LOCAL_ int
rem_bwreadrect(FBIO *ifp, int xmin, int ymin, int width, int height, unsigned char *pp)
{
	int	num;
	int	ret;
	char	buf[4*NET_LONG_LEN+1];

	num = width*height;
	if( num <= 0 )
		return(0);
	/* Send Read Command */
	(void)fbputlong( xmin, &buf[0*NET_LONG_LEN] );
	(void)fbputlong( ymin, &buf[1*NET_LONG_LEN] );
	(void)fbputlong( width, &buf[2*NET_LONG_LEN] );
	(void)fbputlong( height, &buf[3*NET_LONG_LEN] );
	if( pkg_send( MSG_FBBWREADRECT, buf, 4*NET_LONG_LEN, PCP(ifp) ) < 4*NET_LONG_LEN )
		return	-2;

	/* Get response;  0 len means failure */
	ret = pkg_waitfor( MSG_RETURN, (char *)pp, num, PCP(ifp) );
	if( ret <= 0 )  {
		fb_log( "rem_bwrectread: read %d at <%d,%d> failed, ret=%d.\n",
			num, xmin, ymin, ret );
		return	-3;
	}
	return ret;
}

/*
 *			R E M _ B W W R I T E R E C T
 */
_LOCAL_ int
rem_bwwriterect(FBIO *ifp, int xmin, int ymin, int width, int height, const unsigned char *pp)
{
	int	num;
	int	ret;
	char	buf[4*NET_LONG_LEN+1];

	num = width*height;
	if( num <= 0 )
		return(0);

	/* Send Write Command */
	(void)fbputlong( xmin, &buf[0*NET_LONG_LEN] );
	(void)fbputlong( ymin, &buf[1*NET_LONG_LEN] );
	(void)fbputlong( width, &buf[2*NET_LONG_LEN] );
	(void)fbputlong( height, &buf[3*NET_LONG_LEN] );
	ret = pkg_2send( MSG_FBBWWRITERECT+MSG_NORETURN,
		buf, 4*NET_LONG_LEN,
		(char *)pp, num,
		PCP(ifp) );
	ret -= 4*NET_LONG_LEN;
	if( ret < 0 )
		return	-4;	/* Error from libpkg */
	return	ret;
	/* No reading an error return package, sacrificed for speed. */
}

/*
 *  32-bit longs: mode, x, y
 */
_LOCAL_ int
rem_cursor(FBIO *ifp, int mode, int x, int y)
{
	char	buf[3*NET_LONG_LEN+1];

	/* Send Command */
	(void)fbputlong( mode, &buf[0*NET_LONG_LEN] );
	(void)fbputlong( x, &buf[1*NET_LONG_LEN] );
	(void)fbputlong( y, &buf[2*NET_LONG_LEN] );
	if( pkg_send( MSG_FBCURSOR, buf, 3*NET_LONG_LEN, PCP(ifp) ) < 3*NET_LONG_LEN )
		return	-2;
	if( pkg_waitfor( MSG_RETURN, buf, NET_LONG_LEN, PCP(ifp) ) < 1*NET_LONG_LEN )
		return	-3;
	return( fbgetlong( buf ) );
}

/*
 */
_LOCAL_ int
rem_getcursor(FBIO *ifp, int *mode, int *x, int *y)
{
	char	buf[4*NET_LONG_LEN+1];

	/* Send Command */
	if( pkg_send( MSG_FBGETCURSOR, (char *)0, 0, PCP(ifp) ) < 0 )
		return	-2;

	/* return code, xcenter, ycenter, xzoom, yzoom as longs */
	if( pkg_waitfor( MSG_RETURN, buf, sizeof(buf), PCP(ifp) ) < 4*NET_LONG_LEN )
		return	-3;
	*mode = fbgetlong( &buf[1*NET_LONG_LEN] );
	*x = fbgetlong( &buf[2*NET_LONG_LEN] );
	*y = fbgetlong( &buf[3*NET_LONG_LEN] );
	if( fbgetlong( &buf[0*NET_LONG_LEN] ) != 0 )
		return	-4;		/* fail */
	return( 0 );			/* OK */
}

/*
 *			R E M _ S E T C U R S O R
 *
 *  Program the "shape" of the cursor.
 *
 *  bits[] has xbits*ybits bits in it, rounded up to next largest byte.
 *
 *  Do not confuse this routine with the old fb_scursor() call.
 */
_LOCAL_ int
rem_setcursor(FBIO *ifp, const unsigned char *bits, int xbits, int ybits, int xorig, int yorig)
{
	char	buf[4*NET_LONG_LEN+1];
	int	ret;

	(void)fbputlong( xbits, &buf[0*NET_LONG_LEN] );
	(void)fbputlong( ybits, &buf[1*NET_LONG_LEN] );
	(void)fbputlong( xorig, &buf[2*NET_LONG_LEN] );
	(void)fbputlong( yorig, &buf[3*NET_LONG_LEN] );

	ret = pkg_2send( MSG_FBSETCURSOR+MSG_NORETURN,
		buf, 4*NET_LONG_LEN,
		(char *)bits, (xbits*ybits+7)>>3,
		PCP(ifp) );
	ret -= 4*NET_LONG_LEN;
	if( ret < 0 )
		return	-1;	/* Error from libpkg */

#if 0
	if( pkg_waitfor( MSG_RETURN, buf, NET_LONG_LEN, PCP(ifp) ) < 1*NET_LONG_LEN )
		return	-2;
	return( fbgetlong( buf ) );
#else
	/* Since this call got somehow overlooked until Release 4.3,
	 * older 'fbserv' programs won't have support for this request.
	 * Rather than dooming LGT users to endless frustration,
	 * simply launch off the request and tell our caller that all is well.
	 * LGT never actually checks the return code of this routine anyway.
	 */
	return 0;
#endif
}

/*
 */
_LOCAL_ int
rem_view(FBIO *ifp, int xcenter, int ycenter, int xzoom, int yzoom)
{
	char	buf[4*NET_LONG_LEN+1];

	/* Send Command */
	(void)fbputlong( xcenter, &buf[0*NET_LONG_LEN] );
	(void)fbputlong( ycenter, &buf[1*NET_LONG_LEN] );
	(void)fbputlong( xzoom, &buf[2*NET_LONG_LEN] );
	(void)fbputlong( yzoom, &buf[3*NET_LONG_LEN] );
	if( pkg_send( MSG_FBVIEW, buf, 4*NET_LONG_LEN, PCP(ifp) ) < 4*NET_LONG_LEN )
		return	-2;
	if( pkg_waitfor( MSG_RETURN, buf, NET_LONG_LEN, PCP(ifp) ) < 1*NET_LONG_LEN )
		return	-3;
	return( fbgetlong( buf ) );
}

/*
 */
_LOCAL_ int
rem_getview(FBIO *ifp, int *xcenter, int *ycenter, int *xzoom, int *yzoom)
{
	char	buf[5*NET_LONG_LEN+1];

	/* Send Command */
	if( pkg_send( MSG_FBGETVIEW, (char *)0, 0, PCP(ifp) ) < 0 )
		return	-2;

	/* return code, xcenter, ycenter, xzoom, yzoom as longs */
	if( pkg_waitfor( MSG_RETURN, buf, sizeof(buf), PCP(ifp) ) < 5*NET_LONG_LEN )
		return	-3;
	*xcenter = fbgetlong( &buf[1*NET_LONG_LEN] );
	*ycenter = fbgetlong( &buf[2*NET_LONG_LEN] );
	*xzoom = fbgetlong( &buf[3*NET_LONG_LEN] );
	*yzoom = fbgetlong( &buf[4*NET_LONG_LEN] );
	if( fbgetlong( &buf[0*NET_LONG_LEN] ) != 0 )
		return	-4;		/* fail */
	return( 0 );			/* OK */
}

#define REM_CMAP_BYTES	(256*3*2)

_LOCAL_ int
rem_rmap(register FBIO *ifp, register ColorMap *cmap)
{
	register int	i;
	char	buf[NET_LONG_LEN+1];
	char	cm[REM_CMAP_BYTES+4];

	if( pkg_send( MSG_FBRMAP, (char *)0, 0, PCP(ifp) ) < 0 )
		return	-2;
	if( pkg_waitfor( MSG_DATA, cm, REM_CMAP_BYTES, PCP(ifp) ) < REM_CMAP_BYTES )
		return	-3;
	for( i = 0; i < 256; i++ ) {
		cmap->cm_red[i] = fbgetshort( cm+2*(0+i) );
		cmap->cm_green[i] = fbgetshort( cm+2*(256+i) );
		cmap->cm_blue[i] = fbgetshort( cm+2*(512+i) );
	}
	if( pkg_waitfor( MSG_RETURN, buf, NET_LONG_LEN, PCP(ifp) ) < 1*NET_LONG_LEN )
		return	-4;
	return( fbgetlong( &buf[0*NET_LONG_LEN] ) );
}

_LOCAL_ int
rem_wmap(register FBIO *ifp, const ColorMap *cmap)
{
	register int	i;
	char	buf[NET_LONG_LEN+1];
	char	cm[REM_CMAP_BYTES+4];

	if( cmap == COLORMAP_NULL ) {
		if( pkg_send( MSG_FBWMAP, (char *)0, 0, PCP(ifp) ) < 0 )
			return	-2;
	} else {
		for( i = 0; i < 256; i++ ) {
			(void)fbputshort( cmap->cm_red[i], cm+2*(0+i) );
			(void)fbputshort( cmap->cm_green[i], cm+2*(256+i) );
			(void)fbputshort( cmap->cm_blue[i], cm+2*(512+i) );
		}
		if( pkg_send( MSG_FBWMAP, cm, REM_CMAP_BYTES, PCP(ifp) ) < REM_CMAP_BYTES )
			return	-3;
	}
	if( pkg_waitfor( MSG_RETURN, buf, NET_LONG_LEN, PCP(ifp) ) < 1*NET_LONG_LEN )
		return	-4;
	return( fbgetlong( &buf[0*NET_LONG_LEN] ) );
}

/*
 *  Poll tells the remote end to handle input events.
 *  There is no need to wait for a reply (FLUSH can be
 *  used for synchronization.
 *  In fact, we may not want to send polls at all....
 */
_LOCAL_ int
rem_poll(FBIO *ifp)
{
	/* send a poll package to remote */
	if( pkg_send( MSG_FBPOLL, (char *)0, 0, PCP(ifp) ) < 0 )
		return	-1;
	return( 0 );
}

_LOCAL_ int
rem_flush(FBIO *ifp)
{
	char	buf[NET_LONG_LEN+1];

	/* send a flush package to remote */
	if( pkg_send( MSG_FBFLUSH, (char *)0, 0, PCP(ifp) ) < 0 )
		return	-2;
	if( pkg_waitfor( MSG_RETURN, buf, NET_LONG_LEN, PCP(ifp) ) < 1*NET_LONG_LEN )
		return	-3;
	return( fbgetlong( &buf[0*NET_LONG_LEN] ) );
}

/*
 *			R E M _ H E L P
 */
_LOCAL_ int
rem_help(FBIO *ifp)
{
	char	buf[1*NET_LONG_LEN+1];

	fb_log( "Remote Interface:\n" );

	/* Send Command */
	(void)fbputlong( 0L, &buf[0*NET_LONG_LEN] );
	if( pkg_send( MSG_FBHELP, buf, 1*NET_LONG_LEN, PCP(ifp) ) < 1*NET_LONG_LEN )
		return	-2;
	if( pkg_waitfor( MSG_RETURN, buf, NET_LONG_LEN, PCP(ifp) ) < 1*NET_LONG_LEN )
		return	-3;
	return( fbgetlong( &buf[0*NET_LONG_LEN] ) );
}

/*
 *			P K G E R R O R
 *
 *  This is where we come on asynchronous error or log messages.
 *  We are counting on the remote machine now to prefix his own
 *  name to messages, so we don't touch them ourselves.
 */
static void
pkgerror(struct pkg_conn *pcpp, char *buf)
{
	fb_log( "%s", buf );
	free(buf);
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
