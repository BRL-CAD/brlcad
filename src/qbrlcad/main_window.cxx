/*                 M A I N _ W I N D O W . C X X
 * BRL-CAD
 *
 * Copyright (c) 2014 United States Government as represented by
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
#include "cadtreeview.h"

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

    cad_exit = new QAction("Exit", this);
    connect(cad_exit, SIGNAL(triggered()), this, SLOT(close()));
    file_menu->addAction(cad_exit);

    view_menu = menuBar()->addMenu("View");

    menuBar()->addSeparator();

    help_menu = menuBar()->addMenu("Help");

    // Set up OpenGL canvas
    canvas = new QGLWidget();  //TODO - will need to subclass this so libdm/libfb updates are done correctly
    setCentralWidget(canvas);


    // Define dock layout
    console_dock = new QDockWidget("Console", this);
    addDockWidget(Qt::BottomDockWidgetArea, console_dock);
    console_dock->setAllowedAreas(Qt::BottomDockWidgetArea);
    view_menu->addAction(console_dock->toggleViewAction());

    tree_dock = new QDockWidget("Hierarchy", this);
    addDockWidget(Qt::LeftDockWidgetArea, tree_dock);
    tree_dock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    view_menu->addAction(tree_dock->toggleViewAction());

    panel_dock = new QDockWidget("Edit Panel", this);
    addDockWidget(Qt::RightDockWidgetArea, panel_dock);
    panel_dock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    view_menu->addAction(panel_dock->toggleViewAction());

    /* Because the console usually doesn't need a huge amount of
     * horizontal space and the tree can use all the vertical space
     * it can get, give the bottom corners to the left/right docks */
    setCorner(Qt::BottomLeftCorner, Qt::LeftDockWidgetArea);
    setCorner(Qt::BottomRightCorner, Qt::RightDockWidgetArea);


    /***** Create and add the widgets that inhabit the dock *****/

    /* Console */
    console = new CADConsole(console_dock);
    console_dock->setWidget(console);

    /* Geometry Tree */
    treemodel = new CADTreeModel();
    treeview = new CADTreeView(tree_dock);
    tree_dock->setWidget(treeview);
    treeview->setModel(treemodel);
    treeview->setItemDelegate(new GObjectDelegate());
    treeview->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    treeview->header()->setStretchLastSection(true);
    QObject::connect((CADApp *)qApp, SIGNAL(db_change()), treemodel, SLOT(refresh()));
    QObject::connect(treeview, SIGNAL(expanded(const QModelIndex &)), treeview, SLOT(tree_column_size(const QModelIndex &)));
    QObject::connect(treeview, SIGNAL(collapsed(const QModelIndex &)), treeview, SLOT(tree_column_size(const QModelIndex &)));
    QObject::connect(treeview, SIGNAL(clicked(const QModelIndex &)), treemodel, SLOT(update_selected_node_relationships(const QModelIndex &)));
    QObject::connect(treeview, SIGNAL(expanded(const QModelIndex &)), treemodel, SLOT(expand_tree_node_relationships(const QModelIndex &)));
    QObject::connect(treeview, SIGNAL(collapsed(const QModelIndex &)), treemodel, SLOT(close_tree_node_relationships(const QModelIndex &)));
    QObject::connect(treeview, SIGNAL(customContextMenuRequested(const QPoint&)), treeview, SLOT(context_menu(const QPoint&)));
    treemodel->populate(DBI_NULL);
    ((CADApp *)qApp)->cadtreeview = (CADTreeView *)treeview;

    /* Edit panel */
    panel = new QAccordianWidget(panel_dock);
    panel_dock->setWidget(panel);

    QPushButton *obj1_c = new QPushButton("contents 1");
    obj1_c->setMinimumHeight(300);
    QAccordianObject *obj1 = new QAccordianObject(panel, obj1_c, "Accordian Object 1");
    panel->addObject(obj1);

    QPushButton *obj2_c = new QPushButton("contents 2");
    obj2_c->setMinimumHeight(340);
    QAccordianObject *obj2 = new QAccordianObject(panel, obj2_c, "Accordian Object 2");
    panel->addObject(obj2);

    QPushButton *obj3_c = new QPushButton("contents 3");
    obj3_c->setMinimumHeight(100);
    QAccordianObject *obj3 = new QAccordianObject(panel, obj3_c, "Accordian Object 3");
    panel->addObject(obj3);

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
    QString fileName = QFileDialog::getOpenFileName(this,
	    "Open Geometry File",
	    qApp->applicationDirPath(),
	    "BRL-CAD (*.g *.asc);;Rhino (*.3dm);;STEP (*.stp *.step);;All Files (*)");
    if (!fileName.isEmpty()) {
	int ret = ((CADApp *)qApp)->opendb(fileName.toLocal8Bit());
	if (ret) {
	    statusBar()->showMessage("open failed");
	} else {
	    statusBar()->showMessage(fileName);
	}
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

