/*                  Q G M E A S U R E F I L T E R . H
 * BRL-CAD
 *
 * Copyright (c) 2021-2026 United States Government as represented by
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
#include "bg/polygon.h"
#include "bsg.h"
#include "raytrace.h"
}

#include <string>
#include <vector>
#include <QBoxLayout>
#include <QEvent>
#include <QMouseEvent>
#include <QWidget>
#include "qtcad/defines.h"
#include "qtcad/QgViewFilter.h"

// Filters designed for specific editing modes
class QTCAD_EXPORT QgMeasureFilter : public QgViewFilter {
	Q_OBJECT
	Q_DISABLE_COPY_MOVE(QgMeasureFilter)


    public:
	QgMeasureFilter() = default;
	// Primary mouse interaction.  As it happens the 2D and 3D mouse event
	// filtering is the same, so this is not a virtual function.  See
	// get_point for the 2D/3D specific logic.
	bool eventFilter(QObject *, QEvent *) override;

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
	virtual bool get_point()
	{
		return false;
	};
	point_t mpnt;

	// If the client code only wants a simple length measurement,
	// without the follow-on behavior of a second angle measuring
	// line, setting this variable to true will alter the mouse
	// binding behavior accordingly.
	bool length_only = false;

public:
	bsg_feature_ref feature = BSG_FEATURE_REF_NULL_INIT;
	std::string oname = std::string("tool:measurement");

	/* Measure results populated via bsg_measure_candidates when each
	 * segment is finalized.  mr12 covers p1→p2, mr23 covers p2→p3. */
	struct bsg_measure_result mr12 = {0.0, 0.0, 0.0, 0};
	struct bsg_measure_result mr23 = {0.0, 0.0, 0.0, 0};

public slots:
	void update_color(struct bu_color *);

private:
	point_t p1 = VINIT_ZERO;
	point_t p2 = VINIT_ZERO;
	point_t p3 = VINIT_ZERO;
};

class QTCAD_EXPORT QMeasure2DFilter : public QgMeasureFilter {
	Q_OBJECT
	Q_DISABLE_COPY_MOVE(QMeasure2DFilter)


public:
	QMeasure2DFilter() = default;
	bool eventFilter(QObject *, QEvent *e) override;

private:
	bool get_point() override;
};


class QTCAD_EXPORT QMeasure3DFilter : public QgMeasureFilter {
	Q_OBJECT
	Q_DISABLE_COPY_MOVE(QMeasure3DFilter)


public:
	QMeasure3DFilter();
	~QMeasure3DFilter();
	bool eventFilter(QObject *, QEvent *e) override;
	struct db_i *dbip = nullptr;

private:
	bool get_point() override;

	int prev_cnt = 0;
	std::vector<std::string> scene_obj_paths;
	struct application *ap = nullptr;
	struct rt_i *rtip = nullptr;
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
