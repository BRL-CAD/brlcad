/*           E D I T R E V O L V E F A C T O R Y . H
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
/** @file EditRevolveFactory.h
 *
 * Qt plugin factory + QgToolBase wrapper for the edit/revolve plugin.
 * demonstrates typed edit-preview integration.
 */

#ifndef EDITREVOLVEFACTORY_H
#define EDITREVOLVEFACTORY_H

#include <QIcon>

#include "qtcad/QgPluginDescriptor.h"
#include "qtcad/QgPluginInterfaces.h"
#include "qtcad/QgToolBase.h"
#include "qtcad/QgToolPalette.h"
#include "QRevolve.h"

class QgPluginContext;
class QgView;

class EditRevolveTool : public QgToolBase, public IQgViewEventFilter
{
    Q_OBJECT
    Q_INTERFACES(IQgViewEventFilter)

    public:
	explicit EditRevolveTool(QgPluginContext *ctx, QObject *parent = nullptr);

	QgToolPaletteElement *paletteElement() override;
	QWidget *createWidget() override;
	QIcon *createIcon() override;

	QObject *viewEventFilter() override { return this; }
	void attachToView(QgView *view) override;
	void detachFromView(QgView *view) override;

    public slots:
	void refresh() override;
	void onDbChanged() override;
	void onViewChanged() override;

    private:
	QRevolve *m_rev = nullptr;
	bool m_extra_wired = false;
};

class EditRevolveFactory : public QObject, public IQgToolFactory
{
    Q_OBJECT
    Q_INTERFACES(IQgToolFactory)
    Q_PLUGIN_METADATA(IID "org.brlcad.qtcad.IQgToolFactory/1" FILE "revolve.json")

    public:
	QgPluginDescriptor descriptor() const override;
	QgToolBase *create(QgPluginContext *ctx, QObject *parent = nullptr) override;
};

#endif /* EDITREVOLVEFACTORY_H */

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
