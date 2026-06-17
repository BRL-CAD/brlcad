/*   H O S T S T A T U S P A N E L F A C T O R Y . H
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
/** @file HostStatusPanelFactory.h
 *
 * IQgPanelFactory sample for the Phase 7 qged extension-point slice.
 */

#ifndef HOSTSTATUSPANELFACTORY_H
#define HOSTSTATUSPANELFACTORY_H

#include "qtcad/QgPluginDescriptor.h"
#include "qtcad/QgPluginInterfaces.h"

class HostStatusPanelFactory : public QObject, public IQgPanelFactory
{
    Q_OBJECT
    Q_INTERFACES(IQgPanelFactory)
    Q_PLUGIN_METADATA(IID "org.brlcad.qtcad.IQgPanelFactory/1" FILE "host_status_panel.json")

    public:
	QgPluginDescriptor descriptor() const override;
	QDockWidget *create(QgPluginContext *ctx, QWidget *parent = nullptr) override;
};

#endif /* HOSTSTATUSPANELFACTORY_H */

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
