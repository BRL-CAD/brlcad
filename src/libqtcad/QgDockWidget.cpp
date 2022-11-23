/*                 Q G D O C K W I D G E T . C P P
 * BRL-CAD
 *
 * Copyright (c) 2014-2022 United States Government as represented by
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
/** @file QgDockWidget.cpp
 *
 */

#include <map>
#include <set>
#include <QEvent>
#include <QMouseEvent>
#include <QRect>
#include "qtcad/QgDockWidget.h"

QgDockWidget::QgDockWidget(const QString &title, QWidget *parent)
    : QDockWidget(title, parent)
{
}

void
QgDockWidget::toWindow(bool floating)
{
    if (floating) {
	setWindowFlags(
		Qt::CustomizeWindowHint |
                Qt::Window |
                Qt::WindowMinimizeButtonHint |
                Qt::WindowMaximizeButtonHint |
		Qt::WindowCloseButtonHint
		);
	show();
    }
}

bool
QgDockWidget::event(QEvent *e)
{
    if (e->type() == QEvent::MouseMove) {
	moving = true;
	return QDockWidget::event(e);
    }
    if (e->type() != QEvent::MouseButtonRelease)
	return QDockWidget::event(e);
    QMouseEvent *m_e = (QMouseEvent *)e;
    if (moving) {
	moving = false;
	return QDockWidget::event(e);
    }
#ifdef USE_QT6
    QPoint gpos = m_e->globalPosition().toPoint();
#else
    QPoint gpos = m_e->globalPos();
#endif
    QRect trect = this->geometry();
    if (!trect.contains(gpos))
	return QDockWidget::event(e);

    emit banner_click();

    if (m) {
	if (m->flatten_hierarchy) {
	    setWindowTitle("Objects");
	} else {
	    setWindowTitle("Hierarchy");
	}
    }

    return QDockWidget::event(e);
}

/*
 * Local Variables:
 * mode: C++
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
