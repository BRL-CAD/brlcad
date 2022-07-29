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
#include "bv.h"
#include "ged/defines.h"
#include "ged/commands.h"
#include "qtcad/QtCADQuad.h"

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

    views[UPPER_RIGHT_QUADRANT] = createView(UPPER_RIGHT_QUADRANT);
    bv_set_add_view(&gedp->ged_views, views[UPPER_RIGHT_QUADRANT]->view());
    gedp->ged_gvp = views[UPPER_RIGHT_QUADRANT]->view();

    // Define the spacers
    spacerTop = new QSpacerItem(3, 0, QSizePolicy::Fixed, QSizePolicy::Expanding);
    spacerBottom = new QSpacerItem(3, 0, QSizePolicy::Fixed, QSizePolicy::Expanding);
    spacerLeft = new QSpacerItem(0, 3, QSizePolicy::Expanding, QSizePolicy::Fixed);
    spacerRight = new QSpacerItem(0, 3, QSizePolicy::Expanding, QSizePolicy::Fixed);
    spacerCenter = new QSpacerItem(3, 3, QSizePolicy::Fixed, QSizePolicy::Fixed);

    views[UPPER_RIGHT_QUADRANT]->set_current(1);
    currentView = views[UPPER_RIGHT_QUADRANT];

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
QtCADQuad::createView(unsigned int index)
{
    QtCADView *view = new QtCADView(this, graphicsType);
    bu_vls_sprintf(&view->view()->gv_name, "%s", VIEW_NAMES[index]);
    view->set_current(0);
    view->installEventFilter(this);

    view->view()->vset = &gedp->ged_views;
    view->view()->independent = 0;

    QObject::connect(view, &QtCADView::changed, this, &QtCADQuad::do_view_changed);
    QObject::connect(view, &QtCADView::init_done, this, &QtCADQuad::do_init_done);
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
    layout->addWidget(views[UPPER_RIGHT_QUADRANT], 0, 2);

    for (int i = 1; i < 4; i++) {
	// Don't want use cpu for views that are not visible
	if (views[i] != nullptr) {
	    views[i]->disconnect();
	    bv_set_rm_view(&gedp->ged_views, views[i]->view());
	    delete views[i];
	    views[i] = nullptr;
	}
    }

    views[UPPER_RIGHT_QUADRANT]->set_current(1);
    currentView = views[UPPER_RIGHT_QUADRANT];
    // This is only used in quad mode
    currentView->select(1);

    default_views();
}

/**
 * @brief Changes the viewport layout to have 4 views the views will be equal size and not resizeable.  This will create the extra view if needed.
 *
 */
void
QtCADQuad::changeToQuadFrame()
{
    for (int i = UPPER_RIGHT_QUADRANT + 1; i < LOWER_RIGHT_QUADRANT + 1; i++) {
	if (views[i] == nullptr) {
	    views[i] = createView(i);
	}
	bv_set_add_view(&gedp->ged_views, views[i]->view());
    }
    QGridLayout *layout = (QGridLayout *)this->layout();
    if (layout == nullptr) {
	layout = createLayout();
    }
    while (layout->takeAt(0) != NULL);

    layout->addWidget(views[UPPER_LEFT_QUADRANT], 0, 0);
    layout->addItem(spacerTop, 0, 1);
    layout->addWidget(views[UPPER_RIGHT_QUADRANT], 0, 2);
    layout->addItem(spacerLeft, 1, 0);
    layout->addItem(spacerCenter, 1, 1);
    layout->addItem(spacerRight, 1, 2);
    layout->addWidget(views[LOWER_LEFT_QUADRANT], 2, 0);
    layout->addItem(spacerBottom, 2, 1);
    layout->addWidget(views[LOWER_RIGHT_QUADRANT], 2, 2);

    default_views();

    // Not sure if this is the right way to do this but need to autoset each of the views
    const char *av[2];
    av[0] = "autoview";
    av[1] = (char *)0;
    for (int i = UPPER_RIGHT_QUADRANT + 1; i < LOWER_RIGHT_QUADRANT + 1; i++) {
	gedp->ged_gvp = views[i]->view();
	ged_exec(gedp, 1, (const char **)av);
    }
    gedp->ged_gvp = views[UPPER_RIGHT_QUADRANT]->view();
    views[UPPER_RIGHT_QUADRANT]->set_current(1);
    currentView = views[UPPER_RIGHT_QUADRANT];
    // This is only used in quad mode
    currentView->select(1);
}

