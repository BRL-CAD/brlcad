/*   Q G W I N D O W E X T E N S I O N C O N T R O L L E R . C P P
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

#include <QAction>
#include <QDialog>
#include <QDockWidget>
#include <QMainWindow>
#include <QMenu>

#include "qtcad/QgPluginContext.h"
#include "qtcad/QgPluginInterfaces.h"
#include "qtcad/QgPluginManager.h"
#include "qtcad/QgWindowExtensionController.h"

QgWindowExtensionController::QgWindowExtensionController(
	QMainWindow *window, QgPluginManager *manager,
	QgPluginContext *context, QObject *parent)
    : QObject(parent), m_window(window), m_manager(manager),
      m_context(context)
{
    if (m_manager) {
	connect(m_manager, &QgPluginManager::pluginsAboutToUnload,
		this, &QgWindowExtensionController::clear,
		Qt::DirectConnection);
	connect(m_manager, &QgPluginManager::catalogChanged,
		this, &QgWindowExtensionController::rebuild);
    }
}

QgWindowExtensionController::~QgWindowExtensionController()
{
    clear();
}

void
QgWindowExtensionController::setPanelMenu(QMenu *menu,
	const QString &category)
{
    clearPanels();
    m_panelMenu = menu;
    m_panelCategory = category;
}

void
QgWindowExtensionController::setDialogMenu(QMenu *menu,
	const QString &category)
{
    clearDialogs();
    m_dialogMenu = menu;
    m_dialogCategory = category;
    if (m_dialogMenu) {
	m_dialogMenu->setEnabled(false);
	m_dialogMenu->menuAction()->setVisible(false);
    }
}

void
QgWindowExtensionController::removePanel(const QString &id)
{
    QPointer<QDockWidget> panel = m_panels.take(id);
    if (panel) {
	if (m_panelMenu)
	    m_panelMenu->removeAction(panel->toggleViewAction());
	if (m_window)
	    m_window->removeDockWidget(panel);
	/* Plugin-created objects must be destroyed before their library is
	 * synchronously unloaded. */
	delete panel.data();
    }

    if (m_panels.isEmpty() && m_panelSeparator) {
	if (m_panelMenu)
	    m_panelMenu->removeAction(m_panelSeparator);
	delete m_panelSeparator.data();
	m_panelSeparator = nullptr;
    }
}

void
QgWindowExtensionController::clearPanels()
{
    const QStringList ids = m_panels.keys();
    for (const QString &id : ids)
	removePanel(id);
}

void
QgWindowExtensionController::populatePanels()
{
    if (!m_window || !m_manager || !m_panelMenu ||
	    m_panelCategory.isEmpty())
	return;

    QSet<QString> availableIds;
    const QList<QgPluginDescriptor> descriptors =
	m_manager->descriptors(m_panelCategory);
    for (const QgPluginDescriptor &descriptor : descriptors) {
	QObject *instance = m_manager->instance(descriptor.id);
	IQgPanelFactory *factory = instance ?
	    qobject_cast<IQgPanelFactory *>(instance) : nullptr;
	if (!factory)
	    continue;
	availableIds.insert(descriptor.id);
	if (m_panels.value(descriptor.id))
	    continue;
	m_panels.remove(descriptor.id);

	QDockWidget *panel = factory->create(m_context, m_window);
	if (!panel)
	    continue;
	if (panel->objectName().isEmpty())
	    panel->setObjectName(descriptor.id);
	if (panel->windowTitle().isEmpty())
	    panel->setWindowTitle(descriptor.displayName);
	panel->setProperty("qgPluginId", descriptor.id);
	panel->setProperty("qgTestId",
	    descriptor.id + QStringLiteral(".panel"));
	panel->toggleViewAction()->setProperty("qgTestId",
	    descriptor.id + QStringLiteral(".activate"));

	if (!m_panelSeparator)
	    m_panelSeparator = m_panelMenu->addSeparator();
	m_window->addDockWidget(Qt::LeftDockWidgetArea, panel);
	panel->hide();
	m_panelMenu->addAction(panel->toggleViewAction());
	m_panels.insert(descriptor.id, panel);
    }

    const QStringList existingIds = m_panels.keys();
    for (const QString &id : existingIds) {
	if (!availableIds.contains(id))
	    removePanel(id);
    }
}

