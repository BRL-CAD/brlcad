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

#include "qtcad/QColorRGB.h"

QColorRGB::QColorRGB(QWidget *pparent) : QWidget(pparent)
{
    mlayout = new QHBoxLayout();
    mlayout->setSpacing(0);
    mlayout->setContentsMargins(1,1,1,1);

    rgbcolor = new QPushButton("");
    rgbcolor->setMinimumWidth(30);
    rgbcolor->setMaximumWidth(30);
    rgbcolor->setMinimumHeight(30);
    rgbcolor->setMaximumHeight(30);
    mlayout->addWidget(rgbcolor);

    rgbtext = new QLineEdit("");
    mlayout->addWidget(rgbtext);

    this->setLayout(mlayout);
}

QColorRGB::~QColorRGB()
{
}

void
QColorRGB::set_color_text()
{
}

void
QColorRGB::set_button_color()
{
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

