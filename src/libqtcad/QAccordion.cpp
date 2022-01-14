/*                Q A C C O R D I O N . C P P
 * BRL-CAD
 *
 * Copyright (c) 2014-2022 United States Government as represented by
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
/** @file QAccordion.cpp
 *
 * Simple accordion widget
 *
 */

#include "common.h"

#include "qtcad/QAccordion.h"

QAccordionObject::QAccordionObject(QWidget *pparent, QWidget *object, QString header_title)
    : QWidget(pparent)
{
    title = header_title;
    toggle = new QPushButton(title, this);
    toggle->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
    objscrollarea= new QScrollArea();
    objscrollarea->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Expanding);
    objlayout = new QVBoxLayout(this);
    objlayout->setSpacing(0);
    objlayout->setContentsMargins(0,0,0,0);
    objlayout->setAlignment(Qt::AlignTop);
    objlayout->addWidget(toggle);
    objscrollarea->setWidget(object);
    objscrollarea->setWidgetResizable(true);
    objlayout->addWidget(objscrollarea);
    this->setLayout(objlayout);

    QObject::connect(toggle, &QPushButton::clicked, this, &QAccordionObject::toggleVisibility);
}

QAccordionObject::~QAccordionObject()
{
}

void
QAccordionObject::toggleVisibility()
{
    emit select(this);
}


QAccordion::QAccordion(QWidget *pparent) : QWidget(pparent)
{
    mlayout = new QVBoxLayout();
    mlayout->setSpacing(0);
    mlayout->setContentsMargins(1,1,1,1);
    this->setLayout(mlayout);
}

QAccordion::~QAccordion()
{
}

void
QAccordion::addObject(QAccordionObject *o)
{
    if (!selected) {
	selected = o;
    }
    objs.insert(o);
    foreach(QAccordionObject *obj, objs) {
	if (obj == selected) {
	    obj->objscrollarea->show();
	} else {
	    obj->objscrollarea->hide();
	}
    }
    mlayout->addWidget(o);
    QObject::connect(o, &QAccordionObject::select, this, &QAccordion::open);
}

void
QAccordion::open(QAccordionObject *o)
{
    if (selected == o)
	return;

    selected = o;

    foreach(QAccordionObject *obj, objs) {
	if (obj == selected) {
	    obj->objscrollarea->show();
	} else {
	    obj->objscrollarea->hide();
	}
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

