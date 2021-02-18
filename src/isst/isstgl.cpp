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

#include "bu/parallel.h"
#include "isstgl.h"

isstGL::isstGL()
{
    TIENET_BUFFER_INIT(buffer_image);
    texdata = realloc(texdata, camera.w * camera.h * 3);
    texdata_size = camera.w * camera.h;

    tile.format = RENDER_CAMERA_BIT_DEPTH_24;

    // If these are not initialized, output pixel placement in the buffer may
    // be randomly offset - you may see no image, or an image in the wrong
    // place (or it may happen to work if the values happen to be zero
    // anyway...)
    tile.orig_x = 0;
    tile.orig_y = 0;

    camera.type = RENDER_CAMERA_PERSPECTIVE;
    camera.fov = 25;
    render_camera_init(&camera, bu_avail_cpus());
    render_phong_init(&camera.render, NULL);
}

isstGL::~isstGL()
{
    TIENET_BUFFER_FREE(buffer_image);
    free(texdata);
}

void
isstGL::paintGL()
{
    // IMPORTANT - this reset is necessary or the resultant image will
    // not display correctly in the buffer.
    buffer_image.ind = 0;

    // Core TIE render
    render_camera_prep(&camera);
    render_camera_render(&camera, tie, &tile, &buffer_image);

    // Get the rendered buffer displayed: https://stackoverflow.com/a/51666467
    QImage *image = new QImage(buffer_image.data, camera.w, camera.h, QImage::Format_RGB888);
    QPainter painter(this);
    painter.drawImage(this->rect(), *image);

#if 0
    // If we need to debug the above, we can write out an image
    if (!image->save("file.png"))
	printf("save failed!\n");
#endif
}


void
isstGL::resizeGL(int w, int h)
{
    camera.w = w;
    camera.h = h;
    tile.size_x = camera.w;
    tile.size_y = camera.h;

    // Set up the raytracing image buffer
    TIENET_BUFFER_SIZE(buffer_image, (uint32_t)(3 * camera.w * camera.h));

    // Set up the corresponding texture memory in OpenGL.
    if (texdata_size < camera.w * camera.h) {
	texdata_size = camera.w * camera.h;
	texdata = realloc(texdata, camera.w * camera.h * 3);
    }
    glPixelStorei (GL_UNPACK_ALIGNMENT, 1);
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    glTexImage2D (GL_TEXTURE_2D, 0, GL_RGB, camera.w, camera.h, 0, GL_RGB, GL_UNSIGNED_BYTE, texdata);
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8

