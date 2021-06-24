/*                V I E W _ W I D G E T . H
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
/** @file view_widget.h
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
	CADViewSettings(QWidget *p = 0, struct bview **v = NULL);
	~CADViewSettings();

	QCheckBox *a_ckbx;
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

	QLineEdit *vZ;
    signals:
	void settings_changed(struct bview **);

    public slots:
	void checkbox_refresh(struct bview **);
	void checkbox_update();
	void view_refresh(struct bview **);
	void view_update_int(int);
	void view_update();

    private:
	struct bview **m_v = NULL;
};

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
