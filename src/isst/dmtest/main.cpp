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
 * DM test app
 *
 */

#include <iostream>
#include <QSettings>

#include "main_window.h"
#include "dmapp.h"

#include "bu/app.h"
#include "bu/log.h"
#include "bu/str.h"

int main(int argc, char *argv[])
{

    bu_setprogname(argv[0]);

    QSurfaceFormat format;
    format.setDepthBufferSize(16);
    QSurfaceFormat::setDefaultFormat(format);

    DMApp app(argc, argv);
    app.setOrganizationName("BRL-CAD");
    app.setOrganizationDomain("brlcad.org");
    app.setApplicationName("DM");

    app.w->readSettings();

    QSettings dmsettings("BRL-CAD", "DM");
    std::cout << "Reading settings from " << dmsettings.fileName().toStdString() << "\n";

    argc--; argv++;

    /* FIXME: To draw, we need to init this LIBRT global */
    BU_LIST_INIT(&RTG.rtg_vlfree);

    // If we have command line arguments, process them
    if (argc) {
	if (BU_STR_EQUAL(argv[0], "-s")) {
	    // If we had a -s option it was handled in app creation.
	    // Eventually this should be done properly with bu_opt...
	    argc--; argv++;
	}
	if (argc) {
	    const char *filename = argv[0];
	    argc--; argv++;
	    app.load_g(filename, argc, (const char **)argv);
	    // Make sure libged knows not to nuke our view
	    app.gedp->using_app_views = 1;
	}
    }

    // This is an illustration of how to force an exact size for
    // the OpenGL canvas.  Useful when we need a framebuffer window
    // to exactly match a specified size.
    QSize cminsize, cmaxsize;
    if (app.w->canvas) {
	cminsize = app.w->canvas->minimumSize();
	cmaxsize = app.w->canvas->maximumSize();
	app.w->canvas->setMinimumSize(1100,800);
	app.w->canvas->setMaximumSize(1100,800);
	app.w->canvas->updateGeometry();
    }
    if (app.w->canvas_sw) {
	cminsize = app.w->canvas_sw->minimumSize();
	cmaxsize = app.w->canvas_sw->maximumSize();
	app.w->canvas_sw->setMinimumSize(1100,800);
	app.w->canvas_sw->setMaximumSize(1100,800);
	app.w->canvas_sw->updateGeometry();
    }

    // Draw the window
    app.w->show();

    // If we're trying to use system OpenGL and it doesn't work, fall
    // back on the software option
    if (app.w->canvas && !app.w->canvas->isValid()) {
	bu_log("System OpenGL Canvas didn't work, falling back on Software Rasterizer\n");
	app.w->canvas_sw = new QtSW(app.w);
	app.w->canvas_sw->setMinimumSize(512,512);
	// We should get the same size as before...
	app.w->wgrp->replaceWidget(0, app.w->canvas_sw);
	if (app.gedp)
	    app.gedp->ged_gvp = app.w->canvas_sw->v;
	delete app.w->canvas;
	app.w->canvas = NULL;
    }

    // Having forced the size we wanted, restore the original settings
    // to allow for subsequent change (if it would have been allowed
    // by the original settings.)
    if (app.w->canvas) {
	app.w->canvas->setMinimumSize(cminsize);
	app.w->canvas->setMaximumSize(cmaxsize);
    }
    if (app.w->canvas_sw) {
	app.w->canvas_sw->setMinimumSize(cminsize);
	app.w->canvas_sw->setMaximumSize(cmaxsize);
    }

    // If we have a GED structure, connect the wires
    if (app.gedp) {
	if (app.w->canvas) {
	    app.gedp->ged_dmp = app.w->canvas->dmp;
	    app.gedp->ged_gvp = app.w->canvas->v;
	    app.w->canvas->v->gv_base2local = app.gedp->ged_wdbp->dbip->dbi_base2local;
	    app.w->canvas->v->gv_local2base = app.gedp->ged_wdbp->dbip->dbi_local2base;
	    if (app.w->canvas->dmp)
		dm_set_vp(app.w->canvas->dmp, &app.gedp->ged_gvp->gv_scale);
	}
	if (app.w->canvas_sw) {
	    app.gedp->ged_dmp = app.w->canvas_sw->dmp;
	    app.gedp->ged_gvp = app.w->canvas_sw->v;
	    app.w->canvas_sw->v->gv_base2local = app.gedp->ged_wdbp->dbip->dbi_base2local;
	    app.w->canvas_sw->v->gv_local2base = app.gedp->ged_wdbp->dbip->dbi_local2base;
	    if (app.w->canvas_sw->dmp)
		dm_set_vp(app.w->canvas_sw->dmp, &app.gedp->ged_gvp->gv_scale);
	}
	bu_ptbl_ins(app.gedp->ged_all_dmp, (long int *)app.gedp->ged_dmp);
    }

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
