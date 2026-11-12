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

#ifndef QGPLUGININTERFACES_H
#define QGPLUGININTERFACES_H

#include <QObject>
#include <QString>
#include <QStringList>

#include "qtcad/QgPluginDescriptor.h"
#include "qtcad/QgPluginVersion.h"

class QDialog;
class QDockWidget;
class QWidget;
class QgPluginContext;
class QgToolBase;
class QgView;

/** Factory for a tool hosted in a QgToolPalette. */
class IQgToolFactory
{
    public:
	virtual ~IQgToolFactory() = default;
	virtual QgPluginDescriptor descriptor() const = 0;
	virtual QgToolBase *create(QgPluginContext *context,
		QObject *parent = nullptr) = 0;
};
Q_DECLARE_INTERFACE(IQgToolFactory, QGTCAD_IID("IQgToolFactory"))

/** Factory for a dockable host panel. */
class IQgPanelFactory
{
    public:
	virtual ~IQgPanelFactory() = default;
	virtual QgPluginDescriptor descriptor() const = 0;
	virtual QDockWidget *create(QgPluginContext *context,
		QWidget *parent = nullptr) = 0;
};
Q_DECLARE_INTERFACE(IQgPanelFactory, QGTCAD_IID("IQgPanelFactory"))

/** Factory for a standalone dialog. */
class IQgDialogFactory
{
    public:
	virtual ~IQgDialogFactory() = default;
	virtual QgPluginDescriptor descriptor() const = 0;
	virtual QDialog *create(QgPluginContext *context,
		QWidget *parent = nullptr) = 0;
};
Q_DECLARE_INTERFACE(IQgDialogFactory, QGTCAD_IID("IQgDialogFactory"))

/** Host-console command supplied by a plugin. */
class IQgCommand
{
    public:
	virtual ~IQgCommand() = default;
	virtual QgPluginDescriptor descriptor() const = 0;
	virtual QString verb() const = 0;
	virtual QStringList aliases() const { return QStringList(); }
	virtual int run(QgPluginContext *context, const QStringList &argv,
		QString *output, QString *error) = 0;
};
Q_DECLARE_INTERFACE(IQgCommand, QGTCAD_IID("IQgCommand"))

/** Contract for tools that intercept events from the active view. */
class IQgViewEventFilter
{
    public:
	virtual ~IQgViewEventFilter() = default;
	virtual void attachToView(QgView *view) = 0;
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
