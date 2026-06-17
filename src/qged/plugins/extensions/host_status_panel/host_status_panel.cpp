/*   H O S T _ S T A T U S _ P A N E L . C P P
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
/** @file host_status_panel.cpp
 *
 * IQgPanelFactory sample for the Phase 7 qged extension-point slice.
 */

#include <QDockWidget>

#include "qtcad/QgPluginContext.h"
#include "HostStatusPanelFactory.h"
#include "../common/HostStatusCommon.h"

QgPluginDescriptor
HostStatusPanelFactory::descriptor() const
{
    QgPluginDescriptor d;
    d.id = "org.brlcad.qged.panel.host_status";
    d.displayName = "Host Status";
    d.category = "qged.panel";
    d.sortKey = 0;
    d.description = "Dockable status panel showing the active qged host, database, and view.";
    d.vendor = "BRL-CAD";
    d.version = "1.0";
    return d;
}

QDockWidget *
HostStatusPanelFactory::create(QgPluginContext *ctx, QWidget *parent)
{
    QDockWidget *dock = new QDockWidget(QStringLiteral("Host Status"), parent);
    dock->setObjectName(QStringLiteral("org.brlcad.qged.panel.host_status"));
    dock->setWidget(new HostStatusWidget(ctx, dock));
    return dock;
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
