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

#include <algorithm>

#include <QByteArray>
#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QJsonObject>
#include <QJsonValue>
#include <QLibrary>
#include <QPluginLoader>
#include <QSettings>
#include <QThread>
#include <QVariant>
#include <QVector>

#include "qtcad/QgPluginManager.h"
#include "qtcad/QgPluginVersion.h"

static const char *qg_default_settings_group = "qtcad/plugins";

struct QgPluginManager::Entry {
    QgPluginDescriptor descriptor;
    QPluginLoader *loader = nullptr;
    QObject *staticInstance = nullptr;
    bool triedLoad = false;
    bool failed = false;
    bool metadataFailed = false;
    QString failureReason;
};

static QString
qg_json_string(const QJsonObject &object, const char *key,
	const QString &defaultValue = QString())
{
    const QJsonValue value = object.value(QLatin1String(key));
    return value.isString() ? value.toString() : defaultValue;
}

static int
qg_json_int(const QJsonObject &object, const char *key, int defaultValue)
{
    const QJsonValue value = object.value(QLatin1String(key));
    if (value.isDouble())
	return static_cast<int>(value.toDouble());
    if (value.isString()) {
	bool conversionSucceeded = false;
	const int result = value.toString().toInt(&conversionSucceeded);
	if (conversionSucceeded)
	    return result;
    }
    return defaultValue;
}

static bool
qg_json_bool(const QJsonObject &object, const char *key, bool defaultValue)
{
    const QJsonValue value = object.value(QLatin1String(key));
    return value.isBool() ? value.toBool() : defaultValue;
}

static QJsonObject
qg_plugin_metadata(const QJsonObject &metadata)
{
    return metadata.value(QLatin1String("MetaData")).toObject();
}

static QgPluginDescriptor
qg_plugin_descriptor(const QJsonObject &metadata)
{
    const QJsonObject object = qg_plugin_metadata(metadata);
    QgPluginDescriptor descriptor;
    descriptor.id = qg_json_string(object, "id");
    descriptor.displayName = qg_json_string(object, "displayName",
	    descriptor.id);
    descriptor.category = qg_json_string(object, "category");
    descriptor.iconName = qg_json_string(object, "iconName");
    descriptor.shortcutHint = qg_json_string(object, "shortcutHint");
    descriptor.sortKey = qg_json_int(object, "sortKey", 0);
    descriptor.requiresOpenDb = qg_json_bool(object, "requiresOpenDb", false);
    descriptor.requiresView = qg_json_bool(object, "requiresView", false);
    descriptor.description = qg_json_string(object, "description");
    descriptor.vendor = qg_json_string(object, "vendor");
    descriptor.version = qg_json_string(object, "version");
    return descriptor;
}

static int
qg_plugin_api_version(const QJsonObject &metadata)
{
    return qg_json_int(qg_plugin_metadata(metadata), "apiVersion", -1);
}

static QString
qg_api_version_error(int pluginVersion)
{
    if (pluginVersion < 0) {
	return QStringLiteral("plugin metadata does not declare apiVersion");
    }
    return QStringLiteral("API version mismatch: plugin=%1, host=%2")
	.arg(pluginVersion).arg(QGTCAD_PLUGIN_API_VERSION);
}

QgPluginManager::QgPluginManager(const QStringList &searchDirectories,
	const QString &settingsGroup, QObject *parent)
    : QObject(parent),
      m_searchDirectories(searchDirectories),
      m_settingsGroup(settingsGroup.isEmpty() ?
	QString::fromLatin1(qg_default_settings_group) : settingsGroup)
{
    rescan();
}

QgPluginManager::~QgPluginManager()
{
    QStringList unloadErrors;
    if (!unloadAll(&unloadErrors)) {
	for (const QString &error : unloadErrors)
	    qWarning().noquote() << error;
    }
    for (Entry *entry : m_entries) {
	delete entry->loader;
	delete entry;
    }
}

