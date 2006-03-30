/*                    F B S E R V _ O B J . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2006 United States Government as represented by
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
/** \addtogroup fb */
/*@{*/
/** @file fbserv_obj.c
 * A framebuffer server object contains the attributes and
 * methods for implementing an fbserv. This code was developed
 * in large part by modifying the stand-alone version of fbserv.
 *
 * Source -
 *	SLAD CAD Team
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005
 *
 * Author -
 *	Robert G. Parker
 *
 * Authors of fbserv -
 *	Phillip Dykstra
 *	Michael John Muuss
 *
 */
/*@}*/
#include "common.h"

#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>

#include <sys/socket.h>
#include <netinet/in.h>		/* For htonl(), etc */

#include "tcl.h"
#include "machine.h"
#include "bu.h"
#include "vmath.h"
#include "raytrace.h"
#include "dm.h"

#include "fbmsg.h"

int fbs_open(Tcl_Interp *interp, struct fbserv_obj *fbsp, int port);
int fbs_close(Tcl_Interp *interp, struct fbserv_obj *fbsp);

static void new_client(struct fbserv_obj *fbsp, struct pkg_conn *pcp);
static void drop_client(struct fbserv_obj *fbsp, int sub);
static void new_client_handler(ClientData clientData, int mask);
static void existing_client_handler(ClientData clientData, int mask);
static void comm_error(char *str);
static void setup_socket(int fd);

/*
 * Package Handlers.
 */
void	fbs_pkgfoo(struct pkg_conn *pcp, char *buf);	/* foobar message handler */
void	fbs_rfbopen(struct pkg_conn *pcp, char *buf), fbs_rfbclose(struct pkg_conn *pcp, char *buf), fbs_rfbclear(struct pkg_conn *pcp, char *buf), fbs_rfbread(struct pkg_conn *pcp, char *buf), fbs_rfbwrite(struct pkg_conn *pcp, char *buf);
void	fbs_rfbcursor(struct pkg_conn *pcp, char *buf), fbs_rfbgetcursor(struct pkg_conn *pcp, char *buf);
void	fbs_rfbrmap(struct pkg_conn *pcp, char *buf), fbs_rfbwmap(struct pkg_conn *pcp, char *buf);
void	fbs_rfbhelp(struct pkg_conn *pcp, char *buf);
void	fbs_rfbreadrect(struct pkg_conn *pcp, char *buf), fbs_rfbwriterect(struct pkg_conn *pcp, char *buf);
void	fbs_rfbbwreadrect(struct pkg_conn *pcp, char *buf), fbs_rfbbwwriterect(struct pkg_conn *pcp, char *buf);
void	fbs_rfbpoll(struct pkg_conn *pcp, char *buf), fbs_rfbflush(struct pkg_conn *pcp, char *buf), fbs_rfbfree(struct pkg_conn *pcp, char *buf);
void	fbs_rfbview(struct pkg_conn *pcp, char *buf), fbs_rfbgetview(struct pkg_conn *pcp, char *buf);
void	fbs_rfbsetcursor(struct pkg_conn *pcp, char *buf);
/* Old Routines */
void	fbs_rfbscursor(struct pkg_conn *pcp, char *buf), fbs_rfbwindow(struct pkg_conn *pcp, char *buf), fbs_rfbzoom(struct pkg_conn *pcp, char *buf);

