/*                   C A D P A L E T T E . C P P
 * BRL-CAD
 *
 * Copyright (c) 2020-2021 United States Government as represented by
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
/** @file cadpalette.cpp
 *
 * Brief description
 *
 */

#include <QColor>
#include "palettes.h"

CADPalette::CADPalette(int mode, QWidget *pparent)
    : QWidget(pparent)
{
    m_mode = mode;
    QVBoxLayout *mlayout = new QVBoxLayout();
    mlayout->setSpacing(0);
    mlayout->setContentsMargins(0,0,0,0);
    tpalette = new QToolPalette(this);
    tpalette->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
    for(int i = 1; i < 8; i++) {
	QIcon *obj_icon = new QIcon();
	QString obj_label("tool controls ");
	obj_label.append(QString::number(i));
	QPushButton *obj_control = new QPushButton(obj_label);
	QToolPaletteElement *el = new QToolPaletteElement(obj_icon, obj_control);
	tpalette->addElement(el);
    }
    mlayout->addWidget(tpalette);

    tpalette->selected_style = QString("border: 1px solid rgb(255, 255, 0)");
    // Until the panel buttons are actually clicked on, we don't want the
    // border to be yellow for the selected button - we are using that border
    // color as a visual indicator that this panel is the active panel.
    tpalette->selected->button->setStyleSheet("");

    this->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
    this->setLayout(mlayout);
}

void
CADPalette::makeCurrent(QWidget *w)
{
    if (w == this) {
	this->tpalette->displayElement(this->tpalette->selected);
	emit interaction_mode(m_mode);
    } else {
	this->tpalette->selected->button->setStyleSheet("");
    }
}

void
CADPalette::reflow()
{
    tpalette->button_layout_resize();
}

CADPalette::~CADPalette()
{
    delete tpalette;
}

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

