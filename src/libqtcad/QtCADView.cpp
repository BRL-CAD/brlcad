/*                      Q T C A D V I E W . C P P
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
/** @file QtCADView.cpp
 *
 * Wrapper widget that handles the various widget types which may
 * constitute a Qt based geometry view.
 *
 */

#include "common.h"

#include "qtcad/QtCADView.h"

extern "C" {
#include "bu/malloc.h"
}


QtCADView::QtCADView(QWidget *parent, int type, struct fb *fbp)
    : QWidget(parent)
{
    this->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    l = new QBoxLayout(QBoxLayout::LeftToRight, this);
    l->setSpacing(0);
    l->setContentsMargins(0, 0, 0, 0);

    switch (type) {
#ifdef BRLCAD_OPENGL
	case QtCADView_GL:
	    canvas_gl = new QtGL(this, fbp);
	    canvas_gl->setMinimumSize(50,50);
	    canvas_gl->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	    l->addWidget(canvas_gl);
	    QObject::connect(canvas_gl, &QtGL::changed, this, &QtCADView::do_view_changed);
	    break;
#endif
	case QtCADView_SW:
	    canvas_sw = new QtSW(this, fbp);
	    canvas_sw->setMinimumSize(50,50);
	    canvas_sw->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	    l->addWidget(canvas_sw);
	    QObject::connect(canvas_sw, &QtSW::changed, this, &QtCADView::do_view_changed);
	    break;
	default:
#ifdef BRLCAD_OPENGL
	    canvas_gl = new QtGL(this, fbp);
	    canvas_gl->setMinimumSize(50,50);
	    canvas_gl->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	    l->addWidget(canvas_gl);
	    QObject::connect(canvas_gl, &QtGL::changed, this, &QtCADView::do_view_changed);
#else
	    canvas_sw = new QtSW(this, fbp);
	    canvas_sw->setMinimumSize(50,50);
	    canvas_sw->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	    l->addWidget(canvas_sw);
	    QObject::connect(canvas_sw, &QtSW::changed, this, &QtCADView::do_view_changed);
#endif
	    return;
    }
}

QtCADView::~QtCADView()
{
#ifdef BRLCAD_OPENGL
    if (canvas_gl)
	delete canvas_gl;
#endif
    if (canvas_sw)
	delete canvas_sw;
}

bool
QtCADView::isValid()
{
    if (canvas_sw)
	return true;

#ifdef BRLCAD_OPENGL
    if (canvas_gl)
	return canvas_gl->isValid();
#endif

    return false;
}

void
QtCADView::fallback()
{
    if (canvas_sw)
	return;

#ifdef BRLCAD_OPENGL
    if (canvas_gl && !canvas_gl->isValid()) {
	bu_log("System OpenGL Canvas didn't work, falling back on Software Rasterizer\n");
	delete canvas_gl;
	canvas_gl = NULL;
	canvas_sw = new QtSW(this, NULL);
	canvas_sw->setMinimumSize(50,50);
	canvas_sw->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	l->addWidget(canvas_sw);
    }
#endif
}


int
QtCADView::view_type()
{
#ifdef BRLCAD_OPENGL
    if (canvas_gl)
	return QtCADView_GL;
#endif
    if (canvas_sw)
	return QtCADView_SW;

    return -1;
}


void
QtCADView::save_image(int UNUSED(quad))
{
}

void
QtCADView::select(int UNUSED(quad))
{
}

void
QtCADView::do_view_changed()
{
    emit changed();
}