static struct pkg_switch pkg_switch[] = {
	{ MSG_FBOPEN,		fbs_rfbopen,	"Open Framebuffer" },
	{ MSG_FBCLOSE,		fbs_rfbclose,	"Close Framebuffer" },
	{ MSG_FBCLEAR,		fbs_rfbclear,	"Clear Framebuffer" },
	{ MSG_FBREAD,		fbs_rfbread,	"Read Pixels" },
	{ MSG_FBWRITE,		fbs_rfbwrite,	"Write Pixels" },
	{ MSG_FBWRITE + MSG_NORETURN,	fbs_rfbwrite,	"Asynch write" },
	{ MSG_FBCURSOR,		fbs_rfbcursor,	"Cursor" },
	{ MSG_FBGETCURSOR,	fbs_rfbgetcursor,	"Get Cursor" },	   /*NEW*/
	{ MSG_FBSCURSOR,	fbs_rfbscursor,	"Screen Cursor" }, /*OLD*/
	{ MSG_FBWINDOW,		fbs_rfbwindow,	"Window" },	   /*OLD*/
	{ MSG_FBZOOM,		fbs_rfbzoom,	"Zoom" },	   /*OLD*/
	{ MSG_FBVIEW,		fbs_rfbview,	"View" },	   /*NEW*/
	{ MSG_FBGETVIEW,	fbs_rfbgetview,	"Get View" },	   /*NEW*/
	{ MSG_FBRMAP,		fbs_rfbrmap,	"R Map" },
	{ MSG_FBWMAP,		fbs_rfbwmap,	"W Map" },
	{ MSG_FBHELP,		fbs_rfbhelp,	"Help Request" },
	{ MSG_ERROR,		fbs_pkgfoo,		"Error Message" },
	{ MSG_CLOSE,		fbs_pkgfoo,		"Close Connection" },
	{ MSG_FBREADRECT, 	fbs_rfbreadrect,	"Read Rectangle" },
	{ MSG_FBWRITERECT,	fbs_rfbwriterect,	"Write Rectangle" },
	{ MSG_FBWRITERECT + MSG_NORETURN, fbs_rfbwriterect,"Write Rectangle" },
	{ MSG_FBBWREADRECT, 	fbs_rfbbwreadrect,"Read BW Rectangle" },
	{ MSG_FBBWWRITERECT,	fbs_rfbbwwriterect,"Write BW Rectangle" },
	{ MSG_FBBWWRITERECT+MSG_NORETURN, fbs_rfbbwwriterect,"Write BW Rectangle" },
	{ MSG_FBFLUSH,		fbs_rfbflush,	"Flush Output" },
	{ MSG_FBFLUSH + MSG_NORETURN, fbs_rfbflush, "Flush Output" },
	{ MSG_FBFREE,		fbs_rfbfree,	"Free Resources" },
	{ MSG_FBPOLL,		fbs_rfbpoll,	"Handle Events" },
	{ MSG_FBSETCURSOR,	fbs_rfbsetcursor,	"Set Cursor Shape" },
	{ MSG_FBSETCURSOR + MSG_NORETURN, fbs_rfbsetcursor, "Set Cursor Shape" },
	{ 0,			NULL,		NULL }
};

static FBIO *curr_fbp;		/* current framebuffer pointer */

int
fbs_open(Tcl_Interp *interp, struct fbserv_obj *fbsp, int port)
{
  struct bu_vls vls;
  char portname[32];
  register int i;

  /* Already listening; nothing more to do. */
  if (fbsp->fbs_listener.fbsl_fd >= 0) {
    return TCL_OK;
  }

  fbsp->fbs_listener.fbsl_port = port;

  /* Try a reasonable number of times to hang a listen */
  for (i = 0; i < MAX_PORT_TRIES; ++i) {
    if (fbsp->fbs_listener.fbsl_port < 1024)
      sprintf(portname,"%d", fbsp->fbs_listener.fbsl_port + 5559);
    else
      sprintf(portname,"%d", fbsp->fbs_listener.fbsl_port);

    /*
     * Hang an unending listen for PKG connections
     */
    if ((fbsp->fbs_listener.fbsl_fd = pkg_permserver(portname, 0, 0, comm_error)) < 0)
      ++fbsp->fbs_listener.fbsl_port;
    else
      break;
  }

  if (fbsp->fbs_listener.fbsl_fd < 0) {
    bu_vls_init(&vls);
    bu_vls_printf(&vls, "fbs_open: failed to hang a listen on ports %d - %d\n",
	   fbsp->fbs_listener.fbsl_port, fbsp->fbs_listener.fbsl_port + MAX_PORT_TRIES - 1);
    Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
    bu_vls_free(&vls);

    fbsp->fbs_listener.fbsl_port = -1;

    return TCL_ERROR;
  }

  Tcl_CreateFileHandler(fbsp->fbs_listener.fbsl_fd, TCL_READABLE,
			new_client_handler, (ClientData)&fbsp->fbs_listener);

  return TCL_OK;
}

