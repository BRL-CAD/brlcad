/*                        I S S T G L . H
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
/** @file isstgl.h
 *
 * OpenGL widget wrapper encoding the information specific to the
 * TIE raytracing view.
 *
 * TODO:  Look at f_knob, knob_rot, mged_vrot_syz, mged_rot, etc.
 * to determine how MGED is doing its mouse x,y delta to view
 * manipulation translation logic.
 *
 */

#ifndef ISSTGL_H
#define ISSTGL_H

#include <QOpenGLWidget>
#include <QOpenGLFunctions>
#include <QPainter>
#include <QImage>

#include <QThread>
#include <QMutex>
#include <QWaitCondition>
#include <QElapsedTimer>

extern "C" {
#include "rt/tie.h"
#include "librender/camera.h"
}

class isstGL;

class TIERenderer : public QObject, protected QOpenGLFunctions
{
    Q_OBJECT
    public:
	TIERenderer(isstGL *w);
	~TIERenderer();

	void resize();

	// Thread management
	void lockRenderer() { m_renderMutex.lock(); }
	void unlockRenderer() { m_renderMutex.unlock(); }
	QMutex *grabMutex() { return &m_grabMutex; }
	QWaitCondition *grabCond() { return &m_grabCond; }
	void prepareExit() { m_exiting = true; m_grabCond.wakeAll(); }

    signals:
	void contextWanted();

    public slots:
	void render();
	void res_incr();
	void res_decr();

    public:
	struct tie_s *tie = NULL; // From parent app
	struct render_camera_s camera;
	vect_t camera_pos_init;
	vect_t camera_focus_init;
	bool changed = true;
	bool m_exiting = false;

    private:
	struct camera_tile_s tile;
	void *texdata = NULL;
	long texdata_size = 0;
	tienet_buffer_t buffer_image;
	GLuint texid = 0;
	int resolution = 20;
	int resolution_factor = 0;

	bool m_init = false;
	isstGL *m_w;

	bool scaled = false;

	// Threading variables
	QMutex m_renderMutex;
	QElapsedTimer m_elapsed;
	QMutex m_grabMutex;
	QWaitCondition m_grabCond;
};

// Use QOpenGLFunctions so we don't have to prefix all OpenGL calls with "f->"
class isstGL : public QOpenGLWidget
{
    Q_OBJECT

    public:
	explicit isstGL(QWidget *parent = nullptr);
	~isstGL();

	void set_tie(struct tie_s *in_tie);

	void save_image();

    protected:
	void paintEvent(QPaintEvent *) override { }

	void keyPressEvent(QKeyEvent *k) override;
	void mouseMoveEvent(QMouseEvent *e) override;

    signals:
      void renderRequested();

    public slots:
      void grabContext();

    private slots:
      void onAboutToCompose();
      void onFrameSwapped();
      void onAboutToResize();
      void onResized();

    private:
	int x_prev = -INT_MAX;
	int y_prev = -INT_MAX;

	QThread *m_thread;
	TIERenderer *m_renderer;
};

#endif /* ISSTGL_H */

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8

