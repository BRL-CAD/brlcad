/*                      Q T C A D Q U A D . C P P
 * BRL-CAD
 *
 * Copyright (c) 2021 United States Government as represented by
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
/** @file QtCADQuad.cpp
 *
 * TODO - initialize non-current views to the standard views (check MGED for
 * what the defaults should be.)  Also, need to implement an event filter for
 * this widget (I think there is an example in qged...)  Events should pass
 * through to the current selected widget when they are either key based or
 * take place with the xy coordinates matching the current widget.  However, a
 * mouse click over the quad widget but NOT located with xy coordinates over
 * the currently selected view should change the selected view (updating
 * gedp->ged_gvp, perhaps changing the background or some other visual
 * signature of which widget is currently active.
 *
 * One open question is whether the faceplate settings of the previous
 * selection should be copied/transferred to the new current selection (in
 * effect, making the faceplate settings independent of the specific view
 * selected.)  Maybe this should be a widget setting, since there are arguments
 * that could be made either way...  that we we wouldn't be locked into one
 * approach at the app level.
 *
 */

#include "common.h"

#include <QGridLayout>
#include "qtcad/QtCADQuad.h"

QtCADQuad::QtCADQuad(QWidget *parent, int type)
    : QWidget(parent)
{
    // Create the four view widgets
    ur = new QtCADView(this, type);
    ul = new QtCADView(this, type);
    ll = new QtCADView(this, type);
    lr = new QtCADView(this, type);

    ur->set_current(0);
    ul->set_current(0);
    ll->set_current(0);
    lr->set_current(0);

    // We'll need to do an event filter so we know which widget
    // is current
    ur->installEventFilter(this);
    ul->installEventFilter(this);
    ll->installEventFilter(this);
    lr->installEventFilter(this);

    // Define the spacers
    QSpacerItem *s_top = new QSpacerItem(3, 0, QSizePolicy::Fixed, QSizePolicy::Expanding);
    QSpacerItem *s_bottom = new QSpacerItem(3, 0, QSizePolicy::Fixed, QSizePolicy::Expanding);
    QSpacerItem *s_left = new QSpacerItem(0, 3, QSizePolicy::Expanding, QSizePolicy::Fixed);
    QSpacerItem *s_right = new QSpacerItem(0, 3, QSizePolicy::Expanding, QSizePolicy::Fixed);
    QSpacerItem *s_center = new QSpacerItem(3, 3, QSizePolicy::Fixed, QSizePolicy::Fixed);

    // Default to selecting quadrant 1
    c = ur;
    ur->set_current(1);

    // Lay out the widgets and spacers in a Quad View arrangement
    QGridLayout *gl = new QGridLayout(this);
    gl->setSpacing(0);
    gl->setContentsMargins(0, 0, 0, 0);
    gl->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    this->setLayout(gl);
    gl->addWidget(ul,     0, 0);
    gl->addItem(s_top,    0, 1);
    gl->addWidget(ur,     0, 2);
    gl->addItem(s_left,   1, 0);
    gl->addItem(s_center, 1, 1);
    gl->addItem(s_right,  1, 2);
    gl->addWidget(ll,     2, 0);
    gl->addItem(s_bottom, 2, 1);
    gl->addWidget(lr,     2, 2);

    // Hook up the wiring to detect view changes
    QObject::connect(ur, &QtCADView::changed, this, &QtCADQuad::do_view_changed);
    QObject::connect(ul, &QtCADView::changed, this, &QtCADQuad::do_view_changed);
    QObject::connect(ll, &QtCADView::changed, this, &QtCADQuad::do_view_changed);
    QObject::connect(lr, &QtCADView::changed, this, &QtCADQuad::do_view_changed);

}

QtCADQuad::~QtCADQuad()
{
    delete ur;
    delete ul;
    delete ll;
    delete lr;
}

void
QtCADQuad::do_view_changed()
{
    emit changed();
}

bool
QtCADQuad::isValid()
{
    if (!ur->isValid()) return false;
    if (!ul->isValid()) return false;
    if (!ll->isValid()) return false;
    if (!lr->isValid()) return false;
    return true;
}

void
QtCADQuad::fallback()
{
    ur->fallback();
    ul->fallback();
    ll->fallback();
    lr->fallback();

    ur->set_current(0);
    ul->set_current(0);
    ll->set_current(0);
    lr->set_current(0);

    // ur is still the default current
    ur->set_current(1);

}

bool
QtCADQuad::eventFilter(QObject *t, QEvent *e)
{
    if (e->type() == QEvent::KeyPress || e->type() == QEvent::MouseButtonPress) {
	QtCADView *oc = c;
	if (t == ur) {
	    c = ur;
	} else {
	    ur->set_current(0);
	}
	if (t == ul) {
	    c = ul;
	} else {
	    ul->set_current(0);
	}
	if (t == ll) {
	    c = ll;
	} else {
	    ll->set_current(0);
	}
	if (t == lr) {
	    c = lr;
	} else {
	    lr->set_current(0);
	}

	c->set_current(1);
	if (cv) {
	    (*cv) = c->view();
	    if (c != oc)
		emit selected(cv);
	}
    }
    return false;
}

void
QtCADQuad::default_views()
{
    ur->aet(35, 25, 0);
    ul->aet(0, 90, 0);
    ll->aet(0, 0, 0);
    lr->aet(90, 0, 0);
}

QtCADView *
QtCADQuad::get(int quadrant_id)
{
    switch (quadrant_id) {
	case 1:
	    return ur;
	case 2:
	    return ul;
	case 3:
	    return ll;
	case 4:
	    return lr;
	default:
	    return c;
    }
}

void
QtCADQuad::select(int quadrant_id)
{
    QtCADView *oc = c;
    switch (quadrant_id) {
	case 1:
	    c = ur;
	    break;
	case 2:
	    c = ul;
	    break;
	case 3:
	    c = ll;
	    break;
	case 4:
	    c = lr;
	    break;
	default:
	    return;
    }

    if (cv) {
	(*cv) = c->view();
	if (oc != c)
	    emit selected(cv);
    }
    // TODO - update coloring of bg to
    // indicate active quadrant

}

void
QtCADQuad::select(const char *quadrant_id)
{
    if (BU_STR_EQUIV(quadrant_id, "ur")) {
	select(1);
	return;
    }
    if (BU_STR_EQUIV(quadrant_id, "ul")) {
	select(2);
	return;
    }
    if (BU_STR_EQUIV(quadrant_id, "ll")) {
	select(3);
	return;
    }
    if (BU_STR_EQUIV(quadrant_id, "lr")) {
	select(4);
	return;
    }
}

void
QtCADQuad::need_update(void *)
{
    ur->update();
    ul->update();
    ll->update();
    lr->update();
}

void
QtCADQuad::stash_hashes()
{
    ur->stash_hashes();
    ul->stash_hashes();
    ll->stash_hashes();
    lr->stash_hashes();
}

bool
QtCADQuad::diff_hashes()
{
    bool ur_ret = ur->diff_hashes();
    bool ul_ret = ul->diff_hashes();
    bool ll_ret = ll->diff_hashes();
    bool lr_ret = lr->diff_hashes();

    if (ur_ret || ul_ret || ll_ret || lr_ret)
	return true;

    return false;
}

// TODO - need to enable mouse selection
// of a quadrant as current

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8

