/*       T E S T _ Q T C A D _ P L U G I N _ M A N A G E R . C P P
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

#include <QCoreApplication>
#include <QPointer>
#include <QSettings>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest/QTest>

#include "bu/app.h"
#include "qtcad/QgPluginCommands.h"
#include "qtcad/QgPluginInterfaces.h"
#include "qtcad/QgPluginManager.h"

static const char *valid_plugin_id = "org.brlcad.qtcad.test.command";
static const char *missing_api_plugin_id =
    "org.brlcad.qtcad.test.missing_api";

class QgPluginManagerTest : public QObject
{
    Q_OBJECT

    private slots:
	void initTestCase();
	void discoversAndLoadsPlugins();
	void rejectsMissingApiVersions();
	void publishesAvailabilityChanges();
	void reloadsPluginCode();
	void providesHostNeutralCommands();

    private:
	QString pluginDirectory() const;
	QString settingsGroup() const;
	QTemporaryDir settingsDirectory;
};

QString
QgPluginManagerTest::pluginDirectory() const
{
    return QString::fromLocal8Bit(qgetenv("QTCAD_TEST_PLUGIN_DIR"));
}

QString
QgPluginManagerTest::settingsGroup() const
{
    return QStringLiteral("qtcad/tests/plugin_manager");
}

void
QgPluginManagerTest::initTestCase()
{
    QVERIFY(settingsDirectory.isValid());
    QVERIFY(!pluginDirectory().isEmpty());
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope,
	settingsDirectory.path());
    QSettings settings;
    settings.beginGroup(settingsGroup());
    settings.remove(QString());
    settings.endGroup();
    settings.sync();
}

void
QgPluginManagerTest::discoversAndLoadsPlugins()
{
    QgPluginManager manager(QStringList() << pluginDirectory(),
	settingsGroup());
    const QList<QgPluginDescriptor> plugins = manager.descriptors();
    QCOMPARE(plugins.size(), 2);
    QCOMPARE(plugins.at(0).id, QString::fromLatin1(missing_api_plugin_id));
    QCOMPARE(plugins.at(1).id, QString::fromLatin1(valid_plugin_id));

    QList<IQgCommand *> commands = manager.factories<IQgCommand>(
	QStringLiteral("test.command"));
    QCOMPARE(commands.size(), 1);
    QCOMPARE(commands.first()->verb(), QStringLiteral("qtcad_test"));
    QString output;
    QCOMPARE(commands.first()->run(nullptr,
	QStringList() << QStringLiteral("one") << QStringLiteral("two"),
	&output, nullptr), 0);
    QCOMPARE(output, QStringLiteral("one two"));

    manager.rescan();
    QCOMPARE(manager.descriptors().size(), 2);
}

void
QgPluginManagerTest::rejectsMissingApiVersions()
{
    QgPluginManager manager(QStringList() << pluginDirectory(),
	settingsGroup());
    QVERIFY(manager.instance(QString::fromLatin1(
	missing_api_plugin_id)) == nullptr);
    QVERIFY(manager.diagnosticInfo(QString::fromLatin1(
	missing_api_plugin_id)).contains(
	QStringLiteral("does not declare apiVersion")));
}

void
QgPluginManagerTest::publishesAvailabilityChanges()
{
    QgPluginManager manager(QStringList() << pluginDirectory(),
	settingsGroup());
    const QString id = QString::fromLatin1(valid_plugin_id);
    QSignalSpy availabilitySpy(&manager,
	&QgPluginManager::pluginEnabledChanged);
    QSignalSpy catalogSpy(&manager, &QgPluginManager::catalogChanged);

    QVERIFY(manager.disable(id));
    QVERIFY(!manager.isEnabled(id));
    QVERIFY(manager.instance(id) == nullptr);
    QCOMPARE(availabilitySpy.size(), 1);
    QCOMPARE(catalogSpy.size(), 1);
    QVERIFY(manager.disable(id));
    QCOMPARE(availabilitySpy.size(), 1);
    QCOMPARE(catalogSpy.size(), 1);

    {
	QgPluginManager restored(QStringList() << pluginDirectory(),
	    settingsGroup());
	QVERIFY(!restored.isEnabled(id));
	QVERIFY(restored.instance(id) == nullptr);
    }

    QVERIFY(manager.enable(id));
    QVERIFY(manager.isEnabled(id));
    QVERIFY(manager.instance(id) != nullptr);
    QCOMPARE(availabilitySpy.size(), 2);
    QCOMPARE(catalogSpy.size(), 2);

    QVERIFY(!manager.enable(QStringLiteral("unknown.plugin")));
    QCOMPARE(availabilitySpy.size(), 2);
}

void
QgPluginManagerTest::reloadsPluginCode()
{
    QgPluginManager manager(QStringList() << pluginDirectory(),
	settingsGroup());
    const QString id = QString::fromLatin1(valid_plugin_id);
    QPointer<QObject> original = manager.instance(id);
    QVERIFY(!original.isNull());

    QSignalSpy unloadSpy(&manager,
	&QgPluginManager::pluginsAboutToUnload);
    QSignalSpy catalogSpy(&manager, &QgPluginManager::catalogChanged);
    QSignalSpy registeredSpy(&manager,
	&QgPluginManager::factoryRegistered);
    QSignalSpy unregisteredSpy(&manager,
	&QgPluginManager::factoryUnregistered);
    QStringList errors;
    QVERIFY2(manager.reload(&errors), qPrintable(errors.join(QLatin1Char('\n'))));
    QVERIFY(original.isNull());
    QCOMPARE(unloadSpy.size(), 1);
    QCOMPARE(catalogSpy.size(), 1);
    QCOMPARE(registeredSpy.size(), 2);
    QCOMPARE(unregisteredSpy.size(), 2);
    QVERIFY(manager.instance(id) != nullptr);
}

void
QgPluginManagerTest::providesHostNeutralCommands()
{
    QgPluginManager manager(QStringList() << pluginDirectory(),
	settingsGroup());
    QString output;
    QString error;
    QCOMPARE(QgPluginCommands::run(&manager,
	QStringList() << QStringLiteral("list"), &output, &error), 0);
    QVERIFY(output.contains(QString::fromLatin1(valid_plugin_id)));
    QVERIFY(error.isEmpty());

    output.clear();
    QCOMPARE(QgPluginCommands::run(&manager,
	QStringList() << QStringLiteral("info") <<
	QString::fromLatin1(valid_plugin_id), &output, &error), 0);
    QVERIFY(output.contains(QStringLiteral("Test Command")));

    error.clear();
    QCOMPARE(QgPluginCommands::run(&manager,
	QStringList() << QStringLiteral("unknown"), &output, &error), 1);
    QVERIFY(error.contains(QStringLiteral("unknown subcommand")));
}

int
main(int argc, char **argv)
{
    bu_setprogname(argv[0]);
    QCoreApplication application(argc, argv);
    QgPluginManagerTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "test_qtcad_plugin_manager.moc"

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
