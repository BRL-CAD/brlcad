/*                        F B S E R V . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2026 United States Government as represented by
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
/** @addtogroup libdm */
/** @{ */
/**
 *
 * A framebuffer server object contains the attributes and
 * methods for implementing an fbserv. This code was developed
 * in large part by modifying the stand-alone version of fbserv.
 */
/** @} */

#include "common.h"

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "bio.h"
#include "bnetwork.h"

#include "bu/ipc.h"
#include "bu/str.h"
#include "raytrace.h"
#include "dm.h"
#include "./include/private.h"

/* Enable token generation AND verification for the embedded fbserv.
 * Hosting apps call fbs_generate_token() to create a session token;
 * the MSG_FBAUTH handler verifies incoming client tokens. */
#define FBSERV_AUTH_IMPL
#define FBSERV_AUTH_SERVER
#include "../fbserv/auth.h"

/* Enable TLS server-side functions */
#define FBSERV_TLS_IMPL
#include "../fbserv/tls_wrap.h"

/**
 * Helper: extract the current framebuffer pointer from a pkg_conn
 * whose pkc_server_data points to the owning fbserv_client.
 */
static struct fb *
_fbs_conn_fb(struct pkg_conn *pcp)
{
    struct fbserv_client *fbscp;
    if (!pcp || !pcp->pkc_server_data)
	return NULL;
    fbscp = (struct fbserv_client *)pcp->pkc_server_data;
    if (!fbscp->fbsc_fbsp)
	return NULL;
    return fbscp->fbsc_fbsp->fbs_fbp;
}

static void
drop_client(struct fbserv_obj *fbsp, int sub)
{
    if (fbsp->fbs_clients[sub].fbsc_pkg != PKC_NULL) {
	pkg_close(fbsp->fbs_clients[sub].fbsc_pkg);
	fbsp->fbs_clients[sub].fbsc_pkg = PKC_NULL;
    }

    if (fbsp->fbs_clients[sub].fbsc_fd != 0) {
	/* Use the IPC-specific close handler if the client was opened via IPC
	 * and the caller registered one; otherwise fall back to the generic
	 * TCP close handler. */
	if (fbsp->fbs_clients[sub].fbsc_is_ipc && fbsp->fbs_close_ipc_client_handler)
	    (*fbsp->fbs_close_ipc_client_handler)(fbsp, sub);
	else
	    (*fbsp->fbs_close_client_handler)(fbsp, sub);
	fbsp->fbs_clients[sub].fbsc_fd = 0;
    }
    fbsp->fbs_clients[sub].fbsc_auth_ok = 0;
    fbsp->fbs_clients[sub].fbsc_pending_drop = 0;
    fbsp->fbs_clients[sub].fbsc_is_ipc = 0;
}


/**
 * Guard for data-op handlers: check auth + non-NULL framebuffer.
 * Returns 0 on success.  On failure sends a -1 reply, schedules a
 * deferred drop when auth fails, frees buf, and returns -1.
 */
static int
fbs_data_guard(struct pkg_conn *pcp, char *buf)
{
    char erbuf[NET_LONG_LEN+1];
    struct fbserv_client *fbscp;
    struct fbserv_obj *fbsp;

    if (!pcp || !pcp->pkc_server_data) {
	if (buf) (void)free(buf);
	return -1;
    }
    fbscp = (struct fbserv_client *)pcp->pkc_server_data;
    fbsp  = fbscp->fbsc_fbsp;

    if (fbsp && fbsp->fbs_require_auth && !fbscp->fbsc_auth_ok) {
	bu_log("fbserv: unauthenticated data request (type %d) rejected\n",
	       pcp->pkc_type);
	(void)pkg_plong(erbuf, -1);
	pkg_send(MSG_RETURN, erbuf, NET_LONG_LEN, pcp);
	fbscp->fbsc_pending_drop = 1;
	if (buf) (void)free(buf);
	return -1;
    }

    if (!fbsp || !fbsp->fbs_fbp) {
	bu_log("fbserv: data request (type %d) with null framebuffer\n",
	       pcp->pkc_type);
	(void)pkg_plong(erbuf, -1);
	pkg_send(MSG_RETURN, erbuf, NET_LONG_LEN, pcp);
	if (buf) (void)free(buf);
	return -1;
    }

    return 0;
}


