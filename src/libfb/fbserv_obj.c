/*                    F B S E R V _ O B J . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2014 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @addtogroup fb */
/** @{ */
/** @file fbserv_obj.c
 *
 * A framebuffer server object contains the attributes and
 * methods for implementing an fbserv. This code was developed
 * in large part by modifying the stand-alone version of fbserv.
 *
 */
/** @} */

#include "common.h"

#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#ifdef HAVE_WINSOCK_H
#  include <process.h>
#  include <winsock.h>
#else
#  include <sys/socket.h>
#  include <netinet/in.h>		/* For htonl(), etc. */
#endif
#include <tcl.h>
#include "bio.h"

#include "bu.h"
#include "vmath.h"
#include "raytrace.h"
#include "dm.h"

#include "fbmsg.h"
#include "fbserv_obj.h"


static FBIO *curr_fbp;		/* current framebuffer pointer */


/*
 * Communication error.  An error occurred on the PKG link.
 */
HIDDEN void
comm_error(const char *str)
{
    bu_log("%s", str);
}


HIDDEN void
drop_client(struct fbserv_obj *fbsp, int sub)
{
    if (fbsp->fbs_clients[sub].fbsc_pkg != PKC_NULL) {
	pkg_close(fbsp->fbs_clients[sub].fbsc_pkg);
	fbsp->fbs_clients[sub].fbsc_pkg = PKC_NULL;
    }

    if (fbsp->fbs_clients[sub].fbsc_fd != 0) {
#if defined(_WIN32) && !defined(__CYGWIN__)
	Tcl_DeleteChannelHandler(fbsp->fbs_clients[sub].fbsc_chan, fbsp->fbs_clients[sub].fbsc_handler, (ClientData)fbsp->fbs_clients[sub].fbsc_fd);

	Tcl_Close(fbsp->fbs_interp, fbsp->fbs_clients[sub].fbsc_chan);
	fbsp->fbs_clients[sub].fbsc_chan = NULL;
#else
	Tcl_DeleteFileHandler(fbsp->fbs_clients[sub].fbsc_fd);
#endif

	fbsp->fbs_clients[sub].fbsc_fd = 0;
    }
}


/*
 * Process arrivals from existing clients.
 */
HIDDEN void
existing_client_handler(ClientData clientData, int UNUSED(mask))
{
    register int i;
    struct fbserv_client *fbscp = (struct fbserv_client *)clientData;
    struct fbserv_obj *fbsp = fbscp->fbsc_fbsp;
    int fd = fbscp->fbsc_fd;

    curr_fbp = fbsp->fbs_fbp;

    for (i = MAX_CLIENTS - 1; i >= 0; i--) {
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

    if (fbsp->fbs_callback != (void (*)(genptr_t))FBS_CALLBACK_NULL) {
	/* need to cast func pointer explicitly to get the function call */
	void (*cfp)(void *);
	cfp = (void (*)(void *))fbsp->fbs_callback;
	cfp(fbsp->fbs_clientData);
    }
}


HIDDEN void
setup_socket(int fd)
{
    int on     = 1;
    int retval = 0;

#if defined(SO_KEEPALIVE)
    /* FIXME: better to show an error message but need thread considerations for strerror */
    if ((retval = setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, (char *)&on, sizeof(on))) < 0) {
	bu_log("setsockopt (SO_KEEPALIVE) error return: %d", retval);
    }
#endif
#if defined(SO_RCVBUF)
    /* try to set our buffers up larger */
    {
	int m = -1;
	int n = -1;
	int val;
	int size;

	for (size = 256; size > 16; size /= 2) {
	    val = size * 1024;
	    m = setsockopt(fd, SOL_SOCKET, SO_RCVBUF,
			   (char *)&val, sizeof(val));
	    val = size * 1024;
	    n = setsockopt(fd, SOL_SOCKET, SO_SNDBUF,
			   (char *)&val, sizeof(val));
	    if (m >= 0 && n >= 0) break;
	}

	if (m < 0 || n < 0)
	    bu_log("setup_socket: setsockopt()");
    }
#endif
}


#if defined(_WIN32) && !defined(__CYGWIN__)
HIDDEN void
new_client(struct fbserv_obj *fbsp, struct pkg_conn *pcp, Tcl_Channel chan)
{
    int i;

    if (pcp == PKC_ERROR)
	return;

    for (i = MAX_CLIENTS - 1; i >= 0; i--) {
	/* this slot is being used */
	if (fbsp->fbs_clients[i].fbsc_fd != 0)
	    continue;

	/* Found an available slot */
	fbsp->fbs_clients[i].fbsc_fd = pcp->pkc_fd;
	fbsp->fbs_clients[i].fbsc_pkg = pcp;
	fbsp->fbs_clients[i].fbsc_fbsp = fbsp;
	setup_socket(pcp->pkc_fd);

	fbsp->fbs_clients[i].fbsc_chan = chan;
	fbsp->fbs_clients[i].fbsc_handler = existing_client_handler;
	Tcl_CreateChannelHandler(fbsp->fbs_clients[i].fbsc_chan, TCL_READABLE,
				 fbsp->fbs_clients[i].fbsc_handler,
				 (ClientData)&fbsp->fbs_clients[i]);
	return;
    }

    bu_log("new_client: too many clients\n");
    pkg_close(pcp);
}