void
QgPluginManager::readMetadata(QPluginLoader *loader, Entry &entry)
{
    const QJsonObject metadata = loader->metaData();
    entry.descriptor = qg_plugin_descriptor(metadata);
    const int apiVersion = qg_plugin_api_version(metadata);
    if (apiVersion == QGTCAD_PLUGIN_API_VERSION)
	return;

    entry.failed = true;
    entry.metadataFailed = true;
    entry.failureReason = qg_api_version_error(apiVersion);
}

void
QgPluginManager::addStaticPlugins()
{
    const QVector<QStaticPlugin> plugins = QPluginLoader::staticPlugins();
    for (const QStaticPlugin &plugin : plugins) {
	const QJsonObject metadata = plugin.metaData();
	const QgPluginDescriptor descriptor = qg_plugin_descriptor(metadata);
	if (descriptor.id.isEmpty())
	    continue;
	if (m_byId.contains(descriptor.id)) {
	    const Entry *existing = m_byId.value(descriptor.id);
	    if (!existing->loader &&
		existing->staticInstance == plugin.instance())
		continue;
	    m_scanDiagnostics.append(
		QStringLiteral("duplicate plugin id '%1' from a static plugin")
		.arg(descriptor.id));
	    continue;
	}

	Entry *entry = new Entry;
	entry->descriptor = descriptor;
	const int apiVersion = qg_plugin_api_version(metadata);
	if (apiVersion != QGTCAD_PLUGIN_API_VERSION) {
	    entry->failed = true;
	    entry->metadataFailed = true;
	    entry->failureReason = qg_api_version_error(apiVersion);
	} else {
	    entry->staticInstance = plugin.instance();
	    entry->triedLoad = true;
	    if (!entry->staticInstance) {
		entry->failed = true;
		entry->failureReason = QStringLiteral(
		    "static plugin did not provide an instance");
	    }
	}
	m_entries.append(entry);
	m_byId.insert(descriptor.id, entry);
	emit factoryRegistered(descriptor.id);
    }
}

void
QgPluginManager::addDynamicPlugins()
{
    for (const QString &directoryPath : m_searchDirectories) {
	QDir directory(directoryPath);
	if (!directory.exists())
	    continue;
	const QFileInfoList files = directory.entryInfoList(
	    QDir::Files | QDir::NoDotAndDotDot, QDir::Name);
	for (const QFileInfo &file : files) {
	    const QString path = file.absoluteFilePath();
	    if (!QLibrary::isLibrary(path))
		continue;

	    bool knownPath = false;
	    for (const Entry *entry : m_entries) {
		if (entry->loader && entry->loader->fileName() == path) {
		    knownPath = true;
		    break;
		}
	    }
	    if (knownPath)
		continue;

	    QPluginLoader *loader = new QPluginLoader(path);
	    loader->setLoadHints(loader->loadHints() &
		~QLibrary::PreventUnloadHint);
	    Entry *entry = new Entry;
	    entry->loader = loader;
	    readMetadata(loader, *entry);
	    if (entry->descriptor.id.isEmpty()) {
		m_scanDiagnostics.append(QStringLiteral(
		    "ignored '%1': plugin metadata does not declare an id")
		    .arg(path));
		delete loader;
		delete entry;
		continue;
	    }
	    if (m_byId.contains(entry->descriptor.id)) {
		m_scanDiagnostics.append(QStringLiteral(
		    "ignored '%1': duplicate plugin id '%2'")
		    .arg(path, entry->descriptor.id));
		delete loader;
		delete entry;
		continue;
	    }

	    m_entries.append(entry);
	    m_byId.insert(entry->descriptor.id, entry);
	    emit factoryRegistered(entry->descriptor.id);
	}
    }
}

