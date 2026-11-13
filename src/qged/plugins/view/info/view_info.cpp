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
 */

#include <QIcon>
#include <QPixmap>
#include <QSizePolicy>

#include "qtcad/QgKeyVal.h"
#include "qtcad/QgPluginContext.h"
#include "qtcad/QgPluginInterfaces.h"
#include "qtcad/QgToolBase.h"
#include "QgEdCategories.h"
#include "CADViewModel.h"

class ViewInfoTool : public QgToolBase
{
    Q_OBJECT

    public:
	explicit ViewInfoTool(QgPluginContext *context,
		QObject *parent = nullptr)
	    : QgToolBase(context, parent)
	{
	}

	QWidget *createWidget() override
	{
	    m_model = new CADViewModel(this);
	    m_model->setContext(context());
	    m_model->refresh(QG_VIEW_REFRESH);

	    QgKeyValView *view = new QgKeyValView(nullptr, 0);
	    view->setModel(m_model);
	    view->setSizePolicy(QSizePolicy::Expanding,
		    QSizePolicy::Expanding);
	    view->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	    return view;
	}

	QIcon *createIcon() override
	{
	    return new QIcon(QPixmap(":info.svg"));
	}

    public slots:
	void refresh() override
	{
	    if (m_model)
		m_model->refresh(QG_VIEW_REFRESH);
	}

	void onViewChanged() override
	{
	    refresh();
	}

    private:
	CADViewModel *m_model = nullptr;
};

class ViewInfoFactory : public QObject, public IQgToolFactory
{
    Q_OBJECT
    Q_INTERFACES(IQgToolFactory)
    Q_PLUGIN_METADATA(IID QGTCAD_IID("IQgToolFactory")
	FILE "view_info.json")

    public:
	QgPluginDescriptor descriptor() const override
	{
	    QgPluginDescriptor plugin;
	    plugin.id = QStringLiteral("org.brlcad.qged.view.info");
	    plugin.displayName = QStringLiteral("View Info");
	    plugin.category = QStringLiteral(QGED_CATEGORY_VIEW);
	    plugin.iconName = QStringLiteral(":info.svg");
	    plugin.sortKey = 0;
	    plugin.requiresView = true;
	    plugin.description = QStringLiteral(
		    "Displays parameters of the active view.");
	    plugin.vendor = QStringLiteral("BRL-CAD");
	    plugin.version = QStringLiteral("1.0");
	    return plugin;
	}

	QgToolBase *create(QgPluginContext *context,
		QObject *parent = nullptr) override
	{
	    return new ViewInfoTool(context, parent);
	}
};

#include "view_info.moc"



// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
