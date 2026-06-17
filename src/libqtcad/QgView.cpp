/*                      Q G V I E W . C P P
 * BRL-CAD
 *
 * Copyright (c) 2021-2026 United States Government as represented by
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
/** @file QgView.cpp
 *
 * Wrapper widget that handles the various widget types which may
 * constitute a Qt based geometry view.
 *
 */

#include "common.h"

#include "qtcad/QgCanvasBase.h"
#include "qtcad/QgGL.h"
#include "qtcad/QgSW.h"
#include "qtcad/QgView.h"
#include "qtcad/QgViewFilter.h"
#include "qtcad/QgSignalFlags.h"

extern "C" {
#include "bu/malloc.h"
#include "bsg/view_state.h"
}

#include <algorithm>

static uint32_t
qg_refresh_flags(QgViewUpdateFlags flags)
{
    uint32_t refresh_flags = 0;

    if (!flags)
	return BSG_VIEW_REFRESH_ALL;
    if (flags & QG_VIEW_REFRESH)
	refresh_flags |= BSG_VIEW_REFRESH_VIEW;
    if (flags & QG_VIEW_DRAWN)
	refresh_flags |= BSG_VIEW_REFRESH_DRAW;
    if (flags & QG_VIEW_SELECT)
	refresh_flags |= BSG_VIEW_REFRESH_OVERLAY;
    if (flags & QG_VIEW_MODE)
	refresh_flags |= BSG_VIEW_REFRESH_EDIT;
    if (flags & QG_VIEW_DB)
	refresh_flags |= BSG_VIEW_REFRESH_DRAW;

    return refresh_flags ? refresh_flags : BSG_VIEW_REFRESH_ALL;
}

/* ---------------------------------------------------------------------- */
/* Factory: create the appropriate canvas widget for the requested type.  */
/* The single BRLCAD_OPENGL guard lives here rather than being duplicated  */
/* in every QgView method body.                                            */
/* ---------------------------------------------------------------------- */
static QgCanvasBase *
make_canvas(QWidget *parent, int type, struct fb *fbp)
{
    switch (type) {
#ifdef BRLCAD_OPENGL
    case QgView_GL:
return new QgGL(parent, fbp);
#endif
    case QgView_SW:
return new QgSW(parent, fbp);
    default:
/* QgView_AUTO or any other value: prefer hardware GL, fall back to SW */
#ifdef BRLCAD_OPENGL
return new QgGL(parent, fbp);
#else
return new QgSW(parent, fbp);
#endif
    }
}

/* ---------------------------------------------------------------------- */

QgView::QgView(QWidget *parent, int type, struct fb *fbp)
    : QWidget(parent)
{
    this->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    l = new QBoxLayout(QBoxLayout::LeftToRight, this);
    l->setSpacing(0);
    l->setContentsMargins(0, 0, 0, 0);

    canvas = make_canvas(this, type, fbp);
    if (!canvas)
return;

    QWidget *w = canvas->canvasWidget();
    w->setMinimumSize(50, 50);
    w->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    l->addWidget(w);

    /* Connect canvas signals via old-style macros (QgCanvasBase is not a
     * QObject, so we obtain the QObject* via asQObject()). */
    QObject::connect(canvas->asQObject(), SIGNAL(changed()),
     this, SLOT(do_view_changed()));
    QObject::connect(canvas->asQObject(), SIGNAL(init_done()),
     this, SLOT(do_init_done()));
}

QgView::~QgView()
{
    delete canvas;
}

bool
QgView::isValid()
{
    if (!canvas)
return false;
    return canvas->isValid();
}

int
QgView::view_type()
{
    if (!canvas)
return -1;
#ifdef BRLCAD_OPENGL
    if (dynamic_cast<QgGL *>(canvas))
return QgView_GL;
#endif
    return QgView_SW;
}


void
QgView::save_image(int UNUSED(quad))
{
}

void
QgView::render_to_file(const QString &filename)
{
    if (canvas)
canvas->render_to_file(filename);
}

void
QgView::get_viewport_image(QImage &img)
{
    if (canvas)
canvas->get_viewport_image(img);
    else
img = QImage();
}

void
QgView::do_view_changed()
{
    QTCAD_SLOT("QgView::do_view_changed", 1);
    emit changed(this);
}

void
QgView::need_update(QgViewUpdateFlags flags)
{
    bsg_log(4, "QgView::need_update");
    QTCAD_SLOT("QgView::need_update", 1);
    if (struct bsg_view *bv = view())
	bsg_view_refresh_request(bv, qg_refresh_flags(flags));
    if (canvas)
canvas->need_update();
}

struct bsg_view *
QgView::view()
{
    return canvas ? canvas->view() : nullptr;
}

struct dm *
QgView::dmp()
{
    return canvas ? canvas->displayManager() : nullptr;
}

struct fb *
QgView::ifp()
{
    return canvas ? canvas->frameBuffer() : nullptr;
}

void
QgView::set_view(struct bsg_view *nv)
{
    if (canvas)
canvas->set_view(nv);
}

void
QgView::stash_hashes()
{
    if (canvas)
canvas->stash_hashes();
}

bool
QgView::diff_hashes()
{
    return canvas ? canvas->diff_hashes() : false;
}

void
QgView::aet(double a, double e, double t)
{
    if (canvas)
canvas->aet(a, e, t);
}

void
QgView::set_current(int i)
{
    if (canvas)
canvas->set_current(i);
}

int
QgView::current()
{
    return canvas ? canvas->currentView() : 0;
}

void
QgView::add_event_filter(QObject *o)
{
    curr_event_filter = o;
    filters.push_back(o);
    if (canvas)
canvas->canvasWidget()->installEventFilter(o);
}

void
QgView::installFilter(QgViewFilter *f)
{
    if (!f)
	return;
    f->set_view(view());
    add_event_filter(f);
}

void
QgView::clear_event_filter(QObject *o)
{
    if (!canvas)
return;

    QWidget *w = canvas->canvasWidget();
    if (o) {
w->removeEventFilter(o);
auto fit = std::find(filters.begin(), filters.end(), o);
if (fit != filters.end())
    filters.erase(fit);
    } else {
for (size_t i = 0; i < filters.size(); i++)
    w->removeEventFilter(filters[i]);
filters.clear();
    }

    /* Passing nullptr is the documented "clear all managed filters" mode. */
    if (!o || curr_event_filter == o)
curr_event_filter = nullptr;
}

void
QgView::clearFilter(QgViewFilter *f)
{
    if (!f)
	return;
    clear_event_filter(f);
    f->set_view(nullptr);
}

void
QgView::enableDefaultKeyBindings()
{
    if (canvas)
canvas->enableDefaultKeyBindings();
}

void
QgView::disableDefaultKeyBindings()
{
    if (canvas)
canvas->disableDefaultKeyBindings();
}

void
QgView::enableDefaultMouseBindings()
{
    if (canvas)
canvas->enableDefaultMouseBindings();
}

void
QgView::disableDefaultMouseBindings()
{
    if (canvas)
canvas->disableDefaultMouseBindings();
}


void
QgView::set_lmouse_move_default(int mm)
{
    QTCAD_SLOT("QgView::set_lmouse_move_default", 1);
    if (canvas)
canvas->set_lmouse_move_default(mm);
}


void
QgView::do_init_done()
{
    QTCAD_SLOT("QgView::do_init_done", 1);
    emit init_done();
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
