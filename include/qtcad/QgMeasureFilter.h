/*                  Q G M E A S U R E F I L T E R . H
 * BRL-CAD
 *
 * Copyright (c) 2021-2023 United States Government as represented by
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
/** @file QgMeasureFilter.h
 *
 *  Qt mouse filters for 2D and 3D dimensional measurement
 *  in a view.
 */

#ifndef QGMEASUREFILTER_H
#define QGMEASUREFILTER_H

#include "common.h"

extern "C" {
#include "bu/color.h"
#include "bu/ptbl.h"
#include "bg/polygon.h"
#include "bv.h"
#include "raytrace.h"
}

#include <QBoxLayout>
#include <QEvent>
#include <QMouseEvent>
#include <QObject>
#include <QWidget>
#include "qtcad/defines.h"

// Filters designed for specific editing modes
class QTCAD_EXPORT QgMeasureFilter : public QObject
{
    Q_OBJECT

    public:
	// Primary mouse interaction.  As it happens the 2D and 3D mouse event
	// filtering is the same, so this is not a virtual function.  See
	// get_point for the 2D/3D specific logic.
	bool eventFilter(QObject *, QEvent *);

	// Initialization common to the various polygon filter types
	QMouseEvent *view_sync(QEvent *e);

	double length1();
	double length2();
	double angle(bool radians);

	// Current state of the measurement tool (clients may need to
	// know which information to report)
	// 0 == uninitialized
	// 1 = length measurement in progress
	// 2 = length measurement complete
	// 3 = angle measurement in progress
	// 4 = angle measurement complete
	int mode = 0;

	// 2D and 3D point interrogation is different - hence this
	// is a virtual method to be customized for 2D and 3D
	virtual bool get_point() { return false; };
	point_t mpnt;

	// If the client code only wants a simple length measurement,
	// without the follow-on behavior of a second angle measuring
	// line, setting this variable to true will alter the mouse
	// binding behavior accordingly.
	bool length_only = false;

    signals:
        void view_updated(int);

    public:
	struct bview *v = NULL;
	struct bv_scene_obj *s = NULL;
	std::string oname = std::string("tool:measurement");

    public slots:
	void update_color(struct bu_color *);

    private:
	point_t p1 = VINIT_ZERO;
	point_t p2 = VINIT_ZERO;
	point_t p3 = VINIT_ZERO;
};

class QTCAD_EXPORT QMeasure2DFilter : public QgMeasureFilter
{
    Q_OBJECT

    public:
	bool eventFilter(QObject *, QEvent *e);

    private:
	bool get_point();
};


class QTCAD_EXPORT QMeasure3DFilter : public QgMeasureFilter
{
    Q_OBJECT

    public:
	QMeasure3DFilter();
	~QMeasure3DFilter();
	bool eventFilter(QObject *, QEvent *e);
	struct db_i *dbip = NULL;

    private:
	bool get_point();

	int prev_cnt = 0;
	struct bu_ptbl scene_obj_set = BU_PTBL_INIT_ZERO;
	struct application *ap = NULL;
	struct rt_i *rtip = NULL;
	struct resource *resp = NULL;
};

#endif /* QGMEASUREFILTER_H */

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8