#else /* if defined(_WIN32) && !defined(__CYGWIN__) */
HIDDEN void
new_client(struct fbserv_obj *fbsp, struct pkg_conn *pcp, Tcl_Channel UNUSED(chan))
{
    int i;

    if (pcp == PKC_ERROR)
	return;

    for (i = MAX_CLIENTS - 1; i >= 0; i--) {
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
#endif /* if defined(_WIN32) && !defined(__CYGWIN__) */

#if defined(_WIN32) && !defined(__CYGWIN__)
HIDDEN struct pkg_conn *
fbs_makeconn(int fd, const struct pkg_switch *switchp)
{
    register struct pkg_conn *pc;
#ifdef HAVE_WINSOCK_H
    WSADATA wsaData;
    WORD wVersionRequested; /* initialize Windows socket networking,
			     * increment reference count.
			     */
#endif

    if ((pc = (struct pkg_conn *)malloc(sizeof(struct pkg_conn))) == PKC_NULL) {
	comm_error("fbs_makeconn: malloc failure\n");
	return PKC_ERROR;
    }

#ifdef HAVE_WINSOCK_H
    wVersionRequested = MAKEWORD(1, 1);
    if (WSAStartup(wVersionRequested, &wsaData) != 0) {
	comm_error("fbs_makeconn:  could not find a usable WinSock DLL\n");
	return PKC_ERROR;
    }
#endif

    memset((char *)pc, 0, sizeof(struct pkg_conn));
    pc->pkc_magic = PKG_MAGIC;
    pc->pkc_fd = fd;
    pc->pkc_switch = switchp;
    pc->pkc_errlog = 0;
    pc->pkc_left = -1;
    pc->pkc_buf = (char *)0;
    pc->pkc_curpos = (char *)0;
    pc->pkc_strpos = 0;
    pc->pkc_incur = pc->pkc_inend = 0;

    return pc;
}
#endif


/*
 * This is where we go for message types we don't understand.
 */
HIDDEN void
fbs_rfbunknown(struct pkg_conn *pcp, char *buf)
{
    bu_log("fbserv: unable to handle message type %d\n", pcp->pkc_type);
    if (buf) {
	(void)free(buf);
    }
}


/******** Here's where the hooks lead *********/

HIDDEN void
fbs_rfbopen(struct pkg_conn *pcp, char *buf)
{
    char rbuf[5*NET_LONG_LEN+1];
    int want;

    /* Don't really open a new framebuffer --- use existing one */
    (void)pkg_plong(&rbuf[0*NET_LONG_LEN], 0);	/* ret */
    (void)pkg_plong(&rbuf[1*NET_LONG_LEN], curr_fbp->if_max_width);
    (void)pkg_plong(&rbuf[2*NET_LONG_LEN], curr_fbp->if_max_height);
    (void)pkg_plong(&rbuf[3*NET_LONG_LEN], curr_fbp->if_width);
    (void)pkg_plong(&rbuf[4*NET_LONG_LEN], curr_fbp->if_height);

    want = 5*NET_LONG_LEN;
    if (pkg_send(MSG_RETURN, rbuf, want, pcp) != want)
	comm_error("pkg_send fb_open reply\n");

    if (buf) {
	(void)free(buf);
    }
}


void
fbs_rfbclose(struct pkg_conn *pcp, char *buf)
{
    char rbuf[NET_LONG_LEN+1];

    /*
     * We are playing FB server so we don't really close the frame
     * buffer.  We should flush output however.
     */
    (void)fb_flush(curr_fbp);
    (void)pkg_plong(&rbuf[0], 0);		/* return success */

    /* Don't check for errors, linger mode or other events may have
     * already closed down all the file descriptors.  If communication
     * has broken, other end will know we are gone.
     */
    (void)pkg_send(MSG_RETURN, rbuf, NET_LONG_LEN, pcp);

    if (buf) {
	(void)free(buf);
    }
}


void
fbs_rfbfree(struct pkg_conn *pcp, char *buf)
{
    char rbuf[NET_LONG_LEN+1];

    /* Don't really free framebuffer */
    if (pkg_send(MSG_RETURN, rbuf, NET_LONG_LEN, pcp) != NET_LONG_LEN)
	comm_error("pkg_send fb_free reply\n");

    if (buf) {
	(void)free(buf);
    }
}


void
fbs_rfbclear(struct pkg_conn *pcp, char *buf)
{
    RGBpixel bg;
    char rbuf[NET_LONG_LEN+1];

    if (!buf) {
	bu_log("fbs_rfbclear: null buffer\n");
	return;
    }

    bg[RED] = buf[0];
    bg[GRN] = buf[1];
    bg[BLU] = buf[2];

    (void)pkg_plong(rbuf, fb_clear(curr_fbp, bg));
    pkg_send(MSG_RETURN, rbuf, NET_LONG_LEN, pcp);

    (void)free(buf);
}


void
fbs_rfbread(struct pkg_conn *pcp, char *buf)
{
    int x, y;
    size_t num;
    int ret;
    static unsigned char *scanbuf = NULL;
    static size_t buflen = 0;

    if (!buf) {
	bu_log("fbs_rfbread: null buffer\n");
	return;
    }

    x = pkg_glong(&buf[0*NET_LONG_LEN]);
    y = pkg_glong(&buf[1*NET_LONG_LEN]);
    num = (size_t)pkg_glong(&buf[2*NET_LONG_LEN]);

    if (num*sizeof(RGBpixel) > buflen) {
	if (scanbuf != NULL)
	    free((char *)scanbuf);
	buflen = num*sizeof(RGBpixel);
	if (buflen < 1024*sizeof(RGBpixel))
	    buflen = 1024*sizeof(RGBpixel);
	if ((scanbuf = (unsigned char *)malloc(buflen)) == NULL) {
	    fb_log("fb_read: malloc failed!");
	    (void)free(buf);
	    buflen = 0;
	    return;
	}
    }

    ret = fb_read(curr_fbp, x, y, scanbuf, num);
    if (ret < 0) ret = 0;		/* map error indications */
    /* sending a 0-length package indicates error */
    pkg_send(MSG_RETURN, (char *)scanbuf, ret*sizeof(RGBpixel), pcp);
    (void)free(buf);
}


void
fbs_rfbwrite(struct pkg_conn *pcp, char *buf)
{
    long x, y, num;
    char rbuf[NET_LONG_LEN+1];
    int ret;
    int type;

    if (!buf) {
	bu_log("fbs_rfbwrite: null buffer\n");
	return;
    }

    x = pkg_glong(&buf[0*NET_LONG_LEN]);
    y = pkg_glong(&buf[1*NET_LONG_LEN]);
    num = pkg_glong(&buf[2*NET_LONG_LEN]);
    type = pcp->pkc_type;
    ret = fb_write(curr_fbp, x, y, (unsigned char *)&buf[3*NET_LONG_LEN], (size_t)num);

    if (type < MSG_NORETURN) {
	(void)pkg_plong(&rbuf[0*NET_LONG_LEN], ret);
	pkg_send(MSG_RETURN, rbuf, NET_LONG_LEN, pcp);
    }
    (void)free(buf);
}


void
fbs_rfbreadrect(struct pkg_conn *pcp, char *buf)
{
    int xmin, ymin;
    int width, height;
    size_t num;
    int ret;
    static unsigned char *scanbuf = NULL;
    static size_t buflen = 0;

    if (!buf) {
	bu_log("fbs_rfbreadrect: null buffer\n");
	return;
    }

    xmin = pkg_glong(&buf[0*NET_LONG_LEN]);
    ymin = pkg_glong(&buf[1*NET_LONG_LEN]);
    width = pkg_glong(&buf[2*NET_LONG_LEN]);
    height = pkg_glong(&buf[3*NET_LONG_LEN]);
    num = width * height;

    if (num*sizeof(RGBpixel) > buflen) {
	if (scanbuf != NULL)
	    free((char *)scanbuf);
	buflen = num*sizeof(RGBpixel);
	if (buflen < 1024*sizeof(RGBpixel))
	    buflen = 1024*sizeof(RGBpixel);
	if ((scanbuf = (unsigned char *)malloc(buflen)) == NULL) {
	    fb_log("fb_read: malloc failed!");
	    (void)free(buf);
	    buflen = 0;
	    return;
	}
    }

    ret = fb_readrect(curr_fbp, xmin, ymin, width, height, scanbuf);
    if (ret < 0) ret = 0;		/* map error indications */
    /* sending a 0-length package indicates error */
    pkg_send(MSG_RETURN, (char *)scanbuf, ret*sizeof(RGBpixel), pcp);
    (void)free(buf);
}


void
fbs_rfbwriterect(struct pkg_conn *pcp, char *buf)
{
    int x, y;
    int width, height;
    char rbuf[NET_LONG_LEN+1];
    int ret;
    int type;

    if (!buf) {
	bu_log("fbs_rfbwriterect: null buffer\n");
	return;
    }

    x = pkg_glong(&buf[0*NET_LONG_LEN]);
    y = pkg_glong(&buf[1*NET_LONG_LEN]);
    width = pkg_glong(&buf[2*NET_LONG_LEN]);
    height = pkg_glong(&buf[3*NET_LONG_LEN]);

    type = pcp->pkc_type;
    ret = fb_writerect(curr_fbp, x, y, width, height,
		       (unsigned char *)&buf[4*NET_LONG_LEN]);

    if (type < MSG_NORETURN) {
	(void)pkg_plong(&rbuf[0*NET_LONG_LEN], ret);
	pkg_send(MSG_RETURN, rbuf, NET_LONG_LEN, pcp);
    }
    (void)free(buf);
}


void
fbs_rfbbwreadrect(struct pkg_conn *pcp, char *buf)
{
    int xmin, ymin;
    int width, height;
    int num;
    int ret;
    static unsigned char *scanbuf = NULL;
    static int buflen = 0;

    if (!buf) {
	bu_log("fbs_rfbbwreadrect: null buffer\n");
	return;
    }

    xmin = pkg_glong(&buf[0*NET_LONG_LEN]);
    ymin = pkg_glong(&buf[1*NET_LONG_LEN]);
    width = pkg_glong(&buf[2*NET_LONG_LEN]);
    height = pkg_glong(&buf[3*NET_LONG_LEN]);
    num = width * height;

    if (num > buflen) {
	if (scanbuf != NULL)
	    free((char *)scanbuf);
	buflen = num;
	if (buflen < 1024)
	    buflen = 1024;
	if ((scanbuf = (unsigned char *)malloc(buflen)) == NULL) {
	    fb_log("fbs_rfbbwreadrect: malloc failed!");
	    (void)free(buf);
	    buflen = 0;
	    return;
	}
    }

    ret = fb_bwreadrect(curr_fbp, xmin, ymin, width, height, scanbuf);
    if (ret < 0) ret = 0;		/* map error indications */
    /* sending a 0-length package indicates error */
    pkg_send(MSG_RETURN, (char *)scanbuf, ret, pcp);
    (void)free(buf);
}


void
fbs_rfbbwwriterect(struct pkg_conn *pcp, char *buf)
{
    int x, y;
    int width, height;
    char rbuf[NET_LONG_LEN+1];
    int ret;
    int type;

    if (!buf) {
	bu_log("fbs_rfbbwwriterect: null buffer\n");
	return;
    }

    x = pkg_glong(&buf[0*NET_LONG_LEN]);
    y = pkg_glong(&buf[1*NET_LONG_LEN]);
    width = pkg_glong(&buf[2*NET_LONG_LEN]);
    height = pkg_glong(&buf[3*NET_LONG_LEN]);

    type = pcp->pkc_type;
    ret = fb_writerect(curr_fbp, x, y, width, height,
		       (unsigned char *)&buf[4*NET_LONG_LEN]);

    if (type < MSG_NORETURN) {
	(void)pkg_plong(&rbuf[0*NET_LONG_LEN], ret);
	pkg_send(MSG_RETURN, rbuf, NET_LONG_LEN, pcp);
    }
    (void)free(buf);
}


void
fbs_rfbcursor(struct pkg_conn *pcp, char *buf)
{
    int mode, x, y;
    char rbuf[NET_LONG_LEN+1];

    if (!buf) {
	bu_log("fbs_rfbcursor: null buffer\n");
	return;
    }

    mode = pkg_glong(&buf[0*NET_LONG_LEN]);
    x = pkg_glong(&buf[1*NET_LONG_LEN]);
    y = pkg_glong(&buf[2*NET_LONG_LEN]);

    (void)pkg_plong(&rbuf[0], fb_cursor(curr_fbp, mode, x, y));
    pkg_send(MSG_RETURN, rbuf, NET_LONG_LEN, pcp);
    (void)free(buf);
}


void
fbs_rfbgetcursor(struct pkg_conn *pcp, char *buf)
{
    int ret;
    int mode, x, y;
    char rbuf[4*NET_LONG_LEN+1];

    ret = fb_getcursor(curr_fbp, &mode, &x, &y);
    (void)pkg_plong(&rbuf[0*NET_LONG_LEN], ret);
    (void)pkg_plong(&rbuf[1*NET_LONG_LEN], mode);
    (void)pkg_plong(&rbuf[2*NET_LONG_LEN], x);
    (void)pkg_plong(&rbuf[3*NET_LONG_LEN], y);
    pkg_send(MSG_RETURN, rbuf, 4*NET_LONG_LEN, pcp);

    if (buf) {
	(void)free(buf);
    }
}


void
fbs_rfbsetcursor(struct pkg_conn *pcp, char *buf)
{
    char rbuf[NET_LONG_LEN+1];
    int ret;
    int xbits, ybits;
    int xorig, yorig;

    if (!buf) {
	bu_log("fbs_rfsetcursor: null buffer\n");
	return;
    }

    xbits = pkg_glong(&buf[0*NET_LONG_LEN]);
    ybits = pkg_glong(&buf[1*NET_LONG_LEN]);
    xorig = pkg_glong(&buf[2*NET_LONG_LEN]);
    yorig = pkg_glong(&buf[3*NET_LONG_LEN]);

    ret = fb_setcursor(curr_fbp, (unsigned char *)&buf[4*NET_LONG_LEN],
		       xbits, ybits, xorig, yorig);

    if (pcp->pkc_type < MSG_NORETURN) {
	(void)pkg_plong(&rbuf[0*NET_LONG_LEN], ret);
	pkg_send(MSG_RETURN, rbuf, NET_LONG_LEN, pcp);
    }
    (void)free(buf);
}


/*OLD*/
void
fbs_rfbscursor(struct pkg_conn *pcp, char *buf)
{
    int mode, x, y;
    char rbuf[NET_LONG_LEN+1];

    if (!buf) {
	bu_log("fbs_rfbscursor: null buffer\n");
	return;
    }

    mode = pkg_glong(&buf[0*NET_LONG_LEN]);
    x = pkg_glong(&buf[1*NET_LONG_LEN]);
    y = pkg_glong(&buf[2*NET_LONG_LEN]);

    (void)pkg_plong(&rbuf[0], fb_scursor(curr_fbp, mode, x, y));
    pkg_send(MSG_RETURN, rbuf, NET_LONG_LEN, pcp);
    (void)free(buf);
}


/*OLD*/
void
fbs_rfbwindow(struct pkg_conn *pcp, char *buf)
{
    int x, y;
    char rbuf[NET_LONG_LEN+1];

    if (!buf) {
	bu_log("fbs_rfbwindow: null buffer\n");
	return;
    }

    x = pkg_glong(&buf[0*NET_LONG_LEN]);
    y = pkg_glong(&buf[1*NET_LONG_LEN]);

    (void)pkg_plong(&rbuf[0], fb_window(curr_fbp, x, y));
    pkg_send(MSG_RETURN, rbuf, NET_LONG_LEN, pcp);

    (void)free(buf);
}


/*OLD*/
void
fbs_rfbzoom(struct pkg_conn *pcp, char *buf)
{
    int x, y;
    char rbuf[NET_LONG_LEN+1];

    if (!buf) {
	bu_log("fbs_rfbzoom: null buffer\n");
	return;
    }

    x = pkg_glong(&buf[0*NET_LONG_LEN]);
    y = pkg_glong(&buf[1*NET_LONG_LEN]);

    (void)pkg_plong(&rbuf[0], fb_zoom(curr_fbp, x, y));
    pkg_send(MSG_RETURN, rbuf, NET_LONG_LEN, pcp);
    (void)free(buf);
}


void
fbs_rfbview(struct pkg_conn *pcp, char *buf)
{
    int ret;
    int xcenter, ycenter, xzoom, yzoom;
    char rbuf[NET_LONG_LEN+1];

    if (!buf) {
	bu_log("fbs_rfbview: null buffer\n");
	return;
    }

    xcenter = pkg_glong(&buf[0*NET_LONG_LEN]);
    ycenter = pkg_glong(&buf[1*NET_LONG_LEN]);
    xzoom = pkg_glong(&buf[2*NET_LONG_LEN]);
    yzoom = pkg_glong(&buf[3*NET_LONG_LEN]);

    ret = fb_view(curr_fbp, xcenter, ycenter, xzoom, yzoom);
    (void)pkg_plong(&rbuf[0], ret);
    pkg_send(MSG_RETURN, rbuf, NET_LONG_LEN, pcp);
    (void)free(buf);
}


void
fbs_rfbgetview(struct pkg_conn *pcp, char *buf)
{
    int ret;
    int xcenter, ycenter, xzoom, yzoom;
    char rbuf[5*NET_LONG_LEN+1];

    ret = fb_getview(curr_fbp, &xcenter, &ycenter, &xzoom, &yzoom);
    (void)pkg_plong(&rbuf[0*NET_LONG_LEN], ret);
    (void)pkg_plong(&rbuf[1*NET_LONG_LEN], xcenter);
    (void)pkg_plong(&rbuf[2*NET_LONG_LEN], ycenter);
    (void)pkg_plong(&rbuf[3*NET_LONG_LEN], xzoom);
    (void)pkg_plong(&rbuf[4*NET_LONG_LEN], yzoom);
    pkg_send(MSG_RETURN, rbuf, 5*NET_LONG_LEN, pcp);

    if (buf) {
	(void)free(buf);
    }
}


void
fbs_rfbrmap(struct pkg_conn *pcp, char *buf)
{
    register int i;
    char rbuf[NET_LONG_LEN+1];
    ColorMap map;
    unsigned char cm[256*2*3];

    (void)pkg_plong(&rbuf[0*NET_LONG_LEN], fb_rmap(curr_fbp, &map));
    for (i = 0; i < 256; i++) {
	(void)pkg_pshort((char *)(cm+2*(0+i)), map.cm_red[i]);
	(void)pkg_pshort((char *)(cm+2*(256+i)), map.cm_green[i]);
	(void)pkg_pshort((char *)(cm+2*(512+i)), map.cm_blue[i]);
    }
    pkg_send(MSG_DATA, (char *)cm, sizeof(cm), pcp);
    pkg_send(MSG_RETURN, rbuf, NET_LONG_LEN, pcp);

    if (buf) {
	(void)free(buf);
    }
}


/*
 * Accept a color map sent by the client, and write it to the
 * framebuffer.  Network format is to send each entry as a network
 * (IBM) order 2-byte short, 256 red shorts, followed by 256 green and
 * 256 blue, for a total of 3*256*2 bytes.
 */
void
fbs_rfbwmap(struct pkg_conn *pcp, char *buf)
{
    int i;
    char rbuf[NET_LONG_LEN+1];
    long ret;
    ColorMap map;

    if (!buf) {
	bu_log("fbs_rfbwmap: null buffer\n");
	return;
    }

    if (pcp->pkc_len == 0) {
	ret = fb_wmap(curr_fbp, COLORMAP_NULL);
    } else {
	for (i = 0; i < 256; i++) {
	    map.cm_red[i] = pkg_gshort(buf+2*(0+i));
	    map.cm_green[i] = pkg_gshort(buf+2*(256+i));
	    map.cm_blue[i] = pkg_gshort(buf+2*(512+i));
	}
	ret = fb_wmap(curr_fbp, &map);
    }
    (void)pkg_plong(&rbuf[0], ret);
    pkg_send(MSG_RETURN, rbuf, NET_LONG_LEN, pcp);
    (void)free(buf);
}


void
fbs_rfbflush(struct pkg_conn *pcp, char *buf)
{
    int ret;
    char rbuf[NET_LONG_LEN+1];

    ret = fb_flush(curr_fbp);

    if (pcp->pkc_type < MSG_NORETURN) {
	(void)pkg_plong(rbuf, ret);
	pkg_send(MSG_RETURN, rbuf, NET_LONG_LEN, pcp);
    }

    if (buf) {
	(void)free(buf);
    }
}


void
fbs_rfbpoll(struct pkg_conn *pcp, char *buf)
{
    if (pcp == PKC_ERROR) {
	return;
    }

    (void)fb_poll(curr_fbp);
    if (buf) {
	(void)free(buf);
    }
}


/*
 * At one time at least we couldn't send a zero length PKG message
 * back and forth, so we receive a dummy long here.
 */
void
fbs_rfbhelp(struct pkg_conn *pcp, char *buf)
{
    long ret;
    char rbuf[NET_LONG_LEN+1];

    if (!buf) {
	bu_log("fbs_rfbhelp: null buffer\n");
	return;
    }

    (void)pkg_glong(&buf[0*NET_LONG_LEN]);

    ret = fb_help(curr_fbp);
    (void)pkg_plong(&rbuf[0], ret);
    pkg_send(MSG_RETURN, rbuf, NET_LONG_LEN, pcp);
    (void)free(buf);
}


/*
 * Accept any new client connections.
 */
#if defined(_WIN32) && !defined(__CYGWIN__)
HIDDEN void
new_client_handler(ClientData clientData,
		   Tcl_Channel chan,
		   char *UNUSED(host),
		   int UNUSED(port))
{
    struct fbserv_listener *fbslp = (struct fbserv_listener *)clientData;
    struct fbserv_obj *fbsp = fbslp->fbsl_fbsp;
    uintptr_t fd = (uintptr_t)fbslp->fbsl_fd;

    static struct pkg_switch pswitch[] = {
	{ MSG_FBOPEN, fbs_rfbopen, "Open Framebuffer", NULL },
	{ MSG_FBCLOSE, fbs_rfbclose, "Close Framebuffer", NULL },
	{ MSG_FBCLEAR, fbs_rfbclear, "Clear Framebuffer", NULL },
	{ MSG_FBREAD, fbs_rfbread, "Read Pixels", NULL },
	{ MSG_FBWRITE, fbs_rfbwrite, "Write Pixels", NULL },
	{ MSG_FBWRITE + MSG_NORETURN, fbs_rfbwrite, "Asynch write", NULL },
	{ MSG_FBCURSOR, fbs_rfbcursor, "Cursor", NULL },
	{ MSG_FBGETCURSOR, fbs_rfbgetcursor, "Get Cursor", NULL },  /*NEW*/
	{ MSG_FBSCURSOR, fbs_rfbscursor, "Screen Cursor", NULL }, /*OLD*/
	{ MSG_FBWINDOW, fbs_rfbwindow, "Window", NULL },  /*OLD*/
	{ MSG_FBZOOM, fbs_rfbzoom, "Zoom", NULL },  /*OLD*/
	{ MSG_FBVIEW, fbs_rfbview, "View", NULL },  /*NEW*/
	{ MSG_FBGETVIEW, fbs_rfbgetview, "Get View", NULL },  /*NEW*/
	{ MSG_FBRMAP, fbs_rfbrmap, "R Map", NULL },
	{ MSG_FBWMAP, fbs_rfbwmap, "W Map", NULL },
	{ MSG_FBHELP, fbs_rfbhelp, "Help Request", NULL },
	{ MSG_ERROR, fbs_rfbunknown, "Error Message", NULL },
	{ MSG_CLOSE, fbs_rfbunknown, "Close Connection", NULL },
	{ MSG_FBREADRECT, fbs_rfbreadrect, "Read Rectangle", NULL },
	{ MSG_FBWRITERECT, fbs_rfbwriterect, "Write Rectangle", NULL },
	{ MSG_FBWRITERECT + MSG_NORETURN, fbs_rfbwriterect, "Write Rectangle", NULL },
	{ MSG_FBBWREADRECT, fbs_rfbbwreadrect, "Read BW Rectangle", NULL },
	{ MSG_FBBWWRITERECT, fbs_rfbbwwriterect, "Write BW Rectangle", NULL },
	{ MSG_FBBWWRITERECT+MSG_NORETURN, fbs_rfbbwwriterect, "Write BW Rectangle", NULL },
	{ MSG_FBFLUSH, fbs_rfbflush, "Flush Output", NULL },
	{ MSG_FBFLUSH + MSG_NORETURN, fbs_rfbflush, "Flush Output", NULL },
	{ MSG_FBFREE, fbs_rfbfree, "Free Resources", NULL },
	{ MSG_FBPOLL, fbs_rfbpoll, "Handle Events", NULL },
	{ MSG_FBSETCURSOR, fbs_rfbsetcursor, "Set Cursor Shape", NULL },
	{ MSG_FBSETCURSOR + MSG_NORETURN, fbs_rfbsetcursor, "Set Cursor Shape", NULL },
	{ 0, NULL, NULL, NULL }
    };

    if (Tcl_GetChannelHandle(chan, TCL_READABLE, (ClientData *)&fd) == TCL_OK)
	new_client(fbsp, fbs_makeconn((int)fd, pswitch), chan);
}
#else /* if defined(_WIN32) && !defined(__CYGWIN__) */
HIDDEN void
new_client_handler(ClientData clientData,
		   Tcl_Channel UNUSED(chan),
		   char *UNUSED(host),
		   int UNUSED(port))
{
    struct fbserv_listener *fbslp = (struct fbserv_listener *)clientData;
    struct fbserv_obj *fbsp = fbslp->fbsl_fbsp;
    int fd = fbslp->fbsl_fd;

    static struct pkg_switch pswitch[] = {
	{ MSG_FBOPEN, fbs_rfbopen, "Open Framebuffer", NULL },
	{ MSG_FBCLOSE, fbs_rfbclose, "Close Framebuffer", NULL },
	{ MSG_FBCLEAR, fbs_rfbclear, "Clear Framebuffer", NULL },
	{ MSG_FBREAD, fbs_rfbread, "Read Pixels", NULL },
	{ MSG_FBWRITE, fbs_rfbwrite, "Write Pixels", NULL },
	{ MSG_FBWRITE + MSG_NORETURN, fbs_rfbwrite, "Asynch write", NULL },
	{ MSG_FBCURSOR, fbs_rfbcursor, "Cursor", NULL },
	{ MSG_FBGETCURSOR, fbs_rfbgetcursor, "Get Cursor", NULL },  /*NEW*/
	{ MSG_FBSCURSOR, fbs_rfbscursor, "Screen Cursor", NULL }, /*OLD*/
	{ MSG_FBWINDOW, fbs_rfbwindow, "Window", NULL },  /*OLD*/
	{ MSG_FBZOOM, fbs_rfbzoom, "Zoom", NULL },  /*OLD*/
	{ MSG_FBVIEW, fbs_rfbview, "View", NULL },  /*NEW*/
	{ MSG_FBGETVIEW, fbs_rfbgetview, "Get View", NULL },  /*NEW*/
	{ MSG_FBRMAP, fbs_rfbrmap, "R Map", NULL },
	{ MSG_FBWMAP, fbs_rfbwmap, "W Map", NULL },
	{ MSG_FBHELP, fbs_rfbhelp, "Help Request", NULL },
	{ MSG_ERROR, fbs_rfbunknown, "Error Message", NULL },
	{ MSG_CLOSE, fbs_rfbunknown, "Close Connection", NULL },
	{ MSG_FBREADRECT, fbs_rfbreadrect, "Read Rectangle", NULL },
	{ MSG_FBWRITERECT, fbs_rfbwriterect, "Write Rectangle", NULL },
	{ MSG_FBWRITERECT + MSG_NORETURN, fbs_rfbwriterect, "Write Rectangle", NULL },
	{ MSG_FBBWREADRECT, fbs_rfbbwreadrect, "Read BW Rectangle", NULL },
	{ MSG_FBBWWRITERECT, fbs_rfbbwwriterect, "Write BW Rectangle", NULL },
	{ MSG_FBBWWRITERECT+MSG_NORETURN, fbs_rfbbwwriterect, "Write BW Rectangle", NULL },
	{ MSG_FBFLUSH, fbs_rfbflush, "Flush Output", NULL },
	{ MSG_FBFLUSH + MSG_NORETURN, fbs_rfbflush, "Flush Output", NULL },
	{ MSG_FBFREE, fbs_rfbfree, "Free Resources", NULL },
	{ MSG_FBPOLL, fbs_rfbpoll, "Handle Events", NULL },
	{ MSG_FBSETCURSOR, fbs_rfbsetcursor, "Set Cursor Shape", NULL },
	{ MSG_FBSETCURSOR + MSG_NORETURN, fbs_rfbsetcursor, "Set Cursor Shape", NULL },
	{ 0, NULL, NULL, NULL }
    };

    new_client(fbsp, pkg_getclient(fd, pswitch, comm_error, 0), 0);
}
#endif /* if defined(_WIN32) && !defined(__CYGWIN__) */

int
fbs_open(struct fbserv_obj *fbsp, int port)
{
    int i;
    struct bu_vls vls = BU_VLS_INIT_ZERO;
    char hostname[32] = {0};
    Tcl_DString ds;
    int failed = 0;
    int available_port = port;

    /* Already listening; nothing more to do. */
#if defined(_WIN32) && !defined(__CYGWIN__)
    if (fbsp->fbs_listener.fbsl_chan != NULL) {
	return TCL_OK;
    }
#else /* if defined(_WIN32) && !defined(__CYGWIN__) */
    if (fbsp->fbs_listener.fbsl_fd >= 0) {
	return TCL_OK;
    }
#endif /* if defined(_WIN32) && !defined(__CYGWIN__) */

    /* XXX hardwired for now */
    sprintf(hostname, "localhost");

    if (available_port < 0) {
	available_port = 5559;
    } else if (available_port < 1024) {
	available_port += 5559;
    }

    Tcl_DStringInit(&ds);

    /* Try a reasonable number of times to hang a listen */
    for (i = 0; i < MAX_PORT_TRIES; ++i) {
	/*
	 * Hang an unending listen for PKG connections
	 */
#if defined(_WIN32) && !defined(__CYGWIN__)
	fbsp->fbs_listener.fbsl_chan = Tcl_OpenTcpServer(fbsp->fbs_interp, available_port, hostname, new_client_handler, (ClientData)&fbsp->fbs_listener);
	if (fbsp->fbs_listener.fbsl_chan == NULL) {
	    /* This clobbers the result string which probably has junk
	     * related to the failed open.
	     */
	    Tcl_DStringResult(fbsp->fbs_interp, &ds);
	} else {
	    break;
	}
#else /* if defined(_WIN32) && !defined(__CYGWIN__) */
	char portname[32] = {0};
	sprintf(portname, "%d", available_port);
	fbsp->fbs_listener.fbsl_fd = pkg_permserver(portname, 0, 0, comm_error);
	if (fbsp->fbs_listener.fbsl_fd >= 0)
	    break;
#endif /* if defined(_WIN32) && !defined(__CYGWIN__) */

	++available_port;
    }

#if defined(_WIN32) && !defined(__CYGWIN__)
    if (fbsp->fbs_listener.fbsl_chan == NULL) {
	failed = 1;
    }
#else /* if defined(_WIN32) && !defined(__CYGWIN__) */
    if (fbsp->fbs_listener.fbsl_fd < 0) {
	failed = 1;
    }
#endif /* if defined(_WIN32) && !defined(__CYGWIN__) */

    if (failed) {
	bu_vls_printf(&vls, "fbs_open: failed to hang a listen on ports %d - %d\n", port, available_port);
	Tcl_AppendResult(fbsp->fbs_interp, bu_vls_addr(&vls), (char *)NULL);
	bu_vls_free(&vls);

	fbsp->fbs_listener.fbsl_port = -1;

	return TCL_ERROR;
    }

    fbsp->fbs_listener.fbsl_port = available_port;

#if defined(_WIN32) && !defined(__CYGWIN__)
    Tcl_GetChannelHandle(fbsp->fbs_listener.fbsl_chan, TCL_READABLE, (ClientData *)&fbsp->fbs_listener.fbsl_fd);
#else /* if defined(_WIN32) && !defined(__CYGWIN__) */
    Tcl_CreateFileHandler(fbsp->fbs_listener.fbsl_fd, TCL_READABLE, (Tcl_FileProc *)new_client_handler, (ClientData)&fbsp->fbs_listener);
#endif /* if defined(_WIN32) && !defined(__CYGWIN__) */

    return TCL_OK;
}


int
fbs_close(struct fbserv_obj *fbsp)
{
    int i;

    /* first drop all clients */
    for (i = 0; i < MAX_CLIENTS; ++i)
	drop_client(fbsp, i);

#if defined(_WIN32) && !defined(__CYGWIN__)
    if (fbsp->fbs_listener.fbsl_chan != NULL) {
	Tcl_ChannelProc *callback = (Tcl_ChannelProc *)new_client_handler;
	Tcl_DeleteChannelHandler(fbsp->fbs_listener.fbsl_chan, callback, (ClientData)fbsp->fbs_listener.fbsl_fd);
	Tcl_Close(fbsp->fbs_interp, fbsp->fbs_listener.fbsl_chan);
	fbsp->fbs_listener.fbsl_chan = NULL;
    }
#else
    Tcl_DeleteFileHandler(fbsp->fbs_listener.fbsl_fd);
#endif

    if (0 <= fbsp->fbs_listener.fbsl_fd)
	close(fbsp->fbs_listener.fbsl_fd);
    fbsp->fbs_listener.fbsl_fd = -1;
    fbsp->fbs_listener.fbsl_port = -1;

    return TCL_OK;
}


/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