/*
 * This is where we go for message types we don't understand.
 */
static void
fbs_rfbunknown(struct pkg_conn *pcp, char *buf)
{
    bu_log("fbserv: unable to handle message type %d\n", pcp->pkc_type);
    if (buf) {
	(void)free(buf);
    }
}


/******** Here's where the hooks lead *********/

/**
 * MSG_FBAUTH — session token authentication.
 *
 * The client sends a FBSERV_AUTH_TOKEN_LEN-byte hex token string.
 * If it matches the server's session token the connection is marked
 * authenticated and all subsequent requests are allowed.  If the token
 * is wrong the connection is closed immediately.
 *
 * Old clients that do not send MSG_FBAUTH are still accepted unless
 * the server is running in strict mode (fbs_require_auth != 0).
 */
static void
fbs_rfbauth(struct pkg_conn *pcp, char *buf)
{
    struct fbserv_client *fbscp;
    struct fbserv_obj *fbsp;
    char provided[FBSERV_AUTH_TOKEN_LEN + 1] = {0};

    if (!pcp || !pcp->pkc_server_data) {
	if (buf) (void)free(buf);
	return;
    }

    fbscp = (struct fbserv_client *)pcp->pkc_server_data;
    fbsp = fbscp->fbsc_fbsp;

    if (!fbsp || fbsp->fbs_auth_token[0] == '\0') {
	/* No token configured — mark auth as satisfied */
	fbscp->fbsc_auth_ok = 1;
	if (buf) (void)free(buf);
	return;
    }

    if (buf && pcp->pkc_len >= FBSERV_AUTH_TOKEN_LEN) {
	bu_strlcpy(provided, buf, sizeof(provided));
    }

    if (fbserv_verify_token(provided, fbsp->fbs_auth_token)) {
	fbscp->fbsc_auth_ok = 1;
    } else {
	bu_log("fbserv: MSG_FBAUTH token mismatch from client — dropping\n");
	/* Use deferred drop: pkg_process still holds a reference to pcp.
	 * Setting fbsc_pending_drop causes fbs_existing_client_handler to
	 * call drop_client() after pkg_process() returns. */
	fbscp->fbsc_pending_drop = 1;
    }

    if (buf) (void)free(buf);
}


static void
fbs_rfbopen(struct pkg_conn *pcp, char *buf)
{
    struct fbserv_client *fbscp;
    struct fbserv_obj *fbsp;
    struct fb *curr_fbp = _fbs_conn_fb(pcp);
    char rbuf[5*NET_LONG_LEN+1] = {0};
    int want;

    /* Auth check: if the server requires authentication and this
     * connection has not sent a valid MSG_FBAUTH, reject with an
     * error return code and close the connection. */
    if (pcp && pcp->pkc_server_data) {
	fbscp = (struct fbserv_client *)pcp->pkc_server_data;
	fbsp  = fbscp->fbsc_fbsp;
	if (fbsp && fbsp->fbs_require_auth && !fbscp->fbsc_auth_ok) {
	    bu_log("fbserv: unauthenticated MSG_FBOPEN rejected (strict mode)\n");
	    (void)pkg_plong(&rbuf[0*NET_LONG_LEN], -1);	/* failure */
	    (void)pkg_plong(&rbuf[1*NET_LONG_LEN], 0);
	    (void)pkg_plong(&rbuf[2*NET_LONG_LEN], 0);
	    (void)pkg_plong(&rbuf[3*NET_LONG_LEN], 0);
	    (void)pkg_plong(&rbuf[4*NET_LONG_LEN], 0);
	    pkg_send(MSG_RETURN, rbuf, 5*NET_LONG_LEN, pcp);
	    /* Deferred drop: pkg_process still holds pcp; close after return */
	    fbscp->fbsc_pending_drop = 1;
	    if (buf) (void)free(buf);
	    return;
	}
    }

    /* Don't really open a new framebuffer --- use existing one */
    (void)pkg_plong(&rbuf[0*NET_LONG_LEN], 0);	/* ret */
    (void)pkg_plong(&rbuf[1*NET_LONG_LEN], curr_fbp->i->if_max_width);
    (void)pkg_plong(&rbuf[2*NET_LONG_LEN], curr_fbp->i->if_max_height);
    (void)pkg_plong(&rbuf[3*NET_LONG_LEN], curr_fbp->i->if_width);
    (void)pkg_plong(&rbuf[4*NET_LONG_LEN], curr_fbp->i->if_height);

    want = 5*NET_LONG_LEN;
    if (pkg_send(MSG_RETURN, rbuf, want, pcp) != want)
	bu_log("pkg_send fb_open reply\n");

    if (buf) {
	(void)free(buf);
    }
}


