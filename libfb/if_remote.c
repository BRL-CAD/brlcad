/*
 *		I F _ R E M O T E . C
 *
 *  Remote libfb interface.
 *
 *  Duplicates the functions in libfb via communication
 *  with a remote server (rfbd).
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
 *  Copyright Notice -
 *	This software is Copyright (C) 1986 by the United States Army.
 *	All rights reserved.
 *
 *	$Header$ (BRL)
 */
#ifndef lint
static char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include <stdio.h>

#ifdef BSD
#include <sys/types.h>
#include <sys/uio.h>		/* for struct iovec */
#include <netinet/in.h>		/* for htons(), etc */
#include <netdb.h>		/* for gethostbyname() stuff */
#endif

#ifdef BSD
#include <strings.h>
#else
#include <string.h>
#endif

#include "./pkg.h"
#include "./pkgtypes.h"
#include "fb.h"
#include "./fblocal.h"

#define MAX_PIXELS_NET	1024
#define MAX_HOSTNAME	32

_LOCAL_ int	remote_device_open(),
		remote_device_close(),
		remote_device_clear(),
		remote_buffer_read(),
		remote_buffer_write(),
		remote_colormap_read(),
		remote_colormap_write(),
		remote_window_set(),
		remote_zoom_set(),
		remote_cmemory_addr();

