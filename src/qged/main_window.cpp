/*                 M A I N _ W I N D O W . C X X
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
/** @file main_window.cxx
 *
 * Implementation code for toplevel window for BRL-CAD GUI.
 *
 */

#include <QTimer>
#include <QMessageBox>
#include "qtcad/QViewCtrl.h"
#include "qtcad/QgTreeSelectionModel.h"
#include "main_window.h"
#include "app.h"
#include "palettes.h"
#include "attributes.h"
#include "fbserv.h"

BRLCAD_MainWindow::BRLCAD_MainWindow(int canvas_type, int quad_view)
{
    CADApp *ap = (CADApp *)qApp;
    QgModel *m = ap->mdl;
    struct ged *gedp = m->gedp;
    ap->w = this;

#ifdef BRLCAD_OPENGL
    // Define the surface format for OpenGL drawing
    QSurfaceFormat format;
    format.setDepthBufferSize(16);
    QSurfaceFormat::setDefaultFormat(format);
#endif

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
    QObject::connect(cad_open, &QAction::triggered, ((CADApp *)qApp), &CADApp::open_file);
    file_menu->addAction(cad_open);

    cad_save_settings = new QAction("Save Settings", this);
    connect(cad_save_settings, &QAction::triggered, ((CADApp *)qApp), &CADApp::write_settings);
    file_menu->addAction(cad_save_settings);

#if 0
    cad_save_image = new QAction("Save Image", this);
    connect(cad_save_image, &QAction::triggered, this, &BRLCAD_MainWindow::save_image);
    file_menu->addAction(cad_save_image);
#endif

    cad_single_view = new QAction("Single View", this);
    connect(cad_single_view, &QAction::triggered, ((CADApp *)qApp), &CADApp::switch_to_single_view);
    file_menu->addAction(cad_single_view);

    cad_quad_view = new QAction("Quad View", this);
    connect(cad_quad_view, &QAction::triggered, ((CADApp *)qApp), &CADApp::switch_to_quad_view);
    file_menu->addAction(cad_quad_view);

    cad_exit = new QAction("Exit", this);
    QObject::connect(cad_exit, &QAction::triggered, this, &BRLCAD_MainWindow::close);
    file_menu->addAction(cad_exit);

    view_menu = menuBar()->addMenu("View");
    vm_topview = new QAction("Toggle Hierarchy (ls/tops)", this);
    view_menu->addAction(vm_topview);
    vm_panels = view_menu->addMenu("Panels");

    menuBar()->addSeparator();

    help_menu = menuBar()->addMenu("Help");


    // Define a widget to hold the main view and its associated
    // view control toolbar
    QWidget *cw = new QWidget(this);

    // The core of the interface is the CAD view widget, which is capable
    // of either a single display or showing 4 views in a grid arrangement
    // (the "quad" view).  By default it displays a single view, unless
    // overridden by a user option.
    c4 = new QtCADQuad(cw, gedp, canvas_type);
    if (!c4) {
	QMessageBox *msgbox = new QMessageBox();
	msgbox->setText("Fatal error: unable to create QtCADQuad widget");
	msgbox->exec();
	bu_exit(EXIT_FAILURE, "Unable to create QtCADQuad widget\n");
    }
    c4->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);

    // Make the fundamental connection that allows the view to update in
    // response to commands or widgets taking actions that will impact the
    // scene.  Camera view changes, adding/removing objects or view elements
    // from the scene, and updates such as incremental display of raytracing
    // results in an embedded framebuffer all need to notify the QtCADQuad
    // it is time to update.
    QObject::connect(ap, &CADApp::view_update, c4, &QtCADQuad::do_view_update);

    // Define a graphical toolbar with control widgets
    vcw = new QViewCtrl(cw, gedp);
    vcw->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
    QObject::connect(vcw, &QViewCtrl::view_changed, ap, &CADApp::do_view_changed);
    QObject::connect(ap, &CADApp::view_update, vcw, &QViewCtrl::do_view_update);
    // Make the connection so the view control can change the mouse mode of the Quad View
    QObject::connect(vcw, &QViewCtrl::lmouse_mode, c4, &QtCADQuad::set_lmouse_move_default);

    // The toolbar is added to the layout first, so it is drawn above the Quad view
    QVBoxLayout *cwl = new QVBoxLayout;
    cwl->addWidget(vcw);
    cwl->addWidget(c4);

    // Having defined the layout, we set cw to use it and let the main window
    // know cw is the central widget.
    cw->setLayout(cwl);
    setCentralWidget(cw);

    // Let GED know to use the QtCADQuad view as its current view
    gedp->ged_gvp = c4->view();

    // See if the user has requested a particular mode
    if (quad_view) {
	c4->changeToQuadFrame();
    } else {
	c4->changeToSingleFrame();
    }

    // Set up the connections needed for embedded raytracing
    gedp->fbs_is_listening = &qdm_is_listening;
    gedp->fbs_listen_on_port = &qdm_listen_on_port;
    gedp->fbs_open_server_handler = &qdm_open_server_handler;
    gedp->fbs_close_server_handler = &qdm_close_server_handler;

    // Unfortunately, there are technical differences involved with
    // the embedded fb mechanisms depending on whether we are using
    // the system native OpenGL or our fallback software rasterizer
    int type = c4->get(0)->view_type();
