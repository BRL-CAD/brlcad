/*         P O L Y G O N _ C I R C L E _ C O N T R O L . C P P
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
/** @file polygon_circle_control.cpp
 *
 */

#include "../../app.h"
#include "polygon_circle_control.h"

QCirclePolyControl::QCirclePolyControl(QString s)
    : QPushButton(s)
{
    setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
}

QCirclePolyControl::~QCirclePolyControl()
{
}

bool
QCirclePolyControl::eventFilter(QObject *, QEvent *e)
{
    printf("polygon circle filter\n");

    struct ged *gedp = ((CADApp *)qApp)->gedp;
    if (!gedp) {
	return false;
    }

    if (e->type() == QEvent::MouseButtonPress || e->type() == QEvent::MouseButtonRelease || e->type() == QEvent::MouseButtonDblClick || e->type() == QEvent::MouseMove) {

	QMouseEvent *m_e = (QMouseEvent *)e;

	if (m_e->type() == QEvent::MouseButtonPress && m_e->buttons().testFlag(Qt::LeftButton)) {
	    if (!p) {
		p = bv_create_polygon(gedp->ged_gvp, BV_POLYGON_CIRCLE, m_e->x(), m_e->y(), gedp->free_scene_obj);
		struct bu_vls pname = BU_VLS_INIT_ZERO;
		cpoly_cnt++;
		bu_vls_sprintf(&pname, "circle_polygon_%6d\n", cpoly_cnt);
		bu_vls_init(&p->s_uuid);
		bu_vls_printf(&p->s_uuid, "%s", bu_vls_cstr(&pname));
		bu_vls_free(&pname);
		bu_ptbl_ins(gedp->ged_gvp->gv_view_objs, (long *)p);
		emit view_updated(&gedp->ged_gvp);
		return true;
	    }
	}

	if (m_e->type() == QEvent::MouseMove) {
	    if (p && m_e->buttons().testFlag(Qt::LeftButton) && m_e->modifiers() == Qt::NoModifier) {
		p->s_changed = 0;
		p->s_v = gedp->ged_gvp;
		(*p->s_update_callback)(p);
		emit view_updated(&gedp->ged_gvp);
		return true;
	    }
	}

	if (m_e->type() == QEvent::MouseButtonRelease && m_e->buttons().testFlag(Qt::LeftButton)) {
	    p = NULL;
	    emit view_updated(&gedp->ged_gvp);
	    return true;
	}
    }

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
