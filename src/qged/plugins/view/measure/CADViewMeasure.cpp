/*              C A D V I E W M E A S U R E . C P P
 * BRL-CAD
 *
 * Copyright (c) 2023 United States Government as represented by
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
/** @file CADViewMeasure.cpp
 *
 * Brief description
 *
 */

#include "common.h"
#include <QMouseEvent>
#include <QVBoxLayout>
#include <QtGlobal>
#include "../../../QgEdApp.h"

#include "bu/opt.h"
#include "bu/malloc.h"
#include "bu/str.h"
#include "bg/aabb_ray.h"
#include "bg/plane.h"
#include "bg/lod.h"

#include "./CADViewMeasure.h"

CADViewMeasure::CADViewMeasure(QWidget *)
{
    QVBoxLayout *wl = new QVBoxLayout;
    wl->setAlignment(Qt::AlignTop);

    measure_3d = new QCheckBox("Use 3D hit points");
    wl->addWidget(measure_3d);

    QLabel *ml1_label = new QLabel("Measured Length #1:");
    length1_report = new QLineEdit();
    length1_report->setReadOnly(true);
    wl->addWidget(ml1_label);
    wl->addWidget(length1_report);

    QLabel *ml2_label = new QLabel("Measured Length #2:");
    length2_report = new QLineEdit();
    length2_report->setReadOnly(true);
    wl->addWidget(ml2_label);
    wl->addWidget(length2_report);

    report_radians = new QCheckBox("Report angle in radians");
    wl->addWidget(report_radians);
    QObject::connect(report_radians, &QCheckBox::stateChanged, this, &CADViewMeasure::adjust_text);

    ma_label = new QLabel("Measured Angle (deg):");
    angle_report = new QLineEdit();
    angle_report->setReadOnly(true);
    wl->addWidget(ma_label);
    wl->addWidget(angle_report);

    color_2d = new QgColorRGB(this, "2D:", QColor(Qt::yellow));
    wl->addWidget(color_2d);
    color_3d = new QgColorRGB(this, "3D:", QColor(Qt::green));
    wl->addWidget(color_3d);
    QObject::connect(color_2d, &QgColorRGB::color_changed, this, &CADViewMeasure::update_color);
    QObject::connect(color_3d, &QgColorRGB::color_changed, this, &CADViewMeasure::update_color);

    this->setLayout(wl);

    f2d = new QMeasure2DFilter();
    f3d = new QMeasure3DFilter();
    mf = (measure_3d->isChecked()) ? (QgMeasureFilter *)f3d : (QgMeasureFilter *)f2d;
}

CADViewMeasure::~CADViewMeasure()
{
    if (s)
	bv_obj_put(s);
}

void
CADViewMeasure::update_color()
{
    if (!mf)
	return;
    if (measure_3d->isChecked()) {
	mf->update_color(&color_3d->bc);
	emit view_updated(QG_VIEW_REFRESH);
	return;
    }
    mf->update_color(&color_2d->bc);
    emit view_updated(QG_VIEW_REFRESH);
}

void
CADViewMeasure::adjust_text_db(void *)
{
    adjust_text();
}

void
CADViewMeasure::adjust_text()
{
    QgModel *m = ((QgEdApp *)qApp)->mdl;
    if (!m)
	return;
    struct ged *gedp = m->gedp;
    if (!gedp || !gedp->ged_gvp)
	return;


    double angle;
    if (report_radians->isChecked()) {
	ma_label = new QLabel("Measured Angle (rad):");
	angle = mf->angle(true);
    } else {
	ma_label = new QLabel("Measured Angle (deg):");
	angle = mf->angle(false);
    }

    struct bu_vls buffer = BU_VLS_INIT_ZERO;
    bu_vls_sprintf(&buffer, "%.15f %s", mf->length1()*gedp->dbip->dbi_base2local, bu_units_string(gedp->dbip->dbi_local2base));
    length1_report->setText(bu_vls_cstr(&buffer));

    bu_vls_sprintf(&buffer, "%.15f %s", mf->length2()*gedp->dbip->dbi_base2local, bu_units_string(gedp->dbip->dbi_local2base));
    length2_report->setText(bu_vls_cstr(&buffer));

    bu_vls_sprintf(&buffer, "%.15f", angle);
    angle_report->setText(bu_vls_cstr(&buffer));

    bu_vls_free(&buffer);
}

void
CADViewMeasure::do_filter_view_update()
{
    adjust_text();
    emit view_updated(QG_VIEW_REFRESH);
}


bool
CADViewMeasure::eventFilter(QObject *, QEvent *e)
{
    QgModel *m = ((QgEdApp *)qApp)->mdl;
    if (!m)
	return false;
    struct ged *gedp = m->gedp;
    if (!gedp || !gedp->ged_gvp)
	return false;
    struct bview *v = gedp->ged_gvp;

    f3d->dbip = gedp->dbip;

    mf = (measure_3d->isChecked()) ? (QgMeasureFilter *)f3d : (QgMeasureFilter *)f2d;

    mf->s = s;
    mf->v = v;
    update_color();

    // Connect whatever the current filter is to pass on updating signals from
    // the libqtcad logic.
    QObject::connect(mf, &QgMeasureFilter::view_updated, this, &CADViewMeasure::do_filter_view_update);

    bool ret = mf->eventFilter(NULL, e);

    // Retrieve the scene object from the libqtcad data container
    s = mf->s;

    QObject::disconnect(mf, &QgMeasureFilter::view_updated, this, &CADViewMeasure::do_filter_view_update);

    return ret;
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8