void
fbs_rfbclose(struct pkg_conn *pcp, char *buf)
{
    struct fb *curr_fbp;
    char rbuf[NET_LONG_LEN+1] = {0};

    if (fbs_data_guard(pcp, buf) < 0) return;
    curr_fbp = _fbs_conn_fb(pcp);

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
    char rbuf[NET_LONG_LEN+1] = {0};

    if (fbs_data_guard(pcp, buf) < 0) return;
    /* Don't really free framebuffer */
    if (pkg_send(MSG_RETURN, rbuf, NET_LONG_LEN, pcp) != NET_LONG_LEN)
	bu_log("pkg_send fb_free reply\n");

    if (buf) {
	(void)free(buf);
    }
}


void
fbs_rfbclear(struct pkg_conn *pcp, char *buf)
{
    struct fb *curr_fbp;
    RGBpixel bg;
    char rbuf[NET_LONG_LEN+1] = {0};

    if (!buf) {
	bu_log("fbs_rfbclear: null buffer\n");
	return;
    }
    if (fbs_data_guard(pcp, buf) < 0) return;
    curr_fbp = _fbs_conn_fb(pcp);

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
    struct fb *curr_fbp;

    if (!buf) {
	bu_log("fbs_rfbread: null buffer\n");
	return;
    }
    if (fbs_data_guard(pcp, buf) < 0) return;
    curr_fbp = _fbs_conn_fb(pcp);

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
    char rbuf[NET_LONG_LEN+1] = {0};
    int ret;
    int type;
    struct fb *curr_fbp;

    if (!buf) {
	bu_log("fbs_rfbwrite: null buffer\n");
	return;
    }
    if (fbs_data_guard(pcp, buf) < 0) return;
    curr_fbp = _fbs_conn_fb(pcp);

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
    struct fb *curr_fbp;

    if (!buf) {
	bu_log("fbs_rfbreadrect: null buffer\n");
	return;
    }
    if (fbs_data_guard(pcp, buf) < 0) return;
    curr_fbp = _fbs_conn_fb(pcp);

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
    char rbuf[NET_LONG_LEN+1] = {0};
    int ret;
    int type;
    struct fb *curr_fbp;

    if (!buf) {
	bu_log("fbs_rfbwriterect: null buffer\n");
	return;
    }
    if (fbs_data_guard(pcp, buf) < 0) return;
    curr_fbp = _fbs_conn_fb(pcp);

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
    struct fb *curr_fbp;

    if (!buf) {
	bu_log("fbs_rfbbwreadrect: null buffer\n");
	return;
    }
    if (fbs_data_guard(pcp, buf) < 0) return;
    curr_fbp = _fbs_conn_fb(pcp);

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
    char rbuf[NET_LONG_LEN+1] = {0};
    int ret;
    int type;
    struct fb *curr_fbp;

    if (!buf) {
	bu_log("fbs_rfbbwwriterect: null buffer\n");
	return;
    }
    if (fbs_data_guard(pcp, buf) < 0) return;
    curr_fbp = _fbs_conn_fb(pcp);

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
    char rbuf[NET_LONG_LEN+1] = {0};
    struct fb *curr_fbp;

    if (!buf) {
	bu_log("fbs_rfbcursor: null buffer\n");
	return;
    }
    if (fbs_data_guard(pcp, buf) < 0) return;
    curr_fbp = _fbs_conn_fb(pcp);

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
    char rbuf[4*NET_LONG_LEN+1] = {0};
    struct fb *curr_fbp;

    if (fbs_data_guard(pcp, buf) < 0) return;
    curr_fbp = _fbs_conn_fb(pcp);
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
    char rbuf[NET_LONG_LEN+1] = {0};
    int ret;
    int xbits, ybits;
    int xorig, yorig;
    struct fb *curr_fbp;

    if (!buf) {
	bu_log("fbs_rfsetcursor: null buffer\n");
	return;
    }
    if (fbs_data_guard(pcp, buf) < 0) return;
    curr_fbp = _fbs_conn_fb(pcp);

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
    char rbuf[NET_LONG_LEN+1] = {0};
    struct fb *curr_fbp;

    if (!buf) {
	bu_log("fbs_rfbscursor: null buffer\n");
	return;
    }
    if (fbs_data_guard(pcp, buf) < 0) return;
    curr_fbp = _fbs_conn_fb(pcp);

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
    char rbuf[NET_LONG_LEN+1] = {0};
    struct fb *curr_fbp;

    if (!buf) {
	bu_log("fbs_rfbwindow: null buffer\n");
	return;
    }
    if (fbs_data_guard(pcp, buf) < 0) return;
    curr_fbp = _fbs_conn_fb(pcp);

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
    char rbuf[NET_LONG_LEN+1] = {0};
    struct fb *curr_fbp;

    if (!buf) {
	bu_log("fbs_rfbzoom: null buffer\n");
	return;
    }
    if (fbs_data_guard(pcp, buf) < 0) return;
    curr_fbp = _fbs_conn_fb(pcp);

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
    char rbuf[NET_LONG_LEN+1] = {0};
    struct fb *curr_fbp;

    if (!buf) {
	bu_log("fbs_rfbv: null buffer\n");
	return;
    }
    if (fbs_data_guard(pcp, buf) < 0) return;
    curr_fbp = _fbs_conn_fb(pcp);

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
    char rbuf[5*NET_LONG_LEN+1] = {0};
    struct fb *curr_fbp;

    if (fbs_data_guard(pcp, buf) < 0) return;
    curr_fbp = _fbs_conn_fb(pcp);
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
    char rbuf[NET_LONG_LEN+1] = {0};
    ColorMap map;
    unsigned char cm[256*2*3];
    struct fb *curr_fbp;

    if (fbs_data_guard(pcp, buf) < 0) return;
    curr_fbp = _fbs_conn_fb(pcp);
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
    char rbuf[NET_LONG_LEN+1] = {0};
    long ret;
    ColorMap map;
    struct fb *curr_fbp;

    if (!buf) {
	bu_log("fbs_rfbwmap: null buffer\n");
	return;
    }
    if (fbs_data_guard(pcp, buf) < 0) return;
    curr_fbp = _fbs_conn_fb(pcp);

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
    char rbuf[NET_LONG_LEN+1] = {0};
    struct fb *curr_fbp;

    if (fbs_data_guard(pcp, buf) < 0) return;
    curr_fbp = _fbs_conn_fb(pcp);
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
    struct fb *curr_fbp;

    if (fbs_data_guard(pcp, buf) < 0) return;
    curr_fbp = _fbs_conn_fb(pcp);

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
    char rbuf[NET_LONG_LEN+1] = {0};
    struct fb *curr_fbp;

    if (!buf) {
	bu_log("fbs_rfbhelp: null buffer\n");
	return;
    }
    if (fbs_data_guard(pcp, buf) < 0) return;
    curr_fbp = _fbs_conn_fb(pcp);

    (void)pkg_glong(&buf[0*NET_LONG_LEN]);

    ret = fb_help(curr_fbp);
    (void)pkg_plong(&rbuf[0], ret);
    pkg_send(MSG_RETURN, rbuf, NET_LONG_LEN, pcp);
    (void)free(buf);
}


