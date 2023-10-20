/*                  Q G S E L E C T F I L T E R . H
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
 *  Qt mouse filters for graphically selecting elements in a view.
 */

#ifndef QGSELECTFILTER_H
#define QGSELECTFILTER_H

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
class QTCAD_EXPORT QgSelectFilter : public QObject
{
    Q_OBJECT

    public:
	// Primary mouse interaction.  This differs a bit for the
	// various selection types, hence the virtual definition
	// in the base class.
	virtual bool eventFilter(QObject *, QEvent *) { return false; }

	// Recover info from view (common logic for all selection modes)
	QMouseEvent *view_sync(QEvent *e);

	struct bu_ptbl selected_set = BU_PTBL_INIT_ZERO;

	struct bview *v = NULL;

	// Whenever we're doing selections, we may want either all the objects
	// that match the selection criteria, or just the "closest" object.
	// Default behavior is to return everything, but this flag allows the
	// caller to request the more limited result as well.
	bool first_only = false;

    signals:
        void view_updated(int);
};

class QTCAD_EXPORT QgSelectPntFilter: public QgSelectFilter
{
    Q_OBJECT

    public:
	bool eventFilter(QObject *, QEvent *e);
};

class QTCAD_EXPORT QgSelectBoxFilter: public QgSelectFilter
{
    Q_OBJECT

    public:
	bool eventFilter(QObject *, QEvent *e);

    private:
	fastf_t px = -FLT_MAX;
	fastf_t py = -FLT_MAX;
};

class QTCAD_EXPORT QgSelectRayFilter: public QgSelectFilter
{
    Q_OBJECT

    public:
	bool eventFilter(QObject *, QEvent *e);

	struct db_i *dbip = NULL;
};

#endif /* QGSELECTFILTER_H */

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8