int
fbs_close(Tcl_Interp *interp, struct fbserv_obj *fbsp)
{
  register int i;

  /* first drop all clients */
  for(i = 0; i < MAX_CLIENTS; ++i)
    drop_client(fbsp, i);

  Tcl_DeleteFileHandler(fbsp->fbs_listener.fbsl_fd);
  close(fbsp->fbs_listener.fbsl_fd);
  fbsp->fbs_listener.fbsl_fd = -1;
  fbsp->fbs_listener.fbsl_port = -1;

  return TCL_OK;
}

/*
 *			N E W _ C L I E N T
 */
static void
new_client(struct fbserv_obj *fbsp, struct pkg_conn *pcp)
{
  register int	i;

  if (pcp == PKC_ERROR)
    return;

  for (i = MAX_CLIENTS-1; i >= 0; i--) {
    /* this slot is being used */
    if (fbsp->fbs_clients[i].fbsc_fd != 0)
      continue;

    /* Found an available slot */
    fbsp->fbs_clients[i].fbsc_fd = pcp->pkc_fd;
    fbsp->fbs_clients[i].fbsc_pkg = pcp;
    fbsp->fbs_clients[i].fbsc_fbsp = fbsp;
    setup_socket(pcp->pkc_fd);

    Tcl_CreateFileHandler(fbsp->fbs_clients[i].fbsc_fd, TCL_READABLE,
			  existing_client_handler, (ClientData)&fbsp->fbs_clients[i]);

    return;
  }

  bu_log("new_client: too many clients\n");
  pkg_close(pcp);
}

/*
 *			D R O P _ C L I E N T
 */
static void
drop_client(struct fbserv_obj *fbsp, int sub)
{
  if(fbsp->fbs_clients[sub].fbsc_pkg != PKC_NULL)  {
    pkg_close(fbsp->fbs_clients[sub].fbsc_pkg);
    fbsp->fbs_clients[sub].fbsc_pkg = PKC_NULL;
  }

  if(fbsp->fbs_clients[sub].fbsc_fd != 0)  {
    Tcl_DeleteFileHandler(fbsp->fbs_clients[sub].fbsc_fd);
    close(fbsp->fbs_clients[sub].fbsc_fd);
    fbsp->fbs_clients[sub].fbsc_fd = 0;
  }
}

/*
 * Accept any new client connections.
 */
static void
new_client_handler(ClientData clientData, int mask)
{
  struct fbserv_listener *fbslp = (struct fbserv_listener *)clientData;
  struct fbserv_obj *fbsp = fbslp->fbsl_fbsp;
  int fd = fbslp->fbsl_fd;

  new_client(fbsp, pkg_getclient(fd, pkg_switch, comm_error, 0));
}

/*
 * Process arrivals from existing clients.
 */
static void
existing_client_handler(ClientData clientData, int mask)
{
  register int i;
  struct fbserv_client *fbscp = (struct fbserv_client *)clientData;
  struct fbserv_obj *fbsp = fbscp->fbsc_fbsp;
  int fd = fbscp->fbsc_fd;

  curr_fbp = fbsp->fbs_fbp;

  for (i = MAX_CLIENTS-1; i >= 0; i--) {
    if (fbsp->fbs_clients[i].fbsc_fd == 0)
      continue;

    if ((pkg_process(fbsp->fbs_clients[i].fbsc_pkg)) < 0)
      bu_log("pkg_process error encountered (1)\n");

    if (fbsp->fbs_clients[i].fbsc_fd != fd)
      continue;

    if (pkg_suckin(fbsp->fbs_clients[i].fbsc_pkg) <= 0) {
      /* Probably EOF */
      drop_client(fbsp, i);

      continue;
    }

    if ((pkg_process(fbsp->fbs_clients[i].fbsc_pkg)) < 0)
      bu_log("pkg_process error encountered (2)\n");
  }

  if (fbsp->fbs_callback != FBS_CALLBACK_NULL)
    fbsp->fbs_callback(fbsp->fbs_clientData);
}

