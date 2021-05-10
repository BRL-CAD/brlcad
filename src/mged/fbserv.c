/*                        F B S E R V . C
 * BRL-CAD
 *
 * Copyright (c) 1995-2021 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file mged/fbserv.c
 *
 * This code was developed by modifying the stand-alone version of
 * fbserv to work within MGED.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <ctype.h>

#include "bio.h"
#include "bnetwork.h"
#include "bsocket.h"

#include "tcl.h"
#include "vmath.h"
#include "raytrace.h"

#include "./mged.h"
#include "./mged_dm.h"

#define NET_LONG_LEN 4 /* # bytes to network long */

extern const struct pkg_switch pkg_switch[];

/*
 * Communication error.  An error occurred on the PKG link.
 */
static void
communications_error(const char *str)
{
    bu_log("%s", str);
}


static void
fbserv_setup_socket(int fd)
{
    int on = 1;

#if defined(SO_KEEPALIVE)
    if (setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, (char *)&on, sizeof(on)) < 0) {
	perror("setsockopt (SO_KEEPALIVE)");
    }
#endif
#if defined(SO_RCVBUF)
    /* try to set our buffers up larger */
    {
	int m = -1, n = -1;
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
	    perror("setsockopt");
    }
#endif
}


static void
fbserv_drop_client(int sub)
{
    if (clients[sub].c_pkg != PKC_NULL) {
	pkg_close(clients[sub].c_pkg);
#if defined(_WIN32) && !defined(__CYGWIN__)
	Tcl_DeleteChannelHandler(clients[sub].c_chan,
				 clients[sub].c_handler,
				 (ClientData)clients[sub].c_fd);

	if (dm_interp(DMP) != NULL) {
	    Tcl_Close((Tcl_Interp *)dm_interp(DMP), clients[sub].c_chan);
	}
	clients[sub].c_chan = NULL;
#else
	Tcl_DeleteFileHandler(clients[sub].c_fd);
#endif
	clients[sub].c_pkg = PKC_NULL;
	clients[sub].c_fd = 0;
    }
}


/*
 * Process arrivals from existing clients.
 */
static void
fbserv_existing_client_handler(ClientData clientData, int UNUSED(mask))
{
    int i;
    int fd = (int)((long)clientData & 0xFFFF);	/* fd's will be small */
    int npp;			/* number of processed packages */
    struct mged_dm *dlp = MGED_DM_NULL;
    struct mged_dm *scdlp;  /* save current dm_list pointer */

    for (size_t di = 0; di < BU_PTBL_LEN(&active_dm_set); di++) {
	struct mged_dm *m_dmp = (struct mged_dm *)BU_PTBL_GET(&active_dm_set, di);
	for (i = MAX_CLIENTS-1; i >= 0; i--)
	    if (fd == m_dmp->dm_clients[i].c_fd) {
		dlp = m_dmp;
		goto found;
	    }
    }

    return;

 found:
    /* save */
    scdlp = mged_curr_dm;

    set_curr_dm(dlp);
    for (i = MAX_CLIENTS-1; i >= 0; i--) {
	if (clients[i].c_fd == 0)
	    continue;

	if ((npp = pkg_process(clients[i].c_pkg)) < 0)
	    bu_log("pkg_process error encountered (1)\n");

	if (npp > 0) {
	    DMP_dirty = 1;
	    dm_set_dirty(DMP, 1);
	}

	if (clients[i].c_fd != fd)
	    continue;

	if (pkg_suckin(clients[i].c_pkg) <= 0) {
	    /* Probably EOF */
	    fbserv_drop_client(i);

	    continue;
	}

	if ((npp = pkg_process(clients[i].c_pkg)) < 0)
	    bu_log("pkg_process error encountered (2)\n");

	if (npp > 0) {
	    DMP_dirty = 1;
	    dm_set_dirty(DMP, 1);
	}
    }

    /* restore */
    set_curr_dm(scdlp);
}


#if defined(_WIN32) && !defined(__CYGWIN__)
static struct pkg_conn *fbserv_makeconn(int fd, const struct pkg_switch *switchp);

static void
fbserv_new_client(struct pkg_conn *pcp,
		  Tcl_Channel chan)