void
QtCADQuad::do_view_changed()
{
    emit changed(currentView);
}

bool
QtCADQuad::isValid()
{
    for (int i = UPPER_RIGHT_QUADRANT; i < LOWER_RIGHT_QUADRANT + 1; i++) {
	if (views[i] != nullptr && !views[i]->isValid())
	    return false;
    }
    return true;
}

void
QtCADQuad::fallback()
{
    for (int i = UPPER_RIGHT_QUADRANT; i < LOWER_RIGHT_QUADRANT + 1; i++) {
	if (views[i] != nullptr) {
	    views[i]->fallback();

	    views[i]->set_current(0);
	}
    }

    // ur is still the default current
    views[UPPER_RIGHT_QUADRANT]->set_current(1);
    currentView = views[UPPER_RIGHT_QUADRANT];
}

bool
QtCADQuad::eventFilter(QObject *t, QEvent *e)
{
    if (e->type() == QEvent::KeyPress || e->type() == QEvent::MouseButtonPress) {
	QtCADView *oc = currentView;
	for (int i = UPPER_RIGHT_QUADRANT; i < LOWER_RIGHT_QUADRANT + 1; i++) {
	    if (views[i] != nullptr && t == views[i]) {
		currentView = views[i];
		// Make sure we are in quad mode
		if (views[1] != nullptr) {
		    views[i]->select(1);
		}
	    } else {
		if (views[i] != nullptr) {
		    views[i]->set_current(0);
		    views[i]->select(0);
		}
	    }
	}

	currentView->set_current(1);
	if (currentView != oc)
	    emit selected(currentView);
    }
    return false;
}

void
QtCADQuad::default_views()
{
    if (views[UPPER_RIGHT_QUADRANT] != nullptr) {
	if (views[UPPER_LEFT_QUADRANT] == nullptr) {
	    views[UPPER_RIGHT_QUADRANT]->aet(270, 90, 0);
	}
	else {
	    views[UPPER_RIGHT_QUADRANT]->aet(35, 25, 0);
	}
    }
    if (views[UPPER_LEFT_QUADRANT] != nullptr) {
	views[UPPER_LEFT_QUADRANT]->aet(0, 90, 0);
    }
    if (views[LOWER_LEFT_QUADRANT] != nullptr) {
	views[LOWER_LEFT_QUADRANT]->aet(0, 0, 0);
    }
    if (views[LOWER_RIGHT_QUADRANT] != nullptr) {
	views[LOWER_RIGHT_QUADRANT]->aet(90, 0, 0);
    }
}

struct bview *
QtCADQuad::view(int quadrantId)
{
    if (quadrantId > LOWER_RIGHT_QUADRANT || quadrantId < UPPER_RIGHT_QUADRANT) quadrantId = UPPER_RIGHT_QUADRANT;

    if (views[quadrantId] != nullptr) {
	return views[quadrantId]->view();
    }

    return currentView->view();
}

QtCADView *
QtCADQuad::get(int quadrantId)
{
    if (quadrantId > LOWER_RIGHT_QUADRANT || quadrantId < UPPER_RIGHT_QUADRANT) quadrantId = UPPER_RIGHT_QUADRANT;

    if (views[quadrantId] != nullptr) {
	return views[quadrantId];
    }

    return currentView;
}

