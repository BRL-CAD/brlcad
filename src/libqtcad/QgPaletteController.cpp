/*             Q G P A L E T T E C O N T R O L L E R . C P P
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

#include "qtcad/QgPaletteController.h"
#include "qtcad/QgPluginContext.h"
#include "qtcad/QgPluginInterfaces.h"
#include "qtcad/QgPluginManager.h"
#include "qtcad/QgToolBase.h"
#include "qtcad/QgToolPalette.h"
#include "qtcad/QgView.h"

QgPaletteController::QgPaletteController(QgToolPalette *palette,
					 QgPluginManager *manager,
					 const QString &category,
					 QgPluginContext *ctx,
					 QObject *parent)
    : QObject(parent)
    , m_palette(palette)
    , m_manager(manager)
    , m_category(category)
    , m_ctx(ctx)
{
    if (m_palette) {
	connect(m_palette, &QgToolPalette::palette_element_selected,
		this, &QgPaletteController::onElementSelected);
    }
    if (m_manager) {
	connect(m_manager, &QgPluginManager::factoryRegistered,
		this, &QgPaletteController::onFactoryRegistered);
	connect(m_manager, &QgPluginManager::factoryUnregistered,
		this, &QgPaletteController::onFactoryUnregistered);
    }
}

QgPaletteController::~QgPaletteController() = default;

void
QgPaletteController::populate()
{
    if (!m_palette || !m_manager)
	return;
    QList<IQgToolFactory *> facs = m_manager->factories<IQgToolFactory>(m_category);
    for (IQgToolFactory *fac : facs) {
	QgPluginDescriptor d = fac->descriptor();
	if (m_tools.contains(d.id))
	    continue;
	QgToolBase *tool = fac->create(m_ctx, this);
	if (!tool)
	    continue;
	m_tools.insert(d.id, tool);
	QgToolPaletteElement *el = tool->paletteElement();
	if (!el) {
	    delete tool;
	    m_tools.remove(d.id);
	    continue;
	}
	m_elementToId.insert(el, d.id);
	m_palette->addElement(el);
    }
}

void
QgPaletteController::setActiveView(QgView *view)
{
    if (m_view == view)
	return;
    if (m_currentTool) {
	detachFilter(m_currentTool);
    }
    m_view = view;
    if (m_currentTool) {
	attachFilter(m_currentTool);
    }
}

void
QgPaletteController::attachFilter(QgToolBase *tool)
{
    if (!tool || !m_view)
	return;
    QObject *obj = tool->viewEventFilter();
    if (!obj)
	return;
    IQgViewEventFilter *f = qobject_cast<IQgViewEventFilter *>(obj);
    if (f)
	f->attachToView(m_view);
}

void
QgPaletteController::detachFilter(QgToolBase *tool)
{
    if (!tool || !m_view)
	return;
    QObject *obj = tool->viewEventFilter();
    if (!obj)
	return;
    IQgViewEventFilter *f = qobject_cast<IQgViewEventFilter *>(obj);
    if (f)
	f->detachFromView(m_view);
}

void
QgPaletteController::onElementSelected(QgToolPaletteElement *el)
{
    QString id = m_elementToId.value(el);
    QgToolBase *tool = id.isEmpty() ? nullptr : m_tools.value(id, nullptr);
    if (tool == m_currentTool)
	return;
    if (m_currentTool)
	detachFilter(m_currentTool);
    m_currentTool = tool;
    if (m_currentTool)
	attachFilter(m_currentTool);
    emit currentToolChanged(m_currentTool);
}

void
QgPaletteController::onFactoryRegistered(const QString & /*id*/)
{
    /* Re-populate to pick up the new factory if it matches our
     * category.  populate() is idempotent. */
    populate();
}

void
QgPaletteController::onFactoryUnregistered(const QString &id)
{
    QgToolBase *tool = m_tools.take(id);
    if (!tool)
	return;
    if (tool == m_currentTool) {
	detachFilter(m_currentTool);
	m_currentTool = nullptr;
	emit currentToolChanged(nullptr);
    }
    /* Remove element mapping. */
    for (auto it = m_elementToId.begin(); it != m_elementToId.end(); ) {
	if (it.value() == id) {
	    if (m_palette)
		m_palette->deleteElement(it.key());
	    it = m_elementToId.erase(it);
	} else {
	    ++it;
	}
    }
    delete tool;
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