{
    int i;

    if (pcp == PKC_ERROR)
	return;

    for (i = MAX_CLIENTS-1; i >= 0; i--) {
	ClientData fd;
	if (clients[i].c_fd != 0)
	    continue;

	/* Found an available slot */
	clients[i].c_pkg = pcp;
	clients[i].c_fd = pcp->pkc_fd;
	fbserv_setup_socket(pcp->pkc_fd);

	clients[i].c_chan = chan;
	clients[i].c_handler = fbserv_existing_client_handler;
	fd = (ClientData)clients[i].c_fd;
	Tcl_CreateChannelHandler(clients[i].c_chan, TCL_READABLE, clients[i].c_handler, fd);

	return;
    }

    bu_log("fbserv_new_client: too many clients\n");
    pkg_close(pcp);
}


static void
fbserv_new_client_handler(ClientData clientData,
			  Tcl_Channel chan,
			  char *host,
			  int port)
{
    struct mged_dm *dlp = (struct mged_dm *)clientData;
    struct mged_dm *scdlp;  /* save current dm_list pointer */
    uintptr_t fd;

    if (dlp == NULL)
	return;

    /* save */
    scdlp = mged_curr_dm;

    set_curr_dm(dlp);

    if (Tcl_GetChannelHandle(chan, TCL_READABLE, (ClientData *)&fd) == TCL_OK)
	fbserv_new_client(fbserv_makeconn((int)fd, pkg_switch), chan);

    /* restore */
    set_curr_dm(scdlp);
}


void
fbserv_set_port(const struct bu_structparse *UNUSED(sp), const char *UNUSED(c1), void *UNUSED(v1), const char *UNUSED(c2), void *UNUSED(v2))
{
    int i;
    int save_port;
    int port;
    char hostname[32];

    /* Check to see if previously active --- if so then deactivate */
    if (netchan != NULL) {
	ClientData fd;

	/* first drop all clients */
	for (i = 0; i < MAX_CLIENTS; ++i)
	    fbserv_drop_client(i);

	fd = (ClientData)netfd;
	Tcl_DeleteChannelHandler(netchan, (Tcl_ChannelProc *)fbserv_new_client_handler, fd);

	if (dm_interp(DMP) != NULL) {
	    Tcl_Close((Tcl_Interp *)dm_interp(DMP), netchan);
	}
	netchan = NULL;

	closesocket(netfd);
	netfd = -1;
    }

    if (!mged_variables->mv_listen)
	return;

    if (!mged_variables->mv_fb) {
	mged_variables->mv_listen = 0;
	return;
    }

    /*XXX hardwired for now */
    sprintf(hostname, "localhost");

#define MAX_PORT_TRIES 100

    save_port = mged_variables->mv_port;

    if (mged_variables->mv_port < 0)
	port = 5559;
    else if (mged_variables->mv_port < 1024)
	port = mged_variables->mv_port + 5559;
    else
	port = mged_variables->mv_port;

    /* Try a reasonable number of times to hang a listen */
    for (i = 0; i < MAX_PORT_TRIES; ++i) {
	/*
	 * Hang an unending listen for PKG connections
	 */

	if (dm_interp(DMP) != NULL) {
	    netchan = Tcl_OpenTcpServer((Tcl_Interp *)dm_interp(DMP), port, hostname, fbserv_new_client_handler, (ClientData)mged_curr_dm);
	}

	if (netchan == NULL)
	    ++port;
	else
	    break;
    }

    if (netchan == NULL) {
	mged_variables->mv_port = save_port;
	mged_variables->mv_listen = 0;
	bu_log("fbserv_set_port: failed to hang a listen on ports %d - %d\n",
	       mged_variables->mv_port, mged_variables->mv_port + MAX_PORT_TRIES - 1);
    } else {
	mged_variables->mv_port = port;
	Tcl_GetChannelHandle(netchan, TCL_READABLE, (ClientData *)&netfd);
    }
}


