/*            Q A C C O R D I O N W I D G E T . C X X
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
/** @file QAccordionWidget.cxx
 *
 * Qt Accordion Widget implementation
 *
 */

#include <iostream>

#include "qtcad/QAccordionWidget.h"

QAccordionObject::QAccordionObject(QWidget *pparent, QWidget *object, QString header_title) : QWidget(pparent)
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
    objscrollarea->setWidgetResizable(true);
    objlayout->addWidget(objscrollarea);
    this->setLayout(objlayout);

    child_object = objscrollarea;
    child_object->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    object->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    this->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    QObject::connect(toggle, &QPushButton::clicked, this, &QAccordionObject::toggleVisibility);
}

QAccordionObject::~QAccordionObject()
{
    delete toggle;
    if (child_object) delete child_object;
}

    void
QAccordionObject::setWidget(QWidget *object)
{
    if (child_object) {
	objlayout->removeWidget(child_object);
	delete child_object;
    }
    child_object = object;
    objlayout->addWidget(child_object);
}

QWidget *
QAccordionObject::getWidget()
{
    return child_object;
}


    void
QAccordionObject::setVisibility(int val)
{
    visible = val;
    if (!visible) {
	child_object->hide();
	emit made_hidden(this);
    }
    if (visible) {
	child_object->show();
	emit made_visible(this);
    }
}

void
QAccordionObject::toggleVisibility()
{
    setVisibility(!visible);
}


QAccordionWidget::QAccordionWidget(QWidget *pparent) : QWidget(pparent)
{
    unique_visibility = 0;

    QVBoxLayout *mlayout = new QVBoxLayout();
    mlayout->setSpacing(0);
    mlayout->setContentsMargins(1,1,1,1);

    splitter = new QSplitter();
    splitter->setOrientation(Qt::Vertical);
    splitter->setChildrenCollapsible(false);
    splitter->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    QObject::connect(splitter, &QSplitter::splitterMoved, this, &QAccordionWidget::update_sizes);
    mlayout->addWidget(splitter);

    buffer = new QSpacerItem(1, 0, QSizePolicy::Expanding, QSizePolicy::Minimum);
    mlayout->addItem(buffer);

    this->setLayout(mlayout);
}

QAccordionWidget::~QAccordionWidget()
{
    foreach(QAccordionObject *object, objects) {
	delete object;
    }
}

void
QAccordionWidget::addObject(QAccordionObject *object)
{
    splitter->addWidget(object);
    object->idx = splitter->count() - 1;
    objects.insert(object);
    QObject::connect(object, &QAccordionObject::made_visible, this, &QAccordionWidget::stateUpdate);
    QObject::connect(object, &QAccordionObject::made_hidden,  this, &QAccordionWidget::stateUpdate);
    size_states.clear();
}

void
QAccordionWidget::insertObject(int idx, QAccordionObject *object)
{
    splitter->insertWidget(idx, object);
    object->idx = splitter->count() - 1;
    objects.insert(object);
    QObject::connect(object, &QAccordionObject::made_visible, this, &QAccordionWidget::stateUpdate);
    QObject::connect(object, &QAccordionObject::made_hidden,  this, &QAccordionWidget::stateUpdate);
    foreach(QAccordionObject *obj, objects) {
	if (obj->idx >= idx)
	    obj->idx++;
    }
    size_states.clear();
}

void
QAccordionWidget::deleteObject(QAccordionObject *object)
{
    objects.remove(object);
    size_states.clear();
    delete object;
}

void
QAccordionWidget::setUniqueVisibility(int val)
{
    unique_visibility = val;
    if (!objects.isEmpty()) {
	if (!unique_visibility && val) {
	    open_object = *(objects.begin());
	    open_object->setVisibility(1);
	}
	stateUpdate(open_object);
    }
}

void
QAccordionWidget::update_sizes(int, int) {
    QList<int> currentSizes = splitter->sizes();
    QString statekey;
    foreach(QAccordionObject *obj, objects) {
	if (obj->visible) {
	    statekey.append("1");
	} else {
	    statekey.append("0");
	}
    }
    size_states.insert(statekey, currentSizes);
}

void
QAccordionWidget::stateUpdate(QAccordionObject *new_obj)
{
    if (unique_visibility && new_obj) {
	open_object = new_obj;
	open_object->setVisibility(1);
	foreach(QAccordionObject *obj, objects) {
	    if (obj != open_object)
		obj->setVisibility(0);
	}
    }
    QString statekey;
    foreach(QAccordionObject *obj, objects) {
	if (obj->visible) {
	    statekey.append("1");
	} else {
	    statekey.append("0");
	}
    }

    // If we have been in this open/close combination before, use the previous
    // state (which may have been manually adjusted by the user
    if (size_states.find(statekey) != size_states.end()) {
	splitter->setSizes(size_states.find(statekey).value());
	return;
    }


    int scount = splitter->count();
    int sheight = splitter->height();
    if (!scount || !sheight)
	return;

    // New state - we need to figure out where things should be
    QVector<int> newsizes;
    newsizes.resize(splitter->count());
    QList<int> prevSizes = splitter->sizes();
    QList<int>::const_iterator stlIter;

    int found_hidden = splitter->count() + 1;
    foreach(QAccordionObject *obj, objects) {
	if (!obj->visible) {
	    if (obj->idx < found_hidden) found_hidden = obj->idx;
	    sheight = sheight - obj->toggle->height();
	    scount--;
	} else {
	    if (!found_hidden && obj->idx < new_obj->idx) {
		sheight = sheight - prevSizes.at(obj->idx);
		scount--;
	    }
	}
    }

    if (!scount)
	return;

    foreach(QAccordionObject *obj, objects) {
	if (obj->visible) {
	    if (obj->idx < found_hidden && obj->idx < new_obj->idx) {
		splitter->setStretchFactor(obj->idx, 0);
		newsizes[obj->idx] = prevSizes.at(obj->idx);
	    } else {
		splitter->setStretchFactor(obj->idx, 100000);
		newsizes[obj->idx] = sheight/scount - 1;
	    }
	} else {
	    splitter->setStretchFactor(obj->idx, 0);
	    newsizes[obj->idx] = obj->toggle->height();
	}
    }
    QList<int> newsizesl;
    QVector<int>::const_iterator v_it;
    int oheight = 0;
    for( v_it = newsizes.begin(); v_it != newsizes.end(); ++v_it ) {
	newsizesl.push_back(*v_it);
	oheight += *v_it;
    }

    // Use the spacer to push closed panels towards the top of the accordion
    if (oheight < splitter->height()) {
	buffer->changeSize(1, splitter->height() - oheight, QSizePolicy::Expanding, QSizePolicy::Expanding);
    } else {
	buffer->changeSize(1, 0, QSizePolicy::Expanding, QSizePolicy::Minimum);
    }

    splitter->setSizes(newsizesl);
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

