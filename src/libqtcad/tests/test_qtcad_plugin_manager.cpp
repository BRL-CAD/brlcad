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

#include <QApplication>
#include <QDialog>
#include <QDockWidget>
#include <QMainWindow>
#include <QMenu>
#include <QPointer>
#include <QPushButton>
#include <QSettings>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest/QTest>

#include "bu/app.h"
#include "qtcad/QgPluginCommands.h"
#include "qtcad/QgPluginContext.h"
#include "qtcad/QgPluginInterfaces.h"
#include "qtcad/QgPluginManager.h"
#include "qtcad/QgPaletteController.h"
#include "qtcad/QgToolBase.h"
#include "qtcad/QgToolPalette.h"
#include "qtcad/QgView.h"
#include "qtcad/QgWindowExtensionController.h"

static const char *valid_plugin_id = "org.brlcad.qtcad.test.command";
static const char *missing_api_plugin_id =
    "org.brlcad.qtcad.test.missing_api";
static const char *panel_plugin_id = "org.brlcad.qtcad.test.panel";
static const char *dialog_plugin_id = "org.brlcad.qtcad.test.dialog";

class QgPluginManagerTest : public QObject
{
    Q_OBJECT

    private slots:
	void initTestCase();
	void discoversAndLoadsPlugins();
	void managesToolPalettes();
	void tearsDownPaletteBeforeController();
	void rejectsMissingApiVersions();
	void publishesAvailabilityChanges();
	void reloadsPluginCode();
	void providesHostNeutralCommands();
	void dispatchesPluginCommands();
	void hostsWindowExtensions();

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
    QCOMPARE(plugins.size(), 4);
    QCOMPARE(plugins.at(0).id, QString::fromLatin1(missing_api_plugin_id));
    QCOMPARE(plugins.at(1).id, QString::fromLatin1(valid_plugin_id));
    QCOMPARE(plugins.at(2).id, QString::fromLatin1(panel_plugin_id));
    QCOMPARE(plugins.at(3).id, QString::fromLatin1(dialog_plugin_id));

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
    QCOMPARE(manager.descriptors().size(), 4);
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
QgPluginManagerTest::managesToolPalettes()
{
    QgPluginManager manager(QStringList() << pluginDirectory(),
	settingsGroup());
    QgPluginNotifier notifier;
    QgPluginContext context;
    context.notifier = &notifier;
    QgToolPalette palette;
    palette.setAlwaysSelected(0);
    palette.setIconWidth(41);
    palette.setIconHeight(37);
    palette.addElement(nullptr);
    QgPaletteController controller(&palette, &manager,
	QStringLiteral("test.command"), &context);

    controller.populate();
    QCOMPARE(manager.factories<IQgToolFactory>(
	QStringLiteral("test.command")).size(), 1);
    QCOMPARE(palette.findChildren<QgToolPaletteElement *>().size(), 1);
    QVERIFY(controller.currentTool() == nullptr);

    palette.resize(1, 100);
    palette.button_layout_resize();
    QCOMPARE(palette.findChildren<QgToolPaletteElement *>().size(), 1);

    QgToolPaletteButton *activateButton = nullptr;
    const QList<QgToolPaletteButton *> buttons =
	palette.findChildren<QgToolPaletteButton *>();
    for (QgToolPaletteButton *button : buttons) {
	if (button->property("qgTestId").toString().endsWith(
		QStringLiteral(".activate"))) {
	    activateButton = button;
	    break;
	}
    }
    QVERIFY(activateButton != nullptr);
    QCOMPARE(activateButton->minimumWidth(), 41);
    QCOMPARE(activateButton->maximumWidth(), 41);
    QCOMPARE(activateButton->minimumHeight(), 37);
    QCOMPARE(activateButton->maximumHeight(), 37);
    activateButton->click();

    QPointer<QgToolBase> tool = controller.currentTool();
    QVERIFY(!tool.isNull());
    QVERIFY(tool->isActive());
    QCOMPARE(tool->property("refreshCount").toInt(), 0);
    palette.do_view_update(QG_VIEW_REFRESH);
    QCOMPARE(tool->property("refreshCount").toInt(), 1);

    emit notifier.dbChanged();
    emit notifier.viewChanged();
    emit notifier.settingsChanged();
    QCOMPARE(tool->property("dbChangeCount").toInt(), 1);
    QCOMPARE(tool->property("viewChangeCount").toInt(), 1);
    QCOMPARE(tool->property("settingsChangeCount").toInt(), 1);

    QPointer<QObject> filter = tool->viewEventFilter();
    QVERIFY(qobject_cast<IQgViewEventFilter *>(filter.data()) != nullptr);

    QObject firstFilter;
    QObject secondFilter;
    QgView view(nullptr, QgView_SW);
    view.add_event_filter(&firstFilter);
    view.add_event_filter(&secondFilter);
    QCOMPARE(view.curr_event_filter, &secondFilter);
    view.clear_event_filter(&firstFilter);
    QCOMPARE(view.curr_event_filter, &secondFilter);
    view.clear_event_filter(&secondFilter);
    QVERIFY(view.curr_event_filter == nullptr);

    controller.setActiveView(&view);
    QCOMPARE(filter->property("attachCount").toInt(), 1);
    controller.setActiveView(nullptr);
    QCOMPARE(filter->property("detachCount").toInt(), 1);
    controller.setActiveView(&view);
    QCOMPARE(filter->property("attachCount").toInt(), 2);
    controller.setActiveView(nullptr);
    QCOMPARE(filter->property("detachCount").toInt(), 2);

    const QString id = QString::fromLatin1(valid_plugin_id);
    QVERIFY(manager.disable(id));
    QVERIFY(tool.isNull());
    QVERIFY(filter.isNull());
    QVERIFY(controller.currentTool() == nullptr);
    QVERIFY(palette.selectedElement() == nullptr);

    QVERIFY(manager.enable(id));
    QCOMPARE(palette.findChildren<QgToolPaletteElement *>().size(), 1);
    QStringList errors;
    QVERIFY2(manager.unloadAll(&errors),
	qPrintable(errors.join(QLatin1Char('\n'))));
    QCOMPARE(palette.findChildren<QgToolPaletteElement *>().size(), 0);
}

void
QgPluginManagerTest::tearsDownPaletteBeforeController()
{
    QgPluginManager manager(QStringList() << pluginDirectory(),
	settingsGroup());
    QgPluginContext context;
    QgToolPalette *palette = new QgToolPalette;
    QgPaletteController *controller = new QgPaletteController(palette,
	&manager, QStringLiteral("test.command"), &context);
    controller->populate();

    const QList<QgToolBase *> tools =
	controller->findChildren<QgToolBase *>(QString(),
	    Qt::FindDirectChildrenOnly);
    QCOMPARE(tools.size(), 1);
    QPointer<QgToolBase> tool = tools.first();

    delete palette;
    QVERIFY(tool.isNull());
    QVERIFY(controller->palette() == nullptr);
    QVERIFY(controller->currentTool() == nullptr);
    delete controller;
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
    QCOMPARE(registeredSpy.size(), 4);
    QCOMPARE(unregisteredSpy.size(), 4);
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

void
QgPluginManagerTest::dispatchesPluginCommands()
{
    QgPluginManager manager(QStringList() << pluginDirectory(),
	settingsGroup());
    QgPluginContext context;
    QString output;
    QString error;
    int result = -1;

    QVERIFY(QgPluginCommands::dispatch(&manager, &context,
	QStringLiteral("test.command"),
	QStringList() << QStringLiteral("qtcad_test") <<
	QStringLiteral("argument"), &result, &output, &error));
    QCOMPARE(result, 0);
    QCOMPARE(output, QStringLiteral("qtcad_test argument"));
    QVERIFY(error.isEmpty());

    output.clear();
    QVERIFY(QgPluginCommands::dispatch(&manager, &context,
	QStringLiteral("test.command"),
	QStringList() << QStringLiteral("qtcad_alias"),
	&result, &output, &error));
    QCOMPARE(result, 0);
    QCOMPARE(output, QStringLiteral("qtcad_alias"));

    QVERIFY(!QgPluginCommands::dispatch(&manager, &context,
	QStringLiteral("test.command"),
	QStringList() << QStringLiteral("not_a_plugin_command"),
	&result, &output, &error));
    QVERIFY(!QgPluginCommands::dispatch(&manager, &context,
	QStringLiteral("other.command"),
	QStringList() << QStringLiteral("qtcad_test"),
	&result, &output, &error));
}

void
QgPluginManagerTest::hostsWindowExtensions()
{
    QgPluginManager manager(QStringList() << pluginDirectory(),
	settingsGroup());
    QgPluginContext context;
    QMainWindow window;
    QMenu panelMenu(&window);
    QMenu dialogMenu(&window);
    QgWindowExtensionController controller(&window, &manager, &context);
    controller.setPanelMenu(&panelMenu, QStringLiteral("test.panel"));
    controller.setDialogMenu(&dialogMenu, QStringLiteral("test.dialog"));
    QVERIFY(!dialogMenu.menuAction()->isVisible());
    controller.rebuild();

    QDockWidget *panel = window.findChild<QDockWidget *>(
	QString::fromLatin1(panel_plugin_id));
    QVERIFY(panel != nullptr);
    QCOMPARE(panel->windowTitle(), QStringLiteral("Test Panel"));
    QCOMPARE(panel->property("qgPluginId").toString(),
	QString::fromLatin1(panel_plugin_id));
    QCOMPARE(panel->property("qgTestId").toString(),
	QString::fromLatin1(panel_plugin_id) + QStringLiteral(".panel"));
    QCOMPARE(panelMenu.actions().size(), 2);
    QCOMPARE(panel->toggleViewAction()->property("qgTestId").toString(),
	QString::fromLatin1(panel_plugin_id) + QStringLiteral(".activate"));

    QVERIFY(dialogMenu.isEnabled());
    QVERIFY(dialogMenu.menuAction()->isVisible());
    QCOMPARE(dialogMenu.actions().size(), 1);
    QAction *dialogAction = dialogMenu.actions().first();
    QCOMPARE(dialogAction->data().toString(),
	QString::fromLatin1(dialog_plugin_id));
    QCOMPARE(dialogAction->property("qgTestId").toString(),
	QString::fromLatin1(dialog_plugin_id) + QStringLiteral(".activate"));
    dialogAction->trigger();
    QApplication::processEvents();

    QDialog *dialog = nullptr;
    for (QDialog *candidate : window.findChildren<QDialog *>()) {
	if (candidate->property("qgPluginId").toString() ==
		QString::fromLatin1(dialog_plugin_id)) {
	    dialog = candidate;
	    break;
	}
    }
    QVERIFY(dialog != nullptr);
    QCOMPARE(dialog->windowTitle(), QStringLiteral("Test Dialog"));
    QCOMPARE(dialog->property("qgTestId").toString(),
	QString::fromLatin1(dialog_plugin_id) + QStringLiteral(".dialog"));

    QPointer<QDockWidget> panelDuringCommandToggle = panel;
    QPointer<QDialog> dialogDuringCommandToggle = dialog;
    QVERIFY(manager.disable(QString::fromLatin1(valid_plugin_id)));
    QVERIFY(!panelDuringCommandToggle.isNull());
    QVERIFY(!dialogDuringCommandToggle.isNull());
    QVERIFY(manager.enable(QString::fromLatin1(valid_plugin_id)));

    QPointer<QDialog> dialogDuringPanelToggle = dialog;
    QVERIFY(manager.disable(QString::fromLatin1(panel_plugin_id)));
    QVERIFY(panelDuringCommandToggle.isNull());
    QVERIFY(!dialogDuringPanelToggle.isNull());
    QVERIFY(panelMenu.actions().isEmpty());
    QVERIFY(manager.enable(QString::fromLatin1(panel_plugin_id)));
    panel = window.findChild<QDockWidget *>(
	QString::fromLatin1(panel_plugin_id));
    QVERIFY(panel != nullptr);

    QPointer<QDockWidget> oldPanel = panel;
    QPointer<QDialog> oldDialog = dialog;
    QPointer<QAction> oldAction = dialogAction;
    QStringList errors;
    QVERIFY2(manager.reload(&errors),
	qPrintable(errors.join(QLatin1Char('\n'))));
    QVERIFY(oldPanel.isNull());
    QVERIFY(oldDialog.isNull());
    QVERIFY(oldAction.isNull());
    QVERIFY(window.findChild<QDockWidget *>(
	QString::fromLatin1(panel_plugin_id)) != nullptr);
    QVERIFY(dialogMenu.isEnabled());
    QCOMPARE(dialogMenu.actions().size(), 1);
}

int
main(int argc, char **argv)
{
    bu_setprogname(argv[0]);
    qputenv("QT_QPA_PLATFORM", QByteArrayLiteral("offscreen"));
    QApplication application(argc, argv);
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
