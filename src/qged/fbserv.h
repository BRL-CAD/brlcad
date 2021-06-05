/*                        F B S E R V . H
 * BRL-CAD
 *
 * Copyright (c) 2021 United States Government as represented by
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
/** @file fbserv.h
 *
 * Brief description
 *
 */

#ifndef QDM_FBSERV_H
#define QDM_FBSERV_H

#include "common.h"

#include <QByteArray>
#include <QHostAddress>
#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <iostream>

#include "dm/fbserv.h"

// Per client info
class QFBSocket : public QObject
{
    Q_OBJECT

    public:
	QTcpSocket *s;
	int ind;
	struct fbserv_obj *fbsp;

    signals:
	void updated();

    public slots:
	void client_handler();

    private:
        QByteArray buff;
};

// Overall server that sets up clients
// in response to connection requests
class QFBServer : public QTcpServer
{
    Q_OBJECT

    public:
	QFBServer(struct fbserv_obj *fp = NULL);
	~QFBServer();

	int port = -1;
	struct fbserv_obj *fbsp;

    public slots:
	void on_Connect();
};


__BEGIN_DECLS

extern int
qdm_is_listening(struct fbserv_obj *fbsp);
extern int
qdm_listen_on_port(struct fbserv_obj *fbsp, int available_port);
extern void
qdm_open_server_handler(struct fbserv_obj *fbsp);
extern void
qdm_close_server_handler(struct fbserv_obj *fbsp);
#ifdef BRLCAD_OPENGL
extern void
qdm_open_client_handler(struct fbserv_obj *fbsp, int i, void *data);
#endif
extern void
qdm_open_sw_client_handler(struct fbserv_obj *fbsp, int i, void *data);
extern void
qdm_close_client_handler(struct fbserv_obj *fbsp, int sub);

__END_DECLS

#endif /* QDM_FBSERV_H */

/** @} */
/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
