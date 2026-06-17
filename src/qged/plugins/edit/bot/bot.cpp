/*                      B O T . C P P
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
/** @file bot.cpp
 *
 * IQgToolFactory implementation for the edit/bot palette tool.
 *
 */

#include <QMetaObject>
#include <QIcon>
#include <QPixmap>
#include <QSizePolicy>

#include "qtcad/QgPluginContext.h"
#include "qtcad/QgToolPalette.h"
#include "qtcad/QgView.h"
#include "EditBotFactory.h"

EditBotTool::EditBotTool(QgPluginContext *ctx, QObject *parent)
    : QgToolBase(ctx, parent)
{
}

QgToolPaletteElement *
EditBotTool::paletteElement()
{
    QgToolPaletteElement *el = QgToolBase::paletteElement();
    if (m_bot && el && !m_extra_wired) {
	QObject::connect(m_bot, &QBot::view_updated,
			 el, &QgToolPaletteElement::element_view_changed);
	m_extra_wired = true;
    }
    return el;
}

QWidget *
EditBotTool::createWidget()
{
    m_bot = new QBot();
    m_bot->setContext(m_ctx);
    m_bot->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
    return m_bot;
}

QIcon *
EditBotTool::createIcon()
{
    return new QIcon(QPixmap(":bot.svg"));
}

void
EditBotTool::refresh()
{
    if (!m_bot)
	return;
    m_bot->blockSignals(true);
    QMetaObject::invokeMethod(m_bot, "update_obj_wireframe", Qt::DirectConnection);
    m_bot->blockSignals(false);
}

void
EditBotTool::onDbChanged()
{
    refresh();
}

void
EditBotTool::onViewChanged()
{
    refresh();
}

void
EditBotTool::attachToView(QgView *view)
{
    if (m_bot && view)
	view->installEventFilter(m_bot);
}

void
EditBotTool::detachFromView(QgView *view)
{
    if (m_bot && view)
	view->removeEventFilter(m_bot);
}

QgPluginDescriptor
EditBotFactory::descriptor() const
{
    QgPluginDescriptor d;
    d.id = "org.brlcad.qged.edit.bot";
    d.displayName = "BOT Edit";
    d.category = "qged.object";
    d.iconName = ":bot.svg";
    d.sortKey = 1300;
    d.requiresOpenDb = true;
    d.requiresView = true;
    d.description = "Preview and edit bag-of-triangles (BOT) primitives in the active database.";
    d.vendor = "BRL-CAD";
    d.version = "1.0";
    return d;
}

QgToolBase *
EditBotFactory::create(QgPluginContext *ctx, QObject *parent)
{
    return new EditBotTool(ctx, parent);
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
