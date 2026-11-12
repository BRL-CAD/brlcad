/*       T E S T _ Q T C A D _ P L U G I N _ P A N E L . C P P
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

#include "common.h"

#include <QDockWidget>
#include <QLabel>

#include "qtcad/QgPluginInterfaces.h"

class QgTestPanelPlugin : public QObject, public IQgPanelFactory
{
    Q_OBJECT
    Q_INTERFACES(IQgPanelFactory)
    Q_PLUGIN_METADATA(IID QGTCAD_IID("IQgPanelFactory")
	FILE "test_qtcad_plugin_panel.json")

    public:
	QgPluginDescriptor descriptor() const override
	{
	    QgPluginDescriptor plugin;
	    plugin.id = QStringLiteral("org.brlcad.qtcad.test.panel");
	    plugin.displayName = QStringLiteral("Test Panel");
	    plugin.category = QStringLiteral("test.panel");
	    return plugin;
	}

	QDockWidget *create(QgPluginContext *,
		QWidget *parent = nullptr) override
	{
	    QDockWidget *panel = new QDockWidget(parent);
	    panel->setWidget(new QLabel(QStringLiteral("panel"), panel));
	    return panel;
	}
};

#include "test_qtcad_plugin_panel.moc"

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
