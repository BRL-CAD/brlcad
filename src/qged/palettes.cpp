/*                C A D A C C O R D I O N . C P P
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
/** @file cadaccordion.cpp
 *
 * Brief description
 *
 */

#include <QColor>
#include "palettes.h"

CADViewControls::CADViewControls(QWidget *pparent)
    : QWidget(pparent)
{
    QVBoxLayout *mlayout = new QVBoxLayout();
    mlayout->setSpacing(0);
    mlayout->setContentsMargins(0,0,0,0);
    tpalette = new QToolPalette(this);
    tpalette->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
    info_view = new QWidget(this);
    info_view->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
    for(int i = 1; i < 8; i++) {
	QIcon *obj_icon = new QIcon();
	QString obj_label("tool controls ");
	obj_label.append(QString::number(i));
	QPushButton *obj_control = new QPushButton(obj_label);
	QToolPaletteElement *el = new QToolPaletteElement(0, obj_icon, obj_control);
	tpalette->addElement(el);
    }
    mlayout->addWidget(tpalette);
    mlayout->addWidget(info_view);

    tpalette->selected_style = QString("border: 1px solid rgb(255, 255, 0)");
    // Until the panel buttons are actually clicked on, we don't want the
    // border to be yellow for the selected button - we are using that border
    // color as a visual indicator that this panel is the active panel.
    tpalette->selected->button->setStyleSheet("");

    this->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
    this->setLayout(mlayout);
}

void
CADViewControls::makeCurrent(QWidget *w)
{
    if (w == this) {
	this->tpalette->displayElement(this->tpalette->selected);
	emit interaction_mode(0);
    } else {
	this->tpalette->selected->button->setStyleSheet("");
    }
}

void
CADViewControls::reflow()
{
    tpalette->button_layout_resize();
}

CADViewControls::~CADViewControls()
{
    delete tpalette;
    delete info_view;
}

CADInstanceEdit::CADInstanceEdit(QWidget *pparent)
    : QWidget(pparent)
{
    QVBoxLayout *mlayout = new QVBoxLayout();
    mlayout->setSpacing(0);
    mlayout->setContentsMargins(0,0,0,0);
    tpalette = new QToolPalette(this);
    tpalette->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
    info_view = new QWidget(this);
    info_view->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
    for(int i = 1; i < 4; i++) {
	QIcon *obj_icon = new QIcon();
	QString obj_label("tool controls ");
	obj_label.append(QString::number(i));
	QPushButton *obj_control = new QPushButton(obj_label);
	QToolPaletteElement *el = new QToolPaletteElement(0, obj_icon, obj_control);
	tpalette->addElement(el);
    }
    mlayout->addWidget(tpalette);
    mlayout->addWidget(info_view);

    tpalette->selected_style = QString("border: 1px solid rgb(255, 255, 0)");
    // Until the panel buttons are actually clicked on, we don't want the
    // border to be yellow for the selected button - we are using that border
    // color as a visual indicator that this panel is the active panel.
    tpalette->selected->button->setStyleSheet("");


    this->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
    this->setLayout(mlayout);
}

void
CADInstanceEdit::makeCurrent(QWidget *w)
{
    if (w == this) {
	this->tpalette->displayElement(this->tpalette->selected);
	emit interaction_mode(1);
    } else {
	this->tpalette->selected->button->setStyleSheet("");
    }
}

void
CADInstanceEdit::reflow()
{
    tpalette->button_layout_resize();
}

CADInstanceEdit::~CADInstanceEdit()
{
    delete tpalette;
    delete info_view;
}


CADPrimitiveEdit::CADPrimitiveEdit(QWidget *pparent)
    : QWidget(pparent)
{
    QVBoxLayout *mlayout = new QVBoxLayout();
    mlayout->setSpacing(0);
    mlayout->setContentsMargins(0,0,0,0);
    tpalette = new QToolPalette(this);
    tpalette->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
    shape_properties = new QWidget(this);
    shape_properties->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
    for(int i = 1; i < 15; i++) {
	QIcon *obj_icon = new QIcon();
	QString obj_label("tool controls ");
	obj_label.append(QString::number(i));
	QPushButton *obj_control = new QPushButton(obj_label);
	QToolPaletteElement *el = new QToolPaletteElement(0, obj_icon, obj_control);
	tpalette->addElement(el);
    }
    mlayout->addWidget(tpalette);
    mlayout->addWidget(shape_properties);

    tpalette->selected_style = QString("border: 1px solid rgb(255, 255, 0)");
    // Until the panel buttons are actually clicked on, we don't want the
    // border to be yellow for the selected button - we are using that border
    // color as a visual indicator that this panel is the active panel.
    tpalette->selected->button->setStyleSheet("");

    this->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
    this->setLayout(mlayout);
}

void
CADPrimitiveEdit::makeCurrent(QWidget *w)
{
    if (w == this) {
	this->tpalette->displayElement(this->tpalette->selected);
	emit interaction_mode(2);
    } else {
	this->tpalette->selected->button->setStyleSheet("");
    }
}

void
CADPrimitiveEdit::reflow()
{
    tpalette->button_layout_resize();
}

CADPrimitiveEdit::~CADPrimitiveEdit()
{
    delete tpalette;
    delete shape_properties;
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

