/*                 V I E W _ W I D G E T . C P P
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
/** @file view_widget.cpp
 *
 */

#include <QVBoxLayout>
#include "view_widget.h"

CADViewSettings::CADViewSettings(QWidget *, struct bview **v)
{
    m_v = v;

    QVBoxLayout *wl = new QVBoxLayout;
    wl->setAlignment(Qt::AlignTop);

    a_ckbx = new QCheckBox("Adaptive Plotting");
    adc_ckbx = new QCheckBox("Angle/Dist. Cursor");
    cdot_ckbx = new QCheckBox("Center Dot");
    fb_ckbx = new QCheckBox("Framebuffer");
    fbo_ckbx = new QCheckBox("FB Overlay");
    fps_ckbx = new QCheckBox("FPS");
    grid_ckbx = new QCheckBox("Grid");
    i_ckbx = new QCheckBox("Independent View");
    mdlaxes_ckbx = new QCheckBox("Model Axes");
    params_ckbx = new QCheckBox("Parameters");
    scale_ckbx = new QCheckBox("Scale");
    viewaxes_ckbx = new QCheckBox("View Axes");
    wl->addWidget(a_ckbx);
    QObject::connect(a_ckbx, &QCheckBox::stateChanged, this, &CADViewSettings::view_update);
    wl->addWidget(adc_ckbx);
    QObject::connect(adc_ckbx, &QCheckBox::stateChanged, this, &CADViewSettings::view_update);
    wl->addWidget(cdot_ckbx);
    QObject::connect(cdot_ckbx, &QCheckBox::stateChanged, this, &CADViewSettings::view_update);
    wl->addWidget(fb_ckbx);
    QObject::connect(fb_ckbx, &QCheckBox::stateChanged, this, &CADViewSettings::view_update);
    wl->addWidget(fbo_ckbx);
    QObject::connect(fbo_ckbx, &QCheckBox::stateChanged, this, &CADViewSettings::view_update);
    wl->addWidget(fps_ckbx);
    QObject::connect(fps_ckbx, &QCheckBox::stateChanged, this, &CADViewSettings::view_update);
    wl->addWidget(grid_ckbx);
    QObject::connect(grid_ckbx, &QCheckBox::stateChanged, this, &CADViewSettings::view_update);
    wl->addWidget(i_ckbx);
    QObject::connect(i_ckbx, &QCheckBox::stateChanged, this, &CADViewSettings::view_update);
    wl->addWidget(mdlaxes_ckbx);
    QObject::connect(mdlaxes_ckbx, &QCheckBox::stateChanged, this, &CADViewSettings::view_update);
    wl->addWidget(params_ckbx);
    QObject::connect(params_ckbx, &QCheckBox::stateChanged, this, &CADViewSettings::view_update);
    wl->addWidget(scale_ckbx);
    QObject::connect(scale_ckbx, &QCheckBox::stateChanged, this, &CADViewSettings::view_update);
    wl->addWidget(viewaxes_ckbx);
    QObject::connect(viewaxes_ckbx, &QCheckBox::stateChanged, this, &CADViewSettings::view_update);
    this->setLayout(wl);
}

CADViewSettings::~CADViewSettings()
{
}

void
CADViewSettings::checkbox_update()
{
    checkbox_refresh(m_v);
}

void
CADViewSettings::view_update(int)
{
    view_refresh(m_v);
}

void
CADViewSettings::checkbox_refresh(struct bview **nv)
{
    m_v = nv;
    if (m_v && *m_v) {
	struct bview *v = *m_v;
	if (v->gv_s->adaptive_plot) {
	    a_ckbx->setCheckState(Qt::Checked);
	} else {
	    a_ckbx->setCheckState(Qt::Unchecked);
	}
	if (v->gv_s->gv_adc.draw) {
	    adc_ckbx->setCheckState(Qt::Checked);
	} else {
	    adc_ckbx->setCheckState(Qt::Unchecked);
	}
	if (v->gv_s->gv_center_dot.gos_draw) {
	    cdot_ckbx->setCheckState(Qt::Checked);
	} else {
	    cdot_ckbx->setCheckState(Qt::Unchecked);
	}
	if (v->gv_s->gv_fb_mode != 0) {
	    fb_ckbx->setCheckState(Qt::Checked);
	} else {
	    fb_ckbx->setCheckState(Qt::Unchecked);
	}
	if (v->gv_s->gv_fb_mode == 1) {
	    fbo_ckbx->setCheckState(Qt::Checked);
	} else {
	    fbo_ckbx->setCheckState(Qt::Unchecked);
	}
	if (v->gv_s->gv_fps) {
	    fps_ckbx->setCheckState(Qt::Checked);
	} else {
	    fps_ckbx->setCheckState(Qt::Unchecked);
	}
	if (v->gv_s->gv_grid.draw) {
	    grid_ckbx->setCheckState(Qt::Checked);
	} else {
	    grid_ckbx->setCheckState(Qt::Unchecked);
	}
	if (v->independent) {
	    i_ckbx->setCheckState(Qt::Checked);
	} else {
	    i_ckbx->setCheckState(Qt::Unchecked);
	}
	if (v->gv_s->gv_model_axes.draw) {
	    mdlaxes_ckbx->setCheckState(Qt::Checked);
	} else {
	    mdlaxes_ckbx->setCheckState(Qt::Unchecked);
	}
	if (v->gv_s->gv_view_params.gos_draw) {
	    params_ckbx->setCheckState(Qt::Checked);
	} else {
	    params_ckbx->setCheckState(Qt::Unchecked);
	}
	if (v->gv_s->gv_view_scale.gos_draw) {
	    scale_ckbx->setCheckState(Qt::Checked);
	} else {
	    scale_ckbx->setCheckState(Qt::Unchecked);
	}
	if (v->gv_s->gv_view_axes.draw) {
	    viewaxes_ckbx->setCheckState(Qt::Checked);
	} else {
	    viewaxes_ckbx->setCheckState(Qt::Unchecked);
	}

    }
}

void
CADViewSettings::view_refresh(struct bview **nv)
{
    m_v = nv;
    if (m_v && *m_v) {
	struct bview *v = *m_v;
	if (a_ckbx->checkState() == Qt::Checked) {
	    v->gv_s->adaptive_plot = 1;
	} else {
	    v->gv_s->adaptive_plot = 0;
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
	if (fps_ckbx->checkState() == Qt::Checked) {
	    v->gv_s->gv_fps = 1;
	} else {
	    v->gv_s->gv_fps = 0;
	}
	if (grid_ckbx->checkState() == Qt::Checked) {
	    v->gv_s->gv_grid.draw = 1;
	} else {
	    v->gv_s->gv_grid.draw = 0;
	}
	if (i_ckbx->checkState() == Qt::Checked) {
	    v->independent = 1;
	} else {
	    v->independent = 0;
	}
	if (mdlaxes_ckbx->checkState() == Qt::Checked) {
	    v->gv_s->gv_model_axes.draw = 1;
	} else {
	    v->gv_s->gv_model_axes.draw = 0;
	}
	if (params_ckbx->checkState() == Qt::Checked) {
	    v->gv_s->gv_view_params.gos_draw = 1;
	} else {
	    v->gv_s->gv_view_params.gos_draw = 0;
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

	emit settings_changed(m_v);
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
