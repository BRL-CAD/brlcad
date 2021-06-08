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

#include <QTimer>
#include "main_window.h"
#include "app.h"
#include "palettes.h"
#include "attributes.h"

QBDockWidget::QBDockWidget(const QString &title, QWidget *parent)
    : QDockWidget(title, parent)
{
}

void QBDockWidget::toWindow(bool floating)
{
    if (floating) {
	setWindowFlags(
		Qt::CustomizeWindowHint |
                Qt::Window |
                Qt::WindowMinimizeButtonHint |
                Qt::WindowMaximizeButtonHint |
		Qt::WindowCloseButtonHint
		);
	// undo "setWindowFlags" hiding the widget
	show();
    }
}

BRLCAD_MainWindow::BRLCAD_MainWindow(int canvas_type, int quad_view)
{
    // This solves the disappearing menubar problem on Ubuntu + fluxbox -
    // suspect Unity's "global toolbar" settings are being used even when
    // the Qt app isn't being run under unity - this is probably a quirk
    // of this particular setup, but it sure is an annoying one...
    menuBar()->setNativeMenuBar(false);

    // Redrawing the main canvas may be expensive when docking and undocking -
    // disable animation to minimize window drawing operations:
    // https://stackoverflow.com/a/17885699/2037687
    setAnimated(false);

    // Create Menus
    file_menu = menuBar()->addMenu("File");
    cad_open = new QAction("Open", this);
    QObject::connect(cad_open, &QAction::triggered, this, &BRLCAD_MainWindow::open_file);
    file_menu->addAction(cad_open);

    cad_exit = new QAction("Exit", this);
    QObject::connect(cad_exit, &QAction::triggered, this, &BRLCAD_MainWindow::close);
    file_menu->addAction(cad_exit);

    view_menu = menuBar()->addMenu("View");

    menuBar()->addSeparator();

    help_menu = menuBar()->addMenu("Help");

     // Set up 3D display.  Unlike MGED, we don't create 4 views by default for
     // quad view, since some drawing modes demand more memory for 4 views.
     // Use a single canvas unless the user settings specify quad.
    if (!quad_view) {
	canvas = new QtCADView(this, canvas_type);
	canvas->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
	canvas->setMinimumSize(512,512);
	setCentralWidget(canvas);
    } else {
	c4 = new QtCADQuad(this, canvas_type);
	c4->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
	c4->setMinimumSize(512,512);
	setCentralWidget(c4);
    }

    // Define dock widgets - these are the console, controls, etc. that can be attached
    // and detached from the main window.  Eventually this should be a dynamic set
    // of widgets rather than hardcoded statics, so plugins can define their own
    // graphical elements...

    QBDockWidget *vcd = new QBDockWidget("View Controls", this);
    addDockWidget(Qt::RightDockWidgetArea, vcd);
    vcd->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    view_menu->addAction(vcd->toggleViewAction());
    CADViewControls *vc = new CADViewControls(this);
    vcd->setWidget(vc);
    // Make sure the buttons are all visible - trick from
    // https://stackoverflow.com/a/56852841/2037687
    QTimer::singleShot(0, vc, &CADViewControls::reflow);

    QBDockWidget *icd = new QBDockWidget("Instance Editing", this);
    addDockWidget(Qt::RightDockWidgetArea, icd);
    icd->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    view_menu->addAction(icd->toggleViewAction());
    CADInstanceEdit *ic = new CADInstanceEdit(this);
    icd->setWidget(ic);
    // Make sure the buttons are all visible - trick from
    // https://stackoverflow.com/a/56852841/2037687
    QTimer::singleShot(0, ic, &CADInstanceEdit::reflow);

    QBDockWidget *ocd = new QBDockWidget("Object Editing", this);
    addDockWidget(Qt::RightDockWidgetArea, ocd);
    ocd->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    view_menu->addAction(ocd->toggleViewAction());
    CADPrimitiveEdit *oc = new CADPrimitiveEdit(this);
    ocd->setWidget(oc);
    // Make sure the buttons are all visible - trick from
    // https://stackoverflow.com/a/56852841/2037687
    QTimer::singleShot(0, oc, &CADPrimitiveEdit::reflow);

    QBDockWidget *sattrd = new QBDockWidget("Standard Attributes", this);
    addDockWidget(Qt::RightDockWidgetArea, sattrd);
    sattrd->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    view_menu->addAction(sattrd->toggleViewAction());
    CADAttributesModel *stdpropmodel = new CADAttributesModel(0, DBI_NULL, RT_DIR_NULL, 1, 0);
    CADAttributesView *stdpropview = new CADAttributesView(this, 1);
    stdpropview->setModel(stdpropmodel);
    sattrd->setWidget(stdpropview);

    QBDockWidget *uattrd = new QBDockWidget("User Attributes", this);
    addDockWidget(Qt::RightDockWidgetArea, uattrd);
    uattrd->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    view_menu->addAction(uattrd->toggleViewAction());
    CADAttributesModel *userpropmodel = new CADAttributesModel(0, DBI_NULL, RT_DIR_NULL, 0, 1);
    CADAttributesView *userpropview = new CADAttributesView(this, 0);
    userpropview->setModel(userpropmodel);
    uattrd->setWidget(userpropview);

    /* Because the console usually doesn't need a huge amount of horizontal
     * space and the tree can use all the vertical space it can get when
     * viewing large .g hierarchyes, give the bottom corners to the left/right
     * docks */
    setCorner(Qt::BottomLeftCorner, Qt::LeftDockWidgetArea);
    setCorner(Qt::BottomRightCorner, Qt::RightDockWidgetArea);

    /* Console */
    console_dock = new QBDockWidget("Console", this);
    addDockWidget(Qt::BottomDockWidgetArea, console_dock);
    console_dock->setAllowedAreas(Qt::BottomDockWidgetArea);
    view_menu->addAction(console_dock->toggleViewAction());
    connect(console_dock, &QBDockWidget::topLevelChanged, console_dock, &QBDockWidget::toWindow);
    console = new QtConsole(console_dock);
    console->prompt("$ ");
    console_dock->setWidget(console);
    // The console's run of a command has implications for the entire
    // application, so rather than embedding the command execution logic in the
    // widget we use a signal/slot connection to have the main window's slot
    // execute the command.
    // TODO - run_cmd probably belongs with CADApp, really...
    QObject::connect(this->console, &QtConsole::executeCommand, this, &BRLCAD_MainWindow::run_cmd);

    /* Geometry Tree */
    tree_dock = new QBDockWidget("Hierarchy", this);
    addDockWidget(Qt::LeftDockWidgetArea, tree_dock);
    tree_dock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    view_menu->addAction(tree_dock->toggleViewAction());
    connect(tree_dock, &QBDockWidget::topLevelChanged, tree_dock, &QBDockWidget::toWindow);

    treemodel = new CADTreeModel();
    treeview = new CADTreeView(tree_dock);
    treemodel->cadtreeview = (CADTreeView *)treeview;
    ((CADTreeView *)treeview)->m = treemodel;

    tree_dock->setWidget(treeview);
    treeview->setModel(treemodel);
    treeview->setItemDelegate(new GObjectDelegate((CADTreeView *)treeview));

    treeview->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    treeview->header()->setStretchLastSection(true);
    QObject::connect((CADApp *)qApp, &CADApp::db_change, treemodel, &CADTreeModel::refresh);
    if (canvas) {
	QObject::connect((CADApp *)qApp, &CADApp::db_change, canvas, &QtCADView::need_update);
    } else if (c4) {
	QObject::connect((CADApp *)qApp, &CADApp::db_change, c4, &QtCADQuad::need_update);
    }
    QObject::connect(treeview, &CADTreeView::expanded, (CADTreeView *)treeview, &CADTreeView::tree_column_size);
    QObject::connect(treeview, &CADTreeView::collapsed, (CADTreeView *)treeview, &CADTreeView::tree_column_size);
    QObject::connect(treeview, &CADTreeView::clicked, treemodel, &CADTreeModel::update_selected_node_relationships);
    QObject::connect(treeview, &CADTreeView::expanded, treemodel, &CADTreeModel::expand_tree_node_relationships);
    QObject::connect(treeview, &CADTreeView::collapsed, treemodel, &CADTreeModel::close_tree_node_relationships);
    QObject::connect(treeview, &CADTreeView::customContextMenuRequested, (CADTreeView *)treeview, &CADTreeView::context_menu);
    treemodel->populate(DBI_NULL);
    treemodel->interaction_mode = 0;
    ((CADApp *)qApp)->cadtreeview = (CADTreeView *)treeview;

    QObject::connect(treeview, &CADTreeView::clicked, stdpropmodel, &CADAttributesModel::refresh);
    QObject::connect(treeview, &CADTreeView::clicked, userpropmodel, &CADAttributesModel::refresh);

}

