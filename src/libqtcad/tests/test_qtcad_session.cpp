/*                 T E S T _ Q T C A D _ S E S S I O N . C P P
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

#include <QAbstractItemModelTester>
#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest/QTest>

#include "ged.h"
#include "qtcad/QgModel.h"
#include "qtcad/QgSession.h"

#include "../../libged/dbi.h"

class QgSessionTest : public QObject
{
    Q_OBJECT

private slots:
    void ownsGedContext();
    void publishesChanges();
    void suppliesModelContext();
    void reconcilesModelEvents();
};

void
QgSessionTest::ownsGedContext()
{
    QgSession session;

    QVERIFY(session.ged() != nullptr);
    QVERIFY(session.dbip() == nullptr);
    QVERIFY(session.activeView() != nullptr);
    QCOMPARE(session.activeView(), session.ged()->ged_gvp);
    QVERIFY(session.ged()->dbi_state == nullptr);
    QCOMPARE(session.ged()->new_cmd_forms, 1);
}

void
QgSessionTest::publishesChanges()
{
    QgSession session;
    int databaseChanges = 0;
    int viewChanges = 0;

    connect(&session, &QgSession::db_changed, this,
	[&databaseChanges](struct db_i *) { databaseChanges++; });
    connect(&session, &QgSession::view_changed, this,
	[&viewChanges](unsigned long long) { viewChanges++; });

    session.notifyDbChanged(nullptr);
    session.notifyViewChanged(1);

    QCOMPARE(databaseChanges, 1);
    QCOMPARE(viewChanges, 1);
}

void
QgSessionTest::suppliesModelContext()
{
    QgModel model;

    QVERIFY(model.session() != nullptr);
    QCOMPARE(model.ged(), model.session()->ged());
    QCOMPARE(model.gedp, model.ged());
    QVERIFY(model.ged()->ged_gvp != nullptr);
}

void
QgSessionTest::reconcilesModelEvents()
{
    qRegisterMetaType<struct db_i *>("db_i*");
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    QByteArray databasePath = QFile::encodeName(
	temporaryDirectory.filePath("events.g"));
    constexpr int databaseVersion = 5;
    struct db_i *created = db_create(databasePath.constData(),
	databaseVersion);
    QVERIFY(created != DBI_NULL);
    db_close(created);

    QgModel model;
    QAbstractItemModelTester tester(&model,
	QAbstractItemModelTester::FailureReportingMode::QtTest);
    const char *openArguments[] = {"open", databasePath.constData()};
    QCOMPARE(model.run_cmd(model.ged()->ged_result_str, 2, openArguments),
	BRLCAD_OK);
    QVERIFY(model.ged()->dbi_state != nullptr);
    QCOMPARE(model.rowCount(), 0);

    QSignalSpy changes(model.session(), &QgSession::db_changed);
    QSignalSpy viewChanges(model.session(), &QgSession::view_changed);
    QSignalSpy compatibilityViewChanges(&model, &QgModel::view_change);
    const char *makeArguments[] = {"make", "event.s", "sph"};
    QCOMPARE(model.run_cmd(model.ged()->ged_result_str, 3, makeArguments),
	BRLCAD_OK);
    QCOMPARE(model.rowCount(), 1);
    QCOMPARE(changes.count(), 1);

    DbiState *state = static_cast<DbiState *>(model.ged()->dbi_state);
    QVERIFY(state->change_snapshots().empty());
    QVERIFY(db_lookup(model.ged()->dbip, "event.s", LOOKUP_QUIET) !=
	RT_DIR_NULL);

    QCOMPARE(model.draw("event.s"), BRLCAD_OK);
    QCOMPARE(viewChanges.count(), 1);
    QCOMPARE(compatibilityViewChanges.count(), 1);
    QCOMPARE(model.erase("event.s"), BRLCAD_OK);
    QCOMPARE(viewChanges.count(), 2);
    QCOMPARE(compatibilityViewChanges.count(), 2);

    const char *killArguments[] = {"kill", "event.s"};
    QCOMPARE(model.run_cmd(model.ged()->ged_result_str, 2, killArguments),
	BRLCAD_OK);
    QCOMPARE(model.rowCount(), 0);
    QCOMPARE(changes.count(), 2);
    QVERIFY(state->change_snapshots().empty());
    QVERIFY(db_lookup(model.ged()->dbip, "event.s", LOOKUP_QUIET) ==
	RT_DIR_NULL);
}

QTEST_APPLESS_MAIN(QgSessionTest)

#include "test_qtcad_session.moc"

// Local Variables:
// mode: C++
// tab-width: 8
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
