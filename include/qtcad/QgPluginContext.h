/*                  Q G P L U G I N C O N T E X T . H
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
/** @file QgPluginContext.h
 *
 * Host-agnostic context object handed to libqtcad plugin factories.
 *
 * Plugins must never reach into a host-specific class (e.g. QgEdApp).
 * Instead, the host populates a QgPluginContext and passes it to each
 * factory.  This is the *only* contract a factory needs to know about
 * its environment, which makes the same plugin loadable into qged, a
 * future Qt-based Archer, an embedded panel, or a third-party host.
 *
 * Accessors are deliberately function-pointer/std::function style so
 * the context can re-resolve handles on demand: the underlying ged*
 * and bsg_view* may change across the lifetime of the host (db reopen,
 * view swap, quad-view switching) without the context having to be
 * rebuilt.
 *
 * QgPluginContext does not own any of the resources it exposes -- it
 * is a non-owning view onto the host's state.
 */
#ifndef QGPLUGINCONTEXT_H
#define QGPLUGINCONTEXT_H

#include "common.h"

#include <functional>
#include <QObject>
#include <QString>
#include "qtcad/defines.h"
#include "qtcad/QgTypes.h"

/* Forward declarations: callers that actually need the full types
 * can include them directly.  Keeping these out of the API header
 * minimises transitive include pressure for hosts. */
struct ged;
struct bsg_view;
class QgModel;

/* QgPluginNotifier is a small QObject that emits the lifecycle
 * signals the host already manages internally.  Plugins connect to
 * these signals instead of reaching into the host.
 *
 * The host is responsible for emitting these at appropriate times
 * (database open/close, current view switch, settings change, etc.). */
class QTCAD_EXPORT QgPluginNotifier : public QObject
{
    Q_OBJECT

    public:
	explicit QgPluginNotifier(QObject *parent = nullptr);
	~QgPluginNotifier() override;

    signals:
	/* The active .g database has changed (open/close/reopen). */
	void dbChanged();

	/* The active bsg_view has changed (e.g. quad-view switch). */
	void viewChanged();

	/* The active bsg_view's contents/settings updated; payload is the
	 * standard QgViewUpdateFlags bitmask. */
	void viewUpdated(QgViewUpdateFlags flags);

	/* Settings affecting tools have changed. */
	void settingsChanged();
};

class QTCAD_EXPORT QgPluginContext
{
    public:
	QgPluginContext() = default;
	~QgPluginContext() = default;

	/* Non-copyable: hold by reference/pointer at call sites. */
	QgPluginContext(const QgPluginContext &) = delete;
	QgPluginContext &operator=(const QgPluginContext &) = delete;

	/* Accessor: the host's current ged*.  May return NULL when no
	 * database is open. */
	std::function<struct ged *()> gedAccessor;

	/* Accessor: the host's currently-active bsg_view*.  May return
	 * NULL when no view is set up. */
	std::function<struct bsg_view *()> viewAccessor;

	/* The host's QgModel, if any.  May be NULL for hosts that
	 * don't surface the tree model. */
	QgModel *model = nullptr;

	/* Notifier object owned by the host; never NULL once the
	 * context has been finalised.  Plugins connect to its
	 * signals. */
	QgPluginNotifier *notifier = nullptr;

	/* A short human-readable host identifier (e.g. "qged",
	 * "archer") for diagnostics/logging.  Not used for behaviour
	 * dispatch -- categories are the dispatch mechanism. */
	QString hostName;

	/* Log sink: plugins should route diagnostic output here in
	 * preference to fprintf(stderr,...).  Default is a no-op set
	 * by the host. */
	std::function<void(const QString &)> log;

	/* Convenience wrappers. */
	struct ged *getGed() const { return gedAccessor ? gedAccessor() : nullptr; }
	struct bsg_view *getView() const { return viewAccessor ? viewAccessor() : nullptr; }
	void logMessage(const QString &msg) const { if (log) log(msg); }
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
