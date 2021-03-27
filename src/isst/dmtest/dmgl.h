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
/** @file dmgl.h
 *
 * OpenGL widget wrapper encoding the information specific to the
 * DM raytracing view.
 *
 * TODO:  Look at f_knob, knob_rot, mged_vrot_syz, mged_rot, etc.
 * to determine how MGED is doing its mouse x,y delta to view
 * manipulation translation logic.
 *
 */

#ifndef DMGL_H
#define DMGL_H

#include <QOpenGLWidget>
#include <QOpenGLFunctions>
#include <QPainter>
#include <QImage>

#include <QThread>
#include <QMutex>
#include <QWaitCondition>

extern "C" {
#include "dm.h"
#include "ged.h"
}

class dmGL;

class DMRenderer : public QObject, protected QOpenGLFunctions
{
    Q_OBJECT
    public:
	DMRenderer(dmGL *w);
	~DMRenderer();

	void resize();
	struct dm *dmp = NULL;

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

    public:
	struct tie_s *tie = NULL; // From parent app
	bool changed = true;
	bool m_exiting = false;

    private:
	bool m_init = false;
	dmGL *m_w;


	// Threading variables
	QMutex m_renderMutex;
	QMutex m_grabMutex;
	QWaitCondition m_grabCond;
};

// Use QOpenGLFunctions so we don't have to prefix all OpenGL calls with "f->"
class dmGL : public QOpenGLWidget
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

    protected:
	void paintEvent(QPaintEvent *) override { }

	void keyPressEvent(QKeyEvent *k) override;
	void mouseMoveEvent(QMouseEvent *e) override;

    signals:
      void renderRequested();

    public slots:
      void grabContext();
      void changed() {
	  // TODO - this almost certainly needs some sort of mutex
	  // protections...
	  m_renderer->changed = true;
      }

    private slots:
      void onAboutToCompose();
      void onFrameSwapped();
      void onAboutToResize();
      void onResized();

    private:
	int x_prev = -INT_MAX;
	int y_prev = -INT_MAX;

	QThread *m_thread;
	DMRenderer *m_renderer;
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

