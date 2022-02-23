/*                      Q T C A D Q U A D . C P P
 * BRL-CAD
 *
 * Copyright (c) 2021-2022 United States Government as represented by
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

#include "bu/str.h"
#include "ged/defines.h"
#include "ged/commands.h"
#include "qtcad/QtCADQuad.h"

static const int UPPER_RIGHT = 0;
static const int UPPER_LEFT = 1;
static const int LOWER_LEFT = 2;
static const int LOWER_RIGHT = 3;

static const char *VIEW_NAMES[] = {"Q1", "Q2", "Q3", "Q4"};

/**
 * @brief Construct a new Qt C A D Quad:: Qt C A D Quad object
 *
 * @param parent   Parent Qt widget
 * @param gedpRef  Associated GED struct
 * @param type     Requesting either a GL or SWRAST display mechanism
 */
QtCADQuad::QtCADQuad(QWidget *parent, struct ged *gedpRef, int type) : QWidget(parent)
{
    gedp = gedpRef;
    graphicsType = type;

    views[UPPER_RIGHT] = createView(UPPER_RIGHT);
    bu_ptbl_ins_unique(&gedp->ged_views, (long int *)views[UPPER_RIGHT]->view());
    gedp->ged_gvp = views[UPPER_RIGHT]->view();

    // Define the spacers
    spacerTop = new QSpacerItem(3, 0, QSizePolicy::Fixed, QSizePolicy::Expanding);
    spacerBottom = new QSpacerItem(3, 0, QSizePolicy::Fixed, QSizePolicy::Expanding);
    spacerLeft = new QSpacerItem(0, 3, QSizePolicy::Expanding, QSizePolicy::Fixed);
    spacerRight = new QSpacerItem(0, 3, QSizePolicy::Expanding, QSizePolicy::Fixed);
    spacerCenter = new QSpacerItem(3, 3, QSizePolicy::Fixed, QSizePolicy::Fixed);

    views[UPPER_RIGHT]->set_current(1);
    currentView = views[UPPER_RIGHT];

    QObject::connect(views[UPPER_RIGHT], &QtCADView::changed, this, &QtCADQuad::do_view_changed);
}

QtCADQuad::~QtCADQuad()
{
    for (int i = 0; i < 4; i++) {
	if (views[i] != nullptr) {
	    delete views[i];
	    views[i] = nullptr;
	}
    }

    delete spacerTop;
    delete spacerBottom;
    delete spacerLeft;
    delete spacerRight;
    delete spacerCenter;
}

/**
 * @brief Creates a view for the viewport. Convenience method of common things that need to be done to the view.
 *
 * @param index of the view names to use from the constant list of names
 * @return QtCADView*
 */
QtCADView *
QtCADQuad::createView(int index)
{
    QtCADView *view = new QtCADView(this, graphicsType);
    bu_vls_sprintf(&view->view()->gv_name, "%s", VIEW_NAMES[index]);
    view->set_current(0);
    view->installEventFilter(this);

    view->view()->gv_db_grps = &gedp->ged_db_grps;
    view->view()->gv_view_shared_objs = &gedp->ged_view_shared_objs;
    view->view()->independent = 0;

    return view;
}

/**
 * @brief Convenience method to create the layout and set common parameters.
 *
 * @return QGridLayout*
 */
QGridLayout *
QtCADQuad::createLayout()
{
    QGridLayout *layout = new QGridLayout(this);
    layout->setSpacing(0);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setAlignment(Qt::AlignTop | Qt::AlignLeft);

    this->setLayout(layout);

    if (currentLayout != nullptr) {
	delete currentLayout;
    }
    currentLayout = layout;

    return layout;
}

/**
 * @brief Changes the viewport layout to only have the single view.  We destroy the other views if needed and the flag is set
 *
 */
void
QtCADQuad::changeToSingleFrame()
{
    QGridLayout *layout = (QGridLayout *)this->layout();
    if (layout == nullptr) {
	layout = createLayout();
    }
    while (layout->takeAt(0) != NULL);
    layout->addWidget(views[UPPER_RIGHT], 0, 2);

    for (int i = 1; i < 4; i++) {
	// Don't want use cpu for views that are not visible
	if (views[i] != nullptr) {
	    views[i]->disconnect();
	    bu_ptbl_rm(&gedp->ged_views, (long int *)(views[i]->view()));
	    delete views[i];
	    views[i] = nullptr;
	}
    }

    views[UPPER_RIGHT]->set_current(1);
    currentView = views[UPPER_RIGHT];

    default_views();
}

/**
 * @brief Changes the viewport layout to have 4 views the views will be equal size and not resizeable.  This will create the extra view if needed.
 *
 */
