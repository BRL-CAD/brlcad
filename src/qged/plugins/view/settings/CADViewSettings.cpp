/*             C A D V I E W S E T T I N G S . C P P
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
/** @file CADViewSettings.cpp
 *
 * Brief description
 *
 */

#include <QVBoxLayout>

#include "bu/opt.h"
#include "bu/malloc.h"
#include "bu/str.h"
#include "qtcad/QgSignalFlags.h"
#include "QgEdApp.h"

#include "CADViewSettings.h"

CADViewSettings::CADViewSettings(QWidget *)
{
    QVBoxLayout *wl = new QVBoxLayout;
    wl->setAlignment(Qt::AlignTop);

    acsg_ckbx = new QCheckBox("Adaptive Plotting (CSG)");
    amesh_ckbx = new QCheckBox("Adaptive Plotting (Mesh)");
    adc_ckbx = new QCheckBox("Angle/Dist. Cursor");
    cdot_ckbx = new QCheckBox("Center Dot");
    fb_ckbx = new QCheckBox("Framebuffer");
    fbo_ckbx = new QCheckBox("FB Overlay");
    fps_ckbx = new QCheckBox("FPS");
    grid_ckbx = new QCheckBox("Grid");
    mdlaxes_ckbx = new QCheckBox("Model Axes");
    params_ckbx = new QCheckBox("Parameters");
    scale_ckbx = new QCheckBox("Scale");
    viewaxes_ckbx = new QCheckBox("View Axes");
    wl->addWidget(acsg_ckbx);
    QObject::connect(acsg_ckbx, &QCheckBox::stateChanged, this, &CADViewSettings::view_update_int);
    wl->addWidget(amesh_ckbx);
    QObject::connect(amesh_ckbx, &QCheckBox::stateChanged, this, &CADViewSettings::view_update_int);
    wl->addWidget(adc_ckbx);
    QObject::connect(adc_ckbx, &QCheckBox::stateChanged, this, &CADViewSettings::view_update_int);
    wl->addWidget(cdot_ckbx);
    QObject::connect(cdot_ckbx, &QCheckBox::stateChanged, this, &CADViewSettings::view_update_int);
    wl->addWidget(fb_ckbx);
    QObject::connect(fb_ckbx, &QCheckBox::stateChanged, this, &CADViewSettings::view_update_int);
    wl->addWidget(fbo_ckbx);
    QObject::connect(fbo_ckbx, &QCheckBox::stateChanged, this, &CADViewSettings::view_update_int);
    wl->addWidget(fps_ckbx);
    QObject::connect(fps_ckbx, &QCheckBox::stateChanged, this, &CADViewSettings::view_update_int);
    wl->addWidget(grid_ckbx);
    QObject::connect(grid_ckbx, &QCheckBox::stateChanged, this, &CADViewSettings::view_update_int);
    wl->addWidget(mdlaxes_ckbx);
    QObject::connect(mdlaxes_ckbx, &QCheckBox::stateChanged, this, &CADViewSettings::view_update_int);
    wl->addWidget(params_ckbx);
    QObject::connect(params_ckbx, &QCheckBox::stateChanged, this, &CADViewSettings::view_update_int);
    wl->addWidget(scale_ckbx);
    QObject::connect(scale_ckbx, &QCheckBox::stateChanged, this, &CADViewSettings::view_update_int);
    wl->addWidget(viewaxes_ckbx);
    QObject::connect(viewaxes_ckbx, &QCheckBox::stateChanged, this, &CADViewSettings::view_update_int);

    this->setLayout(wl);
}

CADViewSettings::~CADViewSettings()
{
}

void
CADViewSettings::checkbox_update()
{
    checkbox_refresh(0);
}

void
CADViewSettings::view_update()
{
    view_refresh(0);
}

void
CADViewSettings::view_update_int(int)
{
    view_refresh(0);
}