#ifdef BRLCAD_OPENGL
    if (type == QtCADView_GL) {
	gedp->fbs_open_client_handler = &qdm_open_client_handler;
    }
#endif
    if (type == QtCADView_SW) {
	gedp->fbs_open_client_handler = &qdm_open_sw_client_handler;
    }
    gedp->fbs_close_client_handler = &qdm_close_client_handler;


    // Having set up the central widget and fb connections, we now define
    // dockable control widgets.  These are the console, controls, etc. that
    // can be attached and detached from the main window.  Generally speaking
    // we will also define menu actions to enable and disable these windows,
    // to all them to be recovered even if the user closes them completely.
    //
    // TODO -  Eventually this should be a dynamic set of widgets rather than
    // hardcoded statics, so plugins can define their own graphical elements...

    vcd = new QDockWidget("View Controls", this);
    vcd->setObjectName("View_Controls");
    addDockWidget(Qt::RightDockWidgetArea, vcd);
    vcd->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    vm_panels->addAction(vcd->toggleViewAction());
    vc = new CADPalette(QGED_VC_TOOL_PLUGIN, this);
    vcd->setWidget(vc);

    ocd = new QDockWidget("Object Editing", this);
    ocd->setObjectName("Object_Editing");
    addDockWidget(Qt::RightDockWidgetArea, ocd);
    ocd->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    vm_panels->addAction(ocd->toggleViewAction());
    oc = new CADPalette(QGED_OC_TOOL_PLUGIN, this);
    ocd->setWidget(oc);

    // The makeCurrent connections enforce an either/or paradigm
    // for the view and object editing panels.
    connect(vc, &CADPalette::current, oc, &CADPalette::makeCurrent);
    connect(vc, &CADPalette::current, vc, &CADPalette::makeCurrent);
    connect(oc, &CADPalette::current, oc, &CADPalette::makeCurrent);
    connect(oc, &CADPalette::current, vc, &CADPalette::makeCurrent);

    // Now that we've got everything set up, connect the palette selection
    // signals so they can update the view event filter as needed.  We don't do
    // this before populating the palettes with tools since the initial
    // addition would trigger a selection which we're not going to use.  (We
    // default to selecting the default view tool at the end of this
    // procedure by making vc the current palette.)
    // TODO - need to figure out how this should (or shouldn't) be rolled into
    // do_view_changed
    QObject::connect(vc->tpalette, &QToolPalette::palette_element_selected, ap, &CADApp::element_selected);
    QObject::connect(oc->tpalette, &QToolPalette::palette_element_selected, ap, &CADApp::element_selected);

    // The tools in the view and edit panels may have consequences for the view.
    // Connect to the palette signals and slots (the individual tool connections
    // are handled by the palette container.)
    QObject::connect(ap, &CADApp::view_update, vc->tpalette, &QToolPalette::do_view_update);
    QObject::connect(vc->tpalette, &QToolPalette::view_changed, ap, &CADApp::do_view_changed);
    QObject::connect(ap, &CADApp::view_update, oc->tpalette, &QToolPalette::do_view_update);
    QObject::connect(oc->tpalette, &QToolPalette::view_changed, ap, &CADApp::do_view_changed);

    /* Console */
    console_dock = new QgDockWidget("Console", this);
    console_dock->setObjectName("Console");
    addDockWidget(Qt::BottomDockWidgetArea, console_dock);
    console_dock->setAllowedAreas(Qt::BottomDockWidgetArea);
    vm_panels->addAction(console_dock->toggleViewAction());
    connect(console_dock, &QgDockWidget::topLevelChanged, console_dock, &QgDockWidget::toWindow);
    console = new QtConsole(console_dock);
    console->prompt("$ ");
    console_dock->setWidget(console);
    // The console's run of a command has implications for the entire
    // application, so rather than embedding the command execution logic in the
    // widget we use a signal/slot connection to have the application's slot
    // execute the command.
    QObject::connect(this->console, &QtConsole::executeCommand, ((CADApp *)qApp), &CADApp::run_qcmd);

    // The Qt console itself has no direct knowledge of GED commands.  For contextually
    // aware command prompts, we need a customized completer with knowledge of the database
    // contents and GED commands.
    GEDShellCompleter *cshellcomp = new GEDShellCompleter(console, gedp);
    console->setCompleter(cshellcomp);

    /* Geometry Tree */
    tree_dock = new QgDockWidget("Hierarchy", this);
    tree_dock->setObjectName("Hierarchy");
    addDockWidget(Qt::LeftDockWidgetArea, tree_dock);
    tree_dock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    vm_panels->addAction(tree_dock->toggleViewAction());
    connect(tree_dock, &QgDockWidget::topLevelChanged, tree_dock, &QgDockWidget::toWindow);
    CADApp *ca = (CADApp *)qApp;
    treeview = new QgTreeView(tree_dock, ca->mdl);
    tree_dock->setWidget(treeview);
    tree_dock->m = m;
    connect(tree_dock, &QgDockWidget::banner_click, m, &QgModel::toggle_hierarchy);
    connect(vm_topview, &QAction::triggered, m, &QgModel::toggle_hierarchy);
    connect(m, &QgModel::opened_item, treeview, &QgTreeView::qgitem_select_sync);
    connect(m, &QgModel::view_change, ca, &CADApp::do_view_changed);
    QObject::connect(treeview, &QgTreeView::view_changed, ap, &CADApp::do_view_changed);
    QObject::connect(ap, &CADApp::view_update, treeview, &QgTreeView::do_view_update);

    // We need to record the expanded/contracted state of the tree items,
    // and restore them after a model reset
    connect(treeview, &QgTreeView::expanded, m, &QgModel::item_expanded);
    connect(treeview, &QgTreeView::collapsed, m, &QgModel::item_collapsed);
    connect(m, &QgModel::mdl_changed_db, treeview, &QgTreeView::redo_expansions);
    connect(m, &QgModel::check_highlights, treeview, &QgTreeView::redo_highlights);

    // The tree's highlighting changes based on which set of tools we're using.
    //
    // TODO - instance editing and primitive editing have different non-local
    // implications in the hierarchy.  Originally plan was to have separate
    // panels for the two modes.  That's looking like it may be overkill, so
    // the interaction mode of oc may need to change for the specific comb case
    // of editing an instance in the comb tree.
    //QgTreeSelectionModel *selm = (QgTreeSelectionModel *)treeview->selectionModel();
    //connect(vc, &CADPalette::interaction_mode, selm, &QgTreeSelectionModel::mode_change);
    //connect(oc, &CADPalette::interaction_mode, selm, &QgTreeSelectionModel::mode_change);


    // Dialogues for attribute viewing (and eventually manipulation)
    QDockWidget *sattrd = new QDockWidget("Standard Attributes", this);
    sattrd->setObjectName("Standard_Attributes");
    addDockWidget(Qt::LeftDockWidgetArea, sattrd);
    sattrd->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    vm_panels->addAction(sattrd->toggleViewAction());
    CADAttributesModel *stdpropmodel = new CADAttributesModel(0, DBI_NULL, RT_DIR_NULL, 1, 0);
    QKeyValView *stdpropview = new QKeyValView(this, 1);
    stdpropview->setModel(stdpropmodel);
    sattrd->setWidget(stdpropview);

    QDockWidget *uattrd = new QDockWidget("User Attributes", this);
    uattrd->setObjectName("User_Attributes");
    addDockWidget(Qt::LeftDockWidgetArea, uattrd);
    uattrd->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    vm_panels->addAction(uattrd->toggleViewAction());
    CADAttributesModel *userpropmodel = new CADAttributesModel(0, DBI_NULL, RT_DIR_NULL, 0, 1);
    QKeyValView *userpropview = new QKeyValView(this, 0);
    userpropview->setModel(userpropmodel);
    uattrd->setWidget(userpropview);

    // Update props if we select a new item in the tree.  TODO - these need to be updated when
    // we have a app_changed_db as well, since the change may have been to edit attributes...
    QObject::connect(treeview, &QgTreeView::clicked, stdpropmodel, &CADAttributesModel::refresh);
    QObject::connect(treeview, &QgTreeView::clicked, userpropmodel, &CADAttributesModel::refresh);

    // If the model does something that it things should trigger a view update, let the app know
    QObject::connect(m, &QgModel::view_change, ap, &CADApp::do_view_changed);


    // Connect the primary view widget to the app
    QObject::connect(c4, &QtCADQuad::selected, ap, &CADApp::do_quad_view_change);
    QObject::connect(c4, &QtCADQuad::changed, ap, &CADApp::do_quad_view_change);

    // Some of the dm initialization has to be delayed - make the connections so we can
    // do the work after widget initialization is complete.
    QObject::connect(c4, &QtCADQuad::init_done, this, &BRLCAD_MainWindow::do_dm_init);

    /* Because the console usually doesn't need a huge amount of horizontal
     * space and the tree can use all the vertical space it can get when
     * viewing large .g hierarchies, give the bottom corners to the left/right
     * docks */
    setCorner(Qt::BottomLeftCorner, Qt::LeftDockWidgetArea);
    setCorner(Qt::BottomRightCorner, Qt::RightDockWidgetArea);

    // We start out with the View Control panel as the current panel - by
    // default we are viewing, not editing
    vc->makeCurrent(vc);
}

