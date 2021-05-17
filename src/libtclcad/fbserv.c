/*                        F B S E R V . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2021 United States Government as represented by
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
/** @addtogroup libstruct fb */
/** @{ */
/**
 *
 *  These are the Tcl specific callbacks used for I/O between client
 *  and server.  So far, experiments to simply use file handlers for
 *  Windows as well as other platforms haven't been successful, which
 *  is why we've had to accept the increased complexity of this code.
 */
/** @} */

#include "common.h"

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <tcl.h>
#include "bio.h"
#include "bnetwork.h"
#include "dm.h"
#include "tclcad.h"

/*
 * Communication error.  An error occurred on the PKG link.
 */
HIDDEN void
comm_error(const char *str)
{
    bu_log("%s", str);
}

#if defined(_WIN32) && !defined(__CYGWIN__)
static struct pkg_conn *
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
 * Accept any new client connections.
 */
static void
#if defined(_WIN32) && !defined(__CYGWIN__)
new_client_handler(ClientData clientData, Tcl_Channel chan, char *UNUSED(host), int UNUSED(port))
#else
new_client_handler(ClientData clientData, int UNUSED(port))
#endif
{
    struct fbserv_listener *fbslp = (struct fbserv_listener *)clientData;
    struct fbserv_obj *fbsp = fbslp->fbsl_fbsp;
    struct pkg_switch *pswitch = fbs_pkg_switch();
    int fd = fbslp->fbsl_fd;
    void *cdata = NULL;
    struct pkg_conn *pcp = NULL;

#if defined(_WIN32) && !defined(__CYGWIN__)
    if (Tcl_GetChannelHandle(chan, TCL_READABLE, (ClientData *)&fd) != TCL_OK)
	return;
    cdata = (void *)chan;
    pcp = fbs_makeconn(fd, pswitch);
#else
    pcp = pkg_getclient(fd, pswitch, comm_error, 0);
#endif

    fbs_new_client(fbsp, pcp, cdata);
}

/* Check if we're already listening. */
int
tclcad_is_listening(struct fbserv_obj *fbsp)
{
#if defined(_WIN32) && !defined(__CYGWIN__)
    if (fbsp->fbs_listener.fbsl_chan != NULL) {
#else
    if (fbsp->fbs_listener.fbsl_fd >= 0) {
#endif
	return 1;
    }
    return 0;
}

int
tclcad_listen_on_port(struct fbserv_obj *fbsp, int available_port)
{
    char hostname[32] = {0};
    /* XXX hardwired for now */
    sprintf(hostname, "localhost");

#if defined(_WIN32) && !defined(__CYGWIN__)
    fbsp->fbs_listener.fbsl_chan = Tcl_OpenTcpServer((Tcl_Interp *)fbsp->fbs_interp, available_port, hostname, new_client_handler, (ClientData)&fbsp->fbs_listener);
    if (fbsp->fbs_listener.fbsl_chan == NULL) {
	/* This clobbers the result string which probably has junk
	 * related to the failed open.
	 */
	Tcl_DStringResult((Tcl_Interp *)fbsp->fbs_interp, &ds);
	return 0;
    } else {
	return 1;
    }
#else
    char portname[32] = {0};
    sprintf(portname, "%d", available_port);
    fbsp->fbs_listener.fbsl_fd = pkg_permserver(portname, 0, 0, comm_error);
    if (fbsp->fbs_listener.fbsl_fd >= 0)
	return 1;
#endif
    return 0;
}

void
tclcad_open_server_handler(struct fbserv_obj *fbsp)
{
#if defined(_WIN32) && !defined(__CYGWIN__)
    Tcl_GetChannelHandle(fbsp->fbs_listener.fbsl_chan, TCL_READABLE, (ClientData *)&fbsp->fbs_listener.fbsl_fd);
#else
    Tcl_CreateFileHandler(fbsp->fbs_listener.fbsl_fd, TCL_READABLE, (Tcl_FileProc *)new_client_handler, (ClientData)&fbsp->fbs_listener);
#endif
}

void
tclcad_close_server_handler(struct fbserv_obj *fbsp)
{
#if defined(_WIN32) && !defined(__CYGWIN__)
    if (fbsp->fbs_listener.fbsl_chan != NULL) {
	Tcl_ChannelProc *callback = (Tcl_ChannelProc *)new_client_handler;
	Tcl_DeleteChannelHandler(fbsp->fbs_listener.fbsl_chan, callback, (ClientData)fbsp->fbs_listener.fbsl_fd);
	Tcl_Close((Tcl_Interp *)fbsp->fbs_interp, fbsp->fbs_listener.fbsl_chan);
	fbsp->fbs_listener.fbsl_chan = NULL;
    }
#else
    Tcl_DeleteFileHandler(fbsp->fbs_listener.fbsl_fd);
#endif
}

void
#if defined(_WIN32) && !defined(__CYGWIN__)
tclcad_open_client_handler(struct fbserv_obj *fbsp, int i, void *data)
#else
tclcad_open_client_handler(struct fbserv_obj *fbsp, int i, void *UNUSED(data))
#endif
{
#if defined(_WIN32) && !defined(__CYGWIN__)
    fbsp->fbs_clients[i].fbsc_chan = chan;
    fbsp->fbs_clients[i].fbsc_handler = existing_client_handler;
    Tcl_CreateChannelHandler(fbsp->fbs_clients[i].fbsc_chan, TCL_READABLE,
	    fbsp->fbs_clients[i].fbsc_handler, (ClientData)&fbsp->fbs_clients[i]);
#else
    Tcl_CreateFileHandler(fbsp->fbs_clients[i].fbsc_fd, TCL_READABLE,
	    fbs_existing_client_handler, (ClientData)&fbsp->fbs_clients[i]);
#endif
}

void
tclcad_close_client_handler(struct fbserv_obj *fbsp, int sub)
{
#if defined(_WIN32) && !defined(__CYGWIN__)
    Tcl_DeleteChannelHandler(fbsp->fbs_clients[sub].fbsc_chan, fbsp->fbs_clients[sub].fbsc_handler, (ClientData)fb  sp->fbs_clients[sub].fbsc_fd);

    Tcl_Close((Tcl_Interp *)fbsp->fbs_interp, fbsp->fbs_clients[sub].fbsc_chan);
    fbsp->fbs_clients[sub].fbsc_chan = NULL;
#else
    Tcl_DeleteFileHandler(fbsp->fbs_clients[sub].fbsc_fd);
#endif
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
