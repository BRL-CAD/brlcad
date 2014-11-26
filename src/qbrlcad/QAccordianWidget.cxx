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
#include <QIcon>
#include <QImage>
#include <QPixmap>
#include "QAccordianWidget.h"

QAccordianObject::QAccordianObject(QWidget *pparent, QWidget *object, QString header_title) : QWidget(pparent)
{
    visible = 1;
    title = header_title;
    toggle = new QPushButton(title, this);
    toggle->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    QImage iconimage;
    iconimage.load(":/images/tree/branch-open.png");
    QPixmap iconpixmap;
    iconpixmap.convertFromImage(iconimage);
    QIcon buttonicon(iconpixmap);
    toggle->setIcon(buttonicon);
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
    if (!visible) {
	QImage iconimage;
	iconimage.load(":/images/tree/branch-closed.png");
	QPixmap iconpixmap;
	iconpixmap.convertFromImage(iconimage);
	QIcon buttonicon(iconpixmap);
	toggle->setIcon(buttonicon);
	child_object->hide();
	emit made_hidden(this);
    }
    if (visible) {
	QImage iconimage;
	iconimage.load(":/images/tree/branch-open.png");
	QPixmap iconpixmap;
	iconpixmap.convertFromImage(iconimage);
	QIcon buttonicon(iconpixmap);
	toggle->setIcon(buttonicon);
	child_object->show();
	emit made_visible(this);
    }
}

void
QAccordianObject::toggleVisibility()
{
    setVisibility(!visible);
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
    QObject::connect(splitter, SIGNAL(splitterMoved(int, int)), this, SLOT(update_sizes(int, int)));
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
    QObject::connect(object, SIGNAL(made_visible(QAccordianObject *)), this, SLOT(stateUpdate(QAccordianObject *)));
    QObject::connect(object, SIGNAL(made_hidden(QAccordianObject *)), this, SLOT(stateUpdate(QAccordianObject *)));
    size_states.clear();
}

void
QAccordianWidget::insertObject(int idx, QAccordianObject *object)
{
    splitter->insertWidget(idx, object);
    object->idx = splitter->count() - 1;
    objects.insert(object);
    QObject::connect(object, SIGNAL(made_visible(QAccordianObject *)), this, SLOT(stateUpdate(QAccordianObject *)));
    QObject::connect(object, SIGNAL(made_hidden(QAccordianObject *)), this, SLOT(stateUpdate(QAccordianObject *)));
    foreach(QAccordianObject *obj, objects) {
	if (obj->idx >= idx)
	    obj->idx++;
    }
    size_states.clear();
}

void
QAccordianWidget::deleteObject(QAccordianObject *object)
{
    objects.remove(object);
    size_states.clear();
    delete object;
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
	stateUpdate(open_object);
    }
}

void
QAccordianWidget::update_sizes(int, int) {
    QList<int> currentSizes = splitter->sizes();
    QString statekey;
    foreach(QAccordianObject *obj, objects) {
	if (obj->visible) {
	    statekey.append("1");
	} else {
	    statekey.append("0");
	}
    }
    size_states.insert(statekey, currentSizes);
}

void
QAccordianWidget::stateUpdate(QAccordianObject *new_obj)
{
    if (unique_visibility && new_obj) {
	open_object = new_obj;
	open_object->setVisibility(1);
	foreach(QAccordianObject *obj, objects) {
	    if (obj != open_object)
		obj->setVisibility(0);
	}
    }
    QString statekey;
    foreach(QAccordianObject *obj, objects) {
	if (obj->visible) {
	    statekey.append("1");
	} else {
	    statekey.append("0");
	}
    }

    if (size_states.find(statekey) != size_states.end()) {
	splitter->setSizes(size_states.find(statekey).value());
    } else {

	QVector<int> newsizes;
	newsizes.resize(splitter->count());
	QList<int> prevSizes = splitter->sizes();
	QList<int>::const_iterator stlIter;
	int sheight = splitter->height();
	int scount = splitter->count();
	int found_hidden = splitter->count() + 1;
	foreach(QAccordianObject *obj, objects) {
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
	foreach(QAccordianObject *obj, objects) {
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
	for( v_it = newsizes.begin(); v_it != newsizes.end(); ++v_it ) {
	    newsizesl.push_back(*v_it);
	}

	splitter->setSizes(newsizesl);
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

