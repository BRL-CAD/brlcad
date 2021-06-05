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

#ifndef BRLCAD_MAINWINDOW_H
#define BRLCAD_MAINWINDOW_H
#include <QMainWindow>
#include <QDockWidget>
#include <QMenu>
#include <QMenuBar>
#include <QAction>
#include <QStatusBar>
#include <QFileDialog>
#include <QTreeView>
#include <QHeaderView>
#include <QObject>


#include "accordion.h"
#include "qtcad/QtConsole.h"
#include "qtcad/QtCADTree.h"
#include "qtcad/QtCADView.h"

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
	BRLCAD_MainWindow();

	QtCADView *canvas;
	QtConsole *console;

    public slots:
	void run_cmd(const QString &command);

    private slots:
	void open_file();

    private:
	QMenu *file_menu;
	QAction *cad_open;
	QAction *cad_exit;
	QMenu *view_menu;
	QMenu *help_menu;

	QBDockWidget *console_dock;
	QBDockWidget *tree_dock;
	QBDockWidget *panel_dock;

	CADTreeModel *treemodel;
	QTreeView *treeview;
	CADAccordion *panel;
	QString db_file;
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