static struct pkg_conn *
fbserv_makeconn(int fd,
		const struct pkg_switch *switchp)
{
    struct pkg_conn *pc;
#ifdef HAVE_WINSOCK_H
    WORD wVersionRequested;		/* initialize Windows socket networking, increment reference count */
    WSADATA wsaData;
#endif

    if ((pc = (struct pkg_conn *)malloc(sizeof(struct pkg_conn))) == PKC_NULL) {
	communications_error("fbserv_makeconn: malloc failure\n");
	return PKC_ERROR;
    }

#ifdef HAVE_WINSOCK_H
    wVersionRequested = MAKEWORD(1, 1);
    if (WSAStartup(wVersionRequested, &wsaData) != 0) {
	communications_error("fbserv_makeconn:  could not find a usable WinSock DLL\n");
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


#else /* defined(_WIN32) && !defined(__CYGWIN__) */


static void
fbserv_new_client(struct pkg_conn *pcp)
{
    int i;

    if (pcp == PKC_ERROR)
	return;

    for (i = MAX_CLIENTS-1; i >= 0; i--) {
	if (clients[i].c_fd != 0)
	    continue;

	/* Found an available slot */
	clients[i].c_pkg = pcp;
	clients[i].c_fd = pcp->pkc_fd;
	fbserv_setup_socket(pcp->pkc_fd);

	Tcl_CreateFileHandler(clients[i].c_fd, TCL_READABLE,
			      fbserv_existing_client_handler, (ClientData)(size_t)clients[i].c_fd);

	return;
    }

    bu_log("fbserv_new_client: too many clients\n");
    pkg_close(pcp);
}


/*
 * Accept any new client connections.
 */
static void
fbserv_new_client_handler(ClientData clientData, int UNUSED(mask))
{
    uintptr_t datafd = (uintptr_t)clientData;
    int fd = (int)((int32_t)datafd & 0xFFFF);	/* fd's will be small */
    struct mged_dm *dlp;
    struct mged_dm *scdlp;  /* save current dm_list pointer */

    for (size_t di = 0; di < BU_PTBL_LEN(&active_dm_set); di++) {
	struct mged_dm *m_dmp = (struct mged_dm *)BU_PTBL_GET(&active_dm_set, di);
	if (fd == m_dmp->dm_netfd) {
	    dlp = m_dmp;
	    goto found;
	}
    }

    return;

 found:
    /* save */
    scdlp = mged_curr_dm;

    set_curr_dm(dlp);
    fbserv_new_client(pkg_getclient(fd, pkg_switch, communications_error, 0));

    /* restore */
    set_curr_dm(scdlp);
}


void
fbserv_set_port(const struct bu_structparse *UNUSED(sp), const char *UNUSED(c1), void *UNUSED(v1), const char *UNUSED(c2), void *UNUSED(v2))
{
    int i;
    int save_port;
    char portname[32];

    /* Check to see if previously active --- if so then deactivate */
    if (netfd >= 0) {
	/* first drop all clients */
	for (i = 0; i < MAX_CLIENTS; ++i)
	    fbserv_drop_client(i);

	Tcl_DeleteFileHandler(netfd);
	close(netfd);
	netfd = -1;
    }

    if (!mged_variables->mv_listen)
	return;

    if (!mged_variables->mv_fb) {
	mged_variables->mv_listen = 0;
	return;
    }

#define MAX_PORT_TRIES 100

    save_port = mged_variables->mv_port;
    if (mged_variables->mv_port < 0)
	mged_variables->mv_port = 0;

    /* Try a reasonable number of times to hang a listen */
    for (i = 0; i < MAX_PORT_TRIES; ++i) {
	if (mged_variables->mv_port < 1024)
	    sprintf(portname, "%d", mged_variables->mv_port + 5559);
	else
	    sprintf(portname, "%d", mged_variables->mv_port);

	/*
	 * Hang an unending listen for PKG connections
	 */
	if ((netfd = pkg_permserver(portname, 0, 0, communications_error)) < 0)
	    ++mged_variables->mv_port;
	else
	    break;
    }

    if (netfd < 0) {
	mged_variables->mv_port = save_port;
	mged_variables->mv_listen = 0;
	bu_log("fbserv_set_port: failed to hang a listen on ports %d - %d\n",
	       mged_variables->mv_port, mged_variables->mv_port + MAX_PORT_TRIES - 1);
    } else
	Tcl_CreateFileHandler(netfd, TCL_READABLE,
			      fbserv_new_client_handler, (ClientData)(size_t)netfd);
}
#endif  /* if defined(_WIN32) && !defined(__CYGWIN__) */


/*
 * This is where we go for message types we don't understand.
 */
void
fb_server_fb_unknown(struct pkg_conn *pcp, char *buf)
{
    if (buf == NULL) {
	bu_log("fb_server_fb_unknown: null buffer\n");
	return;
    }

    bu_log("fbserv: unable to handle message type %d\n", pcp->pkc_type);
    (void)free(buf);
}


/******** Here's where the hooks lead *********/

static void
fb_server_fb_open(struct pkg_conn *pcp, char *buf)
{
    char rbuf[5*NET_LONG_LEN+1];
    int want;

    if (buf == NULL) {
	bu_log("fb_server_fb_open: null buffer\n");
	return;
    }

    /* Don't really open a new framebuffer --- use existing one */
    (void)pkg_plong(&rbuf[0*NET_LONG_LEN], 0);	/* ret */
    (void)pkg_plong(&rbuf[1*NET_LONG_LEN], fb_get_max_width(fbp));
    (void)pkg_plong(&rbuf[2*NET_LONG_LEN], fb_get_max_height(fbp));
    (void)pkg_plong(&rbuf[3*NET_LONG_LEN], fb_getwidth(fbp));
    (void)pkg_plong(&rbuf[4*NET_LONG_LEN], fb_getheight(fbp));

    want = 5*NET_LONG_LEN;
    if (pkg_send(MSG_RETURN, rbuf, want, pcp) != want)
	communications_error("pkg_send fb_open reply\n");

    (void)free(buf);
}


static void
fb_server_fb_close(struct pkg_conn *pcp, char *buf)
{
    char rbuf[NET_LONG_LEN+1];

    /*
     * We are playing FB server so we don't really close the frame
     * buffer.  We should flush output however.
     */
    (void)fb_flush(fbp);
    (void)pkg_plong(&rbuf[0], 0);		/* return success */

    /* Don't check for errors, SGI linger mode or other events may
     * have already closed down all the file descriptors.  If
     * communication has broken, other end will know we are gone.
     */
    (void)pkg_send(MSG_RETURN, rbuf, NET_LONG_LEN, pcp);
    if (buf)
	(void)free(buf);
}


static void
fb_server_fb_free(struct pkg_conn *pcp, char *buf)
{
    char rbuf[NET_LONG_LEN+1];

    /* Don't really free framebuffer */
    if (pkg_send(MSG_RETURN, rbuf, NET_LONG_LEN, pcp) != NET_LONG_LEN)
	communications_error("pkg_send fb_free reply\n");

    if (buf)
	(void)free(buf);
}


static void
fb_server_fb_clear(struct pkg_conn *pcp, char *buf)
{
    RGBpixel bg;
    char rbuf[NET_LONG_LEN+1];

    if (buf == NULL) {
	bu_log("fb_server_fb_window: null buffer\n");
	return;
    }

    bg[RED] = buf[0];
    bg[GRN] = buf[1];
    bg[BLU] = buf[2];

    (void)pkg_plong(rbuf, fb_clear(fbp, bg));
    pkg_send(MSG_RETURN, rbuf, NET_LONG_LEN, pcp);

    if (buf)
	(void)free(buf);
}


static void
fb_server_fb_read(struct pkg_conn *pcp, char *buf)
{
    int x, y;
    size_t num;
    int ret;
    static unsigned char *scanbuf = NULL;
    static size_t buflen = 0;

    if (buf == NULL) {
	bu_log("fb_server_fb_readrect: null buffer\n");
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
	    if (buf)
		(void)free(buf);
	    buflen = 0;
	    return;
	}
    }

    ret = fb_read(fbp, x, y, scanbuf, num);
    if (ret < 0) ret = 0;		/* map error indications */
    /* sending a 0-length package indicates error */
    pkg_send(MSG_RETURN, (char *)scanbuf, ret*sizeof(RGBpixel), pcp);
    if (buf)
	(void)free(buf);
}


static void
fb_server_fb_write(struct pkg_conn *pcp, char *buf)
{
    int x, y, num;
    char rbuf[NET_LONG_LEN+1];
    int ret;
    int type;

    if (buf == NULL) {
	bu_log("fb_server_fb_readrect: null buffer\n");
	return;
    }

    x = pkg_glong(&buf[0*NET_LONG_LEN]);
    y = pkg_glong(&buf[1*NET_LONG_LEN]);
    num = pkg_glong(&buf[2*NET_LONG_LEN]);
    type = pcp->pkc_type;
    ret = fb_write(fbp, x, y, (unsigned char *)&buf[3*NET_LONG_LEN], num);

    if (type < MSG_NORETURN) {
	(void)pkg_plong(&rbuf[0*NET_LONG_LEN], ret);
	pkg_send(MSG_RETURN, rbuf, NET_LONG_LEN, pcp);
    }
    if (buf)
	(void)free(buf);
}


static void
fb_server_fb_readrect(struct pkg_conn *pcp, char *buf)
{
    int xmin, ymin;
    int width, height;
    size_t num;
    int ret;
    static unsigned char *scanbuf = NULL;
    static size_t buflen = 0;

    if (buf == NULL) {
	bu_log("fb_server_fb_readrect: null buffer\n");
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
	    if (buf)
		(void)free(buf);
	    buflen = 0;
	    return;
	}
    }

    ret = fb_readrect(fbp, xmin, ymin, width, height, scanbuf);
    if (ret < 0) ret = 0;		/* map error indications */
    /* sending a 0-length package indicates error */
    pkg_send(MSG_RETURN, (char *)scanbuf, ret*sizeof(RGBpixel), pcp);
    if (buf)
	(void)free(buf);
}


