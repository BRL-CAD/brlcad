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

BRLCAD_MainWindow::BRLCAD_MainWindow()
{
    setUnifiedTitleAndToolBarOnMac(true); // Not sure yet if we want this, but put it in as a reminder

    // Create Menus

    file_menu = menuBar()->addMenu("&File");
    cad_exit = new QAction("Exit", this);
    connect(cad_exit, SIGNAL(triggered()), this, SLOT(close()));
    file_menu->addAction(cad_exit);


    // Set up OpenGL canvas
    canvas = new QGLWidget();  //TODO - will need to subclass this so libdm/libfb updates are done correctly
    setCentralWidget(canvas);

    // Define dock layout
    console_dock = new QDockWidget("Console", this);
    addDockWidget(Qt::BottomDockWidgetArea, console_dock);
    console_dock->setAllowedAreas(Qt::BottomDockWidgetArea);

    tree_dock = new QDockWidget("Hierarchy", this);
    addDockWidget(Qt::LeftDockWidgetArea, tree_dock);
    tree_dock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);

    panel_dock = new QDockWidget("Edit Panel", this);
    addDockWidget(Qt::RightDockWidgetArea, panel_dock);
    panel_dock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);

    /* Because the console usually doesn't need a huge amount of
     * horizontal space and the tree can use all the vertical space
     * it can get, give the bottom corners to the left/right docks */
    setCorner(Qt::BottomLeftCorner, Qt::LeftDockWidgetArea);
    setCorner(Qt::BottomRightCorner, Qt::RightDockWidgetArea);

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

