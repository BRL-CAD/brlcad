/*                   Q G T O O L B A S E . C P P
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 */

#include "common.h"

#include <QIcon>
#include <QWidget>

#include "qtcad/QgPluginContext.h"
#include "qtcad/QgToolBase.h"
#include "qtcad/QgToolPalette.h"

QgToolBase::QgToolBase(QgPluginContext *context, QObject *parent)
    : QObject(parent), m_context(context)
{
}

QgToolBase::~QgToolBase()
{
    if (m_element && !m_element->parentWidget())
	delete m_element.data();
    else if (!m_element && m_widget && !m_widget->parentWidget())
	delete m_widget.data();
}

void
QgToolBase::setActive(bool active)
{
    if (m_active == active)
	return;
    m_active = active;
    emit activeChanged(active);
}

QgToolPaletteElement *
QgToolBase::paletteElement()
{
    if (m_element)
	return m_element;

    if (!m_widget)
	m_widget = createWidget();
    if (!m_widget)
	return nullptr;

    QIcon *icon = createIcon();
    m_element = new QgToolPaletteElement(icon, m_widget);
    delete icon;

    if (!m_signalsWired) {
	connect(m_element, &QgToolPaletteElement::element_view_update,
		this, &QgToolBase::refresh);
	if (m_context && m_context->notifier) {
	    connect(m_context->notifier, &QgPluginNotifier::dbChanged,
		    this, &QgToolBase::onDbChanged);
	    connect(m_context->notifier, &QgPluginNotifier::viewChanged,
		    this, &QgToolBase::onViewChanged);
	    connect(m_context->notifier, &QgPluginNotifier::settingsChanged,
		    this, &QgToolBase::onSettingsChanged);
	}
	m_signalsWired = true;
    }

    return m_element;
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