static void
fb_server_fb_writerect(struct pkg_conn *pcp, char *buf)
{
    int x, y;
    int width, height;
    char rbuf[NET_LONG_LEN+1];
    int ret;
    int type;

    if (buf == NULL) {
	bu_log("fb_server_fb_readrect: null buffer\n");
	return;
    }

    x = pkg_glong(&buf[0*NET_LONG_LEN]);
    y = pkg_glong(&buf[1*NET_LONG_LEN]);
    width = pkg_glong(&buf[2*NET_LONG_LEN]);
    height = pkg_glong(&buf[3*NET_LONG_LEN]);

    type = pcp->pkc_type;
    ret = fb_writerect(fbp, x, y, width, height,
		       (unsigned char *)&buf[4*NET_LONG_LEN]);

    if (type < MSG_NORETURN) {
	(void)pkg_plong(&rbuf[0*NET_LONG_LEN], ret);
	pkg_send(MSG_RETURN, rbuf, NET_LONG_LEN, pcp);
    }
    if (buf)
	(void)free(buf);
}


static void
fb_server_fb_bwreadrect(struct pkg_conn *pcp, char *buf)
{
    int xmin, ymin;
    int width, height;
    int num;
    int ret;
    static unsigned char *scanbuf = NULL;
    static int buflen = 0;

    if (buf == NULL) {
	bu_log("fb_server_fb_bwreadrect: null buffer\n");
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
	    fb_log("fb_server_fb_bwreadrect: malloc failed!");
	    (void)free(buf);
	    buflen = 0;
	    return;
	}
    }

    ret = fb_bwreadrect(fbp, xmin, ymin, width, height, scanbuf);
    if (ret < 0) ret = 0;		/* map error indications */
    /* sending a 0-length package indicates error */
    pkg_send(MSG_RETURN, (char *)scanbuf, ret, pcp);
    (void)free(buf);
}


