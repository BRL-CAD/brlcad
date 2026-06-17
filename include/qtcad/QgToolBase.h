/*                     Q G T O O L B A S E . H
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
/** @file QgToolBase.h
 *
 * Concrete base class for libqtcad plugin tools.  Every current
 * qged *_tool.cpp repeats six-or-seven QObject::connect() lines to
 * hook into element_view_update / view_changed / settings_changed,
 * etc.  QgToolBase centralises that wiring and exposes a small set
 * of virtual hooks instead:
 *
 *   - createWidget()    : build (or return cached) controls widget
 *   - viewEventFilter() : optional QObject implementing
 *                         IQgViewEventFilter when the tool wants to
 *                         intercept view events while active
 *   - refresh()         : (re)populate widget from current view state
 *   - onDbChanged()     : the active .g database has changed
 *   - onViewChanged()   : the active bsg_view has changed
 *
 * QgToolBase owns the widget and (optionally) the view-event-filter
 * object via Qt parentage.  It is host-agnostic: it only knows about
 * a QgPluginContext supplied at construction.
 */
#ifndef QGTOOLBASE_H
#define QGTOOLBASE_H

#include "common.h"

#include <QObject>
#include "qtcad/defines.h"
#include "qtcad/QgPluginDescriptor.h"

class QWidget;
class QIcon;
class QgPluginContext;
class QgToolPaletteElement;

class QTCAD_EXPORT QgToolBase : public QObject
{
    Q_OBJECT

    public:
	QgToolBase(QgPluginContext *ctx, QObject *parent = nullptr);
	~QgToolBase() override;

	/* Static descriptor for this tool instance.  Default
	 * implementation returns an empty descriptor; subclasses
	 * normally override (or have the descriptor supplied by their
	 * IQgToolFactory). */
	virtual QgPluginDescriptor descriptor() const { return QgPluginDescriptor(); }

	/* Build (lazily) the controls widget the tool exposes.  Must
	 * not return NULL once called.  The widget is parented to the
	 * tool. */
	virtual QWidget *createWidget() = 0;

	/* If the tool wants to install a Qt event filter on the
	 * active QgView while selected, return a QObject implementing
	 * IQgViewEventFilter.  Default: no filter. */
	virtual QObject *viewEventFilter() { return nullptr; }

	/* Optional icon for palette display.  Default: nullptr. */
	virtual QIcon *createIcon() { return nullptr; }

	/* Convenience: build (or return cached) palette element.
	 * Implemented in terms of createWidget()/createIcon().  Does
	 * the standard signal wiring (element_view_update <->
	 * refresh, etc.) so subclasses don't have to. */
	virtual QgToolPaletteElement *paletteElement();

	QgPluginContext *context() const { return m_ctx; }

    public slots:
	/* Subclasses override to repopulate from the current view. */
	virtual void refresh() {}
	virtual void onDbChanged() {}
	virtual void onViewChanged() {}
	virtual void onSettingsChanged() {}

    protected:
	QgPluginContext *m_ctx = nullptr;

    private:
	QWidget *m_widget = nullptr;
	QgToolPaletteElement *m_element = nullptr;
	bool m_signals_wired = false;
};

#endif /* QGTOOLBASE_H */

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
