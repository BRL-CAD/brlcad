/*                        M A I N . C P P
 * BRL-CAD
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
/** @file main.cpp
 *
 * ISST
 *
 */

#include <iostream>

#include "main_window.h"
#include "isstapp.h"

#include "bu/app.h"
#include "bu/log.h"
#include "brlcad_version.h"

int main(int argc, char *argv[])
{
    bu_setprogname(argv[0]);

    ISSTApp app(argc, argv);
    ISST_MainWindow mainWin;

    argc--; argv++;

    if (argc <= 1) {
	bu_exit(1, "isst file.g obj1 [obj2 ...]");
    }

    if (argc > 1) {
	const char *filename = argv[0];
	argc--; argv++;
	if (app.load_g(filename, argc, (const char **)argv)) {
	    bu_exit(1, "%s%s%s\n", "Error: opening ", filename, " failed.");
	}
    }

    // TODO - this needs to be a setting that is saved and restored
    mainWin.resize(1100, 800);

    // The OpenGL widget manages the rendering, so let it know about the
    // TIE data structure associated with the current model
    mainWin.canvas->tie = app.tie;

    // Initialize the camera position
    VSETALL(mainWin.canvas->camera.pos, app.tie->radius);
    VMOVE(mainWin.canvas->camera.focus, app.tie->mid);

    // Draw the window
    mainWin.show();

    return app.exec();
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
