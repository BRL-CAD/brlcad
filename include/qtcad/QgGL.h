/*                        Q G G L . H
 * BRL-CAD
 *
 * Copyright (c) 2021-2026 United States Government as represented by
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
/** @file QgGL.h
 *
 */

#ifndef QGGL_H
#define QGGL_H

#include "common.h"

#include <QOpenGLFunctions>
#include <QOpenGLWidget>

#include "qtcad/defines.h"
#include "qtcad/QgCanvasBase.h"

class QImage;
class QKeyEvent;
class QMouseEvent;
class QResizeEvent;
class QWheelEvent;

struct bu_ptbl;
struct bsg_view;
struct dm;
struct fb;
struct QgCanvasState;  /* private implementation — defined in QgCanvasState.h */

// Use QOpenGLFunctions so we don't have to prefix all OpenGL calls with "f->"
class QTCAD_EXPORT QgGL : public QOpenGLWidget, protected QOpenGLFunctions, public QgCanvasBase {
Q_OBJECT
Q_DISABLE_COPY_MOVE(QgGL)
Q_PROPERTY(int defaultMouseMode READ lmouseMoveDefault WRITE set_lmouse_move_default)


public:
explicit QgGL(QWidget *parent = nullptr, struct fb *fbp = nullptr);
~QgGL() override;

/* -- QgCanvasBase interface -- */
QWidget *canvasWidget() override { return this; }
QObject *asQObject()    override { return this; }
bool isValid() const    override { return QOpenGLWidget::isValid(); }

struct bsg_view *view()           const override;
struct dm    *displayManager() const override;
struct fb    *frameBuffer()    const override;

void set_view(struct bsg_view *)               override;
void setDisplayManagerSet(struct bu_ptbl *) override;

void stash_hashes() override;
bool diff_hashes()  override;

void aet(double a, double e, double t) override;
void save_image()                      override;
void render_to_file(const QString &filename) override;
void get_viewport_image(QImage &img)   override;

void enableDefaultKeyBindings()    override;
void disableDefaultKeyBindings()   override;
void enableDefaultMouseBindings()  override;
void disableDefaultMouseBindings() override;
int  lmouseMoveDefault() const     override;

int  currentView() const     override;
void set_current(int active) override;

signals:
void changed();
void init_done();

public slots:
void need_update()               override;
void queued_update()             override;
void set_lmouse_move_default(int) override;

protected:
void paintGL() override;
void resizeGL(int w, int h) override;
void resizeEvent(QResizeEvent *e) override;

void keyPressEvent(QKeyEvent *k)       override;
void mouseMoveEvent(QMouseEvent *e)    override;
void mousePressEvent(QMouseEvent *e)   override;
void mouseReleaseEvent(QMouseEvent *e) override;
void wheelEvent(QWheelEvent *e)        override;

private:
QgCanvasState *d = nullptr;
};

#endif /* QGGL_H */

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
