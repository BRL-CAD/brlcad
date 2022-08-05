/*                 M A I N _ W I N D O W . C X X
 * BRL-CAD
 *
 * Copyright (c) 2014-2022 United States Government as represented by
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

#include <map>
#include <set>
#include <QTimer>
#include <QMessageBox>
#include "qtcad/QViewCtrl.h"
#include "qtcad/QgTreeSelectionModel.h"
#include "bu/str.h"
#include "main_window.h"
#include "app.h"
#include "palettes.h"
#include "attributes.h"
#include "fbserv.h"

// The palette tools are loaded as dynamic plugins.  Since the logic for doing
// so is rather verbose and not dependent on the Qt main_window information, we
// break it out into a separate function to keep the main_window initialization
// more readable.
static void
_load_palette_tools(
	struct bu_vls *msgs,
	CADPalette *vc,
	CADPalette *oc,
	std::map<int, std::set<QToolPaletteElement *>> &vc_map,
	std::map<int, std::set<QToolPaletteElement *>> &oc_map
	)
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
		bu_vls_printf(msgs, "%s\n", error_msg);

	    bu_vls_printf(msgs, "Unable to dynamically load '%s' (skipping)\n", pfile);
	    continue;
	}
	{
	    const char *psymbol = "qged_plugin_info";
	    void *info_val = bu_dlsym(dl_handle, psymbol);
	    const struct qged_plugin *(*plugin_info)() = (const struct qged_plugin *(*)())(intptr_t)info_val;
	    if (!plugin_info) {
		const char * const error_msg = bu_dlerror();

		if (error_msg)
		    bu_vls_printf(msgs, "%s\n", error_msg);

		bu_vls_printf(msgs, "Unable to load symbols from '%s' (skipping)\n", pfile);
		bu_vls_printf(msgs, "Could not find '%s' symbol in plugin\n", psymbol);
		bu_dlclose(dl_handle);
		continue;
	    }

	    const struct qged_plugin *plugin = plugin_info();

	    if (!plugin) {
		bu_vls_printf(msgs, "Invalid plugin file '%s' encountered (skipping)\n", pfile);
		bu_dlclose(dl_handle);
		continue;
	    }

	    if (!plugin->cmds) {
		bu_vls_printf(msgs, "Invalid plugin file '%s' encountered (skipping)\n", pfile);
		bu_dlclose(dl_handle);
		continue;
	    }

	    if (!plugin->cmd_cnt) {
		bu_vls_printf(msgs, "Plugin '%s' contains no commands, (skipping)\n", pfile);
		bu_dlclose(dl_handle);
		continue;
	    }

	    const struct qged_tool **cmds = plugin->cmds;
	    uint32_t ptype = *((const uint32_t *)(plugin));

	    switch (ptype) {
		case QGED_VC_TOOL_PLUGIN:
		    for (int c = 0; c < plugin->cmd_cnt; c++) {
			const struct qged_tool *cmd = cmds[c];
			QToolPaletteElement *el = (QToolPaletteElement *)(*cmd->i->tool_create)();
			vc_map[cmd->palette_priority].insert(el);
		    }
		    break;
		case QGED_OC_TOOL_PLUGIN:
		    for (int c = 0; c < plugin->cmd_cnt; c++) {
			const struct qged_tool *cmd = cmds[c];
			QToolPaletteElement *el = (QToolPaletteElement *)(*cmd->i->tool_create)();
			oc_map[cmd->palette_priority].insert(el);
		    }
		    break;
		case QGED_CMD_PLUGIN:
		    bu_vls_printf(msgs, "TODO - implement cmd plugins\n");
		    bu_dlclose(dl_handle);
		    break;
		default:
		    bu_vls_printf(msgs, "Plugin type %d of '%s' does not match any valid candidates (skipping)\n", ptype, pfile);
		    bu_dlclose(dl_handle);
		    continue;
		    break;
	    }
	}
    }
    bu_argv_free(nfiles, filenames);
    bu_vls_free(&plugin_pattern);

    std::map<int, std::set<QToolPaletteElement *>>::iterator e_it;
    for (e_it = vc_map.begin(); e_it != vc_map.end(); e_it++) {
	std::set<QToolPaletteElement *>::iterator el_it;
	for (el_it = e_it->second.begin(); el_it != e_it->second.end(); el_it++) {
	    QToolPaletteElement *el = *el_it;
	    vc->addTool(el);
	}
    }

    for (e_it = oc_map.begin(); e_it != oc_map.end(); e_it++) {
	std::set<QToolPaletteElement *>::iterator el_it;
	for (el_it = e_it->second.begin(); el_it != e_it->second.end(); el_it++) {
	    QToolPaletteElement *el = *el_it;
	    oc->addTool(el);
	}
    }


    // Add placeholder oc tool until we implement more real tools
    {
	QIcon *obj_icon = new QIcon();
	QString obj_label("primitive controls ");
	QPushButton *obj_control = new QPushButton(obj_label);
	QToolPaletteElement *el = new QToolPaletteElement(obj_icon, obj_control);
	oc->addTool(el);
    }

}


BRLCAD_MainWindow::BRLCAD_MainWindow(int canvas_type, int quad_view)
{
    CADApp *ap = (CADApp *)qApp;
    QgModel *m = ap->mdl;
    struct ged *gedp = m->gedp;
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
    addDockWidget(Qt::RightDockWidgetArea, vcd);
    vcd->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    vm_panels->addAction(vcd->toggleViewAction());
    vc = new CADPalette(0, this);
    vcd->setWidget(vc);

    ocd = new QDockWidget("Object Editing", this);
    addDockWidget(Qt::RightDockWidgetArea, ocd);
    ocd->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    vm_panels->addAction(ocd->toggleViewAction());
    oc = new CADPalette(2, this);
    ocd->setWidget(oc);

    // The makeCurrent connections enforce an either/or paradigm
    // for the view and object editing panels.
    connect(vc, &CADPalette::current, oc, &CADPalette::makeCurrent);
    connect(vc, &CADPalette::current, vc, &CADPalette::makeCurrent);
    connect(oc, &CADPalette::current, oc, &CADPalette::makeCurrent);
    connect(oc, &CADPalette::current, vc, &CADPalette::makeCurrent);

    /****************************************************************************
     * The primary view and palette widgets are now in place.  We are ready to
     * start loading tools defined as plugins.
     ****************************************************************************/
    std::map<int, std::set<QToolPaletteElement *>> vc_map;
    std::map<int, std::set<QToolPaletteElement *>> oc_map;
    _load_palette_tools(&ap->init_msgs, vc, oc, vc_map, oc_map);

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

    cshellcomp = new GEDShellCompleter(console, gedp);
    console->setCompleter(cshellcomp);

    /* Geometry Tree */
    tree_dock = new QgDockWidget("Hierarchy", this);
    addDockWidget(Qt::LeftDockWidgetArea, tree_dock);
    tree_dock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    vm_panels->addAction(tree_dock->toggleViewAction());
    connect(tree_dock, &QgDockWidget::topLevelChanged, tree_dock, &QgDockWidget::toWindow);
    CADApp *ca = (CADApp *)qApp;
    treeview = new QgTreeView(tree_dock, ca->mdl);
    tree_dock->setWidget(treeview);
    tree_dock->m = m;
    ap->treeview = treeview;
    connect(tree_dock, &QgDockWidget::banner_click, m, &QgModel::toggle_hierarchy);
    connect(vm_topview, &QAction::triggered, m, &QgModel::toggle_hierarchy);
    connect(m, &QgModel::opened_item, treeview, &QgTreeView::qgitem_select_sync);
    connect(m, &QgModel::view_change, ca, &CADApp::do_view_changed);
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
    QgTreeSelectionModel *selm = (QgTreeSelectionModel *)treeview->selectionModel();
    connect(vc, &CADPalette::interaction_mode, selm, &QgTreeSelectionModel::mode_change);
    connect(oc, &CADPalette::interaction_mode, selm, &QgTreeSelectionModel::mode_change);


    // Dialogues for attribute viewing (and eventually manipulation)
    QDockWidget *sattrd = new QDockWidget("Standard Attributes", this);
    addDockWidget(Qt::LeftDockWidgetArea, sattrd);
    sattrd->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    vm_panels->addAction(sattrd->toggleViewAction());
    CADAttributesModel *stdpropmodel = new CADAttributesModel(0, DBI_NULL, RT_DIR_NULL, 1, 0);
    QKeyValView *stdpropview = new QKeyValView(this, 1);
    stdpropview->setModel(stdpropmodel);
    sattrd->setWidget(stdpropview);

    QDockWidget *uattrd = new QDockWidget("User Attributes", this);
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
    ap->curr_view = c4->get(0);

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
BRLCAD_MainWindow::do_dm_init()
{
    CADApp *ap = (CADApp *)qApp;
    QgModel *m = ap->mdl;
    struct ged *gedp = m->gedp;

    bu_setenv("GED_TEST_NEW_CMD_FORMS", "1", 1);

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
