/*               C A D V I E W S E T T I N G S . H
 * BRL-CAD
 *
 * Copyright (c) 2023-2025 United States Government as represented by
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
/** @file CADViewSettings.h
 *
 * Widget for controlling and reflecting the current state of view
 * settings (faceplate elements).  Covers all fields in bview_settings
 * and bv_params_state that have widget-level controls.
 *
 */

#include <QWidget>
#include <QGroupBox>
#include <QCheckBox>
#include <QComboBox>
#include "bv/defines.h"

class CADViewSettings : public QWidget
{
    Q_OBJECT

    public:
	CADViewSettings(QWidget *p = 0);
	~CADViewSettings();

	/* Top-level faceplate toggles */
	QCheckBox *acsg_ckbx;
	QCheckBox *amesh_ckbx;
	QCheckBox *adc_ckbx;
	QCheckBox *cdot_ckbx;
	QCheckBox *grid_ckbx;
	QCheckBox *mdlaxes_ckbx;
	QCheckBox *scale_ckbx;
	QCheckBox *viewaxes_ckbx;

	/* Framebuffer mode: index 0=off, 1=overlay, 2=underlay */
	QComboBox *fb_mode_combo;

	/* View parameters group */
	QGroupBox *params_grp;
	QCheckBox *params_ckbx;
	QCheckBox *params_size_ckbx;
	QCheckBox *params_center_ckbx;
	QCheckBox *params_az_ckbx;
	QCheckBox *params_el_ckbx;
	QCheckBox *params_tw_ckbx;
	QCheckBox *params_fps_ckbx;

    signals:
	void settings_changed(unsigned long long);

    public slots:
	void checkbox_refresh(unsigned long long);
	void checkbox_update();
	void view_refresh(unsigned long long);
	void view_update_int(int);
	void view_update();
};

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