static void
setup_socket(int fd)
{
  int on = 1;

#if defined(SO_KEEPALIVE)
  if(setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, (char *)&on, sizeof(on)) < 0){
    bu_log("setsockopt (SO_KEEPALIVE): %m");
  }
#endif
#if defined(SO_RCVBUF)
  /* try to set our buffers up larger */
  {
    int	m = -1;
    int n = -1;
    int	val;
    int	size;

    for(size = 256; size > 16; size /= 2){
      val = size * 1024;
      m = setsockopt(fd, SOL_SOCKET, SO_RCVBUF,
		      (char *)&val, sizeof(val));
      val = size * 1024;
      n = setsockopt(fd, SOL_SOCKET, SO_SNDBUF,
		      (char *)&val, sizeof(val));
      if(m >= 0 && n >= 0)  break;
    }

    if(m < 0 || n < 0)
      bu_log("setup_socket: setsockopt()");
  }
#endif
}

/*
 *			C O M M _ E R R O R
 *
 *  Communication error.  An error occured on the PKG link.
 */
static void
comm_error(char *str)
{
  bu_log(str);
}

/*
 * This is where we go for message types we don't understand.
 */
void
fbs_pkgfoo(struct pkg_conn *pcp, char *buf)
{
  bu_log("fbserv: unable to handle message type %d\n", pcp->pkc_type);
  (void)free(buf);
}

/******** Here's where the hooks lead *********/

void
fbs_rfbopen(struct pkg_conn *pcp, char *buf)
{
  char	rbuf[5*NET_LONG_LEN+1];
  int	want;

  /* Don't really open a new framebuffer --- use existing one */
  (void)pkg_plong(&rbuf[0*NET_LONG_LEN], 0);	/* ret */
  (void)pkg_plong(&rbuf[1*NET_LONG_LEN], curr_fbp->if_max_width);
  (void)pkg_plong(&rbuf[2*NET_LONG_LEN], curr_fbp->if_max_height);
  (void)pkg_plong(&rbuf[3*NET_LONG_LEN], curr_fbp->if_width);
  (void)pkg_plong(&rbuf[4*NET_LONG_LEN], curr_fbp->if_height);

  want = 5*NET_LONG_LEN;
  if( pkg_send( MSG_RETURN, rbuf, want, pcp ) != want )
    comm_error("pkg_send fb_open reply\n");

  if(buf)
    (void)free(buf);
}

void
fbs_rfbclose(struct pkg_conn *pcp, char *buf)
{
  char	rbuf[NET_LONG_LEN+1];

  /*
   * We are playing FB server so we don't really close the
   * frame buffer.  We should flush output however.
   */
  (void)fb_flush(curr_fbp);
  (void)pkg_plong(&rbuf[0], 0);		/* return success */

  /* Don't check for errors, SGI linger mode or other events
   * may have already closed down all the file descriptors.
   * If communication has broken, other end will know we are gone.
   */
  (void)pkg_send(MSG_RETURN, rbuf, NET_LONG_LEN, pcp);

  if(buf)
    (void)free(buf);
}

void
fbs_rfbfree(struct pkg_conn *pcp, char *buf)
{
  char	rbuf[NET_LONG_LEN+1];

  /* Don't really free framebuffer */
  if(pkg_send(MSG_RETURN, rbuf, NET_LONG_LEN, pcp) != NET_LONG_LEN)
    comm_error("pkg_send fb_free reply\n");

  if(buf)
    (void)free(buf);
}

void
fbs_rfbclear(struct pkg_conn *pcp, char *buf)
{
  RGBpixel bg;
  char	rbuf[NET_LONG_LEN+1];

  bg[RED] = buf[0];
  bg[GRN] = buf[1];
  bg[BLU] = buf[2];

  (void)pkg_plong(rbuf, fb_clear(curr_fbp, bg));
  pkg_send(MSG_RETURN, rbuf, NET_LONG_LEN, pcp);

  if(buf)
    (void)free(buf);
}

