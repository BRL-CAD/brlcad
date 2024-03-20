/*               C A D V I E W S E T T I N G S . H
 * BRL-CAD
 *
 * Copyright (c) 2023-2024 United States Government as represented by
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
 * TODO - update widget for bv/faceplate.h bv_params_state
 * mode of operation - FPS isn't separate, it's one of the
 * parameters.  Need to have an overall on-of box and a
 * set of specific flags for each individual drawing
 * element, similar to how General Polygon Modes box
 * works in QPolyMod.cpp
 *
 */

#include <QWidget>
#include <QGroupBox>
#include <QCheckBox>
#include <QLabel>
#include <QLineEdit>
#include "bv/defines.h"

class CADViewSettings : public QWidget
{
    Q_OBJECT

    public:
	CADViewSettings(QWidget *p = 0);
	~CADViewSettings();

	QCheckBox *acsg_ckbx;
	QCheckBox *amesh_ckbx;
	QCheckBox *adc_ckbx;
	QCheckBox *cdot_ckbx;
	QCheckBox *fb_ckbx;
	QCheckBox *fbo_ckbx;
	QCheckBox *fps_ckbx;
	QCheckBox *grid_ckbx;
	QCheckBox *mdlaxes_ckbx;
	QCheckBox *params_ckbx;
	QCheckBox *scale_ckbx;
	QCheckBox *viewaxes_ckbx;

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
