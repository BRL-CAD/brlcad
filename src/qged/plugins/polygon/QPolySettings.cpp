/*                 Q P O L Y S E T T I N G S . C P P
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
/** @file polygon_settings.cpp
 *
 */

#include <QLabel>
#include <QString>
#include "bg/polygon.h"
#include "QPolySettings.h"

QPolySettings::QPolySettings()
    : QWidget()
{
    QVBoxLayout *l = new QVBoxLayout;
    l->setAlignment(Qt::AlignTop);
    l->setSpacing(1);
    l->setContentsMargins(1,1,1,1);

    QLabel *vn_label = new QLabel("Polygon view obj name:");
    view_name = new QLineEdit();
    l->addWidget(vn_label);
    l->addWidget(view_name);

    QLabel *sn_label = new QLabel("Polygon sketch (.g) name:");
    l->addWidget(sn_label);
    QWidget *sn = new QWidget();
    QHBoxLayout *snl = new QHBoxLayout();
    snl->setSpacing(0);
    snl->setContentsMargins(1,1,1,1);
    sketch_sync = new QCheckBox();
    snl->addWidget(sketch_sync);
    sketch_name = new QLineEdit();
    sketch_name->setPlaceholderText("Enable to save sketch");
    sketch_name->setStyleSheet("color: rgb(200,200,200)");
    sketch_name->setEnabled(false);
    snl->addWidget(sketch_name);
    QObject::connect(sketch_sync, &QCheckBox::toggled, this, &QPolySettings::sketch_sync_toggled);
    sn->setLayout(snl);
    l->addWidget(sn);

    edge_color = new QColorRGB(this, "Edge:", QColor(Qt::yellow));
    l->addWidget(edge_color);
    QObject::connect(edge_color, &QColorRGB::colorChanged, this, &QPolySettings::do_settings_changed);
    fill_poly = new QCheckBox("Shade polygon interiors");
    l->addWidget(fill_poly);
    QObject::connect(fill_poly, &QCheckBox::toggled, this, &QPolySettings::do_settings_changed);
    fill_color = new QColorRGB(this, "Fill:", QColor(Qt::blue));
    l->addWidget(fill_color);
    QObject::connect(fill_color, &QColorRGB::colorChanged, this, &QPolySettings::do_settings_changed);

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
QPolySettings::sketch_sync_toggled(bool)
{
    if (sketch_sync->isChecked()) {
	if (sketch_name->placeholderText() == QString("Enable to save sketch"))
	    sketch_name->setPlaceholderText("");
	sketch_name->setStyleSheet("");
	sketch_name->setEnabled(true);
    } else {
	sketch_name->setPlaceholderText("Enable to save sketch");
	sketch_name->setStyleSheet("color: rgb(200,200,200)");
	sketch_name->setEnabled(false);
    }
}

void
QPolySettings::do_settings_changed()
{
    emit settings_changed();
}

void
QPolySettings::settings_sync(struct bv_scene_obj *p)
{
    if (!p)
	return;


    struct bv_polygon *ip = (struct bv_polygon *)p->s_i_data;

    edge_color->blockSignals(true);
    edge_color->setText(QString("%1/%2/%3").arg(p->s_color[0]).arg(p->s_color[1]).arg(p->s_color[2]));
    edge_color->blockSignals(false);

    unsigned char frgb[3];
    bu_color_to_rgb_chars(&ip->fill_color, frgb);
    fill_color->blockSignals(true);
    fill_color->setText(QString("%1/%2/%3").arg(frgb[0]).arg(frgb[1]).arg(frgb[2]));
    fill_color->blockSignals(false);

    fill_slope_x->blockSignals(true);
    fill_slope_x->setText(QString("%1").arg(ip->fill_dir[0]));
    fill_slope_x->blockSignals(false);

    fill_slope_y->blockSignals(true);
    fill_slope_y->setText(QString("%1").arg(ip->fill_dir[1]));
    fill_slope_y->blockSignals(false);

    fill_density->blockSignals(true);
    fill_density->setText(QString("%1").arg(ip->fill_delta));
    fill_density->blockSignals(false);

    fill_poly->blockSignals(true);
    if (ip->fill_flag) {
	fill_poly->setChecked(true);
    } else {
	fill_poly->setChecked(false);
    }
    fill_poly->blockSignals(false);

    // Values set, now update the button colors
    this->blockSignals(true);
    edge_color->setColorFromText();
    fill_color->setColorFromText();
    this->blockSignals(false);
}



// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
