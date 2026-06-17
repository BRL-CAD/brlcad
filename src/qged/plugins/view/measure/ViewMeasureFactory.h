/*         V I E W M E A S U R E F A C T O R Y . H
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
/** @file ViewMeasureFactory.h
 *
 * Qt plugin factory + QgToolBase tool class for the view/measure plugin.
 * Replaces the legacy qged_plugin_info ABI with IQgToolFactory.
 * Implements IQgViewEventFilter: installs the CADViewMeasure widget as a
 * Qt event filter on the active QgView while the tool is selected.
 */

#ifndef VIEWMEASUREFACTORY_H
#define VIEWMEASUREFACTORY_H

#include <QIcon>

#include "qtcad/QgPluginDescriptor.h"
#include "qtcad/QgPluginInterfaces.h"
#include "qtcad/QgToolBase.h"
#include "qtcad/QgToolPalette.h"
#include "CADViewMeasure.h"

class QgPluginContext;
class QgView;

/* Tool: wraps CADViewMeasure in the QgToolBase + IQgViewEventFilter lifecycle. */
class ViewMeasureTool : public QgToolBase, public IQgViewEventFilter
{
    Q_OBJECT
    Q_INTERFACES(IQgViewEventFilter)

    public:
	explicit ViewMeasureTool(QgPluginContext *ctx, QObject *parent = nullptr);

	QgToolPaletteElement *paletteElement() override;
	QWidget *createWidget() override;
	QIcon   *createIcon()   override;

	/* IQgViewEventFilter */
	QObject *viewEventFilter() override { return this; }
	void attachToView(QgView *view) override;
	void detachFromView(QgView *view) override;

    public slots:
	void refresh() override;

    private:
	CADViewMeasure *m_measure = nullptr;
	bool m_extra_wired = false;
};


/* Factory: advertises the tool to QgPluginManager via Qt plugin metadata. */
class ViewMeasureFactory : public QObject, public IQgToolFactory
{
    Q_OBJECT
    Q_INTERFACES(IQgToolFactory)
    Q_PLUGIN_METADATA(IID "org.brlcad.qtcad.IQgToolFactory/1" FILE "measure.json")

    public:
	QgPluginDescriptor descriptor() const override;
	QgToolBase *create(QgPluginContext *ctx, QObject *parent = nullptr) override;
};

#endif /* VIEWMEASUREFACTORY_H */

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
