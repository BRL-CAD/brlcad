/*                  E X T R U D E . C P P
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
/** @file extrude.cpp
 *
 * IQgToolFactory implementation for the edit/extrude palette tool.
 *
 */

#include <QMetaObject>
#include <QIcon>
#include <QPixmap>
#include <QSizePolicy>

#include "qtcad/QgPluginContext.h"
#include "qtcad/QgToolPalette.h"
#include "qtcad/QgView.h"
#include "EditExtrudeFactory.h"

EditExtrudeTool::EditExtrudeTool(QgPluginContext *ctx, QObject *parent)
    : QgToolBase(ctx, parent)
{
}

QgToolPaletteElement *
EditExtrudeTool::paletteElement()
{
    QgToolPaletteElement *el = QgToolBase::paletteElement();
    if (m_extr && el && !m_extra_wired) {
	QObject::connect(m_extr, &QExtrude::view_updated,
			 el, &QgToolPaletteElement::element_view_changed);
	m_extra_wired = true;
    }
    return el;
}

QWidget *
EditExtrudeTool::createWidget()
{
    m_extr = new QExtrude();
    m_extr->setContext(m_ctx);
    m_extr->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
    return m_extr;
}

QIcon *
EditExtrudeTool::createIcon()
{
    /* No dedicated SVG yet — use a placeholder Qt icon. */
    return new QIcon(QPixmap(":extrude.svg"));
}

void
EditExtrudeTool::refresh()
{
    if (!m_extr)
	return;
    m_extr->blockSignals(true);
    QMetaObject::invokeMethod(m_extr, "update_obj_wireframe", Qt::DirectConnection);
    m_extr->blockSignals(false);
}

void
EditExtrudeTool::onDbChanged()
{
    refresh();
}

void
EditExtrudeTool::onViewChanged()
{
    refresh();
}

void
EditExtrudeTool::attachToView(QgView *view)
{
    if (m_extr && view)
	view->installEventFilter(m_extr);
}

void
EditExtrudeTool::detachFromView(QgView *view)
{
    if (m_extr && view)
	view->removeEventFilter(m_extr);
}

QgPluginDescriptor
EditExtrudeFactory::descriptor() const
{
    QgPluginDescriptor d;
    d.id = "org.brlcad.qged.edit.extrude";
    d.displayName = "Extrude Edit";
    d.category = "qged.object";
    d.iconName = ":extrude.svg";
    d.sortKey = 1100;
    d.requiresOpenDb = true;
    d.requiresView = true;
    d.description = "Preview and edit extrude primitives in the active database.";
    d.vendor = "BRL-CAD";
    d.version = "1.0";
    return d;
}

QgToolBase *
EditExtrudeFactory::create(QgPluginContext *ctx, QObject *parent)
{
    return new EditExtrudeTool(ctx, parent);
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