/**
 * Initialise fbsp->fbs_auth_token for session authentication.
 *
 * If the FBSERV_TOKEN environment variable is already set to a valid
 * 64-hex-char token, that value is used directly.  This lets the
 * hosting application (MGED, qged, etc.) pre-supply a known token and
 * pass the same value to child processes (e.g. via setenv() before
 * fork/exec of rt).  Token authentication works regardless of whether
 * TLS is enabled — it provides session isolation even on plain TCP.
 *
 * If FBSERV_TOKEN is not set or is the wrong length, a fresh random
 * token is generated.
 *
 * Should be called before fbs_open() so the token is ready for the
 * first connecting client.  Returns a pointer to fbsp->fbs_auth_token.
 */
const char *
fbs_generate_token(struct fbserv_obj *fbsp)
{
    const char *env_token = getenv("FBSERV_TOKEN");
    if (env_token && strlen(env_token) == FBSERV_AUTH_TOKEN_LEN) {
	bu_strlcpy(fbsp->fbs_auth_token, env_token,
		   sizeof(fbsp->fbs_auth_token));
    } else {
	fbserv_generate_token(fbsp->fbs_auth_token);
    }
    return fbsp->fbs_auth_token;
}


int
fbs_open(struct fbserv_obj *fbsp, int port)
{
    int i;
    int available_port = port;

    /* Already listening; nothing more to do. */
    if ((*fbsp->fbs_is_listening)(fbsp)) {
	return BRLCAD_OK;
    }

    if (available_port < 0) {
	available_port = 5559;
    } else if (available_port < 1024) {
	available_port += 5559;
    }

    /* Try a reasonable number of times to hang a listen */
    int have_listen = 0;
    for (i = 0; i < MAX_PORT_TRIES; ++i) {
	/*
	 * Hang an unending listen for PKG connections
	 */
	if (!(*fbsp->fbs_listen_on_port)(fbsp, available_port)) {
	    ++available_port;
	} else {
	    have_listen = 1;
	    break;
	}
    }

    if (!have_listen) {
	if (fbsp->msgs)
	    bu_vls_printf(fbsp->msgs, "fbs_open: failed to hang a listen on ports %d - %d\n", port, available_port);
	fbsp->fbs_listener.fbsl_port = -1;
	return BRLCAD_ERROR;
    }

    fbsp->fbs_listener.fbsl_port = available_port;

    (*fbsp->fbs_open_server_handler)(fbsp);

    return BRLCAD_OK;
}