void
QtCADView::need_update(void *)
{
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
QtCADView::view()
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
QtCADView::dmp()
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
QtCADView::ifp()
{
#ifdef BRLCAD_OPENGL
    if (canvas_gl)
	return canvas_gl->ifp;
#endif
    if (canvas_sw)
	return canvas_sw->ifp;

    return NULL;
}

double
QtCADView::base2local()
{
#ifdef BRLCAD_OPENGL
    if (canvas_gl && canvas_gl->v)
	return canvas_gl->v->gv_base2local;
#endif
    if (canvas_sw && canvas_sw->v)
	return canvas_sw->v->gv_base2local;

    return 1.0;
}

double
QtCADView::local2base()
{
#ifdef BRLCAD_OPENGL
    if (canvas_gl && canvas_gl->v)
	return canvas_gl->v->gv_local2base;
#endif
    if (canvas_sw && canvas_sw->v)
	return canvas_sw->v->gv_local2base;

    return 1.0;
}

// TODO - for quad view, we're going to need to keep
// local views - scene object vlists will be shared,
// but camera settings will not.
void
QtCADView::set_view(struct bview *nv, int UNUSED(quad))
{
#ifdef BRLCAD_OPENGL
    if (canvas_gl) {
	canvas_gl->v = nv;
	if (canvas_gl->dmp && canvas_gl->v) {
	    canvas_gl->v->dmp = canvas_gl->dmp;
	}
    }
#endif
    if (canvas_sw) {
	canvas_sw->v = nv;
    	if (canvas_sw->dmp && canvas_sw->v) {
	    canvas_sw->v->dmp = canvas_sw->dmp;
	}
    }
}

void
QtCADView::set_dmp(struct dm *ndmp, int UNUSED(quad))
{
#ifdef BRLCAD_OPENGL
    if (canvas_gl)
	canvas_gl->dmp = ndmp;
#endif
    if (canvas_sw)
	canvas_sw->dmp = ndmp;
}

void
QtCADView::set_dm_current(struct dm **ndmp, int UNUSED(quad))
{
#ifdef BRLCAD_OPENGL
    if (canvas_gl) {
	canvas_gl->dm_current = ndmp;
	if (canvas_gl->dmp && canvas_gl->dm_current) {
	    (*canvas_gl->dm_current) = canvas_gl->dmp;
	}
    }
#endif
    if (canvas_sw) {
	canvas_sw->dm_current = ndmp;
	if (canvas_sw->dmp && canvas_sw->dm_current) {
	    (*canvas_sw->dm_current) = canvas_sw->dmp;
	}
    }
}

void
QtCADView::set_ifp(struct fb *nfbp, int UNUSED(quad))
{
#ifdef BRLCAD_OPENGL
    if (canvas_gl)
	canvas_gl->ifp = nfbp;
#endif
    if (canvas_sw)
	canvas_sw->ifp = nfbp;
}

void
QtCADView::set_base2local(double nb2l)
{
#ifdef BRLCAD_OPENGL
    if (canvas_gl && canvas_gl->v)
	canvas_gl->v->gv_base2local = nb2l;
#endif
    if (canvas_sw && canvas_sw->v)
	canvas_gl->v->gv_base2local = nb2l;
}

void
QtCADView::set_local2base(double nl2b)
{
#ifdef BRLCAD_OPENGL
    if (canvas_gl && canvas_gl->v)
	canvas_gl->v->gv_local2base = nl2b;
#endif
    if (canvas_sw && canvas_sw->v)
	canvas_sw->v->gv_local2base = nl2b;
}

void
QtCADView::stash_hashes()
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
QtCADView::diff_hashes()
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
QtCADView::aet(double a, double e, double t)
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
QtCADView::set_current(int i)
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
QtCADView::current()
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
QtCADView::add_event_filter(QObject *o)
{
    curr_event_filter = o;
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
QtCADView::clear_event_filter(QObject *o)
{
    if (!o)
	return;
#ifdef BRLCAD_OPENGL
    if (canvas_gl) {
	canvas_gl->removeEventFilter(o);
    }
#endif
    if (canvas_sw) {
	canvas_sw->removeEventFilter(o);
    }
    curr_event_filter = NULL;
}

void
QtCADView::set_draw_custom(void (*draw_custom)(struct bview *, double, double, void *), void *draw_udata)
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

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
