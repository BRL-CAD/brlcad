/*                      I S S T A P P . C X X
 * BRL-ISST
 *
 * Copyright (c) 2014-2021 United States Government as represented by
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
/** @file cadapp.cxx
 *
 * Application level data and functionality implementations.
 *
 */

#include <QFileInfo>
#include <QFile>
#include <QPlainTextEdit>
#include <QTextStream>
#include "isstapp.h"

int
ISSTApp::load_g(const char *filename, int argc, const char *argv[])
{
    if (g.load_g(filename, argc, argv)) {
	w.statusBar()->showMessage("open failed");
	return -1;
    }

    // The OpenGL widget manages the rendering, so let it know about the
    // TIE data structure associated with the current model
    w.canvas->tie = g.tie;

    // Initialize the camera position
    VSETALL(w.canvas->camera.pos, g.tie->radius);
    VMOVE(w.canvas->camera.focus, g.tie->mid);

    // Record the initial settings for use in subsequent calculations
    VSETALL(w.canvas->camera_pos_init, g.tie->radius);
    VMOVE(w.canvas->camera_focus_init, g.tie->mid);

    return 0;
}


/*
 * Local Variables:
 * mode: C++
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */

