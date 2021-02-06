/*                 M A I N _ W I N D O W . C X X
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
/** @file main_window.cxx
 *
 * Implementation code for toplevel window for BRL-CAD GUI.
 *
 */

#include "main_window.h"
#include "cadapp.h"
#include "cadaccordion.h"

BRLCAD_MainWindow::BRLCAD_MainWindow()
{
    // This solves the disappearing menubar problem on Ubuntu + fluxbox -
    // suspect Unity's "global toolbar" settings are being used even when
    // the Qt app isn't being run under unity - this is probably a quirk
    // of this particular setup, but it sure is an annoying one...
    menuBar()->setNativeMenuBar(false);

    // Create Menus
    file_menu = menuBar()->addMenu("File");
    cad_open = new QAction("Open", this);
    connect(cad_open, SIGNAL(triggered()), this, SLOT(open_file()));
    file_menu->addAction(cad_open);

    cad_save_settings = new QAction("Save Settings", this);
    connect(cad_save_settings, SIGNAL(triggered()), this, SLOT(save_settings()));
    file_menu->addAction(cad_save_settings);

    cad_exit = new QAction("Exit", this);
    connect(cad_exit, SIGNAL(triggered()), this, SLOT(close()));
    file_menu->addAction(cad_exit);

    view_menu = menuBar()->addMenu("View");

    menuBar()->addSeparator();

    help_menu = menuBar()->addMenu("Help");


    // Define dock layout
    ads::CDockManager::setConfigFlags(ads::CDockManager::HideSingleCentralWidgetTitleBar);
    dock = new ads::CDockManager(this);

    // TODO - set up our own with the proper values...
    dock->setStyleSheet("");


    // Set up OpenGL canvas
    view_dock = new ads::CDockWidget("Scene");
    view_menu->addAction(view_dock->toggleViewAction());
//    canvas = new QGLWidget();  //TODO - will need to subclass this so libdm/libfb updates are done correctly
    canvas = new Display();  //TODO - will need to subclass this so libdm/libfb updates are done correctly
    canvas->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
    canvas->setMinimumSize(512,512);
    view_dock->setWidget(canvas, ads::CDockWidget::eInsertMode::ForceNoScrollArea);
    view_dock->setMinimumSizeHintMode(ads::CDockWidget::MinimumSizeHintFromContent);
    view_dock->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
    view_dock->setMinimumSize(512,512);
    auto *CentralDockArea = dock->setCentralWidget(view_dock);
    CentralDockArea->setAllowedAreas(ads::DockWidgetArea::OuterDockAreas);

    /* Console */
    console_dock = new ads::CDockWidget("Console");
    view_menu->addAction(console_dock->toggleViewAction());
    console = new CADConsole(console_dock);
    console_dock->setWidget(console);
    console_dock->setMinimumSizeHintMode(ads::CDockWidget::MinimumSizeHintFromContent);
    dock->addDockWidget(ads::BottomDockWidgetArea, console_dock);

   /* Geometry Tree */
    tree_dock = new ads::CDockWidget("Hierarchy");
    view_menu->addAction(tree_dock->toggleViewAction());
    treemodel = new CADTreeModel();
    treeview = new CADTreeView(tree_dock);
    tree_dock->setWidget(treeview);
    tree_dock->setMinimumSizeHintMode(ads::CDockWidget::MinimumSizeHintFromContent);
    treeview->setModel(treemodel);
    treeview->setItemDelegate(new GObjectDelegate());
    treeview->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    treeview->header()->setStretchLastSection(true);
    QObject::connect((CADApp *)qApp, SIGNAL(db_change()), treemodel, SLOT(refresh()));
    QObject::connect((CADApp *)qApp, SIGNAL(db_change()), canvas, SLOT(onDatabaseOpen()));
    QObject::connect(treeview, SIGNAL(expanded(const QModelIndex &)), treeview, SLOT(tree_column_size(const QModelIndex &)));
    QObject::connect(treeview, SIGNAL(collapsed(const QModelIndex &)), treeview, SLOT(tree_column_size(const QModelIndex &)));
    QObject::connect(treeview, SIGNAL(clicked(const QModelIndex &)), treemodel, SLOT(update_selected_node_relationships(const QModelIndex &)));
    QObject::connect(treeview, SIGNAL(expanded(const QModelIndex &)), treemodel, SLOT(expand_tree_node_relationships(const QModelIndex &)));
    QObject::connect(treeview, SIGNAL(collapsed(const QModelIndex &)), treemodel, SLOT(close_tree_node_relationships(const QModelIndex &)));
    QObject::connect(treeview, SIGNAL(customContextMenuRequested(const QPoint&)), treeview, SLOT(context_menu(const QPoint&)));
    treemodel->populate(DBI_NULL);
    ((CADApp *)qApp)->cadtreeview = (CADTreeView *)treeview;
    dock->addDockWidget(ads::LeftDockWidgetArea, tree_dock);

    /* Edit panel */
    panel_dock = new ads::CDockWidget("Edit Panel");
    view_menu->addAction(panel_dock->toggleViewAction());
    panel = new CADAccordion(panel_dock);
    panel_dock->setWidget(panel);
    panel_dock->setMinimumSizeHintMode(ads::CDockWidget::MinimumSizeHintFromContent);
    dock->addDockWidget(ads::RightDockWidgetArea, panel_dock);

    /***** Create and add the widgets that inhabit the dock *****/

    //stdpropview->setMinimumHeight(340);
    QObject::connect(treeview, SIGNAL(clicked(const QModelIndex &)), panel->stdpropmodel, SLOT(refresh(const QModelIndex &)));

    //userpropview->setMinimumHeight(340);
    QObject::connect(treeview, SIGNAL(clicked(const QModelIndex &)), panel->userpropmodel, SLOT(refresh(const QModelIndex &)));
    ((CADApp *)qApp)->cadaccordion= (CADAccordion *)panel;

    /* For testing - don't want uniqueness here, but may need or want it elsewhere */
    //panel->setUniqueVisibility(1);

    /* Set default style for the application */
    QString allstyle;
    QFile stylesheet(":/cadstyle.qss");
    if (stylesheet.open(QIODevice::ReadOnly | QIODevice::Text)) {
	allstyle.append(stylesheet.readAll());
	stylesheet.close();
    }

    QFile treestylesheet(":/cadtreestyle.qss");
    if (treestylesheet.open(QIODevice::ReadOnly | QIODevice::Text)) {
	allstyle.append(treestylesheet.readAll());
	treestylesheet.close();
    }
    qApp->setStyleSheet(allstyle);

}

