/*              T E S T _ Q T C A D _ C O N S O L E . C P P
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
#include <QPointer>
#include <QtTest/QTest>

#include "bu/app.h"
#include "qtcad/QgConsole.h"

class QgConsoleTest : public QObject
{
    Q_OBJECT

    private slots:
    void retiresListenersAfterNotifierCallbacks();
    void ownsActiveListeners();
    void ignoresDuplicateListeners();
};

void
QgConsoleTest::retiresListenersAfterNotifierCallbacks()
{
    struct ged_subprocess process = {};
    process.stdout_active = 1;

    QgConsole console(nullptr);
    console.listen(-1, &process, BU_PROCESS_STDOUT, nullptr, nullptr);
    QConsoleListener *listener = console.findListener(&process,
	BU_PROCESS_STDOUT);
    QVERIFY(listener != nullptr);

    listener->disconnectNotifier();
    listener->on_finished();

    QCOMPARE(process.stdout_active, 0);
    QTRY_VERIFY(console.findListener(&process, BU_PROCESS_STDOUT) == nullptr);
    QCOMPARE(process.stdout_active, 0);
}

void
QgConsoleTest::ownsActiveListeners()
{
    struct ged_subprocess process = {};
    process.stderr_active = 1;
    QPointer<QConsoleListener> listener;
    {
	QgConsole console(nullptr);
	console.listen(-1, &process, BU_PROCESS_STDERR, nullptr, nullptr);
	listener = console.findListener(&process, BU_PROCESS_STDERR);
	QVERIFY(!listener.isNull());
    }
    QVERIFY(listener.isNull());
    QCOMPARE(process.stderr_active, 0);
}

void
QgConsoleTest::ignoresDuplicateListeners()
{
    struct ged_subprocess process = {};
    QgConsole console(nullptr);
    console.listen(-1, &process, BU_PROCESS_STDOUT, nullptr, nullptr);
    QPointer<QConsoleListener> original = console.findListener(&process,
	BU_PROCESS_STDOUT);

    console.listen(-1, &process, BU_PROCESS_STDOUT, nullptr, nullptr);

    QVERIFY(!original.isNull());
    QCOMPARE(console.findListener(&process, BU_PROCESS_STDOUT),
	original.data());
}

int
main(int argc, char **argv)
{
    bu_setprogname(argv[0]);
    if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM"))
	qputenv("QT_QPA_PLATFORM", QByteArray("offscreen"));
    QApplication app(argc, argv);
    QgConsoleTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "test_qtcad_console.moc"

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
