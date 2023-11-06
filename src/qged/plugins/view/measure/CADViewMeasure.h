/*                  C A D V I E W M E A S U R E . H
 * BRL-CAD
 *
 * Copyright (c) 2014-2023 United States Government as represented by
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
/** @file CADViewMeasure.h
 *
 */

#include <QWidget>
#include <QGroupBox>
#include <QCheckBox>
#include <QLabel>
#include <QLineEdit>
#include <QLineEdit>
#include "bv.h"
#include "ged.h"
#include "qtcad/QgColorRGB.h"
#include "qtcad/QgMeasureFilter.h"

class CADViewMeasure : public QWidget
{
    Q_OBJECT

    public:
	CADViewMeasure(QWidget *p = 0);
	~CADViewMeasure();

	QCheckBox *measure_3d;

	QLineEdit *length1_report;
	QLineEdit *length2_report;

	QCheckBox *report_radians;
	QLabel *ma_label;
	QLineEdit *angle_report;

	QgColorRGB *color_2d;
	QgColorRGB *color_3d;

    signals:
	void view_updated(unsigned long long);

    public slots:
        void adjust_text();
        void adjust_text_db(void *);
	void do_filter_view_update();

    private slots:
	void update_color();

    protected:
	bool eventFilter(QObject *, QEvent *);

    private:
	struct bv_scene_obj *s = NULL;
	QgMeasureFilter *mf = NULL;
	QMeasure2DFilter *f2d = NULL;
	QMeasure3DFilter *f3d = NULL;
};

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
