/*                Q G P L U G I N M A N A G E R . C P P
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

#include "common.h"

#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QLibrary>
#include <QPluginLoader>
#include <QSettings>
#include <algorithm>

#include "qtcad/QgPluginInterfaces.h"
#include "qtcad/QgPluginManager.h"

#define QGTCAD_DEFAULT_SETTINGS_GROUP "qtcad/plugins"

struct QgPluginManager::Entry {
    QgPluginDescriptor desc;
    QPluginLoader *loader = nullptr; /* nullptr for static plugins */
    QObject *staticInstance = nullptr;
    bool triedLoad = false;
    bool failed = false;
    QString failureReason;
};

static QString
jsonStr(const QJsonObject &obj, const char *key, const QString &dflt = QString())
{
    QJsonValue v = obj.value(QLatin1String(key));
    return v.isString() ? v.toString() : dflt;
}

static int
jsonInt(const QJsonObject &obj, const char *key, int dflt)
{
    QJsonValue v = obj.value(QLatin1String(key));
    if (v.isDouble())
	return (int)v.toDouble();
    if (v.isString()) {
	bool ok = false;
	int r = v.toString().toInt(&ok);
	if (ok) return r;
    }
    return dflt;
}

static bool
jsonBool(const QJsonObject &obj, const char *key, bool dflt)
{
    QJsonValue v = obj.value(QLatin1String(key));
    return v.isBool() ? v.toBool() : dflt;
}

static QgPluginDescriptor
descriptorFromMetadata(const QJsonObject &meta)
{
    /* MetaData JSON layout (by Qt convention) puts plugin-specific
     * data under the "MetaData" key. */
    QJsonObject md = meta.value(QLatin1String("MetaData")).toObject();
    QgPluginDescriptor d;
    d.id            = jsonStr(md, "id");
    d.displayName   = jsonStr(md, "displayName", d.id);
    d.category      = jsonStr(md, "category");
    d.iconName      = jsonStr(md, "iconName");
    d.shortcutHint  = jsonStr(md, "shortcutHint");
    d.sortKey       = jsonInt(md, "sortKey", 0);
    d.requiresOpenDb = jsonBool(md, "requiresOpenDb", false);
    d.requiresView  = jsonBool(md, "requiresView", false);
    d.description   = jsonStr(md, "description");
    d.vendor        = jsonStr(md, "vendor");
    d.version       = jsonStr(md, "version");
    return d;
}

static int
apiVersionFromMetadata(const QJsonObject &meta)
{
    QJsonObject md = meta.value(QLatin1String("MetaData")).toObject();
    return jsonInt(md, "apiVersion", QGTCAD_PLUGIN_API_VERSION);
}

QgPluginManager::QgPluginManager(const QStringList &searchDirs,
				 const QString &settingsGroup,
				 QObject *parent)
    : QObject(parent)
    , m_searchDirs(searchDirs)
    , m_settingsGroup(settingsGroup.isEmpty()
		      ? QString::fromLatin1(QGTCAD_DEFAULT_SETTINGS_GROUP)
		      : settingsGroup)
{
    rescan();
}

QgPluginManager::~QgPluginManager()
{
    unloadAll();
    for (Entry *e : m_entries)
	delete e;
    m_entries.clear();
    m_byId.clear();
}

void
QgPluginManager::readMetadata(QPluginLoader *loader, Entry &entry)
{
    QJsonObject meta = loader->metaData();
    entry.desc = descriptorFromMetadata(meta);
    int api = apiVersionFromMetadata(meta);
    if (api != QGTCAD_PLUGIN_API_VERSION) {
	entry.failed = true;
	entry.failureReason = QStringLiteral("API version mismatch: plugin=%1, host=%2")
				.arg(api).arg((int)QGTCAD_PLUGIN_API_VERSION);
    }
}