void
QtCADQuad::changeToQuadFrame()
{
    for (int i = 1; i < 4; i++) {
	if (views[i] == nullptr) {
	    views[i] = createView(i);
	}
	bu_ptbl_ins_unique(&gedp->ged_views, (long int *)views[i]->view());
	QObject::connect(views[i], &QtCADView::changed, this, &QtCADQuad::do_view_changed);
    }
    QGridLayout *layout = (QGridLayout *)this->layout();
    if (layout == nullptr) {
	layout = createLayout();
    }
    while (layout->takeAt(0) != NULL);

    layout->addWidget(views[UPPER_LEFT], 0, 0);
    layout->addItem(spacerTop, 0, 1);
    layout->addWidget(views[UPPER_RIGHT], 0, 2);
    layout->addItem(spacerLeft, 1, 0);
    layout->addItem(spacerCenter, 1, 1);
    layout->addItem(spacerRight, 1, 2);
    layout->addWidget(views[LOWER_LEFT], 2, 0);
    layout->addItem(spacerBottom, 2, 1);
    layout->addWidget(views[LOWER_RIGHT], 2, 2);

    default_views();

    // Not sure if this is the right way to do this but need to autoset each of the views
    const char *av[2];
    av[0] = "autoview";
    av[1] = (char *)0;
    for (int i = 1; i < 4; i++) {
	gedp->ged_gvp = views[i]->view();
	ged_exec(gedp, 1, (const char **)av);
    }
    gedp->ged_gvp = views[UPPER_RIGHT]->view();
    views[UPPER_RIGHT]->set_current(1);
    currentView = views[UPPER_RIGHT];
}

void
QtCADQuad::do_view_changed()
{
    emit changed();
}

bool
QtCADQuad::isValid()
{
    for (int i = 0; i < 4; i++) {
	if (views[i] != nullptr && !views[i]->isValid())
	    return false;
    }
    return true;
}

void
QtCADQuad::fallback()
{
    for (int i = 0; i < 4; i++) {
	if (views[i] != nullptr) {
	    views[i]->fallback();

	    views[i]->set_current(0);
	}
    }

    // ur is still the default current
    views[UPPER_RIGHT]->set_current(1);
    currentView = views[UPPER_RIGHT];
}

bool
QtCADQuad::eventFilter(QObject *t, QEvent *e)
{
    if (e->type() == QEvent::KeyPress || e->type() == QEvent::MouseButtonPress) {
	QtCADView *oc = currentView;
	for (int i = 0; i < 4; i++) {
	    if (views[i] != nullptr && t == views[i]) {
		currentView = views[i];
	    }
	    else {
		if (views[i] != nullptr) {
		    views[i]->set_current(0);
		}
	    }
	}

	currentView->set_current(1);
	gedp->ged_dmp = currentView->view()->dmp;
	if (currentView != oc)
	    emit selected(currentView);
    }
    return false;
}

void
QtCADQuad::default_views()
{
    if (views[UPPER_RIGHT] != nullptr) {
	if (views[UPPER_LEFT] == nullptr) {
	    views[UPPER_RIGHT]->aet(270, 90, 0);
	}
	else {
	    views[UPPER_RIGHT]->aet(35, 25, 0);
	}
    }
    if (views[UPPER_LEFT] != nullptr) {
	views[UPPER_LEFT]->aet(0, 90, 0);
    }
    if (views[LOWER_LEFT] != nullptr) {
	views[LOWER_LEFT]->aet(0, 0, 0);
    }
    if (views[LOWER_RIGHT] != nullptr) {
	views[LOWER_RIGHT]->aet(90, 0, 0);
    }
}

struct bview *
QtCADQuad::view(int quadrantId)
{
    if (quadrantId > 0) quadrantId -= 1;

    if (views[quadrantId] != nullptr) {
	return views[quadrantId]->view();
    }

    return currentView->view();
}

QtCADView *
QtCADQuad::get(int quadrantId)
{
    if (quadrantId > 0) quadrantId -= 1;

    if (views[quadrantId] != nullptr) {
	return views[quadrantId];
    }

    return currentView;
}

void
QtCADQuad::select(int quadrantId)
{
    if (quadrantId > 0) quadrantId -= 1;

    QtCADView *oc = currentView;

    if (views[quadrantId] != nullptr) {
	currentView = views[quadrantId];
    }

    if (oc != currentView) {
	emit selected(currentView);
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
    for (int i = 0; i < 4; i++) {
	if (views[i] != nullptr) {
	    views[i]->update();
	}
    }
}

void
QtCADQuad::stash_hashes()
{
    for (int i = 0; i < 4; i++) {
	if (views[i] != nullptr) {
	    views[i]->stash_hashes();
	}
    }
}

bool
QtCADQuad::diff_hashes()
{
    bool ret = false;
    for (int i = 0; i < 4; i++) {
	if (views[i] != nullptr) {
	    if (views[i]->diff_hashes()) {
		ret = true;
	    }
	}
    }

    return ret;
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
