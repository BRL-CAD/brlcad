/*                     W I D G E T . H
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
/** @file view_widget.h
 *
 */

#include <QWidget>
#include <QGroupBox>
#include <QCheckBox>
#include <QLabel>
#include <QLineEdit>
#include "ged.h"

class CADViewMeasure : public QWidget
{
    Q_OBJECT

    public:
	CADViewMeasure(QWidget *p = 0);
	~CADViewMeasure();

	QCheckBox *measure_3d;

    signals:
	void view_updated(struct bview **);

    protected:
	bool eventFilter(QObject *, QEvent *);

    private:
	struct bv_scene_obj *s = NULL;
	int mode = 0;  // 0 == uninitialized, 1 = length measurement in progress, 2 = length measurement complete, 3 = angle measurement in progress, 4 = angle measurement complete
	point_t p1;
	point_t p2;
	point_t p3;
	bool enabled = true;
};

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
