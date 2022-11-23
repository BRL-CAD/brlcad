/*                A X E S R E N D E R E R . C P P
 * BRL-CAD
 *
 * Copyright (c) 2020-2022 United States Government as represented by
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
/** @file AxesRenderer.cpp */

#include "common.h"

#if defined(WIN32) && !defined(__CYGWIN__)
#include<windows.h>
#endif

#ifdef HAVE_GL_GL_H
#  include <GL/gl.h>
#endif

#ifdef HAVE_OPENGL_GL_H
#  include <OpenGL/gl.h>
#endif

#include "AxesRenderer.h"

#define GRID_LINE_LENGTH 100000

void AxesRenderer::render() {
    glBegin(GL_LINES);
    glColor3f(0,1,0);
    glVertex3f(0, GRID_LINE_LENGTH, 0);
    glVertex3f(0, -0, 0);

    glColor3f(0,0,1);
    glVertex3f(0, 0, GRID_LINE_LENGTH);
    glVertex3f(0, 0, -0);

    glColor3f(1,0,0);
    glVertex3f( GRID_LINE_LENGTH, 0,0);
    glVertex3f( -0, 0,0);
    glEnd();
}
