/*                           Q T S W . H
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
/** @file qtcad/QtSW.h
 *
 * This defines a Qt widget for displaying the visualization results of the
 * bundled libosmesa OpenGL software rasterizer, using the swrast libdm
 * backend.
 *
 * Unlike the standard QtGL widget, this can display OpenGL rendered graphics
 * even if the OpenGL stack on the host operating system is non-functional (it
 * will be a great deal slower, but since it does not rely on any system
 * capabilities to produce its images it should work in any environment where a
 * basic Qt gui can load.)
 */

#ifndef QTCAD_QTSW_H
#define QTCAD_QTSW_H

#include <QKeyEvent>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QResizeEvent>
#include <QResizeEvent>
#include <QWheelEvent>
#include <QWidget>

extern "C" {
#include "bu/ptbl.h"
#include "bview.h"
#include "dm.h"
}

#include "qtcad/defines.h"

class QTCAD_EXPORT QtSW : public QWidget
{
    Q_OBJECT

    public:
	explicit QtSW(QWidget *parent = nullptr);
	~QtSW();

	void save_image();

	struct bview *v = NULL;
	struct dm *dmp = NULL;
	struct bu_ptbl *dm_set = NULL;
	struct dm **dm_current = NULL;
	double *base2local = NULL;
	double *local2base = NULL;

    protected:
	void paintEvent(QPaintEvent *e) override;
	void resizeEvent(QResizeEvent *e) override;

	void keyPressEvent(QKeyEvent *k) override;
	void mouseMoveEvent(QMouseEvent *e) override;
	void mousePressEvent(QMouseEvent *e) override;
	void mouseReleaseEvent(QMouseEvent *e) override;
	void wheelEvent(QWheelEvent *e) override;

    private:
	bool m_init = false;
	int x_prev = -INT_MAX;
	int y_prev = -INT_MAX;
};

#endif /* QTCAD_QTSW_H */

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8

