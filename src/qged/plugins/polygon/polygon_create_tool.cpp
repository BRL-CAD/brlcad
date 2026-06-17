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
#include "qtcad/QgToolPalette.h"
#include "qtcad/QgView.h"
#include "PolygonCreateFactory.h"

PolygonCreateTool::PolygonCreateTool(QgPluginContext *ctx, QObject *parent)
    : QgToolBase(ctx, parent)
{
}

QgToolPaletteElement *
PolygonCreateTool::paletteElement()
{
    QgToolPaletteElement *el = QgToolBase::paletteElement();
    if (m_create && el && !m_extra_wired) {
	QObject::connect(m_create, &QPolyCreate::settings_changed,
			 el, &QgToolPaletteElement::element_view_changed);
	QObject::connect(m_create, &QPolyCreate::view_updated,
			 el, &QgToolPaletteElement::element_view_changed);
	m_extra_wired = true;
    }
    return el;
}

QWidget *
PolygonCreateTool::createWidget()
{
    m_create = new QPolyCreate();
    m_create->setContext(m_ctx);
    m_create->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
    return m_create;
}

QIcon *
PolygonCreateTool::createIcon()
{
    return new QIcon(QPixmap(":poly_create.svg"));
}

void
PolygonCreateTool::refresh()
{
    if (m_create)
	m_create->checkbox_refresh(0);
}

void
PolygonCreateTool::attachToView(QgView *view)
{
    if (m_create && view)
	view->installEventFilter(m_create);
}

void
PolygonCreateTool::detachFromView(QgView *view)
{
    if (m_create && view)
	view->removeEventFilter(m_create);
}

QgPluginDescriptor
PolygonCreateFactory::descriptor() const
{
    QgPluginDescriptor d;
    d.id = "org.brlcad.qged.polygon.create";
    d.displayName = "Polygon Create";
    d.category = "qged.view";
    d.iconName = ":poly_create.svg";
    d.sortKey = 100;
    d.requiresView = true;
    d.description = "Create view polygons, copy view polygons, or import sketch geometry as polygons.";
    d.vendor = "BRL-CAD";
    d.version = "1.0";
    return d;
}

QgToolBase *
PolygonCreateFactory::create(QgPluginContext *ctx, QObject *parent)
{
    return new PolygonCreateTool(ctx, parent);
}

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