bool
BRLCAD_MainWindow::isValid3D()
{
    if (canvas)
	return canvas->isValid();
    if (c4)
	return c4->isValid();
    return false;
}

void
BRLCAD_MainWindow::fallback3D()
{
    if (canvas) {
	canvas->fallback();
	return;
    }
    if (c4) {
	c4->fallback();
	return;
    }
}

void
BRLCAD_MainWindow::run_cmd(const QString &command)
{
    if (BU_STR_EQUAL(command.toStdString().c_str(), "q"))
	bu_exit(0, "exit");

    if (BU_STR_EQUAL(command.toStdString().c_str(), "clear")) {
	console->clear();
	console->prompt("$ ");
	return;
    }

    // make an argv array
    struct bu_vls ged_prefixed = BU_VLS_INIT_ZERO;
    bu_vls_sprintf(&ged_prefixed, "%s", command.toStdString().c_str());
    char *input = bu_strdup(bu_vls_addr(&ged_prefixed));
    bu_vls_free(&ged_prefixed);
    char **av = (char **)bu_calloc(strlen(input) + 1, sizeof(char *), "argv array");
    int ac = bu_argv_from_string(av, strlen(input), input);
    struct bu_vls msg = BU_VLS_INIT_ZERO;

    CADApp *bApp = qobject_cast<CADApp *>(qApp);
    struct ged **gedpp = &bApp->gedp;

    /* The "open" and close commands require a bit of
     * awareness at this level, since the gedp pointer
     * must respond to them and the canvas widget needs
     * some info from the current gedp. */
    int cmd_run = 0;
    if (BU_STR_EQUAL(av[0], "open")) {
	if (ac > 1) {
	    if (*gedpp) {
		ged_close(*gedpp);
	    }
	    int ret = ((CADApp *)qApp)->opendb(av[1]);
	    if (ret) {
		bu_vls_sprintf(&msg, "Could not open %s as a .g file\n", av[1]) ;
		console->printString(bu_vls_cstr(&msg));
	    }
	} else {
	    console->printString("Error: invalid ged_open call\n");
	}
	cmd_run = 1;
    }
    if (BU_STR_EQUAL(av[0], "close")) {
	ged_close(*gedpp);
	(*gedpp) = NULL;
	if (canvas) {
	    canvas->set_view(NULL);
	    //canvas->dm_set = NULL;
	    canvas->set_dm_current(NULL);
	    canvas->set_base2local(NULL);
	    canvas->set_local2base(NULL);
	}
	if (c4) {
	    for (int i = 1; i < 5; i++) {
		QtCADView *c = c4->get(i);
		//c->dm_set = NULL;
		c->set_view(NULL);
		c->set_dm_current(NULL);
		c->set_base2local(NULL);
		c->set_local2base(NULL);
	    }
	}
	console->printString("closed database\n");
	cmd_run = 1;
    }

    if (!cmd_run) {
	bApp->ged_run_cmd(&msg, ac, (const char **)av);
	if (bu_vls_strlen(&msg) > 0) {
	    console->printString(bu_vls_cstr(&msg));
	}
    }
    if (*gedpp) {
	bu_vls_trunc(bApp->gedp->ged_result_str, 0);
    }

    bu_vls_free(&msg);
    bu_free(input, "input copy");
    bu_free(av, "input argv");

    console->prompt("$ ");
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
	((CADApp *)qApp)->cadtreeview->m->dbip = ((CADApp *)qApp)->dbip();
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