// These commands can only be successfully executed after the QtCADQuad widget
// initialization is *fully* complete.  Consequently, these steps are separated
// out from the main constructor.
void
BRLCAD_MainWindow::do_dm_init()
{
    QTCAD_SLOT("BRLCAD_MainWindow::do_dm_init", 1);
    CADApp *ap = (CADApp *)qApp;
    QgModel *m = ap->mdl;
    struct ged *gedp = m->gedp;

    ///////////////////////////////////////////////////////////////////////////
    // DEBUG - turn on some of the bells and whistles by default, since they
    // won't normally be tested in other ways.  We need fully set up views and
    // dm contexts for these commands to work, and that setup (especially for
    // dm) can be delayed, so we put these invocations in a slot that is
    // specifically triggered once the underlying widgets have informed us that
    // they're ready to go.
    const char *av[5] = {NULL};
    av[0] = "dm";
    av[1] = "bg";
    av[2] = "110/110/110";
    av[3] = "0/0/50";
    ged_exec(gedp, 4, (const char **)av);

    av[0] = "view";
    av[1] = "lod";
    av[2] = "mesh";
    av[3] = "1";
    ged_exec(gedp, 4, (const char **)av);

    emit ap->view_update(QTCAD_VIEW_REFRESH);
    ///////////////////////////////////////////////////////////////////////////
}


