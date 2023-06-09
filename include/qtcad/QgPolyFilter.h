/*                    Q G P O L Y F I L T E R . H
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
/** @file QgPolyFilter.h
 *
 *  Qt mouse filters for polygon editing modes
 */

#ifndef QGPOLYFILTER_H
#define QGPOLYFILTER_H

#include "common.h"

extern "C" {
#include "bu/ptbl.h"
#include "bg/polygon.h"
#include "bv.h"
}

#include <QBoxLayout>
#include <QEvent>
#include <QMouseEvent>
#include <QObject>
#include <QWidget>
#include "qtcad/defines.h"

// Filters designed for specific editing modes
class QTCAD_EXPORT QgPolyFilter : public QObject
{
    Q_OBJECT

    public:
	// Initialization common to the various polygon filter types
	QMouseEvent *view_sync(QEvent *e);

	// We want to be able to swap derived QgPolyFilter classes in
	// parent calling code - make eventFilter virtual to help
	// simplify doing so.
	virtual bool eventFilter(QObject *, QEvent *) { return false; };

    signals:
        void view_updated(int);
        void finalized(bool);

    public:
	bool close_polygon();

	struct bview *v = NULL;
	bg_clip_t op = bg_None;
	struct bv_scene_obj *wp = NULL;
	int ptype = BV_POLYGON_CIRCLE;
	bool close_general_poly = true; // set to false if application wants to allow non-closed polygons
	struct bu_color fill_color = BU_COLOR_BLUE;
	struct bu_color edge_color = BU_COLOR_YELLOW;
	bool fill_poly = false;
	double fill_slope_x = 1.0;
	double fill_slope_y = 1.0;
	double fill_density = 10.0;
	double vZ = 0.0;
	std::string vname;
};

class QTCAD_EXPORT QPolyCreateFilter : public QgPolyFilter
{
    Q_OBJECT

    public:
	bool eventFilter(QObject *, QEvent *e);
	void finalize(bool);

	struct bu_ptbl bool_objs = BU_PTBL_INIT_ZERO;
};

class QTCAD_EXPORT QPolyUpdateFilter : public QgPolyFilter
{
    Q_OBJECT

    public:
	bool eventFilter(QObject *, QEvent *e);

	struct bu_ptbl bool_objs = BU_PTBL_INIT_ZERO;
};

class QTCAD_EXPORT QPolySelectFilter : public QgPolyFilter
{
    Q_OBJECT

    public:
	bool eventFilter(QObject *, QEvent *e);
};

class QTCAD_EXPORT QPolyPointFilter : public QgPolyFilter
{
    Q_OBJECT

    public:
	bool eventFilter(QObject *, QEvent *e);
};

class QTCAD_EXPORT QPolyMoveFilter : public QgPolyFilter
{
    Q_OBJECT

    public:
	bool eventFilter(QObject *, QEvent *e);
	struct bu_ptbl move_objs = BU_PTBL_INIT_ZERO;
};


#endif /* QGPOLYFILTER_H */

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8

