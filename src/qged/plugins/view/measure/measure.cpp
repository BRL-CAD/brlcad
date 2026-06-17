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
 * IQgToolFactory implementation for the view-measure palette tool.
 * Replaces the legacy qged_plugin_info ABI.
 */

#include <QGroupBox>
#include <QCheckBox>
#include <QIcon>
#include <QPixmap>

#include "qtcad/QgPluginContext.h"
#include "qtcad/QgPluginDescriptor.h"
#include "qtcad/QgToolPalette.h"
#include "qtcad/QgView.h"
#include "ViewMeasureFactory.h"

/* ------------------------------------------------------------------ */
/* ViewMeasureTool                                                      */
/* ------------------------------------------------------------------ */

ViewMeasureTool::ViewMeasureTool(QgPluginContext *ctx, QObject *parent)
    : QgToolBase(ctx, parent)
{
}

/* Override to wire the view_updated -> element_view_changed signal AFTER
 * the element has been created by the base class. */
QgToolPaletteElement *
ViewMeasureTool::paletteElement()
{
    QgToolPaletteElement *el = QgToolBase::paletteElement();
    if (m_measure && el && !m_extra_wired) {
	QObject::connect(m_measure, &CADViewMeasure::view_updated,
			 el, &QgToolPaletteElement::element_view_changed);
	m_extra_wired = true;
    }
    return el;
}

QWidget *
ViewMeasureTool::createWidget()
{
    m_measure = new CADViewMeasure();
    m_measure->setContext(m_ctx);
    return m_measure;
}

QIcon *
ViewMeasureTool::createIcon()
{
    return new QIcon(QPixmap(":measure.svg"));
}

void
ViewMeasureTool::refresh()
{
    /* The measure tool is driven by user mouse events; no explicit
     * refresh action is needed on view update. */
}

/* IQgViewEventFilter: install/remove the measure widget as an event
 * filter on the active view.  The widget's own eventFilter() handles
 * all mouse-based measurement logic. */
void
ViewMeasureTool::attachToView(QgView *view)
{
    if (m_measure && view)
	view->installEventFilter(m_measure);
}

void
ViewMeasureTool::detachFromView(QgView *view)
{
    if (m_measure && view)
	view->removeEventFilter(m_measure);
}

/* ------------------------------------------------------------------ */
/* ViewMeasureFactory                                                   */
/* ------------------------------------------------------------------ */

QgPluginDescriptor
ViewMeasureFactory::descriptor() const
{
    QgPluginDescriptor d;
    d.id           = "org.brlcad.qged.view.measure";
    d.displayName  = "View Measure";
    d.category     = "qged.view";
    d.iconName     = ":measure.svg";
    d.sortKey      = 3;
    d.requiresView = true;
    d.description  = "Measure distances and angles in the view using 2D or 3D hit points.";
    d.vendor       = "BRL-CAD";
    d.version      = "1.0";
    return d;
}

QgToolBase *
ViewMeasureFactory::create(QgPluginContext *ctx, QObject *parent)
{
    return new ViewMeasureTool(ctx, parent);
}


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
