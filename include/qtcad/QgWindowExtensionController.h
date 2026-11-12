/*     Q G W I N D O W E X T E N S I O N C O N T R O L L E R . H
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

#ifndef QGWINDOWEXTENSIONCONTROLLER_H
#define QGWINDOWEXTENSIONCONTROLLER_H

#include <QHash>
#include <QObject>
#include <QPointer>
#include <QSet>
#include <QString>

#include "qtcad/defines.h"

class QAction;
class QDialog;
class QDockWidget;
class QMainWindow;
class QMenu;
class QgPluginContext;
class QgPluginManager;

/** Populate a main window with dock and dialog plugin extensions. */
class QTCAD_EXPORT QgWindowExtensionController : public QObject
{
    Q_DISABLE_COPY_MOVE(QgWindowExtensionController)

    public:
	/** Construct a controller for extensions owned by @p window. */
	QgWindowExtensionController(QMainWindow *window,
		QgPluginManager *manager, QgPluginContext *context,
		QObject *parent = nullptr);
	~QgWindowExtensionController() override;

	/** Configure the menu and plugin category used for dock panels. */
	void setPanelMenu(QMenu *menu, const QString &category);
	/** Configure the menu and plugin category used for dialogs. */
	void setDialogMenu(QMenu *menu, const QString &category);
	/** Recreate enabled extensions from the current plugin catalog. */
	void rebuild();
	/** Synchronously destroy all objects implemented by plugins. */
	void clear();

    private:
	void removePanel(const QString &id);
	void clearPanels();
	void populatePanels();
	void removeDialogExtension(const QString &id);
	void clearDialogs();
	void populateDialogs();
	void launchDialog(const QString &id);

	QPointer<QMainWindow> m_window;
	QPointer<QgPluginManager> m_manager;
	QgPluginContext *m_context = nullptr;
	QPointer<QMenu> m_panelMenu;
	QPointer<QMenu> m_dialogMenu;
	QString m_panelCategory;
	QString m_dialogCategory;
	QPointer<QAction> m_panelSeparator;
	QHash<QString, QPointer<QDockWidget> > m_panels;
	QHash<QString, QPointer<QAction> > m_dialogActions;
	QHash<QString, QSet<QDialog *> > m_dialogs;
};

#endif /* QGWINDOWEXTENSIONCONTROLLER_H */

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
