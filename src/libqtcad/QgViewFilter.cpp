/*                  Q G V I E W F I L T E R . C P P
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
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
/** @file QgViewFilter.cpp
 *
 * Shared Qt event-filter helpers for QgView-based interaction filters.
 */

#include "common.h"

extern "C" {
#include "bsg.h"
}

#include "qtcad/QgViewFilter.h"

class QgViewFilter::QgViewFilterPrivate {
public:
	struct bsg_view *v = nullptr;
};

QgViewFilter::QgViewFilter(QObject *parent)
	: QObject(parent), m(new QgViewFilterPrivate)
{
}

QgViewFilter::~QgViewFilter()
{
	delete m;
}

void
QgViewFilter::set_view(struct bsg_view *nv)
{
	m->v = nv;
}

struct bsg_view *
QgViewFilter::view() const
{
	return m->v;
}

QMouseEvent *
QgViewFilter::view_sync(QEvent *e)
{
	if (!m->v)
		return nullptr;

	/* If this is not a supported mouse event, there is nothing to do. */
	QMouseEvent *m_e = nullptr;
	if (e->type() == QEvent::MouseButtonPress || e->type() == QEvent::MouseButtonRelease || e->type() == QEvent::MouseButtonDblClick || e->type() == QEvent::MouseMove)
		m_e = (QMouseEvent *)e;
	if (!m_e)
		return nullptr;

	/* Capture current mouse coordinates from the Qt event. */
	int e_x = 0, e_y = 0;
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
	e_x = m_e->x();
	e_y = m_e->y();
#else
	e_x = (int)m_e->position().x();
	e_y = (int)m_e->position().y();
#endif

	/* Keep bsg_view mouse state synchronized with the event stream. */
	m->v->gv_prevMouseX = m->v->gv_mouse_x;
	m->v->gv_prevMouseY = m->v->gv_mouse_y;
	m->v->gv_mouse_x = e_x;
	m->v->gv_mouse_y = e_y;
	bsg_screen_pt(&m->v->gv_point, (fastf_t)e_x, (fastf_t)e_y, m->v);

	/* Modifier keys are typically view-nav gestures, not edit operations. */
	if (m_e->modifiers() != Qt::NoModifier)
		return nullptr;

	return m_e;
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
