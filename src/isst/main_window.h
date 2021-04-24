/*                   M A I N _ W I N D O W . H
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
/** @file main_window.h
 *
 * Defines the toplevel window for the BRL-CAD GUI, into which other
 * windows are docked.
 *
 */

#ifndef ISST_MAINWINDOW_H
#define ISST_MAINWINDOW_H

#include <QMainWindow>
#include <QtWidgets/QOpenGLWidget>
#include <QDockWidget>
#include <QMenu>
#include <QMenuBar>
#include <QAction>
#include <QStatusBar>
#include <QFileDialog>
#include <QSettings>
#include "isstgl.h"

class ISST_MainWindow : public QMainWindow
{
    Q_OBJECT
    public:
	ISST_MainWindow();

	void readSettings();

	isstGL *canvas;

    private slots:
	void open_file();
	void write_settings();
	void save_image();

    private:
	QMenu *file_menu;
	QAction *isst_open;
	QAction *isst_save_image;
	QAction *isst_save_settings;
	QAction *isst_exit;
};

#endif /* ISST_MAINWINDOW_H */

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

