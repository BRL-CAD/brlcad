/*           V I E W I N F O F A C T O R Y . H
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
/** @file ViewInfoFactory.h
 *
 * Qt plugin factory + QgToolBase tool class for the view/info plugin.
 * Replaces the legacy qged_plugin_info ABI with IQgToolFactory.
 */

#ifndef VIEWINFOFACTORY_H
#define VIEWINFOFACTORY_H

#include <QIcon>

#include "qtcad/QgPluginDescriptor.h"
#include "qtcad/QgPluginInterfaces.h"
#include "qtcad/QgToolBase.h"
#include "CADViewModel.h"

class QgPluginContext;

/* Tool: wraps CADViewModel + QgKeyValView in the QgToolBase lifecycle. */
class ViewInfoTool : public QgToolBase
{
    Q_OBJECT

    public:
	explicit ViewInfoTool(QgPluginContext *ctx, QObject *parent = nullptr);

	QWidget *createWidget() override;
	QIcon   *createIcon()   override;

    public slots:
	void refresh() override;

    private:
	CADViewModel *m_model = nullptr;
};


/* Factory: advertises the tool to QgPluginManager via Qt plugin metadata. */
class ViewInfoFactory : public QObject, public IQgToolFactory
{
    Q_OBJECT
    Q_INTERFACES(IQgToolFactory)
    Q_PLUGIN_METADATA(IID "org.brlcad.qtcad.IQgToolFactory/1" FILE "view_info.json")

    public:
	QgPluginDescriptor descriptor() const override;
	QgToolBase *create(QgPluginContext *ctx, QObject *parent = nullptr) override;
};

#endif /* VIEWINFOFACTORY_H */

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
