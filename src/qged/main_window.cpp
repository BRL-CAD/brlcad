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
#include "main_window.h"
#include "app.h"
#include "palettes.h"
#include "attributes.h"
#include "fbserv.h"

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
    QgModel *m = (QgModel *)ap->mdl->sourceModel();
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

    menuBar()->addSeparator();

    help_menu = menuBar()->addMenu("Help");

    c4 = new QtCADQuad(this, gedp, canvas_type);
    c4->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
    setCentralWidget(c4);
    gedp->ged_gvp = c4->view();

    if (quad_view) {
	c4->changeToQuadFrame();
    }
    else {
	c4->changeToSingleFrame();
    }

    // Set up the connections needed for embedded raytracing
    gedp->fbs_is_listening = &qdm_is_listening;
    gedp->fbs_listen_on_port = &qdm_listen_on_port;
    gedp->fbs_open_server_handler = &qdm_open_server_handler;
    gedp->fbs_close_server_handler = &qdm_close_server_handler;

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
	std::map<int, std::set<QToolPaletteElement *>> vc_map;
	std::map<int, std::set<QToolPaletteElement *>> ic_map;
	std::map<int, std::set<QToolPaletteElement *>> oc_map;
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
		uint32_t ptype = *((const uint32_t *)(plugin));

		switch (ptype) {
		    case QGED_VC_TOOL_PLUGIN:
			for (int c = 0; c < plugin->cmd_cnt; c++) {
			    const struct qged_tool *cmd = cmds[c];
			    QToolPaletteElement *el = (QToolPaletteElement *)(*cmd->i->tool_create)();
			    vc_map[cmd->palette_priority].insert(el);
			}
			break;
		    case QGED_IC_TOOL_PLUGIN:
			for (int c = 0; c < plugin->cmd_cnt; c++) {
			    const struct qged_tool *cmd = cmds[c];
			    QToolPaletteElement *el = (QToolPaletteElement *)(*cmd->i->tool_create)();
			    ic_map[cmd->palette_priority].insert(el);
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
			bu_vls_printf(&ap->init_msgs, "TODO - implement cmd plugins\n");
			bu_dlclose(dl_handle);
			break;
		    default:
			bu_vls_printf(&ap->init_msgs, "Plugin type %d of '%s' does not match any valid candidates (skipping)\n", ptype, pfile);
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
		QObject::connect(ap, &CADApp::view_change, el, &QToolPaletteElement::do_app_changed_view);
		QObject::connect(m, &QgModel::mdl_changed_db, el, &QToolPaletteElement::do_app_changed_db);

		QObject::connect(el, &QToolPaletteElement::gui_changed_view, ap, &CADApp::do_view_update_from_gui_change);
		QObject::connect(el, &QToolPaletteElement::gui_changed_db, ap, &CADApp::do_db_update_from_gui_change);
	    }
	}


	for (e_it = ic_map.begin(); e_it != ic_map.end(); e_it++) {
	    std::set<QToolPaletteElement *>::iterator el_it;
	    for (el_it = e_it->second.begin(); el_it != e_it->second.end(); el_it++) {
		QToolPaletteElement *el = *el_it;
		ic->addTool(el);
		QObject::connect(ap, &CADApp::view_change, el, &QToolPaletteElement::do_app_changed_view);
		QObject::connect(m, &QgModel::mdl_changed_db, el, &QToolPaletteElement::do_app_changed_db);
		QObject::connect(el, &QToolPaletteElement::gui_changed_view, ap, &CADApp::do_view_update_from_gui_change);
	    }
	}

	for (e_it = oc_map.begin(); e_it != oc_map.end(); e_it++) {
	    std::set<QToolPaletteElement *>::iterator el_it;
	    for (el_it = e_it->second.begin(); el_it != e_it->second.end(); el_it++) {
		QToolPaletteElement *el = *el_it;
		oc->addTool(el);
		QObject::connect(ap, &CADApp::view_change, el, &QToolPaletteElement::do_app_changed_view);
		QObject::connect(m, &QgModel::mdl_changed_db, el, &QToolPaletteElement::do_app_changed_db);
		QObject::connect(el, &QToolPaletteElement::gui_changed_view, ap, &CADApp::do_view_update_from_gui_change);
	    }
	}

    }




    // Add some placeholder tools until we start to implement the real ones
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

    // Now that we've got everything set up, connect the palette selection
    // signals so they can update the view event filter as needed.  We don't do
    // this before populating the palettes with tools since the initial
    // addition would trigger a selection which we're not going to use.  (We
    // default to selecting the default view tool at the end of this
    // procedure by making vc the current palette.)
    QObject::connect(vc->tpalette, &QToolPalette::element_selected, ap, &CADApp::element_selected);
    QObject::connect(ic->tpalette, &QToolPalette::element_selected, ap, &CADApp::element_selected);
    QObject::connect(oc->tpalette, &QToolPalette::element_selected, ap, &CADApp::element_selected);


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
    QObject::connect(this->console, &QtConsole::executeCommand, ((CADApp *)qApp), &CADApp::run_qcmd);

    /* Geometry Tree */
    tree_dock = new QBDockWidget("Hierarchy", this);
    addDockWidget(Qt::LeftDockWidgetArea, tree_dock);
    tree_dock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    view_menu->addAction(tree_dock->toggleViewAction());
    connect(tree_dock, &QBDockWidget::topLevelChanged, tree_dock, &QBDockWidget::toWindow);
    CADApp *ca = (CADApp *)qApp;
    treeview = new QgTreeView(tree_dock, ca->mdl);
    tree_dock->setWidget(treeview);

    // Tell the selection model we have a tree view
    ca->mdl->treeview = treeview;

    // We need to record the expanded/contracted state of the tree items,
    // and restore them after a model reset
    connect(treeview, &QgTreeView::expanded, ca->mdl, &QgSelectionProxyModel::item_expanded);
    connect(treeview, &QgTreeView::collapsed, ca->mdl, &QgSelectionProxyModel::item_collapsed);
    connect(m, &QgModel::mdl_changed_db, treeview, &QgTreeView::redo_expansions);
    connect(m, &QgModel::check_highlights, treeview, &QgTreeView::redo_highlights);

    // The tree's highlighting changes based on which set of tools we're using - instance editing
    // and primitive editing have different non-local implications in the hierarchy.
    connect(vc, &CADPalette::interaction_mode, ca->mdl, &QgSelectionProxyModel::mode_change);
    connect(ic, &CADPalette::interaction_mode, ca->mdl, &QgSelectionProxyModel::mode_change);
    connect(oc, &CADPalette::interaction_mode, ca->mdl, &QgSelectionProxyModel::mode_change);

    // Update props if we select a new item in the tree.  TODO - these need to be updated when
    // we have a app_changed_db as well, since the change may have been to edit attributes...
    QObject::connect(treeview, &QgTreeView::clicked, stdpropmodel, &CADAttributesModel::refresh);
    QObject::connect(treeview, &QgTreeView::clicked, userpropmodel, &CADAttributesModel::refresh);

    // If the database changes, we need to refresh the tree.  (Right now this is only triggered
    // if we open a new .g file, IIRC, but it needs to happen when we've editing combs or added/
    // removed solids too...)  TODO - do we still need this?
    //QObject::connect(m, &QgModel::mdl_changed_db, ca->mdl, &QgSelectionProxyModel::refresh);

    // If the database changes, we need to update our views
    if (c4) {
	QObject::connect(m, &QgModel::mdl_changed_db, c4, &QtCADQuad::need_update);
	QObject::connect((CADApp *)qApp, &CADApp::gui_changed_view, c4, &QtCADQuad::need_update);
	// The Quad View has an additional condition in the sense that the current view may
	// change.  Probably we won't try to track this for floating dms attached to qged,
	// but the quad view is a central view widget so we need to support it.
	QObject::connect(c4, &QtCADQuad::selected, (CADApp *)qApp, &CADApp::do_quad_view_change);
	ap->curr_view = c4->get(0);
    }

    // If the view changes, let the GUI know.  The ap supplied signals are used by other
    // widgets to hide the specifics of the current view implementation (single or quad).
    if (c4) {
	QObject::connect(c4, &QtCADQuad::changed, ap, &CADApp::do_gui_update_from_view_change);
    }

    // We start out with the View Control panel as the current panel - by
    // default we are viewing, not editing
    vc->makeCurrent(vc);
}

bool
BRLCAD_MainWindow::isValid3D()
{
    if (c4)
	return c4->isValid();
    return false;
}

void
BRLCAD_MainWindow::fallback3D()
{
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