void
QgPluginManager::rescan()
{
    QHash<QString, Entry *> kept = m_byId; /* shallow copy of pointers */
    m_byId.clear();
    QList<Entry *> oldEntries = m_entries;
    m_entries.clear();

    /* Pick up static plugins first.  They are intrinsic to the host
     * binary and cannot be removed. */
    const QObjectList staticInstances = QPluginLoader::staticInstances();
    QVector<QStaticPlugin> staticPlugins = QPluginLoader::staticPlugins();
    for (int i = 0; i < staticPlugins.size() && i < staticInstances.size(); ++i) {
	QJsonObject meta = staticPlugins.at(i).metaData();
	QgPluginDescriptor d = descriptorFromMetadata(meta);
	if (d.id.isEmpty())
	    continue;
	if (kept.contains(d.id)) {
	    Entry *e = kept.take(d.id);
	    m_entries.append(e);
	    m_byId.insert(e->desc.id, e);
	    continue;
	}
	Entry *e = new Entry;
	e->desc = d;
	int api = apiVersionFromMetadata(meta);
	if (api != QGTCAD_PLUGIN_API_VERSION) {
	    e->failed = true;
	    e->failureReason = QStringLiteral("API version mismatch: plugin=%1, host=%2")
				 .arg(api).arg((int)QGTCAD_PLUGIN_API_VERSION);
	} else {
	    e->staticInstance = staticInstances.at(i);
	    e->triedLoad = true;
	}
	m_entries.append(e);
	m_byId.insert(d.id, e);
	emit factoryRegistered(d.id);
    }

    /* Now scan search directories. */
    for (const QString &dirPath : m_searchDirs) {
	QDir dir(dirPath);
	if (!dir.exists())
	    continue;
	const QFileInfoList files = dir.entryInfoList(QDir::Files | QDir::NoDotAndDotDot);
	for (const QFileInfo &fi : files) {
	    if (!QLibrary::isLibrary(fi.absoluteFilePath()))
		continue;
	    /* If we already have a loader for this path, keep it. */
	    bool already = false;
	    for (Entry *existing : oldEntries) {
		if (existing->loader &&
		    existing->loader->fileName() == fi.absoluteFilePath()) {
		    already = true;
		    if (!m_byId.contains(existing->desc.id)) {
			m_entries.append(existing);
			m_byId.insert(existing->desc.id, existing);
			kept.remove(existing->desc.id);
		    }
		    break;
		}
	    }
	    if (already)
		continue;

	    QPluginLoader *loader = new QPluginLoader(fi.absoluteFilePath());
	    Entry *e = new Entry;
	    e->loader = loader;
	    readMetadata(loader, *e);
	    if (e->desc.id.isEmpty()) {
		/* Not one of our plugins -- discard quietly. */
		delete loader;
		delete e;
		continue;
	    }
	    if (m_byId.contains(e->desc.id)) {
		/* Duplicate id -- keep the first, drop this one. */
		delete loader;
		delete e;
		continue;
	    }
	    m_entries.append(e);
	    m_byId.insert(e->desc.id, e);
	    emit factoryRegistered(e->desc.id);
	}
    }

    /* Anything still in 'kept' was not rediscovered -- treat as
     * unregistered. */
    for (auto it = kept.constBegin(); it != kept.constEnd(); ++it) {
	Entry *e = it.value();
	emit factoryUnregistered(e->desc.id);
	if (e->loader) {
	    e->loader->unload();
	    delete e->loader;
	}
	delete e;
    }

    /* Deterministic ordering: (sortKey, id). */
    std::sort(m_entries.begin(), m_entries.end(),
	      [](const Entry *a, const Entry *b) {
		  if (a->desc.sortKey != b->desc.sortKey)
		      return a->desc.sortKey < b->desc.sortKey;
		  return a->desc.id < b->desc.id;
	      });
}

void
QgPluginManager::unloadAll()
{
    for (Entry *e : m_entries) {
	if (e->loader) {
	    e->loader->unload();
	}
	e->triedLoad = false;
    }
}

void
QgPluginManager::reload()
{
    unloadAll();
    rescan();
}

QList<QgPluginDescriptor>
QgPluginManager::descriptors() const
{
    QList<QgPluginDescriptor> out;
    out.reserve(m_entries.size());
    for (const Entry *e : m_entries)
	out.append(e->desc);
    return out;
}

