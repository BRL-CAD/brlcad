/*                   Q G T O O L B A S E . C P P
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

#include "common.h"

#include <QIcon>
#include <QWidget>

#include "qtcad/QgPluginContext.h"
#include "qtcad/QgToolBase.h"
#include "qtcad/QgToolPalette.h"

QgToolBase::QgToolBase(QgPluginContext *ctx, QObject *parent)
    : QObject(parent), m_ctx(ctx)
{
}

QgToolBase::~QgToolBase() = default;

QgToolPaletteElement *
QgToolBase::paletteElement()
{
    if (m_element)
	return m_element;

    if (!m_widget)
	m_widget = createWidget();
    QIcon *icon = createIcon();

    QgToolPaletteElement *el = new QgToolPaletteElement(icon, m_widget);
    m_element = el;

    /* Wire the standard element <-> tool connections that every
     * legacy *_tool.cpp used to repeat by hand.  Subclasses override
     * refresh()/onDbChanged()/onViewChanged() to react. */
    if (!m_signals_wired) {
	connect(el, &QgToolPaletteElement::element_view_update,
		this, &QgToolBase::refresh);
	if (m_ctx && m_ctx->notifier) {
	    connect(m_ctx->notifier, &QgPluginNotifier::dbChanged,
		    this, &QgToolBase::onDbChanged);
	    connect(m_ctx->notifier, &QgPluginNotifier::viewChanged,
		    this, &QgToolBase::onViewChanged);
	    connect(m_ctx->notifier, &QgPluginNotifier::settingsChanged,
		    this, &QgToolBase::onSettingsChanged);
	}
	m_signals_wired = true;
    }

    return el;
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
