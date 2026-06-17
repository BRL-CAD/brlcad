/*        V I E W S E T T I N G S F A C T O R Y . H
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
/** @file ViewSettingsFactory.h
 *
 * Qt plugin factory + QgToolBase tool class for the view/settings plugin.
 * Replaces the legacy qged_plugin_info ABI with IQgToolFactory.
 */

#ifndef VIEWSETTINGSFACTORY_H
#define VIEWSETTINGSFACTORY_H

#include <QIcon>

#include "qtcad/QgPluginDescriptor.h"
#include "qtcad/QgPluginInterfaces.h"
#include "qtcad/QgToolBase.h"
#include "qtcad/QgToolPalette.h"
#include "CADViewSettings.h"

class QgPluginContext;

/* Tool: wraps CADViewSettings in the QgToolBase lifecycle. */
class ViewSettingsTool : public QgToolBase
{
    Q_OBJECT

    public:
	explicit ViewSettingsTool(QgPluginContext *ctx, QObject *parent = nullptr);

	QgToolPaletteElement *paletteElement() override;
	QWidget *createWidget() override;
	QIcon   *createIcon()   override;

    public slots:
	void refresh() override;

    private:
	CADViewSettings *m_settings = nullptr;
	bool m_extra_wired = false;
};


/* Factory: advertises the tool to QgPluginManager via Qt plugin metadata. */
class ViewSettingsFactory : public QObject, public IQgToolFactory
{
    Q_OBJECT
    Q_INTERFACES(IQgToolFactory)
    Q_PLUGIN_METADATA(IID "org.brlcad.qtcad.IQgToolFactory/1" FILE "view_settings.json")

    public:
	QgPluginDescriptor descriptor() const override;
	QgToolBase *create(QgPluginContext *ctx, QObject *parent = nullptr) override;
};

#endif /* VIEWSETTINGSFACTORY_H */

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
