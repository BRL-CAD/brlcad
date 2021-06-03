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
#include <string>
#include <QSettings>

#include <tcl.h>

#include "main_window.h"
#include "dmapp.h"
#include "fbserv.h"

#include "bu/app.h"
#include "bu/log.h"
#include "bu/str.h"

int main(int argc, char *argv[])
{

    bu_setprogname(argv[0]);

    Tcl_CreateInterp();

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
	if (BU_STR_EQUAL(argv[0], "-4")) {
	    // If we had a -4 option it was handled in app creation.
	    // Eventually this should be done properly with bu_opt...
	    argc--; argv++;
	}
	if (argc) {
	    const char *filename = argv[0];
	    argc--; argv++;
	    app.load_g(filename, argc, (const char **)argv);
	}
    }

#if 0
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
#endif

    // Draw the window
    app.w->show();

    // If we're trying to use system OpenGL and it doesn't work, fall
    // back on the software option
    if (!app.w->c4) {
	if (!app.w->canvas->isValid()) {
	    app.w->canvas->fallback();
	    if (app.gedp) {
		app.gedp->fbs_open_client_handler = &qdm_open_sw_client_handler;
	    }
	}
    } else {
	if (!app.w->c4->isValid()) {
	    app.w->c4->fallback();
	    if (app.gedp) {
		app.gedp->fbs_open_client_handler = &qdm_open_sw_client_handler;
	    }
	}
    }

#if 0
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
#endif

    // If we have a GED structure, connect the wires
    if (app.gedp) {
	if (app.w->canvas) {
	    BU_GET(app.gedp->ged_gvp, struct bview);
	    bv_init(app.gedp->ged_gvp);
	    bu_vls_sprintf(&app.gedp->ged_gvp->gv_name, "default");
	    app.gedp->ged_gvp->gv_db_grps = &app.gedp->ged_db_grps;
	    app.gedp->ged_gvp->independent = 0;
	    bu_ptbl_ins_unique(&app.gedp->ged_views, (long int *)app.gedp->ged_gvp);
	    app.w->canvas->set_view(app.gedp->ged_gvp);
	    //app.w->canvas->dm_set = app.gedp->ged_all_dmp;
	    app.w->canvas->set_dm_current((struct dm **)&app.gedp->ged_dmp);
	    app.w->canvas->set_base2local(&app.gedp->ged_wdbp->dbip->dbi_base2local);
	    app.w->canvas->set_local2base(&app.gedp->ged_wdbp->dbip->dbi_local2base);
	    app.gedp->ged_gvp = app.w->canvas->view();
	}
	if (app.w->c4) {
	    for (int i = 1; i < 5; i++) {
		QtCADView *c = app.w->c4->get(i);
		struct bview *nv;
		BU_GET(nv, struct bview);
		bv_init(nv);
		bu_vls_sprintf(&nv->gv_name, "Q%d", i);
		nv->gv_db_grps = &app.gedp->ged_db_grps;
		nv->independent = 0;
		bu_ptbl_ins_unique(&app.gedp->ged_views, (long int *)nv);
		c->set_view(nv);
		//c->dm_set = app.gedp->ged_all_dmp;
		c->set_dm_current((struct dm **)&app.gedp->ged_dmp);
		c->set_base2local(&app.gedp->ged_wdbp->dbip->dbi_base2local);
		c->set_local2base(&app.gedp->ged_wdbp->dbip->dbi_local2base);
	    }
	    app.w->c4->cv = &app.gedp->ged_gvp;
	    app.w->c4->select(1);
	    app.w->c4->default_views();
	}
    }

    int have_msg = 0;
    std::string ged_msgs(ged_init_msgs());
    if (ged_msgs.size()) {
	app.w->console->printString(ged_msgs.c_str());
	app.w->console->printString("\n");
	have_msg = 1;
    }
    std::string dm_msgs(dm_init_msgs());
    if (dm_msgs.size()) {
	if (dm_msgs.find("qtgl") != std::string::npos || dm_msgs.find("swrast") != std::string::npos) {
	    app.w->console->printString(dm_msgs.c_str());
	    app.w->console->printString("\n");
	    have_msg = 1;
	}
    }
    if (have_msg)
	app.w->console->prompt("$ ");

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
