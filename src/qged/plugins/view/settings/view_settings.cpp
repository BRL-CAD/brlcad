/*                 V I E W _ S E T T I N G S . C P P
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
/** @file view_settings.cpp
 *
 * IQgToolFactory implementation for the view-settings palette tool.
 * Replaces the legacy qged_plugin_info ABI.
 */

#include <QGroupBox>
#include <QCheckBox>
#include <QIcon>
#include <QPixmap>

#include "qtcad/QgPluginContext.h"
#include "qtcad/QgPluginDescriptor.h"
#include "qtcad/QgToolPalette.h"
#include "ViewSettingsFactory.h"

/* ------------------------------------------------------------------ */
/* ViewSettingsTool                                                     */
/* ------------------------------------------------------------------ */

ViewSettingsTool::ViewSettingsTool(QgPluginContext *ctx, QObject *parent)
    : QgToolBase(ctx, parent)
{
}

/* Override to wire the settings->element_view_changed signal AFTER the
 * element has been created by the base class.  Calling paletteElement()
 * from inside createWidget() would be re-entrant and must be avoided. */
QgToolPaletteElement *
ViewSettingsTool::paletteElement()
{
    QgToolPaletteElement *el = QgToolBase::paletteElement();
    if (m_settings && el && !m_extra_wired) {
	QObject::connect(m_settings, &CADViewSettings::settings_changed,
			 el, &QgToolPaletteElement::element_view_changed);
	m_extra_wired = true;
    }
    return el;
}

QWidget *
ViewSettingsTool::createWidget()
{
    m_settings = new CADViewSettings();
    m_settings->setContext(m_ctx);
    return m_settings;
}

QIcon *
ViewSettingsTool::createIcon()
{
    return new QIcon(QPixmap(":settings.svg"));
}

void
ViewSettingsTool::refresh()
{
    if (m_settings)
	m_settings->checkbox_refresh(0);
}

/* ------------------------------------------------------------------ */
/* ViewSettingsFactory                                                  */
/* ------------------------------------------------------------------ */

QgPluginDescriptor
ViewSettingsFactory::descriptor() const
{
    QgPluginDescriptor d;
    d.id           = "org.brlcad.qged.view.settings";
    d.displayName  = "View Settings";
    d.category     = "qged.view";
    d.iconName     = ":settings.svg";
    d.sortKey      = 1;
    d.requiresView = true;
    d.description  = "Controls faceplate elements: adaptive plotting, ADC, center dot, grid, axes, scale, framebuffer mode, and view parameter display.";
    d.vendor       = "BRL-CAD";
    d.version      = "1.0";
    return d;
}

QgToolBase *
ViewSettingsFactory::create(QgPluginContext *ctx, QObject *parent)
{
    return new ViewSettingsTool(ctx, parent);
}


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