void
fbs_rfbread(struct pkg_conn *pcp, char *buf)
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

	ret = fb_read( curr_fbp, x, y, scanbuf, num );
	if( ret < 0 )  ret = 0;		/* map error indications */
	/* sending a 0-length package indicates error */
	pkg_send( MSG_RETURN, scanbuf, ret*sizeof(RGBpixel), pcp );
	if( buf ) (void)free(buf);
}

void
fbs_rfbwrite(struct pkg_conn *pcp, char *buf)
{
	int	x, y, num;
	char	rbuf[NET_LONG_LEN+1];
	int	ret;
	int	type;

	x = pkg_glong( &buf[0*NET_LONG_LEN] );
	y = pkg_glong( &buf[1*NET_LONG_LEN] );
	num = pkg_glong( &buf[2*NET_LONG_LEN] );
	type = pcp->pkc_type;
	ret = fb_write( curr_fbp, x, y, (unsigned char *)&buf[3*NET_LONG_LEN], num );

	if( type < MSG_NORETURN ) {
		(void)pkg_plong( &rbuf[0*NET_LONG_LEN], ret );
		pkg_send( MSG_RETURN, rbuf, NET_LONG_LEN, pcp );
	}
	if( buf ) (void)free(buf);
}

/*
 *			R F B R E A D R E C T
 */
void
fbs_rfbreadrect(struct pkg_conn *pcp, char *buf)
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

	ret = fb_readrect( curr_fbp, xmin, ymin, width, height, scanbuf );
	if( ret < 0 )  ret = 0;		/* map error indications */
	/* sending a 0-length package indicates error */
	pkg_send( MSG_RETURN, scanbuf, ret*sizeof(RGBpixel), pcp );
	if( buf ) (void)free(buf);
}

/*
 *			R F B W R I T E R E C T
 */
void
fbs_rfbwriterect(struct pkg_conn *pcp, char *buf)
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
	ret = fb_writerect( curr_fbp, x, y, width, height,
		(unsigned char *)&buf[4*NET_LONG_LEN] );

	if( type < MSG_NORETURN ) {
		(void)pkg_plong( &rbuf[0*NET_LONG_LEN], ret );
		pkg_send( MSG_RETURN, rbuf, NET_LONG_LEN, pcp );
	}
	if( buf ) (void)free(buf);
}

/*
 *			R F B B W R E A D R E C T
 */
void
fbs_rfbbwreadrect(struct pkg_conn *pcp, char *buf)
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
			fb_log("fbs_rfbbwreadrect: malloc failed!");
			if( buf ) (void)free(buf);
			buflen = 0;
			return;
		}
	}

	ret = fb_bwreadrect( curr_fbp, xmin, ymin, width, height, scanbuf );
	if( ret < 0 )  ret = 0;		/* map error indications */
	/* sending a 0-length package indicates error */
	pkg_send( MSG_RETURN, scanbuf, ret, pcp );
	if( buf ) (void)free(buf);
}

/*
 *			R F B B W W R I T E R E C T
 */
void
fbs_rfbbwwriterect(struct pkg_conn *pcp, char *buf)
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
	ret = fb_writerect( curr_fbp, x, y, width, height,
		(unsigned char *)&buf[4*NET_LONG_LEN] );

	if( type < MSG_NORETURN ) {
		(void)pkg_plong( &rbuf[0*NET_LONG_LEN], ret );
		pkg_send( MSG_RETURN, rbuf, NET_LONG_LEN, pcp );
	}
	if( buf ) (void)free(buf);
}

void
fbs_rfbcursor(struct pkg_conn *pcp, char *buf)
{
	int	mode, x, y;
	char	rbuf[NET_LONG_LEN+1];

	mode = pkg_glong( &buf[0*NET_LONG_LEN] );
	x = pkg_glong( &buf[1*NET_LONG_LEN] );
	y = pkg_glong( &buf[2*NET_LONG_LEN] );

	(void)pkg_plong( &rbuf[0], fb_cursor( curr_fbp, mode, x, y ) );
	pkg_send( MSG_RETURN, rbuf, NET_LONG_LEN, pcp );
	if( buf ) (void)free(buf);
}