/*
 * A whole rectangle of monochrome pixels at once, probably large.
 */
static void
fb_server_fb_bwwriterect(struct pkg_conn *pcp, char *buf)
{
    int x, y;
    int width, height;
    char rbuf[NET_LONG_LEN+1];
    int ret;
    int type;

    if (buf == NULL) {
	bu_log("rfbbwwriterect: null buffer\n");
	return;
    }

    x = pkg_glong(&buf[0*NET_LONG_LEN]);
    y = pkg_glong(&buf[1*NET_LONG_LEN]);
    width = pkg_glong(&buf[2*NET_LONG_LEN]);
    height = pkg_glong(&buf[3*NET_LONG_LEN]);

    type = pcp->pkc_type;
    ret = fb_writerect(fbp, x, y, width, height,
		       (unsigned char *)&buf[4*NET_LONG_LEN]);

    if (type < MSG_NORETURN) {
	(void)pkg_plong(&rbuf[0*NET_LONG_LEN], ret);
	pkg_send(MSG_RETURN, rbuf, NET_LONG_LEN, pcp);
    }
    if (buf)
	(void)free(buf);
}


static void
fb_server_fb_cursor(struct pkg_conn *pcp, char *buf)
{
    int mode, x, y;
    char rbuf[NET_LONG_LEN+1];

    if (buf == NULL) {
	bu_log("fb_server_fb_window: null buffer\n");
	return;
    }

    mode = pkg_glong(&buf[0*NET_LONG_LEN]);
    x = pkg_glong(&buf[1*NET_LONG_LEN]);
    y = pkg_glong(&buf[2*NET_LONG_LEN]);

    (void)pkg_plong(&rbuf[0], fb_cursor(fbp, mode, x, y));
    pkg_send(MSG_RETURN, rbuf, NET_LONG_LEN, pcp);
    if (buf)
	(void)free(buf);
}


