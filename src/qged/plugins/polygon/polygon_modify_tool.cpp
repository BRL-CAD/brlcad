/*             P O L Y G O N _ M O D I F Y _ T O O L . C P P
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
#include "QPolyMod.h"

class PolygonModifyTool : public QgToolBase, public IQgViewEventFilter
{
    Q_OBJECT
    Q_INTERFACES(IQgViewEventFilter)

    public:
	explicit PolygonModifyTool(QgPluginContext *context,
		QObject *parent = nullptr)
	    : QgToolBase(context, parent)
	{
	}

	QgToolPaletteElement *paletteElement() override
	{
	    QgToolPaletteElement *element = QgToolBase::paletteElement();
	    if (m_modify && element && !m_wired) {
		connect(m_modify, &QPolyMod::settings_changed,
			element, &QgToolPaletteElement::element_view_changed);
		connect(m_modify, &QPolyMod::view_updated,
			element, &QgToolPaletteElement::element_view_changed);
		connect(m_modify, &QPolyMod::view_updated,
			m_modify, &QPolyMod::mod_names_reset);
		connect(element, &QgToolPaletteElement::element_unhide,
			m_modify, &QPolyMod::mod_names_reset);
		m_wired = true;
	    }
	    return element;
	}

	QWidget *createWidget() override
	{
	    m_modify = new QPolyMod();
	    m_modify->setContext(context());
	    m_modify->setSizePolicy(QSizePolicy::Minimum,
		    QSizePolicy::Minimum);
	    m_modify->mod_names_reset();
	    return m_modify;
	}

	QIcon *createIcon() override
	{
	    return new QIcon(QPixmap(":poly_modify.svg"));
	}

	QObject *viewEventFilter() override { return this; }

	void attachToView(QgView *view) override
	{
	    if (m_modify && view)
		view->add_event_filter(m_modify);
	}

	void detachFromView(QgView *view) override
	{
	    if (m_modify && view)
		view->clear_event_filter(m_modify);
	}

    public slots:
	void refresh() override
	{
	    if (m_modify) {
		m_modify->checkbox_refresh(0);
		m_modify->mod_names_reset();
	    }
	}

	void onViewChanged() override { refresh(); }

    private:
	QPolyMod *m_modify = nullptr;
	bool m_wired = false;
};

class PolygonModifyFactory : public QObject, public IQgToolFactory
{
    Q_OBJECT
    Q_INTERFACES(IQgToolFactory)
    Q_PLUGIN_METADATA(IID QGTCAD_IID("IQgToolFactory")
	    FILE "polygon_modify.json")

    public:
	QgPluginDescriptor descriptor() const override
	{
	    QgPluginDescriptor plugin;
	    plugin.id = QStringLiteral("org.brlcad.qged.polygon.modify");
	    plugin.displayName = QStringLiteral("Polygon Modify");
	    plugin.category = QStringLiteral("qged.view");
	    plugin.iconName = QStringLiteral(":poly_modify.svg");
	    plugin.sortKey = 101;
	    plugin.requiresView = true;
	    plugin.description = QStringLiteral(
		    "Modifies, combines, aligns, or deletes view polygons.");
	    plugin.vendor = QStringLiteral("BRL-CAD");
	    plugin.version = QStringLiteral("1.0");
	    return plugin;
	}

	QgToolBase *create(QgPluginContext *context,
		QObject *parent = nullptr) override
	{
	    return new PolygonModifyTool(context, parent);
	}
};

#include "polygon_modify_tool.moc"

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
