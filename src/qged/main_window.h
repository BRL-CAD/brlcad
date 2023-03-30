/*                   M A I N _ W I N D O W . H
 * BRL-CAD
 *
 * Copyright (c) 2014-2023 United States Government as represented by
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
#include "qtcad/SignalFlags.h"
#include "qtcad/QtCADQuad.h"
#include "qtcad/QtCADView.h"
#include "qtcad/QgDockWidget.h"
#include "qtcad/QgTreeView.h"
#include "qtcad/QtCADView.h"
#include "qtcad/QtConsole.h"
#include "qtcad/QViewCtrl.h"

#include "plugins/plugin.h"
#include "attributes.h"
#include "palettes.h"

class BRLCAD_MainWindow : public QMainWindow
{
    Q_OBJECT
    public:
	BRLCAD_MainWindow(int canvas_type = 0, int quad_view = 0);

	QtConsole *console;

	// Post-show methods for checking validity of OpenGL initialization
	bool isValid3D();

	// Report if the quad/central widget is active
	bool isDisplayActive();

	// Get the currently active view of the quad/central display widget
	QtCADView * CurrentDisplay();
	struct bview * CurrentView();

	// Checkpoint display state (used for subsequent diff)
	void DisplayCheckpoint();

	// See if the display state has changed relative to most recent checkpoint
	bool DisplayDiff();

	// Apply visual window changes indicating a raytrace has begun
	void IndicateRaytraceStart(int);

	// Clear visual window changes indicating a raytrace has begun
	void IndicateRaytraceDone(int);

	// Determine interaction mode based on selected palettes and the supplied point
	int InteractionMode(QPoint &gpos);

	// Utility wrapper for the closeEvent to save windowing dimensions
	void closeEvent(QCloseEvent* e);

    public slots:
	//void save_image();
	void do_dm_init();
        void close();
	// Put central display into Quad mode
	void QuadDisplay();
	// Put central display into Single mode
	void SingleDisplay();

    private:

	void CreateWidgets(int canvas_type);
	void LocateWidgets();
	void ConnectWidgets();
	void SetupMenu();

	// Menu actions
	QAction *cad_open;
	QAction *cad_save_settings;
	//QAction *cad_save_image;
	QAction *cad_exit;

	// Organizational widget
	QWidget *cw = NULL;

	// Central widgets
	QViewCtrl *vcw = NULL;
	QtCADQuad *c4 = NULL;
	QAction *cad_single_view = NULL;
	QAction *cad_quad_view = NULL;

	// Docked widgets
	CADAttributesModel *stdpropmodel = NULL;
	CADAttributesModel *userpropmodel = NULL;
	CADPalette *oc = NULL;
	CADPalette *vc = NULL;
	QgTreeView *treeview = NULL;

	// Action for toggling treeview's ls or tree view
	QAction *vm_treeview_mode_toggle = NULL;

	// Docking containers
	QDockWidget *ocd = NULL;
	QDockWidget *sattrd = NULL;
	QDockWidget *uattrd = NULL;
	QDockWidget *vcd = NULL;
	QMenu *vm_panels = NULL;
	QgDockWidget *console_dock = NULL;
	QgDockWidget *tree_dock = NULL;
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