static void
fb_server_fb_getcursor(struct pkg_conn *pcp, char *buf)
{
    int ret;
    int mode, x, y;
    char rbuf[4*NET_LONG_LEN+1];

    ret = fb_getcursor(fbp, &mode, &x, &y);
    (void)pkg_plong(&rbuf[0*NET_LONG_LEN], ret);
    (void)pkg_plong(&rbuf[1*NET_LONG_LEN], mode);
    (void)pkg_plong(&rbuf[2*NET_LONG_LEN], x);
    (void)pkg_plong(&rbuf[3*NET_LONG_LEN], y);
    pkg_send(MSG_RETURN, rbuf, 4*NET_LONG_LEN, pcp);
    if (buf)
	(void)free(buf);
}


static void
fb_server_fb_setcursor(struct pkg_conn *pcp, char *buf)
{
    char rbuf[NET_LONG_LEN+1];
    int ret;
    int xbits, ybits;
    int xorig, yorig;

    if (buf == NULL) {
	bu_log("fb_server_fb_readrect: null buffer\n");
	return;
    }

    xbits = pkg_glong(&buf[0*NET_LONG_LEN]);
    ybits = pkg_glong(&buf[1*NET_LONG_LEN]);
    xorig = pkg_glong(&buf[2*NET_LONG_LEN]);
    yorig = pkg_glong(&buf[3*NET_LONG_LEN]);

    ret = fb_setcursor(fbp, (unsigned char *)&buf[4*NET_LONG_LEN],
		       xbits, ybits, xorig, yorig);

    if (pcp->pkc_type < MSG_NORETURN) {
	(void)pkg_plong(&rbuf[0*NET_LONG_LEN], ret);
	pkg_send(MSG_RETURN, rbuf, NET_LONG_LEN, pcp);
    }
    if (buf)
	(void)free(buf);
}


/*
 * An OLD interface.  Retained so old clients can still be served.
 */
static void
fb_server_fb_scursor(struct pkg_conn *pcp, char *buf)
{
    int mode, x, y;
    char rbuf[NET_LONG_LEN+1];

    if (buf == NULL) {
	bu_log("fb_server_fb_open: null buffer\n");
	return;
    }

    mode = pkg_glong(&buf[0*NET_LONG_LEN]);
    x = pkg_glong(&buf[1*NET_LONG_LEN]);
    y = pkg_glong(&buf[2*NET_LONG_LEN]);

    (void)pkg_plong(&rbuf[0], fb_scursor(fbp, mode, x, y));
    pkg_send(MSG_RETURN, rbuf, NET_LONG_LEN, pcp);
    (void)free(buf);
}


/*
 * An OLD interface.  Retained so old clients can still be served.
 */
static void
fb_server_fb_window(struct pkg_conn *pcp, char *buf)
{
    int x, y;
    char rbuf[NET_LONG_LEN+1];

    if (buf == NULL) {
	bu_log("fb_server_fb_window: null buffer\n");
	return;
    }

    x = pkg_glong(&buf[0*NET_LONG_LEN]);
    y = pkg_glong(&buf[1*NET_LONG_LEN]);

    (void)pkg_plong(&rbuf[0], fb_window(fbp, x, y));
    pkg_send(MSG_RETURN, rbuf, NET_LONG_LEN, pcp);
    if (buf)
	(void)free(buf);
}


/*
 * An OLD interface.  Retained so old clients can still be served.
 */
static void
fb_server_fb_zoom(struct pkg_conn *pcp, char *buf)
{
    int x, y;
    char rbuf[NET_LONG_LEN+1];

    if (buf == NULL) {
	bu_log("fb_server_fb_readrect: null buffer\n");
	return;
    }

    x = pkg_glong(&buf[0*NET_LONG_LEN]);
    y = pkg_glong(&buf[1*NET_LONG_LEN]);

    (void)pkg_plong(&rbuf[0], fb_zoom(fbp, x, y));
    pkg_send(MSG_RETURN, rbuf, NET_LONG_LEN, pcp);
    if (buf)
	(void)free(buf);
}