QList<QgPluginDescriptor>
QgPluginManager::descriptors(const QString &category) const
{
    QList<QgPluginDescriptor> out;
    for (const Entry *e : m_entries) {
	if (e->desc.category == category)
	    out.append(e->desc);
    }
    return out;
}

QgPluginDescriptor
QgPluginManager::descriptor(const QString &id) const
{
    auto it = m_byId.constFind(id);
    if (it == m_byId.constEnd())
	return QgPluginDescriptor();
    return (*it)->desc;
}

QObject *
QgPluginManager::resolveInstance(Entry &entry)
{
    if (entry.failed)
	return nullptr;
    if (entry.staticInstance)
	return entry.staticInstance;
    if (!entry.loader)
	return nullptr;
    if (!entry.triedLoad) {
	entry.triedLoad = true;
	if (!entry.loader->load()) {
	    entry.failed = true;
	    entry.failureReason = entry.loader->errorString();
	    return nullptr;
	}
    }
    return entry.loader->instance();
}

QObject *
QgPluginManager::instance(const QString &id)
{
    if (!isEnabled(id))
	return nullptr;
    auto it = m_byId.find(id);
    if (it == m_byId.end())
	return nullptr;
    return resolveInstance(*(*it));
}

bool
QgPluginManager::isEnabled(const QString &id) const
{
    if (!m_byId.contains(id))
	return false;
    QSettings settings;
    settings.beginGroup(m_settingsGroup);
    bool en = settings.value(QStringLiteral("enabled/%1").arg(id), true).toBool();
    settings.endGroup();
    return en;
}

void
QgPluginManager::persistEnabled(const QString &id, bool enabled)
{
    QSettings settings;
    settings.beginGroup(m_settingsGroup);
    settings.setValue(QStringLiteral("enabled/%1").arg(id), enabled);
    settings.endGroup();
}

bool
QgPluginManager::enable(const QString &id)
{
    if (!m_byId.contains(id))
	return false;
    persistEnabled(id, true);
    return true;
}

bool
QgPluginManager::disable(const QString &id)
{
    if (!m_byId.contains(id))
	return false;
    persistEnabled(id, false);
    return true;
}

QString
QgPluginManager::diagnosticSummary() const
{
    QString out;
    for (const Entry *e : m_entries) {
	out += QStringLiteral("%1\t[%2]\t%3%4\n")
		.arg(e->desc.id,
		     e->desc.category,
		     e->desc.displayName,
		     e->failed ? QStringLiteral("  (failed: ") + e->failureReason + QStringLiteral(")") : QString());
    }
    return out;
}

QString
QgPluginManager::diagnosticInfo(const QString &id) const
{
    auto it = m_byId.constFind(id);
    if (it == m_byId.constEnd())
	return QStringLiteral("Unknown plugin: %1\n").arg(id);
    const Entry *e = *it;
    QString out;
    out += QStringLiteral("id:           %1\n").arg(e->desc.id);
    out += QStringLiteral("displayName:  %1\n").arg(e->desc.displayName);
    out += QStringLiteral("category:     %1\n").arg(e->desc.category);
    out += QStringLiteral("sortKey:      %1\n").arg(e->desc.sortKey);
    out += QStringLiteral("requiresOpenDb: %1\n").arg(e->desc.requiresOpenDb ? "true" : "false");
    out += QStringLiteral("requiresView:   %1\n").arg(e->desc.requiresView ? "true" : "false");
    if (!e->desc.vendor.isEmpty())
	out += QStringLiteral("vendor:       %1\n").arg(e->desc.vendor);
    if (!e->desc.version.isEmpty())
	out += QStringLiteral("version:      %1\n").arg(e->desc.version);
    if (e->loader)
	out += QStringLiteral("path:         %1\n").arg(e->loader->fileName());
    else if (e->staticInstance)
	out += QStringLiteral("path:         (static plugin)\n");
    out += QStringLiteral("enabled:      %1\n").arg(isEnabled(id) ? "true" : "false");
    if (e->failed)
	out += QStringLiteral("failure:      %1\n").arg(e->failureReason);
    if (!e->desc.description.isEmpty())
	out += QStringLiteral("description:\n%1\n").arg(e->desc.description);
    return out;
}

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
