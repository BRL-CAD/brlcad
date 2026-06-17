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
#include "qtcad/QgToolPalette.h"
#include "qtcad/QgView.h"
#include "PolygonModifyFactory.h"

PolygonModifyTool::PolygonModifyTool(QgPluginContext *ctx, QObject *parent)
    : QgToolBase(ctx, parent)
{
}

QgToolPaletteElement *
PolygonModifyTool::paletteElement()
{
    QgToolPaletteElement *el = QgToolBase::paletteElement();
    if (m_modify && el && !m_extra_wired) {
	QObject::connect(m_modify, &QPolyMod::settings_changed,
			 el, &QgToolPaletteElement::element_view_changed);
	QObject::connect(m_modify, &QPolyMod::view_updated,
			 el, &QgToolPaletteElement::element_view_changed);
	QObject::connect(m_modify, &QPolyMod::view_updated,
			 m_modify, &QPolyMod::mod_names_reset);
	QObject::connect(el, &QgToolPaletteElement::element_unhide,
			 m_modify, &QPolyMod::mod_names_reset);
	m_extra_wired = true;
    }
    return el;
}

QWidget *
PolygonModifyTool::createWidget()
{
    m_modify = new QPolyMod();
    m_modify->setContext(m_ctx);
    m_modify->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
    return m_modify;
}

QIcon *
PolygonModifyTool::createIcon()
{
    return new QIcon(QPixmap(":poly_modify.svg"));
}

void
PolygonModifyTool::refresh()
{
    if (m_modify) {
	m_modify->checkbox_refresh(0);
	m_modify->mod_names_reset();
    }
}

void
PolygonModifyTool::attachToView(QgView *view)
{
    if (m_modify && view)
	view->installEventFilter(m_modify);
}

void
PolygonModifyTool::detachFromView(QgView *view)
{
    if (m_modify && view)
	view->removeEventFilter(m_modify);
}

QgPluginDescriptor
PolygonModifyFactory::descriptor() const
{
    QgPluginDescriptor d;
    d.id = "org.brlcad.qged.polygon.modify";
    d.displayName = "Polygon Modify";
    d.category = "qged.view";
    d.iconName = ":poly_modify.svg";
    d.sortKey = 101;
    d.requiresView = true;
    d.description = "Select, update, boolean-combine, align, and delete view polygons.";
    d.vendor = "BRL-CAD";
    d.version = "1.0";
    return d;
}

QgToolBase *
PolygonModifyFactory::create(QgPluginContext *ctx, QObject *parent)
{
    return new PolygonModifyTool(ctx, parent);
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
