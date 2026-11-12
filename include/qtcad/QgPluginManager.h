/*                Q G P L U G I N M A N A G E R . H
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

#ifndef QGPLUGINMANAGER_H
#define QGPLUGINMANAGER_H

#include <QHash>
#include <QList>
#include <QObject>
#include <QString>
#include <QStringList>

#include "qtcad/defines.h"
#include "qtcad/QgPluginDescriptor.h"

class QPluginLoader;

/**
 * Discover and lazily load host-neutral libqtcad plugins.
 *
 * Search paths and settings namespaces are supplied by the host.  Dynamic
 * implementations remain unloaded until instance() or factories() requests
 * them.  All lifecycle methods must run on the manager's QObject thread.
 */
class QTCAD_EXPORT QgPluginManager : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(QgPluginManager)

    public:
	/** Construct a manager and perform its initial catalog scan. */
	explicit QgPluginManager(
		const QStringList &searchDirectories = QStringList(),
		const QString &settingsGroup = QString(),
		QObject *parent = nullptr);
	~QgPluginManager() override;

	/**
	 * Discover plugins not already present in the catalog.
	 *
	 * Use reload() when removed or rebuilt files must be reconciled.
	 */
	void rescan();
	/** Unload dynamic implementations after a synchronous teardown signal. */
	bool unloadAll(QStringList *errors = nullptr);
	/** Unload implementations, rebuild metadata, and rescan search paths. */
	bool reload(QStringList *errors = nullptr);

	/** Return every catalog descriptor in deterministic display order. */
	QList<QgPluginDescriptor> descriptors() const;
	/** Return descriptors whose category exactly matches @p category. */
	QList<QgPluginDescriptor> descriptors(const QString &category) const;
	/** Return the descriptor for @p id, or an empty descriptor. */
	QgPluginDescriptor descriptor(const QString &id) const;
	/** Lazily resolve the root QObject for an enabled plugin. */
	QObject *instance(const QString &id);

	/** Resolve all plugins implementing @p Interface in a category. */
	template <typename Interface>
	QList<Interface *> factories(const QString &category = QString());

	/** Report whether a known plugin is enabled in host settings. */
	bool isEnabled(const QString &id) const;
	/** Persistently enable a known plugin. */
	bool enable(const QString &id);
	/** Persistently disable a known plugin. */
	bool disable(const QString &id);

	/** Return one-line diagnostics for the complete catalog. */
	QString diagnosticSummary() const;
	/** Return detailed diagnostics for one plugin identifier. */
	QString diagnosticInfo(const QString &id) const;
	/** Return warnings produced by the most recent catalog scan. */
	QStringList scanDiagnostics() const { return m_scanDiagnostics; }

    signals:
	/**
	 * Require destruction of factory-created objects before returning.
	 *
	 * Queued cleanup is too late because plugin code is unloaded next.
	 */
	void pluginsAboutToUnload();
	/** Report that catalog contents or availability changed. */
	void catalogChanged();
	/** Report discovery of a new catalog identifier. */
	void factoryRegistered(const QString &id);
	/** Report removal of a catalog identifier during reload. */
	void factoryUnregistered(const QString &id);
	/** Report a persisted plugin availability change. */
	void pluginEnabledChanged(const QString &id, bool enabled);

    private:
	struct Entry;
	void addStaticPlugins();
	void addDynamicPlugins();
	void clearDynamicCatalog();
	void readMetadata(QPluginLoader *loader, Entry &entry);
	QObject *resolveInstance(Entry &entry);
	void persistEnabled(const QString &id, bool enabled);
	bool setEnabled(const QString &id, bool enabled);
	void sortEntries();

	QStringList m_searchDirectories;
	QString m_settingsGroup;
	QList<Entry *> m_entries;
	QHash<QString, Entry *> m_byId;
	QStringList m_scanDiagnostics;
	bool m_unloading = false;
};

template <typename Interface>
QList<Interface *>
QgPluginManager::factories(const QString &category)
{
    QList<Interface *> result;
    const QList<QgPluginDescriptor> available = category.isEmpty() ?
	descriptors() : descriptors(category);
    for (const QgPluginDescriptor &plugin : available) {
	QObject *pluginInstance = instance(plugin.id);
	if (!pluginInstance)
	    continue;
	Interface *factory = qobject_cast<Interface *>(pluginInstance);
	if (factory)
	    result.append(factory);
    }
    return result;
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
