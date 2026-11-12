/*       T E S T _ Q T C A D _ P L U G I N _ C O N T E X T . C P P
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
#include <QSignalSpy>
#include <QtTest/QTest>

#include "bu/app.h"
#include "qtcad/QgPluginContext.h"
#include "qtcad/QgSession.h"

class QgPluginContextTest : public QObject
{
    Q_OBJECT

    private slots:
	void defaultsAreNullSafe();
	void resolvesChangingHostState();
	void relaysHostNotifications();
};

void
QgPluginContextTest::defaultsAreNullSafe()
{
    QgPluginContext context;
    QVERIFY(context.getGed() == nullptr);
    QVERIFY(context.activeViewContext() == nullptr);
    QVERIFY(context.getViewWidget() == nullptr);
    context.logMessage(QStringLiteral("discarded"));
}

void
QgPluginContextTest::resolvesChangingHostState()
{
    QgSession firstSession;
    QgSession secondSession;
    QgSession *activeSession = &firstSession;
    QString loggedMessage;

    QgPluginContext context;
    context.gedAccessor = [&activeSession]() {
	return activeSession->ged();
    };
    context.viewContextAccessor = [&activeSession]() {
	return activeSession->activeView();
    };
    context.log = [&loggedMessage](const QString &message) {
	loggedMessage = message;
    };

    QCOMPARE(context.getGed(), firstSession.ged());
    QCOMPARE(context.activeViewContext(), firstSession.activeView());
    activeSession = &secondSession;
    QCOMPARE(context.getGed(), secondSession.ged());
    QCOMPARE(context.activeViewContext(), secondSession.activeView());

    context.logMessage(QStringLiteral("context message"));
    QCOMPARE(loggedMessage, QStringLiteral("context message"));
}

void
QgPluginContextTest::relaysHostNotifications()
{
    const QgViewUpdateFlags expectedFlags = 17;
    QgPluginNotifier notifier;
    QSignalSpy databaseSpy(&notifier, &QgPluginNotifier::dbChanged);
    QSignalSpy viewSpy(&notifier, &QgPluginNotifier::viewChanged);
    QSignalSpy updateSpy(&notifier, &QgPluginNotifier::viewUpdated);
    QSignalSpy settingsSpy(&notifier, &QgPluginNotifier::settingsChanged);

    emit notifier.dbChanged();
    emit notifier.viewChanged();
    emit notifier.viewUpdated(expectedFlags);
    emit notifier.settingsChanged();

    QCOMPARE(databaseSpy.size(), 1);
    QCOMPARE(viewSpy.size(), 1);
    QCOMPARE(updateSpy.size(), 1);
    QCOMPARE(updateSpy.first().first().toULongLong(), expectedFlags);
    QCOMPARE(settingsSpy.size(), 1);
}

int
main(int argc, char **argv)
{
    bu_setprogname(argv[0]);
    QCoreApplication application(argc, argv);
    QgPluginContextTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "test_qtcad_plugin_context.moc"

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
