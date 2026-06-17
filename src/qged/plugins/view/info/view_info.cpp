/*                 V I E W _ I N F O . C P P
 * BRL-CAD
 *
 * Copyright (c) 2014-2026 United States Government as represented by
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
/** @file view_info.cpp
 *
 * IQgToolFactory implementation for the view-info palette tool.
 * Replaces the legacy qged_plugin_info ABI.
 */

#include <QIcon>
#include <QPixmap>
#include <QSizePolicy>

#include "qtcad/QgKeyVal.h"
#include "qtcad/QgPluginContext.h"
#include "qtcad/QgPluginDescriptor.h"
#include "ViewInfoFactory.h"

/* ------------------------------------------------------------------ */
/* ViewInfoTool                                                         */
/* ------------------------------------------------------------------ */

ViewInfoTool::ViewInfoTool(QgPluginContext *ctx, QObject *parent)
    : QgToolBase(ctx, parent)
{
}

QWidget *
ViewInfoTool::createWidget()
{
    m_model = new CADViewModel(this);
    m_model->setContext(m_ctx);
    m_model->refresh(0);

    QgKeyValView *vview = new QgKeyValView(nullptr, 0);
    vview->setModel(m_model);
    vview->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    vview->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    return vview;
}

QIcon *
ViewInfoTool::createIcon()
{
    return new QIcon(QPixmap(":info.svg"));
}

void
ViewInfoTool::refresh()
{
    if (m_model)
	m_model->refresh(0);
}

/* ------------------------------------------------------------------ */
/* ViewInfoFactory                                                      */
/* ------------------------------------------------------------------ */

QgPluginDescriptor
ViewInfoFactory::descriptor() const
{
    QgPluginDescriptor d;
    d.id           = "org.brlcad.qged.view.info";
    d.displayName  = "View Info";
    d.category     = "qged.view";
    d.iconName     = ":info.svg";
    d.sortKey      = 0;
    d.requiresView = true;
    d.description  = "Displays the current view name, size, azimuth, elevation, twist, and center.";
    d.vendor       = "BRL-CAD";
    d.version      = "1.0";
    return d;
}

QgToolBase *
ViewInfoFactory::create(QgPluginContext *ctx, QObject *parent)
{
    return new ViewInfoTool(ctx, parent);
}


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
