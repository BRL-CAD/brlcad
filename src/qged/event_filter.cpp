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
#include "qtcad/QgTreeSelectionModel.h"

/* We base conditionals on whether the target widget w is active.  Usually the
 * actual focus widget is a child of the widget in question, so we walk up the
 * parents to see if the focusWidget is underneath the target widget. */
static bool
widget_active(QWidget *w)
{
    QWidget *fw = qApp->focusWidget();
    QWidget *cw = fw;
    while (cw) {
	if (cw == w) {
	    return true;
	}
	cw = (QWidget *)cw->parent();
    }
    return false;
}

bool QGEDFilter::eventFilter(QObject *, QEvent *e)
{
    CADApp *c = (CADApp *)qApp;
    if (!c || !c->w)
	return false;

    // TODO - look into QShortcut, see if it might be a better way to manage this
    if (e->type() == QEvent::KeyPress) {
	// If we want to have key bindings that run GED commands, we will need
	// application level information - the view widget doesn't know about
	// the gedp.  To do this, we check if the central widget or one of its
	// children has the focus, and check if the key event is one of our
	// bound events.  If so, we may perform the bound action.
	QKeyEvent *k = (QKeyEvent *)e;
	if (k->modifiers().testFlag(Qt::ShiftModifier) == true && k->key() == 'N') {
	    if (!widget_active(c->w->c4))
		return false;
	    c->run_qcmd(QString("nirt -b"));
	    return true;
	}
	return false;
    }

    // All key binding handling should be above this point - anything below here is
    // mouse event only.
    //
    // Note:  It MIGHT be possible to use the above widget_active test approach
    // for the view/instance/primitive switching below, but it's not clear if
    // the tree highlighting updates would occur immediately or if we'd end up
    // needing two mouse events - one to change the focused widget, and another
    // event after the focus change to trigger this logic properly.  Don't know
    // without testing, not clear if it would be a better approach or just a
    // different one, and the below logic was rather tricky to get working -
    // given those factors, keeping the geometry based approach for this code.
    if (e->type() != QEvent::MouseButtonPress) {
	return false;
    }
    QMouseEvent *m_e = (QMouseEvent *)e;
    if (!c || !c->treeview)
	return false;
    QgTreeSelectionModel *tvsm = (QgTreeSelectionModel *)c->treeview->selectionModel();
    QWidget *vcp = c->w->vc->tpalette;
#ifdef USE_QT6
    QPoint gpos = m_e->globalPosition().toPoint();
#else
    QPoint gpos = m_e->globalPos();
#endif
    if (vcp) {
	QRect lrect = vcp->geometry();
	QPoint mpos = vcp->mapFromGlobal(gpos);
	if (lrect.contains(mpos) && tvsm->interaction_mode != 0) {
	    tvsm->mode_change(0);
	    emit c->view_update(QTCAD_VIEW_MODE);
	    return false;
	}
    }
    QWidget *ocp = c->w->oc->tpalette;
    if (ocp) {
	QRect lrect = ocp->geometry();
	QPoint mpos = ocp->mapFromGlobal(gpos);
	if (lrect.contains(mpos) && tvsm->interaction_mode != 2) {
	    tvsm->mode_change(2);
	    emit c->view_update(QTCAD_VIEW_MODE);
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