void
QgPluginManager::sortEntries()
{
    std::sort(m_entries.begin(), m_entries.end(),
	[](const Entry *left, const Entry *right) {
	    if (left->descriptor.sortKey != right->descriptor.sortKey) {
		return left->descriptor.sortKey < right->descriptor.sortKey;
	    }
	    return left->descriptor.id < right->descriptor.id;
	});
}

void
QgPluginManager::rescan()
{
    if (QThread::currentThread() != thread()) {
	m_scanDiagnostics.append(QStringLiteral(
	    "plugin scan must run on the manager thread"));
	return;
    }

    m_scanDiagnostics.clear();
    addStaticPlugins();
    addDynamicPlugins();
    sortEntries();
    emit catalogChanged();
}

bool
QgPluginManager::unloadAll(QStringList *errors)
{
    if (m_unloading) {
	if (errors)
	    errors->append(QStringLiteral("plugin unload is already active"));
	return false;
    }
    if (QThread::currentThread() != thread()) {
	if (errors) {
	    errors->append(QStringLiteral(
		"plugin unload must run on the manager thread"));
	}
	return false;
    }

    bool hasLoadedPlugin = false;
    for (const Entry *entry : m_entries) {
	if (entry->loader && entry->loader->isLoaded()) {
	    hasLoadedPlugin = true;
	    break;
	}
    }
    if (hasLoadedPlugin) {
	m_unloading = true;
	emit pluginsAboutToUnload();
    }

    bool succeeded = true;
    for (Entry *entry : m_entries) {
	if (!entry->loader)
	    continue;
	if (entry->loader->isLoaded() && !entry->loader->unload()) {
	    succeeded = false;
	    if (errors) {
		errors->append(QStringLiteral("%1: %2")
		    .arg(entry->descriptor.id,
			entry->loader->errorString()));
	    }
	}
	entry->triedLoad = entry->loader->isLoaded();
	if (!entry->metadataFailed && !entry->loader->isLoaded()) {
	    entry->failed = false;
	    entry->failureReason.clear();
	}
    }
    m_unloading = false;
    return succeeded;
}

void
QgPluginManager::clearDynamicCatalog()
{
    QList<Entry *> staticEntries;
    for (Entry *entry : m_entries) {
	if (!entry->loader) {
	    staticEntries.append(entry);
	    continue;
	}
	emit factoryUnregistered(entry->descriptor.id);
	m_byId.remove(entry->descriptor.id);
	delete entry->loader;
	delete entry;
    }
    m_entries = staticEntries;
}

bool
QgPluginManager::reload(QStringList *errors)
{
    if (!unloadAll(errors))
	return false;
    clearDynamicCatalog();
    rescan();
    return true;
}

QList<QgPluginDescriptor>
QgPluginManager::descriptors() const
{
    QList<QgPluginDescriptor> result;
    result.reserve(m_entries.size());
    for (const Entry *entry : m_entries)
	result.append(entry->descriptor);
    return result;
}

QList<QgPluginDescriptor>
QgPluginManager::descriptors(const QString &category) const
{
    QList<QgPluginDescriptor> result;
    for (const Entry *entry : m_entries) {
	if (entry->descriptor.category == category)
	    result.append(entry->descriptor);
    }
    return result;
}

QgPluginDescriptor
QgPluginManager::descriptor(const QString &id) const
{
    const auto entry = m_byId.constFind(id);
    return entry == m_byId.constEnd() ? QgPluginDescriptor() :
	(*entry)->descriptor;
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

    QObject *pluginInstance = entry.loader->instance();
    if (!pluginInstance) {
	entry.failed = true;
	entry.failureReason = entry.loader->errorString();
    }
    return pluginInstance;
}

QObject *
QgPluginManager::instance(const QString &id)
{
    if (m_unloading || !isEnabled(id))
	return nullptr;
    const auto entry = m_byId.find(id);
    return entry == m_byId.end() ? nullptr : resolveInstance(*(*entry));
}