int
fbs_close(struct fbserv_obj *fbsp)
{
    int i;

    /* first drop all clients */
    for (i = 0; i < MAX_CLIENTS; ++i)
	drop_client(fbsp, i);

    (*fbsp->fbs_close_server_handler)(fbsp);

    if (0 <= fbsp->fbs_listener.fbsl_fd)
	close(fbsp->fbs_listener.fbsl_fd);
    fbsp->fbs_listener.fbsl_fd = -1;
    fbsp->fbs_listener.fbsl_port = -1;

    /* Close the IPC child-end channel if one was created by fbs_open_ipc(). */
    if (fbsp->fbs_listener.fbsl_ipc_child) {
	bu_ipc_close(fbsp->fbs_listener.fbsl_ipc_child);
	fbsp->fbs_listener.fbsl_ipc_child = NULL;
    }

    return BRLCAD_OK;
}

struct pkg_switch *
fbs_pkg_switch(void)
{
    static struct pkg_switch pswitch[] = {
	{ MSG_FBAUTH,                        fbs_rfbauth,          "Session Authentication", NULL },
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

    return (struct pkg_switch *)pswitch;
}

void
fbs_setup_socket(int fd)
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

/*
 * Process arrivals from existing clients.
 */
void
fbs_existing_client_handler(void *clientData, int UNUSED(mask))
{
    register int i;
    struct fbserv_client *fbscp = (struct fbserv_client *)clientData;
    struct fbserv_obj *fbsp = fbscp->fbsc_fbsp;
    int fd = fbscp->fbsc_fd;

    for (i = MAX_CLIENTS - 1; i >= 0; i--) {
	if (fbsp->fbs_clients[i].fbsc_fd == 0)
	    continue;

	fbsp->fbs_clients[i].fbsc_pkg->pkc_server_data = (void *)&fbsp->fbs_clients[i];

	if ((pkg_process(fbsp->fbs_clients[i].fbsc_pkg)) < 0)
	    bu_log("pkg_process error encountered (1)\n");

	/* Act on deferred drop requested by a handler (e.g. token mismatch).
	 * We must not call pkg_close() from inside pkg_process's loop. */
	if (fbsp->fbs_clients[i].fbsc_pending_drop) {
	    drop_client(fbsp, i);
	    continue;
	}

	if (fbsp->fbs_clients[i].fbsc_fd != fd)
	    continue;

	if (pkg_suckin(fbsp->fbs_clients[i].fbsc_pkg) <= 0) {
	    /* Probably EOF */
	    drop_client(fbsp, i);

	    continue;
	}

	if ((pkg_process(fbsp->fbs_clients[i].fbsc_pkg)) < 0)
	    bu_log("pkg_process error encountered (2)\n");

	/* Deferred drop from second-pass handler */
	if (fbsp->fbs_clients[i].fbsc_pending_drop) {
	    drop_client(fbsp, i);
	    continue;
	}
    }

    if (fbsp->fbs_callback != (void (*)(void *))FBS_CALLBACK_NULL) {
	/* need to cast func pointer explicitly to get the function call */
	void (*cfp)(void *);
	cfp = (void (*)(void *))fbsp->fbs_callback;
	cfp(fbsp->fbs_clientData);
    }
}


int
fbs_new_client(struct fbserv_obj *fbsp, struct pkg_conn *pcp, void *data)
{
    if (pcp == PKC_ERROR)
	return -1;

    for (int i = MAX_CLIENTS - 1; i >= 0; i--) {
	/* this slot is being used */
	if (fbsp->fbs_clients[i].fbsc_fd != 0)
	    continue;

	/* Found an available slot */
	fbsp->fbs_clients[i].fbsc_fd = pcp->pkc_fd;
	fbsp->fbs_clients[i].fbsc_pkg = pcp;
	fbsp->fbs_clients[i].fbsc_fbsp = fbsp;
	fbsp->fbs_clients[i].fbsc_auth_ok = 0;
	fbsp->fbs_clients[i].fbsc_pending_drop = 0;
	fbs_setup_socket(pcp->pkc_fd);

	/* Point pkc_server_data at the fbserv_client so handlers can
	 * reach back to the fbserv_obj (needed for auth checks). */
	pcp->pkc_server_data = (void *)&fbsp->fbs_clients[i];

#ifdef HAVE_OPENSSL_SSL_H
	/* Optional TLS: if the server has a TLS context, perform the
	 * server-side handshake before the first PKG message is read. */
	if (fbsp->fbs_tls_ctx) {
	    if (fbserv_tls_accept((SSL_CTX *)fbsp->fbs_tls_ctx, pcp) != FBSERV_TLS_OK) {
		bu_log("fbs_new_client: TLS handshake failed — dropping client\n");
		pkg_close(pcp);
		fbsp->fbs_clients[i].fbsc_pkg = PKC_NULL;
		fbsp->fbs_clients[i].fbsc_fd = 0;
		return -1;
	    }
	}
#endif

	(*fbsp->fbs_open_client_handler)(fbsp, i, data);

	return i;
    }

    bu_log("fbs_new_client: too many clients\n");
    pkg_close(pcp);

    return -1;
}


static void
_fbs_ipc_comm_error(const char *msg)
{
    bu_log("%s", msg);
}


int
fbs_open_ipc(struct fbserv_obj *fbsp)
{
    bu_ipc_chan_t *pe = NULL, *ce = NULL;
    struct pkg_conn *pc;
    int rfd, wfd, i;

    if (bu_ipc_pair(&pe, &ce) != 0) {
	if (fbsp->msgs)
	    bu_vls_printf(fbsp->msgs, "fbs_open_ipc: bu_ipc_pair failed\n");
	return BRLCAD_ERROR;
    }

    /* Move child-end fd(s) above bu_process_create()'s close(3..19) sweep
     * so the fd survives into the spawned subprocess after exec().          */
    if (bu_ipc_move_high_fd(ce, 64) != 0)
	bu_log("fbs_open_ipc: bu_ipc_move_high_fd failed; fd may be swept\n");

    rfd = bu_ipc_fileno(pe);
    wfd = bu_ipc_fileno_write(pe);
    pc = pkg_open_fds(rfd, wfd, fbs_pkg_switch(), _fbs_ipc_comm_error);
    if (pc == PKC_ERROR || pc == PKC_NULL) {
	bu_ipc_close(ce);
	bu_ipc_close(pe);
	if (fbsp->msgs)
	    bu_vls_printf(fbsp->msgs, "fbs_open_ipc: pkg_open_fds failed\n");
	return BRLCAD_ERROR;
    }

    /* Find an empty client slot and register the pre-connected pkg_conn.
     * For pipe-based IPC transport pkg_open_fds() uses PKG_STDIO_MODE
     * internally (pkc_fd == -3); the actual readable fd is pkc_in_fd.
     * fbsc_fd is used by callers (Tcl_CreateFileHandler, select-based loops,
     * etc.) as the fd to monitor for readability, so it must always hold a
     * valid (>= 0) file descriptor.                                          */
    int effective_fd = (pc->pkc_fd == PKG_STDIO_MODE) ? pc->pkc_in_fd : pc->pkc_fd;

    for (i = MAX_CLIENTS - 1; i >= 0; i--) {
	if (fbsp->fbs_clients[i].fbsc_fd != 0)
	    continue;

	fbsp->fbs_clients[i].fbsc_fd      = effective_fd;
	fbsp->fbs_clients[i].fbsc_pkg     = pc;
	fbsp->fbs_clients[i].fbsc_fbsp    = fbsp;
	fbsp->fbs_clients[i].fbsc_auth_ok = 1; /* IPC client is implicitly trusted */
	fbsp->fbs_clients[i].fbsc_pending_drop = 0;
	fbsp->fbs_clients[i].fbsc_is_ipc  = 1;
	pc->pkc_server_data = (void *)&fbsp->fbs_clients[i];

	/* Call the IPC-specific open handler if one is registered, otherwise
	 * fall back to the generic TCP client handler.  Callers that use the
	 * generic handler must ensure it handles NULL data gracefully.        */
	if (fbsp->fbs_open_ipc_client_handler)
	    (*fbsp->fbs_open_ipc_client_handler)(fbsp, i, NULL);
	else if (fbsp->fbs_open_client_handler)
	    (*fbsp->fbs_open_client_handler)(fbsp, i, NULL);

	break;
    }

    if (i < 0) {
	bu_log("fbs_open_ipc: too many clients\n");
	bu_ipc_close(ce);
	pkg_close(pc);
	return BRLCAD_ERROR;
    }

    /* Parent end's fds are now owned by pkg_conn; release the wrapper. */
    bu_ipc_detach(pe);

    /* Store the child end so fbs_ipc_child_addr_env() can retrieve it, and
     * so fbs_close() can close it when the session ends.                    */
    if (fbsp->fbs_listener.fbsl_ipc_child)
	bu_ipc_close(fbsp->fbs_listener.fbsl_ipc_child);
    fbsp->fbs_listener.fbsl_ipc_child = ce;

    return BRLCAD_OK;
}


const char *
fbs_ipc_child_addr_env(struct fbserv_obj *fbsp)
{
    if (!fbsp || !fbsp->fbs_listener.fbsl_ipc_child)
	return NULL;
    return bu_ipc_addr_env(fbsp->fbs_listener.fbsl_ipc_child);
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
