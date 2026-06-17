/*                         E L L . C P P
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
/** @file ell.cpp
 *
 * IQgToolFactory implementation for the edit/ell palette tool.
 */

#include <QMetaObject>
#include <QIcon>
#include <QPixmap>
#include <QSizePolicy>

#include "qtcad/QgPluginContext.h"
#include "qtcad/QgToolPalette.h"
#include "qtcad/QgView.h"
#include "EditEllFactory.h"

EditEllTool::EditEllTool(QgPluginContext *ctx, QObject *parent)
    : QgToolBase(ctx, parent)
{
}

QgToolPaletteElement *
EditEllTool::paletteElement()
{
    QgToolPaletteElement *el = QgToolBase::paletteElement();
    if (m_ell && el && !m_extra_wired) {
	QObject::connect(m_ell, &QEll::view_updated,
			 el, &QgToolPaletteElement::element_view_changed);
	m_extra_wired = true;
    }
    return el;
}

QWidget *
EditEllTool::createWidget()
{
    m_ell = new QEll();
    m_ell->setContext(m_ctx);
    m_ell->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
    return m_ell;
}

QIcon *
EditEllTool::createIcon()
{
    return new QIcon(QPixmap(":ell.svg"));
}

void
EditEllTool::refresh()
{
    if (!m_ell)
	return;
    /* Block view_updated during refresh so that update_obj_wireframe()
     * cannot re-emit view_updated -> element_view_changed -> view_update,
     * which would create an infinite recursion.  We are already inside a
     * view_update handler, so the display will be refreshed by the ongoing
     * update pass without an extra signal. */
    m_ell->blockSignals(true);
    QMetaObject::invokeMethod(m_ell, "update_obj_wireframe", Qt::DirectConnection);
    m_ell->blockSignals(false);
}

void
EditEllTool::onDbChanged()
{
    refresh();
}

void
EditEllTool::onViewChanged()
{
    refresh();
}

void
EditEllTool::attachToView(QgView *view)
{
    if (m_ell && view)
	view->installEventFilter(m_ell);
}

void
EditEllTool::detachFromView(QgView *view)
{
    if (m_ell && view)
	view->removeEventFilter(m_ell);
}

QgPluginDescriptor
EditEllFactory::descriptor() const
{
    QgPluginDescriptor d;
    d.id = "org.brlcad.qged.edit.ell";
    d.displayName = "Ell Edit";
    d.category = "qged.object";
    d.iconName = ":ell.svg";
    d.sortKey = 1000;
    d.requiresOpenDb = true;
    d.requiresView = true;
    d.description = "Preview and edit ellipsoid primitives in the active database.";
    d.vendor = "BRL-CAD";
    d.version = "1.0";
    return d;
}

QgToolBase *
EditEllFactory::create(QgPluginContext *ctx, QObject *parent)
{
    return new EditEllTool(ctx, parent);
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
