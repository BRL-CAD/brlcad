/*              T E S T _ Q T C A D _ W I D G E T S . C P P
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
#include <QFocusEvent>
#include <QHeaderView>
#include <QLineEdit>
#include <QTemporaryDir>
#include <QtTest/QTest>

#include <memory>

#include "bu/app.h"
#include "qtcad/QgColorRGB.h"
#include "qtcad/QgModel.h"
#include "qtcad/QgTreeView.h"
#include "qtcad/QgUtil.h"
#include "wdb.h"

class QgWidgetTest : public QObject
{
    Q_OBJECT

    private slots:
	void commitsTypedColorOnFocusLoss();
	void stretchesHierarchyColumn();
	void usesGenericIconForBinaryObjects();
};

void
QgWidgetTest::commitsTypedColorOnFocusLoss()
{
    constexpr int expected_red = 12;
    constexpr int expected_green = 34;
    constexpr int expected_blue = 56;

    QgColorRGB color;

    int change_count = 0;
    QObject::connect(&color, &QgColorRGB::color_changed,
	[&change_count](struct bu_color *) { change_count++; });

    color.rgbtext->selectAll();
    QTest::keyClicks(color.rgbtext, QStringLiteral("12/34/56"));
    QCOMPARE(change_count, 0);

    QFocusEvent focus_out(QEvent::FocusOut, Qt::TabFocusReason);
    QApplication::sendEvent(color.rgbtext, &focus_out);
    QCOMPARE(change_count, 1);

    int red = 0;
    int green = 0;
    int blue = 0;
    QVERIFY(bu_color_to_rgb_ints(&color.bc, &red, &green, &blue));
    QCOMPARE(red, expected_red);
    QCOMPARE(green, expected_green);
    QCOMPARE(blue, expected_blue);
}

void
QgWidgetTest::stretchesHierarchyColumn()
{
    QgModel model;
    QgTreeView tree(nullptr, &model);

    QCOMPARE(tree.header()->sectionResizeMode(0), QHeaderView::Stretch);
    tree.tree_column_size(QModelIndex());
    QCOMPARE(tree.header()->sectionResizeMode(0), QHeaderView::Stretch);
}

void
QgWidgetTest::usesGenericIconForBinaryObjects()
{
    QTemporaryDir temporary_directory;
    QVERIFY(temporary_directory.isValid());
    QByteArray database_path = QFile::encodeName(
	temporary_directory.filePath(QStringLiteral("icons.g")));
    std::unique_ptr<struct rt_wdb, decltype(&wdb_close)> database(
	wdb_fopen(database_path.constData()), wdb_close);
    QVERIFY(database != nullptr);

    const unsigned char data[] = {0};
    QCOMPARE(mk_binunif(database.get(), "data.bin", data,
	WDB_BINUNIF_UINT8, 1), 0);
    struct directory *binary_object = db_lookup(database->dbip, "data.bin",
	LOOKUP_QUIET);
    QVERIFY(binary_object != RT_DIR_NULL);
    QCOMPARE(binary_object->d_major_type, DB5_MAJORTYPE_BINARY_UNIF);
    QCOMPARE(binary_object->d_minor_type, DB5_MINORTYPE_BRLCAD_ARB8);

    QImage expected(QStringLiteral(":/images/primitives/other.png"));
    QVERIFY(!expected.isNull());
    QVERIFY(QgIcon(binary_object, database->dbip) == expected);
    QCOMPARE(QgCombType(binary_object, database->dbip),
	QgCombTypeId::StandardObj);
}

int
main(int argc, char **argv)
{
    bu_setprogname(argv[0]);
    if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM"))
	qputenv("QT_QPA_PLATFORM", QByteArray("offscreen"));
    QApplication app(argc, argv);
    QgWidgetTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "test_qtcad_widgets.moc"

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
