/*                 V I E W _ W I D G E T . C P P
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
/** @file view_widget.cpp
 *
 */

#include "common.h"
#include <QMouseEvent>
#include <QVBoxLayout>
#include "../../../app.h"

#include "bu/opt.h"
#include "bu/malloc.h"
#include "bu/str.h"
#include "bg/aabb_ray.h"
#include "bg/plane.h"
#include "bg/lod.h"

#include "./widget.h"

CADViewMeasure::CADViewMeasure(QWidget *)
{
    QVBoxLayout *wl = new QVBoxLayout;
    wl->setAlignment(Qt::AlignTop);

    measure_3d = new QCheckBox("Use 3D hit points");
    wl->addWidget(measure_3d);

    this->setLayout(wl);
}

CADViewMeasure::~CADViewMeasure()
{
}

bool
CADViewMeasure::eventFilter(QObject *, QEvent *e)
{
    QgSelectionProxyModel *mdl = ((CADApp *)qApp)->mdl;
    if (!mdl)
	return false;

    QgModel *m = (QgModel *)mdl->sourceModel();
    struct ged *gedp = m->gedp;
    if (!gedp || !gedp->ged_gvp)
	return false;
    struct bview *v = gedp->ged_gvp;

    if (e->type() != QEvent::MouseButtonPress && e->type() != QEvent::MouseMove && e->type() != QEvent::MouseButtonRelease)
	return false;


    QMouseEvent *m_e = (QMouseEvent *)e;

    // If any other keys are down, we're not doing an erase
    if (m_e->modifiers() != Qt::NoModifier) {
	bu_log("Have modifier\n");
	return false;
    }

    fastf_t vx, vy;
#ifdef USE_QT6
    vx = m_e->position().x();
    vy = m_e->position().y();
#else
    vx = m_e->x();
    vy = m_e->y();
#endif
    int x = (int)vx;
    int y = (int)vy;
    bv_screen_to_view(v, &vx, &vy, x, y);

    point_t vpnt, mpnt;
    VSET(vpnt, vx, vy, 0);
    MAT4X3PNT(mpnt, v->gv_view2model, vpnt);



    if (e->type() == QEvent::MouseButtonPress) {
	bu_log("Mouse press: %d %d (%f %f %f)\n", x, y, V3ARGS(mpnt));
	return true;
    }

    if (e->type() == QEvent::MouseMove) {
	bu_log("Mouse move: %d %d (%f %f %f)\n", x, y, V3ARGS(mpnt));
	return true;
    }

    if (e->type() == QEvent::MouseButtonRelease) {
	bu_log("Mouse release: %d %d (%f %f %f)\n", x, y, V3ARGS(mpnt));
	return true;
    }

    // Shouldn't get here
    return false;
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
