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
#include "bview/util.h"
#include "main_window.h"
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
    if (canvas_type == 0) {
	canvas = new dmGL();
	canvas->setMinimumSize(512,512);
    }
    if (canvas_type == 1) {
	canvas_sw = new dmSW(this);
	canvas_sw->setMinimumSize(512,512);
    }

    console = new pqConsoleWidget(this);
    console->prompt("$ ");
    wgrp = new QSplitter(Qt::Vertical, this);
    // With the drawing window we do better not to trigger continual redrawing
    // - it's a heavy drawing load and seems to cause occasional issues with
    // the wrong thread trying to grab the OpenGL context
    wgrp->setOpaqueResize(false);
    setCentralWidget(wgrp);
    if (canvas)
	wgrp->addWidget(canvas);
    if (canvas_sw) {
	wgrp->addWidget(canvas_sw);
	bu_log("Using OSMesa software rasterizer\n");
    }
    wgrp->addWidget(console);

    QObject::connect(this->console, &pqConsoleWidget::executeCommand, this, &DM_MainWindow::run_cmd);
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
	int ret = ((DMApp *)qApp)->load_g(fileName.toLocal8Bit(), 0, NULL);
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

    if (!gedp) {
	console->printString("No database open");
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
    if (canvas)
	canvas->ged_run_cmd(&msg, ac, (const char **)av);
    if (canvas_sw)
	canvas_sw->ged_run_cmd(&msg, ac, (const char **)av);
    if (bu_vls_strlen(&msg) > 0) {
	console->printString(bu_vls_cstr(&msg));
    }
    bu_vls_trunc(gedp->ged_result_str, 0);
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
    if (canvas)
	canvas->save_image();
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

