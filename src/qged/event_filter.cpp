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

// TODO - the emission of the events from the filter seems
// to be interfering with widget interactions in the palettes.
// Need to find another way to achieve this...
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
    QPoint gpos = m_e->globalPos();
    if (vcp) {
	QRect lrect = vcp->geometry();
	QPoint mpos = vcp->mapFromGlobal(gpos);
	if (lrect.contains(mpos)) {
	    //CADPalette *v = c->w->vc;
	    //emit v->current(v);
	    return false;
	}
    }
    QWidget *icp = c->w->ic->tpalette;
    if (icp) {
	QRect lrect = icp->geometry();
	QPoint mpos = icp->mapFromGlobal(gpos);
	if (lrect.contains(mpos)) {
	    //CADPalette *ed = c->w->ic;
	    //emit ed->current(ed);
	    return false;
	}
    }
    QWidget *ocp = c->w->oc->tpalette;
    if (ocp) {
	QRect lrect = ocp->geometry();
	QPoint mpos = ocp->mapFromGlobal(gpos);
	if (lrect.contains(mpos)) {
	    //CADPalette *pd = c->w->oc;
	    //emit pd->current(pd);
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