FBIO remote_interface =
		{
		remote_device_open,
		remote_device_close,
		fb_null,				/* reset		*/
		remote_device_clear,
		remote_buffer_read,
		remote_buffer_write,
		remote_colormap_read,
		remote_colormap_write,
		fb_null,				/* viewport_set		*/
		remote_window_set,
		remote_zoom_set,
		fb_null,				/* cursor_init_bitmap	*/
		remote_cmemory_addr,
		fb_null,				/* cursor_move_screen_addr */
		"Remote Device Interface",	/* should be filled in	*/
		1024,				/* " */
		1024,				/* " */
		"*remote*",
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

void	pkg_queue(), flush_queue();

/*
 * Open a connection to the remote libfb.
 * We send 4 bytes of mode, 4 bytes of size, then the
 *  devname (or NULL if default).
 */
_LOCAL_ int
remote_device_open( ifp, devicename, width, height )
register FBIO	*ifp;
register char	*devicename;
int	width, height;
{
	register int	i;
	char		buf[24];
	char		*file;
	char		hostname[MAX_HOSTNAME];
	char		official_hostname[MAX_HOSTNAME];
	long		*lp;
	int		ret;
	struct hostent	*hostentry;
	extern struct hostent	*gethostbyname();

	if( devicename == NULL || (file = strchr( devicename, ':' )) == NULL ) {
		fb_log( "remote_device_open : bad device name \"%s\"\n",
			devicename == NULL ? "(null)" : devicename );
		return	-1;
	}
	for( i = 0; devicename[i] != ':' && i < MAX_HOSTNAME; i++ )
		hostname[i] = devicename[i];
	hostname[i] = '\0';
	if( (hostentry = gethostbyname( hostname )) == NULL ) {
		fb_log(	"remote_device_open : host not found \"%s\".\n",
			hostname );
		return	-1;
	}
	(void) strncpy( official_hostname, hostentry->h_name, MAX_HOSTNAME );
	if( (ifp->if_fd = pkg_open( official_hostname, "mossfb" )) < 0 ) {
		fb_log(	"remote_device_open : can't connect to host \"%s\".\n",
			official_hostname );
		return	-1;
	}
	ifp->if_width = width;
	ifp->if_height = height;
	lp = (long *)&buf[0];
	*lp++ = htonl( ifp->if_width );
	*lp = htonl( ifp->if_height );

	(void) strcpy( &buf[8], file + 1 );
	pkg_send( MSG_FBOPEN, buf, strlen(devicename)+8, ifp->if_fd );

	/* XXX - need to get the size back! */
	pkg_waitfor( MSG_RETURN, &ret, 4, ifp->if_fd );
	ret = ntohl( ret );
	if( ret < 0 )
		fb_log( "remote_device_open : device \"%s\" busy.\n", devicename );
	return	ntohl( ret );
}

_LOCAL_ int
remote_device_close( ifp )
FBIO	*ifp;
{
	int	ret;

	/* send a close package to remote */
	pkg_send( MSG_CLOSE, 0L, 0L, ifp->if_fd );
	pkg_waitfor( MSG_RETURN, &ret, 4, ifp->if_fd );
	pkg_close( ifp->if_fd );
	return	ntohl( ret );
}

_LOCAL_ int
remote_device_clear( ifp, bgpp )
FBIO	*ifp;
Pixel	*bgpp;
{
	int	ret;

	/* XXX - need to send background color */
	/* send a clear package to remote */
	pkg_send( MSG_FBCLEAR, 0L, 0L, ifp->if_fd );
	pkg_waitfor( MSG_RETURN, &ret, 4, ifp->if_fd );
	return	ntohl( ret );
}

_LOCAL_ int
remote_buffer_read( ifp, x, y, pixelp, num )
FBIO	*ifp;
int	x, y;
Pixel	*pixelp;
int	num;
{
	int	ret;
	struct	{
		int	x;
		int	y;
		int	num;
	} cmd;

	/* Send Read Command */
	cmd.x = htonl( x );
	cmd.y = htonl( y );
	cmd.num = htonl( num );
	pkg_send( MSG_FBREAD, &cmd, sizeof(cmd), ifp->if_fd );

	/* Get return first, to see how much data there is */
	pkg_waitfor( MSG_RETURN, &ret, 4, ifp->if_fd );
	ret = ntohl( ret );

	/* Get Data */
	if( ret == 0 )
		pkg_waitfor( MSG_DATA, (char *) pixelp,	num*4, ifp->if_fd );
	else
		fb_log( "remote_buffer_read : read at <%d,%d> failed.\n", x, y );
	return	ret;
}

_LOCAL_ int
remote_buffer_write( ifp, x, y, pixelp, num )
FBIO	*ifp;
int		x, y;
Pixel		*pixelp;
int		num;
{
	int	ret;
	struct	{
		int	x;
		int	y;
		int	num;
	} cmd;

	/* Send Write Command */
	cmd.x = htonl( x );
	cmd.y = htonl( y );
	cmd.num = htonl( num );
	pkg_queue( ifp,	MSG_FBWRITE+MSG_NORETURN, &cmd,	sizeof(cmd), ifp->if_fd );

	/* Send DATA */
	pkg_queue( ifp,	MSG_DATA, (char *) pixelp, num*4, ifp->if_fd );
#ifdef NEVER
	pkg_waitfor( MSG_RETURN, &ret, 4, ifp->if_fd );
	return	ntohl( ret );
#endif
	return	0;	/* No error return, sacrificed for speed.	*/
}

_LOCAL_ int
remote_cmemory_addr( ifp, mode, x, y )
FBIO	*ifp;
int	mode;
int	x, y;
{
	int	ret;
	struct	{
		int	mode;
		int	x;
		int	y;
	} cmd;
	
	/* Send Command */
	cmd.mode = htonl( mode );
	cmd.x = htonl( x );
	cmd.y = htonl( y );
	pkg_send( MSG_FBCURSOR, &cmd, sizeof(cmd), ifp->if_fd );
	pkg_waitfor( MSG_RETURN, &ret, 4, ifp->if_fd );
	return	ntohl( ret );
}

_LOCAL_ int
remote_window_set( ifp, x, y )
FBIO	*ifp;
int	x, y;
{
	int	ret;
	struct	{
		int	x;
		int	y;
	} cmd;
	
	/* Send Command */
	cmd.x = htonl( x );
	cmd.y = htonl( y );
	pkg_send( MSG_FBWINDOW, &cmd, sizeof(cmd), ifp->if_fd );
	pkg_waitfor( MSG_RETURN, &ret, 4, ifp->if_fd );
	return	ntohl( ret );
}

_LOCAL_ int
remote_zoom_set( ifp, x, y )
FBIO	*ifp;
int	x, y;
{
	int	ret;
	struct	{
		int	x;
		int	y;
	} cmd;

	/* Send Command */
	cmd.x = htonl( x );
	cmd.y = htonl( y );
	pkg_send( MSG_FBZOOM, &cmd, sizeof(cmd), ifp->if_fd );
	pkg_waitfor( MSG_RETURN, &ret, 4, ifp->if_fd );
	return	ntohl( ret );
}

_LOCAL_ int
remote_colormap_read( ifp, cmap )
FBIO	*ifp;
ColorMap	*cmap;
{
	int	ret;

	pkg_send( MSG_FBRMAP, 0, 0, ifp->if_fd );
	pkg_waitfor( MSG_DATA, cmap, sizeof(*cmap), ifp->if_fd );
	pkg_waitfor( MSG_RETURN, &ret, 4, ifp->if_fd );
	return	ntohl( ret );
}

_LOCAL_ int
remote_colormap_write( ifp, cmap )
FBIO	*ifp;
ColorMap	*cmap;
{
	int	ret;

	if( cmap == COLORMAP_NULL )
		pkg_send( MSG_FBWMAP, 0, 0, ifp->if_fd );
	else
		pkg_send( MSG_FBWMAP, cmap, sizeof(*cmap), ifp->if_fd );
	pkg_waitfor( MSG_RETURN, &ret, 4, ifp->if_fd );
	return	ntohl( ret );
}

/*
 * Called from pkgswitch.c, this is where we come on
 * error messages.
 */
int
pkgerror(type, buf, length)
int type, length;
char *buf;
{
	/* Dangerous, but such is life */
	buf[length] = '\0';

	fb_log( "PKGFOO: Type %d, \"%s\"\n", type, buf );
	return	0; /* Declared as integer function in pkg_switch.	*/
}

/***** Experimental Queueing routines *****/

/*
 *  Format of the message header as it is transmitted over the network
 *  connection.  Internet network order is used.
 */
#define PKG_MAGIC	0x41FE
typedef struct pkg_header {
	unsigned short	pkg_magic;		/* Ident */
	unsigned short	pkg_type;		/* Message Type */
	long		pkg_len;		/* Byte count of remainder */
	long		pkg_data;		/* start of data */
} pkg_header;

#define	MAXQUEUE	128	/* Largest packet we will queue */
#define	QUEUELEN	16	/* max entries in queue */

struct iovec queue[QUEUELEN];
int q_len = 0;

/*
 *	P K G _ Q U E U E
 *
 * NB: I am ignoring the fd!
 */
static void
pkg_queue( ifp, type, buf, len, fd )
FBIO	*ifp;
int	type, len, fd;
char	*buf;
{
	pkg_header	*pkg;

	if( q_len >= QUEUELEN || len > MAXQUEUE )
		flush_queue( ifp );
	if( len > MAXQUEUE ) {
		/* fb_log( "Too big to queue\n" ); */
		pkg_send( type, buf, len, fd );
		return;
	}
/* fb_log( "Queueing packet\n" ); */

	/* Construct a packet */
	pkg = (pkg_header *)malloc( sizeof(pkg_header) + len - 4 );
	pkg->pkg_magic = htons(PKG_MAGIC);
	pkg->pkg_type = htons(type);	/* should see if it's a valid type */
	pkg->pkg_len = htonl(len);
	bcopy( buf, &pkg->pkg_data, len );

	/* Link it in */
	queue[q_len].iov_base = (char *)pkg;	
	queue[q_len].iov_len = sizeof(pkg_header) + len - 4;
	q_len++;
	return;
}

/*
 *	F L U S H _ Q U E U E 
 *
 * If there are packages on the queue, send them.
 */
static void
flush_queue( ifp )
FBIO	*ifp;
{
	int	i;

	if( q_len == 0 )
		return;
	/* fb_log( "Flushing queued data\n" ); */
	raw_pkg_send( &queue[0], q_len, ifp->if_fd );

	/* Free bufs */
	for( i = 0; i < q_len; i++ )
		free( queue[i].iov_base );
	q_len = 0;
	return;
}
