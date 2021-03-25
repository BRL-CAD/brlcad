/*                      I S S T G L . C P P
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
/** @file isstgl.cpp
 *
 * Brief description
 *
 */

#include <QOpenGLWidget>
#include <QKeyEvent>

#include "bu/parallel.h"
#include "isstgl.h"

TIERenderer::TIERenderer(isstGL *w)
    : m_w(w)
{
    // Initialize texture buffer
    TIENET_BUFFER_INIT(buffer_image);
    texdata = realloc(texdata, m_w->camera.w * m_w->camera.h * 3);
    texdata_size = m_w->camera.w * m_w->camera.h;

    // Initialize TIE tile
    //
    // Note:  If orig_x and orig_Y are not initialized, output pixel placement
    // in the buffer may be randomly offset - you may see no image, or an image
    // in the wrong place (or it may happen to work if the values happen to be
    // zero anyway...)
    tile.orig_x = 0;
    tile.orig_y = 0;
    tile.format = RENDER_CAMERA_BIT_DEPTH_24;
}

TIERenderer::~TIERenderer()
{
    TIENET_BUFFER_FREE(buffer_image);
    free(texdata);
}

void TIERenderer::resize()
{
    m_w->makeCurrent();
    if (!m_init) {
	initializeOpenGLFunctions();
	m_init = true;
    }

    // Set tile size
    tile.size_x = m_w->camera.w;
    tile.size_y = m_w->camera.h;

    // Set up the raytracing image buffer
    TIENET_BUFFER_SIZE(buffer_image, (uint32_t)(3 * m_w->camera.w * m_w->camera.h));

    if (texdata_size < m_w->camera.w * m_w->camera.h) {
	texdata_size = m_w->camera.w * m_w->camera.h;
	texdata = realloc(texdata, m_w->camera.w * m_w->camera.h * 3);
    }
    glPixelStorei (GL_UNPACK_ALIGNMENT, 1);
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    glTexImage2D (GL_TEXTURE_2D, 0, GL_RGB, m_w->camera.w, m_w->camera.h, 0, GL_RGB, GL_UNSIGNED_BYTE, texdata);
}

void TIERenderer::render()
{
    // IMPORTANT - this reset is necessary or the resultant image will
    // not display correctly in the buffer.
    buffer_image.ind = 0;

    // Core TIE render
    render_camera_prep(&m_w->camera);
    render_camera_render(&m_w->camera, m_w->tie, &tile, &buffer_image);
}

isstGL::isstGL()
{
   // Initialize TIE camera
    camera.type = RENDER_CAMERA_PERSPECTIVE;
    camera.fov = 25;
    render_camera_init(&camera, bu_avail_cpus());
    render_phong_init(&camera.render, NULL);

    // Create the renderer
    m_renderer = new TIERenderer(this);

    // This is an important Qt setting for interactivity - it allowing key
    // bindings to propagate to this widget and trigger actions such as
    // resolution scaling, rotation, etc.
    setFocusPolicy(Qt::WheelFocus);
}

isstGL::~isstGL()
{
    delete m_renderer;
}

void
isstGL::paintGL()
{
    if (!m_init) {
	initializeOpenGLFunctions();
	m_init = true;
    }

    if (rescaled || do_render) {
	if (rescaled) {
	    rescaled = 0;

	    if (resolution_factor == 0) {
		camera.w = width();
		camera.h = height();
	    } else {
		camera.w = resolution_factor;
		camera.h = camera.w * height() / width();
	    }

	    m_renderer->resize();
	}
	do_render = 0;
	m_renderer->render();
    }

    glDisable(GL_LIGHTING);

    glViewport(0,0, width(), height());
    glMatrixMode (GL_PROJECTION);
    glLoadIdentity ();
    glOrtho(0, width(), height(), 0, -1, 1);
    glMatrixMode (GL_MODELVIEW);

    glClear(GL_COLOR_BUFFER_BIT);

    glClear(GL_DEPTH_BUFFER_BIT);
    glLoadIdentity();
    glColor3f(1,1,1);
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, m_renderer->texid);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, camera.w, camera.h, GL_RGB, GL_UNSIGNED_BYTE, m_renderer->buffer_image.data + sizeof(camera_tile_t));
    glBegin(GL_TRIANGLE_STRIP);

    glTexCoord2d(0, 0); glVertex3f(0, 0, 0);
    glTexCoord2d(0, 1); glVertex3f(0, height(), 0);
    glTexCoord2d(1, 0); glVertex3f(width(), 0, 0);
    glTexCoord2d(1, 1); glVertex3f(width(), height(), 0);

    glEnd();


#if 0
    // Set up a QImage with the rendered output.  Note - sizeof(camera_tile_t)
    // offset copied from glTexSubImage2D setup in Tcl/Tk gui.  Without it, the
    // image is offset to the right in the OpenGL display.
    QImage image(m_renderer->buffer_image.data + sizeof(camera_tile_t), camera.w, camera.h, QImage::Format_RGB888);

    // Get the QImage version of the buffer displayed: https://stackoverflow.com/a/51666467
    QPainter painter(this);
    painter.drawImage(this->rect(), image);
#endif
}


void
isstGL::resizeGL(int w, int h)
{
    // Translated from Tcl/Tk ISST logic for resolution adjustment
    if (resolution_factor == 0) {
	camera.w = w;
	camera.h = h;
    } else {
	camera.w = resolution_factor;
	camera.h = camera.w * h / w;
    }

    m_renderer->resize();

    // We don't want every paint operation to do a full re-render, so we must
    // specify the cases that need it.  resize is definitely one of the ones
    // that does.
    do_render = 1;
}


void isstGL::keyPressEvent(QKeyEvent *k) {
    //QString kstr = QKeySequence(k->key()).toString();
    //bu_log("%s\n", kstr.toStdString().c_str());
    switch (k->key()) {
	// Increase setting controlling raytracing grid density.
	// Maximum is one raytraced pixel per window pixel
	case '=':
	    resolution++;
	    CLAMP(resolution, 1, 20);
	    resolution_factor = (resolution == 20) ? 0 : lrint(floor(width() * .05 * resolution));
	    rescaled = 1;
	    update();
	    return;
	    break;
	// Decrease setting controlling raytracing grid density.
	// Minimum is clamped - too course and the image is meaningless
	case '-':
	    resolution--;
	    CLAMP(resolution, 1, 20);
	    resolution_factor = (resolution == 20) ? 0 : lrint(floor(height() * .05 * resolution));
	    rescaled = 1;
	    update();
	    return;
	    break;
    }
    QOpenGLWidget::keyPressEvent(k);
}


void isstGL::mouseMoveEvent(QMouseEvent *e) {

    bu_log("(%d,%d)\n", e->x(), e->y());
    if (x_prev > -INT_MAX && y_prev > -INT_MAX) {
	bu_log("Delta: (%d,%d)\n", e->x() - x_prev, e->y() - y_prev);
    }

    x_prev = e->x();
    y_prev = e->y();

    QOpenGLWidget::mouseMoveEvent(e);
}

void isstGL::save_image() {
    QImage image = this->grabFramebuffer();
    image.save("file.png");
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8