void
QtCADQuad::select(int quadrantId)
{
    if (quadrantId > LOWER_RIGHT_QUADRANT || quadrantId < UPPER_RIGHT_QUADRANT) quadrantId = UPPER_RIGHT_QUADRANT;

    QtCADView *oc = currentView;

    if (views[quadrantId] != nullptr) {
	currentView = views[quadrantId];
    }

    if (oc != currentView) {
	emit selected(currentView);
    }
}

void
QtCADQuad::select(const char *quadrant_id)
{
    if (BU_STR_EQUIV(quadrant_id, "ur")) {
	select(UPPER_RIGHT_QUADRANT);
	return;
    }
    if (BU_STR_EQUIV(quadrant_id, "ul")) {
	select(UPPER_LEFT_QUADRANT);
	return;
    }
    if (BU_STR_EQUIV(quadrant_id, "ll")) {
	select(LOWER_LEFT_QUADRANT);
	return;
    }
    if (BU_STR_EQUIV(quadrant_id, "lr")) {
	select(LOWER_RIGHT_QUADRANT);
	return;
    }
}


int
QtCADQuad::get_selected()
{
    if (currentView == views[UPPER_RIGHT_QUADRANT]) {
	return 0;
    }
    if (currentView == views[UPPER_LEFT_QUADRANT]) {
	return 1;
    }
    if (currentView == views[LOWER_LEFT_QUADRANT]) {
	return 2;
    }
    if (currentView == views[LOWER_RIGHT_QUADRANT]) {
	return 3;
    }

    return 0;
}

void
QtCADQuad::need_update(void *)
{
    for (int i = UPPER_RIGHT_QUADRANT; i < LOWER_RIGHT_QUADRANT + 1; i++) {
	if (views[i] != nullptr) {
	    views[i]->need_update(NULL);
	}
    }
}

void
QtCADQuad::stash_hashes()
{
    for (int i = UPPER_RIGHT_QUADRANT; i < LOWER_RIGHT_QUADRANT + 1; i++) {
	if (views[i] != nullptr) {
	    views[i]->stash_hashes();
	}
    }
}

bool
QtCADQuad::diff_hashes()
{
    bool ret = false;
    for (int i = UPPER_RIGHT_QUADRANT; i < LOWER_RIGHT_QUADRANT + 1; i++) {
	if (views[i] != nullptr) {
	    if (views[i]->diff_hashes()) {
		ret = true;
	    }
	}
    }

    return ret;
}

void
QtCADQuad::enableDefaultKeyBindings()
{
    for (int i = UPPER_RIGHT_QUADRANT; i < LOWER_RIGHT_QUADRANT + 1; i++) {
	if (views[i] != nullptr) {
	    views[i]->enableDefaultKeyBindings();
	}
    }
}

void
QtCADQuad::disableDefaultKeyBindings()
{
    for (int i = UPPER_RIGHT_QUADRANT; i < LOWER_RIGHT_QUADRANT + 1; i++) {
	if (views[i] != nullptr) {
	    views[i]->disableDefaultKeyBindings();
	}
    }
}

void
QtCADQuad::enableDefaultMouseBindings()
{
    for (int i = UPPER_RIGHT_QUADRANT; i < LOWER_RIGHT_QUADRANT + 1; i++) {
	if (views[i] != nullptr) {
	    views[i]->enableDefaultMouseBindings();
	}
    }
}

void
QtCADQuad::disableDefaultMouseBindings()
{
    for (int i = UPPER_RIGHT_QUADRANT; i < LOWER_RIGHT_QUADRANT + 1; i++) {
	if (views[i] != nullptr) {
	    views[i]->disableDefaultMouseBindings();
	}
    }
}

void
QtCADQuad::set_lmouse_move_default(int mm)
{
    for (int i = UPPER_RIGHT_QUADRANT; i < LOWER_RIGHT_QUADRANT + 1; i++) {
	if (views[i] != nullptr) {
	    views[i]->set_lmouse_move_default(mm);
	}
    }
}

void
QtCADQuad::do_init_done()
{
    if (!init_done_flag) {
	init_done_flag = true;
	emit init_done();
    }
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
