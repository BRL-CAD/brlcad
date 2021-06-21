/*                   Q C O L O R R G B . C P P
 * BRL-CAD
 *
 * Copyright (c) 2014-2021 United States Government as represented by
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
/** @file QColorRGB.cpp
 *
 * Color selection widget
 *
 */

#include "common.h"

#include <QLabel>
#include "qtcad/QColorRGB.h"

QColorRGB::QColorRGB(QWidget *p, QString lstr, QColor dcolor) : QWidget(p)
{
    mlayout = new QHBoxLayout();
    mlayout->setSpacing(0);
    mlayout->setContentsMargins(1,1,1,1);

    QString lstrm = QString("<font face=\"monospace\">%1</font>").arg(lstr);
    QLabel *clbl = new QLabel(lstrm);


    rgbcolor = new QPushButton("");
    rgbcolor->setMinimumWidth(30);
    rgbcolor->setMaximumWidth(30);
    rgbcolor->setMinimumHeight(30);
    rgbcolor->setMaximumHeight(30);


    qc = dcolor;

    QFont f("");
    f.setStyleHint(QFont::Monospace);
    QString rgbstr = QString("%1/%2/%3").arg(qc.red()).arg(qc.green()).arg(qc.blue());
    rgbtext = new QLineEdit(rgbstr);
    rgbtext->setFont(f);
    set_color_from_text();

    mlayout->addWidget(clbl);
    mlayout->addWidget(rgbtext);
    mlayout->addWidget(rgbcolor);

    this->setLayout(mlayout);

    QObject::connect(rgbcolor, &QPushButton::clicked, this, &QColorRGB::set_color_from_button);
    QObject::connect(rgbtext, &QLineEdit::returnPressed, this, &QColorRGB::set_color_from_text);
}

QColorRGB::~QColorRGB()
{
}

void
QColorRGB::set_color_from_button()
{
    QColor nc = QColorDialog::getColor(qc);
    if (nc.isValid() && nc != qc) {
	qc = nc;
	QString rgbstr = QString("%1/%2/%3").arg(qc.red()).arg(qc.green()).arg(qc.blue());
	rgbtext->setText(rgbstr);
	QString qss = QString("background-color: rgb(%1, %2, %3);").arg(qc.red()).arg(qc.green()).arg(qc.blue());
	rgbcolor->setStyleSheet(qss);

	// Sync bu_color
	QString colstr = rgbtext->text();
	const char *ccstr = colstr.toLocal8Bit();
	bu_opt_color(NULL, 1, &ccstr, (void *)&bc);

	emit color_changed();
    }
}

void
QColorRGB::set_color_from_text()
{
    QString colstr = rgbtext->text();
    const char *ccstr = colstr.toLocal8Bit();

    // TODO - split into argv array, in case of spaces

    int acnt = bu_opt_color(NULL, 1, &ccstr, (void *)&bc);
    if (acnt != 1)
	return;

    int rgb[3];
    if (!bu_color_to_rgb_ints(&bc, &rgb[0], &rgb[1], &rgb[2]))
	return;

    QColor nc(rgb[0], rgb[1], rgb[2]);

    if (nc.isValid()) {
	qc = nc;
	QString qss = QString("background-color: rgb(%1, %2, %3);").arg(qc.red()).arg(qc.green()).arg(qc.blue());
	rgbcolor->setStyleSheet(qss);

	emit color_changed();
    }
}


/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */

