/*                         Q E L L . C P P
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
/** @file QEll.cpp
 *
 */

#include "common.h"
#include <QLabel>
#include <QLineEdit>
#include <QButtonGroup>
#include <QGroupBox>
#include "../../../app.h"
#include "QEll.h"

QEll::QEll()
    : QWidget()
{
    QVBoxLayout *l = new QVBoxLayout;

    QLabel *ell_name_label = new QLabel("Object name:");
    l->addWidget(ell_name_label);
    ell_name = new QLineEdit();
    l->addWidget(ell_name);

    QGroupBox *abox = new QGroupBox("Elements");
    QVBoxLayout *abl = new QVBoxLayout;
    abl->setAlignment(Qt::AlignTop);
    O_pnt = new QCheckBox("O:");
    abl->addWidget(O_pnt);
    A_axis = new QCheckBox("A:");
    abl->addWidget(A_axis);
    B_axis = new QCheckBox("B:");
    abl->addWidget(B_axis);
    C_axis = new QCheckBox("C:");
    abl->addWidget(C_axis);
    abox->setLayout(abl);
    l->addWidget(abox);

    QGroupBox *ac_box = new QGroupBox("Actions");
    QVBoxLayout *acl = new QVBoxLayout;
    write_edit = new QPushButton("Apply");
    acl->addWidget(write_edit);
    make_sph = new QPushButton("Make sph");
    acl->addWidget(make_sph);
    reset_values = new QPushButton("Reset");
    acl->addWidget(reset_values);
    ac_box->setLayout(acl);
    l->addWidget(ac_box);


    l->setAlignment(Qt::AlignTop);
    this->setLayout(l);
    setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
}

QEll::~QEll()
{
}

void
QEll::read_from_db()
{
    if (!dbip)
	return;
}

void
QEll::write_to_db()
{
    if (!dbip)
	return;
}

void
QEll::update_obj_wireframe()
{
    if (!dbip)
	return;
}

bool
QEll::eventFilter(QObject *, QEvent *e)
{
    if (e->type() == QEvent::MouseButtonPress || e->type() == QEvent::MouseButtonRelease ||   e->type() == QEvent::MouseButtonDblClick || e->type() == QEvent::MouseMove) {
	bu_log("ell mouse event\n");
    }
    return false;
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
