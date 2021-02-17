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

    tile.format = RENDER_CAMERA_BIT_DEPTH_24;

    camera.type = RENDER_CAMERA_PERSPECTIVE;
    camera.fov = 25;
    render_camera_init(&camera, bu_avail_cpus());
}

// https://stackoverflow.com/a/51666467
void
isstGL::paintEvent(QPaintEvent*)
{
    QPainter painter(this);

    TIENET_BUFFER_SIZE(buffer_image, (uint32_t)(3 * camera.w * camera.h));

    render_camera_render(&camera, tie, &tile, &buffer_image);

    QImage *image = new QImage(buffer_image.data, camera.w, camera.h, QImage::Format_RGB888);
#if 0
    if (!image->save("file.png"))
	printf("save failed!\n");
#endif

    painter.drawImage(this->rect(), *image);
}


void
isstGL::render()
{
    TIENET_BUFFER_SIZE(buffer_image, (uint32_t)(3 * camera.w * camera.h));
    texid = 0;
    if (texdata)
	free(texdata);
    makeCurrent();
    glGenTextures(1, &texid);

    glPixelStorei (GL_UNPACK_ALIGNMENT, 1);
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

    texdata = malloc(camera.w * camera.h * 3);
    glTexImage2D (GL_TEXTURE_2D, 0, GL_RGB, camera.w, camera.h, 0, GL_RGB, GL_UNSIGNED_BYTE, texdata);
    glDisable(GL_LIGHTING);
    glViewport(0,0, camera.w, camera.h);
    glMatrixMode (GL_PROJECTION);
    glLoadIdentity ();
    glOrtho(0, camera.w, camera.h, 0, -1, 1);
    glMatrixMode (GL_MODELVIEW);

    glClear(GL_COLOR_BUFFER_BIT);

    buffer_image.ind = 0;

    VSETALL(camera.pos, tie->radius);
    VMOVE(camera.focus, tie->mid);

    render_phong_init(&camera.render, NULL);

    render_camera_prep(&camera);
    render_camera_render(&camera, tie, &tile, &buffer_image);

    glClear(GL_DEPTH_BUFFER_BIT);
    glLoadIdentity();
    glColor3f(1,1,1);
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, texid);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, camera.w, camera.h, GL_RGB, GL_UNSIGNED_BYTE, buffer_image.data + sizeof(camera_tile_t));
    glBegin(GL_TRIANGLE_STRIP);

    glTexCoord2d(0, 0); glVertex3f(0, 0, 0);
    glTexCoord2d(0, 1); glVertex3f(0, camera.h, 0);
    glTexCoord2d(1, 0); glVertex3f(camera.w, 0, 0);
    glTexCoord2d(1, 1); glVertex3f(camera.w, camera.h, 0);

    glEnd();
    glFlush();
    doneCurrent();

}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8

