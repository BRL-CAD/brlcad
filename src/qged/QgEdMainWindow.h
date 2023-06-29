/*                 Q G E D M A I N W I N D O W . H
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
/** @file QgEdMainWindow.h
 *
 * Defines the toplevel window for the BRL-CAD GUI, into which other windows
 * are docked.
 *
 * This widget is also responsible for connections between widgets - typically
 * this takes the form of Qt signals/slots that pass information to/from
 * widgets and keep the graphically displayed information in sync.
 */

#ifndef QGEDMAINWINDOW_H
#define QGEDMAINWINDOW_H
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
#include "qtcad/QgAttributesModel.h"
#include "qtcad/QgConsole.h"
#include "qtcad/QgDockWidget.h"
#include "qtcad/QgQuadView.h"
#include "qtcad/QgSignalFlags.h"
#include "qtcad/QgTreeView.h"
#include "qtcad/QgView.h"
#include "qtcad/QgView.h"
#include "qtcad/QgViewCtrl.h"

#include "plugins/plugin.h"
#include "QgEdPalette.h"

class QgEdMainWindow : public QMainWindow
{
    Q_OBJECT
    public:
	QgEdMainWindow(int canvas_type = 0, int quad_view = 0);

	QgConsole *console;

	// Post-show methods for checking validity of OpenGL initialization
	bool isValid3D();

	// Report if the quad/central widget is active
	bool isDisplayActive();

	// Get the currently active view of the quad/central display widget
	QgView * CurrentDisplay();
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
	QgViewCtrl *vcw = NULL;
	QgQuadView *c4 = NULL;
	QAction *cad_single_view = NULL;
	QAction *cad_quad_view = NULL;

	// Docked widgets
	QgAttributesModel *stdpropmodel = NULL;
	QgAttributesModel *userpropmodel = NULL;
	QgEdPalette *oc = NULL;
	QgEdPalette *vc = NULL;
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

#endif /* QGEDMAINWINDOW_H */

// Local Variables:
// mode: C++
// tab-width: 8
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8

