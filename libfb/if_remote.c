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
#endif

#ifdef BSD
#include <strings.h>
#else
#include <string.h>
#endif

#include "pkg.h"
#include "./pkgtypes.h"
#include "fb.h"
#include "./fblocal.h"

#define MAX_PIXELS_NET	1024
#define MAX_HOSTNAME	32
#define	PCP(ptr)	((struct pkg_conn *)((ptr)->u1.p))
#define	PCPL(ptr)	((ptr)->u1.p)	/* left hand side version */

/* Package Handlers. */
static int pkgerror();	/* error message handler */
static struct pkg_switch pkgswitch[] = {
	{ MSG_ERROR, pkgerror, "Error Message" },
	{ 0, NULL, NULL }
};

_LOCAL_ int	rem_dopen(),
		rem_dclose(),
		rem_dclear(),
		rem_bread(),
		rem_bwrite(),
		rem_cmread(),
		rem_cmwrite(),
		rem_window_set(),
		rem_zoom_set(),
		rem_cmemory_addr();

FBIO remote_interface =
		{
		rem_dopen,
		rem_dclose,
		fb_null,			/* reset		*/
		rem_dclear,
		rem_bread,
		rem_bwrite,
		rem_cmread,
		rem_cmwrite,
		fb_null,			/* viewport_set		*/
		rem_window_set,
		rem_zoom_set,
		fb_null,			/* cursor_init_bitmap	*/
		rem_cmemory_addr,
		fb_null,			/* cursor_move_screen_addr */
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
static	struct pkg_conn *pcp;

/* from getput.c */
extern unsigned short getshort();
extern unsigned long getlong();
extern char *putshort(), *putlong();

/*
 * Open a connection to the remote libfb.
 * We send 4 bytes of mode, 4 bytes of size, then the
 *  devname (or NULL if default).
 */
_LOCAL_ int
rem_dopen( ifp, devicename, width, height )
register FBIO	*ifp;
register char	*devicename;
int	width, height;
{
	register int	i;
	struct pkg_conn *pc;
	char	buf[128];
	char	*file;
	char	hostname[MAX_HOSTNAME];

	if( devicename == NULL || (file = strchr( devicename, ':' )) == NULL ) {
		fb_log( "remote_dopen: bad device name \"%s\"\n",
			devicename == NULL ? "(null)" : devicename );
		return	-1;
	}
	for( i = 0; devicename[i] != ':' && i < MAX_HOSTNAME; i++ )
		hostname[i] = devicename[i];
	hostname[i] = '\0';
	if( (pc = pkg_open( hostname, "mfb", pkgswitch, fb_log )) == PKC_ERROR ) {
		fb_log(	"remote_dopen: can't connect to host \"%s\".\n",
			hostname );
		return	-1;
	}
	PCPL(ifp) = (char *)pc;			/* stash in u1 */
	ifp->if_fd = pc->pkc_fd;
	ifp->if_width = width;
	ifp->if_height = height;

	(void)putlong( ifp->if_width, &buf[0] );
	(void)putlong( ifp->if_height, &buf[4] );
	(void) strcpy( &buf[8], file + 1 );
	pkg_send( MSG_FBOPEN, buf, strlen(devicename)+8, pc );

	/* XXX - need to get the size back! */
	pkg_waitfor( MSG_RETURN, buf, 4, pc );
	return( getlong( buf ) );
}

_LOCAL_ int
rem_dclose( ifp )
FBIO	*ifp;
{
	char	buf[4+1];

	/* send a close package to remote */
	pkg_send( MSG_FBCLOSE, 0L, 0L, PCP(ifp) );
	pkg_waitfor( MSG_RETURN, buf, 4, PCP(ifp) );
	pkg_close( PCP(ifp) );
	return( getlong( buf ) );
}

_LOCAL_ int
rem_dclear( ifp, bgpp )
FBIO	*ifp;
Pixel	*bgpp;
{
	char	buf[4+1];

	/* XXX - need to send background color */
	/* send a clear package to remote */
	pkg_send( MSG_FBCLEAR, 0L, 0L, PCP(ifp) );
	pkg_waitfor( MSG_RETURN, buf, 4, PCP(ifp) );
	return( getlong( buf ) );
}

/*
 *  Send as longs:  x, y, num
 */
_LOCAL_ int
rem_bread( ifp, x, y, pixelp, num )
FBIO	*ifp;
int	x, y;
Pixel	*pixelp;
int	num;
{
	int	ret;
	char	buf[3*4+1];

	/* Send Read Command */
	(void)putlong( x, &buf[0] );
	(void)putlong( y, &buf[4] );
	(void)putlong( num, &buf[8] );
	pkg_send( MSG_FBREAD, buf, 3*4, PCP(ifp) );

	/* Get return first, to see how much data there is */
	pkg_waitfor( MSG_RETURN, buf, 4, PCP(ifp) );
	ret = getlong( buf );

	/* Get Data */
	if( ret == 0 )
		pkg_waitfor( MSG_DATA, (char *) pixelp,	num*4, PCP(ifp) );
	else
		fb_log( "remote_bread: read at <%d,%d> failed.\n", x, y );
	return	ret;
}

/*
 * As longs, x, y, num
 */
_LOCAL_ int
rem_bwrite( ifp, x, y, pixelp, num )
FBIO	*ifp;
int	x, y;
Pixel	*pixelp;
int	num;
{
	int	ret;
	char	buf[3*4+1];

	/* Send Write Command */
	(void)putlong( x, &buf[0] );
	(void)putlong( y, &buf[4] );
	(void)putlong( num, &buf[8] );
	pkg_send( MSG_FBWRITE+MSG_NORETURN, buf, 3*4, PCP(ifp) );

	/* Send DATA */
	pkg_send( MSG_DATA, (char *)pixelp, num*4, PCP(ifp) );
#ifdef NEVER
	pkg_waitfor( MSG_RETURN, buf, 4, PCP(ifp) );
	ret = getlong( buf );
	return(ret);
#endif
	return	0;	/* No error return, sacrificed for speed.	*/
}

/*
 *  32-bit longs: mode, x, y
 */
_LOCAL_ int
rem_cmemory_addr( ifp, mode, x, y )
FBIO	*ifp;
int	mode;
int	x, y;
{
	char	buf[3*4+1];
	
	/* Send Command */
	(void)putlong( mode, &buf[0] );
	(void)putlong( x, &buf[4] );
	(void)putlong( y, &buf[8] );
	pkg_send( MSG_FBCURSOR, buf, 3*4, PCP(ifp) );
	pkg_waitfor( MSG_RETURN, buf, 4, PCP(ifp) );
	return( getlong( buf ) );
}

/*
 *	x,y
 */
_LOCAL_ int
rem_window_set( ifp, x, y )
FBIO	*ifp;
int	x, y;
{
	char	buf[3*4+1];
	
	/* Send Command */
	(void)putlong( x, &buf[0] );
	(void)putlong( y, &buf[4] );
	pkg_send( MSG_FBWINDOW, buf, 2*4, PCP(ifp) );
	pkg_waitfor( MSG_RETURN, buf, 4, PCP(ifp) );
	return( getlong( buf ) );
}

/*
 *	x,y
 */
_LOCAL_ int
rem_zoom_set( ifp, x, y )
FBIO	*ifp;
int	x, y;
{
	char	buf[3*4+1];

	/* Send Command */
	(void)putlong( x, &buf[0] );
	(void)putlong( y, &buf[4] );
	pkg_send( MSG_FBZOOM, buf, 2*4, PCP(ifp) );
	pkg_waitfor( MSG_RETURN, buf, 4, PCP(ifp) );
	return( getlong( buf ) );
}

_LOCAL_ int
rem_cmread( ifp, cmap )
FBIO	*ifp;
ColorMap	*cmap;
{
	char	buf[4+1];

	pkg_send( MSG_FBRMAP, 0, 0, PCP(ifp) );
	pkg_waitfor( MSG_DATA, cmap, sizeof(*cmap), PCP(ifp) );
	pkg_waitfor( MSG_RETURN, buf, 4, PCP(ifp) );
	return( getlong(buf) );
}

_LOCAL_ int
rem_cmwrite( ifp, cmap )
FBIO	*ifp;
ColorMap	*cmap;
{
	char	buf[4+1];

	if( cmap == COLORMAP_NULL )
		pkg_send( MSG_FBWMAP, 0, 0, PCP(ifp) );
	else
		pkg_send( MSG_FBWMAP, cmap, sizeof(*cmap), PCP(ifp) );
	pkg_waitfor( MSG_RETURN, buf, 4, PCP(ifp) );
	return( getlong(buf) );
}

/*
 * This is where we come on
 * asynchronous error messages.
 */
static int
pkgerror(pcp, buf)
struct pkg_conn *pcp;
char *buf;
{
	fb_log( "remote: %s", buf );
	(void)free(buf);
	return	0;	/* Declared as integer function in pkg_switch. */
}
#ifdef NEVER
/***** Experimental Queueing routines *****/

/*
 *  Format of the message header as it is transmitted over the network
 *  connection.  Internet network order is used.
 */
typedef struct mypkg_header {
	unsigned short	pkg_magic;		/* Ident */
	unsigned short	pkg_type;		/* Message Type */
	long		pkg_len;		/* Byte count of remainder */
	long		pkg_data;		/* start of data */
} mypkg_header;

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
pkg_queue( ifp, type, buf, len, pcp )
FBIO	*ifp;
int	type, len;
char	*buf;
struct	pkg_conn *pcp;
{
	mypkg_header	*pkg;

	if( q_len >= QUEUELEN || len > MAXQUEUE )
		flush_queue( ifp );
	if( len > MAXQUEUE ) {
		/* fb_log( "Too big to queue\n" ); */
		pkg_send( type, buf, len, pcp );
		return;
	}
/* fb_log( "Queueing packet\n" ); */

	/* Construct a packet */
	pkg = (mypkg_header *)malloc( sizeof(mypkg_header) + len - 4 );
	pkg->pkg_magic = htons(PKG_MAGIC);
	pkg->pkg_type = htons(type);	/* should see if it's a valid type */
	pkg->pkg_len = htonl(len);
	bcopy( buf, &pkg->pkg_data, len );

	/* Link it in */
	queue[q_len].iov_base = (char *)pkg;	
	queue[q_len].iov_len = sizeof(mypkg_header) + len - 4;
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
	raw_pkg_send( &queue[0], q_len, PCP(ifp) );

	/* Free bufs */
	for( i = 0; i < q_len; i++ )
		free( queue[i].iov_base );
	q_len = 0;
	return;
}

#define PKG_CK(p)	{if(p==PKC_NULL||p->pkc_magic!=PKG_MAGIC) {\
			fprintf(stderr,"pkg: bad pointer x%x\n",p);abort();}}