void
QgWindowExtensionController::removeDialogExtension(const QString &id)
{
    QPointer<QAction> action = m_dialogActions.take(id);
    if (action) {
	if (m_dialogMenu)
	    m_dialogMenu->removeAction(action);
	delete action.data();
    }

    /* Deleting a dialog emits destroyed(), so remove the index before the
	 * callbacks run. */
    const QSet<QDialog *> dialogs = m_dialogs.take(id);
    for (QDialog *dialog : dialogs)
	delete dialog;
}

void
QgWindowExtensionController::clearDialogs()
{
    QSet<QString> ids;
    const QStringList actionIds = m_dialogActions.keys();
    for (const QString &id : actionIds)
	ids.insert(id);
    const QStringList dialogIds = m_dialogs.keys();
    for (const QString &id : dialogIds)
	ids.insert(id);
    for (const QString &id : ids)
	removeDialogExtension(id);

    if (m_dialogMenu) {
	m_dialogMenu->setEnabled(false);
	m_dialogMenu->menuAction()->setVisible(false);
    }
}

void
QgWindowExtensionController::populateDialogs()
{
    if (!m_window || !m_manager || !m_dialogMenu ||
	    m_dialogCategory.isEmpty())
	return;

    QSet<QString> availableIds;
    const QList<QgPluginDescriptor> descriptors =
	m_manager->descriptors(m_dialogCategory);
    for (const QgPluginDescriptor &descriptor : descriptors) {
	QObject *instance = m_manager->instance(descriptor.id);
	IQgDialogFactory *factory = instance ?
	    qobject_cast<IQgDialogFactory *>(instance) : nullptr;
	if (!factory)
	    continue;
	availableIds.insert(descriptor.id);
	if (m_dialogActions.value(descriptor.id))
	    continue;
	m_dialogActions.remove(descriptor.id);

	QAction *action = new QAction(descriptor.displayName, m_dialogMenu);
	action->setData(descriptor.id);
	action->setProperty("qgTestId",
	    descriptor.id + QStringLiteral(".activate"));
	if (!descriptor.description.isEmpty())
	    action->setToolTip(descriptor.description);
	connect(action, &QAction::triggered, this,
	    [this, id = descriptor.id]() { launchDialog(id); });
	m_dialogMenu->addAction(action);
	m_dialogActions.insert(descriptor.id, action);
    }

    const QStringList existingIds = m_dialogActions.keys();
    for (const QString &id : existingIds) {
	if (!availableIds.contains(id))
	    removeDialogExtension(id);
    }

    m_dialogMenu->setEnabled(!m_dialogActions.isEmpty());
    m_dialogMenu->menuAction()->setVisible(!m_dialogActions.isEmpty());
}

void
QgWindowExtensionController::launchDialog(const QString &id)
{
    if (!m_window || !m_manager)
	return;

    QObject *instance = m_manager->instance(id);
    IQgDialogFactory *factory = instance ?
	qobject_cast<IQgDialogFactory *>(instance) : nullptr;
    if (!factory)
	return;

    QDialog *dialog = factory->create(m_context, m_window);
    if (!dialog)
	return;

    const QgPluginDescriptor descriptor = m_manager->descriptor(id);
    if (dialog->windowTitle().isEmpty())
	dialog->setWindowTitle(descriptor.displayName);
    dialog->setProperty("qgPluginId", id);
    dialog->setProperty("qgTestId", id + QStringLiteral(".dialog"));
    dialog->setAttribute(Qt::WA_DeleteOnClose, true);
    m_dialogs[id].insert(dialog);
    connect(dialog, &QObject::destroyed, this, [this, id, dialog]() {
	auto pluginDialogs = m_dialogs.find(id);
	if (pluginDialogs == m_dialogs.end())
	    return;
	pluginDialogs.value().remove(dialog);
	if (pluginDialogs.value().isEmpty())
	    m_dialogs.erase(pluginDialogs);
    });
    dialog->show();
    dialog->raise();
    dialog->activateWindow();
}

void
QgWindowExtensionController::rebuild()
{
    populatePanels();
    populateDialogs();
}

void
QgWindowExtensionController::clear()
{
    clearPanels();
    clearDialogs();
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
