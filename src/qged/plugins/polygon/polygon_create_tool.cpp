/*             P O L Y G O N _ C R E A T E _ T O O L . C P P
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

#include <QIcon>
#include <QPixmap>
#include <QSizePolicy>

#include "qtcad/QgPluginContext.h"
#include "qtcad/QgPluginInterfaces.h"
#include "qtcad/QgToolBase.h"
#include "qtcad/QgToolPalette.h"
#include "qtcad/QgView.h"
#include "QgEdCategories.h"
#include "QPolyCreate.h"

class PolygonCreateTool : public QgToolBase, public IQgViewEventFilter
{
    Q_OBJECT
    Q_INTERFACES(IQgViewEventFilter)

    public:
	explicit PolygonCreateTool(QgPluginContext *context,
		QObject *parent = nullptr)
	    : QgToolBase(context, parent)
	{
	}

	QgToolPaletteElement *paletteElement() override
	{
	    QgToolPaletteElement *element = QgToolBase::paletteElement();
	    if (m_create && element && !m_wired) {
		connect(m_create, &QPolyCreate::settings_changed,
			element, &QgToolPaletteElement::element_view_changed);
		connect(m_create, &QPolyCreate::view_updated,
			element, &QgToolPaletteElement::element_view_changed);
		m_wired = true;
	    }
	    return element;
	}

	QWidget *createWidget() override
	{
	    m_create = new QPolyCreate();
	    m_create->setContext(context());
	    m_create->setSizePolicy(QSizePolicy::Minimum,
		    QSizePolicy::Minimum);
	    return m_create;
	}

	QIcon *createIcon() override
	{
	    return new QIcon(QPixmap(":poly_create.svg"));
	}

	QObject *viewEventFilter() override { return this; }

	void attachToView(QgView *view) override
	{
	    if (m_create && view)
		view->add_event_filter(m_create);
	}

	void detachFromView(QgView *view) override
	{
	    if (m_create && view)
		view->clear_event_filter(m_create);
	}

    public slots:
	void refresh() override
	{
	    if (m_create)
		m_create->checkbox_refresh(0);
	}

	void onViewChanged() override { refresh(); }

    private:
	QPolyCreate *m_create = nullptr;
	bool m_wired = false;
};

class PolygonCreateFactory : public QObject, public IQgToolFactory
{
    Q_OBJECT
    Q_INTERFACES(IQgToolFactory)
    Q_PLUGIN_METADATA(IID QGTCAD_IID("IQgToolFactory")
	    FILE "polygon_create.json")

    public:
	QgPluginDescriptor descriptor() const override
	{
	    QgPluginDescriptor plugin;
	    plugin.id = QStringLiteral("org.brlcad.qged.polygon.create");
	    plugin.displayName = QStringLiteral("Polygon Create");
	    plugin.category = QStringLiteral(QGED_CATEGORY_VIEW);
	    plugin.iconName = QStringLiteral(":poly_create.svg");
	    plugin.sortKey = 100;
	    plugin.requiresView = true;
	    plugin.description = QStringLiteral(
		    "Creates view polygons or imports sketch geometry.");
	    plugin.vendor = QStringLiteral("BRL-CAD");
	    plugin.version = QStringLiteral("1.0");
	    return plugin;
	}

	QgToolBase *create(QgPluginContext *context,
		QObject *parent = nullptr) override
	{
	    return new PolygonCreateTool(context, parent);
	}
};

#include "polygon_create_tool.moc"

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