static void
fb_server_fb_view(struct pkg_conn *pcp, char *buf)
{
    int ret;
    int xcenter, ycenter, xzoom, yzoom;
    char rbuf[NET_LONG_LEN+1];

    if (buf == NULL) {
	bu_log("fb_server_fb_readrect: null buffer\n");
	return;
    }

    xcenter = pkg_glong(&buf[0*NET_LONG_LEN]);
    ycenter = pkg_glong(&buf[1*NET_LONG_LEN]);
    xzoom = pkg_glong(&buf[2*NET_LONG_LEN]);
    yzoom = pkg_glong(&buf[3*NET_LONG_LEN]);

    ret = fb_view(fbp, xcenter, ycenter, xzoom, yzoom);
    (void)pkg_plong(&rbuf[0], ret);
    pkg_send(MSG_RETURN, rbuf, NET_LONG_LEN, pcp);
    if (buf)
	(void)free(buf);
}


static void
fb_server_fb_getview(struct pkg_conn *pcp, char *buf)
{
    int ret;
    int xcenter, ycenter, xzoom, yzoom;
    char rbuf[5*NET_LONG_LEN+1];

    ret = fb_getview(fbp, &xcenter, &ycenter, &xzoom, &yzoom);
    (void)pkg_plong(&rbuf[0*NET_LONG_LEN], ret);
    (void)pkg_plong(&rbuf[1*NET_LONG_LEN], xcenter);
    (void)pkg_plong(&rbuf[2*NET_LONG_LEN], ycenter);
    (void)pkg_plong(&rbuf[3*NET_LONG_LEN], xzoom);
    (void)pkg_plong(&rbuf[4*NET_LONG_LEN], yzoom);
    pkg_send(MSG_RETURN, rbuf, 5*NET_LONG_LEN, pcp);
    if (buf)
	(void)free(buf);
}


static void
fb_server_fb_rmap(struct pkg_conn *pcp, char *buf)
{
    int i;
    char rbuf[NET_LONG_LEN+1];
    ColorMap map;
    unsigned char cm[256*2*3];

    (void)pkg_plong(&rbuf[0*NET_LONG_LEN], fb_rmap(fbp, &map));
    for (i = 0; i < 256; i++) {
	(void)pkg_pshort((char *)(cm+2*(0+i)), map.cm_red[i]);
	(void)pkg_pshort((char *)(cm+2*(256+i)), map.cm_green[i]);
	(void)pkg_pshort((char *)(cm+2*(512+i)), map.cm_blue[i]);
    }
    pkg_send(MSG_DATA, (char *)cm, sizeof(cm), pcp);
    pkg_send(MSG_RETURN, rbuf, NET_LONG_LEN, pcp);
    if (buf)
	(void)free(buf);
}


/*
 * Accept a color map sent by the client, and write it to the
 * framebuffer.  Network format is to send each entry as a network
 * (IBM) order 2-byte short, 256 red shorts, followed by 256 green and
 * 256 blue, for a total of 3*256*2 bytes.
 */
static void
fb_server_fb_wmap(struct pkg_conn *pcp, char *buf)
{
    int i;
    char rbuf[NET_LONG_LEN+1];
    long ret;
    ColorMap map;

    if (buf == NULL) {
	bu_log("fb_server_fb_wmap: null buffer\n");
	return;
    }

    if (pcp->pkc_len == 0)
	ret = fb_wmap(fbp, COLORMAP_NULL);
    else {
	for (i = 0; i < 256; i++) {
	    map.cm_red[i] = pkg_gshort(buf+2*(0+i));
	    map.cm_green[i] = pkg_gshort(buf+2*(256+i));
	    map.cm_blue[i] = pkg_gshort(buf+2*(512+i));
	}
	ret = fb_wmap(fbp, &map);
    }
    (void)pkg_plong(&rbuf[0], ret);
    pkg_send(MSG_RETURN, rbuf, NET_LONG_LEN, pcp);
    if (buf)
	(void)free(buf);
}


static void
fb_server_fb_flush(struct pkg_conn *pcp, char *buf)
{
    int ret;
    char rbuf[NET_LONG_LEN+1];

    ret = fb_flush(fbp);

    if (pcp->pkc_type < MSG_NORETURN) {
	(void)pkg_plong(rbuf, ret);
	pkg_send(MSG_RETURN, rbuf, NET_LONG_LEN, pcp);
    }

    if (buf)
	(void)free(buf);
}


