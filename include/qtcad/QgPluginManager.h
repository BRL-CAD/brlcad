/*                  Q G P L U G I N M A N A G E R . H
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
/** @file QgPluginManager.h
 *
 * Generic QPluginLoader-based plugin manager for libqtcad hosts.
 *
 * The manager:
 *   - takes a list of search directories at construction (the host
 *     supplies them; libqtcad does not hard-code any path);
 *   - reads JSON metadata via QPluginLoader::metaData() without
 *     dlopen()ing the binary body, so descriptors can be enumerated
 *     cheaply;
 *   - loads each plugin lazily and at most once;
 *   - rejects plugins whose embedded API version disagrees with
 *     QGTCAD_PLUGIN_API_VERSION;
 *   - supports an allow/deny list backed by a QSettings group whose
 *     name the host supplies (defaults to "qtcad/plugins");
 *   - supports statically-linked plugins registered with
 *     Q_IMPORT_PLUGIN: those are picked up via
 *     QPluginLoader::staticPlugins();
 *   - sorts factories deterministically by (sortKey, id);
 *   - emits signals when factories become available or are unloaded.
 *
 * Typed queries are provided as inline templates so callers can ask
 * for, e.g., factories<IQgToolFactory>("qged.view").
 */
#ifndef QGPLUGINMANAGER_H
#define QGPLUGINMANAGER_H

#include "common.h"

#include <QHash>
#include <QList>
#include <QObject>
#include <QString>
#include <QStringList>
#include "qtcad/defines.h"
#include "qtcad/QgPluginDescriptor.h"

class QPluginLoader;

class QTCAD_EXPORT QgPluginManager : public QObject
{
    Q_OBJECT

    public:
	/* @param searchDirs Directories to scan for shared-object
	 *                   plugins.  May be empty (static plugins
	 *                   only).
	 * @param settingsGroup QSettings group used for the
	 *                   allow/deny list; defaults to
	 *                   "qtcad/plugins".
	 */
	explicit QgPluginManager(const QStringList &searchDirs = QStringList(),
				 const QString &settingsGroup = QString(),
				 QObject *parent = nullptr);
	~QgPluginManager() override;

	/* Scan (or re-scan) search directories and refresh the list
	 * of available plugins.  Already-loaded plugins are kept;
	 * descriptors of newly-discovered plugins become available. */
	void rescan();

	/* Unload every plugin and clear caches.  Static plugins
	 * remain available (they cannot be unloaded). */
	void unloadAll();

	/* Convenience for hosts that mix the manager with a console:
	 * a one-shot reload that unloads everything and rescans. */
	void reload();

	/* All known plugin descriptors, deterministically ordered. */
	QList<QgPluginDescriptor> descriptors() const;

	/* Descriptors filtered by category. */
	QList<QgPluginDescriptor> descriptors(const QString &category) const;

	/* Look up a descriptor by id; returns an empty descriptor
	 * (id.isEmpty()) if not known. */
	QgPluginDescriptor descriptor(const QString &id) const;

	/* Resolve the QObject implementing a particular plugin id.
	 * Lazily loads the underlying .so on first call.  Returns
	 * NULL on load failure, ABI mismatch, or denied id. */
	QObject *instance(const QString &id);

	/* Typed lookup: return every loaded instance that implements
	 * @p Interface, optionally restricted to @p category.  Lazily
	 * loads plugins as needed. */
	template <typename Interface>
	QList<Interface *> factories(const QString &category = QString());

	/* Allow/deny list management.  Persisted to QSettings under
	 * @c settingsGroup.  enable()/disable() return false if @p id
	 * is not known. */
	bool isEnabled(const QString &id) const;
	bool enable(const QString &id);
	bool disable(const QString &id);

	/* Diagnostics for "plugins list" / "plugins info <id>". */
	QString diagnosticSummary() const;
	QString diagnosticInfo(const QString &id) const;

    signals:
	void factoryRegistered(const QString &id);
	void factoryUnregistered(const QString &id);

    private:
	struct Entry; /* opaque -- see .cpp */
	void readMetadata(QPluginLoader *loader, Entry &entry);
	QObject *resolveInstance(Entry &entry);
	void persistEnabled(const QString &id, bool enabled);

	QStringList m_searchDirs;
	QString m_settingsGroup;
	QList<Entry *> m_entries;
	QHash<QString, Entry *> m_byId;
};

/* Inline template implementation -- needs QObject definitions */
#include <QObject>

template <typename Interface>
QList<Interface *> QgPluginManager::factories(const QString &category)
{
    QList<Interface *> out;
    const QList<QgPluginDescriptor> descs = category.isEmpty()
	? descriptors()
	: descriptors(category);
    for (const QgPluginDescriptor &d : descs) {
	QObject *obj = instance(d.id);
	if (!obj)
	    continue;
	Interface *iface = qobject_cast<Interface *>(obj);
	if (iface)
	    out.append(iface);
    }
    return out;
}

#endif /* QGPLUGINMANAGER_H */

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
