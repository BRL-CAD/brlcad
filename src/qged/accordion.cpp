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

#include <QPalette>
#include <QColor>

#include "accordion.h"
#include "app.h"

CADViewControls::CADViewControls(QWidget *pparent)
    : QWidget(pparent)
{
    QVBoxLayout *mlayout = new QVBoxLayout();
    mlayout->setSpacing(0);
    mlayout->setContentsMargins(0,0,0,0);
    tpalette = new QToolPalette(this);
    info_view = new QWidget(this);
    info_view->setMinimumHeight(100);
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
    this->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    this->setLayout(mlayout);
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
    info_view = new QWidget(this);
    info_view->setMinimumHeight(100);
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
    this->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    this->setLayout(mlayout);
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
    shape_properties = new QWidget(this);
    shape_properties->setMinimumHeight(100);
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
    this->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    this->setLayout(mlayout);
}

CADPrimitiveEdit::~CADPrimitiveEdit()
{
    delete tpalette;
    delete shape_properties;
}

bool EditStateFilter::eventFilter(QObject *target, QEvent *e)
{
    CADAccordion *accordion = ((CADApp *)qApp)->cadaccordion;
    if (e->type() == QEvent::MouseButtonPress) {
	QMouseEvent *me = (QMouseEvent *)e;
	QWidget *target_widget = static_cast<QWidget *>(target);
	QPoint mpos = target_widget->mapTo(accordion, me->pos());

	for (int i = 0; i < accordion->active_objects.size(); i++) {
	    int margin = accordion->splitter->handleWidth()/2;
	    QPushButton *obj_toggle = accordion->active_objects.at(i)->toggle;
	    QPoint obj_topleft = obj_toggle->mapTo(accordion, obj_toggle->geometry().topLeft());
	    obj_topleft.setX(obj_topleft.x() - margin);
	    obj_topleft.setY(obj_topleft.y() - margin);
	    QPoint obj_bottomright = obj_toggle->mapTo(accordion, obj_toggle->geometry().bottomRight());
	    obj_bottomright.setX(obj_bottomright.x() + margin);
	    obj_bottomright.setY(obj_bottomright.y() + margin);
	    QRect obj_rect(obj_topleft, obj_bottomright);
	    if (obj_rect.contains(mpos)) {
		return QObject::eventFilter(target, e);
	    }
	}

	QPoint view_ctrls_topleft = accordion->view_ctrls->mapTo(accordion, accordion->view_ctrls->geometry().topLeft());
	QPoint view_ctrls_bottomright = accordion->view_ctrls->mapTo(accordion, accordion->view_ctrls->geometry().bottomRight());
	QRect view_ctrls_rect(view_ctrls_topleft, view_ctrls_bottomright);

	QPoint instance_ctrls_topleft = accordion->instance_ctrls->mapTo(accordion, accordion->instance_ctrls->geometry().topLeft());
	QPoint instance_ctrls_bottomright = accordion->instance_ctrls->mapTo(accordion, accordion->instance_ctrls->geometry().bottomRight());
	QRect instance_ctrls_rect(instance_ctrls_topleft, instance_ctrls_bottomright);

	QPoint primitive_ctrls_topleft = accordion->primitive_ctrls->mapTo(accordion, accordion->primitive_ctrls->geometry().topLeft());
	QPoint primitive_ctrls_bottomright = accordion->primitive_ctrls->mapTo(accordion, accordion->primitive_ctrls->geometry().bottomRight());
	QRect primitive_ctrls_rect(primitive_ctrls_topleft, primitive_ctrls_bottomright);

	QPoint stdpropview_topleft = accordion->stdpropview->mapTo(accordion, accordion->stdpropview->geometry().topLeft());
	QPoint stdpropview_bottomright = accordion->stdpropview->mapTo(accordion, accordion->stdpropview->geometry().bottomRight());
	QRect stdpropview_rect(stdpropview_topleft, stdpropview_bottomright);

	QPoint userpropview_topleft = accordion->userpropview->mapTo(accordion, accordion->userpropview->geometry().topLeft());
	QPoint userpropview_bottomright = accordion->userpropview->mapTo(accordion, accordion->userpropview->geometry().bottomRight());
	QRect userpropview_rect(userpropview_topleft, userpropview_bottomright);

	CADTreeView *tview = (CADTreeView *)(((CADApp *)qApp)->cadtreeview);
	if (!tview)
	    return QObject::eventFilter(target, e);
	CADTreeModel *tmodel = (CADTreeModel *)(tview->model());
	int interaction_mode = tview->m->interaction_mode;
	int *imode = &tview->m->interaction_mode;

	if (view_ctrls_rect.contains(mpos)) {
	    (*imode) = 0;
	    accordion->highlight_selected(accordion->view_obj);
	}
	if (instance_ctrls_rect.contains(mpos)) {
	    (*imode) = 1;
	    accordion->highlight_selected(accordion->instance_obj);
	}

	if (primitive_ctrls_rect.contains(mpos)) {
	    (*imode) = 2;
	    accordion->highlight_selected(accordion->primitive_obj);
	}

	if (stdpropview_rect.contains(mpos)) {
	    (*imode) = 2;
	    accordion->highlight_selected(accordion->stdprop_obj);
	}

	if (userpropview_rect.contains(mpos)) {
	    (*imode) = 2;
	    accordion->highlight_selected(accordion->userprop_obj);
	}
	if (interaction_mode != (*imode)) {
	    tmodel->update_selected_node_relationships(tview->selected());
	}
    }

    return QObject::eventFilter(target, e);
}