static void
fb_server_fb_poll(struct pkg_conn *pcp, char *buf)
{
    if (!pcp) return;
    (void)fb_poll(fbp);
    if (buf) (void)free(buf);
}


/*
 * At one time at least we couldn't send a zero length PKG message
 * back and forth, so we receive a dummy long here.
 */
static void
fb_server_fb_help(struct pkg_conn *pcp, char *buf)
{
    long ret;
    char rbuf[NET_LONG_LEN+1];

    if (buf == NULL) {
	bu_log("fb_server_fb_window: null buffer\n");
	return;
    }

    (void)pkg_glong(&buf[0*NET_LONG_LEN]);

    ret = fb_help(fbp);
    (void)pkg_plong(&rbuf[0], ret);
    pkg_send(MSG_RETURN, rbuf, NET_LONG_LEN, pcp);
    if (buf)
	(void)free(buf);
}

const struct pkg_switch pkg_switch[] = {
    { MSG_FBOPEN,                       fb_server_fb_open,        "Open Framebuffer", NULL },
    { MSG_FBCLOSE,                      fb_server_fb_close,       "Close Framebuffer", NULL },
    { MSG_FBCLEAR,                      fb_server_fb_clear,       "Clear Framebuffer", NULL },
    { MSG_FBREAD,                       fb_server_fb_read,        "Read Pixels", NULL },
    { MSG_FBWRITE,                      fb_server_fb_write,       "Write Pixels", NULL },
    { MSG_FBWRITE + MSG_NORETURN,       fb_server_fb_write,       "Asynch write", NULL },
    { MSG_FBCURSOR,                     fb_server_fb_cursor,      "Cursor", NULL },
    { MSG_FBGETCURSOR,                  fb_server_fb_getcursor,   "Get Cursor", NULL },      /*NEW*/
    { MSG_FBSCURSOR,                    fb_server_fb_scursor,     "Screen Cursor", NULL }, /*OLD*/
    { MSG_FBWINDOW,                     fb_server_fb_window,      "Window", NULL },          /*OLD*/
    { MSG_FBZOOM,                       fb_server_fb_zoom,        "Zoom", NULL },    /*OLD*/
    { MSG_FBV,                       fb_server_fb_view,        "View", NULL },    /*NEW*/
    { MSG_FBGETVIEW,                    fb_server_fb_getview,     "Get View", NULL },        /*NEW*/
    { MSG_FBRMAP,                       fb_server_fb_rmap,        "R Map", NULL },
    { MSG_FBWMAP,                       fb_server_fb_wmap,        "W Map", NULL },
    { MSG_FBHELP,                       fb_server_fb_help,        "Help Request", NULL },
    { MSG_ERROR,                        fb_server_fb_unknown,     "Error Message", NULL },
    { MSG_CLOSE,                        fb_server_fb_unknown,     "Close Connection", NULL },
    { MSG_FBREADRECT,                   fb_server_fb_readrect,    "Read Rectangle", NULL },
    { MSG_FBWRITERECT,                  fb_server_fb_writerect,   "Write Rectangle", NULL },
    { MSG_FBWRITERECT + MSG_NORETURN,   fb_server_fb_writerect,   "Write Rectangle", NULL },
    { MSG_FBBWREADRECT,                 fb_server_fb_bwreadrect,  "Read BW Rectangle", NULL },
    { MSG_FBBWWRITERECT,                fb_server_fb_bwwriterect, "Write BW Rectangle", NULL },
    { MSG_FBBWWRITERECT + MSG_NORETURN, fb_server_fb_bwwriterect, "Write BW Rectangle", NULL },
    { MSG_FBFLUSH,                      fb_server_fb_flush,       "Flush Output", NULL },
    { MSG_FBFLUSH + MSG_NORETURN,       fb_server_fb_flush,       "Flush Output", NULL },
    { MSG_FBFREE,                       fb_server_fb_free,        "Free Resources", NULL },
    { MSG_FBPOLL,                       fb_server_fb_poll,        "Handle Events", NULL },
    { MSG_FBSETCURSOR,                  fb_server_fb_setcursor,   "Set Cursor Shape", NULL },
    { MSG_FBSETCURSOR + MSG_NORETURN,   fb_server_fb_setcursor,   "Set Cursor Shape", NULL },
    { 0,                                NULL,           NULL, NULL }
};


/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
