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

#include "main_window.h"
#include "isstapp.h"

ISST_MainWindow::ISST_MainWindow()
{
    // This solves the disappearing menubar problem on Ubuntu + fluxbox -
    // suspect Unity's "global toolbar" settings are being used even when
    // the Qt app isn't being run under unity - this is probably a quirk
    // of this particular setup, but it sure is an annoying one...
    menuBar()->setNativeMenuBar(false);

    // Create Menus
    file_menu = menuBar()->addMenu("File");
    isst_open = new QAction("Open", this);
    connect(isst_open, SIGNAL(triggered()), this, SLOT(open_file()));
    file_menu->addAction(isst_open);

    isst_save_settings = new QAction("Save Settings", this);
    connect(isst_save_settings, SIGNAL(triggered()), this, SLOT(write_settings()));
    file_menu->addAction(isst_save_settings);

    isst_exit = new QAction("Exit", this);
    connect(isst_exit, SIGNAL(triggered()), this, SLOT(close()));
    file_menu->addAction(isst_exit);

    // Set up Display canvas
    //canvas = new QOpenGLWidget();
    canvas = new isstGL();
    canvas->setMinimumSize(512,512);
    setCentralWidget(canvas);

}

void
ISST_MainWindow::open_file()
{
    const char *file_filters = "BRL-CAD (*.g *.asc);;All Files (*)";
    QString fileName = QFileDialog::getOpenFileName((QWidget *)this,
	    "Open Geometry File",
	    qApp->applicationDirPath(),
	    file_filters,
	    NULL,
	    QFileDialog::DontUseNativeDialog);
    if (!fileName.isEmpty()) {
	int ret = ((ISSTApp *)qApp)->load_g(fileName.toLocal8Bit(), 0, NULL);
	if (ret) {
	    statusBar()->showMessage("open failed");
	} else {
	    statusBar()->showMessage(fileName);
	}
    }
}


void ISST_MainWindow::readSettings()
{
    QSettings settings("BRL-CAD", "ISST");

    settings.beginGroup("ISST_MainWindow");
    resize(settings.value("size", QSize(1100, 800)).toSize());
    settings.endGroup();
}

void ISST_MainWindow::write_settings()
{
    QSettings settings("BRL-CAD", "ISST");

    settings.beginGroup("ISST_MainWindow");
    settings.setValue("size", size());
    settings.endGroup();
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

