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
#include <QSettings>

#include "main_window.h"
#include "isstapp.h"

#include "bu/app.h"
#include "bu/log.h"

int main(int argc, char *argv[])
{

    bu_setprogname(argv[0]);

    ISSTApp app(argc, argv);
    app.setOrganizationName("BRL-CAD");
    app.setOrganizationDomain("brlcad.org");
    app.setApplicationName("ISST");

    app.w.readSettings();

    QSettings isstsettings("BRL-CAD", "ISST");
    std::cout << "Reading settings from " << isstsettings.fileName().toStdString() << "\n";

    argc--; argv++;

    // If we have command line arguments, process them
    if (argc > 1) {
	const char *filename = argv[0];
	argc--; argv++;
	app.load_g(filename, argc, (const char **)argv);
    }

    // This is an illustration of how to force an exact size for
    // the OpenGL canvas.  Useful when we need a framebuffer window
    // to exactly match a specified size.
    QSize cminsize = app.w.canvas->minimumSize();
    QSize cmaxsize = app.w.canvas->maximumSize();
    app.w.canvas->setMinimumSize(1100,800);
    app.w.canvas->setMaximumSize(1100,800);
    app.w.canvas->updateGeometry();

    // Draw the window
    app.w.show();

    // Having forced the size we wanted, restore the original settings
    // to allow for subsequent change (if it would have been allowed
    // by the original settings.)
    app.w.canvas->setMinimumSize(cminsize);
    app.w.canvas->setMaximumSize(cmaxsize);

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
