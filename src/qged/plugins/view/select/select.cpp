/*                     S E L E C T . C P P
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
/** @file select.cpp
 *
 */

#include <QIcon>
#include <QPixmap>

#include "qtcad/QgPluginContext.h"
#include "qtcad/QgPluginInterfaces.h"
#include "qtcad/QgToolBase.h"
#include "qtcad/QgToolPalette.h"
#include "qtcad/QgView.h"
#include "CADViewSelector.h"

class ViewSelectTool : public QgToolBase, public IQgViewEventFilter
{
    Q_OBJECT
    Q_INTERFACES(IQgViewEventFilter)

    public:
	explicit ViewSelectTool(QgPluginContext *context,
		QObject *parent = nullptr)
	    : QgToolBase(context, parent)
	{
	}

	QgToolPaletteElement *paletteElement() override
	{
	    QgToolPaletteElement *element = QgToolBase::paletteElement();
	    if (m_selector && element && !m_selectorWired) {
		connect(m_selector, &CADViewSelector::view_changed,
			element, &QgToolPaletteElement::element_view_changed);
		m_selectorWired = true;
	    }
	    return element;
	}

	QWidget *createWidget() override
	{
	    m_selector = new CADViewSelector();
	    m_selector->setContext(context());
	    m_selector->do_view_update(QG_VIEW_REFRESH);
	    return m_selector;
	}

	QIcon *createIcon() override
	{
	    return new QIcon(QPixmap(":selective-tool.svg"));
	}

	QObject *viewEventFilter() override { return this; }

	void attachToView(QgView *view) override
	{
	    if (m_selector)
		m_selector->attachToView(view);
	}

	void detachFromView(QgView *view) override
	{
	    if (m_selector)
		m_selector->detachFromView(view);
	}

    public slots:
	void refresh() override
	{
	    if (m_selector)
		m_selector->do_view_update(QG_VIEW_REFRESH);
	}

	void onViewChanged() override
	{
	    refresh();
	}

    private:
	CADViewSelector *m_selector = nullptr;
	bool m_selectorWired = false;
};

class ViewSelectFactory : public QObject, public IQgToolFactory
{
    Q_OBJECT
    Q_INTERFACES(IQgToolFactory)
    Q_PLUGIN_METADATA(IID QGTCAD_IID("IQgToolFactory") FILE "select.json")

    public:
	QgPluginDescriptor descriptor() const override
	{
	    QgPluginDescriptor plugin;
	    plugin.id = QStringLiteral("org.brlcad.qged.view.select");
	    plugin.displayName = QStringLiteral("View Select");
	    plugin.category = QStringLiteral("qged.view");
	    plugin.iconName = QStringLiteral(":selective-tool.svg");
	    plugin.sortKey = 2;
	    plugin.requiresOpenDb = true;
	    plugin.requiresView = true;
	    plugin.description = QStringLiteral(
		    "Selects, draws, or erases geometry using view gestures.");
	    plugin.vendor = QStringLiteral("BRL-CAD");
	    plugin.version = QStringLiteral("1.0");
	    return plugin;
	}

	QgToolBase *create(QgPluginContext *context,
		QObject *parent = nullptr) override
	{
	    return new ViewSelectTool(context, parent);
	}
};

#include "select.moc"



// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