void
BRLCAD_MainWindow::open_file()
{
    const char *file_filters = "BRL-CAD (*.g *.asc);;Rhino (*.3dm);;STEP (*.stp *.step);;All Files (*)";
    QString fileName = QFileDialog::getOpenFileName((QWidget *)this,
	    "Open Geometry File",
	    qApp->applicationDirPath(),
	    file_filters,
	    NULL,
	    QFileDialog::DontUseNativeDialog);
    if (!fileName.isEmpty()) {
	int ret = ((CADApp *)qApp)->opendb(fileName.toLocal8Bit());
	if (ret) {
	    statusBar()->showMessage("open failed");
	} else {
	    statusBar()->showMessage(fileName);
	}
    }
}

void
BRLCAD_MainWindow::save_settings()
{
    QSettings Settings("Settings.ini", QSettings::IniFormat);
    Settings.setValue("mainWindow/Geometry", this->saveGeometry());
    Settings.setValue("mainWindow/State", this->saveState());
    Settings.setValue("mainWindow/DockingState", dock->saveState());
}

void
BRLCAD_MainWindow::restore_settings()
{
    if (bu_file_exists("Settings.ini", NULL)) {
	QSettings Settings("Settings.ini", QSettings::IniFormat);
	this->restoreGeometry(Settings.value("mainWindow/Geometry").toByteArray());
	this->restoreState(Settings.value("mainWindow/State").toByteArray());
	dock->restoreState(Settings.value("mainWindow/DockingState").toByteArray());
    } else {
	this->resize(1100, 800);
    }
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