void
CADViewSettings::checkbox_refresh(unsigned long long)
{
    QgModel *m = ((QgEdApp *)qApp)->mdl;
    if (!m)
	return;
    struct ged *gedp = m->gedp;
    if (!gedp)
	return;

    struct bview *v = gedp->ged_gvp;
    if (!v)
	return;

    acsg_ckbx->blockSignals(true);
    if (v->gv_s->adaptive_plot_csg) {
	acsg_ckbx->setCheckState(Qt::Checked);
    } else {
	acsg_ckbx->setCheckState(Qt::Unchecked);
    }
    acsg_ckbx->blockSignals(false);

    amesh_ckbx->blockSignals(true);
    if (v->gv_s->adaptive_plot_mesh) {
	amesh_ckbx->setCheckState(Qt::Checked);
    } else {
	amesh_ckbx->setCheckState(Qt::Unchecked);
    }
    amesh_ckbx->blockSignals(false);

    adc_ckbx->blockSignals(true);
    if (v->gv_s->gv_adc.draw) {
	adc_ckbx->setCheckState(Qt::Checked);
    } else {
	adc_ckbx->setCheckState(Qt::Unchecked);
    }
    adc_ckbx->blockSignals(false);

    cdot_ckbx->blockSignals(true);
    if (v->gv_s->gv_center_dot.gos_draw) {
	cdot_ckbx->setCheckState(Qt::Checked);
    } else {
	cdot_ckbx->setCheckState(Qt::Unchecked);
    }
    cdot_ckbx->blockSignals(false);

    fb_ckbx->blockSignals(true);
    if (v->gv_s->gv_fb_mode != 0) {
	fb_ckbx->setCheckState(Qt::Checked);
    } else {
	fb_ckbx->setCheckState(Qt::Unchecked);
    }
    fb_ckbx->blockSignals(false);

    fbo_ckbx->blockSignals(true);
    if (v->gv_s->gv_fb_mode == 1) {
	fbo_ckbx->setCheckState(Qt::Checked);
    } else {
	fbo_ckbx->setCheckState(Qt::Unchecked);
    }
    fbo_ckbx->blockSignals(false);

    grid_ckbx->blockSignals(true);
    if (v->gv_s->gv_grid.draw) {
	grid_ckbx->setCheckState(Qt::Checked);
    } else {
	grid_ckbx->setCheckState(Qt::Unchecked);
    }
    grid_ckbx->blockSignals(false);

    mdlaxes_ckbx->blockSignals(true);
    if (v->gv_s->gv_model_axes.draw) {
	mdlaxes_ckbx->setCheckState(Qt::Checked);
    } else {
	mdlaxes_ckbx->setCheckState(Qt::Unchecked);
    }
    mdlaxes_ckbx->blockSignals(false);


    // TODO - add other params checkboxes
    struct bv_params_state *pst = &v->gv_s->gv_view_params;
    params_ckbx->blockSignals(true);
    if (pst->draw) {
	params_ckbx->setCheckState(Qt::Checked);
    } else {
	params_ckbx->setCheckState(Qt::Unchecked);
    }
    params_ckbx->blockSignals(false);

    fps_ckbx->blockSignals(true);
    if (pst->draw_fps) {
	fps_ckbx->setCheckState(Qt::Checked);
    } else {
	fps_ckbx->setCheckState(Qt::Unchecked);
    }
    fps_ckbx->blockSignals(false);


    scale_ckbx->blockSignals(true);
    if (v->gv_s->gv_view_scale.gos_draw) {
	scale_ckbx->setCheckState(Qt::Checked);
    } else {
	scale_ckbx->setCheckState(Qt::Unchecked);
    }
    scale_ckbx->blockSignals(false);

    viewaxes_ckbx->blockSignals(true);
    if (v->gv_s->gv_view_axes.draw) {
	viewaxes_ckbx->setCheckState(Qt::Checked);
    } else {
	viewaxes_ckbx->setCheckState(Qt::Unchecked);
    }
    viewaxes_ckbx->blockSignals(false);
}

void
CADViewSettings::view_refresh(unsigned long long)
{
    QgModel *m = ((QgEdApp *)qApp)->mdl;
    if (!m)
	return;
    struct ged *gedp = m->gedp;
    if (!gedp)
	return;

    struct bview *v = gedp->ged_gvp;

    if (acsg_ckbx->checkState() == Qt::Checked) {
	v->gv_s->adaptive_plot_csg = 1;
    } else {
	v->gv_s->adaptive_plot_csg = 0;
    }
    if (amesh_ckbx->checkState() == Qt::Checked) {
	v->gv_s->adaptive_plot_mesh = 1;
    } else {
	v->gv_s->adaptive_plot_mesh = 0;
    }
    if (adc_ckbx->checkState() == Qt::Checked) {
	v->gv_s->gv_adc.draw = 1;
    } else {
	v->gv_s->gv_adc.draw = 0;
    }
    if (cdot_ckbx->checkState() == Qt::Checked) {
	v->gv_s->gv_center_dot.gos_draw = 1;
    } else {
	v->gv_s->gv_center_dot.gos_draw = 0;
    }
    if (fb_ckbx->checkState() == Qt::Checked) {
	if (fbo_ckbx->checkState() == Qt::Checked) {
	    v->gv_s->gv_fb_mode = 1;
	} else {
	    v->gv_s->gv_fb_mode = 2;
	}
    } else {
	v->gv_s->gv_fb_mode = 0;
    }

    struct bv_params_state *pst = &v->gv_s->gv_view_params;
    if (fps_ckbx->checkState() == Qt::Checked) {
	pst->draw_fps = 1;
    } else {
	pst->draw_fps = 0;
    }
    if (grid_ckbx->checkState() == Qt::Checked) {
	v->gv_s->gv_grid.draw = 1;
    } else {
	v->gv_s->gv_grid.draw = 0;
    }
    if (mdlaxes_ckbx->checkState() == Qt::Checked) {
	v->gv_s->gv_model_axes.draw = 1;
    } else {
	v->gv_s->gv_model_axes.draw = 0;
    }
    if (params_ckbx->checkState() == Qt::Checked) {
	pst->draw = 1;
    } else {
	pst->draw = 0;
    }
    if (scale_ckbx->checkState() == Qt::Checked) {
	v->gv_s->gv_view_scale.gos_draw = 1;
    } else {
	v->gv_s->gv_view_scale.gos_draw = 0;
    }
    if (viewaxes_ckbx->checkState() == Qt::Checked) {
	v->gv_s->gv_view_axes.draw = 1;
    } else {
	v->gv_s->gv_view_axes.draw = 0;
    }

    emit settings_changed(QG_VIEW_DRAWN);
}


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
