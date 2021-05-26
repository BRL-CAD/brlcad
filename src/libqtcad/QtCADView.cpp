/*                      Q T C A D V I E W . C P P
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
/** @file QtCADView.cpp
 *
 * Wrapper widget that handles the various widget types which may
 * constitute a Qt based geometry view.
 *
 */

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
	case QtCADView_GL:
	    canvas_gl = new QtGL(this, fbp);
	    canvas_gl->setMinimumSize(512,512);
	    canvas_gl->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	    l->addWidget(canvas_gl);
	    break;
	case QtCADView_SW:
	    canvas_sw = new QtSW(this, fbp);
	    canvas_sw->setMinimumSize(512,512);
	    canvas_sw->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	    l->addWidget(canvas_sw);
	    break;
	case QtCADView_QUAD_AUTO:
	    break;
	case QtCADView_QUAD_GL:
	    break;
	case QtCADView_QUAD_SW:
	    break;
	default:
	    canvas_gl = new QtGL(this, fbp);
	    canvas_gl->setMinimumSize(512,512);
	    canvas_gl->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	    l->addWidget(canvas_gl);
	    return;
    }
}

QtCADView::~QtCADView()
{
    if (canvas_gl)
	delete canvas_gl;
    if (canvas_sw)
	delete canvas_sw;
}

bool
QtCADView::isValid()
{
    if (canvas_sw)
	return true;

    if (canvas_gl)
	return canvas_gl->isValid();

    return false;
}

void
QtCADView::fallback()
{
    if (canvas_sw)
	return;

    if (canvas_gl && !canvas_gl->isValid()) {
	bu_log("System OpenGL Canvas didn't work, falling back on Software Rasterizer\n");
	struct fb *fbp = canvas_gl->ifp;
	delete canvas_gl;
	canvas_gl = NULL;
	canvas_sw = new QtSW(this, fbp);
	canvas_sw->setMinimumSize(512,512);
	canvas_sw->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	l->addWidget(canvas_sw);
    }
}


int
QtCADView::view_type()
{
    if (canvas_gl)
	return QtCADView_GL;
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
QtCADView::need_update()
{
    if (canvas_gl) {
	canvas_gl->need_update();
	return;
    }

    if (canvas_sw) {
	canvas_sw->need_update();
	return;
    }
}

struct bview *
QtCADView::view()
{
    if (canvas_gl)
	return canvas_gl->v;

    if (canvas_sw)
	return canvas_sw->v;

    return NULL;
}

struct dm *
QtCADView::dmp()
{
    if (canvas_gl)
	return canvas_gl->dmp;

    if (canvas_sw)
	return canvas_sw->dmp;

    return NULL;
}

struct fb *
QtCADView::ifp()
{
    if (canvas_gl)
	return canvas_gl->ifp;

    if (canvas_sw)
	return canvas_sw->ifp;

    return NULL;
}

double
QtCADView::base2local()
{
    if (canvas_gl) {
	if (canvas_gl->base2local)
	    return *canvas_gl->base2local;
	return 1.0;
    }
    if (canvas_sw) {
	if (canvas_sw->base2local)
	    return *canvas_sw->base2local;
	return 1.0;
    }

    return 1.0;
}

double
QtCADView::local2base()
{
    if (canvas_gl) {
	if (canvas_gl->local2base)
	    return *canvas_gl->local2base;
	return 1.0;
    }
    if (canvas_sw) {
	if (canvas_sw->local2base)
	    return *canvas_sw->local2base;
	return 1.0;
    }

    return 1.0;
}

void
QtCADView::set_view(struct bview *nv, int UNUSED(quad))
{
    if (canvas_gl) {
	canvas_gl->v = nv;
	if (canvas_gl->dmp && canvas_gl->v) {
	    canvas_gl->v->dmp = canvas_gl->dmp;
	}
    }

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
    if (canvas_gl)
	canvas_gl->dmp = ndmp;

    if (canvas_sw)
	canvas_sw->dmp = ndmp;
}

void
QtCADView::set_dm_current(struct dm **ndmp, int UNUSED(quad))
{
    if (canvas_gl) {
	canvas_gl->dm_current = ndmp;
	if (canvas_gl->dmp && canvas_gl->dm_current) {
	    (*canvas_gl->dm_current) = canvas_gl->dmp;
	}
    }

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
    if (canvas_gl)
	canvas_gl->ifp = nfbp;

    if (canvas_sw)
	canvas_sw->ifp = nfbp;
}

void
QtCADView::set_base2local(double *nb2l)
{
    if (canvas_gl) {
	canvas_gl->base2local= nb2l;
	if (canvas_gl->v && canvas_gl->base2local) {
	    canvas_gl->v->gv_base2local = *canvas_gl->base2local;
	}
    }

    if (canvas_sw) {
	canvas_sw->base2local= nb2l;
	if (canvas_sw->v && canvas_sw->base2local) {
	    canvas_sw->v->gv_base2local = *canvas_sw->base2local;
	}
    }
}

void
QtCADView::set_local2base(double *nl2b)
{
    if (canvas_gl) {
	canvas_gl->local2base = nl2b;
    	if (canvas_gl->v && canvas_gl->local2base) {
	    canvas_gl->v->gv_local2base = *canvas_gl->local2base;
	}
    }

    if (canvas_sw) {
	canvas_sw->local2base = nl2b;
	if (canvas_sw->v && canvas_sw->local2base) {
	    canvas_sw->v->gv_local2base = *canvas_sw->local2base;
	}
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

