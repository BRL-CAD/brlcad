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

#include <QtTest/QTest>

#include "ged.h"
#include "qtcad/QgModel.h"
#include "qtcad/QgSession.h"

class QgSessionTest : public QObject
{
    Q_OBJECT

private slots:
    void ownsGedContext();
    void publishesChanges();
    void suppliesModelContext();
};

void
QgSessionTest::ownsGedContext()
{
    QgSession session;

    QVERIFY(session.ged() != nullptr);
    QVERIFY(session.dbip() == nullptr);
    QVERIFY(session.activeView() != nullptr);
    QCOMPARE(session.activeView(), session.ged()->ged_gvp);
    QVERIFY(session.ged()->dbi_state != nullptr);
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
    QCOMPARE(model.gedp, model.session()->ged());
    QVERIFY(model.gedp->ged_gvp != nullptr);
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
