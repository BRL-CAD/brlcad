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
	show();
    }
}

BRLCAD_MainWindow::BRLCAD_MainWindow(int canvas_type, int quad_view)
{
    CADApp *ap = (CADApp *)qApp;
    ap->w = this;

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
	setCentralWidget(canvas);
    } else {
	c4 = new QtCADQuad(this, canvas_type);
	c4->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
	setCentralWidget(c4);
    }

    // Define dock widgets - these are the console, controls, etc. that can be attached
    // and detached from the main window.  Eventually this should be a dynamic set
    // of widgets rather than hardcoded statics, so plugins can define their own
    // graphical elements...

    vcd = new QDockWidget("View Controls", this);
    addDockWidget(Qt::RightDockWidgetArea, vcd);
    vcd->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    view_menu->addAction(vcd->toggleViewAction());
    vc = new CADPalette(0, this);
    vcd->setWidget(vc);

    icd = new QDockWidget("Instance Editing", this);
    addDockWidget(Qt::RightDockWidgetArea, icd);
    icd->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    view_menu->addAction(icd->toggleViewAction());
    ic = new CADPalette(1, this);
    icd->setWidget(ic);

    ocd = new QDockWidget("Object Editing", this);
    addDockWidget(Qt::RightDockWidgetArea, ocd);
    ocd->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    view_menu->addAction(ocd->toggleViewAction());
    oc = new CADPalette(2, this);
    ocd->setWidget(oc);

    // The view and edit panels have consequences for the tree widget, so each
    // needs to know which one is current
    connect(vc, &CADPalette::current, vc, &CADPalette::makeCurrent);
    connect(vc, &CADPalette::current, ic, &CADPalette::makeCurrent);
    connect(vc, &CADPalette::current, oc, &CADPalette::makeCurrent);
    connect(ic, &CADPalette::current, vc, &CADPalette::makeCurrent);
    connect(ic, &CADPalette::current, ic, &CADPalette::makeCurrent);
    connect(ic, &CADPalette::current, oc, &CADPalette::makeCurrent);
    connect(oc, &CADPalette::current, vc, &CADPalette::makeCurrent);
    connect(oc, &CADPalette::current, ic, &CADPalette::makeCurrent);
    connect(oc, &CADPalette::current, oc, &CADPalette::makeCurrent);


    /****************************************************************************
     * The primary view and palette widgets are now in place.  We are ready to
     * start loading tools defined as plugins.
     ****************************************************************************/

    {
	const char *ppath = bu_dir(NULL, 0, BU_DIR_LIBEXEC, "qged", NULL);
	char **filenames;
	struct bu_vls plugin_pattern = BU_VLS_INIT_ZERO;
	bu_vls_sprintf(&plugin_pattern, "*%s", QGED_PLUGIN_SUFFIX);
	size_t nfiles = bu_file_list(ppath, bu_vls_cstr(&plugin_pattern), &filenames);
	for (size_t i = 0; i < nfiles; i++) {
	    char pfile[MAXPATHLEN] = {0};
	    bu_dir(pfile, MAXPATHLEN, BU_DIR_LIBEXEC, "qged", filenames[i], NULL);
	    void *dl_handle;
	    dl_handle = bu_dlopen(pfile, BU_RTLD_NOW);
	    if (!dl_handle) {
		const char * const error_msg = bu_dlerror();
		if (error_msg)
		    bu_vls_printf(&ap->init_msgs, "%s\n", error_msg);

		bu_vls_printf(&ap->init_msgs, "Unable to dynamically load '%s' (skipping)\n", pfile);
		continue;
	    }
	    {
		const char *psymbol = "qged_plugin_info";
		void *info_val = bu_dlsym(dl_handle, psymbol);
		const struct qged_plugin *(*plugin_info)() = (const struct qged_plugin *(*)())(intptr_t)info_val;
		if (!plugin_info) {
		    const char * const error_msg = bu_dlerror();

		    if (error_msg)
			bu_vls_printf(&ap->init_msgs, "%s\n", error_msg);

		    bu_vls_printf(&ap->init_msgs, "Unable to load symbols from '%s' (skipping)\n", pfile);
		    bu_vls_printf(&ap->init_msgs, "Could not find '%s' symbol in plugin\n", psymbol);
		    bu_dlclose(dl_handle);
		    continue;
		}

		const struct qged_plugin *plugin = plugin_info();

		if (!plugin) {
		    bu_vls_printf(&ap->init_msgs, "Invalid plugin file '%s' encountered (skipping)\n", pfile);
		    bu_dlclose(dl_handle);
		    continue;
		}

		if (*((const uint32_t *)(plugin)) != (uint32_t)  (QGED_TOOL_PLUGIN) && *((const uint32_t *)(plugin)) != (uint32_t)  (QGED_CMD_PLUGIN)) {
		    bu_vls_printf(&ap->init_msgs, "Plugin version %d of '%s' does not match any valid candidates (skipping)\n", *((const uint32_t   *)(plugin)), pfile);
		    bu_dlclose(dl_handle);
		    continue;
		}

		if (!plugin->cmds) {
		    bu_vls_printf(&ap->init_msgs, "Invalid plugin file '%s' encountered (skipping)\n", pfile);
		    bu_dlclose(dl_handle);
		    continue;
		}

		if (!plugin->cmd_cnt) {
		    bu_vls_printf(&ap->init_msgs, "Plugin '%s' contains no commands, (skipping)\n", pfile);
		    bu_dlclose(dl_handle);
		    continue;
		}

		const struct qged_tool **cmds = plugin->cmds;
		for (int c = 0; c < plugin->cmd_cnt; c++) {
		    const struct qged_tool *cmd = cmds[c];
		    (*cmd->i->tool_create)((CADApp *)qApp);
		}
	    }
	}
    }

    // Add some placeholder tools until we start to implement the real ones
