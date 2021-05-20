/*                      F B S E R V . C P P
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
/** @file fbserv.cpp
 *
 *  These are the Qt specific callbacks used for I/O between client
 *  and server.
 *
 *  TODO - Look into QLocalSocket, and whether we might be able to
 *  generalize libpkg (or even just use parts of it) to allow us
 *  to communicate using that mechanism...
 *
 *  Initial thought - optional callback functions to replace
 *  select, read, etc - if not set default to current behavior,
 *  if set do the callback instead of those calls...
 */

#include "common.h"
#include "bu/log.h"
#include "bu/vls.h"
#include "./fbserv.h"

/*
 * Communication error.  An error occurred on the PKG link.
 */
static void
comm_error(const char *str)
{
    bu_log("%s", str);
}

QFBListener::QFBListener(struct fbserv_obj *fp, int is_client)
{
    this->fbsp = fp;
    fd = fp->fbs_listener.fbsl_fd;
    if (is_client) {
	client = 1;
    }

    QObject::connect(
	    this, &QFBListener::client_handler,
	    this, &QFBListener::on_client_handler,
	    Qt::QueuedConnection
	    );

    m_notifier = new QSocketNotifier(fd, QSocketNotifier::Read);

    // NOTE : move to thread to avoid blocking, then sync with
    // main thread using a QueuedConnection with (TODO - figure
    // out what to do for this instead of finishedGetLine)
    m_notifier->moveToThread(&m_thread);
    QObject::connect(&m_thread , &QThread::finished, m_notifier, &QObject::deleteLater);


    QObject::connect(m_notifier, &QSocketNotifier::activated, m_notifier,
	    [this]() {
	    if (client) {
	       emit QFBListener::client_handler();
	    } else {
	       struct pkg_switch *pswitch = fbs_pkg_switch();
	       void *cdata = this;
	       struct pkg_conn *pcp = pkg_getclient(fd, pswitch, comm_error, 0);
	       fbs_new_client(fbsp, pcp, cdata);
	    }
	    });
    m_thread.start();
}

QFBListener::~QFBListener()
{
    m_notifier->disconnect();
    m_thread.quit();
    m_thread.wait();
}

void
QFBListener::on_client_handler()
{
    bu_log("on_client_handler\n");
    fbs_existing_client_handler(this->fbsp, 0);
}

/* Check if we're already listening. */
int
qdm_is_listening(struct fbserv_obj *fbsp)
{
    if (fbsp->fbs_listener.fbsl_fd >= 0) {
	return 1;
    }
    return 0;
}

int
qdm_listen_on_port(struct fbserv_obj *fbsp, int available_port)
{
    struct bu_vls portname = BU_VLS_INIT_ZERO;
    bu_vls_sprintf(&portname, "%d", available_port);
    fbsp->fbs_listener.fbsl_fd = pkg_permserver(bu_vls_cstr(&portname), 0, 0, comm_error);
    bu_vls_free(&portname);
    if (fbsp->fbs_listener.fbsl_fd >= 0)
	return 1;
    return 0;
}

void
qdm_open_server_handler(struct fbserv_obj *fbsp)
{
    fbsp->fbs_listener.fbsl_chan = new QFBListener(fbsp, 0);
}

void
qdm_close_server_handler(struct fbserv_obj *fbsp)
{
    QFBListener *fbl = (QFBListener *)fbsp->fbs_listener.fbsl_chan;
    fbl->m_notifier->disconnect();
    delete fbl;
}

void
qdm_open_client_handler(struct fbserv_obj *fbsp, int i, void *UNUSED(data))
{
    fbsp->fbs_clients[i].fbsc_chan = new QFBListener(fbsp, i);
}

void
qdm_close_client_handler(struct fbserv_obj *fbsp, int sub)
{
    QFBListener *fbl = (QFBListener *)fbsp->fbs_clients[sub].fbsc_chan;
    fbl->m_notifier->disconnect();
    delete fbl;
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