bool
BRLCAD_MainWindow::isValid3D()
{
    return c4->isValid();
}

void
BRLCAD_MainWindow::fallback3D()
{
    c4->fallback();
}

void
BRLCAD_MainWindow::close()
{
    closeEvent(NULL);
    QMainWindow::close();
}

void
BRLCAD_MainWindow::closeEvent(QCloseEvent* e)
{
    QSettings settings("BRL-CAD", "QGED");
    // https://bugreports.qt.io/browse/QTBUG-16252?focusedCommentId=250562&page=com.atlassian.jira.plugin.system.issuetabpanels%3Acomment-tabpanel#comment-250562
    settings.setValue("geometry", QVariant(geometry()));
    settings.setValue("windowState", saveState());
    bu_log("mainwindow write settings\n");
    if (!e)
	return;
    QMainWindow::closeEvent(e);
}

void
BRLCAD_MainWindow::SetUnitConv(fastf_t base2local, fastf_t local2base)
{
    for (int i = 1; i < 5; i++) {
	QtCADView *c = c4->get(i);
	c->set_base2local(base2local);
	c->set_local2base(local2base);
    }
}

/* We base conditionals on whether the target widget w is active.  Usually the
 * actual focus widget is a child of the widget in question, so we walk up the
 * parents to see if the focusWidget is underneath the target widget. */
static bool
widget_active(QWidget *w)
{
    QWidget *fw = qApp->focusWidget();
    QWidget *cw = fw;
    while (cw) {
	if (cw == w) {
	    return true;
	}
	cw = (QWidget *)cw->parent();
    }
    return false;
}

bool
BRLCAD_MainWindow::isDisplayActive()
{
    return widget_active(c4);
}

QtCADView *
BRLCAD_MainWindow::CurrentDisplay()
{
    // TODO - should this always be 0?
    return c4->get(0);
}

void
BRLCAD_MainWindow::DisplayCheckpoint()
{
    c4->stash_hashes();
}

bool
BRLCAD_MainWindow::DisplayDiff()
{
    return c4->diff_hashes();
}

void
BRLCAD_MainWindow::QuadDisplay()
{
    c4->changeToQuadFrame();
}

void
BRLCAD_MainWindow::SingleDisplay()
{
    c4->changeToSingleFrame();
}

void
BRLCAD_MainWindow::IndicateRaytraceStart(int val)
{
    vcw->raytrace_start(val);
}

void
BRLCAD_MainWindow::IndicateRaytraceDone(int val)
{
    vcw->raytrace_done(val);
}

int
BRLCAD_MainWindow::InteractionMode(QPoint &gpos)
{
    if (vc->tpalette) {
	QWidget *vcp = vc->tpalette;
	QRect lrect = vcp->geometry();
	QPoint mpos = vcp->mapFromGlobal(gpos);
	if (lrect.contains(mpos)) {
	    return 0;
	}
    }

    if (oc->tpalette) {
	QWidget *ocp = oc->tpalette;
	QRect lrect = ocp->geometry();
	QPoint mpos = ocp->mapFromGlobal(gpos);
	if (lrect.contains(mpos)) {
	    return 2;
	}
    }

    return -1;
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