#if 0
    CADViewModel *vmodel = new CADViewModel();
    {

	QIcon *obj_icon = new QIcon(QPixmap(":info.svg"));
	QKeyValView *vview = new QKeyValView(this, 0);
	vview->setModel(vmodel);
	vview->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
	QObject::connect(((CADApp *)qApp), &CADApp::view_change, vmodel, &CADViewModel::refresh);
	QObject::connect(this, &BRLCAD_MainWindow::view_change, vmodel, &CADViewModel::refresh);
	QToolPaletteElement *el = new QToolPaletteElement(obj_icon, vview);
	vc->addTool(el);
    }
#endif
    {
	QIcon *obj_icon = new QIcon();
	QString obj_label("instance controls ");
	QPushButton *obj_control = new QPushButton(obj_label);
	QToolPaletteElement *el = new QToolPaletteElement(obj_icon, obj_control);
	ic->addTool(el);
    }
    {
	QIcon *obj_icon = new QIcon();
	QString obj_label("primitive controls ");
	QPushButton *obj_control = new QPushButton(obj_label);
	QToolPaletteElement *el = new QToolPaletteElement(obj_icon, obj_control);
	oc->addTool(el);
    }

#if 0
    // TODO - check if we still need this after we get proper post-construction
    // plugin population working for palettes
    //
    // Make sure the palette buttons are all visible - trick from
    // https://stackoverflow.com/a/56852841/2037687
    QTimer::singleShot(0, vc, &CADPalette::reflow);
    QTimer::singleShot(0, ic, &CADPalette::reflow);
    QTimer::singleShot(0, oc, &CADPalette::reflow);