void
fbs_rfbgetcursor(struct pkg_conn *pcp, char *buf)
{
	int	ret;
	int	mode, x, y;
	char	rbuf[4*NET_LONG_LEN+1];

	ret = fb_getcursor( curr_fbp, &mode, &x, &y );
	(void)pkg_plong( &rbuf[0*NET_LONG_LEN], ret );
	(void)pkg_plong( &rbuf[1*NET_LONG_LEN], mode );
	(void)pkg_plong( &rbuf[2*NET_LONG_LEN], x );
	(void)pkg_plong( &rbuf[3*NET_LONG_LEN], y );
	pkg_send( MSG_RETURN, rbuf, 4*NET_LONG_LEN, pcp );
	if( buf ) (void)free(buf);
}

void
fbs_rfbsetcursor(struct pkg_conn *pcp, char *buf)
{
	char	rbuf[NET_LONG_LEN+1];
	int	ret;
	int	xbits, ybits;
	int	xorig, yorig;

	xbits = pkg_glong( &buf[0*NET_LONG_LEN] );
	ybits = pkg_glong( &buf[1*NET_LONG_LEN] );
	xorig = pkg_glong( &buf[2*NET_LONG_LEN] );
	yorig = pkg_glong( &buf[3*NET_LONG_LEN] );

	ret = fb_setcursor( curr_fbp, (unsigned char *)&buf[4*NET_LONG_LEN],
		xbits, ybits, xorig, yorig );

	if( pcp->pkc_type < MSG_NORETURN ) {
		(void)pkg_plong( &rbuf[0*NET_LONG_LEN], ret );
		pkg_send( MSG_RETURN, rbuf, NET_LONG_LEN, pcp );
	}
	if( buf ) (void)free(buf);
}

/*OLD*/
void
fbs_rfbscursor(struct pkg_conn *pcp, char *buf)
{
	int	mode, x, y;
	char	rbuf[NET_LONG_LEN+1];

	mode = pkg_glong( &buf[0*NET_LONG_LEN] );
	x = pkg_glong( &buf[1*NET_LONG_LEN] );
	y = pkg_glong( &buf[2*NET_LONG_LEN] );

	(void)pkg_plong( &rbuf[0], fb_scursor( curr_fbp, mode, x, y ) );
	pkg_send( MSG_RETURN, rbuf, NET_LONG_LEN, pcp );
	if( buf ) (void)free(buf);
}

/*OLD*/
void
fbs_rfbwindow(struct pkg_conn *pcp, char *buf)
{
	int	x, y;
	char	rbuf[NET_LONG_LEN+1];

	x = pkg_glong( &buf[0*NET_LONG_LEN] );
	y = pkg_glong( &buf[1*NET_LONG_LEN] );

	(void)pkg_plong( &rbuf[0], fb_window( curr_fbp, x, y ) );
	pkg_send( MSG_RETURN, rbuf, NET_LONG_LEN, pcp );
	if( buf ) (void)free(buf);
}

/*OLD*/
void
fbs_rfbzoom(struct pkg_conn *pcp, char *buf)
{
	int	x, y;
	char	rbuf[NET_LONG_LEN+1];

	x = pkg_glong( &buf[0*NET_LONG_LEN] );
	y = pkg_glong( &buf[1*NET_LONG_LEN] );

	(void)pkg_plong( &rbuf[0], fb_zoom( curr_fbp, x, y ) );
	pkg_send( MSG_RETURN, rbuf, NET_LONG_LEN, pcp );
	if( buf ) (void)free(buf);
}

