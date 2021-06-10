/*                E V E N T _ F I L T E R . C P P
 * BRL-CAD
 *
 * Copyright (c) 2020-2021 United States Government as represented by
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
/** @file event_filter.cpp
 *
 *
 */

#include <iostream>
#include <QEvent>
#include <QMouseEvent>
#include "app.h"
#include "event_filter.h"

bool EditStateFilter::eventFilter(QObject *, QEvent *e)
{
    if (e->type() != QEvent::MouseButtonPress) {
	return false;
    }
    QMouseEvent *m_e = (QMouseEvent *)e;
    CADApp *c = (CADApp *)qApp;
    if (!c || !c->w)
	return false;
    QWidget *vcp = c->w->vc->tpalette;
    CADViewControls *v = c->w->vc;
    if (vcp) {
	QRect lrect = vcp->geometry();
	QPoint gpos = m_e->globalPos();
	QPoint mpos = vcp->mapFromGlobal(gpos);
	if (lrect.contains(mpos)) {
	    emit v->current(v);
	    return false;
	}
    }
    QWidget *icp = c->w->ic->tpalette;
    CADInstanceEdit *ed = c->w->ic;
    if (icp) {
	QRect lrect = icp->geometry();
	QPoint gpos = m_e->globalPos();
	QPoint mpos = icp->mapFromGlobal(gpos);
	if (lrect.contains(mpos)) {
	    emit ed->current(ed);
	    return false;
	}
    }
    QWidget *ocp = c->w->oc->tpalette;
    CADPrimitiveEdit *pd = c->w->oc;
    if (ocp) {
	QRect lrect = ocp->geometry();
	QPoint gpos = m_e->globalPos();
	QPoint mpos = ocp->mapFromGlobal(gpos);
	if (lrect.contains(mpos)) {
	    emit pd->current(pd);
	    return false;
	}
    }

    QWidget *cw = c->w->console;
    if (cw) {
	QRect lrect = cw->geometry();
	QPoint gpos = m_e->globalPos();
	QPoint mpos = cw->mapFromGlobal(gpos);
	if (lrect.contains(mpos)) {
	    // Clear all selections if we click in the console
	    v->makeCurrent(NULL);
	    ed->makeCurrent(NULL);
	    pd->makeCurrent(NULL);
	    return false;
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
