/*             C A D V I E W S E T T I N G S . C P P
 * BRL-CAD
 *
 * Copyright (c) 2023-2026 United States Government as represented by
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
 * Widget implementation for viewing and controlling faceplate settings,
 * reflecting the current state of bsg_view_settings and bsg_params_state.
 *
 */

#include <QHBoxLayout>
#include <QLabel>
#include <QVBoxLayout>

#include "bu/opt.h"
#include "bu/malloc.h"
#include "bu/str.h"
#include "bsg/render_settings.h"
#include "bsg/view_state.h"
#include "qtcad/QgPluginContext.h"
#include "qtcad/QgSignalFlags.h"

#include "CADViewSettings.h"

/* Helper: update a checkbox to reflect an integer flag, blocking signals
 * to prevent triggering a spurious view_refresh round-trip. */
static void
set_ckbx(QCheckBox *cb, int val)
{
    cb->blockSignals(true);
    cb->setCheckState(val ? Qt::Checked : Qt::Unchecked);
    cb->blockSignals(false);
}

/* Helper: return 1 if the checkbox is checked, 0 otherwise. */
static int
ckbx_val(QCheckBox *cb)
{
    return (cb->checkState() == Qt::Checked) ? 1 : 0;
}

CADViewSettings::CADViewSettings(QWidget *)
{
    QVBoxLayout *wl = new QVBoxLayout;
    wl->setAlignment(Qt::AlignTop);

    /* Top-level faceplate toggles */
    acsg_ckbx = new QCheckBox("Adaptive Plotting (CSG)");
    amesh_ckbx = new QCheckBox("Adaptive Plotting (Mesh)");
    adc_ckbx = new QCheckBox("Angle/Dist. Cursor");
    cdot_ckbx = new QCheckBox("Center Dot");
    grid_ckbx = new QCheckBox("Grid");
    mdlaxes_ckbx = new QCheckBox("Model Axes");
    scale_ckbx = new QCheckBox("Scale");
    viewaxes_ckbx = new QCheckBox("View Axes");

    /* Framebuffer mode selector: Off / Overlay / Underlay */
    QHBoxLayout *fbl = new QHBoxLayout;
    fbl->addWidget(new QLabel("Framebuffer:"));
    fb_mode_combo = new QComboBox;
    fb_mode_combo->addItem("Off");      /* index 0 -> gv_fb_mode = 0 */
    fb_mode_combo->addItem("Overlay");  /* index 1 -> gv_fb_mode = 1 */
    fb_mode_combo->addItem("Underlay"); /* index 2 -> gv_fb_mode = 2 */
    fbl->addWidget(fb_mode_combo);
    fbl->addStretch();

    /* Parameters group: master toggle + per-element sub-flags */
    params_grp = new QGroupBox("Parameters");
    QVBoxLayout *pgl = new QVBoxLayout;
    params_ckbx = new QCheckBox("Enable");
    params_size_ckbx = new QCheckBox("Size");
    params_center_ckbx = new QCheckBox("Center");
    params_az_ckbx = new QCheckBox("Azimuth");
    params_el_ckbx = new QCheckBox("Elevation");
    params_tw_ckbx = new QCheckBox("Twist");
    params_fps_ckbx = new QCheckBox("FPS");
    pgl->addWidget(params_ckbx);
    pgl->addWidget(params_size_ckbx);
    pgl->addWidget(params_center_ckbx);
    pgl->addWidget(params_az_ckbx);
    pgl->addWidget(params_el_ckbx);
    pgl->addWidget(params_tw_ckbx);
    pgl->addWidget(params_fps_ckbx);
    params_grp->setLayout(pgl);

    /* Wire up signals -> view_refresh.
     * QCheckBox::stateChanged(int) works across all Qt5/Qt6 versions.
     * checkStateChanged(Qt::CheckState) was only added in Qt 6.7, so we
     * use the version-specific signal to avoid deprecation warnings when
     * building with a sufficiently new Qt. */
#if QT_VERSION < QT_VERSION_CHECK(6, 7, 0)
    QObject::connect(acsg_ckbx,         &QCheckBox::stateChanged, this, &CADViewSettings::view_update_int);
    QObject::connect(amesh_ckbx,        &QCheckBox::stateChanged, this, &CADViewSettings::view_update_int);
    QObject::connect(adc_ckbx,          &QCheckBox::stateChanged, this, &CADViewSettings::view_update_int);
    QObject::connect(cdot_ckbx,         &QCheckBox::stateChanged, this, &CADViewSettings::view_update_int);
    QObject::connect(grid_ckbx,         &QCheckBox::stateChanged, this, &CADViewSettings::view_update_int);
    QObject::connect(mdlaxes_ckbx,      &QCheckBox::stateChanged, this, &CADViewSettings::view_update_int);
    QObject::connect(scale_ckbx,        &QCheckBox::stateChanged, this, &CADViewSettings::view_update_int);
    QObject::connect(viewaxes_ckbx,     &QCheckBox::stateChanged, this, &CADViewSettings::view_update_int);
    QObject::connect(params_ckbx,       &QCheckBox::stateChanged, this, &CADViewSettings::view_update_int);
    QObject::connect(params_size_ckbx,  &QCheckBox::stateChanged, this, &CADViewSettings::view_update_int);
    QObject::connect(params_center_ckbx,&QCheckBox::stateChanged, this, &CADViewSettings::view_update_int);
    QObject::connect(params_az_ckbx,    &QCheckBox::stateChanged, this, &CADViewSettings::view_update_int);
    QObject::connect(params_el_ckbx,    &QCheckBox::stateChanged, this, &CADViewSettings::view_update_int);
    QObject::connect(params_tw_ckbx,    &QCheckBox::stateChanged, this, &CADViewSettings::view_update_int);
    QObject::connect(params_fps_ckbx,   &QCheckBox::stateChanged, this, &CADViewSettings::view_update_int);
#else
    QObject::connect(acsg_ckbx,         &QCheckBox::checkStateChanged, this, &CADViewSettings::view_update_int);
    QObject::connect(amesh_ckbx,        &QCheckBox::checkStateChanged, this, &CADViewSettings::view_update_int);
    QObject::connect(adc_ckbx,          &QCheckBox::checkStateChanged, this, &CADViewSettings::view_update_int);
    QObject::connect(cdot_ckbx,         &QCheckBox::checkStateChanged, this, &CADViewSettings::view_update_int);
    QObject::connect(grid_ckbx,         &QCheckBox::checkStateChanged, this, &CADViewSettings::view_update_int);
    QObject::connect(mdlaxes_ckbx,      &QCheckBox::checkStateChanged, this, &CADViewSettings::view_update_int);
    QObject::connect(scale_ckbx,        &QCheckBox::checkStateChanged, this, &CADViewSettings::view_update_int);
    QObject::connect(viewaxes_ckbx,     &QCheckBox::checkStateChanged, this, &CADViewSettings::view_update_int);
    QObject::connect(params_ckbx,       &QCheckBox::checkStateChanged, this, &CADViewSettings::view_update_int);
    QObject::connect(params_size_ckbx,  &QCheckBox::checkStateChanged, this, &CADViewSettings::view_update_int);
    QObject::connect(params_center_ckbx,&QCheckBox::checkStateChanged, this, &CADViewSettings::view_update_int);
    QObject::connect(params_az_ckbx,    &QCheckBox::checkStateChanged, this, &CADViewSettings::view_update_int);
    QObject::connect(params_el_ckbx,    &QCheckBox::checkStateChanged, this, &CADViewSettings::view_update_int);
    QObject::connect(params_tw_ckbx,    &QCheckBox::checkStateChanged, this, &CADViewSettings::view_update_int);
    QObject::connect(params_fps_ckbx,   &QCheckBox::checkStateChanged, this, &CADViewSettings::view_update_int);
#endif
    QObject::connect(fb_mode_combo,
		     QOverload<int>::of(&QComboBox::currentIndexChanged),
		     this, &CADViewSettings::view_update_int);

    /* Assemble the top-level layout */
    wl->addWidget(acsg_ckbx);
    wl->addWidget(amesh_ckbx);
    wl->addWidget(adc_ckbx);
    wl->addWidget(cdot_ckbx);
    wl->addWidget(grid_ckbx);
    wl->addWidget(mdlaxes_ckbx);
    wl->addWidget(scale_ckbx);
    wl->addWidget(viewaxes_ckbx);
    wl->addLayout(fbl);
    wl->addWidget(params_grp);

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

/* Read current view state and update all widgets to match, without
 * triggering a spurious write-back via the signal connections. */
void
CADViewSettings::checkbox_refresh(unsigned long long)
{
    struct bsg_view *v = m_ctx ? m_ctx->getView() : nullptr;
    if (!v)
	return;

    /* Read render policy through the canonical settings record. */
    struct bsg_render_settings rs;
    bsg_render_settings_from_view(&rs, v);

    /* Top-level faceplate elements */
    struct bsg_adc_state adc = {};
    struct bsg_other_state center_dot = {};
    struct bsg_grid_state grid = {};
    struct bsg_axes model_axes = {};
    struct bsg_other_state scale_state = {};
    struct bsg_axes view_axes = {};
    struct bsg_params_state params = {};
    (void)bsg_view_adc_get(v, &adc);
    (void)bsg_view_center_dot_get(v, &center_dot);
    (void)bsg_view_grid_get(v, &grid);
    (void)bsg_view_model_axes_get(v, &model_axes);
    (void)bsg_view_scale_state_get(v, &scale_state);
    (void)bsg_view_view_axes_get(v, &view_axes);
    (void)bsg_view_params_get(v, &params);

    set_ckbx(acsg_ckbx,     rs.lod_source_policy.csg_enabled);
    set_ckbx(amesh_ckbx,    rs.lod_source_policy.mesh_enabled);
    set_ckbx(adc_ckbx,      adc.draw);
    set_ckbx(cdot_ckbx,     center_dot.gos_draw);
    set_ckbx(grid_ckbx,     grid.draw);
    set_ckbx(mdlaxes_ckbx,  model_axes.draw);
    set_ckbx(scale_ckbx,    scale_state.gos_draw);
    set_ckbx(viewaxes_ckbx, view_axes.draw);

    /* Framebuffer mode (0=off, 1=overlay, 2=underlay) maps directly to
     * combo index. Clamp to a valid range in case of unexpected values. */
    int fb_mode = (int)rs.fb_mode;
    if (fb_mode < 0 || fb_mode > 2)
	fb_mode = 0;
    fb_mode_combo->blockSignals(true);
    fb_mode_combo->setCurrentIndex(fb_mode);
    fb_mode_combo->blockSignals(false);

    /* Parameters group: master draw toggle + per-element sub-flags */
    set_ckbx(params_ckbx,        params.draw);
    set_ckbx(params_size_ckbx,   params.draw_size);
    set_ckbx(params_center_ckbx, params.draw_center);
    set_ckbx(params_az_ckbx,     params.draw_az);
    set_ckbx(params_el_ckbx,     params.draw_el);
    set_ckbx(params_tw_ckbx,     params.draw_tw);
    set_ckbx(params_fps_ckbx,    params.draw_fps);
}

/* Read all widget states and write them back to the view, then signal
 * that the view needs to be redrawn. */
void
CADViewSettings::view_refresh(unsigned long long)
{
    struct bsg_view *v = m_ctx ? m_ctx->getView() : nullptr;
    if (!v)
	return;

    /* Start from the current render state so non-widget
     * policy fields are preserved, overwrite the widget-backed render-policy
     * fields, then flush back to the view. */
    struct bsg_render_settings rs;
    bsg_render_settings_from_view(&rs, v);
    rs.lod_source_policy.csg_enabled = ckbx_val(acsg_ckbx);
    rs.lod_source_policy.mesh_enabled = ckbx_val(amesh_ckbx);
    rs.lod_source_policy.zoom_refresh =
	rs.lod_source_policy.csg_enabled || rs.lod_source_policy.mesh_enabled;
    rs.fb_mode            = (bsg_framebuffer_mode)fb_mode_combo->currentIndex();
    bsg_render_settings_apply_to_view(&rs, v);

    struct bsg_adc_state adc = {};
    struct bsg_other_state center_dot = {};
    struct bsg_grid_state grid = {};
    struct bsg_axes model_axes = {};
    struct bsg_other_state scale_state = {};
    struct bsg_axes view_axes = {};
    struct bsg_params_state params = {};
    (void)bsg_view_adc_get(v, &adc);
    (void)bsg_view_center_dot_get(v, &center_dot);
    (void)bsg_view_grid_get(v, &grid);
    (void)bsg_view_model_axes_get(v, &model_axes);
    (void)bsg_view_scale_state_get(v, &scale_state);
    (void)bsg_view_view_axes_get(v, &view_axes);
    (void)bsg_view_params_get(v, &params);

    adc.draw = ckbx_val(adc_ckbx);
    center_dot.gos_draw = ckbx_val(cdot_ckbx);
    grid.draw = ckbx_val(grid_ckbx);
    model_axes.draw = ckbx_val(mdlaxes_ckbx);
    scale_state.gos_draw = ckbx_val(scale_ckbx);
    view_axes.draw = ckbx_val(viewaxes_ckbx);

    params.draw        = ckbx_val(params_ckbx);
    params.draw_size   = ckbx_val(params_size_ckbx);
    params.draw_center = ckbx_val(params_center_ckbx);
    params.draw_az     = ckbx_val(params_az_ckbx);
    params.draw_el     = ckbx_val(params_el_ckbx);
    params.draw_tw     = ckbx_val(params_tw_ckbx);
    params.draw_fps    = ckbx_val(params_fps_ckbx);

    bsg_view_adc_set(v, &adc);
    bsg_view_center_dot_set(v, &center_dot);
    bsg_view_grid_set(v, &grid);
    bsg_view_model_axes_set(v, &model_axes);
    bsg_view_scale_state_set(v, &scale_state);
    bsg_view_view_axes_set(v, &view_axes);
    bsg_view_params_set(v, &params);

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
