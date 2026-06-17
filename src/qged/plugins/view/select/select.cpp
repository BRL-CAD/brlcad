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
 * IQgToolFactory implementation for the view-select palette tool.
 * Replaces the legacy qged_plugin_info ABI.
 */

#include <QGroupBox>
#include <QCheckBox>
#include <QIcon>
#include <QPixmap>
#include <QtGlobal>

#include "qtcad/QgPluginContext.h"
#include "qtcad/QgPluginDescriptor.h"
#include "qtcad/QgToolPalette.h"
#include "qtcad/QgView.h"
#include "ViewSelectFactory.h"

/* ------------------------------------------------------------------ */
/* ViewSelectTool                                                       */
/* ------------------------------------------------------------------ */

ViewSelectTool::ViewSelectTool(QgPluginContext *ctx, QObject *parent)
    : QgToolBase(ctx, parent)
{
}

/* Override to wire the view_changed -> element_view_changed signal AFTER
 * the element has been created by the base class. */
QgToolPaletteElement *
ViewSelectTool::paletteElement()
{
    QgToolPaletteElement *el = QgToolBase::paletteElement();
    if (m_selector && el && !m_extra_wired) {
	QObject::connect(m_selector, &CADViewSelector::view_changed,
			 el, &QgToolPaletteElement::element_view_changed);
	m_extra_wired = true;
    }
    return el;
}

QWidget *
ViewSelectTool::createWidget()
{
    m_selector = new CADViewSelector();
    m_selector->setContext(m_ctx);
    return m_selector;
}

QIcon *
ViewSelectTool::createIcon()
{
    return new QIcon(QPixmap(":selective-tool.svg"));
}

void
ViewSelectTool::refresh()
{
    if (m_selector)
	m_selector->do_view_update(QgViewUpdateFlags{});
}

/* IQgViewEventFilter: install/remove the selector widget as an event
 * filter on the active view.  The widget's own eventFilter() handles
 * all mouse-based selection logic. */
void
ViewSelectTool::attachToView(QgView *view)
{
    if (m_selector && view)
	view->installEventFilter(m_selector);
}

void
ViewSelectTool::detachFromView(QgView *view)
{
    if (m_selector && view)
	view->removeEventFilter(m_selector);
}

/* ------------------------------------------------------------------ */
/* ViewSelectFactory                                                    */
/* ------------------------------------------------------------------ */

QgPluginDescriptor
ViewSelectFactory::descriptor() const
{
    QgPluginDescriptor d;
    d.id             = "org.brlcad.qged.view.select";
    d.displayName    = "View Select";
    d.category       = "qged.view";
    d.iconName       = ":selective-tool.svg";
    d.sortKey        = 2;
    d.requiresOpenDb = true;
    d.requiresView   = true;
    d.description    = "Select objects in the scene by point, rectangle, or ray test; add to or remove from selection sets; draw or erase selected geometry.";
    d.vendor         = "BRL-CAD";
    d.version        = "1.0";
    return d;
}

QgToolBase *
ViewSelectFactory::create(QgPluginContext *ctx, QObject *parent)
{
    return new ViewSelectTool(ctx, parent);
}


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
