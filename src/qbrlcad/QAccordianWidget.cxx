/*            Q A C C O R D I A N W I D G E T . C X X
 * BRL-CAD
 *
 * Copyright (c) 2014 United States Government as represented by
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
/** @file QAccordianWidget.cxx
 *
 * Qt Accordian Widget implementation
 *
 */

#include <iostream>
#include "QAccordianWidget.h"


QAccordianObject::QAccordianObject(QWidget *pparent, QWidget *object, QString header_title) : QWidget(pparent)
{
    visible = 1;
    title = header_title;
    toggle = new QPushButton(title, this);
    toggle->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    QScrollArea *objscrollarea= new QScrollArea();
    objlayout = new QVBoxLayout(this);
    objlayout->setSpacing(0);
    objlayout->setContentsMargins(0,0,0,0);
    objlayout->setAlignment(Qt::AlignTop);
    objlayout->addWidget(toggle);
    objscrollarea->setWidget(object);
    objlayout->addWidget(objscrollarea);
    this->setLayout(objlayout);

    child_object = objscrollarea;

    QObject::connect(toggle, SIGNAL(clicked()), this, SLOT(toggleVisibility()));
}

QAccordianObject::~QAccordianObject()
{
    delete toggle;
    if (child_object) delete child_object;
}

    void
QAccordianObject::setWidget(QWidget *object)
{
    if (child_object) {
	objlayout->removeWidget(child_object);
	delete child_object;
    }
    child_object = object;
    objlayout->addWidget(child_object);
}

QWidget *
QAccordianObject::getWidget()
{
    return child_object;
}


    void
QAccordianObject::setVisibility(int val)
{
    visible = val;
    if (!visible) child_object->hide();
    if (visible) child_object->show();
}

void
QAccordianObject::toggleVisibility()
{
    if (!visible) {
	visible = 1;
	child_object->show();
	emit made_visible(this);
	return;
    }

    if (visible) {
	visible = 0;
	child_object->hide();
	emit made_hidden(this);
	return;
    }

}




QAccordianWidget::QAccordianWidget(QWidget *pparent) : QWidget(pparent)
{
    unique_visibility = 0;

    QVBoxLayout *mlayout = new QVBoxLayout();
    mlayout->setSpacing(0);
    mlayout->setContentsMargins(1,1,1,1);

    splitter = new QSplitter();
    splitter->setOrientation(Qt::Vertical);
    splitter->setChildrenCollapsible(false);
    mlayout->addWidget(splitter);

    this->setLayout(mlayout);
}

QAccordianWidget::~QAccordianWidget()
{
    foreach(QAccordianObject *object, objects) {
	delete object;
    }
}

    void
QAccordianWidget::addObject(QAccordianObject *object)
{
    splitter->addWidget(object);
    object->idx = splitter->count() - 1;
    objects.insert(object);
    QObject::connect(object, SIGNAL(made_visible(QAccordianObject *)), this, SLOT(setOpenObject(QAccordianObject *)));
    QObject::connect(object, SIGNAL(made_hidden(QAccordianObject *)), this, SLOT(setOpenObject(QAccordianObject *)));
}

    void
QAccordianWidget::insertObject(int idx, QAccordianObject *object)
{
    splitter->insertWidget(idx, object);
    object->idx = splitter->count() - 1;
    objects.insert(object);
    QObject::connect(object, SIGNAL(made_visible(QAccordianObject *)), this, SLOT(setOpenObject(QAccordianObject *)));
    QObject::connect(object, SIGNAL(made_hidden(QAccordianObject *)), this, SLOT(setOpenObject(QAccordianObject *)));
    foreach(QAccordianObject *obj, objects) {
	if (obj->idx >= idx)
	    obj->idx++;
    }
}

    void
QAccordianWidget::deleteObject(QAccordianObject *object)
{
    objects.remove(object);
    delete object;
}

    void
QAccordianWidget::syncVisibility()
{
    if (unique_visibility) {
	foreach(QAccordianObject *obj, objects) {
	    if (obj != open_object)
		obj->setVisibility(0);
	}
    }
}

    void
QAccordianWidget::setUniqueVisibility(int val)
{
    unique_visibility = val;
    if (!objects.isEmpty()) {
	if (!unique_visibility && val) {
	    open_object = *(objects.begin());
	    open_object->setVisibility(1);
	}
	syncVisibility();
    }
}

void
QAccordianWidget::setOpenObject(QAccordianObject *new_obj)
{
    if (unique_visibility) {
	open_object = new_obj;
	open_object->setVisibility(1);
	syncVisibility();
    }
    foreach(QAccordianObject *obj, objects) {
	if (obj->visible || obj->idx == splitter->count() - 1) {
	    splitter->setStretchFactor(obj->idx, 100000);
	} else {
	    splitter->setStretchFactor(obj->idx, 0);
	}
    }
    QList<int> currentSizes = splitter->sizes();
    QList<int> newsizes;
    int spsize;
    foreach(spsize, currentSizes) {
	newsizes.push_back(1);
    }
    splitter->setSizes(newsizes);
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

