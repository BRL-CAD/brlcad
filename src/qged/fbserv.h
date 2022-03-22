/*                        F B S E R V . H
 * BRL-CAD
 *
 * Copyright (c) 2021-2022 United States Government as represented by
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
#ifdef BRLCAD_OPENGL
    static void qdm_open_client_handler(struct fbserv_obj *fbsp, int i, void *data);
#endif
    static void qdm_open_sw_client_handler(struct fbserv_obj *fbsp, int i, void *data);
    static int qdm_is_listening(struct fbserv_obj *fbsp);
    static int qdm_listen_on_port(struct fbserv_obj *fbsp, int available_port);
    static void qdm_open_server_handler(struct fbserv_obj *fbsp);
    static void qdm_close_server_handler(struct fbserv_obj *fbsp);
    static void qdm_close_client_handler(struct fbserv_obj *fbsp, int sub);

    signals:
	void updated();

    public slots:
        void client_handler();

      private:
        QTcpSocket *s;
        int ind;
        struct fbserv_obj *fbsp;

        QByteArray buff;

        friend class QFBServer;
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
