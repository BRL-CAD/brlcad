/*                 M A I N _ W I N D O W . C P P
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
/** @file main_window.cpp
 *
 */

#include <QSplitter>
#include "bv/util.h"
#include "main_window.h"
#include "qtcad/QtCADView.h"
#ifdef BRLCAD_OPENGL
#  include "qtcad/QtGL.h"
#  include "qtcad/QtGLQuad.h"
#endif
#include "dmapp.h"

DM_MainWindow::DM_MainWindow(int canvas_type)
{
    // This solves the disappearing menubar problem on Ubuntu + fluxbox -
    // suspect Unity's "global toolbar" settings are being used even when
    // the Qt app isn't being run under unity - this is probably a quirk
    // of this particular setup, but it sure is an annoying one...
    menuBar()->setNativeMenuBar(false);

    // Create Menus
    file_menu = menuBar()->addMenu("File");
    g_open = new QAction("Open", this);
    connect(g_open, &QAction::triggered, this, &DM_MainWindow::open_file);
    file_menu->addAction(g_open);

    g_save_image = new QAction("Save Image", this);
    connect(g_save_image, &QAction::triggered, this, &DM_MainWindow::save_image);
    file_menu->addAction(g_save_image);

    g_save_settings = new QAction("Save Settings", this);
    connect(g_save_settings, &QAction::triggered, this, &DM_MainWindow::write_settings);
    file_menu->addAction(g_save_settings);

    g_exit = new QAction("Exit", this);
    connect(g_exit, &QAction::triggered , this, &DM_MainWindow::close);
    file_menu->addAction(g_exit);

    // Set up Display canvas
    if (canvas_type != 2) {
	canvas = new QtCADView(this, canvas_type);
	canvas->setMinimumSize(512,512);
    }
#ifdef BRLCAD_OPENGL
    if (canvas_type == 2) {
	c4 = new QtGLQuad(this);
	c4->setMinimumSize(512,512);
    }
#endif
    console = new QtConsole(this);
    console->prompt("$ ");
    wgrp = new QSplitter(Qt::Vertical, this);
    // With the drawing window we do better not to trigger continual redrawing
    // - it's a heavy drawing load and seems to cause occasional issues with
    // the wrong thread trying to grab the OpenGL context
    wgrp->setOpaqueResize(false);
    setCentralWidget(wgrp);
    if (canvas)
	wgrp->addWidget(canvas);
    if (c4) {
	wgrp->addWidget(c4);
	bu_log("Quad View\n");
    }
    wgrp->addWidget(console);

    // The console's run of a command has implications for the entire
    // application, so rather than embedding the command execution logic in the
    // widget we use a signal/slot connection to have the main window's slot
    // execute the command.
    QObject::connect(this->console, &QtConsole::executeCommand, this, &DM_MainWindow::run_cmd);
}

void
DM_MainWindow::open_file()
{
    const char *file_filters = "BRL-CAD (*.g *.asc);;All Files (*)";
    QString fileName = QFileDialog::getOpenFileName((QWidget *)this,
	    "Open Geometry File",
	    qApp->applicationDirPath(),
	    file_filters,
	    NULL,
	    QFileDialog::DontUseNativeDialog);
    if (!fileName.isEmpty()) {
	DMApp *dmApp = qobject_cast<DMApp *>(qApp);
	int ret = dmApp->load_g(fileName.toLocal8Bit(), 0, NULL);
	if (ret) {
	    statusBar()->showMessage("open failed");
	} else {
	    statusBar()->showMessage(fileName);
	}
    }
}

