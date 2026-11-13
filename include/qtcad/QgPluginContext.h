/*                Q G P L U G I N C O N T E X T . H
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

#ifndef QGPLUGINCONTEXT_H
#define QGPLUGINCONTEXT_H

#include <functional>

#include <QObject>
#include <QString>

#include "ged/scene_types.h"
#include "qtcad/defines.h"
#include "qtcad/QgSignalFlags.h"

struct ged;
class QgModel;
class QgView;

/** Host-owned lifecycle notifications exposed to plugins. */
class QTCAD_EXPORT QgPluginNotifier : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(QgPluginNotifier)

    public:
	explicit QgPluginNotifier(QObject *parent = nullptr);
	~QgPluginNotifier() override;

    signals:
	/** Report that the active geometry database changed. */
	void dbChanged();
	/** Report that the host selected a different active view. */
	void viewChanged();
	/** Report changes to the active view's contents or settings. */
	void viewUpdated(QgViewUpdateFlags flags);
	/** Report changes to settings consumed by plugins. */
	void settingsChanged();
};

/**
 * Non-owning, host-neutral access to application services used by plugins.
 *
 * Accessors resolve mutable host state on demand, allowing one context to
 * remain valid across database reopen and active-view changes.  The host owns
 * every pointer returned by this object.
 */
class QgPluginContext
{
    public:
	QgPluginContext() = default;
	~QgPluginContext() = default;
	QgPluginContext(const QgPluginContext &) = delete;
	QgPluginContext &operator=(const QgPluginContext &) = delete;

	/** Resolve the host's active GED context, or nullptr. */
	struct ged *getGed() const
	{
	    return gedAccessor ? gedAccessor() : nullptr;
	}

	/** Resolve the renderer-neutral active view context, or nullptr. */
	ged_view_context *activeViewContext() const
	{
	    return viewContextAccessor ? viewContextAccessor() : nullptr;
	}

	/** Resolve the host's active QgView widget, or nullptr. */
	QgView *getViewWidget() const
	{
	    return viewWidgetAccessor ? viewWidgetAccessor() : nullptr;
	}

	/** Resolve the host's active semantic selection-set name. */
	QString selectionSetName() const
	{
	    return selectionSetAccessor ? selectionSetAccessor() : QString();
	}

	/** Send a diagnostic to the host when it provides a log sink. */
	void logMessage(const QString &message) const
	{
	    if (log)
		log(message);
	}

	/** Late-bound access to the host's GED context. */
	std::function<struct ged *()> gedAccessor;
	/** Late-bound access to the host's semantic view context. */
	std::function<ged_view_context *()> viewContextAccessor;
	/** Late-bound access to the active Qt view widget. */
	std::function<QgView *()> viewWidgetAccessor;
	/** Late-bound access to the active semantic selection set. */
	std::function<QString()> selectionSetAccessor;
	/** Optional host tree model. */
	QgModel *model = nullptr;
	/** Host-owned notification relay. */
	QgPluginNotifier *notifier = nullptr;
	/** Human-readable host identifier used only for diagnostics. */
	QString hostName;
	/** Optional host diagnostic sink. */
	std::function<void(const QString &)> log;
};

#endif /* QGPLUGINCONTEXT_H */

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
