/*                   M A I N _ W I N D O W . H
 * BRL-CAD
 *
 * Copyright (c) 2014-2022 United States Government as represented by
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

#ifndef BRLCAD_MAINWINDOW_H
#define BRLCAD_MAINWINDOW_H
#include <QAction>
#include <QDockWidget>
#include <QFileDialog>
#include <QHeaderView>
#include <QMainWindow>
#include <QMenu>
#include <QMenuBar>
#include <QObject>
#include <QSettings>
#include <QStatusBar>
#include <QTreeView>

#include "ged.h"
#include "qtcad/QtCADQuad.h"
#include "qtcad/QgSelectionProxyModel.h"
#include "qtcad/QgTreeView.h"
#include "qtcad/QtCADView.h"
#include "qtcad/QtConsole.h"

#include "plugins/plugin.h"
#include "palettes.h"

// https://stackoverflow.com/q/44707344/2037687
class QBDockWidget : public QDockWidget
{
    Q_OBJECT

    public:
	QBDockWidget(const QString &title, QWidget *parent);
    public slots:
       void toWindow(bool floating);
};

class BRLCAD_MainWindow : public QMainWindow
{
    Q_OBJECT
    public:
	BRLCAD_MainWindow(int canvas_type = 0, int quad_view = 0);

	QtCADQuad *c4 = NULL;
	bool isValid3D();
	void fallback3D();

	QgTreeView *treeview;
	QtConsole *console;
	GEDShellCompleter *cshellcomp;
	CADPalette *vc;
	CADPalette *ic;
	CADPalette *oc;

    public slots:
        //void save_image();
	void do_dm_init();

    private:
	QMenu *file_menu;
	QAction *cad_open;
	QAction *cad_save_settings;
	//QAction *cad_save_image;
	QAction *cad_exit;
	QMenu *view_menu;
	QMenu *help_menu;
	QAction *cad_single_view;
	QAction *cad_quad_view;

	QBDockWidget *console_dock;
	QBDockWidget *tree_dock;
	QDockWidget *vcd;
	QDockWidget *icd;
	QDockWidget *ocd;

};

#endif /* BRLCAD_MAINWINDOW_H */

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

