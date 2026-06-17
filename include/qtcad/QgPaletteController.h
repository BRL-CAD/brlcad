/*               Q G P A L E T T E C O N T R O L L E R . H
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
/** @file QgPaletteController.h
 *
 * Generic controller that wires together:
 *
 *   - a QgToolPalette widget
 *   - a QgPluginManager
 *   - a category name (e.g. "qged.view")
 *   - the currently-active QgView
 *
 * The controller owns the QgToolBase instances it creates via Qt
 * parentage, manages "current tool" state, and installs/uninstalls
 * IQgViewEventFilter implementations on the active QgView as tools
 * are selected/deselected.
 *
 * Hosts compose with the controller; they do not subclass it.
 */
#ifndef QGPALETTECONTROLLER_H
#define QGPALETTECONTROLLER_H

#include "common.h"

#include <QHash>
#include <QList>
#include <QObject>
#include <QString>
#include "qtcad/defines.h"

class QgPluginContext;
class QgPluginManager;
class QgToolBase;
class QgToolPalette;
class QgToolPaletteElement;
class QgView;

class QTCAD_EXPORT QgPaletteController : public QObject
{
    Q_OBJECT

    public:
	QgPaletteController(QgToolPalette *palette,
			    QgPluginManager *manager,
			    const QString &category,
			    QgPluginContext *ctx,
			    QObject *parent = nullptr);
	~QgPaletteController() override;

	QString category() const { return m_category; }
	QgToolPalette *palette() const { return m_palette; }

	/* Populate the palette from the manager.  Safe to call again
	 * after rescans -- elements are reconciled rather than
	 * duplicated. */
	void populate();

	/* Update the active view that event-filter tools should be
	 * attached to.  May be NULL (no view).  When changed, the
	 * currently-active tool's filter is migrated. */
	void setActiveView(QgView *view);
	QgView *activeView() const { return m_view; }

	QgToolBase *currentTool() const { return m_currentTool; }

    signals:
	/* Emitted when the user selects a different tool in this
	 * palette.  May be NULL for "no tool selected". */
	void currentToolChanged(QgToolBase *tool);

    private slots:
	void onElementSelected(QgToolPaletteElement *el);
	void onFactoryRegistered(const QString &id);
	void onFactoryUnregistered(const QString &id);

    private:
	void attachFilter(QgToolBase *tool);
	void detachFilter(QgToolBase *tool);

	QgToolPalette  *m_palette = nullptr;
	QgPluginManager *m_manager = nullptr;
	QString          m_category;
	QgPluginContext *m_ctx = nullptr;
	QgView          *m_view = nullptr;

	/* Tool instances indexed by plugin id.  Owned via QObject
	 * parentage (parent is *this*). */
	QHash<QString, QgToolBase *>             m_tools;
	QHash<QgToolPaletteElement *, QString>   m_elementToId;
	QgToolBase                              *m_currentTool = nullptr;
};

#endif /* QGPALETTECONTROLLER_H */

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
