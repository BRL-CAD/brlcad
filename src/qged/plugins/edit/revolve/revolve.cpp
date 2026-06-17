/*                  R E V O L V E . C P P
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
/** @file revolve.cpp
 *
 * IQgToolFactory implementation for the edit/revolve palette tool.
 *
 */

#include <QMetaObject>
#include <QIcon>
#include <QPixmap>
#include <QSizePolicy>

#include "qtcad/QgPluginContext.h"
#include "qtcad/QgToolPalette.h"
#include "qtcad/QgView.h"
#include "EditRevolveFactory.h"

EditRevolveTool::EditRevolveTool(QgPluginContext *ctx, QObject *parent)
    : QgToolBase(ctx, parent)
{
}

QgToolPaletteElement *
EditRevolveTool::paletteElement()
{
    QgToolPaletteElement *el = QgToolBase::paletteElement();
    if (m_rev && el && !m_extra_wired) {
	QObject::connect(m_rev, &QRevolve::view_updated,
			 el, &QgToolPaletteElement::element_view_changed);
	m_extra_wired = true;
    }
    return el;
}

QWidget *
EditRevolveTool::createWidget()
{
    m_rev = new QRevolve();
    m_rev->setContext(m_ctx);
    m_rev->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
    return m_rev;
}

QIcon *
EditRevolveTool::createIcon()
{
    return new QIcon(QPixmap(":revolve.svg"));
}

void
EditRevolveTool::refresh()
{
    if (!m_rev)
	return;
    m_rev->blockSignals(true);
    QMetaObject::invokeMethod(m_rev, "update_obj_wireframe", Qt::DirectConnection);
    m_rev->blockSignals(false);
}

void
EditRevolveTool::onDbChanged()
{
    refresh();
}

void
EditRevolveTool::onViewChanged()
{
    refresh();
}

void
EditRevolveTool::attachToView(QgView *view)
{
    if (m_rev && view)
	view->installEventFilter(m_rev);
}

void
EditRevolveTool::detachFromView(QgView *view)
{
    if (m_rev && view)
	view->removeEventFilter(m_rev);
}

QgPluginDescriptor
EditRevolveFactory::descriptor() const
{
    QgPluginDescriptor d;
    d.id = "org.brlcad.qged.edit.revolve";
    d.displayName = "Revolve Edit";
    d.category = "qged.object";
    d.iconName = ":revolve.svg";
    d.sortKey = 1200;
    d.requiresOpenDb = true;
    d.requiresView = true;
    d.description = "Preview and edit revolve primitives in the active database.";
    d.vendor = "BRL-CAD";
    d.version = "1.0";
    return d;
}

QgToolBase *
EditRevolveFactory::create(QgPluginContext *ctx, QObject *parent)
{
    return new EditRevolveTool(ctx, parent);
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