void
DM_MainWindow::run_cmd(const QString &command)
{
    if (BU_STR_EQUAL(command.toStdString().c_str(), "q"))
	bu_exit(0, "exit");

    if (BU_STR_EQUAL(command.toStdString().c_str(), "clear")) {
	console->clear();
	console->prompt("$ ");
	return;
    }

    // make an argv array
    struct bu_vls ged_prefixed = BU_VLS_INIT_ZERO;
    bu_vls_sprintf(&ged_prefixed, "%s", command.toStdString().c_str());
    char *input = bu_strdup(bu_vls_addr(&ged_prefixed));
    bu_vls_free(&ged_prefixed);
    char **av = (char **)bu_calloc(strlen(input) + 1, sizeof(char *), "argv array");
    int ac = bu_argv_from_string(av, strlen(input), input);
    struct bu_vls msg = BU_VLS_INIT_ZERO;

    DMApp *dmApp = qobject_cast<DMApp *>(qApp);
    struct ged **gedpp = &(dmApp->gedp);

    /* The "open" and close commands require a bit of
     * awareness at this level, since the gedp pointer
     * must respond to them and the canvas widget needs
     * some info from the current gedp. */
    int cmd_run = 0;
    if (BU_STR_EQUAL(av[0], "open")) {
	if (ac > 1) {
	    if (*gedpp) {
		ged_close(*gedpp);
	    }
	    (*gedpp) = ged_open("db", av[1], 0);
	    if (!*gedpp) {
		bu_vls_sprintf(&msg, "Could not open %s as a .g file\n", av[1]) ;
		console->printString(bu_vls_cstr(&msg));
	    } else {
		BU_GET((*gedpp)->ged_gvp, struct bview);
		bv_init((*gedpp)->ged_gvp);
		bu_ptbl_ins_unique(&(*gedpp)->ged_views, (long int *)(*gedpp)->ged_gvp);

		if (canvas) {
		    canvas->set_view((*gedpp)->ged_gvp);
		    //canvas->dm_set = (*gedpp)->ged_all_dmp;
		    canvas->set_dm_current((struct dm **)&((*gedpp)->ged_dmp));
		    canvas->set_base2local(&(*gedpp)->ged_wdbp->dbip->dbi_base2local);
		    canvas->set_local2base(&(*gedpp)->ged_wdbp->dbip->dbi_local2base);
		    if (canvas->dmp()) {
			// canvas may already be initialized, so set this here
			dm_set_vp(canvas->dmp(), &canvas->view()->gv_scale);
		    }
		}
#ifdef BRLCAD_OPENGL
		if (c4) {
		    for (int i = 1; i < 5; i++) {
			QtGL *c = c4->get(i);
			c->v = (*gedpp)->ged_gvp; // TODO - ged_gvp will need to be driven by selected QtGL...
			c->dm_set = (*gedpp)->ged_all_dmp;
			c->dm_current = (struct dm **)&((*gedpp)->ged_dmp);
			c->base2local = &(*gedpp)->ged_wdbp->dbip->dbi_base2local;
			c->local2base = &(*gedpp)->ged_wdbp->dbip->dbi_local2base;
			if (c->dmp) {
			    // c may already be initialized, so set these here
			    c->v->dmp = c->dmp;
			    (*c->dm_current) = c->dmp;
			    dm_set_vp(c->dmp, &c->v->gv_scale);
			}
		    }
		}
#endif
		if ((*gedpp)->ged_dmp)
		    bu_ptbl_ins((*gedpp)->ged_all_dmp, (long int *)(*gedpp)->ged_dmp);

		bu_vls_sprintf(&msg, "Opened file %s\n", av[1]);
		console->printString(bu_vls_cstr(&msg));
	    }
	} else {
	    console->printString("Error: invalid ged_open call\n");
	}
	cmd_run = 1;
    }

    if (BU_STR_EQUAL(av[0], "close")) {
	ged_close(*gedpp);
	(*gedpp) = NULL;
	if (canvas) {
	    canvas->set_view(NULL);
	    //canvas->dm_set = NULL;
	    canvas->set_dm_current(NULL);
	    canvas->set_base2local(NULL);
	    canvas->set_local2base(NULL);
	}
#ifdef BRLCAD_OPENGL
	if (c4) {
	    for (int i = 1; i < 5; i++) {
		QtGL *c = c4->get(i);
		c->v = NULL;
		c->dm_set = NULL;
		c->dm_current = NULL;
		c->base2local = NULL;
		c->local2base = NULL;
	    }
	}
#endif
	console->printString("closed database\n");
	cmd_run = 1;
    }

    if (!cmd_run) {
	dmApp->ged_run_cmd(&msg, ac, (const char **)av);
	if (bu_vls_strlen(&msg) > 0) {
	    console->printString(bu_vls_cstr(&msg));
	}
    }
    if (*gedpp) {
	bu_vls_trunc(dmApp->gedp->ged_result_str, 0);
    }

    bu_vls_free(&msg);
    bu_free(input, "input copy");
    bu_free(av, "input argv");

    console->prompt("$ ");
}

void DM_MainWindow::readSettings()
{
    QSettings settings("BRL-CAD", "DM");

    settings.beginGroup("DM_MainWindow");
    resize(settings.value("size", QSize(1100, 800)).toSize());
    settings.endGroup();
}

void DM_MainWindow::write_settings()
{
    QSettings settings("BRL-CAD", "DM");

    settings.beginGroup("DM_MainWindow");
    settings.setValue("size", size());
    settings.endGroup();
}

void DM_MainWindow::save_image()
{
    //if (canvas)
	//canvas->save_image();
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