CADAccordion::CADAccordion(QWidget *pparent)
    : QAccordionWidget(pparent)
{
    view_ctrls = new CADViewControls(this);
    instance_ctrls = new CADInstanceEdit(this);
    primitive_ctrls = new CADPrimitiveEdit(this);

    stdpropmodel = new CADAttributesModel(0, DBI_NULL, RT_DIR_NULL, 1, 0);
    stdpropview = new CADAttributesView(this, 1);
    stdpropview->setModel(stdpropmodel);

    userpropmodel = new CADAttributesModel(0, DBI_NULL, RT_DIR_NULL, 0, 1);
    userpropview = new CADAttributesView(this, 0);
    userpropview->setModel(userpropmodel);

    view_obj = new QAccordionObject(this, view_ctrls, "View Controls");
    this->addObject(view_obj);
    active_objects.push_back(view_obj);
    instance_obj = new QAccordionObject(this, instance_ctrls, "Instance Editing");
    this->addObject(instance_obj);
    active_objects.push_back(instance_obj);
    primitive_obj = new QAccordionObject(this, primitive_ctrls, "Object Editing");
    this->addObject(primitive_obj);
    active_objects.push_back(primitive_obj);
    stdprop_obj = new QAccordionObject(this, stdpropview, "Standard Attributes");
    this->addObject(stdprop_obj);
    active_objects.push_back(stdprop_obj);
    userprop_obj = new QAccordionObject(this, userpropview, "User Attributes");
    this->addObject(userprop_obj);
    active_objects.push_back(userprop_obj);

    QList<QWidget*> list = this->findChildren<QWidget *>();
    EditStateFilter *efilter = new EditStateFilter();
    foreach(QWidget *w, list) {
	w->installEventFilter(efilter);
    }

    highlight_selected(view_obj);
}

void
CADAccordion::highlight_selected(QAccordionObject *selected)
{
    QString highlight_style("background-color: rgb(10,10,200);");
    if (view_obj == selected) {
	view_obj->toggle->setStyleSheet(highlight_style);
    } else {
	view_obj->toggle->setStyleSheet("");
    }
    if (instance_obj == selected) {
	instance_obj->toggle->setStyleSheet(highlight_style);
    } else {
	instance_obj->toggle->setStyleSheet("");
    }
    if (primitive_obj == selected) {
	primitive_obj->toggle->setStyleSheet(highlight_style);
    } else {
	primitive_obj->toggle->setStyleSheet("");
    }
    if (stdprop_obj == selected) {
	stdprop_obj->toggle->setStyleSheet(highlight_style);
    } else {
	stdprop_obj->toggle->setStyleSheet("");
    }
    if (userprop_obj == selected) {
	userprop_obj->toggle->setStyleSheet(highlight_style);
    } else {
	userprop_obj->toggle->setStyleSheet("");
    }

    view_obj->toggle->update();
    instance_obj->toggle->update();
    primitive_obj->toggle->update();
    stdprop_obj->toggle->update();
    userprop_obj->toggle->update();
}

CADAccordion::~CADAccordion()
{
    delete view_ctrls;
    delete instance_ctrls;
    delete primitive_ctrls;
    delete stdpropmodel;
    delete stdpropview;
    delete userpropmodel;
    delete userpropview;
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

