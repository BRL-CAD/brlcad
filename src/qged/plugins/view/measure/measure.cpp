/*                     M E A S U R E . C P P
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
/** @file measure.cpp
 *
 */

#include <QIcon>
#include <QPixmap>

#include "qtcad/QgPluginContext.h"
#include "qtcad/QgPluginInterfaces.h"
#include "qtcad/QgToolBase.h"
#include "qtcad/QgToolPalette.h"
#include "qtcad/QgView.h"
#include "QgEdCategories.h"
#include "CADViewMeasure.h"

class ViewMeasureTool : public QgToolBase, public IQgViewEventFilter
{
    Q_OBJECT
    Q_INTERFACES(IQgViewEventFilter)

    public:
	explicit ViewMeasureTool(QgPluginContext *context,
		QObject *parent = nullptr)
	    : QgToolBase(context, parent)
	{
	}

	QgToolPaletteElement *paletteElement() override
	{
	    QgToolPaletteElement *element = QgToolBase::paletteElement();
	    if (m_measure && element && !m_measureWired) {
		connect(m_measure, &CADViewMeasure::view_updated,
			element, &QgToolPaletteElement::element_view_changed);
		m_measureWired = true;
	    }
	    return element;
	}

	QWidget *createWidget() override
	{
	    m_measure = new CADViewMeasure();
	    m_measure->setContext(context());
	    return m_measure;
	}

	QIcon *createIcon() override
	{
	    return new QIcon(QPixmap(":measure.svg"));
	}

	QObject *viewEventFilter() override { return this; }

	void attachToView(QgView *view) override
	{
	    if (m_measure)
		m_measure->attachToView(view);
	}

	void detachFromView(QgView *view) override
	{
	    if (m_measure)
		m_measure->detachFromView(view);
	}

    private:
	CADViewMeasure *m_measure = nullptr;
	bool m_measureWired = false;
};

class ViewMeasureFactory : public QObject, public IQgToolFactory
{
    Q_OBJECT
    Q_INTERFACES(IQgToolFactory)
    Q_PLUGIN_METADATA(IID QGTCAD_IID("IQgToolFactory") FILE "measure.json")

    public:
	QgPluginDescriptor descriptor() const override
	{
	    QgPluginDescriptor plugin;
	    plugin.id = QStringLiteral("org.brlcad.qged.view.measure");
	    plugin.displayName = QStringLiteral("View Measure");
	    plugin.category = QStringLiteral(QGED_CATEGORY_VIEW);
	    plugin.iconName = QStringLiteral(":measure.svg");
	    plugin.sortKey = 3;
	    plugin.requiresView = true;
	    plugin.description = QStringLiteral(
		    "Measures distances and angles using 2D or 3D points.");
	    plugin.vendor = QStringLiteral("BRL-CAD");
	    plugin.version = QStringLiteral("1.0");
	    return plugin;
	}

	QgToolBase *create(QgPluginContext *context,
		QObject *parent = nullptr) override
	{
	    return new ViewMeasureTool(context, parent);
	}
};

#include "measure.moc"



// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