#endif


    QDockWidget *sattrd = new QDockWidget("Standard Attributes", this);
    addDockWidget(Qt::RightDockWidgetArea, sattrd);
    sattrd->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    view_menu->addAction(sattrd->toggleViewAction());
    CADAttributesModel *stdpropmodel = new CADAttributesModel(0, DBI_NULL, RT_DIR_NULL, 1, 0);
    QKeyValView *stdpropview = new QKeyValView(this, 1);
    stdpropview->setModel(stdpropmodel);
    sattrd->setWidget(stdpropview);

    QDockWidget *uattrd = new QDockWidget("User Attributes", this);
    addDockWidget(Qt::RightDockWidgetArea, uattrd);
    uattrd->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    view_menu->addAction(uattrd->toggleViewAction());
    CADAttributesModel *userpropmodel = new CADAttributesModel(0, DBI_NULL, RT_DIR_NULL, 0, 1);
    QKeyValView *userpropview = new QKeyValView(this, 0);
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
    // widget we use a signal/slot connection to have the application's slot
    // execute the command.
    QObject::connect(this->console, &QtConsole::executeCommand, ((CADApp *)qApp), &CADApp::run_cmd);

    /* Geometry Tree */
    tree_dock = new QBDockWidget("Hierarchy", this);
    addDockWidget(Qt::LeftDockWidgetArea, tree_dock);
    tree_dock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    view_menu->addAction(tree_dock->toggleViewAction());
    connect(tree_dock, &QBDockWidget::topLevelChanged, tree_dock, &QBDockWidget::toWindow);
    treemodel = new CADTreeModel();
    treeview = new CADTreeView(tree_dock, treemodel);
    tree_dock->setWidget(treeview);

    // The tree's highlighting changes based on which set of tools we're using - instance editing
    // and primitive editing have different non-local implications in the hierarchy.
    connect(vc, &CADPalette::interaction_mode, treemodel, &CADTreeModel::mode_change);
    connect(ic, &CADPalette::interaction_mode, treemodel, &CADTreeModel::mode_change);
    connect(oc, &CADPalette::interaction_mode, treemodel, &CADTreeModel::mode_change);

    // Update props if we select a new item in the tree.  TODO - these need to be updated when
    // we have a db_change as well, since the change may have been to edit attributes...
    QObject::connect(treeview, &CADTreeView::clicked, stdpropmodel, &CADAttributesModel::refresh);
    QObject::connect(treeview, &CADTreeView::clicked, userpropmodel, &CADAttributesModel::refresh);

    // If the database changes, we need to refresh the tree.  (Right now this is only triggered
    // if we open a new .g file, IIRC, but it needs to happen when we've editing combs or added/
    // removed solids too...)
    QObject::connect((CADApp *)qApp, &CADApp::db_change, treemodel, &CADTreeModel::refresh);

    // If the database changes, we need to update our views
    if (canvas) {
	QObject::connect((CADApp *)qApp, &CADApp::db_change, canvas, &QtCADView::need_update);
    } else if (c4) {
	QObject::connect((CADApp *)qApp, &CADApp::db_change, c4, &QtCADQuad::need_update);
	// The Quad View has an additional condition in the sense that the current view may
	// change.  Probably we won't try to track this for floating dms attached to qged,
	// but the quad view is a central view widget so we need to support it.
	QObject::connect(c4, &QtCADQuad::selected, (CADApp *)qApp, &CADApp::do_view_change);
    }

#if 0
    // If the view changes according to either the view or the app, we need to
    // update our view info in the tool.  TODO - this needs to be packaged a
    // bit somehow - other tools may need to make some sort of connections to
    // the main app and/or views... (for example, an editing tool needs to know if
    // a kill command yanks the selected object away from it...)
    QObject::connect((CADApp *)qApp, &CADApp::view_change, vmodel, &CADViewModel::update);
    if (canvas) {
	QObject::connect(canvas, &QtCADView::changed, vmodel, &CADViewModel::update);
    } else if (c4) {
	QObject::connect(c4, &QtCADQuad::changed, vmodel, &CADViewModel::update);
    }
#endif


    // We start out with the View Control panel as the current panel - by
    // default we are viewing, not editing
    vc->makeCurrent(vc);
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

