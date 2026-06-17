/* H O S T _ S T A T U S _ C O M M A N D . C P P
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
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
/** @file host_status_command.cpp
 *
 * IQgCommand sample for the Phase 7 qged extension-point slice.
 */

#include "qtcad/QgPluginContext.h"
#include "qtcad/QgSignalFlags.h"
#include "HostStatusCommandFactory.h"
#include "../common/HostStatusCommon.h"

QgPluginDescriptor
HostStatusCommandFactory::descriptor() const
{
    QgPluginDescriptor d;
    d.id = "org.brlcad.qged.command.host_status";
    d.displayName = "host_status";
    d.category = "qged.command";
    d.sortKey = 0;
    d.description = "Reports qged host status and optionally requests a view refresh.";
    d.vendor = "BRL-CAD";
    d.version = "1.0";
    return d;
}

QString
HostStatusCommandFactory::verb() const
{
    return QStringLiteral("host_status");
}

QStringList
HostStatusCommandFactory::aliases() const
{
    return QStringList() << QStringLiteral("qged_status");
}

int
HostStatusCommandFactory::run(QgPluginContext *ctx,
			      const QStringList &argv,
			      QString *out,
			      QString *err)
{
    bool request_refresh = false;
    for (int i = 1; i < argv.size(); ++i) {
	if (argv.at(i) == QStringLiteral("refresh") ||
	    argv.at(i) == QStringLiteral("--refresh")) {
	    request_refresh = true;
	    continue;
	}
	if (err) {
	    err->append(QStringLiteral("host_status: unknown argument '%1'\n")
			.arg(argv.at(i)));
	}
	return 1;
    }

    if (out)
	out->append(qged_host_status_summary(ctx));

    if (request_refresh && ctx && ctx->notifier) {
	emit ctx->notifier->viewUpdated(QG_VIEW_REFRESH);
	if (out)
	    out->append(QStringLiteral("Requested view refresh.\n"));
    }

    return 0;
}

/*
 * Local Variables:
 * mode: C++
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
