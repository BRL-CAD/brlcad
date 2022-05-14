/*                      Q V I E W C T R L . C X X
 * BRL-CAD
 *
 * Copyright (c) 2022 United States Government as represented by
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
/** @file QViewCtrl.cpp
 *
 * Qt BRL-CAD View Control button panel implementation
 *
 */

#include "common.h"

#include "qtcad/QViewCtrl.h"


QViewCtrl::QViewCtrl(QWidget *pparent) : QWidget(pparent)
{
    bg = new QButtonGroup(this);
    bg->setExclusive(true);
    bl = new QFlowLayout();
    bl->setHorizontalSpacing(0);
    bl->setVerticalSpacing(0);
    bl->setContentsMargins(0,0,0,0);
    this->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
    this->setMinimumHeight(icon_height);
    this->setMinimumWidth(icon_width*5+1);
    this->setLayout(bl);

    sca = new QPushButton(this);
    sca->setIcon(QIcon(QPixmap(":images/view/view_scale.png")));
    sca->setMinimumWidth(icon_size);
    sca->setMaximumWidth(icon_size);
    sca->setMinimumHeight(icon_size);
    sca->setMaximumHeight(icon_size);
    bl->addWidget(sca);
    bg->addButton(sca);
    QObject::connect(sca, &QPushButton::clicked, this, &QViewCtrl::do_sca);

    rot = new QPushButton(this);
    rot->setIcon(QIcon(QPixmap(":images/view/view_rotate.png")));
    rot->setMinimumWidth(icon_size);
    rot->setMaximumWidth(icon_size);
    rot->setMinimumHeight(icon_size);
    rot->setMaximumHeight(icon_size);
    bl->addWidget(rot);
    bg->addButton(rot);
    QObject::connect(rot, &QPushButton::clicked, this, &QViewCtrl::do_rot);

    tra = new QPushButton(this);
    tra->setIcon(QIcon(QPixmap(":images/view/view_translate.png")));
    tra->setMinimumWidth(icon_size);
    tra->setMaximumWidth(icon_size);
    tra->setMinimumHeight(icon_size);
    tra->setMaximumHeight(icon_size);
    bl->addWidget(tra);
    bg->addButton(tra);
    QObject::connect(tra, &QPushButton::clicked, this, &QViewCtrl::do_tra);

    center = new QPushButton(this);
    center->setIcon(QIcon(QPixmap(":images/view/view_center.png")));
    center->setMinimumWidth(icon_size);
    center->setMaximumWidth(icon_size);
    center->setMinimumHeight(icon_size);
    center->setMaximumHeight(icon_size);
    bl->addWidget(center);
    bg->addButton(center);
    QObject::connect(center, &QPushButton::clicked, this, &QViewCtrl::do_center);
}

QViewCtrl::~QViewCtrl()
{
}

void QViewCtrl::do_sca() { emit mode_change(VIEWCTL_SCA); }
void QViewCtrl::do_rot() { emit mode_change(VIEWCTL_ROT); }
void QViewCtrl::do_tra() { emit mode_change(VIEWCTL_TRA); }
void QViewCtrl::do_center() { emit mode_change(VIEWCTL_CENTER); }

void
QViewCtrl::resizeEvent(QResizeEvent *pevent)
{
    QWidget::resizeEvent(pevent);
    div_t layout_dim = div(this->size().width()-1, icon_size);
    div_t layout_grid = div(bg->buttons().count(), (int)layout_dim.quot);
    if (layout_grid.rem > 0) {
	this->setMinimumHeight((layout_grid.quot + 1) * icon_size);
	this->setMaximumHeight((layout_grid.quot + 1) * icon_size);
    } else {
	this->setMinimumHeight((layout_grid.quot) * icon_size);
	this->setMaximumHeight((layout_grid.quot) * icon_size);
    }
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8


