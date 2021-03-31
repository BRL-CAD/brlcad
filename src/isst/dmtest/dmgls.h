/*                        D M G L S . H
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
/** @file dmgls.h
 *
 */

#ifndef DMGL_H
#define DMGL_H

#include <QOpenGLWidget>
#include <QOpenGLFunctions>
#include <QPainter>
#include <QImage>

extern "C" {
#include "dm.h"
#include "ged.h"
}

// Use QOpenGLFunctions so we don't have to prefix all OpenGL calls with "f->"
class dmGL : public QOpenGLWidget, protected QOpenGLFunctions
{
    Q_OBJECT

    public:
	explicit dmGL(QWidget *parent = nullptr);
	~dmGL();

	void save_image();

	struct bview *v = NULL;
	struct ged *gedp = NULL;
	struct dm *dmp = NULL;

	unsigned long long prev_vhash = 0;
	unsigned long long prev_lhash = 0;

    protected:
	void paintGL() override;
	void resizeGL(int w, int h) override;

	void keyPressEvent(QKeyEvent *k) override;
	void mouseMoveEvent(QMouseEvent *e) override;
	void mouseReleaseEvent(QMouseEvent *e) override;
	void wheelEvent(QWheelEvent *e) override;

    private:
	bool m_init = false;
	int x_prev = -INT_MAX;
	int y_prev = -INT_MAX;
};

#endif /* DMGL_H */

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8

