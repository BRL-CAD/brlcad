/*                          N C P . H
 * BRL-CAD
 *
 * Copyright (c) 2006-2021 United States Government as represented by
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
/** @file libpkg/example/ncp.h
 *
 * Example network communications protocol definition
 *
 */

#include "pkg.h"

/* simple network communication protocol. connection starts with a HELO,
 * then a variable number of GEOM/ARGS messages, then a CIAO to end.
 */
#define MAGIC_ID	"TPKG"
#define MSG_HELO	1
#define MSG_DATA	2
#define MSG_CIAO	3

/* maximum number of digits on a port number */
#define MAX_DIGITS      5

// NOTE: the client code isn't supposed to know about Qt.  However, both
// the server.cpp file and the moc code need to know about the class
// definitions, and only server.cpp had QT_SERVER defined.  So instead
// we define the following generally EXCEPT when building the client.
// For a real program this would be a separate header and the common
// definitions above would be in another, so this wouldn't arise.
#ifndef QT_CLIENT
#include "bu/vls.h"
#include <QHostAddress>
#include <QTcpServer>
#include <QTcpSocket>
#include <QObject>

class PKGServer : public QTcpServer
{
    Q_OBJECT

    public:

	PKGServer();
	~PKGServer();

    public slots:
	void pgetc();

    public:
	int psend(int type, const char *data);
	void waitfor_client();

	struct pkg_switch *callbacks = NULL;
	struct pkg_conn *client = NULL;
	int netfd = -2;
	int port = 2000;
	struct bu_vls buffer = BU_VLS_INIT_ZERO;
	char *msgbuffer;
	long bytes = 0;

	QTcpSocket *s;
};
#endif


/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