bool
QgPluginManager::isEnabled(const QString &id) const
{
    if (!m_byId.contains(id))
	return false;
    const QByteArray propertyName = QByteArrayLiteral(
	"qtcad.plugin.enabled.") + id.toUtf8();
    const QVariant enabledOverride = property(propertyName.constData());
    if (enabledOverride.isValid())
	return enabledOverride.toBool();

    QSettings settings;
    settings.beginGroup(m_settingsGroup);
    const bool enabled = settings.value(
	QStringLiteral("enabled/%1").arg(id), true).toBool();
    settings.endGroup();
    return enabled;
}

void
QgPluginManager::persistEnabled(const QString &id, bool enabled)
{
    QSettings settings;
    settings.beginGroup(m_settingsGroup);
    settings.setValue(QStringLiteral("enabled/%1").arg(id), enabled);
    settings.endGroup();
    settings.sync();
}

bool
QgPluginManager::setEnabled(const QString &id, bool enabled)
{
    if (!m_byId.contains(id))
	return false;
    if (isEnabled(id) == enabled)
	return true;

    const QByteArray propertyName = QByteArrayLiteral(
	"qtcad.plugin.enabled.") + id.toUtf8();
    setProperty(propertyName.constData(), enabled);
    persistEnabled(id, enabled);
    emit pluginEnabledChanged(id, enabled);
    emit catalogChanged();
    return true;
}

bool
QgPluginManager::enable(const QString &id)
{
    return setEnabled(id, true);
}

bool
QgPluginManager::disable(const QString &id)
{
    return setEnabled(id, false);
}

QString
QgPluginManager::diagnosticSummary() const
{
    QString result;
    for (const Entry *entry : m_entries) {
	result += QStringLiteral("%1\t[%2]\t%3%4\n")
	    .arg(entry->descriptor.id, entry->descriptor.category,
		entry->descriptor.displayName,
		entry->failed ? QStringLiteral("  (failed: ") +
		    entry->failureReason + QLatin1Char(')') : QString());
    }
    for (const QString &diagnostic : m_scanDiagnostics)
	result += QStringLiteral("warning\t%1\n").arg(diagnostic);
    return result;
}

QString
QgPluginManager::diagnosticInfo(const QString &id) const
{
    const auto found = m_byId.constFind(id);
    if (found == m_byId.constEnd())
	return QStringLiteral("Unknown plugin: %1\n").arg(id);

    const Entry *entry = *found;
    const QgPluginDescriptor &plugin = entry->descriptor;
    QString result;
    result += QStringLiteral("id:             %1\n").arg(plugin.id);
    result += QStringLiteral("displayName:    %1\n").arg(plugin.displayName);
    result += QStringLiteral("category:       %1\n").arg(plugin.category);
    result += QStringLiteral("sortKey:        %1\n").arg(plugin.sortKey);
    result += QStringLiteral("requiresOpenDb: %1\n").arg(
	plugin.requiresOpenDb ? QStringLiteral("true") : QStringLiteral("false"));
    result += QStringLiteral("requiresView:   %1\n").arg(
	plugin.requiresView ? QStringLiteral("true") : QStringLiteral("false"));
    if (!plugin.vendor.isEmpty())
	result += QStringLiteral("vendor:         %1\n").arg(plugin.vendor);
    if (!plugin.version.isEmpty())
	result += QStringLiteral("version:        %1\n").arg(plugin.version);
    result += entry->loader ?
	QStringLiteral("path:           %1\n").arg(entry->loader->fileName()) :
	QStringLiteral("path:           (static plugin)\n");
    result += QStringLiteral("enabled:        %1\n").arg(
	isEnabled(id) ? QStringLiteral("true") : QStringLiteral("false"));
    if (entry->failed)
	result += QStringLiteral("failure:        %1\n").arg(
	    entry->failureReason);
    if (!plugin.description.isEmpty())
	result += QStringLiteral("description:\n%1\n").arg(plugin.description);
    return result;
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
