/*                E V E N T _ F I L T E R . C P P
 * BRL-CAD
 *
 * Copyright (c) 2020-2022 United States Government as represented by
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

#include "common.h"
#include <iostream>
#include <QTimer>
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
#ifdef USE_QT6
    QPoint gpos = m_e->globalPosition().toPoint();
#else
    QPoint gpos = m_e->globalPos();
#endif
    if (vcp) {
	QRect lrect = vcp->geometry();
	QPoint mpos = vcp->mapFromGlobal(gpos);
	if (lrect.contains(mpos) && c->interaction_mode != 0) {
	    c->prev_interaction_mode = c->interaction_mode;
	    c->interaction_mode = 0;
	    QTimer::singleShot(0, c, &CADApp::tree_update);
	    return false;
	}
    }
    QWidget *icp = c->w->ic->tpalette;
    if (icp) {
	QRect lrect = icp->geometry();
	QPoint mpos = icp->mapFromGlobal(gpos);
	if (lrect.contains(mpos) && c->interaction_mode != 1) {
	    c->prev_interaction_mode = c->interaction_mode;
	    c->interaction_mode = 1;
	    QTimer::singleShot(0, c, &CADApp::tree_update);
	    return false;
	}
    }
    QWidget *ocp = c->w->oc->tpalette;
    if (ocp) {
	QRect lrect = ocp->geometry();
	QPoint mpos = ocp->mapFromGlobal(gpos);
	if (lrect.contains(mpos) && c->interaction_mode != 2) {
	    c->prev_interaction_mode = c->interaction_mode;
	    c->interaction_mode = 2;
	    QTimer::singleShot(0, c, &CADApp::tree_update);
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
