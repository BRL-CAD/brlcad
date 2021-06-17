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
    set_button_color();

    QFont f("");
    f.setStyleHint(QFont::Monospace);
    rgbtext = new QLineEdit("");
    rgbtext->setFont(f);
    set_color_text();

    mlayout->addWidget(clbl);
    mlayout->addWidget(rgbtext);
    mlayout->addWidget(rgbcolor);

    this->setLayout(mlayout);
}

QColorRGB::~QColorRGB()
{
}

void
QColorRGB::set_color_text()
{
    QString rgbstr = QString("%1/%2/%3").arg(qc.red()).arg(qc.green()).arg(qc.blue());
    rgbtext->setText(rgbstr);
}

void
QColorRGB::set_button_color()
{
    QString qss = QString("background-color: rgb(%1, %2, %3);").arg(qc.red()).arg(qc.green()).arg(qc.blue());
    rgbcolor->setStyleSheet(qss);
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

