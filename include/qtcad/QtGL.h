/*                        Q T G L . H
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
/** @file qtgl.h
 *
 */

#ifndef QTGL_H
#define QTGL_H

#include <QImage>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QObject>
#include <QOpenGLFunctions>
#include <QOpenGLWidget>
#include <QPainter>
#include <QWheelEvent>
#include <QWidget>

extern "C" {
#include "bu/ptbl.h"
#include "bv.h"
#include "dm.h"
}

#include "qtcad/defines.h"

// Use QOpenGLFunctions so we don't have to prefix all OpenGL calls with "f->"
class QTCAD_EXPORT QtGL : public QOpenGLWidget, protected QOpenGLFunctions
{
    Q_OBJECT

    public:
	explicit QtGL(QWidget *parent = nullptr, struct fb *fbp = NULL);
	~QtGL();

	void save_image();

	struct bview *v = NULL;
	struct dm *dmp = NULL;
	struct fb *ifp = NULL;
	struct bu_ptbl *dm_set = NULL;
	struct dm **dm_current = NULL;
	double *base2local = NULL;
	double *local2base = NULL;

    protected:
	void paintGL() override;
	void resizeGL(int w, int h) override;

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

#endif /* QTGL_H */

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8

