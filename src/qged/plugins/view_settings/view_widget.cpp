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

#include "bu/opt.h"
#include "bu/malloc.h"
#include "bu/str.h"

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
    mdlaxes_ckbx = new QCheckBox("Model Axes");
    params_ckbx = new QCheckBox("Parameters");
    scale_ckbx = new QCheckBox("Scale");
    viewaxes_ckbx = new QCheckBox("View Axes");
    wl->addWidget(a_ckbx);
    QObject::connect(a_ckbx, &QCheckBox::stateChanged, this, &CADViewSettings::view_update_int);
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

    QWidget *zw = new QWidget();
    QHBoxLayout *zl = new QHBoxLayout();
    zl->setSpacing(0);
    zl->setContentsMargins(1,1,1,1);
    QLabel *zlbl = new QLabel("Data vZ");
    vZ = new QLineEdit(QString("0"));
    QObject::connect(vZ, &QLineEdit::editingFinished, this, &CADViewSettings::view_update);
    zl->addWidget(zlbl);
    zl->addWidget(vZ);
    zw->setLayout(zl);
    wl->addWidget(zw);

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
CADViewSettings::view_update()
{
    view_refresh(m_v);
}

void
CADViewSettings::view_update_int(int)
{
    view_refresh(m_v);
}

void
CADViewSettings::checkbox_refresh(struct bview **nv)
{
    m_v = nv;
    if (m_v && *m_v) {
	struct bview *v = *m_v;
	a_ckbx->blockSignals(true);
	if (v->gv_s->adaptive_plot) {
	    a_ckbx->setCheckState(Qt::Checked);
	} else {
	    a_ckbx->setCheckState(Qt::Unchecked);
	}
	a_ckbx->blockSignals(false);

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

	fps_ckbx->blockSignals(true);
	if (v->gv_s->gv_fps) {
	    fps_ckbx->setCheckState(Qt::Checked);
	} else {
	    fps_ckbx->setCheckState(Qt::Unchecked);
	}
	fps_ckbx->blockSignals(false);

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

	params_ckbx->blockSignals(true);
	if (v->gv_s->gv_view_params.gos_draw) {
	    params_ckbx->setCheckState(Qt::Checked);
	} else {
	    params_ckbx->setCheckState(Qt::Unchecked);
	}
	params_ckbx->blockSignals(false);

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

	vZ->blockSignals(true);
	vZ->setText(QVariant(v->gv_data_vZ).toString());
	vZ->blockSignals(false);

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

	char *vZstr = bu_strdup(vZ->text().toLocal8Bit().data());
	fastf_t val;
	if (bu_opt_fastf_t(NULL, 1, (const char **)&vZstr, (void *)&val) == 1) {
	    v->gv_data_vZ = val;
	} else {
	    char **av = (char **)bu_calloc(strlen(vZstr) + 1, sizeof(char *), "argv array");
	    int nargs = bu_argv_from_string(av, strlen(vZstr), vZstr);
	    if (nargs) {
		vect_t mpt;
		int acnt = bu_opt_vect_t(NULL, nargs, (const char **)av, (void *)&mpt);
		if (acnt == 1 || acnt == 3) {
		    vect_t vpt;
		    MAT4X3PNT(vpt, v->gv_model2view, mpt);
		    v->gv_data_vZ = vpt[Z];
		}
	    }
	    bu_free(av, "argv array");
	}
	bu_free(vZstr, "vZstr cpy");

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