void
fbs_rfbview(struct pkg_conn *pcp, char *buf)
{
	int	ret;
	int	xcenter, ycenter, xzoom, yzoom;
	char	rbuf[NET_LONG_LEN+1];

	xcenter = pkg_glong( &buf[0*NET_LONG_LEN] );
	ycenter = pkg_glong( &buf[1*NET_LONG_LEN] );
	xzoom = pkg_glong( &buf[2*NET_LONG_LEN] );
	yzoom = pkg_glong( &buf[3*NET_LONG_LEN] );

	ret = fb_view( curr_fbp, xcenter, ycenter, xzoom, yzoom );
	(void)pkg_plong( &rbuf[0], ret );
	pkg_send( MSG_RETURN, rbuf, NET_LONG_LEN, pcp );
	if( buf ) (void)free(buf);
}

void
fbs_rfbgetview(struct pkg_conn *pcp, char *buf)
{
	int	ret;
	int	xcenter, ycenter, xzoom, yzoom;
	char	rbuf[5*NET_LONG_LEN+1];

	ret = fb_getview( curr_fbp, &xcenter, &ycenter, &xzoom, &yzoom );
	(void)pkg_plong( &rbuf[0*NET_LONG_LEN], ret );
	(void)pkg_plong( &rbuf[1*NET_LONG_LEN], xcenter );
	(void)pkg_plong( &rbuf[2*NET_LONG_LEN], ycenter );
	(void)pkg_plong( &rbuf[3*NET_LONG_LEN], xzoom );
	(void)pkg_plong( &rbuf[4*NET_LONG_LEN], yzoom );
	pkg_send( MSG_RETURN, rbuf, 5*NET_LONG_LEN, pcp );
	if( buf ) (void)free(buf);
}

void
fbs_rfbrmap(struct pkg_conn *pcp, char *buf)
{
	register int	i;
	char	rbuf[NET_LONG_LEN+1];
	ColorMap map;
	unsigned char	cm[256*2*3];

	(void)pkg_plong( &rbuf[0*NET_LONG_LEN], fb_rmap( curr_fbp, &map ) );
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
 *			R F B W M A P
 *
 *  Accept a color map sent by the client, and write it to the framebuffer.
 *  Network format is to send each entry as a network (IBM) order 2-byte
 *  short, 256 red shorts, followed by 256 green and 256 blue, for a total
 *  of 3*256*2 bytes.
 */
void
fbs_rfbwmap(struct pkg_conn *pcp, char *buf)
{
	int	i;
	char	rbuf[NET_LONG_LEN+1];
	long	ret;
	ColorMap map;

	if( pcp->pkc_len == 0 )
		ret = fb_wmap( curr_fbp, COLORMAP_NULL );
	else {
		for( i = 0; i < 256; i++ ) {
			map.cm_red[i] = pkg_gshort( buf+2*(0+i) );
			map.cm_green[i] = pkg_gshort( buf+2*(256+i) );
			map.cm_blue[i] = pkg_gshort( buf+2*(512+i) );
		}
		ret = fb_wmap( curr_fbp, &map );
	}
	(void)pkg_plong( &rbuf[0], ret );
	pkg_send( MSG_RETURN, rbuf, NET_LONG_LEN, pcp );
	if( buf ) (void)free(buf);
}

void
fbs_rfbflush(struct pkg_conn *pcp, char *buf)
{
	int	ret;
	char	rbuf[NET_LONG_LEN+1];

	ret = fb_flush( curr_fbp );

	if( pcp->pkc_type < MSG_NORETURN ) {
		(void)pkg_plong( rbuf, ret );
		pkg_send( MSG_RETURN, rbuf, NET_LONG_LEN, pcp );
	}
	if( buf ) (void)free(buf);
}

void
fbs_rfbpoll(struct pkg_conn *pcp, char *buf)
{
	(void)fb_poll( curr_fbp );
	if( buf ) (void)free(buf);
}

/*
 *  At one time at least we couldn't send a zero length PKG
 *  message back and forth, so we receive a dummy long here.
 */
void
fbs_rfbhelp(struct pkg_conn *pcp, char *buf)
{
	long	ret;
	char	rbuf[NET_LONG_LEN+1];

	(void)pkg_glong( &buf[0*NET_LONG_LEN] );

	ret = fb_help(curr_fbp);
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
