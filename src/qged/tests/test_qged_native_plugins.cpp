/*       T E S T _ Q G E D _ N A T I V E _ P L U G I N S . C P P
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 */

#include "common.h"

#include <QApplication>
#include <QSettings>
#include <QTemporaryDir>
#include <QtTest/QTest>

#include "bu/app.h"
#include "qtcad/QgPaletteController.h"
#include "qtcad/QgPluginContext.h"
#include "qtcad/QgPluginManager.h"
#include "qtcad/QgSession.h"
#include "qtcad/QgToolBase.h"
#include "qtcad/QgToolPalette.h"
#include "qtcad/QgView.h"

class QgedNativePluginTest : public QObject
{
    Q_OBJECT

    private slots:
	void discoversCreatesAndUnloadsTools();
};

void
QgedNativePluginTest::discoversCreatesAndUnloadsTools()
{
    const QString pluginDirectory =
	QString::fromLocal8Bit(qgetenv("QGED_TEST_PLUGIN_DIR"));
    QVERIFY(!pluginDirectory.isEmpty());

    QTemporaryDir settingsDirectory;
    QVERIFY(settingsDirectory.isValid());
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope,
	settingsDirectory.path());

    QgPluginManager manager(QStringList() << pluginDirectory,
	QStringLiteral("qged/tests/native_plugins"));
    QVERIFY2(manager.scanDiagnostics().isEmpty(),
	qPrintable(manager.scanDiagnostics().join(QLatin1Char('\n'))));
    const QStringList expectedIds = {
	QStringLiteral("org.brlcad.qged.view.info"),
	QStringLiteral("org.brlcad.qged.view.settings"),
	QStringLiteral("org.brlcad.qged.view.select"),
	QStringLiteral("org.brlcad.qged.view.measure")
    };
    for (const QString &id : expectedIds)
	QVERIFY2(!manager.descriptor(id).id.isEmpty(), qPrintable(id));

    QgSession session;
    QgPluginNotifier notifier;
    QgPluginContext context;
    context.gedAccessor = [&session]() { return session.ged(); };
    context.viewContextAccessor = [&session]() {
	return session.activeView();
    };
    context.selectionSetAccessor = []() {
	return QStringLiteral("native-plugin-test");
    };
    context.notifier = &notifier;

    QgToolPalette palette;
    palette.setAlwaysSelected(0);
    QgPaletteController controller(&palette, &manager,
	QStringLiteral("qged.view"), &context);
    QgView view(nullptr, QgView_SW);
    context.viewWidgetAccessor = [&view]() { return &view; };
    controller.setActiveView(&view);
    controller.populate();

    const QList<QgToolPaletteButton *> buttons =
	palette.findChildren<QgToolPaletteButton *>();
    for (const QString &id : expectedIds) {
	QgToolPaletteButton *matchingButton = nullptr;
	for (QgToolPaletteButton *button : buttons) {
	    if (button->property("qgTestId").toString() ==
		    id + QStringLiteral(".activate")) {
		matchingButton = button;
		break;
	    }
	}
	QVERIFY2(matchingButton, qPrintable(id));
	matchingButton->click();
	QVERIFY(controller.currentTool());
    }

    QStringList errors;
    QVERIFY2(manager.unloadAll(&errors),
	qPrintable(errors.join(QLatin1Char('\n'))));
    QVERIFY(controller.currentTool() == nullptr);
    QVERIFY(view.curr_event_filter == nullptr);
}

int
main(int argc, char **argv)
{
    bu_setprogname(argv[0]);
    QApplication application(argc, argv);
    QgedNativePluginTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "test_qged_native_plugins.moc"

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
