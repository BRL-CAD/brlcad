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

#if defined(__GNUC__) && (__GNUC__ == 4 && __GNUC_MINOR__ < 6) && !defined(__clang__)
#  pragma message "Disabling GCC float equality comparison warnings via pragma due to Qt headers..."
#endif
#if defined(__GNUC__) && (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 6)) && !defined(__clang__)
#  pragma GCC diagnostic push
#endif
#if defined(__clang__)
#  pragma clang diagnostic push
#endif
#if defined(__GNUC__) && (__GNUC__ == 4 && __GNUC_MINOR__ >= 3) && !defined(__clang__)
#  pragma GCC diagnostic ignored "-Wfloat-equal"
#endif
#if defined(__clang__)
#  pragma clang diagnostic ignored "-Wfloat-equal"
#endif
#undef Success
#include <QMainWindow>
#undef Success
#include <QGLWidget>
#ifndef ENABLE_QTADS
#  undef Success
#  include <QDockWidget>
#endif
#undef Success
#include <QMenu>
#undef Success
#include <QMenuBar>
#undef Success
#include <QAction>
#undef Success
#include <QStatusBar>
#undef Success
#include <QFileDialog>
#undef Success
#include <QTreeView>
#undef Success
#include <QHeaderView>
#undef Success
#include <QObject>
#if defined(__GNUC__) && (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 6)) && !defined(__clang__)
#  pragma GCC diagnostic pop
#endif
#if defined(__clang__)
#  pragma clang diagnostic pop
#endif

#ifdef ENABLE_QTADS
#  include "qtads/DockManager.h"
#  include "qtads/DockAreaWidget.h"
#endif
#include "cadconsole.h"
#include "cadtreemodel.h"
#include "cadaccordion.h"
#include <display/Display.h>

class BRLCAD_MainWindow : public QMainWindow
{
    Q_OBJECT
    public:
	BRLCAD_MainWindow();
#ifdef ENABLE_QTADS
	void restore_settings();
#endif

	BRLCADDisplay *canvas;

    private slots:
	void open_file();
#ifdef ENABLE_QTADS
	void save_settings();
#endif

    private:
	QMenu *file_menu;
	QAction *cad_open;
#ifdef ENABLE_QTADS
	QAction *cad_save_settings;
	QAction *cad_restore_settings;
#endif
	QAction *cad_exit;
	QMenu *view_menu;
	QMenu *help_menu;

#ifdef ENABLE_QTADS
	ads::CDockManager *dock;
	ads::CDockWidget *view_dock;
	ads::CDockWidget *console_dock;
	ads::CDockWidget *tree_dock;
	ads::CDockWidget *panel_dock;
#else
	QDockWidget *console_dock;
	QDockWidget *tree_dock;
	QDockWidget *panel_dock;
#endif

	CADConsole *console;
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

