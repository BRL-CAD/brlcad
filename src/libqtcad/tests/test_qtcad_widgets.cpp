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
#include <QLineEdit>
#include <QtTest/QTest>

#include "bu/app.h"
#include "qtcad/QgColorRGB.h"

class QgWidgetTest : public QObject
{
    Q_OBJECT

    private slots:
	void commitsTypedColorOnFocusLoss();
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
