/*                      Q G V I E W . C P P
 * BRL-CAD
 *
 * Copyright (c) 2021-2023 United States Government as represented by
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

#include "bg/polygon.h"
#include "bv.h"
#include "raytrace.h" // For finalize polygon sketch export functionality (TODO - need to move...)
#include "qtcad/QgView.h"
#include "qtcad/QgSignalFlags.h"

extern "C" {
#include "bu/malloc.h"
}


QgView::QgView(QWidget *parent, int type, struct fb *fbp)
    : QWidget(parent)
{
    this->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    l = new QBoxLayout(QBoxLayout::LeftToRight, this);
    l->setSpacing(0);
    l->setContentsMargins(0, 0, 0, 0);

    switch (type) {
#ifdef BRLCAD_OPENGL
	case QgView_GL:
	    canvas_gl = new QgGL(this, fbp);
	    canvas_gl->setMinimumSize(50,50);
	    canvas_gl->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	    l->addWidget(canvas_gl);
	    QObject::connect(canvas_gl, &QgGL::changed, this, &QgView::do_view_changed);
	    QObject::connect(canvas_gl, &QgGL::init_done, this, &QgView::do_init_done);
	    break;
#endif
	case QgView_SW:
	    canvas_sw = new QgSW(this, fbp);
	    canvas_sw->setMinimumSize(50,50);
	    canvas_sw->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	    l->addWidget(canvas_sw);
	    QObject::connect(canvas_sw, &QgSW::changed, this, &QgView::do_view_changed);
	    QObject::connect(canvas_sw, &QgSW::init_done, this, &QgView::do_init_done);
	    break;
	default:
#ifdef BRLCAD_OPENGL
	    canvas_gl = new QgGL(this, fbp);
	    canvas_gl->setMinimumSize(50,50);
	    canvas_gl->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	    l->addWidget(canvas_gl);
	    QObject::connect(canvas_gl, &QgGL::changed, this, &QgView::do_view_changed);
	    QObject::connect(canvas_gl, &QgGL::init_done, this, &QgView::do_init_done);
#else
	    canvas_sw = new QgSW(this, fbp);
	    canvas_sw->setMinimumSize(50,50);
	    canvas_sw->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	    l->addWidget(canvas_sw);
	    QObject::connect(canvas_sw, &QgSW::changed, this, &QgView::do_view_changed);
	    QObject::connect(canvas_sw, &QgSW::init_done, this, &QgView::do_init_done);
#endif
	    return;
    }
}

QgView::~QgView()
{
#ifdef BRLCAD_OPENGL
    if (canvas_gl)
	delete canvas_gl;
#endif
    if (canvas_sw)
	delete canvas_sw;
}

bool
QgView::isValid()
{
    if (canvas_sw)
	return true;

#ifdef BRLCAD_OPENGL
    if (canvas_gl)
	return canvas_gl->isValid();
#endif

    return false;
}

int
QgView::view_type()
{
#ifdef BRLCAD_OPENGL
    if (canvas_gl)
	return QgView_GL;
#endif
    if (canvas_sw)
	return QgView_SW;

    return -1;
}


void
QgView::save_image(int UNUSED(quad))
{
}

void
QgView::do_view_changed()
{
    QTCAD_SLOT("QgView::do_view_changed", 1);
    emit changed(this);
}

void
QgView::need_update(unsigned long long)
{
    bv_log(4, "QgView::need_update");
    QTCAD_SLOT("QgView::need_update", 1);
#ifdef BRLCAD_OPENGL
    if (canvas_gl) {
	canvas_gl->need_update();
	return;
    }
#endif
    if (canvas_sw) {
	canvas_sw->need_update();
	return;
    }
}

struct bview *
QgView::view()
{
#ifdef BRLCAD_OPENGL
    if (canvas_gl)
	return canvas_gl->v;
#endif
    if (canvas_sw)
	return canvas_sw->v;

    return NULL;
}

struct dm *
QgView::dmp()
{
#ifdef BRLCAD_OPENGL
    if (canvas_gl)
	return canvas_gl->dmp;
#endif
    if (canvas_sw)
	return canvas_sw->dmp;

    return NULL;
}

struct fb *
QgView::ifp()
{
#ifdef BRLCAD_OPENGL
    if (canvas_gl)
	return canvas_gl->ifp;
#endif
    if (canvas_sw)
	return canvas_sw->ifp;

    return NULL;
}

void
QgView::set_view(struct bview *nv)
{
#ifdef BRLCAD_OPENGL
    if (canvas_gl) {
	canvas_gl->v = nv;
	if (canvas_gl->dmp && canvas_gl->v) {
	    canvas_gl->v->dmp = canvas_gl->dmp;
	    struct dm *dmp = (struct dm *)canvas_gl->dmp;
	    dm_configure_win(dmp, 0);
	    canvas_gl->v->gv_width = dm_get_width(dmp);
	    canvas_gl->v->gv_height = dm_get_height(dmp);
	}
    }
#endif
    if (canvas_sw) {
	canvas_sw->v = nv;
    	if (canvas_sw->dmp && canvas_sw->v) {
	    canvas_sw->v->dmp = canvas_sw->dmp;
	    struct dm *dmp = (struct dm *)canvas_sw->dmp;
	    dm_configure_win(dmp, 0);
	    canvas_gl->v->gv_width = dm_get_width(dmp);
	    canvas_gl->v->gv_height = dm_get_height(dmp);
	}
    }
}

void
QgView::stash_hashes()
{
#ifdef BRLCAD_OPENGL
    if (canvas_gl) {
	canvas_gl->stash_hashes();
    }
#endif
    if (canvas_sw) {
	canvas_sw->stash_hashes();
    }
}

bool
QgView::diff_hashes()
{
#ifdef BRLCAD_OPENGL
    if (canvas_gl) {
	return canvas_gl->diff_hashes();
    }
#endif
    if (canvas_sw) {
	return canvas_sw->diff_hashes();
    }

    return false;
}

void
QgView::aet(double a, double e, double t)
{
#ifdef BRLCAD_OPENGL
    if (canvas_gl) {
	canvas_gl->aet(a, e, t);
    }
#endif
    if (canvas_sw) {
	canvas_sw->aet(a, e, t);
    }
}

void
QgView::set_current(int i)
{
#ifdef BRLCAD_OPENGL
    if (canvas_gl) {
	canvas_gl->current = i;
    }
#endif
    if (canvas_sw) {
	canvas_sw->current = i;
    }
}

int
QgView::current()
{
#ifdef BRLCAD_OPENGL
    if (canvas_gl) {
	return canvas_gl->current;
    }
#endif
    if (canvas_sw) {
	return canvas_sw->current;
    }

    return 0;
}

void
QgView::add_event_filter(QObject *o)
{
    curr_event_filter = o;
    filters.push_back(o);
#ifdef BRLCAD_OPENGL
    if (canvas_gl) {
	canvas_gl->installEventFilter(o);
	return;
    }
#endif
    if (canvas_sw) {
	canvas_sw->installEventFilter(o);
	return;
    }
}

void
QgView::clear_event_filter(QObject *o)
{
#ifdef BRLCAD_OPENGL
    if (canvas_gl) {
	if (o) {
	    canvas_gl->removeEventFilter(o);
	} else {
	    for (size_t i = 0; i < filters.size(); i++) {
		canvas_gl->removeEventFilter(filters[i]);
	    }
	    filters.clear();
	}
    }
#endif
    if (canvas_sw) {
	if (o) {
	    canvas_sw->removeEventFilter(o);
	} else {
	    for (size_t i = 0; i < filters.size(); i++) {
		canvas_sw->removeEventFilter(filters[i]);
	    }
	    filters.clear();
	}
    }
    curr_event_filter = NULL;
}

void
QgView::set_draw_custom(void (*draw_custom)(struct bview *, void *), void *draw_udata)
{

#ifdef BRLCAD_OPENGL
    if (canvas_gl) {
	canvas_gl->draw_custom = draw_custom;
	canvas_gl->draw_udata = draw_udata;
	return;
    }
#endif
    if (canvas_sw) {
	canvas_sw->draw_custom = draw_custom;
	canvas_sw->draw_udata = draw_udata;
	return;
    }
}

void
QgView::enableDefaultKeyBindings()
{
#ifdef BRLCAD_OPENGL
    if (canvas_gl) {
	canvas_gl->enableDefaultKeyBindings();
	return;
    }
#endif
    if (canvas_sw) {
	canvas_sw->enableDefaultKeyBindings();
	return;
    }
}

void
QgView::disableDefaultKeyBindings()
{
#ifdef BRLCAD_OPENGL
    if (canvas_gl) {
	canvas_gl->disableDefaultKeyBindings();
	return;
    }
#endif
    if (canvas_sw) {
	canvas_sw->disableDefaultKeyBindings();
	return;
    }
}

void
QgView::enableDefaultMouseBindings()
{
#ifdef BRLCAD_OPENGL
    if (canvas_gl) {
	canvas_gl->enableDefaultMouseBindings();
	return;
    }
#endif
    if (canvas_sw) {
	canvas_sw->enableDefaultMouseBindings();
	return;
    }


}

void
QgView::disableDefaultMouseBindings()
{
#ifdef BRLCAD_OPENGL
    if (canvas_gl) {
	canvas_gl->disableDefaultMouseBindings();
	return;
    }
#endif
    if (canvas_sw) {
	canvas_sw->disableDefaultMouseBindings();
	return;
    }
}


void
QgView::set_lmouse_move_default(int mm)
{
    QTCAD_SLOT("QgView::set_lmouse_move_default", 1);

#ifdef BRLCAD_OPENGL
    if (canvas_gl) {
	canvas_gl->set_lmouse_move_default(mm);
	return;
    }
#endif
    if (canvas_sw) {
	canvas_sw->set_lmouse_move_default(mm);
	return;
    }
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