/*
 *  			R A W _ P K G _ S E N D
 *
 *  Send an iovec list of "raw packets", i.e. user is responsible
 *   for putting on magic headers, network byte order, etc.
 *
 *  Note that the whole message should be transmitted by
 *  TCP with only one TCP_PUSH at the end, due to the use of writev().
 *
 *  Returns number of bytes of user data actually sent.
 */
int
raw_pkg_send( buf, len, pc )
struct iovec buf[];
int len;
register struct pkg_conn *pc;
{
	long bits;
	register int i;
	int start;

	PKG_CK(pc);
	if( len < 0 )  len=0;

	do  {
		/* Finish any partially read message */
		if( pc->pkc_left > 0 )
			if( pkg_block(pc) < 0 )
				return(-1);
	} while( pc->pkc_left > 0 );

	/*
	 * TODO:  set this FD to NONBIO.  If not all output got sent,
	 * loop in select() waiting for capacity to go out, and
	 * reading input as well.  Prevents deadlocking.
	 */
	start = 0;
	while( len > 0 ) {
		if( (i = writev( pc->pkc_fd, &buf[start], (len>8?8:len) )) < 0 )  {
			fb_log( "pkg_send: writev failed.\n" );
			return(-1);
		}
		len -= 8;
		start += 8;
	}
	return(len);
}
#endif NEVER
