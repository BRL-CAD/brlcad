/*            P O L Y G O N _ S E T T I N G S . C P P
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
/** @file polygon_settings.cpp
 *
 */

#include <QLabel>
#include <QString>
#include "polygon_settings.h"

QPolySettings::QPolySettings()
    : QWidget()
{
    QVBoxLayout *l = new QVBoxLayout;
    l->setAlignment(Qt::AlignTop);
    l->setSpacing(1);
    l->setContentsMargins(1,1,1,1);

    edge_color = new QColorRGB(this, "Edge:", QColor(Qt::yellow));
    l->addWidget(edge_color);
    QObject::connect(edge_color, &QColorRGB::color_changed, this, &QPolySettings::do_settings_changed);
    fill_poly = new QCheckBox("Shade polygon interiors");
    l->addWidget(fill_poly);
    fill_color = new QColorRGB(this, "Fill:", QColor(Qt::blue));
    l->addWidget(fill_color);
    QObject::connect(fill_color, &QColorRGB::color_changed, this, &QPolySettings::do_settings_changed);

    QFont f("");
    f.setStyleHint(QFont::Monospace);

    QWidget *xw = new QWidget();
    QHBoxLayout *hx = new QHBoxLayout();
    hx->setSpacing(0);
    hx->setContentsMargins(1,1,1,1);
    QLabel *xlbl = new QLabel(QString("<font face=\"monospace\">Fill Slope X:</font>"));
    hx->addWidget(xlbl);
    fill_slope_x = new QLineEdit(QString("1"));
    fill_slope_x->setFont(f);
    QDoubleValidator *hxv = new QDoubleValidator(0, DBL_MAX, 5);
    fill_slope_x->setValidator(hxv);
    QObject::connect(fill_slope_x, &QLineEdit::editingFinished, this, &QPolySettings::do_settings_changed);
    hx->addWidget(fill_slope_x);
    xw->setLayout(hx);
    l->addWidget(xw);

    QWidget *yw = new QWidget();
    QHBoxLayout *hy = new QHBoxLayout();
    hy->setSpacing(0);
    hy->setContentsMargins(1,1,1,1);
    QLabel *ylbl = new QLabel(QString("<font face=\"monospace\">Fill Slope Y:</font>"));
    hy->addWidget(ylbl);
    fill_slope_y = new QLineEdit(QString("1"));
    fill_slope_y->setFont(f);
    QDoubleValidator *hyv = new QDoubleValidator(0, DBL_MAX, 5);
    fill_slope_y->setValidator(hyv);
    QObject::connect(fill_slope_y, &QLineEdit::editingFinished, this, &QPolySettings::do_settings_changed);
    hy->addWidget(fill_slope_y);
    yw->setLayout(hy);
    l->addWidget(yw);


    QWidget *dw = new QWidget();
    QHBoxLayout *hd = new QHBoxLayout();
    hd->setSpacing(0);
    hd->setContentsMargins(1,1,1,1);
    QLabel *dlbl = new QLabel(QString("<font face=\"monospace\">Fill Spacing:</font>"));
    hd->addWidget(dlbl);
    fill_density = new QLineEdit(QString("10"));
    fill_density->setFont(f);
    QDoubleValidator *hdv = new QDoubleValidator(0, DBL_MAX, 5);
    fill_density->setValidator(hdv);
    QObject::connect(fill_density, &QLineEdit::editingFinished, this, &QPolySettings::do_settings_changed);
    hd->addWidget(fill_density);
    dw->setLayout(hd);
    l->addWidget(dw);

    this->setLayout(l);
}

QPolySettings::~QPolySettings()
{
}

void
QPolySettings::do_settings_changed()
{
    emit settings_changed();
}

void
QPolySettings::settings_sync(struct bv_scene_obj *)
{
}



// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
