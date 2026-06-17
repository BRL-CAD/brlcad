/*              Q G P L U G I N I N T E R F A C E S . H
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
/** @file QgPluginInterfaces.h
 *
 * Pure virtual interfaces a libqtcad plugin can implement.  Each
 * interface is registered via Q_DECLARE_INTERFACE so that
 * Q_PLUGIN_METADATA / QPluginLoader-based discovery can downcast
 * loaded objects to the relevant role.
 *
 * Plugins typically subclass QObject and implement one (or several)
 * of these interfaces.  The host queries QgPluginManager for plugins
 * of a particular interface type and a particular category.
 *
 * Stability: the IID strings embed an API version suffix
 * ("/<QGTCAD_PLUGIN_API_VERSION>") so QPluginLoader will silently
 * reject plugins that were built against a different ABI.
 */
#ifndef QGPLUGININTERFACES_H
#define QGPLUGININTERFACES_H

#include "common.h"

#include <QObject>
#include <QString>
#include <QStringList>
#include "qtcad/defines.h"
#include "qtcad/QgPluginDescriptor.h"

class QWidget;
class QDockWidget;
class QDialog;
class QgPluginContext;
class QgToolBase;
class QgView;

/*
 * The bumpable API version baked into every interface IID and into
 * QgPluginManager's metadata check.  Bump whenever the interfaces
 * below change in a binary-incompatible way.
 */
#define QGTCAD_PLUGIN_API_VERSION 1

#define QGTCAD_IID_PREFIX "org.brlcad.qtcad."
#define QGTCAD_IID(name) QGTCAD_IID_PREFIX name "/1"


/* ------------------------------------------------------------------
 * IQgToolFactory -- palette-style tool factory.
 * ------------------------------------------------------------------
 * Each factory describes and creates one entry in a QgToolPalette.
 * Concrete tools are typically QgToolBase subclasses.
 */
class QTCAD_EXPORT IQgToolFactory
{
    public:
	virtual ~IQgToolFactory() = default;
	virtual QgPluginDescriptor descriptor() const = 0;
	/* Create an instance; the returned QgToolBase is owned by the
	 * caller and must be parented appropriately.  Returns NULL on
	 * failure. */
	virtual QgToolBase *create(QgPluginContext *ctx, QObject *parent = nullptr) = 0;
};
Q_DECLARE_INTERFACE(IQgToolFactory, QGTCAD_IID("IQgToolFactory"))


/* ------------------------------------------------------------------
 * IQgPanelFactory -- QDockWidget panel factory.
 * ------------------------------------------------------------------
 * Used for dockable panels (attribute editor, raytrace control panel,
 * etc.) that the host can register against its main window.
 */
class QTCAD_EXPORT IQgPanelFactory
{
    public:
	virtual ~IQgPanelFactory() = default;
	virtual QgPluginDescriptor descriptor() const = 0;
	virtual QDockWidget *create(QgPluginContext *ctx, QWidget *parent = nullptr) = 0;
};
Q_DECLARE_INTERFACE(IQgPanelFactory, QGTCAD_IID("IQgPanelFactory"))


/* ------------------------------------------------------------------
 * IQgDialogFactory -- free-floating dialog factory.
 * ------------------------------------------------------------------
 * Used for complex modal/modeless dialogs that don't belong inside a
 * dock or palette (combination editor, BoT edit, sketch editor, ...).
 */
class QTCAD_EXPORT IQgDialogFactory
{
    public:
	virtual ~IQgDialogFactory() = default;
	virtual QgPluginDescriptor descriptor() const = 0;
	virtual QDialog *create(QgPluginContext *ctx, QWidget *parent = nullptr) = 0;
};
Q_DECLARE_INTERFACE(IQgDialogFactory, QGTCAD_IID("IQgDialogFactory"))


/* ------------------------------------------------------------------
 * IQgCommand -- ged-style command callable from a QgConsole.
 * ------------------------------------------------------------------
 * Lets plugins extend the host's console with commands that have
 * access to a QgPluginContext (and therefore to the host's ged and
 * bsg_view).  The host wires invocation; the plugin provides the
 * verb name(s) and the run() callback.
 */
class QTCAD_EXPORT IQgCommand
{
    public:
	virtual ~IQgCommand() = default;
	virtual QgPluginDescriptor descriptor() const = 0;
	/* Primary verb (e.g. "raytrace"); aliases() may be empty. */
	virtual QString verb() const = 0;
	virtual QStringList aliases() const { return QStringList(); }
	/* Standard exit-code convention: 0 success, non-zero error.
	 * out/err are appended-to by the implementation. */
	virtual int run(QgPluginContext *ctx,
			const QStringList &argv,
			QString *out,
			QString *err) = 0;
};
Q_DECLARE_INTERFACE(IQgCommand, QGTCAD_IID("IQgCommand"))


/* ------------------------------------------------------------------
 * IQgViewEventFilter -- typed view-event-filter contract.
 * ------------------------------------------------------------------
 * Replaces the public 'use_event_filter' bool on QgToolPaletteElement.
 * A QgToolBase that implements this interface is installed as a Qt
 * event filter on the active QgView automatically by
 * QgPaletteController while the tool is selected, and uninstalled
 * when it is deselected.
 */
class QTCAD_EXPORT IQgViewEventFilter
{
    public:
	virtual ~IQgViewEventFilter() = default;
	/* Called by the controller when the tool becomes active. */
	virtual void attachToView(QgView *view) = 0;
	/* Called by the controller when the tool is no longer active. */
	virtual void detachFromView(QgView *view) = 0;
};
Q_DECLARE_INTERFACE(IQgViewEventFilter, QGTCAD_IID("IQgViewEventFilter"))


#endif /* QGPLUGININTERFACES_H */

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
